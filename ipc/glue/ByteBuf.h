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

#include "mozilla/Assertions.h"

namespace IPC {
template <typename T>
struct ParamTraits;
}

namespace mozilla {

namespace ipc {

class ByteBuf final {
  friend struct IPC::ParamTraits<mozilla::ipc::ByteBuf>;

 public:
  bool Allocate(size_t aLength) {
    MOZ_ASSERT(mData == nullptr);
    mData = (uint8_t*)malloc(aLength);
    if (!mData) {
      return false;
    }
    mLen = aLength;
    mCapacity = aLength;
    return true;
  }

  ByteBuf() : mData(nullptr), mLen(0), mCapacity(0) {}

  ByteBuf(uint8_t* aData, size_t aLen, size_t aCapacity)
      : mData(aData), mLen(aLen), mCapacity(aCapacity) {}

  ByteBuf(const ByteBuf& aFrom) = delete;

  ByteBuf(ByteBuf&& aFrom)
      : mData(aFrom.mData), mLen(aFrom.mLen), mCapacity(aFrom.mCapacity) {
    aFrom.mData = nullptr;
    aFrom.mLen = 0;
    aFrom.mCapacity = 0;
  }

  ByteBuf& operator=(ByteBuf&& aFrom) {
    std::swap(mData, aFrom.mData);
    std::swap(mLen, aFrom.mLen);
    std::swap(mCapacity, aFrom.mCapacity);
    return *this;
  }

  ~ByteBuf() { free(mData); }

  uint8_t* mData;
  size_t mLen;
  size_t mCapacity;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // ifndef mozilla_ipc_ByteBuf_h
