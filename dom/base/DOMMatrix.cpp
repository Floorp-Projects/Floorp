/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMMatrix.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMMatrixBinding.h"
#include "mozilla/dom/DOMPoint.h"
#include "mozilla/dom/DOMPointBinding.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/ServoCSSParser.h"
#include "nsCSSParser.h"
#include "nsStyleTransformMatrix.h"

#include <math.h>

namespace mozilla {
namespace dom {

static const double radPerDegree = 2.0 * M_PI / 360.0;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMMatrixReadOnly, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMMatrixReadOnly, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMMatrixReadOnly, Release)

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::Translate(double aTx,
                             double aTy,
                             double aTz) const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  retval->TranslateSelf(aTx, aTy, aTz);

  return retval.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::Scale(double aScale,
                         double aOriginX,
                         double aOriginY) const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  retval->ScaleSelf(aScale, aOriginX, aOriginY);

  return retval.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::Scale3d(double aScale,
                           double aOriginX,
                           double aOriginY,
                           double aOriginZ) const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  retval->Scale3dSelf(aScale, aOriginX, aOriginY, aOriginZ);

  return retval.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::ScaleNonUniform(double aScaleX,
                                   double aScaleY,
                                   double aScaleZ,
                                   double aOriginX,
                                   double aOriginY,
                                   double aOriginZ) const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  retval->ScaleNonUniformSelf(aScaleX, aScaleY, aScaleZ, aOriginX, aOriginY, aOriginZ);

  return retval.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::Rotate(double aAngle,
                          double aOriginX ,
                          double aOriginY) const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  retval->RotateSelf(aAngle, aOriginX, aOriginY);

  return retval.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::RotateFromVector(double x,
                                    double y) const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  retval->RotateFromVectorSelf(x, y);

  return retval.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::RotateAxisAngle(double aX,
                                   double aY,
                                   double aZ,
                                   double aAngle) const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  retval->RotateAxisAngleSelf(aX, aY, aZ, aAngle);

  return retval.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::SkewX(double aSx) const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  retval->SkewXSelf(aSx);

  return retval.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::SkewY(double aSy) const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  retval->SkewYSelf(aSy);

  return retval.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::Multiply(const DOMMatrix& other) const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  retval->MultiplySelf(other);

  return retval.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::FlipX() const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  if (mMatrix3D) {
    gfx::Matrix4x4 m;
    m._11 = -1;
    retval->mMatrix3D = new gfx::Matrix4x4(m * *mMatrix3D);
  } else {
    gfx::Matrix m;
    m._11 = -1;
    retval->mMatrix2D = new gfx::Matrix(mMatrix2D ? m * *mMatrix2D : m);
  }

  return retval.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::FlipY() const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  if (mMatrix3D) {
    gfx::Matrix4x4 m;
    m._22 = -1;
    retval->mMatrix3D = new gfx::Matrix4x4(m * *mMatrix3D);
  } else {
    gfx::Matrix m;
    m._22 = -1;
    retval->mMatrix2D = new gfx::Matrix(mMatrix2D ? m * *mMatrix2D : m);
  }

  return retval.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrixReadOnly::Inverse() const
{
  RefPtr<DOMMatrix> retval = new DOMMatrix(mParent, *this);
  retval->InvertSelf();

  return retval.forget();
}

bool
DOMMatrixReadOnly::Is2D() const
{
  return !mMatrix3D;
}

bool
DOMMatrixReadOnly::IsIdentity() const
{
  if (mMatrix3D) {
    return mMatrix3D->IsIdentity();
  }

  return mMatrix2D->IsIdentity();
}

already_AddRefed<DOMPoint>
DOMMatrixReadOnly::TransformPoint(const DOMPointInit& point) const
{
  RefPtr<DOMPoint> retval = new DOMPoint(mParent);

  if (mMatrix3D) {
    gfx::Point4D transformedPoint;
    transformedPoint.x = point.mX;
    transformedPoint.y = point.mY;
    transformedPoint.z = point.mZ;
    transformedPoint.w = point.mW;

    transformedPoint = mMatrix3D->TransformPoint(transformedPoint);

    retval->SetX(transformedPoint.x);
    retval->SetY(transformedPoint.y);
    retval->SetZ(transformedPoint.z);
    retval->SetW(transformedPoint.w);
  } else if (point.mZ != 0 || point.mW != 1.0) {
    gfx::Matrix4x4 tempMatrix(gfx::Matrix4x4::From2D(*mMatrix2D));

    gfx::Point4D transformedPoint;
    transformedPoint.x = point.mX;
    transformedPoint.y = point.mY;
    transformedPoint.z = point.mZ;
    transformedPoint.w = point.mW;

    transformedPoint = tempMatrix.TransformPoint(transformedPoint);

    retval->SetX(transformedPoint.x);
    retval->SetY(transformedPoint.y);
    retval->SetZ(transformedPoint.z);
    retval->SetW(transformedPoint.w);
  } else {
    gfx::Point transformedPoint;
    transformedPoint.x = point.mX;
    transformedPoint.y = point.mY;

    transformedPoint = mMatrix2D->TransformPoint(transformedPoint);

    retval->SetX(transformedPoint.x);
    retval->SetY(transformedPoint.y);
    retval->SetZ(point.mZ);
    retval->SetW(point.mW);
  }
  return retval.forget();
}

template <typename T> void GetDataFromMatrix(const DOMMatrixReadOnly* aMatrix, T* aData)
{
  aData[0] = static_cast<T>(aMatrix->M11());
  aData[1] = static_cast<T>(aMatrix->M12());
  aData[2] = static_cast<T>(aMatrix->M13());
  aData[3] = static_cast<T>(aMatrix->M14());
  aData[4] = static_cast<T>(aMatrix->M21());
  aData[5] = static_cast<T>(aMatrix->M22());
  aData[6] = static_cast<T>(aMatrix->M23());
  aData[7] = static_cast<T>(aMatrix->M24());
  aData[8] = static_cast<T>(aMatrix->M31());
  aData[9] = static_cast<T>(aMatrix->M32());
  aData[10] = static_cast<T>(aMatrix->M33());
  aData[11] = static_cast<T>(aMatrix->M34());
  aData[12] = static_cast<T>(aMatrix->M41());
  aData[13] = static_cast<T>(aMatrix->M42());
  aData[14] = static_cast<T>(aMatrix->M43());
  aData[15] = static_cast<T>(aMatrix->M44());
}

void
DOMMatrixReadOnly::ToFloat32Array(JSContext* aCx, JS::MutableHandle<JSObject*> aResult, ErrorResult& aRv) const
{
  AutoTArray<float, 16> arr;
  arr.SetLength(16);
  GetDataFromMatrix(this, arr.Elements());
  JS::Rooted<JS::Value> value(aCx);
  if (!ToJSValue(aCx, TypedArrayCreator<Float32Array>(arr), &value)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  aResult.set(&value.toObject());
}

void
DOMMatrixReadOnly::ToFloat64Array(JSContext* aCx, JS::MutableHandle<JSObject*> aResult, ErrorResult& aRv) const
{
  AutoTArray<double, 16> arr;
  arr.SetLength(16);
  GetDataFromMatrix(this, arr.Elements());
  JS::Rooted<JS::Value> value(aCx);
  if (!ToJSValue(aCx, TypedArrayCreator<Float64Array>(arr), &value)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  aResult.set(&value.toObject());
}

// Convenient way to append things as floats, not doubles.  We use this because
// we only want to output about 6 digits of precision for our matrix()
// functions, to preserve the behavior we used to have when we used
// AppendPrintf.
static void
AppendFloat(nsAString& aStr, float f)
{
  aStr.AppendFloat(f);
}

void
DOMMatrixReadOnly::Stringify(nsAString& aResult)
{
  nsAutoString matrixStr;
  if (mMatrix3D) {
    // We can't use AppendPrintf here, because it does locale-specific
    // formatting of floating-point values.
    matrixStr.AssignLiteral("matrix3d(");
    AppendFloat(matrixStr, M11()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M12()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M13()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M14()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M21()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M22()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M23()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M24()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M31()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M32()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M33()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M34()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M41()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M42()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M43()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, M44());
    matrixStr.AppendLiteral(")");
  } else {
    // We can't use AppendPrintf here, because it does locale-specific
    // formatting of floating-point values.
    matrixStr.AssignLiteral("matrix(");
    AppendFloat(matrixStr, A()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, B()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, C()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, D()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, E()); matrixStr.AppendLiteral(", ");
    AppendFloat(matrixStr, F());
    matrixStr.AppendLiteral(")");
  }

  aResult = matrixStr;
}

static bool
IsStyledByServo(JSContext* aContext)
{
  nsGlobalWindowInner* win = xpc::CurrentWindowOrNull(aContext);
  nsIDocument* doc = win ? win->GetDoc() : nullptr;
  return doc ? doc->IsStyledByServo() : false;
}

already_AddRefed<DOMMatrix>
DOMMatrix::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  RefPtr<DOMMatrix> obj = new DOMMatrix(aGlobal.GetAsSupports(),
                                        IsStyledByServo(aGlobal.Context()));
  return obj.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrix::Constructor(const GlobalObject& aGlobal, const nsAString& aTransformList, ErrorResult& aRv)
{
  RefPtr<DOMMatrix> obj = new DOMMatrix(aGlobal.GetAsSupports(),
                                        IsStyledByServo(aGlobal.Context()));

  obj = obj->SetMatrixValue(aTransformList, aRv);
  return obj.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrix::Constructor(const GlobalObject& aGlobal, const DOMMatrixReadOnly& aOther, ErrorResult& aRv)
{
  RefPtr<DOMMatrix> obj = new DOMMatrix(aGlobal.GetAsSupports(), aOther);
  return obj.forget();
}

template <typename T> void SetDataInMatrix(DOMMatrix* aMatrix, const T* aData, int aLength, ErrorResult& aRv)
{
  if (aLength == 16) {
    aMatrix->SetM11(aData[0]);
    aMatrix->SetM12(aData[1]);
    aMatrix->SetM13(aData[2]);
    aMatrix->SetM14(aData[3]);
    aMatrix->SetM21(aData[4]);
    aMatrix->SetM22(aData[5]);
    aMatrix->SetM23(aData[6]);
    aMatrix->SetM24(aData[7]);
    aMatrix->SetM31(aData[8]);
    aMatrix->SetM32(aData[9]);
    aMatrix->SetM33(aData[10]);
    aMatrix->SetM34(aData[11]);
    aMatrix->SetM41(aData[12]);
    aMatrix->SetM42(aData[13]);
    aMatrix->SetM43(aData[14]);
    aMatrix->SetM44(aData[15]);
  } else if (aLength == 6) {
    aMatrix->SetA(aData[0]);
    aMatrix->SetB(aData[1]);
    aMatrix->SetC(aData[2]);
    aMatrix->SetD(aData[3]);
    aMatrix->SetE(aData[4]);
    aMatrix->SetF(aData[5]);
  } else {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
  }
}

already_AddRefed<DOMMatrix>
DOMMatrix::Constructor(const GlobalObject& aGlobal, const Float32Array& aArray32, ErrorResult& aRv)
{
  RefPtr<DOMMatrix> obj = new DOMMatrix(aGlobal.GetAsSupports(),
                                        IsStyledByServo(aGlobal.Context()));
  aArray32.ComputeLengthAndData();
  SetDataInMatrix(obj, aArray32.Data(), aArray32.Length(), aRv);

  return obj.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrix::Constructor(const GlobalObject& aGlobal, const Float64Array& aArray64, ErrorResult& aRv)
{
  RefPtr<DOMMatrix> obj = new DOMMatrix(aGlobal.GetAsSupports(),
                                        IsStyledByServo(aGlobal.Context()));
  aArray64.ComputeLengthAndData();
  SetDataInMatrix(obj, aArray64.Data(), aArray64.Length(), aRv);

  return obj.forget();
}

already_AddRefed<DOMMatrix>
DOMMatrix::Constructor(const GlobalObject& aGlobal, const Sequence<double>& aNumberSequence, ErrorResult& aRv)
{
  RefPtr<DOMMatrix> obj = new DOMMatrix(aGlobal.GetAsSupports(),
                                        IsStyledByServo(aGlobal.Context()));
  SetDataInMatrix(obj, aNumberSequence.Elements(), aNumberSequence.Length(), aRv);

  return obj.forget();
}

void DOMMatrix::Ensure3DMatrix()
{
  if (!mMatrix3D) {
    mMatrix3D = new gfx::Matrix4x4(gfx::Matrix4x4::From2D(*mMatrix2D));
    mMatrix2D = nullptr;
  }
}

DOMMatrix*
DOMMatrix::MultiplySelf(const DOMMatrix& aOther)
{
  if (aOther.IsIdentity()) {
    return this;
  }

  if (aOther.Is2D()) {
    if (mMatrix3D) {
      *mMatrix3D = gfx::Matrix4x4::From2D(*aOther.mMatrix2D) * *mMatrix3D;
    } else {
      *mMatrix2D = *aOther.mMatrix2D * *mMatrix2D;
    }
  } else {
    Ensure3DMatrix();
    *mMatrix3D = *aOther.mMatrix3D * *mMatrix3D;
  }

  return this;
}

DOMMatrix*
DOMMatrix::PreMultiplySelf(const DOMMatrix& aOther)
{
  if (aOther.IsIdentity()) {
    return this;
  }

  if (aOther.Is2D()) {
    if (mMatrix3D) {
      *mMatrix3D = *mMatrix3D * gfx::Matrix4x4::From2D(*aOther.mMatrix2D);
    } else {
      *mMatrix2D = *mMatrix2D * *aOther.mMatrix2D;
    }
  } else {
    Ensure3DMatrix();
    *mMatrix3D = *mMatrix3D * *aOther.mMatrix3D;
  }

  return this;
}

DOMMatrix*
DOMMatrix::TranslateSelf(double aTx,
                         double aTy,
                         double aTz)
{
  if (aTx == 0 && aTy == 0 && aTz == 0) {
    return this;
  }

  if (mMatrix3D || aTz != 0) {
    Ensure3DMatrix();
    mMatrix3D->PreTranslate(aTx, aTy, aTz);
  } else {
    mMatrix2D->PreTranslate(aTx, aTy);
  }

  return this;
}

DOMMatrix*
DOMMatrix::ScaleSelf(double aScale, double aOriginX, double aOriginY)
{
  ScaleNonUniformSelf(aScale, aScale, 1.0, aOriginX, aOriginY, 0);

  return this;
}

DOMMatrix*
DOMMatrix::Scale3dSelf(double aScale, double aOriginX,
                       double aOriginY, double aOriginZ)
{
  ScaleNonUniformSelf(aScale, aScale, aScale, aOriginX, aOriginY, aOriginZ);

  return this;
}

DOMMatrix*
DOMMatrix::ScaleNonUniformSelf(double aScaleX,
                               double aScaleY,
                               double aScaleZ,
                               double aOriginX,
                               double aOriginY,
                               double aOriginZ)
{
  if (aScaleX == 1.0 && aScaleY == 1.0 && aScaleZ == 1.0) {
    return this;
  }

  TranslateSelf(aOriginX, aOriginY, aOriginZ);

  if (mMatrix3D || aScaleZ != 1.0 || aOriginZ != 0) {
    Ensure3DMatrix();
    gfx::Matrix4x4 m;
    m._11 = aScaleX;
    m._22 = aScaleY;
    m._33 = aScaleZ;
    *mMatrix3D = m * *mMatrix3D;
  } else {
    gfx::Matrix m;
    m._11 = aScaleX;
    m._22 = aScaleY;
    *mMatrix2D = m * *mMatrix2D;
  }

  TranslateSelf(-aOriginX, -aOriginY, -aOriginZ);

  return this;
}

DOMMatrix*
DOMMatrix::RotateFromVectorSelf(double aX, double aY)
{
  if (aX == 0.0 || aY == 0.0) {
    return this;
  }

  RotateSelf(atan2(aY, aX) / radPerDegree);

  return this;
}

DOMMatrix*
DOMMatrix::RotateSelf(double aAngle, double aOriginX, double aOriginY)
{
  if (fmod(aAngle, 360) == 0) {
    return this;
  }

  TranslateSelf(aOriginX, aOriginY);

  if (mMatrix3D) {
    RotateAxisAngleSelf(0, 0, 1, aAngle);
  } else {
    *mMatrix2D = mMatrix2D->PreRotate(aAngle * radPerDegree);
  }

  TranslateSelf(-aOriginX, -aOriginY);

  return this;
}

DOMMatrix*
DOMMatrix::RotateAxisAngleSelf(double aX, double aY,
                               double aZ, double aAngle)
{
  if (fmod(aAngle, 360) == 0) {
    return this;
  }

  aAngle *= radPerDegree;

  Ensure3DMatrix();
  gfx::Matrix4x4 m;
  m.SetRotateAxisAngle(aX, aY, aZ, aAngle);

  *mMatrix3D = m * *mMatrix3D;

  return this;
}

DOMMatrix*
DOMMatrix::SkewXSelf(double aSx)
{
  if (fmod(aSx, 360) == 0) {
    return this;
  }

  if (mMatrix3D) {
    gfx::Matrix4x4 m;
    m._21 = tan(aSx * radPerDegree);
    *mMatrix3D = m * *mMatrix3D;
  } else {
    gfx::Matrix m;
    m._21 = tan(aSx * radPerDegree);
    *mMatrix2D = m * *mMatrix2D;
  }

  return this;
}

DOMMatrix*
DOMMatrix::SkewYSelf(double aSy)
{
  if (fmod(aSy, 360) == 0) {
    return this;
  }

  if (mMatrix3D) {
    gfx::Matrix4x4 m;
    m._12 = tan(aSy * radPerDegree);
    *mMatrix3D = m * *mMatrix3D;
  } else {
    gfx::Matrix m;
    m._12 = tan(aSy * radPerDegree);
    *mMatrix2D = m * *mMatrix2D;
  }

  return this;
}

DOMMatrix*
DOMMatrix::InvertSelf()
{
  if (mMatrix3D) {
    if (!mMatrix3D->Invert()) {
      mMatrix3D->SetNAN();
    }
  } else if (!mMatrix2D->Invert()) {
    mMatrix2D = nullptr;

    mMatrix3D = new gfx::Matrix4x4();
    mMatrix3D->SetNAN();
  }

  return this;
}

DOMMatrix*
DOMMatrix::SetMatrixValue(const nsAString& aTransformList, ErrorResult& aRv)
{
  // An empty string is a no-op.
  if (aTransformList.IsEmpty()) {
    return this;
  }

  gfx::Matrix4x4 transform;
  bool contains3dTransform = false;
  if (mIsServo) {
    if (!ServoCSSParser::ParseTransformIntoMatrix(aTransformList,
                                                  contains3dTransform,
                                                  transform.components)) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return nullptr;
    }
  } else {
    nsCSSValue value;
    nsCSSParser parser;
    bool parseSuccess = parser.ParseTransformProperty(aTransformList,
                                                      true,
                                                      value);
    if (!parseSuccess) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return nullptr;
    }

    // A value of "none" results in a 2D identity matrix.
    if (value.GetUnit() == eCSSUnit_None) {
      mMatrix3D = nullptr;
      mMatrix2D = new gfx::Matrix();
      return this;
    }

    // A value other than a transform-list is a syntax error.
    if (value.GetUnit() != eCSSUnit_SharedList) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return nullptr;
    }

    RuleNodeCacheConditions dummy;
    nsStyleTransformMatrix::TransformReferenceBox dummyBox;
    transform = nsStyleTransformMatrix::ReadTransforms(
                    value.GetSharedListValue()->mHead,
                    nullptr, nullptr, dummy, dummyBox,
                    nsPresContext::AppUnitsPerCSSPixel(),
                    &contains3dTransform);
  }

  if (!contains3dTransform) {
    mMatrix3D = nullptr;
    mMatrix2D = new gfx::Matrix();

    SetA(transform._11);
    SetB(transform._12);
    SetC(transform._21);
    SetD(transform._22);
    SetE(transform._41);
    SetF(transform._42);
  } else {
    mMatrix3D = new gfx::Matrix4x4(transform);
    mMatrix2D = nullptr;
  }

  return this;
}

JSObject*
DOMMatrix::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DOMMatrixBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
