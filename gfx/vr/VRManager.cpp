/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "VRManager.h"
#include "VRManagerParent.h"
#include "gfxVR.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/VRDevice.h"
#include "mozilla/unused.h"

#include "gfxPrefs.h"
#include "gfxVR.h"
#if defined(XP_WIN)
#include "gfxVROculus.h"
#endif
#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_LINUX)
#include "gfxVROculus050.h"
#endif
#include "gfxVRCardboard.h"


namespace mozilla {
namespace gfx {

static StaticRefPtr<VRManager> sVRManagerSingleton;

/*static*/ void
VRManager::ManagerInit()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sVRManagerSingleton == nullptr) {
    sVRManagerSingleton = new VRManager();
    ClearOnShutdown(&sVRManagerSingleton);
  }
}

VRManager::VRManager()
  : mInitialized(false)
{
  MOZ_COUNT_CTOR(VRManager);
  MOZ_ASSERT(sVRManagerSingleton == nullptr);

  RefPtr<VRHMDManager> mgr;

  // we'll only load the 0.5.0 oculus runtime if
  // the >= 0.6.0 one failed to load; otherwise
  // we might initialize oculus twice
  bool useOculus050 = true;
  Unused << useOculus050;

#if defined(XP_WIN)
  mgr = VRHMDManagerOculus::Create();
  if (mgr) {
    useOculus050 = false;
    mManagers.AppendElement(mgr);
  }
#endif

#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_LINUX)
  if (useOculus050) {
    mgr = VRHMDManagerOculus050::Create();
    if (mgr) {
      mManagers.AppendElement(mgr);
    }
  }
#endif

  mgr = VRHMDManagerCardboard::Create();
  if (mgr) {
    mManagers.AppendElement(mgr);
  }
}

VRManager::~VRManager()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInitialized);
  MOZ_COUNT_DTOR(VRManager);
}

void
VRManager::Destroy()
{
  for (uint32_t i = 0; i < mManagers.Length(); ++i) {
    mManagers[i]->Destroy();
  }
  mInitialized = false;
}

void
VRManager::Init()
{
  for (uint32_t i = 0; i < mManagers.Length(); ++i) {
    mManagers[i]->Init();
  }
  mInitialized = true;
}

/* static */VRManager*
VRManager::Get()
{
  MOZ_ASSERT(sVRManagerSingleton != nullptr);

  return sVRManagerSingleton;
}

void
VRManager::AddVRManagerParent(VRManagerParent* aVRManagerParent)
{
  if (mVRManagerParents.IsEmpty()) {
    Init();
  }
  mVRManagerParents.PutEntry(aVRManagerParent);
}

void
VRManager::RemoveVRManagerParent(VRManagerParent* aVRManagerParent)
{
  mVRManagerParents.RemoveEntry(aVRManagerParent);
  if (mVRManagerParents.IsEmpty()) {
    Destroy();
  }
}

void
VRManager::NotifyVsync(const TimeStamp& aVsyncTimestamp)
{
  for (auto iter = mVRDevices.Iter(); !iter.Done(); iter.Next()) {
    gfx::VRHMDInfo* device = iter.UserData();
    device->NotifyVsync(aVsyncTimestamp);
  }
  DispatchVRDeviceSensorUpdate();
}

void
VRManager::RefreshVRDevices()
{
  nsTArray<RefPtr<gfx::VRHMDInfo> > devices;

  for (uint32_t i = 0; i < mManagers.Length(); ++i) {
    mManagers[i]->GetHMDs(devices);
  }

  bool deviceInfoChanged = false;

  if (devices.Length() != mVRDevices.Count()) {
    deviceInfoChanged = true;
  }

  for (const auto& device: devices) {
    RefPtr<VRHMDInfo> oldDevice = GetDevice(device->GetDeviceInfo().GetDeviceID());
    if (oldDevice == nullptr) {
      deviceInfoChanged = true;
      break;
    }
    if (oldDevice->GetDeviceInfo() != device->GetDeviceInfo()) {
      deviceInfoChanged = true;
      break;
    }
  }

  if (deviceInfoChanged) {
    mVRDevices.Clear();
    for (const auto& device: devices) {
      mVRDevices.Put(device->GetDeviceInfo().GetDeviceID(), device);
    }
  }

  DispatchVRDeviceInfoUpdate();
}

void
VRManager::DispatchVRDeviceInfoUpdate()
{
  nsTArray<VRDeviceUpdate> update;
  for (auto iter = mVRDevices.Iter(); !iter.Done(); iter.Next()) {
    gfx::VRHMDInfo* device = iter.UserData();
    update.AppendElement(VRDeviceUpdate(device->GetDeviceInfo(),
                                        device->GetSensorState()));
  }

  for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
    Unused << iter.Get()->GetKey()->SendUpdateDeviceInfo(update);
  }
}

void
VRManager::DispatchVRDeviceSensorUpdate()
{
  nsTArray<VRSensorUpdate> update;

  for (auto iter = mVRDevices.Iter(); !iter.Done(); iter.Next()) {
    gfx::VRHMDInfo* device = iter.UserData();
    if (!device->GetDeviceInfo().GetUseMainThreadOrientation()) {
      update.AppendElement(VRSensorUpdate(device->GetDeviceInfo().GetDeviceID(),
                                          device->GetSensorState()));
    }
  }
  if (update.Length() > 0) {
    for (auto iter = mVRManagerParents.Iter(); !iter.Done(); iter.Next()) {
      Unused << iter.Get()->GetKey()->SendUpdateDeviceSensors(update);
    }
  }
}

RefPtr<gfx::VRHMDInfo>
VRManager::GetDevice(const uint32_t& aDeviceID)
{
  RefPtr<gfx::VRHMDInfo> device;
  if (mVRDevices.Get(aDeviceID, getter_AddRefs(device))) {
    return device;
  }
  return nullptr;
}

} // namespace gfx
} // namespace mozilla
