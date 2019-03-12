/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "GraphRunner.h"

#include "GraphDriver.h"
#include "MediaStreamGraph.h"
#include "MediaStreamGraphImpl.h"
#include "nsISupportsImpl.h"
#include "prthread.h"
#include "Tracing.h"

namespace mozilla {

static void Start(void* aArg) {
  GraphRunner* th = static_cast<GraphRunner*>(aArg);
  th->Run();
}

GraphRunner::GraphRunner(MediaStreamGraphImpl* aGraph)
    : mMonitor("GraphRunner::mMonitor"),
      mGraph(aGraph),
      mStateEnd(0),
      mStillProcessing(true),
      mShutdown(false),
      mStarted(false),
      // Note that mThread needs to be initialized last, as it may pre-empt the
      // thread running this ctor and enter Run() with uninitialized members.
      mThread(PR_CreateThread(PR_SYSTEM_THREAD, &Start, this,
                              PR_PRIORITY_URGENT, PR_GLOBAL_THREAD,
                              PR_JOINABLE_THREAD, 0)) {
  MOZ_COUNT_CTOR(GraphRunner);
}

GraphRunner::~GraphRunner() {
  MOZ_COUNT_DTOR(GraphRunner);
#ifdef DEBUG
  {
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(mShutdown);
  }
#endif
  PR_JoinThread(mThread);
}

void GraphRunner::Shutdown() {
  MonitorAutoLock lock(mMonitor);
  mShutdown = true;
  mMonitor.Notify();
}

bool GraphRunner::OneIteration(GraphTime aStateEnd) {
  TRACE_AUDIO_CALLBACK();

  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(!mShutdown);
  mStateEnd = aStateEnd;

  if (!mStarted) {
    mMonitor.Wait();
    MOZ_ASSERT(mStarted);
  }

#ifdef DEBUG
  if (auto audioDriver = mGraph->CurrentDriver()->AsAudioCallbackDriver()) {
    mAudioDriverThreadId = audioDriver->ThreadId();
  } else if (auto clockDriver =
                 mGraph->CurrentDriver()->AsSystemClockDriver()) {
    mClockDriverThread = clockDriver->Thread();
  } else {
    MOZ_CRASH("Unknown GraphDriver");
  }
#endif

  mMonitor.Notify();  // Signal that mStateEnd was updated
  mMonitor.Wait();    // Wait for mStillProcessing to update

#ifdef DEBUG
  mAudioDriverThreadId = std::thread::id();
  mClockDriverThread = nullptr;
#endif

  return mStillProcessing;
}

void GraphRunner::Run() {
  PR_SetCurrentThreadName("GraphRunner");
  MonitorAutoLock lock(mMonitor);
  mStarted = true;
  mMonitor.Notify();  // Signal that mStarted was set, in case the audio
                      // callback is already waiting for us
  while (true) {
    mMonitor.Wait();  // Wait for mStateEnd or mShutdown to update
    if (mShutdown) {
      break;
    }
    TRACE();
    mStillProcessing = mGraph->OneIterationImpl(mStateEnd);
    mMonitor.Notify();  // Signal that mStillProcessing was updated
  }
}

bool GraphRunner::OnThread() { return PR_GetCurrentThread() == mThread; }

#ifdef DEBUG
bool GraphRunner::RunByGraphDriver(GraphDriver* aDriver) {
  if (!OnThread()) {
    return false;
  }

  if (auto audioDriver = aDriver->AsAudioCallbackDriver()) {
    return audioDriver->ThreadId() == mAudioDriverThreadId;
  }

  if (auto clockDriver = aDriver->AsSystemClockDriver()) {
    return clockDriver->Thread() == mClockDriverThread;
  }

  MOZ_CRASH("Unknown driver");
}
#endif

}  // namespace mozilla
