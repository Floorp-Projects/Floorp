/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "DriftCompensation.h"

using namespace mozilla;

class DriftCompensatorTest : public ::testing::Test {
 public:
  const TrackRate mRate = 44100;
  const TimeStamp mStart;
  const RefPtr<DriftCompensator> mComp;

  DriftCompensatorTest()
      : mStart(TimeStamp::Now()),
        mComp(MakeRefPtr<DriftCompensator>(GetCurrentThreadEventTarget(),
                                           mRate)) {
    mComp->NotifyAudioStart(mStart);
    // NotifyAudioStart dispatched a runnable to update the audio mStart time on
    // the video thread. Because this is a test, the video thread is the current
    // thread. We spin the event loop until we know the mStart time is updated.
    {
      bool updated = false;
      NS_DispatchToCurrentThread(
          NS_NewRunnableFunction(__func__, [&] { updated = true; }));
      SpinEventLoopUntil([&] { return updated; });
    }
  }

  // Past() is half as far from `mStart` as `aNow`.
  TimeStamp Past(TimeStamp aNow) {
    return mStart + (aNow - mStart) / (int64_t)2;
  }

  // Future() is twice as far from `mStart` as `aNow`.
  TimeStamp Future(TimeStamp aNow) { return mStart + (aNow - mStart) * 2; }
};

TEST_F(DriftCompensatorTest, Initialized) {
  EXPECT_EQ(mComp->GetVideoTime(mStart, mStart), mStart);
}

TEST_F(DriftCompensatorTest, SlowerAudio) {
  // 10s of audio took 20 seconds of wall clock to play out
  mComp->NotifyAudio(mRate * 10);
  TimeStamp now = mStart + TimeDuration::FromSeconds(20);
  EXPECT_EQ((mComp->GetVideoTime(now, mStart) - mStart).ToSeconds(), 0.0);
  EXPECT_EQ((mComp->GetVideoTime(now, Past(now)) - mStart).ToSeconds(), 5.0);
  EXPECT_EQ((mComp->GetVideoTime(now, now) - mStart).ToSeconds(), 10.0);
  EXPECT_EQ((mComp->GetVideoTime(now, Future(now)) - mStart).ToSeconds(), 20.0);
}

TEST_F(DriftCompensatorTest, NoDrift) {
  // 10s of audio took 10 seconds of wall clock to play out
  mComp->NotifyAudio(mRate * 10);
  TimeStamp now = mStart + TimeDuration::FromSeconds(10);
  EXPECT_EQ((mComp->GetVideoTime(now, mStart) - mStart).ToSeconds(), 0.0);
  EXPECT_EQ((mComp->GetVideoTime(now, Past(now)) - mStart).ToSeconds(), 5.0);
  EXPECT_EQ((mComp->GetVideoTime(now, now) - mStart).ToSeconds(), 10.0);
  EXPECT_EQ((mComp->GetVideoTime(now, Future(now)) - mStart).ToSeconds(), 20.0);
}

TEST_F(DriftCompensatorTest, NoProgress) {
  // 10s of audio took 0 seconds of wall clock to play out
  mComp->NotifyAudio(mRate * 10);
  TimeStamp now = mStart;
  TimeStamp future = mStart + TimeDuration::FromSeconds(5);
  EXPECT_EQ((mComp->GetVideoTime(now, mStart) - mStart).ToSeconds(), 0.0);
  EXPECT_EQ((mComp->GetVideoTime(now, future) - mStart).ToSeconds(), 5.0);
}

TEST_F(DriftCompensatorTest, FasterAudio) {
  // 20s of audio took 10 seconds of wall clock to play out
  mComp->NotifyAudio(mRate * 20);
  TimeStamp now = mStart + TimeDuration::FromSeconds(10);
  EXPECT_EQ((mComp->GetVideoTime(now, mStart) - mStart).ToSeconds(), 0.0);
  EXPECT_EQ((mComp->GetVideoTime(now, Past(now)) - mStart).ToSeconds(), 10.0);
  EXPECT_EQ((mComp->GetVideoTime(now, now) - mStart).ToSeconds(), 20.0);
  EXPECT_EQ((mComp->GetVideoTime(now, Future(now)) - mStart).ToSeconds(), 40.0);
}
