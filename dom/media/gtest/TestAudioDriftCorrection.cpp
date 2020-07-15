/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDriftCorrection.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

TEST(TestClockDrift, Basic)
{
  ClockDrift c(48000, 48000);
  EXPECT_EQ(c.GetCorrection(), 1.0);

  // Keep buffered frames to the wanted level in order to not affect that test.
  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(480, 480, 5 * 480);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }

  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(480, 480 + 48, 5 * 480);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }

  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(480, 480, 5 * 480);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.1);
  }

  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(480 + 48, 480, 5 * 480);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }

  c.UpdateClock(0, 0, 5 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.90909094);
}

TEST(TestClockDrift, BasicResampler)
{
  ClockDrift c(24000, 48000);
  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(240, 480, 5 * 240);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }

  // Keep buffered frames to the wanted level in order to not affect that test.
  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(240, 480 + 48, 5 * 240);  // +10%
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }

  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(240 + 24, 480, 5 * 240);  // +10%
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.1);
  }

  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(240, 480 - 48, 5 * 240);  // -10%
    EXPECT_FLOAT_EQ(c.GetCorrection(), 0.90909094);
  }

  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(240 + 12, 480 - 24, 5 * 240);  //-5%, -5%
    EXPECT_FLOAT_EQ(c.GetCorrection(), 0.90909094);
  }

  c.UpdateClock(0, 0, 5 * 240);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.90909094);
}

TEST(TestClockDrift, BufferedInput)
{
  ClockDrift c(48000, 48000);
  EXPECT_EQ(c.GetCorrection(), 1.0);

  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(480, 480, 5 * 480);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }
  c.UpdateClock(480, 480, 0);  // 0 buffered on 100th iteration
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0526316);

  for (int i = 0; i < 99; ++i) {
    c.UpdateClock(480, 480, 2 * 480);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0526316);
  }
  c.UpdateClock(480, 480, 2 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0309278);

  for (int i = 0; i < 99; ++i) {
    c.UpdateClock(480, 480, 5 * 480);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0309278);
  }
  c.UpdateClock(480, 480, 5 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);

  for (int i = 0; i < 99; ++i) {
    c.UpdateClock(480, 480, 7 * 480);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }
  c.UpdateClock(480, 480, 7 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.980392);
}

TEST(TestClockDrift, BufferedInputWithResampling)
{
  ClockDrift c(24000, 48000);
  EXPECT_EQ(c.GetCorrection(), 1.0);

  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(240, 480, 5 * 240);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }
  c.UpdateClock(240, 480, 0);  // 0 buffered on 100th iteration
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0526316);

  for (int i = 0; i < 99; ++i) {
    c.UpdateClock(240, 480, 2 * 240);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0526316);
  }
  c.UpdateClock(240, 480, 2 * 240);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0309278);

  for (int i = 0; i < 99; ++i) {
    c.UpdateClock(240, 480, 5 * 240);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0309278);
  }
  c.UpdateClock(240, 480, 5 * 240);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);

  for (int i = 0; i < 99; ++i) {
    c.UpdateClock(240, 480, 7 * 240);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }
  c.UpdateClock(240, 480, 7 * 240);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.980392);
}

TEST(TestClockDrift, Clamp)
{
  ClockDrift c(48000, 48000);

  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(480, 480 + 2 * 48, 5 * 480);  // +20%
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }
  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(480, 480 - 2 * 48, 5 * 480);  // -20%
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.1);
  }
  c.UpdateClock(0, 0, 5 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 0.9);
}

TEST(TestClockDrift, SmallDiff)
{
  ClockDrift c(48000, 48000);

  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(480 + 4, 480, 5 * 480);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }
  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(480 + 5, 480, 5 * 480);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }
  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(480, 480, 5 * 480);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 0.98969072);
  }
  // Reset to 1.0 again
  for (int i = 0; i < 100; ++i) {
    c.UpdateClock(480, 480 + 4, 5 * 480);  // +0.83%
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  }
  c.UpdateClock(0, 0, 5 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0083333);
}

TEST(TestClockDrift, SmallBufferedFrames)
{
  ClockDrift c(48000, 48000);

  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  c.UpdateClock(480, 480, 5 * 480);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0);
  c.UpdateClock(480, 480, 0);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0526316);

  for (int i = 0; i < 50; ++i) {
    c.UpdateClock(480, 480, 5 * 480);
    EXPECT_FLOAT_EQ(c.GetCorrection(), 1.0526316);
  }
  c.UpdateClock(480, 480, 0);
  EXPECT_FLOAT_EQ(c.GetCorrection(), 1.1);
}

template <typename T>
class AudioToneGenerator {
 public:
  static_assert(std::is_same<T, int16_t>::value ||
                std::is_same<T, float>::value);

  explicit AudioToneGenerator(int32_t aRate) : mRate(aRate) {
    MOZ_ASSERT(mRate > 0);
  }

  void Write(AudioChunk& aChunk) {
    float time = mTime;
    for (uint32_t i = 0; i < aChunk.ChannelCount(); ++i) {
      mTime = time;  // reset the time for each channel
      T* buffer = aChunk.ChannelDataForWrite<T>(0);
      Write(buffer, aChunk.GetDuration());
    }
  }

  void Write(T* aBuffer, int32_t aFrames) {
    for (int i = 0; i < aFrames; ++i) {
      double value = Amplitude() * sin(2 * M_PI * mFrequency * mTime + mPhase);
      aBuffer[i] = static_cast<T>(value);
      mTime += mDeltaTime;
    }
  }

  T MaxMagnitudeDifference() {
    return static_cast<T>(Amplitude() *
                          sin(2 * M_PI * mFrequency * mDeltaTime + mPhase));
  }

  const int32_t mRate;
  const int32_t mFrequency = 100;
  const float mPhase = 0.0;
  const float mDeltaTime = 1.0f / mRate;

  static T Amplitude() {
    if (std::is_same<T, int16_t>::value) {
      return 19384;  // int16_t::max / 2
    }
    return 0.5f;  // 1.0f / 2
  }

 private:
  float mTime = 0.0;
};

template <typename T>
class AudioToneVerifier {
 public:
  explicit AudioToneVerifier(int32_t aRate)
      : mRate(aRate), mExpectedTone(aRate) {
    MOZ_ASSERT(mRate > 0);
  }

  // Only the mono channel is taken into account.
  void AppendData(const AudioSegment& segment) {
    for (AudioSegment::ConstChunkIterator iter(segment); !iter.IsEnded();
         iter.Next()) {
      const AudioChunk& c = *iter;
      CheckBuffer(c.ChannelData<T>()[0], c.GetDuration());
    }
  }

  int32_t EstimatedFreq() const {
    if (mTotalFramesSoFar == PreSilenceSamples() || mZeroCrossCount <= 1) {
      return 0.0f;
    }
    MOZ_ASSERT(mZeroCrossCount > 1);
    return mRate / (mSumPeriodInSamples / (mZeroCrossCount - 1));
  }

  int64_t PreSilenceSamples() const {
    // The first sample of the sinewave is zero.
    MOZ_ASSERT(mPreSilenceSamples >= 1);
    return mPreSilenceSamples - 1;
  }

  int32_t CountDiscontinuities() const {
    // Every discontinuity is counted twice one on current sample and one more
    // on previous sample.
    return mDiscontinuitiesCount / 2;
  }

 private:
  void CheckBuffer(const T* aBuffer, int32_t aSamples) {
    for (int i = 0; i < aSamples; ++i) {
      ++mTotalFramesSoFar;
      // Avoid pre-silence
      if (!CountPreSilence(aBuffer[i])) {
        CountZeroCrossing(aBuffer[i]);
        CountDiscontinuities(aBuffer[i]);
      }

      mPrevious = aBuffer[i];
    }
  }

  bool CountPreSilence(T aCurrentSample) {
    if (IsZero(aCurrentSample) && mPreSilenceSamples == mTotalFramesSoFar - 1) {
      ++mPreSilenceSamples;
      return true;
    }
    return false;
  }

  // Positive to negative direction
  void CountZeroCrossing(T aCurrentSample) {
    if (mPrevious > 0 && aCurrentSample <= 0) {
      if (mZeroCrossCount++) {
        MOZ_ASSERT(mZeroCrossCount > 1);
        mSumPeriodInSamples += mTotalFramesSoFar - mLastZeroCrossPosition;
      }
      mLastZeroCrossPosition = mTotalFramesSoFar;
    }
  }

  void CountDiscontinuities(T aCurrentSample) {
    mDiscontinuitiesCount += fabs(fabs(aCurrentSample) - fabs(mPrevious)) >
                             2 * mExpectedTone.MaxMagnitudeDifference();
  }

  bool IsZero(float a) { return fabs(a) < 1e-8; }
  bool IsZero(short a) { return a == 0; }

 private:
  const int32_t mRate;
  AudioToneGenerator<T> mExpectedTone;

  int32_t mZeroCrossCount = 0;
  int64_t mLastZeroCrossPosition = 0;
  int64_t mSumPeriodInSamples = 0;

  int64_t mTotalFramesSoFar = 0;
  int64_t mPreSilenceSamples = 0;

  int32_t mDiscontinuitiesCount = 0;
  // This is needed to connect previous the previous buffers.
  T mPrevious = {};
};

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
  const int32_t sampleRateTransmitter = aSourceRate;
  const int32_t sampleRateReceiver = aTargetRate;
  AudioDriftCorrection ad(sampleRateTransmitter, sampleRateReceiver);

  AudioToneGenerator<AudioDataValue> tone(sampleRateTransmitter);
  AudioToneVerifier<AudioDataValue> inToneVerifier(sampleRateTransmitter);
  AudioToneVerifier<AudioDataValue> outToneVerifier(sampleRateReceiver);

  int32_t sourceFrames;
  const int32_t targetFrames = sampleRateReceiver / 100;

  // Run for some time: 6 * 250 = 1500 iterations
  for (int j = 0; j < 6; ++j) {
    // apply some drift
    if (j % 2 == 0) {
      sourceFrames = sampleRateTransmitter / 100 + 10;
    } else {
      sourceFrames = sampleRateTransmitter / 100 - 10;
    }

    for (int n = 0; n < 250; ++n) {
      // Create the input (sine tone)
      AudioChunk chunk = CreateAudioChunk<AudioDataValue>(sourceFrames, 1,
                                                          AUDIO_OUTPUT_FORMAT);
      tone.Write(chunk);
      AudioSegment inSegment;
      inSegment.AppendAndConsumeChunk(&chunk);
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
  EXPECT_EQ(inToneVerifier.EstimatedFreq(), tone.mFrequency);
  EXPECT_EQ(inToneVerifier.PreSilenceSamples(), 0);
  EXPECT_EQ(inToneVerifier.CountDiscontinuities(), 0);

  EXPECT_EQ(outToneVerifier.EstimatedFreq(), tone.mFrequency);
  // The expected pre-silence is 50ms plus the resampling, this is roughly more
  // than 2000 frames for the samples rates being used here
  EXPECT_GT(outToneVerifier.PreSilenceSamples(), 2000);
  EXPECT_EQ(outToneVerifier.CountDiscontinuities(), 0);
}

TEST(TestAudioDriftCorrection, Basic)
{
  testAudioCorrection(48000, 48000);
  testAudioCorrection(48000, 44100);
  testAudioCorrection(44100, 48000);
}

void testMonoToStereoInput(int aSourceRate, int aTargetRate) {
  const int32_t sampleRateTransmitter = aSourceRate;
  const int32_t sampleRateReceiver = aTargetRate;
  AudioDriftCorrection ad(sampleRateTransmitter, sampleRateReceiver);

  AudioToneGenerator<AudioDataValue> tone(sampleRateTransmitter);
  AudioToneVerifier<AudioDataValue> inToneVerify(sampleRateTransmitter);
  AudioToneVerifier<AudioDataValue> outToneVerify(sampleRateReceiver);

  int32_t sourceFrames;
  const int32_t targetFrames = sampleRateReceiver / 100;

  // Run for some time: 6 * 250 = 1500 iterations
  for (int j = 0; j < 6; ++j) {
    // apply some drift
    if (j % 2 == 0) {
      sourceFrames = sampleRateTransmitter / 100 + 10;
    } else {
      sourceFrames = sampleRateTransmitter / 100 - 10;
    }

    for (int n = 0; n < 250; ++n) {
      // Create the input (sine tone)
      AudioChunk chunk = CreateAudioChunk<AudioDataValue>(sourceFrames / 2, 1,
                                                          AUDIO_OUTPUT_FORMAT);
      tone.Write(chunk);

      AudioChunk chunk2 = CreateAudioChunk<AudioDataValue>(sourceFrames / 2, 2,
                                                           AUDIO_OUTPUT_FORMAT);
      tone.Write(chunk2);

      AudioSegment inSegment;
      inSegment.AppendAndConsumeChunk(&chunk);
      inSegment.AppendAndConsumeChunk(&chunk2);
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
  EXPECT_EQ(inToneVerify.EstimatedFreq(), tone.mFrequency);
  EXPECT_EQ(inToneVerify.PreSilenceSamples(), 0);
  EXPECT_EQ(inToneVerify.CountDiscontinuities(), 0);

  EXPECT_GT(outToneVerify.CountDiscontinuities(), 0)
      << "Expect discontinuities";
  EXPECT_NE(outToneVerify.EstimatedFreq(), tone.mFrequency)
      << "Estimation is not accurate due to discontinuities";
  // The expected pre-silence is 50ms plus the resampling. However, due to
  // discontinuities pre-silence is expected only in the first iteration which
  // is routhly a little more than 400 frames for the chosen sample rates.
  EXPECT_GT(outToneVerify.PreSilenceSamples(), 400);
}

TEST(TestAudioDriftCorrection, MonoToStereoInput)
{
  testMonoToStereoInput(48000, 48000);
  testMonoToStereoInput(48000, 44100);
  testMonoToStereoInput(44100, 48000);
}

TEST(TestAudioDriftCorrection, NotEnoughFrames)
{
  const int32_t sampleRateTransmitter = 48000;
  const int32_t sampleRateReceiver = 48000;
  AudioDriftCorrection ad(sampleRateTransmitter, sampleRateReceiver);
  const int32_t targetFrames = sampleRateReceiver / 100;

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
  const int32_t sampleRateTransmitter = 48000;
  const int32_t sampleRateReceiver = 48000;
  AudioDriftCorrection ad(sampleRateTransmitter, sampleRateReceiver);
  const int32_t targetFrames = sampleRateReceiver / 100;

  for (int i = 0; i < 100; ++i) {
    AudioChunk chunk = CreateAudioChunk<float>(sampleRateTransmitter / 1000, 1,
                                               AUDIO_FORMAT_FLOAT32);
    AudioSegment inSegment;
    inSegment.AppendAndConsumeChunk(&chunk);

    AudioSegment outSegment = ad.RequestFrames(inSegment, targetFrames);
    EXPECT_EQ(outSegment.GetDuration(), targetFrames);
  }
}
