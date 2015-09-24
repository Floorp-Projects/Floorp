/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WIN32
#ifndef MOZILLA_GFX_TASKSCHEDULER_WIN32_H_
#define MOZILLA_GFX_TASKSCHEDULER_WIN32_H_

#define NOT_IMPLEMENTED MOZ_CRASH("Not implemented")

#include "mozilla/RefPtr.h"

namespace mozilla {
namespace gfx {

class WorkerThread;
class Job;

class Mutex {
public:
  Mutex() { NOT_IMPLEMENTED; }
  ~Mutex() { NOT_IMPLEMENTED; }
  void Lock() { NOT_IMPLEMENTED; }
  void Unlock() { NOT_IMPLEMENTED; }
};

// The public interface of this class must remain identical to its equivalent
// in JobScheduler_posix.h
class MultiThreadedJobQueue {
public:
  enum AccessType {
    BLOCKING,
    NON_BLOCKING
  };

  bool WaitForJob(Job*& aOutCommands) { NOT_IMPLEMENTED; }
  bool PopJob(Job*& aOutCommands, AccessType aAccess) { NOT_IMPLEMENTED; }
  void SubmitJob(Job* aCommands) { NOT_IMPLEMENTED; }
  void ShutDown() { NOT_IMPLEMENTED; }
  size_t NumJobs() { NOT_IMPLEMENTED;  }
  bool IsEmpty() { NOT_IMPLEMENTED; }
  void RegisterThread() { NOT_IMPLEMENTED; }
  void UnregisterThread() { NOT_IMPLEMENTED; }

  friend class WorkerThread;
};


// The public interface of this class must remain identical to its equivalent
// in JobScheduler_posix.h
class EventObject : public external::AtomicRefCounted<EventObject>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(EventObject)

  EventObject() { NOT_IMPLEMENTED; }
  ~EventObject() { NOT_IMPLEMENTED; }
  void Wait() { NOT_IMPLEMENTED; }
  bool Peak() { NOT_IMPLEMENTED; }
  void Set() { NOT_IMPLEMENTED; }
};

// The public interface of this class must remain identical to its equivalent
// in JobScheduler_posix.h
class WorkerThread {
public:
  explicit WorkerThread(MultiThreadedJobQueue* aJobQueue) { NOT_IMPLEMENTED; }
  void Run();
};

} // namespace
} // namespace

#endif
#endif
