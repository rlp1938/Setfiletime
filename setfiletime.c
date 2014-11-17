/*      setfiletime.c
 *
 *	Copyright 2011 Bob Parker <rlp1938@gmail.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *	MA 02110-1301, USA.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include <libgen.h>
#include <dirent.h>
#include "fileutil.h"

static time_t parsetimestring(const char *dts);
static time_t ftbyage(int age, char aunit);

static char *helpmsg = "\n\tUsage: setfiletime [option] file\n"
  "\tUpdates file m time to equal the time the program starts or as\n"
  "\t optionally specified.\n"
  "\n\tOptions:\n"
  "\t-h outputs this help message.\n"
  "\t-t N[Mm|Dd], sets file m time to N periods less than now.\n"
  "\t   Periods are by default years, or months (M|m), or days (D|d).\n"
  "\t-o YYYY[MM[DD], sets the actual time for the new m time.\n"
  "\t   It may not be in the future.\n"
  ;

static const char *pathend = "!*END*!"; // Anyone who puts shit like
                                        // that in a filename deserves
                                        // what happens.

void dohelp(int forced);

int main(int argc, char **argv)
{
	int opt;
	time_t newtime;
	char aunit;
	int age;
	char *infile;
	char inbuf[PATH_MAX];
	struct stat sb;
	FILE *fpi;
	struct utimbuf utb;

	// set defaults
	aunit = 'Y';
	newtime = time(0);
	utb.actime = newtime;

	while((opt = getopt(argc, argv, ":ht:o:")) != -1) {
		switch(opt){
		case 'h':
			dohelp(0);
		break;
		case 't': // time periods to deduct from now.
            age = strtol(optarg, NULL, 10);
            if (strchr(optarg, 'M')) aunit = 'M';
            if (strchr(optarg, 'm')) aunit = 'M';
            if (strchr(optarg, 'D')) aunit = 'D';
            if (strchr(optarg, 'd')) aunit = 'D';
            if (strchr(optarg, 'Y')) aunit = 'Y';
            if (strchr(optarg, 'y')) aunit = 'Y';
            newtime = ftbyage(age, aunit);
		break;
		case 'o': // actual time to set m time
			newtime = parsetimestring(optarg);
		break;
		case ':':
			fprintf(stderr, "Option %c requires an argument\n",optopt);
			dohelp(1);
		break;
		case '?':
			fprintf(stderr, "Illegal option: %c\n",optopt);
			dohelp(1);
		break;
		} //switch()
	}//while()
	// now process the non-option arguments

	// 1.Check that argv[???] exists.
	if (!(argv[optind])) {
		fprintf(stderr, "No file provided\n");
		dohelp(1);
	}

	infile = argv[optind];

	// 2. Check that the object exists
    if (stat(infile, &sb) == -1) {
        perror(infile);
        exit(EXIT_FAILURE);
    }
    // 3. It exists then, but is it a file?
    if (!(S_ISREG(sb.st_mode))) {
        fprintf(stderr, "%s is not a regular file\n", infile);
        exit(EXIT_FAILURE);
    }

	// input file
	fpi = dofopen(infile, "r");
	utb.modtime = newtime;
	utb.actime = newtime;

	// update times
	while(fgets(inbuf, PATH_MAX, fpi)) {
		char *eop, *eol;
		eop = strstr(inbuf, pathend);
		eol = strchr(inbuf, '\n');
		if (eop) {
			*eop = '\0';
		} else if (eol) {
			*eol = '\0';
		} else {
			fprintf(stderr, "Malformed line in: %s\n%s\n\n", infile,
							inbuf);
			continue;
		}
		if (utime(inbuf, &utb) == -1) {
			perror(inbuf);	// don't abort, might be just one path
		}
	} // while())

	fclose(fpi);

	return 0;
}//main()

void dohelp(int forced)
{
  fputs(helpmsg, stderr);
  exit(forced);
}

time_t parsetimestring(const char *dts)
{
	// I don't really care if someone enters eg 20130232 the date
	// functions will automatically convert to 20130304 or ..03
	// in a leap year.
	struct tm dt;
	char work[32];
	time_t when;
	time_t now = time(0);

	dt.tm_sec	= 0;
	dt.tm_min	= 0;
	dt.tm_hour	= 0;
	dt.tm_mday	= 1;
	dt.tm_mon	= 0;
	dt.tm_year	= 0;
	dt.tm_wday	= 0;
	dt.tm_yday	= 0;
	dt.tm_isdst	= 0;

	strcpy(work, dts);
	if (strlen(work) > 8) work[8] = '\0';	// discard hh:mm:ss if any.
	switch(strlen(work)) {
		char buf[8];
		case 8:	// yyyymmdd input, process days
		strncpy(buf, &work[6], 2);
		buf[2] = '\0';
		dt.tm_mday = strtol(buf, NULL, 10);
		case 6:	//yyyymm input, process months.
		strncpy(buf, &work[4], 2);
		buf[2] = '\0';
		dt.tm_mon = strtol(buf, NULL, 10) - 1;
		case 4:	// yyyy input, process years.
		strncpy(buf, work, 4);
		buf[4] = '\0';
		dt.tm_year = strtol(buf, NULL, 10) - 1900;
		break;
		default:
		fprintf(stderr, "%s must be an even number long.\n", dts);
		dohelp(EXIT_FAILURE);
		break;
	}
	when = mktime(&dt);
	if (when > now) {
		fprintf(stderr, "%s is a time in the future.\n", dts);
		dohelp(EXIT_FAILURE);
	}
	return mktime(&dt);
} // parsetimestring()

time_t ftbyage(int age, char aunit)
{
	int deduct;
	time_t now, result;
	const double daysinyear = 365.25;
	const double secondsinday = 3600. * 24.;
	const double secondsinyear = daysinyear * secondsinday;
	const double secondsinmonth = secondsinyear / 12. ;

	deduct = 0;
	now = time(0);
	switch(aunit) {
		case 'Y':
		case 'y':
		deduct = age * secondsinyear;
		break;
		case 'M':
		case 'm':
		deduct = age * secondsinmonth;
		break;
		case 'D':
		case 'd':
		deduct = age * secondsinday;
		break;
	}
	result = now - deduct;
	if (result > now) {
		fprintf(stderr, "You may not set a future time\n");
	}
	return result;
} // ftbyage()

