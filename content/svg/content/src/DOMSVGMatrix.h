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

#ifndef MOZILLA_DOMSVGMATRIX_H__
#define MOZILLA_DOMSVGMATRIX_H__

#include "DOMSVGTransform.h"
#include "gfxMatrix.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMSVGMatrix.h"
#include "mozilla/Attributes.h"

// We make DOMSVGMatrix a pseudo-interface to allow us to QI to it in order
// to check that the objects that scripts pass in are our *native* matrix
// objects.
//
// {633419E5-7E88-4C3E-8A9A-856F635E90A3}
#define MOZILLA_DOMSVGMATRIX_IID \
  { 0x633419E5, 0x7E88, 0x4C3E, \
    { 0x8A, 0x9A, 0x85, 0x6F, 0x63, 0x5E, 0x90, 0xA3 } }

namespace mozilla {

/**
 * DOM wrapper for an SVG matrix.
 */
class DOMSVGMatrix MOZ_FINAL : public nsIDOMSVGMatrix
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOMSVGMATRIX_IID)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMSVGMatrix)
  NS_DECL_NSIDOMSVGMATRIX

  /**
   * Ctor for DOMSVGMatrix objects that belong to a DOMSVGTransform.
   */
  DOMSVGMatrix(DOMSVGTransform& aTransform) : mTransform(&aTransform) { }

  /**
   * Ctors for DOMSVGMatrix objects created independently of a DOMSVGTransform.
   */
  DOMSVGMatrix() { } // Default ctor for gfxMatrix will produce identity mx
  DOMSVGMatrix(const gfxMatrix &aMatrix) : mMatrix(aMatrix) { }
  ~DOMSVGMatrix() {
    if (mTransform) {
      mTransform->ClearMatrixTearoff(this);
    }
  }

  const gfxMatrix& Matrix() const {
    return mTransform ? mTransform->Matrix() : mMatrix;
  }

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

  nsRefPtr<DOMSVGTransform> mTransform;

  // Typically we operate on the matrix data accessed via mTransform but for
  // matrices that exist independently of an SVGTransform we use mMatrix below.
  gfxMatrix mMatrix;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMSVGMatrix, MOZILLA_DOMSVGMATRIX_IID)

} // namespace mozilla

#endif // MOZILLA_DOMSVGMATRIX_H__
