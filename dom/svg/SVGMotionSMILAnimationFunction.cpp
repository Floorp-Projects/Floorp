/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGMotionSMILAnimationFunction.h"

#include "mozilla/dom/SVGAnimationElement.h"
#include "mozilla/dom/SVGPathElement.h"
#include "mozilla/dom/SVGMPathElement.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/SMILParserUtils.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "SVGAnimatedOrient.h"
#include "SVGMotionSMILPathUtils.h"
#include "SVGMotionSMILType.h"
#include "SVGPathDataParser.h"

using namespace mozilla::dom;
using namespace mozilla::dom::SVGAngle_Binding;
using namespace mozilla::gfx;

namespace mozilla {

SVGMotionSMILAnimationFunction::SVGMotionSMILAnimationFunction()
    : mRotateType(eRotateType_Explicit),
      mRotateAngle(0.0f),
      mPathSourceType(ePathSourceType_None),
      mIsPathStale(true)  // Try to initialize path on first GetValues call
{}

void SVGMotionSMILAnimationFunction::MarkStaleIfAttributeAffectsPath(
    nsAtom* aAttribute) {
  bool isAffected;
  if (aAttribute == nsGkAtoms::path) {
    isAffected = (mPathSourceType <= ePathSourceType_PathAttr);
  } else if (aAttribute == nsGkAtoms::values) {
    isAffected = (mPathSourceType <= ePathSourceType_ValuesAttr);
  } else if (aAttribute == nsGkAtoms::from || aAttribute == nsGkAtoms::to) {
    isAffected = (mPathSourceType <= ePathSourceType_ToAttr);
  } else if (aAttribute == nsGkAtoms::by) {
    isAffected = (mPathSourceType <= ePathSourceType_ByAttr);
  } else {
    MOZ_ASSERT_UNREACHABLE(
        "Should only call this method for path-describing "
        "attrs");
    isAffected = false;
  }

  if (isAffected) {
    mIsPathStale = true;
    mHasChanged = true;
  }
}

bool SVGMotionSMILAnimationFunction::SetAttr(nsAtom* aAttribute,
                                             const nsAString& aValue,
                                             nsAttrValue& aResult,
                                             nsresult* aParseResult) {
  // Handle motion-specific attrs
  if (aAttribute == nsGkAtoms::keyPoints) {
    nsresult rv = SetKeyPoints(aValue, aResult);
    if (aParseResult) {
      *aParseResult = rv;
    }
  } else if (aAttribute == nsGkAtoms::rotate) {
    nsresult rv = SetRotate(aValue, aResult);
    if (aParseResult) {
      *aParseResult = rv;
    }
  } else if (aAttribute == nsGkAtoms::path || aAttribute == nsGkAtoms::by ||
             aAttribute == nsGkAtoms::from || aAttribute == nsGkAtoms::to ||
             aAttribute == nsGkAtoms::values) {
    aResult.SetTo(aValue);
    MarkStaleIfAttributeAffectsPath(aAttribute);
    if (aParseResult) {
      *aParseResult = NS_OK;
    }
  } else {
    // Defer to superclass method
    return SMILAnimationFunction::SetAttr(aAttribute, aValue, aResult,
                                          aParseResult);
  }

  return true;
}

bool SVGMotionSMILAnimationFunction::UnsetAttr(nsAtom* aAttribute) {
  if (aAttribute == nsGkAtoms::keyPoints) {
    UnsetKeyPoints();
  } else if (aAttribute == nsGkAtoms::rotate) {
    UnsetRotate();
  } else if (aAttribute == nsGkAtoms::path || aAttribute == nsGkAtoms::by ||
             aAttribute == nsGkAtoms::from || aAttribute == nsGkAtoms::to ||
             aAttribute == nsGkAtoms::values) {
    MarkStaleIfAttributeAffectsPath(aAttribute);
  } else {
    // Defer to superclass method
    return SMILAnimationFunction::UnsetAttr(aAttribute);
  }

  return true;
}

SMILAnimationFunction::SMILCalcMode
SVGMotionSMILAnimationFunction::GetCalcMode() const {
  const nsAttrValue* value = GetAttr(nsGkAtoms::calcMode);
  if (!value) {
    return CALC_PACED;  // animateMotion defaults to calcMode="paced"
  }

  return SMILCalcMode(value->GetEnumValue());
}

//----------------------------------------------------------------------
// Helpers for GetValues

/*
 * Returns the first <mpath> child of the given element
 */

static SVGMPathElement* GetFirstMPathChild(nsIContent* aElem) {
  for (nsIContent* child = aElem->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsSVGElement(nsGkAtoms::mpath)) {
      return static_cast<SVGMPathElement*>(child);
    }
  }

  return nullptr;
}

void SVGMotionSMILAnimationFunction::RebuildPathAndVerticesFromBasicAttrs(
    const nsIContent* aContextElem) {
  MOZ_ASSERT(!HasAttr(nsGkAtoms::path),
             "Should be using |path| attr if we have it");
  MOZ_ASSERT(!mPath, "regenerating when we already have path");
  MOZ_ASSERT(mPathVertices.IsEmpty(),
             "regenerating when we already have vertices");

  if (!aContextElem->IsSVGElement()) {
    NS_ERROR("Uh oh, SVG animateMotion element targeting a non-SVG node");
    return;
  }

  SVGMotionSMILPathUtils::PathGenerator pathGenerator(
      static_cast<const SVGElement*>(aContextElem));

  bool success = false;
  if (HasAttr(nsGkAtoms::values)) {
    // Generate path based on our values array
    mPathSourceType = ePathSourceType_ValuesAttr;
    const nsAString& valuesStr = GetAttr(nsGkAtoms::values)->GetStringValue();
    SVGMotionSMILPathUtils::MotionValueParser parser(&pathGenerator,
                                                     &mPathVertices);
    success = SMILParserUtils::ParseValuesGeneric(valuesStr, parser);
  } else if (HasAttr(nsGkAtoms::to) || HasAttr(nsGkAtoms::by)) {
    // Apply 'from' value (or a dummy 0,0 'from' value)
    if (HasAttr(nsGkAtoms::from)) {
      const nsAString& fromStr = GetAttr(nsGkAtoms::from)->GetStringValue();
      success = pathGenerator.MoveToAbsolute(fromStr);
      if (!mPathVertices.AppendElement(0.0, fallible)) {
        success = false;
      }
    } else {
      // Create dummy 'from' value at 0,0, if we're doing by-animation.
      // (NOTE: We don't add the dummy 0-point to our list for *to-animation*,
      // because the SMILAnimationFunction logic for to-animation doesn't
      // expect a dummy value. It only expects one value: the final 'to' value.)
      pathGenerator.MoveToOrigin();
      success = true;
      if (!HasAttr(nsGkAtoms::to)) {
        if (!mPathVertices.AppendElement(0.0, fallible)) {
          success = false;
        }
      }
    }

    // Apply 'to' or 'by' value
    if (success) {
      double dist;
      if (HasAttr(nsGkAtoms::to)) {
        mPathSourceType = ePathSourceType_ToAttr;
        const nsAString& toStr = GetAttr(nsGkAtoms::to)->GetStringValue();
        success = pathGenerator.LineToAbsolute(toStr, dist);
      } else {  // HasAttr(nsGkAtoms::by)
        mPathSourceType = ePathSourceType_ByAttr;
        const nsAString& byStr = GetAttr(nsGkAtoms::by)->GetStringValue();
        success = pathGenerator.LineToRelative(byStr, dist);
      }
      if (success) {
        if (!mPathVertices.AppendElement(dist, fallible)) {
          success = false;
        }
      }
    }
  }
  if (success) {
    mPath = pathGenerator.GetResultingPath();
  } else {
    // Parse failure. Leave path as null, and clear path-related member data.
    mPathVertices.Clear();
  }
}

void SVGMotionSMILAnimationFunction::RebuildPathAndVerticesFromMpathElem(
    SVGMPathElement* aMpathElem) {
  mPathSourceType = ePathSourceType_Mpath;

  // Use the shape that's the target of our chosen <mpath> child.
  SVGGeometryElement* shapeElem = aMpathElem->GetReferencedPath();
  if (shapeElem && shapeElem->HasValidDimensions()) {
    bool ok = shapeElem->GetDistancesFromOriginToEndsOfVisibleSegments(
        &mPathVertices);
    if (ok && mPathVertices.Length()) {
      mPath = shapeElem->GetOrBuildPathForMeasuring();
    }
  }
}

void SVGMotionSMILAnimationFunction::RebuildPathAndVerticesFromPathAttr() {
  const nsAString& pathSpec = GetAttr(nsGkAtoms::path)->GetStringValue();
  mPathSourceType = ePathSourceType_PathAttr;

  // Generate Path from |path| attr
  SVGPathData path;
  SVGPathDataParser pathParser(pathSpec, &path);

  // We ignore any failure returned from Parse() since the SVG spec says to
  // accept all segments up to the first invalid token. Instead we must
  // explicitly check that the parse produces at least one path segment (if
  // the path data doesn't begin with a valid "M", then it's invalid).
  pathParser.Parse();
  if (!path.Length()) {
    return;
  }

  mPath = path.BuildPathForMeasuring();
  bool ok = path.GetDistancesFromOriginToEndsOfVisibleSegments(&mPathVertices);
  if (!ok || !mPathVertices.Length()) {
    mPath = nullptr;
    mPathVertices.Clear();
  }
}

// Helper to regenerate our path representation & its list of vertices
void SVGMotionSMILAnimationFunction::RebuildPathAndVertices(
    const nsIContent* aTargetElement) {
  MOZ_ASSERT(mIsPathStale, "rebuilding path when it isn't stale");

  // Clear stale data
  mPath = nullptr;
  mPathVertices.Clear();
  mPathSourceType = ePathSourceType_None;

  // Do we have a mpath child? if so, it trumps everything. Otherwise, we look
  // through our list of path-defining attributes, in order of priority.
  SVGMPathElement* firstMpathChild = GetFirstMPathChild(mAnimationElement);

  if (firstMpathChild) {
    RebuildPathAndVerticesFromMpathElem(firstMpathChild);
    mValueNeedsReparsingEverySample = false;
  } else if (HasAttr(nsGkAtoms::path)) {
    RebuildPathAndVerticesFromPathAttr();
    mValueNeedsReparsingEverySample = false;
  } else {
    // Get path & vertices from basic SMIL attrs: from/by/to/values

    RebuildPathAndVerticesFromBasicAttrs(aTargetElement);
    mValueNeedsReparsingEverySample = true;
  }
  mIsPathStale = false;
}

bool SVGMotionSMILAnimationFunction::GenerateValuesForPathAndPoints(
    Path* aPath, bool aIsKeyPoints, FallibleTArray<double>& aPointDistances,
    SMILValueArray& aResult) {
  MOZ_ASSERT(aResult.IsEmpty(), "outparam is non-empty");

  // If we're using "keyPoints" as our list of input distances, then we need
  // to de-normalize from the [0, 1] scale to the [0, totalPathLen] scale.
  double distanceMultiplier = aIsKeyPoints ? aPath->ComputeLength() : 1.0;
  const uint32_t numPoints = aPointDistances.Length();
  for (uint32_t i = 0; i < numPoints; ++i) {
    double curDist = aPointDistances[i] * distanceMultiplier;
    if (!aResult.AppendElement(SVGMotionSMILType::ConstructSMILValue(
                                   aPath, curDist, mRotateType, mRotateAngle),
                               fallible)) {
      return false;
    }
  }
  return true;
}

nsresult SVGMotionSMILAnimationFunction::GetValues(const SMILAttr& aSMILAttr,
                                                   SMILValueArray& aResult) {
  if (mIsPathStale) {
    RebuildPathAndVertices(aSMILAttr.GetTargetNode());
  }
  MOZ_ASSERT(!mIsPathStale, "Forgot to clear 'is path stale' state");

  if (!mPath) {
    // This could be due to e.g. a parse error.
    MOZ_ASSERT(mPathVertices.IsEmpty(), "have vertices but no path");
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(!mPathVertices.IsEmpty(), "have a path but no vertices");

  // Now: Make the actual list of SMILValues (using keyPoints, if set)
  bool isUsingKeyPoints = !mKeyPoints.IsEmpty();
  bool success = GenerateValuesForPathAndPoints(
      mPath, isUsingKeyPoints, isUsingKeyPoints ? mKeyPoints : mPathVertices,
      aResult);
  if (!success) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void SVGMotionSMILAnimationFunction::CheckValueListDependentAttrs(
    uint32_t aNumValues) {
  // Call superclass method.
  SMILAnimationFunction::CheckValueListDependentAttrs(aNumValues);

  // Added behavior: Do checks specific to keyPoints.
  CheckKeyPoints();
}

bool SVGMotionSMILAnimationFunction::IsToAnimation() const {
  // Rely on inherited method, but not if we have an <mpath> child or a |path|
  // attribute, because they'll override any 'to' attr we might have.
  // NOTE: We can't rely on mPathSourceType, because it might not have been
  // set to a useful value yet (or it might be stale).
  return !GetFirstMPathChild(mAnimationElement) && !HasAttr(nsGkAtoms::path) &&
         SMILAnimationFunction::IsToAnimation();
}

void SVGMotionSMILAnimationFunction::CheckKeyPoints() {
  if (!HasAttr(nsGkAtoms::keyPoints)) return;

  // attribute is ignored for calcMode="paced" (even if it's got errors)
  if (GetCalcMode() == CALC_PACED) {
    SetKeyPointsErrorFlag(false);
  }

  if (mKeyPoints.Length() != mKeyTimes.Length()) {
    // there must be exactly as many keyPoints as keyTimes
    SetKeyPointsErrorFlag(true);
    return;
  }

  // Nothing else to check -- we can catch all keyPoints errors elsewhere.
  // -  Formatting & range issues will be caught in SetKeyPoints, and will
  //  result in an empty mKeyPoints array, which will drop us into the error
  //  case above.
}

nsresult SVGMotionSMILAnimationFunction::SetKeyPoints(
    const nsAString& aKeyPoints, nsAttrValue& aResult) {
  mKeyPoints.Clear();
  aResult.SetTo(aKeyPoints);

  mHasChanged = true;

  if (!SMILParserUtils::ParseSemicolonDelimitedProgressList(aKeyPoints, false,
                                                            mKeyPoints)) {
    mKeyPoints.Clear();
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void SVGMotionSMILAnimationFunction::UnsetKeyPoints() {
  mKeyPoints.Clear();
  SetKeyPointsErrorFlag(false);
  mHasChanged = true;
}

nsresult SVGMotionSMILAnimationFunction::SetRotate(const nsAString& aRotate,
                                                   nsAttrValue& aResult) {
  mHasChanged = true;

  aResult.SetTo(aRotate);
  if (aRotate.EqualsLiteral("auto")) {
    mRotateType = eRotateType_Auto;
  } else if (aRotate.EqualsLiteral("auto-reverse")) {
    mRotateType = eRotateType_AutoReverse;
  } else {
    mRotateType = eRotateType_Explicit;

    uint16_t angleUnit;
    if (!SVGAnimatedOrient::GetValueFromString(aRotate, mRotateAngle,
                                               &angleUnit)) {
      mRotateAngle = 0.0f;  // set default rotate angle
      // XXX report to console?
      return NS_ERROR_DOM_SYNTAX_ERR;
    }

    // Convert to radian units, if we're not already in radians.
    if (angleUnit != SVG_ANGLETYPE_RAD) {
      mRotateAngle *= SVGAnimatedOrient::GetDegreesPerUnit(angleUnit) /
                      SVGAnimatedOrient::GetDegreesPerUnit(SVG_ANGLETYPE_RAD);
    }
  }
  return NS_OK;
}

void SVGMotionSMILAnimationFunction::UnsetRotate() {
  mRotateAngle = 0.0f;  // default value
  mRotateType = eRotateType_Explicit;
  mHasChanged = true;
}

}  // namespace mozilla
