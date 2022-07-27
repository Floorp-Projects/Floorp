/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/BigBuffer.h"

#include "mozilla/ipc/SharedMemoryBasic.h"
#include "nsDebug.h"

namespace mozilla::ipc {

BigBuffer::BigBuffer(Adopt, SharedMemory* aSharedMemory, size_t aSize)
    : mSize(aSize), mData(AsVariant(RefPtr{aSharedMemory})) {
  MOZ_RELEASE_ASSERT(aSharedMemory && aSharedMemory->memory(),
                     "shared memory must be non-null and mapped");
  MOZ_RELEASE_ASSERT(mSize <= aSharedMemory->Size(),
                     "shared memory region isn't large enough");
}

BigBuffer::BigBuffer(Adopt, uint8_t* aData, size_t aSize)
    : mSize(aSize), mData(AsVariant(UniqueFreePtr<uint8_t[]>{aData})) {}

uint8_t* BigBuffer::Data() {
  return mData.is<0>() ? mData.as<0>().get()
                       : reinterpret_cast<uint8_t*>(mData.as<1>()->memory());
}
const uint8_t* BigBuffer::Data() const {
  return mData.is<0>()
             ? mData.as<0>().get()
             : reinterpret_cast<const uint8_t*>(mData.as<1>()->memory());
}

auto BigBuffer::AllocBuffer(size_t aSize) -> Storage {
  if (aSize <= kShmemThreshold) {
    return AsVariant(UniqueFreePtr<uint8_t[]>{
        reinterpret_cast<uint8_t*>(moz_xmalloc(aSize))});
  }

  RefPtr<SharedMemory> shmem = new SharedMemoryBasic();
  size_t capacity = SharedMemory::PageAlignedSize(aSize);
  if (!shmem->Create(capacity) || !shmem->Map(capacity)) {
    NS_ABORT_OOM(capacity);
  }
  return AsVariant(shmem);
}

}  // namespace mozilla::ipc

void IPC::ParamTraits<mozilla::ipc::BigBuffer>::Write(MessageWriter* aWriter,
                                                      paramType&& aParam) {
  using namespace mozilla::ipc;
  size_t size = std::exchange(aParam.mSize, 0);
  auto data = std::exchange(aParam.mData, BigBuffer::NoData());

  WriteParam(aWriter, size);
  bool isShmem = data.is<1>();
  WriteParam(aWriter, isShmem);

  if (isShmem) {
    if (!data.as<1>()->WriteHandle(aWriter)) {
      aWriter->FatalError("Failed to write data shmem");
    }
  } else {
    aWriter->WriteBytes(data.as<0>().get(), size);
  }
}

bool IPC::ParamTraits<mozilla::ipc::BigBuffer>::Read(MessageReader* aReader,
                                                     paramType* aResult) {
  using namespace mozilla::ipc;
  size_t size = 0;
  bool isShmem = false;
  if (!ReadParam(aReader, &size) || !ReadParam(aReader, &isShmem)) {
    aReader->FatalError("Failed to read data size and format");
    return false;
  }

  if (isShmem) {
    RefPtr<SharedMemory> shmem = new SharedMemoryBasic();
    size_t capacity = SharedMemory::PageAlignedSize(size);
    if (!shmem->ReadHandle(aReader) || !shmem->Map(capacity)) {
      aReader->FatalError("Failed to read data shmem");
      return false;
    }
    *aResult = BigBuffer(BigBuffer::Adopt{}, shmem, size);
    return true;
  }

  mozilla::UniqueFreePtr<uint8_t[]> buf{
      reinterpret_cast<uint8_t*>(malloc(size))};
  if (!buf) {
    aReader->FatalError("Failed to allocate data buffer");
    return false;
  }
  if (!aReader->ReadBytesInto(buf.get(), size)) {
    aReader->FatalError("Failed to read data");
    return false;
  }
  *aResult = BigBuffer(BigBuffer::Adopt{}, buf.release(), size);
  return true;
}
