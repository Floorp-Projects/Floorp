/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "DriftController.h"
#include "mozilla/Maybe.h"

using namespace mozilla;

TEST(TestDriftController, Basic)
{
  // The buffer level is the only input to the controller logic.
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = 3 * 480;
  constexpr uint32_t bufferedHigh = 7 * 480;

  DriftController c(48000, 48000, media::TimeUnit::FromSeconds(0.05));
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000U);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(48000, 48000);

  c.UpdateClock(oneSec, oneSec, buffered, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  c.UpdateClock(oneSec, oneSec, bufferedLow, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47952u);

  c.UpdateClock(oneSec, oneSec, bufferedHigh, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  c.UpdateClock(oneSec, oneSec, bufferedHigh, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48048u);
}

TEST(TestDriftController, BasicResampler)
{
  // The buffer level is the only input to the controller logic.
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = 3 * 480;
  constexpr uint32_t bufferedHigh = 7 * 480;

  DriftController c(48000, 24000, media::TimeUnit::FromSeconds(0.05));

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(48000, 48000);

  c.UpdateClock(oneSec, oneSec, buffered, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // low
  c.UpdateClock(oneSec, oneSec, bufferedLow, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47952u);

  // high
  c.UpdateClock(oneSec, oneSec, bufferedHigh, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // high
  c.UpdateClock(oneSec, oneSec, bufferedHigh, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48048u);
}

TEST(TestDriftController, BufferedInput)
{
  // The buffer level is the only input to the controller logic.
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = 3 * 480;
  constexpr uint32_t bufferedHigh = 7 * 480;

  DriftController c(48000, 48000, media::TimeUnit::FromSeconds(0.05));
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(48000, 48000);

  c.UpdateClock(oneSec, oneSec, buffered, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // 0 buffered when updating correction
  c.UpdateClock(oneSec, oneSec, 0, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47952u);

  c.UpdateClock(oneSec, oneSec, bufferedLow, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  c.UpdateClock(oneSec, oneSec, buffered, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  c.UpdateClock(oneSec, oneSec, bufferedHigh, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48048u);
}

TEST(TestDriftController, BufferedInputWithResampling)
{
  // The buffer level is the only input to the controller logic.
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = 3 * 480;
  constexpr uint32_t bufferedHigh = 7 * 480;

  DriftController c(48000, 24000, media::TimeUnit::FromSeconds(0.05));
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(24000, 24000);

  c.UpdateClock(oneSec, oneSec, buffered, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // 0 buffered when updating correction
  c.UpdateClock(oneSec, oneSec, 0, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47952u);

  c.UpdateClock(oneSec, oneSec, bufferedLow, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  c.UpdateClock(oneSec, oneSec, buffered, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  c.UpdateClock(oneSec, oneSec, bufferedHigh, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48048u);
}

TEST(TestDriftController, SmallError)
{
  // The buffer level is the only input to the controller logic.
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = buffered - 48;
  constexpr uint32_t bufferedHigh = buffered + 48;

  DriftController c(48000, 48000, media::TimeUnit::FromSeconds(0.05));
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(48000, 48000);

  c.UpdateClock(oneSec, oneSec, buffered, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  c.UpdateClock(oneSec, oneSec, bufferedLow, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  c.UpdateClock(oneSec, oneSec, bufferedHigh, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);
  c.UpdateClock(oneSec, oneSec, bufferedHigh, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);
}

TEST(TestDriftController, SmallBufferedFrames)
{
  // The buffer level is the only input to the controller logic.
  constexpr uint32_t bufferedLow = 3 * 480;

  DriftController c(48000, 48000, media::TimeUnit::FromSeconds(0.05));
  media::TimeUnit oneSec = media::TimeUnit::FromSeconds(1);
  media::TimeUnit hundredMillis = oneSec / 10;

  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000U);
  for (uint32_t i = 0; i < 9; ++i) {
    c.UpdateClock(hundredMillis, hundredMillis, bufferedLow, 0);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000U);
  c.UpdateClock(hundredMillis, hundredMillis, bufferedLow, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47952U);
}

TEST(TestDriftController, VerySmallBufferedFrames)
{
  // The buffer level is the only input to the controller logic.
  uint32_t bufferedLow = 1;
  uint32_t nominalRate = 48000;

  DriftController c(nominalRate, nominalRate, media::TimeUnit::FromSeconds(1));
  media::TimeUnit oneSec = media::TimeUnit::FromSeconds(1);
  media::TimeUnit sourceDuration(1, nominalRate);

  EXPECT_EQ(c.GetCorrectedSourceRate(), nominalRate);
  uint32_t previousCorrected = nominalRate;
  // Steps are limited to nominalRate/1000.
  // Perform 1001 steps to check the corrected rate does not underflow zero.
  for (uint32_t i = 0; i < 1001; ++i) {
    c.UpdateClock(sourceDuration, oneSec, bufferedLow, 0);
    uint32_t correctedRate = c.GetCorrectedSourceRate();
    EXPECT_LE(correctedRate, previousCorrected) << "for i=" << i;
    EXPECT_GT(correctedRate, 0u) << "for i=" << i;
    previousCorrected = correctedRate;
  }
}
