/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTrackGraphImpl.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

#include "CrossGraphPort.h"
#ifdef MOZ_WEBRTC
#  include "MediaEngineWebRTCAudio.h"
#endif  // MOZ_WEBRTC
#include "MockCubeb.h"
#include "mozilla/Preferences.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "WaitFor.h"
#include "WavDumper.h"

#define DRIFT_BUFFERING_PREF "media.clockdrift.buffering"

using namespace mozilla;

namespace {
// Short-hand for InvokeAsync on the current thread.
#define Invoke(f) InvokeAsync(GetCurrentSerialEventTarget(), __func__, f)

// Short-hand for DispatchToCurrentThread with a function.
#define DispatchFunction(f) \
  NS_DispatchToCurrentThread(NS_NewRunnableFunction(__func__, f))

// Short-hand for DispatchToCurrentThread with a method with arguments
#define DispatchMethod(t, m, args...) \
  NS_DispatchToCurrentThread(NewRunnableMethod(__func__, t, m, ##args))

#ifdef MOZ_WEBRTC
/*
 * Common ControlMessages
 */
struct StartInputProcessing : public ControlMessage {
  const RefPtr<AudioProcessingTrack> mProcessingTrack;
  const RefPtr<AudioInputProcessing> mInputProcessing;

  StartInputProcessing(AudioProcessingTrack* aTrack,
                       AudioInputProcessing* aInputProcessing)
      : ControlMessage(aTrack),
        mProcessingTrack(aTrack),
        mInputProcessing(aInputProcessing) {}
  void Run() override { mInputProcessing->Start(mTrack->GraphImpl()); }
};

struct StopInputProcessing : public ControlMessage {
  const RefPtr<AudioInputProcessing> mInputProcessing;

  explicit StopInputProcessing(AudioProcessingTrack* aTrack,
                               AudioInputProcessing* aInputProcessing)
      : ControlMessage(aTrack), mInputProcessing(aInputProcessing) {}
  void Run() override { mInputProcessing->Stop(mTrack->GraphImpl()); }
};

struct SetPassThrough : public ControlMessage {
  const RefPtr<AudioInputProcessing> mInputProcessing;
  const bool mPassThrough;

  SetPassThrough(MediaTrack* aTrack, AudioInputProcessing* aInputProcessing,
                 bool aPassThrough)
      : ControlMessage(aTrack),
        mInputProcessing(aInputProcessing),
        mPassThrough(aPassThrough) {}
  void Run() override {
    EXPECT_EQ(mInputProcessing->PassThrough(mTrack->GraphImpl()),
              !mPassThrough);
    mInputProcessing->SetPassThrough(mTrack->GraphImpl(), mPassThrough);
  }
};

struct SetRequestedInputChannelCount : public ControlMessage {
  const RefPtr<AudioInputProcessing> mInputProcessing;
  const uint32_t mChannelCount;

  SetRequestedInputChannelCount(MediaTrack* aTrack,
                                AudioInputProcessing* aInputProcessing,
                                uint32_t aChannelCount)
      : ControlMessage(aTrack),
        mInputProcessing(aInputProcessing),
        mChannelCount(aChannelCount) {}
  void Run() override {
    mInputProcessing->SetRequestedInputChannelCount(mTrack->GraphImpl(),
                                                    mChannelCount);
  }
};
#endif  // MOZ_WEBRTC

class GoFaster : public ControlMessage {
  MockCubeb* mCubeb;

 public:
  explicit GoFaster(MockCubeb* aCubeb)
      : ControlMessage(nullptr), mCubeb(aCubeb) {}
  void Run() override { mCubeb->GoFaster(); }
};

}  // namespace

/*
 * The set of tests here are a bit special. In part because they're async and
 * depends on the graph thread to function. In part because they depend on main
 * thread stable state to send messages to the graph.
 *
 * Any message sent from the main thread to the graph through the graph's
 * various APIs are scheduled to run in stable state. Stable state occurs after
 * a task in the main thread eventloop has run to completion.
 *
 * Since gtests are generally sync and on main thread, calling into the graph
 * may schedule a stable state runnable but with no task in the eventloop to
 * trigger stable state. Therefore care must be taken to always call into the
 * graph from a task, typically via InvokeAsync or a dispatch to main thread.
 */

TEST(TestAudioTrackGraph, DifferentDeviceIDs)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* g1 = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ nullptr, GetMainThreadSerialEventTarget());

  MediaTrackGraph* g2 = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(1),
      GetMainThreadSerialEventTarget());

  MediaTrackGraph* g1_2 = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ nullptr, GetMainThreadSerialEventTarget());

  MediaTrackGraph* g2_2 = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(1),
      GetMainThreadSerialEventTarget());

  EXPECT_NE(g1, g2) << "Different graphs due to different device ids";
  EXPECT_EQ(g1, g1_2) << "Same graphs for same device ids";
  EXPECT_EQ(g2, g2_2) << "Same graphs for same device ids";

  for (MediaTrackGraph* g : {g1, g2}) {
    // Dummy track to make graph rolling. Add it and remove it to remove the
    // graph from the global hash table and let it shutdown.

    using SourceTrackPromise = MozPromise<SourceMediaTrack*, nsresult, true>;
    auto p = Invoke([g] {
      return SourceTrackPromise::CreateAndResolve(
          g->CreateSourceTrack(MediaSegment::AUDIO), __func__);
    });

    WaitFor(cubeb->StreamInitEvent());
    RefPtr<SourceMediaTrack> dummySource = WaitFor(p).unwrap();

    DispatchMethod(dummySource, &SourceMediaTrack::Destroy);

    WaitFor(cubeb->StreamDestroyEvent());
  }
}

TEST(TestAudioTrackGraph, SetOutputDeviceID)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  // Set the output device id in GetInstance method confirm that it is the one
  // used in cubeb_stream_init.
  MediaTrackGraph* graph = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(2),
      GetMainThreadSerialEventTarget());

  // Dummy track to make graph rolling. Add it and remove it to remove the
  // graph from the global hash table and let it shutdown.
  RefPtr<SourceMediaTrack> dummySource;
  DispatchFunction(
      [&] { dummySource = graph->CreateSourceTrack(MediaSegment::AUDIO); });

  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());

  EXPECT_EQ(stream->GetOutputDeviceID(), reinterpret_cast<cubeb_devid>(2))
      << "After init confirm the expected output device id";

  // Test has finished, destroy the track to shutdown the MTG.
  DispatchMethod(dummySource, &SourceMediaTrack::Destroy);
  WaitFor(cubeb->StreamDestroyEvent());
}

TEST(TestAudioTrackGraph, NotifyDeviceStarted)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* graph = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr,
      GetMainThreadSerialEventTarget());

  RefPtr<SourceMediaTrack> dummySource;
  Unused << WaitFor(Invoke([&] {
    // Dummy track to make graph rolling. Add it and remove it to remove the
    // graph from the global hash table and let it shutdown.
    dummySource = graph->CreateSourceTrack(MediaSegment::AUDIO);

    return graph->NotifyWhenDeviceStarted(dummySource);
  }));

  {
    MediaTrackGraphImpl* graph = dummySource->GraphImpl();
    MonitorAutoLock lock(graph->GetMonitor());
    EXPECT_TRUE(graph->CurrentDriver()->AsAudioCallbackDriver());
    EXPECT_TRUE(graph->CurrentDriver()->ThreadRunning());
  }

  // Test has finished, destroy the track to shutdown the MTG.
  DispatchMethod(dummySource, &SourceMediaTrack::Destroy);
  WaitFor(cubeb->StreamDestroyEvent());
}

#ifdef MOZ_WEBRTC
TEST(TestAudioTrackGraph, ErrorCallback)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* graph = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr,
      GetMainThreadSerialEventTarget());

  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;

  // Dummy track to make graph rolling. Add it and remove it to remove the
  // graph from the global hash table and let it shutdown.
  //
  // We open an input through this track so that there's something triggering
  // EnsureNextIteration on the fallback driver after the callback driver has
  // gotten the error.
  RefPtr<AudioProcessingTrack> processingTrack;
  RefPtr<AudioInputProcessing> listener;
  auto started = Invoke([&] {
    processingTrack = AudioProcessingTrack::Create(graph);
    listener = new AudioInputProcessing(2);
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(processingTrack, listener, true));
    processingTrack->SetInputProcessing(listener);
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(processingTrack, listener));
    processingTrack->ConnectDeviceInput(deviceId, listener,
                                        PRINCIPAL_HANDLE_NONE);
    EXPECT_EQ(processingTrack->DeviceId().value(), deviceId);
    return graph->NotifyWhenDeviceStarted(processingTrack);
  });

  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  Result<bool, nsresult> rv = WaitFor(started);
  EXPECT_TRUE(rv.unwrapOr(false));

  // Force a cubeb state_callback error and see that we don't crash.
  DispatchFunction([&] { stream->ForceError(); });

  // Wait for both the error to take effect, and the driver to restart.
  bool errored = false, init = false;
  MediaEventListener errorListener = stream->ErrorForcedEvent().Connect(
      AbstractThread::GetCurrent(), [&] { errored = true; });
  MediaEventListener initListener = cubeb->StreamInitEvent().Connect(
      AbstractThread::GetCurrent(), [&] { init = true; });
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      "TEST(TestAudioTrackGraph, ErrorCallback)"_ns,
      [&] { return errored && init; });
  errorListener.Disconnect();
  initListener.Disconnect();

  // Clean up.
  DispatchFunction([&] {
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(processingTrack, listener));
    processingTrack->DisconnectDeviceInput();
    processingTrack->Destroy();
  });
  WaitFor(cubeb->StreamDestroyEvent());
}

TEST(TestAudioTrackGraph, AudioProcessingTrack)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());
  auto unforcer = WaitFor(cubeb->ForceAudioThread()).unwrap();
  Unused << unforcer;

  // Start on a system clock driver, then switch to full-duplex in one go. If we
  // did output-then-full-duplex we'd risk a second NotifyWhenDeviceStarted
  // resolving early after checking the first audio driver only.
  MediaTrackGraph* graph = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr,
      GetMainThreadSerialEventTarget());

  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;

  RefPtr<AudioProcessingTrack> processingTrack;
  RefPtr<ProcessedMediaTrack> outputTrack;
  RefPtr<MediaInputPort> port;
  RefPtr<AudioInputProcessing> listener;
  auto p = Invoke([&] {
    processingTrack = AudioProcessingTrack::Create(graph);
    outputTrack = graph->CreateForwardedInputTrack(MediaSegment::AUDIO);
    outputTrack->QueueSetAutoend(false);
    outputTrack->AddAudioOutput(reinterpret_cast<void*>(1));
    port = outputTrack->AllocateInputPort(processingTrack);
    /* Primary graph: Open Audio Input through SourceMediaTrack */
    listener = new AudioInputProcessing(2);
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(processingTrack, listener, true));
    processingTrack->SetInputProcessing(listener);
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(processingTrack, listener));
    // Device id does not matter. Ignore.
    processingTrack->ConnectDeviceInput(deviceId, listener,
                                        PRINCIPAL_HANDLE_NONE);
    return graph->NotifyWhenDeviceStarted(processingTrack);
  });

  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);
  Unused << WaitFor(p);

  // Wait for a second worth of audio data. GoFaster is dispatched through a
  // ControlMessage so that it is called in the first audio driver iteration.
  // Otherwise the audio driver might be going very fast while the fallback
  // system clock driver is still in an iteration.
  DispatchFunction([&] {
    processingTrack->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb));
  });
  uint32_t totalFrames = 0;
  WaitUntil(stream->FramesVerifiedEvent(), [&](uint32_t aFrames) {
    totalFrames += aFrames;
    return totalFrames > static_cast<uint32_t>(graph->GraphRate());
  });
  cubeb->DontGoFaster();

  // Clean up.
  DispatchFunction([&] {
    outputTrack->RemoveAudioOutput((void*)1);
    outputTrack->Destroy();
    port->Destroy();
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(processingTrack, listener));
    processingTrack->DisconnectDeviceInput();
    processingTrack->Destroy();
  });

  uint32_t inputRate = stream->InputSampleRate();
  uint32_t inputFrequency = stream->InputFrequency();
  uint64_t preSilenceSamples;
  uint32_t estimatedFreq;
  uint32_t nrDiscontinuities;
  Tie(preSilenceSamples, estimatedFreq, nrDiscontinuities) =
      WaitFor(stream->OutputVerificationEvent());

  EXPECT_EQ(estimatedFreq, inputFrequency);
  std::cerr << "PreSilence: " << preSilenceSamples << std::endl;
  // We buffer 128 frames. See DeviceInputTrack::ProcessInput.
  EXPECT_GE(preSilenceSamples, 128U);
  // If the fallback system clock driver is doing a graph iteration before the
  // first audio driver iteration comes in, that iteration is ignored and
  // results in zeros. It takes one fallback driver iteration *after* the audio
  // driver has started to complete the switch, *usually* resulting two
  // 10ms-iterations of silence; sometimes only one.
  EXPECT_LE(preSilenceSamples, 128U + 2 * inputRate / 100 /* 2*10ms */);
  // The waveform from AudioGenerator starts at 0, but we don't control its
  // ending, so we expect a discontinuity there.
  EXPECT_LE(nrDiscontinuities, 1U);
}

TEST(TestAudioTrackGraph, ReConnectDeviceInput)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  // 48k is a native processing rate, and avoids a resampling pass compared
  // to 44.1k. The resampler may add take a few frames to stabilize, which show
  // as unexected discontinuities in the test.
  const TrackRate rate = 48000;

  MediaTrackGraph* graph = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*Window ID*/ 1, rate, nullptr,
      GetMainThreadSerialEventTarget());

  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;

  RefPtr<AudioProcessingTrack> processingTrack;
  RefPtr<ProcessedMediaTrack> outputTrack;
  RefPtr<MediaInputPort> port;
  RefPtr<AudioInputProcessing> listener;
  auto p = Invoke([&] {
    processingTrack = AudioProcessingTrack::Create(graph);
    outputTrack = graph->CreateForwardedInputTrack(MediaSegment::AUDIO);
    outputTrack->QueueSetAutoend(false);
    outputTrack->AddAudioOutput(reinterpret_cast<void*>(1));
    port = outputTrack->AllocateInputPort(processingTrack);
    listener = new AudioInputProcessing(2);
    processingTrack->SetInputProcessing(listener);
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(processingTrack, listener));
    processingTrack->ConnectDeviceInput(deviceId, listener,
                                        PRINCIPAL_HANDLE_NONE);
    return graph->NotifyWhenDeviceStarted(processingTrack);
  });

  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);
  Unused << WaitFor(p);

  // Set a drift factor so that we don't dont produce perfect 10ms-chunks. This
  // will exercise whatever buffers are in the audio processing pipeline, and
  // the bookkeeping surrounding them.
  stream->SetDriftFactor(1.111);

  // Wait for a second worth of audio data. GoFaster is dispatched through a
  // ControlMessage so that it is called in the first audio driver iteration.
  // Otherwise the audio driver might be going very fast while the fallback
  // system clock driver is still in an iteration.
  DispatchFunction([&] {
    processingTrack->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb));
  });
  {
    uint32_t totalFrames = 0;
    WaitUntil(stream->FramesProcessedEvent(), [&](uint32_t aFrames) {
      totalFrames += aFrames;
      return totalFrames > static_cast<uint32_t>(graph->GraphRate());
    });
  }
  cubeb->DontGoFaster();

  // Close the input to see that no asserts go off due to bad state.
  DispatchFunction([&] { processingTrack->DisconnectDeviceInput(); });

  stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_FALSE(stream->mHasInput);
  Unused << WaitFor(
      Invoke([&] { return graph->NotifyWhenDeviceStarted(processingTrack); }));

  // Output-only. Wait for another second before unmuting.
  DispatchFunction([&] {
    processingTrack->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb));
  });
  {
    uint32_t totalFrames = 0;
    WaitUntil(stream->FramesProcessedEvent(), [&](uint32_t aFrames) {
      totalFrames += aFrames;
      return totalFrames > static_cast<uint32_t>(graph->GraphRate());
    });
  }
  cubeb->DontGoFaster();

  // Re-open the input to again see that no asserts go off due to bad state.
  DispatchFunction([&] {
    // Device id does not matter. Ignore.
    processingTrack->ConnectDeviceInput(deviceId, listener,
                                        PRINCIPAL_HANDLE_NONE);
  });

  stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);
  Unused << WaitFor(
      Invoke([&] { return graph->NotifyWhenDeviceStarted(processingTrack); }));

  // Full-duplex. Wait for another second before finishing.
  DispatchFunction([&] {
    processingTrack->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb));
  });
  {
    uint32_t totalFrames = 0;
    WaitUntil(stream->FramesProcessedEvent(), [&](uint32_t aFrames) {
      totalFrames += aFrames;
      return totalFrames > static_cast<uint32_t>(graph->GraphRate());
    });
  }
  cubeb->DontGoFaster();

  // Clean up.
  DispatchFunction([&] {
    outputTrack->RemoveAudioOutput((void*)1);
    outputTrack->Destroy();
    port->Destroy();
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(processingTrack, listener));
    processingTrack->DisconnectDeviceInput();
    processingTrack->Destroy();
  });

  uint32_t inputRate = stream->InputSampleRate();
  uint32_t inputFrequency = stream->InputFrequency();
  uint64_t preSilenceSamples;
  uint32_t estimatedFreq;
  uint32_t nrDiscontinuities;
  Tie(preSilenceSamples, estimatedFreq, nrDiscontinuities) =
      WaitFor(stream->OutputVerificationEvent());

  EXPECT_EQ(estimatedFreq, inputFrequency);
  std::cerr << "PreSilence: " << preSilenceSamples << std::endl;
  // We buffer 10ms worth of frames in non-passthrough mode, plus up to 128
  // frames as we round up to the nearest block. See
  // AudioInputProcessing::Process and DeviceInputTrack::PrcoessInput.
  EXPECT_GE(preSilenceSamples, 128U + inputRate / 100);
  // If the fallback system clock driver is doing a graph iteration before the
  // first audio driver iteration comes in, that iteration is ignored and
  // results in zeros. It takes one fallback driver iteration *after* the audio
  // driver has started to complete the switch, *usually* resulting two
  // 10ms-iterations of silence; sometimes only one.
  EXPECT_LE(preSilenceSamples, 128U + 3 * inputRate / 100 /* 3*10ms */);
  // The waveform from AudioGenerator starts at 0, but we don't control its
  // ending, so we expect a discontinuity there. Note that this check is only
  // for the waveform on the stream *after* re-opening the input.
  EXPECT_LE(nrDiscontinuities, 1U);
}

// Sum the signal to mono and compute the root mean square, in float32,
// regardless of the input format.
float rmsf32(AudioDataValue* aSamples, uint32_t aChannels, uint32_t aFrames) {
  float downmixed;
  float rms = 0.;
  uint32_t readIdx = 0;
  for (uint32_t i = 0; i < aFrames; i++) {
    downmixed = 0.;
    for (uint32_t j = 0; j < aChannels; j++) {
      downmixed += AudioSampleToFloat(aSamples[readIdx++]);
    }
    rms += downmixed * downmixed;
  }
  rms = rms / aFrames;
  return sqrt(rms);
}

TEST(TestAudioTrackGraph, AudioProcessingTrackDisabling)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* graph = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr,
      GetMainThreadSerialEventTarget());

  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;

  RefPtr<AudioProcessingTrack> processingTrack;
  RefPtr<ProcessedMediaTrack> outputTrack;
  RefPtr<MediaInputPort> port;
  RefPtr<AudioInputProcessing> listener;
  auto p = Invoke([&] {
    processingTrack = AudioProcessingTrack::Create(graph);
    outputTrack = graph->CreateForwardedInputTrack(MediaSegment::AUDIO);
    outputTrack->QueueSetAutoend(false);
    outputTrack->AddAudioOutput(reinterpret_cast<void*>(1));
    port = outputTrack->AllocateInputPort(processingTrack);
    /* Primary graph: Open Audio Input through SourceMediaTrack */
    listener = new AudioInputProcessing(2);
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(processingTrack, listener, true));
    processingTrack->SetInputProcessing(listener);
    processingTrack->ConnectDeviceInput(deviceId, listener,
                                        PRINCIPAL_HANDLE_NONE);
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(processingTrack, listener));
    return graph->NotifyWhenDeviceStarted(processingTrack);
  });

  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);
  Unused << WaitFor(p);

  stream->SetOutputRecordingEnabled(true);

  // Wait for a second worth of audio data.
  uint32_t totalFrames = 0;
  WaitUntil(stream->FramesProcessedEvent(), [&](uint32_t aFrames) {
    totalFrames += aFrames;
    return totalFrames > static_cast<uint32_t>(graph->GraphRate());
  });

  const uint32_t ITERATION_COUNT = 5;
  uint32_t iterations = ITERATION_COUNT;
  DisabledTrackMode currentMode = DisabledTrackMode::SILENCE_BLACK;
  while (iterations--) {
    // toggle the track enabled mode, wait a second, do this ITERATION_COUNT
    // times
    DispatchFunction([&] {
      processingTrack->SetDisabledTrackMode(currentMode);
      if (currentMode == DisabledTrackMode::SILENCE_BLACK) {
        currentMode = DisabledTrackMode::ENABLED;
      } else {
        currentMode = DisabledTrackMode::SILENCE_BLACK;
      }
    });

    totalFrames = 0;
    WaitUntil(stream->FramesProcessedEvent(), [&](uint32_t aFrames) {
      totalFrames += aFrames;
      return totalFrames > static_cast<uint32_t>(graph->GraphRate());
    });
  }

  // Clean up.
  DispatchFunction([&] {
    outputTrack->RemoveAudioOutput((void*)1);
    outputTrack->Destroy();
    port->Destroy();
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(processingTrack, listener));
    processingTrack->DisconnectDeviceInput();
    processingTrack->Destroy();
  });

  uint64_t preSilenceSamples;
  uint32_t estimatedFreq;
  uint32_t nrDiscontinuities;
  Tie(preSilenceSamples, estimatedFreq, nrDiscontinuities) =
      WaitFor(stream->OutputVerificationEvent());

  auto data = stream->TakeRecordedOutput();

  // check that there is non-silence and silence at the expected time in the
  // stereo recording, while allowing for a bit of scheduling uncertainty, by
  // checking half a second after the theoretical muting/unmuting.
  // non-silence starts around: 0s, 2s, 4s
  // silence start around: 1s, 3s, 5s
  // To detect silence or non-silence, we compute the RMS of the signal for
  // 100ms.
  float noisyTime_s[] = {0.5, 2.5, 4.5};
  float silenceTime_s[] = {1.5, 3.5, 5.5};

  uint32_t rate = graph->GraphRate();
  for (float& time : noisyTime_s) {
    uint32_t startIdx = time * rate * 2 /* stereo */;
    EXPECT_NE(rmsf32(&(data[startIdx]), 2, rate / 10), 0.0);
  }

  for (float& time : silenceTime_s) {
    uint32_t startIdx = time * rate * 2 /* stereo */;
    EXPECT_EQ(rmsf32(&(data[startIdx]), 2, rate / 10), 0.0);
  }
}

TEST(TestAudioTrackGraph, SetRequestedInputChannelCount)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* graph = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr,
      GetMainThreadSerialEventTarget());

  // Open a 2-channel input stream.
  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;
  RefPtr<AudioProcessingTrack> track = AudioProcessingTrack::Create(graph);
  RefPtr<AudioInputProcessing> listener = new AudioInputProcessing(2);
  track->SetInputProcessing(listener);
  track->GraphImpl()->AppendMessage(
      MakeUnique<SetPassThrough>(track, listener, true));
  track->GraphImpl()->AppendMessage(
      MakeUnique<StartInputProcessing>(track, listener));
  track->ConnectDeviceInput(deviceId, listener, PRINCIPAL_HANDLE_NONE);
  EXPECT_EQ(track->DeviceId().value(), deviceId);

  auto started = Invoke([&] { return graph->NotifyWhenDeviceStarted(track); });

  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);
  EXPECT_TRUE(stream->mHasOutput);
  EXPECT_EQ(stream->InputChannels(), 2U);
  Unused << WaitFor(started);

  // Request an input channel count of 1. This should re-create the input stream
  // accordingly.
  {
    bool destroyed = false;
    MediaEventListener destroyListener = cubeb->StreamDestroyEvent().Connect(
        AbstractThread::GetCurrent(),
        [&](const RefPtr<SmartMockCubebStream>& aDestroyed) {
          destroyed = aDestroyed.get() == stream.get();
        });

    RefPtr<SmartMockCubebStream> newStream;
    MediaEventListener restartListener = cubeb->StreamInitEvent().Connect(
        AbstractThread::GetCurrent(),
        [&](const RefPtr<SmartMockCubebStream>& aCreated) {
          newStream = aCreated;
        });

    DispatchFunction([&] {
      track->GraphImpl()->AppendMessage(
          MakeUnique<SetRequestedInputChannelCount>(track, listener, 1));
    });

    SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
        "TEST(TestAudioTrackGraph, SetRequestedInputChannelCount)"_ns,
        [&] { return destroyed && newStream; });

    destroyListener.Disconnect();
    restartListener.Disconnect();

    stream = newStream;

    EXPECT_TRUE(stream->mHasInput);
    EXPECT_EQ(stream->InputChannels(), 1U);
  }

  // Clean up.
  DispatchFunction([&] {
    track->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(track, listener));
    track->DisconnectDeviceInput();
    track->Destroy();
  });
  RefPtr<SmartMockCubebStream> destroyed = WaitFor(cubeb->StreamDestroyEvent());
  EXPECT_EQ(destroyed.get(), stream.get());
}

// The GraphDriver's input channel count is always the same as the max input
// channel among the GraphDriver's AudioProcessingTracks. This test checks if
// the GraphDriver is switched when the max input channel among the
// AudioProcessingTracks change.
TEST(TestAudioTrackGraph, SwitchingDriverIfMaxChannelCountChanged)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());
  auto unforcer = WaitFor(cubeb->ForceAudioThread()).unwrap();
  Unused << unforcer;

  MediaTrackGraph* graph = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr,
      GetMainThreadSerialEventTarget());

  // Request a new input channel count and expect to have a new stream.
  auto setNewChannelCount = [&](const RefPtr<AudioProcessingTrack>& aTrack,
                                const RefPtr<AudioInputProcessing>& aListener,
                                RefPtr<SmartMockCubebStream>& aStream,
                                uint32_t aChannelCount) {
    ASSERT_TRUE(!!aTrack);
    ASSERT_TRUE(!!aListener);
    ASSERT_TRUE(!!aStream);
    ASSERT_TRUE(aStream->mHasInput);
    ASSERT_NE(aChannelCount, 0U);

    bool destroyed = false;
    MediaEventListener destroyListener = cubeb->StreamDestroyEvent().Connect(
        AbstractThread::GetCurrent(),
        [&](const RefPtr<SmartMockCubebStream>& aDestroyed) {
          destroyed = aDestroyed.get() == aStream.get();
        });

    RefPtr<SmartMockCubebStream> newStream;
    MediaEventListener restartListener = cubeb->StreamInitEvent().Connect(
        AbstractThread::GetCurrent(),
        [&](const RefPtr<SmartMockCubebStream>& aCreated) {
          newStream = aCreated;
        });

    DispatchFunction([&] {
      aTrack->GraphImpl()->AppendMessage(
          MakeUnique<SetRequestedInputChannelCount>(aTrack, aListener,
                                                    aChannelCount));
    });

    SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
        "TEST(TestAudioTrackGraph, SwitchingDriverIfMaxChannelCountChanged) #1"_ns,
        [&] { return destroyed && newStream; });

    destroyListener.Disconnect();
    restartListener.Disconnect();

    aStream = newStream;
  };

  // Open a new track and expect to have a new stream.
  auto openTrack = [&](RefPtr<SmartMockCubebStream>& aCurrentStream,
                       RefPtr<AudioProcessingTrack>& aTrack,
                       RefPtr<AudioInputProcessing>& aListener,
                       CubebUtils::AudioDeviceID aDevice,
                       uint32_t aChannelCount) {
    ASSERT_TRUE(!!aCurrentStream);
    ASSERT_TRUE(aCurrentStream->mHasInput);
    ASSERT_TRUE(aChannelCount > aCurrentStream->InputChannels());
    ASSERT_TRUE(!aTrack);
    ASSERT_TRUE(!aListener);

    bool destroyed = false;
    MediaEventListener destroyListener = cubeb->StreamDestroyEvent().Connect(
        AbstractThread::GetCurrent(),
        [&](const RefPtr<SmartMockCubebStream>& aDestroyed) {
          destroyed = aDestroyed.get() == aCurrentStream.get();
        });

    RefPtr<SmartMockCubebStream> newStream;
    MediaEventListener restartListener = cubeb->StreamInitEvent().Connect(
        AbstractThread::GetCurrent(),
        [&](const RefPtr<SmartMockCubebStream>& aCreated) {
          newStream = aCreated;
        });

    aTrack = AudioProcessingTrack::Create(graph);
    aListener = new AudioInputProcessing(aChannelCount);
    aTrack->SetInputProcessing(aListener);
    aTrack->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(aTrack, aListener, true));
    aTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(aTrack, aListener));

    DispatchFunction([&] {
      aTrack->ConnectDeviceInput(aDevice, aListener, PRINCIPAL_HANDLE_NONE);
    });

    SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
        "TEST(TestAudioTrackGraph, SwitchingDriverIfMaxChannelCountChanged) #2"_ns,
        [&] { return destroyed && newStream; });

    destroyListener.Disconnect();
    restartListener.Disconnect();

    aCurrentStream = newStream;
  };

  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;

  // Open a 1-channel AudioProcessingTrack.
  RefPtr<AudioProcessingTrack> track1 = AudioProcessingTrack::Create(graph);
  RefPtr<AudioInputProcessing> listener1 = new AudioInputProcessing(1);
  track1->SetInputProcessing(listener1);
  track1->GraphImpl()->AppendMessage(
      MakeUnique<SetPassThrough>(track1, listener1, true));
  track1->GraphImpl()->AppendMessage(
      MakeUnique<StartInputProcessing>(track1, listener1));
  track1->ConnectDeviceInput(deviceId, listener1, PRINCIPAL_HANDLE_NONE);
  EXPECT_EQ(track1->DeviceId().value(), deviceId);

  auto started = Invoke([&] { return graph->NotifyWhenDeviceStarted(track1); });

  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);
  EXPECT_TRUE(stream->mHasOutput);
  EXPECT_EQ(stream->InputChannels(), 1U);
  Unused << WaitFor(started);

  // Open a 2-channel AudioProcessingTrack and wait for a new driver since the
  // max-channel becomes 2 now.
  RefPtr<AudioProcessingTrack> track2;
  RefPtr<AudioInputProcessing> listener2;
  openTrack(stream, track2, listener2, deviceId, 2);
  EXPECT_EQ(stream->InputChannels(), 2U);

  // Set the second AudioProcessingTrack to 1-channel and wait for a new driver
  // since the max-channel becomes 1 now.
  setNewChannelCount(track2, listener2, stream, 1);
  EXPECT_EQ(stream->InputChannels(), 1U);

  // Set the first AudioProcessingTrack to 2-channel and wait for a new driver
  // since the max input channel becomes 2 now.
  setNewChannelCount(track1, listener1, stream, 2);
  EXPECT_EQ(stream->InputChannels(), 2U);

  // Close the second AudioProcessingTrack (1-channel) then the first one
  // (2-channel) so we won't have driver switching.
  DispatchFunction([&] {
    track2->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(track2, listener2));
    track2->DisconnectDeviceInput();
    track2->Destroy();
  });
  DispatchFunction([&] {
    track1->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(track1, listener1));
    track1->DisconnectDeviceInput();
    track1->Destroy();
  });
  RefPtr<SmartMockCubebStream> destroyedStream =
      WaitFor(cubeb->StreamDestroyEvent());
  EXPECT_EQ(destroyedStream.get(), stream.get());
}

TEST(TestAudioTrackGraph, SetInputChannelCountBeforeAudioCallbackDriver)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* graph = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr,
      GetMainThreadSerialEventTarget());

  // Set the input channel count of AudioInputProcessing, which will force
  // MediaTrackGraph to re-evaluate input device, when the MediaTrackGraph is
  // driven by the SystemClockDriver.

  RefPtr<AudioProcessingTrack> track;
  RefPtr<AudioInputProcessing> listener;
  {
    MozPromiseHolder<GenericPromise> h;
    RefPtr<GenericPromise> p = h.Ensure(__func__);

    struct GuardMessage : public ControlMessage {
      MozPromiseHolder<GenericPromise> mHolder;

      GuardMessage(MediaTrack* aTrack,
                   MozPromiseHolder<GenericPromise>&& aHolder)
          : ControlMessage(aTrack), mHolder(std::move(aHolder)) {}
      void Run() override {
        mTrack->GraphImpl()->Dispatch(NS_NewRunnableFunction(
            "TestAudioTrackGraph::SetInputChannel::Message::Resolver",
            [holder = std::move(mHolder)]() mutable {
              holder.Resolve(true, __func__);
            }));
      }
    };

    DispatchFunction([&] {
      track = AudioProcessingTrack::Create(graph);
      listener = new AudioInputProcessing(2);
      track->GraphImpl()->AppendMessage(
          MakeUnique<SetPassThrough>(track, listener, true));
      track->SetInputProcessing(listener);
      track->GraphImpl()->AppendMessage(
          MakeUnique<SetRequestedInputChannelCount>(track, listener, 1));
      track->GraphImpl()->AppendMessage(
          MakeUnique<GuardMessage>(track, std::move(h)));
    });

    Unused << WaitFor(p);
  }

  // Open a full-duplex AudioCallbackDriver.

  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;
  RefPtr<MediaInputPort> port;
  DispatchFunction([&] {
    track->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(track, listener));
    track->ConnectDeviceInput(deviceId, listener, PRINCIPAL_HANDLE_NONE);
  });

  // MediaTrackGraph will create a output-only AudioCallbackDriver in
  // CheckDriver before we open an audio input above, since AudioProcessingTrack
  // is a audio-type MediaTrack, so we need to wait here until the duplex
  // AudioCallbackDriver is created.
  RefPtr<SmartMockCubebStream> stream;
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      "TEST(TestAudioTrackGraph, SetInputChannelCountBeforeAudioCallbackDriver)"_ns,
      [&] {
        stream = WaitFor(cubeb->StreamInitEvent());
        EXPECT_TRUE(stream->mHasOutput);
        return stream->mHasInput;
      });
  EXPECT_EQ(stream->InputChannels(), 1U);

  Unused << WaitFor(
      Invoke([&] { return graph->NotifyWhenDeviceStarted(track); }));

  // Clean up.
  DispatchFunction([&] {
    track->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(track, listener));
    track->DisconnectDeviceInput();
    track->Destroy();
  });
  Unused << WaitFor(cubeb->StreamDestroyEvent());
}

TEST(TestAudioTrackGraph, StartAudioDeviceBeforeStartingAudioProcessing)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* graph = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr,
      GetMainThreadSerialEventTarget());

  // Create a duplex AudioCallbackDriver
  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;
  RefPtr<AudioProcessingTrack> track;
  RefPtr<AudioInputProcessing> listener;
  auto started = Invoke([&] {
    track = AudioProcessingTrack::Create(graph);
    listener = new AudioInputProcessing(2);
    track->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(track, listener, true));
    track->SetInputProcessing(listener);
    // Start audio device without starting audio processing.
    track->ConnectDeviceInput(deviceId, listener, PRINCIPAL_HANDLE_NONE);
    return graph->NotifyWhenDeviceStarted(track);
  });

  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  Result<bool, nsresult> rv = WaitFor(started);
  EXPECT_TRUE(rv.unwrapOr(false));
  EXPECT_TRUE(stream->mHasInput);
  EXPECT_TRUE(stream->mHasOutput);

  // Wait for a second to make sure audio output callback has been fired.
  DispatchFunction(
      [&] { track->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb)); });
  {
    uint32_t totalFrames = 0;
    WaitUntil(stream->FramesProcessedEvent(), [&](uint32_t aFrames) {
      totalFrames += aFrames;
      return totalFrames > static_cast<uint32_t>(graph->GraphRate());
    });
  }
  cubeb->DontGoFaster();

  // Start the audio processing.
  DispatchFunction([&] {
    track->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(track, listener));
  });

  // Wait for a second to make sure audio output callback has been fired.
  DispatchFunction(
      [&] { track->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb)); });
  {
    uint32_t totalFrames = 0;
    WaitUntil(stream->FramesProcessedEvent(), [&](uint32_t aFrames) {
      totalFrames += aFrames;
      return totalFrames > static_cast<uint32_t>(graph->GraphRate());
    });
  }
  cubeb->DontGoFaster();

  // Clean up.
  DispatchFunction([&] {
    track->DisconnectDeviceInput();
    track->Destroy();
  });
  Unused << WaitFor(cubeb->StreamDestroyEvent());
}

TEST(TestAudioTrackGraph, StopAudioProcessingBeforeStoppingAudioDevice)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* graph = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*Window ID*/ 1,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr,
      GetMainThreadSerialEventTarget());

  // Create a duplex AudioCallbackDriver
  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;
  RefPtr<AudioProcessingTrack> track;
  RefPtr<AudioInputProcessing> listener;
  auto started = Invoke([&] {
    track = AudioProcessingTrack::Create(graph);
    listener = new AudioInputProcessing(2);
    track->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(track, listener, true));
    track->SetInputProcessing(listener);
    track->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(track, listener));
    track->ConnectDeviceInput(deviceId, listener, PRINCIPAL_HANDLE_NONE);
    return graph->NotifyWhenDeviceStarted(track);
  });

  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  Result<bool, nsresult> rv = WaitFor(started);
  EXPECT_TRUE(rv.unwrapOr(false));
  EXPECT_TRUE(stream->mHasInput);
  EXPECT_TRUE(stream->mHasOutput);

  // Wait for a second to make sure audio output callback has been fired.
  DispatchFunction(
      [&] { track->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb)); });
  {
    uint32_t totalFrames = 0;
    WaitUntil(stream->FramesProcessedEvent(), [&](uint32_t aFrames) {
      totalFrames += aFrames;
      return totalFrames > static_cast<uint32_t>(graph->GraphRate());
    });
  }
  cubeb->DontGoFaster();

  // Stop the audio processing
  DispatchFunction([&] {
    track->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(track, listener));
  });

  // Wait for a second to make sure audio output callback has been fired.
  DispatchFunction(
      [&] { track->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb)); });
  {
    uint32_t totalFrames = 0;
    WaitUntil(stream->FramesProcessedEvent(), [&](uint32_t aFrames) {
      totalFrames += aFrames;
      return totalFrames > static_cast<uint32_t>(graph->GraphRate());
    });
  }
  cubeb->DontGoFaster();

  // Clean up.
  DispatchFunction([&] {
    track->DisconnectDeviceInput();
    track->Destroy();
  });
  Unused << WaitFor(cubeb->StreamDestroyEvent());
}

void TestCrossGraphPort(uint32_t aInputRate, uint32_t aOutputRate,
                        float aDriftFactor, uint32_t aBufferMs = 50) {
  std::cerr << "TestCrossGraphPort input: " << aInputRate
            << ", output: " << aOutputRate << ", driftFactor: " << aDriftFactor
            << std::endl;

  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());
  auto unforcer = WaitFor(cubeb->ForceAudioThread()).unwrap();
  Unused << unforcer;

  cubeb->SetStreamStartFreezeEnabled(true);

  /* Primary graph: Create the graph. */
  MediaTrackGraph* primary = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER,
      /*Window ID*/ 1, aInputRate, nullptr, GetMainThreadSerialEventTarget());

  /* Partner graph: Create the graph. */
  MediaTrackGraph* partner = MediaTrackGraphImpl::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*Window ID*/ 1, aOutputRate,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(1),
      GetMainThreadSerialEventTarget());

  const CubebUtils::AudioDeviceID deviceId = (CubebUtils::AudioDeviceID)1;

  RefPtr<AudioProcessingTrack> processingTrack;
  RefPtr<AudioInputProcessing> listener;
  auto primaryStarted = Invoke([&] {
    /* Primary graph: Create input track and open it */
    processingTrack = AudioProcessingTrack::Create(primary);
    listener = new AudioInputProcessing(2);
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(processingTrack, listener, true));
    processingTrack->SetInputProcessing(listener);
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(processingTrack, listener));
    processingTrack->ConnectDeviceInput(deviceId, listener,
                                        PRINCIPAL_HANDLE_NONE);
    return primary->NotifyWhenDeviceStarted(processingTrack);
  });

  RefPtr<SmartMockCubebStream> inputStream = WaitFor(cubeb->StreamInitEvent());

  RefPtr<CrossGraphTransmitter> transmitter;
  RefPtr<MediaInputPort> port;
  RefPtr<CrossGraphReceiver> receiver;
  auto partnerStarted = Invoke([&] {
    /* Partner graph: Create CrossGraphReceiver */
    receiver = partner->CreateCrossGraphReceiver(primary->GraphRate());

    /* Primary graph: Create CrossGraphTransmitter */
    transmitter = primary->CreateCrossGraphTransmitter(receiver);

    /* How the input track connects to another ProcessedMediaTrack.
     * Check in MediaManager how it is connected to AudioStreamTrack. */
    port = transmitter->AllocateInputPort(processingTrack);
    receiver->AddAudioOutput((void*)1);
    return partner->NotifyWhenDeviceStarted(receiver);
  });

  RefPtr<SmartMockCubebStream> partnerStream =
      WaitFor(cubeb->StreamInitEvent());
  partnerStream->SetDriftFactor(aDriftFactor);

  cubeb->SetStreamStartFreezeEnabled(false);

  // One source of non-determinism in this type of test is that inputStream
  // and partnerStream are started in sequence by the CubebOperation thread pool
  // (of size 1). To minimize the chance that the stream that starts first sees
  // an iteration before the other has started - this is a source of pre-silence
  // - we freeze both on start and thaw them together here.
  // Note that another source of non-determinism is the fallback driver. Handing
  // over from the fallback to the audio driver requires first an audio callback
  // (deterministic with the fake audio thread), then a fallback driver
  // iteration (non-deterministic, since each graph has its own fallback driver,
  // each with its own dedicated thread, which we have no control over). This
  // non-determinism is worrisome, but both fallback drivers are likely to
  // exhibit similar characteristics, hopefully keeping the level of
  // non-determinism down sufficiently for this test to pass.
  inputStream->Thaw();
  partnerStream->Thaw();

  Unused << WaitFor(primaryStarted);
  Unused << WaitFor(partnerStarted);

  // Wait for 3s worth of audio data on the receiver stream.
  DispatchFunction([&] {
    processingTrack->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb));
  });
  uint32_t totalFrames = 0;
  WaitUntil(partnerStream->FramesVerifiedEvent(), [&](uint32_t aFrames) {
    totalFrames += aFrames;
    return totalFrames > static_cast<uint32_t>(partner->GraphRate() * 3);
  });
  cubeb->DontGoFaster();

  DispatchFunction([&] {
    // Clean up on MainThread
    receiver->RemoveAudioOutput((void*)1);
    receiver->Destroy();
    transmitter->Destroy();
    port->Destroy();
    processingTrack->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(processingTrack, listener));
    processingTrack->DisconnectDeviceInput();
    processingTrack->Destroy();
  });

  uint32_t inputFrequency = inputStream->InputFrequency();
  uint32_t partnerRate = partnerStream->InputSampleRate();

  uint64_t preSilenceSamples;
  float estimatedFreq;
  uint32_t nrDiscontinuities;
  Tie(preSilenceSamples, estimatedFreq, nrDiscontinuities) =
      WaitFor(partnerStream->OutputVerificationEvent());

  EXPECT_NEAR(estimatedFreq, inputFrequency / aDriftFactor, 5);
  uint32_t expectedPreSilence =
      static_cast<uint32_t>(partnerRate * aDriftFactor / 1000 * aBufferMs);
  uint32_t margin = partnerRate / 20 /* +/- 50ms */;
  EXPECT_NEAR(preSilenceSamples, expectedPreSilence, margin);
  // The waveform from AudioGenerator starts at 0, but we don't control its
  // ending, so we expect a discontinuity there.
  EXPECT_LE(nrDiscontinuities, 1U);
}

TEST(TestAudioTrackGraph, CrossGraphPort)
{
  TestCrossGraphPort(44100, 44100, 1);
  TestCrossGraphPort(44100, 44100, 1.08);
  TestCrossGraphPort(44100, 44100, 0.92);

  TestCrossGraphPort(48000, 44100, 1);
  TestCrossGraphPort(48000, 44100, 1.08);
  TestCrossGraphPort(48000, 44100, 0.92);

  TestCrossGraphPort(44100, 48000, 1);
  TestCrossGraphPort(44100, 48000, 1.08);
  TestCrossGraphPort(44100, 48000, 0.92);

  TestCrossGraphPort(52110, 17781, 1);
  TestCrossGraphPort(52110, 17781, 1.08);
  TestCrossGraphPort(52110, 17781, 0.92);
}

TEST(TestAudioTrackGraph, CrossGraphPortLargeBuffer)
{
  const int32_t oldBuffering = Preferences::GetInt(DRIFT_BUFFERING_PREF);
  const int32_t longBuffering = 5000;
  Preferences::SetInt(DRIFT_BUFFERING_PREF, longBuffering);

  TestCrossGraphPort(44100, 44100, 1.02, longBuffering);
  TestCrossGraphPort(48000, 44100, 1.08, longBuffering);
  TestCrossGraphPort(44100, 48000, 0.95, longBuffering);
  TestCrossGraphPort(52110, 17781, 0.92, longBuffering);

  Preferences::SetInt(DRIFT_BUFFERING_PREF, oldBuffering);
}
#endif  // MOZ_WEBRTC
