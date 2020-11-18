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

#define DRIFT_BUFFERING_PREF "media.clockdrift.buffering"

using namespace mozilla;

namespace {
/**
 * Waits for an occurrence of aEvent on the current thread (by blocking it,
 * except tasks added to the event loop may run) and returns the event's
 * templated value, if it's non-void.
 *
 * The caller must be wary of eventloop issues, in
 * particular cases where we rely on a stable state runnable, but there is never
 * a task to trigger stable state. In such cases it is the responsibility of the
 * caller to create the needed tasks, as JS would. A noteworthy API that relies
 * on stable state is MediaTrackGraph::GetInstance.
 */
template <typename T>
T WaitFor(MediaEventSource<T>& aEvent) {
  Maybe<T> value;
  MediaEventListener listener = aEvent.Connect(
      AbstractThread::GetCurrent(), [&](T aValue) { value = Some(aValue); });
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      [&] { return value.isSome(); });
  listener.Disconnect();
  return value.value();
}

/**
 * Specialization of WaitFor<T> for void.
 */
void WaitFor(MediaEventSource<void>& aEvent) {
  bool done = false;
  MediaEventListener listener =
      aEvent.Connect(AbstractThread::GetCurrent(), [&] { done = true; });
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      [&] { return done; });
  listener.Disconnect();
}

/**
 * Variant of WaitFor that blocks the caller until a MozPromise has either been
 * resolved or rejected.
 */
template <typename R, typename E, bool Exc>
Result<R, E> WaitFor(const RefPtr<MozPromise<R, E, Exc>>& aPromise) {
  Maybe<Result<R, E>> result;
  aPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [&](R aResult) { result = Some(Result<R, E>(aResult)); },
      [&](E aError) { result = Some(Result<R, E>(aError)); });
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      [&] { return result.isSome(); });
  return result.extract();
}

/**
 * A variation of WaitFor that takes a callback to be called each time aEvent is
 * raised. Blocks the caller until the callback function returns true.
 */
template <typename T, typename CallbackFunction>
void WaitUntil(MediaEventSource<T>& aEvent, const CallbackFunction& aF) {
  bool done = false;
  MediaEventListener listener =
      aEvent.Connect(AbstractThread::GetCurrent(), [&](T aValue) {
        if (!done) {
          done = aF(aValue);
        }
      });
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      [&] { return done; });
  listener.Disconnect();
}

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
  const RefPtr<AudioInputTrack> mInputTrack;
  const RefPtr<AudioInputProcessing> mInputProcessing;

  StartInputProcessing(AudioInputTrack* aTrack,
                       AudioInputProcessing* aInputProcessing)
      : ControlMessage(aTrack),
        mInputTrack(aTrack),
        mInputProcessing(aInputProcessing) {}
  void Run() override { mInputProcessing->Start(); }
};

struct StopInputProcessing : public ControlMessage {
  const RefPtr<AudioInputProcessing> mInputProcessing;

  explicit StopInputProcessing(AudioInputProcessing* aInputProcessing)
      : ControlMessage(nullptr), mInputProcessing(aInputProcessing) {}
  void Run() override { mInputProcessing->Stop(); }
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

  MediaTrackGraph* g1 = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ nullptr);

  MediaTrackGraph* g2 = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(1));

  MediaTrackGraph* g1_2 = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ nullptr);

  MediaTrackGraph* g2_2 = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(1));

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
  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(2));

  // Dummy track to make graph rolling. Add it and remove it to remove the
  // graph from the global hash table and let it shutdown.
  RefPtr<SourceMediaTrack> dummySource;
  DispatchFunction(
      [&] { dummySource = graph->CreateSourceTrack(MediaSegment::AUDIO); });

  MockCubebStream* stream = WaitFor(cubeb->StreamInitEvent());

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

  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr);

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

  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr);

  // Dummy track to make graph rolling. Add it and remove it to remove the
  // graph from the global hash table and let it shutdown.
  RefPtr<AudioInputTrack> inputTrack;
  RefPtr<AudioInputProcessing> listener;
  Unused << WaitFor(Invoke([&] {
    inputTrack = AudioInputTrack::Create(graph);
    listener = new AudioInputProcessing(2, PRINCIPAL_HANDLE_NONE);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(inputTrack, listener, true));
    inputTrack->SetInputProcessing(listener);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(inputTrack, listener));
    return graph->NotifyWhenDeviceStarted(inputTrack);
  }));

  // We open an input through this track so that there's something triggering
  // EnsureNextIteration on the fallback driver after the callback driver has
  // gotten the error.
  auto started = Invoke([&] {
    inputTrack->OpenAudioInput((void*)1, listener);
    return graph->NotifyWhenDeviceStarted(inputTrack);
  });

  MockCubebStream* stream = WaitFor(cubeb->StreamInitEvent());
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
      [&] { return errored && init; });
  errorListener.Disconnect();
  initListener.Disconnect();

  // Clean up.
  DispatchFunction([&] {
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(listener));
    Maybe<CubebUtils::AudioDeviceID> id =
        Some(reinterpret_cast<CubebUtils::AudioDeviceID>(1));
    inputTrack->CloseAudioInput(id);
    inputTrack->Destroy();
  });
  WaitFor(cubeb->StreamDestroyEvent());
}

TEST(TestAudioTrackGraph, AudioInputTrack)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr);

  RefPtr<AudioInputTrack> inputTrack;
  RefPtr<ProcessedMediaTrack> outputTrack;
  RefPtr<MediaInputPort> port;
  RefPtr<AudioInputProcessing> listener;
  Unused << WaitFor(Invoke([&] {
    inputTrack = AudioInputTrack::Create(graph);
    outputTrack = graph->CreateForwardedInputTrack(MediaSegment::AUDIO);
    outputTrack->QueueSetAutoend(false);
    outputTrack->AddAudioOutput(reinterpret_cast<void*>(1));
    port = outputTrack->AllocateInputPort(inputTrack);
    /* Primary graph: Open Audio Input through SourceMediaTrack */
    listener = new AudioInputProcessing(2, PRINCIPAL_HANDLE_NONE);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(inputTrack, listener, true));
    inputTrack->SetInputProcessing(listener);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(inputTrack, listener));
    return graph->NotifyWhenDeviceStarted(inputTrack);
  }));

  DispatchFunction([&] {
    // Device id does not matter. Ignore.
    inputTrack->OpenAudioInput((void*)1, listener);
  });

  auto p = Invoke([&] { return graph->NotifyWhenDeviceStarted(inputTrack); });
  MockCubebStream* stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);
  Unused << WaitFor(p);

  // Wait for a second worth of audio data. GoFaster is dispatched through a
  // ControlMessage so that it is called in the first audio driver iteration.
  // Otherwise the audio driver might be going very fast while the fallback
  // system clock driver is still in an iteration.
  DispatchFunction([&] {
    inputTrack->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb));
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
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(listener));
    Maybe<CubebUtils::AudioDeviceID> id =
        Some(reinterpret_cast<CubebUtils::AudioDeviceID>(1));
    inputTrack->CloseAudioInput(id);
    inputTrack->Destroy();
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
  // We buffer 128 frames in passthrough mode. See AudioInputProcessing::Pull.
  EXPECT_GE(preSilenceSamples, 128U);
  // If the fallback system clock driver is doing a graph iteration before the
  // first audio driver iteration comes in, that iteration is ignored and
  // results in zeros. It takes one fallback driver iteration *after* the audio
  // driver has started to complete the switch, *usually* resulting two
  // 10ms-iterations of silence; sometimes only one.
  EXPECT_LE(preSilenceSamples, 128U + 2 * inputRate / 100 /* 2*10ms */);
  // Waveform may start after the beginning. In this case, there is a gap
  // at the beginning and the end which is counted as discontinuity.
  EXPECT_GE(nrDiscontinuities, 0U);
  EXPECT_LE(nrDiscontinuities, 2U);
}

TEST(TestAudioTrackGraph, ReOpenAudioInput)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  // 48k is a native processing rate, and avoids a resampling pass compared
  // to 44.1k. The resampler may add take a few frames to stabilize, which show
  // as unexected discontinuities in the test.
  const TrackRate rate = 48000;

  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*window*/ nullptr, rate, nullptr);

  RefPtr<AudioInputTrack> inputTrack;
  RefPtr<ProcessedMediaTrack> outputTrack;
  RefPtr<MediaInputPort> port;
  RefPtr<AudioInputProcessing> listener;
  auto p = Invoke([&] {
    inputTrack = AudioInputTrack::Create(graph);
    outputTrack = graph->CreateForwardedInputTrack(MediaSegment::AUDIO);
    outputTrack->QueueSetAutoend(false);
    outputTrack->AddAudioOutput(reinterpret_cast<void*>(1));
    port = outputTrack->AllocateInputPort(inputTrack);
    listener = new AudioInputProcessing(2, PRINCIPAL_HANDLE_NONE);
    inputTrack->SetInputProcessing(listener);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(inputTrack, listener));
    inputTrack->OpenAudioInput((void*)1, listener);
    return graph->NotifyWhenDeviceStarted(inputTrack);
  });

  MockCubebStream* stream = WaitFor(cubeb->StreamInitEvent());
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
    inputTrack->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb));
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
  DispatchFunction([&] {
    // Device id does not matter. Ignore.
    auto id = Some((CubebUtils::AudioDeviceID)1);
    inputTrack->CloseAudioInput(id);
  });

  stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_FALSE(stream->mHasInput);
  Unused << WaitFor(
      Invoke([&] { return graph->NotifyWhenDeviceStarted(inputTrack); }));

  // Output-only. Wait for another second before unmuting.
  DispatchFunction([&] {
    inputTrack->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb));
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
    inputTrack->OpenAudioInput((void*)1, listener);
  });

  stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);
  Unused << WaitFor(
      Invoke([&] { return graph->NotifyWhenDeviceStarted(inputTrack); }));

  // Full-duplex. Wait for another second before finishing.
  DispatchFunction([&] {
    inputTrack->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb));
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
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(listener));
    Maybe<CubebUtils::AudioDeviceID> id =
        Some(reinterpret_cast<CubebUtils::AudioDeviceID>(1));
    inputTrack->CloseAudioInput(id);
    inputTrack->Destroy();
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
  // frames as we round up to the nearest block. See AudioInputProcessing::Pull.
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

void TestCrossGraphPort(uint32_t aInputRate, uint32_t aOutputRate,
                        float aDriftFactor, uint32_t aBufferMs = 50) {
  std::cerr << "TestCrossGraphPort input: " << aInputRate
            << ", output: " << aOutputRate << ", driftFactor: " << aDriftFactor
            << std::endl;

  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  /* Primary graph: Create the graph and a SourceMediaTrack. */
  MediaTrackGraph* primary =
      MediaTrackGraph::GetInstance(MediaTrackGraph::AUDIO_THREAD_DRIVER,
                                   /*window*/ nullptr, aInputRate, nullptr);

  RefPtr<AudioInputTrack> inputTrack;
  RefPtr<AudioInputProcessing> listener;
  DispatchFunction([&] {
    inputTrack = AudioInputTrack::Create(primary);
    listener = new AudioInputProcessing(2, PRINCIPAL_HANDLE_NONE);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(inputTrack, listener, true));
    inputTrack->SetInputProcessing(listener);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(inputTrack, listener));
  });
  WaitFor(cubeb->StreamInitEvent());

  /* Partner graph: Create graph and the CrossGraphReceiver. */
  MediaTrackGraph* partner = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr, aOutputRate,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(1));

  RefPtr<CrossGraphReceiver> receiver;
  RefPtr<CrossGraphTransmitter> transmitter;
  RefPtr<MediaInputPort> port;
  DispatchFunction([&] {
    receiver = partner->CreateCrossGraphReceiver(primary->GraphRate());

    /* Primary graph: Create CrossGraphTransmitter */
    transmitter = primary->CreateCrossGraphTransmitter(receiver);

    /* How the input track connects to another ProcessedMediaTrack.
     * Check in MediaManager how it is connected to AudioStreamTrack. */
    port = transmitter->AllocateInputPort(inputTrack);
    receiver->AddAudioOutput((void*)1);

    /* Primary graph: Open Audio Input through SourceMediaTrack */
    // Device id does not matter. Ignore.
    inputTrack->OpenAudioInput((void*)1, listener);
  });

  MockCubebStream* inputStream = nullptr;
  MockCubebStream* partnerStream = nullptr;
  // Wait for the streams to be created.
  WaitUntil(cubeb->StreamInitEvent(), [&](MockCubebStream* aStream) {
    if (aStream->mHasInput) {
      inputStream = aStream;
    } else {
      partnerStream = aStream;
    }
    return inputStream && partnerStream;
  });

  partnerStream->SetDriftFactor(aDriftFactor);

  // Wait for a second worth of audio data. GoFaster is dispatched through a
  // ControlMessage so that it is called in the first audio driver iteration.
  // Otherwise the audio driver might be going very fast while the fallback
  // system clock driver is still in an iteration.
  // Wait for 3s worth of audio data on the receiver stream.
  DispatchFunction([&] {
    inputTrack->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb));
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
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(listener));
    Maybe<CubebUtils::AudioDeviceID> id =
        Some(reinterpret_cast<CubebUtils::AudioDeviceID>(1));
    inputTrack->CloseAudioInput(id);
    inputTrack->Destroy();
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
  EXPECT_LE(nrDiscontinuities, 2U);
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
