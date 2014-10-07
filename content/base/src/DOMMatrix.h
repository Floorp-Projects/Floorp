/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_DOMMATRIX_H_
#define MOZILLA_DOM_DOMMATRIX_H_

#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/gfx/Matrix.h" // for Matrix4x4

namespace mozilla {
namespace dom {

class GlobalObject;
class DOMMatrix;

class DOMMatrixReadOnly : public nsWrapperCache
{
public:
  explicit DOMMatrixReadOnly(nsISupports* aParent)
    : mParent(aParent), mMatrix2D(new gfx::Matrix())
  {
  }

  DOMMatrixReadOnly(nsISupports* aParent, const DOMMatrixReadOnly& other)
    : mParent(aParent)
  {
    if (other.mMatrix2D) {
      mMatrix2D = new gfx::Matrix(*other.mMatrix2D);
    } else {
      mMatrix3D = new gfx::Matrix4x4(*other.mMatrix3D);
    }
  }

#define GetMatrixMember(entry2D, entry3D, default) \
{ \
  if (mMatrix3D) { \
    return mMatrix3D->entry3D; \
  } \
  return mMatrix2D->entry2D; \
}

#define Get3DMatrixMember(entry3D, default) \
{ \
  if (mMatrix3D) { \
    return mMatrix3D->entry3D; \
  } \
  return default; \
}

  double A() const GetMatrixMember(_11, _11, 1.0)
  double B() const GetMatrixMember(_12, _12, 0)
  double C() const GetMatrixMember(_21, _21, 0)
  double D() const GetMatrixMember(_22, _22, 1.0)
  double E() const GetMatrixMember(_31, _41, 0)
  double F() const GetMatrixMember(_32, _42, 0)

  double M11() const GetMatrixMember(_11, _11, 1.0)
  double M12() const GetMatrixMember(_12, _12, 0)
  double M13() const Get3DMatrixMember(_13, 0)
  double M14() const Get3DMatrixMember(_14, 0)
  double M21() const GetMatrixMember(_21, _21, 0)
  double M22() const GetMatrixMember(_22, _22, 1.0)
  double M23() const Get3DMatrixMember(_23, 0)
  double M24() const Get3DMatrixMember(_24, 0)
  double M31() const Get3DMatrixMember(_31, 0)
  double M32() const Get3DMatrixMember(_32, 0)
  double M33() const Get3DMatrixMember(_33, 1.0)
  double M34() const Get3DMatrixMember(_34, 0)
  double M41() const GetMatrixMember(_31, _41, 0)
  double M42() const GetMatrixMember(_32, _42, 0)
  double M43() const Get3DMatrixMember(_43, 0)
  double M44() const Get3DMatrixMember(_44, 1.0)

#undef GetMatrixMember
#undef Get3DMatrixMember

  already_AddRefed<DOMMatrix> Translate(double aTx,
                                        double aTy,
                                        double aTz = 0) const;
  already_AddRefed<DOMMatrix> Scale(double aScale,
                                    double aOriginX = 0,
                                    double aOriginY = 0) const;
  already_AddRefed<DOMMatrix> Scale3d(double aScale,
                                      double aOriginX = 0,
                                      double aOriginY = 0,
                                      double aOriginZ = 0) const;
  already_AddRefed<DOMMatrix> ScaleNonUniform(double aScaleX,
                                              double aScaleY = 1.0,
                                              double aScaleZ = 1.0,
                                              double aOriginX = 0,
                                              double aOriginY = 0,
                                              double aOriginZ = 0) const;
  already_AddRefed<DOMMatrix> Rotate(double aAngle,
                                     double aOriginX = 0,
                                     double aOriginY = 0) const;
  already_AddRefed<DOMMatrix> RotateFromVector(double aX,
                                               double aY) const;
  already_AddRefed<DOMMatrix> RotateAxisAngle(double aX,
                                              double aY,
                                              double aZ,
                                              double aAngle) const;
  already_AddRefed<DOMMatrix> SkewX(double aSx) const;
  already_AddRefed<DOMMatrix> SkewY(double aSy) const;
  already_AddRefed<DOMMatrix> Multiply(const DOMMatrix& aOther) const;
  already_AddRefed<DOMMatrix> FlipX() const;
  already_AddRefed<DOMMatrix> FlipY() const;
  already_AddRefed<DOMMatrix> Inverse() const;

  bool                        Is2D() const;
  bool                        Identity() const;
  already_AddRefed<DOMPoint>  TransformPoint(const DOMPointInit& aPoint) const;
  void                        ToFloat32Array(JSContext* aCx,
                                             JS::MutableHandle<JSObject*> aResult,
                                             ErrorResult& aRv) const;
  void                        ToFloat64Array(JSContext* aCx,
                                             JS::MutableHandle<JSObject*> aResult,
                                             ErrorResult& aRv) const;
protected:
  nsCOMPtr<nsISupports>     mParent;
  nsAutoPtr<gfx::Matrix>    mMatrix2D;
  nsAutoPtr<gfx::Matrix4x4> mMatrix3D;

  ~DOMMatrixReadOnly()
  {
  }
private:
  DOMMatrixReadOnly() MOZ_DELETE;
  DOMMatrixReadOnly(const DOMMatrixReadOnly&) MOZ_DELETE;
  DOMMatrixReadOnly& operator=(const DOMMatrixReadOnly&) MOZ_DELETE;
};

class DOMMatrix MOZ_FINAL : public DOMMatrixReadOnly
{
public:
  explicit DOMMatrix(nsISupports* aParent)
    : DOMMatrixReadOnly(aParent)
  {}

  DOMMatrix(nsISupports* aParent, const DOMMatrixReadOnly& other)
    : DOMMatrixReadOnly(aParent, other)
  {}

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMMatrix)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMMatrix)

  static already_AddRefed<DOMMatrix>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);
  static already_AddRefed<DOMMatrix>
  Constructor(const GlobalObject& aGlobal, const nsAString& aTransformList, ErrorResult& aRv);
  static already_AddRefed<DOMMatrix>
  Constructor(const GlobalObject& aGlobal, const DOMMatrixReadOnly& aOther, ErrorResult& aRv);
  static already_AddRefed<DOMMatrix>
  Constructor(const GlobalObject& aGlobal, const Float32Array& aArray32, ErrorResult& aRv);
  static already_AddRefed<DOMMatrix>
  Constructor(const GlobalObject& aGlobal, const Float64Array& aArray64, ErrorResult& aRv);
  static already_AddRefed<DOMMatrix>
  Constructor(const GlobalObject& aGlobal, const Sequence<double>& aNumberSequence, ErrorResult& aRv);

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

#define Set2DMatrixMember(entry2D, entry3D) \
{ \
  if (mMatrix3D) { \
    mMatrix3D->entry3D = v; \
  } else { \
    mMatrix2D->entry2D = v; \
  } \
}

#define Set3DMatrixMember(entry3D, default) \
{ \
  if (mMatrix3D || (v != default)) { \
    Ensure3DMatrix(); \
    mMatrix3D->entry3D = v; \
  } \
}

  void SetA(double v) Set2DMatrixMember(_11, _11)
  void SetB(double v) Set2DMatrixMember(_12, _12)
  void SetC(double v) Set2DMatrixMember(_21, _21)
  void SetD(double v) Set2DMatrixMember(_22, _22)
  void SetE(double v) Set2DMatrixMember(_31, _41)
  void SetF(double v) Set2DMatrixMember(_32, _42)

  void SetM11(double v) Set2DMatrixMember(_11, _11)
  void SetM12(double v) Set2DMatrixMember(_12, _12)
  void SetM13(double v) Set3DMatrixMember(_13, 0)
  void SetM14(double v) Set3DMatrixMember(_14, 0)
  void SetM21(double v) Set2DMatrixMember(_21, _21)
  void SetM22(double v) Set2DMatrixMember(_22, _22)
  void SetM23(double v) Set3DMatrixMember(_23, 0)
  void SetM24(double v) Set3DMatrixMember(_24, 0)
  void SetM31(double v) Set3DMatrixMember(_31, 0)
  void SetM32(double v) Set3DMatrixMember(_32, 0)
  void SetM33(double v) Set3DMatrixMember(_33, 1.0)
  void SetM34(double v) Set3DMatrixMember(_34, 0)
  void SetM41(double v) Set2DMatrixMember(_31, _41)
  void SetM42(double v) Set2DMatrixMember(_32, _42)
  void SetM43(double v) Set3DMatrixMember(_43, 0)
  void SetM44(double v) Set3DMatrixMember(_44, 1.0)

#undef Set2DMatrixMember
#undef Set3DMatrixMember

  DOMMatrix* MultiplySelf(const DOMMatrix& aOther);
  DOMMatrix* PreMultiplySelf(const DOMMatrix& aOther);
  DOMMatrix* TranslateSelf(double aTx,
                           double aTy,
                           double aTz = 0);
  DOMMatrix* ScaleSelf(double aScale,
                       double aOriginX = 0,
                       double aOriginY = 0);
  DOMMatrix* Scale3dSelf(double aScale,
                         double aOriginX = 0,
                         double aOriginY = 0,
                         double aOriginZ = 0);
  DOMMatrix* ScaleNonUniformSelf(double aScaleX,
                                 double aScaleY = 1,
                                 double aScaleZ = 1,
                                 double aOriginX = 0,
                                 double aOriginY = 0,
                                 double aOriginZ = 0);
  DOMMatrix* RotateSelf(double aAngle,
                        double aOriginX = 0,
                        double aOriginY = 0);
  DOMMatrix* RotateFromVectorSelf(double aX,
                                  double aY);
  DOMMatrix* RotateAxisAngleSelf(double aX,
                                 double aY,
                                 double aZ,
                                 double aAngle);
  DOMMatrix* SkewXSelf(double aSx);
  DOMMatrix* SkewYSelf(double aSy);
  DOMMatrix* InvertSelf();
  DOMMatrix* SetMatrixValue(const nsAString& aTransformList, ErrorResult& aRv);
private:
  void Ensure3DMatrix();

  ~DOMMatrix() {}
};

}
}

#endif /*MOZILLA_DOM_DOMMATRIX_H_*/
