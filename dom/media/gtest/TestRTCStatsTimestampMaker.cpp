/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cmath>

#include "gtest/gtest.h"
#include "libwebrtcglue/SystemTime.h"

using namespace mozilla;
using namespace dom;

TEST(RTCStatsTimestampMakerRealtimeClock, ConvertTimestampToNtpTime)
{
  auto maker = RTCStatsTimestampMaker::Create();
  RTCStatsTimestampMakerRealtimeClock clock(maker);
  constexpr auto ntpTo1Jan1970Ms = webrtc::kNtpJan1970 * 1000LL;
  for (int i = 1000; i < 20000; i += 93) {
    const auto t = webrtc::Timestamp::Micros(i);
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
    const auto t = webrtc::Timestamp::Micros(i);
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
