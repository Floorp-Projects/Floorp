/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SystemTime.h"

#include "TimeUnits.h"

namespace mozilla {

RTCStatsTimestampMakerRealtimeClock::RTCStatsTimestampMakerRealtimeClock(
    const dom::RTCStatsTimestampMaker& aTimestampMaker)
    : mTimestampMaker(aTimestampMaker) {}

webrtc::Timestamp RTCStatsTimestampMakerRealtimeClock::CurrentTime() {
  return mTimestampMaker.GetNowRealtime();
}

webrtc::NtpTime RTCStatsTimestampMakerRealtimeClock::ConvertTimestampToNtpTime(
    webrtc::Timestamp aRealtime) {
  return CreateNtp(mTimestampMaker.ConvertRealtimeTo1Jan1970(aRealtime) +
                   webrtc::TimeDelta::Seconds(webrtc::kNtpJan1970));
}

static TimeStamp CalculateBaseOffset(TimeStamp aNow) {
  uint32_t offset = 24 * 60 * 60;
  // If `converted` has underflowed it is capped at 0, which is an invalid
  // timestamp. Reduce the offset in case that happens.
  TimeStamp base;
  do {
    base = aNow - TimeDuration::FromSeconds(offset);
    offset /= 2;
  } while (!base);
  return base;
}

TimeStamp WebrtcSystemTimeBase() {
  static TimeStamp now = TimeStamp::Now();
  // Make it obvious that these timestamps use a different base than
  // RTCStatsTimestampMakerRealtimeClock::CurrentTime.
  static TimeStamp base = CalculateBaseOffset(now);
  return base;
}

webrtc::Timestamp WebrtcSystemTime() {
  const TimeStamp base = WebrtcSystemTimeBase();
  const TimeStamp now = TimeStamp::Now();
  return webrtc::Timestamp::Micros((now - base).ToMicroseconds());
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
