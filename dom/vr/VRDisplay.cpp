/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWrapperCache.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/VRDisplay.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/VRDisplayBinding.h"
#include "Navigator.h"
#include "gfxVR.h"
#include "VRDisplayClient.h"
#include "VRManagerChild.h"
#include "VRDisplayPresentation.h"
#include "nsIObserverService.h"
#include "nsIFrame.h"
#include "nsISupportsPrimitives.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

VRFieldOfView::VRFieldOfView(nsISupports* aParent,
                             double aUpDegrees, double aRightDegrees,
                             double aDownDegrees, double aLeftDegrees)
  : mParent(aParent)
  , mUpDegrees(aUpDegrees)
  , mRightDegrees(aRightDegrees)
  , mDownDegrees(aDownDegrees)
  , mLeftDegrees(aLeftDegrees)
{
}

VRFieldOfView::VRFieldOfView(nsISupports* aParent, const gfx::VRFieldOfView& aSrc)
  : mParent(aParent)
  , mUpDegrees(aSrc.upDegrees)
  , mRightDegrees(aSrc.rightDegrees)
  , mDownDegrees(aSrc.downDegrees)
  , mLeftDegrees(aSrc.leftDegrees)
{
}

bool
VRDisplayCapabilities::HasPosition() const
{
  return bool(mFlags & gfx::VRDisplayCapabilityFlags::Cap_Position);
}

bool
VRDisplayCapabilities::HasOrientation() const
{
  return bool(mFlags & gfx::VRDisplayCapabilityFlags::Cap_Orientation);
}

bool
VRDisplayCapabilities::HasExternalDisplay() const
{
  return bool(mFlags & gfx::VRDisplayCapabilityFlags::Cap_External);
}

bool
VRDisplayCapabilities::CanPresent() const
{
  return bool(mFlags & gfx::VRDisplayCapabilityFlags::Cap_Present);
}

uint32_t
VRDisplayCapabilities::MaxLayers() const
{
  return CanPresent() ? 1 : 0;
}

/*static*/ bool
VRDisplay::RefreshVRDisplays(dom::Navigator* aNavigator)
{
  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  return vm && vm->RefreshVRDisplaysWithCallback(aNavigator);
}

/*static*/ void
VRDisplay::UpdateVRDisplays(nsTArray<RefPtr<VRDisplay>>& aDisplays, nsPIDOMWindowInner* aWindow)
{
  nsTArray<RefPtr<VRDisplay>> displays;

  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  nsTArray<RefPtr<gfx::VRDisplayClient>> updatedDisplays;
  if (vm && vm->GetVRDisplays(updatedDisplays)) {
    for (size_t i = 0; i < updatedDisplays.Length(); i++) {
      RefPtr<gfx::VRDisplayClient> display = updatedDisplays[i];
      bool isNewDisplay = true;
      for (size_t j = 0; j < aDisplays.Length(); j++) {
        if (aDisplays[j]->GetClient()->GetDisplayInfo() == display->GetDisplayInfo()) {
          displays.AppendElement(aDisplays[j]);
          isNewDisplay = false;
        }
      }

      if (isNewDisplay) {
        displays.AppendElement(new VRDisplay(aWindow, display));
      }
    }
  }

  aDisplays = displays;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(VRFieldOfView, mParent)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(VRFieldOfView, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(VRFieldOfView, Release)


JSObject*
VRFieldOfView::WrapObject(JSContext* aCx,
                          JS::Handle<JSObject*> aGivenProto)
{
  return VRFieldOfViewBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(VREyeParameters)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(VREyeParameters)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent, mFOV)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mOffset = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(VREyeParameters)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent, mFOV)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(VREyeParameters)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mOffset)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(VREyeParameters, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(VREyeParameters, Release)

VREyeParameters::VREyeParameters(nsISupports* aParent,
                                 const gfx::Point3D& aEyeTranslation,
                                 const gfx::VRFieldOfView& aFOV,
                                 const gfx::IntSize& aRenderSize)
  : mParent(aParent)
  , mEyeTranslation(aEyeTranslation)
  , mRenderSize(aRenderSize)
{
  mFOV = new VRFieldOfView(aParent, aFOV);
  mozilla::HoldJSObjects(this);
}

VREyeParameters::~VREyeParameters()
{
  mozilla::DropJSObjects(this);
}

VRFieldOfView*
VREyeParameters::FieldOfView()
{
  return mFOV;
}

void
VREyeParameters::GetOffset(JSContext* aCx, JS::MutableHandle<JSObject*> aRetval, ErrorResult& aRv)
{
  if (!mOffset) {
    // Lazily create the Float32Array
    mOffset = dom::Float32Array::Create(aCx, this, 3, mEyeTranslation.components);
    if (!mOffset) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  aRetval.set(mOffset);
}

JSObject*
VREyeParameters::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VREyeParametersBinding::Wrap(aCx, this, aGivenProto);
}

VRStageParameters::VRStageParameters(nsISupports* aParent,
                                     const gfx::Matrix4x4& aSittingToStandingTransform,
                                     const gfx::Size& aSize)
  : mParent(aParent)
  , mSittingToStandingTransform(aSittingToStandingTransform)
  , mSittingToStandingTransformArray(nullptr)
  , mSize(aSize)
{
  mozilla::HoldJSObjects(this);
}

VRStageParameters::~VRStageParameters()
{
  mozilla::DropJSObjects(this);
}

JSObject*
VRStageParameters::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VRStageParametersBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(VRStageParameters)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(VRStageParameters)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mSittingToStandingTransformArray = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(VRStageParameters)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(VRStageParameters)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mSittingToStandingTransformArray)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(VRStageParameters, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(VRStageParameters, Release)

void
VRStageParameters::GetSittingToStandingTransform(JSContext* aCx,
                                                 JS::MutableHandle<JSObject*> aRetval,
                                                 ErrorResult& aRv)
{
  if (!mSittingToStandingTransformArray) {
    // Lazily create the Float32Array
    mSittingToStandingTransformArray = dom::Float32Array::Create(aCx, this, 16,
      mSittingToStandingTransform.components);
    if (!mSittingToStandingTransformArray) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  aRetval.set(mSittingToStandingTransformArray);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(VRDisplayCapabilities, mParent)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(VRDisplayCapabilities, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(VRDisplayCapabilities, Release)

JSObject*
VRDisplayCapabilities::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VRDisplayCapabilitiesBinding::Wrap(aCx, this, aGivenProto);
}

VRPose::VRPose(nsISupports* aParent, const gfx::VRHMDSensorState& aState)
  : Pose(aParent)
  , mVRState(aState)
{
  mFrameId = aState.inputFrameID;
  mozilla::HoldJSObjects(this);
}

VRPose::VRPose(nsISupports* aParent)
  : Pose(aParent)
{
  mFrameId = 0;
  mVRState.Clear();
  mozilla::HoldJSObjects(this);
}

VRPose::~VRPose()
{
  mozilla::DropJSObjects(this);
}

void
VRPose::GetPosition(JSContext* aCx,
                    JS::MutableHandle<JSObject*> aRetval,
                    ErrorResult& aRv)
{
  SetFloat32Array(aCx, aRetval, mPosition, mVRState.position, 3,
    !mPosition && bool(mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_Position),
    aRv);
}

void
VRPose::GetLinearVelocity(JSContext* aCx,
                          JS::MutableHandle<JSObject*> aRetval,
                          ErrorResult& aRv)
{
  SetFloat32Array(aCx, aRetval, mLinearVelocity, mVRState.linearVelocity, 3,
    !mLinearVelocity && bool(mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_Position),
    aRv);
}

void
VRPose::GetLinearAcceleration(JSContext* aCx,
                              JS::MutableHandle<JSObject*> aRetval,
                              ErrorResult& aRv)
{
  SetFloat32Array(aCx, aRetval, mLinearAcceleration, mVRState.linearAcceleration, 3,
    !mLinearAcceleration && bool(mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_LinearAcceleration),
    aRv);

}

void
VRPose::GetOrientation(JSContext* aCx,
                       JS::MutableHandle<JSObject*> aRetval,
                       ErrorResult& aRv)
{
  SetFloat32Array(aCx, aRetval, mOrientation, mVRState.orientation, 4,
    !mOrientation && bool(mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_Orientation),
    aRv);
}

void
VRPose::GetAngularVelocity(JSContext* aCx,
                           JS::MutableHandle<JSObject*> aRetval,
                           ErrorResult& aRv)
{
  SetFloat32Array(aCx, aRetval, mAngularVelocity, mVRState.angularVelocity, 3,
    !mAngularVelocity && bool(mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_Orientation),
    aRv);
}

void
VRPose::GetAngularAcceleration(JSContext* aCx,
                               JS::MutableHandle<JSObject*> aRetval,
                               ErrorResult& aRv)
{
  SetFloat32Array(aCx, aRetval, mAngularAcceleration, mVRState.angularAcceleration, 3,
    !mAngularAcceleration && bool(mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_AngularAcceleration),
    aRv);
}

JSObject*
VRPose::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VRPoseBinding::Wrap(aCx, this, aGivenProto);
}

/* virtual */ JSObject*
VRDisplay::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VRDisplayBinding::Wrap(aCx, this, aGivenProto);
}

VRDisplay::VRDisplay(nsPIDOMWindowInner* aWindow, gfx::VRDisplayClient* aClient)
  : DOMEventTargetHelper(aWindow)
  , mClient(aClient)
  , mDepthNear(0.01f) // Default value from WebVR Spec
  , mDepthFar(10000.0f) // Default value from WebVR Spec
{
  const gfx::VRDisplayInfo& info = aClient->GetDisplayInfo();
  mDisplayId = info.GetDisplayID();
  mDisplayName = NS_ConvertASCIItoUTF16(info.GetDisplayName());
  mCapabilities = new VRDisplayCapabilities(aWindow, info.GetCapabilities());
  if (info.GetCapabilities() & gfx::VRDisplayCapabilityFlags::Cap_StageParameters) {
    mStageParameters = new VRStageParameters(aWindow,
                                             info.GetSittingToStandingTransform(),
                                             info.GetStageSize());
  }
  mozilla::HoldJSObjects(this);
}

VRDisplay::~VRDisplay()
{
  ExitPresentInternal();
  mozilla::DropJSObjects(this);
}

void
VRDisplay::LastRelease()
{
  // We don't want to wait for the CC to free up the presentation
  // for use in other documents, so we do this in LastRelease().
  ExitPresentInternal();
}

already_AddRefed<VREyeParameters>
VRDisplay::GetEyeParameters(VREye aEye)
{
  gfx::VRDisplayInfo::Eye eye = aEye == VREye::Left ? gfx::VRDisplayInfo::Eye_Left : gfx::VRDisplayInfo::Eye_Right;
  RefPtr<VREyeParameters> params =
    new VREyeParameters(GetParentObject(),
                        mClient->GetDisplayInfo().GetEyeTranslation(eye),
                        mClient->GetDisplayInfo().GetEyeFOV(eye),
                        mClient->GetDisplayInfo().SuggestedEyeResolution());
  return params.forget();
}

VRDisplayCapabilities*
VRDisplay::Capabilities()
{
  return mCapabilities;
}

VRStageParameters*
VRDisplay::GetStageParameters()
{
  return mStageParameters;
}

void
VRDisplay::UpdateFrameInfo()
{
  /**
   * The WebVR 1.1 spec Requires that VRDisplay.getPose and VRDisplay.getFrameData
   * must return the same values until the next VRDisplay.submitFrame.
   *
   * mFrameInfo is marked dirty at the end of the frame or start of a new
   * composition and lazily created here in order to receive mid-frame
   * pose-prediction updates while still ensuring conformance to the WebVR spec
   * requirements.
   *
   * If we are not presenting WebVR content, the frame will never end and we should
   * return the latest frame data always.
   */
  if (mFrameInfo.IsDirty() || !mPresentation) {
    gfx::VRHMDSensorState state = mClient->GetSensorState();
    const gfx::VRDisplayInfo& info = mClient->GetDisplayInfo();
    mFrameInfo.Update(info, state, mDepthNear, mDepthFar);
  }
}

bool
VRDisplay::GetFrameData(VRFrameData& aFrameData)
{
  UpdateFrameInfo();
  aFrameData.Update(mFrameInfo);
  return true;
}

already_AddRefed<VRPose>
VRDisplay::GetPose()
{
  UpdateFrameInfo();
  RefPtr<VRPose> obj = new VRPose(GetParentObject(), mFrameInfo.mVRState);

  return obj.forget();
}

void
VRDisplay::ResetPose()
{
  mClient->ZeroSensor();
}

already_AddRefed<Promise>
VRDisplay::RequestPresent(const nsTArray<VRLayer>& aLayers, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, nullptr);

  if (mClient->GetIsPresenting()) {
    // Only one presentation allowed per VRDisplay
    // on a first-come-first-serve basis.
    promise->MaybeRejectWithUndefined();
  } else {
    mPresentation = mClient->BeginPresentation(aLayers);
    mFrameInfo.Clear();

    nsresult rv = obs->AddObserver(this, "inner-window-destroyed", false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPresentation = nullptr;
      promise->MaybeRejectWithUndefined();
    } else {
      promise->MaybeResolve(JS::UndefinedHandleValue);
    }
  }
  return promise.forget();
}

NS_IMETHODIMP
VRDisplay::Observe(nsISupports* aSubject, const char* aTopic,
  const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (strcmp(aTopic, "inner-window-destroyed") == 0) {
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

    uint64_t innerID;
    nsresult rv = wrapper->GetData(&innerID);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!GetOwner() || GetOwner()->WindowID() == innerID) {
      ExitPresentInternal();
    }

    return NS_OK;
  }

  // This should not happen.
  return NS_ERROR_FAILURE;
}

already_AddRefed<Promise>
VRDisplay::ExitPresent(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }


  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  if (!IsPresenting()) {
    // We can not exit a presentation outside of the context that
    // started the presentation.
    promise->MaybeRejectWithUndefined();
  } else {
    promise->MaybeResolve(JS::UndefinedHandleValue);
    ExitPresentInternal();
  }

  return promise.forget();
}

void
VRDisplay::ExitPresentInternal()
{
  mPresentation = nullptr;
}

void
VRDisplay::GetLayers(nsTArray<VRLayer>& result)
{
  if (mPresentation) {
    mPresentation->GetDOMLayers(result);
  } else {
    result = nsTArray<VRLayer>();
  }
}

void
VRDisplay::SubmitFrame()
{
  if (mPresentation) {
    mPresentation->SubmitFrame();
  }
  mFrameInfo.Clear();
}

int32_t
VRDisplay::RequestAnimationFrame(FrameRequestCallback& aCallback,
ErrorResult& aError)
{
  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();

  int32_t handle;
  aError = vm->ScheduleFrameRequestCallback(aCallback, &handle);
  return handle;
}

void
VRDisplay::CancelAnimationFrame(int32_t aHandle, ErrorResult& aError)
{
  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  vm->CancelFrameRequestCallback(aHandle);
}


bool
VRDisplay::IsPresenting() const
{
  // IsPresenting returns true only if this Javascript context is presenting
  // and will return false if another context is presenting.
  return mPresentation != nullptr;
}

bool
VRDisplay::IsConnected() const
{
  return mClient->GetIsConnected();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(VRDisplay, DOMEventTargetHelper, mCapabilities, mStageParameters)

NS_IMPL_ADDREF_INHERITED(VRDisplay, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(VRDisplay, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(VRDisplay)
NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, DOMEventTargetHelper)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(VRFrameData)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(VRFrameData)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent, mPose)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mLeftProjectionMatrix = nullptr;
  tmp->mLeftViewMatrix = nullptr;
  tmp->mRightProjectionMatrix = nullptr;
  tmp->mRightViewMatrix = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(VRFrameData)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent, mPose)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(VRFrameData)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mLeftProjectionMatrix)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mLeftViewMatrix)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mRightProjectionMatrix)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mRightViewMatrix)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(VRFrameData, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(VRFrameData, Release)

VRFrameData::VRFrameData(nsISupports* aParent)
  : mParent(aParent)
  , mLeftProjectionMatrix(nullptr)
  , mLeftViewMatrix(nullptr)
  , mRightProjectionMatrix(nullptr)
  , mRightViewMatrix(nullptr)
{
  mPose = new VRPose(aParent);
}

VRFrameData::~VRFrameData()
{
  mozilla::DropJSObjects(this);
}

/* static */ already_AddRefed<VRFrameData>
VRFrameData::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  RefPtr<VRFrameData> obj = new VRFrameData(aGlobal.GetAsSupports());
  return obj.forget();
}

JSObject*
VRFrameData::WrapObject(JSContext* aCx,
                        JS::Handle<JSObject*> aGivenProto)
{
  return VRFrameDataBinding::Wrap(aCx, this, aGivenProto);
}

VRPose*
VRFrameData::Pose()
{
  return mPose;
}

void
VRFrameData::LazyCreateMatrix(JS::Heap<JSObject*>& aArray, gfx::Matrix4x4& aMat, JSContext* aCx,
                              JS::MutableHandle<JSObject*> aRetval, ErrorResult& aRv)
{
  if (!aArray) {
    // Lazily create the Float32Array
    aArray = dom::Float32Array::Create(aCx, this, 16, aMat.components);
    if (!aArray) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  if (aArray) {
    JS::ExposeObjectToActiveJS(aArray);
  }
  aRetval.set(aArray);
}

double
VRFrameData::Timestamp() const
{
  // Converting from seconds to milliseconds
  return mFrameInfo.mVRState.timestamp * 1000.0f;
}

void
VRFrameData::GetLeftProjectionMatrix(JSContext* aCx,
                                     JS::MutableHandle<JSObject*> aRetval,
                                     ErrorResult& aRv)
{
  LazyCreateMatrix(mLeftProjectionMatrix, mFrameInfo.mLeftProjection, aCx,
                   aRetval, aRv);
}

void
VRFrameData::GetLeftViewMatrix(JSContext* aCx,
                               JS::MutableHandle<JSObject*> aRetval,
                               ErrorResult& aRv)
{
  LazyCreateMatrix(mLeftViewMatrix, mFrameInfo.mLeftView, aCx, aRetval, aRv);
}

void
VRFrameData::GetRightProjectionMatrix(JSContext* aCx,
                                      JS::MutableHandle<JSObject*> aRetval,
                                      ErrorResult& aRv)
{
  LazyCreateMatrix(mRightProjectionMatrix, mFrameInfo.mRightProjection, aCx,
                   aRetval, aRv);
}

void
VRFrameData::GetRightViewMatrix(JSContext* aCx,
                                JS::MutableHandle<JSObject*> aRetval,
                                ErrorResult& aRv)
{
  LazyCreateMatrix(mRightViewMatrix, mFrameInfo.mRightView, aCx, aRetval, aRv);
}

void
VRFrameData::Update(const VRFrameInfo& aFrameInfo)
{
  mFrameInfo = aFrameInfo;

  mLeftProjectionMatrix = nullptr;
  mLeftViewMatrix = nullptr;
  mRightProjectionMatrix = nullptr;
  mRightViewMatrix = nullptr;

  mPose = new VRPose(GetParentObject(), mFrameInfo.mVRState);
}

void
VRFrameInfo::Update(const gfx::VRDisplayInfo& aInfo,
                    const gfx::VRHMDSensorState& aState,
                    float aDepthNear,
                    float aDepthFar)
{
  mVRState = aState;

  gfx::Quaternion qt;
  if (mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_Orientation) {
    qt.x = mVRState.orientation[0];
    qt.y = mVRState.orientation[1];
    qt.z = mVRState.orientation[2];
    qt.w = mVRState.orientation[3];
  }
  gfx::Point3D pos;
  if (mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_Position) {
    pos.x = -mVRState.position[0];
    pos.y = -mVRState.position[1];
    pos.z = -mVRState.position[2];
  }
  gfx::Matrix4x4 matHead;
  matHead.SetRotationFromQuaternion(qt);
  matHead.PreTranslate(pos);

  mLeftView = matHead;
  mLeftView.PostTranslate(-aInfo.mEyeTranslation[gfx::VRDisplayInfo::Eye_Left]);

  mRightView = matHead;
  mRightView.PostTranslate(-aInfo.mEyeTranslation[gfx::VRDisplayInfo::Eye_Right]);

  // Avoid division by zero within ConstructProjectionMatrix
  const float kEpsilon = 0.00001f;
  if (fabs(aDepthFar - aDepthNear) < kEpsilon) {
    aDepthFar = aDepthNear + kEpsilon;
  }

  const gfx::VRFieldOfView leftFOV = aInfo.mEyeFOV[gfx::VRDisplayInfo::Eye_Left];
  mLeftProjection = leftFOV.ConstructProjectionMatrix(aDepthNear, aDepthFar, true);
  const gfx::VRFieldOfView rightFOV = aInfo.mEyeFOV[gfx::VRDisplayInfo::Eye_Right];
  mRightProjection = rightFOV.ConstructProjectionMatrix(aDepthNear, aDepthFar, true);
}

VRFrameInfo::VRFrameInfo()
{
  mVRState.Clear();
}

bool
VRFrameInfo::IsDirty()
{
  return mVRState.timestamp == 0;
}

void
VRFrameInfo::Clear()
{
  mVRState.Clear();
}

} // namespace dom
} // namespace mozilla
