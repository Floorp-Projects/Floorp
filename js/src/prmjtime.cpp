/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* PR time code. */

#include "mozilla/MathAlgorithms.h"

#ifdef SOLARIS
#define _REENTRANT 1
#endif
#include <string.h>
#include <time.h>

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
#include <mmsystem.h> /* for timeBegin/EndPeriod */
/* VC++ 8.0 or later */
#if _MSC_VER >= 1400
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

#ifdef XP_UNIX

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

/* Constants for GMT offset from 1970 */
#define G1970GMTMICROHI        0x00dcdcad /* micro secs to 1970 hi */
#define G1970GMTMICROLOW       0x8b3fa000 /* micro secs to 1970 low */

#define G2037GMTMICROHI        0x00e45fab /* micro secs to 2037 high */
#define G2037GMTMICROLOW       0x7a238000 /* micro secs to 2037 low */

#if defined(XP_WIN)

static const int64_t win2un = 0x19DB1DED53E8000;

#define FILETIME2INT64(ft) (((int64_t)ft.dwHighDateTime) << 32LL | (int64_t)ft.dwLowDateTime)

typedef struct CalibrationData {
    long double freq;         /* The performance counter frequency */
    long double offset;       /* The low res 'epoch' */
    long double timer_offset; /* The high res 'epoch' */

    /* The last high res time that we returned since recalibrating */
    int64_t last;

    bool calibrated;

#ifdef JS_THREADSAFE
    CRITICAL_SECTION data_lock;
    CRITICAL_SECTION calibration_lock;
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
        int64_t calibrationDelta = 0;

        /* By wrapping a timeBegin/EndPeriod pair of calls around this loop,
           the loop seems to take much less time (1 ms vs 15ms) on Vista. */
        timeBeginPeriod(1);
        GetSystemTimeAsFileTime(&ftStart);
        do {
            GetSystemTimeAsFileTime(&ft);
        } while (memcmp(&ftStart,&ft, sizeof(ft)) == 0);
        timeEndPeriod(1);

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

        calibration.calibrated = true;
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
    InitializeCriticalSectionAndSpinCount(&calibration.calibration_lock, CALIBRATIONLOCK_SPINCOUNT);
    InitializeCriticalSectionAndSpinCount(&calibration.data_lock, DATALOCK_SPINCOUNT);
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
#define MUTEX_SETSPINCOUNT(m, c) SetCriticalSectionSpinCount((m),(c))

static PRCallOnceType calibrationOnce = { 0 };

#else

#define MUTEX_LOCK(m)
#define MUTEX_TRYLOCK(m) 1
#define MUTEX_UNLOCK(m)
#define MUTEX_SETSPINCOUNT(m, c)

#endif

#endif /* XP_WIN */


#if defined(XP_OS2)
int64_t
PRMJ_Now(void)
{
    struct timeb b;
    ftime(&b);
    return (int64_t(b.time) * PRMJ_USEC_PER_SEC) + (int64_t(b.millitm) * PRMJ_USEC_PER_MSEC);
}

#elif defined(XP_UNIX)
int64_t
PRMJ_Now(void)
{
    struct timeval tv;

#ifdef _SVID_GETTOD   /* Defined only on Solaris, see Solaris <sys/types.h> */
    gettimeofday(&tv);
#else
    gettimeofday(&tv, 0);
#endif /* _SVID_GETTOD */

    return int64_t(tv.tv_sec) * PRMJ_USEC_PER_SEC + int64_t(tv.tv_usec);
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

int64_t
PRMJ_Now(void)
{
    static int nCalls = 0;
    long double lowresTime, highresTimerValue;
    FILETIME ft;
    LARGE_INTEGER now;
    bool calibrated = false;
    bool needsCalibration = false;
    int64_t returnedTime;
    long double cachedOffset = 0.0;

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

                calibrated = true;

                /* Restore spin count */
                MUTEX_SETSPINCOUNT(&calibration.data_lock, DATALOCK_SPINCOUNT);
            }
            MUTEX_UNLOCK(&calibration.data_lock);
            MUTEX_UNLOCK(&calibration.calibration_lock);
        }


        /* Calculate a low resolution time */
        GetSystemTimeAsFileTime(&ft);
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
            calibration.last = js::Max(calibration.last, int64_t(highresTime));
            returnedTime = calibration.last;
            MUTEX_UNLOCK(&calibration.data_lock);

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

            /* Check for clock skew */
            diff = lowresTime - highresTime;

            /* For some reason that I have not determined, the skew can be
               up to twice a kernel tick. This does not seem to happen by
               itself, but I have only seen it triggered by another program
               doing some kind of file I/O. The symptoms are a negative diff
               followed by an equally large positive diff. */
            if (mozilla::Abs(diff) > 2 * skewThreshold) {
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
                    returnedTime = int64_t(lowresTime);
                    needsCalibration = false;
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
                    needsCalibration = true;
                }
            } else {
                /* No detectable clock skew */
                returnedTime = int64_t(highresTime);
                needsCalibration = false;
            }
        } else {
            /* No high resolution timer is available, so fall back */
            returnedTime = int64_t(lowresTime);
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
#if defined(XP_UNIX) || defined(XP_WIN) || defined(XP_OS2)
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
