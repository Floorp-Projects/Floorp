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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mJSProjectionMatrix = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(XRView)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(XRView)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mJSProjectionMatrix)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(XRView, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(XRView, Release)

XRView::XRView(nsISupports* aParent, const XREye& aEye,
               const gfx::Point3D& aPosition,
               const gfx::Matrix4x4& aProjectionMatrix)
    : mParent(aParent),
      mEye(aEye),
      mPosition(aPosition),
      mProjectionMatrix(aProjectionMatrix),
      mJSProjectionMatrix(nullptr) {
  mozilla::HoldJSObjects(this);
}

XRView::~XRView() { mozilla::DropJSObjects(this); }

JSObject* XRView::WrapObject(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) {
  return XRView_Binding::Wrap(aCx, this, aGivenProto);
}

XREye XRView::Eye() const { return mEye; }

void XRView::GetProjectionMatrix(JSContext* aCx,
                                 JS::MutableHandle<JSObject*> aRetval,
                                 ErrorResult& aRv) {
  LazyCreateMatrix(mJSProjectionMatrix, mProjectionMatrix, aCx, aRetval, aRv);
}

already_AddRefed<XRRigidTransform> XRView::GetTransform(ErrorResult& aRv) {
  // TODO(kearwood) - Implement eye orientation component of rigid transform
  RefPtr<XRRigidTransform> transform = new XRRigidTransform(
      mParent, gfx::PointDouble3D(mPosition.x, mPosition.y, mPosition.z),
      gfx::QuaternionDouble(0.0f, 0.0f, 0.0f, 1.0f));
  return transform.forget();
  ;
}

void XRView::LazyCreateMatrix(JS::Heap<JSObject*>& aArray, gfx::Matrix4x4& aMat,
                              JSContext* aCx,
                              JS::MutableHandle<JSObject*> aRetval,
                              ErrorResult& aRv) {
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

}  // namespace dom
}  // namespace mozilla
