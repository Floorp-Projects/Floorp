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

/* static */ CrossProcessSemaphore*
CrossProcessSemaphore::Create(const char*, uint32_t aInitialValue)
{
  // We explicitly share this using DuplicateHandle, we do -not- want this to
  // be inherited by child processes by default! So no security attributes are
  // given.
  HANDLE semaphore = ::CreateSemaphoreA(nullptr, aInitialValue, 0x7fffffff, nullptr);
  if (!semaphore) {
    return nullptr;
  }
  return new CrossProcessSemaphore(semaphore);
}

/* static */ CrossProcessSemaphore*
CrossProcessSemaphore::Create(CrossProcessSemaphoreHandle aHandle)
{
  DWORD flags;
  if (!::GetHandleInformation(aHandle, &flags)) {
    return nullptr;
  }

  return new CrossProcessSemaphore(aHandle);
}

CrossProcessSemaphore::CrossProcessSemaphore(HANDLE aSemaphore)
  : mSemaphore(aSemaphore)
{
  MOZ_COUNT_CTOR(CrossProcessSemaphore);
}

CrossProcessSemaphore::~CrossProcessSemaphore()
{
  MOZ_ASSERT(mSemaphore, "Improper construction of semaphore or double free.");
  ::CloseHandle(mSemaphore);
  MOZ_COUNT_DTOR(CrossProcessSemaphore);
}

bool
CrossProcessSemaphore::Wait(const Maybe<TimeDuration>& aWaitTime)
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
