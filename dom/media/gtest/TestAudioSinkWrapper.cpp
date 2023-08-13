/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSink.h"
#include "AudioSinkWrapper.h"
#include "CubebUtils.h"
#include "MockCubeb.h"
#include "TimeUnits.h"
#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/gtest/WaitFor.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"

using namespace mozilla;

// This is a crashtest to check that AudioSinkWrapper::mEndedPromiseHolder is
// not settled twice when sync and async AudioSink initializations race.
TEST(TestAudioSinkWrapper, AsyncInitFailureWithSyncInitSuccess)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaQueue<AudioData> audioQueue;
  MediaInfo info;
  info.EnableAudio();
  auto audioSinkCreator = [&]() {
    return UniquePtr<AudioSink>{new AudioSink(AbstractThread::GetCurrent(),
                                              audioQueue, info.mAudio,
                                              /*resistFingerprinting*/ false)};
  };
  const double initialVolume = 0.0;  // so that there is initially no AudioSink
  RefPtr wrapper = new AudioSinkWrapper(
      AbstractThread::GetCurrent(), audioQueue, std::move(audioSinkCreator),
      initialVolume, /*playbackRate*/ 1.0, /*preservesPitch*/ true,
      /*sinkDevice*/ nullptr);

  wrapper->Start(media::TimeUnit::Zero(), info);
  // The first AudioSink init occurs on a background thread.  Listen for this,
  // but don't process any events on the current thread so that the
  // AudioSinkWrapper does not yet handle the result of AudioSink
  // initialization.
  RefPtr backgroundQueue =
      nsThreadManager::get().CreateBackgroundTaskQueue(__func__);
  Monitor monitor(__func__);
  bool initDone = false;
  MediaEventListener initListener = cubeb->StreamInitEvent().Connect(
      backgroundQueue, [&](RefPtr<SmartMockCubebStream> aStream) {
        EXPECT_EQ(aStream, nullptr);
        MonitorAutoLock lock(monitor);
        initDone = true;
        lock.Notify();
      });
  cubeb->ForceStreamInitError();
  wrapper->SetVolume(0.5);  // triggers async sink init, which fails
  {
    // Wait for the async init to complete.
    MonitorAutoLock lock(monitor);
    while (!initDone) {
      lock.Wait();
    }
  }
  initListener.Disconnect();
  wrapper->SetPlaying(false);
  // The second AudioSink init is synchronous.
  nsIThread* currentThread = NS_GetCurrentThread();
  RefPtr<SmartMockCubebStream> stream;
  initListener = cubeb->StreamInitEvent().Connect(
      currentThread, [&](RefPtr<SmartMockCubebStream> aStream) {
        stream = std::move(aStream);
      });
  wrapper->SetPlaying(true);  // sync sink init, which succeeds
  // Let AudioSinkWrapper handle the (first) AudioSink initialization failure
  // and allow `stream` to be set.
  NS_ProcessPendingEvents(currentThread);
  initListener.Disconnect();
  cubeb_state state = CUBEB_STATE_STARTED;
  MediaEventListener stateListener = stream->StateEvent().Connect(
      currentThread, [&](cubeb_state aState) { state = aState; });
  // Run AudioSinkWrapper::OnAudioEnded().
  // This test passes if there is no crash.  Bug 1845811.
  audioQueue.Finish();
  SpinEventLoopUntil("stream state change"_ns,
                     [&] { return state != CUBEB_STATE_STARTED; });
  stateListener.Disconnect();
  EXPECT_EQ(state, CUBEB_STATE_DRAINED);
  wrapper->Stop();
  wrapper->Shutdown();
}

// This is a crashtest to check that AudioSinkWrapper::mEndedPromiseHolder is
// not settled twice when the audio ends during async AudioSink initialization.
TEST(TestAudioSinkWrapper, AsyncInitWithEndOfAudio)
{
  MockCubeb* cubeb = new MockCubeb();
  CubebUtils::ForceSetCubebContext(cubeb->AsCubebContext());

  MediaQueue<AudioData> audioQueue;
  MediaInfo info;
  info.EnableAudio();
  auto audioSinkCreator = [&]() {
    return UniquePtr<AudioSink>{new AudioSink(AbstractThread::GetCurrent(),
                                              audioQueue, info.mAudio,
                                              /*resistFingerprinting*/ false)};
  };
  const double initialVolume = 0.0;  // so that there is initially no AudioSink
  RefPtr wrapper = new AudioSinkWrapper(
      AbstractThread::GetCurrent(), audioQueue, std::move(audioSinkCreator),
      initialVolume, /*playbackRate*/ 1.0, /*preservesPitch*/ true,
      /*sinkDevice*/ nullptr);

  wrapper->Start(media::TimeUnit::Zero(), info);
  // The first AudioSink init occurs on a background thread.  Listen for this,
  // but don't process any events on the current thread so that the
  // AudioSinkWrapper does not yet use the initialized AudioSink.
  RefPtr backgroundQueue =
      nsThreadManager::get().CreateBackgroundTaskQueue(__func__);
  Monitor monitor(__func__);
  RefPtr<SmartMockCubebStream> stream;
  MediaEventListener initListener = cubeb->StreamInitEvent().Connect(
      backgroundQueue, [&](RefPtr<SmartMockCubebStream> aStream) {
        EXPECT_NE(aStream, nullptr);
        MonitorAutoLock lock(monitor);
        stream = std::move(aStream);
        lock.Notify();
      });
  wrapper->SetVolume(0.5);  // triggers async sink init
  {
    // Wait for the async init to complete.
    MonitorAutoLock lock(monitor);
    while (!stream) {
      lock.Wait();
    }
  }
  initListener.Disconnect();
  // Finish the audio before AudioSinkWrapper considers using the initialized
  // AudioSink.
  audioQueue.Finish();
  // Wait for AudioSinkWrapper to destroy the initialized stream.
  // This test passes if there is no crash.  Bug 1846854.
  WaitFor(cubeb->StreamDestroyEvent());
  wrapper->Stop();
  wrapper->Shutdown();
}
