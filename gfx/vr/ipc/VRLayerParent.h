/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_LAYERPARENT_H
#define GFX_VR_LAYERPARENT_H

#include "VRManager.h"

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/PVRLayerParent.h"
#include "gfxVR.h"

namespace mozilla {
namespace gfx {

class VRLayerParent : public PVRLayerParent {
  NS_INLINE_DECL_REFCOUNTING(VRLayerParent)

public:
  VRLayerParent(uint32_t aVRDisplayID, const Rect& aLeftEyeRect, const Rect& aRightEyeRect);
  virtual mozilla::ipc::IPCResult RecvSubmitFrame(PTextureParent* texture) override;
  virtual mozilla::ipc::IPCResult RecvDestroy() override;
  uint32_t GetDisplayID() const { return mVRDisplayID; }
protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual ~VRLayerParent();
  void Destroy();

  bool mIPCOpen;

  uint32_t mVRDisplayID;
  gfx::IntSize mSize;
  gfx::Rect mLeftEyeRect;
  gfx::Rect mRightEyeRect;
};

} // namespace gfx
} // namespace mozilla

#endif
