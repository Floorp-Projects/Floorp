/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "AudioDriftCorrection.h"
#include "AudioGenerator.h"
#include "AudioVerifier.h"
#include "mozilla/StaticPrefs_media.h"
#include "nsContentUtils.h"

using namespace mozilla;

// Print the mono channel of a segment.
void printAudioSegment(const AudioSegment& segment) {
  for (AudioSegment::ConstChunkIterator iter(segment); !iter.IsEnded();
       iter.Next()) {
    const AudioChunk& c = *iter;
    for (uint32_t i = 0; i < c.GetDuration(); ++i) {
      if (c.mBufferFormat == AUDIO_FORMAT_FLOAT32) {
        printf("%f\n", c.ChannelData<float>()[0][i]);
      } else {
        printf("%d\n", c.ChannelData<int16_t>()[0][i]);
      }
    }
  }
}

template <class T>
AudioChunk CreateAudioChunk(uint32_t aFrames, uint32_t aChannels,
                            AudioSampleFormat aSampleFormat);

void testAudioCorrection(int32_t aSourceRate, int32_t aTargetRate) {
  const uint32_t sampleRateTransmitter = aSourceRate;
  const uint32_t sampleRateReceiver = aTargetRate;
  const uint32_t frequency = 100;
  const uint32_t buffering = StaticPrefs::media_clockdrift_buffering();
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  AudioDriftCorrection ad(sampleRateTransmitter, sampleRateReceiver, buffering,
                          testPrincipal);

  AudioGenerator<AudioDataValue> tone(1, sampleRateTransmitter, frequency);
  AudioVerifier<AudioDataValue> inToneVerifier(sampleRateTransmitter,
                                               frequency);
  AudioVerifier<AudioDataValue> outToneVerifier(sampleRateReceiver, frequency);

  uint32_t sourceFrames;
  const uint32_t targetFrames = sampleRateReceiver / 100;

  // Run for some time: 3 * 5000 = 15000 iterations
  for (uint32_t j = 0; j < 3; ++j) {
    // apply some drift
    if (j % 2 == 0) {
      sourceFrames =
          sampleRateTransmitter * /*1.002*/ 1002 / 1000 / /*1s->10ms*/ 100;
    } else {
      sourceFrames =
          sampleRateTransmitter * /*0.998*/ 998 / 1000 / /*1s->10ms*/ 100;
    }

    // 50 seconds, allows for at least 50 correction changes, to stabilize
    // on the current drift.
    for (uint32_t n = 0; n < 5000; ++n) {
      // Create the input (sine tone)
      AudioSegment inSegment;
      tone.Generate(inSegment, sourceFrames);
      inToneVerifier.AppendData(inSegment);
      // Print the input for debugging
      // printAudioSegment(inSegment);

      // Get the output of the correction
      AudioSegment outSegment = ad.RequestFrames(inSegment, targetFrames);
      EXPECT_EQ(outSegment.GetDuration(), targetFrames);
      for (AudioSegment::ConstChunkIterator ci(outSegment); !ci.IsEnded();
           ci.Next()) {
        EXPECT_EQ(ci->mPrincipalHandle, testPrincipal);
      }
      // Print the output for debugging
      // printAudioSegment(outSegment);
      outToneVerifier.AppendData(outSegment);
    }
  }

  const int32_t expectedBuffering =
      ad.mDesiredBuffering - sampleRateTransmitter / 100 /* 10ms */;
  EXPECT_NEAR(ad.CurrentBuffering(), expectedBuffering, 512);

  EXPECT_NEAR(inToneVerifier.EstimatedFreq(), tone.mFrequency, 1.0f);
  EXPECT_EQ(inToneVerifier.PreSilenceSamples(), 0U);
  EXPECT_EQ(inToneVerifier.CountDiscontinuities(), 0U);

  EXPECT_NEAR(outToneVerifier.EstimatedFreq(), tone.mFrequency, 1.0f);
  // The expected pre-silence is 50ms plus the resampling, minus the size of the
  // first resampled segment.
  EXPECT_GE(outToneVerifier.PreSilenceSamples(),
            aTargetRate * 50 / 1000U - aTargetRate * 102 / 100 / 100);
  EXPECT_EQ(outToneVerifier.CountDiscontinuities(), 0U);
}

TEST(TestAudioDriftCorrection, Basic)
{
  printf("Testing AudioCorrection 48 -> 48\n");
  testAudioCorrection(48000, 48000);
  printf("Testing AudioCorrection 48 -> 44.1\n");
  testAudioCorrection(48000, 44100);
  printf("Testing AudioCorrection 44.1 -> 48\n");
  testAudioCorrection(44100, 48000);
  printf("Testing AudioCorrection 23458 -> 25113\n");
  testAudioCorrection(23458, 25113);
}

void testMonoToStereoInput(uint32_t aSourceRate, uint32_t aTargetRate) {
  const uint32_t frequency = 100;
  const uint32_t sampleRateTransmitter = aSourceRate;
  const uint32_t sampleRateReceiver = aTargetRate;
  const uint32_t buffering = StaticPrefs::media_clockdrift_buffering();
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  AudioDriftCorrection ad(sampleRateTransmitter, sampleRateReceiver, buffering,
                          testPrincipal);

  AudioGenerator<AudioDataValue> tone(1, sampleRateTransmitter, frequency);
  AudioVerifier<AudioDataValue> inToneVerify(sampleRateTransmitter, frequency);
  AudioVerifier<AudioDataValue> outToneVerify(sampleRateReceiver, frequency);

  uint32_t sourceFrames;
  const uint32_t targetFrames = sampleRateReceiver / 100;

  // Run for some time: 6 * 250 = 1500 iterations
  for (uint32_t j = 0; j < 6; ++j) {
    // apply some drift
    if (j % 2 == 0) {
      sourceFrames = sampleRateTransmitter / 100 + 10;
    } else {
      sourceFrames = sampleRateTransmitter / 100 - 10;
    }

    for (uint32_t n = 0; n < 250; ++n) {
      // Create the input (sine tone) of two chunks.
      AudioSegment inSegment;
      tone.Generate(inSegment, sourceFrames / 2);
      tone.SetChannelsCount(2);
      tone.Generate(inSegment, sourceFrames / 2);
      tone.SetChannelsCount(1);
      inToneVerify.AppendData(inSegment);
      // Print the input for debugging
      // printAudioSegment(inSegment);

      // Get the output of the correction
      AudioSegment outSegment = ad.RequestFrames(inSegment, targetFrames);
      EXPECT_EQ(outSegment.GetDuration(), targetFrames);
      for (AudioSegment::ConstChunkIterator ci(outSegment); !ci.IsEnded();
           ci.Next()) {
        EXPECT_EQ(ci->mPrincipalHandle, testPrincipal);
      }
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
  const uint32_t frequency = 100;
  const uint32_t sampleRateTransmitter = 48000;
  const uint32_t sampleRateReceiver = 48000;
  const uint32_t buffering = StaticPrefs::media_clockdrift_buffering();
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  AudioDriftCorrection ad(sampleRateTransmitter, sampleRateReceiver, buffering,
                          testPrincipal);
  const uint32_t targetFrames = sampleRateReceiver / 100;

  AudioGenerator<AudioDataValue> tone(1, sampleRateTransmitter, frequency);
  AudioVerifier<AudioDataValue> outToneVerifier(sampleRateReceiver, frequency);

  for (uint32_t i = 0; i < 7; ++i) {
    // Input is something small, 10 frames here, in order to dry out fast,
    // after 4 iterations (pre-buffer = 2400)
    AudioSegment inSegment;
    tone.Generate(inSegment, 10);

    AudioSegment outSegment = ad.RequestFrames(inSegment, targetFrames);
    EXPECT_EQ(outSegment.GetDuration(), targetFrames);
    EXPECT_FALSE(outSegment.IsNull());
    for (AudioSegment::ConstChunkIterator ci(outSegment); !ci.IsEnded();
         ci.Next()) {
      if (i < 5) {
        if (!ci->IsNull()) {
          EXPECT_EQ(ci->mPrincipalHandle, testPrincipal);
        }
      }
    }

    outToneVerifier.AppendData(outSegment);
  }
  EXPECT_EQ(outToneVerifier.CountDiscontinuities(), 1u);
}

TEST(TestAudioDriftCorrection, CrashInAudioResampler)
{
  const uint32_t sampleRateTransmitter = 48000;
  const uint32_t sampleRateReceiver = 48000;
  const uint32_t buffering = StaticPrefs::media_clockdrift_buffering();
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  AudioDriftCorrection ad(sampleRateTransmitter, sampleRateReceiver, buffering,
                          testPrincipal);
  const uint32_t targetFrames = sampleRateReceiver / 100;

  for (uint32_t i = 0; i < 100; ++i) {
    AudioChunk chunk = CreateAudioChunk<float>(sampleRateTransmitter / 1000, 1,
                                               AUDIO_FORMAT_FLOAT32);
    AudioSegment inSegment;
    inSegment.AppendAndConsumeChunk(std::move(chunk));

    AudioSegment outSegment = ad.RequestFrames(inSegment, targetFrames);
    EXPECT_EQ(outSegment.GetDuration(), targetFrames);
    for (AudioSegment::ConstChunkIterator ci(outSegment); !ci.IsEnded();
         ci.Next()) {
      if (!ci->IsNull()) {  // Don't check the data if ad is dried out.
        EXPECT_EQ(ci->mPrincipalHandle, testPrincipal);
      }
    }
  }
}

TEST(TestAudioDriftCorrection, HighLatencyProducerLowLatencyConsumer)
{
  constexpr uint32_t transmitterBlockSize = 2048;
  constexpr uint32_t receiverBlockSize = 128;
  constexpr uint32_t sampleRate = 48000;
  const uint32_t bufferingMs = StaticPrefs::media_clockdrift_buffering();
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  AudioDriftCorrection ad(sampleRate, sampleRate, bufferingMs, testPrincipal);

  uint32_t numBlocksProduced = 0;
  for (uint32_t i = 0; i < (sampleRate / 1000) * 500; i += receiverBlockSize) {
    AudioSegment inSegment;
    if ((i / transmitterBlockSize) > numBlocksProduced) {
      AudioChunk chunk = CreateAudioChunk<float>(transmitterBlockSize, 1,
                                                 AUDIO_FORMAT_FLOAT32);
      inSegment.AppendAndConsumeChunk(std::move(chunk));
      ++numBlocksProduced;
    }

    AudioSegment outSegment = ad.RequestFrames(inSegment, receiverBlockSize);
    EXPECT_EQ(outSegment.GetDuration(), receiverBlockSize);
  }

  // Input is stable so no corrections should occur.
  EXPECT_EQ(ad.NumCorrectionChanges(), 0U);
}

TEST(TestAudioDriftCorrection, LargerTransmitterBlockSizeThanDesiredBuffering)
{
  constexpr uint32_t transmitterBlockSize = 4096;
  constexpr uint32_t receiverBlockSize = 128;
  constexpr uint32_t sampleRate = 48000;
  constexpr uint32_t bufferingMs = 50;  // 2400 frames
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  AudioDriftCorrection ad(sampleRate, sampleRate, bufferingMs, testPrincipal);

  uint32_t numBlocksTransmitted = 0;
  for (uint32_t i = 0; i < (sampleRate / 1000) * 500; i += receiverBlockSize) {
    AudioSegment inSegment;
    if (uint32_t currentBlock = i / transmitterBlockSize;
        currentBlock > numBlocksTransmitted) {
      AudioChunk chunk = CreateAudioChunk<float>(transmitterBlockSize, 1,
                                                 AUDIO_FORMAT_FLOAT32);
      inSegment.AppendAndConsumeChunk(std::move(chunk));
      numBlocksTransmitted = currentBlock;
    }

    AudioSegment outSegment = ad.RequestFrames(inSegment, receiverBlockSize);
    EXPECT_EQ(outSegment.GetDuration(), receiverBlockSize);

    if (numBlocksTransmitted > 0) {
      EXPECT_GT(ad.CurrentBuffering(), 0U);
    }
  }

  // Input is stable so no corrections should occur.
  EXPECT_EQ(ad.NumCorrectionChanges(), 0U);
}

TEST(TestAudioDriftCorrection, LargerReceiverBlockSizeThanDesiredBuffering)
{
  constexpr uint32_t transmitterBlockSize = 128;
  constexpr uint32_t receiverBlockSize = 4096;
  constexpr uint32_t sampleRate = 48000;
  constexpr uint32_t bufferingMs = 50;  // 2400 frames
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  AudioDriftCorrection ad(sampleRate, sampleRate, bufferingMs, testPrincipal);

  for (uint32_t i = 0; i < (sampleRate / 1000) * 500;
       i += transmitterBlockSize) {
    AudioSegment inSegment;
    AudioChunk chunk =
        CreateAudioChunk<float>(transmitterBlockSize, 1, AUDIO_FORMAT_FLOAT32);
    inSegment.AppendAndConsumeChunk(std::move(chunk));

    if (i % receiverBlockSize == 0) {
      AudioSegment outSegment = ad.RequestFrames(inSegment, receiverBlockSize);
      EXPECT_EQ(outSegment.GetDuration(), receiverBlockSize);
    }

    if (i >= receiverBlockSize) {
      EXPECT_GT(ad.CurrentBuffering(), 0U);
    }
  }

  // Input is stable so no corrections should occur.
  EXPECT_EQ(ad.NumCorrectionChanges(), 0U);
}

TEST(TestAudioDriftCorrection, DynamicInputBufferSizeChanges)
{
  constexpr uint32_t transmitterBlockSize1 = 2048;
  constexpr uint32_t transmitterBlockSize2 = 4096;
  constexpr uint32_t receiverBlockSize = 128;
  constexpr uint32_t sampleRate = 48000;
  constexpr uint32_t frequencyHz = 100;
  const uint32_t bufferingMs = StaticPrefs::media_clockdrift_buffering();
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  AudioDriftCorrection ad(sampleRate, sampleRate, bufferingMs, testPrincipal);

  AudioGenerator<AudioDataValue> tone(1, sampleRate, frequencyHz);
  AudioVerifier<AudioDataValue> inToneVerifier(sampleRate, frequencyHz);
  AudioVerifier<AudioDataValue> outToneVerifier(sampleRate, frequencyHz);

  TrackTime totalFramesTransmitted = 0;
  TrackTime totalFramesReceived = 0;

  // Produces 10s of data.
  const auto produceSomeData = [&](uint32_t aTransmitterBlockSize) {
    TrackTime transmittedFramesStart = totalFramesTransmitted;
    TrackTime receivedFramesStart = totalFramesReceived;
    uint32_t numBlocksTransmitted = 0;
    for (uint32_t i = 0; i < 10 * sampleRate; i += receiverBlockSize) {
      AudioSegment inSegment;
      if (((receivedFramesStart - transmittedFramesStart + i) /
           aTransmitterBlockSize) > numBlocksTransmitted) {
        tone.Generate(inSegment, aTransmitterBlockSize);
        MOZ_ASSERT(!inSegment.IsNull());
        inToneVerifier.AppendData(inSegment);
        MOZ_ASSERT(!inSegment.IsNull());
        ++numBlocksTransmitted;
        totalFramesTransmitted += aTransmitterBlockSize;
      }

      AudioSegment outSegment = ad.RequestFrames(inSegment, receiverBlockSize);
      EXPECT_EQ(outSegment.GetDuration(), receiverBlockSize);
      outToneVerifier.AppendData(outSegment);
      totalFramesReceived += receiverBlockSize;
    }
  };

  produceSomeData(transmitterBlockSize1);

  // Input is stable so no corrections should occur.
  EXPECT_EQ(ad.NumCorrectionChanges(), 0U);

  // Increase input latency. We expect this to underrun, but only once as the
  // drift correction adapts its buffer size and desired buffering level.
  produceSomeData(transmitterBlockSize2);

  // Decrease input latency. We expect the drift correction to gradually
  // decrease its desired buffering level.
  produceSomeData(transmitterBlockSize1);

  EXPECT_NEAR(inToneVerifier.EstimatedFreq(), tone.mFrequency, 1.0f);
  EXPECT_EQ(inToneVerifier.PreSilenceSamples(), 0U);
  EXPECT_EQ(inToneVerifier.CountDiscontinuities(), 0U);

  EXPECT_NEAR(outToneVerifier.EstimatedFreq(), tone.mFrequency, 1.0f);
  // The expected pre-silence is 50ms plus the resampling, minus the size of the
  // first resampled segment.
  EXPECT_GE(outToneVerifier.PreSilenceSamples(),
            sampleRate * bufferingMs / 1000U - transmitterBlockSize1);
}

/**
 * This is helpful to run together with
 *   MOZ_LOG=raw,DriftControllerGraphs:5 MOZ_LOG_FILE=./plot_values.csv
 * to be able to plot the step response of a change in source clock rate (i.e.
 * drift). Useful for calculating and verifying PID coefficients.
 */
TEST(TestAudioDriftCorrection, DriftStepResponse)
{
  constexpr uint32_t nominalRate = 48000;
  constexpr uint32_t interval = nominalRate;
  constexpr uint32_t inputRate = nominalRate * 1005 / 1000;  // 0.5% drift
  constexpr uint32_t inputInterval = inputRate;
  constexpr uint32_t iterations = 200;
  const uint32_t bufferingMs = StaticPrefs::media_clockdrift_buffering();
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  AudioGenerator<AudioDataValue> tone(1, nominalRate, 440);
  AudioDriftCorrection ad(nominalRate, nominalRate, bufferingMs, testPrincipal);
  for (uint32_t i = 0; i < interval * iterations; i += interval / 100) {
    AudioSegment inSegment;
    tone.Generate(inSegment, inputInterval / 100);
    ad.RequestFrames(inSegment, interval / 100);
  }
}

/**
 * Similar to DriftStepResponse but will underrun to allow testing the underrun
 * handling. This is helpful to run together with
 *   MOZ_LOG=raw,DriftControllerGraphs:5 MOZ_LOG_FILE=./plot_values.csv
 */
TEST(TestAudioDriftCorrection, DriftStepResponseUnderrun)
{
  constexpr uint32_t nominalRate = 48000;
  constexpr uint32_t interval = nominalRate;
  constexpr uint32_t iterations = 200;
  const uint32_t bufferingMs = StaticPrefs::media_clockdrift_buffering();
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  uint32_t inputRate = nominalRate * 1005 / 1000;  // 0.5% drift
  uint32_t inputInterval = inputRate;
  Preferences::SetUint("media.clockdrift.buffering", 10);
  AudioGenerator<AudioDataValue> tone(1, nominalRate, 440);
  AudioDriftCorrection ad(nominalRate, nominalRate, bufferingMs, testPrincipal);
  for (uint32_t i = 0; i < interval * iterations; i += interval / 100) {
    AudioSegment inSegment;
    tone.Generate(inSegment, inputInterval / 100);
    ad.RequestFrames(inSegment, interval / 100);
  }

  inputRate = nominalRate * 998 / 1000;  // -0.2% drift
  inputInterval = inputRate;
  for (uint32_t i = 0; i < interval * iterations; i += interval / 100) {
    AudioSegment inSegment;
    tone.Generate(inSegment, inputInterval / 100);
    ad.RequestFrames(inSegment, interval / 100);
  }

  Preferences::ClearUser("media.clockdrift.buffering");
}

/**
 * Similar to DriftStepResponse but with a high-latency input, and will underrun
 * to allow testing the underrun handling. This is helpful to run together with
 *   MOZ_LOG=raw,DriftControllerGraphs:5 MOZ_LOG_FILE=./plot_values.csv
 */
TEST(TestAudioDriftCorrection, DriftStepResponseUnderrunHighLatencyInput)
{
  constexpr uint32_t nominalRate = 48000;
  constexpr uint32_t interval = nominalRate;
  constexpr uint32_t iterations = 200;
  const uint32_t bufferingMs = StaticPrefs::media_clockdrift_buffering();
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  uint32_t inputRate = nominalRate * 1005 / 1000;  // 0.5% drift
  uint32_t inputInterval = inputRate;
  Preferences::SetUint("media.clockdrift.buffering", 10);
  AudioGenerator<AudioDataValue> tone(1, nominalRate, 440);
  AudioDriftCorrection ad(nominalRate, nominalRate, bufferingMs, testPrincipal);
  for (uint32_t i = 0; i < interval * iterations; i += interval / 100) {
    AudioSegment inSegment;
    if (i > 0 && i % interval == 0) {
      tone.Generate(inSegment, inputInterval);
    }
    ad.RequestFrames(inSegment, interval / 100);
  }

  inputRate = nominalRate * 995 / 1000;  // -0.5% drift
  inputInterval = inputRate;
  for (uint32_t i = 0; i < interval * iterations; i += interval / 100) {
    AudioSegment inSegment;
    if (i > 0 && i % interval == 0) {
      tone.Generate(inSegment, inputInterval);
    }
    ad.RequestFrames(inSegment, interval / 100);
  }

  Preferences::ClearUser("media.clockdrift.buffering");
}

/**
 * Similar to DriftStepResponse but with a high-latency input, and will overrun
 * (input callback buffer is larger than AudioDriftCorrection's ring buffer for
 * input data) to allow testing the overrun handling. This is helpful to run
 * together with
 *   MOZ_LOG=raw,DriftControllerGraphs:5 MOZ_LOG_FILE=./plot_values.csv
 */
TEST(TestAudioDriftCorrection, DriftStepResponseOverrun)
{
  constexpr uint32_t nominalRate = 48000;
  constexpr uint32_t interval = nominalRate;
  constexpr uint32_t inputRate = nominalRate * 1005 / 1000;  // 0.5% drift
  constexpr uint32_t inputInterval = inputRate;
  constexpr uint32_t iterations = 200;
  const uint32_t bufferingMs = StaticPrefs::media_clockdrift_buffering();
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());

  AudioGenerator<AudioDataValue> tone(1, nominalRate, 440);
  AudioDriftCorrection ad(nominalRate, nominalRate, bufferingMs, testPrincipal);

  for (uint32_t i = 0; i < interval * iterations; i += interval / 100) {
    AudioSegment inSegment;
    tone.Generate(inSegment, inputInterval / 100);
    ad.RequestFrames(inSegment, interval / 100);
  }

  // Change input callbacks to 2000ms (+0.5% drift) = 48200 frames, which will
  // overrun the ring buffer.
  for (uint32_t i = 0; i < interval * iterations; i += interval / 100) {
    AudioSegment inSegment;
    if (i > 0 && i % interval == 0) {
      // This simulates the input stream latency increasing externally. It's
      // building up a second worth of data before the next callback. This also
      // causes an underrun.
      tone.Generate(inSegment, inputInterval);
    }
    ad.RequestFrames(inSegment, interval / 100);
  }
}
