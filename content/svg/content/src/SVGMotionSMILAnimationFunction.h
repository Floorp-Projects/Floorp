/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGMOTIONSMILANIMATIONFUNCTION_H_
#define MOZILLA_SVGMOTIONSMILANIMATIONFUNCTION_H_

#include "gfxPath.h"  // for gfxFlattenedPath
#include "nsAutoPtr.h"
#include "nsSMILAnimationFunction.h"
#include "nsTArray.h"
#include "SVGMotionSMILType.h"  // for RotateType

class nsAttrValue;
class nsIAtom;
class nsIContent;
class nsISMILAttr;
class nsSMILValue;
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
  virtual bool SetAttr(nsIAtom* aAttribute,
                       const nsAString& aValue,
                       nsAttrValue& aResult,
                       nsresult* aParseResult = nullptr) MOZ_OVERRIDE;
  virtual bool UnsetAttr(nsIAtom* aAttribute) MOZ_OVERRIDE;

  // Method to allow our owner-element to signal us when our <mpath>
  // has changed or been added/removed.  When that happens, we need to
  // mark ourselves as changed so we'll get recomposed, and mark our path data
  // as stale so it'll get regenerated (regardless of mPathSourceType, since
  // <mpath> trumps all the other sources of path data)
  void MpathChanged() { mIsPathStale = mHasChanged = true; }

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

  virtual nsSMILCalcMode GetCalcMode() const MOZ_OVERRIDE;
  virtual nsresult GetValues(const nsISMILAttr& aSMILAttr,
                             nsSMILValueArray& aResult) MOZ_OVERRIDE;
  virtual void CheckValueListDependentAttrs(uint32_t aNumValues) MOZ_OVERRIDE;

  virtual bool IsToAnimation() const MOZ_OVERRIDE;

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
  bool     GenerateValuesForPathAndPoints(gfxFlattenedPath* aPath,
                                          bool aIsKeyPoints,
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

  bool                       mIsPathStale;
};

} // namespace mozilla

#endif // MOZILLA_SVGMOTIONSMILANIMATIONFUNCTION_H_
