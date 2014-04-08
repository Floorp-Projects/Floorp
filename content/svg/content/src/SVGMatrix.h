/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Notes on transforms in Mozilla and the SVG code.
 *
 * It's important to note that the matrix convention used in the SVG standard
 * is the opposite convention to the one used in the Mozilla code or, more
 * specifically, the convention used in Thebes code (code using gfxMatrix).
 * Whereas the SVG standard uses the column vector convention, Thebes code uses
 * the row vector convention. Thus, whereas in the SVG standard you have
 * [M1][M2][M3]|p|, in Thebes you have |p|'[M3]'[M2]'[M1]'. In other words, the
 * following are equivalent:
 *
 *                       / a1 c1 tx1 \   / a2 c2 tx2 \   / a3 c3 tx3 \   / x \
 * SVG:                  | b1 d1 ty1 |   | b2 d2 ty2 |   | b3 d3 ty3 |   | y |
 *                       \  0  0   1 /   \  0  0   1 /   \  0  0   1 /   \ 1 /
 *
 *                       /  a3  b3 0 \   /  a2  b2 0 \   /  a1  b1 0 \
 * Thebes:   [ x y 1 ]   |  c3  d3 0 |   |  c2  d2 0 |   |  c1  d1 0 |
 *                       \ tx3 ty3 1 /   \ tx2 ty2 1 /   \ tx1 ty1 1 /
 *
 * Because the Thebes representation of a transform is the transpose of the SVG
 * representation, our transform order must be reversed when representing SVG
 * transforms using gfxMatrix in the SVG code. Since the SVG implementation
 * stores and obtains matrices in SVG order, to do this we must pre-multiply
 * gfxMatrix objects that represent SVG transforms instead of post-multiplying
 * them as we would for matrices using SVG's column vector convention.
 * Pre-multiplying may look wrong if you're only familiar with the SVG
 * convention, but in that case hopefully the above explanation clears things
 * up.
 */

#ifndef mozilla_dom_SVGMatrix_h
#define mozilla_dom_SVGMatrix_h

#include "mozilla/dom/SVGTransform.h"
#include "gfxMatrix.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {

/**
 * DOM wrapper for an SVG matrix.
 */
class SVGMatrix MOZ_FINAL : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(SVGMatrix)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(SVGMatrix)

  /**
   * Ctor for SVGMatrix objects that belong to a SVGTransform.
   */
  SVGMatrix(SVGTransform& aTransform) : mTransform(&aTransform) {
    SetIsDOMBinding();
  }

  /**
   * Ctors for SVGMatrix objects created independently of a SVGTransform.
   */
  // Default ctor for gfxMatrix will produce identity mx
  SVGMatrix() {
    SetIsDOMBinding();
  }

  SVGMatrix(const gfxMatrix &aMatrix) : mMatrix(aMatrix) {
    SetIsDOMBinding();
  }

  const gfxMatrix& GetMatrix() const {
    return mTransform ? mTransform->Matrixgfx() : mMatrix;
  }

  // WebIDL
  SVGTransform* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  float A() const { return static_cast<float>(GetMatrix().xx); }
  void SetA(float aA, ErrorResult& rv);
  float B() const { return static_cast<float>(GetMatrix().yx); }
  void SetB(float aB, ErrorResult& rv);
  float C() const { return static_cast<float>(GetMatrix().xy); }
  void SetC(float aC, ErrorResult& rv);
  float D() const { return static_cast<float>(GetMatrix().yy); }
  void SetD(float aD, ErrorResult& rv);
  float E() const { return static_cast<float>(GetMatrix().x0); }
  void SetE(float aE, ErrorResult& rv);
  float F() const { return static_cast<float>(GetMatrix().y0); }
  void SetF(float aF, ErrorResult& rv);
  already_AddRefed<SVGMatrix> Multiply(SVGMatrix& aMatrix);
  already_AddRefed<SVGMatrix> Inverse(ErrorResult& aRv);
  already_AddRefed<SVGMatrix> Translate(float x, float y);
  already_AddRefed<SVGMatrix> Scale(float scaleFactor);
  already_AddRefed<SVGMatrix> ScaleNonUniform(float scaleFactorX,
                                              float scaleFactorY);
  already_AddRefed<SVGMatrix> Rotate(float angle);
  already_AddRefed<SVGMatrix> RotateFromVector(float x,
                                               float y,
                                               ErrorResult& aRv);
  already_AddRefed<SVGMatrix> FlipX();
  already_AddRefed<SVGMatrix> FlipY();
  already_AddRefed<SVGMatrix> SkewX(float angle, ErrorResult& rv);
  already_AddRefed<SVGMatrix> SkewY(float angle, ErrorResult& rv);

private:
  void SetMatrix(const gfxMatrix& aMatrix) {
    if (mTransform) {
      mTransform->SetMatrix(aMatrix);
    } else {
      mMatrix = aMatrix;
    }
  }

  bool IsAnimVal() const {
    return mTransform ? mTransform->IsAnimVal() : false;
  }

  nsRefPtr<SVGTransform> mTransform;

  // Typically we operate on the matrix data accessed via mTransform but for
  // matrices that exist independently of an SVGTransform we use mMatrix below.
  gfxMatrix mMatrix;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGMatrix_h
