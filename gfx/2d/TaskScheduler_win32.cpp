/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TaskScheduler.h"
#include "mozilla/gfx/Logging.h"

using namespace std;

namespace mozilla {
namespace gfx {

DWORD __stdcall ThreadCallback(void* threadData);

class WorkerThreadWin32 : public WorkerThread {
public:
  WorkerThreadWin32(MultiThreadedTaskQueue* aTaskQueue)
  : WorkerThread(aTaskQueue)
  {
    mThread = ::CreateThread(nullptr, 0, ThreadCallback, static_cast<WorkerThread*>(this), 0, nullptr);
  }

  ~WorkerThreadWin32()
  {
    ::WaitForSingleObject(mThread, INFINITE);
    ::CloseHandle(mThread);
  }

protected:
  HANDLE mThread;
};

DWORD __stdcall ThreadCallback(void* threadData)
{
  WorkerThread* thread = static_cast<WorkerThread*>(threadData);
  thread->Run();
  return 0;
}

WorkerThread*
WorkerThread::Create(MultiThreadedTaskQueue* aTaskQueue)
{
  return new WorkerThreadWin32(aTaskQueue);
}

bool
MultiThreadedTaskQueue::PopTask(Task*& aOutTask, AccessType aAccess)
{
  for (;;) {
    while (aAccess == BLOCKING && mTasks.empty()) {
      {
        MutexAutoLock lock(&mMutex);
        if (mShuttingDown) {
          return false;
        }
      }

      HANDLE handles[] = { mAvailableEvent, mShutdownEvent };
      ::WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    }

    MutexAutoLock lock(&mMutex);

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

    if (mTasks.empty()) {
      ::ResetEvent(mAvailableEvent);
    }

    aOutTask = task;
    return true;
  }
}

void
MultiThreadedTaskQueue::SubmitTask(Task* aTask)
{
  MOZ_ASSERT(aTask);
  MOZ_ASSERT(aTask->GetTaskQueue() == this);
  MutexAutoLock lock(&mMutex);
  mTasks.push_back(aTask);
  ::SetEvent(mAvailableEvent);
}

void
MultiThreadedTaskQueue::ShutDown()
{
  {
    MutexAutoLock lock(&mMutex);
    mShuttingDown = true;
  }
  while (mThreadsCount) {
    ::SetEvent(mAvailableEvent);
    ::WaitForSingleObject(mShutdownEvent, INFINITE);
  }
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
    ::SetEvent(mShutdownEvent);
  }
}

} // namespace
} // namespace
