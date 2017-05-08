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
#include "mozilla/dom/Pose.h"
#include "mozilla/TimeStamp.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

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

class VRPose final : public Pose
{

public:
  VRPose(nsISupports* aParent, const gfx::VRHMDSensorState& aState);
  explicit VRPose(nsISupports* aParent);

  uint32_t FrameID() const { return mFrameId; }

  virtual void GetPosition(JSContext* aCx,
                           JS::MutableHandle<JSObject*> aRetval,
                           ErrorResult& aRv) override;
  virtual void GetLinearVelocity(JSContext* aCx,
                                 JS::MutableHandle<JSObject*> aRetval,
                                 ErrorResult& aRv) override;
  virtual void GetLinearAcceleration(JSContext* aCx,
                                     JS::MutableHandle<JSObject*> aRetval,
                                     ErrorResult& aRv) override;
  virtual void GetOrientation(JSContext* aCx,
                              JS::MutableHandle<JSObject*> aRetval,
                              ErrorResult& aRv) override;
  virtual void GetAngularVelocity(JSContext* aCx,
                                  JS::MutableHandle<JSObject*> aRetval,
                                  ErrorResult& aRv) override;
  virtual void GetAngularAcceleration(JSContext* aCx,
                                      JS::MutableHandle<JSObject*> aRetval,
                                      ErrorResult& aRv) override;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  ~VRPose();

  uint32_t mFrameId;
  gfx::VRHMDSensorState mVRState;
};

struct VRFrameInfo
{
  VRFrameInfo();

  void Update(const gfx::VRDisplayInfo& aInfo,
              const gfx::VRHMDSensorState& aState,
              float aDepthNear,
              float aDepthFar);

  void Clear();
  bool IsDirty();

  gfx::VRHMDSensorState mVRState;
  gfx::Matrix4x4 mLeftProjection;
  gfx::Matrix4x4 mLeftView;
  gfx::Matrix4x4 mRightProjection;
  gfx::Matrix4x4 mRightView;

  /**
   * In order to avoid leaking information related to the duration of
   * the user's VR session, we re-base timestamps.
   * mTimeStampOffset is added to the actual timestamp returned by the
   * underlying VR platform API when returned through WebVR API's.
   */
  double mTimeStampOffset;
};

class VRFrameData final : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(VRFrameData)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(VRFrameData)

  explicit VRFrameData(nsISupports* aParent);
  static already_AddRefed<VRFrameData> Constructor(const GlobalObject& aGlobal,
                                                   ErrorResult& aRv);

  void Update(const VRFrameInfo& aFrameInfo);

  // WebIDL Members
  double Timestamp() const;
  void GetLeftProjectionMatrix(JSContext* aCx,
                               JS::MutableHandle<JSObject*> aRetval,
                               ErrorResult& aRv);
  void GetLeftViewMatrix(JSContext* aCx,
                         JS::MutableHandle<JSObject*> aRetval,
                         ErrorResult& aRv);
  void GetRightProjectionMatrix(JSContext* aCx,
                               JS::MutableHandle<JSObject*> aRetval,
                               ErrorResult& aRv);
  void GetRightViewMatrix(JSContext* aCx,
                          JS::MutableHandle<JSObject*> aRetval,
                          ErrorResult& aRv);

  VRPose* Pose();

  // WebIDL Boilerplate
  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  ~VRFrameData();
  nsCOMPtr<nsISupports> mParent;

  VRFrameInfo mFrameInfo;
  RefPtr<VRPose> mPose;
  JS::Heap<JSObject*> mLeftProjectionMatrix;
  JS::Heap<JSObject*> mLeftViewMatrix;
  JS::Heap<JSObject*> mRightProjectionMatrix;
  JS::Heap<JSObject*> mRightViewMatrix;

  void LazyCreateMatrix(JS::Heap<JSObject*>& aArray, gfx::Matrix4x4& aMat,
                        JSContext* aCx, JS::MutableHandle<JSObject*> aRetval,
                        ErrorResult& aRv);
};

class VRStageParameters final : public nsWrapperCache
{
public:
  VRStageParameters(nsISupports* aParent,
                    const gfx::Matrix4x4& aSittingToStandingTransform,
                    const gfx::Size& aSize);

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
  ~VRStageParameters();

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
  ~VREyeParameters();

  nsCOMPtr<nsISupports> mParent;


  gfx::Point3D mEyeTranslation;
  gfx::IntSize mRenderSize;
  JS::Heap<JSObject*> mOffset;
  RefPtr<VRFieldOfView> mFOV;
};

class VRSubmitFrameResult final : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(VRSubmitFrameResult)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(VRSubmitFrameResult)

  explicit VRSubmitFrameResult(nsISupports* aParent);
  static already_AddRefed<VRSubmitFrameResult> Constructor(const GlobalObject& aGlobal,
                                                           ErrorResult& aRv);

  void Update(uint32_t aFrameNum, const nsACString& aBase64Image);
  // WebIDL Members
  double FrameNum() const;
  void GetBase64Image(nsAString& aImage) const;

  // WebIDL Boilerplate
  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  ~VRSubmitFrameResult();

  nsCOMPtr<nsISupports> mParent;
  nsString mBase64Image;
  uint32_t mFrameNum;
};

class VRDisplay final : public DOMEventTargetHelper
                      , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(VRDisplay, DOMEventTargetHelper)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint32_t PresentingGroups() const;
  uint32_t GroupMask() const;
  void SetGroupMask(const uint32_t& aGroupMask);
  bool IsAnyPresenting(uint32_t aGroupMask) const;
  bool IsPresenting() const;
  bool IsConnected() const;

  VRDisplayCapabilities* Capabilities();
  VRStageParameters* GetStageParameters();

  uint32_t DisplayId() const { return mDisplayId; }
  void GetDisplayName(nsAString& aDisplayName) const { aDisplayName = mDisplayName; }

  static bool RefreshVRDisplays(uint64_t aWindowId);
  static void UpdateVRDisplays(nsTArray<RefPtr<VRDisplay> >& aDisplays,
                               nsPIDOMWindowInner* aWindow);

  gfx::VRDisplayClient *GetClient() {
    return mClient;
  }

  virtual already_AddRefed<VREyeParameters> GetEyeParameters(VREye aEye);

  bool GetFrameData(VRFrameData& aFrameData);
  bool GetSubmitFrameResult(VRSubmitFrameResult& aResult);
  already_AddRefed<VRPose> GetPose();
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

  already_AddRefed<Promise> RequestPresent(const nsTArray<VRLayer>& aLayers,
                                           CallerType aCallerType,
                                           ErrorResult& aRv);
  already_AddRefed<Promise> ExitPresent(ErrorResult& aRv);
  void GetLayers(nsTArray<VRLayer>& result);
  void SubmitFrame();

  int32_t RequestAnimationFrame(mozilla::dom::FrameRequestCallback& aCallback,
                                mozilla::ErrorResult& aError);
  void CancelAnimationFrame(int32_t aHandle, mozilla::ErrorResult& aError);
  void StartHandlingVRNavigationEvent();
  void StopHandlingVRNavigationEvent();
  bool IsHandlingVRNavigationEvent();

protected:
  VRDisplay(nsPIDOMWindowInner* aWindow, gfx::VRDisplayClient* aClient);
  virtual ~VRDisplay();
  virtual void LastRelease() override;

  void ExitPresentInternal();
  void Shutdown();
  void UpdateFrameInfo();

  RefPtr<gfx::VRDisplayClient> mClient;

  uint32_t mDisplayId;
  nsString mDisplayName;

  RefPtr<VRDisplayCapabilities> mCapabilities;
  RefPtr<VRStageParameters> mStageParameters;

  double mDepthNear;
  double mDepthFar;

  RefPtr<gfx::VRDisplayPresentation> mPresentation;

  /**
  * The WebVR 1.1 spec Requires that VRDisplay.getPose and VRDisplay.getFrameData
  * must return the same values until the next VRDisplay.submitFrame.
  * mFrameInfo is updated only on the first call to either function within one
  * frame.  Subsequent calls before the next SubmitFrame or ExitPresent call
  * will use these cached values.
  */
  VRFrameInfo mFrameInfo;

  // Time at which we began expecting VR navigation.
  TimeStamp mHandlingVRNavigationEventStart;
  int32_t mVRNavigationEventDepth;
  bool mShutdown;
};

} // namespace dom
} // namespace mozilla

#endif
