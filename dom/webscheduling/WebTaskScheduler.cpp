/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTHashMap.h"
#include "WebTaskScheduler.h"
#include "WebTaskSchedulerWorker.h"
#include "WebTaskSchedulerMainThread.h"
#include "TaskSignal.h"
#include "nsGlobalWindowInner.h"

#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/TimeoutManager.h"

namespace mozilla::dom {

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, WebTaskQueue& aQueue,
    const char* aName, uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aQueue.Tasks(), aName, aFlags);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(WebTask)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WebTask)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WebTask)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebTask)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebTask)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebTask)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(DelayedWebTaskHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DelayedWebTaskHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DelayedWebTaskHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DelayedWebTaskHandler)

void WebTask::RunAbortAlgorithm() {
  // no-op if WebTask::Run has been called already
  if (mPromise->State() == Promise::PromiseState::Pending) {
    // There are two things that can keep a WebTask alive, either the abort
    // signal or WebTaskQueue.
    // It's possible that this task get cleared out from the WebTaskQueue first,
    // and then the abort signal get aborted. For example, the callback function
    // was async and there's a signal.abort() call in the callback.
    if (isInList()) {
      remove();
    }

    AutoJSAPI jsapi;
    if (!jsapi.Init(mPromise->GetGlobalObject())) {
      mPromise->MaybeReject(NS_ERROR_UNEXPECTED);
    } else {
      JSContext* cx = jsapi.cx();
      JS::Rooted<JS::Value> reason(cx);
      Signal()->GetReason(cx, &reason);
      mPromise->MaybeReject(reason);
    }
  }

  MOZ_ASSERT(!isInList());
}

bool WebTask::Run() {
  MOZ_ASSERT(HasScheduled());
  MOZ_ASSERT(mOwnerQueue);
  remove();

  mOwnerQueue->RemoveEntryFromTaskQueueMapIfNeeded();
  mOwnerQueue = nullptr;
  // At this point mOwnerQueue is destructed and this is fine.
  // The caller of WebTask::Run keeps it alive.

  ErrorResult error;

  nsIGlobalObject* global = mPromise->GetGlobalObject();
  if (!global || global->IsDying()) {
    return false;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(global)) {
    return false;
  }

  JS::Rooted<JS::Value> returnVal(jsapi.cx());

  MOZ_ASSERT(mPromise->State() == Promise::PromiseState::Pending);

  MOZ_KnownLive(mCallback)->Call(&returnVal, error, "WebTask",
                                 CallbackFunction::eRethrowExceptions);

  error.WouldReportJSException();

#ifdef DEBUG
  Promise::PromiseState promiseState = mPromise->State();

  // If the state is Rejected, it means the above Call triggers the
  // RunAbortAlgorithm method and rejected the promise
  MOZ_ASSERT_IF(promiseState != Promise::PromiseState::Pending,
                promiseState == Promise::PromiseState::Rejected);
#endif

  if (error.Failed()) {
    if (!error.IsUncatchableException()) {
      mPromise->MaybeReject(std::move(error));
    } else {
      error.SuppressException();
    }
  } else {
    mPromise->MaybeResolve(returnVal);
  }

  MOZ_ASSERT(!isInList());
  return true;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebTaskScheduler, mParent,
                                      mStaticPriorityTaskQueues,
                                      mDynamicPriorityTaskQueues)

/* static */
already_AddRefed<WebTaskSchedulerMainThread>
WebTaskScheduler::CreateForMainThread(nsGlobalWindowInner* aWindow) {
  RefPtr<WebTaskSchedulerMainThread> scheduler =
      new WebTaskSchedulerMainThread(aWindow->AsGlobal());
  return scheduler.forget();
}

already_AddRefed<WebTaskSchedulerWorker> WebTaskScheduler::CreateForWorker(
    WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();
  RefPtr<WebTaskSchedulerWorker> scheduler =
      new WebTaskSchedulerWorker(aWorkerPrivate);
  return scheduler.forget();
}

WebTaskScheduler::WebTaskScheduler(nsIGlobalObject* aParent)
    : mParent(aParent), mNextEnqueueOrder(1) {
  MOZ_ASSERT(aParent);
}

JSObject* WebTaskScheduler::WrapObject(JSContext* cx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return Scheduler_Binding::Wrap(cx, this, aGivenProto);
}

already_AddRefed<Promise> WebTaskScheduler::PostTask(
    SchedulerPostTaskCallback& aCallback,
    const SchedulerPostTaskOptions& aOptions) {
  const Optional<OwningNonNull<AbortSignal>>& taskSignal = aOptions.mSignal;
  const Optional<TaskPriority>& taskPriority = aOptions.mPriority;

  ErrorResult rv;
  // Instead of making WebTaskScheduler::PostTask throws, we always
  // create the promise and return it. This is because we need to
  // create the promise explicitly to be able to reject it with
  // signal's reason.
  RefPtr<Promise> promise = Promise::Create(mParent, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  nsIGlobalObject* global = GetParentObject();
  if (!global || global->IsDying()) {
    promise->MaybeRejectWithNotSupportedError("Current window is detached");
    return promise.forget();
  }

  if (taskSignal.WasPassed()) {
    AbortSignal& signalValue = taskSignal.Value();

    if (signalValue.Aborted()) {
      AutoJSAPI jsapi;
      if (!jsapi.Init(global)) {
        promise->MaybeReject(NS_ERROR_UNEXPECTED);
        return promise.forget();
      }

      JSContext* cx = jsapi.cx();
      JS::Rooted<JS::Value> reason(cx);
      signalValue.GetReason(cx, &reason);
      promise->MaybeReject(reason);
      return promise.forget();
    }
  }

  // Let queue be the result of selecting the scheduler task queue for scheduler
  // given signal and priority.
  WebTaskQueue& taskQueue = SelectTaskQueue(taskSignal, taskPriority);

  uint64_t delay = aOptions.mDelay;

  RefPtr<WebTask> task = CreateTask(taskQueue, taskSignal, aCallback, promise);
  if (delay > 0) {
    nsresult rv = SetTimeoutForDelayedTask(task, delay);
    if (NS_FAILED(rv)) {
      promise->MaybeRejectWithUnknownError(
          "Failed to setup timeout for delayed task");
    }
    return promise.forget();
  }

  if (!QueueTask(task)) {
    MOZ_ASSERT(task->isInList());
    task->remove();

    promise->MaybeRejectWithNotSupportedError("Unable to queue the task");
    return promise.forget();
  }

  return promise.forget();
}

already_AddRefed<WebTask> WebTaskScheduler::CreateTask(
    WebTaskQueue& aQueue, const Optional<OwningNonNull<AbortSignal>>& aSignal,
    SchedulerPostTaskCallback& aCallback, Promise* aPromise) {
  uint32_t nextEnqueueOrder = mNextEnqueueOrder;
  ++mNextEnqueueOrder;

  RefPtr<WebTask> task = new WebTask(nextEnqueueOrder, aCallback, aPromise);

  aQueue.AddTask(task);

  if (aSignal.WasPassed()) {
    AbortSignal& signalValue = aSignal.Value();
    task->Follow(&signalValue);
  }

  return task.forget();
}

bool WebTaskScheduler::QueueTask(WebTask* aTask) {
  if (!DispatchEventLoopRunnable()) {
    return false;
  }
  MOZ_ASSERT(!aTask->HasScheduled());
  aTask->SetHasScheduled(true);
  return true;
}

WebTask* WebTaskScheduler::GetNextTask() const {
  // We first combine queues from both mStaticPriorityTaskQueues and
  // mDynamicPriorityTaskQueues into a single hash map which the
  // keys are the priorities and the values are all the queues that
  // belong to this priority.
  //
  // Then From the queues which
  //   1. Have scheduled tasks
  //   2. Its priority is not less than any other queues' priority
  // We pick the task which has the smallest enqueue order.
  nsTHashMap<nsUint32HashKey, nsTArray<WebTaskQueue*>> allQueues;

  for (auto iter = mStaticPriorityTaskQueues.ConstIter(); !iter.Done();
       iter.Next()) {
    const auto& queue = iter.Data();
    if (!queue->Tasks().isEmpty() && queue->GetFirstScheduledTask()) {
      nsTArray<WebTaskQueue*>& queuesForThisPriority =
          allQueues.LookupOrInsert(static_cast<uint32_t>(iter.Key()));
      queuesForThisPriority.AppendElement(queue.get());
    }
  }

  for (auto iter = mDynamicPriorityTaskQueues.ConstIter(); !iter.Done();
       iter.Next()) {
    const auto& queue = iter.Data();
    if (!queue->Tasks().isEmpty() && queue->GetFirstScheduledTask()) {
      nsTArray<WebTaskQueue*>& queuesForThisPriority = allQueues.LookupOrInsert(
          static_cast<uint32_t>(iter.Key()->Priority()));
      queuesForThisPriority.AppendElement(queue.get());
    }
  }

  if (allQueues.IsEmpty()) {
    return nullptr;
  }

  for (uint32_t priority = static_cast<uint32_t>(TaskPriority::User_blocking);
       priority < static_cast<uint32_t>(TaskPriority::EndGuard_); ++priority) {
    if (auto queues = allQueues.Lookup(priority)) {
      WebTaskQueue* oldestQueue = nullptr;
      MOZ_ASSERT(!queues.Data().IsEmpty());
      for (auto& webTaskQueue : queues.Data()) {
        MOZ_ASSERT(webTaskQueue->GetFirstScheduledTask());
        if (!oldestQueue) {
          oldestQueue = webTaskQueue;
        } else {
          WebTask* firstScheduledRunnableForCurrentQueue =
              webTaskQueue->GetFirstScheduledTask();
          WebTask* firstScheduledRunnableForOldQueue =
              oldestQueue->GetFirstScheduledTask();
          if (firstScheduledRunnableForOldQueue->EnqueueOrder() >
              firstScheduledRunnableForCurrentQueue->EnqueueOrder()) {
            oldestQueue = webTaskQueue;
          }
        }
      }
      MOZ_ASSERT(oldestQueue);
      return oldestQueue->GetFirstScheduledTask();
    }
  }
  return nullptr;
}

void WebTaskScheduler::Disconnect() {
  mStaticPriorityTaskQueues.Clear();
  mDynamicPriorityTaskQueues.Clear();
}

void WebTaskScheduler::RunTaskSignalPriorityChange(TaskSignal* aTaskSignal) {
  if (WebTaskQueue* const taskQueue =
          mDynamicPriorityTaskQueues.Get(aTaskSignal)) {
    taskQueue->SetPriority(aTaskSignal->Priority());
  }
}

WebTaskQueue& WebTaskScheduler::SelectTaskQueue(
    const Optional<OwningNonNull<AbortSignal>>& aSignal,
    const Optional<TaskPriority>& aPriority) {
  bool useSignal = !aPriority.WasPassed() && aSignal.WasPassed() &&
                   aSignal.Value().IsTaskSignal();

  if (useSignal) {
    TaskSignal* taskSignal = static_cast<TaskSignal*>(&(aSignal.Value()));
    WebTaskQueue* const taskQueue =
        mDynamicPriorityTaskQueues.GetOrInsertNew(taskSignal, taskSignal, this);
    taskQueue->SetPriority(taskSignal->Priority());
    taskSignal->SetWebTaskScheduler(this);
    MOZ_ASSERT(mDynamicPriorityTaskQueues.Contains(taskSignal));

    return *taskQueue;
  }

  TaskPriority taskPriority =
      aPriority.WasPassed() ? aPriority.Value() : TaskPriority::User_visible;

  uint32_t staticTaskQueueMapKey = static_cast<uint32_t>(taskPriority);
  WebTaskQueue* const taskQueue = mStaticPriorityTaskQueues.GetOrInsertNew(
      staticTaskQueueMapKey, staticTaskQueueMapKey, this);
  taskQueue->SetPriority(taskPriority);
  MOZ_ASSERT(
      mStaticPriorityTaskQueues.Contains(static_cast<uint32_t>(taskPriority)));
  return *taskQueue;
}

void WebTaskScheduler::DeleteEntryFromStaticQueueMap(uint32_t aKey) {
  DebugOnly<bool> result = mStaticPriorityTaskQueues.Remove(aKey);
  MOZ_ASSERT(result);
}

void WebTaskScheduler::DeleteEntryFromDynamicQueueMap(TaskSignal* aKey) {
  DebugOnly<bool> result = mDynamicPriorityTaskQueues.Remove(aKey);
  MOZ_ASSERT(result);
}

void WebTaskQueue::RemoveEntryFromTaskQueueMapIfNeeded() {
  MOZ_ASSERT(mScheduler);
  if (mTasks.isEmpty()) {
    if (mOwnerKey.is<uint32_t>()) {
      mScheduler->DeleteEntryFromStaticQueueMap(mOwnerKey.as<uint32_t>());
    } else {
      MOZ_ASSERT(mOwnerKey.is<TaskSignal*>());
      mScheduler->DeleteEntryFromDynamicQueueMap(mOwnerKey.as<TaskSignal*>());
    }
  }
}
}  // namespace mozilla::dom
