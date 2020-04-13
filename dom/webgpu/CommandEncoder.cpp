/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "CommandEncoder.h"

#include "CommandBuffer.h"
#include "Buffer.h"
#include "ComputePassEncoder.h"
#include "Device.h"
#include "RenderPassEncoder.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(CommandEncoder, mParent, mBridge)
GPU_IMPL_JS_WRAP(CommandEncoder)

static ffi::WGPUBufferCopyView ConvertBufferCopyView(
    const dom::GPUBufferCopyView& aView) {
  ffi::WGPUBufferCopyView view;
  view.buffer = aView.mBuffer->mId;
  view.offset = aView.mOffset;
  view.bytes_per_row = aView.mBytesPerRow;
  view.rows_per_image = aView.mRowsPerImage;
  return view;
}

static ffi::WGPUTextureCopyView ConvertTextureCopyView(
    const dom::GPUTextureCopyView& aView) {
  ffi::WGPUTextureCopyView view = {};
  view.texture = aView.mTexture->mId;
  view.mip_level = aView.mMipLevel;
  view.array_layer = aView.mArrayLayer;
  if (aView.mOrigin.WasPassed()) {
    const auto& origin = aView.mOrigin.Value();
    if (origin.IsUnsignedLongSequence()) {
      const auto& seq = origin.GetAsUnsignedLongSequence();
      view.origin.x = seq.Length() > 0 ? seq[0] : 0;
      view.origin.y = seq.Length() > 1 ? seq[1] : 0;
      view.origin.z = seq.Length() > 2 ? seq[2] : 0;
    } else if (origin.IsGPUOrigin3DDict()) {
      const auto& dict = origin.GetAsGPUOrigin3DDict();
      view.origin.x = dict.mX;
      view.origin.y = dict.mY;
      view.origin.z = dict.mZ;
    } else {
      MOZ_CRASH("Unexpected origin type");
    }
  }
  return view;
}

static ffi::WGPUExtent3d ConvertExtent(const dom::GPUExtent3D& aExtent) {
  ffi::WGPUExtent3d extent;
  if (aExtent.IsUnsignedLongSequence()) {
    const auto& seq = aExtent.GetAsUnsignedLongSequence();
    extent.width = seq.Length() > 0 ? seq[0] : 0;
    extent.height = seq.Length() > 1 ? seq[1] : 0;
    extent.depth = seq.Length() > 2 ? seq[2] : 0;
  } else if (aExtent.IsGPUExtent3DDict()) {
    const auto& dict = aExtent.GetAsGPUExtent3DDict();
    extent.width = dict.mWidth;
    extent.height = dict.mHeight;
    extent.depth = dict.mDepth;
  } else {
    MOZ_CRASH("Unexptected extent type");
  }
  return extent;
}

CommandEncoder::CommandEncoder(Device* const aParent,
                               WebGPUChild* const aBridge, RawId aId)
    : ChildOf(aParent), mId(aId), mBridge(aBridge) {}

CommandEncoder::~CommandEncoder() { Cleanup(); }

void CommandEncoder::Cleanup() {
  if (mValid && mParent) {
    mValid = false;
    auto bridge = mParent->GetBridge();
    if (bridge && bridge->IsOpen()) {
      bridge->SendCommandEncoderDestroy(mId);
    }
  }
}

void CommandEncoder::CopyBufferToBuffer(const Buffer& aSource,
                                        BufferAddress aSourceOffset,
                                        const Buffer& aDestination,
                                        BufferAddress aDestinationOffset,
                                        BufferAddress aSize) {
  if (mValid) {
    mBridge->SendCommandEncoderCopyBufferToBuffer(
        mId, aSource.mId, aSourceOffset, aDestination.mId, aDestinationOffset,
        aSize);
  }
}

void CommandEncoder::CopyBufferToTexture(
    const dom::GPUBufferCopyView& aSource,
    const dom::GPUTextureCopyView& aDestination,
    const dom::GPUExtent3D& aCopySize) {
  if (mValid) {
    const auto source = ConvertBufferCopyView(aSource);
    const auto destination = ConvertTextureCopyView(aDestination);
    const auto size = ConvertExtent(aCopySize);
    mBridge->SendCommandEncoderCopyBufferToTexture(mId, source, destination,
                                                   size);
  }
}
void CommandEncoder::CopyTextureToBuffer(
    const dom::GPUTextureCopyView& aSource,
    const dom::GPUBufferCopyView& aDestination,
    const dom::GPUExtent3D& aCopySize) {
  if (mValid) {
    const auto source = ConvertTextureCopyView(aSource);
    const auto destination = ConvertBufferCopyView(aDestination);
    const auto size = ConvertExtent(aCopySize);
    mBridge->SendCommandEncoderCopyTextureToBuffer(mId, source, destination,
                                                   size);
  }
}
void CommandEncoder::CopyTextureToTexture(
    const dom::GPUTextureCopyView& aSource,
    const dom::GPUTextureCopyView& aDestination,
    const dom::GPUExtent3D& aCopySize) {
  if (mValid) {
    const auto source = ConvertTextureCopyView(aSource);
    const auto destination = ConvertTextureCopyView(aDestination);
    const auto size = ConvertExtent(aCopySize);
    mBridge->SendCommandEncoderCopyTextureToTexture(mId, source, destination,
                                                    size);
  }
}

already_AddRefed<ComputePassEncoder> CommandEncoder::BeginComputePass(
    const dom::GPUComputePassDescriptor& aDesc) {
  RefPtr<ComputePassEncoder> pass = new ComputePassEncoder(this, aDesc);
  return pass.forget();
}

already_AddRefed<RenderPassEncoder> CommandEncoder::BeginRenderPass(
    const dom::GPURenderPassDescriptor& aDesc) {
  RefPtr<RenderPassEncoder> pass = new RenderPassEncoder(this, aDesc);
  return pass.forget();
}

void CommandEncoder::EndComputePass(Span<const uint8_t> aData,
                                    ErrorResult& aRv) {
  if (!mValid) {
    return aRv.ThrowInvalidStateError("Command encoder is not valid");
  }
  ipc::Shmem shmem;
  if (!mBridge->AllocShmem(aData.Length(), ipc::Shmem::SharedMemory::TYPE_BASIC,
                           &shmem)) {
    return aRv.ThrowAbortError(nsPrintfCString(
        "Unable to allocate shmem of size %zu", aData.Length()));
  }

  memcpy(shmem.get<uint8_t>(), aData.data(), aData.Length());
  mBridge->SendCommandEncoderRunComputePass(mId, std::move(shmem));
}

void CommandEncoder::EndRenderPass(Span<const uint8_t> aData,
                                   ErrorResult& aRv) {
  if (!mValid) {
    return aRv.ThrowInvalidStateError("Command encoder is not valid");
  }
  ipc::Shmem shmem;
  if (!mBridge->AllocShmem(aData.Length(), ipc::Shmem::SharedMemory::TYPE_BASIC,
                           &shmem)) {
    return aRv.ThrowAbortError(nsPrintfCString(
        "Unable to allocate shmem of size %zu", aData.Length()));
  }

  memcpy(shmem.get<uint8_t>(), aData.data(), aData.Length());
  mBridge->SendCommandEncoderRunRenderPass(mId, std::move(shmem));
}

already_AddRefed<CommandBuffer> CommandEncoder::Finish(
    const dom::GPUCommandBufferDescriptor& aDesc) {
  RawId id = 0;
  if (mValid) {
    mValid = false;
    id = mBridge->CommandEncoderFinish(mId, aDesc);
  }
  RefPtr<CommandBuffer> comb = new CommandBuffer(mParent, id);
  return comb.forget();
}

}  // namespace webgpu
}  // namespace mozilla
