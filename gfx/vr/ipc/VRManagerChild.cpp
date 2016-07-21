/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRManagerChild.h"
#include "VRManagerParent.h"
#include "VRDeviceProxy.h"
#include "VRDeviceProxyOrientationFallBack.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/layers/CompositorThread.h" // for CompositorThread
#include "mozilla/dom/Navigator.h"

namespace mozilla {
namespace gfx {

static StaticRefPtr<VRManagerChild> sVRManagerChildSingleton;
static StaticRefPtr<VRManagerParent> sVRManagerParentSingleton;

void ReleaseVRManagerParentSingleton() {
  sVRManagerParentSingleton = nullptr;
}

VRManagerChild::VRManagerChild()
  : mInputFrameID(-1)
{
  MOZ_COUNT_CTOR(VRManagerChild);
  MOZ_ASSERT(NS_IsMainThread());
}

VRManagerChild::~VRManagerChild()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_DTOR(VRManagerChild);
}

/*static*/ VRManagerChild*
VRManagerChild::Get()
{
  MOZ_ASSERT(sVRManagerChildSingleton);
  return sVRManagerChildSingleton;
}

/* static */ bool
VRManagerChild::IsCreated()
{
  return !!sVRManagerChildSingleton;
}

/* static */ bool
VRManagerChild::InitForContent(Endpoint<PVRManagerChild>&& aEndpoint)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sVRManagerChildSingleton);

  RefPtr<VRManagerChild> child(new VRManagerChild());
  if (!aEndpoint.Bind(child, nullptr)) {
    NS_RUNTIMEABORT("Couldn't Open() Compositor channel.");
    return false;
  }
  sVRManagerChildSingleton = child;
  return true;
}

/*static*/ void
VRManagerChild::InitSameProcess()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sVRManagerChildSingleton);

  sVRManagerChildSingleton = new VRManagerChild();
  sVRManagerParentSingleton = VRManagerParent::CreateSameProcess();
  sVRManagerChildSingleton->Open(sVRManagerParentSingleton->GetIPCChannel(),
                                 mozilla::layers::CompositorThreadHolder::Loop(),
                                 mozilla::ipc::ChildSide);
}

/* static */ void
VRManagerChild::InitWithGPUProcess(Endpoint<PVRManagerChild>&& aEndpoint)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sVRManagerChildSingleton);

  sVRManagerChildSingleton = new VRManagerChild();
  if (!aEndpoint.Bind(sVRManagerChildSingleton, nullptr)) {
    NS_RUNTIMEABORT("Couldn't Open() Compositor channel.");
    return;
  }
}

/*static*/ void
VRManagerChild::ShutDown()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sVRManagerChildSingleton) {
    sVRManagerChildSingleton->Destroy();
    sVRManagerChildSingleton = nullptr;
  }
}

/*static*/ void
VRManagerChild::DeferredDestroy(RefPtr<VRManagerChild> aVRManagerChild)
{
    aVRManagerChild->Close();
}

void
VRManagerChild::Destroy()
{
  // This must not be called from the destructor!
  MOZ_ASSERT(mRefCnt != 0);

  // Keep ourselves alive until everything has been shut down
  RefPtr<VRManagerChild> selfRef = this;

  // The DeferredDestroyVRManager task takes ownership of
  // the VRManagerChild and will release it when it runs.
  MessageLoop::current()->PostTask(
             NewRunnableFunction(DeferredDestroy, selfRef));
}

bool
VRManagerChild::RecvUpdateDeviceInfo(nsTArray<VRDeviceUpdate>&& aDeviceUpdates)
{
  // mDevices could be a hashed container for more scalability, but not worth
  // it now as we expect < 10 entries.
  nsTArray<RefPtr<VRDeviceProxy> > devices;
  for (auto& deviceUpdate: aDeviceUpdates) {
    bool isNewDevice = true;
    for (auto& device: mDevices) {
      if (device->GetDeviceInfo().GetDeviceID() == deviceUpdate.mDeviceInfo.GetDeviceID()) {
        device->UpdateDeviceInfo(deviceUpdate);
        devices.AppendElement(device);
        isNewDevice = false;
        break;
      }
    }
    if (isNewDevice) {
      if (deviceUpdate.mDeviceInfo.GetUseMainThreadOrientation()) {
        devices.AppendElement(new VRDeviceProxyOrientationFallBack(deviceUpdate));
      } else {
        devices.AppendElement(new VRDeviceProxy(deviceUpdate));
      }
    }
  }

  mDevices = devices;


  for (auto& nav: mNavigatorCallbacks) {
    nav->NotifyVRDevicesUpdated();
  }
  mNavigatorCallbacks.Clear();

  return true;
}

bool
VRManagerChild::RecvUpdateDeviceSensors(nsTArray<VRSensorUpdate>&& aDeviceSensorUpdates)
{
  // mDevices could be a hashed container for more scalability, but not worth
  // it now as we expect < 10 entries.
  for (auto& sensorUpdate: aDeviceSensorUpdates) {
    for (auto& device: mDevices) {
      if (device->GetDeviceInfo().GetDeviceID() == sensorUpdate.mDeviceID) {
        device->UpdateSensorState(sensorUpdate.mSensorState);
        mInputFrameID = sensorUpdate.mSensorState.inputFrameID;
        break;
      }
    }
  }

  return true;
}

bool
VRManagerChild::GetVRDevices(nsTArray<RefPtr<VRDeviceProxy> >& aDevices)
{
  aDevices = mDevices;
  return true;
}

bool
VRManagerChild::RefreshVRDevicesWithCallback(dom::Navigator* aNavigator)
{
  bool success = SendRefreshDevices();
  if (success) {
    mNavigatorCallbacks.AppendElement(aNavigator);
  }
  return success;
}

int
VRManagerChild::GetInputFrameID()
{
  return mInputFrameID;
}

} // namespace gfx
} // namespace mozilla
