/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "DeviceInputTrack.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "AudioGenerator.h"
#include "MediaTrackGraphImpl.h"
#include "nsContentUtils.h"

using namespace mozilla;
using testing::NiceMock;
using testing::Return;

class MockGraphImpl : public MediaTrackGraphImpl {
 public:
  MockGraphImpl(TrackRate aRate, uint32_t aChannels)
      : MediaTrackGraphImpl(OFFLINE_THREAD_DRIVER, DIRECT_DRIVER, aRate,
                            aChannels, nullptr, NS_GetCurrentThread()) {
    ON_CALL(*this, OnGraphThread).WillByDefault(Return(true));
    // We have to call `Destroy()` manually in order to break the reference.
    // The reason we don't assign a null driver is because we would add a track
    // to the graph, then it would trigger graph's `EnsureNextIteration()` that
    // requires a non-null driver.
    SetCurrentDriver(new NiceMock<MockDriver>());
  }

  MOCK_CONST_METHOD0(OnGraphThread, bool());
  MOCK_METHOD1(AppendMessage, void(UniquePtr<ControlMessage>));

 protected:
  ~MockGraphImpl() = default;

  class MockDriver : public GraphDriver {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MockDriver, override);

    MockDriver() : GraphDriver(nullptr, nullptr, 0) {
      ON_CALL(*this, OnThread).WillByDefault(Return(true));
      ON_CALL(*this, ThreadRunning).WillByDefault(Return(true));
    }

    MOCK_METHOD0(Start, void());
    MOCK_METHOD0(Shutdown, void());
    MOCK_METHOD0(IterationDuration, uint32_t());
    MOCK_METHOD0(EnsureNextIteration, void());
    MOCK_CONST_METHOD0(OnThread, bool());
    MOCK_CONST_METHOD0(ThreadRunning, bool());

   protected:
    ~MockDriver() = default;
  };
};

class TestDeviceInputTrack : public testing::Test {
 protected:
  TestDeviceInputTrack() : mChannels(2), mRate(44100) {}

  void SetUp() override {
    mGraph = MakeRefPtr<NiceMock<MockGraphImpl>>(mRate, mChannels);
  }

  void TearDown() override { mGraph->Destroy(); }

  const uint32_t mChannels;
  const TrackRate mRate;
  RefPtr<MockGraphImpl> mGraph;
};

TEST_F(TestDeviceInputTrack, NativeInputTrackData) {
  const uint32_t flags = 0;
  const CubebUtils::AudioDeviceID deviceId = (void*)1;

  AudioGenerator<AudioDataValue> generator(mChannels, mRate);
  const size_t nrFrames = 10;
  const size_t bufferSize = nrFrames * mChannels;
  nsTArray<AudioDataValue> buffer(bufferSize);
  buffer.AppendElements(bufferSize);

  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());

  RefPtr<NativeInputTrack> track =
      NativeInputTrack::Create(mGraph.get(), deviceId, testPrincipal);

  generator.GenerateInterleaved(buffer.Elements(), nrFrames);
  track->NotifyInputData(mGraph.get(), buffer.Elements(), nrFrames, mRate,
                         mChannels, 0);

  track->ProcessInput(0, WEBAUDIO_BLOCK_SIZE + nrFrames, flags);
  EXPECT_EQ(static_cast<size_t>(track->GetEnd()),
            static_cast<size_t>(WEBAUDIO_BLOCK_SIZE) + nrFrames);

  // Check pre-buffering: null data with PRINCIPAL_HANDLE_NONE principal
  AudioSegment preBuffering;
  preBuffering.AppendSlice(*track->GetData(), 0, WEBAUDIO_BLOCK_SIZE);
  EXPECT_TRUE(preBuffering.IsNull());
  for (AudioSegment::ConstChunkIterator iter(preBuffering); !iter.IsEnded();
       iter.Next()) {
    const AudioChunk& chunk = *iter;
    EXPECT_EQ(chunk.mPrincipalHandle, PRINCIPAL_HANDLE_NONE);
  }

  // Check rest of the data
  AudioSegment data;
  data.AppendSlice(*track->GetData(), WEBAUDIO_BLOCK_SIZE,
                   WEBAUDIO_BLOCK_SIZE + nrFrames);
  nsTArray<AudioDataValue> interleaved;
  size_t sampleCount = data.WriteToInterleavedBuffer(interleaved, mChannels);
  EXPECT_EQ(sampleCount, bufferSize);
  EXPECT_EQ(interleaved, buffer);

  // Check principal in data
  for (AudioSegment::ConstChunkIterator iter(data); !iter.IsEnded();
       iter.Next()) {
    const AudioChunk& chunk = *iter;
    EXPECT_EQ(chunk.mPrincipalHandle, testPrincipal);
  }

  track->Destroy();
  mGraph->RemoveTrackGraphThread(track);
}
