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
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DOMPoint.h"
#include "mozilla/dom/DOMRect.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

#include "gfxVR.h"

namespace mozilla {
namespace gfx {
class VRDisplayClient;
class VRDisplayPresentation;
struct VRFieldOfView;
enum class VRDisplayCapabilityFlags : uint16_t;
struct VRHMDSensorState;
}
namespace dom {
class Navigator;

class VRFieldOfView final : public nsWrapperCache
{
public:
  VRFieldOfView(nsISupports* aParent,
                double aUpDegrees, double aRightDegrees,
                double aDownDegrees, double aLeftDegrees);
  VRFieldOfView(nsISupports* aParent, const gfx::VRFieldOfView& aSrc);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(VRFieldOfView)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(VRFieldOfView)

  double UpDegrees() const { return mUpDegrees; }
  double RightDegrees() const { return mRightDegrees; }
  double DownDegrees() const { return mDownDegrees; }
  double LeftDegrees() const { return mLeftDegrees; }

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  virtual ~VRFieldOfView() {}

  nsCOMPtr<nsISupports> mParent;

  double mUpDegrees;
  double mRightDegrees;
  double mDownDegrees;
  double mLeftDegrees;
};

class VRDisplayCapabilities final : public nsWrapperCache
{
public:
  VRDisplayCapabilities(nsISupports* aParent, const gfx::VRDisplayCapabilityFlags& aFlags)
    : mParent(aParent)
    , mFlags(aFlags)
  {
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(VRDisplayCapabilities)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(VRDisplayCapabilities)

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  bool HasPosition() const;
  bool HasOrientation() const;
  bool HasExternalDisplay() const;
  bool CanPresent() const;
  uint32_t MaxLayers() const;

protected:
  ~VRDisplayCapabilities() {}
  nsCOMPtr<nsISupports> mParent;
  gfx::VRDisplayCapabilityFlags mFlags;
};

class VRPose final : public nsWrapperCache
{

public:
  VRPose(nsISupports* aParent, const gfx::VRHMDSensorState& aState);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(VRPose)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(VRPose)

  double Timestamp() const { return mTimeStamp; }
  uint32_t FrameID() const { return mFrameId; }

  void GetPosition(JSContext* aCx,
                   JS::MutableHandle<JSObject*> aRetval,
                   ErrorResult& aRv);
  void GetLinearVelocity(JSContext* aCx,
                         JS::MutableHandle<JSObject*> aRetval,
                         ErrorResult& aRv);
  void GetLinearAcceleration(JSContext* aCx,
                             JS::MutableHandle<JSObject*> aRetval,
                             ErrorResult& aRv);
  void GetOrientation(JSContext* aCx,
                      JS::MutableHandle<JSObject*> aRetval,
                      ErrorResult& aRv);
  void GetAngularVelocity(JSContext* aCx,
                          JS::MutableHandle<JSObject*> aRetval,
                          ErrorResult& aRv);
  void GetAngularAcceleration(JSContext* aCx,
                              JS::MutableHandle<JSObject*> aRetval,
                              ErrorResult& aRv);

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  ~VRPose() {}
  nsCOMPtr<nsISupports> mParent;

  double mTimeStamp;
  uint32_t mFrameId;
  gfx::VRHMDSensorState mVRState;

  JS::Heap<JSObject*> mPosition;
  JS::Heap<JSObject*> mLinearVelocity;
  JS::Heap<JSObject*> mLinearAcceleration;
  JS::Heap<JSObject*> mOrientation;
  JS::Heap<JSObject*> mAngularVelocity;
  JS::Heap<JSObject*> mAngularAcceleration;

};

class VRStageParameters final : public nsWrapperCache
{
public:
  VRStageParameters(nsISupports* aParent,
                    const gfx::Matrix4x4& aSittingToStandingTransform,
                    const gfx::Size& aSize)
    : mParent(aParent)
    , mSittingToStandingTransform(aSittingToStandingTransform)
    , mSittingToStandingTransformArray(nullptr)
    , mSize(aSize)
  {
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(VRStageParameters)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(VRStageParameters)

  void GetSittingToStandingTransform(JSContext* aCx,
                                     JS::MutableHandle<JSObject*> aRetval,
                                     ErrorResult& aRv);
  float SizeX() const { return mSize.width; }
  float SizeZ() const { return mSize.height; }

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  ~VRStageParameters() {}

  nsCOMPtr<nsISupports> mParent;

  gfx::Matrix4x4 mSittingToStandingTransform;
  JS::Heap<JSObject*> mSittingToStandingTransformArray;
  gfx::Size mSize;
};

class VREyeParameters final : public nsWrapperCache
{
public:
  VREyeParameters(nsISupports* aParent,
                  const gfx::Point3D& aEyeTranslation,
                  const gfx::VRFieldOfView& aFOV,
                  const gfx::IntSize& aRenderSize);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(VREyeParameters)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(VREyeParameters)

  void GetOffset(JSContext* aCx, JS::MutableHandle<JSObject*> aRetVal,
                 ErrorResult& aRv);

  VRFieldOfView* FieldOfView();

  uint32_t RenderWidth() const { return mRenderSize.width; }
  uint32_t RenderHeight() const { return mRenderSize.height; }

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
protected:
  ~VREyeParameters() {}

  nsCOMPtr<nsISupports> mParent;


  gfx::Point3D mEyeTranslation;
  gfx::IntSize mRenderSize;
  JS::Heap<JSObject*> mOffset;
  RefPtr<VRFieldOfView> mFOV;
};

class VRDisplay final : public DOMEventTargetHelper
                      , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(VRDisplay, DOMEventTargetHelper)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  bool IsPresenting() const;
  bool IsConnected() const;

  VRDisplayCapabilities* Capabilities();
  VRStageParameters* GetStageParameters();

  uint32_t DisplayId() const { return mDisplayId; }
  void GetDisplayName(nsAString& aDisplayName) const { aDisplayName = mDisplayName; }

  static bool RefreshVRDisplays(dom::Navigator* aNavigator);
  static void UpdateVRDisplays(nsTArray<RefPtr<VRDisplay> >& aDisplays,
                               nsPIDOMWindowInner* aWindow);

  gfx::VRDisplayClient *GetClient() {
    return mClient;
  }

  virtual already_AddRefed<VREyeParameters> GetEyeParameters(VREye aEye);

  already_AddRefed<VRPose> GetPose();
  already_AddRefed<VRPose> GetImmediatePose();
  void ResetPose();

  double DepthNear() {
    return mDepthNear;
  }

  double DepthFar() {
    return mDepthFar;
  }

  void SetDepthNear(double aDepthNear) {
    // XXX When we start sending depth buffers to VRLayer's we will want
    // to communicate this with the VRDisplayHost
    mDepthNear = aDepthNear;
  }

  void SetDepthFar(double aDepthFar) {
    // XXX When we start sending depth buffers to VRLayer's we will want
    // to communicate this with the VRDisplayHost
    mDepthFar = aDepthFar;
  }

  already_AddRefed<Promise> RequestPresent(const nsTArray<VRLayer>& aLayers, ErrorResult& aRv);
  already_AddRefed<Promise> ExitPresent(ErrorResult& aRv);
  void GetLayers(nsTArray<VRLayer>& result);
  void SubmitFrame(const Optional<NonNull<VRPose>>& aPose);

  int32_t RequestAnimationFrame(mozilla::dom::FrameRequestCallback& aCallback,
                                mozilla::ErrorResult& aError);
  void CancelAnimationFrame(int32_t aHandle, mozilla::ErrorResult& aError);

protected:
  VRDisplay(nsPIDOMWindowInner* aWindow, gfx::VRDisplayClient* aClient);
  virtual ~VRDisplay();
  virtual void LastRelease() override;

  void ExitPresentInternal();

  RefPtr<gfx::VRDisplayClient> mClient;

  uint32_t mDisplayId;
  nsString mDisplayName;

  RefPtr<VRDisplayCapabilities> mCapabilities;

  double mDepthNear;
  double mDepthFar;

  RefPtr<gfx::VRDisplayPresentation> mPresentation;
};

} // namespace dom
} // namespace mozilla

#endif
