/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "RenderPassEncoder.h"
#include "BindGroup.h"
#include "CommandEncoder.h"
#include "RenderPipeline.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(RenderPassEncoder, mParent, mUsedBindGroups,
                          mUsedBuffers, mUsedPipelines, mUsedTextureViews)
GPU_IMPL_JS_WRAP(RenderPassEncoder)

ffi::WGPULoadOp ConvertLoadOp(const dom::GPULoadOp& aOp) {
  switch (aOp) {
    case dom::GPULoadOp::Load:
      return ffi::WGPULoadOp_Load;
    default:
      MOZ_CRASH("Unexpected load op");
  }
}

ffi::WGPUStoreOp ConvertStoreOp(const dom::GPUStoreOp& aOp) {
  switch (aOp) {
    case dom::GPUStoreOp::Store:
      return ffi::WGPUStoreOp_Store;
    case dom::GPUStoreOp::Clear:
      return ffi::WGPUStoreOp_Clear;
    default:
      MOZ_CRASH("Unexpected load op");
  }
}

ffi::WGPUColor ConvertColor(const dom::GPUColorDict& aColor) {
  ffi::WGPUColor color = {aColor.mR, aColor.mG, aColor.mB, aColor.mA};
  return color;
}

ffi::WGPURawPass BeginRenderPass(RawId aEncoderId,
                                 const dom::GPURenderPassDescriptor& aDesc) {
  ffi::WGPURenderPassDescriptor desc = {};

  ffi::WGPURenderPassDepthStencilAttachmentDescriptor dsDesc = {};
  if (aDesc.mDepthStencilAttachment.WasPassed()) {
    const auto& dsa = aDesc.mDepthStencilAttachment.Value();
    dsDesc.attachment = dsa.mAttachment->mId;

    if (dsa.mDepthLoadValue.IsFloat()) {
      dsDesc.depth_load_op = ffi::WGPULoadOp_Clear;
      dsDesc.clear_depth = dsa.mDepthLoadValue.GetAsFloat();
    }
    if (dsa.mDepthLoadValue.IsGPULoadOp()) {
      dsDesc.depth_load_op =
          ConvertLoadOp(dsa.mDepthLoadValue.GetAsGPULoadOp());
    }
    dsDesc.depth_store_op = ConvertStoreOp(dsa.mDepthStoreOp);

    if (dsa.mStencilLoadValue.IsUnsignedLong()) {
      dsDesc.stencil_load_op = ffi::WGPULoadOp_Clear;
      dsDesc.clear_stencil = dsa.mStencilLoadValue.GetAsUnsignedLong();
    }
    if (dsa.mStencilLoadValue.IsGPULoadOp()) {
      dsDesc.stencil_load_op =
          ConvertLoadOp(dsa.mStencilLoadValue.GetAsGPULoadOp());
    }
    dsDesc.stencil_store_op = ConvertStoreOp(dsa.mStencilStoreOp);

    desc.depth_stencil_attachment = &dsDesc;
  }

  std::array<ffi::WGPURenderPassColorAttachmentDescriptor,
             WGPUMAX_COLOR_TARGETS>
      colorDescs = {};
  desc.color_attachments = colorDescs.data();
  desc.color_attachments_length = aDesc.mColorAttachments.Length();

  for (size_t i = 0; i < aDesc.mColorAttachments.Length(); ++i) {
    const auto& ca = aDesc.mColorAttachments[i];
    ffi::WGPURenderPassColorAttachmentDescriptor& cd = colorDescs[i];
    cd.attachment = ca.mAttachment->mId;
    cd.store_op = ConvertStoreOp(ca.mStoreOp);

    if (ca.mResolveTarget.WasPassed()) {
      cd.resolve_target = ca.mResolveTarget.Value().mId;
    }
    if (ca.mLoadValue.IsGPULoadOp()) {
      cd.load_op = ConvertLoadOp(ca.mLoadValue.GetAsGPULoadOp());
    } else {
      cd.load_op = ffi::WGPULoadOp_Clear;
      if (ca.mLoadValue.IsDoubleSequence()) {
        const auto& seq = ca.mLoadValue.GetAsDoubleSequence();
        if (seq.Length() >= 1) {
          cd.clear_color.r = seq[0];
        }
        if (seq.Length() >= 2) {
          cd.clear_color.g = seq[1];
        }
        if (seq.Length() >= 3) {
          cd.clear_color.b = seq[2];
        }
        if (seq.Length() >= 4) {
          cd.clear_color.a = seq[3];
        }
      }
      if (ca.mLoadValue.IsGPUColorDict()) {
        cd.clear_color = ConvertColor(ca.mLoadValue.GetAsGPUColorDict());
      }
    }
  }

  return ffi::wgpu_command_encoder_begin_render_pass(aEncoderId, &desc);
}

RenderPassEncoder::RenderPassEncoder(CommandEncoder* const aParent,
                                     const dom::GPURenderPassDescriptor& aDesc)
    : ChildOf(aParent), mRaw(BeginRenderPass(aParent->mId, aDesc)) {
  for (const auto& at : aDesc.mColorAttachments) {
    mUsedTextureViews.AppendElement(at.mAttachment);
  }
  if (aDesc.mDepthStencilAttachment.WasPassed()) {
    mUsedTextureViews.AppendElement(
        aDesc.mDepthStencilAttachment.Value().mAttachment);
  }
}

RenderPassEncoder::~RenderPassEncoder() {
  if (mValid) {
    mValid = false;
    ffi::wgpu_render_pass_destroy(mRaw);
  }
}

void RenderPassEncoder::SetBindGroup(
    uint32_t aSlot, const BindGroup& aBindGroup,
    const dom::Sequence<uint32_t>& aDynamicOffsets) {
  if (mValid) {
    mUsedBindGroups.AppendElement(&aBindGroup);
    ffi::wgpu_render_pass_set_bind_group(&mRaw, aSlot, aBindGroup.mId,
                                         aDynamicOffsets.Elements(),
                                         aDynamicOffsets.Length());
  }
}

void RenderPassEncoder::SetPipeline(const RenderPipeline& aPipeline) {
  if (mValid) {
    mUsedPipelines.AppendElement(&aPipeline);
    ffi::wgpu_render_pass_set_pipeline(&mRaw, aPipeline.mId);
  }
}

void RenderPassEncoder::SetIndexBuffer(const Buffer& aBuffer, uint64_t aOffset,
                                       uint64_t aSize) {
  if (mValid) {
    mUsedBuffers.AppendElement(&aBuffer);
    ffi::wgpu_render_pass_set_index_buffer(&mRaw, aBuffer.mId, aOffset, aSize);
  }
}

void RenderPassEncoder::SetVertexBuffer(uint32_t aSlot, const Buffer& aBuffer,
                                        uint64_t aOffset, uint64_t aSize) {
  if (mValid) {
    mUsedBuffers.AppendElement(&aBuffer);
    ffi::wgpu_render_pass_set_vertex_buffer(&mRaw, aSlot, aBuffer.mId, aOffset,
                                            aSize);
  }
}

void RenderPassEncoder::Draw(uint32_t aVertexCount, uint32_t aInstanceCount,
                             uint32_t aFirstVertex, uint32_t aFirstInstance) {
  if (mValid) {
    ffi::wgpu_render_pass_draw(&mRaw, aVertexCount, aInstanceCount,
                               aFirstVertex, aFirstInstance);
  }
}

void RenderPassEncoder::DrawIndexed(uint32_t aIndexCount,
                                    uint32_t aInstanceCount,
                                    uint32_t aFirstIndex, int32_t aBaseVertex,
                                    uint32_t aFirstInstance) {
  if (mValid) {
    ffi::wgpu_render_pass_draw_indexed(&mRaw, aIndexCount, aInstanceCount,
                                       aFirstIndex, aBaseVertex,
                                       aFirstInstance);
  }
}

void RenderPassEncoder::EndPass(ErrorResult& aRv) {
  if (mValid) {
    mValid = false;
    uintptr_t length = 0;
    const uint8_t* pass_data = ffi::wgpu_render_pass_finish(&mRaw, &length);
    mParent->EndRenderPass(Span(pass_data, length), aRv);
    ffi::wgpu_render_pass_destroy(mRaw);
  }
}

}  // namespace webgpu
}  // namespace mozilla
