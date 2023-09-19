/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CrossProcessSemaphore_h
#define mozilla_CrossProcessSemaphore_h

#include "base/process.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Maybe.h"

#if defined(XP_WIN) || defined(XP_DARWIN)
#  include "mozilla/UniquePtrExtensions.h"
#else
#  include <pthread.h>
#  include <semaphore.h>
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
//  - CrossProcessSemaphore, a semaphore that can be shared across processes
namespace mozilla {

template <typename T>
inline bool IsHandleValid(const T& handle) {
  return bool(handle);
}

#if defined(XP_WIN)
typedef mozilla::UniqueFileHandle CrossProcessSemaphoreHandle;
#elif defined(XP_DARWIN)
typedef mozilla::UniqueMachSendRight CrossProcessSemaphoreHandle;
#else
typedef mozilla::ipc::SharedMemoryBasic::Handle CrossProcessSemaphoreHandle;

template <>
inline bool IsHandleValid<CrossProcessSemaphoreHandle>(
    const CrossProcessSemaphoreHandle& handle) {
  return !(handle == mozilla::ipc::SharedMemoryBasic::NULLHandle());
}
#endif

class CrossProcessSemaphore {
 public:
  /**
   * CrossProcessSemaphore
   * @param name A name which can reference this lock (currently unused)
   **/
  static CrossProcessSemaphore* Create(const char* aName,
                                       uint32_t aInitialValue);

  /**
   * CrossProcessSemaphore
   * @param handle A handle of an existing cross process semaphore that can be
   *               opened.
   */
  static CrossProcessSemaphore* Create(CrossProcessSemaphoreHandle aHandle);

  ~CrossProcessSemaphore();

  /**
   * Decrements the current value of the semaphore. This will block if the
   * current value of the semaphore is 0, up to a maximum of aWaitTime (if
   * specified).
   * Returns true if the semaphore was succesfully decremented, false otherwise.
   **/
  bool Wait(const Maybe<TimeDuration>& aWaitTime = Nothing());

  /**
   * Increments the current value of the semaphore.
   **/
  void Signal();

  /**
   * CloneHandle
   * This function is called to generate a serializable structure that can
   * be sent to the specified process and opened on the other side.
   *
   * @returns A handle that can be shared to another process
   */
  CrossProcessSemaphoreHandle CloneHandle();

  void CloseHandle();

 private:
  friend struct IPC::ParamTraits<CrossProcessSemaphore>;

  CrossProcessSemaphore();
  CrossProcessSemaphore(const CrossProcessSemaphore&);
  CrossProcessSemaphore& operator=(const CrossProcessSemaphore&);

#if defined(XP_WIN)
  explicit CrossProcessSemaphore(HANDLE aSemaphore);

  HANDLE mSemaphore;
#elif defined(XP_DARWIN)
  explicit CrossProcessSemaphore(CrossProcessSemaphoreHandle aSemaphore);

  CrossProcessSemaphoreHandle mSemaphore;
#else
  RefPtr<mozilla::ipc::SharedMemoryBasic> mSharedBuffer;
  sem_t* mSemaphore;
  mozilla::Atomic<int32_t>* mRefCount;
#endif
};

}  // namespace mozilla

#endif
