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

void printAudioSegment(const AudioSegment& segment) {
  for (AudioSegment::ConstChunkIterator iter(segment); !iter.IsEnded();
       iter.Next()) {
    const AudioChunk& c = *iter;
    const float* buffer = c.ChannelData<float>()[0];
    for (int i = 0; i < c.GetDuration(); ++i) {
      printf("%f\n", buffer[i]);
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

  const float amplitude = 0.5;
  const float frequency = 10;
  const float phase = 0.0;
  float time = 0.0;
  const float deltaTime = 1.0f / sampleRateTransmitter;

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
      AudioChunk chunk =
          CreateAudioChunk<float>(sourceFrames, 1, AUDIO_FORMAT_FLOAT32);
      float* monoBuffer = chunk.ChannelDataForWrite<float>(0);
      for (int i = 0; i < sourceFrames; ++i) {
        double value = amplitude * sin(2 * M_PI * frequency * time + phase);
        monoBuffer[i] = static_cast<float>(value);
        time += deltaTime;
      }
      AudioSegment inSegment;
      inSegment.AppendAndConsumeChunk(&chunk);
      // Print the input for debugging
      // printAudioSegment(inSegment);

      // Get the output of the correction
      AudioSegment outSegment = ad.RequestFrames(inSegment, targetFrames);
      EXPECT_EQ(outSegment.GetDuration(), targetFrames);
      // Print the output for debugging
      // printAudioSegment(outSegment);
    }
  }
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

  const float amplitude = 0.5;
  const float frequency = 10;
  const float phase = 0.0;
  float time = 0.0;
  const float deltaTime = 1.0f / sampleRateTransmitter;

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
      AudioChunk chunk =
          CreateAudioChunk<float>(sourceFrames / 2, 1, AUDIO_FORMAT_FLOAT32);
      float* monoBuffer = chunk.ChannelDataForWrite<float>(0);
      for (int i = 0; i < chunk.GetDuration(); ++i) {
        double value = amplitude * sin(2 * M_PI * frequency * time + phase);
        monoBuffer[i] = static_cast<float>(value);
        time += deltaTime;
      }
      AudioChunk chunk2 =
          CreateAudioChunk<float>(sourceFrames / 2, 2, AUDIO_FORMAT_FLOAT32);
      for (int i = 0; i < chunk2.GetDuration(); ++i) {
        double value = amplitude * sin(2 * M_PI * frequency * time + phase);
        chunk2.ChannelDataForWrite<float>(0)[i] =
            chunk2.ChannelDataForWrite<float>(1)[i] = static_cast<float>(value);
        time += deltaTime;
      }
      AudioSegment inSegment;
      inSegment.AppendAndConsumeChunk(&chunk);
      inSegment.AppendAndConsumeChunk(&chunk2);
      // Print the input for debugging
      // printAudioSegment(inSegment);

      // Get the output of the correction
      AudioSegment outSegment = ad.RequestFrames(inSegment, targetFrames);
      EXPECT_EQ(outSegment.GetDuration(), targetFrames);
      // Print the output for debugging
      // printAudioSegment(outSegment);
    }
  }
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
