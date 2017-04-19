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

#if !defined(OS_WIN) && !defined(OS_MACOSX)
#include <pthread.h>
#include <semaphore.h>
#include "SharedMemoryBasic.h"
#include "mozilla/Atomics.h"
#endif

namespace IPC {
template<typename T>
struct ParamTraits;
} // namespace IPC

//
// Provides:
//
//  - CrossProcessSemaphore, a semaphore that can be shared across processes
namespace mozilla {
#if defined(OS_WIN)
typedef HANDLE CrossProcessSemaphoreHandle;
#elif !defined(OS_MACOSX)
typedef mozilla::ipc::SharedMemoryBasic::Handle CrossProcessSemaphoreHandle;
#else
// Stub for other platforms. We can't use uintptr_t here since different
// processes could disagree on its size.
typedef uintptr_t CrossProcessSemaphoreHandle;
#endif

class CrossProcessSemaphore
{
public:
  /**
   * CrossProcessSemaphore
   * @param name A name which can reference this lock (currently unused)
   **/
  static CrossProcessSemaphore* Create(const char* aName, uint32_t aInitialValue);

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
   * ShareToProcess
   * This function is called to generate a serializable structure that can
   * be sent to the specified process and opened on the other side.
   *
   * @returns A handle that can be shared to another process
   */
  CrossProcessSemaphoreHandle ShareToProcess(base::ProcessId aTargetPid);

private:
  friend struct IPC::ParamTraits<CrossProcessSemaphore>;

  CrossProcessSemaphore();
  CrossProcessSemaphore(const CrossProcessSemaphore&);
  CrossProcessSemaphore &operator=(const CrossProcessSemaphore&);

#if defined(OS_WIN)
  explicit CrossProcessSemaphore(HANDLE aSemaphore);

  HANDLE mSemaphore;
#elif !defined(OS_MACOSX)
  RefPtr<mozilla::ipc::SharedMemoryBasic> mSharedBuffer;
  sem_t* mSemaphore;
  mozilla::Atomic<int32_t>* mRefCount;
#endif
};

} // namespace mozilla

#endif
