/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGMOTIONSMILANIMATIONFUNCTION_H_
#define DOM_SVG_SVGMOTIONSMILANIMATIONFUNCTION_H_

#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SMILAnimationFunction.h"
#include "SVGMotionSMILType.h"
#include "nsTArray.h"

class nsAttrValue;
class nsAtom;
class nsIContent;

namespace mozilla {

class SMILAttr;
class SMILValue;

namespace dom {
class SVGMPathElement;
}  // namespace dom

//----------------------------------------------------------------------
// SVGMotionSMILAnimationFunction
//
// Subclass of SMILAnimationFunction to support a few extra features offered
// by the <animateMotion> element.
//
class SVGMotionSMILAnimationFunction final : public SMILAnimationFunction {
  using Path = mozilla::gfx::Path;

 public:
  SVGMotionSMILAnimationFunction();
  virtual bool SetAttr(nsAtom* aAttribute, const nsAString& aValue,
                       nsAttrValue& aResult,
                       nsresult* aParseResult = nullptr) override;
  virtual bool UnsetAttr(nsAtom* aAttribute) override;

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
    ePathSourceType_None,    // uninitialized or not applicable
    ePathSourceType_ByAttr,  // by or from-by animation
    ePathSourceType_ToAttr,  // to or from-to animation
    ePathSourceType_ValuesAttr,
    ePathSourceType_PathAttr,
    ePathSourceType_Mpath
  };

  virtual SMILCalcMode GetCalcMode() const override;
  virtual nsresult GetValues(const SMILAttr& aSMILAttr,
                             SMILValueArray& aResult) override;
  virtual void CheckValueListDependentAttrs(uint32_t aNumValues) override;

  virtual bool IsToAnimation() const override;

  void CheckKeyPoints();
  nsresult SetKeyPoints(const nsAString& aKeyPoints, nsAttrValue& aResult);
  void UnsetKeyPoints();
  nsresult SetRotate(const nsAString& aRotate, nsAttrValue& aResult);
  void UnsetRotate();

  // Helpers for GetValues
  void MarkStaleIfAttributeAffectsPath(nsAtom* aAttribute);
  void RebuildPathAndVertices(const nsIContent* aTargetElement);
  void RebuildPathAndVerticesFromMpathElem(dom::SVGMPathElement* aMpathElem);
  void RebuildPathAndVerticesFromPathAttr();
  void RebuildPathAndVerticesFromBasicAttrs(const nsIContent* aContextElem);
  bool GenerateValuesForPathAndPoints(Path* aPath, bool aIsKeyPoints,
                                      FallibleTArray<double>& aPointDistances,
                                      SMILValueArray& aResult);

  // Members
  // -------
  FallibleTArray<double> mKeyPoints;  // parsed from "keyPoints" attribute.

  RotateType mRotateType;  // auto, auto-reverse, or explicit.
  float mRotateAngle;      // the angle value, if explicit.

  PathSourceType mPathSourceType;        // source of our Path.
  RefPtr<Path> mPath;                    // representation of motion path.
  FallibleTArray<double> mPathVertices;  // distances of vertices along path.

  bool mIsPathStale;
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGMOTIONSMILANIMATIONFUNCTION_H_
