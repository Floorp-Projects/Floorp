/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRRigidTransform.h"
#include "mozilla/dom/DOMPoint.h"
#include "mozilla/dom/Pose.h"
#include "mozilla/dom/DOMPointBinding.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WITH_JS_MEMBERS(XRRigidTransform,
                                                      (mParent, mPosition,
                                                       mOrientation, mInverse),
                                                      (mMatrixArray))

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
  UpdateInternal();
}

void XRRigidTransform::Update(const gfx::Matrix4x4Double& aTransform) {
  mNeedsUpdate = true;
  mRawTransformMatrix = aTransform;
  gfx::PointDouble3D scale;
  mRawTransformMatrix.Decompose(mRawPosition, mRawOrientation, scale);
  UpdateInternal();
}

void XRRigidTransform::UpdateInternal() {
  if (mPosition) {
    mPosition->SetX(mRawPosition.x);
    mPosition->SetY(mRawPosition.y);
    mPosition->SetZ(mRawPosition.z);
  }
  if (mOrientation) {
    mOrientation->SetX(mRawOrientation.x);
    mOrientation->SetY(mRawOrientation.y);
    mOrientation->SetZ(mRawOrientation.z);
    mOrientation->SetW(mRawOrientation.w);
  }
  if (mInverse) {
    gfx::Matrix4x4Double inverseMatrix = mRawTransformMatrix;
    Unused << inverseMatrix.Invert();
    mInverse->Update(inverseMatrix);
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
    gfx::Matrix4x4Double inverseMatrix = mRawTransformMatrix;
    Unused << inverseMatrix.Invert();
    mInverse = new XRRigidTransform(mParent, inverseMatrix);
  }

  RefPtr<XRRigidTransform> inverse = mInverse;
  return inverse.forget();
}

}  // namespace mozilla::dom
