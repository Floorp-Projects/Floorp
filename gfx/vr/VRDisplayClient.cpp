/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "prlink.h"
#include "prmem.h"
#include "prenv.h"
#include "gfxPrefs.h"
#include "nsString.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"

#ifdef XP_WIN
#include "../layers/d3d11/CompositorD3D11.h"
#endif

#include "VRDisplayClient.h"
#include "VRDisplayPresentation.h"
#include "VRManagerChild.h"
#include "VRLayerChild.h"

using namespace mozilla;
using namespace mozilla::gfx;

VRDisplayClient::VRDisplayClient(const VRDisplayInfo& aDisplayInfo)
  : mDisplayInfo(aDisplayInfo)
  , bLastEventWasMounted(false)
  , bLastEventWasPresenting(false)
  , mPresentationCount(0)
{
  MOZ_COUNT_CTOR(VRDisplayClient);
}

VRDisplayClient::~VRDisplayClient() {
  MOZ_COUNT_DTOR(VRDisplayClient);
}

void
VRDisplayClient::UpdateDisplayInfo(const VRDisplayInfo& aDisplayInfo)
{
  mDisplayInfo = aDisplayInfo;
}

already_AddRefed<VRDisplayPresentation>
VRDisplayClient::BeginPresentation(const nsTArray<mozilla::dom::VRLayer>& aLayers)
{
  ++mPresentationCount;
  RefPtr<VRDisplayPresentation> presentation = new VRDisplayPresentation(this, aLayers);
  return presentation.forget();
}

void
VRDisplayClient::PresentationDestroyed()
{
  --mPresentationCount;
}

void
VRDisplayClient::ZeroSensor()
{
  VRManagerChild *vm = VRManagerChild::Get();
  vm->SendResetSensor(mDisplayInfo.mDisplayID);
}

VRHMDSensorState
VRDisplayClient::GetSensorState()
{
  VRHMDSensorState sensorState;
  VRManagerChild *vm = VRManagerChild::Get();
  Unused << vm->SendGetSensorState(mDisplayInfo.mDisplayID, &sensorState);
  return sensorState;
}

VRHMDSensorState
VRDisplayClient::GetImmediateSensorState()
{
  VRHMDSensorState sensorState;

  VRManagerChild *vm = VRManagerChild::Get();
  Unused << vm->SendGetImmediateSensorState(mDisplayInfo.mDisplayID, &sensorState);
  return sensorState;
}

const double kVRDisplayRAFMaxDuration = 32; // milliseconds

void
VRDisplayClient::NotifyVsync()
{
  VRManagerChild *vm = VRManagerChild::Get();

  bool isPresenting = GetIsPresenting();

  bool bShouldCallback = !isPresenting;
  if (mLastVSyncTime.IsNull()) {
    bShouldCallback = true;
  } else {
    TimeDuration duration = TimeStamp::Now() - mLastVSyncTime;
    if (duration.ToMilliseconds() > kVRDisplayRAFMaxDuration) {
      bShouldCallback = true;
    }
  }

  if (bShouldCallback) {
    vm->RunFrameRequestCallbacks();
    mLastVSyncTime = TimeStamp::Now();
  }

  // Check if we need to trigger onVRDisplayPresentChange event
  if (bLastEventWasPresenting != isPresenting) {
    bLastEventWasPresenting = isPresenting;
    vm->FireDOMVRDisplayPresentChangeEvent();
  }

  // Check if we need to trigger onvrdisplayactivate event
  if (!bLastEventWasMounted && mDisplayInfo.mIsMounted) {
    bLastEventWasMounted = true;
    vm->FireDOMVRDisplayMountedEvent(mDisplayInfo.mDisplayID);
  }

  // Check if we need to trigger onvrdisplaydeactivate event
  if (bLastEventWasMounted && !mDisplayInfo.mIsMounted) {
    bLastEventWasMounted = false;
    vm->FireDOMVRDisplayUnmountedEvent(mDisplayInfo.mDisplayID);
  }
}

void
VRDisplayClient::NotifyVRVsync()
{
  VRManagerChild *vm = VRManagerChild::Get();
  vm->RunFrameRequestCallbacks();
  mLastVSyncTime = TimeStamp::Now();
}

bool
VRDisplayClient::GetIsConnected() const
{
  return mDisplayInfo.GetIsConnected();
}

bool
VRDisplayClient::GetIsPresenting() const
{
  return mDisplayInfo.GetIsPresenting();
}

void
VRDisplayClient::NotifyDisconnected()
{
  mDisplayInfo.mIsConnected = false;
}
