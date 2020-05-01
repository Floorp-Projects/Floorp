/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRView.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(XRView)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(XRView)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent, mTransform)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mJSProjectionMatrix = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(XRView)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent, mTransform)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(XRView)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mJSProjectionMatrix)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(XRView, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(XRView, Release)

XRView::XRView(nsISupports* aParent, const XREye& aEye)
    : mParent(aParent),
      mEye(aEye),
      mPosition(gfx::PointDouble3D()),
      mOrientation(gfx::QuaternionDouble()),
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

}  // namespace dom
}  // namespace mozilla
