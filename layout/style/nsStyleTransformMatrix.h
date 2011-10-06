/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation
 *
 * Contributor(s):
 *   Keith Schwarz <kschwarz@mozilla.com> (original author)
 *   Matt Woodrow <mwoodrow@mozilla.com>
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

/*
 * A class representing three matrices that can be used for style transforms.
 */

#ifndef nsStyleTransformMatrix_h_
#define nsStyleTransformMatrix_h_

#include "nsCSSValue.h"
#include "gfxMatrix.h"
#include "gfx3DMatrix.h"
#include "nsRect.h"

struct nsCSSValueList;
class nsStyleContext;
class nsPresContext;

/**
 * A helper class to generate gfxMatrixes from css transform functions.
 */
class nsStyleTransformMatrix
{
 public:
  /**
   * Return the transform function, as an nsCSSKeyword, for the given
   * nsCSSValue::Array from a transform list.
   */
  static nsCSSKeyword TransformFunctionOf(const nsCSSValue::Array* aData);

  /**
   * Given an nsCSSValue::Array* containing a -moz-transform function,
   * returns a matrix containing the value of that function.
   *
   * @param aData The nsCSSValue::Array* containing the transform function.
   * @param aContext The style context, used for unit conversion.
   * @param aPresContext The presentation context, used for unit conversion.
   * @param aCanStoreInRuleTree Set to false if the result cannot be cached
   *                            in the rule tree, otherwise untouched.
   * @param aBounds The frame's bounding rectangle.
   * @param aAppUnitsPerMatrixUnit The number of app units per device pixel.
   *
   * aContext and aPresContext may be null if all of the (non-percent)
   * length values in aData are already known to have been converted to
   * eCSSUnit_Pixel (as they are in an nsStyleAnimation::Value)
   */
  static gfx3DMatrix MatrixForTransformFunction(const nsCSSValue::Array* aData,
                                                nsStyleContext* aContext,
                                                nsPresContext* aPresContext,
                                                PRBool& aCanStoreInRuleTree,
                                                nsRect& aBounds, 
                                                float aAppUnitsPerMatrixUnit);

  /**
   * The same as MatrixForTransformFunction, but for a list of transform
   * functions.
   */
  static gfx3DMatrix ReadTransforms(const nsCSSValueList* aList,
                                    nsStyleContext* aContext,
                                    nsPresContext* aPresContext,
                                    PRBool &aCanStoreInRuleTree,
                                    nsRect& aBounds,
                                    float aAppUnitsPerMatrixUnit);

 private:
  static gfx3DMatrix ProcessMatrix(const nsCSSValue::Array *aData,
                                   nsStyleContext *aContext,
                                   nsPresContext *aPresContext,
                                   PRBool &aCanStoreInRuleTree,
                                   nsRect& aBounds, float aAppUnitsPerMatrixUnit,
                                   PRBool *aPercentX = nsnull, 
                                   PRBool *aPercentY = nsnull);
  static gfx3DMatrix ProcessMatrix3D(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessInterpolateMatrix(const nsCSSValue::Array *aData,
                                              nsStyleContext *aContext,
                                              nsPresContext *aPresContext,
                                              PRBool &aCanStoreInRuleTree,
                                              nsRect& aBounds, float aAppUnitsPerMatrixUnit);
  static gfx3DMatrix ProcessTranslateX(const nsCSSValue::Array *aData,
                                       nsStyleContext *aContext,
                                       nsPresContext *aPresContext,
                                       PRBool &aCanStoreInRuleTree,
                                       nsRect& aBounds, float aAppUnitsPerMatrixUnit);
  static gfx3DMatrix ProcessTranslateY(const nsCSSValue::Array *aData,
                                       nsStyleContext *aContext,
                                       nsPresContext *aPresContext,
                                       PRBool &aCanStoreInRuleTree,
                                       nsRect& aBounds, float aAppUnitsPerMatrixUnit);
  static gfx3DMatrix ProcessTranslateZ(const nsCSSValue::Array *aData,
                                       nsStyleContext *aContext,
                                       nsPresContext *aPresContext,
                                       PRBool &aCanStoreInRuleTree,
                                       float aAppUnitsPerMatrixUnit);
  static gfx3DMatrix ProcessTranslate(const nsCSSValue::Array *aData,
                                      nsStyleContext *aContext,
                                      nsPresContext *aPresContext,
                                      PRBool &aCanStoreInRuleTree,
                                      nsRect& aBounds, float aAppUnitsPerMatrixUnit);
  static gfx3DMatrix ProcessTranslate3D(const nsCSSValue::Array *aData,
                                        nsStyleContext *aContext,
                                        nsPresContext *aPresContext,
                                        PRBool &aCanStoreInRuleTree,
                                        nsRect& aBounds, float aAppUnitsPerMatrixUnit);
  static gfx3DMatrix ProcessScaleHelper(float aXScale, float aYScale, float aZScale);
  static gfx3DMatrix ProcessScaleX(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessScaleY(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessScaleZ(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessScale(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessScale3D(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessSkewHelper(double aXAngle, double aYAngle);
  static gfx3DMatrix ProcessSkewX(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessSkewY(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessSkew(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessRotateX(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessRotateY(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessRotateZ(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessRotate3D(const nsCSSValue::Array *aData);
  static gfx3DMatrix ProcessPerspective(const nsCSSValue::Array *aData,
                                        nsStyleContext *aContext,
                                        nsPresContext *aPresContext,
                                        PRBool &aCanStoreInRuleTree,
                                        float aAppUnitsPerMatrixUnit);
};

#endif
