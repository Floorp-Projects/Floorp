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
 * A helper to generate gfxMatrixes from css transform functions.
 */
namespace nsStyleTransformMatrix {
  
  /**
   * Return the transform function, as an nsCSSKeyword, for the given
   * nsCSSValue::Array from a transform list.
   */
  nsCSSKeyword TransformFunctionOf(const nsCSSValue::Array* aData);

  /**
   * Given an nsCSSValueList containing -moz-transform functions,
   * returns a matrix containing the value of those functions.
   *
   * @param aData The nsCSSValueList containing the transform functions
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
  gfx3DMatrix ReadTransforms(const nsCSSValueList* aList,
                             nsStyleContext* aContext,
                             nsPresContext* aPresContext,
                             bool &aCanStoreInRuleTree,
                             nsRect& aBounds,
                             float aAppUnitsPerMatrixUnit);

} // namespace nsStyleTransformMatrix

#endif
