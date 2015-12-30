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

#include "mozilla/unused.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureHost.h"

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

using namespace mozilla;
using namespace mozilla::gfx;

Atomic<uint32_t> VRHMDManager::sDeviceBase(0);

VRHMDInfo::VRHMDInfo(VRHMDType aType, bool aUseMainThreadOrientation)
{
  MOZ_COUNT_CTOR(VRHMDInfo);
  mDeviceInfo.mType = aType;
  mDeviceInfo.mDeviceID = VRHMDManager::AllocateDeviceID();
  mDeviceInfo.mUseMainThreadOrientation = aUseMainThreadOrientation;
}

VRHMDInfo::~VRHMDInfo()
{
  MOZ_COUNT_DTOR(VRHMDInfo);
}

/* static */ uint32_t
VRHMDManager::AllocateDeviceID()
{
  return ++sDeviceBase;
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
