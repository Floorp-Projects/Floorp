/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRRigidTransform.h"
#include "mozilla/dom/DOMPoint.h"
#include "mozilla/dom/DOMPointBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(XRRigidTransform)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(XRRigidTransform)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent, mPosition, mOrientation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mMatrixArray = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(XRRigidTransform)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent, mPosition, mOrientation)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(XRRigidTransform)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mMatrixArray)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(XRRigidTransform, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(XRRigidTransform, Release)

XRRigidTransform::XRRigidTransform(nsISupports* aParent,
                                   const gfx::PointDouble3D& aPosition,
                                   const gfx::QuaternionDouble& aOrientation)
    : mParent(aParent), mMatrixArray(nullptr) {
  mozilla::HoldJSObjects(this);
  mPosition =
      new DOMPoint(aParent, aPosition.x, aPosition.y, aPosition.z, 1.0f);
  mOrientation = new DOMPoint(aParent, aOrientation.x, aOrientation.y,
                              aOrientation.z, aOrientation.w);
}

XRRigidTransform::~XRRigidTransform() { mozilla::DropJSObjects(this); }

/* static */ already_AddRefed<XRRigidTransform> XRRigidTransform::Constructor(
    const GlobalObject& aGlobal, const DOMPointInit& aOrigin,
    const DOMPointInit& aDirection, ErrorResult& aRv) {
  gfx::PointDouble3D position(aOrigin.mX, aOrigin.mY, aOrigin.mZ);
  gfx::QuaternionDouble orientation(aDirection.mX, aDirection.mY, aDirection.mZ,
                                    aDirection.mW);
  orientation.Normalize();
  RefPtr<XRRigidTransform> obj =
      new XRRigidTransform(aGlobal.GetAsSupports(), position, orientation);
  return obj.forget();
}

JSObject* XRRigidTransform::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return XRRigidTransform_Binding::Wrap(aCx, this, aGivenProto);
}

DOMPoint* XRRigidTransform::Position() { return mPosition; }

DOMPoint* XRRigidTransform::Orientation() { return mOrientation; }

XRRigidTransform& XRRigidTransform::operator=(const XRRigidTransform& aOther) {
  mPosition->SetX(aOther.mPosition->X());
  mPosition->SetY(aOther.mPosition->Y());
  mPosition->SetZ(aOther.mPosition->Z());
  mOrientation->SetX(aOther.mOrientation->X());
  mOrientation->SetY(aOther.mOrientation->Y());
  mOrientation->SetZ(aOther.mOrientation->Z());
  mOrientation->SetW(aOther.mOrientation->W());
  return *this;
}

gfx::QuaternionDouble XRRigidTransform::RawOrientation() const {
  return gfx::QuaternionDouble(mOrientation->X(), mOrientation->Y(),
                               mOrientation->Z(), mOrientation->W());
}
gfx::PointDouble3D XRRigidTransform::RawPosition() const {
  return gfx::PointDouble3D(mPosition->X(), mPosition->Y(), mPosition->Z());
}

void XRRigidTransform::GetMatrix(JSContext* aCx,
                                 JS::MutableHandle<JSObject*> aRetval,
                                 ErrorResult& aRv) {
  if (!mMatrixArray) {
    gfx::Matrix4x4 mat;
    mat.SetRotationFromQuaternion(
        gfx::Quaternion(mOrientation->X(), mOrientation->Y(), mOrientation->Z(),
                        mOrientation->W()));
    mat.PreTranslate((float)mPosition->X(), (float)mPosition->Y(),
                     (float)mPosition->Z());
    // Lazily create the Float32Array
    mMatrixArray = dom::Float32Array::Create(aCx, this, 16, mat.components);
    if (!mMatrixArray) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }
  if (mMatrixArray) {
    JS::ExposeObjectToActiveJS(mMatrixArray);
  }
  aRetval.set(mMatrixArray);
}

already_AddRefed<XRRigidTransform> XRRigidTransform::Inverse() {
  gfx::QuaternionDouble q(mOrientation->X(), mOrientation->Y(),
                          mOrientation->Z(), mOrientation->W());
  gfx::PointDouble3D p(-mPosition->X(), -mPosition->Y(), -mPosition->Z());
  p = q.RotatePoint(p);
  q.Invert();
  RefPtr<XRRigidTransform> inv = new XRRigidTransform(mParent, p, q);

  return inv.forget();
}

}  // namespace dom
}  // namespace mozilla
