/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JobScheduler.h"
#include "mozilla/gfx/Logging.h"

using namespace std;

namespace mozilla {
namespace gfx {

DWORD __stdcall ThreadCallback(void* threadData);

class WorkerThreadWin32 : public WorkerThread {
public:
  explicit WorkerThreadWin32(MultiThreadedJobQueue* aJobQueue)
  : WorkerThread(aJobQueue)
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
WorkerThread::Create(MultiThreadedJobQueue* aJobQueue)
{
  return new WorkerThreadWin32(aJobQueue);
}

bool
MultiThreadedJobQueue::PopJob(Job*& aOutJob, AccessType aAccess)
{
  for (;;) {
    while (aAccess == BLOCKING && mJobs.empty()) {
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

    if (mJobs.empty()) {
      if (aAccess == NON_BLOCKING) {
        return false;
      }
      continue;
    }

    Job* task = mJobs.front();
    MOZ_ASSERT(task);

    mJobs.pop_front();

    if (mJobs.empty()) {
      ::ResetEvent(mAvailableEvent);
    }

    aOutJob = task;
    return true;
  }
}

void
MultiThreadedJobQueue::SubmitJob(Job* aJob)
{
  MOZ_ASSERT(aJob);
  MutexAutoLock lock(&mMutex);
  mJobs.push_back(aJob);
  ::SetEvent(mAvailableEvent);
}

void
MultiThreadedJobQueue::ShutDown()
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
MultiThreadedJobQueue::NumJobs()
{
  MutexAutoLock lock(&mMutex);
  return mJobs.size();
}

bool
MultiThreadedJobQueue::IsEmpty()
{
  MutexAutoLock lock(&mMutex);
  return mJobs.empty();
}

void
MultiThreadedJobQueue::RegisterThread()
{
  mThreadsCount += 1;
}

void
MultiThreadedJobQueue::UnregisterThread()
{
  MutexAutoLock lock(&mMutex);
  mThreadsCount -= 1;
  if (mThreadsCount == 0) {
    ::SetEvent(mShutdownEvent);
  }
}

} // namespace
} // namespace
