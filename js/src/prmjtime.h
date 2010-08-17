/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

struct JSContext;

/*
 * Implements a small cache for daylight saving time offset computation.
 *
 * The basic idea is premised upon this fact: the DST offset never changes more
 * than once in any thirty-day period.  If we know the offset at t_0 is o_0,
 * the offset at [t_1, t_2] is also o_0, where t_1 + 3_0 days == t_2,
 * t_1 <= t_0, and t0 <= t2.  (In other words, t_0 is always somewhere within a
 * thirty-day range where the DST offset is constant: DST changes never occur
 * more than once in any thirty-day period.)  Therefore, if we intelligently
 * retain knowledge of the offset for a range of dates (which may vary over
 * time), and if requests are usually for dates within that range, we can often
 * provide a response without repeated offset calculation.
 *
 * Our caching strategy is as follows: on the first request at date t_0 compute
 * the requested offset o_0.  Save { start: t_0, end: t_0, offset: o_0 } as the
 * cache's state.  Subsequent requests within that range are straightforwardly
 * handled.  If a request for t_i is far outside the range (more than thirty
 * days), compute o_i = dstOffset(t_i) and save { start: t_i, end: t_i,
 * offset: t_i }.  Otherwise attempt to *overextend* the range to either
 * [start - 30d, end] or [start, end + 30d] as appropriate to encompass
 * t_i.  If the offset o_i30 is the same as the cached offset, extend the
 * range.  Otherwise the over-guess crossed a DST change -- compute
 * o_i = dstOffset(t_i) and either extend the original range (if o_i == offset)
 * or start a new one beneath/above the current one with o_i30 as the offset.
 *
 * This cache strategy results in 0 to 2 DST offset computations.  The naive
 * always-compute strategy is 1 computation, and since cache maintenance is a
 * handful of integer arithmetic instructions the speed difference between
 * always-1 and 1-with-cache is negligible.  Caching loses if two computations
 * happen: when the date is within 30 days of the cached range and when that
 * 30-day range crosses a DST change.  This is relatively uncommon.  Further,
 * instances of such are often dominated by in-range hits, so caching is an
 * overall slight win.
 *
 * Why 30 days?  For correctness the duration must be smaller than any possible
 * duration between DST changes.  Past that, note that 1) a large duration
 * increases the likelihood of crossing a DST change while reducing the number
 * of cache misses, and 2) a small duration decreases the size of the cached
 * range while producing more misses.  Using a month as the interval change is
 * a balance between these two that tries to optimize for the calendar month at
 * a time that a site might display.  (One could imagine an adaptive duration
 * that accommodates near-DST-change dates better; we don't believe the
 * potential win from better caching offsets the loss from extra complexity.)
 */
class DSTOffsetCache {
  public:
    inline DSTOffsetCache();
    JSInt64 getDSTOffsetMilliseconds(int64 localTimeMilliseconds, JSContext *cx);

    inline void purge();
#ifdef JS_METER_DST_OFFSET_CACHING
    void dumpStats();
#endif

  private:
    JSInt64 computeDSTOffsetMilliseconds(int64 localTimeSeconds);

    JSInt64 offsetMilliseconds;
    JSInt64 rangeStartSeconds, rangeEndSeconds;

    JSInt64 oldOffsetMilliseconds;
    JSInt64 oldRangeStartSeconds, oldRangeEndSeconds;

#ifdef JS_METER_DST_OFFSET_CACHING
    size_t totalCalculations;
    size_t hit;
    size_t missIncreasing;
    size_t missDecreasing;
    size_t missIncreasingOffsetChangeUpper;
    size_t missIncreasingOffsetChangeExpand;
    size_t missLargeIncrease;
    size_t missDecreasingOffsetChangeLower;
    size_t missDecreasingOffsetChangeExpand;
    size_t missLargeDecrease;
#endif

    static const JSInt64 MAX_UNIX_TIMET = 2145859200; /* time_t 12/31/2037 */
    static const JSInt64 MILLISECONDS_PER_SECOND = 1000;
    static const JSInt64 SECONDS_PER_MINUTE = 60;
    static const JSInt64 SECONDS_PER_HOUR = 60 * SECONDS_PER_MINUTE;
    static const JSInt64 SECONDS_PER_DAY = 24 * SECONDS_PER_HOUR;

    static const JSInt64 RANGE_EXPANSION_AMOUNT = 30 * SECONDS_PER_DAY;

  private:
    void sanityCheck();

#ifdef JS_METER_DST_OFFSET_CACHING
#define NOTE_GENERIC(member) this->member++
#else
#define NOTE_GENERIC(member) ((void)0)
#endif
    void noteOffsetCalculation() {
        NOTE_GENERIC(totalCalculations);
    }
    void noteCacheHit() {
        NOTE_GENERIC(hit);
    }
    void noteCacheMissIncrease() {
        NOTE_GENERIC(missIncreasing);
    }
    void noteCacheMissDecrease() {
        NOTE_GENERIC(missDecreasing);
    }
    void noteCacheMissIncreasingOffsetChangeUpper() {
        NOTE_GENERIC(missIncreasingOffsetChangeUpper);
    }
    void noteCacheMissIncreasingOffsetChangeExpand() {
        NOTE_GENERIC(missIncreasingOffsetChangeExpand);
    }
    void noteCacheMissLargeIncrease() {
        NOTE_GENERIC(missLargeIncrease);
    }
    void noteCacheMissDecreasingOffsetChangeLower() {
        NOTE_GENERIC(missDecreasingOffsetChangeLower);
    }
    void noteCacheMissDecreasingOffsetChangeExpand() {
        NOTE_GENERIC(missDecreasingOffsetChangeExpand);
    }
    void noteCacheMissLargeDecrease() {
        NOTE_GENERIC(missLargeDecrease);
    }
#undef NOTE_GENERIC
};

JS_BEGIN_EXTERN_C

typedef struct PRMJTime       PRMJTime;

/*
 * Broken down form of 64 bit time value.
 */
struct PRMJTime {
    JSInt32 tm_usec;            /* microseconds of second (0-999999) */
    JSInt8 tm_sec;              /* seconds of minute (0-59) */
    JSInt8 tm_min;              /* minutes of hour (0-59) */
    JSInt8 tm_hour;             /* hour of day (0-23) */
    JSInt8 tm_mday;             /* day of month (1-31) */
    JSInt8 tm_mon;              /* month of year (0-11) */
    JSInt8 tm_wday;             /* 0=sunday, 1=monday, ... */
    JSInt32 tm_year;            /* absolute year, AD */
    JSInt16 tm_yday;            /* day of year (0 to 365) */
    JSInt8 tm_isdst;            /* non-zero if DST in effect */
};

/* Some handy constants */
#define PRMJ_USEC_PER_SEC       1000000L
#define PRMJ_USEC_PER_MSEC      1000L

/* Return the current local time in micro-seconds */
extern JSInt64
PRMJ_Now(void);

/* Release the resources associated with PRMJ_Now; don't call PRMJ_Now again */
#if defined(JS_THREADSAFE) && (defined(HAVE_GETSYSTEMTIMEASFILETIME) || defined(HAVE_SYSTEMTIMETOFILETIME))
extern void
PRMJ_NowShutdown(void);
#else
#define PRMJ_NowShutdown()
#endif

/* get the difference between this time zone and  gmt timezone in seconds */
extern JSInt32
PRMJ_LocalGMTDifference(void);

/* Format a time value into a buffer. Same semantics as strftime() */
extern size_t
PRMJ_FormatTime(char *buf, int buflen, const char *fmt, PRMJTime *tm);

JS_END_EXTERN_C

#endif /* prmjtime_h___ */

