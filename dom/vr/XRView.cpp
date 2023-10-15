/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRView.h"

#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/XRRigidTransform.h"
#include "mozilla/dom/Pose.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WITH_JS_MEMBERS(XRView,
                                                      (mParent, mTransform),
                                                      (mJSProjectionMatrix))

XRView::XRView(nsISupports* aParent, const XREye& aEye)
    : mParent(aParent),
      mEye(aEye),

      mJSProjectionMatrix(nullptr) {
  mozilla::HoldJSObjects(this);
}

XRView::~XRView() { mozilla::DropJSObjects(this); }

JSObject* XRView::WrapObject(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) {
  return XRView_Binding::Wrap(aCx, this, aGivenProto);
}

void XRView::Update(const gfx::PointDouble3D& aPosition,
                    const gfx::QuaternionDouble& aOrientation,
                    const gfx::Matrix4x4& aProjectionMatrix) {
  mPosition = aPosition;
  mOrientation = aOrientation;
  mProjectionMatrix = aProjectionMatrix;
  if (mTransform) {
    mTransform->Update(aPosition, aOrientation);
  }
  if (aProjectionMatrix != mProjectionMatrix) {
    mProjectionNeedsUpdate = true;
    mProjectionMatrix = aProjectionMatrix;
  }
}

XREye XRView::Eye() const { return mEye; }

void XRView::GetProjectionMatrix(JSContext* aCx,
                                 JS::MutableHandle<JSObject*> aRetval,
                                 ErrorResult& aRv) {
  if (!mJSProjectionMatrix || mProjectionNeedsUpdate) {
    mProjectionNeedsUpdate = false;
    gfx::Matrix4x4 mat;

    Pose::SetFloat32Array(aCx, this, aRetval, mJSProjectionMatrix,
                          mProjectionMatrix.components, 16, aRv);
    if (!mJSProjectionMatrix) {
      return;
    }
  }
  if (mJSProjectionMatrix) {
    JS::ExposeObjectToActiveJS(mJSProjectionMatrix);
  }
  aRetval.set(mJSProjectionMatrix);
}

already_AddRefed<XRRigidTransform> XRView::GetTransform(ErrorResult& aRv) {
  if (!mTransform) {
    mTransform = new XRRigidTransform(mParent, mPosition, mOrientation);
  }
  RefPtr<XRRigidTransform> transform = mTransform;
  return transform.forget();
}

}  // namespace mozilla::dom
