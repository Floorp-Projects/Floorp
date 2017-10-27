/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "gfxVR.h"
#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

using namespace mozilla;
using namespace mozilla::gfx;

Atomic<uint32_t> VRSystemManager::sDisplayBase(0);
Atomic<uint32_t> VRSystemManager::sControllerBase(0);

/* static */ uint32_t
VRSystemManager::AllocateDisplayID()
{
  return ++sDisplayBase;
}

/* static */ uint32_t
VRSystemManager::AllocateControllerID()
{
  return ++sControllerBase;
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

void
VRSystemManager::AddGamepad(const VRControllerInfo& controllerInfo)
{
  dom::GamepadAdded a(NS_ConvertUTF8toUTF16(controllerInfo.GetControllerName()),
                      controllerInfo.GetMappingType(),
                      controllerInfo.GetHand(),
                      controllerInfo.GetDisplayID(),
                      controllerInfo.GetNumButtons(),
                      controllerInfo.GetNumAxes(),
                      controllerInfo.GetNumHaptics());

  VRManager* vm = VRManager::Get();
  vm->NotifyGamepadChange<dom::GamepadAdded>(mControllerCount, a);
}

void
VRSystemManager::RemoveGamepad(uint32_t aIndex)
{
  dom::GamepadRemoved a;

  VRManager* vm = VRManager::Get();
  vm->NotifyGamepadChange<dom::GamepadRemoved>(aIndex, a);
}

void
VRSystemManager::NewButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed, bool aTouched, double aValue)
{
  dom::GamepadButtonInformation a(aButton, aValue, aPressed, aTouched);

  VRManager* vm = VRManager::Get();
  vm->NotifyGamepadChange<dom::GamepadButtonInformation>(aIndex, a);
}

void
VRSystemManager::NewAxisMove(uint32_t aIndex, uint32_t aAxis,
                             double aValue)
{
  dom::GamepadAxisInformation a(aAxis, aValue);

  VRManager* vm = VRManager::Get();
  vm->NotifyGamepadChange<dom::GamepadAxisInformation>(aIndex, a);
}

void
VRSystemManager::NewPoseState(uint32_t aIndex,
                              const dom::GamepadPoseState& aPose)
{
  dom::GamepadPoseInformation a(aPose);

  VRManager* vm = VRManager::Get();
  vm->NotifyGamepadChange<dom::GamepadPoseInformation>(aIndex, a);
}

void
VRSystemManager::NewHandChangeEvent(uint32_t aIndex,
                                    const dom::GamepadHand aHand)
{
  dom::GamepadHandInformation a(aHand);

  VRManager* vm = VRManager::Get();
  vm->NotifyGamepadChange<dom::GamepadHandInformation>(aIndex, a);
}

