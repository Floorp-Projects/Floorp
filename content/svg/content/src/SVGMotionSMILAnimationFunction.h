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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>
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

#ifndef MOZILLA_SVGMOTIONSMILANIMATIONFUNCTION_H_
#define MOZILLA_SVGMOTIONSMILANIMATIONFUNCTION_H_

#include "nsSMILAnimationFunction.h"
#include "SVGMotionSMILType.h" // for RotateType
#include "gfxPath.h"  // for gfxFlattenedPath

class nsSVGMpathElement;

namespace mozilla {

//----------------------------------------------------------------------
// SVGMotionSMILAnimationFunction
//
// Subclass of nsSMILAnimationFunction to support a few extra features offered
// by the <animateMotion> element.
//
class SVGMotionSMILAnimationFunction : public nsSMILAnimationFunction
{
public:
  SVGMotionSMILAnimationFunction();
  NS_OVERRIDE virtual PRBool SetAttr(nsIAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsAttrValue& aResult,
                                     nsresult* aParseResult = nsnull);
  NS_OVERRIDE virtual PRBool UnsetAttr(nsIAtom* aAttribute);

  // Method to allow our owner-element to signal us when our <mpath>
  // has changed or been added/removed.  When that happens, we need to
  // mark ourselves as changed so we'll get recomposed, and mark our path data
  // as stale so it'll get regenerated (regardless of mPathSourceType, since
  // <mpath> trumps all the other sources of path data)
  void MpathChanged() { mIsPathStale = mHasChanged = PR_TRUE; }

protected:
  enum PathSourceType {
    // NOTE: Ordering matters here. Higher-priority path-descriptors should
    // have higher enumerated values
    ePathSourceType_None,      // uninitialized or not applicable
    ePathSourceType_ByAttr,    // by or from-by animation
    ePathSourceType_ToAttr,    // to or from-to animation
    ePathSourceType_ValuesAttr,
    ePathSourceType_PathAttr,
    ePathSourceType_Mpath
  };

  NS_OVERRIDE virtual nsSMILCalcMode GetCalcMode() const;
  NS_OVERRIDE virtual nsresult GetValues(const nsISMILAttr& aSMILAttr,
                                         nsSMILValueArray& aResult);
  NS_OVERRIDE virtual void CheckValueListDependentAttrs(PRUint32 aNumValues);

  NS_OVERRIDE virtual PRBool IsToAnimation() const;

  void     CheckKeyPoints();
  nsresult SetKeyPoints(const nsAString& aKeyPoints, nsAttrValue& aResult);
  void     UnsetKeyPoints();
  nsresult SetRotate(const nsAString& aRotate, nsAttrValue& aResult);
  void     UnsetRotate();

  // Helpers for GetValues
  void     MarkStaleIfAttributeAffectsPath(nsIAtom* aAttribute);
  void     RebuildPathAndVertices(const nsIContent* aContextElem);
  void     RebuildPathAndVerticesFromMpathElem(nsSVGMpathElement* aMpathElem);
  void     RebuildPathAndVerticesFromPathAttr();
  void     RebuildPathAndVerticesFromBasicAttrs(const nsIContent* aContextElem);
  PRBool   GenerateValuesForPathAndPoints(gfxFlattenedPath* aPath,
                                          PRBool aIsKeyPoints,
                                          nsTArray<double>& aPointDistances,
                                          nsTArray<nsSMILValue>& aResult);

  // Members
  // -------
  nsTArray<double>           mKeyPoints; // parsed from "keyPoints" attribute.

  RotateType                 mRotateType;  // auto, auto-reverse, or explicit.
  float                      mRotateAngle; // the angle value, if explicit.

  PathSourceType             mPathSourceType; // source of our gfxFlattenedPath.
  nsRefPtr<gfxFlattenedPath> mPath;           // representation of motion path.
  nsTArray<double>           mPathVertices; // distances of vertices along path.

  PRPackedBool               mIsPathStale;
};

} // namespace mozilla

#endif // MOZILLA_SVGMOTIONSMILANIMATIONFUNCTION_H_
