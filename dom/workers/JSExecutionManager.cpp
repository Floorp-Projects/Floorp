/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSExecutionManager.h"

#include "WorkerCommon.h"
#include "WorkerPrivate.h"

#include "mozilla/dom/DocGroup.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPtr.h"

namespace mozilla::dom {

JSExecutionManager* JSExecutionManager::mCurrentMTManager;

const uint32_t kTimeSliceExpirationMS = 50;

using RequestState = JSExecutionManager::RequestState;

static StaticRefPtr<JSExecutionManager> sSABSerializationManager;

void JSExecutionManager::Initialize() {
  if (StaticPrefs::dom_workers_serialized_sab_access()) {
    sSABSerializationManager = MakeRefPtr<JSExecutionManager>(1);
  }
}

void JSExecutionManager::Shutdown() { sSABSerializationManager = nullptr; }

JSExecutionManager* JSExecutionManager::GetSABSerializationManager() {
  return sSABSerializationManager.get();
}

RequestState JSExecutionManager::RequestJSThreadExecution() {
  if (NS_IsMainThread()) {
    return RequestJSThreadExecutionMainThread();
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();

  if (!workerPrivate || workerPrivate->GetExecutionGranted()) {
    return RequestState::ExecutingAlready;
  }

  MutexAutoLock lock(mExecutionQueueMutex);
  MOZ_ASSERT(mMaxRunning >= mRunning);

  if ((mExecutionQueue.size() + (mMainThreadAwaitingExecution ? 1 : 0)) <
      size_t(mMaxRunning - mRunning)) {
    // There's slots ready for things to run, execute right away.
    workerPrivate->SetExecutionGranted(true);
    workerPrivate->ScheduleTimeSliceExpiration(kTimeSliceExpirationMS);

    mRunning++;
    return RequestState::Granted;
  }

  mExecutionQueue.push_back(workerPrivate);

  TimeStamp waitStart = TimeStamp::Now();

  while (mRunning >= mMaxRunning || (workerPrivate != mExecutionQueue.front() ||
                                     mMainThreadAwaitingExecution)) {
    // If there is no slots available, the main thread is awaiting permission
    // or we are not first in line for execution, wait until notified.
    mExecutionQueueCondVar.Wait(TimeDuration::FromMilliseconds(500));
    if ((TimeStamp::Now() - waitStart) > TimeDuration::FromSeconds(20)) {
      // Crash so that these types of situations are actually caught in the
      // crash reporter.
      MOZ_CRASH();
    }
  }

  workerPrivate->SetExecutionGranted(true);
  workerPrivate->ScheduleTimeSliceExpiration(kTimeSliceExpirationMS);

  mExecutionQueue.pop_front();
  mRunning++;
  if (mRunning < mMaxRunning) {
    // If a thread woke up before that wasn't first in line it will have gone
    // back to sleep, if there's more slots available, wake it now.
    mExecutionQueueCondVar.NotifyAll();
  }

  return RequestState::Granted;
}

void JSExecutionManager::YieldJSThreadExecution() {
  if (NS_IsMainThread()) {
    MOZ_ASSERT(mMainThreadIsExecuting);
    mMainThreadIsExecuting = false;

    MutexAutoLock lock(mExecutionQueueMutex);
    mRunning--;
    mExecutionQueueCondVar.NotifyAll();
    return;
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();

  if (!workerPrivate) {
    return;
  }

  MOZ_ASSERT(workerPrivate->GetExecutionGranted());

  workerPrivate->CancelTimeSliceExpiration();

  MutexAutoLock lock(mExecutionQueueMutex);
  mRunning--;
  mExecutionQueueCondVar.NotifyAll();
  workerPrivate->SetExecutionGranted(false);
}

bool JSExecutionManager::YieldJSThreadExecutionIfGranted() {
  if (NS_IsMainThread()) {
    if (mMainThreadIsExecuting) {
      YieldJSThreadExecution();
      return true;
    }
    return false;
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();

  if (workerPrivate && workerPrivate->GetExecutionGranted()) {
    YieldJSThreadExecution();
    return true;
  }

  return false;
}

RequestState JSExecutionManager::RequestJSThreadExecutionMainThread() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mMainThreadIsExecuting) {
    return RequestState::ExecutingAlready;
  }

  MutexAutoLock lock(mExecutionQueueMutex);
  MOZ_ASSERT(mMaxRunning >= mRunning);

  if ((mMaxRunning - mRunning) > 0) {
    // If there's any slots available run, the main thread always takes
    // precedence over any worker threads.
    mRunning++;
    mMainThreadIsExecuting = true;
    return RequestState::Granted;
  }

  mMainThreadAwaitingExecution = true;

  TimeStamp waitStart = TimeStamp::Now();

  while (mRunning >= mMaxRunning) {
    if ((TimeStamp::Now() - waitStart) > TimeDuration::FromSeconds(20)) {
      // Crash so that these types of situations are actually caught in the
      // crash reporter.
      MOZ_CRASH();
    }
    mExecutionQueueCondVar.Wait(TimeDuration::FromMilliseconds(500));
  }

  mMainThreadAwaitingExecution = false;
  mMainThreadIsExecuting = true;

  mRunning++;
  if (mRunning < mMaxRunning) {
    // If a thread woke up before that wasn't first in line it will have gone
    // back to sleep, if there's more slots available, wake it now.
    mExecutionQueueCondVar.NotifyAll();
  }

  return RequestState::Granted;
}

AutoRequestJSThreadExecution::AutoRequestJSThreadExecution(
    nsIGlobalObject* aGlobalObject, bool aIsMainThread) {
  JSExecutionManager* manager = nullptr;

  mIsMainThread = aIsMainThread;
  if (mIsMainThread) {
    mOldGrantingManager = JSExecutionManager::mCurrentMTManager;

    nsPIDOMWindowInner* innerWindow = nullptr;
    if (aGlobalObject) {
      innerWindow = aGlobalObject->GetAsInnerWindow();
    }

    DocGroup* docGroup = nullptr;
    if (innerWindow) {
      docGroup = innerWindow->GetDocGroup();
    }

    if (docGroup) {
      manager = docGroup->GetExecutionManager();
    }

    if (JSExecutionManager::mCurrentMTManager == manager) {
      return;
    }

    if (JSExecutionManager::mCurrentMTManager) {
      JSExecutionManager::mCurrentMTManager->YieldJSThreadExecution();
      JSExecutionManager::mCurrentMTManager = nullptr;
    }
  } else {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();

    if (workerPrivate) {
      manager = workerPrivate->GetExecutionManager();
    }
  }

  if (manager &&
      (manager->RequestJSThreadExecution() == RequestState::Granted)) {
    if (NS_IsMainThread()) {
      // Make sure we restore permission on destruction if needed.
      JSExecutionManager::mCurrentMTManager = manager;
    }
    mExecutionGrantingManager = std::move(manager);
  }
}

AutoYieldJSThreadExecution::AutoYieldJSThreadExecution() {
  JSExecutionManager* manager = nullptr;

  if (NS_IsMainThread()) {
    manager = JSExecutionManager::mCurrentMTManager;
  } else {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();

    if (workerPrivate) {
      manager = workerPrivate->GetExecutionManager();
    }
  }

  if (manager && manager->YieldJSThreadExecutionIfGranted()) {
    mExecutionGrantingManager = std::move(manager);
    if (NS_IsMainThread()) {
      JSExecutionManager::mCurrentMTManager = nullptr;
    }
  }
}

}  // namespace mozilla::dom
