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

#ifndef prmjtime_h___
#define prmjtime_h___
/*
 * PR date stuff for mocha and java. Placed here temporarily not to break
 * Navigator and localize changes to mocha.
 */
#include <time.h>
#include "prlong.h"
#ifdef MOZILLA_CLIENT
#include "jscompat.h"
#endif

PR_BEGIN_EXTERN_C

typedef struct PRMJTime       PRMJTime;

/*
 * Broken down form of 64 bit time value.
 */
struct PRMJTime {
    PRInt32 tm_usec;		/* microseconds of second (0-999999) */
    PRInt8 tm_sec;		/* seconds of minute (0-59) */
    PRInt8 tm_min;		/* minutes of hour (0-59) */
    PRInt8 tm_hour;		/* hour of day (0-23) */
    PRInt8 tm_mday;		/* day of month (1-31) */
    PRInt8 tm_mon;		/* month of year (0-11) */
    PRInt8 tm_wday;		/* 0=sunday, 1=monday, ... */
    PRInt16 tm_year;		/* absolute year, AD */
    PRInt16 tm_yday;		/* day of year (0 to 365) */
    PRInt8 tm_isdst;		/* non-zero if DST in effect */
};

/* Some handy constants */
#define PRMJ_MSEC_PER_SEC		1000
#define PRMJ_USEC_PER_SEC		1000000L
#define PRMJ_NSEC_PER_SEC		1000000000L
#define PRMJ_USEC_PER_MSEC	1000
#define PRMJ_NSEC_PER_MSEC	1000000L

/* Return the current local time in micro-seconds */
extern PR_IMPLEMENT(PRInt64) PRMJ_Now(void);

/* Return the current local time in micro-seconds */
extern PR_IMPLEMENT(PRInt64) PRMJ_NowLocal(void);

/* Return the current local time, in milliseconds */
extern PR_IMPLEMENT(PRInt64) PRMJ_NowMS(void);

/* Return the current local time in seconds */
extern PR_IMPLEMENT(PRInt64) PRMJ_NowS(void);

/* Convert a local time value into a GMT time value */
extern PR_IMPLEMENT(PRInt64) PRMJ_ToGMT(PRInt64 time);

/* Convert a GMT time value into a lcoal time value */
extern PR_IMPLEMENT(PRInt64) PRMJ_ToLocal(PRInt64 time);

/* get the difference between this time zone and  gmt timezone in seconds */
extern PR_IMPLEMENT(time_t) PRMJ_LocalGMTDifference(void);

/* Explode a 64 bit time value into its components */
extern PR_IMPLEMENT(void) PRMJ_ExplodeTime(PRMJTime *to, PRInt64 time);

/* Compute the 64 bit time value from the components */
extern PR_IMPLEMENT(PRInt64) PRMJ_ComputeTime(PRMJTime *tm);

/* Format a time value into a buffer. Same semantics as strftime() */
extern PR_IMPLEMENT(size_t) PRMJ_FormatTime(char *buf, int buflen, char *fmt,
					    PRMJTime *tm);

/*
 * Format a time value into a buffer. Time is always in US English format,
 * regardless of locale setting.
 */
extern PR_IMPLEMENT(size_t) PRMJ_FormatTimeUSEnglish(char* buf, size_t bufSize,
						   const char* format,
						   const PRMJTime* time);

/* Convert prtm structure into seconds since 1st January, 0 A.D. */
extern PR_IMPLEMENT(PRInt64) PRMJ_mktime(PRMJTime *prtm);

/* Get the Local time into prtime from tsecs */
extern PR_IMPLEMENT(void) PRMJ_localtime(PRInt64 tsecs,PRMJTime *prtm);

/* Get the gmt time into prtime from tsecs */
extern PR_IMPLEMENT(void) PRMJ_gmtime(PRInt64 tsecs,PRMJTime *prtm);

/* Get the DST offset for the local time passed in
 */
extern PR_IMPLEMENT(PRInt64) PRMJ_DSTOffset(PRInt64 time);

PR_END_EXTERN_C

#endif /* prmjtime_h___ */

