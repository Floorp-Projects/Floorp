/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "VRLayerParent.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace gfx {

VRLayerParent::VRLayerParent(uint32_t aVRDisplayID, const Rect& aLeftEyeRect, const Rect& aRightEyeRect)
  : mIPCOpen(true)
  , mVRDisplayID(aVRDisplayID)
  , mLeftEyeRect(aLeftEyeRect)
  , mRightEyeRect(aRightEyeRect)
{
  MOZ_COUNT_CTOR(VRLayerParent);
}

VRLayerParent::~VRLayerParent()
{
  MOZ_COUNT_DTOR(VRLayerParent);
}

bool
VRLayerParent::RecvDestroy()
{
  Destroy();
  return true;
}

void
VRLayerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mIPCOpen = false;
}

void
VRLayerParent::Destroy()
{
  if (mIPCOpen) {
    Unused << PVRLayerParent::Send__delete__(this);
  }
}

bool
VRLayerParent::RecvSubmitFrame(const int32_t& aInputFrameID,
                               PTextureParent* texture)
{
  VRManager* vm = VRManager::Get();
  vm->SubmitFrame(this, aInputFrameID, texture, mLeftEyeRect, mRightEyeRect);

  return true;
}


} // namespace gfx
} // namespace mozilla
