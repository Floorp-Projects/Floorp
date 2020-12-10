/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_SERIALIZEDSTRUCTUREDCLONEBUFFER_H__
#define __IPC_GLUE_SERIALIZEDSTRUCTUREDCLONEBUFFER_H__

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_message_utils.h"
#include "js/AllocPolicy.h"
#include "js/StructuredClone.h"
#include "mozilla/Assertions.h"
#include "mozilla/BufferList.h"
#include "mozilla/Vector.h"
#include "mozilla/mozalloc.h"
class PickleIterator;

namespace mozilla {
template <typename...>
class Variant;

namespace detail {
template <typename...>
struct VariantTag;
}
}  // namespace mozilla

namespace mozilla {

struct SerializedStructuredCloneBuffer final {
  SerializedStructuredCloneBuffer() = default;

  SerializedStructuredCloneBuffer(SerializedStructuredCloneBuffer&&) = default;
  SerializedStructuredCloneBuffer& operator=(
      SerializedStructuredCloneBuffer&&) = default;

  SerializedStructuredCloneBuffer(const SerializedStructuredCloneBuffer&) =
      delete;
  SerializedStructuredCloneBuffer& operator=(
      const SerializedStructuredCloneBuffer& aOther) = delete;

  bool operator==(const SerializedStructuredCloneBuffer& aOther) const {
    // The copy assignment operator and the equality operator are
    // needed by the IPDL generated code. We relied on the copy
    // assignment operator at some places but we never use the
    // equality operator.
    return false;
  }

  JSStructuredCloneData data{JS::StructuredCloneScope::Unassigned};
};

}  // namespace mozilla

namespace IPC {
template <>
struct ParamTraits<JSStructuredCloneData> {
  typedef JSStructuredCloneData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    MOZ_ASSERT(!(aParam.Size() % sizeof(uint64_t)));
    WriteParam(aMsg, aParam.Size());
    aParam.ForEachDataChunk([&](const char* aData, size_t aSize) {
      return aMsg->WriteBytes(aData, aSize, sizeof(uint64_t));
    });
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    size_t length = 0;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }
    MOZ_ASSERT(!(length % sizeof(uint64_t)));

    mozilla::BufferList<InfallibleAllocPolicy> buffers(0, 0, 4096);

    // Borrowing is not suitable to use for IPC to hand out data
    // because we often want to store the data somewhere for
    // processing after IPC has released the underlying buffers. One
    // case is PContentChild::SendGetXPCOMProcessAttributes. We can't
    // return a borrowed buffer because the out param outlives the
    // IPDL callback.
    if (length &&
        !aMsg->ExtractBuffers(aIter, length, &buffers, sizeof(uint64_t))) {
      return false;
    }

    bool success;
    mozilla::BufferList<js::SystemAllocPolicy> out =
        buffers.MoveFallible<js::SystemAllocPolicy>(&success);
    if (!success) {
      return false;
    }

    *aResult = JSStructuredCloneData(
        std::move(out), JS::StructuredCloneScope::DifferentProcess);

    return true;
  }
};

template <>
struct ParamTraits<mozilla::SerializedStructuredCloneBuffer> {
  typedef mozilla::SerializedStructuredCloneBuffer paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.data);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->data);
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    LogParam(aParam.data.Size(), aLog);
  }
};

}  // namespace IPC

#endif /* __IPC_GLUE_SERIALIZEDSTRUCTUREDCLONEBUFFER_H__ */
