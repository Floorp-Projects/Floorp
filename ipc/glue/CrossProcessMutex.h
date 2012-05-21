/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CrossProcessMutex_h
#define mozilla_CrossProcessMutex_h

#include "base/process.h"
#include "mozilla/Mutex.h"

namespace IPC {
template<typename T>
struct ParamTraits;
}

//
// Provides:
//
//  - CrossProcessMutex, a non-recursive mutex that can be shared across processes
//  - CrossProcessMutexAutoLock, an RAII class for ensuring that Mutexes are
//    properly locked and unlocked
//
// Using CrossProcessMutexAutoLock/CrossProcessMutexAutoUnlock is MUCH
// preferred to making bare calls to CrossProcessMutex.Lock and Unlock.
//
namespace mozilla {
#ifdef XP_WIN
typedef HANDLE CrossProcessMutexHandle;
#else
// Stub for other platforms. We can't use uintptr_t here since different
// processes could disagree on its size.
typedef uintptr_t CrossProcessMutexHandle;
#endif

class NS_COM_GLUE CrossProcessMutex
{
public:
  /**
   * CrossProcessMutex
   * @param name A name which can reference this lock (currently unused)
   **/
  CrossProcessMutex(const char* aName);
  /**
   * CrossProcessMutex
   * @param handle A handle of an existing cross process mutex that can be
   *               opened.
   */
  CrossProcessMutex(CrossProcessMutexHandle aHandle);

  /**
   * ~CrossProcessMutex
   **/
  ~CrossProcessMutex();

  /**
   * Lock
   * This will lock the mutex. Any other thread in any other process that
   * has access to this mutex calling lock will block execution until the
   * initial caller of lock has made a call to Unlock.
   *
   * If the owning process is terminated unexpectedly the mutex will be
   * released.
   **/
  void Lock();

  /**
   * Unlock
   * This will unlock the mutex. A single thread currently waiting on a lock
   * call will resume execution and aquire ownership of the lock. No
   * guarantees are made as to the order in which waiting threads will resume
   * execution.
   **/
  void Unlock();

  /**
   * ShareToProcess
   * This function is called to generate a serializable structure that can
   * be sent to the specified process and opened on the other side.
   *
   * @returns A handle that can be shared to another process
   */
  CrossProcessMutexHandle ShareToProcess(base::ProcessHandle aTarget);

private:
  friend struct IPC::ParamTraits<CrossProcessMutex>;

  CrossProcessMutex();
  CrossProcessMutex(const CrossProcessMutex&);
  CrossProcessMutex &operator=(const CrossProcessMutex&);

#ifdef XP_WIN
  HANDLE mMutex;
#endif
};

typedef BaseAutoLock<CrossProcessMutex> CrossProcessMutexAutoLock;
typedef BaseAutoUnlock<CrossProcessMutex> CrossProcessMutexAutoUnlock;

}
#endif
