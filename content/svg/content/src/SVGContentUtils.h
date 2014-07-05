/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGCONTENTUTILS_H
#define MOZILLA_SVGCONTENTUTILS_H

// include math.h to pick up definition of M_ maths defines e.g. M_PI
#define _USE_MATH_DEFINES
#include <math.h>

#include "mozilla/gfx/Matrix.h"
#include "mozilla/RangedPtr.h"
#include "nsError.h"
#include "nsStringFwd.h"
#include "gfx2DGlue.h"

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

namespace gfx {
class Matrix;
} // namespace gfx
} // namespace mozilla

inline bool
IsSVGWhitespace(char aChar)
{
  return aChar == '\x20' || aChar == '\x9' ||
         aChar == '\xD'  || aChar == '\xA';
}

inline bool
IsSVGWhitespace(char16_t aChar)
{
  return aChar == char16_t('\x20') || aChar == char16_t('\x9') ||
         aChar == char16_t('\xD')  || aChar == char16_t('\xA');
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
                                  const char16_t **aParams,
                                  uint32_t aParamsLength);

  static mozilla::gfx::Matrix GetCTM(nsSVGElement *aElement, bool aScreenCTM);

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

  static mozilla::gfx::Matrix
  GetViewBoxTransform(float aViewportWidth, float aViewportHeight,
                      float aViewboxX, float aViewboxY,
                      float aViewboxWidth, float aViewboxHeight,
                      const SVGAnimatedPreserveAspectRatio &aPreserveAspectRatio);

  static mozilla::gfx::Matrix
  GetViewBoxTransform(float aViewportWidth, float aViewportHeight,
                      float aViewboxX, float aViewboxY,
                      float aViewboxWidth, float aViewboxHeight,
                      const SVGPreserveAspectRatio &aPreserveAspectRatio);

  static mozilla::RangedPtr<const char16_t>
  GetStartRangedPtr(const nsAString& aString);

  static mozilla::RangedPtr<const char16_t>
  GetEndRangedPtr(const nsAString& aString);

  /**
   * True if 'aCh' is a decimal digit.
   */
  static inline bool IsDigit(char16_t aCh)
  {
    return aCh >= '0' && aCh <= '9';
  }

 /**
  * Assuming that 'aCh' is a decimal digit, return its numeric value.
  */
  static inline uint32_t DecimalDigitValue(char16_t aCh)
  {
    MOZ_ASSERT(IsDigit(aCh), "Digit expected");
    return aCh - '0';
  }

  /**
   * Parses the sign (+ or -) of a number and moves aIter to the next
   * character if a sign is found.
   * @param aSignMultiplier [outparam] -1 if the sign is negative otherwise 1
   * @return false if we hit the end of the string (i.e. if aIter is initially
   *         at aEnd, or if we reach aEnd right after the sign character).
   */
  static inline bool
  ParseOptionalSign(mozilla::RangedPtr<const char16_t>& aIter,
                    const mozilla::RangedPtr<const char16_t>& aEnd,
                    int32_t& aSignMultiplier)
  {
    if (aIter == aEnd) {
      return false;
    }
    aSignMultiplier = *aIter == '-' ? -1 : 1;

    mozilla::RangedPtr<const char16_t> iter(aIter);

    if (*iter == '-' || *iter == '+') {
      ++iter;
      if (iter == aEnd) {
        return false;
      }
    }
    aIter = iter;
    return true;
  }

  /**
   * Parse a number of the form:
   * number ::= integer ([Ee] integer)? | [+-]? [0-9]* "." [0-9]+ ([Ee] integer)?
   * Parsing fails if the number cannot be represented by a floatType.
   * If parsing succeeds, aIter is updated so that it points to the character
   * after the end of the number, otherwise it is left unchanged
   */
  template<class floatType>
  static bool
  ParseNumber(mozilla::RangedPtr<const char16_t>& aIter,
              const mozilla::RangedPtr<const char16_t>& aEnd,
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
   * The returned number is clamped to an int32_t if outside that range.
   * If parsing succeeds, aIter is updated so that it points to the character
   * after the end of the number, otherwise it is left unchanged
   */
  static bool ParseInteger(mozilla::RangedPtr<const char16_t>& aIter,
                           const mozilla::RangedPtr<const char16_t>& aEnd,
                           int32_t& aValue);

  /**
   * Parse an integer of the form:
   * integer ::= [+-]? [0-9]+
   * The returned number is clamped to an int32_t if outside that range.
   * Parsing fails if there is anything left over after the number.
   */
  static bool ParseInteger(const nsAString& aString, int32_t& aValue);

  /**
   * Converts an nsStyleCoord into a userspace value.  Handles units
   * Factor (straight userspace), Coord (dimensioned), and Percent (of
   * aContent's SVG viewport)
   */
  static float CoordToFloat(nsSVGElement *aContent,
                            const nsStyleCoord &aCoord);
  /**
   * Parse the SVG path string
   * Returns a path
   * string formatted as an SVG path
   */
  static mozilla::TemporaryRef<mozilla::gfx::Path>
  GetPath(const nsAString& aPathString);
};

#endif
