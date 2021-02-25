/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A type that can be sent without needing to make a copy during
 * serialization. In addition the receiver can take ownership of the
 * data to avoid having to make an additional copy. */

#ifndef mozilla_ipc_ByteBufUtils_h
#define mozilla_ipc_ByteBufUtils_h

#include "mozilla/ipc/ByteBuf.h"
#include "mozilla/mozalloc_oom.h"
#include "ipc/IPCMessageUtils.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::ipc::ByteBuf> {
  typedef mozilla::ipc::ByteBuf paramType;

  // this is where we transfer the memory from the ByteBuf to IPDL, avoiding a
  // copy
  static void Write(Message* aMsg, paramType&& aParam) {
    WriteParam(aMsg, aParam.mLen);
    // hand over ownership of the buffer to the Message
    aMsg->WriteBytesZeroCopy(aParam.mData, aParam.mLen, aParam.mCapacity);
    aParam.mData = nullptr;
    aParam.mCapacity = 0;
    aParam.mLen = 0;
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    // We make a copy from the BufferList so that we get a contigous result.
    // For users the can handle a non-contiguous result using ExtractBuffers
    // is an option, alternatively if the users don't need to take ownership of
    // the data they can use the removed FlattenBytes (bug 1297981)
    size_t length;
    if (!ReadParam(aMsg, aIter, &length)) return false;
    if (!aResult->Allocate(length)) {
      mozalloc_handle_oom(length);
      return false;
    }
    return aMsg->ReadBytesInto(aIter, aResult->mData, length);
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    aLog->append(L"(byte buf)");
  }
};

}  // namespace IPC

#endif  // ifndef mozilla_ipc_ByteBufUtils_h
