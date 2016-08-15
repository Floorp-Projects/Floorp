/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_MANAGER_H
#define GFX_VR_MANAGER_H

#include "nsRefPtrHashtable.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsDataHashtable.h"
#include "mozilla/TimeStamp.h"
#include "gfxVR.h"

namespace mozilla {
namespace layers {
class TextureHost;
}
namespace gfx {

class VRLayerParent;
class VRManagerParent;
class VRDisplayHost;

class VRManager
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::gfx::VRManager)

public:
  static void ManagerInit();
  static VRManager* Get();

  void AddVRManagerParent(VRManagerParent* aVRManagerParent);
  void RemoveVRManagerParent(VRManagerParent* aVRManagerParent);

  void NotifyVsync(const TimeStamp& aVsyncTimestamp);
  void NotifyVRVsync(const uint32_t& aDisplayID);
  void RefreshVRDisplays(bool aMustDispatch = false);
  RefPtr<gfx::VRDisplayHost> GetDisplay(const uint32_t& aDisplayID);

  void SubmitFrame(VRLayerParent* aLayer, const int32_t& aInputFrameID,
                   layers::PTextureParent* aTexture, const gfx::Rect& aLeftEyeRect,
                   const gfx::Rect& aRightEyeRect);

protected:
  VRManager();
  ~VRManager();

private:
  RefPtr<layers::TextureHost> mLastFrame;

  void Init();
  void Destroy();

  void DispatchVRDisplayInfoUpdate();

  typedef nsTHashtable<nsRefPtrHashKey<VRManagerParent>> VRManagerParentSet;
  VRManagerParentSet mVRManagerParents;

  typedef nsTArray<RefPtr<VRDisplayManager>> VRDisplayManagerArray;
  VRDisplayManagerArray mManagers;

  typedef nsRefPtrHashtable<nsUint32HashKey, gfx::VRDisplayHost> VRDisplayHostHashMap;
  VRDisplayHostHashMap mVRDisplays;

  Atomic<bool> mInitialized;

  TimeStamp mLastRefreshTime;
};

} // namespace gfx
} // namespace mozilla

#endif // GFX_VR_MANAGER_H
