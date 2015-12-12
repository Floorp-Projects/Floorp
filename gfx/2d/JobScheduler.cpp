/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JobScheduler.h"
#include "Logging.h"

namespace mozilla {
namespace gfx {

JobScheduler* JobScheduler::sSingleton = nullptr;

bool JobScheduler::Init(uint32_t aNumThreads, uint32_t aNumQueues)
{
  MOZ_ASSERT(!sSingleton);
  MOZ_ASSERT(aNumThreads >= aNumQueues);

  sSingleton = new JobScheduler();
  sSingleton->mNextQueue = 0;

  for (uint32_t i = 0; i < aNumQueues; ++i) {
    sSingleton->mDrawingQueues.push_back(new MultiThreadedJobQueue());
  }

  for (uint32_t i = 0; i < aNumThreads; ++i) {
    sSingleton->mWorkerThreads.push_back(WorkerThread::Create(sSingleton->mDrawingQueues[i%aNumQueues]));
  }
  return true;
}

void JobScheduler::ShutDown()
{
  MOZ_ASSERT(IsEnabled());
  if (!IsEnabled()) {
    return;
  }

  for (auto queue : sSingleton->mDrawingQueues) {
    queue->ShutDown();
    delete queue;
  }

  for (WorkerThread* thread : sSingleton->mWorkerThreads) {
    // this will block until the thread is joined.
    delete thread;
  }

  sSingleton->mWorkerThreads.clear();
  delete sSingleton;
  sSingleton = nullptr;
}

JobStatus
JobScheduler::ProcessJob(Job* aJob)
{
  MOZ_ASSERT(aJob);
  auto status = aJob->Run();
  if (status == JobStatus::Error || status == JobStatus::Complete) {
    delete aJob;
  }
  return status;
}

void
JobScheduler::SubmitJob(Job* aJob)
{
  MOZ_ASSERT(aJob);
  RefPtr<SyncObject> start = aJob->GetStartSync();
  if (start && start->Register(aJob)) {
    // The Job buffer starts with a non-signaled sync object, it
    // is now registered in the list of task buffers waiting on the
    // sync object, so we should not place it in the queue.
    return;
  }

  GetQueueForJob(aJob)->SubmitJob(aJob);
}

MultiThreadedJobQueue*
JobScheduler::GetQueueForJob(Job* aJob)
{
  return aJob->IsPinnedToAThread() ? aJob->GetWorkerThread()->GetJobQueue()
                                    : GetDrawingQueue();
}

Job::Job(SyncObject* aStart, SyncObject* aCompletion, WorkerThread* aThread)
: mNextWaitingJob(nullptr)
, mStartSync(aStart)
, mCompletionSync(aCompletion)
, mPinToThread(aThread)
{
  if (mStartSync) {
    mStartSync->AddSubsequent(this);
  }
  if (mCompletionSync) {
    mCompletionSync->AddPrerequisite(this);
  }
}

Job::~Job()
{
  if (mCompletionSync) {
    //printf(" -- Job %p dtor completion %p\n", this, mCompletionSync);
    mCompletionSync->Signal();
    mCompletionSync = nullptr;
  }
}

JobStatus
SetEventJob::Run()
{
  mEvent->Set();
  return JobStatus::Complete;
}

SetEventJob::SetEventJob(EventObject* aEvent,
                           SyncObject* aStart, SyncObject* aCompletion,
                           WorkerThread* aWorker)
: Job(aStart, aCompletion, aWorker)
, mEvent(aEvent)
{}

SetEventJob::~SetEventJob()
{}

SyncObject::SyncObject(uint32_t aNumPrerequisites)
: mSignals(aNumPrerequisites)
, mFirstWaitingJob(nullptr)
#ifdef DEBUG
, mNumPrerequisites(aNumPrerequisites)
, mAddedPrerequisites(0)
#endif
{}

SyncObject::~SyncObject()
{
  MOZ_ASSERT(mFirstWaitingJob == nullptr);
}

bool
SyncObject::Register(Job* aJob)
{
  MOZ_ASSERT(aJob);

  // For now, ensure that when we schedule the first subsequent, we have already
  // created all of the prerequisites. This is an arbitrary restriction because
  // we specify the number of prerequisites in the constructor, but in the typical
  // scenario, if the assertion FreezePrerequisite blows up here it probably means
  // we got the initial nmber of prerequisites wrong. We can decide to remove
  // this restriction if needed.
  FreezePrerequisites();

  int32_t signals = mSignals;

  if (signals > 0) {
    AddWaitingJob(aJob);
    // Since Register and Signal can be called concurrently, it can happen that
    // reading mSignals in Register happens before decrementing mSignals in Signal,
    // but SubmitWaitingJobs happens before AddWaitingJob. This ordering means
    // the SyncObject ends up in the signaled state with a task sitting in the
    // waiting list. To prevent that we check mSignals a second time and submit
    // again if signals reached zero in the mean time.
    // We do this instead of holding a mutex around mSignals+mJobs to reduce
    // lock contention.
    int32_t signals2 = mSignals;
    if (signals2 == 0) {
      SubmitWaitingJobs();
    }
    return true;
  }

  return false;
}

void
SyncObject::Signal()
{
  int32_t signals = --mSignals;
  MOZ_ASSERT(signals >= 0);

  if (signals == 0) {
    SubmitWaitingJobs();
  }
}

void
SyncObject::AddWaitingJob(Job* aJob)
{
  // Push (using atomics) the task into the list of waiting tasks.
  for (;;) {
    Job* first = mFirstWaitingJob;
    aJob->mNextWaitingJob = first;
    if (mFirstWaitingJob.compareExchange(first, aJob)) {
      break;
    }
  }
}

void SyncObject::SubmitWaitingJobs()
{
  // Scheduling the tasks can cause code that modifies <this>'s reference
  // count to run concurrently, and cause the caller of this function to
  // be owned by another thread. We need to make sure the reference count
  // does not reach 0 on another thread before the end of this method, so
  // hold a strong ref to prevent that!
  RefPtr<SyncObject> kungFuDeathGrip(this);

  // First atomically swap mFirstWaitingJob and waitingJobs...
  Job* waitingJobs = nullptr;
  for (;;) {
    waitingJobs = mFirstWaitingJob;
    if (mFirstWaitingJob.compareExchange(waitingJobs, nullptr)) {
      break;
    }
  }

  // ... and submit all of the waiting tasks in waitingJob now that they belong
  // to this thread.
  while (waitingJobs) {
    Job* next = waitingJobs->mNextWaitingJob;
    waitingJobs->mNextWaitingJob = nullptr;
    JobScheduler::GetQueueForJob(waitingJobs)->SubmitJob(waitingJobs);
    waitingJobs = next;
  }
}

bool
SyncObject::IsSignaled()
{
  return mSignals == 0;
}

void
SyncObject::FreezePrerequisites()
{
  MOZ_ASSERT(mAddedPrerequisites == mNumPrerequisites);
}

void
SyncObject::AddPrerequisite(Job* aJob)
{
  MOZ_ASSERT(++mAddedPrerequisites <= mNumPrerequisites);
}

void
SyncObject::AddSubsequent(Job* aJob)
{
}

WorkerThread::WorkerThread(MultiThreadedJobQueue* aJobQueue)
: mQueue(aJobQueue)
{
  aJobQueue->RegisterThread();
}

void
WorkerThread::Run()
{
  SetName("gfx worker");

  for (;;) {
    Job* commands = nullptr;
    if (!mQueue->WaitForJob(commands)) {
      mQueue->UnregisterThread();
      return;
    }

    JobStatus status = JobScheduler::ProcessJob(commands);

    if (status == JobStatus::Error) {
      // Don't try to handle errors for now, but that's open to discussions.
      // I expect errors to be mostly OOM issues.
      gfxDevCrash(LogReason::JobStatusError) << "Invalid job status " << (int)status;
    }
  }
}

} //namespace
} //namespace
