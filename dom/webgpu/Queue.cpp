/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "Queue.h"

#include "CommandBuffer.h"
#include "CommandEncoder.h"
#include "ipc/WebGPUChild.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(Queue, mParent, mBridge)
GPU_IMPL_JS_WRAP(Queue)

Queue::Queue(Device* const aParent, WebGPUChild* aBridge, RawId aId)
    : ChildOf(aParent), mBridge(aBridge), mId(aId) {}

Queue::~Queue() { Cleanup(); }

void Queue::Submit(
    const dom::Sequence<OwningNonNull<CommandBuffer>>& aCommandBuffers) {
  nsTArray<RawId> list(aCommandBuffers.Length());
  for (uint32_t i = 0; i < aCommandBuffers.Length(); ++i) {
    auto idMaybe = aCommandBuffers[i]->Commit();
    if (idMaybe) {
      list.AppendElement(*idMaybe);
    }
  }

  mBridge->SendQueueSubmit(mId, list);
}

void Queue::WriteBuffer(const Buffer& aBuffer, uint64_t aBufferOffset,
                        const dom::ArrayBuffer& aData, uint64_t aDataOffset,
                        const dom::Optional<uint64_t>& aSize,
                        ErrorResult& aRv) {
  aData.ComputeState();
  const auto checkedSize =
      aSize.WasPassed() ? CheckedInt<size_t>(aSize.Value())
                        : CheckedInt<size_t>(aData.Length()) - aDataOffset;
  if (!checkedSize.isValid()) {
    aRv.ThrowRangeError("Mapped size is too large");
    return;
  }

  const auto& size = checkedSize.value();
  if (aDataOffset + size > aData.Length()) {
    aRv.ThrowAbortError(nsPrintfCString("Wrong data size %" PRIuPTR, size));
    return;
  }

  ipc::Shmem shmem;
  if (!mBridge->AllocShmem(size, ipc::Shmem::SharedMemory::TYPE_BASIC,
                           &shmem)) {
    aRv.ThrowAbortError(
        nsPrintfCString("Unable to allocate shmem of size %" PRIuPTR, size));
    return;
  }

  memcpy(shmem.get<uint8_t>(), aData.Data() + aDataOffset, size);
  mBridge->SendQueueWriteBuffer(mId, aBuffer.mId, aBufferOffset,
                                std::move(shmem));
}

void Queue::WriteTexture(const dom::GPUTextureCopyView& aDestination,
                         const dom::ArrayBuffer& aData,
                         const dom::GPUTextureDataLayout& aDataLayout,
                         const dom::GPUExtent3D& aSize, ErrorResult& aRv) {
  ffi::WGPUTextureCopyView copyView = {};
  CommandEncoder::ConvertTextureCopyViewToFFI(aDestination, &copyView);
  ffi::WGPUTextureDataLayout dataLayout = {};
  CommandEncoder::ConvertTextureDataLayoutToFFI(aDataLayout, &dataLayout);
  dataLayout.offset = 0;  // our Shmem has the contents starting from 0.
  ffi::WGPUExtent3d extent = {};
  CommandEncoder::ConvertExtent3DToFFI(aSize, &extent);

  const auto bpt = aDestination.mTexture->BytesPerTexel();
  if (bpt == 0) {
    aRv.ThrowAbortError(nsPrintfCString("Invalid texture format"));
    return;
  }
  if (extent.width == 0 || extent.height == 0 || extent.depth == 0) {
    aRv.ThrowAbortError(nsPrintfCString("Invalid copy size"));
    return;
  }

  aData.ComputeState();
  const auto fullRows =
      (CheckedInt<size_t>(extent.depth - 1) * aDataLayout.mRowsPerImage +
       extent.height - 1);
  const auto checkedSize = fullRows * aDataLayout.mBytesPerRow +
                           CheckedInt<size_t>(extent.width) * bpt;
  if (!checkedSize.isValid()) {
    aRv.ThrowRangeError("Mapped size is too large");
    return;
  }

  const auto& size = checkedSize.value();
  auto availableSize = aData.Length();
  if (availableSize < aDataLayout.mOffset ||
      size > (availableSize - aDataLayout.mOffset)) {
    aRv.ThrowAbortError(nsPrintfCString("Wrong data size %" PRIuPTR, size));
    return;
  }

  ipc::Shmem shmem;
  if (!mBridge->AllocShmem(size, ipc::Shmem::SharedMemory::TYPE_BASIC,
                           &shmem)) {
    aRv.ThrowAbortError(
        nsPrintfCString("Unable to allocate shmem of size %" PRIuPTR, size));
    return;
  }

  memcpy(shmem.get<uint8_t>(), aData.Data() + aDataLayout.mOffset, size);
  mBridge->SendQueueWriteTexture(mId, copyView, std::move(shmem), dataLayout,
                                 extent);
}

}  // namespace webgpu
}  // namespace mozilla
