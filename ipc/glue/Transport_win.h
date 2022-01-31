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
#include "nsXULAppAPI.h"
#include "mozilla/UniquePtrExtensions.h"

namespace mozilla {
namespace ipc {

struct TransportDescriptor {
  std::wstring mPipeName;
  HANDLE mServerPipeHandle;
  base::ProcessId mDestinationProcessId;
};

}  // namespace ipc
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::ipc::TransportDescriptor> {
  typedef mozilla::ipc::TransportDescriptor paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mPipeName);
    WriteParam(aMsg, mozilla::UniqueFileHandle(aParam.mServerPipeHandle));
    WriteParam(aMsg, aParam.mDestinationProcessId);
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    mozilla::UniqueFileHandle serverPipeHandle;
    bool r = (ReadParam(aMsg, aIter, &aResult->mPipeName) &&
              ReadParam(aMsg, aIter, &serverPipeHandle) &&
              ReadParam(aMsg, aIter, &aResult->mDestinationProcessId));
    if (!r) {
      return r;
    }

    if (serverPipeHandle) {
      aResult->mServerPipeHandle = serverPipeHandle.release();
    } else {
      aResult->mServerPipeHandle = INVALID_HANDLE_VALUE;
    }
    return true;
  }
};

}  // namespace IPC

#endif  // mozilla_ipc_Transport_win_h
