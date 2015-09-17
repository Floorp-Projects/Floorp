/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZThreadUtils.h"

#include "mozilla/layers/Compositor.h"
#ifdef MOZ_ANDROID_APZ
#include "AndroidBridge.h"
#endif

namespace mozilla {
namespace layers {

static bool sThreadAssertionsEnabled = true;
static MessageLoop* sControllerThread;

/*static*/ void
APZThreadUtils::SetThreadAssertionsEnabled(bool aEnabled) {
  sThreadAssertionsEnabled = aEnabled;
}

/*static*/ bool
APZThreadUtils::GetThreadAssertionsEnabled() {
  return sThreadAssertionsEnabled;
}

/*static*/ void
APZThreadUtils::SetControllerThread(MessageLoop* aLoop)
{
  // We must either be setting the initial controller thread, or removing it,
  // or re-using an existing controller thread.
  MOZ_ASSERT(!sControllerThread || !aLoop || sControllerThread == aLoop);
  sControllerThread = aLoop;
}

/*static*/ void
APZThreadUtils::AssertOnControllerThread() {
  if (!GetThreadAssertionsEnabled()) {
    return;
  }

  MOZ_ASSERT(sControllerThread == MessageLoop::current());
}

/*static*/ void
APZThreadUtils::AssertOnCompositorThread()
{
  if (GetThreadAssertionsEnabled()) {
    Compositor::AssertOnCompositorThread();
  }
}

/*static*/ void
APZThreadUtils::RunOnControllerThread(Task* aTask)
{
#ifdef MOZ_ANDROID_APZ
  // This is needed while nsWindow::ConfigureAPZControllerThread is not propper
  // implemented.
  if (AndroidBridge::IsJavaUiThread()) {
    aTask->Run();
    delete aTask;
  } else {
    AndroidBridge::Bridge()->PostTaskToUiThread(aTask, 0);
  }
#else
  if (!sControllerThread) {
    // Could happen on startup
    NS_WARNING("Dropping task posted to controller thread");
    delete aTask;
    return;
  }

  if (sControllerThread == MessageLoop::current()) {
    aTask->Run();
    delete aTask;
  } else {
    sControllerThread->PostTask(FROM_HERE, aTask);
  }
#endif
}

NS_IMPL_ISUPPORTS(GenericTimerCallbackBase, nsITimerCallback)

} // namespace layers
} // namespace mozilla
