/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VRDevice_h_
#define mozilla_dom_VRDevice_h_

#include <stdint.h>

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/VRDeviceBinding.h"
#include "mozilla/dom/DOMPoint.h"
#include "mozilla/dom/DOMRect.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

#include "gfxVR.h"

namespace mozilla {
namespace dom {

class Element;

class VRFieldOfViewReadOnly : public NonRefcountedDOMObject
{
public:
  VRFieldOfViewReadOnly(double aUpDegrees, double aRightDegrees,
                        double aDownDegrees, double aLeftDegrees)
    : mUpDegrees(aUpDegrees)
    , mRightDegrees(aRightDegrees)
    , mDownDegrees(aDownDegrees)
    , mLeftDegrees(aLeftDegrees)
  {
  }

  double UpDegrees() const { return mUpDegrees; }
  double RightDegrees() const { return mRightDegrees; }
  double DownDegrees() const { return mDownDegrees; }
  double LeftDegrees() const { return mLeftDegrees; }

protected:
  double mUpDegrees;
  double mRightDegrees;
  double mDownDegrees;
  double mLeftDegrees;
};

class VRFieldOfView MOZ_FINAL : public VRFieldOfViewReadOnly
{
public:
  explicit VRFieldOfView(double aUpDegrees = 0.0, double aRightDegrees = 0.0,
                         double aDownDegrees = 0.0, double aLeftDegrees = 0.0)
    : VRFieldOfViewReadOnly(aUpDegrees, aRightDegrees, aDownDegrees, aLeftDegrees)
  {}

  static VRFieldOfView*
  Constructor(const GlobalObject& aGlobal, const VRFieldOfViewInit& aParams,
              ErrorResult& aRv);

  static VRFieldOfView*
  Constructor(const GlobalObject& aGlobal,
              double aUpDegrees, double aRightDegrees,
              double aDownDegrees, double aLeftDegrees,
              ErrorResult& aRv);

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector);

  void SetUpDegrees(double aVal) { mUpDegrees = aVal; }
  void SetRightDegrees(double aVal) { mRightDegrees = aVal; }
  void SetDownDegrees(double aVal) { mDownDegrees = aVal; }
  void SetLeftDegrees(double aVal) { mLeftDegrees = aVal; }
};

class VRPositionState MOZ_FINAL : public nsWrapperCache
{
  ~VRPositionState() {}
public:
  explicit VRPositionState(nsISupports* aParent, const gfx::VRHMDSensorState& aState);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(VRPositionState)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(VRPositionState)

  double TimeStamp() const { return mTimeStamp; }

  bool HasPosition() const { return mPosition != nullptr; }
  DOMPoint* GetPosition() const { return mPosition; }

  bool HasOrientation() const { return mOrientation != nullptr; }
  DOMPoint* GetOrientation() const { return mOrientation; }

  // these are created lazily
  DOMPoint* GetLinearVelocity();
  DOMPoint* GetLinearAcceleration();
  DOMPoint* GetAngularVelocity();
  DOMPoint* GetAngularAcceleration();

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) MOZ_OVERRIDE;

protected:
  nsCOMPtr<nsISupports> mParent;

  double mTimeStamp;
  gfx::VRHMDSensorState mVRState;

  nsRefPtr<DOMPoint> mPosition;
  nsRefPtr<DOMPoint> mLinearVelocity;
  nsRefPtr<DOMPoint> mLinearAcceleration;

  nsRefPtr<DOMPoint> mOrientation;
  nsRefPtr<DOMPoint> mAngularVelocity;
  nsRefPtr<DOMPoint> mAngularAcceleration;
};

class VRDevice : public nsISupports,
                 public nsWrapperCache
{
public:
  // create new VRDevice objects for all known underlying gfx::vr devices
  static bool CreateAllKnownVRDevices(nsISupports *aParent, nsTArray<nsRefPtr<VRDevice>>& aDevices);

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(VRDevice)

  void GetHardwareUnitId(nsAString& aHWID) const { aHWID = mHWID; }
  void GetDeviceId(nsAString& aDeviceId) const { aDeviceId = mDeviceId; }
  void GetDeviceName(nsAString& aDeviceName) const { aDeviceName = mDeviceName; }

  bool IsValid() { return mValid; }

  virtual void Shutdown() { }

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  enum VRDeviceType {
    HMD,
    PositionSensor
  };

  VRDeviceType GetType() const { return mType; }

protected:
  VRDevice(nsISupports* aParent, VRDeviceType aType)
    : mParent(aParent)
    , mType(aType)
    , mValid(false)
  {
    mHWID.AssignLiteral("uknown");
    mDeviceId.AssignLiteral("unknown");
    mDeviceName.AssignLiteral("unknown");
  }

  virtual ~VRDevice() {
    Shutdown();
  }

  nsCOMPtr<nsISupports> mParent;
  nsString mHWID;
  nsString mDeviceId;
  nsString mDeviceName;

  VRDeviceType mType;

  bool mValid;
};

class HMDVRDevice : public VRDevice
{
public:
  virtual already_AddRefed<DOMPoint> GetEyeTranslation(VREye aEye) = 0;

  virtual void SetFieldOfView(const VRFieldOfViewInit& aLeftFOV,
                              const VRFieldOfViewInit& aRightFOV,
                              double zNear, double zFar) = 0;
  virtual VRFieldOfView* GetCurrentEyeFieldOfView(VREye aEye) = 0;
  virtual VRFieldOfView* GetRecommendedEyeFieldOfView(VREye aEye) = 0;
  virtual VRFieldOfView* GetMaximumEyeFieldOfView(VREye aEye) = 0;
  virtual already_AddRefed<DOMRect> GetRecommendedEyeRenderRect(VREye aEye) = 0;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) MOZ_OVERRIDE;

  void XxxToggleElementVR(Element& aElement);

  gfx::VRHMDInfo *GetHMD() { return mHMD.get(); }

protected:
  HMDVRDevice(nsISupports* aParent, gfx::VRHMDInfo* aHMD)
    : VRDevice(aParent, VRDevice::HMD)
    , mHMD(aHMD)
  { }

  virtual ~HMDVRDevice() { }

  nsRefPtr<gfx::VRHMDInfo> mHMD;
};

class PositionSensorVRDevice : public VRDevice
{
public:
  virtual already_AddRefed<VRPositionState> GetState(double timeOffset) = 0;

  virtual void ZeroSensor() = 0;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) MOZ_OVERRIDE;

protected:
  explicit PositionSensorVRDevice(nsISupports* aParent)
    : VRDevice(aParent, VRDevice::PositionSensor)
  { }

  virtual ~PositionSensorVRDevice() { }
};

} // namespace dom
} // namespace mozilla

#endif
