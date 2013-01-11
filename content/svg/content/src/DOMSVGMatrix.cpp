/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGMatrix.h"
#include "nsError.h"
#include <math.h>
#include "nsContentUtils.h"
#include "mozilla/dom/SVGMatrixBinding.h"

const double radPerDegree = 2.0 * M_PI / 360.0;

namespace mozilla {

//----------------------------------------------------------------------
// nsISupports methods:

// Make sure we clear the weak ref in the owning transform (if there is one)
// upon unlink.
NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGMatrix)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGMatrix)
  if (tmp->mTransform) {
    tmp->mTransform->ClearMatrixTearoff(tmp);
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK(mTransform)
NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGMatrix)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransform)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMSVGMatrix)
NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGMatrix)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGMatrix)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGMatrix)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(mozilla::DOMSVGMatrix) // pseudo-interface
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DOMSVGTransform*
DOMSVGMatrix::GetParentObject() const
{
  return mTransform;
}

JSObject*
DOMSVGMatrix::WrapObject(JSContext* aCx, JSObject* aScope, bool* aTriedToWrap)
{
  return mozilla::dom::SVGMatrixBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

void
DOMSVGMatrix::SetA(float aA, ErrorResult& rv)
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
DOMSVGMatrix::SetB(float aB, ErrorResult& rv)
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
DOMSVGMatrix::SetC(float aC, ErrorResult& rv)
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
DOMSVGMatrix::SetD(float aD, ErrorResult& rv)
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
DOMSVGMatrix::SetE(float aE, ErrorResult& rv)
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
DOMSVGMatrix::SetF(float aF, ErrorResult& rv)
{
  if (IsAnimVal()) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  gfxMatrix mx = Matrix();
  mx.y0 = aF;
  SetMatrix(mx);
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::Multiply(DOMSVGMatrix& aMatrix)
{
  nsCOMPtr<DOMSVGMatrix> matrix = new DOMSVGMatrix(aMatrix.Matrix() * Matrix());
  return matrix.forget();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::Inverse(ErrorResult& rv)
{
  if (Matrix().IsSingular()) {
    rv.Throw(NS_ERROR_DOM_SVG_MATRIX_NOT_INVERTABLE);
    return nullptr;
  }
  nsRefPtr<DOMSVGMatrix> matrix = new DOMSVGMatrix(gfxMatrix(Matrix()).Invert());
  return matrix.forget();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::Translate(float x, float y)
{
  nsRefPtr<DOMSVGMatrix> matrix =
    new DOMSVGMatrix(gfxMatrix(Matrix()).Translate(gfxPoint(x, y)));
  return matrix.forget();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::Scale(float scaleFactor)
{
  return ScaleNonUniform(scaleFactor, scaleFactor);
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::ScaleNonUniform(float scaleFactorX,
                              float scaleFactorY)
{
  nsRefPtr<DOMSVGMatrix> matrix =
    new DOMSVGMatrix(gfxMatrix(Matrix()).Scale(scaleFactorX, scaleFactorY));
  return matrix.forget();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::Rotate(float angle)
{
  nsRefPtr<DOMSVGMatrix> matrix =
    new DOMSVGMatrix(gfxMatrix(Matrix()).Rotate(angle*radPerDegree));
  return matrix.forget();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::RotateFromVector(float x, float y, ErrorResult& rv)
{
  if (x == 0.0 || y == 0.0) {
    rv.Throw(NS_ERROR_RANGE_ERR);
    return nullptr;
  }

  nsRefPtr<DOMSVGMatrix> matrix =
    new DOMSVGMatrix(gfxMatrix(Matrix()).Rotate(atan2(y, x)));
  return matrix.forget();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::FlipX()
{
  const gfxMatrix& mx = Matrix();
  nsRefPtr<DOMSVGMatrix> matrix =
    new DOMSVGMatrix(gfxMatrix(-mx.xx, -mx.yx, mx.xy, mx.yy, mx.x0, mx.y0));
  return matrix.forget();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::FlipY()
{
  const gfxMatrix& mx = Matrix();
  nsRefPtr<DOMSVGMatrix> matrix =
    new DOMSVGMatrix(gfxMatrix(mx.xx, mx.yx, -mx.xy, -mx.yy, mx.x0, mx.y0));
  return matrix.forget();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::SkewX(float angle, ErrorResult& rv)
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
  nsRefPtr<DOMSVGMatrix> matrix = new DOMSVGMatrix(skewMx);
  return matrix.forget();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::SkewY(float angle, ErrorResult& rv)
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

  nsRefPtr<DOMSVGMatrix> matrix = new DOMSVGMatrix(skewMx);
  return matrix.forget();
}

} // namespace mozilla

