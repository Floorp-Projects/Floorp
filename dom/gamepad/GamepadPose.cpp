/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWrapperCache.h"

#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/GamepadPoseBinding.h"
#include "mozilla/dom/GamepadPose.h"

namespace mozilla {
namespace dom {

GamepadPose::GamepadPose(nsISupports* aParent, const GamepadPoseState& aState)
  : Pose(aParent),
    mPoseState(aState)
{
  mozilla::HoldJSObjects(this);
}

GamepadPose::GamepadPose(nsISupports* aParent)
  : Pose(aParent)
{
  mozilla::HoldJSObjects(this);
  mPoseState.Clear();
}

GamepadPose::~GamepadPose()
{
  mozilla::DropJSObjects(this);
}

/* virtual */ JSObject*
GamepadPose::WrapObject(JSContext* aJSContext, JS::Handle<JSObject*> aGivenProto)
{
  return GamepadPoseBinding::Wrap(aJSContext, this, aGivenProto);
}

bool
GamepadPose::HasOrientation() const
{
  return bool(mPoseState.flags & GamepadCapabilityFlags::Cap_Position);
}

bool
GamepadPose::HasPosition() const
{
  return bool(mPoseState.flags & GamepadCapabilityFlags::Cap_Orientation);
}

void
GamepadPose::GetPosition(JSContext* aJSContext,
                         JS::MutableHandle<JSObject*> aRetval,
                         ErrorResult& aRv)
{
  SetFloat32Array(aJSContext, aRetval, mPosition, mPoseState.position, 3,
    bool(mPoseState.flags & GamepadCapabilityFlags::Cap_Position), aRv);
}

void
GamepadPose::GetLinearVelocity(JSContext* aJSContext,
                               JS::MutableHandle<JSObject*> aRetval,
                               ErrorResult& aRv)
{
  SetFloat32Array(aJSContext, aRetval, mLinearVelocity, mPoseState.linearVelocity, 3,
    bool(mPoseState.flags & GamepadCapabilityFlags::Cap_Position), aRv);
}

void
GamepadPose::GetLinearAcceleration(JSContext* aJSContext,
                                   JS::MutableHandle<JSObject*> aRetval,
                                   ErrorResult& aRv)
{
  SetFloat32Array(aJSContext, aRetval, mLinearAcceleration, mPoseState.linearAcceleration, 3,
    bool(mPoseState.flags & GamepadCapabilityFlags::Cap_LinearAcceleration), aRv);
}

void
GamepadPose::GetOrientation(JSContext* aJSContext,
                            JS::MutableHandle<JSObject*> aRetval,
                            ErrorResult& aRv)
{
  SetFloat32Array(aJSContext, aRetval, mOrientation, mPoseState.orientation, 4,
    bool(mPoseState.flags & GamepadCapabilityFlags::Cap_Orientation), aRv);
}

void
GamepadPose::GetAngularVelocity(JSContext* aJSContext,
                                JS::MutableHandle<JSObject*> aRetval,
                                ErrorResult& aRv)
{
  SetFloat32Array(aJSContext, aRetval, mAngularVelocity, mPoseState.angularVelocity, 3,
    bool(mPoseState.flags & GamepadCapabilityFlags::Cap_Orientation), aRv);
}

void
GamepadPose::GetAngularAcceleration(JSContext* aJSContext,
                                    JS::MutableHandle<JSObject*> aRetval,
                                    ErrorResult& aRv)
{
  SetFloat32Array(aJSContext, aRetval, mAngularAcceleration, mPoseState.angularAcceleration, 3,
    bool(mPoseState.flags & GamepadCapabilityFlags::Cap_AngularAcceleration), aRv);
}

void
GamepadPose::SetPoseState(const GamepadPoseState& aPose)
{
  mPoseState = aPose;
}

const GamepadPoseState&
GamepadPose::GetPoseState()
{
  return mPoseState;
}

} // namespace dom
} // namespace mozilla
