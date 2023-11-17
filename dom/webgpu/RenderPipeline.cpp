/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderPipeline.h"

#include "Device.h"
#include "ipc/WebGPUChild.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(RenderPipeline, mParent)
GPU_IMPL_JS_WRAP(RenderPipeline)

RenderPipeline::RenderPipeline(Device* const aParent, RawId aId,
                               RawId aImplicitPipelineLayoutId,
                               nsTArray<RawId>&& aImplicitBindGroupLayoutIds)
    : ChildOf(aParent),
      mImplicitPipelineLayoutId(aImplicitPipelineLayoutId),
      mImplicitBindGroupLayoutIds(std::move(aImplicitBindGroupLayoutIds)),
      mId(aId) {
  MOZ_RELEASE_ASSERT(aId);
}

RenderPipeline::~RenderPipeline() { Cleanup(); }

void RenderPipeline::Cleanup() {
  if (mValid && mParent) {
    mValid = false;
    auto bridge = mParent->GetBridge();
    if (bridge && bridge->IsOpen()) {
      bridge->SendRenderPipelineDrop(mId);
      if (mImplicitPipelineLayoutId) {
        // Bug 1862759: wgpu does not yet guarantee that the implicit pipeline
        // layout was actually created, and requesting its destruction in such
        // a case will crash the parent process. Until this is fixed, we leak
        // all implicit pipeline layouts and bind group layouts.
        /*
        bridge->SendImplicitLayoutDrop(mImplicitPipelineLayoutId,
                                       mImplicitBindGroupLayoutIds);
        */
      }
    }
  }
}

already_AddRefed<BindGroupLayout> RenderPipeline::GetBindGroupLayout(
    uint32_t aIndex) const {
  auto bridge = mParent->GetBridge();
  auto* client = bridge->GetClient();

  ipc::ByteBuf bb;
  const RawId bglId = ffi::wgpu_client_render_pipeline_get_bind_group_layout(
      client, mId, aIndex, ToFFI(&bb));

  if (!bridge->SendDeviceAction(mParent->GetId(), std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }

  RefPtr<BindGroupLayout> object = new BindGroupLayout(mParent, bglId, false);
  return object.forget();
}

}  // namespace mozilla::webgpu
