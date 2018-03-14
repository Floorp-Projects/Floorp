/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "gfxVR.h"
#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"
#include "VRDisplayHost.h"

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

/**
 * VRSystemManager::NotifyVsync must be called even when a WebVR site is
 * not active, in order to poll for respond to VR Platform API requests.
 * This should be called very often, ideally once per frame.
 * VRSystemManager::Refresh will not activate VR hardware or
 * initialize VR runtimes that have not already been activated.
 */
void
VRSystemManager::NotifyVSync()
{
  // VRDisplayHost::NotifyVSync may modify mVRDisplays, so we iterate
  // through a local copy here.
  nsTArray<RefPtr<VRDisplayHost>> displays;
  GetHMDs(displays);
  for (const auto& display : displays) {
    display->NotifyVSync();
  }

  // Ensure that the controller state is updated at least
  // on every 2d display VSync when not in a VR presentation.
  if (!GetIsPresenting()) {
    HandleInput();
  }
}

/**
 * VRSystemManager::GetHMDs must not be called unless
 * VRSystemManager::ShouldInhibitEnumeration is called
 * on all VRSystemManager instances and they all return
 * false.
 *
 * This is used to ensure that VR devices that can be
 * enumerated by multiple API's are only enumerated by
 * one API.
 *
 * GetHMDs is called for the most specific API
 * (ie. Oculus SDK) first before calling GetHMDs on
 * more generic api's (ie. OpenVR) to ensure that a device
 * is accessed using the API most optimized for it.
 *
 * ShouldInhibitEnumeration may also be used to prevent
 * devices from jumping to other API's when they are
 * intentionally ignored, such as when responding to
 * requests by the VR platform to unload the libraries
 * for runtime software updates.
 */
bool
VRSystemManager::ShouldInhibitEnumeration()
{
  return false;
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

void
VRHMDSensorState::CalcViewMatrices(const gfx::Matrix4x4* aHeadToEyeTransforms)
{

  gfx::Matrix4x4 matHead;
  if (flags & VRDisplayCapabilityFlags::Cap_Orientation) {
    matHead.SetRotationFromQuaternion(gfx::Quaternion(orientation[0], orientation[1],
                                                      orientation[2], orientation[3]));
  }
  matHead.PreTranslate(-position[0], -position[1], -position[2]);

  gfx::Matrix4x4 matView = matHead * aHeadToEyeTransforms[VRDisplayState::Eye_Left];
  matView.Normalize();
  memcpy(leftViewMatrix, matView.components, sizeof(matView.components));
  matView = matHead * aHeadToEyeTransforms[VRDisplayState::Eye_Right];
  matView.Normalize();
  memcpy(rightViewMatrix, matView.components, sizeof(matView.components));
}

const IntSize
VRDisplayInfo::SuggestedEyeResolution() const
{
  return IntSize(mDisplayState.mEyeResolution.width,
                 mDisplayState.mEyeResolution.height);
}

const Point3D
VRDisplayInfo::GetEyeTranslation(uint32_t whichEye) const
{
  return Point3D(mDisplayState.mEyeTranslation[whichEye].x,
                 mDisplayState.mEyeTranslation[whichEye].y,
                 mDisplayState.mEyeTranslation[whichEye].z);
}

const Size
VRDisplayInfo::GetStageSize() const
{
  return Size(mDisplayState.mStageSize.width,
              mDisplayState.mStageSize.height);
}

const Matrix4x4
VRDisplayInfo::GetSittingToStandingTransform() const
{
  Matrix4x4 m;
  // If we could replace Matrix4x4 with a pod type, we could
  // use it directly from the VRDisplayInfo struct.
  memcpy(m.components, mDisplayState.mSittingToStandingTransform, sizeof(float) * 16);
  return m;
}
