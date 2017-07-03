/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "prlink.h"
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
  , mLastEventFrameId(0)
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
  FireEvents();
}

already_AddRefed<VRDisplayPresentation>
VRDisplayClient::BeginPresentation(const nsTArray<mozilla::dom::VRLayer>& aLayers,
                                   uint32_t aGroup)
{
  ++mPresentationCount;
  RefPtr<VRDisplayPresentation> presentation = new VRDisplayPresentation(this, aLayers, aGroup);
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

void
VRDisplayClient::SetGroupMask(uint32_t aGroupMask)
{
  VRManagerChild *vm = VRManagerChild::Get();
  vm->SendSetGroupMask(mDisplayInfo.mDisplayID, aGroupMask);
}

void
VRDisplayClient::FireEvents()
{
  VRManagerChild *vm = VRManagerChild::Get();
  // Only fire these events for non-chrome VR sessions
  bool isPresenting = (mDisplayInfo.mPresentingGroups & kVRGroupContent) != 0;

  // Check if we need to trigger onVRDisplayPresentChange event
  if (bLastEventWasPresenting != isPresenting) {
    bLastEventWasPresenting = isPresenting;
    vm->FireDOMVRDisplayPresentChangeEvent(mDisplayInfo.mDisplayID);
  }

  // Check if we need to trigger onvrdisplayactivate event
  if (!bLastEventWasMounted && mDisplayInfo.mIsMounted) {
    bLastEventWasMounted = true;
    if (gfxPrefs::VRAutoActivateEnabled()) {
      vm->FireDOMVRDisplayMountedEvent(mDisplayInfo.mDisplayID);
    }
  }

  // Check if we need to trigger onvrdisplaydeactivate event
  if (bLastEventWasMounted && !mDisplayInfo.mIsMounted) {
    bLastEventWasMounted = false;
    if (gfxPrefs::VRAutoActivateEnabled()) {
      vm->FireDOMVRDisplayUnmountedEvent(mDisplayInfo.mDisplayID);
    }
  }

  // Check if we need to trigger VRDisplay.requestAnimationFrame
  if (mLastEventFrameId != mDisplayInfo.mFrameId) {
    mLastEventFrameId = mDisplayInfo.mFrameId;
    vm->RunFrameRequestCallbacks();
  }
}

VRHMDSensorState
VRDisplayClient::GetSensorState()
{
  return mDisplayInfo.GetSensorState();
}

bool
VRDisplayClient::GetIsConnected() const
{
  return mDisplayInfo.GetIsConnected();
}

void
VRDisplayClient::NotifyDisconnected()
{
  mDisplayInfo.mIsConnected = false;
}

void
VRDisplayClient::UpdateSubmitFrameResult(const VRSubmitFrameResultInfo& aResult)
{
  mSubmitFrameResult = aResult;
}

void
VRDisplayClient::GetSubmitFrameResult(VRSubmitFrameResultInfo& aResult)
{
  aResult = mSubmitFrameResult;
}
