/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "gfxVR.h"
#ifdef MOZ_GAMEPAD
#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"
#endif

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

using namespace mozilla;
using namespace mozilla::gfx;

Atomic<uint32_t> VRDisplayManager::sDisplayBase(0);
Atomic<uint32_t> VRControllerManager::sControllerBase(0);

/* static */ uint32_t
VRDisplayManager::AllocateDisplayID()
{
  return ++sDisplayBase;
}

Matrix4x4
VRFieldOfView::ConstructProjectionMatrix(float zNear, float zFar,
                                         bool rightHanded) const
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

/* static */ uint32_t
VRControllerManager::AllocateControllerID()
{
  return ++sControllerBase;
}

void
VRControllerManager::AddGamepad(const char* aID, uint32_t aMapping,
                                uint32_t aNumButtons, uint32_t aNumAxes)
{
  dom::GamepadAdded a(NS_ConvertUTF8toUTF16(nsDependentCString(aID)), mControllerCount,
                     aMapping, dom::GamepadServiceType::VR, aNumButtons,
                     aNumAxes);

  VRManager* vm = VRManager::Get();
  MOZ_ASSERT(vm);
  vm->NotifyGamepadChange<dom::GamepadAdded>(a);
}

void
VRControllerManager::RemoveGamepad(uint32_t aIndex)
{
  dom::GamepadRemoved a(aIndex, dom::GamepadServiceType::VR);

  VRManager* vm = VRManager::Get();
  MOZ_ASSERT(vm);
  vm->NotifyGamepadChange<dom::GamepadRemoved>(a);
}

void
VRControllerManager::NewButtonEvent(uint32_t aIndex, uint32_t aButton,
                                    bool aPressed)
{
  dom::GamepadButtonInformation a(aIndex, dom::GamepadServiceType::VR,
                                  aButton, aPressed, aPressed ? 1.0L : 0.0L);

  VRManager* vm = VRManager::Get();
  MOZ_ASSERT(vm);
  vm->NotifyGamepadChange<dom::GamepadButtonInformation>(a);
}

void
VRControllerManager::NewAxisMove(uint32_t aIndex, uint32_t aAxis,
                                 double aValue)
{
  dom::GamepadAxisInformation a(aIndex, dom::GamepadServiceType::VR,
                                aAxis, aValue);

  VRManager* vm = VRManager::Get();
  MOZ_ASSERT(vm);
  vm->NotifyGamepadChange<dom::GamepadAxisInformation>(a);
}
