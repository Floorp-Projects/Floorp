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
        CriticalSectionAutoEnter lock(&mSection);
        if (mShuttingDown) {
          return false;
        }
      }

      HANDLE handles[] = { mAvailableEvent, mShutdownEvent };
      ::WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    }

    CriticalSectionAutoEnter lock(&mSection);

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
  CriticalSectionAutoEnter lock(&mSection);
  mJobs.push_back(aJob);
  ::SetEvent(mAvailableEvent);
}

void
MultiThreadedJobQueue::ShutDown()
{
  {
    CriticalSectionAutoEnter lock(&mSection);
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
  CriticalSectionAutoEnter lock(&mSection);
  return mJobs.size();
}

bool
MultiThreadedJobQueue::IsEmpty()
{
  CriticalSectionAutoEnter lock(&mSection);
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
  mSection.Enter();
  mThreadsCount -= 1;
  bool finishShutdown = mThreadsCount == 0;
  mSection.Leave();

  if (finishShutdown) {
    // Can't touch mSection or any other member from now on because this object
    // may get deleted on the main thread after mShutdownEvent is set.
    ::SetEvent(mShutdownEvent);
  }
}

} // namespace
} // namespace
