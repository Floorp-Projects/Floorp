/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TaskController.h"
#include "mozilla/dom/TimeoutManager.h"

#include "nsContentUtils.h"
#include "WebTaskSchedulerMainThread.h"

namespace mozilla::dom {

NS_IMETHODIMP WebTaskMainThreadRunnable::Run() {
  if (mScheduler) {
    RefPtr<WebTask> task = mScheduler->GetNextTask();
    if (task) {
      task->Run();
    }
  }
  return NS_OK;
}

nsresult WebTaskSchedulerMainThread::SetTimeoutForDelayedTask(WebTask* aTask,
                                                              uint64_t aDelay) {
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    return NS_ERROR_UNEXPECTED;
  }
  nsIGlobalObject* global = GetParentObject();
  MOZ_ASSERT(global);

  RefPtr<DelayedWebTaskHandler> handler =
      new DelayedWebTaskHandler(cx, this, aTask);

  int32_t delay = aDelay > INT32_MAX ? INT32_MAX : (int32_t)aDelay;

  int32_t handle;
  return global->GetAsInnerWindow()->TimeoutManager().SetTimeout(
      handler, delay, /* aIsInterval */ false,
      Timeout::Reason::eDelayedWebTaskTimeout, &handle);
}

bool WebTaskSchedulerMainThread::DispatchEventLoopRunnable() {
  RefPtr<WebTaskMainThreadRunnable> runnable =
      new WebTaskMainThreadRunnable(this);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThreadQueue(runnable.forget(),
                                                   EventQueuePriority::Normal));
  return true;
}
}  // namespace mozilla::dom
