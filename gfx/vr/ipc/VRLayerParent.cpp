/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "VRLayerParent.h"
#include "mozilla/Unused.h"
#include "VRDisplayHost.h"
#include "mozilla/layers/CompositorThread.h"

namespace mozilla {
using namespace layers;
namespace gfx {

VRLayerParent::VRLayerParent(uint32_t aVRDisplayID, const uint32_t aGroup)
  : mIPCOpen(true)
  , mVRDisplayID(aVRDisplayID)
  , mGroup(aGroup)
{
}

VRLayerParent::~VRLayerParent()
{
  MOZ_COUNT_DTOR(VRLayerParent);
}

mozilla::ipc::IPCResult
VRLayerParent::RecvDestroy()
{
  Destroy();
  return IPC_OK();
}

void
VRLayerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mIPCOpen = false;
}

void
VRLayerParent::Destroy()
{
  if (mVRDisplayID) {
    VRManager* vm = VRManager::Get();
    RefPtr<gfx::VRDisplayHost> display = vm->GetDisplay(mVRDisplayID);
    if (display) {
      display->RemoveLayer(this);
    }
    // 0 will never be a valid VRDisplayID; we can use it to indicate that
    // we are destroyed and no longer associated with a display.
    mVRDisplayID = 0;
  }

  if (mIPCOpen) {
    Unused << PVRLayerParent::Send__delete__(this);
  }
}

mozilla::ipc::IPCResult
VRLayerParent::RecvSubmitFrame(const layers::SurfaceDescriptor &aTexture,
                               const uint64_t& aFrameId,
                               const gfx::Rect& aLeftEyeRect,
                               const gfx::Rect& aRightEyeRect)
{
  if (mVRDisplayID) {
    VRManager* vm = VRManager::Get();
    RefPtr<VRDisplayHost> display = vm->GetDisplay(mVRDisplayID);
    if (display) {
      display->SubmitFrame(this, aTexture, aFrameId, aLeftEyeRect, aRightEyeRect);
    }
  }

  return IPC_OK();
}

} // namespace gfx
} // namespace mozilla
