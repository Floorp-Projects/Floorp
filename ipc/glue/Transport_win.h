/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Transport_win_h
#define mozilla_ipc_Transport_win_h 1

#include <string>

#include "base/process.h"
#include "ipc/IPCMessageUtils.h"


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

    WriteParam(aMsg, aParam.mPipeName);
    WriteParam(aMsg, pipe);
    WriteParam(aMsg, aParam.mDestinationProcessId);
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool r = (ReadParam(aMsg, aIter, &aResult->mPipeName) &&
              ReadParam(aMsg, aIter, &aResult->mServerPipeHandle) &&
              ReadParam(aMsg, aIter, &aResult->mDestinationProcessId));
    if (!r) {
      return r;
    }
    if (aResult->mServerPipeHandle != INVALID_HANDLE_VALUE) {
      MOZ_RELEASE_ASSERT(aResult->mDestinationProcessId == base::GetCurrentProcId());
    }
    return r;
  }
};

} // namespace IPC


#endif  // mozilla_ipc_Transport_win_h
