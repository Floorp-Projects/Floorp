/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGCONTENTUTILS_H
#define MOZILLA_SVGCONTENTUTILS_H

// include math.h to pick up definition of M_ maths defines e.g. M_PI
#define _USE_MATH_DEFINES
#include <math.h>

#include "gfxMatrix.h"
#include "mozilla/RangedPtr.h"

class nsIContent;
class nsIDocument;
class nsIFrame;
class nsPresContext;
class nsStyleContext;
class nsStyleCoord;
class nsSVGElement;

namespace mozilla {
class SVGAnimatedPreserveAspectRatio;
class SVGPreserveAspectRatio;
namespace dom {
class Element;
class SVGSVGElement;
} // namespace dom
} // namespace mozilla

inline bool
IsSVGWhitespace(char aChar)
{
  return aChar == '\x20' || aChar == '\x9' ||
         aChar == '\xD'  || aChar == '\xA';
}

inline bool
IsSVGWhitespace(PRUnichar aChar)
{
  return aChar == PRUnichar('\x20') || aChar == PRUnichar('\x9') ||
         aChar == PRUnichar('\xD')  || aChar == PRUnichar('\xA');
}

/**
 * Functions generally used by SVG Content classes. Functions here
 * should not generally depend on layout methods/classes e.g. nsSVGUtils
 */
class SVGContentUtils
{
public:
  typedef mozilla::SVGAnimatedPreserveAspectRatio SVGAnimatedPreserveAspectRatio;
  typedef mozilla::SVGPreserveAspectRatio SVGPreserveAspectRatio;

  /*
   * Get the outer SVG element of an nsIContent
   */
  static mozilla::dom::SVGSVGElement *GetOuterSVGElement(nsSVGElement *aSVGElement);

  /**
   * Activates the animation element aContent as a result of navigation to the
   * fragment identifier that identifies aContent. aContent must be an instance
   * of nsSVGAnimationElement.
   *
   * This is just a shim to allow nsSVGAnimationElement::ActivateByHyperlink to
   * be called from layout/base without adding to that directory's include paths.
   */
  static void ActivateByHyperlink(nsIContent *aContent);

  /*
   * Get the number of CSS px (user units) per em (i.e. the em-height in user
   * units) for an nsIContent
   *
   * XXX document the conditions under which these may fail, and what they
   * return in those cases.
   */
  static float GetFontSize(mozilla::dom::Element *aElement);
  static float GetFontSize(nsIFrame *aFrame);
  static float GetFontSize(nsStyleContext *aStyleContext);
  /*
   * Get the number of CSS px (user units) per ex (i.e. the x-height in user
   * units) for an nsIContent
   *
   * XXX document the conditions under which these may fail, and what they
   * return in those cases.
   */
  static float GetFontXHeight(mozilla::dom::Element *aElement);
  static float GetFontXHeight(nsIFrame *aFrame);
  static float GetFontXHeight(nsStyleContext *aStyleContext);

  /*
   * Report a localized error message to the error console.
   */
  static nsresult ReportToConsole(nsIDocument* doc,
                                  const char* aWarning,
                                  const PRUnichar **aParams,
                                  uint32_t aParamsLength);

  static gfxMatrix GetCTM(nsSVGElement *aElement, bool aScreenCTM);

  /**
   * Check if this is one of the SVG elements that SVG 1.1 Full says
   * establishes a viewport: svg, symbol, image or foreignObject.
   */
  static bool EstablishesViewport(nsIContent *aContent);

  static nsSVGElement*
  GetNearestViewportElement(nsIContent *aContent);

  /* enum for specifying coordinate direction for ObjectSpace/UserSpace */
  enum ctxDirection { X, Y, XY };

  /**
   * Computes sqrt((aWidth^2 + aHeight^2)/2);
   */
  static double ComputeNormalizedHypotenuse(double aWidth, double aHeight);

  /* Returns the angle halfway between the two specified angles */
  static float
  AngleBisect(float a1, float a2);

  /* Generate a viewbox to viewport tranformation matrix */

  static gfxMatrix
  GetViewBoxTransform(float aViewportWidth, float aViewportHeight,
                      float aViewboxX, float aViewboxY,
                      float aViewboxWidth, float aViewboxHeight,
                      const SVGAnimatedPreserveAspectRatio &aPreserveAspectRatio);

  static gfxMatrix
  GetViewBoxTransform(float aViewportWidth, float aViewportHeight,
                      float aViewboxX, float aViewboxY,
                      float aViewboxWidth, float aViewboxHeight,
                      const SVGPreserveAspectRatio &aPreserveAspectRatio);

  static mozilla::RangedPtr<const PRUnichar>
  GetStartRangedPtr(const nsAString& aString);

  static mozilla::RangedPtr<const PRUnichar>
  GetEndRangedPtr(const nsAString& aString);

  /**
   * Parse a number of the form:
   * number ::= integer ([Ee] integer)? | [+-]? [0-9]* "." [0-9]+ ([Ee] integer)?
   * Parsing fails if the number cannot be represented by a floatType.
   * If parsing succeeds, aIter is updated so that it points to the character
   * after the end of the number, otherwise it is left unchanged
   */
  template<class floatType>
  static bool
  ParseNumber(mozilla::RangedPtr<const PRUnichar>& aIter,
              const mozilla::RangedPtr<const PRUnichar>& aEnd,
              floatType& aValue);

  /**
   * Parse a number of the form:
   * number ::= integer ([Ee] integer)? | [+-]? [0-9]* "." [0-9]+ ([Ee] integer)?
   * Parsing fails if there is anything left over after the number,
   * or the number cannot be represented by a floatType.
   */
  template<class floatType>
  static bool
  ParseNumber(const nsAString& aString, floatType& aValue);

  /**
   * Parse an integer of the form:
   * integer ::= [+-]? [0-9]+
   * Parsing fails if the number cannot be represented by an int32_t.
   */
  static bool
  ParseInteger(const nsAString& aString, int32_t& aValue);

  /**
   * Converts an nsStyleCoord into a userspace value.  Handles units
   * Factor (straight userspace), Coord (dimensioned), and Percent (of
   * aContent's SVG viewport)
   */
  static float CoordToFloat(nsPresContext *aPresContext,
                            nsSVGElement *aContent,
                            const nsStyleCoord &aCoord);
};

#endif
