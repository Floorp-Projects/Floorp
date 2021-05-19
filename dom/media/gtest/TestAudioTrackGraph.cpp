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
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr);

  const CubebUtils::AudioDeviceID deviceId = (void*)1;

  // Dummy track to make graph rolling. Add it and remove it to remove the
  // graph from the global hash table and let it shutdown.
  //
  // We open an input through this track so that there's something triggering
  // EnsureNextIteration on the fallback driver after the callback driver has
  // gotten the error.
  RefPtr<AudioInputTrack> inputTrack;
  RefPtr<AudioInputProcessing> listener;
  auto started = Invoke([&] {
    inputTrack = AudioInputTrack::Create(graph);
    listener = new AudioInputProcessing(2, PRINCIPAL_HANDLE_NONE);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(inputTrack, listener, true));
    inputTrack->SetInputProcessing(listener);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(inputTrack, listener));
    inputTrack->OpenAudioInput(deviceId, listener);
    EXPECT_EQ(inputTrack->DeviceId().value(), deviceId);
    return graph->NotifyWhenDeviceStarted(inputTrack);
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
      [&] { return errored && init; });
  errorListener.Disconnect();
  initListener.Disconnect();

  // Clean up.
  DispatchFunction([&] {
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(listener));
    inputTrack->CloseAudioInput();
    inputTrack->Destroy();
  });
  WaitFor(cubeb->StreamDestroyEvent());
}

TEST(TestAudioTrackGraph, AudioInputTrack)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());
  auto unforcer = WaitFor(cubeb->ForceAudioThread()).unwrap();
  Unused << unforcer;

  // Start on a system clock driver, then switch to full-duplex in one go. If we
  // did output-then-full-duplex we'd risk a second NotifyWhenDeviceStarted
  // resolving early after checking the first audio driver only.
  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr);

  const CubebUtils::AudioDeviceID deviceId = (void*)1;

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
    /* Primary graph: Open Audio Input through SourceMediaTrack */
    listener = new AudioInputProcessing(2, PRINCIPAL_HANDLE_NONE);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(inputTrack, listener, true));
    inputTrack->SetInputProcessing(listener);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(inputTrack, listener));
    // Device id does not matter. Ignore.
    inputTrack->OpenAudioInput(deviceId, listener);
    return graph->NotifyWhenDeviceStarted(inputTrack);
  });

  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
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
    inputTrack->CloseAudioInput();
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
  // The waveform from AudioGenerator starts at 0, but we don't control its
  // ending, so we expect a discontinuity there.
  EXPECT_LE(nrDiscontinuities, 1U);
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

  const CubebUtils::AudioDeviceID deviceId = (void*)1;

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
    inputTrack->OpenAudioInput(deviceId, listener);
    return graph->NotifyWhenDeviceStarted(inputTrack);
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
  DispatchFunction([&] { inputTrack->CloseAudioInput(); });

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
    inputTrack->OpenAudioInput(deviceId, listener);
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
    inputTrack->CloseAudioInput();
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

#  ifndef WIN32  // failure on windows10x32
TEST(TestAudioTrackGraph, AudioInputTrackDisabling)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr);

  const CubebUtils::AudioDeviceID deviceId = (void*)1;

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
    /* Primary graph: Open Audio Input through SourceMediaTrack */
    listener = new AudioInputProcessing(2, PRINCIPAL_HANDLE_NONE);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(inputTrack, listener, true));
    inputTrack->SetInputProcessing(listener);
    inputTrack->OpenAudioInput(deviceId, listener);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(inputTrack, listener));
    return graph->NotifyWhenDeviceStarted(inputTrack);
  });

  RefPtr<SmartMockCubebStream> stream = WaitFor(cubeb->StreamInitEvent());
  EXPECT_TRUE(stream->mHasInput);
  Unused << WaitFor(p);

  stream->SetOutputRecordingEnabled(true);

  // Wait for a second worth of audio data. GoFaster is dispatched through a
  // ControlMessage so that it is called in the first audio driver iteration.
  // Otherwise the audio driver might be going very fast while the fallback
  // system clock driver is still in an iteration.
  DispatchFunction([&] {
    inputTrack->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb));
  });
  uint32_t totalFrames = 0;
  WaitUntil(stream->FramesProcessedEvent(), [&](uint32_t aFrames) {
    totalFrames += aFrames;
    return totalFrames > static_cast<uint32_t>(graph->GraphRate());
  });
  cubeb->DontGoFaster();

  const uint32_t ITERATION_COUNT = 5;
  uint32_t iterations = ITERATION_COUNT;
  DisabledTrackMode currentMode = DisabledTrackMode::SILENCE_BLACK;
  while (iterations--) {
    // toggle the track enabled mode, wait a second, do this ITERATION_COUNT
    // times
    DispatchFunction([&] {
      inputTrack->SetDisabledTrackMode(currentMode);
      if (currentMode == DisabledTrackMode::SILENCE_BLACK) {
        currentMode = DisabledTrackMode::ENABLED;
      } else {
        currentMode = DisabledTrackMode::SILENCE_BLACK;
      }
      inputTrack->GraphImpl()->AppendMessage(MakeUnique<GoFaster>(cubeb));
    });

    totalFrames = 0;
    WaitUntil(stream->FramesProcessedEvent(), [&](uint32_t aFrames) {
      totalFrames += aFrames;
      return totalFrames > static_cast<uint32_t>(graph->GraphRate());
    });
    cubeb->DontGoFaster();
  }

  // Clean up.
  DispatchFunction([&] {
    outputTrack->RemoveAudioOutput((void*)1);
    outputTrack->Destroy();
    port->Destroy();
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StopInputProcessing>(listener));
    inputTrack->CloseAudioInput();
    inputTrack->Destroy();
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
#  endif  // win32

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
  MediaTrackGraph* primary =
      MediaTrackGraph::GetInstance(MediaTrackGraph::SYSTEM_THREAD_DRIVER,
                                   /*window*/ nullptr, aInputRate, nullptr);

  /* Partner graph: Create the graph. */
  MediaTrackGraph* partner = MediaTrackGraph::GetInstance(
      MediaTrackGraph::SYSTEM_THREAD_DRIVER, /*window*/ nullptr, aOutputRate,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(1));

  const CubebUtils::AudioDeviceID deviceId = (void*)1;

  RefPtr<AudioInputTrack> inputTrack;
  RefPtr<AudioInputProcessing> listener;
  auto primaryStarted = Invoke([&] {
    /* Primary graph: Create input track and open it */
    inputTrack = AudioInputTrack::Create(primary);
    listener = new AudioInputProcessing(2, PRINCIPAL_HANDLE_NONE);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<SetPassThrough>(inputTrack, listener, true));
    inputTrack->SetInputProcessing(listener);
    inputTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartInputProcessing>(inputTrack, listener));
    inputTrack->OpenAudioInput(deviceId, listener);
    return primary->NotifyWhenDeviceStarted(inputTrack);
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
    port = transmitter->AllocateInputPort(inputTrack);
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
    inputTrack->CloseAudioInput();
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
