/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RawShmem.h"
#include "mozilla/ipc/ProtocolUtils.h"

namespace mozilla::ipc {

UnsafeSharedMemoryHandle::UnsafeSharedMemoryHandle()
    : mHandle(ipc::SharedMemoryBasic::NULLHandle()), mSize(0) {}

UnsafeSharedMemoryHandle::UnsafeSharedMemoryHandle(
    UnsafeSharedMemoryHandle&& aOther) noexcept
    : mHandle(std::move(aOther.mHandle)), mSize(aOther.mSize) {
  aOther.mHandle = ipc::SharedMemoryBasic::NULLHandle();
  aOther.mSize = 0;
}

UnsafeSharedMemoryHandle& UnsafeSharedMemoryHandle::operator=(
    UnsafeSharedMemoryHandle&& aOther) noexcept {
  if (this == &aOther) {
    return *this;
  }

  mHandle = std::move(aOther.mHandle);
  mSize = aOther.mSize;
  aOther.mHandle = ipc::SharedMemoryBasic::NULLHandle();
  aOther.mSize = 0;
  return *this;
}

Maybe<std::pair<UnsafeSharedMemoryHandle, WritableSharedMemoryMapping>>
UnsafeSharedMemoryHandle::CreateAndMap(size_t aSize) {
  if (aSize == 0) {
    return Some(std::make_pair(UnsafeSharedMemoryHandle(),
                               WritableSharedMemoryMapping()));
  }

  RefPtr<ipc::SharedMemoryBasic> shm = MakeAndAddRef<ipc::SharedMemoryBasic>();
  if (NS_WARN_IF(!shm->Create(aSize)) || NS_WARN_IF(!shm->Map(aSize))) {
    return Nothing();
  }

  auto handle = shm->TakeHandle();

  auto size = shm->Size();

  return Some(std::make_pair(UnsafeSharedMemoryHandle(std::move(handle), size),
                             WritableSharedMemoryMapping(std::move(shm))));
}

WritableSharedMemoryMapping::WritableSharedMemoryMapping(
    RefPtr<ipc::SharedMemoryBasic>&& aRef)
    : mRef(aRef) {}

Maybe<WritableSharedMemoryMapping> WritableSharedMemoryMapping::Open(
    UnsafeSharedMemoryHandle aHandle) {
  if (aHandle.mSize == 0) {
    return Some(WritableSharedMemoryMapping(nullptr));
  }

  RefPtr<ipc::SharedMemoryBasic> shm = MakeAndAddRef<ipc::SharedMemoryBasic>();
  if (NS_WARN_IF(!shm->SetHandle(std::move(aHandle.mHandle),
                                 ipc::SharedMemory::RightsReadWrite)) ||
      NS_WARN_IF(!shm->Map(aHandle.mSize))) {
    return Nothing();
  }

  shm->CloseHandle();

  return Some(WritableSharedMemoryMapping(std::move(shm)));
}

size_t WritableSharedMemoryMapping::Size() const {
  if (!mRef) {
    return 0;
  }

  return mRef->Size();
}

Span<uint8_t> WritableSharedMemoryMapping::Bytes() {
  if (!mRef) {
    return Span<uint8_t>();
  }

  uint8_t* mem = static_cast<uint8_t*>(mRef->memory());
  return Span(mem, mRef->Size());
}

}  // namespace mozilla::ipc

namespace IPC {
auto ParamTraits<mozilla::ipc::UnsafeSharedMemoryHandle>::Write(
    IPC::MessageWriter* aWriter, paramType&& aVar) -> void {
  IPC::WriteParam(aWriter, std::move(aVar.mHandle));
  IPC::WriteParam(aWriter, aVar.mSize);
}

auto ParamTraits<mozilla::ipc::UnsafeSharedMemoryHandle>::Read(
    IPC::MessageReader* aReader, paramType* aVar) -> bool {
  return IPC::ReadParam(aReader, &aVar->mHandle) &&
         IPC::ReadParam(aReader, &aVar->mSize);
}

}  // namespace IPC
