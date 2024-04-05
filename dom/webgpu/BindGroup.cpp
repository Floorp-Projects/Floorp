/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "BindGroup.h"
#include "ipc/WebGPUChild.h"

#include "Device.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(BindGroup, mParent)
GPU_IMPL_JS_WRAP(BindGroup)

BindGroup::BindGroup(Device* const aParent, RawId aId)
    : ChildOf(aParent), mId(aId) {
  MOZ_RELEASE_ASSERT(aId);
}

BindGroup::~BindGroup() { Cleanup(); }

void BindGroup::Cleanup() {
  if (!mValid) {
    return;
  }
  mValid = false;

  auto bridge = mParent->GetBridge();
  if (!bridge) {
    return;
  }

  if (bridge->CanSend()) {
    bridge->SendBindGroupDrop(mId);
  }

  wgpu_client_free_bind_group_id(bridge->GetClient(), mId);
}

}  // namespace mozilla::webgpu
