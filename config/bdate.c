/*
** Needs the NPL blurb.
**
** bdate.c: Possibly cross-platform date-based build number
**          generator.  Output is YYJJJ, where YY == 2-digit
**          year, and JJJ is the Julian date (day of the year).
**
** Author: briano@netscape.com
**
*/

#include <stdio.h>
#include <time.h>

#ifdef SUNOS4
#include "sunos4.h"
#endif

void main(void)
{
	time_t t = time(NULL);
	struct tm *tms;

	tms = localtime(&t);
	printf("500%02d%03d%02d\n", tms->tm_year, 1+tms->tm_yday, tms->tm_hour);
	exit(0);
}
