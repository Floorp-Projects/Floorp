/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cmath>

#include "gtest/gtest.h"
#include "libwebrtcglue/SystemTime.h"

using namespace mozilla;
using dom::PerformanceService;
using dom::RTCStatsTimestamp;
using dom::RTCStatsTimestampMaker;

static constexpr auto kWebrtcTimeOffset = webrtc::Timestamp::Seconds(123456789);

TEST(RTCStatsTimestampMakerRealtimeClock, ConvertTimestampToNtpTime)
{
  auto maker = RTCStatsTimestampMaker::Create();
  RTCStatsTimestampMakerRealtimeClock clock(maker);
  constexpr auto ntpTo1Jan1970Ms = webrtc::kNtpJan1970 * 1000LL;
  for (int i = 1000; i < 20000; i += 93) {
    const auto t = kWebrtcTimeOffset + webrtc::TimeDelta::Micros(i);
    const auto ntp = clock.ConvertTimestampToNtpTime(t);
    // Because of precision differences, these round to a specific millisecond
    // slightly differently.
    EXPECT_NEAR(ntp.ToMs() - ntpTo1Jan1970Ms,
                RTCStatsTimestamp::FromRealtime(maker, t).To1Jan1970().ms(),
                1.0)
        << " for i=" << i;
  }
}

TEST(RTCStatsTimestampMaker, ConvertNtpToDomTime)
{
  auto maker = RTCStatsTimestampMaker::Create();
  RTCStatsTimestampMakerRealtimeClock clock(maker);
  for (int i = 1000; i < 20000; i += 93) {
    const auto t = kWebrtcTimeOffset + webrtc::TimeDelta::Micros(i);
    const auto ntp = clock.ConvertTimestampToNtpTime(t);
    const auto dom =
        RTCStatsTimestamp::FromNtp(maker, webrtc::Timestamp::Millis(ntp.ToMs()))
            .ToDom();
    // Because of precision differences, these round to a specific millisecond
    // slightly differently.
    EXPECT_NEAR(std::lround(dom),
                std::lround(RTCStatsTimestamp::FromRealtime(maker, t).ToDom()),
                1.0)
        << " for i=" << i;
  }
}

TEST(RTCStatsTimestampMaker, ConvertMozTime)
{
  auto maker = RTCStatsTimestampMaker::Create();
  const auto start = TimeStamp::Now();
  RTCStatsTimestampMakerRealtimeClock clock(maker);
  for (int i = 1000; i < 20000; i += 93) {
    const auto duration = TimeDuration::FromMicroseconds(i);
    const auto time = RTCStatsTimestamp::FromMozTime(maker, start + duration);
    EXPECT_EQ(duration.ToMicroseconds(),
              (time.ToMozTime() - start).ToMicroseconds())
        << " for i=" << i;
  }
}

TEST(RTCStatsTimestampMaker, ConvertRealtime)
{
  auto maker = RTCStatsTimestampMaker::Create();
  const auto start = kWebrtcTimeOffset;
  RTCStatsTimestampMakerRealtimeClock clock(maker);
  for (int i = 1000; i < 20000; i += 93) {
    const auto duration = webrtc::TimeDelta::Micros(i);
    const auto time = RTCStatsTimestamp::FromRealtime(maker, start + duration);
    // Because of precision differences, these round to a specific Microsecond
    // slightly differently.
    EXPECT_NEAR(duration.us(), (time.ToRealtime() - start).us(), 1)
        << " for i=" << i;
  }
}

TEST(RTCStatsTimestampMaker, Convert1Jan1970)
{
  auto maker = RTCStatsTimestampMaker::Create();
  const auto start =
      kWebrtcTimeOffset +
      webrtc::TimeDelta::Millis(PerformanceService::GetOrCreate()->TimeOrigin(
          WebrtcSystemTimeBase()));
  RTCStatsTimestampMakerRealtimeClock clock(maker);
  for (int i = 1000; i < 20000; i += 93) {
    const auto duration = webrtc::TimeDelta::Micros(i);
    const auto time = RTCStatsTimestamp::From1Jan1970(maker, start + duration);
    // Because of precision differences, these round to a specific Microsecond
    // slightly differently.
    EXPECT_NEAR(duration.us(), (time.To1Jan1970() - start).us(), 1)
        << " for i=" << i;
  }
}

TEST(RTCStatsTimestampMaker, ConvertDomRealtime)
{
  auto maker = RTCStatsTimestampMaker::Create();
  const auto start = kWebrtcTimeOffset;
  RTCStatsTimestampMakerRealtimeClock clock(maker);
  for (int i = 1000; i < 20000; i += 93) {
    const auto duration = webrtc::TimeDelta::Micros(i);
    const auto time =
        RTCStatsTimestamp::FromDomRealtime(maker, start + duration);
    // Because of precision differences, these round to a specific Microsecond
    // slightly differently.
    EXPECT_NEAR(duration.us(), (time.ToDomRealtime() - start).us(), 1)
        << " for i=" << i;
  }
}
