/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
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
