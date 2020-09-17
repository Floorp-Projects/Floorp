/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTrackGraphImpl.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

#include "CrossGraphPort.h"
#include "GMPTestMonitor.h"
#include "MediaEngineWebRTCAudio.h"
#include "MockCubeb.h"

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

  EXPECT_NE(g1, g2) << "Different graphs have due to different device ids";
  EXPECT_EQ(g1, g1_2) << "Same graphs for same device ids";
  EXPECT_EQ(g2, g2_2) << "Same graphs for same device ids";

  // Dummy track to make graph rolling. Add it and remove it to remove the
  // graph from the global hash table and let it shutdown.
  RefPtr<SourceMediaTrack> dummySource1 =
      g1->CreateSourceTrack(MediaSegment::AUDIO);
  RefPtr<SourceMediaTrack> dummySource2 =
      g2->CreateSourceTrack(MediaSegment::AUDIO);

  // Use a test monitor and a counter to wait for the current Graphs. It will
  // wait for the Graphs to init and shutting down before the test finish.
  // Otherwise, the workflow of the current graphs might affect the following
  // tests (cubeb is a single instance process-wide).
  GMPTestMonitor testMonitor;
  Atomic<int> counter{0};

  /* Use a ControlMessage to signal that the Graph has requested shutdown. */
  class Message : public ControlMessage {
   public:
    explicit Message(MediaTrack* aTrack) : ControlMessage(aTrack) {}
    void Run() override {
      MOZ_ASSERT(mTrack->GraphImpl()->CurrentDriver()->AsAudioCallbackDriver());
      if (++(*mCounter) == 2) {
        mTestMonitor->SetFinished();
      }
    }
    void RunDuringShutdown() override {
      // During shutdown we still want the listener's NotifyRemoved to be
      // called, since not doing that might block shutdown of other modules.
      Run();
    }
    GMPTestMonitor* mTestMonitor = nullptr;
    Atomic<int>* mCounter = nullptr;
  };

  UniquePtr<Message> message1 = MakeUnique<Message>(dummySource1);
  message1->mTestMonitor = &testMonitor;
  message1->mCounter = &counter;
  dummySource1->GraphImpl()->AppendMessage(std::move(message1));

  UniquePtr<Message> message2 = MakeUnique<Message>(dummySource2);
  message2->mTestMonitor = &testMonitor;
  message2->mCounter = &counter;
  dummySource2->GraphImpl()->AppendMessage(std::move(message2));

  dummySource1->Destroy();
  dummySource2->Destroy();

  testMonitor.AwaitFinished();
}

TEST(TestAudioTrackGraph, SetOutputDeviceID)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  EXPECT_EQ(cubeb->CurrentStream(), nullptr)
      << "Cubeb stream has not been initialized yet";

  // Set the output device id in GetInstance method confirm that it is the one
  // used in cubeb_stream_init.
  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(2));

  // Dummy track to make graph rolling. Add it and remove it to remove the
  // graph from the global hash table and let it shutdown.
  RefPtr<SourceMediaTrack> dummySource =
      graph->CreateSourceTrack(MediaSegment::AUDIO);

  GMPTestMonitor mon;
  RefPtr<GenericPromise> p = graph->NotifyWhenDeviceStarted(dummySource);
  p->Then(GetMainThreadSerialEventTarget(), __func__,
          [&mon, cubeb, dummySource]() {
            EXPECT_EQ(cubeb->CurrentStream()->GetOutputDeviceID(),
                      reinterpret_cast<cubeb_devid>(2))
                << "After init confirm the expected output device id";
            // Test has finished, destroy the track to shutdown the MTG.
            dummySource->Destroy();
            mon.SetFinished();
          });

  mon.AwaitFinished();
}

TEST(TestAudioTrackGraph, NotifyDeviceStarted)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr);

  // Dummy track to make graph rolling. Add it and remove it to remove the
  // graph from the global hash table and let it shutdown.
  RefPtr<SourceMediaTrack> dummySource =
      graph->CreateSourceTrack(MediaSegment::AUDIO);

  RefPtr<GenericPromise> p = graph->NotifyWhenDeviceStarted(dummySource);

  GMPTestMonitor mon;
  p->Then(GetMainThreadSerialEventTarget(), __func__, [&mon, dummySource]() {
    {
      MediaTrackGraphImpl* graph = dummySource->GraphImpl();
      MonitorAutoLock lock(graph->GetMonitor());
      EXPECT_TRUE(graph->CurrentDriver()->AsAudioCallbackDriver());
      EXPECT_TRUE(graph->CurrentDriver()->ThreadRunning());
    }
    // Test has finished, destroy the track to shutdown the MTG.
    dummySource->Destroy();
    mon.SetFinished();
  });

  mon.AwaitFinished();
}

TEST(TestAudioTrackGraph, ErrorStateCrash)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr);

  // Dummy track to make graph rolling. Add it and remove it to remove the
  // graph from the global hash table and let it shutdown.
  RefPtr<SourceMediaTrack> dummySource =
      graph->CreateSourceTrack(MediaSegment::AUDIO);

  RefPtr<GenericPromise> p = graph->NotifyWhenDeviceStarted(dummySource);

  GMPTestMonitor mon;

  p->Then(GetMainThreadSerialEventTarget(), __func__,
          [&mon, dummySource, cubeb]() {
            cubeb->CurrentStream()->ForceError();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            // Test has finished, destroy the track to shutdown the MTG.
            dummySource->Destroy();
            mon.SetFinished();
          });

  mon.AwaitFinished();
}

TEST(TestAudioTrackGraph, SourceTrack)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaTrackGraph* primary = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr);

  RefPtr<SourceMediaTrack> sourceTrack =
      primary->CreateSourceTrack(MediaSegment::AUDIO);

  RefPtr<ProcessedMediaTrack> outputTrack =
      primary->CreateForwardedInputTrack(MediaSegment::AUDIO);

  /* How the source track connects to another ProcessedMediaTrack.
   * Check in MediaManager how it is connected to AudioStreamTrack. */
  RefPtr<MediaInputPort> port = outputTrack->AllocateInputPort(sourceTrack);
  outputTrack->AddAudioOutput(reinterpret_cast<void*>(1));

  {
    // Wait for the output-only stream to be created.
    bool done = false;
    MediaEventListener onStreamInit = cubeb->StreamInitEvent().Connect(
        AbstractThread::GetCurrent(), [&] { done = true; });
    SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
        [&] { return done; });
    onStreamInit.Disconnect();
  }

  /* Primary graph: Open Audio Input through SourceMediaTrack */
  RefPtr<AudioInputProcessing> listener =
      new AudioInputProcessing(2, sourceTrack, PRINCIPAL_HANDLE_NONE);
  listener->SetPassThrough(true);

  RefPtr<AudioInputProcessingPullListener> pullListener =
      new AudioInputProcessingPullListener(listener);

  sourceTrack->AddListener(pullListener);

  class StartStopListener : public ControlMessage {
   public:
    enum StartStop { Start, Stop };

    StartStopListener(AudioInputProcessing* aInputProcessing, StartStop aAction)
        : ControlMessage(nullptr),
          mInputProcessing(aInputProcessing),
          mAction(aAction) {}

    void Run() override {
      if (mAction == StartStopListener::Start) {
        mInputProcessing->Start();
      } else if (mAction == StartStopListener::Stop) {
        mInputProcessing->Stop();
      } else {
        MOZ_CRASH("Invalid enum value");
      }
    }

   protected:
    RefPtr<AudioInputProcessing> mInputProcessing;
    StartStop mAction;
  };

  sourceTrack->GraphImpl()->AppendMessage(
      MakeUnique<StartStopListener>(listener, StartStopListener::Start));
  sourceTrack->SetPullingEnabled(true);
  // Device id does not matter. Ignore.
  sourceTrack->OpenAudioInput((void*)1, listener);

  MockCubebStream* stream = nullptr;
  {
    // Wait for the full-duplex stream to be created.
    MediaEventListener onStreamInit = cubeb->StreamInitEvent().Connect(
        AbstractThread::GetCurrent(),
        [&](MockCubebStream* aStream) { stream = aStream; });
    SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
        [&] { return !!stream; });
    onStreamInit.Disconnect();
  }

  {
    // Wait for a second worth of audio data.
    uint32_t totalFrames = 0;
    MediaEventListener onFrames = stream->FramesProcessedEvent().Connect(
        AbstractThread::GetCurrent(),
        [&](uint32_t aFrames) { totalFrames += aFrames; });
    stream->VerifyOutput();
    stream->GoFaster();
    SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>([&] {
      return totalFrames > static_cast<uint32_t>(primary->GraphRate());
    });
    stream->DontGoFaster();
    onFrames.Disconnect();
  }

  {
    // Wait for the full-duplex stream to be destroyed.
    bool done = false;
    MediaEventListener onStreamDestroy = cubeb->StreamDestroyEvent().Connect(
        AbstractThread::GetCurrent(), [&] { done = true; });

    // Clean up on MainThread
    outputTrack->RemoveAudioOutput((void*)1);
    outputTrack->Destroy();
    port->Destroy();
    sourceTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartStopListener>(listener, StartStopListener::Stop));
    sourceTrack->RemoveListener(pullListener);
    sourceTrack->SetPullingEnabled(false);
    Maybe<CubebUtils::AudioDeviceID> id =
        Some(reinterpret_cast<CubebUtils::AudioDeviceID>(1));
    sourceTrack->CloseAudioInput(id);
    sourceTrack->Destroy();

    SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
        [&] { return done; });
    onStreamDestroy.Disconnect();
  }
}

TEST(TestAudioTrackGraph, CrossGraphPort)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  /* Primary graph: Create the graph a SourceMediaTrack. */
  MediaTrackGraph* primary = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE, nullptr);

  RefPtr<SourceMediaTrack> sourceTrack =
      primary->CreateSourceTrack(MediaSegment::AUDIO);

  {
    // Wait for the primary output stream to be created.
    bool done = false;
    MediaEventListener onStreamInit = cubeb->StreamInitEvent().Connect(
        AbstractThread::GetCurrent(), [&] { done = true; });
    SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
        [&] { return done; });
    onStreamInit.Disconnect();
  }

  /* Partner graph: Create graph and the CrossGraphReceiver. */
  MediaTrackGraph* partner = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr,
      MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      /*OutputDeviceID*/ reinterpret_cast<cubeb_devid>(1));

  RefPtr<CrossGraphReceiver> receiver =
      partner->CreateCrossGraphReceiver(primary->GraphRate());

  /* Primary graph: Create CrossGraphTransmitter */
  RefPtr<CrossGraphTransmitter> transmitter =
      primary->CreateCrossGraphTransmitter(receiver);

  /* How the source track connects to another ProcessedMediaTrack.
   * Check in MediaManager how it is connected to AudioStreamTrack. */
  RefPtr<MediaInputPort> port = transmitter->AllocateInputPort(sourceTrack);
  receiver->AddAudioOutput((void*)1);

  /* Primary graph: Open Audio Input through SourceMediaTrack */
  RefPtr<AudioInputProcessing> listener =
      new AudioInputProcessing(2, sourceTrack, PRINCIPAL_HANDLE_NONE);
  listener->SetPassThrough(true);

  RefPtr<AudioInputProcessingPullListener> pullListener =
      new AudioInputProcessingPullListener(listener);

  sourceTrack->AddListener(pullListener);

  class StartStopListener : public ControlMessage {
   public:
    enum StartStop { Start, Stop };

    StartStopListener(AudioInputProcessing* aInputProcessing, StartStop aAction)
        : ControlMessage(nullptr),
          mInputProcessing(aInputProcessing),
          mAction(aAction) {}

    void Run() override {
      if (mAction == StartStopListener::Start) {
        mInputProcessing->Start();
      } else if (mAction == StartStopListener::Stop) {
        mInputProcessing->Stop();
      } else {
        MOZ_CRASH("Invalid enum value");
      }
    }

   protected:
    RefPtr<AudioInputProcessing> mInputProcessing;
    StartStop mAction;
  };

  sourceTrack->GraphImpl()->AppendMessage(
      MakeUnique<StartStopListener>(listener, StartStopListener::Start));
  sourceTrack->SetPullingEnabled(true);
  // Device id does not matter ignore.
  sourceTrack->OpenAudioInput((void*)1, listener);

  MockCubebStream* stream = nullptr;
  {
    // Wait for the primary full-duplex stream to be created.
    MediaEventListener onStreamInit = cubeb->StreamInitEvent().Connect(
        AbstractThread::GetCurrent(),
        [&](MockCubebStream* aStream) { stream = aStream; });
    SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
        [&] { return !!stream; });
    onStreamInit.Disconnect();
  }

  {
    // Wait for half a second worth of audio data.
    uint32_t totalFrames = 0;
    MediaEventListener onFrames = stream->FramesProcessedEvent().Connect(
        AbstractThread::GetCurrent(),
        [&](uint32_t aFrames) { totalFrames += aFrames; });
    stream->GoFaster();
    SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>([&] {
      return totalFrames > static_cast<uint32_t>(primary->GraphRate() / 2);
    });
    stream->DontGoFaster();
    onFrames.Disconnect();
  }

  {
    // Wait for the full-duplex stream to be destroyed.
    bool done = false;
    MediaEventListener onStreamDestroy = cubeb->StreamDestroyEvent().Connect(
        AbstractThread::GetCurrent(), [&] { done = true; });

    // Clean up on MainThread
    receiver->RemoveAudioOutput((void*)1);
    receiver->Destroy();
    transmitter->Destroy();
    port->Destroy();
    sourceTrack->GraphImpl()->AppendMessage(
        MakeUnique<StartStopListener>(listener, StartStopListener::Stop));
    sourceTrack->RemoveListener(pullListener);
    sourceTrack->SetPullingEnabled(false);
    Maybe<CubebUtils::AudioDeviceID> id =
        Some(reinterpret_cast<CubebUtils::AudioDeviceID>(1));
    sourceTrack->CloseAudioInput(id);
    sourceTrack->Destroy();

    SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
        [&] { return done; });
    onStreamDestroy.Disconnect();
  }
}
