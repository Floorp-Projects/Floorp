/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_MANAGER_H
#define GFX_VR_MANAGER_H

#include "nsRefPtrHashtable.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "mozilla/TimeStamp.h"
#include "gfxVR.h"

namespace mozilla {
namespace gfx {

class VRManagerParent;
class VRHMDInfo;

class VRManager
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::gfx::VRManager)

public:
  static void ManagerInit();
  static VRManager* Get();

  void AddVRManagerParent(VRManagerParent* aVRManagerParent);
  void RemoveVRManagerParent(VRManagerParent* aVRManagerParent);

  void NotifyVsync(const TimeStamp& aVsyncTimestamp);
  void RefreshVRDevices();
  RefPtr<gfx::VRHMDInfo> GetDevice(const uint32_t& aDeviceID);

protected:
  VRManager();
  ~VRManager();

private:

  void Init();
  void Destroy();

  void DispatchVRDeviceInfoUpdate();
  void DispatchVRDeviceSensorUpdate();

  typedef nsTHashtable<nsRefPtrHashKey<VRManagerParent>> VRManagerParentSet;
  VRManagerParentSet mVRManagerParents;

  typedef nsTArray<RefPtr<VRHMDManager>> VRHMDManagerArray;
  VRHMDManagerArray mManagers;

  typedef nsRefPtrHashtable<nsUint32HashKey, gfx::VRHMDInfo> VRHMDInfoHashMap;
  VRHMDInfoHashMap mVRDevices;

  Atomic<bool> mInitialized;

};

} // namespace gfx
} // namespace mozilla

#endif // GFX_VR_MANAGER_H
