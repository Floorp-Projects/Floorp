/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerJobQueue.h"

#include "ServiceWorkerJob.h"
#include "Workers.h"

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorkerJobQueue::Callback final : public ServiceWorkerJob::Callback
{
  RefPtr<ServiceWorkerJobQueue> mQueue;

  ~Callback()
  {
  }

public:
  explicit Callback(ServiceWorkerJobQueue* aQueue)
    : mQueue(aQueue)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mQueue);
  }

  virtual void
  JobFinished(ServiceWorkerJob* aJob, ErrorResult& aStatus) override
  {
    AssertIsOnMainThread();
    mQueue->JobFinished(aJob);
  }

  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerJobQueue::Callback, override)
};

ServiceWorkerJobQueue::~ServiceWorkerJobQueue()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mJobList.IsEmpty());
}

void
ServiceWorkerJobQueue::JobFinished(ServiceWorkerJob* aJob)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aJob);

  // XXX There are some corner cases where jobs can double-complete.  Until
  // we track all these down we do a non-fatal assert in debug builds and
  // a runtime check to verify the queue is in the correct state.
  NS_ASSERTION(!mJobList.IsEmpty(),
               "Job queue should contain the job that just completed.");
  NS_ASSERTION(mJobList.SafeElementAt(0, nullptr) == aJob,
               "Job queue should contain the job that just completed.");
  if (NS_WARN_IF(mJobList.SafeElementAt(0, nullptr) != aJob)) {
    return;
  }

  mJobList.RemoveElementAt(0);

  if (mJobList.IsEmpty()) {
    return;
  }

  RunJob();
}

void
ServiceWorkerJobQueue::RunJob()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!mJobList.IsEmpty());
  MOZ_ASSERT(mJobList[0]->GetState() == ServiceWorkerJob::State::Initial);

  RefPtr<Callback> callback = new Callback(this);
  mJobList[0]->Start(callback);
}

ServiceWorkerJobQueue::ServiceWorkerJobQueue()
{
  AssertIsOnMainThread();
}

void
ServiceWorkerJobQueue::ScheduleJob(ServiceWorkerJob* aJob)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aJob);
  MOZ_ASSERT(!mJobList.Contains(aJob));

  if (mJobList.IsEmpty()) {
    mJobList.AppendElement(aJob);
    RunJob();
    return;
  }

  MOZ_ASSERT(mJobList[0]->GetState() == ServiceWorkerJob::State::Started);

  RefPtr<ServiceWorkerJob>& tailJob = mJobList[mJobList.Length() - 1];
  if (!tailJob->ResultCallbacksInvoked() && aJob->IsEquivalentTo(tailJob)) {
    tailJob->StealResultCallbacksFrom(aJob);
    return;
  }

  mJobList.AppendElement(aJob);
}

void
ServiceWorkerJobQueue::CancelAll()
{
  AssertIsOnMainThread();

  for (RefPtr<ServiceWorkerJob>& job : mJobList) {
    job->Cancel();
  }

  // Remove jobs that are queued but not started since they should never
  // run after being canceled.  This means throwing away all jobs except
  // for the job at the front of the list.
  if (!mJobList.IsEmpty()) {
    MOZ_ASSERT(mJobList[0]->GetState() == ServiceWorkerJob::State::Started);
    mJobList.TruncateLength(1);
  }
}

} // namespace workers
} // namespace dom
} // namespace mozilla
