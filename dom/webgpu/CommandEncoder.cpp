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
#include "mozilla/webgpu/CanvasContext.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "ipc/WebGPUChild.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(CommandEncoder, mParent, mBridge)
GPU_IMPL_JS_WRAP(CommandEncoder)

void CommandEncoder::ConvertTextureDataLayoutToFFI(
    const dom::GPUImageDataLayout& aLayout,
    ffi::WGPUImageDataLayout* aLayoutFFI) {
  *aLayoutFFI = {};
  aLayoutFFI->offset = aLayout.mOffset;
  aLayoutFFI->bytes_per_row = aLayout.mBytesPerRow;
  aLayoutFFI->rows_per_image = aLayout.mRowsPerImage;
}

void CommandEncoder::ConvertTextureCopyViewToFFI(
    const dom::GPUImageCopyTexture& aCopy,
    ffi::WGPUImageCopyTexture* aViewFFI) {
  *aViewFFI = {};
  aViewFFI->texture = aCopy.mTexture->mId;
  aViewFFI->mip_level = aCopy.mMipLevel;
  if (aCopy.mOrigin.WasPassed()) {
    const auto& origin = aCopy.mOrigin.Value();
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
    aExtentFFI->depth_or_array_layers = seq.Length() > 2 ? seq[2] : 0;
  } else if (aExtent.IsGPUExtent3DDict()) {
    const auto& dict = aExtent.GetAsGPUExtent3DDict();
    aExtentFFI->width = dict.mWidth;
    aExtentFFI->height = dict.mHeight;
    aExtentFFI->depth_or_array_layers = dict.mDepthOrArrayLayers;
  } else {
    MOZ_CRASH("Unexptected extent type");
  }
}

static ffi::WGPUImageCopyBuffer ConvertBufferCopyView(
    const dom::GPUImageCopyBuffer& aCopy) {
  ffi::WGPUImageCopyBuffer view = {};
  view.buffer = aCopy.mBuffer->mId;
  CommandEncoder::ConvertTextureDataLayoutToFFI(aCopy, &view.layout);
  return view;
}

static ffi::WGPUImageCopyTexture ConvertTextureCopyView(
    const dom::GPUImageCopyTexture& aCopy) {
  ffi::WGPUImageCopyTexture view = {};
  CommandEncoder::ConvertTextureCopyViewToFFI(aCopy, &view);
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
  if (mValid) {
    mValid = false;
    if (mBridge->IsOpen()) {
      mBridge->SendCommandEncoderDestroy(mId);
    }
  }
}

void CommandEncoder::CopyBufferToBuffer(const Buffer& aSource,
                                        BufferAddress aSourceOffset,
                                        const Buffer& aDestination,
                                        BufferAddress aDestinationOffset,
                                        BufferAddress aSize) {
  if (mValid && mBridge->IsOpen()) {
    ipc::ByteBuf bb;
    ffi::wgpu_command_encoder_copy_buffer_to_buffer(
        aSource.mId, aSourceOffset, aDestination.mId, aDestinationOffset, aSize,
        ToFFI(&bb));
    mBridge->SendCommandEncoderAction(mId, mParent->mId, std::move(bb));
  }
}

void CommandEncoder::CopyBufferToTexture(
    const dom::GPUImageCopyBuffer& aSource,
    const dom::GPUImageCopyTexture& aDestination,
    const dom::GPUExtent3D& aCopySize) {
  if (mValid && mBridge->IsOpen()) {
    ipc::ByteBuf bb;
    ffi::wgpu_command_encoder_copy_buffer_to_texture(
        ConvertBufferCopyView(aSource), ConvertTextureCopyView(aDestination),
        ConvertExtent(aCopySize), ToFFI(&bb));
    mBridge->SendCommandEncoderAction(mId, mParent->mId, std::move(bb));

    const auto& targetContext = aDestination.mTexture->mTargetContext;
    if (targetContext) {
      mTargetContexts.AppendElement(targetContext);
    }
  }
}
void CommandEncoder::CopyTextureToBuffer(
    const dom::GPUImageCopyTexture& aSource,
    const dom::GPUImageCopyBuffer& aDestination,
    const dom::GPUExtent3D& aCopySize) {
  if (mValid && mBridge->IsOpen()) {
    ipc::ByteBuf bb;
    ffi::wgpu_command_encoder_copy_texture_to_buffer(
        ConvertTextureCopyView(aSource), ConvertBufferCopyView(aDestination),
        ConvertExtent(aCopySize), ToFFI(&bb));
    mBridge->SendCommandEncoderAction(mId, mParent->mId, std::move(bb));
  }
}
void CommandEncoder::CopyTextureToTexture(
    const dom::GPUImageCopyTexture& aSource,
    const dom::GPUImageCopyTexture& aDestination,
    const dom::GPUExtent3D& aCopySize) {
  if (mValid && mBridge->IsOpen()) {
    ipc::ByteBuf bb;
    ffi::wgpu_command_encoder_copy_texture_to_texture(
        ConvertTextureCopyView(aSource), ConvertTextureCopyView(aDestination),
        ConvertExtent(aCopySize), ToFFI(&bb));
    mBridge->SendCommandEncoderAction(mId, mParent->mId, std::move(bb));

    const auto& targetContext = aDestination.mTexture->mTargetContext;
    if (targetContext) {
      mTargetContexts.AppendElement(targetContext);
    }
  }
}

void CommandEncoder::PushDebugGroup(const nsAString& aString) {
  if (mValid && mBridge->IsOpen()) {
    ipc::ByteBuf bb;
    NS_ConvertUTF16toUTF8 marker(aString);
    ffi::wgpu_command_encoder_push_debug_group(&marker, ToFFI(&bb));
    mBridge->SendCommandEncoderAction(mId, mParent->mId, std::move(bb));
  }
}
void CommandEncoder::PopDebugGroup() {
  if (mValid && mBridge->IsOpen()) {
    ipc::ByteBuf bb;
    ffi::wgpu_command_encoder_pop_debug_group(ToFFI(&bb));
    mBridge->SendCommandEncoderAction(mId, mParent->mId, std::move(bb));
  }
}
void CommandEncoder::InsertDebugMarker(const nsAString& aString) {
  if (mValid && mBridge->IsOpen()) {
    ipc::ByteBuf bb;
    NS_ConvertUTF16toUTF8 marker(aString);
    ffi::wgpu_command_encoder_insert_debug_marker(&marker, ToFFI(&bb));
    mBridge->SendCommandEncoderAction(mId, mParent->mId, std::move(bb));
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
    auto* targetContext = at.mView->GetTargetContext();
    if (targetContext) {
      mTargetContexts.AppendElement(targetContext);
    }
    if (at.mResolveTarget.WasPassed()) {
      targetContext = at.mResolveTarget.Value().GetTargetContext();
      mTargetContexts.AppendElement(targetContext);
    }
  }

  RefPtr<RenderPassEncoder> pass = new RenderPassEncoder(this, aDesc);
  return pass.forget();
}

void CommandEncoder::EndComputePass(ffi::WGPUComputePass& aPass,
                                    ErrorResult& aRv) {
  if (!mValid || !mBridge->IsOpen()) {
    return aRv.ThrowInvalidStateError("Command encoder is not valid");
  }

  ipc::ByteBuf byteBuf;
  ffi::wgpu_compute_pass_finish(&aPass, ToFFI(&byteBuf));
  mBridge->SendCommandEncoderAction(mId, mParent->mId, std::move(byteBuf));
}

void CommandEncoder::EndRenderPass(ffi::WGPURenderPass& aPass,
                                   ErrorResult& aRv) {
  if (!mValid || !mBridge->IsOpen()) {
    return aRv.ThrowInvalidStateError("Command encoder is not valid");
  }

  ipc::ByteBuf byteBuf;
  ffi::wgpu_render_pass_finish(&aPass, ToFFI(&byteBuf));
  mBridge->SendCommandEncoderAction(mId, mParent->mId, std::move(byteBuf));
}

already_AddRefed<CommandBuffer> CommandEncoder::Finish(
    const dom::GPUCommandBufferDescriptor& aDesc) {
  RawId id = 0;
  if (mValid && mBridge->IsOpen()) {
    mValid = false;
    id = mBridge->CommandEncoderFinish(mId, mParent->mId, aDesc);
  }
  RefPtr<CommandBuffer> comb =
      new CommandBuffer(mParent, id, std::move(mTargetContexts));
  return comb.forget();
}

}  // namespace mozilla::webgpu
