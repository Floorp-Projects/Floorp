/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "BindGroupLayout.h"
#include "ipc/WebGPUChild.h"

#include "Device.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(BindGroupLayout, mParent)
GPU_IMPL_JS_WRAP(BindGroupLayout)

BindGroupLayout::BindGroupLayout(Device* const aParent, RawId aId, bool aOwning)
    : ChildOf(aParent), mId(aId), mOwning(aOwning) {
  MOZ_RELEASE_ASSERT(aId);
}

BindGroupLayout::~BindGroupLayout() { Cleanup(); }

void BindGroupLayout::Cleanup() {
  if (!mValid) {
    return;
  }
  mValid = false;

  auto bridge = mParent->GetBridge();
  if (!bridge) {
    return;
  }

  if (mOwning) {
    if (bridge->CanSend()) {
      bridge->SendBindGroupLayoutDrop(mId);
    }

    wgpu_client_free_bind_group_layout_id(bridge->GetClient(), mId);
  }
}

}  // namespace mozilla::webgpu
