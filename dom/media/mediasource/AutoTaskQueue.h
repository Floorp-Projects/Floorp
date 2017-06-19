/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUTOTASKQUEUE_H_
#define MOZILLA_AUTOTASKQUEUE_H_

#include "mozilla/RefPtr.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/TaskQueue.h"

namespace mozilla {

// A convenience TaskQueue not requiring explicit shutdown.
class AutoTaskQueue : public AbstractThread
{
public:
  explicit AutoTaskQueue(already_AddRefed<SharedThreadPool> aPool,
                         bool aSupportsTailDispatch = false)
  : AbstractThread(aSupportsTailDispatch)
  , mTaskQueue(new TaskQueue(Move(aPool), aSupportsTailDispatch))
  {}

  AutoTaskQueue(already_AddRefed<SharedThreadPool> aPool,
                const char* aName,
                bool aSupportsTailDispatch = false)
  : AbstractThread(aSupportsTailDispatch)
  , mTaskQueue(new TaskQueue(Move(aPool), aName, aSupportsTailDispatch))
  {}

  TaskDispatcher& TailDispatcher() override
  {
    return mTaskQueue->TailDispatcher();
  }

  void Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                DispatchFailureHandling aFailureHandling = AssertDispatchSuccess,
                DispatchReason aReason = NormalDispatch) override
  {
    mTaskQueue->Dispatch(Move(aRunnable), aFailureHandling, aReason);
  }

  // Prevent a GCC warning about the other overload of Dispatch being hidden.
  using AbstractThread::Dispatch;

  // Blocks until all tasks finish executing.
  void AwaitIdle() { mTaskQueue->AwaitIdle(); }

  bool IsEmpty() { return mTaskQueue->IsEmpty(); }

  // Returns true if the current thread is currently running a Runnable in
  // the task queue.
  bool IsCurrentThreadIn() override { return mTaskQueue->IsCurrentThreadIn(); }

private:
  ~AutoTaskQueue()
  {
    RefPtr<TaskQueue> taskqueue = mTaskQueue;
    nsCOMPtr<nsIRunnable> task =
      NS_NewRunnableFunction([taskqueue]() { taskqueue->BeginShutdown(); });
    SystemGroup::Dispatch("~AutoTaskQueue", TaskCategory::Other, task.forget());
  }
  RefPtr<TaskQueue> mTaskQueue;
};

} // namespace mozilla

#endif
