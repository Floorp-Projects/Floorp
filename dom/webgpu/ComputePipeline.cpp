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
  if (!mValid) {
    return;
  }
  mValid = false;

  auto bridge = mParent->GetBridge();
  if (!bridge) {
    return;
  }

  if (bridge->CanSend()) {
    bridge->SendComputePipelineDrop(mId);
    if (mImplicitPipelineLayoutId) {
      bridge->SendImplicitLayoutDrop(mImplicitPipelineLayoutId,
                                     mImplicitBindGroupLayoutIds);
    }
  }

  if (mImplicitPipelineLayoutId) {
    wgpu_client_free_pipeline_layout_id(bridge->GetClient(),
                                        mImplicitPipelineLayoutId);
  }

  for (const auto& id : mImplicitBindGroupLayoutIds) {
    wgpu_client_free_bind_group_layout_id(bridge->GetClient(), id);
  }
}

already_AddRefed<BindGroupLayout> ComputePipeline::GetBindGroupLayout(
    uint32_t aIndex) const {
  auto bridge = mParent->GetBridge();
  MOZ_ASSERT(bridge && bridge->CanSend());
  auto* client = bridge->GetClient();

  ipc::ByteBuf bb;
  const RawId bglId = ffi::wgpu_client_compute_pipeline_get_bind_group_layout(
      client, mId, aIndex, ToFFI(&bb));

  bridge->SendDeviceAction(mParent->GetId(), std::move(bb));

  RefPtr<BindGroupLayout> object = new BindGroupLayout(mParent, bglId, false);
  return object.forget();
}

}  // namespace mozilla::webgpu
