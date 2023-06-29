/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_TASKQUEUEWRAPPER_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_TASKQUEUEWRAPPER_H_

#include "api/task_queue/task_queue_factory.h"
#include "mozilla/DataMutex.h"
#include "mozilla/RecursiveMutex.h"
#include "mozilla/ProfilerRunnable.h"
#include "mozilla/TaskQueue.h"
#include "VideoUtils.h"
#include "mozilla/media/MediaUtils.h"  // For media::Await

namespace mozilla {

enum class DeletionPolicy : uint8_t { Blocking, NonBlocking };

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
template <DeletionPolicy Deletion>
class TaskQueueWrapper : public webrtc::TaskQueueBase {
 public:
  TaskQueueWrapper(RefPtr<TaskQueue> aTaskQueue, nsCString aName)
      : mTaskQueue(std::move(aTaskQueue)), mName(std::move(aName)) {}
  ~TaskQueueWrapper() = default;

  void Delete() override {
    {
      // Scope this to make sure it does not race against the promise chain we
      // set up below.
      auto hasShutdown = mHasShutdown.Lock();
      *hasShutdown = true;
    }

    MOZ_RELEASE_ASSERT(Deletion == DeletionPolicy::NonBlocking ||
                       !mTaskQueue->IsOnCurrentThread());

    nsCOMPtr<nsISerialEventTarget> backgroundTaskQueue;
    NS_CreateBackgroundTaskQueue(__func__, getter_AddRefs(backgroundTaskQueue));
    if (NS_WARN_IF(!backgroundTaskQueue)) {
      // Ok... that's pretty broken. Try main instead.
      MOZ_ASSERT(false);
      backgroundTaskQueue = GetMainThreadSerialEventTarget();
    }

    RefPtr<GenericPromise> shutdownPromise = mTaskQueue->BeginShutdown()->Then(
        backgroundTaskQueue, __func__, [this] {
          // Wait until shutdown is complete, then delete for real. Although we
          // prevent queued tasks from executing with mHasShutdown, that is a
          // member variable, which means we still need to ensure that the
          // queue is done executing tasks before destroying it.
          delete this;
          return GenericPromise::CreateAndResolve(true, __func__);
        });
    if constexpr (Deletion == DeletionPolicy::Blocking) {
      media::Await(backgroundTaskQueue.forget(), shutdownPromise);
    } else {
      Unused << shutdownPromise;
    }
  }

  already_AddRefed<Runnable> CreateTaskRunner(
      absl::AnyInvocable<void() &&> aTask) {
    return NS_NewRunnableFunction(
        "TaskQueueWrapper::CreateTaskRunner",
        [this, task = std::move(aTask),
         name = nsPrintfCString("TQ %s: webrtc::QueuedTask",
                                mName.get())]() mutable {
          CurrentTaskQueueSetter current(this);
          auto hasShutdown = mHasShutdown.Lock();
          if (*hasShutdown) {
            return;
          }
          AUTO_PROFILE_FOLLOWING_RUNNABLE(name);
          std::move(task)();
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
          AUTO_PROFILE_FOLLOWING_RUNNABLE(runnable);
          runnable->Run();
        });
  }

  void PostTaskImpl(absl::AnyInvocable<void() &&> aTask,
                    const PostTaskTraits& aTraits,
                    const webrtc::Location& aLocation) override {
    MOZ_ALWAYS_SUCCEEDS(
        mTaskQueue->Dispatch(CreateTaskRunner(std::move(aTask))));
  }

  void PostDelayedTaskImpl(absl::AnyInvocable<void() &&> aTask,
                           webrtc::TimeDelta aDelay,
                           const PostDelayedTaskTraits& aTraits,
                           const webrtc::Location& aLocation) override {
    if (aDelay.ms() == 0) {
      // AbstractThread::DelayedDispatch doesn't support delay 0
      PostTaskImpl(std::move(aTask), PostTaskTraits{}, aLocation);
      return;
    }
    MOZ_ALWAYS_SUCCEEDS(mTaskQueue->DelayedDispatch(
        CreateTaskRunner(std::move(aTask)), aDelay.ms()));
  }

  const RefPtr<TaskQueue> mTaskQueue;
  const nsCString mName;

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

template <DeletionPolicy Deletion>
class DefaultDelete<TaskQueueWrapper<Deletion>>
    : public webrtc::TaskQueueDeleter {
 public:
  void operator()(TaskQueueWrapper<Deletion>* aPtr) const {
    webrtc::TaskQueueDeleter::operator()(aPtr);
  }
};

class SharedThreadPoolWebRtcTaskQueueFactory : public webrtc::TaskQueueFactory {
 public:
  SharedThreadPoolWebRtcTaskQueueFactory() {}

  template <DeletionPolicy Deletion>
  UniquePtr<TaskQueueWrapper<Deletion>> CreateTaskQueueWrapper(
      absl::string_view aName, bool aSupportTailDispatch, Priority aPriority,
      MediaThreadType aThreadType = MediaThreadType::WEBRTC_WORKER) const {
    // XXX Do something with aPriority
    nsCString name(aName.data(), aName.size());
    auto taskQueue = TaskQueue::Create(GetMediaThreadPool(aThreadType),
                                       name.get(), aSupportTailDispatch);
    return MakeUnique<TaskQueueWrapper<Deletion>>(std::move(taskQueue),
                                                  std::move(name));
  }

  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view aName, Priority aPriority) const override {
    // libwebrtc will dispatch some tasks sync, i.e., block the origin thread
    // until they've run, and that doesn't play nice with tail dispatching since
    // there will never be a tail.
    // DeletionPolicy::Blocking because this is for libwebrtc use and that's
    // what they expect.
    constexpr bool supportTailDispatch = false;
    return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
        CreateTaskQueueWrapper<DeletionPolicy::Blocking>(
            std::move(aName), supportTailDispatch, aPriority)
            .release(),
        webrtc::TaskQueueDeleter());
  }
};

}  // namespace mozilla

#endif
