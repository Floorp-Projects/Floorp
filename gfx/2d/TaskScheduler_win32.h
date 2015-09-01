/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WIN32
#ifndef MOZILLA_GFX_TASKSCHEDULER_WIN32_H_
#define MOZILLA_GFX_TASKSCHEDULER_WIN32_H_

#define NOT_IMPLEMENTED MOZ_CRASH("Not implemented")

#include "mozilla/RefPtr.h"
#include <windows.h>
#include <list>

namespace mozilla {
namespace gfx {

class WorkerThread;
class Task;

class Mutex {
public:
  Mutex() {
    ::InitializeCriticalSection(&mMutex);
#ifdef DEBUG
    mOwner = 0;
#endif
  }

  ~Mutex() { ::DeleteCriticalSection(&mMutex); }

  void Lock() {
    ::EnterCriticalSection(&mMutex);
#ifdef DEBUG
    MOZ_ASSERT(mOwner != GetCurrentThreadId(), "recursive locking");
    mOwner = GetCurrentThreadId();
#endif
  }

  void Unlock() {
#ifdef DEBUG
    // GetCurrentThreadId cannot return 0: it is not a valid thread id
    MOZ_ASSERT(mOwner == GetCurrentThreadId(), "mismatched lock/unlock");
    mOwner = 0;
#endif
    ::LeaveCriticalSection(&mMutex);
  }

protected:
  CRITICAL_SECTION mMutex;
#ifdef DEBUG
  DWORD mOwner;
#endif
};

// The public interface of this class must remain identical to its equivalent
// in TaskScheduler_posix.h
class MultiThreadedTaskQueue {
public:
  enum AccessType {
    BLOCKING,
    NON_BLOCKING
  };

  MultiThreadedTaskQueue()
  : mThreadsCount(0)
  , mShuttingDown(false)
  {
    mAvailableEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
    mShutdownEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
  }

  ~MultiThreadedTaskQueue()
  {
    ::CloseHandle(mAvailableEvent);
    ::CloseHandle(mShutdownEvent);
  }

  bool WaitForTask(Task*& aOutTask) { return PopTask(aOutTask, BLOCKING); }

  bool PopTask(Task*& aOutTask, AccessType aAccess);

  void SubmitTask(Task* aTask);

  void ShutDown();

  size_t NumTasks();

  bool IsEmpty();

  void RegisterThread();

  void UnregisterThread();

protected:
  std::list<Task*> mTasks;
  Mutex mMutex;
  HANDLE mAvailableEvent;
  HANDLE mShutdownEvent;
  int32_t mThreadsCount;
  bool mShuttingDown;

  friend class WorkerThread;
};


// The public interface of this class must remain identical to its equivalent
// in TaskScheduler_posix.h
class EventObject : public external::AtomicRefCounted<EventObject>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(EventObject)

  EventObject() { mEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr); }

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
