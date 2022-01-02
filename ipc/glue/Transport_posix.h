/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Transport_posix_h
#define mozilla_ipc_Transport_posix_h 1

#include "ipc/IPCMessageUtils.h"

namespace mozilla {
namespace ipc {

struct TransportDescriptor {
  int mFd;
};

}  // namespace ipc
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::ipc::TransportDescriptor> {
  typedef mozilla::ipc::TransportDescriptor paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, mozilla::UniqueFileHandle{aParam.mFd});
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    mozilla::UniqueFileHandle fd;
    if (!ReadParam(aMsg, aIter, &fd)) {
      return false;
    }
    aResult->mFd = fd.release();
    return true;
  }
};

}  // namespace IPC

#endif  // mozilla_ipc_Transport_posix_h
