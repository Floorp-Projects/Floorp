/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A type that can be sent without needing to make a copy during
 * serialization. In addition the receiver can take ownership of the
 * data to avoid having to make an additional copy. */

#ifndef mozilla_ipc_ByteBuf_h
#define mozilla_ipc_ByteBuf_h

#include "ipc/IPCMessageUtils.h"

namespace mozilla {

namespace ipc {

class ByteBuf final
{
  friend struct IPC::ParamTraits<mozilla::ipc::ByteBuf>;
public:
  bool
  Allocate(size_t aLength)
  {
    MOZ_ASSERT(mData == nullptr);
    mData = (uint8_t*)malloc(aLength);
    if (!mData) {
      return false;
    }
    mLen = aLength;
    mCapacity = aLength;
    return true;
  }

  ByteBuf()
    : mData(nullptr)
    , mLen(0)
    , mCapacity(0)
  {}

  ByteBuf(uint8_t *aData, size_t aLen, size_t aCapacity)
    : mData(aData)
    , mLen(aLen)
    , mCapacity(aCapacity)
  {}

  ByteBuf(const ByteBuf& aFrom) = delete;

  ByteBuf(ByteBuf&& aFrom)
    : mData(aFrom.mData)
    , mLen(aFrom.mLen)
    , mCapacity(aFrom.mCapacity)
  {
    aFrom.mData = nullptr;
    aFrom.mLen = 0;
    aFrom.mCapacity = 0;
  }

  ~ByteBuf()
  {
    free(mData);
  }

  uint8_t* mData;
  size_t mLen;
  size_t mCapacity;
};


} // namespace ipc
} // namespace mozilla


namespace IPC {

template<>
struct ParamTraits<mozilla::ipc::ByteBuf>
{
  typedef mozilla::ipc::ByteBuf paramType;

  // this is where we transfer the memory from the ByteBuf to IPDL, avoiding a copy
  static void Write(Message* aMsg, paramType& aParam)
  {
    WriteParam(aMsg, aParam.mLen);
    // hand over ownership of the buffer to the Message
    aMsg->WriteBytesZeroCopy(aParam.mData, aParam.mLen, aParam.mCapacity);
    aParam.mData = nullptr;
    aParam.mCapacity = 0;
    aParam.mLen = 0;
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    // We make a copy from the BufferList so that we get a contigous result.
    // For users the can handle a non-contiguous result using ExtractBuffers
    // is an option, alternatively if the users don't need to take ownership of
    // the data they can use the removed FlattenBytes (bug 1297981)
    size_t length;
    return ReadParam(aMsg, aIter, &length)
      && aResult->Allocate(length)
      && aMsg->ReadBytesInto(aIter, aResult->mData, length);
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(L"(byte buf)");
  }
};


} // namespace IPC


#endif // ifndef mozilla_ipc_Shmem_h
