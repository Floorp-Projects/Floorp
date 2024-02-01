/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "AudioGenerator.h"
#include "MediaEngineWebRTCAudio.h"
#include "MediaTrackGraphImpl.h"
#include "PrincipalHandle.h"
#include "mozilla/Attributes.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsTArray.h"

using namespace mozilla;
using testing::NiceMock;
using testing::Return;

class MockGraph : public MediaTrackGraphImpl {
 public:
  MockGraph(TrackRate aRate, uint32_t aChannels)
      : MediaTrackGraphImpl(OFFLINE_THREAD_DRIVER, DIRECT_DRIVER, 0, aRate,
                            aChannels, nullptr, AbstractThread::MainThread()) {
    ON_CALL(*this, OnGraphThread).WillByDefault(Return(true));
    // Remove this graph's driver since it holds a ref. If no AppendMessage
    // takes place, the driver never starts. This will also make sure no-one
    // tries to use it. We are still kept alive by the self-ref. Destroy() must
    // be called to break that cycle.
    SetCurrentDriver(nullptr);
  }

  MOCK_CONST_METHOD0(OnGraphThread, bool());

 protected:
  ~MockGraph() = default;
};

// AudioInputProcessing will put extra frames as pre-buffering data to avoid
// glitchs in non pass-through mode. The main goal of the test is to check how
// many frames left in the AudioInputProcessing's mSegment in various situations
// after input data has been processed.
TEST(TestAudioInputProcessing, Buffering)
{
  const TrackRate rate = 8000;  // So packet size is 80
  const uint32_t channels = 1;
  auto graph = MakeRefPtr<NiceMock<MockGraph>>(rate, channels);
  auto aip = MakeRefPtr<AudioInputProcessing>(channels);

  const size_t frames = 72;

  AudioGenerator<AudioDataValue> generator(channels, rate);
  GraphTime processedTime;
  GraphTime nextTime;
  AudioSegment output;

  // Toggle pass-through mode without starting
  {
    EXPECT_EQ(aip->PassThrough(graph), false);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 0);

    aip->SetPassThrough(graph, true);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 0);

    aip->SetPassThrough(graph, false);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 0);

    aip->SetPassThrough(graph, true);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 0);
  }

  {
    // Need (nextTime - processedTime) = 128 - 0 = 128 frames this round.
    // aip has not started and set to processing mode yet, so output will be
    // filled with silence data directly.
    processedTime = 0;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(frames);

    AudioSegment input;
    generator.Generate(input, nextTime - processedTime);

    aip->Process(graph, processedTime, nextTime, &input, &output);
    EXPECT_EQ(input.GetDuration(), nextTime - processedTime);
    EXPECT_EQ(output.GetDuration(), nextTime);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 0);
  }

  // Set aip to processing/non-pass-through mode
  aip->SetPassThrough(graph, false);
  {
    // Need (nextTime - processedTime) = 256 - 128 = 128 frames this round.
    // aip has not started yet, so output will be filled with silence data
    // directly.
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(2 * frames);

    AudioSegment input;
    generator.Generate(input, nextTime - processedTime);

    aip->Process(graph, processedTime, nextTime, &input, &output);
    EXPECT_EQ(input.GetDuration(), nextTime - processedTime);
    EXPECT_EQ(output.GetDuration(), nextTime);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 0);
  }

  // aip has been started and set to processing mode so it will insert 80 frames
  // into aip's internal buffer as pre-buffering.
  aip->Start(graph);
  {
    // Need (nextTime - processedTime) = 256 - 256 = 0 frames this round.
    // The Process() aip will take 0 frames from input, packetize and process
    // these frames into 0 80-frame packet(0 frames left in packetizer), insert
    // packets into aip's internal buffer, then move 0 frames the internal
    // buffer to output, leaving 80 + 0 - 0 = 80 frames in aip's internal
    // buffer.
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(3 * frames);

    AudioSegment input;
    generator.Generate(input, nextTime - processedTime);

    aip->Process(graph, processedTime, nextTime, &input, &output);
    EXPECT_EQ(input.GetDuration(), nextTime - processedTime);
    EXPECT_EQ(output.GetDuration(), nextTime);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 80);
  }

  {
    // Need (nextTime - processedTime) = 384 - 256 = 128 frames this round.
    // The Process() aip will take 128 frames from input, packetize and process
    // these frames into floor(128/80) = 1 80-frame packet (48 frames left in
    // packetizer), insert packets into aip's internal buffer, then move 128
    // frames the internal buffer to output, leaving 80 + 80 - 128 = 32 frames
    // in aip's internal buffer.
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(4 * frames);

    AudioSegment input;
    generator.Generate(input, nextTime - processedTime);

    aip->Process(graph, processedTime, nextTime, &input, &output);
    EXPECT_EQ(input.GetDuration(), nextTime - processedTime);
    EXPECT_EQ(output.GetDuration(), nextTime);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 32);
  }

  {
    // Need (nextTime - processedTime) = 384 - 384 = 0 frames this round.
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(5 * frames);

    AudioSegment input;
    generator.Generate(input, nextTime - processedTime);

    aip->Process(graph, processedTime, nextTime, &input, &output);
    EXPECT_EQ(input.GetDuration(), nextTime - processedTime);
    EXPECT_EQ(output.GetDuration(), nextTime);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 32);
  }

  {
    // Need (nextTime - processedTime) = 512 - 384 = 128 frames this round.
    // The Process() aip will take 128 frames from input, packetize and process
    // these frames into floor(128+48/80) = 2 80-frame packet (16 frames left in
    // packetizer), insert packets into aip's internal buffer, then move 128
    // frames the internal buffer to output, leaving 32 + 2*80 - 128 = 64 frames
    // in aip's internal buffer.
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(6 * frames);

    AudioSegment input;
    generator.Generate(input, nextTime - processedTime);

    aip->Process(graph, processedTime, nextTime, &input, &output);
    EXPECT_EQ(input.GetDuration(), nextTime - processedTime);
    EXPECT_EQ(output.GetDuration(), nextTime);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 64);
  }

  aip->SetPassThrough(graph, true);
  {
    // Need (nextTime - processedTime) = 512 - 512 = 0 frames this round.
    // No buffering in pass-through mode
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(7 * frames);

    AudioSegment input;
    generator.Generate(input, nextTime - processedTime);

    aip->Process(graph, processedTime, nextTime, &input, &output);
    EXPECT_EQ(input.GetDuration(), nextTime - processedTime);
    EXPECT_EQ(output.GetDuration(), processedTime);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 0);
  }

  aip->Stop(graph);
  graph->Destroy();
}

TEST(TestAudioInputProcessing, ProcessDataWithDifferentPrincipals)
{
  const TrackRate rate = 48000;  // so # of output frames from packetizer is 480
  const uint32_t channels = 2;
  auto graph = MakeRefPtr<NiceMock<MockGraph>>(rate, channels);
  auto aip = MakeRefPtr<AudioInputProcessing>(channels);
  AudioGenerator<AudioDataValue> generator(channels, rate);

  RefPtr<nsIPrincipal> dummy_principal =
      NullPrincipal::CreateWithoutOriginAttributes();
  const PrincipalHandle principal1 = MakePrincipalHandle(dummy_principal.get());
  const PrincipalHandle principal2 =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());

  // Total 4800 frames. It's easier to test with frames of multiples of 480.
  nsTArray<std::pair<TrackTime, PrincipalHandle>> framesWithPrincipal = {
      {100, principal1},
      {200, PRINCIPAL_HANDLE_NONE},
      {300, principal2},
      {400, principal1},
      {440, PRINCIPAL_HANDLE_NONE},
      // 3 packet-size above.
      {480, principal1},
      {480, principal2},
      {480, PRINCIPAL_HANDLE_NONE},
      // 3 packet-size above.
      {500, principal2},
      {490, principal1},
      {600, principal1},
      {330, principal1}
      // 4 packet-size above.
  };

  // Generate 4800 frames of data with different principals.
  AudioSegment input;
  {
    for (const auto& [duration, principal] : framesWithPrincipal) {
      AudioSegment data;
      generator.Generate(data, duration);
      for (AudioSegment::ChunkIterator it(data); !it.IsEnded(); it.Next()) {
        it->mPrincipalHandle = principal;
      }

      input.AppendFrom(&data);
    }
  }

  auto verifyPrincipals = [&](const AudioSegment& data) {
    TrackTime start = 0;
    for (const auto& [duration, principal] : framesWithPrincipal) {
      const TrackTime end = start + duration;

      AudioSegment slice;
      slice.AppendSlice(data, start, end);
      start = end;

      for (AudioSegment::ChunkIterator it(slice); !it.IsEnded(); it.Next()) {
        EXPECT_EQ(it->mPrincipalHandle, principal);
      }
    }
  };

  // Check the principals in audio-processing mode.
  EXPECT_EQ(aip->PassThrough(graph), false);
  aip->Start(graph);
  {
    EXPECT_EQ(aip->NumBufferedFrames(graph), 480);
    AudioSegment output;
    {
      // Trim the prebuffering silence.

      AudioSegment data;
      aip->Process(graph, 0, 4800, &input, &data);
      EXPECT_EQ(input.GetDuration(), 4800);
      EXPECT_EQ(data.GetDuration(), 4800);

      AudioSegment dummy;
      dummy.AppendNullData(480);
      aip->Process(graph, 0, 480, &dummy, &data);
      EXPECT_EQ(dummy.GetDuration(), 480);
      EXPECT_EQ(data.GetDuration(), 480 + 4800);

      // Ignore the pre-buffering data
      output.AppendSlice(data, 480, 480 + 4800);
    }

    verifyPrincipals(output);
  }

  // Check the principals in pass-through mode.
  aip->SetPassThrough(graph, true);
  {
    AudioSegment output;
    aip->Process(graph, 0, 4800, &input, &output);
    EXPECT_EQ(input.GetDuration(), 4800);
    EXPECT_EQ(output.GetDuration(), 4800);

    verifyPrincipals(output);
  }

  aip->Stop(graph);
  graph->Destroy();
}

TEST(TestAudioInputProcessing, Downmixing)
{
  const TrackRate rate = 44100;
  const uint32_t channels = 4;
  auto graph = MakeRefPtr<NiceMock<MockGraph>>(rate, channels);
  auto aip = MakeRefPtr<AudioInputProcessing>(channels);

  const size_t frames = 44100;

  AudioGenerator<AudioDataValue> generator(channels, rate);
  GraphTime processedTime;
  GraphTime nextTime;

  aip->SetPassThrough(graph, false);
  aip->Start(graph);

  processedTime = 0;
  nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(frames);

  {
    AudioSegment input;
    AudioSegment output;
    generator.Generate(input, nextTime - processedTime);

    // Intentionally reduce the amplitude of the generated sine wave so there's
    // no chance the max amplitude reaches 1.0, but not enough so that 4
    // channels summed together won't clip.
    input.ApplyVolume(0.9);

    // Process is going to see that it has 4 channels of input, and is going to
    // downmix to mono, scaling the input by 1/4 in the process.
    // We can't compare the input and output signal because the sine is going to
    // be mangledui
    aip->Process(graph, processedTime, nextTime, &input, &output);
    EXPECT_EQ(input.GetDuration(), nextTime - processedTime);
    EXPECT_EQ(output.GetDuration(), nextTime);
    EXPECT_EQ(output.MaxChannelCount(), 1u);

    // Verify that it doesn't clip: the input signal has likely been mangled by
    // the various processing passes, but at least it shouldn't clip. We know we
    // always have floating point audio here, regardless of the sample-type used
    // by Gecko.
    for (AudioSegment::ChunkIterator iterOutput(output); !iterOutput.IsEnded();
         iterOutput.Next()) {
      const float* const output = iterOutput->ChannelData<float>()[0];
      for (uint32_t i = 0; i < iterOutput->GetDuration(); i++) {
        // Very conservative here, it's likely that the AGC lowers the volume a
        // lot.
        EXPECT_LE(std::abs(output[i]), 0.95);
      }
    }
  }

  // Now, repeat the test, checking we get the unmodified 4 channels.
  aip->SetPassThrough(graph, true);

  AudioSegment input, output;
  processedTime = nextTime;
  nextTime += MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(frames);
  generator.Generate(input, nextTime - processedTime);

  aip->Process(graph, processedTime, nextTime, &input, &output);
  EXPECT_EQ(input.GetDuration(), nextTime - processedTime);
  EXPECT_EQ(output.GetDuration(), nextTime - processedTime);
  // This time, no downmix: 4 channels of input, 4 channels of output
  EXPECT_EQ(output.MaxChannelCount(), 4u);

  nsTArray<AudioDataValue> inputLinearized, outputLinearized;
  input.WriteToInterleavedBuffer(inputLinearized, input.MaxChannelCount());
  output.WriteToInterleavedBuffer(outputLinearized, output.MaxChannelCount());

  // The data should be passed through, and exactly equal.
  for (uint32_t i = 0; i < frames * channels; i++) {
    EXPECT_EQ(inputLinearized[i], outputLinearized[i]);
  }

  aip->Stop(graph);
  graph->Destroy();
}
