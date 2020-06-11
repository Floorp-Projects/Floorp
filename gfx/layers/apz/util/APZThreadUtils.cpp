/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZThreadUtils.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticMutex.h"

namespace mozilla {
namespace layers {

static bool sThreadAssertionsEnabled = true;
static StaticRefPtr<nsISerialEventTarget> sControllerThread;
static StaticMutex sControllerThreadMutex;

/*static*/
void APZThreadUtils::SetThreadAssertionsEnabled(bool aEnabled) {
  StaticMutexAutoLock lock(sControllerThreadMutex);
  sThreadAssertionsEnabled = aEnabled;
}

/*static*/
bool APZThreadUtils::GetThreadAssertionsEnabled() {
  StaticMutexAutoLock lock(sControllerThreadMutex);
  return sThreadAssertionsEnabled;
}

/*static*/
void APZThreadUtils::SetControllerThread(nsISerialEventTarget* aThread) {
  // We must either be setting the initial controller thread, or removing it,
  // or re-using an existing controller thread.
  StaticMutexAutoLock lock(sControllerThreadMutex);
  MOZ_ASSERT(!sControllerThread || !aThread || sControllerThread == aThread);
  if (aThread != sControllerThread) {
    // This can only happen once, on startup.
    sControllerThread = aThread;
    ClearOnShutdown(&sControllerThread);
  }
}

/*static*/
void APZThreadUtils::AssertOnControllerThread() {
#if DEBUG
  if (!GetThreadAssertionsEnabled()) {
    return;
  }
  StaticMutexAutoLock lock(sControllerThreadMutex);
  MOZ_ASSERT(sControllerThread && sControllerThread->IsOnCurrentThread());
#endif
}

/*static*/
void APZThreadUtils::RunOnControllerThread(RefPtr<Runnable>&& aTask) {
  RefPtr<nsISerialEventTarget> thread;
  {
    StaticMutexAutoLock lock(sControllerThreadMutex);
    thread = sControllerThread;
  }
  RefPtr<Runnable> task = std::move(aTask);

  if (!thread) {
    // Could happen on startup or if Shutdown() got called.
    NS_WARNING("Dropping task posted to controller thread");
    return;
  }

  if (thread->IsOnCurrentThread()) {
    task->Run();
  } else {
    thread->Dispatch(task.forget());
  }
}

/*static*/
bool APZThreadUtils::IsControllerThread() {
  StaticMutexAutoLock lock(sControllerThreadMutex);
  return sControllerThread && sControllerThread->IsOnCurrentThread();
}

/*static*/
void APZThreadUtils::DelayedDispatch(already_AddRefed<Runnable> aRunnable,
                                     int aDelayMs) {
  MOZ_ASSERT(!XRE_IsContentProcess(),
             "ContentProcessController should only be used remotely.");
  RefPtr<nsISerialEventTarget> thread;
  {
    StaticMutexAutoLock lock(sControllerThreadMutex);
    thread = sControllerThread;
  }
  if (!thread) {
    // Could happen on startup
    NS_WARNING("Dropping task posted to controller thread");
    return;
  }
  if (aDelayMs) {
    thread->DelayedDispatch(std::move(aRunnable), aDelayMs);
  } else {
    thread->Dispatch(std::move(aRunnable));
  }
}

NS_IMPL_ISUPPORTS(GenericNamedTimerCallbackBase, nsITimerCallback, nsINamed)

}  // namespace layers
}  // namespace mozilla
