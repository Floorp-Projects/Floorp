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

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(ComputePassEncoder, mParent, mUsedBindGroups,
                          mUsedPipelines)
GPU_IMPL_JS_WRAP(ComputePassEncoder)

ffi::WGPUComputePass* ScopedFfiComputeTraits::empty() { return nullptr; }

void ScopedFfiComputeTraits::release(ffi::WGPUComputePass* raw) {
  if (raw) {
    ffi::wgpu_compute_pass_destroy(raw);
  }
}

ffi::WGPUComputePass* BeginComputePass(
    RawId aEncoderId, const dom::GPUComputePassDescriptor& aDesc) {
  ffi::WGPUComputePassDescriptor desc = {};
  Unused << aDesc;  // no useful fields
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
    ffi::wgpu_compute_pass_set_bind_group(mPass, aSlot, aBindGroup.mId,
                                          aDynamicOffsets.Elements(),
                                          aDynamicOffsets.Length());
  }
}

void ComputePassEncoder::SetPipeline(const ComputePipeline& aPipeline) {
  if (mValid) {
    mUsedPipelines.AppendElement(&aPipeline);
    ffi::wgpu_compute_pass_set_pipeline(mPass, aPipeline.mId);
  }
}

void ComputePassEncoder::Dispatch(uint32_t x, uint32_t y, uint32_t z) {
  if (mValid) {
    ffi::wgpu_compute_pass_dispatch(mPass, x, y, z);
  }
}

void ComputePassEncoder::DispatchIndirect(const Buffer& aIndirectBuffer,
                                          uint64_t aIndirectOffset) {
  if (mValid) {
    ffi::wgpu_compute_pass_dispatch_indirect(mPass, aIndirectBuffer.mId,
                                             aIndirectOffset);
  }
}

void ComputePassEncoder::EndPass(ErrorResult& aRv) {
  if (mValid) {
    mValid = false;
    auto* pass = mPass.forget();
    MOZ_ASSERT(pass);
    mParent->EndComputePass(*pass, aRv);
  }
}

}  // namespace webgpu
}  // namespace mozilla
