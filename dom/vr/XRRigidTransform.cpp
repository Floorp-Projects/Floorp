/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRRigidTransform.h"
#include "mozilla/dom/DOMPoint.h"
#include "mozilla/dom/Pose.h"
#include "mozilla/dom/DOMPointBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(XRRigidTransform)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(XRRigidTransform)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent, mPosition, mOrientation, mInverse)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mMatrixArray = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(XRRigidTransform)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent, mPosition, mOrientation, mInverse)
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
    : mParent(aParent),
      mMatrixArray(nullptr),
      mPosition(nullptr),
      mOrientation(nullptr),
      mInverse(nullptr),
      mRawPosition(aPosition),
      mRawOrientation(aOrientation),
      mNeedsUpdate(true) {
  mozilla::HoldJSObjects(this);
  mRawTransformMatrix.SetRotationFromQuaternion(mRawOrientation);
  mRawTransformMatrix.PostTranslate(mRawPosition);
}

XRRigidTransform::XRRigidTransform(nsISupports* aParent,
                                   const gfx::Matrix4x4Double& aTransform)
    : mParent(aParent),
      mMatrixArray(nullptr),
      mPosition(nullptr),
      mOrientation(nullptr),
      mInverse(nullptr),
      mNeedsUpdate(true) {
  mozilla::HoldJSObjects(this);
  gfx::PointDouble3D scale;
  mRawTransformMatrix = aTransform;
  mRawTransformMatrix.Decompose(mRawPosition, mRawOrientation, scale);
  // TODO: Investigate why we need to do this invert after getting orientation
  // from the transform matrix. It looks like we have a bug at
  // Matrix4x4Typed.SetFromRotationMatrix() (Bug 1635363).
  mRawOrientation.Invert();
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

DOMPoint* XRRigidTransform::Position() {
  if (!mPosition) {
    mPosition = new DOMPoint(mParent, mRawPosition.x, mRawPosition.y,
                             mRawPosition.z, 1.0f);
  }
  return mPosition;
}

DOMPoint* XRRigidTransform::Orientation() {
  if (!mOrientation) {
    mOrientation = new DOMPoint(mParent, mRawOrientation.x, mRawOrientation.y,
                                mRawOrientation.z, mRawOrientation.w);
  }
  return mOrientation;
}

XRRigidTransform& XRRigidTransform::operator=(const XRRigidTransform& aOther) {
  Update(aOther.mRawPosition, aOther.mRawOrientation);
  return *this;
}

gfx::QuaternionDouble XRRigidTransform::RawOrientation() const {
  return mRawOrientation;
}
gfx::PointDouble3D XRRigidTransform::RawPosition() const {
  return mRawPosition;
}

void XRRigidTransform::Update(const gfx::PointDouble3D& aPosition,
                              const gfx::QuaternionDouble& aOrientation) {
  mNeedsUpdate = true;
  mRawPosition = aPosition;
  mRawOrientation = aOrientation;
  mRawTransformMatrix.SetRotationFromQuaternion(mRawOrientation);
  mRawTransformMatrix.PostTranslate(mRawPosition);

  if (mPosition) {
    mPosition->SetX(aPosition.x);
    mPosition->SetY(aPosition.y);
    mPosition->SetZ(aPosition.z);
  }
  if (mOrientation) {
    mOrientation->SetX(aOrientation.x);
    mOrientation->SetY(aOrientation.y);
    mOrientation->SetZ(aOrientation.z);
    mOrientation->SetW(aOrientation.w);
  }
  if (mInverse) {
    gfx::QuaternionDouble q(mRawOrientation);
    gfx::PointDouble3D p = -mRawPosition;
    p = q.RotatePoint(p);
    q.Invert();
    mInverse->Update(p, q);
  }
}

void XRRigidTransform::GetMatrix(JSContext* aCx,
                                 JS::MutableHandle<JSObject*> aRetval,
                                 ErrorResult& aRv) {
  if (!mMatrixArray || mNeedsUpdate) {
    mNeedsUpdate = false;

    const uint32_t size = 16;
    float components[size] = {};
    // In order to avoid some platforms which only copy
    // the first or last two bytes of a Float64 to a Float32.
    for (uint32_t i = 0; i < size; ++i) {
      components[i] = mRawTransformMatrix.components[i];
    }
    Pose::SetFloat32Array(aCx, this, aRetval, mMatrixArray, components, 16,
                          aRv);
    if (!mMatrixArray) {
      return;
    }
  }
  if (mMatrixArray) {
    JS::ExposeObjectToActiveJS(mMatrixArray);
  }
  aRetval.set(mMatrixArray);
}

already_AddRefed<XRRigidTransform> XRRigidTransform::Inverse() {
  if (!mInverse) {
    gfx::QuaternionDouble q(mRawOrientation);
    gfx::PointDouble3D p = -mRawPosition;
    p = q.RotatePoint(p);
    q.Invert();
    mInverse = new XRRigidTransform(mParent, p, q);
  }

  RefPtr<XRRigidTransform> inverse = mInverse;
  return inverse.forget();
}

}  // namespace dom
}  // namespace mozilla
