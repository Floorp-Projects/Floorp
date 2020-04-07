/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "GraphRunner.h"

#include "GraphDriver.h"
#include "MediaTrackGraph.h"
#include "MediaTrackGraphImpl.h"
#include "mozilla/dom/WorkletThread.h"
#include "nsISupportsImpl.h"
#include "prthread.h"
#include "Tracing.h"
#include "audio_thread_priority.h"

namespace mozilla {

GraphRunner::GraphRunner(MediaTrackGraphImpl* aGraph,
                         already_AddRefed<nsIThread> aThread)
    : Runnable("GraphRunner"),
      mMonitor("GraphRunner::mMonitor"),
      mGraph(aGraph),
      mThreadState(ThreadState::Wait),
      mThread(aThread) {
  mThread->Dispatch(do_AddRef(this));
}

GraphRunner::~GraphRunner() {
  MOZ_ASSERT(mThreadState == ThreadState::Shutdown);
}

/* static */
already_AddRefed<GraphRunner> GraphRunner::Create(MediaTrackGraphImpl* aGraph) {
  nsCOMPtr<nsIThread> thread;
  if (NS_WARN_IF(NS_FAILED(
          NS_NewNamedThread("GraphRunner", getter_AddRefs(thread))))) {
    return nullptr;
  }
  nsCOMPtr<nsISupportsPriority> supportsPriority = do_QueryInterface(thread);
  MOZ_ASSERT(supportsPriority);
  MOZ_ALWAYS_SUCCEEDS(
      supportsPriority->SetPriority(nsISupportsPriority::PRIORITY_HIGHEST));

  return do_AddRef(new GraphRunner(aGraph, thread.forget()));
}

void GraphRunner::Shutdown() {
  {
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(mThreadState == ThreadState::Wait);
    mThreadState = ThreadState::Shutdown;
    mMonitor.Notify();
  }
  mThread->Shutdown();
}

auto GraphRunner::OneIteration(GraphTime aStateEnd, GraphTime aIterationEnd,
                               AudioMixer* aMixer) -> IterationResult {
  TRACE();

  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mThreadState == ThreadState::Wait);
  mIterationState = Some(IterationState(aStateEnd, aIterationEnd, aMixer));

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
  // Signal that mIterationState was updated
  mThreadState = ThreadState::Run;
  mMonitor.Notify();
  // Wait for mIterationResult to update
  do {
    mMonitor.Wait();
  } while (mThreadState == ThreadState::Run);

#ifdef DEBUG
  mAudioDriverThreadId = std::thread::id();
  mClockDriverThread = nullptr;
#endif

  mIterationState = Nothing();

  IterationResult result = std::move(mIterationResult);
  mIterationResult = IterationResult();
  return result;
}

NS_IMETHODIMP GraphRunner::Run() {
  atp_handle* handle =
      atp_promote_current_thread_to_real_time(0, mGraph->GraphRate());

  nsCOMPtr<nsIThreadInternal> threadInternal = do_QueryInterface(mThread);
  threadInternal->SetObserver(mGraph);

  MonitorAutoLock lock(mMonitor);
  while (true) {
    while (mThreadState == ThreadState::Wait) {
      mMonitor.Wait();  // Wait for mIterationState to update or for shutdown
    }
    if (mThreadState == ThreadState::Shutdown) {
      break;
    }
    MOZ_DIAGNOSTIC_ASSERT(mIterationState.isSome());
    TRACE();
    mIterationResult = mGraph->OneIterationImpl(mIterationState->StateEnd(),
                                                mIterationState->IterationEnd(),
                                                mIterationState->Mixer());
    // Signal that mIterationResult was updated
    mThreadState = ThreadState::Wait;
    mMonitor.Notify();
  }

  if (handle) {
    atp_demote_current_thread_from_real_time(handle);
  }

  dom::WorkletThread::DeleteCycleCollectedJSContext();

  return NS_OK;
}

bool GraphRunner::OnThread() {
  return mThread->EventTarget()->IsOnCurrentThread();
}

#ifdef DEBUG
bool GraphRunner::InDriverIteration(GraphDriver* aDriver) {
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
