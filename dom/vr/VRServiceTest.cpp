/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VRServiceTest.h"
#include "mozilla/dom/VRServiceTestBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(VRMockDisplay)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(VRMockDisplay,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(VRMockDisplay,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(VRMockDisplay)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(VRMockDisplay, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(VRMockDisplay, DOMEventTargetHelper)

VRMockDisplay::VRMockDisplay(const nsCString& aID, uint32_t aDeviceID)
 : mDeviceID(aDeviceID)
 , mTimestamp(TimeStamp::Now())
{
  mDisplayInfo.mDisplayName = aID;
  mDisplayInfo.mType = VRDeviceType::Puppet;
  mDisplayInfo.mIsConnected = true;
  mDisplayInfo.mIsMounted = false;
  mDisplayInfo.mCapabilityFlags = VRDisplayCapabilityFlags::Cap_None |
                                  VRDisplayCapabilityFlags::Cap_Orientation |
                                  VRDisplayCapabilityFlags::Cap_AngularAcceleration |
                                  VRDisplayCapabilityFlags::Cap_Position |
                                  VRDisplayCapabilityFlags::Cap_LinearAcceleration |
                                  VRDisplayCapabilityFlags::Cap_External |
                                  VRDisplayCapabilityFlags::Cap_Present |
                                  VRDisplayCapabilityFlags::Cap_StageParameters |
                                  VRDisplayCapabilityFlags::Cap_MountDetection;
}

JSObject*
VRMockDisplay::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VRMockDisplayBinding::Wrap(aCx, this, aGivenProto);
}

void VRMockDisplay::SetEyeResolution(unsigned long aRenderWidth, unsigned long aRenderHeight)
{
  mDisplayInfo.mEyeResolution.width = aRenderWidth;
  mDisplayInfo.mEyeResolution.height = aRenderHeight;
}

void
VRMockDisplay::SetEyeParameter(VREye aEye, double aOffsetX, double aOffsetY,
                               double aOffsetZ, double aUpDegree, double aRightDegree,
                               double aDownDegree, double aLeftDegree)
{
  uint32_t eye = static_cast<uint32_t>(aEye);
  mDisplayInfo.mEyeFOV[eye] = gfx ::VRFieldOfView(aUpDegree, aRightDegree,
                                                  aRightDegree, aLeftDegree);
  mDisplayInfo.mEyeTranslation[eye].x = aOffsetX;
  mDisplayInfo.mEyeTranslation[eye].y = aOffsetY;
  mDisplayInfo.mEyeTranslation[eye].z = aOffsetZ;
}

void
VRMockDisplay::SetPose(const Nullable<Float32Array>& aPosition,
                       const Nullable<Float32Array>& aLinearVelocity,
                       const Nullable<Float32Array>& aLinearAcceleration,
                       const Nullable<Float32Array>& aOrientation,
                       const Nullable<Float32Array>& aAngularVelocity,
                       const Nullable<Float32Array>& aAngularAcceleration)
{
  mSensorState.Clear();
  mSensorState.timestamp = (TimeStamp::Now() - mTimestamp).ToSeconds();
  mSensorState.flags = VRDisplayCapabilityFlags::Cap_Orientation |
                       VRDisplayCapabilityFlags::Cap_Position |
                       VRDisplayCapabilityFlags::Cap_AngularAcceleration |
                       VRDisplayCapabilityFlags::Cap_LinearAcceleration |
                       VRDisplayCapabilityFlags::Cap_External |
                       VRDisplayCapabilityFlags::Cap_MountDetection |
                       VRDisplayCapabilityFlags::Cap_Present;

  if (!aOrientation.IsNull()) {
    const Float32Array& value = aOrientation.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 4);
    mSensorState.orientation[0] = value.Data()[0];
    mSensorState.orientation[1] = value.Data()[1];
    mSensorState.orientation[2] = value.Data()[2];
    mSensorState.orientation[3] = value.Data()[3];
  }
  if (!aAngularVelocity.IsNull()) {
    const Float32Array& value = aAngularVelocity.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    mSensorState.angularVelocity[0] = value.Data()[0];
    mSensorState.angularVelocity[1] = value.Data()[1];
    mSensorState.angularVelocity[2] = value.Data()[2];
  }
  if (!aAngularAcceleration.IsNull()) {
    const Float32Array& value = aAngularAcceleration.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    mSensorState.angularAcceleration[0] = value.Data()[0];
    mSensorState.angularAcceleration[1] = value.Data()[1];
    mSensorState.angularAcceleration[2] = value.Data()[2];
  }
  if (!aPosition.IsNull()) {
    const Float32Array& value = aPosition.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    mSensorState.position[0] = value.Data()[0];
    mSensorState.position[1] = value.Data()[1];
    mSensorState.position[2] = value.Data()[2];
  }
  if (!aLinearVelocity.IsNull()) {
    const Float32Array& value = aLinearVelocity.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    mSensorState.linearVelocity[0] = value.Data()[0];
    mSensorState.linearVelocity[1] = value.Data()[1];
    mSensorState.linearVelocity[2] = value.Data()[2];
  }
  if (!aLinearAcceleration.IsNull()) {
    const Float32Array& value = aLinearAcceleration.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    mSensorState.linearAcceleration[0] = value.Data()[0];
    mSensorState.linearAcceleration[1] = value.Data()[1];
    mSensorState.linearAcceleration[2] = value.Data()[2];
  }
}

void
VRMockDisplay::Update()
{
  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();

  vm->SendSetSensorStateToMockDisplay(mDeviceID, mSensorState);
  vm->SendSetDisplayInfoToMockDisplay(mDeviceID, mDisplayInfo);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(VRMockController)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(VRMockController,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(VRMockController,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(VRMockController)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(VRMockController, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(VRMockController, DOMEventTargetHelper)

VRMockController::VRMockController(const nsCString& aID, uint32_t aDeviceID)
 : mID(aID), mDeviceID(aDeviceID)
{
}

JSObject*
VRMockController::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VRMockControllerBinding::Wrap(aCx, this, aGivenProto);
}

void
VRMockController::NewButtonEvent(unsigned long aButton, bool aPressed)
{
  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  vm->SendNewButtonEventToMockController(mDeviceID, aButton, aPressed);
}

void
VRMockController::NewAxisMoveEvent(unsigned long aAxis, double aValue)
{
  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  vm->SendNewAxisMoveEventToMockController(mDeviceID, aAxis, aValue);
}

void
VRMockController::NewPoseMove(const Nullable<Float32Array>& aPosition,
                              const Nullable<Float32Array>& aLinearVelocity,
                              const Nullable<Float32Array>& aLinearAcceleration,
                              const Nullable<Float32Array>& aOrientation,
                              const Nullable<Float32Array>& aAngularVelocity,
                              const Nullable<Float32Array>& aAngularAcceleration)
{
  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  GamepadPoseState poseState;

  poseState.flags = GamepadCapabilityFlags::Cap_Orientation |
                    GamepadCapabilityFlags::Cap_Position |
                    GamepadCapabilityFlags::Cap_AngularAcceleration |
                    GamepadCapabilityFlags::Cap_LinearAcceleration;
  if (!aOrientation.IsNull()) {
    const Float32Array& value = aOrientation.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 4);
    poseState.orientation[0] = value.Data()[0];
    poseState.orientation[1] = value.Data()[1];
    poseState.orientation[2] = value.Data()[2];
    poseState.orientation[3] = value.Data()[3];
  }
  if (!aPosition.IsNull()) {
    const Float32Array& value = aPosition.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    poseState.position[0] = value.Data()[0];
    poseState.position[1] = value.Data()[1];
    poseState.position[2] = value.Data()[2];
  }
  if (!aAngularVelocity.IsNull()) {
    const Float32Array& value = aAngularVelocity.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    poseState.angularVelocity[0] = value.Data()[0];
    poseState.angularVelocity[1] = value.Data()[1];
    poseState.angularVelocity[2] = value.Data()[2];
  }
  if (!aAngularAcceleration.IsNull()) {
    const Float32Array& value = aAngularAcceleration.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    poseState.angularAcceleration[0] = value.Data()[0];
    poseState.angularAcceleration[1] = value.Data()[1];
    poseState.angularAcceleration[2] = value.Data()[2];
  }
  if (!aLinearVelocity.IsNull()) {
    const Float32Array& value = aLinearVelocity.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    poseState.linearVelocity[0] = value.Data()[0];
    poseState.linearVelocity[1] = value.Data()[1];
    poseState.linearVelocity[2] = value.Data()[2];
  }
  if (!aLinearAcceleration.IsNull()) {
    const Float32Array& value = aLinearAcceleration.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    poseState.linearAcceleration[0] = value.Data()[0];
    poseState.linearAcceleration[1] = value.Data()[1];
    poseState.linearAcceleration[2] = value.Data()[2];
  }
  vm->SendNewPoseMoveToMockController(mDeviceID, poseState);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(VRServiceTest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(VRServiceTest,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(VRServiceTest,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(VRServiceTest)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(VRServiceTest, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(VRServiceTest, DOMEventTargetHelper)


JSObject*
VRServiceTest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VRServiceTestBinding::Wrap(aCx, this, aGivenProto);
}

// static
already_AddRefed<VRServiceTest>
VRServiceTest::CreateTestService(nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(aWindow);
  RefPtr<VRServiceTest> service = new VRServiceTest(aWindow);
  return service.forget();
}

VRServiceTest::VRServiceTest(nsPIDOMWindowInner* aWindow)
  : mWindow(aWindow),
    mShuttingDown(false)
{
  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  vm->SendCreateVRTestSystem();
}

VRServiceTest::~VRServiceTest()
{}

void
VRServiceTest::Shutdown()
{
  MOZ_ASSERT(!mShuttingDown);
  mShuttingDown = true;
  mWindow = nullptr;
}

already_AddRefed<Promise>
VRServiceTest::AttachVRDisplay(const nsAString& aID, ErrorResult& aRv)
{
  if (mShuttingDown) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(mWindow);

  RefPtr<Promise> p = Promise::Create(go, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  vm->CreateVRServiceTestDisplay(NS_ConvertUTF16toUTF8(aID), p);

  return p.forget();
}

already_AddRefed<Promise>
VRServiceTest::AttachVRController(const nsAString& aID, ErrorResult& aRv)
{
  if (mShuttingDown) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(mWindow);

  RefPtr<Promise> p = Promise::Create(go, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  vm->CreateVRServiceTestController(NS_ConvertUTF16toUTF8(aID), p);

  return p.forget();
}

} // namespace dom
} // namespace mozilla
