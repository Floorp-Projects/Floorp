/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JobScheduler.h"
#include "mozilla/gfx/Logging.h"

using namespace std;

namespace mozilla {
namespace gfx {

void* ThreadCallback(void* threadData);

class WorkerThreadPosix : public WorkerThread {
public:
  explicit WorkerThreadPosix(MultiThreadedJobQueue* aJobQueue)
  : WorkerThread(aJobQueue)
  {
    pthread_create(&mThread, nullptr, ThreadCallback, static_cast<WorkerThread*>(this));
  }

  ~WorkerThreadPosix()
  {
    pthread_join(mThread, nullptr);
  }

  virtual void SetName(const char*) override
  {
// XXX - temporarily disabled, see bug 1209039
//
//    // Call this from the thread itself because of Mac.
//#ifdef XP_MACOSX
//    pthread_setname_np(aName);
//#elif defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
//    pthread_set_name_np(mThread, aName);
//#elif defined(__NetBSD__)
//    pthread_setname_np(mThread, "%s", (void*)aName);
//#else
//    pthread_setname_np(mThread, aName);
//#endif
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
WorkerThread::Create(MultiThreadedJobQueue* aJobQueue)
{
  return new WorkerThreadPosix(aJobQueue);
}

MultiThreadedJobQueue::MultiThreadedJobQueue()
: mThreadsCount(0)
, mShuttingDown(false)
{}

MultiThreadedJobQueue::~MultiThreadedJobQueue()
{
  MOZ_ASSERT(mJobs.empty());
}

bool
MultiThreadedJobQueue::WaitForJob(Job*& aOutJob)
{
  return PopJob(aOutJob, BLOCKING);
}

bool
MultiThreadedJobQueue::PopJob(Job*& aOutJobs, AccessType aAccess)
{
  for (;;) {
    MutexAutoLock lock(&mMutex);

    while (aAccess == BLOCKING && !mShuttingDown && mJobs.empty()) {
      mAvailableCondvar.Wait(&mMutex);
    }

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

    aOutJobs = task;
    return true;
  }
}

void
MultiThreadedJobQueue::SubmitJob(Job* aJobs)
{
  MOZ_ASSERT(aJobs);
  MutexAutoLock lock(&mMutex);
  mJobs.push_back(aJobs);
  mAvailableCondvar.Broadcast();
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
MultiThreadedJobQueue::ShutDown()
{
  MutexAutoLock lock(&mMutex);
  mShuttingDown = true;
  while (mThreadsCount) {
    mAvailableCondvar.Broadcast();
    mShutdownCondvar.Wait(&mMutex);
  }
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
