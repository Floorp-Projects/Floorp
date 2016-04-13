/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VRDisplay_h_
#define mozilla_dom_VRDisplay_h_

#include <stdint.h>

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/VRDisplayBinding.h"
#include "mozilla/dom/DOMPoint.h"
#include "mozilla/dom/DOMRect.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

#include "gfxVR.h"
#include "VRDisplayProxy.h"

namespace mozilla {
namespace dom {
class Navigator;

class VRFieldOfViewReadOnly : public nsWrapperCache
{
public:
  VRFieldOfViewReadOnly(nsISupports* aParent,
                        double aUpDegrees, double aRightDegrees,
                        double aDownDegrees, double aLeftDegrees)
    : mParent(aParent)
    , mUpDegrees(aUpDegrees)
    , mRightDegrees(aRightDegrees)
    , mDownDegrees(aDownDegrees)
    , mLeftDegrees(aLeftDegrees)
  {
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(VRFieldOfViewReadOnly)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(VRFieldOfViewReadOnly)

  double UpDegrees() const { return mUpDegrees; }
  double RightDegrees() const { return mRightDegrees; }
  double DownDegrees() const { return mDownDegrees; }
  double LeftDegrees() const { return mLeftDegrees; }

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  virtual ~VRFieldOfViewReadOnly() {}

  nsCOMPtr<nsISupports> mParent;

  double mUpDegrees;
  double mRightDegrees;
  double mDownDegrees;
  double mLeftDegrees;
};

class VRFieldOfView final : public VRFieldOfViewReadOnly
{
public:
  VRFieldOfView(nsISupports* aParent, const gfx::VRFieldOfView& aSrc)
    : VRFieldOfViewReadOnly(aParent,
                            aSrc.upDegrees, aSrc.rightDegrees,
                            aSrc.downDegrees, aSrc.leftDegrees)
  {}

  explicit VRFieldOfView(nsISupports* aParent,
                         double aUpDegrees = 0.0, double aRightDegrees = 0.0,
                         double aDownDegrees = 0.0, double aLeftDegrees = 0.0)
    : VRFieldOfViewReadOnly(aParent,
                            aUpDegrees, aRightDegrees, aDownDegrees, aLeftDegrees)
  {}

  static already_AddRefed<VRFieldOfView>
  Constructor(const GlobalObject& aGlobal, const VRFieldOfViewInit& aParams,
              ErrorResult& aRv);

  static already_AddRefed<VRFieldOfView>
  Constructor(const GlobalObject& aGlobal,
              double aUpDegrees, double aRightDegrees,
              double aDownDegrees, double aLeftDegrees,
              ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void SetUpDegrees(double aVal) { mUpDegrees = aVal; }
  void SetRightDegrees(double aVal) { mRightDegrees = aVal; }
  void SetDownDegrees(double aVal) { mDownDegrees = aVal; }
  void SetLeftDegrees(double aVal) { mLeftDegrees = aVal; }
};

class VRPositionState final : public nsWrapperCache
{
  ~VRPositionState() {}
public:
  VRPositionState(nsISupports* aParent, const gfx::VRHMDSensorState& aState);

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
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  nsCOMPtr<nsISupports> mParent;

  double mTimeStamp;
  gfx::VRHMDSensorState mVRState;

  RefPtr<DOMPoint> mPosition;
  RefPtr<DOMPoint> mLinearVelocity;
  RefPtr<DOMPoint> mLinearAcceleration;

  RefPtr<DOMPoint> mOrientation;
  RefPtr<DOMPoint> mAngularVelocity;
  RefPtr<DOMPoint> mAngularAcceleration;
};

class VREyeParameters final : public nsWrapperCache
{
public:
  VREyeParameters(nsISupports* aParent,
                  const gfx::VRFieldOfView& aMinFOV,
                  const gfx::VRFieldOfView& aMaxFOV,
                  const gfx::VRFieldOfView& aRecFOV,
                  const gfx::Point3D& aEyeTranslation,
                  const gfx::VRFieldOfView& aCurFOV,
                  const gfx::IntRect& aRenderRect);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(VREyeParameters)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(VREyeParameters)

  VRFieldOfView* MinimumFieldOfView();
  VRFieldOfView* MaximumFieldOfView();
  VRFieldOfView* RecommendedFieldOfView();
  DOMPoint* EyeTranslation();

  VRFieldOfView* CurrentFieldOfView();
  DOMRect* RenderRect();

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
protected:
  ~VREyeParameters() {}

  nsCOMPtr<nsISupports> mParent;

  RefPtr<VRFieldOfView> mMinFOV;
  RefPtr<VRFieldOfView> mMaxFOV;
  RefPtr<VRFieldOfView> mRecFOV;
  RefPtr<DOMPoint> mEyeTranslation;
  RefPtr<VRFieldOfView> mCurFOV;
  RefPtr<DOMRect> mRenderRect;
};

class VRDisplay : public nsISupports,
                 public nsWrapperCache
{
public:

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(VRDisplay)

  void GetHardwareUnitId(nsAString& aHWID) const { aHWID = mHWID; }
  void GetDeviceId(nsAString& aDeviceId) const { aDeviceId = mDeviceId; }
  void GetDeviceName(nsAString& aDeviceName) const { aDeviceName = mDeviceName; }

  bool IsValid() { return mValid; }

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  enum VRDisplayType {
    HMD,
    PositionSensor
  };

  VRDisplayType GetType() const { return mType; }

  static bool RefreshVRDisplays(dom::Navigator* aNavigator);
  static void UpdateVRDisplays(nsTArray<RefPtr<VRDisplay> >& aDevices,
                              nsISupports* aParent);

  gfx::VRDisplayProxy *GetHMD() {
    return mHMD;
  }

protected:
  VRDisplay(nsISupports* aParent,
           gfx::VRDisplayProxy* aHMD,
           VRDisplayType aType)
    : mParent(aParent)
    , mHMD(aHMD)
    , mType(aType)
    , mValid(false)
  {
    MOZ_COUNT_CTOR(VRDisplay);
    mHWID.AssignLiteral("uknown");
    mDeviceId.AssignLiteral("unknown");
    mDeviceName.AssignLiteral("unknown");
  }

  virtual ~VRDisplay()
  {
    MOZ_COUNT_DTOR(VRDisplay);
  }

  nsCOMPtr<nsISupports> mParent;
  RefPtr<gfx::VRDisplayProxy> mHMD;
  nsString mHWID;
  nsString mDeviceId;
  nsString mDeviceName;

  VRDisplayType mType;

  bool mValid;
};

class HMDVRDisplay : public VRDisplay
{
public:
  virtual already_AddRefed<VREyeParameters> GetEyeParameters(VREye aEye) = 0;

  virtual void SetFieldOfView(const VRFieldOfViewInit& aLeftFOV,
                              const VRFieldOfViewInit& aRightFOV,
                              double zNear, double zFar) = 0;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  HMDVRDisplay(nsISupports* aParent, gfx::VRDisplayProxy* aHMD)
    : VRDisplay(aParent, aHMD, VRDisplay::HMD)
  {
    MOZ_COUNT_CTOR_INHERITED(HMDVRDisplay, VRDisplay);
  }

  virtual ~HMDVRDisplay()
  {
    MOZ_COUNT_DTOR_INHERITED(HMDVRDisplay, VRDisplay);
  }
};

class HMDInfoVRDisplay : public HMDVRDisplay
{
public:
  HMDInfoVRDisplay(nsISupports* aParent, gfx::VRDisplayProxy* aHMD);
  virtual ~HMDInfoVRDisplay();

  /* If a field of view that is set to all 0's is passed in,
   * the recommended field of view for that eye is used.
   */
  virtual void SetFieldOfView(const VRFieldOfViewInit& aLeftFOV,
                              const VRFieldOfViewInit& aRightFOV,
                              double zNear, double zFar) override;
  virtual already_AddRefed<VREyeParameters> GetEyeParameters(VREye aEye) override;
};

class PositionSensorVRDisplay : public VRDisplay
{
public:
  virtual already_AddRefed<VRPositionState> GetState() = 0;
  virtual already_AddRefed<VRPositionState> GetImmediateState() = 0;
  virtual void ResetSensor() = 0;
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  explicit PositionSensorVRDisplay(nsISupports* aParent, gfx::VRDisplayProxy* aHMD)
    : VRDisplay(aParent, aHMD, VRDisplay::PositionSensor)
  {
    MOZ_COUNT_CTOR_INHERITED(PositionSensorVRDisplay, VRDisplay);
  }

  virtual ~PositionSensorVRDisplay()
  {
    MOZ_COUNT_DTOR_INHERITED(PositionSensorVRDisplay, VRDisplay);
  }
};

class HMDPositionVRDisplay : public PositionSensorVRDisplay
{
public:
  HMDPositionVRDisplay(nsISupports* aParent, gfx::VRDisplayProxy* aHMD);
  ~HMDPositionVRDisplay();

  virtual already_AddRefed<VRPositionState> GetState() override;
  virtual already_AddRefed<VRPositionState> GetImmediateState() override;
  virtual void ResetSensor() override;
};

} // namespace dom
} // namespace mozilla

#endif
