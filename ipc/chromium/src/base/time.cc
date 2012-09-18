// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "prtime.h"

#include "base/logging.h"

namespace base {

// TimeDelta ------------------------------------------------------------------

int TimeDelta::InDays() const {
  return static_cast<int>(delta_ / Time::kMicrosecondsPerDay);
}

int TimeDelta::InHours() const {
  return static_cast<int>(delta_ / Time::kMicrosecondsPerHour);
}

int TimeDelta::InMinutes() const {
  return static_cast<int>(delta_ / Time::kMicrosecondsPerMinute);
}

double TimeDelta::InSecondsF() const {
  return static_cast<double>(delta_) / Time::kMicrosecondsPerSecond;
}

int64_t TimeDelta::InSeconds() const {
  return delta_ / Time::kMicrosecondsPerSecond;
}

double TimeDelta::InMillisecondsF() const {
  return static_cast<double>(delta_) / Time::kMicrosecondsPerMillisecond;
}

int64_t TimeDelta::InMilliseconds() const {
  return delta_ / Time::kMicrosecondsPerMillisecond;
}

int64_t TimeDelta::InMicroseconds() const {
  return delta_;
}

// Time -----------------------------------------------------------------------

// static
Time Time::FromTimeT(time_t tt) {
  if (tt == 0)
    return Time();  // Preserve 0 so we can tell it doesn't exist.
  return (tt * kMicrosecondsPerSecond) + kTimeTToMicrosecondsOffset;
}

time_t Time::ToTimeT() const {
  if (us_ == 0)
    return 0;  // Preserve 0 so we can tell it doesn't exist.
  return (us_ - kTimeTToMicrosecondsOffset) / kMicrosecondsPerSecond;
}

// static
Time Time::FromDoubleT(double dt) {
  return (dt * static_cast<double>(kMicrosecondsPerSecond)) +
      kTimeTToMicrosecondsOffset;
}

double Time::ToDoubleT() const {
  if (us_ == 0)
    return 0;  // Preserve 0 so we can tell it doesn't exist.
  return (static_cast<double>(us_ - kTimeTToMicrosecondsOffset) /
          static_cast<double>(kMicrosecondsPerSecond));
}

Time Time::LocalMidnight() const {
  Exploded exploded;
  LocalExplode(&exploded);
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  return FromLocalExploded(exploded);
}

// static
bool Time::FromString(const wchar_t* time_string, Time* parsed_time) {
  DCHECK((time_string != NULL) && (parsed_time != NULL));
  std::string ascii_time_string = SysWideToUTF8(time_string);
  if (ascii_time_string.length() == 0)
    return false;
  PRTime result_time = 0;
  PRStatus result = PR_ParseTimeString(ascii_time_string.c_str(), PR_FALSE,
                                       &result_time);
  if (PR_SUCCESS != result)
    return false;
  result_time += kTimeTToMicrosecondsOffset;
  *parsed_time = Time(result_time);
  return true;
}

}  // namespace base
