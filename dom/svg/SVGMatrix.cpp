/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGMatrix.h"
#include "nsError.h"
#include <math.h>
#include "mozilla/dom/SVGMatrixBinding.h"
#include "mozilla/FloatingPoint.h"

const double radPerDegree = 2.0 * M_PI / 360.0;

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SVGMatrix, mTransform)

DOMSVGTransform* SVGMatrix::GetParentObject() const { return mTransform; }

JSObject* SVGMatrix::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return SVGMatrix_Binding::Wrap(aCx, this, aGivenProto);
}

void SVGMatrix::SetA(float aA, ErrorResult& rv) {
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = GetMatrix();
  mx._11 = aA;
  SetMatrix(mx);
}

void SVGMatrix::SetB(float aB, ErrorResult& rv) {
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = GetMatrix();
  mx._12 = aB;
  SetMatrix(mx);
}

void SVGMatrix::SetC(float aC, ErrorResult& rv) {
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = GetMatrix();
  mx._21 = aC;
  SetMatrix(mx);
}

void SVGMatrix::SetD(float aD, ErrorResult& rv) {
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = GetMatrix();
  mx._22 = aD;
  SetMatrix(mx);
}

void SVGMatrix::SetE(float aE, ErrorResult& rv) {
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = GetMatrix();
  mx._31 = aE;
  SetMatrix(mx);
}

void SVGMatrix::SetF(float aF, ErrorResult& rv) {
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = GetMatrix();
  mx._32 = aF;
  SetMatrix(mx);
}

already_AddRefed<SVGMatrix> SVGMatrix::Multiply(SVGMatrix& aMatrix) {
  return do_AddRef(new SVGMatrix(aMatrix.GetMatrix() * GetMatrix()));
}

already_AddRefed<SVGMatrix> SVGMatrix::Inverse(ErrorResult& rv) {
  gfxMatrix mat = GetMatrix();
  if (!mat.Invert()) {
    rv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  return do_AddRef(new SVGMatrix(mat));
}

already_AddRefed<SVGMatrix> SVGMatrix::Translate(float x, float y) {
  return do_AddRef(
      new SVGMatrix(gfxMatrix(GetMatrix()).PreTranslate(gfxPoint(x, y))));
}

already_AddRefed<SVGMatrix> SVGMatrix::Scale(float scaleFactor) {
  return ScaleNonUniform(scaleFactor, scaleFactor);
}

already_AddRefed<SVGMatrix> SVGMatrix::ScaleNonUniform(float scaleFactorX,
                                                       float scaleFactorY) {
  return do_AddRef(new SVGMatrix(
      gfxMatrix(GetMatrix()).PreScale(scaleFactorX, scaleFactorY)));
}

already_AddRefed<SVGMatrix> SVGMatrix::Rotate(float angle) {
  return do_AddRef(
      new SVGMatrix(gfxMatrix(GetMatrix()).PreRotate(angle * radPerDegree)));
}

already_AddRefed<SVGMatrix> SVGMatrix::RotateFromVector(float x, float y,
                                                        ErrorResult& rv) {
  if (x == 0.0 || y == 0.0) {
    rv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  return do_AddRef(
      new SVGMatrix(gfxMatrix(GetMatrix()).PreRotate(atan2(y, x))));
}

already_AddRefed<SVGMatrix> SVGMatrix::FlipX() {
  const gfxMatrix& mx = GetMatrix();
  return do_AddRef(new SVGMatrix(
      gfxMatrix(-mx._11, -mx._12, mx._21, mx._22, mx._31, mx._32)));
}

already_AddRefed<SVGMatrix> SVGMatrix::FlipY() {
  const gfxMatrix& mx = GetMatrix();
  return do_AddRef(new SVGMatrix(
      gfxMatrix(mx._11, mx._12, -mx._21, -mx._22, mx._31, mx._32)));
}

already_AddRefed<SVGMatrix> SVGMatrix::SkewX(float angle, ErrorResult& rv) {
  double ta = tan(angle * radPerDegree);
  if (!std::isfinite(ta)) {
    rv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  const gfxMatrix& mx = GetMatrix();
  gfxMatrix skewMx(mx._11, mx._12, mx._21 + mx._11 * ta, mx._22 + mx._12 * ta,
                   mx._31, mx._32);
  return do_AddRef(new SVGMatrix(skewMx));
}

already_AddRefed<SVGMatrix> SVGMatrix::SkewY(float angle, ErrorResult& rv) {
  double ta = tan(angle * radPerDegree);
  if (!std::isfinite(ta)) {
    rv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  const gfxMatrix& mx = GetMatrix();
  gfxMatrix skewMx(mx._11 + mx._21 * ta, mx._12 + mx._22 * ta, mx._21, mx._22,
                   mx._31, mx._32);

  return do_AddRef(new SVGMatrix(skewMx));
}

}  // namespace mozilla::dom
