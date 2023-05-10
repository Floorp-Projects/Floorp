/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SystemTime.h"

#include "TimeUnits.h"

namespace mozilla {

// webrtc::Timestamp may not be negative. `now-base` for the first call to
// WebrtcSystemTime() is always 0, which makes it impossible for libwebrtc
// code to calculate a timestamp older than the first one returned. This
// offset makes sure the clock starts at a value equivalent to roughly 4.5h.
static constexpr webrtc::TimeDelta kWebrtcTimeOffset =
    webrtc::TimeDelta::Micros(0x10000000);

RTCStatsTimestampMakerRealtimeClock::RTCStatsTimestampMakerRealtimeClock(
    const dom::RTCStatsTimestampMaker& aTimestampMaker)
    : mTimestampMaker(aTimestampMaker) {}

webrtc::Timestamp RTCStatsTimestampMakerRealtimeClock::CurrentTime() {
  return mTimestampMaker.GetNow().ToRealtime();
}

webrtc::NtpTime RTCStatsTimestampMakerRealtimeClock::ConvertTimestampToNtpTime(
    webrtc::Timestamp aRealtime) {
  return CreateNtp(
      dom::RTCStatsTimestamp::FromRealtime(mTimestampMaker, aRealtime).ToNtp());
}

TimeStamp WebrtcSystemTimeBase() {
  static TimeStamp now = TimeStamp::Now();
  return now;
}

webrtc::Timestamp WebrtcSystemTime() {
  const TimeStamp base = WebrtcSystemTimeBase();
  const TimeStamp now = TimeStamp::Now();
  return webrtc::Timestamp::Micros((now - base).ToMicroseconds()) +
         kWebrtcTimeOffset;
}

webrtc::NtpTime CreateNtp(webrtc::Timestamp aTime) {
  const int64_t timeNtpUs = aTime.us();
  const uint32_t seconds = static_cast<uint32_t>(timeNtpUs / USECS_PER_S);

  constexpr int64_t fractionsPerSec = 1LL << 32;
  const int64_t fractionsUs = timeNtpUs % USECS_PER_S;
  const uint32_t fractions = (fractionsUs * fractionsPerSec) / USECS_PER_S;

  return webrtc::NtpTime(seconds, fractions);
}
}  // namespace mozilla

namespace rtc {
int64_t SystemTimeNanos() { return mozilla::WebrtcSystemTime().us() * 1000; }
}  // namespace rtc
