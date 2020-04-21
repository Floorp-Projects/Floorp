/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRFrame.h"
#include "mozilla/dom/XRRenderState.h"
#include "mozilla/dom/XRViewerPose.h"
#include "mozilla/dom/XRView.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(XRFrame, mParent, mSession)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(XRFrame, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(XRFrame, Release)

XRFrame::XRFrame(nsISupports* aParent, XRSession* aXRSession)
    : mParent(aParent),
      mSession(aXRSession),
      mActive(false),
      mAnimationFrame(false) {}

JSObject* XRFrame::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto) {
  return XRFrame_Binding::Wrap(aCx, this, aGivenProto);
}

XRSession* XRFrame::Session() { return mSession; }

already_AddRefed<XRViewerPose> XRFrame::GetViewerPose(
    const XRReferenceSpace& aReferenceSpace, ErrorResult& aRv) {
  if (!mActive || !mAnimationFrame) {
    aRv.ThrowInvalidStateError(
        "GetViewerPose can only be called on an XRFrame during an "
        "XRSession.requestAnimationFrame callback.");
    return nullptr;
  }

  if (aReferenceSpace.GetSession() != mSession) {
    aRv.ThrowInvalidStateError(
        "The XRReferenceSpace passed to GetViewerPose must belong to the "
        "XRSession that GetViewerPose is called on.");
    return nullptr;
  }

  // TODO (Bug 1616390) - Validate that poses may be reported:
  // https://immersive-web.github.io/webxr/#poses-may-be-reported

  // TODO (Bug 1616393) - Check if poses must be limited:
  // https://immersive-web.github.io/webxr/#poses-must-be-limited

  gfx::PointDouble3D viewerPosition;
  gfx::QuaternionDouble viewerOrientation;
  bool emulatedPosition = aReferenceSpace.IsPositionEmulated();
  nsTArray<RefPtr<XRView>> views;

  XRRenderState* renderState = mSession->GetActiveRenderState();
  float depthNear = (float)renderState->DepthNear();
  float depthFar = (float)renderState->DepthFar();

  gfx::VRDisplayClient* display = mSession->GetDisplayClient();
  if (display) {
    // Have a VRDisplayClient
    const VRDisplayInfo& displayInfo =
        mSession->GetDisplayClient()->GetDisplayInfo();
    const gfx::VRHMDSensorState& sensorState = display->GetSensorState();

    viewerPosition = gfx::PointDouble3D(sensorState.pose.position[0],
                                        sensorState.pose.position[1],
                                        sensorState.pose.position[2]);
    viewerOrientation = gfx::QuaternionDouble(
        sensorState.pose.orientation[0], sensorState.pose.orientation[1],
        sensorState.pose.orientation[2], sensorState.pose.orientation[3]);

    // Quaternion was inverted for WebVR. We need to invert it here again.
    // TODO: Remove those extra inverts when WebVR support is disabled.
    viewerOrientation.Invert();

    gfx::Matrix4x4Double headTransform;
    headTransform.SetRotationFromQuaternion(viewerOrientation);
    headTransform.PostTranslate(viewerPosition);

    gfx::Matrix4x4Double originTransform;
    originTransform.SetRotationFromQuaternion(
        aReferenceSpace.GetEffectiveOriginOrientation());
    originTransform.PreTranslate(-aReferenceSpace.GetEffectiveOriginPosition());

    headTransform *= originTransform;

    auto addEye = [&](XREye xrEye, VRDisplayState::Eye eye) {
      auto offset = displayInfo.GetEyeTranslation(eye);
      auto eyeFromHead = gfx::Matrix4x4Double::Translation(
          gfx::PointDouble3D(offset.x, offset.y, offset.z));
      auto eyeTransform = eyeFromHead * headTransform;
      gfx::PointDouble3D eyePosition;
      gfx::QuaternionDouble eyeRotation;
      gfx::PointDouble3D eyeScale;
      eyeTransform.Decompose(eyePosition, eyeRotation, eyeScale);

      const gfx::VRFieldOfView fov = displayInfo.mDisplayState.eyeFOV[eye];
      Matrix4x4 projection =
          fov.ConstructProjectionMatrix(depthNear, depthFar, true);
      RefPtr<XRView> view =
          new XRView(mParent, xrEye, eyePosition, eyeRotation, projection);
      views.AppendElement(view);
    };

    addEye(XREye::Left, gfx::VRDisplayState::Eye_Left);
    addEye(XREye::Right, gfx::VRDisplayState::Eye_Right);
  } else {
    auto inlineVerticalFov = renderState->GetInlineVerticalFieldOfView();
    const double fov =
        inlineVerticalFov.IsNull() ? M_PI * 0.5f : inlineVerticalFov.Value();
    HTMLCanvasElement* canvas = renderState->GetOutputCanvas();
    float aspect = 1.0f;
    if (canvas) {
      aspect = (float)canvas->Width() / (float)canvas->Height();
    }
    Matrix4x4 projection =
        ConstructInlineProjection((float)fov, aspect, depthNear, depthFar);
    RefPtr<XRView> view = new XRView(mParent, XREye::None, gfx::PointDouble3D(),
                                     gfx::QuaternionDouble(), projection);
    views.AppendElement(view);
  }

  RefPtr<XRRigidTransform> transform =
      new XRRigidTransform(mParent, viewerPosition, viewerOrientation);

  RefPtr<XRViewerPose> pose =
      new XRViewerPose(mParent, transform, emulatedPosition, views);

  return pose.forget();
}

XRPose* XRFrame::GetPose(const XRSpace& aSpace, const XRSpace& aBaseSpace,
                         ErrorResult& aRv) {
  if (!mActive) {
    aRv.ThrowInvalidStateError(
        "GetPose can not be called on an XRFrame that is not active.");
    return nullptr;
  }

  if (aSpace.GetSession() != mSession || aBaseSpace.GetSession() != mSession) {
    aRv.ThrowInvalidStateError(
        "The XRSpace passed to GetPose must belong to the "
        "XRSession that GetPose is called on.");
    return nullptr;
  }

  // TODO (Bug 1616390) - Validate that poses may be reported:
  // https://immersive-web.github.io/webxr/#poses-may-be-reported
  if (aSpace.GetSession()->VisibilityState() != XRVisibilityState::Visible) {
    aRv.ThrowInvalidStateError(
        "An XRSpace ’s visibilityState in not 'visible'.");
    return nullptr;
  }

  // TODO (Bug 1616393) - Check if poses must be limited:
  // https://immersive-web.github.io/webxr/#poses-must-be-limited

  const bool emulatedPosition = aSpace.IsPositionEmulated();
  gfx::Matrix4x4Double base;
  base.SetRotationFromQuaternion(aBaseSpace.GetEffectiveOriginOrientation());
  base.PreTranslate(-aBaseSpace.GetEffectiveOriginPosition());

  gfx::Matrix4x4Double matrix = aSpace.GetEffectiveOriginTransform() * base;

  RefPtr<XRRigidTransform> transform = new XRRigidTransform(mParent, matrix);
  RefPtr<XRPose> pose = new XRPose(mParent, transform, emulatedPosition);

  return pose;
}

void XRFrame::StartAnimationFrame() {
  mActive = true;
  mAnimationFrame = true;
}

void XRFrame::EndAnimationFrame() { mActive = false; }

void XRFrame::StartInputSourceEvent() { mActive = true; }

void XRFrame::EndInputSourceEvent() { mActive = false; }

gfx::Matrix4x4 XRFrame::ConstructInlineProjection(float aFov, float aAspect,
                                                  float aNear, float aFar) {
  Matrix4x4 m;
  const float depth = aFar - aNear;
  const float invDepth = 1 / depth;
  if (aFov == 0) {
    aFov = 0.5f * M_PI;
  }

  m._22 = 1.0f / tan(0.5f * aFov);
  m._11 = -m._22 / aAspect;
  m._33 = depth * invDepth;
  m._43 = (-aFar * aNear) * invDepth;
  m._34 = 1.0f;

  return m;
}

}  // namespace dom
}  // namespace mozilla
