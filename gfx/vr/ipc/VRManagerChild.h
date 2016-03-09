/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_VR_VRMANAGERCHILD_H
#define MOZILLA_GFX_VR_VRMANAGERCHILD_H

#include "mozilla/gfx/PVRManagerChild.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

namespace mozilla {
namespace dom {
class Navigator;
class VRDevice;
} // namespace dom
namespace gfx {
class VRDeviceProxy;


class VRManagerChild : public PVRManagerChild
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VRManagerChild)

  int GetInputFrameID();
  bool GetVRDevices(nsTArray<RefPtr<VRDeviceProxy> >& aDevices);
  bool RefreshVRDevicesWithCallback(dom::Navigator* aNavigator);
  static VRManagerChild* StartUpInChildProcess(Transport* aTransport,
                                               ProcessId aOtherProcess);

  static void StartUpSameProcess();
  static void ShutDown();


  static VRManagerChild* Get();

protected:
  explicit VRManagerChild();
  ~VRManagerChild();
  void Destroy();
  static void DeferredDestroy(RefPtr<VRManagerChild> aVRManagerChild);

  virtual bool RecvUpdateDeviceInfo(nsTArray<VRDeviceUpdate>&& aDeviceUpdates) override;
  virtual bool RecvUpdateDeviceSensors(nsTArray<VRSensorUpdate>&& aDeviceSensorUpdates) override;

  friend class layers::CompositorChild;

private:

  nsTArray<RefPtr<VRDeviceProxy> > mDevices;
  nsTArray<dom::Navigator*> mNavigatorCallbacks;

  int32_t mInputFrameID;
};

} // namespace mozilla
} // namespace gfx

#endif // MOZILLA_GFX_VR_VRMANAGERCHILD_H