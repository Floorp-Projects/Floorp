/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRBoundedReferenceSpace.h"
#include "mozilla/dom/XRRigidTransform.h"
#include "mozilla/dom/DOMPoint.h"

namespace mozilla {
namespace dom {

XRBoundedReferenceSpace::XRBoundedReferenceSpace(nsIGlobalObject* aParent,
                                                 XRSession* aSession,
                                                 XRNativeOrigin* aNativeOrigin)
    : XRReferenceSpace(aParent, aSession, aNativeOrigin,
                       XRReferenceSpaceType::Bounded_floor) {}

JSObject* XRBoundedReferenceSpace::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return XRBoundedReferenceSpace_Binding::Wrap(aCx, this, aGivenProto);
}

void XRBoundedReferenceSpace::GetBoundsGeometry(
    nsTArray<RefPtr<DOMPointReadOnly>>& result) {
  const auto size =
      mSession->GetDisplayClient()->GetDisplayInfo().GetStageSize();
  if (size.width == 0 || size.height == 0) {
    return;
  }

  // https://immersive-web.github.io/webxr/#dom-xrboundedreferencespace-boundsgeometry
  // bounds geometry must be premultiplied by the inverse of the origin offset.
  gfx::PointDouble3D offset = mNativeOrigin->GetPosition();

  const auto addPoint = [&](const double x, const double z) {
    RefPtr<DOMPointReadOnly> obj = new DOMPointReadOnly(
        GetParentObject(), x - offset.x, 0.0f, z - offset.z, 1.0f);
    result.EmplaceBack(obj);
  };

  addPoint(-size.width * 0.5f, size.height * 0.5f);
  addPoint(size.width * 0.5f, size.height * 0.5f);
  addPoint(size.width * 0.5f, -size.height * 0.5f);
  addPoint(-size.width * 0.5f, -size.height * 0.5f);

  // TODO (Bug 1611526): Support WebXR bounded reference spaces
}

already_AddRefed<XRReferenceSpace>
XRBoundedReferenceSpace::GetOffsetReferenceSpace(
    const XRRigidTransform& aOriginOffset) {
  RefPtr<XRBoundedReferenceSpace> offsetReferenceSpace =
      new XRBoundedReferenceSpace(GetParentObject(), mSession, mNativeOrigin);

  // https://immersive-web.github.io/webxr/#multiply-transforms
  // An XRRigidTransform is essentially a rotation followed by a translation
  gfx::QuaternionDouble otherOrientation = aOriginOffset.RawOrientation();
  // The resulting rotation is the two combined
  offsetReferenceSpace->mOriginOffsetOrientation =
      mOriginOffsetOrientation * otherOrientation;
  // We first apply the rotation of aOriginOffset to
  // mOriginOffsetPosition offset, then translate by the offset of
  // aOriginOffset
  offsetReferenceSpace->mOriginOffsetPosition =
      otherOrientation.RotatePoint(mOriginOffsetPosition) +
      aOriginOffset.RawPosition();

  return offsetReferenceSpace.forget();
}

}  // namespace dom
}  // namespace mozilla
