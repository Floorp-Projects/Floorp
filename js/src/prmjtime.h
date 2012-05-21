/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef prmjtime_h___
#define prmjtime_h___

#include <time.h>

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
    int64_t getDSTOffsetMilliseconds(int64_t localTimeMilliseconds, JSContext *cx);

    inline void purge();

  private:
    int64_t computeDSTOffsetMilliseconds(int64_t localTimeSeconds);

    int64_t offsetMilliseconds;
    int64_t rangeStartSeconds, rangeEndSeconds;

    int64_t oldOffsetMilliseconds;
    int64_t oldRangeStartSeconds, oldRangeEndSeconds;

    static const int64_t MAX_UNIX_TIMET = 2145859200; /* time_t 12/31/2037 */
    static const int64_t MILLISECONDS_PER_SECOND = 1000;
    static const int64_t SECONDS_PER_MINUTE = 60;
    static const int64_t SECONDS_PER_HOUR = 60 * SECONDS_PER_MINUTE;
    static const int64_t SECONDS_PER_DAY = 24 * SECONDS_PER_HOUR;

    static const int64_t RANGE_EXPANSION_AMOUNT = 30 * SECONDS_PER_DAY;

  private:
    void sanityCheck();
};

JS_BEGIN_EXTERN_C

typedef struct PRMJTime       PRMJTime;

/*
 * Broken down form of 64 bit time value.
 */
struct PRMJTime {
    int32_t tm_usec;            /* microseconds of second (0-999999) */
    int8_t tm_sec;              /* seconds of minute (0-59) */
    int8_t tm_min;              /* minutes of hour (0-59) */
    int8_t tm_hour;             /* hour of day (0-23) */
    int8_t tm_mday;             /* day of month (1-31) */
    int8_t tm_mon;              /* month of year (0-11) */
    int8_t tm_wday;             /* 0=sunday, 1=monday, ... */
    int32_t tm_year;            /* absolute year, AD */
    int16_t tm_yday;            /* day of year (0 to 365) */
    int8_t tm_isdst;            /* non-zero if DST in effect */
};

/* Some handy constants */
#define PRMJ_USEC_PER_SEC       1000000L
#define PRMJ_USEC_PER_MSEC      1000L

/* Return the current local time in micro-seconds */
extern int64_t
PRMJ_Now(void);

/* Release the resources associated with PRMJ_Now; don't call PRMJ_Now again */
#if defined(JS_THREADSAFE) && (defined(HAVE_GETSYSTEMTIMEASFILETIME) || defined(HAVE_SYSTEMTIMETOFILETIME))
extern void
PRMJ_NowShutdown(void);
#else
#define PRMJ_NowShutdown()
#endif

/* get the difference between this time zone and  gmt timezone in seconds */
extern int32_t
PRMJ_LocalGMTDifference(void);

/* Format a time value into a buffer. Same semantics as strftime() */
extern size_t
PRMJ_FormatTime(char *buf, int buflen, const char *fmt, PRMJTime *tm);

JS_END_EXTERN_C

#endif /* prmjtime_h___ */

