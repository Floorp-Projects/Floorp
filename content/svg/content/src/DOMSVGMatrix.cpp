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

} // namespace mozilla
DOMCI_DATA(SVGMatrix, mozilla::DOMSVGMatrix)
namespace mozilla {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGMatrix)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(mozilla::DOMSVGMatrix) // pseudo-interface
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGMatrix)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGMatrix)
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

//----------------------------------------------------------------------
// nsIDOMSVGMatrix methods:

/* attribute float a; */
NS_IMETHODIMP DOMSVGMatrix::GetA(float *aA)
{
  *aA = A();
  return NS_OK;
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

NS_IMETHODIMP DOMSVGMatrix::SetA(float aA)
{
  NS_ENSURE_FINITE(aA, NS_ERROR_ILLEGAL_VALUE);
  ErrorResult rv;
  SetA(aA, rv);
  return rv.ErrorCode();
}

/* attribute float b; */
NS_IMETHODIMP DOMSVGMatrix::GetB(float *aB)
{
  *aB = B();
  return NS_OK;
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

NS_IMETHODIMP DOMSVGMatrix::SetB(float aB)
{
  NS_ENSURE_FINITE(aB, NS_ERROR_ILLEGAL_VALUE);
  ErrorResult rv;
  SetB(aB, rv);
  return rv.ErrorCode();
}

/* attribute float c; */
NS_IMETHODIMP DOMSVGMatrix::GetC(float *aC)
{
  *aC = C();
  return NS_OK;
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

NS_IMETHODIMP DOMSVGMatrix::SetC(float aC)
{
  NS_ENSURE_FINITE(aC, NS_ERROR_ILLEGAL_VALUE);
  ErrorResult rv;
  SetC(aC, rv);
  return rv.ErrorCode();
}

/* attribute float d; */
NS_IMETHODIMP DOMSVGMatrix::GetD(float *aD)
{
  *aD = D();
  return NS_OK;
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

NS_IMETHODIMP DOMSVGMatrix::SetD(float aD)
{
  NS_ENSURE_FINITE(aD, NS_ERROR_ILLEGAL_VALUE);
  ErrorResult rv;
  SetD(aD, rv);
  return rv.ErrorCode();
}

/* attribute float e; */
NS_IMETHODIMP DOMSVGMatrix::GetE(float *aE)
{
  *aE = E();
  return NS_OK;
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

NS_IMETHODIMP DOMSVGMatrix::SetE(float aE)
{
  NS_ENSURE_FINITE(aE, NS_ERROR_ILLEGAL_VALUE);
  ErrorResult rv;
  SetE(aE, rv);
  return rv.ErrorCode();
}

/* attribute float f; */
NS_IMETHODIMP DOMSVGMatrix::GetF(float *aF)
{
  *aF = F();
  return NS_OK;
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

NS_IMETHODIMP DOMSVGMatrix::SetF(float aF)
{
  NS_ENSURE_FINITE(aF, NS_ERROR_ILLEGAL_VALUE);
  ErrorResult rv;
  SetF(aF, rv);
  return rv.ErrorCode();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::Multiply(DOMSVGMatrix& aMatrix)
{
  nsCOMPtr<DOMSVGMatrix> matrix = new DOMSVGMatrix(aMatrix.Matrix() * Matrix());
  return matrix.forget();
}

/* nsIDOMSVGMatrix multiply (in nsIDOMSVGMatrix secondMatrix); */
NS_IMETHODIMP DOMSVGMatrix::Multiply(nsIDOMSVGMatrix *secondMatrix,
                                     nsIDOMSVGMatrix **_retval)
{
  *_retval = nullptr;
  nsCOMPtr<DOMSVGMatrix> domMatrix = do_QueryInterface(secondMatrix);
  if (!domMatrix)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  *_retval = Multiply(*domMatrix).get();
  return NS_OK;
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

/* nsIDOMSVGMatrix inverse (); */
NS_IMETHODIMP DOMSVGMatrix::Inverse(nsIDOMSVGMatrix **_retval)
{
  ErrorResult rv;
  *_retval = Inverse(rv).get();
  return rv.ErrorCode();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::Translate(float x, float y)
{
  nsRefPtr<DOMSVGMatrix> matrix =
    new DOMSVGMatrix(gfxMatrix(Matrix()).Translate(gfxPoint(x, y)));
  return matrix.forget();
}


/* nsIDOMSVGMatrix translate (in float x, in float y); */
NS_IMETHODIMP DOMSVGMatrix::Translate(float x, float y,
                                      nsIDOMSVGMatrix **_retval)
{
  if (!NS_finite(x) || !NS_finite(y)) {
    *_retval = nullptr;
    return NS_ERROR_ILLEGAL_VALUE;
  }
  *_retval = Translate(x, y).get();
  return NS_OK;
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::Scale(float scaleFactor)
{
  return ScaleNonUniform(scaleFactor, scaleFactor);
}

 /* nsIDOMSVGMatrix scale (in float scaleFactor); */
NS_IMETHODIMP DOMSVGMatrix::Scale(float scaleFactor, nsIDOMSVGMatrix **_retval)
{
  return ScaleNonUniform(scaleFactor, scaleFactor, _retval);
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::ScaleNonUniform(float scaleFactorX,
                              float scaleFactorY)
{
  nsRefPtr<DOMSVGMatrix> matrix =
    new DOMSVGMatrix(gfxMatrix(Matrix()).Scale(scaleFactorX, scaleFactorY));
  return matrix.forget();
}


 /* nsIDOMSVGMatrix scaleNonUniform (in float scaleFactorX,
 *                                  in float scaleFactorY); */
NS_IMETHODIMP DOMSVGMatrix::ScaleNonUniform(float scaleFactorX,
                                            float scaleFactorY,
                                            nsIDOMSVGMatrix **_retval)
{
  if (!NS_finite(scaleFactorX) || !NS_finite(scaleFactorY)) {
    *_retval = nullptr;
    return NS_ERROR_ILLEGAL_VALUE;
  }
  *_retval = ScaleNonUniform(scaleFactorX, scaleFactorY).get();
  return NS_OK;
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::Rotate(float angle)
{
  nsRefPtr<DOMSVGMatrix> matrix =
    new DOMSVGMatrix(gfxMatrix(Matrix()).Rotate(angle*radPerDegree));
  return matrix.forget();
}

/* nsIDOMSVGMatrix rotate (in float angle); */
NS_IMETHODIMP DOMSVGMatrix::Rotate(float angle, nsIDOMSVGMatrix **_retval)
{
  if (!NS_finite(angle)) {
    *_retval = nullptr;
    return NS_ERROR_ILLEGAL_VALUE;
  }
  *_retval = Rotate(angle).get();
  return NS_OK;
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

/* nsIDOMSVGMatrix rotateFromVector (in float x, in float y); */
NS_IMETHODIMP DOMSVGMatrix::RotateFromVector(float x, float y,
                                             nsIDOMSVGMatrix **_retval)
{
  if (!NS_finite(x) || !NS_finite(y)) {
    *_retval = nullptr;
    return NS_ERROR_ILLEGAL_VALUE;
  }

  ErrorResult rv;
  *_retval = RotateFromVector(x, y, rv).get();
  return rv.ErrorCode();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::FlipX()
{
  const gfxMatrix& mx = Matrix();
  nsRefPtr<DOMSVGMatrix> matrix =
    new DOMSVGMatrix(gfxMatrix(-mx.xx, -mx.yx, mx.xy, mx.yy, mx.x0, mx.y0));
  return matrix.forget();
}

/* nsIDOMSVGMatrix flipX (); */
NS_IMETHODIMP DOMSVGMatrix::FlipX(nsIDOMSVGMatrix **_retval)
{
  *_retval = FlipX().get();
  return NS_OK;
}

already_AddRefed<DOMSVGMatrix>
DOMSVGMatrix::FlipY()
{
  const gfxMatrix& mx = Matrix();
  nsRefPtr<DOMSVGMatrix> matrix =
    new DOMSVGMatrix(gfxMatrix(mx.xx, mx.yx, -mx.xy, -mx.yy, mx.x0, mx.y0));
  return matrix.forget();
}

/* nsIDOMSVGMatrix flipY (); */
NS_IMETHODIMP DOMSVGMatrix::FlipY(nsIDOMSVGMatrix **_retval)
{
  *_retval = FlipY().get();
  return NS_OK;
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

/* nsIDOMSVGMatrix skewX (in float angle); */
NS_IMETHODIMP DOMSVGMatrix::SkewX(float angle, nsIDOMSVGMatrix **_retval)
{
  if (!NS_finite(angle)) {
    *_retval = nullptr;
    return NS_ERROR_ILLEGAL_VALUE;
  }

  ErrorResult rv;
  *_retval = SkewX(angle, rv).get();
  return rv.ErrorCode();
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

/* nsIDOMSVGMatrix skewY (in float angle); */
NS_IMETHODIMP DOMSVGMatrix::SkewY(float angle, nsIDOMSVGMatrix **_retval)
{
  if (!NS_finite(angle)) {
    *_retval = nullptr;
    return NS_ERROR_ILLEGAL_VALUE;
  }

  ErrorResult rv;
  *_retval = SkewY(angle, rv).get();
  return rv.ErrorCode();
}


} // namespace mozilla
