/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWrapperCache.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/VRDeviceBinding.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/VRDevice.h"
#include "gfxVR.h"
#include "nsIFrame.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(VRFieldOfViewReadOnly, mParent)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(VRFieldOfViewReadOnly, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(VRFieldOfViewReadOnly, Release)

JSObject*
VRFieldOfViewReadOnly::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VRFieldOfViewReadOnlyBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<VRFieldOfView>
VRFieldOfView::Constructor(const GlobalObject& aGlobal, const VRFieldOfViewInit& aParams,
                           ErrorResult& aRV)
{
  nsRefPtr<VRFieldOfView> fov =
    new VRFieldOfView(aGlobal.GetAsSupports(),
                      aParams.mUpDegrees, aParams.mRightDegrees,
                      aParams.mDownDegrees, aParams.mLeftDegrees);
  return fov.forget();
}

already_AddRefed<VRFieldOfView>
VRFieldOfView::Constructor(const GlobalObject& aGlobal,
                           double aUpDegrees, double aRightDegrees,
                           double aDownDegrees, double aLeftDegrees,
                           ErrorResult& aRV)
{
  nsRefPtr<VRFieldOfView> fov =
    new VRFieldOfView(aGlobal.GetAsSupports(),
                      aUpDegrees, aRightDegrees, aDownDegrees,
                      aLeftDegrees);
  return fov.forget();
}

JSObject*
VRFieldOfView::WrapObject(JSContext* aCx,
                          JS::Handle<JSObject*> aGivenProto)
{
  return VRFieldOfViewBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(VREyeParameters, mParent, mMinFOV, mMaxFOV, mRecFOV, mCurFOV, mEyeTranslation, mRenderRect)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(VREyeParameters, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(VREyeParameters, Release)

VREyeParameters::VREyeParameters(nsISupports* aParent,
                                 const gfx::VRFieldOfView& aMinFOV,
                                 const gfx::VRFieldOfView& aMaxFOV,
                                 const gfx::VRFieldOfView& aRecFOV,
                                 const gfx::Point3D& aEyeTranslation,
                                 const gfx::VRFieldOfView& aCurFOV,
                                 const gfx::IntRect& aRenderRect)
  : mParent(aParent)
{
  mMinFOV = new VRFieldOfView(aParent, aMinFOV);
  mMaxFOV = new VRFieldOfView(aParent, aMaxFOV);
  mRecFOV = new VRFieldOfView(aParent, aRecFOV);
  mCurFOV = new VRFieldOfView(aParent, aCurFOV);

  mEyeTranslation = new DOMPoint(aParent, aEyeTranslation.x, aEyeTranslation.y, aEyeTranslation.z, 0.0);
  mRenderRect = new DOMRect(aParent, aRenderRect.x, aRenderRect.y, aRenderRect.width, aRenderRect.height);
}

VRFieldOfView*
VREyeParameters::MinimumFieldOfView()
{
  return mMinFOV;
}

VRFieldOfView*
VREyeParameters::MaximumFieldOfView()
{
  return mMaxFOV;
}

VRFieldOfView*
VREyeParameters::RecommendedFieldOfView()
{
  return mRecFOV;
}

VRFieldOfView*
VREyeParameters::CurrentFieldOfView()
{
  return mCurFOV;
}

DOMPoint*
VREyeParameters::EyeTranslation()
{
  return mEyeTranslation;
}

DOMRect*
VREyeParameters::RenderRect()
{
  return mRenderRect;
}

JSObject*
VREyeParameters::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VREyeParametersBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(VRPositionState, mParent)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(VRPositionState, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(VRPositionState, Release)

VRPositionState::VRPositionState(nsISupports* aParent, const gfx::VRHMDSensorState& aState)
  : mParent(aParent)
  , mVRState(aState)
{
  mTimeStamp = aState.timestamp;

  if (aState.flags & gfx::VRHMDInfo::State_Position) {
    mPosition = new DOMPoint(mParent, aState.position[0], aState.position[1], aState.position[2], 0.0);
  }

  if (aState.flags & gfx::VRHMDInfo::State_Orientation) {
    mOrientation = new DOMPoint(mParent, aState.orientation[0], aState.orientation[1], aState.orientation[2], aState.orientation[3]);
  }
}

DOMPoint*
VRPositionState::GetLinearVelocity()
{
  if (!mLinearVelocity) {
    mLinearVelocity = new DOMPoint(mParent, mVRState.linearVelocity[0], mVRState.linearVelocity[1], mVRState.linearVelocity[2], 0.0);
  }
  return mLinearVelocity;
}

DOMPoint*
VRPositionState::GetLinearAcceleration()
{
  if (!mLinearAcceleration) {
    mLinearAcceleration = new DOMPoint(mParent, mVRState.linearAcceleration[0], mVRState.linearAcceleration[1], mVRState.linearAcceleration[2], 0.0);
  }
  return mLinearAcceleration;
}

DOMPoint*
VRPositionState::GetAngularVelocity()
{
  if (!mAngularVelocity) {
    mAngularVelocity = new DOMPoint(mParent, mVRState.angularVelocity[0], mVRState.angularVelocity[1], mVRState.angularVelocity[2], 0.0);
  }
  return mAngularVelocity;
}

DOMPoint*
VRPositionState::GetAngularAcceleration()
{
  if (!mAngularAcceleration) {
    mAngularAcceleration = new DOMPoint(mParent, mVRState.angularAcceleration[0], mVRState.angularAcceleration[1], mVRState.angularAcceleration[2], 0.0);
  }
  return mAngularAcceleration;
}

JSObject*
VRPositionState::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VRPositionStateBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(VRDevice)
NS_IMPL_CYCLE_COLLECTING_RELEASE(VRDevice)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(VRDevice)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(VRDevice, mParent)

/* virtual */ JSObject*
HMDVRDevice::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HMDVRDeviceBinding::Wrap(aCx, this, aGivenProto);
}

/* virtual */ JSObject*
PositionSensorVRDevice::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PositionSensorVRDeviceBinding::Wrap(aCx, this, aGivenProto);
}

namespace {

class HMDInfoVRDevice : public HMDVRDevice
{
public:
  HMDInfoVRDevice(nsISupports* aParent, gfx::VRHMDInfo* aHMD)
    : HMDVRDevice(aParent, aHMD)
  {
    uint64_t hmdid = aHMD->GetDeviceIndex() << 8;
    uint64_t devid = hmdid | 0x00; // we generate a devid with low byte 0 for the HMD, 1 for the position sensor

    mHWID.Truncate();
    mHWID.AppendPrintf("0x%llx", hmdid);

    mDeviceId.Truncate();
    mDeviceId.AppendPrintf("0x%llx", devid);

    mDeviceName.Truncate();
    mDeviceName.Append(NS_ConvertASCIItoUTF16(aHMD->GetDeviceName()));
    mDeviceName.AppendLiteral(" (HMD)");

    mValid = true;
  }

  virtual ~HMDInfoVRDevice() { }

  /* If a field of view that is set to all 0's is passed in,
   * the recommended field of view for that eye is used.
   */
  virtual void SetFieldOfView(const VRFieldOfViewInit& aLeftFOV,
                              const VRFieldOfViewInit& aRightFOV,
                              double zNear, double zFar) override
  {
    gfx::VRFieldOfView left = gfx::VRFieldOfView(aLeftFOV.mUpDegrees, aLeftFOV.mRightDegrees,
                                                 aLeftFOV.mDownDegrees, aLeftFOV.mLeftDegrees);
    gfx::VRFieldOfView right = gfx::VRFieldOfView(aRightFOV.mUpDegrees, aRightFOV.mRightDegrees,
                                                  aRightFOV.mDownDegrees, aRightFOV.mLeftDegrees);

    if (left.IsZero())
      left = mHMD->GetRecommendedEyeFOV(VRHMDInfo::Eye_Left);
    if (right.IsZero())
      right = mHMD->GetRecommendedEyeFOV(VRHMDInfo::Eye_Right);

    mHMD->SetFOV(left, right, zNear, zFar);
  }

  virtual already_AddRefed<VREyeParameters> GetEyeParameters(VREye aEye) override
  {
    gfx::IntSize sz(mHMD->SuggestedEyeResolution());
    gfx::VRHMDInfo::Eye eye = aEye == VREye::Left ? gfx::VRHMDInfo::Eye_Left : gfx::VRHMDInfo::Eye_Right;
    nsRefPtr<VREyeParameters> params =
      new VREyeParameters(mParent,
                          gfx::VRFieldOfView(15, 15, 15, 15), // XXX min?
                          mHMD->GetMaximumEyeFOV(eye),
                          mHMD->GetRecommendedEyeFOV(eye),
                          mHMD->GetEyeTranslation(eye),
                          mHMD->GetEyeFOV(eye),
                          gfx::IntRect((aEye == VREye::Left) ? 0 : sz.width, 0, sz.width, sz.height));
    return params.forget();
  }

protected:
};

class HMDPositionVRDevice : public PositionSensorVRDevice
{
public:
  HMDPositionVRDevice(nsISupports* aParent, gfx::VRHMDInfo* aHMD)
    : PositionSensorVRDevice(aParent)
    , mHMD(aHMD)
    , mTracking(false)
  {

    uint64_t hmdid = aHMD->GetDeviceIndex() << 8;
    uint64_t devid = hmdid | 0x01; // we generate a devid with low byte 0 for the HMD, 1 for the position sensor

    mHWID.Truncate();
    mHWID.AppendPrintf("0x%llx", hmdid);

    mDeviceId.Truncate();
    mDeviceId.AppendPrintf("0x%llx", devid);

    mDeviceName.Truncate();
    mDeviceName.Append(NS_ConvertASCIItoUTF16(aHMD->GetDeviceName()));
    mDeviceName.AppendLiteral(" (Sensor)");

    mValid = true;
  }

  ~HMDPositionVRDevice()
  {
    if (mTracking) {
      mHMD->StopSensorTracking();
    }
  }

  virtual already_AddRefed<VRPositionState> GetState() override
  {
    if (!mTracking) {
      mHMD->StartSensorTracking();
      mTracking = true;
    }

    gfx::VRHMDSensorState state = mHMD->GetSensorState();
    nsRefPtr<VRPositionState> obj = new VRPositionState(mParent, state);

    return obj.forget();
  }

  virtual already_AddRefed<VRPositionState> GetImmediateState() override
  {
    if (!mTracking) {
      mHMD->StartSensorTracking();
      mTracking = true;
    }

    gfx::VRHMDSensorState state = mHMD->GetSensorState();
    nsRefPtr<VRPositionState> obj = new VRPositionState(mParent, state);

    return obj.forget();
  }

  virtual void ResetSensor() override
  {
    mHMD->ZeroSensor();
  }

protected:
  nsRefPtr<gfx::VRHMDInfo> mHMD;
  bool mTracking;
};

} // namespace

bool
VRDevice::CreateAllKnownVRDevices(nsISupports *aParent, nsTArray<nsRefPtr<VRDevice>>& aDevices)
{
  nsTArray<nsRefPtr<gfx::VRHMDInfo>> hmds;
  gfx::VRHMDManager::GetAllHMDs(hmds);

  for (size_t i = 0; i < hmds.Length(); ++i) {
    uint32_t sensorBits = hmds[i]->GetSupportedSensorStateBits();
    aDevices.AppendElement(new HMDInfoVRDevice(aParent, hmds[i]));

    if (sensorBits &
        (gfx::VRHMDInfo::State_Position | gfx::VRHMDInfo::State_Orientation))
    {
      aDevices.AppendElement(new HMDPositionVRDevice(aParent, hmds[i]));
    }
  }

  return true;
}

} // namespace dom
} // namespace mozilla
