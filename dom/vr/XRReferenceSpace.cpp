/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRReferenceSpace.h"
#include "mozilla/dom/XRRigidTransform.h"
#include "VRDisplayClient.h"

namespace mozilla {
namespace dom {

XRReferenceSpace::XRReferenceSpace(nsIGlobalObject* aParent,
                                   XRSession* aSession,
                                   XRNativeOrigin* aNativeOrigin,
                                   XRReferenceSpaceType aType)
    : XRSpace(aParent, aSession, aNativeOrigin), mType(aType) {}

already_AddRefed<XRReferenceSpace> XRReferenceSpace::GetOffsetReferenceSpace(
    const XRRigidTransform& aOriginOffset) {
  RefPtr<XRReferenceSpace> offsetReferenceSpace =
      new XRReferenceSpace(GetParentObject(), mSession, mNativeOrigin, mType);

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

JSObject* XRReferenceSpace::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return XRReferenceSpace_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
