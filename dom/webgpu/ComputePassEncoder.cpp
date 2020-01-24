/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "ComputePassEncoder.h"
#include "BindGroup.h"
#include "ComputePipeline.h"

#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla {
namespace webgpu {

NS_IMPL_CYCLE_COLLECTION_INHERITED(ComputePassEncoder, ProgrammablePassEncoder,
                                   mParent)
NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(ComputePassEncoder,
                                               ProgrammablePassEncoder)
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ComputePassEncoder,
                                               ProgrammablePassEncoder)
NS_IMPL_CYCLE_COLLECTION_TRACE_END
GPU_IMPL_JS_WRAP(ComputePassEncoder)

ffi::WGPURawPass BeginComputePass(RawId aEncoderId,
                                  const dom::GPUComputePassDescriptor& aDesc) {
  ffi::WGPUComputePassDescriptor desc = {};
  Unused << aDesc;  // no useful fields
  return ffi::wgpu_command_encoder_begin_compute_pass(aEncoderId, &desc);
}

ComputePassEncoder::ComputePassEncoder(
    CommandEncoder* const aParent, const dom::GPUComputePassDescriptor& aDesc)
    : ChildOf(aParent), mRaw(BeginComputePass(aParent->mId, aDesc)) {}

ComputePassEncoder::~ComputePassEncoder() {
  if (mValid) {
    mValid = false;
    ffi::wgpu_compute_pass_destroy(mRaw);
  }
}

void ComputePassEncoder::SetBindGroup(
    uint32_t aSlot, const BindGroup& aBindGroup,
    const dom::Sequence<uint32_t>& aDynamicOffsets) {
  if (mValid) {
    ffi::wgpu_compute_pass_set_bind_group(&mRaw, aSlot, aBindGroup.mId,
                                          aDynamicOffsets.Elements(),
                                          aDynamicOffsets.Length());
  }
}

void ComputePassEncoder::SetPipeline(const ComputePipeline& aPipeline) {
  if (mValid) {
    ffi::wgpu_compute_pass_set_pipeline(&mRaw, aPipeline.mId);
  }
}

void ComputePassEncoder::Dispatch(uint32_t x, uint32_t y, uint32_t z) {
  if (mValid) {
    ffi::wgpu_compute_pass_dispatch(&mRaw, x, y, z);
  }
}

void ComputePassEncoder::EndPass(ErrorResult& aRv) {
  if (mValid) {
    mValid = false;
    uintptr_t length = 0;
    const uint8_t* pass_data = ffi::wgpu_compute_pass_finish(&mRaw, &length);
    mParent->EndComputePass(Span(pass_data, length), aRv);
    ffi::wgpu_compute_pass_destroy(mRaw);
  }
}

}  // namespace webgpu
}  // namespace mozilla
