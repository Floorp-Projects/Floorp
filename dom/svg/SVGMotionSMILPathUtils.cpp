/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGMotionSMILPathUtils.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h" // for NS_ENSURE_FINITE2
#include "SVGContentUtils.h"
#include "SVGLength.h"

using namespace mozilla::gfx;

namespace mozilla {

//----------------------------------------------------------------------
// PathGenerator methods

// For the dummy 'from' value used in pure by-animation & to-animation
void
SVGMotionSMILPathUtils::PathGenerator::
  MoveToOrigin()
{
  MOZ_ASSERT(!mHaveReceivedCommands,
             "Not expecting requests for mid-path MoveTo commands");
  mHaveReceivedCommands = true;
  mPathBuilder->MoveTo(Point(0, 0));
}

// For 'from' and the first entry in 'values'.
bool
SVGMotionSMILPathUtils::PathGenerator::
  MoveToAbsolute(const nsAString& aCoordPairStr)
{
  MOZ_ASSERT(!mHaveReceivedCommands,
             "Not expecting requests for mid-path MoveTo commands");
  mHaveReceivedCommands = true;

  float xVal, yVal;
  if (!ParseCoordinatePair(aCoordPairStr, xVal, yVal)) {
    return false;
  }
  mPathBuilder->MoveTo(Point(xVal, yVal));
  return true;
}

// For 'to' and every entry in 'values' except the first.
bool
SVGMotionSMILPathUtils::PathGenerator::
  LineToAbsolute(const nsAString& aCoordPairStr, double& aSegmentDistance)
{
  mHaveReceivedCommands = true;

  float xVal, yVal;
  if (!ParseCoordinatePair(aCoordPairStr, xVal, yVal)) {
    return false;
  }
  Point initialPoint = mPathBuilder->CurrentPoint();

  mPathBuilder->LineTo(Point(xVal, yVal));
  aSegmentDistance = NS_hypot(initialPoint.x - xVal, initialPoint.y -yVal);
  return true;
}

// For 'by'.
bool
SVGMotionSMILPathUtils::PathGenerator::
  LineToRelative(const nsAString& aCoordPairStr, double& aSegmentDistance)
{
  mHaveReceivedCommands = true;

  float xVal, yVal;
  if (!ParseCoordinatePair(aCoordPairStr, xVal, yVal)) {
    return false;
  }
  mPathBuilder->LineTo(mPathBuilder->CurrentPoint() + Point(xVal, yVal));
  aSegmentDistance = NS_hypot(xVal, yVal);
  return true;
}

TemporaryRef<Path>
SVGMotionSMILPathUtils::PathGenerator::GetResultingPath()
{
  return mPathBuilder->Finish();
}

//----------------------------------------------------------------------
// Helper / protected methods

bool
SVGMotionSMILPathUtils::PathGenerator::
  ParseCoordinatePair(const nsAString& aCoordPairStr,
                      float& aXVal, float& aYVal)
{
  nsCharSeparatedTokenizerTemplate<IsSVGWhitespace>
    tokenizer(aCoordPairStr, ',',
              nsCharSeparatedTokenizer::SEPARATOR_OPTIONAL);

  SVGLength x, y;

  if (!tokenizer.hasMoreTokens() ||
      !x.SetValueFromString(tokenizer.nextToken())) {
    return false;
  }

  if (!tokenizer.hasMoreTokens() ||
      !y.SetValueFromString(tokenizer.nextToken())) { 
    return false;
  }

  if (tokenizer.separatorAfterCurrentToken() ||  // Trailing comma.
      tokenizer.hasMoreTokens()) {               // More text remains
    return false;
  }

  float xRes = x.GetValueInUserUnits(mSVGElement, SVGContentUtils::X);
  float yRes = y.GetValueInUserUnits(mSVGElement, SVGContentUtils::Y);

  NS_ENSURE_FINITE2(xRes, yRes, false);

  aXVal = xRes;
  aYVal = yRes;
  return true;
}

//----------------------------------------------------------------------
// MotionValueParser methods
bool
SVGMotionSMILPathUtils::MotionValueParser::
  Parse(const nsAString& aValueStr)
{
  bool success;
  if (!mPathGenerator->HaveReceivedCommands()) {
    // Interpret first value in "values" attribute as the path's initial MoveTo
    success = mPathGenerator->MoveToAbsolute(aValueStr);
    if (success) {
      success = !!mPointDistances->AppendElement(0.0);
    }
  } else {
    double dist;
    success = mPathGenerator->LineToAbsolute(aValueStr, dist);
    if (success) {
      mDistanceSoFar += dist;
      success = !!mPointDistances->AppendElement(mDistanceSoFar);
    }
  }
  return success;
}

} // namespace mozilla
