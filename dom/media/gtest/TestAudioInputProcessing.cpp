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
    aip->NotifyInputData(graph,
                         AudioInputProcessing::BufferInfo{
                             buffer.Elements(), nrFrames, channels, rate},
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
    aip->NotifyInputData(graph,
                         AudioInputProcessing::BufferInfo{
                             buffer.Elements(), nrFrames, channels, rate},
                         nextTime - (2 * nrFrames));
    aip->ProcessInput(graph, nullptr);
    aip->Pull(graph, processedTime, nextTime, segment.GetDuration(), &segment,
              true, &ended);
    EXPECT_EQ(aip->NumBufferedFrames(graph), 120U);
  }

  graph->Destroy();
}
