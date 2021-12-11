/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRSession.h"

#include "mozilla/dom/XRSessionEvent.h"
#include "mozilla/dom/XRInputSourceEvent.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Promise.h"
#include "XRSystem.h"
#include "XRRenderState.h"
#include "XRBoundedReferenceSpace.h"
#include "XRFrame.h"
#include "XRNativeOrigin.h"
#include "XRNativeOriginFixed.h"
#include "XRNativeOriginViewer.h"
#include "XRNativeOriginLocal.h"
#include "XRNativeOriginLocalFloor.h"
#include "XRView.h"
#include "XRViewerPose.h"
#include "VRLayerChild.h"
#include "XRInputSourceArray.h"
#include "nsGlobalWindow.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsRefreshDriver.h"
#include "VRDisplayClient.h"
#include "VRDisplayPresentation.h"

/**
 * Maximum instances of XRFrame and XRViewerPose objects
 * created in the pool.
 */
const uint32_t kMaxPoolSize = 16;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(XRSession)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XRSession,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mXRSystem)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mActiveRenderState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingRenderState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputSources)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mViewerPosePool)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFramePool)

  for (uint32_t i = 0; i < tmp->mFrameRequestCallbacks.Length(); ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mFrameRequestCallbacks[i]");
    cb.NoteXPCOMChild(tmp->mFrameRequestCallbacks[i].mCallback);
  }

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XRSession, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mXRSystem)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mActiveRenderState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingRenderState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInputSources)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mViewerPosePool)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFramePool)

  tmp->mFrameRequestCallbacks.Clear();

  // Don't need NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER because
  // DOMEventTargetHelper does it for us.
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(XRSession, DOMEventTargetHelper)

already_AddRefed<XRSession> XRSession::CreateInlineSession(
    nsPIDOMWindowInner* aWindow, XRSystem* aXRSystem,
    const nsTArray<XRReferenceSpaceType>& aEnabledReferenceSpaceTypes) {
  nsGlobalWindowInner* win = nsGlobalWindowInner::Cast(aWindow);
  if (!win) {
    return nullptr;
  }
  Document* doc = aWindow->GetExtantDoc();
  if (!doc) {
    return nullptr;
  }
  nsPresContext* context = doc->GetPresContext();
  if (!context) {
    return nullptr;
  }
  nsRefreshDriver* driver = context->RefreshDriver();
  if (!driver) {
    return nullptr;
  }

  RefPtr<XRSession> session =
      new XRSession(aWindow, aXRSystem, driver, nullptr, gfx::kVRGroupContent,
                    aEnabledReferenceSpaceTypes);
  driver->AddRefreshObserver(session, FlushType::Display, "XR Session");
  return session.forget();
}

already_AddRefed<XRSession> XRSession::CreateImmersiveSession(
    nsPIDOMWindowInner* aWindow, XRSystem* aXRSystem,
    gfx::VRDisplayClient* aClient, uint32_t aPresentationGroup,
    const nsTArray<XRReferenceSpaceType>& aEnabledReferenceSpaceTypes) {
  RefPtr<XRSession> session =
      new XRSession(aWindow, aXRSystem, nullptr, aClient, aPresentationGroup,
                    aEnabledReferenceSpaceTypes);
  return session.forget();
}

XRSession::XRSession(
    nsPIDOMWindowInner* aWindow, XRSystem* aXRSystem,
    nsRefreshDriver* aRefreshDriver, gfx::VRDisplayClient* aClient,
    uint32_t aPresentationGroup,
    const nsTArray<XRReferenceSpaceType>& aEnabledReferenceSpaceTypes)
    : DOMEventTargetHelper(aWindow),
      mXRSystem(aXRSystem),
      mShutdown(false),
      mEnded(false),
      mRefreshDriver(aRefreshDriver),
      mDisplayClient(aClient),
      mFrameRequestCallbackCounter(0),
      mEnabledReferenceSpaceTypes(aEnabledReferenceSpaceTypes.Clone()),
      mViewerPosePoolIndex(0),
      mFramePoolIndex(0) {
  if (aClient) {
    aClient->SessionStarted(this);
  }
  mActiveRenderState = new XRRenderState(aWindow, this);
  mStartTimeStamp = TimeStamp::Now();
  if (IsImmersive()) {
    mDisplayPresentation =
        mDisplayClient->BeginPresentation({}, aPresentationGroup);
  }
  if (mDisplayClient) {
    mDisplayClient->SetXRAPIMode(gfx::VRAPIMode::WebXR);
  }
  // TODO: Handle XR input sources are no longer available cases.
  // https://immersive-web.github.io/webxr/#dom-xrsession-inputsources
  mInputSources = new XRInputSourceArray(aWindow);
}

XRSession::~XRSession() { MOZ_ASSERT(mShutdown); }

gfx::VRDisplayClient* XRSession::GetDisplayClient() const {
  return mDisplayClient;
}

JSObject* XRSession::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return XRSession_Binding::Wrap(aCx, this, aGivenProto);
}

bool XRSession::IsEnded() const { return mEnded; }

already_AddRefed<Promise> XRSession::End(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = GetParentObject();
  NS_ENSURE_TRUE(global, nullptr);

  ExitPresentInternal();

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  promise->MaybeResolve(JS::UndefinedHandleValue);

  return promise.forget();
}

bool XRSession::IsImmersive() const {
  // Only immersive sessions have a VRDisplayClient
  return mDisplayClient != nullptr;
}

XRVisibilityState XRSession::VisibilityState() const {
  return XRVisibilityState::Visible;
  // TODO (Bug 1609771): Implement changing visibility state
}

// https://immersive-web.github.io/webxr/#poses-may-be-reported
// Given that an XRSession cannot be requested without explicit consent
// by the user, the only necessary check is whether the XRSession's
// visiblityState is 'visible'.
bool XRSession::CanReportPoses() const {
  return VisibilityState() == XRVisibilityState::Visible;
}

// https://immersive-web.github.io/webxr/#dom-xrsession-updaterenderstate
void XRSession::UpdateRenderState(const XRRenderStateInit& aNewState,
                                  ErrorResult& aRv) {
  if (mEnded) {
    aRv.ThrowInvalidStateError(
        "UpdateRenderState can not be called on an XRSession that has ended.");
    return;
  }
  if (aNewState.mBaseLayer.WasPassed() &&
      aNewState.mBaseLayer.Value()->mSession != this) {
    aRv.ThrowInvalidStateError(
        "The baseLayer passed in to UpdateRenderState must "
        "belong to the XRSession that UpdateRenderState is "
        "being called on.");
    return;
  }
  if (aNewState.mInlineVerticalFieldOfView.WasPassed() && IsImmersive()) {
    aRv.ThrowInvalidStateError(
        "The inlineVerticalFieldOfView can not be set on an "
        "XRRenderState for an immersive XRSession.");
    return;
  }
  if (mPendingRenderState == nullptr) {
    mPendingRenderState = new XRRenderState(*mActiveRenderState);
  }
  if (aNewState.mDepthNear.WasPassed()) {
    mPendingRenderState->SetDepthNear(aNewState.mDepthNear.Value());
  }
  if (aNewState.mDepthFar.WasPassed()) {
    mPendingRenderState->SetDepthFar(aNewState.mDepthFar.Value());
  }
  if (aNewState.mInlineVerticalFieldOfView.WasPassed()) {
    mPendingRenderState->SetInlineVerticalFieldOfView(
        aNewState.mInlineVerticalFieldOfView.Value());
  }
  if (aNewState.mBaseLayer.WasPassed()) {
    mPendingRenderState->SetBaseLayer(aNewState.mBaseLayer.Value());
  }
}

XRRenderState* XRSession::RenderState() { return mActiveRenderState; }

XRInputSourceArray* XRSession::InputSources() { return mInputSources; }

// https://immersive-web.github.io/webxr/#apply-the-pending-render-state
void XRSession::ApplyPendingRenderState() {
  if (mPendingRenderState == nullptr) {
    return;
  }
  mActiveRenderState = mPendingRenderState;
  mPendingRenderState = nullptr;

  // https://immersive-web.github.io/webxr/#minimum-inline-field-of-view
  const double kMinimumInlineVerticalFieldOfView = 0.0f;

  // https://immersive-web.github.io/webxr/#maximum-inline-field-of-view
  const double kMaximumInlineVerticalFieldOfView = M_PI;

  if (!mActiveRenderState->GetInlineVerticalFieldOfView().IsNull()) {
    double verticalFOV =
        mActiveRenderState->GetInlineVerticalFieldOfView().Value();
    if (verticalFOV < kMinimumInlineVerticalFieldOfView) {
      verticalFOV = kMinimumInlineVerticalFieldOfView;
    }
    if (verticalFOV > kMaximumInlineVerticalFieldOfView) {
      verticalFOV = kMaximumInlineVerticalFieldOfView;
    }
    mActiveRenderState->SetInlineVerticalFieldOfView(verticalFOV);
  }

  // Our minimum near plane value is set to a small value close but not equal to
  // zero (kEpsilon) The maximum far plane is infinite.
  const float kEpsilon = 0.00001f;
  double depthNear = mActiveRenderState->DepthNear();
  double depthFar = mActiveRenderState->DepthFar();
  if (depthNear < 0.0f) {
    depthNear = 0.0f;
  }
  if (depthFar < 0.0f) {
    depthFar = 0.0f;
  }
  // Ensure at least a small distance between the near and far planes
  if (fabs(depthFar - depthNear) < kEpsilon) {
    depthFar = depthNear + kEpsilon;
  }
  mActiveRenderState->SetDepthNear(depthNear);
  mActiveRenderState->SetDepthFar(depthFar);

  XRWebGLLayer* baseLayer = mActiveRenderState->GetBaseLayer();
  if (baseLayer) {
    if (!IsImmersive() && baseLayer->mCompositionDisabled) {
      mActiveRenderState->SetCompositionDisabled(true);
      mActiveRenderState->SetOutputCanvas(baseLayer->GetCanvas());
    } else {
      mActiveRenderState->SetCompositionDisabled(false);
      mActiveRenderState->SetOutputCanvas(nullptr);
      mDisplayPresentation->UpdateXRWebGLLayer(baseLayer);
    }
  }  // if (baseLayer)
}

void XRSession::WillRefresh(mozilla::TimeStamp aTime) {
  // Inline sessions are driven by nsRefreshDriver directly,
  // unlike immersive sessions, which are driven VRDisplayClient.
  if (!IsImmersive() && !mXRSystem->HasActiveImmersiveSession()) {
    nsGlobalWindowInner* win = nsGlobalWindowInner::Cast(GetOwner());
    if (win) {
      if (JSObject* obj = win->AsGlobal()->GetGlobalJSObject()) {
        js::NotifyAnimationActivity(obj);
      }
    }
    StartFrame();
  }
}

void XRSession::StartFrame() {
  if (mShutdown || mEnded) {
    return;
  }
  ApplyPendingRenderState();

  XRWebGLLayer* baseLayer = mActiveRenderState->GetBaseLayer();
  if (!baseLayer) {
    return;
  }

  if (!IsImmersive() && mActiveRenderState->GetOutputCanvas() == nullptr) {
    return;
  }

  // Determine timestamp for the callbacks
  TimeStamp nowTime = TimeStamp::Now();
  mozilla::TimeDuration duration = nowTime - mStartTimeStamp;
  DOMHighResTimeStamp timeStamp = duration.ToMilliseconds();

  // Create an XRFrame for the callbacks
  RefPtr<XRFrame> frame = PooledFrame();
  frame->StartAnimationFrame();

  baseLayer->StartAnimationFrame();
  nsTArray<XRFrameRequest> callbacks;
  callbacks.AppendElements(mFrameRequestCallbacks);
  mFrameRequestCallbacks.Clear();
  for (auto& callback : callbacks) {
    callback.Call(timeStamp, *frame);
  }

  baseLayer->EndAnimationFrame();
  frame->EndAnimationFrame();
  if (mDisplayPresentation) {
    mDisplayPresentation->SubmitFrame();
  }
}

void XRSession::ExitPresent() { ExitPresentInternal(); }

already_AddRefed<Promise> XRSession::RequestReferenceSpace(
    const XRReferenceSpaceType& aReferenceSpaceType, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = GetParentObject();
  NS_ENSURE_TRUE(global, nullptr);

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  if (!mEnabledReferenceSpaceTypes.Contains(aReferenceSpaceType)) {
    promise->MaybeRejectWithNotSupportedError(nsLiteralCString(
        "Requested XRReferenceSpaceType not available for the XRSession."));
    return promise.forget();
  }
  RefPtr<XRReferenceSpace> space;
  RefPtr<XRNativeOrigin> nativeOrigin;
  if (mDisplayClient) {
    switch (aReferenceSpaceType) {
      case XRReferenceSpaceType::Viewer:
        nativeOrigin = new XRNativeOriginViewer(mDisplayClient);
        break;
      case XRReferenceSpaceType::Local:
        nativeOrigin = new XRNativeOriginLocal(mDisplayClient);
        break;
      case XRReferenceSpaceType::Local_floor:
      case XRReferenceSpaceType::Bounded_floor:
        nativeOrigin = new XRNativeOriginLocalFloor(mDisplayClient);
        break;
      default:
        nativeOrigin = new XRNativeOriginFixed(gfx::PointDouble3D());
        break;
    }
  } else {
    // We currently only support XRReferenceSpaceType::Viewer when
    // there is no XR hardware.  In this case, the native origin
    // will always be at {0, 0, 0} which will always be the same
    // as the 'tracked' position of the non-existant pose.
    MOZ_ASSERT(aReferenceSpaceType == XRReferenceSpaceType::Viewer);
    nativeOrigin = new XRNativeOriginFixed(gfx::PointDouble3D());
  }
  if (aReferenceSpaceType == XRReferenceSpaceType::Bounded_floor) {
    space = new XRBoundedReferenceSpace(GetParentObject(), this, nativeOrigin);
  } else {
    space = new XRReferenceSpace(GetParentObject(), this, nativeOrigin,
                                 aReferenceSpaceType);
  }

  promise->MaybeResolve(space);
  return promise.forget();
}

XRRenderState* XRSession::GetActiveRenderState() const {
  return mActiveRenderState;
}

void XRSession::XRFrameRequest::Call(const DOMHighResTimeStamp& aTimeStamp,
                                     XRFrame& aFrame) {
  RefPtr<mozilla::dom::XRFrameRequestCallback> callback = mCallback;
  callback->Call(aTimeStamp, aFrame);
}

int32_t XRSession::RequestAnimationFrame(XRFrameRequestCallback& aCallback,
                                         ErrorResult& aError) {
  if (mShutdown) {
    return 0;
  }

  int32_t handle = ++mFrameRequestCallbackCounter;

  mFrameRequestCallbacks.AppendElement(XRFrameRequest(aCallback, handle));

  return handle;
}

void XRSession::CancelAnimationFrame(int32_t aHandle, ErrorResult& aError) {
  mFrameRequestCallbacks.RemoveElementSorted(aHandle);
}

void XRSession::Shutdown() {
  mShutdown = true;
  ExitPresentInternal();
  mViewerPosePool.Clear();
  mViewerPosePoolIndex = 0;
  mFramePool.Clear();
  mFramePoolIndex = 0;
  mActiveRenderState = nullptr;
  mPendingRenderState = nullptr;
  mFrameRequestCallbacks.Clear();

  // Unregister from nsRefreshObserver
  if (mRefreshDriver) {
    mRefreshDriver->RemoveRefreshObserver(this, FlushType::Display);
    mRefreshDriver = nullptr;
  }
}

void XRSession::ExitPresentInternal() {
  if (mInputSources) {
    mInputSources->Clear(this);
  }
  if (mDisplayClient) {
    mDisplayClient->SessionEnded(this);
  }

  if (mXRSystem) {
    mXRSystem->SessionEnded(this);
  }

  if (mActiveRenderState) {
    mActiveRenderState->SessionEnded();
  }

  if (mPendingRenderState) {
    mPendingRenderState->SessionEnded();
  }

  mDisplayPresentation = nullptr;
  if (!mEnded) {
    mEnded = true;

    XRSessionEventInit init;
    init.mBubbles = false;
    init.mCancelable = false;
    init.mSession = this;
    RefPtr<XRSessionEvent> event =
        XRSessionEvent::Constructor(this, u"end"_ns, init);

    event->SetTrusted(true);
    this->DispatchEvent(*event);
  }
}

void XRSession::DisconnectFromOwner() {
  MOZ_ASSERT(NS_IsMainThread());
  Shutdown();
  DOMEventTargetHelper::DisconnectFromOwner();
}

void XRSession::LastRelease() {
  // We don't want to wait for the GC to free up the presentation
  // for use in other documents, so we do this in LastRelease().
  Shutdown();
}

RefPtr<XRViewerPose> XRSession::PooledViewerPose(
    const gfx::Matrix4x4Double& aTransform, bool aEmulatedPosition) {
  RefPtr<XRViewerPose> pose;
  if (mViewerPosePool.Length() > mViewerPosePoolIndex) {
    pose = mViewerPosePool.ElementAt(mViewerPosePoolIndex);
    pose->Transform()->Update(aTransform);
    pose->SetEmulatedPosition(aEmulatedPosition);
  } else {
    RefPtr<XRRigidTransform> transform = new XRRigidTransform(this, aTransform);
    nsTArray<RefPtr<XRView>> views;
    if (IsImmersive()) {
      views.AppendElement(new XRView(GetParentObject(), XREye::Left));
      views.AppendElement(new XRView(GetParentObject(), XREye::Right));
    } else {
      views.AppendElement(new XRView(GetParentObject(), XREye::None));
    }
    pose = new XRViewerPose(this, transform, aEmulatedPosition, views);
    mViewerPosePool.AppendElement(pose);
  }

  mViewerPosePoolIndex++;
  if (mViewerPosePoolIndex >= kMaxPoolSize) {
    mViewerPosePoolIndex = 0;
  }

  return pose;
}

RefPtr<XRFrame> XRSession::PooledFrame() {
  RefPtr<XRFrame> frame;
  if (mFramePool.Length() > mFramePoolIndex) {
    frame = mFramePool.ElementAt(mFramePoolIndex);
  } else {
    frame = new XRFrame(GetParentObject(), this);
    mFramePool.AppendElement(frame);
  }

  return frame;
}

}  // namespace dom
}  // namespace mozilla
