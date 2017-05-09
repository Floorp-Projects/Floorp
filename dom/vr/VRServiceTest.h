/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VRServiceTest_h_
#define mozilla_dom_VRServiceTest_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/VRServiceTestBinding.h"

#include "gfxVR.h"

namespace mozilla {
namespace dom {

class VRMockDisplay final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(VRMockDisplay, DOMEventTargetHelper)

  VRMockDisplay(const nsCString& aID, uint32_t aDeviceID);
  void SetEyeParameter(VREye aEye, double aOffsetX, double aOffsetY, double aOffsetZ,
                       double aUpDegree, double aRightDegree,
                       double aDownDegree, double aLeftDegree);
  void SetEyeResolution(unsigned long aRenderWidth, unsigned long aRenderHeight);
  void SetPose(const Nullable<Float32Array>& aPosition, const Nullable<Float32Array>& aLinearVelocity,
               const Nullable<Float32Array>& aLinearAcceleration, const Nullable<Float32Array>& aOrientation,
               const Nullable<Float32Array>& aAngularVelocity, const Nullable<Float32Array>& aAngularAcceleration);
  void SetMountState(bool aIsMounted) { mDisplayInfo.mIsMounted = aIsMounted; }
  void Update();
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  ~VRMockDisplay() = default;

  uint32_t mDeviceID;
  gfx::VRDisplayInfo mDisplayInfo;
  gfx::VRHMDSensorState mSensorState;
  TimeStamp mTimestamp;
};

class VRMockController : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(VRMockController, DOMEventTargetHelper)

  VRMockController(const nsCString& aID, uint32_t aDeviceID);
  void NewButtonEvent(unsigned long aButton, bool aPressed);
  void NewAxisMoveEvent(unsigned long aAxis, double aValue);
  void NewPoseMove(const Nullable<Float32Array>& aPosition, const Nullable<Float32Array>& aLinearVelocity,
                   const Nullable<Float32Array>& aLinearAcceleration, const Nullable<Float32Array>& aOrientation,
                   const Nullable<Float32Array>& aAngularVelocity, const Nullable<Float32Array>& aAngularAcceleration);
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  ~VRMockController() = default;

  nsCString mID;
  uint32_t mDeviceID;
};

class VRServiceTest final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(VRServiceTest, DOMEventTargetHelper)

  already_AddRefed<Promise> AttachVRDisplay(const nsAString& aID, ErrorResult& aRv);
  already_AddRefed<Promise> AttachVRController(const nsAString& aID, ErrorResult& aRv);
  void Shutdown();

  static already_AddRefed<VRServiceTest> CreateTestService(nsPIDOMWindowInner* aWindow);
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  explicit VRServiceTest(nsPIDOMWindowInner* aWindow);
  ~VRServiceTest();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  bool mShuttingDown;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_VRServiceTest_h_
