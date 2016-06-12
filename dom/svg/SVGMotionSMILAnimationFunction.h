/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGMOTIONSMILANIMATIONFUNCTION_H_
#define MOZILLA_SVGMOTIONSMILANIMATIONFUNCTION_H_

#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsSMILAnimationFunction.h"
#include "nsTArray.h"
#include "SVGMotionSMILType.h"  // for RotateType

class nsAttrValue;
class nsIAtom;
class nsIContent;
class nsISMILAttr;
class nsSMILValue;

namespace mozilla {

namespace dom {
class SVGMPathElement;
} // namespace dom

//----------------------------------------------------------------------
// SVGMotionSMILAnimationFunction
//
// Subclass of nsSMILAnimationFunction to support a few extra features offered
// by the <animateMotion> element.
//
class SVGMotionSMILAnimationFunction final : public nsSMILAnimationFunction
{
  typedef mozilla::gfx::Path Path;

public:
  SVGMotionSMILAnimationFunction();
  virtual bool SetAttr(nsIAtom* aAttribute,
                       const nsAString& aValue,
                       nsAttrValue& aResult,
                       nsresult* aParseResult = nullptr) override;
  virtual bool UnsetAttr(nsIAtom* aAttribute) override;

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

  virtual nsSMILCalcMode GetCalcMode() const override;
  virtual nsresult GetValues(const nsISMILAttr& aSMILAttr,
                             nsSMILValueArray& aResult) override;
  virtual void CheckValueListDependentAttrs(uint32_t aNumValues) override;

  virtual bool IsToAnimation() const override;

  void     CheckKeyPoints();
  nsresult SetKeyPoints(const nsAString& aKeyPoints, nsAttrValue& aResult);
  void     UnsetKeyPoints();
  nsresult SetRotate(const nsAString& aRotate, nsAttrValue& aResult);
  void     UnsetRotate();

  // Helpers for GetValues
  void     MarkStaleIfAttributeAffectsPath(nsIAtom* aAttribute);
  void     RebuildPathAndVertices(const nsIContent* aContextElem);
  void     RebuildPathAndVerticesFromMpathElem(dom::SVGMPathElement* aMpathElem);
  void     RebuildPathAndVerticesFromPathAttr();
  void     RebuildPathAndVerticesFromBasicAttrs(const nsIContent* aContextElem);
  bool     GenerateValuesForPathAndPoints(Path* aPath,
                                          bool aIsKeyPoints,
                                          FallibleTArray<double>& aPointDistances,
                                          nsSMILValueArray& aResult);

  // Members
  // -------
  FallibleTArray<double>     mKeyPoints; // parsed from "keyPoints" attribute.

  RotateType                 mRotateType;  // auto, auto-reverse, or explicit.
  float                      mRotateAngle; // the angle value, if explicit.

  PathSourceType             mPathSourceType; // source of our Path.
  RefPtr<Path>               mPath;           // representation of motion path.
  FallibleTArray<double>     mPathVertices; // distances of vertices along path.

  bool                       mIsPathStale;
};

} // namespace mozilla

#endif // MOZILLA_SVGMOTIONSMILANIMATIONFUNCTION_H_
