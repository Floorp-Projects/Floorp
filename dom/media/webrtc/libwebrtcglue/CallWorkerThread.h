/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_CALLWORKERTHREAD_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_CALLWORKERTHREAD_H_

#include "mozilla/AbstractThread.h"
#include "nsIDirectTaskDispatcher.h"
#include "TaskQueueWrapper.h"

namespace mozilla {

// Implements AbstractThread for running things on the webrtc TaskQueue.
// Webrtc TaskQueues are not refcounted so cannot implement AbstractThread
// directly.
class CallWorkerThread final : public AbstractThread,
                               public nsIDirectTaskDispatcher {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDIRECTTASKDISPATCHER

  explicit CallWorkerThread(UniquePtr<TaskQueueWrapper> aWebrtcTaskQueue)
      : AbstractThread(aWebrtcTaskQueue->mTaskQueue->SupportsTailDispatch()),
        mWebrtcTaskQueue(std::move(aWebrtcTaskQueue)) {}

  // AbstractThread overrides
  nsresult Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                    DispatchReason aReason) override;
  bool IsCurrentThreadIn() const override;
  TaskDispatcher& TailDispatcher() override;
  nsIEventTarget* AsEventTarget() override;
  NS_IMETHOD
  DelayedDispatch(already_AddRefed<nsIRunnable> aEvent,
                  uint32_t aDelayMs) override;

  const UniquePtr<TaskQueueWrapper> mWebrtcTaskQueue;

 protected:
  ~CallWorkerThread() = default;
};

NS_IMPL_ISUPPORTS_INHERITED(CallWorkerThread, AbstractThread,
                            nsIDirectTaskDispatcher);

//-----------------------------------------------------------------------------
// AbstractThread
//-----------------------------------------------------------------------------

nsresult CallWorkerThread::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                                    DispatchReason aReason) {
  RefPtr<nsIRunnable> runnable = aRunnable;
  return mWebrtcTaskQueue->mTaskQueue->Dispatch(
      mWebrtcTaskQueue->CreateTaskRunner(std::move(runnable)), aReason);
}

bool CallWorkerThread::IsCurrentThreadIn() const {
  return mWebrtcTaskQueue->mTaskQueue->IsOnCurrentThreadInfallible() &&
         mWebrtcTaskQueue->IsCurrent();
}

TaskDispatcher& CallWorkerThread::TailDispatcher() {
  return mWebrtcTaskQueue->mTaskQueue->TailDispatcher();
}

nsIEventTarget* CallWorkerThread::AsEventTarget() {
  return mWebrtcTaskQueue->mTaskQueue->AsEventTarget();
}

NS_IMETHODIMP
CallWorkerThread::DelayedDispatch(already_AddRefed<nsIRunnable> aEvent,
                                  uint32_t aDelayMs) {
  RefPtr<nsIRunnable> event = aEvent;
  return mWebrtcTaskQueue->mTaskQueue->DelayedDispatch(
      mWebrtcTaskQueue->CreateTaskRunner(std::move(event)), aDelayMs);
}

//-----------------------------------------------------------------------------
// nsIDirectTaskDispatcher
//-----------------------------------------------------------------------------

NS_IMETHODIMP
CallWorkerThread::DispatchDirectTask(already_AddRefed<nsIRunnable> aEvent) {
  nsCOMPtr<nsIRunnable> event = aEvent;
  return mWebrtcTaskQueue->mTaskQueue->DispatchDirectTask(
      mWebrtcTaskQueue->CreateTaskRunner(std::move(event)));
}

NS_IMETHODIMP CallWorkerThread::DrainDirectTasks() {
  return mWebrtcTaskQueue->mTaskQueue->DrainDirectTasks();
}

NS_IMETHODIMP CallWorkerThread::HaveDirectTasks(bool* aValue) {
  return mWebrtcTaskQueue->mTaskQueue->HaveDirectTasks(aValue);
}

}  // namespace mozilla

#endif
