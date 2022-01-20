/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_TASKQUEUEWRAPPER_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_TASKQUEUEWRAPPER_H_

#include "api/task_queue/task_queue_factory.h"
#include "mozilla/DataMutex.h"
#include "mozilla/TaskQueue.h"
#include "VideoUtils.h"

namespace mozilla {

/**
 * A wrapper around Mozilla TaskQueues in the shape of a libwebrtc TaskQueue.
 *
 * Allows libwebrtc to use Mozilla threads where tooling, e.g. profiling, is set
 * up and just works.
 *
 * Mozilla APIs like Runnables, MozPromise, etc. can also be used with the
 * wrapped TaskQueue to run things on the right thread when interacting with
 * libwebrtc.
 */
class TaskQueueWrapper : public webrtc::TaskQueueBase {
 public:
  explicit TaskQueueWrapper(RefPtr<TaskQueue> aTaskQueue)
      : mTaskQueue(std::move(aTaskQueue)) {}
  ~TaskQueueWrapper() = default;

  void Delete() override {
    MOZ_ASSERT(!mTaskQueue->IsOnCurrentThread(),
               "TaskQueue::AwaitIdle must not be called on itself");
    // Must block this thread until shutdown is complete.
    {
      auto hasShutdown = mHasShutdown.Lock();
      *hasShutdown = true;
      mTaskQueue->BeginShutdown();
    }
    mTaskQueue->AwaitIdle();
    delete this;
  }

  already_AddRefed<Runnable> CreateTaskRunner(
      std::unique_ptr<webrtc::QueuedTask> aTask) {
    return NS_NewRunnableFunction("TaskQueueWrapper::CreateTaskRunner",
                                  [this, task = std::move(aTask)]() mutable {
                                    CurrentTaskQueueSetter current(this);
                                    auto hasShutdown = mHasShutdown.Lock();
                                    if (*hasShutdown) {
                                      return;
                                    }
                                    bool toDelete = task->Run();
                                    if (!toDelete) {
                                      task.release();
                                    }
                                  });
  }

  already_AddRefed<Runnable> CreateTaskRunner(nsCOMPtr<nsIRunnable> aRunnable) {
    return NS_NewRunnableFunction(
        "TaskQueueWrapper::CreateTaskRunner",
        [this, runnable = std::move(aRunnable)]() mutable {
          CurrentTaskQueueSetter current(this);
          auto hasShutdown = mHasShutdown.Lock();
          if (*hasShutdown) {
            return;
          }
          runnable->Run();
        });
  }

  void PostTask(std::unique_ptr<webrtc::QueuedTask> aTask) override {
    MOZ_ALWAYS_SUCCEEDS(
        mTaskQueue->Dispatch(CreateTaskRunner(std::move(aTask))));
  }

  void PostDelayedTask(std::unique_ptr<webrtc::QueuedTask> aTask,
                       uint32_t aMilliseconds) override {
    if (aMilliseconds == 0) {
      // AbstractThread::DelayedDispatch doesn't support delay 0
      PostTask(std::move(aTask));
      return;
    }
    MOZ_ALWAYS_SUCCEEDS(mTaskQueue->DelayedDispatch(
        CreateTaskRunner(std::move(aTask)), aMilliseconds));
  }

  const RefPtr<TaskQueue> mTaskQueue;

  // This is a recursive mutex because a TaskRunner holding this mutex while
  // running its runnable may end up running other - tail dispatched - runnables
  // too, and they'll again try to grab the mutex.
  // The mutex must be held while running the runnable since otherwise there'd
  // be a race between shutting down the underlying task queue and the runnable
  // dispatching to that task queue (and we assert it succeeds in e.g.,
  // PostTask()).
  DataMutexBase<bool, RecursiveMutex> mHasShutdown{
      false, "TaskQueueWrapper::mHasShutdown"};
};

template <>
class DefaultDelete<TaskQueueWrapper> : public webrtc::TaskQueueDeleter {
 public:
  void operator()(TaskQueueWrapper* aPtr) const {
    webrtc::TaskQueueDeleter::operator()(aPtr);
  }
};

class SharedThreadPoolWebRtcTaskQueueFactory : public webrtc::TaskQueueFactory {
 public:
  SharedThreadPoolWebRtcTaskQueueFactory() {}

  UniquePtr<TaskQueueWrapper> CreateTaskQueueWrapper(
      absl::string_view aName, bool aSupportTailDispatch, Priority aPriority,
      MediaThreadType aThreadType = MediaThreadType::WEBRTC_WORKER) const {
    // XXX Do something with aPriority
    nsCString name(aName.data(), aName.size());
    auto taskQueue = MakeRefPtr<TaskQueue>(GetMediaThreadPool(aThreadType),
                                           name.get(), aSupportTailDispatch);
    return MakeUnique<TaskQueueWrapper>(std::move(taskQueue));
  }

  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view aName, Priority aPriority) const override {
    // libwebrtc will dispatch some tasks sync, i.e., block the origin thread
    // until they've run, and that doesn't play nice with tail dispatching since
    // there will never be a tail.
    constexpr bool supportTailDispatch = false;
    return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
        CreateTaskQueueWrapper(std::move(aName), supportTailDispatch, aPriority)
            .release(),
        webrtc::TaskQueueDeleter());
  }
};

}  // namespace mozilla

#endif
