/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

#include "base/process_util.h"
#include "CrossProcessMutex.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"

using namespace base;

namespace mozilla {

CrossProcessMutex::CrossProcessMutex(const char*)
{
  // We explicitly share this using DuplicateHandle, we do -not- want this to
  // be inherited by child processes by default! So no security attributes are
  // given.
  mMutex = ::CreateMutexA(nullptr, FALSE, nullptr);
  if (!mMutex) {
    NS_RUNTIMEABORT("This shouldn't happen - failed to create mutex!");
  }
  MOZ_COUNT_CTOR(CrossProcessMutex);
}

CrossProcessMutex::CrossProcessMutex(CrossProcessMutexHandle aHandle)
{
  DWORD flags;
  if (!::GetHandleInformation(aHandle, &flags)) {
    NS_RUNTIMEABORT("Attempt to construct a mutex from an invalid handle!");
  }
  mMutex = aHandle;
  MOZ_COUNT_CTOR(CrossProcessMutex);
}

CrossProcessMutex::~CrossProcessMutex()
{
  NS_ASSERTION(mMutex, "Improper construction of mutex or double free.");
  ::CloseHandle(mMutex);
  MOZ_COUNT_DTOR(CrossProcessMutex);
}

void
CrossProcessMutex::Lock()
{
  NS_ASSERTION(mMutex, "Improper construction of mutex.");
  ::WaitForSingleObject(mMutex, INFINITE);
}

void
CrossProcessMutex::Unlock()
{
  NS_ASSERTION(mMutex, "Improper construction of mutex.");
  ::ReleaseMutex(mMutex);
}

CrossProcessMutexHandle
CrossProcessMutex::ShareToProcess(ProcessHandle aHandle)
{
  HANDLE newHandle;
  bool succeeded = ::DuplicateHandle(GetCurrentProcessHandle(),
                                     mMutex, aHandle, &newHandle,
                                     0, FALSE, DUPLICATE_SAME_ACCESS);

  if (!succeeded) {
    return nullptr;
  }

  return newHandle;
}

}
