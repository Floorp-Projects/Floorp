/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDriftCorrection.h"
#include "AudioGenerator.h"
#include "AudioVerifier.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

// Runs UpdateClock() and checks that the reported correction level doesn't
// change for enough time to trigger a correction update on the first
// following UpdateClock(). Returns the first reported correction level.
static float RunUntilCorrectionUpdate(ClockDrift& aC, int aSource, int aTarget,
                                      int aBuffering, int aSourceOffset = 0,
                                      int aTargetOffset = 0) {
  Maybe<float> correction;
  for (int s = aSourceOffset, t = aTargetOffset;
       s < aC.mSourceRate && t < aC.mTargetRate; s += aSource, t += aTarget) {
    aC.UpdateClock(aSource, aTarget, aBuffering);
    if (correction) {
      EXPECT_FLOAT_EQ(aC.GetCorrection(), *correction)
          << "s=" << s << "; t=" << t;
    } else {
      correction = Some(aC.GetCorrection());
    }
  }
  return *correction;
};

TEST(TestClockDrift, Basic)
{
  // Keep buffered frames to the wanted level in order to not affect that test.
  const int buffered = 5 * 480;

  ClockDrift c(48000, 48000, buffered);
  EXPECT_EQ(c.GetCorrection(), 1.0);

  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480, buffered), 1.0);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480 + 48, buffered), 1.0);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480, buffered), 1.1);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480 + 48, 480, buffered), 1.0);

  c.UpdateClock(0, 0, 5 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.90909094);
}

TEST(TestClockDrift, BasicResampler)
{
  // Keep buffered frames to the wanted level in order to not affect that test.
  const int buffered = 5 * 240;

  ClockDrift c(24000, 48000, buffered);

  // Keep buffered frames to the wanted level in order to not affect that test.
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 240, 480, buffered), 1.0);

  // +10%
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 240, 480 + 48, buffered), 1.0);

  // +10%
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 240 + 24, 480, buffered), 1.1);

  // -10%
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 240, 480 - 48, buffered),
                  0.90909094);

  // +5%, -5%
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 240 + 12, 480 - 24, buffered),
                  0.90909094);

  c.UpdateClock(0, 0, buffered);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.90909094);
}

TEST(TestClockDrift, BufferedInput)
{
  ClockDrift c(48000, 48000, 5 * 480);
  EXPECT_EQ(c.GetCorrection(), 1.0);

  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480, 5 * 480), 1.0);

  c.UpdateClock(480, 480, 0);  // 0 buffered when updating correction
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0526316);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480, 2 * 480, 480, 480),
                  1.0526316);

  c.UpdateClock(480, 480, 2 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0309278);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480, 5 * 480, 480, 480),
                  1.0309278);

  c.UpdateClock(480, 480, 5 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480, 7 * 480, 480, 480),
                  1.0);

  c.UpdateClock(480, 480, 7 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.980392);
}

TEST(TestClockDrift, BufferedInputWithResampling)
{
  ClockDrift c(24000, 48000, 5 * 240);
  EXPECT_EQ(c.GetCorrection(), 1.0);

  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 240, 480, 5 * 240), 1.0);

  c.UpdateClock(240, 480, 0);  // 0 buffered when updating correction
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0526316);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 240, 480, 2 * 240, 240, 480),
                  1.0526316);

  c.UpdateClock(240, 480, 2 * 240);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0309278);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 240, 480, 5 * 240, 240, 480),
                  1.0309278);

  c.UpdateClock(240, 480, 5 * 240);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 240, 480, 7 * 240, 240, 480),
                  1.0);

  c.UpdateClock(240, 480, 7 * 240);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.980392);
}

TEST(TestClockDrift, Clamp)
{
  // Keep buffered frames to the wanted level in order to not affect that test.
  const int buffered = 5 * 480;

  ClockDrift c(48000, 48000, buffered);

  // +20%
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480 + 2 * 48, buffered),
                  1.0);

  // -20%
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480 - 2 * 48, buffered),
                  1.1);

  c.UpdateClock(0, 0, buffered);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.9);
}

TEST(TestClockDrift, SmallDiff)
{
  // Keep buffered frames to the wanted level in order to not affect that test.
  const int buffered = 5 * 480;

  ClockDrift c(48000, 48000, buffered);

  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480 + 4, 480, buffered), 1.0);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480 + 5, 480, buffered), 1.0);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480, buffered), 0.98969072);
  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480 + 4, buffered), 1.0);
  c.UpdateClock(0, 0, buffered);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0083333);
}

TEST(TestClockDrift, SmallBufferedFrames)
{
  ClockDrift c(48000, 48000, 5 * 480);

  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  c.UpdateClock(480, 480, 5 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  c.UpdateClock(480, 480, 0);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0526316);

  EXPECT_FLOAT_EQ(RunUntilCorrectionUpdate(c, 480, 480, 5 * 480, 24000, 24000),
                  1.0526316);
  c.UpdateClock(480, 480, 0);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.1);
}

// Print the mono channel of a segment.
void printAudioSegment(const AudioSegment& segment) {
  for (AudioSegment::ConstChunkIterator iter(segment); !iter.IsEnded();
       iter.Next()) {
    const AudioChunk& c = *iter;
    for (int i = 0; i < c.GetDuration(); ++i) {
      if (c.mBufferFormat == AUDIO_FORMAT_FLOAT32) {
        printf("%f\n", c.ChannelData<float>()[0][i]);
      } else {
        printf("%d\n", c.ChannelData<int16_t>()[0][i]);
      }
    }
  }
}

template <class T>
AudioChunk CreateAudioChunk(uint32_t aFrames, int aChannels,
                            AudioSampleFormat aSampleFormat);

void testAudioCorrection(int32_t aSourceRate, int32_t aTargetRate) {
  const uint32_t channels = 1;
  const uint32_t sampleRateTransmitter = aSourceRate;
  const uint32_t sampleRateReceiver = aTargetRate;
  const uint32_t frequency = 100;
  AudioDriftCorrection ad(sampleRateTransmitter, sampleRateReceiver);

  AudioGenerator<AudioDataValue> tone(channels, sampleRateTransmitter,
                                      frequency);
  AudioVerifier<AudioDataValue> inToneVerifier(sampleRateTransmitter,
                                               frequency);
  AudioVerifier<AudioDataValue> outToneVerifier(sampleRateReceiver, frequency);

  uint32_t sourceFrames;
  const uint32_t targetFrames = sampleRateReceiver / 100;

  // Run for some time: 3 * 1050 = 3150 iterations
  for (int j = 0; j < 3; ++j) {
    // apply some drift
    if (j % 2 == 0) {
      sourceFrames = sampleRateTransmitter / 100 + 10;
    } else {
      sourceFrames = sampleRateTransmitter / 100 - 10;
    }

    // 10.5 seconds, allows for at least 10 correction changes, to stabilize
    // around the desired buffer.
    for (int n = 0; n < 1050; ++n) {
      // Create the input (sine tone)
      AudioSegment inSegment;
      tone.Generate(inSegment, sourceFrames);
      inToneVerifier.AppendData(inSegment);
      // Print the input for debugging
      // printAudioSegment(inSegment);

      // Get the output of the correction
      AudioSegment outSegment = ad.RequestFrames(inSegment, targetFrames);
      EXPECT_EQ(outSegment.GetDuration(), targetFrames);
      // Print the output for debugging
      // printAudioSegment(outSegment);
      outToneVerifier.AppendData(outSegment);
    }
  }

  EXPECT_NEAR(ad.CurrentBuffering(),
              ad.mDesiredBuffering - sampleRateTransmitter / 100, 512);

  EXPECT_EQ(inToneVerifier.EstimatedFreq(), tone.mFrequency);
  EXPECT_EQ(inToneVerifier.PreSilenceSamples(), 0U);
  EXPECT_EQ(inToneVerifier.CountDiscontinuities(), 0U);

  EXPECT_EQ(outToneVerifier.EstimatedFreq(), tone.mFrequency);
  // The expected pre-silence is 50ms plus the resampling, this is roughly more
  // than 2000 frames for the samples rates being used here
  EXPECT_GT(outToneVerifier.PreSilenceSamples(), 2000U);
  EXPECT_EQ(outToneVerifier.CountDiscontinuities(), 0U);
}

TEST(TestAudioDriftCorrection, Basic)
{
  testAudioCorrection(48000, 48000);
  testAudioCorrection(48000, 44100);
  testAudioCorrection(44100, 48000);
}

void testMonoToStereoInput(int aSourceRate, int aTargetRate) {
  const uint32_t frequency = 100;
  const uint32_t sampleRateTransmitter = aSourceRate;
  const uint32_t sampleRateReceiver = aTargetRate;
  AudioDriftCorrection ad(sampleRateTransmitter, sampleRateReceiver);

  AudioGenerator<AudioDataValue> monoTone(1, sampleRateTransmitter, frequency);
  AudioGenerator<AudioDataValue> stereoTone(2, sampleRateTransmitter,
                                            frequency);
  AudioVerifier<AudioDataValue> inToneVerify(sampleRateTransmitter, frequency);
  AudioVerifier<AudioDataValue> outToneVerify(sampleRateReceiver, frequency);

  uint32_t sourceFrames;
  const uint32_t targetFrames = sampleRateReceiver / 100;

  // Run for some time: 6 * 250 = 1500 iterations
  for (int j = 0; j < 6; ++j) {
    // apply some drift
    if (j % 2 == 0) {
      sourceFrames = sampleRateTransmitter / 100 + 10;
    } else {
      sourceFrames = sampleRateTransmitter / 100 - 10;
    }

    for (int n = 0; n < 250; ++n) {
      // Create the input (sine tone) of two chunks.
      AudioSegment inSegment;
      monoTone.Generate(inSegment, sourceFrames / 2);
      stereoTone.SetOffset(monoTone.Offset());
      stereoTone.Generate(inSegment, sourceFrames / 2);
      monoTone.SetOffset(stereoTone.Offset());
      inToneVerify.AppendData(inSegment);
      // Print the input for debugging
      // printAudioSegment(inSegment);

      // Get the output of the correction
      AudioSegment outSegment = ad.RequestFrames(inSegment, targetFrames);
      EXPECT_EQ(outSegment.GetDuration(), targetFrames);
      // Print the output for debugging
      // printAudioSegment(outSegment);
      outToneVerify.AppendData(outSegment);
    }
  }
  EXPECT_EQ(inToneVerify.EstimatedFreq(), frequency);
  EXPECT_EQ(inToneVerify.PreSilenceSamples(), 0U);
  EXPECT_EQ(inToneVerify.CountDiscontinuities(), 0U);

  EXPECT_GT(outToneVerify.CountDiscontinuities(), 0U)
      << "Expect discontinuities";
  EXPECT_NE(outToneVerify.EstimatedFreq(), frequency)
      << "Estimation is not accurate due to discontinuities";
  // The expected pre-silence is 50ms plus the resampling. However, due to
  // discontinuities pre-silence is expected only in the first iteration which
  // is routhly a little more than 400 frames for the chosen sample rates.
  EXPECT_GT(outToneVerify.PreSilenceSamples(), 400U);
}

TEST(TestAudioDriftCorrection, MonoToStereoInput)
{
  testMonoToStereoInput(48000, 48000);
  testMonoToStereoInput(48000, 44100);
  testMonoToStereoInput(44100, 48000);
}

TEST(TestAudioDriftCorrection, NotEnoughFrames)
{
  const uint32_t sampleRateTransmitter = 48000;
  const uint32_t sampleRateReceiver = 48000;
  AudioDriftCorrection ad(sampleRateTransmitter, sampleRateReceiver);
  const uint32_t targetFrames = sampleRateReceiver / 100;

  for (int i = 0; i < 7; ++i) {
    // Input is something small, 10 frames here, in order to dry out fast,
    // after 4 iterations
    AudioChunk chunk = CreateAudioChunk<float>(10, 1, AUDIO_FORMAT_FLOAT32);
    AudioSegment inSegment;
    inSegment.AppendAndConsumeChunk(&chunk);

    AudioSegment outSegment = ad.RequestFrames(inSegment, targetFrames);
    EXPECT_EQ(outSegment.GetDuration(), targetFrames);
    if (i < 5) {
      EXPECT_FALSE(outSegment.IsNull());
    } else {
      // Last 2 iterations, the 5th and 6th, will be null. It has used all
      // buffered data so the output is silence.
      EXPECT_TRUE(outSegment.IsNull());
    }
  }
}

TEST(TestAudioDriftCorrection, CrashInAudioResampler)
{
  const uint32_t sampleRateTransmitter = 48000;
  const uint32_t sampleRateReceiver = 48000;
  AudioDriftCorrection ad(sampleRateTransmitter, sampleRateReceiver);
  const uint32_t targetFrames = sampleRateReceiver / 100;

  for (int i = 0; i < 100; ++i) {
    AudioChunk chunk = CreateAudioChunk<float>(sampleRateTransmitter / 1000, 1,
                                               AUDIO_FORMAT_FLOAT32);
    AudioSegment inSegment;
    inSegment.AppendAndConsumeChunk(&chunk);

    AudioSegment outSegment = ad.RequestFrames(inSegment, targetFrames);
    EXPECT_EQ(outSegment.GetDuration(), targetFrames);
  }
}
