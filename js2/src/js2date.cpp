/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
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

#ifdef _WIN32
#include "msvc_pragma.h"
#endif

#include <algorithm>
#include <list>
#include <map>
#include <stack>

#include <assert.h>
#include <ctype.h>

#include "world.h"
#include "utilities.h"
#include "js2value.h"
#include "numerics.h"
#include "reader.h"
#include "parser.h"
#include "regexp.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"
#include "jslong.h"
#include "fdlibm_ns.h"
#include "prmjtime.h"

namespace JavaScript {    
namespace MetaData {

#define HalfTimeDomain  8.64e15
#define HoursPerDay     24.0
#define MinutesPerDay   (HoursPerDay * MinutesPerHour)
#define MinutesPerHour  60.0
#define SecondsPerDay   (MinutesPerDay * SecondsPerMinute)
#define SecondsPerHour  (MinutesPerHour * SecondsPerMinute)
#define SecondsPerMinute 60.0
#ifdef XP_PC
/* Work around msvc double optimization bug by making these runtime values; if
 * they're available at compile time, msvc optimizes division by them by
 * computing the reciprocal and multiplying instead of dividing - this loses
 * when the reciprocal isn't representable in a double.
 */
static float64 msPerSecond = 1000.0;
static float64 msPerDay = SecondsPerDay * 1000.0;
static float64 msPerHour = SecondsPerHour * 1000.0;
static float64 msPerMinute = SecondsPerMinute * 1000.0;
#else
#define msPerDay        (SecondsPerDay * msPerSecond)
#define msPerHour       (SecondsPerHour * msPerSecond)
#define msPerMinute     (SecondsPerMinute * msPerSecond)
#define msPerSecond     1000.0
#endif

/* LocalTZA gets set by initDateObject() */
static float64          LocalTZA;
/*
 * The following array contains the day of year for the first day of
 * each month, where index 0 is January, and day 0 is January 1.
 */
static float64 firstDayOfMonth[2][12] = {
    {0.0, 31.0, 59.0, 90.0, 120.0, 151.0, 181.0, 212.0, 243.0, 273.0, 304.0, 334.0},
    {0.0, 31.0, 60.0, 91.0, 121.0, 152.0, 182.0, 213.0, 244.0, 274.0, 305.0, 335.0}
};


#define MakeTime(hour, min, sec, ms)    (((hour * MinutesPerHour + min) * SecondsPerMinute + sec) * msPerSecond + ms)
#define MakeDate(day, time)             (day * msPerDay + time)
#define Day(t)                          floor((t) / msPerDay)
#define TIMECLIP(d)                     ((JSDOUBLE_IS_FINITE(d) \
		                            && !((d < 0 ? -d : d) > HalfTimeDomain)) \
                                                ? JS2Engine::float64toInt32(d + (+0.)) : nan)
#define LocalTime(t)                    ((t) + LocalTZA + DaylightSavingTA(t))
#define DayFromMonth(m, leap)           firstDayOfMonth[leap][(int32)m];
#define DayFromYear(y)                  (365 * ((y)-1970) + fd::floor(((y)-1969)/4.0)            \
                                            - fd::floor(((y)-1901)/100.0) + fd::floor(((y)-1601)/400.0))
#define TimeFromYear(y)                 (DayFromYear(y) * msPerDay)
#define DaysInYear(y)                   ((y) % 4 == 0 && ((y) % 100 || ((y) % 400 == 0)) ? 366 : 365)
#define InLeapYear(t)                   (bool)(DaysInYear(YearFromTime(t)) == 366)
#define DayWithinYear(t, year)          ((int32)(Day(t) - DayFromYear(year)))


typedef enum formatspec {
    FORMATSPEC_FULL, FORMATSPEC_DATE, FORMATSPEC_TIME
} formatspec;


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


static float64 DaylightSavingTA(float64 t)
{
    volatile int64 PR_t;
    int64 ms2us;
    int64 offset;
    float64 result;

    /* abort if NaN */
    if (JSDOUBLE_IS_NaN(t))
	return t;

    /* put our t in an LL, and map it to usec for prtime */
    JSLL_D2L(PR_t, t);
    JSLL_I2L(ms2us, PRMJ_USEC_PER_MSEC);
    JSLL_MUL(PR_t, PR_t, ms2us);

    offset = PRMJ_DSTOffset(PR_t);
    JSLL_DIV(offset, offset, ms2us);
    JSLL_L2D(result, offset);

    return result;
}

static float64 UTC(float64 t)
{
    return t - LocalTZA - DaylightSavingTA(t - LocalTZA);
}

static float64 MakeDay(float64 year, float64 month, float64 date)
{
    float64 result;
    bool leap;
    float64 yearday;
    float64 monthday;

    year += fd::floor(month / 12);

    month = fd::fmod(month, 12.0);
    if (month < 0)
	month += 12;

    leap = (DaysInYear((int32) year) == 366);

    yearday = floor(TimeFromYear(year) / msPerDay);
    monthday = DayFromMonth(month, leap);

    result = yearday
	     + monthday
	     + date - 1;
    return result;
}

static float64 TimeWithinDay(float64 t)
{
    float64 result;
    result = fd::fmod(t, msPerDay);
    if (result < 0)
	result += msPerDay;
    return result;
}

/* math here has to be f.p, because we need
 *  floor((1968 - 1969) / 4) == -1
 */

static int32 YearFromTime(float64 t)
{
    int32 y = (int32) floor(t /(msPerDay*365.2425)) + 1970;
    float64 t2 = (float64) TimeFromYear(y);

    if (t2 > t) {
        y--;
    } else {
        if (t2 + msPerDay * DaysInYear(y) <= t)
            y++;
    }
    return y;
}

static int32 MonthFromTime(float64 t)
{
    int32 d, step;
    int32 year = YearFromTime(t);
    d = DayWithinYear(t, year);

    if (d < (step = 31))
	return 0;
    step += (InLeapYear(t) ? 29 : 28);
    if (d < step)
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

static int32 DateFromTime(float64 t)
{
    int32 d, step, next;
    int32 year = YearFromTime(t);
    d = DayWithinYear(t, year);

    if (d <= (next = 30))
	return d + 1;
    step = next;
    next += (InLeapYear(t) ? 29 : 28);
    if (d <= next)
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

static int32 WeekDay(float64 t)
{
    int32 result;
    result = (int32) Day(t) + 4;
    result = result % 7;
    if (result < 0)
	result += 7;
    return result;
}

static float64 *Date_getProlog(JS2Metadata *meta, const js2val thisValue)
{
    if (meta->objectType(thisValue) != meta->dateClass)
        meta->reportError(Exception::typeError, "You really need a date", meta->engine->errorPos());
    DateInstance *dateInst = checked_cast<DateInstance *>(JS2VAL_TO_OBJECT(thisValue));
    return &dateInst->ms;
}

/* for use by date_parse */
static char* wtb[] = {
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



static int32 HourFromTime(float64 t)
{
    int32 result = (int32)fd::fmod(floor(t/msPerHour), HoursPerDay);
    if (result < 0)
	result += (int32)HoursPerDay;
    return result;
}

static int32 MinFromTime(float64 t)
{
    int32 result = (int32)fd::fmod(floor(t / msPerMinute), MinutesPerHour);
    if (result < 0)
	result += (int32)MinutesPerHour;
    return result;
}

static int32 SecFromTime(float64 t)
{
    int32 result = (int32)fd::fmod(floor(t / msPerSecond), SecondsPerMinute);
    if (result < 0)
	result += (int32)SecondsPerMinute;
    return result;
}

static int32 msFromTime(float64 t)
{
    int32 result = (int32)fd::fmod(t, msPerSecond);
    if (result < 0)
	result += (int32)msPerSecond;
    return result;
}

static js2val Date_makeTime(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc, uint32 maxargs, bool local)
{
    uint32 i;
    float64 args[4], *argp, *stop;
    float64 hour, min, sec, msec;
    float64 lorutime; /* Local or UTC version of *date */

    float64 msec_time;
    float64 result;

    float64 *date = Date_getProlog(meta, thisValue);

    result = *date;

    /* just return NaN if the date is already NaN */
    if (JSDOUBLE_IS_NaN(result))
	return meta->engine->nanValue;

    if (argc == 0) {
        *date = nan;
        return meta->engine->nanValue;
    }
    else if (argc > maxargs)
	argc = maxargs;  /* clamp argc */

    for (i = 0; i < argc; i++) {
        float64 f = meta->toFloat64(argv[i]);        
        if (JSDOUBLE_IS_NaN(f)) {
            *date = nan;
            return meta->engine->nanValue;
        }
        args[i] = (float64)(JS2Engine::float64toInt32(f));
    }

    if (local)
	lorutime = LocalTime(result);
    else
	lorutime = result;

    argp = args;
    stop = argp + argc;
    if ((maxargs >= 4) && (argp < stop))
	hour = *argp++;
    else
	hour = HourFromTime(lorutime);

    if ((maxargs >= 3) && (argp < stop))
	min = *argp++;
    else
	min = MinFromTime(lorutime);

    if ((maxargs >= 2) && (argp < stop))
	sec = *argp++;
    else
	sec = SecFromTime(lorutime);

    if ((maxargs >= 1) && (argp < stop))
	msec = *argp;
    else
	msec = msFromTime(lorutime);

    msec_time = MakeTime(hour, min, sec, msec);
    result = MakeDate(Day(lorutime), msec_time);

    if (local)
	result = UTC(result);

    *date = TIMECLIP(result);
    return meta->engine->allocNumber(*date);
}
    
static js2val Date_makeDate(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc, uint32 maxargs, bool local)
{
    uint32 i;
    float64 lorutime; /* local or UTC version of *date */
    float64 args[3], *argp, *stop;
    float64 year, month, day;
    float64 result;

    float64 *date = Date_getProlog(meta, thisValue);

    result = *date;

    /* see complaint about ECMA in date_MakeTime */
    if (argc == 0) {
        *date = nan;
        return meta->engine->nanValue;
    }
    else if (argc > maxargs)
	argc = maxargs;   /* clamp argc */

    for (i = 0; i < argc; i++) {
        float64 f = meta->toFloat64(argv[i]);      
        if (JSDOUBLE_IS_NaN(f)) {
            *date = nan;
            return meta->engine->nanValue;
        }
        args[i] = (float64)(JS2Engine::float64toInt32(f));
    }

    /* return NaN if date is NaN and we're not setting the year,
     * If we are, use 0 as the time. */
    if (!(JSDOUBLE_IS_FINITE(result))) {
	if (argc < 3)
	    return meta->engine->nanValue;
	else
	    lorutime = +0.;
    } else {
	if (local)
	    lorutime = LocalTime(result);
	else
	    lorutime = result;
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
	result = UTC(result);

    *date = TIMECLIP(result);
    return meta->engine->allocNumber(*date);
}

/* find UTC time from given date... no 1900 correction! */
static float64 date_msecFromDate(float64 year, float64 mon, float64 mday, float64 hour, float64 min, float64 sec, float64 msec)
{
    float64 day;
    float64 msec_time;
    float64 result;

    day = MakeDay(year, mon, mday);
    msec_time = MakeTime(hour, min, sec, msec);
    result = MakeDate(day, msec_time);
    return result;
}

static float64 date_parseString(const String &s)
{
    float64 msec;

    int year = -1;
    int mon = -1;
    int mday = -1;
    int hour = -1;
    int min = -1;
    int sec = -1;
    int c = -1;
    uint32 i = 0;
    int n = -1;
    float64 tzoffset = -1;  /* was an int, overflowed on win16!!! */
    int prevc = 0;
    uint32 limit = 0;
    bool seenplusminus = false;

    limit = s.length();
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
		seenplusminus = true;

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
	    } else if (n >= 70  ||
		       (prevc == '/' && mon >= 0 && mday >= 0 && year < 0)) {
		if (year >= 0)
		    goto syntax;
		else if (c <= ' ' || c == ',' || c == '/' || i >= limit)
		    year = n < 100 ? n + 1900 : n;
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
		if (mon < 0)
		    mon = /*byte*/ n-1;
		else if (mday < 0)
		    mday = /*byte*/ n;
		else
		    goto syntax;
	    } else if (i < limit && c != ',' && c > ' ' && c != '-') {
		goto syntax;
	    } else if (seenplusminus && n < 60) {  /* handle GMT-3:30 */
		if (tzoffset < 0)
		    tzoffset -= n;
		else
		    tzoffset += n;
	    } else if (hour >= 0 && min < 0) {
		min = /*byte*/ n;
	    } else if (min >= 0 && sec < 0) {
		sec = /*byte*/ n;
	    } else if (mday < 0) {
		mday = /*byte*/ n;
	    } else {
		goto syntax;
	    }
	    prevc = 0;
	} else if (c == '/' || c == ':' || c == '+' || c == '-') {
	    prevc = c;
	} else {
	    uint32 st = i - 1;
	    int k;
	    while (i < limit) {
		c = s[i];
		if (!(('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z')))
		    break;
		i++;
	    }
	    if (i <= st + 1)
		goto syntax;
	    for (k = (sizeof(wtb)/sizeof(char*)); --k >= 0;)
		if (regionMatches(wtb[k], s, st, i-st, 1)) {
		    int action = ttb[k];
		    if (action != 0) {
                        if (action < 0) {
                            /*
                             * AM/PM. Count 12:30 AM as 00:30, 12:30 PM as
                             * 12:30, instead of blindly adding 12 if PM.
                             */
                            ASSERT(action == -1 || action == -2);
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
			    if (mon < 0) {
				mon = /*byte*/ (action - 2);
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
    if (sec < 0)
	sec = 0;
    if (min < 0)
	min = 0;
    if (hour < 0)
	hour = 0;
    if (tzoffset == -1) { /* no time zone specified, have to use local */
	float64 msec_time;
	msec_time = date_msecFromDate(year, mon, mday, hour, min, sec, 0);

	return UTC(msec_time);
    }

    msec = date_msecFromDate(year, mon, mday, hour, min, sec, 0);
    msec += tzoffset * msPerMinute;
    return msec;

syntax:
    /* syntax error */
    return nan;
}


/* for Date.toLocaleString; interface to PRMJTime date struct.
 * If findEquivalent is true, then try to map the year to an equivalent year
 * that's in range.
 */
static void new_explode(float64 timeval, PRMJTime *split, bool findEquivalent)
{
    int32 year = YearFromTime(timeval);
    int16 adjustedYear;

    /* If the year doesn't fit in a PRMJTime, find something to do about it. */
    if (year > 32767 || year < -32768) {
	if (findEquivalent) {
	    /* We're really just trying to get a timezone string; map the year
	     * to some equivalent year in the range 0 to 2800.  Borrowed from
	     * A. D. Olsen.
	     */
	    int32 cycles;
#define CYCLE_YEARS 2800L
	    cycles = (year >= 0) ? year / CYCLE_YEARS
				 : -1 - (-1 - year) / CYCLE_YEARS;
	    adjustedYear = (int16)(year - cycles * CYCLE_YEARS);
	} else {
	    /* Clamp it to the nearest representable year. */
	    adjustedYear = (int16)((year > 0) ? 32767 : - 32768);
	}
    } else {
	adjustedYear = (int16)year;
    }

    split->tm_usec = (uint32) msFromTime(timeval) * 1000;
    split->tm_sec = (uint8) SecFromTime(timeval);
    split->tm_min = (uint8) MinFromTime(timeval);
    split->tm_hour = (uint8) HourFromTime(timeval);
    split->tm_mday = (uint8) DateFromTime(timeval);
    split->tm_mon = (uint8) MonthFromTime(timeval);
    split->tm_wday = (uint8) WeekDay(timeval);
    split->tm_year = (uint16) adjustedYear;
    split->tm_yday = (uint16) DayWithinYear(timeval, year);

    /* not sure how this affects things, but it doesn't seem
       to matter. */
    split->tm_isdst = (DaylightSavingTA(timeval) != 0);
}

/* helper function */
static js2val Date_format(JS2Metadata *meta, float64 date, formatspec format)
{
    StringFormatter outf;
    char tzbuf[100];
    bool usetz;
    size_t i, tzlen, bytesStored;
    PRMJTime split;

    if (!JSDOUBLE_IS_FINITE(date)) {
        outf << js_NaN_date_str;
    } else {
        float64 local = LocalTime(date);

        /* offset from GMT in minutes.  The offset includes daylight savings,
           if it applies. */
        int32 minutes = (int32)fd::floor((LocalTZA + DaylightSavingTA(date)) / msPerMinute);

        /* map 510 minutes to 0830 hours */
        int32 offset = (minutes / 60) * 100 + minutes % 60;

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
        new_explode(date, &split, true);
        bytesStored = PRMJ_FormatTime(tzbuf, sizeof tzbuf, "(%Z)", &split);

        /* Decide whether to use the resulting timezone string.
         *
         * Reject it if it contains any non-ASCII, non-alphanumeric characters.
         * It's then likely in some other character encoding, and we probably
         * won't display it correctly.
         */
        if (bytesStored > 0) {
            usetz = true;
            tzlen = strlen(tzbuf);
            if (tzlen > 100) {
                usetz = false;
            } else {
                for (i = 0; i < tzlen; i++) {
                    int16 c = tzbuf[i];
                    if (c > 127 ||
                        !(isalpha(c) || isdigit(c) ||
                          c == ' ' || c == '(' || c == ')')) {
                        usetz = false;
                    }
                }
            }

            /* Also reject it if it's not parenthesized or if it's '()'. */
            if (tzbuf[0] != '(' || tzbuf[1] == ')')
                usetz = false;
        }
        else
            usetz = false;

        switch (format) {
          case FORMATSPEC_FULL:
            /*
             * Avoid dependence on PRMJ_FormatTimeUSEnglish, because it
             * requires a PRMJTime... which only has 16-bit years.  Sub-ECMA.
             */
            /* Tue Oct 31 09:41:40 GMT-0800 (PST) 2000 */
            printFormat(outf, "%s %s %.2d %.2d:%.2d:%.2d GMT%+.4d %s%s%.4d",
                        days[WeekDay(local)],
                        months[MonthFromTime(local)],
                        DateFromTime(local),
                        HourFromTime(local),
                        MinFromTime(local),
                        SecFromTime(local),
                        offset,
                        usetz ? tzbuf : "",
                        usetz ? " " : "",
                        YearFromTime(local));
            break;
          case FORMATSPEC_DATE:
            /* Tue Oct 31 2000 */
            printFormat(outf, "%s %s %.2d %.4d",
                        days[WeekDay(local)],
                        months[MonthFromTime(local)],
                        DateFromTime(local),
                        YearFromTime(local));
            break;
          case FORMATSPEC_TIME:
            /* 09:41:40 GMT-0800 (PST) */
            printFormat(outf, "%.2d:%.2d:%.2d GMT%+.4d%s%s",
                        HourFromTime(local),
                        MinFromTime(local),
                        SecFromTime(local),
                        offset,
                        usetz ? " " : "",
                        usetz ? tzbuf : "");
            break;
        }
    }

    return meta->engine->allocString(outf.getString());
}


#define MAXARGS        7
js2val Date_Constructor(JS2Metadata *meta, const js2val /* thisValue */, js2val *argv, uint32 argc)
{
    js2val thatValue = OBJECT_TO_JS2VAL(new DateInstance(meta->dateClass));
    DateInstance *thisInst = checked_cast<DateInstance *>(JS2VAL_TO_OBJECT(thatValue));

    /* Date called as constructor */
    if (argc == 0) {
	int64 us, ms, us2ms;
	float64 msec_time;

	us = PRMJ_Now();
	JSLL_UI2L(us2ms, PRMJ_USEC_PER_MSEC);
	JSLL_DIV(ms, us, us2ms);
	JSLL_L2D(msec_time, ms);

	thisInst->ms = msec_time;
    } 
    else {
        if (argc == 1) {
            if (!JS2VAL_IS_STRING(argv[0])) {
	        /* the argument is a millisecond number */
                float64 d = meta->toFloat64(argv[0]);
	        thisInst->ms = TIMECLIP(d);
	    } else {
	        /* the argument is a string; parse it. */
                const String *str = meta->toString(argv[0]);
	        float64 d = date_parseString(*str);
	        thisInst->ms = TIMECLIP(d);
	    }
        } 
        else {
	    float64 array[MAXARGS];
	    uint32 loop;
	    float64 day;
	    float64 msec_time;

	    for (loop = 0; loop < MAXARGS; loop++) {
	        if (loop < argc) {
	            float64 double_arg = meta->toFloat64(argv[loop]);
		    /* if any arg is NaN, make a NaN date object
		       and return */
		    if (!JSDOUBLE_IS_FINITE(double_arg)) {
		        thisInst->ms = nan;
                        return thatValue;
		    }
                    array[loop] = meta->engine->allocNumber(JS2Engine::float64toInt32(double_arg));
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

	    day = MakeDay(array[0], array[1], array[2]);
	    msec_time = MakeTime(array[3], array[4], array[5], array[6]);
	    msec_time = MakeDate(day, msec_time);
	    msec_time = UTC(msec_time);
	    thisInst->ms = TIMECLIP(msec_time);
        }
    }
    return thatValue;
}


js2val Date_parse(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 /*argc*/)
{
    const String *str = meta->toString(argv[0]);
    float64 d = date_parseString(*str);
    d = TIMECLIP(d);
    return meta->engine->allocNumber(d);
}

js2val Date_UTC(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    float64 array[MAXARGS];
    uint32 loop;
    float64 d;

    for (loop = 0; loop < MAXARGS; loop++) {
	if (loop < argc) {
            d = meta->toFloat64(argv[loop]);
	    if (!JSDOUBLE_IS_FINITE(d))
                return meta->engine->allocNumber(d);
	    array[loop] = floor(d);
	} 
        else 
            array[loop] = 0;
    }

    /* adjust 2-digit years into the 20th century */
    if ((array[0] >= 0) && (array[0] <= 99))
	array[0] += 1900;
    /* if we got a 0 for 'date' (which is out of range)
     * pretend it's a 1.  (So Date.UTC(1972, 5) works) */
    if (array[2] < 1)
	array[2] = 1;

    d = date_msecFromDate(array[0], array[1], array[2],
			      array[3], array[4], array[5], array[6]);
    d = TIMECLIP(d);
    return meta->engine->allocNumber(d);
}

static js2val Date_toGMTString(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    StringFormatter buf;
    float64 *date = Date_getProlog(meta, thisValue);

    if (!JSDOUBLE_IS_FINITE(*date)) {
	buf << js_NaN_date_str;
    } else {
	float64 temp = *date;

	/* Avoid dependence on PRMJ_FormatTimeUSEnglish, because it
	 * requires a PRMJTime... which only has 16-bit years.  Sub-ECMA.
	 */
        printFormat(buf, "%s, %.2d %s %.4d %.2d:%.2d:%.2d GMT",
		    days[WeekDay(temp)],
		    DateFromTime(temp),
		    months[MonthFromTime(temp)],
		    YearFromTime(temp),
		    HourFromTime(temp),
		    MinFromTime(temp),
		    SecFromTime(temp));
    }
    return meta->engine->allocString(buf.getString());
}

static js2val Date_toLocaleHelper(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/, char *format)
{
    StringFormatter outf;
    char buf[100];
    PRMJTime split;

    float64 *date = Date_getProlog(meta, thisValue);

    if (!JSDOUBLE_IS_FINITE(*date)) {
	outf << js_NaN_date_str;
    } else {
	uint32 result_len;
	float64 local = LocalTime(*date);
	new_explode(local, &split, false);

	/* let PRMJTime format it.	 */
	result_len = PRMJ_FormatTime(buf, sizeof buf, format, &split);

	/* If it failed, default to toString. */
	if (result_len == 0)
	    return Date_format(meta, *date, FORMATSPEC_FULL);

        /* Hacked check against undesired 2-digit year 00/00/00 form. */
        if ((buf[result_len - 3] == '/')
                && isdigit(buf[result_len - 2]) && isdigit(buf[result_len - 1])) {
            printString(outf, buf + (result_len - 2), buf + ((sizeof buf) - (result_len - 2)) );
            printFormat(outf, "%d", (int32)YearFromTime(LocalTime(*date)) );
        }
        else
            outf << buf;
    }

    return meta->engine->allocString(outf.getString());
}

static js2val Date_toLocaleString(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    /* Use '%#c' for windows, because '%c' is
     * backward-compatible and non-y2k with msvc; '%#c' requests that a
     * full year be used in the result string.
     */
    return Date_toLocaleHelper(meta, thisValue, argv, argc,
#if defined(_WIN32) && !defined(__MWERKS__)
				   "%#c"
#else
				   "%c"
#endif
				   );
}

static js2val Date_toLocaleDateString(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    /* Use '%#x' for windows, because '%x' is
     * backward-compatible and non-y2k with msvc; '%#x' requests that a
     * full year be used in the result string.
     */
    return Date_toLocaleHelper(meta, thisValue, argv, argc,
#if defined(_WIN32) && !defined(__MWERKS__)
				   "%#x"
#else
				   "%x"
#endif
				   );
}

static js2val Date_toLocaleTimeString(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
{
    return Date_toLocaleHelper(meta, thisValue, argv, argc, "%X");
}

static js2val Date_toTimeString(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    float64 *date = Date_getProlog(meta, thisValue);
    return Date_format(meta, *date, FORMATSPEC_TIME);
}

static js2val Date_toDateString(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    float64 *date = Date_getProlog(meta, thisValue);
    return Date_format(meta, *date, FORMATSPEC_DATE);
}

static js2val Date_toString(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    float64 *date = Date_getProlog(meta, thisValue);
    return Date_format(meta, *date, FORMATSPEC_FULL);
}

static js2val Date_toSource(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    StringFormatter buf;
    float64 *date = Date_getProlog(meta, thisValue);
    buf << "(new Date(" << *meta->engine->numberToString(date) << "))";
    return meta->engine->allocString(buf.getString());
}

static js2val Date_getTime(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    float64 *date = Date_getProlog(meta, thisValue);
    return meta->engine->allocNumber(*date);
}

static js2val Date_getTimezoneOffset(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    float64 result;
    float64 *date = Date_getProlog(meta, thisValue);
    result = *date;

    /*
     * Return the time zone offset in minutes for the current locale
     * that is appropriate for this time. This value would be a
     * constant except for daylight savings time.
     */
    result = (result - LocalTime(result)) / msPerMinute;
    return meta->engine->allocNumber(result);
}

static js2val Date_getYear(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
{
    float64 result;
    float64 *date = Date_getProlog(meta, thisValue);
    result = *date;

    if (!JSDOUBLE_IS_FINITE(result))
        return meta->engine->allocNumber(result);

    result = YearFromTime(LocalTime(result));

#if 0
    XXX
    /*
     * During the great date rewrite of 1.3, we tried to track the evolving ECMA
     * standard, which then had a definition of getYear which always subtracted
     * 1900.  Which we implemented, not realizing that it was incompatible with
     * the old behavior...  now, rather than thrash the behavior yet again,
     * we've decided to leave it with the - 1900 behavior and point people to
     * the getFullYear method.  But we try to protect existing scripts that
     * have specified a version...
     */
    if (cx->version == JSVERSION_1_0 ||
        cx->version == JSVERSION_1_1 ||
        cx->version == JSVERSION_1_2)
    {
        if (result >= 1900 && result < 2000)
            result -= 1900;
    } else {
        result -= 1900;
    }
#else
    result -= 1900;
#endif
    return meta->engine->allocNumber(result);
}

static js2val Date_getFullYear(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
        return meta->engine->allocNumber(result);
    result = YearFromTime(LocalTime(result));
    return meta->engine->allocNumber(result);
}

static js2val Date_getUTCFullYear(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = YearFromTime(result);
    return meta->engine->allocNumber(result);
}

static js2val Date_getMonth(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = MonthFromTime(LocalTime(result));
    return meta->engine->allocNumber(result);
}

static js2val Date_getUTCMonth(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = MonthFromTime(result);
    return meta->engine->allocNumber(result);
}

static js2val Date_getDate(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = DateFromTime(LocalTime(result));
    return meta->engine->allocNumber(result);
}

static js2val Date_getUTCDate(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = DateFromTime(result);
    return meta->engine->allocNumber(result);
}

static js2val Date_getDay(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = WeekDay(LocalTime(result));
    return meta->engine->allocNumber(result);
}

static js2val Date_getUTCDay(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = WeekDay(result);
    return meta->engine->allocNumber(result);
}

static js2val Date_getHours(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = HourFromTime(LocalTime(result));
    return meta->engine->allocNumber(result);
}

static js2val Date_getUTCHours(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = HourFromTime(result);
    return meta->engine->allocNumber(result);
}

static js2val Date_getMinutes(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = MinFromTime(LocalTime(result));
    return meta->engine->allocNumber(result);
}

static js2val Date_getUTCMinutes(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = MinFromTime(result);
    return meta->engine->allocNumber(result);
}

/* Date.getSeconds is mapped to getUTCSeconds */
static js2val Date_getUTCSeconds(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = SecFromTime(result);
    return meta->engine->allocNumber(result);
}

/* Date.getMilliseconds is mapped to getUTCMilliseconds */
static js2val Date_getUTCMilliseconds(JS2Metadata *meta, const js2val thisValue, js2val * /* argv */, uint32 /*argc*/)
{
    float64 result = *Date_getProlog(meta, thisValue);
    if (!JSDOUBLE_IS_FINITE(result))
	return meta->engine->allocNumber(result);
    result = msFromTime(result);
    return meta->engine->allocNumber(result);
}


static js2val Date_setTime(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 /*argc*/)
{
    float64 *date = Date_getProlog(meta, thisValue);
    float64 result = meta->toFloat64(argv[0]);
    *date = TIMECLIP(result);
    return meta->engine->allocNumber(*date);
}

static js2val Date_setYear(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 /*argc*/)
{
    float64 t;
    float64 year;
    float64 day;
    float64 result;

    float64 *date = Date_getProlog(meta, thisValue);
    result = *date;
    year = meta->toFloat64(argv[0]);
    if (!JSDOUBLE_IS_FINITE(year)) {
	*date = nan;
        return meta->engine->allocNumber(*date);
    }

    year = JS2Engine::float64toInt32(year);

    if (!JSDOUBLE_IS_FINITE(result)) {
	t = +0.0;
    } else {
	t = LocalTime(result);
    }

    if (year >= 0 && year <= 99)
	year += 1900;

    day = MakeDay(year, MonthFromTime(t), DateFromTime(t));
    result = MakeDate(day, TimeWithinDay(t));
    result = UTC(result);

    *date = TIMECLIP(result);
    return meta->engine->allocNumber(*date);
}

static js2val Date_setFullYear(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeDate(meta, thisValue, argv, argc, 3, true);
}

static js2val Date_setUTCFullYear(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeDate(meta, thisValue, argv, argc, 3, false);
}

static js2val Date_setMonth(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeDate(meta, thisValue, argv, argc, 2, true);
}

static js2val Date_setUTCMonth(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeDate(meta, thisValue, argv, argc, 2, false);
}

static js2val Date_setDate(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeDate(meta, thisValue, argv, argc, 1, true);
}

static js2val Date_setUTCDate(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeDate(meta, thisValue, argv, argc, 1, false);
}

static js2val Date_setHours(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeTime(meta, thisValue, argv, argc, 4, true);
}

static js2val Date_setUTCHours(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeTime(meta, thisValue, argv, argc, 4, false);
}

static js2val Date_setMinutes(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeTime(meta, thisValue, argv, argc, 3, true);
}

static js2val Date_setUTCMinutes(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeTime(meta, thisValue, argv, argc, 3, false);
}

static js2val Date_setSeconds(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeTime(meta, thisValue, argv, argc, 2, true);
}

static js2val Date_setUTCSeconds(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeTime(meta, thisValue, argv, argc, 2, true);
}

static js2val Date_setMilliseconds(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeTime(meta, thisValue, argv, argc, 1, true);
}

static js2val Date_setUTCMilliseconds(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    return Date_makeTime(meta, thisValue, argv, argc, 1, true);
}

// SpiderMonkey has a 'hinted' version:
#if JS_HAS_VALUEOF_HINT
static js2val Date_valueOf(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
{
    /* If called directly with no arguments, convert to a time number. */
    if (argc == 0)
	return Date_getTime(meta, thisValue, argv, argc);

    /* Convert to number only if the hint was given, otherwise favor string. */
    if (argc == 1) {
        const String *str = JSValue::string(JSValue::toString(meta, argv[0]));
	if (str->compare(&cx->Number_StringAtom) == 0)
	    return Date_getTime(meta, thisValue, argv, argc);
    }
    return Date_toString(meta, thisValue, argv, argc);

}
#endif

void initDateObject(JS2Metadata *meta)
{

    typedef struct {
        char *name;
        uint16 length;
        NativeCode *code;
    } PrototypeFunction;

    PrototypeFunction prototypeFunctions[] =
    {
            { "getTime",            0, Date_getTime            },
            { "getTimezoneOffset",  0, Date_getTimezoneOffset  },
            { "getYear",            0, Date_getYear            },
            { "getFullYear",        0, Date_getFullYear        },
            { "getUTCFullYear",     0, Date_getUTCFullYear     },
            { "getMonth",           0, Date_getMonth           },
            { "getUTCMonth",        0, Date_getUTCMonth        },
            { "getDate",            0, Date_getDate            },
            { "getUTCDate",         0, Date_getUTCDate         },
            { "getDay",             0, Date_getDay             },
            { "getUTCDay",          0, Date_getUTCDay          },
            { "getHours",           0, Date_getHours           },
            { "getUTCHours",        0, Date_getUTCHours        },
            { "getMinutes",         0, Date_getMinutes         },
            { "getUTCMinutes",      0, Date_getUTCMinutes      },
            { "getSeconds",         0, Date_getUTCSeconds      },
            { "getUTCSeconds",      0, Date_getUTCSeconds      },
            { "getMilliseconds",    0, Date_getUTCMilliseconds },
            { "getUTCMilliseconds", 0, Date_getUTCMilliseconds },
            { "setTime",            1, Date_setTime            },
            { "setYear",            1, Date_setYear            },
            { "setFullYear",        1, Date_setFullYear        },
            { "setUTCFullYear",     3, Date_setUTCFullYear     },
            { "setMonth",           2, Date_setMonth           },
            { "setUTCMonth",        2, Date_setUTCMonth        },
            { "setDate",            1, Date_setDate            },
            { "setUTCDate",         1, Date_setUTCDate         },
            { "setHours",           4, Date_setHours           },
            { "setUTCHours",        4, Date_setUTCHours        },
            { "setMinutes",         3, Date_setMinutes         },
            { "setUTCMinutes",      3, Date_setUTCMinutes      },
            { "setSeconds",         2, Date_setSeconds         },
            { "setUTCSeconds",      2, Date_setUTCSeconds      },
            { "setMilliseconds",    1, Date_setMilliseconds    },
            { "setUTCMilliseconds", 1, Date_setUTCMilliseconds },
            { "toUTCString",        0, Date_toGMTString        },
            { "toGMTString",        0, Date_toGMTString        },      // XXX this is a SpiderMonkey extension?
            { "toLocaleString",     0, Date_toLocaleString     },
            { "toLocaleDateString", 0, Date_toLocaleDateString },
            { "toLocaleTimeString", 0, Date_toLocaleTimeString },
            { "toDateString",       0, Date_toDateString       },
            { "toTimeString",       0, Date_toTimeString       },
            { "toSource",           0, Date_toSource           },
            { "toString",           0, Date_toString           },
#if JS_HAS_VALUEOF_HINT
            { "valueOf",            0, Date_valueOf            },
#else
            { "valueOf",            0, Date_getTime            },
#endif
            { NULL }
    };

    LocalTZA = -(PRMJ_LocalGMTDifference() * msPerSecond);

    meta->dateClass->construct = Date_Constructor;

    NamespaceList publicNamespaceList;
    publicNamespaceList.push_back(meta->publicNamespace);

    PrototypeFunction *pf = &prototypeFunctions[0];
    while (pf->name) {
        CallableInstance *fInst = new CallableInstance(meta->functionClass);
        fInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_INACCESSIBLE, true), pf->code);
/*
XXX not prototype object function properties, like ECMA3
        meta->writeDynamicProperty(meta->dateClass->prototype, new Multiname(meta->world.identifiers[pf->name], meta->publicNamespace), true, OBJECT_TO_JS2VAL(fInst), RunPhase);
*/
/*
XXX not static members, since those can't be accessed from the instance
          Variable *v = new Variable(meta->functionClass, OBJECT_TO_JS2VAL(fInst), true);
          meta->defineStaticMember(&meta->env, &meta->world.identifiers[pf->name], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0);
*/
        InstanceMember *m = new InstanceMethod(fInst);
        meta->defineInstanceMember(meta->dateClass, &meta->cxt, &meta->world.identifiers[pf->name], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, m, 0);
        pf++;
    }

}


}
}
