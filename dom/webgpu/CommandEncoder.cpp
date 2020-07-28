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

void CommandEncoder::ConvertTextureDataLayoutToFFI(
    const dom::GPUTextureDataLayout& aLayout,
    ffi::WGPUTextureDataLayout* aLayoutFFI) {
  *aLayoutFFI = {};
  aLayoutFFI->offset = aLayout.mOffset;
  aLayoutFFI->bytes_per_row = aLayout.mBytesPerRow;
  aLayoutFFI->rows_per_image = aLayout.mRowsPerImage;
}

void CommandEncoder::ConvertTextureCopyViewToFFI(
    const dom::GPUTextureCopyView& aView, ffi::WGPUTextureCopyView* aViewFFI) {
  *aViewFFI = {};
  aViewFFI->texture = aView.mTexture->mId;
  aViewFFI->mip_level = aView.mMipLevel;
  if (aView.mOrigin.WasPassed()) {
    const auto& origin = aView.mOrigin.Value();
    if (origin.IsRangeEnforcedUnsignedLongSequence()) {
      const auto& seq = origin.GetAsRangeEnforcedUnsignedLongSequence();
      aViewFFI->origin.x = seq.Length() > 0 ? seq[0] : 0;
      aViewFFI->origin.y = seq.Length() > 1 ? seq[1] : 0;
      aViewFFI->origin.z = seq.Length() > 2 ? seq[2] : 0;
    } else if (origin.IsGPUOrigin3DDict()) {
      const auto& dict = origin.GetAsGPUOrigin3DDict();
      aViewFFI->origin.x = dict.mX;
      aViewFFI->origin.y = dict.mY;
      aViewFFI->origin.z = dict.mZ;
    } else {
      MOZ_CRASH("Unexpected origin type");
    }
  }
}

void CommandEncoder::ConvertExtent3DToFFI(const dom::GPUExtent3D& aExtent,
                                          ffi::WGPUExtent3d* aExtentFFI) {
  *aExtentFFI = {};
  if (aExtent.IsRangeEnforcedUnsignedLongSequence()) {
    const auto& seq = aExtent.GetAsRangeEnforcedUnsignedLongSequence();
    aExtentFFI->width = seq.Length() > 0 ? seq[0] : 0;
    aExtentFFI->height = seq.Length() > 1 ? seq[1] : 0;
    aExtentFFI->depth = seq.Length() > 2 ? seq[2] : 0;
  } else if (aExtent.IsGPUExtent3DDict()) {
    const auto& dict = aExtent.GetAsGPUExtent3DDict();
    aExtentFFI->width = dict.mWidth;
    aExtentFFI->height = dict.mHeight;
    aExtentFFI->depth = dict.mDepth;
  } else {
    MOZ_CRASH("Unexptected extent type");
  }
}

static ffi::WGPUBufferCopyView ConvertBufferCopyView(
    const dom::GPUBufferCopyView& aView) {
  ffi::WGPUBufferCopyView view = {};
  view.buffer = aView.mBuffer->mId;
  CommandEncoder::ConvertTextureDataLayoutToFFI(aView, &view.layout);
  return view;
}

static ffi::WGPUTextureCopyView ConvertTextureCopyView(
    const dom::GPUTextureCopyView& aView) {
  ffi::WGPUTextureCopyView view = {};
  CommandEncoder::ConvertTextureCopyViewToFFI(aView, &view);
  return view;
}

static ffi::WGPUExtent3d ConvertExtent(const dom::GPUExtent3D& aExtent) {
  ffi::WGPUExtent3d extent = {};
  CommandEncoder::ConvertExtent3DToFFI(aExtent, &extent);
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
  for (const auto& at : aDesc.mColorAttachments) {
    auto* targetCanvasElement = at.mAttachment->GetTargetCanvasElement();
    if (targetCanvasElement) {
      if (mTargetCanvasElement) {
        NS_WARNING("Command encoder touches more than one canvas");
      } else {
        mTargetCanvasElement = targetCanvasElement;
      }
    }
  }

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
  RefPtr<CommandBuffer> comb =
      new CommandBuffer(mParent, id, mTargetCanvasElement);
  return comb.forget();
}

}  // namespace webgpu
}  // namespace mozilla
