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
#include "mozilla/unused.h"
#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"


#ifdef XP_WIN
#include "../layers/d3d11/CompositorD3D11.h"
#endif

#include "VRDeviceProxy.h"
#include "VRManagerChild.h"

using namespace mozilla;
using namespace mozilla::gfx;

VRDeviceProxy::VRDeviceProxy(const VRDeviceUpdate& aDeviceUpdate)
  : mDeviceInfo(aDeviceUpdate.mDeviceInfo)
  , mSensorState(aDeviceUpdate.mSensorState)
{
  MOZ_COUNT_CTOR(VRDeviceProxy);

  if (mDeviceInfo.mScreenRect.width && mDeviceInfo.mScreenRect.height) {
    if (mDeviceInfo.mIsFakeScreen) {
      mScreen = MakeFakeScreen(mDeviceInfo.mScreenRect);
    } else {
      nsCOMPtr<nsIScreenManager> screenmgr = do_GetService("@mozilla.org/gfx/screenmanager;1");
      if (screenmgr) {
        screenmgr->ScreenForRect(mDeviceInfo.mScreenRect.x, mDeviceInfo.mScreenRect.y,
                                 mDeviceInfo.mScreenRect.width, mDeviceInfo.mScreenRect.height,
                                 getter_AddRefs(mScreen));
      }
    }
#ifdef DEBUG
    printf_stderr("VR DEVICE SCREEN: %d %d %d %d\n",
                  mDeviceInfo.mScreenRect.x, mDeviceInfo.mScreenRect.y,
                  mDeviceInfo.mScreenRect.width, mDeviceInfo.mScreenRect.height);
#endif
  }
}

VRDeviceProxy::~VRDeviceProxy() {
  MOZ_COUNT_DTOR(VRDeviceProxy);
}

void
VRDeviceProxy::UpdateDeviceInfo(const VRDeviceUpdate& aDeviceUpdate)
{
  mDeviceInfo = aDeviceUpdate.mDeviceInfo;
  mSensorState = aDeviceUpdate.mSensorState;
}

bool
VRDeviceProxy::SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight,
                       double zNear, double zFar)
{
  VRManagerChild *vm = VRManagerChild::Get();
  vm->SendSetFOV(mDeviceInfo.mDeviceID, aFOVLeft, aFOVRight, zNear, zFar);
  return true;
}

void
VRDeviceProxy::ZeroSensor()
{
  VRManagerChild *vm = VRManagerChild::Get();
  vm->SendResetSensor(mDeviceInfo.mDeviceID);
}

VRHMDSensorState
VRDeviceProxy::GetSensorState(double timeOffset)
{
  VRManagerChild *vm = VRManagerChild::Get();
  Unused << vm->SendKeepSensorTracking(mDeviceInfo.mDeviceID);
  return mSensorState;
}

void
VRDeviceProxy::UpdateSensorState(const VRHMDSensorState& aSensorState)
{
  mSensorState = aSensorState;
}

// Dummy nsIScreen implementation, for when we just need to specify a size
class FakeScreen : public nsIScreen
{
public:
  explicit FakeScreen(const IntRect& aScreenRect)
    : mScreenRect(aScreenRect)
  { }

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetRect(int32_t *l, int32_t *t, int32_t *w, int32_t *h) override {
    *l = mScreenRect.x;
    *t = mScreenRect.y;
    *w = mScreenRect.width;
    *h = mScreenRect.height;
    return NS_OK;
  }
  NS_IMETHOD GetAvailRect(int32_t *l, int32_t *t, int32_t *w, int32_t *h) override {
    return GetRect(l, t, w, h);
  }
  NS_IMETHOD GetRectDisplayPix(int32_t *l, int32_t *t, int32_t *w, int32_t *h) override {
    return GetRect(l, t, w, h);
  }
  NS_IMETHOD GetAvailRectDisplayPix(int32_t *l, int32_t *t, int32_t *w, int32_t *h) override {
    return GetAvailRect(l, t, w, h);
  }

  NS_IMETHOD GetId(uint32_t* aId) override { *aId = (uint32_t)-1; return NS_OK; }
  NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth) override { *aPixelDepth = 24; return NS_OK; }
  NS_IMETHOD GetColorDepth(int32_t* aColorDepth) override { *aColorDepth = 24; return NS_OK; }

  NS_IMETHOD LockMinimumBrightness(uint32_t aBrightness) override { return NS_ERROR_NOT_AVAILABLE; }
  NS_IMETHOD UnlockMinimumBrightness(uint32_t aBrightness) override { return NS_ERROR_NOT_AVAILABLE; }
  NS_IMETHOD GetRotation(uint32_t* aRotation) override {
    *aRotation = nsIScreen::ROTATION_0_DEG;
    return NS_OK;
  }
  NS_IMETHOD SetRotation(uint32_t aRotation) override { return NS_ERROR_NOT_AVAILABLE; }
  NS_IMETHOD GetContentsScaleFactor(double* aContentsScaleFactor) override {
    *aContentsScaleFactor = 1.0;
    return NS_OK;
  }

protected:
  virtual ~FakeScreen() {}

  IntRect mScreenRect;
};

NS_IMPL_ISUPPORTS(FakeScreen, nsIScreen)


/* static */ already_AddRefed<nsIScreen>
VRDeviceProxy::MakeFakeScreen(const IntRect& aScreenRect)
{
  nsCOMPtr<nsIScreen> screen = new FakeScreen(aScreenRect);
  return screen.forget();
}

