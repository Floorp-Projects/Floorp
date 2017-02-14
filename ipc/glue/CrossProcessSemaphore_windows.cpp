/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

#include "base/process_util.h"
#include "CrossProcessSemaphore.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include "ProtocolUtils.h"

using base::GetCurrentProcessHandle;
using base::ProcessHandle;

namespace mozilla {

CrossProcessSemaphore::CrossProcessSemaphore(const char*, uint32_t aInitialValue)
{
  // We explicitly share this using DuplicateHandle, we do -not- want this to
  // be inherited by child processes by default! So no security attributes are
  // given.
  mSemaphore = ::CreateSemaphoreA(nullptr, aInitialValue, 0x7fffffff, nullptr);
  if (!mSemaphore) {
    MOZ_CRASH("This shouldn't happen - failed to create semaphore!");
  }
  MOZ_COUNT_CTOR(CrossProcessSemaphore);
}

CrossProcessSemaphore::CrossProcessSemaphore(CrossProcessSemaphoreHandle aHandle)
{
  DWORD flags;
  if (!::GetHandleInformation(aHandle, &flags)) {
    MOZ_CRASH("Attempt to construct a semaphore from an invalid handle!");
  }
  mSemaphore = aHandle;
  MOZ_COUNT_CTOR(CrossProcessSemaphore);
}

CrossProcessSemaphore::~CrossProcessSemaphore()
{
  MOZ_ASSERT(mSemaphore, "Improper construction of semaphore or double free.");
  ::CloseHandle(mSemaphore);
  MOZ_COUNT_DTOR(CrossProcessSemaphore);
}

bool
CrossProcessSemaphore::Wait(Maybe<TimeDuration> aWaitTime)
{
  MOZ_ASSERT(mSemaphore, "Improper construction of semaphore.");
  HRESULT hr = ::WaitForSingleObject(mSemaphore, aWaitTime.isSome() ?
                                                 aWaitTime->ToMilliseconds() :
                                                 INFINITE);
  return hr == WAIT_OBJECT_0;
}

void
CrossProcessSemaphore::Signal()
{
  MOZ_ASSERT(mSemaphore, "Improper construction of semaphore.");
  ::ReleaseSemaphore(mSemaphore, 1, nullptr);
}

CrossProcessSemaphoreHandle
CrossProcessSemaphore::ShareToProcess(base::ProcessId aTargetPid)
{
  HANDLE newHandle;
  bool succeeded = ipc::DuplicateHandle(mSemaphore, aTargetPid, &newHandle,
                                        0, DUPLICATE_SAME_ACCESS);

  if (!succeeded) {
    return nullptr;
  }

  return newHandle;
}

}
