/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZThreadUtils.h"

namespace mozilla {
namespace layers {

static bool sThreadAssertionsEnabled = true;
static nsISerialEventTarget* sControllerThread;

/*static*/
void APZThreadUtils::SetThreadAssertionsEnabled(bool aEnabled) {
  sThreadAssertionsEnabled = aEnabled;
}

/*static*/
bool APZThreadUtils::GetThreadAssertionsEnabled() {
  return sThreadAssertionsEnabled;
}

/*static*/
void APZThreadUtils::SetControllerThread(nsISerialEventTarget* aThread) {
  // We must either be setting the initial controller thread, or removing it,
  // or re-using an existing controller thread.
  MOZ_ASSERT(!sControllerThread || !aThread || sControllerThread == aThread);
  sControllerThread = aThread;
}

/*static*/
void APZThreadUtils::AssertOnControllerThread() {
  if (!GetThreadAssertionsEnabled()) {
    return;
  }

  MOZ_ASSERT(sControllerThread->IsOnCurrentThread());
}

/*static*/
void APZThreadUtils::RunOnControllerThread(RefPtr<Runnable>&& aTask) {
  RefPtr<Runnable> task = std::move(aTask);

  if (!sControllerThread) {
    // Could happen on startup
    NS_WARNING("Dropping task posted to controller thread");
    return;
  }

  if (sControllerThread->IsOnCurrentThread()) {
    task->Run();
  } else {
    sControllerThread->Dispatch(task.forget());
  }
}

/*static*/
bool APZThreadUtils::IsControllerThread() {
  return sControllerThread == NS_GetCurrentThread();
}

/*static*/
void APZThreadUtils::DelayedDispatch(already_AddRefed<Runnable> aRunnable,
                                     int aDelayMs) {
  MOZ_ASSERT(sControllerThread && sControllerThread->IsOnCurrentThread());
  MOZ_ASSERT(!XRE_IsContentProcess(),
             "ContentProcessController should only be used remotely.");
  if (aDelayMs) {
    sControllerThread->DelayedDispatch(std::move(aRunnable), aDelayMs);
  } else {
    sControllerThread->Dispatch(std::move(aRunnable));
  }
}

NS_IMPL_ISUPPORTS(GenericNamedTimerCallbackBase, nsITimerCallback, nsINamed)

}  // namespace layers
}  // namespace mozilla
