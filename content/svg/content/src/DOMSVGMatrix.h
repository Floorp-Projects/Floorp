/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *   Brian Birtles <birtles@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#include "nsIDOMSVGMatrix.h"
#include "DOMSVGTransform.h"
#include "gfxMatrix.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"

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
class DOMSVGMatrix : public nsIDOMSVGMatrix
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

  PRBool IsAnimVal() const {
    return mTransform ? mTransform->IsAnimVal() : PR_FALSE;
  }

  nsRefPtr<DOMSVGTransform> mTransform;

  // Typically we operate on the matrix data accessed via mTransform but for
  // matrices that exist independently of an SVGTransform we use mMatrix below.
  gfxMatrix mMatrix;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMSVGMatrix, MOZILLA_DOMSVGMATRIX_IID)

} // namespace mozilla

#endif // MOZILLA_DOMSVGMATRIX_H__
