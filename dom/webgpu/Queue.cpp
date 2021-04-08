/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/dom/UnionTypes.h"
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

  mBridge->SendQueueSubmit(mId, mParent->mId, list);
}

void Queue::WriteBuffer(const Buffer& aBuffer, uint64_t aBufferOffset,
                        const dom::ArrayBufferViewOrArrayBuffer& aData,
                        uint64_t aDataOffset,
                        const dom::Optional<uint64_t>& aSize,
                        ErrorResult& aRv) {
  uint64_t length = 0;
  uint8_t* data = nullptr;
  if (aData.IsArrayBufferView()) {
    const auto& view = aData.GetAsArrayBufferView();
    view.ComputeState();
    length = view.Length();
    data = view.Data();
  }
  if (aData.IsArrayBuffer()) {
    const auto& ab = aData.GetAsArrayBuffer();
    ab.ComputeState();
    length = ab.Length();
    data = ab.Data();
  }
  MOZ_ASSERT(data != nullptr);

  const auto checkedSize = aSize.WasPassed()
                               ? CheckedInt<size_t>(aSize.Value())
                               : CheckedInt<size_t>(length) - aDataOffset;
  if (!checkedSize.isValid()) {
    aRv.ThrowRangeError("Mapped size is too large");
    return;
  }

  const auto& size = checkedSize.value();
  if (aDataOffset + size > length) {
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

  memcpy(shmem.get<uint8_t>(), data + aDataOffset, size);
  ipc::ByteBuf bb;
  ffi::wgpu_queue_write_buffer(aBuffer.mId, aBufferOffset, ToFFI(&bb));
  if (!mBridge->SendQueueWriteAction(mId, mParent->mId, std::move(bb),
                                     std::move(shmem))) {
    MOZ_CRASH("IPC failure");
  }
}

void Queue::WriteTexture(const dom::GPUImageCopyTexture& aDestination,
                         const dom::ArrayBufferViewOrArrayBuffer& aData,
                         const dom::GPUImageDataLayout& aDataLayout,
                         const dom::GPUExtent3D& aSize, ErrorResult& aRv) {
  ffi::WGPUImageCopyTexture copyView = {};
  CommandEncoder::ConvertTextureCopyViewToFFI(aDestination, &copyView);
  ffi::WGPUImageDataLayout dataLayout = {};
  CommandEncoder::ConvertTextureDataLayoutToFFI(aDataLayout, &dataLayout);
  dataLayout.offset = 0;  // our Shmem has the contents starting from 0.
  ffi::WGPUExtent3d extent = {};
  CommandEncoder::ConvertExtent3DToFFI(aSize, &extent);

  uint64_t availableSize = 0;
  uint8_t* data = nullptr;
  if (aData.IsArrayBufferView()) {
    const auto& view = aData.GetAsArrayBufferView();
    view.ComputeState();
    availableSize = view.Length();
    data = view.Data();
  }
  if (aData.IsArrayBuffer()) {
    const auto& ab = aData.GetAsArrayBuffer();
    ab.ComputeState();
    availableSize = ab.Length();
    data = ab.Data();
  }
  MOZ_ASSERT(data != nullptr);

  const auto bpb = aDestination.mTexture->mBytesPerBlock;
  if (!bpb) {
    aRv.ThrowAbortError(nsPrintfCString("Invalid texture format"));
    return;
  }
  if (extent.width == 0 || extent.height == 0 ||
      extent.depth_or_array_layers == 0) {
    aRv.ThrowAbortError(nsPrintfCString("Invalid copy size"));
    return;
  }

  // TODO: support block-compressed formats
  const auto fullRows = (CheckedInt<size_t>(extent.depth_or_array_layers - 1) *
                             aDataLayout.mRowsPerImage +
                         extent.height - 1);
  const auto checkedSize = fullRows * aDataLayout.mBytesPerRow +
                           CheckedInt<size_t>(extent.width) * bpb.value();
  if (!checkedSize.isValid()) {
    aRv.ThrowRangeError("Mapped size is too large");
    return;
  }

  const auto& size = checkedSize.value();
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

  memcpy(shmem.get<uint8_t>(), data + aDataLayout.mOffset, size);
  ipc::ByteBuf bb;
  ffi::wgpu_queue_write_texture(copyView, dataLayout, extent, ToFFI(&bb));
  if (!mBridge->SendQueueWriteAction(mId, mParent->mId, std::move(bb),
                                     std::move(shmem))) {
    MOZ_CRASH("IPC failure");
  }
}

}  // namespace webgpu
}  // namespace mozilla
