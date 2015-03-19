/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
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

VRFieldOfView*
VRFieldOfView::Constructor(const GlobalObject& aGlobal, const VRFieldOfViewInit& aParams,
                           ErrorResult& aRV)
{
  return new VRFieldOfView(aParams.mUpDegrees, aParams.mRightDegrees,
                           aParams.mDownDegrees, aParams.mLeftDegrees);
}

VRFieldOfView*
VRFieldOfView::Constructor(const GlobalObject& aGlobal,
                           double aUpDegrees, double aRightDegrees,
                           double aDownDegrees, double aLeftDegrees,
                           ErrorResult& aRV)
{
  return new VRFieldOfView(aUpDegrees, aRightDegrees, aDownDegrees,
                           aLeftDegrees);
}

bool
VRFieldOfView::WrapObject(JSContext* aCx,
                          JS::Handle<JSObject*> aGivenProto,
                          JS::MutableHandle<JSObject*> aReflector)
{
  return VRFieldOfViewBinding::Wrap(aCx, this, aGivenProto, aReflector);
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

static void
ReleaseHMDInfoRef(void *, nsIAtom*, void *aPropertyValue, void *)
{
  if (aPropertyValue) {
    static_cast<VRHMDInfo*>(aPropertyValue)->Release();
  }
}

void
HMDVRDevice::XxxToggleElementVR(Element& aElement)
{
  VRHMDInfo* hmdPtr = static_cast<VRHMDInfo*>(aElement.GetProperty(nsGkAtoms::vr_state));
  if (hmdPtr) {
    aElement.DeleteProperty(nsGkAtoms::vr_state);
    return;
  }

  nsRefPtr<VRHMDInfo> hmdRef = mHMD;
  aElement.SetProperty(nsGkAtoms::vr_state, hmdRef.forget().take(),
                       ReleaseHMDInfoRef,
                       true);
}

namespace {

gfx::VRHMDInfo::Eye
EyeToEye(const VREye& aEye)
{
  return aEye == VREye::Left ? gfx::VRHMDInfo::Eye_Left : gfx::VRHMDInfo::Eye_Right;
}

class HMDInfoVRDevice : public HMDVRDevice
{
public:
  HMDInfoVRDevice(nsISupports* aParent, gfx::VRHMDInfo* aHMD)
    : HMDVRDevice(aParent, aHMD)
  {
    // XXX TODO use real names/IDs
    mHWID.AppendPrintf("HMDInfo-0x%llx", aHMD);
    mDeviceId.AssignLiteral("somedevid");
    mDeviceName.AssignLiteral("HMD Device");

    mValid = true;
  }

  virtual ~HMDInfoVRDevice() { }

  /* If a field of view that is set to all 0's is passed in,
   * the recommended field of view for that eye is used.
   */
  virtual void SetFieldOfView(const VRFieldOfViewInit& aLeftFOV,
                              const VRFieldOfViewInit& aRightFOV,
                              double zNear, double zFar) MOZ_OVERRIDE
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

  virtual already_AddRefed<DOMPoint> GetEyeTranslation(VREye aEye) MOZ_OVERRIDE
  {
    gfx::Point3D p = mHMD->GetEyeTranslation(EyeToEye(aEye));

    nsRefPtr<DOMPoint> obj = new DOMPoint(mParent, p.x, p.y, p.z, 0.0);
    return obj.forget();
  }

  virtual VRFieldOfView* GetCurrentEyeFieldOfView(VREye aEye) MOZ_OVERRIDE
  {
    return CopyFieldOfView(mHMD->GetEyeFOV(EyeToEye(aEye)));
  }

  virtual VRFieldOfView* GetRecommendedEyeFieldOfView(VREye aEye) MOZ_OVERRIDE
  {
    return CopyFieldOfView(mHMD->GetRecommendedEyeFOV(EyeToEye(aEye)));
  }

  virtual VRFieldOfView* GetMaximumEyeFieldOfView(VREye aEye) MOZ_OVERRIDE
  {
    return CopyFieldOfView(mHMD->GetMaximumEyeFOV(EyeToEye(aEye)));
  }

  virtual already_AddRefed<DOMRect> GetRecommendedEyeRenderRect(VREye aEye) MOZ_OVERRIDE
  {
    const IntSize& a(mHMD->SuggestedEyeResolution());
    nsRefPtr<DOMRect> obj =
      new DOMRect(mParent,
                  (aEye == VREye::Left) ? 0 : a.width, 0,
                  a.width, a.height);
    return obj.forget();
  }

protected:
  VRFieldOfView*
  CopyFieldOfView(const gfx::VRFieldOfView& aSrc)
  {
    return new VRFieldOfView(aSrc.upDegrees, aSrc.rightDegrees,
                             aSrc.downDegrees, aSrc.leftDegrees);
  }
};

class HMDPositionVRDevice : public PositionSensorVRDevice
{
public:
  HMDPositionVRDevice(nsISupports* aParent, gfx::VRHMDInfo* aHMD)
    : PositionSensorVRDevice(aParent)
    , mHMD(aHMD)
    , mTracking(false)
  {
    // XXX TODO use real names/IDs
    mHWID.AppendPrintf("HMDInfo-0x%llx", aHMD);
    mDeviceId.AssignLiteral("somedevid");
    mDeviceName.AssignLiteral("HMD Position Device");

    mValid = true;
  }

  ~HMDPositionVRDevice()
  {
    if (mTracking) {
      mHMD->StopSensorTracking();
    }
  }

  virtual already_AddRefed<VRPositionState> GetState(double timeOffset) MOZ_OVERRIDE
  {
    if (!mTracking) {
      mHMD->StartSensorTracking();
      mTracking = true;
    }

    gfx::VRHMDSensorState state = mHMD->GetSensorState(timeOffset);
    nsRefPtr<VRPositionState> obj = new VRPositionState(mParent, state);

    return obj.forget();
  }

  virtual void ZeroSensor() MOZ_OVERRIDE
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
  if (!gfx::VRHMDManagerOculus::Init()) {
    NS_WARNING("Failed to initialize Oculus HMD Manager");
    return false;
  }

  nsTArray<nsRefPtr<gfx::VRHMDInfo>> hmds;
  gfx::VRHMDManagerOculus::GetOculusHMDs(hmds);

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
