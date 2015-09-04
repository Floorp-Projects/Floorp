/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TaskScheduler.h"

namespace mozilla {
namespace gfx {

TaskScheduler* TaskScheduler::sSingleton = nullptr;

bool TaskScheduler::Init(uint32_t aNumThreads, uint32_t aNumQueues)
{
  MOZ_ASSERT(!sSingleton);
  MOZ_ASSERT(aNumThreads >= aNumQueues);

  sSingleton = new TaskScheduler();
  sSingleton->mNextQueue = 0;

  for (uint32_t i = 0; i < aNumQueues; ++i) {
    sSingleton->mDrawingQueues.push_back(new MultiThreadedTaskQueue());
  }

  for (uint32_t i = 0; i < aNumThreads; ++i) {
    sSingleton->mWorkerThreads.push_back(WorkerThread::Create(sSingleton->mDrawingQueues[i%aNumQueues]));
  }
  return true;
}

void TaskScheduler::ShutDown()
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

TaskStatus
TaskScheduler::ProcessTask(Task* aTask)
{
  MOZ_ASSERT(aTask);
  auto status = aTask->Run();
  if (status == TaskStatus::Error || status == TaskStatus::Complete) {
    delete aTask;
  }
  return status;
}

void
TaskScheduler::SubmitTask(Task* aTask)
{
  MOZ_ASSERT(aTask);
  RefPtr<SyncObject> start = aTask->GetStartSync();
  if (start && start->Register(aTask)) {
    // The Task buffer starts with a non-signaled sync object, it
    // is now registered in the list of task buffers waiting on the
    // sync object, so we should not place it in the queue.
    return;
  }

  aTask->GetTaskQueue()->SubmitTask(aTask);
}

Task::Task(MultiThreadedTaskQueue* aQueue, SyncObject* aStart, SyncObject* aCompletion)
: mQueue(aQueue)
, mStartSync(aStart)
, mCompletionSync(aCompletion)
{
  if (mStartSync) {
    mStartSync->AddSubsequent(this);
  }
  if (mCompletionSync) {
    mCompletionSync->AddPrerequisite(this);
  }
}

Task::~Task()
{
  if (mCompletionSync) {
    //printf(" -- Task %p dtor completion %p\n", this, mCompletionSync);
    mCompletionSync->Signal();
    mCompletionSync = nullptr;
  }
}

TaskStatus
SetEventTask::Run()
{
  mEvent->Set();
  return TaskStatus::Complete;
}

SetEventTask::SetEventTask(MultiThreadedTaskQueue* aQueue,
                           SyncObject* aStart, SyncObject* aCompletion)
: Task(aQueue, aStart, aCompletion)
{
  mEvent = new EventObject();
}

SetEventTask::~SetEventTask()
{}


SyncObject::SyncObject()
: mSignals(0)
, mHasSubmittedSubsequent(false)
{}

SyncObject::~SyncObject()
{
  MOZ_ASSERT(mWaitingTasks.size() == 0);
}

bool
SyncObject::Register(Task* aTask)
{
  MOZ_ASSERT(aTask);

  mHasSubmittedSubsequent = true;

  int32_t signals = mSignals;

  if (signals > 0) {
    AddWaitingTask(aTask);
    // Since Register and Signal can be called concurrently, it can happen that
    // reading mSignals in Register happens before decrementing mSignals in Signal,
    // but SubmitWaitingTasks happens before AddWaitingTask. This ordering means
    // the SyncObject ends up in the signaled state with a task sitting in the
    // waiting list. To prevent that we check mSignals a second time and submit
    // again if signals reached zero in the mean time.
    // We do this instead of holding a mutex around mSignals+mTasks to reduce
    // lock contention.
    int32_t signals2 = mSignals;
    if (signals2 == 0) {
      SubmitWaitingTasks();
    }
    return true;
  }

  return false;
}

void
SyncObject::Signal()
{
  // Fetch mHasSubmittedSubsequent *before* mSignals to avoid a race condition
  // where signals reach zero before we have created all of the prerequisites
  // which can lead to SubmitTasks being called with subsequents added in the
  // mean time if the thread is interrupted between the read from mSignals and
  // the read from mHasSubmittedSubsequents.
  bool hasSubmittedSubsequent = mHasSubmittedSubsequent;
  int32_t signals = --mSignals;
  MOZ_ASSERT(signals >= 0);

  if (hasSubmittedSubsequent && signals == 0) {
    SubmitWaitingTasks();
  }
}

void
SyncObject::AddWaitingTask(Task* aTask)
{
  MutexAutoLock lock(&mMutex);
  mWaitingTasks.push_back(aTask);
}

void SyncObject::SubmitWaitingTasks()
{
  std::vector<Task*> tasksToSubmit;
  {
    // Scheduling the tasks can cause code that modifies <this>'s reference
    // count to run concurrently, and cause the caller of this function to
    // be owned by another thread. We need to make sure the reference count
    // does not reach 0 on another thread before mWaitingTasks.clear(), so
    // hold a strong ref to prevent that!
    RefPtr<SyncObject> kungFuDeathGrip(this);

    MutexAutoLock lock(&mMutex);
    tasksToSubmit = Move(mWaitingTasks);
    mWaitingTasks.clear();
  }

  for (Task* task : tasksToSubmit) {
    task->GetTaskQueue()->SubmitTask(task);
  }
}

bool
SyncObject::IsSignaled()
{
  return mSignals == 0;
}

void
SyncObject::AddPrerequisite(Task* aTask)
{
#ifdef DEBUG
  mPrerequisites.push_back(aTask);
#endif
  // If this assertion blows up it means that a Task that depends on this sync
  // object has been submitted before we declared all of the prerequisites.
  // This is racy because if mSignals reaches zero before all prerequisites
  // have been declared, a subsequent can be scheduled before the completion
  // of the undeclared prerequisites.
  MOZ_ASSERT(!mHasSubmittedSubsequent);

  mSignals++;
}

void
SyncObject::AddSubsequent(Task* aTask)
{
#ifdef DEBUG
  mSubsequents.push_back(aTask);
#endif
}

WorkerThread::WorkerThread(MultiThreadedTaskQueue* aTaskQueue)
: mQueue(aTaskQueue)
{
  aTaskQueue->RegisterThread();
}

void
WorkerThread::Run()
{
  for (;;) {
    Task* commands = nullptr;
    if (!mQueue->WaitForTask(commands)) {
      mQueue->UnregisterThread();
      return;
    }

    TaskStatus status = TaskScheduler::ProcessTask(commands);

    if (status == TaskStatus::Error) {
      // Don't try to handle errors for now, but that's open to discussions.
      // I expect errors to be mostly OOM issues.
      MOZ_CRASH();
    }
  }
}

} //namespace
} //namespace
