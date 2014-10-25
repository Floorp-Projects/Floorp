/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "OpusTrackEncoder.h"

using namespace mozilla;

class TestOpusTrackEncoder : public OpusTrackEncoder
{
public:
  // Return true if it has successfully initialized the Opus encoder.
  bool TestOpusCreation(int aChannels, int aSamplingRate)
  {
    if (Init(aChannels, aSamplingRate) == NS_OK) {
      if (GetPacketDuration()) {
        return true;
      }
    }
    return false;
  }

  // Return the sample rate of data to be fed to the Opus encoder, could be
  // re-sampled if it was not one of the Opus supported sampling rates.
  // Init() is expected to be called first.
  int TestGetOutputSampleRate()
  {
    return mInitialized ? GetOutputSampleRate() : 0;
  }
};

static bool
TestOpusInit(int aChannels, int aSamplingRate)
{
  TestOpusTrackEncoder encoder;
  return encoder.TestOpusCreation(aChannels, aSamplingRate);
}

static int
TestOpusResampler(int aChannels, int aSamplingRate)
{
  TestOpusTrackEncoder encoder;
  EXPECT_TRUE(encoder.TestOpusCreation(aChannels, aSamplingRate));
  return encoder.TestGetOutputSampleRate();
}

TEST(Media, OpusEncoder_Init)
{
  // Expect false with 0 or negative channels of input signal.
  EXPECT_FALSE(TestOpusInit(0, 16000));
  EXPECT_FALSE(TestOpusInit(-1, 16000));

  // The Opus format supports up to 8 channels, and supports multitrack audio up
  // to 255 channels, but the current implementation supports only mono and
  // stereo, and downmixes any more than that.
  // Expect false with channels of input signal exceed the max supported number.
  EXPECT_FALSE(TestOpusInit(8 + 1, 16000));

  // Should accept channels within valid range.
  for (int i = 1; i <= 8; i++) {
    EXPECT_TRUE(TestOpusInit(i, 16000));
  }

  // Expect false with 0 or negative sampling rate of input signal.
  EXPECT_FALSE(TestOpusInit(1, 0));
  EXPECT_FALSE(TestOpusInit(1, -1));
}

TEST(Media, OpusEncoder_Resample)
{
  // Sampling rates of data to be fed to Opus encoder, should remain unchanged
  // if it is one of Opus supported rates (8000, 12000, 16000, 24000 and 48000
  // (kHz)) at initialization.
  EXPECT_TRUE(TestOpusResampler(1, 8000) == 8000);
  EXPECT_TRUE(TestOpusResampler(1, 12000) == 12000);
  EXPECT_TRUE(TestOpusResampler(1, 16000) == 16000);
  EXPECT_TRUE(TestOpusResampler(1, 24000) == 24000);
  EXPECT_TRUE(TestOpusResampler(1, 48000) == 48000);

  // Otherwise, it should be resampled to 48kHz by resampler.
  EXPECT_FALSE(TestOpusResampler(1, 9600) == 9600);
  EXPECT_FALSE(TestOpusResampler(1, 44100) == 44100);
}
