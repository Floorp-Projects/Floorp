/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "RenderPassEncoder.h"
#include "BindGroup.h"
#include "CommandEncoder.h"
#include "RenderBundle.h"
#include "RenderPipeline.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(RenderPassEncoder, mParent, mUsedBindGroups,
                          mUsedBuffers, mUsedPipelines, mUsedTextureViews,
                          mUsedRenderBundles)
GPU_IMPL_JS_WRAP(RenderPassEncoder)

void ffiWGPURenderPassDeleter::operator()(ffi::WGPURenderPass* raw) {
  if (raw) {
    ffi::wgpu_render_pass_destroy(raw);
  }
}

static ffi::WGPULoadOp ConvertLoadOp(const dom::GPULoadOp& aOp) {
  switch (aOp) {
    case dom::GPULoadOp::Load:
      return ffi::WGPULoadOp_Load;
    case dom::GPULoadOp::Clear:
      return ffi::WGPULoadOp_Clear;
    case dom::GPULoadOp::EndGuard_:
      break;
  }
  MOZ_CRASH("bad GPULoadOp");
}

static ffi::WGPUStoreOp ConvertStoreOp(const dom::GPUStoreOp& aOp) {
  switch (aOp) {
    case dom::GPUStoreOp::Store:
      return ffi::WGPUStoreOp_Store;
    case dom::GPUStoreOp::Discard:
      return ffi::WGPUStoreOp_Discard;
    case dom::GPUStoreOp::EndGuard_:
      break;
  }
  MOZ_CRASH("bad GPUStoreOp");
}

static ffi::WGPUColor ConvertColor(const dom::Sequence<double>& aSeq) {
  ffi::WGPUColor color;
  color.r = aSeq.SafeElementAt(0, 0.0);
  color.g = aSeq.SafeElementAt(1, 0.0);
  color.b = aSeq.SafeElementAt(2, 0.0);
  color.a = aSeq.SafeElementAt(3, 1.0);
  return color;
}

static ffi::WGPUColor ConvertColor(const dom::GPUColorDict& aColor) {
  ffi::WGPUColor color = {aColor.mR, aColor.mG, aColor.mB, aColor.mA};
  return color;
}

static ffi::WGPUColor ConvertColor(
    const dom::DoubleSequenceOrGPUColorDict& aColor) {
  if (aColor.IsDoubleSequence()) {
    return ConvertColor(aColor.GetAsDoubleSequence());
  }
  if (aColor.IsGPUColorDict()) {
    return ConvertColor(aColor.GetAsGPUColorDict());
  }
  MOZ_ASSERT_UNREACHABLE(
      "Unexpected dom::DoubleSequenceOrGPUColorDict variant");
  return ffi::WGPUColor();
}
static ffi::WGPUColor ConvertColor(
    const dom::OwningDoubleSequenceOrGPUColorDict& aColor) {
  if (aColor.IsDoubleSequence()) {
    return ConvertColor(aColor.GetAsDoubleSequence());
  }
  if (aColor.IsGPUColorDict()) {
    return ConvertColor(aColor.GetAsGPUColorDict());
  }
  MOZ_ASSERT_UNREACHABLE(
      "Unexpected dom::OwningDoubleSequenceOrGPUColorDict variant");
  return ffi::WGPUColor();
}

ffi::WGPURenderPass* BeginRenderPass(
    CommandEncoder* const aParent, const dom::GPURenderPassDescriptor& aDesc) {
  ffi::WGPURenderPassDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ffi::WGPURenderPassDepthStencilAttachment dsDesc = {};
  if (aDesc.mDepthStencilAttachment.WasPassed()) {
    const auto& dsa = aDesc.mDepthStencilAttachment.Value();
    dsDesc.view = dsa.mView->mId;

    // -

    if (dsa.mDepthClearValue.WasPassed()) {
      dsDesc.depth.clear_value = dsa.mDepthClearValue.Value();
    }
    if (dsa.mDepthLoadOp.WasPassed()) {
      dsDesc.depth.load_op = ConvertLoadOp(dsa.mDepthLoadOp.Value());
    }
    if (dsa.mDepthStoreOp.WasPassed()) {
      dsDesc.depth.store_op = ConvertStoreOp(dsa.mDepthStoreOp.Value());
    }
    dsDesc.depth.read_only = dsa.mDepthReadOnly;

    // -

    dsDesc.stencil.clear_value = dsa.mStencilClearValue;
    if (dsa.mStencilLoadOp.WasPassed()) {
      dsDesc.stencil.load_op = ConvertLoadOp(dsa.mStencilLoadOp.Value());
    }
    if (dsa.mStencilStoreOp.WasPassed()) {
      dsDesc.stencil.store_op = ConvertStoreOp(dsa.mStencilStoreOp.Value());
    }
    dsDesc.stencil.read_only = dsa.mStencilReadOnly;

    // -

    desc.depth_stencil_attachment = &dsDesc;
  }

  if (aDesc.mColorAttachments.Length() > WGPUMAX_COLOR_ATTACHMENTS) {
    aParent->GetDevice()->GenerateValidationError(nsLiteralCString(
        "Too many color attachments in GPURenderPassDescriptor"));
    return nullptr;
  }

  std::array<ffi::WGPURenderPassColorAttachment, WGPUMAX_COLOR_ATTACHMENTS>
      colorDescs = {};
  desc.color_attachments = colorDescs.data();
  desc.color_attachments_length = aDesc.mColorAttachments.Length();

  for (size_t i = 0; i < aDesc.mColorAttachments.Length(); ++i) {
    const auto& ca = aDesc.mColorAttachments[i];
    ffi::WGPURenderPassColorAttachment& cd = colorDescs[i];
    cd.view = ca.mView->mId;
    cd.channel.store_op = ConvertStoreOp(ca.mStoreOp);

    if (ca.mResolveTarget.WasPassed()) {
      cd.resolve_target = ca.mResolveTarget.Value().mId;
    }

    cd.channel.load_op = ConvertLoadOp(ca.mLoadOp);
    if (ca.mClearValue.WasPassed()) {
      cd.channel.clear_value = ConvertColor(ca.mClearValue.Value());
    }
  }

  return ffi::wgpu_command_encoder_begin_render_pass(aParent->mId, &desc);
}

RenderPassEncoder::RenderPassEncoder(CommandEncoder* const aParent,
                                     const dom::GPURenderPassDescriptor& aDesc)
    : ChildOf(aParent), mPass(BeginRenderPass(aParent, aDesc)) {
  if (!mPass) {
    mValid = false;
    return;
  }

  for (const auto& at : aDesc.mColorAttachments) {
    mUsedTextureViews.AppendElement(at.mView);
  }
  if (aDesc.mDepthStencilAttachment.WasPassed()) {
    mUsedTextureViews.AppendElement(
        aDesc.mDepthStencilAttachment.Value().mView);
  }
}

RenderPassEncoder::~RenderPassEncoder() {
  if (mValid) {
    mValid = false;
  }
}

void RenderPassEncoder::SetBindGroup(
    uint32_t aSlot, const BindGroup& aBindGroup,
    const dom::Sequence<uint32_t>& aDynamicOffsets) {
  if (mValid) {
    mUsedBindGroups.AppendElement(&aBindGroup);
    ffi::wgpu_render_pass_set_bind_group(mPass.get(), aSlot, aBindGroup.mId,
                                         aDynamicOffsets.Elements(),
                                         aDynamicOffsets.Length());
  }
}

void RenderPassEncoder::SetPipeline(const RenderPipeline& aPipeline) {
  if (mValid) {
    mUsedPipelines.AppendElement(&aPipeline);
    ffi::wgpu_render_pass_set_pipeline(mPass.get(), aPipeline.mId);
  }
}

void RenderPassEncoder::SetIndexBuffer(const Buffer& aBuffer,
                                       const dom::GPUIndexFormat& aIndexFormat,
                                       uint64_t aOffset, uint64_t aSize) {
  if (mValid) {
    mUsedBuffers.AppendElement(&aBuffer);
    const auto iformat = aIndexFormat == dom::GPUIndexFormat::Uint32
                             ? ffi::WGPUIndexFormat_Uint32
                             : ffi::WGPUIndexFormat_Uint16;
    ffi::wgpu_render_pass_set_index_buffer(mPass.get(), aBuffer.mId, iformat, aOffset,
                                           aSize);
  }
}

void RenderPassEncoder::SetVertexBuffer(uint32_t aSlot, const Buffer& aBuffer,
                                        uint64_t aOffset, uint64_t aSize) {
  if (mValid) {
    mUsedBuffers.AppendElement(&aBuffer);
    ffi::wgpu_render_pass_set_vertex_buffer(mPass.get(), aSlot, aBuffer.mId, aOffset,
                                            aSize);
  }
}

void RenderPassEncoder::Draw(uint32_t aVertexCount, uint32_t aInstanceCount,
                             uint32_t aFirstVertex, uint32_t aFirstInstance) {
  if (mValid) {
    ffi::wgpu_render_pass_draw(mPass.get(), aVertexCount, aInstanceCount,
                               aFirstVertex, aFirstInstance);
  }
}

void RenderPassEncoder::DrawIndexed(uint32_t aIndexCount,
                                    uint32_t aInstanceCount,
                                    uint32_t aFirstIndex, int32_t aBaseVertex,
                                    uint32_t aFirstInstance) {
  if (mValid) {
    ffi::wgpu_render_pass_draw_indexed(mPass.get(), aIndexCount, aInstanceCount,
                                       aFirstIndex, aBaseVertex,
                                       aFirstInstance);
  }
}

void RenderPassEncoder::DrawIndirect(const Buffer& aIndirectBuffer,
                                     uint64_t aIndirectOffset) {
  if (mValid) {
    ffi::wgpu_render_pass_draw_indirect(mPass.get(), aIndirectBuffer.mId,
                                        aIndirectOffset);
  }
}

void RenderPassEncoder::DrawIndexedIndirect(const Buffer& aIndirectBuffer,
                                            uint64_t aIndirectOffset) {
  if (mValid) {
    ffi::wgpu_render_pass_draw_indexed_indirect(mPass.get(), aIndirectBuffer.mId,
                                                aIndirectOffset);
  }
}

void RenderPassEncoder::SetViewport(float x, float y, float width, float height,
                                    float minDepth, float maxDepth) {
  if (mValid) {
    ffi::wgpu_render_pass_set_viewport(mPass.get(), x, y, width, height, minDepth,
                                       maxDepth);
  }
}

void RenderPassEncoder::SetScissorRect(uint32_t x, uint32_t y, uint32_t width,
                                       uint32_t height) {
  if (mValid) {
    ffi::wgpu_render_pass_set_scissor_rect(mPass.get(), x, y, width, height);
  }
}

void RenderPassEncoder::SetBlendConstant(
    const dom::DoubleSequenceOrGPUColorDict& color) {
  if (mValid) {
    ffi::WGPUColor aColor = ConvertColor(color);
    ffi::wgpu_render_pass_set_blend_constant(mPass.get(), &aColor);
  }
}

void RenderPassEncoder::SetStencilReference(uint32_t reference) {
  if (mValid) {
    ffi::wgpu_render_pass_set_stencil_reference(mPass.get(), reference);
  }
}

void RenderPassEncoder::ExecuteBundles(
    const dom::Sequence<OwningNonNull<RenderBundle>>& aBundles) {
  if (mValid) {
    nsTArray<ffi::WGPURenderBundleId> renderBundles(aBundles.Length());
    for (const auto& bundle : aBundles) {
      mUsedRenderBundles.AppendElement(bundle);
      renderBundles.AppendElement(bundle->mId);
    }
    ffi::wgpu_render_pass_execute_bundles(mPass.get(), renderBundles.Elements(),
                                          renderBundles.Length());
  }
}

void RenderPassEncoder::PushDebugGroup(const nsAString& aString) {
  if (mValid) {
    const NS_ConvertUTF16toUTF8 utf8(aString);
    ffi::wgpu_render_pass_push_debug_group(mPass.get(), utf8.get(), 0);
  }
}
void RenderPassEncoder::PopDebugGroup() {
  if (mValid) {
    ffi::wgpu_render_pass_pop_debug_group(mPass.get());
  }
}
void RenderPassEncoder::InsertDebugMarker(const nsAString& aString) {
  if (mValid) {
    const NS_ConvertUTF16toUTF8 utf8(aString);
    ffi::wgpu_render_pass_insert_debug_marker(mPass.get(), utf8.get(), 0);
  }
}

void RenderPassEncoder::End() {
  if (mValid) {
    mValid = false;
    auto* pass = mPass.release();
    MOZ_ASSERT(pass);
    mParent->EndRenderPass(*pass);
  }
}

}  // namespace mozilla::webgpu
