/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioInputSource.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "MockCubeb.h"
#include "mozilla/gtest/WaitFor.h"
#include "nsContentUtils.h"

using namespace mozilla;
using testing::ContainerEq;

namespace {
#define DispatchFunction(f) \
  NS_DispatchToCurrentThread(NS_NewRunnableFunction(__func__, f))
}  // namespace

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

TEST(TestAudioInputSource, StartAndStop)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  const AudioInputSource::Id sourceId = 1;
  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;
  const uint32_t channels = 2;
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  const TrackRate sourceRate = 44100;
  const TrackRate targetRate = 48000;

  auto listener = MakeRefPtr<MockEventListener>();
  EXPECT_CALL(*listener,
              AudioStateCallback(
                  sourceId, AudioInputSource::EventListener::State::Started))
      .Times(2);
  EXPECT_CALL(*listener,
              AudioStateCallback(
                  sourceId, AudioInputSource::EventListener::State::Stopped))
      .Times(4);

  RefPtr<AudioInputSource> ais = MakeRefPtr<AudioInputSource>(
      std::move(listener), sourceId, deviceId, channels, true, testPrincipal,
      sourceRate, targetRate);
  ASSERT_TRUE(ais);

  // Make sure start and stop works.
  {
    DispatchFunction([&] { ais->Start(); });
    RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
    EXPECT_TRUE(stream->mHasInput);
    EXPECT_FALSE(stream->mHasOutput);
    EXPECT_EQ(stream->GetInputDeviceID(), deviceId);
    EXPECT_EQ(stream->InputChannels(), channels);
    EXPECT_EQ(stream->InputSampleRate(), static_cast<uint32_t>(sourceRate));

    Unused << WaitFor(stream->FramesProcessedEvent());

    DispatchFunction([&] { ais->Stop(); });
    Unused << WaitFor(cubeb->StreamDestroyEvent());
  }

  // Make sure restart is ok.
  {
    DispatchFunction([&] { ais->Start(); });
    RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
    EXPECT_TRUE(stream->mHasInput);
    EXPECT_FALSE(stream->mHasOutput);
    EXPECT_EQ(stream->GetInputDeviceID(), deviceId);
    EXPECT_EQ(stream->InputChannels(), channels);
    EXPECT_EQ(stream->InputSampleRate(), static_cast<uint32_t>(sourceRate));

    Unused << WaitFor(stream->FramesProcessedEvent());

    DispatchFunction([&] { ais->Stop(); });
    Unused << WaitFor(cubeb->StreamDestroyEvent());
  }

  ais = nullptr;  // Drop the SharedThreadPool here.
}

TEST(TestAudioInputSource, DataOutputBeforeStartAndAfterStop)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  const AudioInputSource::Id sourceId = 1;
  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;
  const uint32_t channels = 2;
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  const TrackRate sourceRate = 44100;
  const TrackRate targetRate = 48000;

  const TrackTime requestFrames = 2 * WEBAUDIO_BLOCK_SIZE;

  auto listener = MakeRefPtr<MockEventListener>();
  EXPECT_CALL(*listener,
              AudioStateCallback(
                  sourceId, AudioInputSource::EventListener::State::Started));
  EXPECT_CALL(*listener,
              AudioStateCallback(
                  sourceId, AudioInputSource::EventListener::State::Stopped))
      .Times(2);

  RefPtr<AudioInputSource> ais = MakeRefPtr<AudioInputSource>(
      std::move(listener), sourceId, deviceId, channels, true, testPrincipal,
      sourceRate, targetRate);
  ASSERT_TRUE(ais);

  // It's ok to call GetAudioSegment before starting
  {
    AudioSegment data =
        ais->GetAudioSegment(requestFrames, AudioInputSource::Consumer::Same);
    EXPECT_EQ(data.GetDuration(), requestFrames);
    EXPECT_TRUE(data.IsNull());
  }

  DispatchFunction([&] { ais->Start(); });
  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);
  EXPECT_FALSE(stream->mHasOutput);
  EXPECT_EQ(stream->InputChannels(), channels);

  stream->SetInputRecordingEnabled(true);

  Unused << WaitFor(stream->FramesProcessedEvent());

  DispatchFunction([&] { ais->Stop(); });
  Unused << WaitFor(cubeb->StreamDestroyEvent());

  // Check the data output
  {
    nsTArray<AudioDataValue> record = stream->TakeRecordedInput();
    size_t frames = record.Length() / channels;
    AudioSegment deinterleaved;
    deinterleaved.AppendFromInterleavedBuffer(record.Elements(), frames,
                                              channels, testPrincipal);
    AudioDriftCorrection driftCorrector(sourceRate, targetRate, testPrincipal);
    AudioSegment expectedSegment = driftCorrector.RequestFrames(
        deinterleaved, static_cast<uint32_t>(requestFrames));

    CopyableTArray<AudioDataValue> expected;
    size_t expectedSamples =
        expectedSegment.WriteToInterleavedBuffer(expected, channels);

    AudioSegment actualSegment =
        ais->GetAudioSegment(requestFrames, AudioInputSource::Consumer::Same);
    EXPECT_EQ(actualSegment.GetDuration(), requestFrames);
    CopyableTArray<AudioDataValue> actual;
    size_t actualSamples =
        actualSegment.WriteToInterleavedBuffer(actual, channels);

    EXPECT_EQ(actualSamples, expectedSamples);
    EXPECT_EQ(actualSamples / channels, static_cast<size_t>(requestFrames));
    EXPECT_THAT(actual, ContainerEq(expected));
  }

  ais = nullptr;  // Drop the SharedThreadPool here.
}

TEST(TestAudioInputSource, ErrorCallback)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  const AudioInputSource::Id sourceId = 1;
  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;
  const uint32_t channels = 2;
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  const TrackRate sourceRate = 44100;
  const TrackRate targetRate = 48000;

  auto listener = MakeRefPtr<MockEventListener>();
  EXPECT_CALL(*listener,
              AudioStateCallback(
                  sourceId, AudioInputSource::EventListener::State::Started));
  EXPECT_CALL(*listener,
              AudioStateCallback(
                  sourceId, AudioInputSource::EventListener::State::Error));
  EXPECT_CALL(*listener,
              AudioStateCallback(
                  sourceId, AudioInputSource::EventListener::State::Stopped))
      .Times(2);

  RefPtr<AudioInputSource> ais = MakeRefPtr<AudioInputSource>(
      std::move(listener), sourceId, deviceId, channels, true, testPrincipal,
      sourceRate, targetRate);
  ASSERT_TRUE(ais);

  DispatchFunction([&] { ais->Start(); });
  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);
  EXPECT_FALSE(stream->mHasOutput);
  EXPECT_EQ(stream->InputChannels(), channels);

  Unused << WaitFor(stream->FramesProcessedEvent());

  DispatchFunction([&] { stream->ForceError(); });
  WaitFor(stream->ErrorForcedEvent());

  DispatchFunction([&] { ais->Stop(); });
  Unused << WaitFor(cubeb->StreamDestroyEvent());

  ais = nullptr;  // Drop the SharedThreadPool here.
}

TEST(TestAudioInputSource, DeviceChangedCallback)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  const AudioInputSource::Id sourceId = 1;
  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;
  const uint32_t channels = 2;
  const PrincipalHandle testPrincipal =
      MakePrincipalHandle(nsContentUtils::GetSystemPrincipal());
  const TrackRate sourceRate = 44100;
  const TrackRate targetRate = 48000;

  auto listener = MakeRefPtr<MockEventListener>();
  EXPECT_CALL(*listener, AudioDeviceChanged(sourceId));
  EXPECT_CALL(*listener,
              AudioStateCallback(
                  sourceId, AudioInputSource::EventListener::State::Started));
  EXPECT_CALL(*listener,
              AudioStateCallback(
                  sourceId, AudioInputSource::EventListener::State::Stopped))
      .Times(2);

  RefPtr<AudioInputSource> ais = MakeRefPtr<AudioInputSource>(
      std::move(listener), sourceId, deviceId, channels, true, testPrincipal,
      sourceRate, targetRate);
  ASSERT_TRUE(ais);

  DispatchFunction([&] { ais->Start(); });
  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);
  EXPECT_FALSE(stream->mHasOutput);
  EXPECT_EQ(stream->InputChannels(), channels);

  Unused << WaitFor(stream->FramesProcessedEvent());

  DispatchFunction([&] { stream->ForceDeviceChanged(); });
  WaitFor(stream->DeviceChangeForcedEvent());

  DispatchFunction([&] { ais->Stop(); });
  Unused << WaitFor(cubeb->StreamDestroyEvent());

  ais = nullptr;  // Drop the SharedThreadPool here.
}
