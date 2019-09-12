/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMPoint.h"

#include "mozilla/dom/DOMPointBinding.h"
#include "mozilla/dom/BindingDeclarations.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMPointReadOnly, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMPointReadOnly, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMPointReadOnly, Release)

already_AddRefed<DOMPointReadOnly> DOMPointReadOnly::FromPoint(
    const GlobalObject& aGlobal, const DOMPointInit& aParams) {
  RefPtr<DOMPointReadOnly> obj = new DOMPointReadOnly(
      aGlobal.GetAsSupports(), aParams.mX, aParams.mY, aParams.mZ, aParams.mW);
  return obj.forget();
}

already_AddRefed<DOMPointReadOnly> DOMPointReadOnly::Constructor(
    const GlobalObject& aGlobal, double aX, double aY, double aZ, double aW) {
  RefPtr<DOMPointReadOnly> obj =
      new DOMPointReadOnly(aGlobal.GetAsSupports(), aX, aY, aZ, aW);
  return obj.forget();
}

JSObject* DOMPointReadOnly::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return DOMPointReadOnly_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DOMPoint> DOMPointReadOnly::MatrixTransform(
    const DOMMatrixInit& aInit, ErrorResult& aRv) {
  RefPtr<DOMMatrixReadOnly> matrix =
      DOMMatrixReadOnly::FromMatrix(mParent, aInit, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  DOMPointInit init;
  init.mX = this->mX;
  init.mY = this->mY;
  init.mZ = this->mZ;
  init.mW = this->mW;
  RefPtr<DOMPoint> point = matrix->TransformPoint(init);
  return point.forget();
}

// https://drafts.fxtf.org/geometry/#structured-serialization
bool DOMPointReadOnly::WriteStructuredClone(
    JSContext* aCx, JSStructuredCloneWriter* aWriter) const {
#define WriteDouble(d)                                                       \
  JS_WriteUint32Pair(aWriter, (BitwiseCast<uint64_t>(d) >> 32) & 0xffffffff, \
                     BitwiseCast<uint64_t>(d) & 0xffffffff)

  return WriteDouble(mX) && WriteDouble(mY) && WriteDouble(mZ) &&
         WriteDouble(mW);

#undef WriteDouble
}

// static
already_AddRefed<DOMPointReadOnly> DOMPointReadOnly::ReadStructuredClone(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    JSStructuredCloneReader* aReader) {
  RefPtr<DOMPointReadOnly> retval = new DOMPointReadOnly(aGlobal);
  if (!retval->ReadStructuredClone(aReader)) {
    return nullptr;
  }
  return retval.forget();
  ;
}

bool DOMPointReadOnly::ReadStructuredClone(JSStructuredCloneReader* aReader) {
  uint32_t high;
  uint32_t low;

#define ReadDouble(d)                             \
  if (!JS_ReadUint32Pair(aReader, &high, &low)) { \
    return false;                                 \
  }                                               \
  (*(d) = BitwiseCast<double>(static_cast<uint64_t>(high) << 32 | low))

  ReadDouble(&mX);
  ReadDouble(&mY);
  ReadDouble(&mZ);
  ReadDouble(&mW);

  return true;
#undef ReadDouble
}

already_AddRefed<DOMPoint> DOMPoint::FromPoint(const GlobalObject& aGlobal,
                                               const DOMPointInit& aParams) {
  RefPtr<DOMPoint> obj = new DOMPoint(aGlobal.GetAsSupports(), aParams.mX,
                                      aParams.mY, aParams.mZ, aParams.mW);
  return obj.forget();
}

already_AddRefed<DOMPoint> DOMPoint::Constructor(const GlobalObject& aGlobal,
                                                 double aX, double aY,
                                                 double aZ, double aW) {
  RefPtr<DOMPoint> obj = new DOMPoint(aGlobal.GetAsSupports(), aX, aY, aZ, aW);
  return obj.forget();
}

JSObject* DOMPoint::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return DOMPoint_Binding::Wrap(aCx, this, aGivenProto);
}

// static
already_AddRefed<DOMPoint> DOMPoint::ReadStructuredClone(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    JSStructuredCloneReader* aReader) {
  RefPtr<DOMPoint> retval = new DOMPoint(aGlobal);
  if (!retval->ReadStructuredClone(aReader)) {
    return nullptr;
  }
  return retval.forget();
  ;
}
