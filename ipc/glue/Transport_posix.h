/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Transport_posix_h
#define mozilla_ipc_Transport_posix_h 1

#include "ipc/IPCMessageUtils.h"


namespace mozilla {
namespace ipc {

struct TransportDescriptor
{
  base::FileDescriptor mFd;
};

} // namespace ipc
} // namespace mozilla


namespace IPC {

template<>
struct ParamTraits<mozilla::ipc::TransportDescriptor>
{
  typedef mozilla::ipc::TransportDescriptor paramType;
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mFd);
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mFd);
  }
};

} // namespace IPC


#endif  // mozilla_ipc_Transport_posix_h
