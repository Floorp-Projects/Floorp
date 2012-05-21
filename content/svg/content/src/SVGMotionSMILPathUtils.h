/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Helper class to help with generating anonymous path elements for
   <animateMotion> elements to use. */

#ifndef MOZILLA_SVGMOTIONSMILPATHUTILS_H_
#define MOZILLA_SVGMOTIONSMILPATHUTILS_H_

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsSMILParserUtils.h"
#include "nsTArray.h"

class gfxFlattenedPath;
class nsAString;
class nsSVGElement;

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
        mHaveReceivedCommands(false)
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
