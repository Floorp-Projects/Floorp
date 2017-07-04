/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "VRLayerParent.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace gfx {

VRLayerParent::VRLayerParent(uint32_t aVRDisplayID, const Rect& aLeftEyeRect, const Rect& aRightEyeRect, const uint32_t aGroup)
  : mIPCOpen(true)
  , mVRDisplayID(aVRDisplayID)
  , mLeftEyeRect(aLeftEyeRect)
  , mRightEyeRect(aRightEyeRect)
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
VRLayerParent::RecvSubmitFrame(PTextureParent* texture)
{
  if (mVRDisplayID) {
    VRManager* vm = VRManager::Get();
    vm->SubmitFrame(this, texture, mLeftEyeRect, mRightEyeRect);
  }

  return IPC_OK();
}


} // namespace gfx
} // namespace mozilla
