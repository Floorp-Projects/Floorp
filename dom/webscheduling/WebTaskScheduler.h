/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebTaskScheduler_h
#define mozilla_dom_WebTaskScheduler_h

#include "nsThreadUtils.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"
#include "nsClassHashtable.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/AbortFollower.h"
#include "mozilla/dom/TimeoutHandler.h"
#include "mozilla/dom/WebTaskSchedulingBinding.h"

namespace mozilla::dom {
class WebTask : public LinkedListElement<RefPtr<WebTask>>,
                public AbortFollower,
                public SupportsWeakPtr {
  friend class WebTaskScheduler;

 public:
  MOZ_CAN_RUN_SCRIPT bool Run();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_CLASS(WebTask)
  WebTask(uint32_t aEnqueueOrder, SchedulerPostTaskCallback& aCallback,
          Promise* aPromise)
      : mEnqueueOrder(aEnqueueOrder),
        mCallback(&aCallback),
        mPromise(aPromise),
        mHasScheduled(false) {}

  void RunAbortAlgorithm() override;

  bool HasScheduled() const { return mHasScheduled; }

  uint32_t EnqueueOrder() const { return mEnqueueOrder; }

 private:
  void SetHasScheduled(bool aHasScheduled) { mHasScheduled = aHasScheduled; }

  uint32_t mEnqueueOrder;

  RefPtr<SchedulerPostTaskCallback> mCallback;
  RefPtr<Promise> mPromise;

  bool mHasScheduled;

  ~WebTask() = default;
};

class WebTaskQueue {
 public:
  WebTaskQueue() = default;

  TaskPriority Priority() const { return mPriority; }
  void SetPriority(TaskPriority aNewPriority) { mPriority = aNewPriority; }

  LinkedList<RefPtr<WebTask>>& Tasks() { return mTasks; }

  void AddTask(WebTask* aTask) { mTasks.insertBack(aTask); }

  // TODO: To optimize it, we could have the scheduled and unscheduled
  // tasks stored separately.
  WebTask* GetFirstScheduledTask() {
    for (const auto& task : mTasks) {
      if (task->HasScheduled()) {
        return task;
      }
    }
    return nullptr;
  }

  ~WebTaskQueue() { mTasks.clear(); }

 private:
  TaskPriority mPriority = TaskPriority::User_visible;
  LinkedList<RefPtr<WebTask>> mTasks;
};

class WebTaskSchedulerMainThread;
class WebTaskSchedulerWorker;

class WebTaskScheduler : public nsWrapperCache, public SupportsWeakPtr {
  friend class DelayedWebTaskHandler;

 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebTaskScheduler)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(WebTaskScheduler)

  static already_AddRefed<WebTaskSchedulerMainThread> CreateForMainThread(
      nsGlobalWindowInner* aWindow);

  static already_AddRefed<WebTaskSchedulerWorker> CreateForWorker(
      WorkerPrivate* aWorkerPrivate);

  explicit WebTaskScheduler(nsIGlobalObject* aParent);

  already_AddRefed<Promise> PostTask(SchedulerPostTaskCallback& aCallback,
                                     const SchedulerPostTaskOptions& aOptions);

  nsIGlobalObject* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) override;

  WebTask* GetNextTask() const;

  void Disconnect();

  void RunTaskSignalPriorityChange(TaskSignal* aTaskSignal);

 protected:
  virtual ~WebTaskScheduler() = default;
  nsCOMPtr<nsIGlobalObject> mParent;

  uint32_t mNextEnqueueOrder;

 private:
  already_AddRefed<WebTask> CreateTask(
      WebTaskQueue& aQueue, const Optional<OwningNonNull<AbortSignal>>& aSignal,
      SchedulerPostTaskCallback& aCallback, Promise* aPromise);

  bool QueueTask(WebTask* aTask);

  WebTaskQueue& SelectTaskQueue(
      const Optional<OwningNonNull<AbortSignal>>& aSignal,
      const Optional<TaskPriority>& aPriority);

  virtual nsresult SetTimeoutForDelayedTask(WebTask* aTask,
                                            uint64_t aDelay) = 0;
  virtual bool DispatchEventLoopRunnable() = 0;

  nsClassHashtable<nsUint32HashKey, WebTaskQueue> mStaticPriorityTaskQueues;
  nsClassHashtable<nsPtrHashKey<TaskSignal>, WebTaskQueue>
      mDynamicPriorityTaskQueues;
};

class DelayedWebTaskHandler final : public TimeoutHandler {
 public:
  DelayedWebTaskHandler(JSContext* aCx, WebTaskScheduler* aScheduler,
                        WebTask* aTask)
      : TimeoutHandler(aCx), mScheduler(aScheduler), mWebTask(aTask) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DelayedWebTaskHandler)

  MOZ_CAN_RUN_SCRIPT bool Call(const char* /* unused */) override {
    if (mScheduler && mWebTask) {
      MOZ_ASSERT(!mWebTask->HasScheduled());
      if (!mScheduler->QueueTask(mWebTask)) {
        return false;
      }
    }
    return true;
  }

 private:
  ~DelayedWebTaskHandler() override = default;
  WeakPtr<WebTaskScheduler> mScheduler;
  // WebTask gets added to WebTaskQueue, and WebTaskQueue keeps its alive.
  WeakPtr<WebTask> mWebTask;
};
}  // namespace mozilla::dom
#endif
