/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIN32
#ifndef MOZILLA_GFX_TASKSCHEDULER_POSIX_H_
#define MOZILLA_GFX_TASKSCHEDULER_POSIX_H_

#include <string>
#include <vector>
#include <list>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include "mozilla/RefPtr.h"
#include "mozilla/DebugOnly.h"

namespace mozilla {
namespace gfx {

class Job;
class PosixCondVar;

class Mutex {
public:
  Mutex() {
    DebugOnly<int> err = pthread_mutex_init(&mMutex, nullptr);
    MOZ_ASSERT(!err);
  }

  ~Mutex() {
    DebugOnly<int> err = pthread_mutex_destroy(&mMutex);
    MOZ_ASSERT(!err);
  }

  void Lock() {
    DebugOnly<int> err = pthread_mutex_lock(&mMutex);
    MOZ_ASSERT(!err);
  }

  void Unlock() {
    DebugOnly<int> err = pthread_mutex_unlock(&mMutex);
    MOZ_ASSERT(!err);
  }

protected:
  pthread_mutex_t mMutex;
  friend class PosixCondVar;
};

// posix platforms only!
class PosixCondVar {
public:
  PosixCondVar() {
    DebugOnly<int> err = pthread_cond_init(&mCond, nullptr);
    MOZ_ASSERT(!err);
  }

  ~PosixCondVar() {
    DebugOnly<int> err = pthread_cond_destroy(&mCond);
    MOZ_ASSERT(!err);
  }

  void Wait(Mutex* aMutex) {
    DebugOnly<int> err = pthread_cond_wait(&mCond, &aMutex->mMutex);
    MOZ_ASSERT(!err);
  }

  void Broadcast() {
    DebugOnly<int> err = pthread_cond_broadcast(&mCond);
    MOZ_ASSERT(!err);
  }

protected:
  pthread_cond_t mCond;
};


/// A simple and naive multithreaded task queue
///
/// The public interface of this class must remain identical to its equivalent
/// in JobScheduler_win32.h
class MultiThreadedJobQueue {
public:
  enum AccessType {
    BLOCKING,
    NON_BLOCKING
  };

  // Producer thread
  MultiThreadedJobQueue();

  // Producer thread
  ~MultiThreadedJobQueue();

  // Worker threads
  bool WaitForJob(Job*& aOutJob);

  // Any thread
  bool PopJob(Job*& aOutJob, AccessType aAccess);

  // Any threads
  void SubmitJob(Job* aJob);

  // Producer thread
  void ShutDown();

  // Any thread
  size_t NumJobs();

  // Any thread
  bool IsEmpty();

  // Producer thread
  void RegisterThread();

  // Worker threads
  void UnregisterThread();

protected:

  std::list<Job*> mJobs;
  Mutex mMutex;
  PosixCondVar mAvailableCondvar;
  PosixCondVar mShutdownCondvar;
  int32_t mThreadsCount;
  bool mShuttingDown;

  friend class WorkerThread;
};

/// Worker thread that continuously dequeues Jobs from a MultiThreadedJobQueue
/// and process them.
///
/// The public interface of this class must remain identical to its equivalent
/// in JobScheduler_win32.h
class WorkerThread {
public:
  explicit WorkerThread(MultiThreadedJobQueue* aJobQueue);

  ~WorkerThread();

  void Run();

  MultiThreadedJobQueue* GetJobQueue() { return mQueue; }
protected:
  void SetName(const char* name);

  MultiThreadedJobQueue* mQueue;
  pthread_t mThread;
};

/// An object that a thread can synchronously wait on.
/// Usually set by a SetEventJob.
class EventObject : public external::AtomicRefCounted<EventObject>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(EventObject)

  EventObject();

  ~EventObject();

  /// Synchronously wait until the event is set.
  void Wait();

  /// Return true if the event is set, without blocking.
  bool Peak();

  /// Set the event.
  void Set();

protected:
  Mutex mMutex;
  PosixCondVar mCond;
  bool mIsSet;
};

} // namespace
} // namespace

#include "JobScheduler.h"

#endif
#endif
