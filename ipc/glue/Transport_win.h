/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Transport_win_h
#define mozilla_ipc_Transport_win_h 1

#include <string>

#include "base/process.h"
#include "ipc/IPCMessageUtils.h"
#include "nsWindowsHelpers.h"

namespace mozilla {
namespace ipc {

struct TransportDescriptor
{
  std::wstring mPipeName;
  HANDLE mServerPipeHandle;
  base::ProcessId mDestinationProcessId;
};

HANDLE
TransferHandleToProcess(HANDLE source, base::ProcessId pid);

} // namespace ipc
} // namespace mozilla


namespace IPC {

template<>
struct ParamTraits<mozilla::ipc::TransportDescriptor>
{
  typedef mozilla::ipc::TransportDescriptor paramType;
  static void Write(Message* aMsg, const paramType& aParam)
  {
    HANDLE pipe = mozilla::ipc::TransferHandleToProcess(aParam.mServerPipeHandle,
                                                        aParam.mDestinationProcessId);
    DWORD duplicateFromProcessId = 0;
    if (!pipe) {
      if (XRE_IsParentProcess()) {
        // If we are the parent and failed to transfer then there is no hope,
        // just close the handle.
        ::CloseHandle(aParam.mServerPipeHandle);
      } else {
        // We are probably sending to parent so it should be able to duplicate.
        pipe = aParam.mServerPipeHandle;
        duplicateFromProcessId = ::GetCurrentProcessId();
      }
    }

    WriteParam(aMsg, aParam.mPipeName);
    WriteParam(aMsg, pipe);
    WriteParam(aMsg, duplicateFromProcessId);
    WriteParam(aMsg, aParam.mDestinationProcessId);
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    DWORD duplicateFromProcessId;
    bool r = (ReadParam(aMsg, aIter, &aResult->mPipeName) &&
              ReadParam(aMsg, aIter, &aResult->mServerPipeHandle) &&
              ReadParam(aMsg, aIter, &duplicateFromProcessId) &&
              ReadParam(aMsg, aIter, &aResult->mDestinationProcessId));
    if (!r) {
      return r;
    }

    MOZ_RELEASE_ASSERT(aResult->mServerPipeHandle,
                       "Main process failed to duplicate pipe handle to child.");

    // If this is a not the "server" side descriptor, we have finished.
    if (aResult->mServerPipeHandle == INVALID_HANDLE_VALUE) {
      return true;
    }

    MOZ_RELEASE_ASSERT(aResult->mDestinationProcessId == base::GetCurrentProcId());

    // If the pipe has already been duplicated to us, we have finished.
    if (!duplicateFromProcessId) {
      return true;
    }

    // Otherwise duplicate the handle to us.
    nsAutoHandle sourceProcess(::OpenProcess(PROCESS_DUP_HANDLE, FALSE,
                                             duplicateFromProcessId));
    if (!sourceProcess) {
      return false;
    }

    HANDLE ourHandle;
    BOOL duped = ::DuplicateHandle(sourceProcess, aResult->mServerPipeHandle,
                                   ::GetCurrentProcess(), &ourHandle, 0, FALSE,
                                   DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS);
    if (!duped) {
      aResult->mServerPipeHandle = INVALID_HANDLE_VALUE;
      return false;
    }

    aResult->mServerPipeHandle = ourHandle;
    return true;
  }
};

} // namespace IPC


#endif  // mozilla_ipc_Transport_win_h
