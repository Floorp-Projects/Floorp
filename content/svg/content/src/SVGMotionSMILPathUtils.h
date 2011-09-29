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

/* Helper class to help with generating anonymous path elements for
   <animateMotion> elements to use. */

#ifndef MOZILLA_SVGMOTIONSMILPATHUTILS_H_
#define MOZILLA_SVGMOTIONSMILPATHUTILS_H_

#include "nsSMILParserUtils.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsDebug.h"
#include "gfxContext.h"
#include "nsSVGUtils.h"
#include "gfxPlatform.h"

class nsSVGElement;
class nsIContent;
class nsIDocument;
class nsAString;

namespace mozilla {

class SVGMotionSMILPathUtils {
public:
  // Class to assist in generating a gfxFlattenedPath, based on
  // coordinates in the <animateMotion> from/by/to/values attributes.
  class PathGenerator {
  public:
    PathGenerator(const nsSVGElement* aSVGElement)
      : mSVGElement(aSVGElement),
        mGfxContext(gfxPlatform::GetPlatform()->ScreenReferenceSurface()),
        mHaveReceivedCommands(PR_FALSE)
    {}

    // Methods for adding various path commands to output path.
    // Note: aCoordPairStr is expected to be a whitespace and/or
    // comma-separated x,y coordinate-pair -- see description of
    // "the specified values for from, by, to, and values" at
    //    http://www.w3.org/TR/SVG11/animate.html#AnimateMotionElement
    void   MoveToOrigin();
    bool MoveToAbsolute(const nsAString& aCoordPairStr);
    bool LineToAbsolute(const nsAString& aCoordPairStr,
                          double& aSegmentDistance);
    bool LineToRelative(const nsAString& aCoordPairStr,
                          double& aSegmentDistance);

    // Accessor to let clients check if we've received any commands yet.
    inline bool HaveReceivedCommands() { return mHaveReceivedCommands; }
    // Accessor to get the finalized path
    already_AddRefed<gfxFlattenedPath> GetResultingPath();

  protected:
    // Helper methods
    bool ParseCoordinatePair(const nsAString& aStr,
                               float& aXVal, float& aYVal);

    // Member data
    const nsSVGElement* mSVGElement; // context for converting to user units
    gfxContext    mGfxContext;
    bool          mHaveReceivedCommands;
  };

  // Class to assist in passing each subcomponent of a |values| attribute to
  // a PathGenerator, for generating a corresponding gfxFlattenedPath.
  class MotionValueParser : public nsSMILParserUtils::GenericValueParser
  {
  public:
    MotionValueParser(PathGenerator* aPathGenerator,
                      nsTArray<double>* aPointDistances)
      : mPathGenerator(aPathGenerator),
        mPointDistances(aPointDistances),
        mDistanceSoFar(0.0)
    {
      NS_ABORT_IF_FALSE(mPointDistances->IsEmpty(),
                        "expecting point distances array to start empty");
    }

    // nsSMILParserUtils::GenericValueParser interface
    virtual nsresult Parse(const nsAString& aValueStr);

  protected:
    PathGenerator*    mPathGenerator;
    nsTArray<double>* mPointDistances;
    double            mDistanceSoFar;
  };

};

} // namespace mozilla

#endif // MOZILLA_SVGMOTIONSMILPATHUTILS_H_
