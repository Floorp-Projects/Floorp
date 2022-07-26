/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_BigBuffer_h
#define mozilla_ipc_BigBuffer_h

#include <stdlib.h>
#include <inttypes.h>
#include "mozilla/Span.h"
#include "mozilla/Variant.h"
#include "mozilla/ipc/SharedMemory.h"

namespace mozilla::ipc {

class BigBuffer {
 public:
  static constexpr size_t kShmemThreshold = 64 * 1024;

  // Return a new BigBuffer which wraps no data.
  BigBuffer() : mSize(0), mData(NoData()) {}

  BigBuffer(const BigBuffer&) = delete;
  BigBuffer& operator=(const BigBuffer&) = delete;

  BigBuffer(BigBuffer&& aOther) noexcept
      : mSize(std::exchange(aOther.mSize, 0)),
        mData(std::exchange(aOther.mData, NoData())) {}

  BigBuffer& operator=(BigBuffer&& aOther) noexcept {
    mSize = std::exchange(aOther.mSize, 0);
    mData = std::exchange(aOther.mData, NoData());
    return *this;
  }

  // Create a new BigBuffer with the given size.
  // The buffer will be created uninitialized and must be fully initialized
  // before sending over IPC to avoid leaking uninitialized memory to another
  // process.
  explicit BigBuffer(size_t aSize) : mSize(aSize), mData(AllocBuffer(aSize)) {}

  // Create a new BigBuffer containing the data from the provided byte slice.
  explicit BigBuffer(Span<const uint8_t> aData) : BigBuffer(aData.Length()) {
    memcpy(Data(), aData.Elements(), aData.Length());
  }

  // Marker to indicate that a particular constructor of BigBuffer adopts
  // ownership of the provided data.
  struct Adopt {};

  // Create a new BigBuffer from an existing shared memory region, taking
  // ownership of that shared memory region. The shared memory region must be
  // non-null, mapped, and large enough to fit aSize bytes.
  BigBuffer(Adopt, SharedMemory* aSharedMemory, size_t aSize);

  // Create a new BigBuffer from an existing memory buffer, taking ownership of
  // that memory region. The region will be freed using `free()` when it is no
  // longer needed.
  BigBuffer(Adopt, uint8_t* aData, size_t aSize);

  ~BigBuffer() = default;

  // Returns a pointer to the data stored by this BigBuffer, regardless of
  // backing storage type.
  uint8_t* Data();
  const uint8_t* Data() const;

  // Returns the size of the data stored by this BigBuffer, regardless of
  // backing storage type.
  size_t Size() const { return mSize; }

  // Get a view of the BigBuffer's data as a span.
  Span<uint8_t> AsSpan() { return Span{Data(), Size()}; }
  Span<const uint8_t> AsSpan() const { return Span{Data(), Size()}; }

  // If the BigBuffer is backed by shared memory, returns a pointer to the
  // backing SharedMemory region.
  SharedMemory* GetSharedMemory() const {
    return mData.is<1>() ? mData.as<1>().get() : nullptr;
  }

 private:
  friend struct IPC::ParamTraits<mozilla::ipc::BigBuffer>;

  using Storage = Variant<UniqueFreePtr<uint8_t[]>, RefPtr<SharedMemory>>;

  // Empty storage which holds no data.
  static Storage NoData() { return AsVariant(UniqueFreePtr<uint8_t[]>{}); }

  // Infallibly allocate a new storage of the given size.
  static Storage AllocBuffer(size_t aSize);

  size_t mSize;
  Storage mData;
};

}  // namespace mozilla::ipc

namespace IPC {

template <>
struct ParamTraits<mozilla::ipc::BigBuffer> {
  using paramType = mozilla::ipc::BigBuffer;
  static void Write(MessageWriter* aWriter, paramType&& aParam);
  static bool Read(MessageReader* aReader, paramType* aResult);
};

}  // namespace IPC

#endif  // mozilla_BigBuffer_h
