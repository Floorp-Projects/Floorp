/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaSession.h"

using namespace mozilla;
using namespace mozilla::dom;

struct CurrentPlaybackPositionFixture {
  double mDuration = 0.0;
  double mPlaybackRate = 0.0;
  double mLastReportedTime = 0.0;

  TimeDuration mDelta;
  double mExpectedPosition = -1;
};

class CurrentPlaybackPositionTest
    : public testing::TestWithParam<CurrentPlaybackPositionFixture> {};

static const std::initializer_list<CurrentPlaybackPositionFixture> kFixtures = {
    // position must be positive
    {
        10.0,
        1.0,
        0.0,
        TimeDuration::FromSeconds(-1.0),
        0.0,
    },
    // no time elapsed
    {
        10.0,
        1.0,
        0.0,
        TimeDuration::FromSeconds(0.0),
        0.0,
    },
    {
        10.0,
        1.0,
        0.0,
        TimeDuration::FromSeconds(3.0),
        3.0,
    },
    {
        10.0,
        1.0,
        0.0,
        TimeDuration::FromSeconds(10.0),
        10.0,
    },
    // position is clamped to the duration
    {
        10.0,
        1.0,
        0.0,
        TimeDuration::FromSeconds(20.0),
        10.0,
    },
    {
        10.0,
        1.0,
        5.0,
        TimeDuration::FromSeconds(-1.0),
        4.0,
    },
    {
        10.0,
        1.0,
        5.0,
        TimeDuration::FromSeconds(-6.0),
        0.0,
    },
    {
        10.0,
        1.0,
        5.0,
        TimeDuration::FromSeconds(6.0),
        10.0,
    },
    // expected: 5s + 2 * 2s
    {
        10.0,
        2.0,
        5.0,
        TimeDuration::FromSeconds(2.0),
        9.0,
    },
    // expected: 5s + 0.5 * 2s
    {
        10.0,
        0.5,
        5.0,
        TimeDuration::FromSeconds(2.0),
        6.0,
    },
    {
        5.0,
        4.0,
        10.0,
        TimeDuration::FromSeconds(20.0),
        5.0,
    },
    // empty media (0s)
    {
        0.0,
        4.0,
        5.0,
        TimeDuration::FromSeconds(20.0),
        0.0,
    },
};

TEST_P(CurrentPlaybackPositionTest, Run) {
  const auto& fixture = GetParam();
  PositionState state(fixture.mDuration, fixture.mPlaybackRate,
                      fixture.mLastReportedTime, TimeStamp::Now());

  ASSERT_DOUBLE_EQ(state.CurrentPlaybackPosition(state.mPositionUpdatedTime +
                                                 fixture.mDelta),
                   fixture.mExpectedPosition);
}

INSTANTIATE_TEST_SUITE_P(PositionState, CurrentPlaybackPositionTest,
                         testing::ValuesIn(kFixtures));
