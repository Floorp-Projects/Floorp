/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef prtime_h___
#define prtime_h___
/*
 * PR time functions needed by jsdate.c.
 */
#include <time.h>
#include "prlong.h"

PR_BEGIN_EXTERN_C

/*
 * Broken down form of 64 bit time value.
 */
struct PRTime {
    int32 tm_usec;		/* microseconds of second (0-999999) */
    int8 tm_sec;		/* seconds of minute (0-59) */
    int8 tm_min;		/* minutes of hour (0-59) */
    int8 tm_hour;		/* hour of day (0-23) */
    int8 tm_mday;		/* day of month (1-31) */
    int8 tm_mon;		/* month of year (0-11) */
    int8 tm_wday;		/* 0=sunday, 1=monday, ... */
    int16 tm_year;		/* absolute year, AD */
    int16 tm_yday;		/* day of year (0 to 365) */
    int8 tm_isdst;		/* non-zero if DST in effect */
};

/* Some handy constants */
#define PR_USEC_PER_SEC		1000000L
#define PR_USEC_PER_MSEC	1000L

/* Return the current local time in micro-seconds */
extern PR_PUBLIC_API(int64)
PR_Now(void);

/* get the difference between this time zone and  gmt timezone in seconds */
extern PR_PUBLIC_API(time_t)
PR_LocalGMTDifference(void);

/* Format a time value into a buffer. Same semantics as strftime() */
extern PR_PUBLIC_API(size_t)
PR_FormatTime(char *buf, int buflen, char *fmt, PRTime *tm);

/* Get the DST offset for the local time passed in */
extern PR_PUBLIC_API(int64)
PR_DSTOffset(int64 time);

PR_END_EXTERN_C

#endif /* prtime_h___ */
