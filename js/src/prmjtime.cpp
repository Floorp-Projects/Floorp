/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * PR time code.
 */
#ifdef SOLARIS
#define _REENTRANT 1
#endif
#include <string.h>
#include <time.h>

#include "jsstdint.h"
#include "jstypes.h"
#include "jsutil.h"

#include "jsprf.h"
#include "jslock.h"
#include "prmjtime.h"

#define PRMJ_DO_MILLISECONDS 1

#ifdef XP_OS2
#include <sys/timeb.h>
#endif
#ifdef XP_WIN
#include <windef.h>
#include <winbase.h>
#include <math.h>     /* for fabs */
#include <mmsystem.h> /* for timeBegin/EndPeriod */
/* VC++ 8.0 or later, and not WINCE */
#if _MSC_VER >= 1400 && !defined(WINCE)
#define NS_HAVE_INVALID_PARAMETER_HANDLER 1
#endif
#ifdef NS_HAVE_INVALID_PARAMETER_HANDLER
#include <stdlib.h>   /* for _set_invalid_parameter_handler */
#include <crtdbg.h>   /* for _CrtSetReportMode */
#endif

#ifdef JS_THREADSAFE
#include <prinit.h>
#endif

#endif

#if defined(XP_UNIX) || defined(XP_BEOS)

#ifdef _SVID_GETTOD   /* Defined only on Solaris, see Solaris <sys/types.h> */
extern int gettimeofday(struct timeval *tv);
#endif

#include <sys/time.h>

#endif /* XP_UNIX */

#define PRMJ_YEAR_DAYS 365L
#define PRMJ_FOUR_YEARS_DAYS (4 * PRMJ_YEAR_DAYS + 1)
#define PRMJ_CENTURY_DAYS (25 * PRMJ_FOUR_YEARS_DAYS - 1)
#define PRMJ_FOUR_CENTURIES_DAYS (4 * PRMJ_CENTURY_DAYS + 1)
#define PRMJ_HOUR_SECONDS  3600L
#define PRMJ_DAY_SECONDS  (24L * PRMJ_HOUR_SECONDS)
#define PRMJ_YEAR_SECONDS (PRMJ_DAY_SECONDS * PRMJ_YEAR_DAYS)
#define PRMJ_MAX_UNIX_TIMET 2145859200L /*time_t value equiv. to 12/31/2037 */

/* Get the local time. localtime_r is preferred as it is reentrant. */
static inline bool
ComputeLocalTime(time_t local, struct tm *ptm)
{
#ifdef HAVE_LOCALTIME_R
    return localtime_r(&local, ptm);
#else
    struct tm *otm = localtime(&local);
    if (!otm)
        return false;
    *ptm = *otm;
    return true;
#endif
}

/*
 * get the difference in seconds between this time zone and UTC (GMT)
 */
JSInt32
PRMJ_LocalGMTDifference()
{
#if defined(XP_WIN) && !defined(WINCE)
    /* Windows does not follow POSIX. Updates to the
     * TZ environment variable are not reflected
     * immediately on that platform as they are
     * on UNIX systems without this call.
     */
    _tzset();
#endif

    /*
     * Get the difference between this time zone and GMT, by checking the local
     * time for days 0 and 180 of 1970, using a date for which daylight savings
     * time was not in effect.
     */
    int day = 0;
    struct tm tm;

    if (!ComputeLocalTime(0, &tm))
        return 0;
    if (tm.tm_isdst > 0) {
        day = 180;
        if (!ComputeLocalTime(PRMJ_DAY_SECONDS * day, &tm))
            return 0;
    }

    int time = (tm.tm_hour * 3600) + (tm.tm_min * 60) + tm.tm_sec;
    time = PRMJ_DAY_SECONDS - time;

    if (tm.tm_yday == day)
        time -= PRMJ_DAY_SECONDS;

    return time;
}

/* Constants for GMT offset from 1970 */
#define G1970GMTMICROHI        0x00dcdcad /* micro secs to 1970 hi */
#define G1970GMTMICROLOW       0x8b3fa000 /* micro secs to 1970 low */

#define G2037GMTMICROHI        0x00e45fab /* micro secs to 2037 high */
#define G2037GMTMICROLOW       0x7a238000 /* micro secs to 2037 low */

#ifdef HAVE_SYSTEMTIMETOFILETIME

static const JSInt64 win2un = JSLL_INIT(0x19DB1DE, 0xD53E8000);

#define FILETIME2INT64(ft) (((JSInt64)ft.dwHighDateTime) << 32LL | (JSInt64)ft.dwLowDateTime)

#endif

#if defined(HAVE_GETSYSTEMTIMEASFILETIME) || defined(HAVE_SYSTEMTIMETOFILETIME)

#if defined(HAVE_GETSYSTEMTIMEASFILETIME)
inline void
LowResTime(LPFILETIME lpft)
{
    GetSystemTimeAsFileTime(lpft);
}
#elif defined(HAVE_SYSTEMTIMETOFILETIME)
inline void
LowResTime(LPFILETIME lpft)
{
    GetCurrentFT(lpft);
}
#else
#error "No implementation of PRMJ_Now was selected."
#endif

typedef struct CalibrationData {
    long double freq;         /* The performance counter frequency */
    long double offset;       /* The low res 'epoch' */
    long double timer_offset; /* The high res 'epoch' */

    /* The last high res time that we returned since recalibrating */
    JSInt64 last;

    JSBool calibrated;

#ifdef JS_THREADSAFE
    CRITICAL_SECTION data_lock;
    CRITICAL_SECTION calibration_lock;
#endif
#ifdef WINCE
    JSInt64 granularity;
#endif
} CalibrationData;

static CalibrationData calibration = { 0 };

static void
NowCalibrate()
{
    FILETIME ft, ftStart;
    LARGE_INTEGER liFreq, now;

    if (calibration.freq == 0.0) {
        if(!QueryPerformanceFrequency(&liFreq)) {
            /* High-performance timer is unavailable */
            calibration.freq = -1.0;
        } else {
            calibration.freq = (long double) liFreq.QuadPart;
        }
    }
    if (calibration.freq > 0.0) {
        JSInt64 calibrationDelta = 0;

        /* By wrapping a timeBegin/EndPeriod pair of calls around this loop,
           the loop seems to take much less time (1 ms vs 15ms) on Vista. */
        timeBeginPeriod(1);
        LowResTime(&ftStart);
        do {
            LowResTime(&ft);
        } while (memcmp(&ftStart,&ft, sizeof(ft)) == 0);
        timeEndPeriod(1);

#ifdef WINCE
        calibration.granularity = (FILETIME2INT64(ft) -
                                   FILETIME2INT64(ftStart))/10;
#endif
        /*
        calibrationDelta = (FILETIME2INT64(ft) - FILETIME2INT64(ftStart))/10;
        fprintf(stderr, "Calibration delta was %I64d us\n", calibrationDelta);
        */

        QueryPerformanceCounter(&now);

        calibration.offset = (long double) FILETIME2INT64(ft);
        calibration.timer_offset = (long double) now.QuadPart;

        /* The windows epoch is around 1600. The unix epoch is around
           1970. win2un is the difference (in windows time units which
           are 10 times more highres than the JS time unit) */
        calibration.offset -= win2un;
        calibration.offset *= 0.1;
        calibration.last = 0;

        calibration.calibrated = JS_TRUE;
    }
}

#define CALIBRATIONLOCK_SPINCOUNT 0
#define DATALOCK_SPINCOUNT 4096
#define LASTLOCK_SPINCOUNT 4096

#ifdef JS_THREADSAFE
static PRStatus
NowInit(void)
{
    memset(&calibration, 0, sizeof(calibration));
    NowCalibrate();
#ifdef WINCE
    InitializeCriticalSection(&calibration.calibration_lock);
    InitializeCriticalSection(&calibration.data_lock);
#else
    InitializeCriticalSectionAndSpinCount(&calibration.calibration_lock, CALIBRATIONLOCK_SPINCOUNT);
    InitializeCriticalSectionAndSpinCount(&calibration.data_lock, DATALOCK_SPINCOUNT);
#endif
    return PR_SUCCESS;
}

void
PRMJ_NowShutdown()
{
    DeleteCriticalSection(&calibration.calibration_lock);
    DeleteCriticalSection(&calibration.data_lock);
}

#define MUTEX_LOCK(m) EnterCriticalSection(m)
#define MUTEX_TRYLOCK(m) TryEnterCriticalSection(m)
#define MUTEX_UNLOCK(m) LeaveCriticalSection(m)
#ifdef WINCE
#define MUTEX_SETSPINCOUNT(m, c)
#else
#define MUTEX_SETSPINCOUNT(m, c) SetCriticalSectionSpinCount((m),(c))
#endif

static PRCallOnceType calibrationOnce = { 0 };

#else

#define MUTEX_LOCK(m)
#define MUTEX_TRYLOCK(m) 1
#define MUTEX_UNLOCK(m)
#define MUTEX_SETSPINCOUNT(m, c)

#endif

#endif /* HAVE_GETSYSTEMTIMEASFILETIME */


#if defined(XP_OS2)
JSInt64
PRMJ_Now(void)
{
    JSInt64 s, us, ms2us, s2us;
    struct timeb b;

    ftime(&b);
    JSLL_UI2L(ms2us, PRMJ_USEC_PER_MSEC);
    JSLL_UI2L(s2us, PRMJ_USEC_PER_SEC);
    JSLL_UI2L(s, b.time);
    JSLL_UI2L(us, b.millitm);
    JSLL_MUL(us, us, ms2us);
    JSLL_MUL(s, s, s2us);
    JSLL_ADD(s, s, us);
    return s;
}

#elif defined(XP_UNIX) || defined(XP_BEOS)
JSInt64
PRMJ_Now(void)
{
    struct timeval tv;
    JSInt64 s, us, s2us;

#ifdef _SVID_GETTOD   /* Defined only on Solaris, see Solaris <sys/types.h> */
    gettimeofday(&tv);
#else
    gettimeofday(&tv, 0);
#endif /* _SVID_GETTOD */
    JSLL_UI2L(s2us, PRMJ_USEC_PER_SEC);
    JSLL_UI2L(s, tv.tv_sec);
    JSLL_UI2L(us, tv.tv_usec);
    JSLL_MUL(s, s, s2us);
    JSLL_ADD(s, s, us);
    return s;
}

#else
/*

Win32 python-esque pseudo code
Please see bug 363258 for why the win32 timing code is so complex.

calibration mutex : Win32CriticalSection(spincount=0)
data mutex : Win32CriticalSection(spincount=4096)

def NowInit():
  init mutexes
  PRMJ_NowCalibration()

def NowCalibration():
  expensive up-to-15ms call

def PRMJ_Now():
  returnedTime = 0
  needCalibration = False
  cachedOffset = 0.0
  calibrated = False
  PR_CallOnce(PRMJ_NowInit)
  do
    if not global.calibrated or needCalibration:
      acquire calibration mutex
        acquire data mutex

          // Only recalibrate if someone didn't already
          if cachedOffset == calibration.offset:
            // Have all waiting threads immediately wait
            set data mutex spin count = 0
            PRMJ_NowCalibrate()
            calibrated = 1

            set data mutex spin count = default
        release data mutex
      release calibration mutex

    calculate lowres time

    if highres timer available:
      acquire data mutex
        calculate highres time
        cachedOffset = calibration.offset
        highres time = calibration.last = max(highres time, calibration.last)
      release data mutex

      get kernel tick interval

      if abs(highres - lowres) < kernel tick:
        returnedTime = highres time
        needCalibration = False
      else:
        if calibrated:
          returnedTime = lowres
          needCalibration = False
        else:
          needCalibration = True
    else:
      returnedTime = lowres
  while needCalibration

*/

// We parameterize the delay count just so that shell builds can
// set it to 0 in order to get high-resolution benchmarking.
// 10 seems to be the number of calls to load with a blank homepage.
int CALIBRATION_DELAY_COUNT = 10;

JSInt64
PRMJ_Now(void)
{
    static int nCalls = 0;
    long double lowresTime, highresTimerValue;
    FILETIME ft;
    LARGE_INTEGER now;
    JSBool calibrated = JS_FALSE;
    JSBool needsCalibration = JS_FALSE;
    JSInt64 returnedTime;
    long double cachedOffset = 0.0;

    /* To avoid regressing startup time (where high resolution is likely
       not needed), give the old behavior for the first few calls.
       This does not appear to be needed on Vista as the timeBegin/timeEndPeriod
       calls seem to immediately take effect. */
    int thiscall = JS_ATOMIC_INCREMENT(&nCalls);
    if (thiscall <= CALIBRATION_DELAY_COUNT) {
        LowResTime(&ft);
        return (FILETIME2INT64(ft)-win2un)/10L;
    }

    /* For non threadsafe platforms, NowInit is not necessary */
#ifdef JS_THREADSAFE
    PR_CallOnce(&calibrationOnce, NowInit);
#endif
    do {
        if (!calibration.calibrated || needsCalibration) {
            MUTEX_LOCK(&calibration.calibration_lock);
            MUTEX_LOCK(&calibration.data_lock);

            /* Recalibrate only if no one else did before us */
            if(calibration.offset == cachedOffset) {
                /* Since calibration can take a while, make any other
                   threads immediately wait */
                MUTEX_SETSPINCOUNT(&calibration.data_lock, 0);

                NowCalibrate();

                calibrated = JS_TRUE;

                /* Restore spin count */
                MUTEX_SETSPINCOUNT(&calibration.data_lock, DATALOCK_SPINCOUNT);
            }
            MUTEX_UNLOCK(&calibration.data_lock);
            MUTEX_UNLOCK(&calibration.calibration_lock);
        }


        /* Calculate a low resolution time */
        LowResTime(&ft);
        lowresTime = 0.1*(long double)(FILETIME2INT64(ft) - win2un);

        if (calibration.freq > 0.0) {
            long double highresTime, diff;

            DWORD timeAdjustment, timeIncrement;
            BOOL timeAdjustmentDisabled;

            /* Default to 15.625 ms if the syscall fails */
            long double skewThreshold = 15625.25;
            /* Grab high resolution time */
            QueryPerformanceCounter(&now);
            highresTimerValue = (long double)now.QuadPart;

            MUTEX_LOCK(&calibration.data_lock);
            highresTime = calibration.offset + PRMJ_USEC_PER_SEC*
                 (highresTimerValue-calibration.timer_offset)/calibration.freq;
            cachedOffset = calibration.offset;

            /* On some dual processor/core systems, we might get an earlier time
               so we cache the last time that we returned */
            calibration.last = JS_MAX(calibration.last,(JSInt64)highresTime);
            returnedTime = calibration.last;
            MUTEX_UNLOCK(&calibration.data_lock);

#ifdef WINCE
            /* Get an estimate of clock ticks per second from our own test */
            skewThreshold = calibration.granularity;
#else
            /* Rather than assume the NT kernel ticks every 15.6ms, ask it */
            if (GetSystemTimeAdjustment(&timeAdjustment,
                                        &timeIncrement,
                                        &timeAdjustmentDisabled)) {
                if (timeAdjustmentDisabled) {
                    /* timeAdjustment is in units of 100ns */
                    skewThreshold = timeAdjustment/10.0;
                } else {
                    /* timeIncrement is in units of 100ns */
                    skewThreshold = timeIncrement/10.0;
                }
            }
#endif
            /* Check for clock skew */
            diff = lowresTime - highresTime;

            /* For some reason that I have not determined, the skew can be
               up to twice a kernel tick. This does not seem to happen by
               itself, but I have only seen it triggered by another program
               doing some kind of file I/O. The symptoms are a negative diff
               followed by an equally large positive diff. */
            if (fabs(diff) > 2*skewThreshold) {
                /*fprintf(stderr,"Clock skew detected (diff = %f)!\n", diff);*/

                if (calibrated) {
                    /* If we already calibrated once this instance, and the
                       clock is still skewed, then either the processor(s) are
                       wildly changing clockspeed or the system is so busy that
                       we get switched out for long periods of time. In either
                       case, it would be infeasible to make use of high
                       resolution results for anything, so let's resort to old
                       behavior for this call. It's possible that in the
                       future, the user will want the high resolution timer, so
                       we don't disable it entirely. */
                    returnedTime = (JSInt64)lowresTime;
                    needsCalibration = JS_FALSE;
                } else {
                    /* It is possible that when we recalibrate, we will return a
                       value less than what we have returned before; this is
                       unavoidable. We cannot tell the different between a
                       faulty QueryPerformanceCounter implementation and user
                       changes to the operating system time. Since we must
                       respect user changes to the operating system time, we
                       cannot maintain the invariant that Date.now() never
                       decreases; the old implementation has this behavior as
                       well. */
                    needsCalibration = JS_TRUE;
                }
            } else {
                /* No detectable clock skew */
                returnedTime = (JSInt64)highresTime;
                needsCalibration = JS_FALSE;
            }
        } else {
            /* No high resolution timer is available, so fall back */
            returnedTime = (JSInt64)lowresTime;
        }
    } while (needsCalibration);

    return returnedTime;
}
#endif

#ifdef NS_HAVE_INVALID_PARAMETER_HANDLER
static void
PRMJ_InvalidParameterHandler(const wchar_t *expression,
                             const wchar_t *function,
                             const wchar_t *file,
                             unsigned int   line,
                             uintptr_t      pReserved)
{
    /* empty */
}
#endif

/* Format a time value into a buffer. Same semantics as strftime() */
size_t
PRMJ_FormatTime(char *buf, int buflen, const char *fmt, PRMJTime *prtm)
{
    size_t result = 0;
#if defined(XP_UNIX) || defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS)
    struct tm a;
    int fake_tm_year = 0;
#ifdef NS_HAVE_INVALID_PARAMETER_HANDLER
    _invalid_parameter_handler oldHandler;
    int oldReportMode;
#endif

    memset(&a, 0, sizeof(struct tm));

    a.tm_sec = prtm->tm_sec;
    a.tm_min = prtm->tm_min;
    a.tm_hour = prtm->tm_hour;
    a.tm_mday = prtm->tm_mday;
    a.tm_mon = prtm->tm_mon;
    a.tm_wday = prtm->tm_wday;

    /*
     * On systems where |struct tm| has members tm_gmtoff and tm_zone, we
     * must fill in those values, or else strftime will return wrong results
     * (e.g., bug 511726, bug 554338).
     */
#if defined(HAVE_LOCALTIME_R) && defined(HAVE_TM_ZONE_TM_GMTOFF)
    {
        /*
         * Fill out |td| to the time represented by |prtm|, leaving the
         * timezone fields zeroed out. localtime_r will then fill in the
         * timezone fields for that local time according to the system's
         * timezone parameters.
         */
        struct tm td;
        memset(&td, 0, sizeof(td));
        td.tm_sec = prtm->tm_sec;
        td.tm_min = prtm->tm_min;
        td.tm_hour = prtm->tm_hour;
        td.tm_mday = prtm->tm_mday;
        td.tm_mon = prtm->tm_mon;
        td.tm_wday = prtm->tm_wday;
        td.tm_year = prtm->tm_year - 1900;
        td.tm_yday = prtm->tm_yday;
        td.tm_isdst = prtm->tm_isdst;
        time_t t = mktime(&td);
        localtime_r(&t, &td);

        a.tm_gmtoff = td.tm_gmtoff;
        a.tm_zone = td.tm_zone;
    }
#endif

    /*
     * Years before 1900 and after 9999 cause strftime() to abort on Windows.
     * To avoid that we replace it with FAKE_YEAR_BASE + year % 100 and then
     * replace matching substrings in the strftime() result with the real year.
     * Note that FAKE_YEAR_BASE should be a multiple of 100 to make 2-digit
     * year formats (%y) work correctly (since we won't find the fake year
     * in that case).
     * e.g. new Date(1873, 0).toLocaleFormat('%Y %y') => "1873 73"
     * See bug 327869.
     */
#define FAKE_YEAR_BASE 9900
    if (prtm->tm_year < 1900 || prtm->tm_year > 9999) {
        fake_tm_year = FAKE_YEAR_BASE + prtm->tm_year % 100;
        a.tm_year = fake_tm_year - 1900;
    }
    else {
        a.tm_year = prtm->tm_year - 1900;
    }
    a.tm_yday = prtm->tm_yday;
    a.tm_isdst = prtm->tm_isdst;

    /*
     * Even with the above, SunOS 4 seems to detonate if tm_zone and tm_gmtoff
     * are null.  This doesn't quite work, though - the timezone is off by
     * tzoff + dst.  (And mktime seems to return -1 for the exact dst
     * changeover time.)
     */

#ifdef NS_HAVE_INVALID_PARAMETER_HANDLER
    oldHandler = _set_invalid_parameter_handler(PRMJ_InvalidParameterHandler);
    oldReportMode = _CrtSetReportMode(_CRT_ASSERT, 0);
#endif

    result = strftime(buf, buflen, fmt, &a);

#ifdef NS_HAVE_INVALID_PARAMETER_HANDLER
    _set_invalid_parameter_handler(oldHandler);
    _CrtSetReportMode(_CRT_ASSERT, oldReportMode);
#endif

    if (fake_tm_year && result) {
        char real_year[16];
        char fake_year[16];
        size_t real_year_len;
        size_t fake_year_len;
        char* p;

        sprintf(real_year, "%d", prtm->tm_year);
        real_year_len = strlen(real_year);
        sprintf(fake_year, "%d", fake_tm_year);
        fake_year_len = strlen(fake_year);

        /* Replace the fake year in the result with the real year. */
        for (p = buf; (p = strstr(p, fake_year)); p += real_year_len) {
            size_t new_result = result + real_year_len - fake_year_len;
            if ((int)new_result >= buflen) {
                return 0;
            }
            memmove(p + real_year_len, p + fake_year_len, strlen(p + fake_year_len));
            memcpy(p, real_year, real_year_len);
            result = new_result;
            *(buf + result) = '\0';
        }
    }
#endif
    return result;
}

JSInt64
DSTOffsetCache::computeDSTOffsetMilliseconds(int64 localTimeSeconds)
{
    JS_ASSERT(localTimeSeconds >= 0);
    JS_ASSERT(localTimeSeconds <= MAX_UNIX_TIMET);

#if defined(XP_WIN) && !defined(WINCE)
    /* Windows does not follow POSIX. Updates to the
     * TZ environment variable are not reflected
     * immediately on that platform as they are
     * on UNIX systems without this call.
     */
    _tzset();
#endif

    struct tm tm;
    if (!ComputeLocalTime(static_cast<time_t>(localTimeSeconds), &tm))
        return 0;

    JSInt32 base = PRMJ_LocalGMTDifference();

    int32 dayoff = int32((localTimeSeconds - base) % (SECONDS_PER_HOUR * 24));
    int32 tmoff = tm.tm_sec + (tm.tm_min * SECONDS_PER_MINUTE) +
        (tm.tm_hour * SECONDS_PER_HOUR);

    JSInt32 diff = tmoff - dayoff;

    if (diff < 0)
        diff += SECONDS_PER_DAY;

    return diff * MILLISECONDS_PER_SECOND;
}

JSInt64
DSTOffsetCache::getDSTOffsetMilliseconds(JSInt64 localTimeMilliseconds, JSContext *cx)
{
    sanityCheck();
    noteOffsetCalculation();

    JSInt64 localTimeSeconds = localTimeMilliseconds / MILLISECONDS_PER_SECOND;

    if (localTimeSeconds > MAX_UNIX_TIMET) {
        localTimeSeconds = MAX_UNIX_TIMET;
    } else if (localTimeSeconds < 0) {
        /* Go ahead a day to make localtime work (does not work with 0). */
        localTimeSeconds = SECONDS_PER_DAY;
    }

    /*
     * NB: Be aware of the initial range values when making changes to this
     *     code: the first call to this method, with those initial range
     *     values, must result in a cache miss.
     */

    if (rangeStartSeconds <= localTimeSeconds &&
        localTimeSeconds <= rangeEndSeconds) {
        noteCacheHit();
        return offsetMilliseconds;
    }

    if (oldRangeStartSeconds <= localTimeSeconds &&
        localTimeSeconds <= oldRangeEndSeconds) {
        noteCacheHit();
        return oldOffsetMilliseconds;
    }

    oldOffsetMilliseconds = offsetMilliseconds;
    oldRangeStartSeconds = rangeStartSeconds;
    oldRangeEndSeconds = rangeEndSeconds;

    if (rangeStartSeconds <= localTimeSeconds) {
        JSInt64 newEndSeconds = JS_MIN(rangeEndSeconds + RANGE_EXPANSION_AMOUNT, MAX_UNIX_TIMET);
        if (newEndSeconds >= localTimeSeconds) {
            JSInt64 endOffsetMilliseconds = computeDSTOffsetMilliseconds(newEndSeconds);
            if (endOffsetMilliseconds == offsetMilliseconds) {
                noteCacheMissIncrease();
                rangeEndSeconds = newEndSeconds;
                return offsetMilliseconds;
            }

            offsetMilliseconds = computeDSTOffsetMilliseconds(localTimeSeconds);
            if (offsetMilliseconds == endOffsetMilliseconds) {
                noteCacheMissIncreasingOffsetChangeUpper();
                rangeStartSeconds = localTimeSeconds;
                rangeEndSeconds = newEndSeconds;
            } else {
                noteCacheMissIncreasingOffsetChangeExpand();
                rangeEndSeconds = localTimeSeconds;
            }
            return offsetMilliseconds;
        }

        noteCacheMissLargeIncrease();
        offsetMilliseconds = computeDSTOffsetMilliseconds(localTimeSeconds);
        rangeStartSeconds = rangeEndSeconds = localTimeSeconds;
        return offsetMilliseconds;
    }

    JSInt64 newStartSeconds = JS_MAX(rangeStartSeconds - RANGE_EXPANSION_AMOUNT, 0);
    if (newStartSeconds <= localTimeSeconds) {
        JSInt64 startOffsetMilliseconds = computeDSTOffsetMilliseconds(newStartSeconds);
        if (startOffsetMilliseconds == offsetMilliseconds) {
            noteCacheMissDecrease();
            rangeStartSeconds = newStartSeconds;
            return offsetMilliseconds;
        }

        offsetMilliseconds = computeDSTOffsetMilliseconds(localTimeSeconds);
        if (offsetMilliseconds == startOffsetMilliseconds) {
            noteCacheMissDecreasingOffsetChangeLower();
            rangeStartSeconds = newStartSeconds;
            rangeEndSeconds = localTimeSeconds;
        } else {
            noteCacheMissDecreasingOffsetChangeExpand();
            rangeStartSeconds = localTimeSeconds;
        }
        return offsetMilliseconds;
    }

    noteCacheMissLargeDecrease();
    rangeStartSeconds = rangeEndSeconds = localTimeSeconds;
    offsetMilliseconds = computeDSTOffsetMilliseconds(localTimeSeconds);
    return offsetMilliseconds;
}

void
DSTOffsetCache::sanityCheck()
{
    JS_ASSERT(rangeStartSeconds <= rangeEndSeconds);
    JS_ASSERT_IF(rangeStartSeconds == INT64_MIN, rangeEndSeconds == INT64_MIN);
    JS_ASSERT_IF(rangeEndSeconds == INT64_MIN, rangeStartSeconds == INT64_MIN);
    JS_ASSERT_IF(rangeStartSeconds != INT64_MIN,
                 rangeStartSeconds >= 0 && rangeEndSeconds >= 0);
    JS_ASSERT_IF(rangeStartSeconds != INT64_MIN,
                 rangeStartSeconds <= MAX_UNIX_TIMET && rangeEndSeconds <= MAX_UNIX_TIMET);

#ifdef JS_METER_DST_OFFSET_CACHING
    JS_ASSERT(totalCalculations ==
              hit +
              missIncreasing + missDecreasing +
              missIncreasingOffsetChangeExpand + missIncreasingOffsetChangeUpper +
              missDecreasingOffsetChangeExpand + missDecreasingOffsetChangeLower +
              missLargeIncrease + missLargeDecrease);
#endif
}

#ifdef JS_METER_DST_OFFSET_CACHING
void
DSTOffsetCache::dumpStats()
{
    if (!getenv("JS_METER_DST_OFFSET_CACHING"))
        return;
    FILE *fp = fopen("/tmp/dst-offset-cache.stats", "a");
    if (!fp)
        return;
    typedef unsigned long UL;
    fprintf(fp,
            "hit:\n"
            "  in range: %lu\n"
            "misses:\n"
            "  increase range end:                 %lu\n"
            "  decrease range start:               %lu\n"
            "  increase, offset change, expand:    %lu\n"
            "  increase, offset change, new range: %lu\n"
            "  decrease, offset change, expand:    %lu\n"
            "  decrease, offset change, new range: %lu\n"
            "  large increase:                     %lu\n"
            "  large decrease:                     %lu\n"
            "total: %lu\n\n",
            UL(hit),
            UL(missIncreasing), UL(missDecreasing),
            UL(missIncreasingOffsetChangeExpand), UL(missIncreasingOffsetChangeUpper),
            UL(missDecreasingOffsetChangeExpand), UL(missDecreasingOffsetChangeLower),
            UL(missLargeIncrease), UL(missLargeDecrease),
            UL(totalCalculations));
    fclose(fp);
}
#endif
