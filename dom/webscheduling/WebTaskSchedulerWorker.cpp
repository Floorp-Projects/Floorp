/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTaskSchedulerWorker.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/TimeoutManager.h"

namespace mozilla::dom {

WebTaskWorkerRunnable::WebTaskWorkerRunnable(
    WorkerPrivate* aWorkerPrivate, WebTaskSchedulerWorker* aSchedulerWorker)
    : WorkerSameThreadRunnable(aWorkerPrivate, "WebTaskWorkerRunnable"),
      mSchedulerWorker(aSchedulerWorker) {
  MOZ_ASSERT(mSchedulerWorker);
}

WebTaskSchedulerWorker::WebTaskSchedulerWorker(WorkerPrivate* aWorkerPrivate)
    : WebTaskScheduler(aWorkerPrivate->GlobalScope()),
      mWorkerPrivate(aWorkerPrivate) {}

bool WebTaskWorkerRunnable::WorkerRun(JSContext* aCx,
                                      WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();

  if (mSchedulerWorker) {
    RefPtr<WebTask> task = mSchedulerWorker->GetNextTask();
    if (task) {
      task->Run();
    }
  }
  return true;
}

nsresult WebTaskSchedulerWorker::SetTimeoutForDelayedTask(WebTask* aTask,
                                                          uint64_t aDelay) {
  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    return NS_ERROR_UNEXPECTED;
  }
  RefPtr<DelayedWebTaskHandler> handler =
      new DelayedWebTaskHandler(cx, this, aTask);
  ErrorResult rv;

  int32_t delay = aDelay > INT32_MAX ? INT32_MAX : (int32_t)aDelay;
  mWorkerPrivate->SetTimeout(cx, handler, delay,
                             /* aIsInterval */ false,
                             Timeout::Reason::eDelayedWebTaskTimeout, rv);
  return rv.StealNSResult();
}

bool WebTaskSchedulerWorker::DispatchEventLoopRunnable() {
  if (!mWorkerPrivate) {
    return false;
  }
  RefPtr<WebTaskWorkerRunnable> runnable =
      new WebTaskWorkerRunnable(mWorkerPrivate, this);
  return runnable->Dispatch();
}

void WebTaskSchedulerWorker::Disconnect() {
  if (mWorkerPrivate) {
    mWorkerPrivate = nullptr;
  }
  WebTaskScheduler::Disconnect();
}
}  // namespace mozilla::dom
