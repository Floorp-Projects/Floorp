/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "ComputePassEncoder.h"
#include "BindGroup.h"
#include "ComputePipeline.h"
#include "CommandEncoder.h"

#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(ComputePassEncoder, mParent, mUsedBindGroups,
                          mUsedPipelines)
GPU_IMPL_JS_WRAP(ComputePassEncoder)

void ffiWGPUComputePassDeleter::operator()(ffi::WGPUComputePass* raw) {
  if (raw) {
    ffi::wgpu_compute_pass_destroy(raw);
  }
}

ffi::WGPUComputePass* BeginComputePass(
    RawId aEncoderId, const dom::GPUComputePassDescriptor& aDesc) {
  MOZ_RELEASE_ASSERT(aEncoderId);
  ffi::WGPUComputePassDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  return ffi::wgpu_command_encoder_begin_compute_pass(aEncoderId, &desc);
}

ComputePassEncoder::ComputePassEncoder(
    CommandEncoder* const aParent, const dom::GPUComputePassDescriptor& aDesc)
    : ChildOf(aParent), mPass(BeginComputePass(aParent->mId, aDesc)) {}

ComputePassEncoder::~ComputePassEncoder() {
  if (mValid) {
    mValid = false;
  }
}

void ComputePassEncoder::SetBindGroup(
    uint32_t aSlot, const BindGroup& aBindGroup,
    const dom::Sequence<uint32_t>& aDynamicOffsets) {
  if (mValid) {
    mUsedBindGroups.AppendElement(&aBindGroup);
    ffi::wgpu_compute_pass_set_bind_group(mPass.get(), aSlot, aBindGroup.mId,
                                          aDynamicOffsets.Elements(),
                                          aDynamicOffsets.Length());
  }
}

void ComputePassEncoder::SetPipeline(const ComputePipeline& aPipeline) {
  if (mValid) {
    mUsedPipelines.AppendElement(&aPipeline);
    ffi::wgpu_compute_pass_set_pipeline(mPass.get(), aPipeline.mId);
  }
}

void ComputePassEncoder::DispatchWorkgroups(uint32_t workgroupCountX,
                                            uint32_t workgroupCountY,
                                            uint32_t workgroupCountZ) {
  if (mValid) {
    ffi::wgpu_compute_pass_dispatch_workgroups(
        mPass.get(), workgroupCountX, workgroupCountY, workgroupCountZ);
  }
}

void ComputePassEncoder::DispatchWorkgroupsIndirect(
    const Buffer& aIndirectBuffer, uint64_t aIndirectOffset) {
  if (mValid) {
    ffi::wgpu_compute_pass_dispatch_workgroups_indirect(
        mPass.get(), aIndirectBuffer.mId, aIndirectOffset);
  }
}

void ComputePassEncoder::PushDebugGroup(const nsAString& aString) {
  if (mValid) {
    const NS_ConvertUTF16toUTF8 utf8(aString);
    ffi::wgpu_compute_pass_push_debug_group(mPass.get(), utf8.get(), 0);
  }
}
void ComputePassEncoder::PopDebugGroup() {
  if (mValid) {
    ffi::wgpu_compute_pass_pop_debug_group(mPass.get());
  }
}
void ComputePassEncoder::InsertDebugMarker(const nsAString& aString) {
  if (mValid) {
    const NS_ConvertUTF16toUTF8 utf8(aString);
    ffi::wgpu_compute_pass_insert_debug_marker(mPass.get(), utf8.get(), 0);
  }
}

void ComputePassEncoder::End() {
  if (mValid) {
    mValid = false;
    auto* pass = mPass.release();
    MOZ_ASSERT(pass);
    mParent->EndComputePass(*pass);
  }
}

}  // namespace mozilla::webgpu
