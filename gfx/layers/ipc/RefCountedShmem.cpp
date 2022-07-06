/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RefCountedShmem.h"

#include "mozilla/Atomics.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/WebRenderMessages.h"

#define SHM_REFCOUNT_HEADER_SIZE 16

namespace mozilla {
namespace layers {

uint8_t* RefCountedShm::GetBytes(const RefCountedShmem& aShm) {
  uint8_t* data = aShm.buffer().get<uint8_t>();
  if (!data) {
    return nullptr;
  }
  return data + SHM_REFCOUNT_HEADER_SIZE;
}

size_t RefCountedShm::GetSize(const RefCountedShmem& aShm) {
  if (!IsValid(aShm)) {
    return 0;
  }
  size_t totalSize = aShm.buffer().Size<uint8_t>();
  if (totalSize < SHM_REFCOUNT_HEADER_SIZE) {
    return 0;
  }

  return totalSize - SHM_REFCOUNT_HEADER_SIZE;
}

bool RefCountedShm::IsValid(const RefCountedShmem& aShm) {
  return aShm.buffer().IsWritable() &&
         aShm.buffer().Size<uint8_t>() > SHM_REFCOUNT_HEADER_SIZE;
}

int32_t RefCountedShm::GetReferenceCount(const RefCountedShmem& aShm) {
  if (!IsValid(aShm)) {
    return 0;
  }

  return *aShm.buffer().get<Atomic<int32_t>>();
}

int32_t RefCountedShm::AddRef(const RefCountedShmem& aShm) {
  if (!IsValid(aShm)) {
    return 0;
  }

  auto* counter = aShm.buffer().get<Atomic<int32_t>>();
  if (counter) {
    return (*counter)++;
  }
  return 0;
}

int32_t RefCountedShm::Release(const RefCountedShmem& aShm) {
  if (!IsValid(aShm)) {
    return 0;
  }

  auto* counter = aShm.buffer().get<Atomic<int32_t>>();
  if (counter) {
    return --(*counter);
  }

  return 0;
}

bool RefCountedShm::Alloc(mozilla::ipc::IProtocol* aAllocator, size_t aSize,
                          RefCountedShmem& aShm) {
  MOZ_ASSERT(!IsValid(aShm));
  auto size = aSize + SHM_REFCOUNT_HEADER_SIZE;
  if (!aAllocator->AllocUnsafeShmem(size, &aShm.buffer())) {
    return false;
  }
  return true;
}

void RefCountedShm::Dealloc(mozilla::ipc::IProtocol* aAllocator,
                            RefCountedShmem& aShm) {
  aAllocator->DeallocShmem(aShm.buffer());
  aShm.buffer() = ipc::Shmem();
}

}  // namespace layers
}  // namespace mozilla
