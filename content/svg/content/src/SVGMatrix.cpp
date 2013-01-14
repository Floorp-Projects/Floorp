/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGMatrix.h"
#include "nsError.h"
#include <math.h>
#include "nsContentUtils.h"
#include "mozilla/dom/SVGMatrixBinding.h"

const double radPerDegree = 2.0 * M_PI / 360.0;

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_CYCLE_COLLECTION_CLASS(SVGMatrix)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SVGMatrix)
NS_IMPL_CYCLE_COLLECTION_UNLINK(mTransform)
NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SVGMatrix)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransform)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(SVGMatrix)
NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGMatrix)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGMatrix)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGMatrix)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::SVGMatrix) // pseudo-interface
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DOMSVGTransform*
SVGMatrix::GetParentObject() const
{
  return mTransform;
}

JSObject*
SVGMatrix::WrapObject(JSContext* aCx, JSObject* aScope, bool* aTriedToWrap)
{
  return SVGMatrixBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

void
SVGMatrix::SetA(float aA, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = Matrix();
  mx.xx = aA;
  SetMatrix(mx);
}

void
SVGMatrix::SetB(float aB, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = Matrix();
  mx.yx = aB;
  SetMatrix(mx);
}

void
SVGMatrix::SetC(float aC, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = Matrix();
  mx.xy = aC;
  SetMatrix(mx);
}

void
SVGMatrix::SetD(float aD, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = Matrix();
  mx.yy = aD;
  SetMatrix(mx);
}

void
SVGMatrix::SetE(float aE, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = Matrix();
  mx.x0 = aE;
  SetMatrix(mx);
}

void
SVGMatrix::SetF(float aF, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = Matrix();
  mx.y0 = aF;
  SetMatrix(mx);
}

already_AddRefed<SVGMatrix>
SVGMatrix::Multiply(SVGMatrix& aMatrix)
{
  nsCOMPtr<SVGMatrix> matrix = new SVGMatrix(aMatrix.Matrix() * Matrix());
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::Inverse(ErrorResult& rv)
{
  if (Matrix().IsSingular()) {
    rv.Throw(NS_ERROR_DOM_SVG_MATRIX_NOT_INVERTABLE);
    return nullptr;
  }
  nsRefPtr<SVGMatrix> matrix = new SVGMatrix(gfxMatrix(Matrix()).Invert());
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::Translate(float x, float y)
{
  nsRefPtr<SVGMatrix> matrix =
    new SVGMatrix(gfxMatrix(Matrix()).Translate(gfxPoint(x, y)));
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
  nsRefPtr<SVGMatrix> matrix =
    new SVGMatrix(gfxMatrix(Matrix()).Scale(scaleFactorX, scaleFactorY));
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::Rotate(float angle)
{
  nsRefPtr<SVGMatrix> matrix =
    new SVGMatrix(gfxMatrix(Matrix()).Rotate(angle*radPerDegree));
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::RotateFromVector(float x, float y, ErrorResult& rv)
{
  if (x == 0.0 || y == 0.0) {
    rv.Throw(NS_ERROR_RANGE_ERR);
    return nullptr;
  }

  nsRefPtr<SVGMatrix> matrix =
    new SVGMatrix(gfxMatrix(Matrix()).Rotate(atan2(y, x)));
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::FlipX()
{
  const gfxMatrix& mx = Matrix();
  nsRefPtr<SVGMatrix> matrix =
    new SVGMatrix(gfxMatrix(-mx.xx, -mx.yx, mx.xy, mx.yy, mx.x0, mx.y0));
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::FlipY()
{
  const gfxMatrix& mx = Matrix();
  nsRefPtr<SVGMatrix> matrix =
    new SVGMatrix(gfxMatrix(mx.xx, mx.yx, -mx.xy, -mx.yy, mx.x0, mx.y0));
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::SkewX(float angle, ErrorResult& rv)
{
  double ta = tan( angle*radPerDegree );
  if (!NS_finite(ta)) {
    rv.Throw(NS_ERROR_RANGE_ERR);
    return nullptr;
  }

  const gfxMatrix& mx = Matrix();
  gfxMatrix skewMx(mx.xx, mx.yx,
                   (float) (mx.xy + mx.xx*ta), (float) (mx.yy + mx.yx*ta),
                   mx.x0, mx.y0);
  nsRefPtr<SVGMatrix> matrix = new SVGMatrix(skewMx);
  return matrix.forget();
}

already_AddRefed<SVGMatrix>
SVGMatrix::SkewY(float angle, ErrorResult& rv)
{
  double ta = tan( angle*radPerDegree );
  if (!NS_finite(ta)) {
    rv.Throw(NS_ERROR_RANGE_ERR);
    return nullptr;
  }

  const gfxMatrix& mx = Matrix();
  gfxMatrix skewMx((float) (mx.xx + mx.xy*ta), (float) (mx.yx + mx.yy*ta),
                   mx.xy, mx.yy,
                   mx.x0, mx.y0);

  nsRefPtr<SVGMatrix> matrix = new SVGMatrix(skewMx);
  return matrix.forget();
}

} // namespace dom
} // namespace mozilla
