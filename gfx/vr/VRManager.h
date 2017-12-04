/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
  void RefreshVRControllers();
  void ScanForControllers();
  void RemoveControllers();
  template<class T> void NotifyGamepadChange(uint32_t aIndex, const T& aInfo);
  RefPtr<gfx::VRDisplayHost> GetDisplay(const uint32_t& aDisplayID);
  void GetVRDisplayInfo(nsTArray<VRDisplayInfo>& aDisplayInfo);
  RefPtr<gfx::VRControllerHost> GetController(const uint32_t& aControllerID);
  void GetVRControllerInfo(nsTArray<VRControllerInfo>& aControllerInfo);
  void CreateVRTestSystem();
  void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                     double aIntensity, double aDuration, const VRManagerPromise& aPromise);
  void StopVibrateHaptic(uint32_t aControllerIdx);
  void NotifyVibrateHapticCompleted(const VRManagerPromise& aPromise);
  void DispatchSubmitFrameResult(uint32_t aDisplayID, const VRSubmitFrameResultInfo& aResult);

protected:
  VRManager();
  ~VRManager();

private:

  void Init();
  void Destroy();
  void Shutdown();

  void DispatchVRDisplayInfoUpdate();

  typedef nsTHashtable<nsRefPtrHashKey<VRManagerParent>> VRManagerParentSet;
  VRManagerParentSet mVRManagerParents;

  typedef nsTArray<RefPtr<VRSystemManager>> VRSystemManagerArray;
  VRSystemManagerArray mManagers;

  typedef nsRefPtrHashtable<nsUint32HashKey, gfx::VRDisplayHost> VRDisplayHostHashMap;
  VRDisplayHostHashMap mVRDisplays;

  typedef nsRefPtrHashtable<nsUint32HashKey, gfx::VRControllerHost> VRControllerHostHashMap;
  VRControllerHostHashMap mVRControllers;

  Atomic<bool> mInitialized;

  TimeStamp mLastRefreshTime;
  TimeStamp mLastActiveTime;
  bool mVRTestSystemCreated;
};

} // namespace gfx
} // namespace mozilla

#endif // GFX_VR_MANAGER_H
