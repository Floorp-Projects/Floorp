/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"

#ifdef OS_MACOSX
#include <mach/mach_time.h>
#endif
#include <sys/time.h>
#if defined(ANDROID) && !defined(__LP64__)
#include <time64.h>
#else
#include <time.h>
#endif
#if defined(ANDROID) || defined(OS_POSIX)
#include <unistd.h>
#endif

#include <limits>

#include "base/basictypes.h"
#include "base/logging.h"

namespace base {

// The Time routines in this file use standard POSIX routines, or almost-
// standard routines in the case of timegm.  We need to use a Mach-specific
// function for TimeTicks::Now() on Mac OS X.

// Time -----------------------------------------------------------------------

// Some functions in time.cc use time_t directly, so we provide a zero offset
// for them.  The epoch is 1970-01-01 00:00:00 UTC.
// static
const int64_t Time::kTimeTToMicrosecondsOffset = GG_INT64_C(0);

// static
Time Time::Now() {
  struct timeval tv;
  struct timezone tz = { 0, 0 };  // UTC
  if (gettimeofday(&tv, &tz) != 0) {
    DCHECK(0) << "Could not determine time of day";
  }
  // Combine seconds and microseconds in a 64-bit field containing microseconds
  // since the epoch.  That's enough for nearly 600 centuries.
  return Time(tv.tv_sec * kMicrosecondsPerSecond + tv.tv_usec);
}

// static
Time Time::NowFromSystemTime() {
  // Just use Now() because Now() returns the system time.
  return Now();
}

// static
Time Time::FromExploded(bool is_local, const Exploded& exploded) {
  struct tm timestruct;
  timestruct.tm_sec    = exploded.second;
  timestruct.tm_min    = exploded.minute;
  timestruct.tm_hour   = exploded.hour;
  timestruct.tm_mday   = exploded.day_of_month;
  timestruct.tm_mon    = exploded.month - 1;
  timestruct.tm_year   = exploded.year - 1900;
  timestruct.tm_wday   = exploded.day_of_week;  // mktime/timegm ignore this
  timestruct.tm_yday   = 0;     // mktime/timegm ignore this
  timestruct.tm_isdst  = -1;    // attempt to figure it out
#ifndef OS_SOLARIS
  timestruct.tm_gmtoff = 0;     // not a POSIX field, so mktime/timegm ignore
  timestruct.tm_zone   = NULL;  // not a POSIX field, so mktime/timegm ignore
#endif

  time_t seconds;
#ifdef ANDROID
    seconds = mktime(&timestruct);
#else
  if (is_local)
    seconds = mktime(&timestruct);
  else
    seconds = timegm(&timestruct);
#endif

  int64_t milliseconds;
  // Handle overflow.  Clamping the range to what mktime and timegm might
  // return is the best that can be done here.  It's not ideal, but it's better
  // than failing here or ignoring the overflow case and treating each time
  // overflow as one second prior to the epoch.
  if (seconds == -1 &&
      (exploded.year < 1969 || exploded.year > 1970)) {
    // If exploded.year is 1969 or 1970, take -1 as correct, with the
    // time indicating 1 second prior to the epoch.  (1970 is allowed to handle
    // time zone and DST offsets.)  Otherwise, return the most future or past
    // time representable.  Assumes the time_t epoch is 1970-01-01 00:00:00 UTC.
    //
    // The minimum and maximum representible times that mktime and timegm could
    // return are used here instead of values outside that range to allow for
    // proper round-tripping between exploded and counter-type time
    // representations in the presence of possible truncation to time_t by
    // division and use with other functions that accept time_t.
    //
    // When representing the most distant time in the future, add in an extra
    // 999ms to avoid the time being less than any other possible value that
    // this function can return.

    // Take care to avoid overflows when time_t is int64_t.
    if (exploded.year < 1969) {
      int64_t min_seconds = (sizeof(time_t) < sizeof(int64_t))
                          ? std::numeric_limits<time_t>::min()
                          : std::numeric_limits<int32_t>::min();
      milliseconds = min_seconds * kMillisecondsPerSecond;
    } else {
      int64_t max_seconds = (sizeof(time_t) < sizeof(int64_t))
                          ? std::numeric_limits<time_t>::max()
                          : std::numeric_limits<int32_t>::max();
      milliseconds = max_seconds * kMillisecondsPerSecond;
      milliseconds += kMillisecondsPerSecond - 1;
    }
  } else {
    milliseconds = seconds * kMillisecondsPerSecond + exploded.millisecond;
  }

  return Time(milliseconds * kMicrosecondsPerMillisecond);
}

void Time::Explode(bool is_local, Exploded* exploded) const {
  // Time stores times with microsecond resolution, but Exploded only carries
  // millisecond resolution, so begin by being lossy.
  int64_t milliseconds = us_ / kMicrosecondsPerMillisecond;
  time_t seconds = milliseconds / kMillisecondsPerSecond;

  struct tm timestruct;
  if (is_local)
    localtime_r(&seconds, &timestruct);
  else
    gmtime_r(&seconds, &timestruct);

  exploded->year         = timestruct.tm_year + 1900;
  exploded->month        = timestruct.tm_mon + 1;
  exploded->day_of_week  = timestruct.tm_wday;
  exploded->day_of_month = timestruct.tm_mday;
  exploded->hour         = timestruct.tm_hour;
  exploded->minute       = timestruct.tm_min;
  exploded->second       = timestruct.tm_sec;
  exploded->millisecond  = milliseconds % kMillisecondsPerSecond;
}

// TimeTicks ------------------------------------------------------------------

// static
TimeTicks TimeTicks::Now() {
  uint64_t absolute_micro;

#if defined(OS_MACOSX)
  static mach_timebase_info_data_t timebase_info;
  if (timebase_info.denom == 0) {
    // Zero-initialization of statics guarantees that denom will be 0 before
    // calling mach_timebase_info.  mach_timebase_info will never set denom to
    // 0 as that would be invalid, so the zero-check can be used to determine
    // whether mach_timebase_info has already been called.  This is
    // recommended by Apple's QA1398.
    kern_return_t kr = mach_timebase_info(&timebase_info);
    DCHECK(kr == KERN_SUCCESS);
  }

  // mach_absolute_time is it when it comes to ticks on the Mac.  Other calls
  // with less precision (such as TickCount) just call through to
  // mach_absolute_time.

  // timebase_info converts absolute time tick units into nanoseconds.  Convert
  // to microseconds up front to stave off overflows.
  absolute_micro = mach_absolute_time() / Time::kNanosecondsPerMicrosecond *
                   timebase_info.numer / timebase_info.denom;

  // Don't bother with the rollover handling that the Windows version does.
  // With numer and denom = 1 (the expected case), the 64-bit absolute time
  // reported in nanoseconds is enough to last nearly 585 years.

#elif defined(OS_OPENBSD) || defined(OS_POSIX) && \
      defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK >= 0

  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    NOTREACHED() << "clock_gettime(CLOCK_MONOTONIC) failed.";
    return TimeTicks();
  }

  absolute_micro =
      (static_cast<int64_t>(ts.tv_sec) * Time::kMicrosecondsPerSecond) +
      (static_cast<int64_t>(ts.tv_nsec) / Time::kNanosecondsPerMicrosecond);

#else  // _POSIX_MONOTONIC_CLOCK
#error No usable tick clock function on this platform.
#endif  // _POSIX_MONOTONIC_CLOCK

  return TimeTicks(absolute_micro);
}

// static
TimeTicks TimeTicks::HighResNow() {
  return Now();
}

}  // namespace base
