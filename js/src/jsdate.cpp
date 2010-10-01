/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JS date methods.
 */

/*
 * "For example, OS/360 devotes 26 bytes of the permanently
 *  resident date-turnover routine to the proper handling of
 *  December 31 on leap years (when it is Day 366).  That
 *  might have been left to the operator."
 *
 * Frederick Brooks, 'The Second-System Effect'.
 */

#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsprf.h"
#include "prmjtime.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsversion.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsdate.h"
#include "jsinterp.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsstr.h"

#include "jsobjinlines.h"

using namespace js;

/*
 * The JS 'Date' object is patterned after the Java 'Date' object.
 * Here is an script:
 *
 *    today = new Date();
 *
 *    print(today.toLocaleString());
 *
 *    weekDay = today.getDay();
 *
 *
 * These Java (and ECMA-262) methods are supported:
 *
 *     UTC
 *     getDate (getUTCDate)
 *     getDay (getUTCDay)
 *     getHours (getUTCHours)
 *     getMinutes (getUTCMinutes)
 *     getMonth (getUTCMonth)
 *     getSeconds (getUTCSeconds)
 *     getMilliseconds (getUTCMilliseconds)
 *     getTime
 *     getTimezoneOffset
 *     getYear
 *     getFullYear (getUTCFullYear)
 *     parse
 *     setDate (setUTCDate)
 *     setHours (setUTCHours)
 *     setMinutes (setUTCMinutes)
 *     setMonth (setUTCMonth)
 *     setSeconds (setUTCSeconds)
 *     setMilliseconds (setUTCMilliseconds)
 *     setTime
 *     setYear (setFullYear, setUTCFullYear)
 *     toGMTString (toUTCString)
 *     toLocaleString
 *     toString
 *
 *
 * These Java methods are not supported
 *
 *     setDay
 *     before
 *     after
 *     equals
 *     hashCode
 */

/*
 * 11/97 - jsdate.c has been rewritten to conform to the ECMA-262 language
 * definition and reduce dependence on NSPR.  NSPR is used to get the current
 * time in milliseconds, the time zone offset, and the daylight savings time
 * offset for a given time.  NSPR is also used for Date.toLocaleString(), for
 * locale-specific formatting, and to get a string representing the timezone.
 * (Which turns out to be platform-dependent.)
 *
 * To do:
 * (I did some performance tests by timing how long it took to run what
 *  I had of the js ECMA conformance tests.)
 *
 * - look at saving results across multiple calls to supporting
 * functions; the toString functions compute some of the same values
 * multiple times.  Although - I took a quick stab at this, and I lost
 * rather than gained.  (Fractionally.)  Hard to tell what compilers/processors
 * are doing these days.
 *
 * - look at tweaking function return types to return double instead
 * of int; this seems to make things run slightly faster sometimes.
 * (though it could be architecture-dependent.)  It'd be good to see
 * how this does on win32.  (Tried it on irix.)  Types could use a
 * general going-over.
 */

/*
 * Supporting functions - ECMA 15.9.1.*
 */

#define HalfTimeDomain  8.64e15
#define HoursPerDay     24.0
#define MinutesPerDay   (HoursPerDay * MinutesPerHour)
#define MinutesPerHour  60.0
#define SecondsPerDay   (MinutesPerDay * SecondsPerMinute)
#define SecondsPerHour  (MinutesPerHour * SecondsPerMinute)
#define SecondsPerMinute 60.0

#if defined(XP_WIN) || defined(XP_OS2)
/* Work around msvc double optimization bug by making these runtime values; if
 * they're available at compile time, msvc optimizes division by them by
 * computing the reciprocal and multiplying instead of dividing - this loses
 * when the reciprocal isn't representable in a double.
 */
static jsdouble msPerSecond = 1000.0;
static jsdouble msPerDay = SecondsPerDay * 1000.0;
static jsdouble msPerHour = SecondsPerHour * 1000.0;
static jsdouble msPerMinute = SecondsPerMinute * 1000.0;
#else
#define msPerDay        (SecondsPerDay * msPerSecond)
#define msPerHour       (SecondsPerHour * msPerSecond)
#define msPerMinute     (SecondsPerMinute * msPerSecond)
#define msPerSecond     1000.0
#endif

#define Day(t)          floor((t) / msPerDay)

static jsdouble
TimeWithinDay(jsdouble t)
{
    jsdouble result;
    result = fmod(t, msPerDay);
    if (result < 0)
        result += msPerDay;
    return result;
}

static inline bool
IsLeapYear(jsint year)
{
    return year % 4 == 0 && (year % 100 || (year % 400 == 0));
}

static inline jsint
DaysInYear(jsint year) 
{
    return IsLeapYear(year) ? 366 : 365;
}

static inline jsint
DaysInFebruary(jsint year)
{
    return IsLeapYear(year) ? 29 : 28;
}

/* math here has to be f.p, because we need
 *  floor((1968 - 1969) / 4) == -1
 */
#define DayFromYear(y)  (365 * ((y)-1970) + floor(((y)-1969)/4.0)            \
                         - floor(((y)-1901)/100.0) + floor(((y)-1601)/400.0))
#define TimeFromYear(y) (DayFromYear(y) * msPerDay)

static jsint
YearFromTime(jsdouble t)
{
    jsint y = (jsint) floor(t /(msPerDay*365.2425)) + 1970;
    jsdouble t2 = (jsdouble) TimeFromYear(y);

    /*
     * Adjust the year if the approximation was wrong.  Since the year was
     * computed using the average number of ms per year, it will usually
     * be wrong for dates within several hours of a year transition.
     */
    if (t2 > t) {
        y--;
    } else {
        if (t2 + msPerDay * DaysInYear(y) <= t)
            y++;
    }
    return y;
}

#define DayWithinYear(t, year) ((intN) (Day(t) - DayFromYear(year)))

/*
 * The following array contains the day of year for the first day of
 * each month, where index 0 is January, and day 0 is January 1.
 */
static jsdouble firstDayOfMonth[2][13] = {
    {0.0, 31.0, 59.0, 90.0, 120.0, 151.0, 181.0, 212.0, 243.0, 273.0, 304.0, 334.0, 365.0},
    {0.0, 31.0, 60.0, 91.0, 121.0, 152.0, 182.0, 213.0, 244.0, 274.0, 305.0, 335.0, 366.0}
};

#define DayFromMonth(m, leap) firstDayOfMonth[leap][(intN)m]

static intN
DaysInMonth(jsint year, jsint month)
{
    JSBool leap = IsLeapYear(year);
    intN result = intN(DayFromMonth(month, leap) - DayFromMonth(month-1, leap));
    return result;
}

static intN
MonthFromTime(jsdouble t)
{
    intN d, step;
    jsint year = YearFromTime(t);
    d = DayWithinYear(t, year);

    if (d < (step = 31))
        return 0;
    if (d < (step += DaysInFebruary(year)))
        return 1;
    if (d < (step += 31))
        return 2;
    if (d < (step += 30))
        return 3;
    if (d < (step += 31))
        return 4;
    if (d < (step += 30))
        return 5;
    if (d < (step += 31))
        return 6;
    if (d < (step += 31))
        return 7;
    if (d < (step += 30))
        return 8;
    if (d < (step += 31))
        return 9;
    if (d < (step += 30))
        return 10;
    return 11;
}

static intN
DateFromTime(jsdouble t)
{
    intN d, step, next;
    jsint year = YearFromTime(t);
    d = DayWithinYear(t, year);

    if (d <= (next = 30))
        return d + 1;
    step = next;
    if (d <= (next += DaysInFebruary(year)))
        return d - step;
    step = next;
    if (d <= (next += 31))
        return d - step;
    step = next;
    if (d <= (next += 30))
        return d - step;
    step = next;
    if (d <= (next += 31))
        return d - step;
    step = next;
    if (d <= (next += 30))
        return d - step;
    step = next;
    if (d <= (next += 31))
        return d - step;
    step = next;
    if (d <= (next += 31))
        return d - step;
    step = next;
    if (d <= (next += 30))
        return d - step;
    step = next;
    if (d <= (next += 31))
        return d - step;
    step = next;
    if (d <= (next += 30))
        return d - step;
    step = next;
    return d - step;
}

static intN
WeekDay(jsdouble t)
{
    jsint result;
    result = (jsint) Day(t) + 4;
    result = result % 7;
    if (result < 0)
        result += 7;
    return (intN) result;
}

#define MakeTime(hour, min, sec, ms) \
((((hour) * MinutesPerHour + (min)) * SecondsPerMinute + (sec)) * msPerSecond + (ms))

static jsdouble
MakeDay(jsdouble year, jsdouble month, jsdouble date)
{
    JSBool leap;
    jsdouble yearday;
    jsdouble monthday;

    year += floor(month / 12);

    month = fmod(month, 12.0);
    if (month < 0)
        month += 12;

    leap = IsLeapYear((jsint) year);

    yearday = floor(TimeFromYear(year) / msPerDay);
    monthday = DayFromMonth(month, leap);

    return yearday + monthday + date - 1;
}

#define MakeDate(day, time) ((day) * msPerDay + (time))

/*
 * Years and leap years on which Jan 1 is a Sunday, Monday, etc.
 *
 * yearStartingWith[0][i] is an example non-leap year where
 * Jan 1 appears on Sunday (i == 0), Monday (i == 1), etc.
 *
 * yearStartingWith[1][i] is an example leap year where
 * Jan 1 appears on Sunday (i == 0), Monday (i == 1), etc.
 */
static jsint yearStartingWith[2][7] = {
    {1978, 1973, 1974, 1975, 1981, 1971, 1977},
    {1984, 1996, 1980, 1992, 1976, 1988, 1972}
};

/*
 * Find a year for which any given date will fall on the same weekday.
 *
 * This function should be used with caution when used other than
 * for determining DST; it hasn't been proven not to produce an
 * incorrect year for times near year boundaries.
 */
static jsint
EquivalentYearForDST(jsint year)
{
    jsint day;

    day = (jsint) DayFromYear(year) + 4;
    day = day % 7;
    if (day < 0)
        day += 7;

    return yearStartingWith[IsLeapYear(year)][day];
}

/* LocalTZA gets set by js_InitDateClass() */
static jsdouble LocalTZA;

static jsdouble
DaylightSavingTA(jsdouble t, JSContext *cx)
{
    /* abort if NaN */
    if (JSDOUBLE_IS_NaN(t))
        return t;

    /*
     * If earlier than 1970 or after 2038, potentially beyond the ken of
     * many OSes, map it to an equivalent year before asking.
     */
    if (t < 0.0 || t > 2145916800000.0) {
        jsint year = EquivalentYearForDST(YearFromTime(t));
        jsdouble day = MakeDay(year, MonthFromTime(t), DateFromTime(t));
        t = MakeDate(day, TimeWithinDay(t));
    }

    int64 timeMilliseconds = static_cast<int64>(t);
    int64 offsetMilliseconds = cx->dstOffsetCache.getDSTOffsetMilliseconds(timeMilliseconds, cx);
    return static_cast<jsdouble>(offsetMilliseconds);
}

static jsdouble
AdjustTime(jsdouble date, JSContext *cx)
{
    jsdouble t = DaylightSavingTA(date, cx) + LocalTZA;
    t = (LocalTZA >= 0) ? fmod(t, msPerDay) : -fmod(msPerDay - t, msPerDay);
    return t;
}

static jsdouble
LocalTime(jsdouble t, JSContext *cx)
{
    return t + AdjustTime(t, cx);
}

static jsdouble
UTC(jsdouble t, JSContext *cx)
{
    return t - AdjustTime(t - LocalTZA, cx);
}

static intN
HourFromTime(jsdouble t)
{
    intN result = (intN) fmod(floor(t/msPerHour), HoursPerDay);
    if (result < 0)
        result += (intN)HoursPerDay;
    return result;
}

static intN
MinFromTime(jsdouble t)
{
    intN result = (intN) fmod(floor(t / msPerMinute), MinutesPerHour);
    if (result < 0)
        result += (intN)MinutesPerHour;
    return result;
}

static intN
SecFromTime(jsdouble t)
{
    intN result = (intN) fmod(floor(t / msPerSecond), SecondsPerMinute);
    if (result < 0)
        result += (intN)SecondsPerMinute;
    return result;
}

static intN
msFromTime(jsdouble t)
{
    intN result = (intN) fmod(t, msPerSecond);
    if (result < 0)
        result += (intN)msPerSecond;
    return result;
}

#define TIMECLIP(d) ((JSDOUBLE_IS_FINITE(d) \
                      && !((d < 0 ? -d : d) > HalfTimeDomain)) \
                     ? js_DoubleToInteger(d + (+0.)) : js_NaN)

/**
 * end of ECMA 'support' functions
 */

/*
 * Other Support routines and definitions
 */

Class js_DateClass = {
    js_Date_str,
    JSCLASS_HAS_RESERVED_SLOTS(JSObject::DATE_CLASS_RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Date),
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub
};

/* for use by date_parse */

static const char* wtb[] = {
    "am", "pm",
    "monday", "tuesday", "wednesday", "thursday", "friday",
    "saturday", "sunday",
    "january", "february", "march", "april", "may", "june",
    "july", "august", "september", "october", "november", "december",
    "gmt", "ut", "utc",
    "est", "edt",
    "cst", "cdt",
    "mst", "mdt",
    "pst", "pdt"
    /* time zone table needs to be expanded */
};

static int ttb[] = {
    -1, -2, 0, 0, 0, 0, 0, 0, 0,       /* AM/PM */
    2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
    10000 + 0, 10000 + 0, 10000 + 0,   /* GMT/UT/UTC */
    10000 + 5 * 60, 10000 + 4 * 60,    /* EST/EDT */
    10000 + 6 * 60, 10000 + 5 * 60,    /* CST/CDT */
    10000 + 7 * 60, 10000 + 6 * 60,    /* MST/MDT */
    10000 + 8 * 60, 10000 + 7 * 60     /* PST/PDT */
};

/* helper for date_parse */
static JSBool
date_regionMatches(const char* s1, int s1off, const jschar* s2, int s2off,
                   int count, int ignoreCase)
{
    JSBool result = JS_FALSE;
    /* return true if matches, otherwise, false */

    while (count > 0 && s1[s1off] && s2[s2off]) {
        if (ignoreCase) {
            if (JS_TOLOWER((jschar)s1[s1off]) != JS_TOLOWER(s2[s2off])) {
                break;
            }
        } else {
            if ((jschar)s1[s1off] != s2[s2off]) {
                break;
            }
        }
        s1off++;
        s2off++;
        count--;
    }

    if (count == 0) {
        result = JS_TRUE;
    }

    return result;
}

/* find UTC time from given date... no 1900 correction! */
static jsdouble
date_msecFromDate(jsdouble year, jsdouble mon, jsdouble mday, jsdouble hour,
                  jsdouble min, jsdouble sec, jsdouble msec)
{
    jsdouble day;
    jsdouble msec_time;
    jsdouble result;

    day = MakeDay(year, mon, mday);
    msec_time = MakeTime(hour, min, sec, msec);
    result = MakeDate(day, msec_time);
    return result;
}

/* compute the time in msec (unclipped) from the given args */
#define MAXARGS        7

static JSBool
date_msecFromArgs(JSContext *cx, uintN argc, Value *argv, jsdouble *rval)
{
    uintN loop;
    jsdouble array[MAXARGS];
    jsdouble msec_time;

    for (loop = 0; loop < MAXARGS; loop++) {
        if (loop < argc) {
            jsdouble d;
            if (!ValueToNumber(cx, argv[loop], &d))
                return JS_FALSE;
            /* return NaN if any arg is not finite */
            if (!JSDOUBLE_IS_FINITE(d)) {
                *rval = js_NaN;
                return JS_TRUE;
            }
            array[loop] = js_DoubleToInteger(d);
        } else {
            if (loop == 2) {
                array[loop] = 1; /* Default the date argument to 1. */
            } else {
                array[loop] = 0;
            }
        }
    }

    /* adjust 2-digit years into the 20th century */
    if (array[0] >= 0 && array[0] <= 99)
        array[0] += 1900;

    msec_time = date_msecFromDate(array[0], array[1], array[2],
                                  array[3], array[4], array[5], array[6]);
    *rval = msec_time;
    return JS_TRUE;
}

/*
 * See ECMA 15.9.4.[3-10];
 */
static JSBool
date_UTC(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble msec_time;

    if (!date_msecFromArgs(cx, argc, vp + 2, &msec_time))
        return JS_FALSE;

    msec_time = TIMECLIP(msec_time);

    vp->setNumber(msec_time);
    return JS_TRUE;
}

/*
 * Read and convert decimal digits from s[*i] into *result
 * while *i < limit. 
 * 
 * Succeed if any digits are converted. Advance *i only
 * as digits are consumed.
 */
static JSBool
digits(size_t *result, const jschar *s, size_t *i, size_t limit)
{
    size_t init = *i;
    *result = 0;
    while (*i < limit && 
           ('0' <= s[*i] && s[*i] <= '9')) {
        *result *= 10;
        *result += (s[*i] - '0');
        ++(*i);
    }
    return (*i != init);
}

/* 
 * Read and convert decimal digits to the right of a decimal point,
 * representing a fractional integer, from s[*i] into *result
 * while *i < limit. 
 * 
 * Succeed if any digits are converted. Advance *i only
 * as digits are consumed.
 */
static JSBool
fractional(jsdouble *result, const jschar *s, size_t *i, size_t limit)
{
    jsdouble factor = 0.1;
    size_t init = *i;
    *result = 0.0;
    while (*i < limit && 
           ('0' <= s[*i] && s[*i] <= '9')) {
        *result += (s[*i] - '0') * factor;
        factor *= 0.1;
        ++(*i);
    }
    return (*i != init);
}

/* 
 * Read and convert exactly n decimal digits from s[*i] 
 * to s[min(*i+n,limit)] into *result. 
 *
 * Succeed if exactly n digits are converted. Advance *i only
 * on success.
 */
static JSBool
ndigits(size_t n, size_t *result, const jschar *s, size_t* i, size_t limit)
{
    size_t init = *i;

    if (digits(result, s, i, JS_MIN(limit, init+n)))
        return ((*i - init) == n);
    
    *i = init;
    return JS_FALSE;
}

/* 
 * Parse a string in one of the date-time formats given by the W3C
 * "NOTE-datetime" specification. These formats make up a restricted
 * profile of the ISO 8601 format. Quoted here:
 *
 *   The formats are as follows. Exactly the components shown here
 *   must be present, with exactly this punctuation. Note that the "T"
 *   appears literally in the string, to indicate the beginning of the
 *   time element, as specified in ISO 8601.
 *
 *   Any combination of the date formats with the time formats is
 *   allowed, and also either the date or the time can be missing.
 *
 *   The specification is silent on the meaning when fields are
 *   ommitted so the interpretations are a guess, but hopefully a
 *   reasonable one. We default the month to January, the day to the
 *   1st, and hours minutes and seconds all to 0. If the date is
 *   missing entirely then we assume 1970-01-01 so that the time can
 *   be aded to a date later. If the time is missing then we assume
 *   00:00 UTC.  If the time is present but the time zone field is
 *   missing then we use local time.
 * 
 * Date part:
 *
 *  Year:
 *     YYYY (eg 1997)
 *
 *  Year and month:
 *     YYYY-MM (eg 1997-07)
 *
 *  Complete date:
 *     YYYY-MM-DD (eg 1997-07-16)
 *
 * Time part:
 *
 *  Hours and minutes:
 *     Thh:mmTZD (eg T19:20+01:00)
 * 
 *  Hours, minutes and seconds:
 *     Thh:mm:ssTZD (eg T19:20:30+01:00)
 *
 *  Hours, minutes, seconds and a decimal fraction of a second:
 *     Thh:mm:ss.sTZD (eg T19:20:30.45+01:00)
 *
 * where:
 *
 *   YYYY = four-digit year or six digit year as +YYYYYY or -YYYYYY
 *   MM   = two-digit month (01=January, etc.)
 *   DD   = two-digit day of month (01 through 31)
 *   hh   = two digits of hour (00 through 23) (am/pm NOT allowed)
 *   mm   = two digits of minute (00 through 59)
 *   ss   = two digits of second (00 through 59)
 *   s    = one or more digits representing a decimal fraction of a second
 *   TZD  = time zone designator (Z or +hh:mm or -hh:mm or missing for local)
 */

static JSBool
date_parseISOString(JSString *str, jsdouble *result, JSContext *cx)
{
    jsdouble msec;

    const jschar *s;
    size_t limit;
    size_t i = 0;
    int tzMul = 1;
    int dateMul = 1;
    size_t year = 1970;
    size_t month = 1;
    size_t day = 1;
    size_t hour = 0;
    size_t min = 0;
    size_t sec = 0;
    jsdouble frac = 0;
    bool isLocalTime = JS_FALSE;
    size_t tzHour = 0;
    size_t tzMin = 0;

#define PEEK(ch) (i < limit && s[i] == ch)

#define NEED(ch)                                                     \
    JS_BEGIN_MACRO                                                   \
        if (i >= limit || s[i] != ch) { goto syntax; } else { ++i; } \
    JS_END_MACRO 

#define DONE_DATE_UNLESS(ch)                                            \
    JS_BEGIN_MACRO                                                      \
        if (i >= limit || s[i] != ch) { goto done_date; } else { ++i; } \
    JS_END_MACRO 

#define DONE_UNLESS(ch)                                            \
    JS_BEGIN_MACRO                                                 \
        if (i >= limit || s[i] != ch) { goto done; } else { ++i; } \
    JS_END_MACRO 

#define NEED_NDIGITS(n, field)                                      \
    JS_BEGIN_MACRO                                                  \
        if (!ndigits(n, &field, s, &i, limit)) { goto syntax; }     \
    JS_END_MACRO 

    str->getCharsAndLength(s, limit);

    if (PEEK('+') || PEEK('-')) {
        if (PEEK('-'))
            dateMul = -1;
        ++i;
        NEED_NDIGITS(6, year);
    } else if (!PEEK('T')) {
        NEED_NDIGITS(4, year);
    }
    DONE_DATE_UNLESS('-');
    NEED_NDIGITS(2, month);
    DONE_DATE_UNLESS('-');
    NEED_NDIGITS(2, day);

 done_date:
    DONE_UNLESS('T');
    NEED_NDIGITS(2, hour);
    NEED(':');
    NEED_NDIGITS(2, min);

    if (PEEK(':')) {
        ++i;
        NEED_NDIGITS(2, sec);
        if (PEEK('.')) {
            ++i;
            if (!fractional(&frac, s, &i, limit))
                goto syntax;
        }
    }

    if (PEEK('Z')) {
        ++i;
    } else if (PEEK('+') || PEEK('-')) {
        if (PEEK('-'))
            tzMul = -1;
        ++i;
        NEED_NDIGITS(2, tzHour);
        NEED(':');
        NEED_NDIGITS(2, tzMin);
    } else {
        isLocalTime = JS_TRUE;
    }

 done:
    if (year > 275943 // ceil(1e8/365) + 1970
        || (month == 0 || month > 12)
        || (day == 0 || day > size_t(DaysInMonth(year,month)))
        || hour > 24 
        || ((hour == 24) && (min > 0 || sec > 0))
        || min > 59 
        || sec > 59
        || tzHour > 23
        || tzMin > 59) 
        goto syntax;

    if (i != limit)
        goto syntax;

    month -= 1; /* convert month to 0-based */

    msec = date_msecFromDate(dateMul * (jsdouble)year, month, day,
                             hour, min, sec,
                             frac * 1000.0);;

    if (isLocalTime) {
        msec = UTC(msec, cx);
    } else {
        msec -= ((tzMul) * ((tzHour * msPerHour) 
                            + (tzMin * msPerMinute)));
    }

    if (msec < -8.64e15 || msec > 8.64e15)
        goto syntax;

    *result = msec;

    return JS_TRUE;

 syntax:
    /* syntax error */
    *result = 0;
    return JS_FALSE;

#undef PEEK
#undef NEED
#undef DONE_UNLESS
#undef NEED_NDIGITS
}

static JSBool
date_parseString(JSString *str, jsdouble *result, JSContext *cx)
{
    jsdouble msec;

    const jschar *s;
    size_t limit;
    size_t i = 0;
    int year = -1;
    int mon = -1;
    int mday = -1;
    int hour = -1;
    int min = -1;
    int sec = -1;
    int c = -1;
    int n = -1;
    int tzoffset = -1;
    int prevc = 0;
    JSBool seenplusminus = JS_FALSE;
    int temp;
    JSBool seenmonthname = JS_FALSE;

    if (date_parseISOString(str, result, cx))
        return JS_TRUE;

    str->getCharsAndLength(s, limit);
    if (limit == 0)
        goto syntax;
    while (i < limit) {
        c = s[i];
        i++;
        if (c <= ' ' || c == ',' || c == '-') {
            if (c == '-' && '0' <= s[i] && s[i] <= '9') {
              prevc = c;
            }
            continue;
        }
        if (c == '(') { /* comments) */
            int depth = 1;
            while (i < limit) {
                c = s[i];
                i++;
                if (c == '(') depth++;
                else if (c == ')')
                    if (--depth <= 0)
                        break;
            }
            continue;
        }
        if ('0' <= c && c <= '9') {
            n = c - '0';
            while (i < limit && '0' <= (c = s[i]) && c <= '9') {
                n = n * 10 + c - '0';
                i++;
            }

            /* allow TZA before the year, so
             * 'Wed Nov 05 21:49:11 GMT-0800 1997'
             * works */

            /* uses of seenplusminus allow : in TZA, so Java
             * no-timezone style of GMT+4:30 works
             */

            if ((prevc == '+' || prevc == '-')/*  && year>=0 */) {
                /* make ':' case below change tzoffset */
                seenplusminus = JS_TRUE;

                /* offset */
                if (n < 24)
                    n = n * 60; /* EG. "GMT-3" */
                else
                    n = n % 100 + n / 100 * 60; /* eg "GMT-0430" */
                if (prevc == '+')       /* plus means east of GMT */
                    n = -n;
                if (tzoffset != 0 && tzoffset != -1)
                    goto syntax;
                tzoffset = n;
            } else if (prevc == '/' && mon >= 0 && mday >= 0 && year < 0) {
                if (c <= ' ' || c == ',' || c == '/' || i >= limit)
                    year = n;
                else
                    goto syntax;
            } else if (c == ':') {
                if (hour < 0)
                    hour = /*byte*/ n;
                else if (min < 0)
                    min = /*byte*/ n;
                else
                    goto syntax;
            } else if (c == '/') {
                /* until it is determined that mon is the actual
                   month, keep it as 1-based rather than 0-based */
                if (mon < 0)
                    mon = /*byte*/ n;
                else if (mday < 0)
                    mday = /*byte*/ n;
                else
                    goto syntax;
            } else if (i < limit && c != ',' && c > ' ' && c != '-' && c != '(') {
                goto syntax;
            } else if (seenplusminus && n < 60) {  /* handle GMT-3:30 */
                if (tzoffset < 0)
                    tzoffset -= n;
                else
                    tzoffset += n;
            } else if (hour >= 0 && min < 0) {
                min = /*byte*/ n;
            } else if (prevc == ':' && min >= 0 && sec < 0) {
                sec = /*byte*/ n;
            } else if (mon < 0) {
                mon = /*byte*/n;
            } else if (mon >= 0 && mday < 0) {
                mday = /*byte*/ n;
            } else if (mon >= 0 && mday >= 0 && year < 0) {
                year = n;
            } else {
                goto syntax;
            }
            prevc = 0;
        } else if (c == '/' || c == ':' || c == '+' || c == '-') {
            prevc = c;
        } else {
            size_t st = i - 1;
            int k;
            while (i < limit) {
                c = s[i];
                if (!(('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z')))
                    break;
                i++;
            }
            if (i <= st + 1)
                goto syntax;
            for (k = JS_ARRAY_LENGTH(wtb); --k >= 0;)
                if (date_regionMatches(wtb[k], 0, s, st, i-st, 1)) {
                    int action = ttb[k];
                    if (action != 0) {
                        if (action < 0) {
                            /*
                             * AM/PM. Count 12:30 AM as 00:30, 12:30 PM as
                             * 12:30, instead of blindly adding 12 if PM.
                             */
                            JS_ASSERT(action == -1 || action == -2);
                            if (hour > 12 || hour < 0) {
                                goto syntax;
                            } else {
                                if (action == -1 && hour == 12) { /* am */
                                    hour = 0;
                                } else if (action == -2 && hour != 12) { /* pm */
                                    hour += 12;
                                }
                            }
                        } else if (action <= 13) { /* month! */
                            /* Adjust mon to be 1-based until the final values
                               for mon, mday and year are adjusted below */
                            if (seenmonthname) {
                                goto syntax;
                            }
                            seenmonthname = JS_TRUE;
                            temp = /*byte*/ (action - 2) + 1;

                            if (mon < 0) {
                                mon = temp;
                            } else if (mday < 0) {
                                mday = mon;
                                mon = temp;
                            } else if (year < 0) {
                                year = mon;
                                mon = temp;
                            } else {
                                goto syntax;
                            }
                        } else {
                            tzoffset = action - 10000;
                        }
                    }
                    break;
                }
            if (k < 0)
                goto syntax;
            prevc = 0;
        }
    }
    if (year < 0 || mon < 0 || mday < 0)
        goto syntax;
    /*
      Case 1. The input string contains an English month name.
              The form of the string can be month f l, or f month l, or
              f l month which each evaluate to the same date.
              If f and l are both greater than or equal to 70, or
              both less than 70, the date is invalid.
              The year is taken to be the greater of the values f, l.
              If the year is greater than or equal to 70 and less than 100,
              it is considered to be the number of years after 1900.
      Case 2. The input string is of the form "f/m/l" where f, m and l are
              integers, e.g. 7/16/45.
              Adjust the mon, mday and year values to achieve 100% MSIE
              compatibility.
              a. If 0 <= f < 70, f/m/l is interpreted as month/day/year.
                 i.  If year < 100, it is the number of years after 1900
                 ii. If year >= 100, it is the number of years after 0.
              b. If 70 <= f < 100
                 i.  If m < 70, f/m/l is interpreted as
                     year/month/day where year is the number of years after
                     1900.
                 ii. If m >= 70, the date is invalid.
              c. If f >= 100
                 i.  If m < 70, f/m/l is interpreted as
                     year/month/day where year is the number of years after 0.
                 ii. If m >= 70, the date is invalid.
    */
    if (seenmonthname) {
        if ((mday >= 70 && year >= 70) || (mday < 70 && year < 70)) {
            goto syntax;
        }
        if (mday > year) {
            temp = year;
            year = mday;
            mday = temp;
        }
        if (year >= 70 && year < 100) {
            year += 1900;
        }
    } else if (mon < 70) { /* (a) month/day/year */
        if (year < 100) {
            year += 1900;
        }
    } else if (mon < 100) { /* (b) year/month/day */
        if (mday < 70) {
            temp = year;
            year = mon + 1900;
            mon = mday;
            mday = temp;
        } else {
            goto syntax;
        }
    } else { /* (c) year/month/day */
        if (mday < 70) {
            temp = year;
            year = mon;
            mon = mday;
            mday = temp;
        } else {
            goto syntax;
        }
    }
    mon -= 1; /* convert month to 0-based */
    if (sec < 0)
        sec = 0;
    if (min < 0)
        min = 0;
    if (hour < 0)
        hour = 0;

    msec = date_msecFromDate(year, mon, mday, hour, min, sec, 0);

    if (tzoffset == -1) { /* no time zone specified, have to use local */
        msec = UTC(msec, cx);
    } else {
        msec += tzoffset * msPerMinute;
    }

    *result = msec;
    return JS_TRUE;

syntax:
    /* syntax error */
    *result = 0;
    return JS_FALSE;
}

static JSBool
date_parse(JSContext *cx, uintN argc, Value *vp)
{
    JSString *str;
    jsdouble result;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return true;
    }
    str = js_ValueToString(cx, vp[2]);
    if (!str)
        return JS_FALSE;
    vp[2].setString(str);
    if (!date_parseString(str, &result, cx)) {
        vp->setDouble(js_NaN);
        return true;
    }

    result = TIMECLIP(result);
    vp->setNumber(result);
    return true;
}

static inline jsdouble
NowAsMillis()
{
    return (jsdouble) (PRMJ_Now() / PRMJ_USEC_PER_MSEC);
}

static JSBool
date_now(JSContext *cx, uintN argc, Value *vp)
{
    vp->setDouble(NowAsMillis());
    return JS_TRUE;
}

#ifdef JS_TRACER
static jsdouble FASTCALL
date_now_tn(JSContext*)
{
    return NowAsMillis();
}
#endif

/*
 * Get UTC time from the date object. Returns false if the object is not
 * Date type.
 */
static JSBool
GetUTCTime(JSContext *cx, JSObject *obj, Value *vp, jsdouble *dp)
{
    if (!InstanceOf(cx, obj, &js_DateClass, vp ? vp + 2 : NULL))
        return JS_FALSE;
    *dp = obj->getDateUTCTime().toNumber();
    return JS_TRUE;
}

/*
 * Set UTC time to a given time and invalidate cached local time.
 */
static JSBool
SetUTCTime(JSContext *cx, JSObject *obj, jsdouble t, Value *vp = NULL)
{
    JS_ASSERT(obj->isDate());

    size_t slotCap = JS_MIN(obj->numSlots(), JSObject::DATE_CLASS_RESERVED_SLOTS);
    for (size_t ind = JSObject::JSSLOT_DATE_COMPONENTS_START; ind < slotCap; ind++)
        obj->getSlotRef(ind).setUndefined();

    obj->setDateUTCTime(DoubleValue(t));
    if (vp)
        vp->setDouble(t);
    return true;
}

static void
SetDateToNaN(JSContext *cx, JSObject *obj, Value *vp = NULL)
{
    jsdouble NaN = cx->runtime->NaNValue.getDoubleRef();
    SetUTCTime(cx, obj, NaN, vp);
}

/*
 * Cache the local time, year, month, and so forth of the object.
 * If UTC time is not finite (e.g., NaN), the local time
 * slots will be set to the UTC time without conversion.
 */
static bool
FillLocalTimes(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isDate());

    jsdouble utcTime = obj->getDateUTCTime().toNumber();

    /* Make sure there are slots to store the cached information. */
    if (obj->numSlots() < JSObject::DATE_CLASS_RESERVED_SLOTS) {
        if (!obj->growSlots(cx, JSObject::DATE_CLASS_RESERVED_SLOTS))
            return false;
    }

    if (!JSDOUBLE_IS_FINITE(utcTime)) {
        for (size_t ind = JSObject::JSSLOT_DATE_COMPONENTS_START;
             ind < JSObject::DATE_CLASS_RESERVED_SLOTS;
             ind++) {
            obj->setSlot(ind, DoubleValue(utcTime));
        }
        return true;
    }

    jsdouble localTime = LocalTime(utcTime, cx);

    obj->setSlot(JSObject::JSSLOT_DATE_LOCAL_TIME, DoubleValue(localTime));

    jsint year = (jsint) floor(localTime /(msPerDay*365.2425)) + 1970;
    jsdouble yearStartTime = (jsdouble) TimeFromYear(year);

    /* Adjust the year in case the approximation was wrong, as in YearFromTime. */
    jsint yearDays;
    if (yearStartTime > localTime) {
        year--;
        yearStartTime -= (msPerDay * DaysInYear(year));
        yearDays = DaysInYear(year);
    } else {
        yearDays = DaysInYear(year);
        jsdouble nextStart = yearStartTime + (msPerDay * yearDays);
        if (nextStart <= localTime) {
            year++;
            yearStartTime = nextStart;
            yearDays = DaysInYear(year);
        }
    }

    obj->setSlot(JSObject::JSSLOT_DATE_LOCAL_YEAR, Int32Value(year));

    uint64 yearTime = uint64(localTime - yearStartTime);
    jsint yearSeconds = uint32(yearTime / 1000);

    jsint day = yearSeconds / jsint(SecondsPerDay);

    jsint step = -1, next = 30;
    jsint month;

    do {
        if (day <= next) {
            month = 0;
            break;
        }
        step = next;
        next += ((yearDays == 366) ? 29 : 28);
        if (day <= next) {
            month = 1;
            break;
        }
        step = next;
        if (day <= (next += 31)) {
            month = 2;
            break;
        }
        step = next;
        if (day <= (next += 30)) {
            month = 3;
            break;
        }
        step = next;
        if (day <= (next += 31)) {
            month = 4;
            break;
        }
        step = next;
        if (day <= (next += 30)) {
            month = 5;
            break;
        }
        step = next;
        if (day <= (next += 31)) {
            month = 6;
            break;
        }
        step = next;
        if (day <= (next += 31)) {
            month = 7;
            break;
        }
        step = next;
        if (day <= (next += 30)) {
            month = 8;
            break;
        }
        step = next;
        if (day <= (next += 31)) {
            month = 9;
            break;
        }
        step = next;
        if (day <= (next += 30)) {
            month = 10;
            break;
        }
        step = next;
        month = 11;
    } while (0);

    obj->setSlot(JSObject::JSSLOT_DATE_LOCAL_MONTH, Int32Value(month));
    obj->setSlot(JSObject::JSSLOT_DATE_LOCAL_DATE, Int32Value(day - step));

    jsint weekday = WeekDay(localTime);

    obj->setSlot(JSObject::JSSLOT_DATE_LOCAL_DAY, Int32Value(weekday));

    jsint seconds = yearSeconds % 60;

    obj->setSlot(JSObject::JSSLOT_DATE_LOCAL_SECONDS, Int32Value(seconds));

    jsint minutes = (yearSeconds / 60) % 60;

    obj->setSlot(JSObject::JSSLOT_DATE_LOCAL_MINUTES, Int32Value(minutes));

    jsint hours = (yearSeconds / (60 * 60)) % 24;

    obj->setSlot(JSObject::JSSLOT_DATE_LOCAL_HOURS, Int32Value(hours));

    return true;
}

/* Cache the local times in obj, if necessary. */
static inline JSBool
GetAndCacheLocalTime(JSContext *cx, JSObject *obj, Value *vp, jsdouble *time = NULL)
{
    if (!obj || !InstanceOf(cx, obj, &js_DateClass, vp ? vp + 2 : NULL))
        return false;

    /* If the local time is undefined, we need to fill in the cached values. */
    if (obj->getSlot(JSObject::JSSLOT_DATE_LOCAL_TIME).isUndefined()) {
        if (!FillLocalTimes(cx, obj))
            return false;
    }

    if (time)
        *time = obj->getSlot(JSObject::JSSLOT_DATE_LOCAL_TIME).toDouble();

    return true;
}

/*
 * See ECMA 15.9.5.4 thru 15.9.5.23
 */
static JSBool
date_getTime(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble result;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &result))
        return JS_FALSE;
    vp->setNumber(result);
    return JS_TRUE;
}

static JSBool
date_getYear(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!GetAndCacheLocalTime(cx, obj, vp))
        return JS_FALSE;

    Value yearVal = obj->getSlot(JSObject::JSSLOT_DATE_LOCAL_YEAR);
    if (yearVal.isInt32()) {
        /* Follow ECMA-262 to the letter, contrary to IE JScript. */
        jsint year = yearVal.toInt32() - 1900;
        vp->setInt32(year);
    } else {
        *vp = yearVal;
    }

    return JS_TRUE;
}

static JSBool
date_getFullYear(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!GetAndCacheLocalTime(cx, obj, vp))
        return JS_FALSE;

    *vp = obj->getSlot(JSObject::JSSLOT_DATE_LOCAL_YEAR);
    return JS_TRUE;
}

static JSBool
date_getUTCFullYear(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble result;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &result))
        return JS_FALSE;

    if (JSDOUBLE_IS_FINITE(result))
        result = YearFromTime(result);

    vp->setNumber(result);
    return JS_TRUE;
}

static JSBool
date_getMonth(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!GetAndCacheLocalTime(cx, obj, vp))
        return JS_FALSE;

    *vp = obj->getSlot(JSObject::JSSLOT_DATE_LOCAL_MONTH);
    return JS_TRUE;
}

static JSBool
date_getUTCMonth(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble result;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &result))
        return JS_FALSE;

    if (JSDOUBLE_IS_FINITE(result))
        result = MonthFromTime(result);

    vp->setNumber(result);
    return JS_TRUE;
}

static JSBool
date_getDate(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!GetAndCacheLocalTime(cx, obj, vp))
        return JS_FALSE;

    *vp = obj->getSlot(JSObject::JSSLOT_DATE_LOCAL_DATE);
    return JS_TRUE;
}

static JSBool
date_getUTCDate(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble result;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &result))
        return JS_FALSE;

    if (JSDOUBLE_IS_FINITE(result))
        result = DateFromTime(result);

    vp->setNumber(result);
    return JS_TRUE;
}

static JSBool
date_getDay(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!GetAndCacheLocalTime(cx, obj, vp))
        return JS_FALSE;

    *vp = obj->getSlot(JSObject::JSSLOT_DATE_LOCAL_DAY);
    return JS_TRUE;
}

static JSBool
date_getUTCDay(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble result;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &result))
        return JS_FALSE;

    if (JSDOUBLE_IS_FINITE(result))
        result = WeekDay(result);

    vp->setNumber(result);
    return JS_TRUE;
}

static JSBool
date_getHours(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!GetAndCacheLocalTime(cx, obj, vp))
        return JS_FALSE;

    *vp = obj->getSlot(JSObject::JSSLOT_DATE_LOCAL_HOURS);
    return JS_TRUE;
}

static JSBool
date_getUTCHours(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble result;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &result))
        return JS_FALSE;

    if (JSDOUBLE_IS_FINITE(result))
        result = HourFromTime(result);

    vp->setNumber(result);
    return JS_TRUE;
}

static JSBool
date_getMinutes(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!GetAndCacheLocalTime(cx, obj, vp))
        return JS_FALSE;

    *vp = obj->getSlot(JSObject::JSSLOT_DATE_LOCAL_MINUTES);
    return JS_TRUE;
}

static JSBool
date_getUTCMinutes(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble result;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &result))
        return JS_FALSE;

    if (JSDOUBLE_IS_FINITE(result))
        result = MinFromTime(result);

    vp->setNumber(result);
    return JS_TRUE;
}

/* Date.getSeconds is mapped to getUTCSeconds */

static JSBool
date_getUTCSeconds(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!GetAndCacheLocalTime(cx, obj, vp))
        return JS_FALSE;

    *vp = obj->getSlot(JSObject::JSSLOT_DATE_LOCAL_SECONDS);
    return JS_TRUE;
}

/* Date.getMilliseconds is mapped to getUTCMilliseconds */

static JSBool
date_getUTCMilliseconds(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble result;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &result))
        return JS_FALSE;

    if (JSDOUBLE_IS_FINITE(result))
        result = msFromTime(result);

    vp->setNumber(result);
    return JS_TRUE;
}

static JSBool
date_getTimezoneOffset(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    jsdouble utctime, localtime, result;

    obj = ComputeThisFromVp(cx, vp);
    if (!GetUTCTime(cx, obj, vp, &utctime))
        return JS_FALSE;
    if (!GetAndCacheLocalTime(cx, obj, NULL, &localtime))
        return JS_FALSE;

    /*
     * Return the time zone offset in minutes for the current locale that is
     * appropriate for this time. This value would be a constant except for
     * daylight savings time.
     */
    result = (utctime - localtime) / msPerMinute;
    vp->setNumber(result);
    return JS_TRUE;
}

static JSBool
date_setTime(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!InstanceOf(cx, obj, &js_DateClass, vp + 2))
        return false;

    if (argc == 0) {
        SetDateToNaN(cx, obj, vp);
        return true;
    }

    jsdouble result;
    if (!ValueToNumber(cx, vp[2], &result))
        return false;

    return SetUTCTime(cx, obj, TIMECLIP(result), vp);
}

static JSBool
date_makeTime(JSContext *cx, uintN maxargs, JSBool local, uintN argc, Value *vp)
{
    JSObject *obj;
    Value *argv;
    uintN i;
    jsdouble args[4], *argp, *stop;
    jsdouble hour, min, sec, msec;
    jsdouble lorutime; /* Local or UTC version of *date */

    jsdouble msec_time;
    jsdouble result;

    obj = ComputeThisFromVp(cx, vp);
    if (!GetUTCTime(cx, obj, vp, &result))
        return false;

    /* just return NaN if the date is already NaN */
    if (!JSDOUBLE_IS_FINITE(result)) {
        vp->setNumber(result);
        return true;
    }

    /*
     * Satisfy the ECMA rule that if a function is called with
     * fewer arguments than the specified formal arguments, the
     * remaining arguments are set to undefined.  Seems like all
     * the Date.setWhatever functions in ECMA are only varargs
     * beyond the first argument; this should be set to undefined
     * if it's not given.  This means that "d = new Date();
     * d.setMilliseconds()" returns NaN.  Blech.
     */
    if (argc == 0) {
        SetDateToNaN(cx, obj, vp);
        return true;
    }
    if (argc > maxargs)
        argc = maxargs;  /* clamp argc */
    JS_ASSERT(argc <= 4);

    argv = vp + 2;
    for (i = 0; i < argc; i++) {
        if (!ValueToNumber(cx, argv[i], &args[i]))
            return false;
        if (!JSDOUBLE_IS_FINITE(args[i])) {
            SetDateToNaN(cx, obj, vp);
            return true;
        }
        args[i] = js_DoubleToInteger(args[i]);
    }

    if (local)
        lorutime = LocalTime(result, cx);
    else
        lorutime = result;

    argp = args;
    stop = argp + argc;
    if (maxargs >= 4 && argp < stop)
        hour = *argp++;
    else
        hour = HourFromTime(lorutime);

    if (maxargs >= 3 && argp < stop)
        min = *argp++;
    else
        min = MinFromTime(lorutime);

    if (maxargs >= 2 && argp < stop)
        sec = *argp++;
    else
        sec = SecFromTime(lorutime);

    if (maxargs >= 1 && argp < stop)
        msec = *argp;
    else
        msec = msFromTime(lorutime);

    msec_time = MakeTime(hour, min, sec, msec);
    result = MakeDate(Day(lorutime), msec_time);

/*     fprintf(stderr, "%f\n", result); */

    if (local)
        result = UTC(result, cx);

/*     fprintf(stderr, "%f\n", result); */

    return SetUTCTime(cx, obj, TIMECLIP(result), vp);
}

static JSBool
date_setMilliseconds(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeTime(cx, 1, JS_TRUE, argc, vp);
}

static JSBool
date_setUTCMilliseconds(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeTime(cx, 1, JS_FALSE, argc, vp);
}

static JSBool
date_setSeconds(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeTime(cx, 2, JS_TRUE, argc, vp);
}

static JSBool
date_setUTCSeconds(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeTime(cx, 2, JS_FALSE, argc, vp);
}

static JSBool
date_setMinutes(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeTime(cx, 3, JS_TRUE, argc, vp);
}

static JSBool
date_setUTCMinutes(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeTime(cx, 3, JS_FALSE, argc, vp);
}

static JSBool
date_setHours(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeTime(cx, 4, JS_TRUE, argc, vp);
}

static JSBool
date_setUTCHours(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeTime(cx, 4, JS_FALSE, argc, vp);
}

static JSBool
date_makeDate(JSContext *cx, uintN maxargs, JSBool local, uintN argc, Value *vp)
{
    JSObject *obj;
    Value *argv;
    uintN i;
    jsdouble lorutime; /* local or UTC version of *date */
    jsdouble args[3], *argp, *stop;
    jsdouble year, month, day;
    jsdouble result;

    obj = ComputeThisFromVp(cx, vp);
    if (!GetUTCTime(cx, obj, vp, &result))
        return false;

    /* see complaint about ECMA in date_MakeTime */
    if (argc == 0) {
        SetDateToNaN(cx, obj, vp);
        return true;
    }
    if (argc > maxargs)
        argc = maxargs;   /* clamp argc */
    JS_ASSERT(1 <= argc && argc <= 3);

    argv = vp + 2;
    for (i = 0; i < argc; i++) {
        if (!ValueToNumber(cx, argv[i], &args[i]))
            return JS_FALSE;
        if (!JSDOUBLE_IS_FINITE(args[i])) {
            SetDateToNaN(cx, obj, vp);
            return true;
        }
        args[i] = js_DoubleToInteger(args[i]);
    }

    /* return NaN if date is NaN and we're not setting the year,
     * If we are, use 0 as the time. */
    if (!(JSDOUBLE_IS_FINITE(result))) {
        if (maxargs < 3) {
            vp->setDouble(result);
            return true;
        }
        lorutime = +0.;
    } else {
        lorutime = local ? LocalTime(result, cx) : result;
    }

    argp = args;
    stop = argp + argc;
    if (maxargs >= 3 && argp < stop)
        year = *argp++;
    else
        year = YearFromTime(lorutime);

    if (maxargs >= 2 && argp < stop)
        month = *argp++;
    else
        month = MonthFromTime(lorutime);

    if (maxargs >= 1 && argp < stop)
        day = *argp++;
    else
        day = DateFromTime(lorutime);

    day = MakeDay(year, month, day); /* day within year */
    result = MakeDate(day, TimeWithinDay(lorutime));

    if (local)
        result = UTC(result, cx);

    return SetUTCTime(cx, obj, TIMECLIP(result), vp);
}

static JSBool
date_setDate(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeDate(cx, 1, JS_TRUE, argc, vp);
}

static JSBool
date_setUTCDate(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeDate(cx, 1, JS_FALSE, argc, vp);
}

static JSBool
date_setMonth(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeDate(cx, 2, JS_TRUE, argc, vp);
}

static JSBool
date_setUTCMonth(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeDate(cx, 2, JS_FALSE, argc, vp);
}

static JSBool
date_setFullYear(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeDate(cx, 3, JS_TRUE, argc, vp);
}

static JSBool
date_setUTCFullYear(JSContext *cx, uintN argc, Value *vp)
{
    return date_makeDate(cx, 3, JS_FALSE, argc, vp);
}

static JSBool
date_setYear(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);

    jsdouble result;
    if (!GetUTCTime(cx, obj, vp, &result))
        return false;

    if (argc == 0) {
        /* Call this only after GetUTCTime has verified that obj is Date. */
        SetDateToNaN(cx, obj, vp);
        return true;
    }

    jsdouble year;
    if (!ValueToNumber(cx, vp[2], &year))
        return false;
    if (!JSDOUBLE_IS_FINITE(year)) {
        SetDateToNaN(cx, obj, vp);
        return true;
    }
    year = js_DoubleToInteger(year);
    if (year >= 0 && year <= 99)
        year += 1900;

    jsdouble t = JSDOUBLE_IS_FINITE(result) ? LocalTime(result, cx) : +0.0;
    jsdouble day = MakeDay(year, MonthFromTime(t), DateFromTime(t));
    result = MakeDate(day, TimeWithinDay(t));
    result = UTC(result, cx);

    return SetUTCTime(cx, obj, TIMECLIP(result), vp);
}

/* constants for toString, toUTCString */
static char js_NaN_date_str[] = "Invalid Date";
static const char* days[] =
{
   "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};
static const char* months[] =
{
   "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


// Avoid dependence on PRMJ_FormatTimeUSEnglish, because it
// requires a PRMJTime... which only has 16-bit years.  Sub-ECMA.
static void
print_gmt_string(char* buf, size_t size, jsdouble utctime)
{
    JS_snprintf(buf, size, "%s, %.2d %s %.4d %.2d:%.2d:%.2d GMT",
                days[WeekDay(utctime)],
                DateFromTime(utctime),
                months[MonthFromTime(utctime)],
                YearFromTime(utctime),
                HourFromTime(utctime),
                MinFromTime(utctime),
                SecFromTime(utctime));
}

static void
print_iso_string(char* buf, size_t size, jsdouble utctime)
{
    JS_snprintf(buf, size, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d.%.3dZ",
                YearFromTime(utctime),
                MonthFromTime(utctime) + 1,
                DateFromTime(utctime),
                HourFromTime(utctime),
                MinFromTime(utctime),
                SecFromTime(utctime),
                msFromTime(utctime));
}

static JSBool
date_utc_format(JSContext *cx, Value *vp,
                void (*printFunc)(char*, size_t, jsdouble))
{
    char buf[100];
    JSString *str;
    jsdouble utctime;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &utctime))
        return JS_FALSE;

    if (!JSDOUBLE_IS_FINITE(utctime)) {
        JS_snprintf(buf, sizeof buf, js_NaN_date_str);
    } else {
        (*printFunc)(buf, sizeof buf, utctime);
    }
    str = JS_NewStringCopyZ(cx, buf);
    if (!str)
        return JS_FALSE;
    vp->setString(str);
    return JS_TRUE;
}

static JSBool
date_toGMTString(JSContext *cx, uintN argc, Value *vp)
{
    return date_utc_format(cx, vp, print_gmt_string);
}

static JSBool
date_toISOString(JSContext *cx, uintN argc, Value *vp)
{
    return date_utc_format(cx, vp, print_iso_string);
}

namespace {

/* ES5 15.9.5.44. */
JSBool
date_toJSON(JSContext *cx, uintN argc, Value *vp)
{
    /* Step 1. */
    JSObject *obj = js_ValueToNonNullObject(cx, vp[1]);
    if (!obj)
        return false;

    /* Step 2. */
    Value &tv = vp[0];
    if (!DefaultValue(cx, obj, JSTYPE_NUMBER, &tv))
        return false;

    /* Step 3. */
    if (tv.isDouble() && !JSDOUBLE_IS_FINITE(tv.toDouble())) {
        vp->setNull();
        return true;
    }

    /* Step 4. */
    Value &toISO = vp[0];
    if (!obj->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.toISOStringAtom), &toISO))
        return false;

    /* Step 5. */
    if (!js_IsCallable(toISO)) {
        JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                     JSMSG_BAD_TOISOSTRING_PROP);
        return false;
    }

    /* Step 6. */
    LeaveTrace(cx);
    InvokeArgsGuard args;
    if (!cx->stack().pushInvokeArgs(cx, 0, &args))
        return false;

    args.callee() = toISO;
    args.thisv().setObject(*obj);

    if (!Invoke(cx, args, 0))
        return false;
    *vp = args.rval();
    return true;
}

}

/* for Date.toLocaleString; interface to PRMJTime date struct.
 */
static void
new_explode(jsdouble timeval, PRMJTime *split, JSContext *cx)
{
    jsint year = YearFromTime(timeval);

    split->tm_usec = (int32) msFromTime(timeval) * 1000;
    split->tm_sec = (int8) SecFromTime(timeval);
    split->tm_min = (int8) MinFromTime(timeval);
    split->tm_hour = (int8) HourFromTime(timeval);
    split->tm_mday = (int8) DateFromTime(timeval);
    split->tm_mon = (int8) MonthFromTime(timeval);
    split->tm_wday = (int8) WeekDay(timeval);
    split->tm_year = year;
    split->tm_yday = (int16) DayWithinYear(timeval, year);

    /* not sure how this affects things, but it doesn't seem
       to matter. */
    split->tm_isdst = (DaylightSavingTA(timeval, cx) != 0);
}

typedef enum formatspec {
    FORMATSPEC_FULL, FORMATSPEC_DATE, FORMATSPEC_TIME
} formatspec;

/* helper function */
static JSBool
date_format(JSContext *cx, jsdouble date, formatspec format, Value *rval)
{
    char buf[100];
    JSString *str;
    char tzbuf[100];
    JSBool usetz;
    size_t i, tzlen;
    PRMJTime split;

    if (!JSDOUBLE_IS_FINITE(date)) {
        JS_snprintf(buf, sizeof buf, js_NaN_date_str);
    } else {
        jsdouble local = LocalTime(date, cx);

        /* offset from GMT in minutes.  The offset includes daylight savings,
           if it applies. */
        jsint minutes = (jsint) floor(AdjustTime(date, cx) / msPerMinute);

        /* map 510 minutes to 0830 hours */
        intN offset = (minutes / 60) * 100 + minutes % 60;

        /* print as "Wed Nov 05 19:38:03 GMT-0800 (PST) 1997" The TZA is
         * printed as 'GMT-0800' rather than as 'PST' to avoid
         * operating-system dependence on strftime (which
         * PRMJ_FormatTimeUSEnglish calls, for %Z only.)  win32 prints
         * PST as 'Pacific Standard Time.'  This way we always know
         * what we're getting, and can parse it if we produce it.
         * The OS TZA string is included as a comment.
         */

        /* get a timezone string from the OS to include as a
           comment. */
        new_explode(date, &split, cx);
        if (PRMJ_FormatTime(tzbuf, sizeof tzbuf, "(%Z)", &split) != 0) {

            /* Decide whether to use the resulting timezone string.
             *
             * Reject it if it contains any non-ASCII, non-alphanumeric
             * characters.  It's then likely in some other character
             * encoding, and we probably won't display it correctly.
             */
            usetz = JS_TRUE;
            tzlen = strlen(tzbuf);
            if (tzlen > 100) {
                usetz = JS_FALSE;
            } else {
                for (i = 0; i < tzlen; i++) {
                    jschar c = tzbuf[i];
                    if (c > 127 ||
                        !(isalpha(c) || isdigit(c) ||
                          c == ' ' || c == '(' || c == ')')) {
                        usetz = JS_FALSE;
                    }
                }
            }

            /* Also reject it if it's not parenthesized or if it's '()'. */
            if (tzbuf[0] != '(' || tzbuf[1] == ')')
                usetz = JS_FALSE;
        } else
            usetz = JS_FALSE;

        switch (format) {
          case FORMATSPEC_FULL:
            /*
             * Avoid dependence on PRMJ_FormatTimeUSEnglish, because it
             * requires a PRMJTime... which only has 16-bit years.  Sub-ECMA.
             */
            /* Tue Oct 31 2000 09:41:40 GMT-0800 (PST) */
            JS_snprintf(buf, sizeof buf,
                        "%s %s %.2d %.4d %.2d:%.2d:%.2d GMT%+.4d%s%s",
                        days[WeekDay(local)],
                        months[MonthFromTime(local)],
                        DateFromTime(local),
                        YearFromTime(local),
                        HourFromTime(local),
                        MinFromTime(local),
                        SecFromTime(local),
                        offset,
                        usetz ? " " : "",
                        usetz ? tzbuf : "");
            break;
          case FORMATSPEC_DATE:
            /* Tue Oct 31 2000 */
            JS_snprintf(buf, sizeof buf,
                        "%s %s %.2d %.4d",
                        days[WeekDay(local)],
                        months[MonthFromTime(local)],
                        DateFromTime(local),
                        YearFromTime(local));
            break;
          case FORMATSPEC_TIME:
            /* 09:41:40 GMT-0800 (PST) */
            JS_snprintf(buf, sizeof buf,
                        "%.2d:%.2d:%.2d GMT%+.4d%s%s",
                        HourFromTime(local),
                        MinFromTime(local),
                        SecFromTime(local),
                        offset,
                        usetz ? " " : "",
                        usetz ? tzbuf : "");
            break;
        }
    }

    str = JS_NewStringCopyZ(cx, buf);
    if (!str)
        return JS_FALSE;
    rval->setString(str);
    return JS_TRUE;
}

static JSBool
date_toLocaleHelper(JSContext *cx, const char *format, Value *vp)
{
    JSObject *obj;
    char buf[100];
    JSString *str;
    PRMJTime split;
    jsdouble utctime;

    obj = ComputeThisFromVp(cx, vp);
    if (!GetUTCTime(cx, obj, vp, &utctime))
        return JS_FALSE;

    if (!JSDOUBLE_IS_FINITE(utctime)) {
        JS_snprintf(buf, sizeof buf, js_NaN_date_str);
    } else {
        intN result_len;
        jsdouble local = LocalTime(utctime, cx);
        new_explode(local, &split, cx);

        /* let PRMJTime format it.       */
        result_len = PRMJ_FormatTime(buf, sizeof buf, format, &split);

        /* If it failed, default to toString. */
        if (result_len == 0)
            return date_format(cx, utctime, FORMATSPEC_FULL, vp);

        /* Hacked check against undesired 2-digit year 00/00/00 form. */
        if (strcmp(format, "%x") == 0 && result_len >= 6 &&
            /* Format %x means use OS settings, which may have 2-digit yr, so
               hack end of 3/11/22 or 11.03.22 or 11Mar22 to use 4-digit yr...*/
            !isdigit(buf[result_len - 3]) &&
            isdigit(buf[result_len - 2]) && isdigit(buf[result_len - 1]) &&
            /* ...but not if starts with 4-digit year, like 2022/3/11. */
            !(isdigit(buf[0]) && isdigit(buf[1]) &&
              isdigit(buf[2]) && isdigit(buf[3]))) {
            JS_snprintf(buf + (result_len - 2), (sizeof buf) - (result_len - 2),
                        "%d", js_DateGetYear(cx, obj));
        }

    }

    if (cx->localeCallbacks && cx->localeCallbacks->localeToUnicode)
        return cx->localeCallbacks->localeToUnicode(cx, buf, Jsvalify(vp));

    str = JS_NewStringCopyZ(cx, buf);
    if (!str)
        return JS_FALSE;
    vp->setString(str);
    return JS_TRUE;
}

static JSBool
date_toLocaleString(JSContext *cx, uintN argc, Value *vp)
{
    /* Use '%#c' for windows, because '%c' is
     * backward-compatible and non-y2k with msvc; '%#c' requests that a
     * full year be used in the result string.
     */
    return date_toLocaleHelper(cx,
#if defined(_WIN32) && !defined(__MWERKS__)
                                   "%#c"
#else
                                   "%c"
#endif
                               , vp);
}

static JSBool
date_toLocaleDateString(JSContext *cx, uintN argc, Value *vp)
{
    /* Use '%#x' for windows, because '%x' is
     * backward-compatible and non-y2k with msvc; '%#x' requests that a
     * full year be used in the result string.
     */
    return date_toLocaleHelper(cx,
#if defined(_WIN32) && !defined(__MWERKS__)
                                   "%#x"
#else
                                   "%x"
#endif
                               , vp);
}

static JSBool
date_toLocaleTimeString(JSContext *cx, uintN argc, Value *vp)
{
    return date_toLocaleHelper(cx, "%X", vp);
}

static JSBool
date_toLocaleFormat(JSContext *cx, uintN argc, Value *vp)
{
    JSString *fmt;
    const char *fmtbytes;

    if (argc == 0)
        return date_toLocaleString(cx, argc, vp);

    fmt = js_ValueToString(cx, vp[2]);
    if (!fmt)
        return JS_FALSE;
    vp[2].setString(fmt);
    fmtbytes = js_GetStringBytes(cx, fmt);
    if (!fmtbytes)
        return JS_FALSE;

    return date_toLocaleHelper(cx, fmtbytes, vp);
}

static JSBool
date_toTimeString(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble utctime;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &utctime))
        return JS_FALSE;
    return date_format(cx, utctime, FORMATSPEC_TIME, vp);
}

static JSBool
date_toDateString(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble utctime;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &utctime))
        return JS_FALSE;
    return date_format(cx, utctime, FORMATSPEC_DATE, vp);
}

#if JS_HAS_TOSOURCE
#include <string.h>
#include "jsnum.h"

static JSBool
date_toSource(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble utctime;
    char *numStr, *bytes;
    JSString *str;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &utctime))
        return JS_FALSE;

    ToCStringBuf cbuf;
    numStr = NumberToCString(cx, &cbuf, utctime);
    if (!numStr) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }

    bytes = JS_smprintf("(new %s(%s))", js_Date_str, numStr);
    if (!bytes) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }

    str = JS_NewString(cx, bytes, strlen(bytes));
    if (!str) {
        js_free(bytes);
        return JS_FALSE;
    }
    vp->setString(str);
    return JS_TRUE;
}
#endif

static JSBool
date_toString(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble utctime;

    if (!GetUTCTime(cx, ComputeThisFromVp(cx, vp), vp, &utctime))
        return JS_FALSE;
    return date_format(cx, utctime, FORMATSPEC_FULL, vp);
}

static JSBool
date_valueOf(JSContext *cx, uintN argc, Value *vp)
{
    JSString *str, *number_str;

    /* It is an error to call date_valueOf on a non-date object, but we don't
     * need to check for that explicitly here because every path calls
     * GetUTCTime, which does the check.
     */

    /* If called directly with no arguments, convert to a time number. */
    if (argc == 0)
        return date_getTime(cx, argc, vp);

    /* Convert to number only if the hint was given, otherwise favor string. */
    str = js_ValueToString(cx, vp[2]);
    if (!str)
        return JS_FALSE;
    number_str = ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[JSTYPE_NUMBER]);
    if (js_EqualStrings(str, number_str))
        return date_getTime(cx, argc, vp);
    return date_toString(cx, argc, vp);
}

// Don't really need an argument here, but we don't support arg-less builtins
JS_DEFINE_TRCINFO_1(date_now,
    (1, (static, DOUBLE, date_now_tn, CONTEXT, 0, nanojit::ACCSET_STORE_ANY)))

static JSFunctionSpec date_static_methods[] = {
    JS_FN("UTC",                 date_UTC,                MAXARGS,0),
    JS_FN("parse",               date_parse,              1,0),
    JS_TN("now",                 date_now,                0,0, &date_now_trcinfo),
    JS_FS_END
};

static JSFunctionSpec date_methods[] = {
    JS_FN("getTime",             date_getTime,            0,0),
    JS_FN("getTimezoneOffset",   date_getTimezoneOffset,  0,0),
    JS_FN("getYear",             date_getYear,            0,0),
    JS_FN("getFullYear",         date_getFullYear,        0,0),
    JS_FN("getUTCFullYear",      date_getUTCFullYear,     0,0),
    JS_FN("getMonth",            date_getMonth,           0,0),
    JS_FN("getUTCMonth",         date_getUTCMonth,        0,0),
    JS_FN("getDate",             date_getDate,            0,0),
    JS_FN("getUTCDate",          date_getUTCDate,         0,0),
    JS_FN("getDay",              date_getDay,             0,0),
    JS_FN("getUTCDay",           date_getUTCDay,          0,0),
    JS_FN("getHours",            date_getHours,           0,0),
    JS_FN("getUTCHours",         date_getUTCHours,        0,0),
    JS_FN("getMinutes",          date_getMinutes,         0,0),
    JS_FN("getUTCMinutes",       date_getUTCMinutes,      0,0),
    JS_FN("getSeconds",          date_getUTCSeconds,      0,0),
    JS_FN("getUTCSeconds",       date_getUTCSeconds,      0,0),
    JS_FN("getMilliseconds",     date_getUTCMilliseconds, 0,0),
    JS_FN("getUTCMilliseconds",  date_getUTCMilliseconds, 0,0),
    JS_FN("setTime",             date_setTime,            1,0),
    JS_FN("setYear",             date_setYear,            1,0),
    JS_FN("setFullYear",         date_setFullYear,        3,0),
    JS_FN("setUTCFullYear",      date_setUTCFullYear,     3,0),
    JS_FN("setMonth",            date_setMonth,           2,0),
    JS_FN("setUTCMonth",         date_setUTCMonth,        2,0),
    JS_FN("setDate",             date_setDate,            1,0),
    JS_FN("setUTCDate",          date_setUTCDate,         1,0),
    JS_FN("setHours",            date_setHours,           4,0),
    JS_FN("setUTCHours",         date_setUTCHours,        4,0),
    JS_FN("setMinutes",          date_setMinutes,         3,0),
    JS_FN("setUTCMinutes",       date_setUTCMinutes,      3,0),
    JS_FN("setSeconds",          date_setSeconds,         2,0),
    JS_FN("setUTCSeconds",       date_setUTCSeconds,      2,0),
    JS_FN("setMilliseconds",     date_setMilliseconds,    1,0),
    JS_FN("setUTCMilliseconds",  date_setUTCMilliseconds, 1,0),
    JS_FN("toUTCString",         date_toGMTString,        0,0),
    JS_FN(js_toLocaleString_str, date_toLocaleString,     0,0),
    JS_FN("toLocaleDateString",  date_toLocaleDateString, 0,0),
    JS_FN("toLocaleTimeString",  date_toLocaleTimeString, 0,0),
    JS_FN("toLocaleFormat",      date_toLocaleFormat,     0,0),
    JS_FN("toDateString",        date_toDateString,       0,0),
    JS_FN("toTimeString",        date_toTimeString,       0,0),
    JS_FN("toISOString",         date_toISOString,        0,0),
    JS_FN(js_toJSON_str,         date_toJSON,             1,0),
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,       date_toSource,           0,0),
#endif
    JS_FN(js_toString_str,       date_toString,           0,0),
    JS_FN(js_valueOf_str,        date_valueOf,            0,0),
    JS_FS_END
};

JSBool
js_Date(JSContext *cx, uintN argc, Value *vp)
{
    /* Date called as function. */
    if (!IsConstructing(vp))
        return date_format(cx, NowAsMillis(), FORMATSPEC_FULL, vp);

    Value *argv = vp + 2;

    /* Date called as constructor. */
    jsdouble d;
    if (argc == 0) {
        d = NowAsMillis();
    } else if (argc == 1) {
        if (!argv[0].isString()) {
            /* the argument is a millisecond number */
            if (!ValueToNumber(cx, argv[0], &d))
                return false;
            d = TIMECLIP(d);
        } else {
            /* the argument is a string; parse it. */
            JSString *str = js_ValueToString(cx, argv[0]);
            if (!str)
                return false;
            argv[0].setString(str);

            if (!date_parseString(str, &d, cx))
                d = js_NaN;
            else
                d = TIMECLIP(d);
        }
    } else {
        jsdouble msec_time;
        if (!date_msecFromArgs(cx, argc, argv, &msec_time))
            return false;

        if (JSDOUBLE_IS_FINITE(msec_time)) {
            msec_time = UTC(msec_time, cx);
            msec_time = TIMECLIP(msec_time);
        }
        d = msec_time;
    }

    JSObject *obj = js_NewDateObjectMsec(cx, d);
    if (!obj)
        return false;
    vp->setObject(*obj);

    return true;
}

JSObject *
js_InitDateClass(JSContext *cx, JSObject *obj)
{
    /* set static LocalTZA */
    LocalTZA = -(PRMJ_LocalGMTDifference() * msPerSecond);
    JSObject *proto = js_InitClass(cx, obj, NULL, &js_DateClass, js_Date, MAXARGS,
                                   NULL, date_methods, NULL, date_static_methods);
    if (!proto)
        return NULL;

    AutoObjectRooter tvr(cx, proto);

    SetDateToNaN(cx, proto);

    /*
     * ES5 B.2.6:
     *   The Function object that is the initial value of
     *   Date.prototype.toGMTString is the same Function
     *   object that is the initial value of
     *   Date.prototype.toUTCString.
     */
    AutoValueRooter toUTCStringFun(cx);
    jsid toUTCStringId = ATOM_TO_JSID(cx->runtime->atomState.toUTCStringAtom);
    jsid toGMTStringId = ATOM_TO_JSID(cx->runtime->atomState.toGMTStringAtom);
    if (!js_GetProperty(cx, proto, toUTCStringId, toUTCStringFun.addr()) ||
        !js_DefineProperty(cx, proto, toGMTStringId, toUTCStringFun.addr(),
                           PropertyStub, PropertyStub, 0)) {
        return NULL;
    }

    return proto;
}

JS_FRIEND_API(JSObject *)
js_NewDateObjectMsec(JSContext *cx, jsdouble msec_time)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &js_DateClass);
    if (!obj || !SetUTCTime(cx, obj, msec_time))
        return NULL;
    return obj;
}

JS_FRIEND_API(JSObject *)
js_NewDateObject(JSContext* cx, int year, int mon, int mday,
                 int hour, int min, int sec)
{
    JSObject *obj;
    jsdouble msec_time;

    JS_ASSERT(mon < 12);
    msec_time = date_msecFromDate(year, mon, mday, hour, min, sec, 0);
    obj = js_NewDateObjectMsec(cx, UTC(msec_time, cx));
    return obj;
}

JS_FRIEND_API(JSBool)
js_DateIsValid(JSContext *cx, JSObject* obj)
{
    jsdouble utctime;
    return GetUTCTime(cx, obj, NULL, &utctime) && !JSDOUBLE_IS_NaN(utctime);
}

JS_FRIEND_API(int)
js_DateGetYear(JSContext *cx, JSObject* obj)
{
    jsdouble localtime;

    /* Preserve legacy API behavior of returning 0 for invalid dates. */
    if (!GetAndCacheLocalTime(cx, obj, NULL, &localtime) ||
        JSDOUBLE_IS_NaN(localtime)) {
        return 0;
    }

    return (int) YearFromTime(localtime);
}

JS_FRIEND_API(int)
js_DateGetMonth(JSContext *cx, JSObject* obj)
{
    jsdouble localtime;

    if (!GetAndCacheLocalTime(cx, obj, NULL, &localtime) ||
        JSDOUBLE_IS_NaN(localtime)) {
        return 0;
    }

    return (int) MonthFromTime(localtime);
}

JS_FRIEND_API(int)
js_DateGetDate(JSContext *cx, JSObject* obj)
{
    jsdouble localtime;

    if (!GetAndCacheLocalTime(cx, obj, NULL, &localtime) ||
        JSDOUBLE_IS_NaN(localtime)) {
        return 0;
    }

    return (int) DateFromTime(localtime);
}

JS_FRIEND_API(int)
js_DateGetHours(JSContext *cx, JSObject* obj)
{
    jsdouble localtime;

    if (!GetAndCacheLocalTime(cx, obj, NULL, &localtime) ||
        JSDOUBLE_IS_NaN(localtime)) {
        return 0;
    }

    return (int) HourFromTime(localtime);
}

JS_FRIEND_API(int)
js_DateGetMinutes(JSContext *cx, JSObject* obj)
{
    jsdouble localtime;

    if (!GetAndCacheLocalTime(cx, obj, NULL, &localtime) ||
        JSDOUBLE_IS_NaN(localtime)) {
        return 0;
    }

    return (int) MinFromTime(localtime);
}

JS_FRIEND_API(int)
js_DateGetSeconds(JSContext *cx, JSObject* obj)
{
    jsdouble utctime;

    if (!GetUTCTime(cx, obj, NULL, &utctime) || JSDOUBLE_IS_NaN(utctime))
        return 0;

    return (int) SecFromTime(utctime);
}

JS_FRIEND_API(void)
js_DateSetYear(JSContext *cx, JSObject *obj, int year)
{
    jsdouble local;

    if (!GetAndCacheLocalTime(cx, obj, NULL, &local))
        return;

    /* reset date if it was NaN */
    if (JSDOUBLE_IS_NaN(local))
        local = 0;

    local = date_msecFromDate(year,
                              MonthFromTime(local),
                              DateFromTime(local),
                              HourFromTime(local),
                              MinFromTime(local),
                              SecFromTime(local),
                              msFromTime(local));

    /* SetUTCTime also invalidates local time cache. */
    SetUTCTime(cx, obj, UTC(local, cx));
}

JS_FRIEND_API(void)
js_DateSetMonth(JSContext *cx, JSObject *obj, int month)
{
    jsdouble local;

    JS_ASSERT(month < 12);

    if (!GetAndCacheLocalTime(cx, obj, NULL, &local))
        return;

    /* bail if date was NaN */
    if (JSDOUBLE_IS_NaN(local))
        return;

    local = date_msecFromDate(YearFromTime(local),
                              month,
                              DateFromTime(local),
                              HourFromTime(local),
                              MinFromTime(local),
                              SecFromTime(local),
                              msFromTime(local));
    SetUTCTime(cx, obj, UTC(local, cx));
}

JS_FRIEND_API(void)
js_DateSetDate(JSContext *cx, JSObject *obj, int date)
{
    jsdouble local;

    if (!GetAndCacheLocalTime(cx, obj, NULL, &local))
        return;

    if (JSDOUBLE_IS_NaN(local))
        return;

    local = date_msecFromDate(YearFromTime(local),
                              MonthFromTime(local),
                              date,
                              HourFromTime(local),
                              MinFromTime(local),
                              SecFromTime(local),
                              msFromTime(local));
    SetUTCTime(cx, obj, UTC(local, cx));
}

JS_FRIEND_API(void)
js_DateSetHours(JSContext *cx, JSObject *obj, int hours)
{
    jsdouble local;

    if (!GetAndCacheLocalTime(cx, obj, NULL, &local))
        return;

    if (JSDOUBLE_IS_NaN(local))
        return;
    local = date_msecFromDate(YearFromTime(local),
                              MonthFromTime(local),
                              DateFromTime(local),
                              hours,
                              MinFromTime(local),
                              SecFromTime(local),
                              msFromTime(local));
    SetUTCTime(cx, obj, UTC(local, cx));
}

JS_FRIEND_API(void)
js_DateSetMinutes(JSContext *cx, JSObject *obj, int minutes)
{
    jsdouble local;

    if (!GetAndCacheLocalTime(cx, obj, NULL, &local))
        return;

    if (JSDOUBLE_IS_NaN(local))
        return;
    local = date_msecFromDate(YearFromTime(local),
                              MonthFromTime(local),
                              DateFromTime(local),
                              HourFromTime(local),
                              minutes,
                              SecFromTime(local),
                              msFromTime(local));
    SetUTCTime(cx, obj, UTC(local, cx));
}

JS_FRIEND_API(void)
js_DateSetSeconds(JSContext *cx, JSObject *obj, int seconds)
{
    jsdouble local;

    if (!GetAndCacheLocalTime(cx, obj, NULL, &local))
        return;

    if (JSDOUBLE_IS_NaN(local))
        return;
    local = date_msecFromDate(YearFromTime(local),
                              MonthFromTime(local),
                              DateFromTime(local),
                              HourFromTime(local),
                              MinFromTime(local),
                              seconds,
                              msFromTime(local));
    SetUTCTime(cx, obj, UTC(local, cx));
}

JS_FRIEND_API(jsdouble)
js_DateGetMsecSinceEpoch(JSContext *cx, JSObject *obj)
{
    jsdouble utctime;
    if (!GetUTCTime(cx, obj, NULL, &utctime))
        return 0;
    return utctime;
}

#ifdef JS_THREADSAFE
#include "prinrval.h"

JS_FRIEND_API(uint32)
js_IntervalNow()
{
    return uint32(PR_IntervalToMilliseconds(PR_IntervalNow()));
}

#else /* !JS_THREADSAFE */

JS_FRIEND_API(uint32)
js_IntervalNow()
{
    return uint32(PRMJ_Now() / PRMJ_USEC_PER_MSEC);
}
#endif
