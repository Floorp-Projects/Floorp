/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebKitCSSMatrix.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/WebKitCSSMatrixBinding.h"

namespace mozilla {
namespace dom {

static const double sRadPerDegree = 2.0 * M_PI / 360.0;

bool
WebKitCSSMatrix::FeatureEnabled(JSContext* aCx, JSObject* aObj)
{
  return Preferences::GetBool("layout.css.DOMMatrix.enabled", false) &&
         Preferences::GetBool("layout.css.prefixes.webkit", false);
}

already_AddRefed<WebKitCSSMatrix>
WebKitCSSMatrix::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  RefPtr<WebKitCSSMatrix> obj = new WebKitCSSMatrix(aGlobal.GetAsSupports());
  return obj.forget();
}

already_AddRefed<WebKitCSSMatrix>
WebKitCSSMatrix::Constructor(const GlobalObject& aGlobal,
                             const nsAString& aTransformList, ErrorResult& aRv)
{
  RefPtr<WebKitCSSMatrix> obj = new WebKitCSSMatrix(aGlobal.GetAsSupports());
  obj = obj->SetMatrixValue(aTransformList, aRv);
  return obj.forget();
}

already_AddRefed<WebKitCSSMatrix>
WebKitCSSMatrix::Constructor(const GlobalObject& aGlobal,
                             const DOMMatrixReadOnly& aOther, ErrorResult& aRv)
{
  RefPtr<WebKitCSSMatrix> obj = new WebKitCSSMatrix(aGlobal.GetAsSupports(),
                                                    aOther);
  return obj.forget();
}

JSObject*
WebKitCSSMatrix::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WebKitCSSMatrixBinding::Wrap(aCx, this, aGivenProto);
}

WebKitCSSMatrix*
WebKitCSSMatrix::SetMatrixValue(const nsAString& aTransformList,
                                ErrorResult& aRv)
{
  DOMMatrix::SetMatrixValue(aTransformList, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return this;
}

already_AddRefed<WebKitCSSMatrix>
WebKitCSSMatrix::Multiply(const WebKitCSSMatrix& other) const
{
  RefPtr<WebKitCSSMatrix> retval = new WebKitCSSMatrix(mParent, *this);
  retval->MultiplySelf(other);

  return retval.forget();
}

already_AddRefed<WebKitCSSMatrix>
WebKitCSSMatrix::Inverse(ErrorResult& aRv) const
{
  RefPtr<WebKitCSSMatrix> retval = new WebKitCSSMatrix(mParent, *this);
  retval->InvertSelfThrow(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return retval.forget();
}

WebKitCSSMatrix*
WebKitCSSMatrix::InvertSelfThrow(ErrorResult& aRv)
{
  if (mMatrix3D) {
    if (!mMatrix3D->Invert()) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }
  } else if (!mMatrix2D->Invert()) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  return this;
}

already_AddRefed<WebKitCSSMatrix>
WebKitCSSMatrix::Translate(double aTx,
                           double aTy,
                           double aTz) const
{
  RefPtr<WebKitCSSMatrix> retval = new WebKitCSSMatrix(mParent, *this);
  retval->TranslateSelf(aTx, aTy, aTz);

  return retval.forget();
}

already_AddRefed<WebKitCSSMatrix>
WebKitCSSMatrix::Scale(double aScaleX,
                       const Optional<double>& aScaleY,
                       double aScaleZ) const
{
  double scaleX = aScaleX;
  double scaleY = aScaleY.WasPassed() ? aScaleY.Value() : scaleX;
  double scaleZ = aScaleZ;

  RefPtr<WebKitCSSMatrix> retval = new WebKitCSSMatrix(mParent, *this);
  retval->ScaleNonUniformSelf(scaleX, scaleY, scaleZ);

  return retval.forget();
}

already_AddRefed<WebKitCSSMatrix>
WebKitCSSMatrix::Rotate(double aRotX,
                        const Optional<double>& aRotY,
                        const Optional<double>& aRotZ) const
{
  double rotX = aRotX;
  double rotY;
  double rotZ;

  if (!aRotY.WasPassed() && !aRotZ.WasPassed()) {
    rotZ = rotX;
    rotX = 0;
    rotY = 0;
  } else {
    rotY = aRotY.WasPassed() ? aRotY.Value() : 0;
    rotZ = aRotZ.WasPassed() ? aRotZ.Value() : 0;
  }

  RefPtr<WebKitCSSMatrix> retval = new WebKitCSSMatrix(mParent, *this);
  retval->Rotate3dSelf(rotX, rotY, rotZ);

  return retval.forget();
}

WebKitCSSMatrix*
WebKitCSSMatrix::Rotate3dSelf(double aRotX,
                              double aRotY,
                              double aRotZ)
{
  if (aRotX != 0 || aRotY != 0) {
    Ensure3DMatrix();
  }

  if (mMatrix3D) {
    if (fmod(aRotZ, 360) != 0) {
      mMatrix3D->RotateZ(aRotZ * sRadPerDegree);
    }
    if (fmod(aRotY, 360) != 0) {
      mMatrix3D->RotateY(aRotY * sRadPerDegree);
    }
    if (fmod(aRotX, 360) != 0) {
      mMatrix3D->RotateX(aRotX * sRadPerDegree);
    }
  } else if (fmod(aRotZ, 360) != 0) {
    mMatrix2D->PreRotate(aRotZ * sRadPerDegree);
  }

  return this;
}

already_AddRefed<WebKitCSSMatrix>
WebKitCSSMatrix::RotateAxisAngle(double aX,
                                 double aY,
                                 double aZ,
                                 double aAngle) const
{
  RefPtr<WebKitCSSMatrix> retval = new WebKitCSSMatrix(mParent, *this);
  retval->RotateAxisAngleSelf(aX, aY, aZ, aAngle);

  return retval.forget();
}

already_AddRefed<WebKitCSSMatrix>
WebKitCSSMatrix::SkewX(double aSx) const
{
  RefPtr<WebKitCSSMatrix> retval = new WebKitCSSMatrix(mParent, *this);
  retval->SkewXSelf(aSx);

  return retval.forget();
}

already_AddRefed<WebKitCSSMatrix>
WebKitCSSMatrix::SkewY(double aSy) const
{
  RefPtr<WebKitCSSMatrix> retval = new WebKitCSSMatrix(mParent, *this);
  retval->SkewYSelf(aSy);

  return retval.forget();
}

} // namespace dom
} // namespace mozilla
