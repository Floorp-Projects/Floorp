/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CrossProcessMutex_h
#define mozilla_CrossProcessMutex_h

#include "base/process.h"
#include "mozilla/Mutex.h"

#if defined(XP_WIN)
#  include "mozilla/UniquePtrExtensions.h"
#endif
#if !defined(XP_WIN) && !defined(XP_NETBSD) && !defined(XP_OPENBSD)
#  include <pthread.h>
#  include "mozilla/ipc/SharedMemoryBasic.h"
#  include "mozilla/Atomics.h"
#endif

namespace IPC {
template <typename T>
struct ParamTraits;
}  // namespace IPC

//
// Provides:
//
//  - CrossProcessMutex, a non-recursive mutex that can be shared across
//    processes
//  - CrossProcessMutexAutoLock, an RAII class for ensuring that Mutexes are
//    properly locked and unlocked
//
// Using CrossProcessMutexAutoLock/CrossProcessMutexAutoUnlock is MUCH
// preferred to making bare calls to CrossProcessMutex.Lock and Unlock.
//
namespace mozilla {
#if defined(XP_WIN)
typedef mozilla::UniqueFileHandle CrossProcessMutexHandle;
#elif !defined(XP_NETBSD) && !defined(XP_OPENBSD)
typedef mozilla::ipc::SharedMemoryBasic::Handle CrossProcessMutexHandle;
#else
// Stub for other platforms. We can't use uintptr_t here since different
// processes could disagree on its size.
typedef uintptr_t CrossProcessMutexHandle;
#endif

class CrossProcessMutex {
 public:
  /**
   * CrossProcessMutex
   * @param name A name which can reference this lock (currently unused)
   **/
  explicit CrossProcessMutex(const char* aName);
  /**
   * CrossProcessMutex
   * @param handle A handle of an existing cross process mutex that can be
   *               opened.
   */
  explicit CrossProcessMutex(CrossProcessMutexHandle aHandle);

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
   * CloneHandle
   * This function is called to generate a serializable structure that can
   * be sent to the specified process and opened on the other side.
   *
   * @returns A handle that can be shared to another process
   */
  CrossProcessMutexHandle CloneHandle();

 private:
  friend struct IPC::ParamTraits<CrossProcessMutex>;

  CrossProcessMutex();
  CrossProcessMutex(const CrossProcessMutex&);
  CrossProcessMutex& operator=(const CrossProcessMutex&);

#if defined(XP_WIN)
  HANDLE mMutex;
#elif !defined(XP_NETBSD) && !defined(XP_OPENBSD)
  RefPtr<mozilla::ipc::SharedMemoryBasic> mSharedBuffer;
  pthread_mutex_t* mMutex;
  mozilla::Atomic<int32_t>* mCount;
#endif
};

typedef detail::BaseAutoLock<CrossProcessMutex&> CrossProcessMutexAutoLock;
typedef detail::BaseAutoUnlock<CrossProcessMutex&> CrossProcessMutexAutoUnlock;

}  // namespace mozilla

#endif
