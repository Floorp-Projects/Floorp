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
    RefPtr<Runnable> deleteRunnable = NS_NewRunnableFunction(__func__, [this] {
      MOZ_RELEASE_ASSERT(!mTaskQueue->IsOnCurrentThread(),
                         "Cannot AwaitIdle() on ourselves. Will deadlock.");
      // Must block this thread until shutdown is complete.
      {
        auto hasShutdown = mHasShutdown.Lock();
        *hasShutdown = true;
        mTaskQueue->BeginShutdown();
      }
      mTaskQueue->AwaitIdle();
      delete this;
    });

    if (mTaskQueue->IsOnCurrentThread() || NS_IsMainThread()) {
      // TaskQueue::AwaitIdle on itself will cause a deadlock, and we don't want
      // to block on main thread. Dispatching to a background thread if
      // possible.
      if (NS_SUCCEEDED(NS_DispatchBackgroundTask(
              deleteRunnable, NS_DISPATCH_EVENT_MAY_BLOCK))) {
        return;
      }
    }

    if (mTaskQueue->IsOnCurrentThread()) {
      // Dispatching to a background thread failed. We must be in shutdown. Try
      // to dispatch to main. This should work as mTaskQueue should block
      // xpcom-shutdown-threads.
      if (NS_SUCCEEDED(NS_DispatchToMainThread(deleteRunnable))) {
        return;
      }
    }

    // We are either on a suitable thread or everything else has failed.
    deleteRunnable->Run();
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
  DataMutex<bool> mHasShutdown{false, "TaskQueueWrapper::mHasShutdown"};
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

  UniquePtr<TaskQueueWrapper> CreateTaskQueueWrapper(absl::string_view aName,
                                                     bool aSupportTailDispatch,
                                                     Priority aPriority) const {
    // XXX Do something with aPriority
    nsCString name(aName.data(), aName.size());
    auto taskQueue = MakeRefPtr<TaskQueue>(
        GetMediaThreadPool(MediaThreadType::WEBRTC_DECODER), name.get(),
        aSupportTailDispatch);
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
