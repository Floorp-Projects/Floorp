/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifndef prmjtime_h___
#define prmjtime_h___
/*
 * PR date stuff for mocha and java. Placed here temporarily not to break
 * Navigator and localize changes to mocha.
 */
#include <time.h>
#include "jslong.h"
#ifdef MOZILLA_CLIENT
#include "jscompat.h"
#endif

JS_BEGIN_EXTERN_C

typedef struct PRMJTime       PRMJTime;

/*
 * Broken down form of 64 bit time value.
 */
struct PRMJTime {
    JSInt32 tm_usec;		/* microseconds of second (0-999999) */
    JSInt8 tm_sec;		/* seconds of minute (0-59) */
    JSInt8 tm_min;		/* minutes of hour (0-59) */
    JSInt8 tm_hour;		/* hour of day (0-23) */
    JSInt8 tm_mday;		/* day of month (1-31) */
    JSInt8 tm_mon;		/* month of year (0-11) */
    JSInt8 tm_wday;		/* 0=sunday, 1=monday, ... */
    JSInt16 tm_year;		/* absolute year, AD */
    JSInt16 tm_yday;		/* day of year (0 to 365) */
    JSInt8 tm_isdst;		/* non-zero if DST in effect */
};

/* Some handy constants */
#define PRMJ_USEC_PER_SEC	1000000L
#define PRMJ_USEC_PER_MSEC	1000L

/* Return the current local time in micro-seconds */
extern JSInt64
PRMJ_Now(void);

/* get the difference between this time zone and  gmt timezone in seconds */
extern JSInt32
PRMJ_LocalGMTDifference(void);

/* Format a time value into a buffer. Same semantics as strftime() */
extern size_t
PRMJ_FormatTime(char *buf, int buflen, char *fmt, PRMJTime *tm);

/* Get the DST offset for the local time passed in */
extern JSInt64
PRMJ_DSTOffset(JSInt64 local_time);

JS_END_EXTERN_C

#endif /* prmjtime_h___ */

