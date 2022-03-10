/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRLayerParent.h"
#include "VRManager.h"
#include "mozilla/Unused.h"
#include "mozilla/layers/CompositorThread.h"

namespace mozilla {
using namespace layers;
namespace gfx {

VRLayerParent::VRLayerParent(uint32_t aVRDisplayID, const uint32_t aGroup)
    : mIPCOpen(true), mDestroyed(false), mGroup(aGroup) {}

VRLayerParent::~VRLayerParent() {
  Destroy();
  MOZ_COUNT_DTOR(VRLayerParent);
}

mozilla::ipc::IPCResult VRLayerParent::RecvDestroy() {
  Destroy();
  return IPC_OK();
}

void VRLayerParent::ActorDestroy(ActorDestroyReason aWhy) { mIPCOpen = false; }

void VRLayerParent::Destroy() {
  if (!mDestroyed) {
    VRManager* vm = VRManager::Get();
    vm->RemoveLayer(this);
    mDestroyed = true;
  }

  if (mIPCOpen) {
    Unused << PVRLayerParent::Send__delete__(this);
  }
}

mozilla::ipc::IPCResult VRLayerParent::RecvSubmitFrame(
    const layers::SurfaceDescriptor& aTexture, const uint64_t& aFrameId,
    const gfx::Rect& aLeftEyeRect, const gfx::Rect& aRightEyeRect) {
  if (!mDestroyed) {
    VRManager* vm = VRManager::Get();
    vm->SubmitFrame(this, aTexture, aFrameId, aLeftEyeRect, aRightEyeRect);
  }

  return IPC_OK();
}

}  // namespace gfx
}  // namespace mozilla
