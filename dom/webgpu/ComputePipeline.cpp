/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ComputePipeline.h"

#include "Device.h"
#include "ipc/WebGPUChild.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(ComputePipeline, mParent)
GPU_IMPL_JS_WRAP(ComputePipeline)

ComputePipeline::ComputePipeline(Device* const aParent, RawId aId,
                                 RawId aImplicitPipelineLayoutId,
                                 nsTArray<RawId>&& aImplicitBindGroupLayoutIds)
    : ChildOf(aParent),
      mImplicitPipelineLayoutId(aImplicitPipelineLayoutId),
      mImplicitBindGroupLayoutIds(std::move(aImplicitBindGroupLayoutIds)),
      mId(aId) {
  MOZ_RELEASE_ASSERT(aId);
}

ComputePipeline::~ComputePipeline() { Cleanup(); }

void ComputePipeline::Cleanup() {
  if (mValid && mParent) {
    mValid = false;
    auto bridge = mParent->GetBridge();
    if (bridge && bridge->IsOpen()) {
      bridge->SendComputePipelineDrop(mId);
      if (mImplicitPipelineLayoutId) {
        bridge->SendImplicitLayoutDrop(mImplicitPipelineLayoutId,
                                       mImplicitBindGroupLayoutIds);
      }
    }
  }
}

already_AddRefed<BindGroupLayout> ComputePipeline::GetBindGroupLayout(
    uint32_t aIndex) const {
  auto bridge = mParent->GetBridge();
  auto* client = bridge->GetClient();

  ipc::ByteBuf bb;
  const RawId bglId = ffi::wgpu_client_compute_pipeline_get_bind_group_layout(
      client, mId, aIndex, ToFFI(&bb));

  if (!bridge->SendDeviceAction(mParent->GetId(), std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }

  RefPtr<BindGroupLayout> object = new BindGroupLayout(mParent, bglId, false);
  return object.forget();
}

}  // namespace mozilla::webgpu
