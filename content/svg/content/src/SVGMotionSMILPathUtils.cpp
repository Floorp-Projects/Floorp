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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
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

#include "SVGMotionSMILPathUtils.h"
#include "nsSVGElement.h"
#include "nsSVGLength2.h"
#include "nsContentCreatorFunctions.h" // For NS_NewSVGElement


namespace mozilla {

//----------------------------------------------------------------------
// PathGenerator methods

// For the dummy 'from' value used in pure by-animation & to-animation
void
SVGMotionSMILPathUtils::PathGenerator::
  MoveToOrigin()
{
  NS_ABORT_IF_FALSE(!mHaveReceivedCommands,
                    "Not expecting requests for mid-path MoveTo commands");
  mHaveReceivedCommands = PR_TRUE;
  mGfxContext.MoveTo(gfxPoint(0, 0));
}

// For 'from' and the first entry in 'values'.
PRBool
SVGMotionSMILPathUtils::PathGenerator::
  MoveToAbsolute(const nsAString& aCoordPairStr)
{
  NS_ABORT_IF_FALSE(!mHaveReceivedCommands,
                    "Not expecting requests for mid-path MoveTo commands");
  mHaveReceivedCommands = PR_TRUE;

  float xVal, yVal;
  if (!ParseCoordinatePair(aCoordPairStr, xVal, yVal)) {
    return PR_FALSE;
  }
  mGfxContext.MoveTo(gfxPoint(xVal, yVal));
  return PR_TRUE;
}

// For 'to' and every entry in 'values' except the first.
PRBool
SVGMotionSMILPathUtils::PathGenerator::
  LineToAbsolute(const nsAString& aCoordPairStr, double& aSegmentDistance)
{
  mHaveReceivedCommands = PR_TRUE;

  float xVal, yVal;
  if (!ParseCoordinatePair(aCoordPairStr, xVal, yVal)) {
    return PR_FALSE;
  }
  gfxPoint initialPoint = mGfxContext.CurrentPoint();

  mGfxContext.LineTo(gfxPoint(xVal, yVal));
  aSegmentDistance = NS_hypot(initialPoint.x - xVal, initialPoint.y -yVal);
  return PR_TRUE;
}

// For 'by'.
PRBool
SVGMotionSMILPathUtils::PathGenerator::
  LineToRelative(const nsAString& aCoordPairStr, double& aSegmentDistance)
{
  mHaveReceivedCommands = PR_TRUE;

  float xVal, yVal;
  if (!ParseCoordinatePair(aCoordPairStr, xVal, yVal)) {
    return PR_FALSE;
  }
  mGfxContext.LineTo(mGfxContext.CurrentPoint() + gfxPoint(xVal, yVal));
  aSegmentDistance = NS_hypot(xVal, yVal);
  return PR_TRUE;
}

already_AddRefed<gfxFlattenedPath>
SVGMotionSMILPathUtils::PathGenerator::GetResultingPath()
{
  return mGfxContext.GetFlattenedPath();
}

//----------------------------------------------------------------------
// Helper / protected methods

static PRBool
ParseOneCoordinate(char** aRest, nsSVGLength2& aLengthVal)
{
  aLengthVal.Init();

  // Grab token, up to next delimiter
  // XXXdholbert Ideally we'd like to know if the delimeter we found was a
  // comma (and if so, fail if we come across any more commas before the next
  // value). Current behavior will accept mutliple commas in between values,
  // and that's not technically spec-correct, but it's simpler and it matches
  // our behavior elsewhere where we use strtok with SVG_COMMA_WSP_DELIM.
  char* token = nsCRT::strtok(*aRest, SVG_COMMA_WSP_DELIM, aRest);
  if (!token) {
    return PR_FALSE;
  }

  // Parse token into value + unit
  nsresult rv = aLengthVal.SetBaseValueString(NS_ConvertASCIItoUTF16(token),
                                              nsnull, PR_FALSE);
  return NS_SUCCEEDED(rv);
}

PRBool
SVGMotionSMILPathUtils::PathGenerator::
  ParseCoordinatePair(const nsAString& aCoordPairStr,
                      float& aXVal, float& aYVal)
{
  nsSVGLength2 xLength, yLength;
  
  char* str = ToNewCString(aCoordPairStr);
  char* rest = str;
  PRBool success = PR_FALSE;

  if (ParseOneCoordinate(&rest, xLength) &&
      ParseOneCoordinate(&rest, yLength)) {

    // Check for any non-whitespace characters remaining at the end.
    PRBool foundTrailingNonWhitespace = PR_FALSE;
    while (*rest != '\0') {
      if (!IsSVGWhitespace(*rest)) {
        foundTrailingNonWhitespace = PR_TRUE;
        break;
      }
    }
    if (!foundTrailingNonWhitespace) {
      success = PR_TRUE;
    }
  }
  nsMemory::Free(str);

  if (success) {
    aXVal = xLength.GetBaseValue(mSVGElement);
    aYVal = yLength.GetBaseValue(mSVGElement);
  }
  return success;
}

//----------------------------------------------------------------------
// MotionValueParser methods
nsresult
SVGMotionSMILPathUtils::MotionValueParser::
  Parse(const nsAString& aValueStr)
{
  PRBool success;
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
  return success ? NS_OK : NS_ERROR_FAILURE;
}

} // namespace mozilla
