/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "prlink.h"
#include "prmem.h"
#include "prenv.h"
#include "nsString.h"

#include "gfxPrefs.h"
#include "gfxVR.h"
#if defined(XP_WIN)
#include "gfxVROculus.h"
#endif
#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_LINUX)
#include "gfxVROculus050.h"
#endif
#include "gfxVRCardboard.h"

#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"

#include "mozilla/unused.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureHost.h"

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

using namespace mozilla;
using namespace mozilla::gfx;

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

VRHMDInfo::VRHMDInfo(VRHMDType aType)
  : mType(aType)
{
  MOZ_COUNT_CTOR(VRHMDInfo);

  mDeviceIndex = VRHMDManager::AllocateDeviceIndex();
  mDeviceName.AssignLiteral("Unknown Device");
}


VRHMDManager::VRHMDManagerArray *VRHMDManager::sManagers = nullptr;
Atomic<uint32_t> VRHMDManager::sDeviceBase(0);

/* static */ void
VRHMDManager::ManagerInit()
{
  if (sManagers)
    return;

  sManagers = new VRHMDManagerArray();

  nsRefPtr<VRHMDManager> mgr;

  // we'll only load the 0.5.0 oculus runtime if
  // the >= 0.6.0 one failed to load; otherwise
  // we might initialize oculus twice
  bool useOculus050 = true;
  unused << useOculus050;

#if defined(XP_WIN)
  mgr = new VRHMDManagerOculus();
  if (mgr->PlatformInit()) {
    useOculus050 = false;
    sManagers->AppendElement(mgr);
  }
#endif

#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_LINUX)
  if (useOculus050) {
    mgr = new VRHMDManagerOculus050();
    if (mgr->PlatformInit())
      sManagers->AppendElement(mgr);
  }
#endif

  mgr = new VRHMDManagerCardboard();
  if (mgr->PlatformInit())
    sManagers->AppendElement(mgr);
}

/* static */ void
VRHMDManager::ManagerDestroy()
{
  if (!sManagers)
    return;

  for (uint32_t i = 0; i < sManagers->Length(); ++i) {
    (*sManagers)[i]->Destroy();
  }

  delete sManagers;
  sManagers = nullptr;
}

/* static */ void
VRHMDManager::GetAllHMDs(nsTArray<nsRefPtr<VRHMDInfo>>& aHMDResult)
{
  if (!sManagers)
    return;

  for (uint32_t i = 0; i < sManagers->Length(); ++i) {
    (*sManagers)[i]->GetHMDs(aHMDResult);
  }
}

/* static */ uint32_t
VRHMDManager::AllocateDeviceIndex()
{
  return ++sDeviceBase;
}

/* static */ already_AddRefed<nsIScreen>
VRHMDManager::MakeFakeScreen(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
  nsCOMPtr<nsIScreen> screen = new FakeScreen(IntRect(x, y, width, height));
  return screen.forget();
}

VRHMDRenderingSupport::RenderTargetSet::RenderTargetSet()
  : currentRenderTarget(0)
{
}

VRHMDRenderingSupport::RenderTargetSet::~RenderTargetSet()
{
}

Matrix4x4
VRFieldOfView::ConstructProjectionMatrix(float zNear, float zFar, bool rightHanded)
{
  float upTan = tan(upDegrees * M_PI / 180.0);
  float downTan = tan(downDegrees * M_PI / 180.0);
  float leftTan = tan(leftDegrees * M_PI / 180.0);
  float rightTan = tan(rightDegrees * M_PI / 180.0);

  float handednessScale = rightHanded ? -1.0 : 1.0;

  float pxscale = 2.0f / (leftTan + rightTan);
  float pxoffset = (leftTan - rightTan) * pxscale * 0.5;
  float pyscale = 2.0f / (upTan + downTan);
  float pyoffset = (upTan - downTan) * pyscale * 0.5;

  Matrix4x4 mobj;
  float *m = &mobj._11;

  m[0*4+0] = pxscale;
  m[2*4+0] = pxoffset * handednessScale;

  m[1*4+1] = pyscale;
  m[2*4+1] = -pyoffset * handednessScale;

  m[2*4+2] = zFar / (zNear - zFar) * -handednessScale;
  m[3*4+2] = (zFar * zNear) / (zNear - zFar);

  m[2*4+3] = handednessScale;
  m[3*4+3] = 0.0f;

  return mobj;
}
