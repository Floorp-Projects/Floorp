/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "RenderBundleEncoder.h"

#include "BindGroup.h"
#include "Buffer.h"
#include "RenderBundle.h"
#include "RenderPipeline.h"
#include "ipc/WebGPUChild.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(RenderBundleEncoder, mParent, mUsedBindGroups,
                          mUsedBuffers, mUsedPipelines, mUsedTextureViews)
GPU_IMPL_JS_WRAP(RenderBundleEncoder)

ffi::WGPURenderBundleEncoder* ScopedFfiBundleTraits::empty() { return nullptr; }

void ScopedFfiBundleTraits::release(ffi::WGPURenderBundleEncoder* raw) {
  if (raw) {
    ffi::wgpu_render_bundle_encoder_destroy(raw);
  }
}

ffi::WGPURenderBundleEncoder* CreateRenderBundleEncoder(
    RawId aDeviceId, const dom::GPURenderBundleEncoderDescriptor& aDesc,
    WebGPUChild* const aBridge) {
  if (!aBridge->CanSend()) {
    return nullptr;
  }

  ffi::WGPURenderBundleEncoderDescriptor desc = {};
  desc.sample_count = aDesc.mSampleCount;

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ffi::WGPUTextureFormat depthStencilFormat = {ffi::WGPUTextureFormat_Sentinel};
  if (aDesc.mDepthStencilFormat.WasPassed()) {
    WebGPUChild::ConvertTextureFormatRef(aDesc.mDepthStencilFormat.Value(),
                                         depthStencilFormat);
    desc.depth_stencil_format = &depthStencilFormat;
  }

  std::vector<ffi::WGPUTextureFormat> colorFormats = {};
  for (const auto i : IntegerRange(aDesc.mColorFormats.Length())) {
    ffi::WGPUTextureFormat format = {ffi::WGPUTextureFormat_Sentinel};
    WebGPUChild::ConvertTextureFormatRef(aDesc.mColorFormats[i], format);
    colorFormats.push_back(format);
  }

  desc.color_formats = colorFormats.data();
  desc.color_formats_length = colorFormats.size();

  ipc::ByteBuf failureAction;
  auto* bundle = ffi::wgpu_device_create_render_bundle_encoder(
      aDeviceId, &desc, ToFFI(&failureAction));
  // report an error only if the operation failed
  if (!bundle &&
      !aBridge->SendDeviceAction(aDeviceId, std::move(failureAction))) {
    MOZ_CRASH("IPC failure");
  }
  return bundle;
}

RenderBundleEncoder::RenderBundleEncoder(
    Device* const aParent, WebGPUChild* const aBridge,
    const dom::GPURenderBundleEncoderDescriptor& aDesc)
    : ChildOf(aParent),
      mEncoder(CreateRenderBundleEncoder(aParent->mId, aDesc, aBridge)) {
  mValid = mEncoder.get() != nullptr;
}

RenderBundleEncoder::~RenderBundleEncoder() { Cleanup(); }

void RenderBundleEncoder::Cleanup() {
  if (mValid) {
    mValid = false;
  }
}

void RenderBundleEncoder::SetBindGroup(
    uint32_t aSlot, const BindGroup& aBindGroup,
    const dom::Sequence<uint32_t>& aDynamicOffsets) {
  if (mValid) {
    mUsedBindGroups.AppendElement(&aBindGroup);
    ffi::wgpu_render_bundle_set_bind_group(mEncoder, aSlot, aBindGroup.mId,
                                           aDynamicOffsets.Elements(),
                                           aDynamicOffsets.Length());
  }
}

void RenderBundleEncoder::SetPipeline(const RenderPipeline& aPipeline) {
  if (mValid) {
    mUsedPipelines.AppendElement(&aPipeline);
    ffi::wgpu_render_bundle_set_pipeline(mEncoder, aPipeline.mId);
  }
}

void RenderBundleEncoder::SetIndexBuffer(
    const Buffer& aBuffer, const dom::GPUIndexFormat& aIndexFormat,
    uint64_t aOffset, uint64_t aSize) {
  if (mValid) {
    mUsedBuffers.AppendElement(&aBuffer);
    const auto iformat = aIndexFormat == dom::GPUIndexFormat::Uint32
                             ? ffi::WGPUIndexFormat_Uint32
                             : ffi::WGPUIndexFormat_Uint16;
    ffi::wgpu_render_bundle_set_index_buffer(mEncoder, aBuffer.mId, iformat,
                                             aOffset, aSize);
  }
}

void RenderBundleEncoder::SetVertexBuffer(uint32_t aSlot, const Buffer& aBuffer,
                                          uint64_t aOffset, uint64_t aSize) {
  if (mValid) {
    mUsedBuffers.AppendElement(&aBuffer);
    ffi::wgpu_render_bundle_set_vertex_buffer(mEncoder, aSlot, aBuffer.mId,
                                              aOffset, aSize);
  }
}

void RenderBundleEncoder::Draw(uint32_t aVertexCount, uint32_t aInstanceCount,
                               uint32_t aFirstVertex, uint32_t aFirstInstance) {
  if (mValid) {
    ffi::wgpu_render_bundle_draw(mEncoder, aVertexCount, aInstanceCount,
                                 aFirstVertex, aFirstInstance);
  }
}

void RenderBundleEncoder::DrawIndexed(uint32_t aIndexCount,
                                      uint32_t aInstanceCount,
                                      uint32_t aFirstIndex, int32_t aBaseVertex,
                                      uint32_t aFirstInstance) {
  if (mValid) {
    ffi::wgpu_render_bundle_draw_indexed(mEncoder, aIndexCount, aInstanceCount,
                                         aFirstIndex, aBaseVertex,
                                         aFirstInstance);
  }
}

void RenderBundleEncoder::DrawIndirect(const Buffer& aIndirectBuffer,
                                       uint64_t aIndirectOffset) {
  if (mValid) {
    ffi::wgpu_render_bundle_draw_indirect(mEncoder, aIndirectBuffer.mId,
                                          aIndirectOffset);
  }
}

void RenderBundleEncoder::DrawIndexedIndirect(const Buffer& aIndirectBuffer,
                                              uint64_t aIndirectOffset) {
  if (mValid) {
    ffi::wgpu_render_bundle_draw_indexed_indirect(mEncoder, aIndirectBuffer.mId,
                                                  aIndirectOffset);
  }
}

void RenderBundleEncoder::PushDebugGroup(const nsAString& aString) {
  if (mValid) {
    const NS_ConvertUTF16toUTF8 utf8(aString);
    ffi::wgpu_render_bundle_push_debug_group(mEncoder, utf8.get());
  }
}
void RenderBundleEncoder::PopDebugGroup() {
  if (mValid) {
    ffi::wgpu_render_bundle_pop_debug_group(mEncoder);
  }
}
void RenderBundleEncoder::InsertDebugMarker(const nsAString& aString) {
  if (mValid) {
    const NS_ConvertUTF16toUTF8 utf8(aString);
    ffi::wgpu_render_bundle_insert_debug_marker(mEncoder, utf8.get());
  }
}

already_AddRefed<RenderBundle> RenderBundleEncoder::Finish(
    const dom::GPURenderBundleDescriptor& aDesc) {
  RawId id = 0;
  if (mValid) {
    mValid = false;
    auto bridge = mParent->GetBridge();
    if (bridge && bridge->CanSend()) {
      auto* encoder = mEncoder.forget();
      MOZ_ASSERT(encoder);
      id = bridge->RenderBundleEncoderFinish(*encoder, mParent->mId, aDesc);
    }
  }
  RefPtr<RenderBundle> bundle = new RenderBundle(mParent, id);
  return bundle.forget();
}

}  // namespace mozilla::webgpu
