/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "ClockDrift.h"
#include "mozilla/Maybe.h"

using namespace mozilla;

// Runs UpdateClock() and checks that the reported correction level doesn't
// change for enough time to trigger a correction update on the first
// following UpdateClock(). Returns the first reported correction level.
static float RunUntilCorrectionUpdate(ClockDrift& aC, uint32_t aSource,
                                      uint32_t aTarget, uint32_t aBuffering,
                                      uint32_t aSaturation,
                                      uint32_t aSourceOffset = 0,
                                      uint32_t aTargetOffset = 0) {
  float correction = aC.GetCorrection();
  for (uint32_t s = aSourceOffset, t = aTargetOffset;
       s < aC.mSourceRate && t < aC.mTargetRate; s += aSource, t += aTarget) {
    EXPECT_FLOAT_EQ(aC.GetCorrection(), correction);
    aC.UpdateClock(aSource, aTarget, aBuffering, aSaturation);
  }
  return correction;
};

TEST(TestClockDrift, Basic)
{
  // Keep buffered frames to the wanted level in order to not affect that test.
  const uint32_t buffered = 5 * 480;

  ClockDrift c(48000, 48000, buffered);
  EXPECT_EQ(c.GetCorrection(), 1.0);

  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480, buffered, buffered),
                  1.0);
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 480, 480 + 48, buffered, buffered), 1.0);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480, buffered, buffered),
                  1.06);
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 480 + 48, 480, buffered, buffered), 1.024);

  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.95505452);
}

TEST(TestClockDrift, BasicResampler)
{
  // Keep buffered frames to the wanted level in order to not affect that test.
  const uint32_t buffered = 5 * 240;

  ClockDrift c(24000, 48000, buffered);

  // Keep buffered frames to the wanted level in order to not affect that test.
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 240, 480, buffered, buffered),
                  1.0);

  // +10%
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 240, 480 + 48, buffered, buffered), 1.0);

  // +10%
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 240 + 24, 480, buffered, buffered), 1.06);

  // -10%
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 240, 480 - 48, buffered, buffered),
      0.96945453);

  // +5%, -5%
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 240 + 12, 480 - 24, buffered, buffered),
      0.92778182);

  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.91396987);
}

TEST(TestClockDrift, BufferedInput)
{
  ClockDrift c(48000, 48000, 5 * 480);
  EXPECT_EQ(c.GetCorrection(), 1.0);

  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 480, 480, 5 * 480, 8 * 480, 480, 480), 1.0);

  c.UpdateClock(480, 480, 0, 10 * 480);  // 0 buffered when updating correction
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0473685);
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 480, 480, 3 * 480, 7 * 480, 480, 480),
      1.0473685);

  c.UpdateClock(480, 480, 3 * 480, 7 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0311923);
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 480, 480, 5 * 480, 5 * 480, 480, 480),
      1.0311923);

  c.UpdateClock(480, 480, 5 * 480, 5 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0124769);
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 480, 480, 7 * 480, 3 * 480, 480, 480),
      1.0124769);

  c.UpdateClock(480, 480, 7 * 480, 3 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.99322605);
}

TEST(TestClockDrift, BufferedInputWithResampling)
{
  ClockDrift c(24000, 48000, 5 * 240);
  EXPECT_EQ(c.GetCorrection(), 1.0);

  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 240, 480, 5 * 240, 5 * 240, 240, 480), 1.0);

  c.UpdateClock(240, 480, 0, 10 * 240);  // 0 buffered when updating correction
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0473685);
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 240, 480, 3 * 240, 7 * 240, 240, 480),
      1.0473685);

  c.UpdateClock(240, 480, 3 * 240, 7 * 240);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0311923);
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 240, 480, 5 * 240, 5 * 240, 240, 480),
      1.0311923);

  c.UpdateClock(240, 480, 5 * 240, 5 * 240);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0124769);
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 240, 480, 7 * 240, 3 * 240, 240, 480),
      1.0124769);

  c.UpdateClock(240, 480, 7 * 240, 3 * 240);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.99322605);
}

TEST(TestClockDrift, Clamp)
{
  // Keep buffered frames to the wanted level in order to not affect that test.
  const uint32_t buffered = 5 * 480;

  ClockDrift c(48000, 48000, buffered);

  // +30%
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 480, 480 + 3 * 48, buffered, buffered), 1.0);

  // -30%
  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 480, 480 - 3 * 48, buffered, buffered), 1.1);

  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.9);
}

TEST(TestClockDrift, SmallDiff)
{
  // Keep buffered frames to the wanted level in order to not affect that test.
  const uint32_t buffered = 5 * 480;

  ClockDrift c(48000, 48000, buffered);

  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480 + 4, 480, buffered, buffered),
                  1.0);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480 + 5, 480, buffered, buffered),
                  0.99504131);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480, buffered, buffered),
                  0.991831);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480 + 4, buffered, buffered),
                  0.99673241);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.003693);
}

TEST(TestClockDrift, SmallBufferedFrames)
{
  ClockDrift c(48000, 48000, 5 * 480);

  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  for (uint32_t i = 0; i < 10; ++i) {
    c.UpdateClock(480, 480, 5 * 480, 5 * 480);
  }
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  c.UpdateClock(480, 480, 0, 10 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.1);

  EXPECT_FLOAT_EQ(
      RunUntilCorrectionUpdate(c, 480, 480, 5 * 480, 5 * 480, 24000, 24000),
      1.1);
  c.UpdateClock(480, 480, 0, 10 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.1);
}
