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
#include "mozilla/CheckedInt.h"
#include "mozilla/mozalloc_oom.h"
#include "ipc/IPCMessageUtils.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::ipc::ByteBuf> {
  typedef mozilla::ipc::ByteBuf paramType;

  // this is where we transfer the memory from the ByteBuf to IPDL, avoiding a
  // copy
  static void Write(MessageWriter* aWriter, paramType&& aParam) {
    // We need to send the length as a 32-bit value, not a size_t, because on
    // ARM64 Windows we end up with a 32-bit GMP process sending a ByteBuf to
    // a 64-bit parent process. WriteBytesZeroCopy takes a uint32_t as an
    // argument, so it would end up getting truncated anyways. See bug 1757534.
    mozilla::CheckedInt<uint32_t> length = aParam.mLen;
    MOZ_RELEASE_ASSERT(length.isValid());
    WriteParam(aWriter, length.value());
    // hand over ownership of the buffer to the Message
    aWriter->WriteBytesZeroCopy(aParam.mData, length.value(), aParam.mCapacity);
    aParam.mData = nullptr;
    aParam.mCapacity = 0;
    aParam.mLen = 0;
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    // We make a copy from the BufferList so that we get a contigous result.
    uint32_t length;
    if (!ReadParam(aReader, &length)) return false;
    if (!aResult->Allocate(length)) {
      mozalloc_handle_oom(length);
      return false;
    }
    return aReader->ReadBytesInto(aResult->mData, length);
  }
};

}  // namespace IPC

#endif  // ifndef mozilla_ipc_ByteBufUtils_h
