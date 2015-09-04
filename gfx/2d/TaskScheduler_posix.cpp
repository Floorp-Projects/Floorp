/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TaskScheduler.h"
#include "mozilla/gfx/Logging.h"

using namespace std;

namespace mozilla {
namespace gfx {

void* ThreadCallback(void* threadData);

class WorkerThreadPosix : public WorkerThread {
public:
  explicit WorkerThreadPosix(MultiThreadedTaskQueue* aTaskQueue)
  : WorkerThread(aTaskQueue)
  {
    pthread_create(&mThread, nullptr, ThreadCallback, static_cast<WorkerThread*>(this));
  }

  ~WorkerThreadPosix()
  {
    pthread_join(mThread, nullptr);
  }

protected:
  pthread_t mThread;
};

void* ThreadCallback(void* threadData)
{
  WorkerThread* thread = static_cast<WorkerThread*>(threadData);
  thread->Run();
  return nullptr;
}

WorkerThread*
WorkerThread::Create(MultiThreadedTaskQueue* aTaskQueue)
{
  return new WorkerThreadPosix(aTaskQueue);
}

MultiThreadedTaskQueue::MultiThreadedTaskQueue()
: mThreadsCount(0)
, mShuttingDown(false)
{}

MultiThreadedTaskQueue::~MultiThreadedTaskQueue()
{
  MOZ_ASSERT(mTasks.empty());
}

bool
MultiThreadedTaskQueue::WaitForTask(Task*& aOutTask)
{
  return PopTask(aOutTask, BLOCKING);
}

bool
MultiThreadedTaskQueue::PopTask(Task*& aOutTasks, AccessType aAccess)
{
  for (;;) {
    MutexAutoLock lock(&mMutex);

    while (aAccess == BLOCKING && !mShuttingDown && mTasks.empty()) {
      mAvailableCondvar.Wait(&mMutex);
    }

    if (mShuttingDown) {
      return false;
    }

    if (mTasks.empty()) {
      if (aAccess == NON_BLOCKING) {
        return false;
      }
      continue;
    }

    Task* task = mTasks.front();
    MOZ_ASSERT(task);

    mTasks.pop_front();

    aOutTasks = task;
    return true;
  }
}

void
MultiThreadedTaskQueue::SubmitTask(Task* aTasks)
{
  MOZ_ASSERT(aTasks);
  MOZ_ASSERT(aTasks->GetTaskQueue() == this);
  MutexAutoLock lock(&mMutex);
  mTasks.push_back(aTasks);
  mAvailableCondvar.Broadcast();
}

size_t
MultiThreadedTaskQueue::NumTasks()
{
  MutexAutoLock lock(&mMutex);
  return mTasks.size();
}

bool
MultiThreadedTaskQueue::IsEmpty()
{
  MutexAutoLock lock(&mMutex);
  return mTasks.empty();
}

void
MultiThreadedTaskQueue::ShutDown()
{
  MutexAutoLock lock(&mMutex);
  mShuttingDown = true;
  while (mThreadsCount) {
    mAvailableCondvar.Broadcast();
    mShutdownCondvar.Wait(&mMutex);
  }
}

void
MultiThreadedTaskQueue::RegisterThread()
{
  mThreadsCount += 1;
}

void
MultiThreadedTaskQueue::UnregisterThread()
{
  MutexAutoLock lock(&mMutex);
  mThreadsCount -= 1;
  if (mThreadsCount == 0) {
    mShutdownCondvar.Broadcast();
  }
}

EventObject::EventObject()
: mIsSet(false)
{}

EventObject::~EventObject()
{}

bool
EventObject::Peak()
{
  MutexAutoLock lock(&mMutex);
  return mIsSet;
}

void
EventObject::Set()
{
  MutexAutoLock lock(&mMutex);
  if (!mIsSet) {
    mIsSet = true;
    mCond.Broadcast();
  }
}

void
EventObject::Wait()
{
  MutexAutoLock lock(&mMutex);
  if (mIsSet) {
    return;
  }
  mCond.Wait(&mMutex);
}

} // namespce
} // namespce
