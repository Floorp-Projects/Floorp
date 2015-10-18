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

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SVGMatrix, mTransform)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(SVGMatrix, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(SVGMatrix, Release)

SVGTransform*
SVGMatrix::GetParentObject() const
{
  return mTransform;
}

JSObject*
SVGMatrix::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGMatrixBinding::Wrap(aCx, this, aGivenProto);
}

void
SVGMatrix::SetA(float aA, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = GetMatrix();
  mx._11 = aA;
  SetMatrix(mx);
}

void
SVGMatrix::SetB(float aB, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = GetMatrix();
  mx._12 = aB;
  SetMatrix(mx);
}

void
SVGMatrix::SetC(float aC, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = GetMatrix();
  mx._21 = aC;
  SetMatrix(mx);
}

void
SVGMatrix::SetD(float aD, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = GetMatrix();
  mx._22 = aD;
  SetMatrix(mx);
}

void
SVGMatrix::SetE(float aE, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = GetMatrix();
  mx._31 = aE;
  SetMatrix(mx);
}

void
SVGMatrix::SetF(float aF, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = GetMatrix();
  mx._32 = aF;
  SetMatrix(mx);
}

already_AddRefed<SVGMatrix>
SVGMatrix::Multiply(SVGMatrix& aMatrix)
{
  RefPtr<SVGMatrix> matrix = new SVGMatrix(aMatrix.GetMatrix() * GetMatrix());
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::Inverse(ErrorResult& rv)
{
  gfxMatrix mat = GetMatrix();
  if (!mat.Invert()) {
    rv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  RefPtr<SVGMatrix> matrix = new SVGMatrix(mat);
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::Translate(float x, float y)
{
  RefPtr<SVGMatrix> matrix =
    new SVGMatrix(gfxMatrix(GetMatrix()).Translate(gfxPoint(x, y)));
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::Scale(float scaleFactor)
{
  return ScaleNonUniform(scaleFactor, scaleFactor);
}

already_AddRefed<SVGMatrix>
SVGMatrix::ScaleNonUniform(float scaleFactorX,
                           float scaleFactorY)
{
  RefPtr<SVGMatrix> matrix =
    new SVGMatrix(gfxMatrix(GetMatrix()).Scale(scaleFactorX, scaleFactorY));
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::Rotate(float angle)
{
  RefPtr<SVGMatrix> matrix =
    new SVGMatrix(gfxMatrix(GetMatrix()).Rotate(angle*radPerDegree));
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::RotateFromVector(float x, float y, ErrorResult& rv)
{
  if (x == 0.0 || y == 0.0) {
    rv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  RefPtr<SVGMatrix> matrix =
    new SVGMatrix(gfxMatrix(GetMatrix()).Rotate(atan2(y, x)));
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::FlipX()
{
  const gfxMatrix& mx = GetMatrix();
  RefPtr<SVGMatrix> matrix =
    new SVGMatrix(gfxMatrix(-mx._11, -mx._12, mx._21, mx._22, mx._31, mx._32));
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::FlipY()
{
  const gfxMatrix& mx = GetMatrix();
  RefPtr<SVGMatrix> matrix =
    new SVGMatrix(gfxMatrix(mx._11, mx._12, -mx._21, -mx._22, mx._31, mx._32));
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::SkewX(float angle, ErrorResult& rv)
{
  double ta = tan( angle*radPerDegree );
  if (!IsFinite(ta)) {
    rv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  const gfxMatrix& mx = GetMatrix();
  gfxMatrix skewMx(mx._11, mx._12,
                   (float) (mx._21 + mx._11*ta), (float) (mx._22 + mx._12*ta),
                   mx._31, mx._32);
  RefPtr<SVGMatrix> matrix = new SVGMatrix(skewMx);
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::SkewY(float angle, ErrorResult& rv)
{
  double ta = tan( angle*radPerDegree );
  if (!IsFinite(ta)) {
    rv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  const gfxMatrix& mx = GetMatrix();
  gfxMatrix skewMx((float) (mx._11 + mx._21*ta), (float) (mx._12 + mx._22*ta),
                   mx._21, mx._22,
                   mx._31, mx._32);

  RefPtr<SVGMatrix> matrix = new SVGMatrix(skewMx);
  return matrix.forget();
}

} // namespace dom
} // namespace mozilla
