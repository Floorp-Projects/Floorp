/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZThreadUtils.h"

#include "mozilla/layers/Compositor.h"
#ifdef MOZ_WIDGET_ANDROID
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
APZThreadUtils::RunOnControllerThread(already_AddRefed<Runnable> aTask)
{
  RefPtr<Runnable> task = aTask;

#ifdef MOZ_WIDGET_ANDROID
  // This is needed while nsWindow::ConfigureAPZControllerThread is not propper
  // implemented.
  if (AndroidBridge::IsJavaUiThread()) {
    task->Run();
  } else {
    AndroidBridge::Bridge()->PostTaskToUiThread(task.forget(), 0);
  }
#else
  if (!sControllerThread) {
    // Could happen on startup
    NS_WARNING("Dropping task posted to controller thread");
    return;
  }

  if (sControllerThread == MessageLoop::current()) {
    task->Run();
  } else {
    sControllerThread->PostTask(task.forget());
  }
#endif
}

/*static*/ bool
APZThreadUtils::IsControllerThread()
{
#ifdef MOZ_WIDGET_ANDROID
  return AndroidBridge::IsJavaUiThread();
#else
  return sControllerThread == MessageLoop::current();
#endif
}

NS_IMPL_ISUPPORTS(GenericTimerCallbackBase, nsITimerCallback)

} // namespace layers
} // namespace mozilla
