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

  // Setup: Create a NativeInputTrack and add it to mGraph
  RefPtr<NativeInputTrack> track =
      new NativeInputTrack(mGraph->GraphRate(), deviceId, testPrincipal);
  mGraph->AddTrack(track);

  // Main test below:

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

  // Tear down: Destroy the NativeInputTrack and remove it from mGraph.
  track->Destroy();
  mGraph->RemoveTrackGraphThread(track);
}

TEST_F(TestDeviceInputTrack, OpenTwiceWithoutCloseFirst) {
  const CubebUtils::AudioDeviceID deviceId1 = (void*)1;
  const CubebUtils::AudioDeviceID deviceId2 = (void*)2;

  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());

  auto r1 = NativeInputTrack::OpenAudio(mGraph.get(), deviceId1, testPrincipal,
                                        nullptr);
  ASSERT_TRUE(r1.isOk());
  RefPtr<NativeInputTrack> track1 = r1.unwrap()->AsNativeInputTrack();
  ASSERT_TRUE(track1);

  auto r2 = NativeInputTrack::OpenAudio(mGraph.get(), deviceId2, testPrincipal,
                                        nullptr);
  EXPECT_TRUE(r2.isErr());

  NativeInputTrack::CloseAudio(std::move(track1), nullptr);
}

// TODO: Use AudioInputSource with MockCubeb for the test
TEST_F(TestDeviceInputTrack, NonNativeInputTrackData) {
  // Graph settings
  const uint32_t flags = 0;
  const GraphTime frames = 440;

  // Non native input settings
  const uint32_t channelCount = 2;
  const TrackRate rate = 48000;

  // Declare Buffer here to make sure it lives longer than the TestAudioSource
  // we are going to use below.
  AudioSegment buffer;

  const CubebUtils::AudioDeviceID deviceId = (void*)1;
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());

  // Setup: Create a NonNativeInputTrack and add it to mGraph
  RefPtr<NonNativeInputTrack> track =
      new NonNativeInputTrack(mGraph->GraphRate(), deviceId, testPrincipal);
  mGraph->AddTrack(track);

  // Main test below:

  // Make sure we get null data if the track is not started yet.
  GraphTime current = 0;
  GraphTime next = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(frames);
  ASSERT_NE(current, next);  // Make sure we have data produced in ProcessInput.

  track->ProcessInput(current, next, flags);
  {
    AudioSegment data;
    data.AppendSegment(track->GetData<AudioSegment>());
    EXPECT_TRUE(data.IsNull());
  }

  // Make sure we get the AudioInputSource's data once we start the track.
  current = next;
  next = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(2 * frames);
  ASSERT_NE(current, next);  // Make sure we have data produced in ProcessInput.

  class MockEventListener : public AudioInputSource::EventListener {
   public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MockEventListener, override);
    MOCK_METHOD1(AudioDeviceChanged, void(AudioInputSource::Id));
    MOCK_METHOD2(AudioStateCallback,
                 void(AudioInputSource::Id,
                      AudioInputSource::EventListener::State));

   private:
    ~MockEventListener() = default;
  };

  class TestAudioSource final : public AudioInputSource {
   public:
    TestAudioSource(RefPtr<EventListener>&& aListener, Id aSourceId,
                    CubebUtils::AudioDeviceID aDeviceId, uint32_t aChannelCount,
                    bool aIsVoice, const PrincipalHandle& aPrincipalHandle,
                    TrackRate aSourceRate, TrackRate aTargetRate,
                    uint32_t aBufferMs, AudioSegment& aCurrentData)
        : AudioInputSource(std::move(aListener), aSourceId, aDeviceId,
                           aChannelCount, aIsVoice, aPrincipalHandle,
                           aSourceRate, aTargetRate, aBufferMs),
          mGenerator(mChannelCount, aTargetRate),
          mCurrentData(aCurrentData) {}

    void Start() override {}
    void Stop() override {}
    AudioSegment GetAudioSegment(TrackTime aDuration) override {
      mCurrentData.Clear();
      mGenerator.Generate(mCurrentData, static_cast<uint32_t>(aDuration));
      AudioSegment data;
      data.AppendSegment(&mCurrentData);
      return data;
    }
    const AudioSegment& GetCurrentData() { return mCurrentData; }

   private:
    ~TestAudioSource() = default;

    AudioGenerator<AudioDataValue> mGenerator;
    AudioSegment& mCurrentData;
  };

  // We need to make sure the buffer lives longer than the TestAudioSource here
  // since TestAudioSource will access buffer by reference.
  track->StartAudio(MakeRefPtr<TestAudioSource>(
      MakeRefPtr<MockEventListener>(), 1 /* Ignored */, deviceId, channelCount,
      true /* Ignored*/, testPrincipal, rate, mGraph->GraphRate(),
      50 /* Ignored */, buffer));
  track->ProcessInput(current, next, flags);
  {
    AudioSegment data;
    data.AppendSlice(*track->GetData<AudioSegment>(), current, next);

    TrackTime duration = next - current;
    EXPECT_EQ(data.GetDuration(), duration);
    EXPECT_EQ(data.GetDuration(), buffer.GetDuration());

    // The data we get here should be same as the data in buffer. We don't
    // implement == operator for AudioSegment so we convert the data into
    // interleaved one in nsTArray, which implements the == operator, to do the
    // comparison.
    nsTArray<AudioDataValue> actual;
    size_t actualSampleCount =
        data.WriteToInterleavedBuffer(actual, channelCount);
    nsTArray<AudioDataValue> expected;
    size_t expectedSampleCount =
        buffer.WriteToInterleavedBuffer(expected, channelCount);
    EXPECT_EQ(actualSampleCount, expectedSampleCount);
    EXPECT_EQ(actualSampleCount, static_cast<size_t>(duration) *
                                     static_cast<size_t>(channelCount));
    EXPECT_EQ(actual, expected);
  }

  // Stop the track and make sure it produces null data again.
  current = next;
  next = MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(3 * frames);
  ASSERT_NE(current, next);  // Make sure we have data produced in ProcessInput.

  track->StopAudio();
  track->ProcessInput(current, next, flags);
  {
    AudioSegment data;
    data.AppendSlice(*track->GetData<AudioSegment>(), current, next);
    EXPECT_TRUE(data.IsNull());
  }

  // Tear down: Destroy the NativeInputTrack and remove it from mGraph.
  track->Destroy();
  mGraph->RemoveTrackGraphThread(track);
}
