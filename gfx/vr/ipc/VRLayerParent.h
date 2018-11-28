/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRLayerParent)

public:
  VRLayerParent(uint32_t aVRDisplayID, const uint32_t aGroup);
  virtual mozilla::ipc::IPCResult RecvSubmitFrame(const layers::SurfaceDescriptor &aTexture,
                                                  const uint64_t& aFrameId,
                                                  const gfx::Rect& aLeftEyeRect,
                                                  const gfx::Rect& aRightEyeRect) override;
  virtual mozilla::ipc::IPCResult RecvDestroy() override;
  uint32_t GetDisplayID() const { return mVRDisplayID; }
  uint32_t GetGroup() const { return mGroup; }
protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual ~VRLayerParent();
  void Destroy();

  bool mIPCOpen;

  uint32_t mVRDisplayID;
  gfx::Rect mLeftEyeRect;
  gfx::Rect mRightEyeRect;
  uint32_t mGroup;
};

} // namespace gfx
} // namespace mozilla

#endif
