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
  JS::ExposeObjectToActiveJS(mOffset);
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
  JS::ExposeObjectToActiveJS(mSittingToStandingTransformArray);
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

NS_IMPL_CYCLE_COLLECTION_CLASS(VRPose)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(VRPose)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mPosition = nullptr;
  tmp->mLinearVelocity = nullptr;
  tmp->mLinearAcceleration = nullptr;
  tmp->mOrientation = nullptr;
  tmp->mAngularVelocity = nullptr;
  tmp->mAngularAcceleration = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(VRPose)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(VRPose)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mPosition)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mLinearVelocity)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mLinearAcceleration)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mOrientation)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAngularVelocity)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAngularAcceleration)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(VRPose, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(VRPose, Release)

VRPose::VRPose(nsISupports* aParent, const gfx::VRHMDSensorState& aState)
  : mParent(aParent)
  , mVRState(aState)
  , mPosition(nullptr)
  , mLinearVelocity(nullptr)
  , mLinearAcceleration(nullptr)
  , mOrientation(nullptr)
  , mAngularVelocity(nullptr)
  , mAngularAcceleration(nullptr)
{
  mTimeStamp = aState.timestamp * 1000.0f; // Converting from seconds to ms
  mFrameId = aState.inputFrameID;
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
  if (!mPosition && mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_Position) {
    // Lazily create the Float32Array
    mPosition = dom::Float32Array::Create(aCx, this, 3, mVRState.position);
    if (!mPosition) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  if (mPosition) {
    JS::ExposeObjectToActiveJS(mPosition);
  }
  aRetval.set(mPosition);
}

void
VRPose::GetLinearVelocity(JSContext* aCx,
                          JS::MutableHandle<JSObject*> aRetval,
                          ErrorResult& aRv)
{
  if (!mLinearVelocity && mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_Position) {
    // Lazily create the Float32Array
    mLinearVelocity = dom::Float32Array::Create(aCx, this, 3, mVRState.linearVelocity);
    if (!mLinearVelocity) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  if (mLinearVelocity) {
    JS::ExposeObjectToActiveJS(mLinearVelocity);
  }
  aRetval.set(mLinearVelocity);
}

void
VRPose::GetLinearAcceleration(JSContext* aCx,
                              JS::MutableHandle<JSObject*> aRetval,
                              ErrorResult& aRv)
{
  if (!mLinearAcceleration && mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_LinearAcceleration) {
    // Lazily create the Float32Array
    mLinearAcceleration = dom::Float32Array::Create(aCx, this, 3, mVRState.linearAcceleration);
    if (!mLinearAcceleration) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  if (mLinearAcceleration) {
    JS::ExposeObjectToActiveJS(mLinearAcceleration);
  }
  aRetval.set(mLinearAcceleration);
}

void
VRPose::GetOrientation(JSContext* aCx,
                       JS::MutableHandle<JSObject*> aRetval,
                       ErrorResult& aRv)
{
  if (!mOrientation && mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_Orientation) {
    // Lazily create the Float32Array
    mOrientation = dom::Float32Array::Create(aCx, this, 4, mVRState.orientation);
    if (!mOrientation) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  if (mOrientation) {
    JS::ExposeObjectToActiveJS(mOrientation);
  }
  aRetval.set(mOrientation);
}

void
VRPose::GetAngularVelocity(JSContext* aCx,
                           JS::MutableHandle<JSObject*> aRetval,
                           ErrorResult& aRv)
{
  if (!mAngularVelocity && mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_Orientation) {
    // Lazily create the Float32Array
    mAngularVelocity = dom::Float32Array::Create(aCx, this, 3, mVRState.angularVelocity);
    if (!mAngularVelocity) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  if (mAngularVelocity) {
    JS::ExposeObjectToActiveJS(mAngularVelocity);
  }
  aRetval.set(mAngularVelocity);
}

void
VRPose::GetAngularAcceleration(JSContext* aCx,
                               JS::MutableHandle<JSObject*> aRetval,
                               ErrorResult& aRv)
{
  if (!mAngularAcceleration && mVRState.flags & gfx::VRDisplayCapabilityFlags::Cap_AngularAcceleration) {
    // Lazily create the Float32Array
    mAngularAcceleration = dom::Float32Array::Create(aCx, this, 3, mVRState.angularAcceleration);
    if (!mAngularAcceleration) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  if (mAngularAcceleration) {
    JS::ExposeObjectToActiveJS(mAngularAcceleration);
  }
  aRetval.set(mAngularAcceleration);
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

already_AddRefed<VRPose>
VRDisplay::GetPose()
{
  gfx::VRHMDSensorState state = mClient->GetSensorState();
  RefPtr<VRPose> obj = new VRPose(GetParentObject(), state);

  return obj.forget();
}

already_AddRefed<VRPose>
VRDisplay::GetImmediatePose()
{
  gfx::VRHMDSensorState state = mClient->GetImmediateSensorState();
  RefPtr<VRPose> obj = new VRPose(GetParentObject(), state);

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

  if (IsPresenting()) {
    // Only one presentation allowed per VRDisplay
    // on a first-come-first-serve basis.
    promise->MaybeRejectWithUndefined();
  } else {
    mPresentation = mClient->BeginPresentation(aLayers);

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
  ExitPresentInternal();

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  promise->MaybeResolve(JS::UndefinedHandleValue);
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
VRDisplay::SubmitFrame(const Optional<NonNull<VRPose>>& aPose)
{
  if (mPresentation) {
    if (aPose.WasPassed()) {
      mPresentation->SubmitFrame(aPose.Value().FrameID());
    } else {
      mPresentation->SubmitFrame(0);
    }
  }
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
  return mClient->GetIsPresenting();
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

} // namespace dom
} // namespace mozilla
