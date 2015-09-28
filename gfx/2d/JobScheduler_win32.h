/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WIN32
#ifndef MOZILLA_GFX_TASKSCHEDULER_WIN32_H_
#define MOZILLA_GFX_TASKSCHEDULER_WIN32_H_

#include <windows.h>
#include <list>

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/CriticalSection.h"

namespace mozilla {
namespace gfx {

class WorkerThread;
class Job;

// The public interface of this class must remain identical to its equivalent
// in JobScheduler_posix.h
class MultiThreadedJobQueue {
public:
  enum AccessType {
    BLOCKING,
    NON_BLOCKING
  };

  MultiThreadedJobQueue()
  : mThreadsCount(0)
  , mShuttingDown(false)
  {
    mAvailableEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    mShutdownEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
  }

  ~MultiThreadedJobQueue()
  {
    ::CloseHandle(mAvailableEvent);
    ::CloseHandle(mShutdownEvent);
  }

  bool WaitForJob(Job*& aOutJob) { return PopJob(aOutJob, BLOCKING); }

  bool PopJob(Job*& aOutJob, AccessType aAccess);

  void SubmitJob(Job* aJob);

  void ShutDown();

  size_t NumJobs();

  bool IsEmpty();

  void RegisterThread();

  void UnregisterThread();

protected:
  std::list<Job*> mJobs;
  CriticalSection mSection;
  HANDLE mAvailableEvent;
  HANDLE mShutdownEvent;
  int32_t mThreadsCount;
  bool mShuttingDown;

  friend class WorkerThread;
};


// The public interface of this class must remain identical to its equivalent
// in JobScheduler_posix.h
class EventObject : public external::AtomicRefCounted<EventObject>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(EventObject)

  EventObject() { mEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr); }

  ~EventObject() { ::CloseHandle(mEvent); }

  void Wait() { ::WaitForSingleObject(mEvent, INFINITE); }

  bool Peak() { return ::WaitForSingleObject(mEvent, 0) == WAIT_OBJECT_0; }

  void Set() { ::SetEvent(mEvent); }
protected:
  // TODO: it's expensive to create events so we should try to reuse them
  HANDLE mEvent;
};

} // namespace
} // namespace

#endif
#endif
