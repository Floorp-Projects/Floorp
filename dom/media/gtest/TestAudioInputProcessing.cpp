/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "AudioGenerator.h"
#include "MediaEngineWebRTCAudio.h"
#include "MediaTrackGraphImpl.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "nsTArray.h"

using namespace mozilla;
using testing::NiceMock;
using testing::Return;

class MockGraph : public MediaTrackGraphImpl {
 public:
  MockGraph(TrackRate aRate, uint32_t aChannels)
      : MediaTrackGraphImpl(OFFLINE_THREAD_DRIVER, DIRECT_DRIVER, aRate,
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

TEST(TestAudioInputProcessing, UnaccountedPacketizerBuffering)
{
  const TrackRate rate = 48000;
  const uint32_t channels = 2;
  auto graph = MakeRefPtr<NiceMock<MockGraph>>(48000, 2);
  auto aip = MakeRefPtr<AudioInputProcessing>(channels, PRINCIPAL_HANDLE_NONE);
  AudioGenerator<AudioDataValue> generator(channels, rate);

  // The packetizer takes 480 frames. To trigger this we need to populate the
  // packetizer without filling it completely the first iteration, then trigger
  // the unbounded-buffering-assertion on the second iteration.

  const size_t nrFrames = 440;
  const size_t bufferSize = nrFrames * channels;
  GraphTime processedTime;
  GraphTime nextTime;
  nsTArray<AudioDataValue> buffer(bufferSize);
  buffer.AppendElements(bufferSize);
  AudioSegment segment;
  bool ended;

  aip->Start();

  {
    // First iteration.
    // 440 does not fill the packetizer but accounts for pre-silence buffering.
    // Iterations have processed 72 frames more than provided by callbacks:
    //     512 - 440 = 72
    // Thus the total amount of pre-silence buffering added is:
    //     480 + 128 - 72 = 536
    // The iteration pulls in 512 frames of silence, leaving 24 frames buffered.
    processedTime = 0;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(nrFrames);
    generator.GenerateInterleaved(buffer.Elements(), nrFrames);
    aip->NotifyInputData(graph, buffer.Elements(), nrFrames, rate, channels,
                         nextTime - nrFrames);
    aip->ProcessInput(graph, nullptr);
    aip->Pull(graph, processedTime, nextTime, segment.GetDuration(), &segment,
              true, &ended);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 24U);
  }

  {
    // Second iteration.
    // 880 fills a packet of 480 frames. 400 left in the packetizer.
    // Last iteration left 24 frames buffered, making this iteration have 504
    // frames in the buffer while pulling 384 frames.
    // That leaves 120 frames buffered, which must be no more than the total
    // intended buffering of 480 + 128 = 608 frames.
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(2 * nrFrames);
    generator.GenerateInterleaved(buffer.Elements(), nrFrames);
    aip->NotifyInputData(graph, buffer.Elements(), nrFrames, rate, channels,
                         nextTime - (2 * nrFrames));
    aip->ProcessInput(graph, nullptr);
    aip->Pull(graph, processedTime, nextTime, segment.GetDuration(), &segment,
              true, &ended);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 120U);
  }

  graph->Destroy();
}

TEST(TestAudioInputProcessing, InputDataCapture)
{
  // This test simulates an audio cut issue happens when using Redmi AirDots.
  // Similar issues could happen when using other Bluetooth devices like Bose QC
  // 35 II or Sony WH-XB900N.

  const TrackRate rate = 8000;  // So the packetizer takes 80 frames
  const uint32_t channels = 1;
  auto graph = MakeRefPtr<NiceMock<MockGraph>>(rate, channels);
  auto aip = MakeRefPtr<AudioInputProcessing>(channels, PRINCIPAL_HANDLE_NONE);
  AudioGenerator<AudioDataValue> generator(channels, rate);

  const size_t frames = 72;
  const size_t bufferSize = frames * channels;
  nsTArray<AudioDataValue> buffer(bufferSize);
  buffer.AppendElements(bufferSize);

  GraphTime processedTime;
  GraphTime nextTime;
  AudioSegment segment;
  bool ended;

  aip->Start();

  {
    // First iteration.
    // aip will fill (WEBAUDIO_BLOCK_SIZE + packetizer-size) = 128 + 80 = 208
    // silence frames in begining of its data storage. The iteration will take
    // (nextTime - segment-duration) = (128 - 0) = 128 frames to segment,
    // leaving 208 - 128 = 80 silence frames.
    const TrackTime bufferedFrames = 80U;
    processedTime = 0;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(frames);

    generator.GenerateInterleaved(buffer.Elements(), frames);
    aip->NotifyInputData(graph, buffer.Elements(), frames, rate, channels, 0);
    buffer.ClearAndRetainStorage();
    aip->ProcessInput(graph, nullptr);
    aip->Pull(graph, processedTime, nextTime, segment.GetDuration(), &segment,
              true, &ended);
    EXPECT_EQ(aip->NumBufferedFrames(graph), bufferedFrames);
  }

  {
    // Second iteration.
    // We will packetize 80 frames to aip's data storage. The last round left 80
    // frames so we have 80 + 80 = 160 frames. The iteration will take (nextTime
    // - segment-duration) = (256 - 128) = 128 frames to segment, leaving 160 -
    // 128 = 32 frames.
    const TrackTime bufferedFrames = 32U;
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(2 * frames);

    generator.GenerateInterleaved(buffer.Elements(), frames);
    aip->NotifyInputData(graph, buffer.Elements(), frames, rate, channels,
                         0 /* ignored */);
    buffer.ClearAndRetainStorage();
    aip->ProcessInput(graph, nullptr);
    aip->Pull(graph, processedTime, nextTime, segment.GetDuration(), &segment,
              true, &ended);
    EXPECT_EQ(aip->NumBufferedFrames(graph), bufferedFrames);
  }

  {
    // Third iteration.
    // Sometimes AudioCallbackDriver's buffer, whose type is
    // AudioCallbackBufferWrapper, could be unavailable, and therefore
    // ProcessInput won't be called. In this case, we should queue the audio
    // data and process them when ProcessInput can be called again.
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(3 * frames);
    // Note that processedTime is *equal* to nextTime (processedTime ==
    // nextTime) now but it's ok since we don't call ProcessInput here.

    generator.GenerateInterleaved(buffer.Elements(), frames);
    aip->NotifyInputData(graph, buffer.Elements(), frames, rate, channels,
                         0 /* ignored */);
    Unused << processedTime;
    buffer.ClearAndRetainStorage();
  }

  {
    // Fourth iteration.
    // We will packetize 80 (previous round) + 80 (this round) = 160 frames to
    // aip's data storage. 32 frames are left after the second iteration, so we
    // have 160 + 32 = 192 frames. The iteration will take (nextTime
    // - segment-duration) = (384 - 256) = 128 frames to segment, leaving 192 -
    // 128 = 64 frames.
    const TrackTime bufferedFrames = 64U;
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(4 * frames);
    generator.GenerateInterleaved(buffer.Elements(), frames);
    aip->NotifyInputData(graph, buffer.Elements(), frames, rate, channels,
                         0 /* ignored */);
    buffer.ClearAndRetainStorage();
    aip->ProcessInput(graph, nullptr);
    aip->Pull(graph, processedTime, nextTime, segment.GetDuration(), &segment,
              true, &ended);
    EXPECT_EQ(aip->NumBufferedFrames(graph), bufferedFrames);
  }

  graph->Destroy();
}

TEST(TestAudioInputProcessing, InputDataCapturePassThrough)
{
  // This test simulates an audio cut issue happens when using Redmi AirDots.
  // Similar issues could happen when using other Bluetooth devices like Bose QC
  // 35 II or Sony WH-XB900N.

  const TrackRate rate = 8000;  // So the packetizer takes 80 frames
  const uint32_t channels = 1;
  auto graph = MakeRefPtr<NiceMock<MockGraph>>(rate, channels);
  auto aip = MakeRefPtr<AudioInputProcessing>(channels, PRINCIPAL_HANDLE_NONE);
  AudioGenerator<AudioDataValue> generator(channels, rate);

  const size_t frames = 72;
  const size_t bufferSize = frames * channels;
  nsTArray<AudioDataValue> buffer(bufferSize);
  buffer.AppendElements(bufferSize);

  GraphTime processedTime;
  GraphTime nextTime;
  AudioSegment segment;
  AudioSegment source;
  bool ended;

  aip->SetPassThrough(graph, true);
  aip->Start();

  {
    // First iteration.
    // aip will fill (WEBAUDIO_BLOCK_SIZE + ) = 128 + 72 = 200 silence frames in
    // begining of its data storage. The iteration will take (nextTime -
    // segment-duration) = (128 - 0) = 128 frames to segment, leaving 200 - 128
    // = 72 silence frames.
    const TrackTime bufferedFrames = 72U;
    processedTime = 0;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(frames);

    generator.GenerateInterleaved(buffer.Elements(), frames);
    source.AppendFromInterleavedBuffer(buffer.Elements(), frames, channels,
                                       PRINCIPAL_HANDLE_NONE);
    aip->NotifyInputData(graph, buffer.Elements(), frames, rate, channels, 0);
    buffer.ClearAndRetainStorage();
    aip->ProcessInput(graph, &source);
    aip->Pull(graph, processedTime, nextTime, segment.GetDuration(), &segment,
              true, &ended);
    EXPECT_EQ(aip->NumBufferedFrames(graph), bufferedFrames);
    source.Clear();
  }

  {
    // Second iteration.
    // We will feed 72 frames to aip's data storage. The last round left 72
    // frames so we have 72 + 72 = 144 frames. The iteration will take (nextTime
    // - segment-duration) = (256 - 128) = 128 frames to segment, leaving 144 -
    // 128 = 16 frames.
    const TrackTime bufferedFrames = 16U;
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(2 * frames);

    generator.GenerateInterleaved(buffer.Elements(), frames);
    source.AppendFromInterleavedBuffer(buffer.Elements(), frames, channels,
                                       PRINCIPAL_HANDLE_NONE);
    aip->NotifyInputData(graph, buffer.Elements(), frames, rate, channels,
                         0 /* ignored */);
    buffer.ClearAndRetainStorage();
    aip->ProcessInput(graph, &source);
    aip->Pull(graph, processedTime, nextTime, segment.GetDuration(), &segment,
              true, &ended);
    EXPECT_EQ(aip->NumBufferedFrames(graph), bufferedFrames);
    source.Clear();
  }

  {
    // Third iteration.
    // Sometimes AudioCallbackDriver's buffer, whose type is
    // AudioCallbackBufferWrapper, could be unavailable, and therefore
    // ProcessInput won't be called. In this case, we should queue the audio
    // data and process them when ProcessInput can be called again.
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(3 * frames);
    // Note that processedTime is *equal* to nextTime (processedTime ==
    // nextTime) now but it's ok since we don't call ProcessInput here.

    generator.GenerateInterleaved(buffer.Elements(), frames);
    source.AppendFromInterleavedBuffer(buffer.Elements(), frames, channels,
                                       PRINCIPAL_HANDLE_NONE);
    aip->NotifyInputData(graph, buffer.Elements(), frames, rate, channels,
                         0 /* ignored */);
    Unused << processedTime;
    buffer.ClearAndRetainStorage();
  }

  {
    // Fourth iteration.
    // We will feed 72 (previous round) + 72 (this round) = 144 frames to aip's
    // data storage. 16 frames are left after the second iteration, so we have
    // 144 + 16 = 160 frames. The iteration will take (nextTime -
    // segment-duration) = (384 - 256) = 128 frames to segment, leaving 160 -
    // 128 = 32 frames.
    const TrackTime bufferedFrames = 32U;
    processedTime = nextTime;
    nextTime = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(4 * frames);
    generator.GenerateInterleaved(buffer.Elements(), frames);
    source.AppendFromInterleavedBuffer(buffer.Elements(), frames, channels,
                                       PRINCIPAL_HANDLE_NONE);
    aip->NotifyInputData(graph, buffer.Elements(), frames, rate, channels,
                         0 /* ignored */);
    buffer.ClearAndRetainStorage();
    aip->ProcessInput(graph, &source);
    aip->Pull(graph, processedTime, nextTime, segment.GetDuration(), &segment,
              true, &ended);
    EXPECT_EQ(aip->NumBufferedFrames(graph), bufferedFrames);
    source.Clear();
  }

  graph->Destroy();
}
