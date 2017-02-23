/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGCONTENTUTILS_H
#define MOZILLA_SVGCONTENTUTILS_H

// include math.h to pick up definition of M_ maths defines e.g. M_PI
#include <math.h>

#include "mozilla/gfx/2D.h" // for StrokeOptions
#include "mozilla/gfx/Matrix.h"
#include "mozilla/RangedPtr.h"
#include "nsError.h"
#include "nsStringFwd.h"
#include "gfx2DGlue.h"

class nsIContent;
class nsIDocument;
class nsIFrame;
class nsStyleContext;
class nsStyleCoord;
class nsSVGElement;

namespace mozilla {
class nsSVGAnimatedTransformList;
class SVGAnimatedPreserveAspectRatio;
class SVGContextPaint;
class SVGPreserveAspectRatio;
namespace dom {
class Element;
class SVGSVGElement;
} // namespace dom

namespace gfx {
class Matrix;
} // namespace gfx
} // namespace mozilla

#define SVG_ZERO_LENGTH_PATH_FIX_FACTOR 512

/**
 * SVGTransformTypes controls the transforms that PrependLocalTransformsTo
 * applies.
 *
 * If aWhich is eAllTransforms, then all the transforms from the coordinate
 * space established by this element for its children to the coordinate
 * space established by this element's parent element for this element, are
 * included.
 *
 * If aWhich is eUserSpaceToParent, then only the transforms from this
 * element's userspace to the coordinate space established by its parent is
 * included. This includes any transforms introduced by the 'transform'
 * attribute, transform animations and animateMotion, but not any offsets
 * due to e.g. 'x'/'y' attributes, or any transform due to a 'viewBox'
 * attribute. (SVG userspace is defined to be the coordinate space in which
 * coordinates on an element apply.)
 *
 * If aWhich is eChildToUserSpace, then only the transforms from the
 * coordinate space established by this element for its childre to this
 * elements userspace are included. This includes any offsets due to e.g.
 * 'x'/'y' attributes, and any transform due to a 'viewBox' attribute, but
 * does not include any transforms due to the 'transform' attribute.
 */
enum SVGTransformTypes {
   eAllTransforms,
   eUserSpaceToParent,
   eChildToUserSpace
};

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
  typedef mozilla::gfx::Float Float;
  typedef mozilla::gfx::Matrix Matrix;
  typedef mozilla::gfx::Rect Rect;
  typedef mozilla::gfx::StrokeOptions StrokeOptions;
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

  /**
   * Moz2D's StrokeOptions requires someone else to own its mDashPattern
   * buffer, which is a pain when you want to initialize a StrokeOptions object
   * in a helper function and pass it out. This sub-class owns the mDashPattern
   * buffer so that consumers of such a helper function don't need to worry
   * about creating it, passing it in, or deleting it. (An added benefit is
   * that in the typical case when stroke-dasharray is short it will avoid
   * allocating.)
   */
  struct AutoStrokeOptions : public StrokeOptions {
    AutoStrokeOptions()
    {
      MOZ_ASSERT(mDashLength == 0, "InitDashPattern() depends on this");
    }
    ~AutoStrokeOptions() {
      if (mDashPattern && mDashPattern != mSmallArray) {
        delete [] mDashPattern;
      }
    }
    /**
     * Creates the buffer to store the stroke-dasharray, assuming out-of-memory
     * does not occur. The buffer's address is assigned to mDashPattern and
     * returned to the caller as a non-const pointer (so that the caller can
     * initialize the values in the buffer, since mDashPattern is const).
     */
    Float* InitDashPattern(size_t aDashCount) {
      if (aDashCount <= MOZ_ARRAY_LENGTH(mSmallArray)) {
        mDashPattern = mSmallArray;
        return mSmallArray;
      }
      Float* nonConstArray = new (mozilla::fallible) Float[aDashCount];
      mDashPattern = nonConstArray;
      return nonConstArray;
    }
    void DiscardDashPattern() {
      if (mDashPattern && mDashPattern != mSmallArray) {
        delete [] mDashPattern;
      }
      mDashLength = 0;
      mDashPattern = nullptr;
    }
  private:
    // Most dasharrays will fit in this and save us allocating
    Float mSmallArray[16];
  };

  enum StrokeOptionFlags {
    eAllStrokeOptions,
    eIgnoreStrokeDashing
  };
  /**
   * Note: the linecap style returned in aStrokeOptions is not valid when
   * ShapeTypeHasNoCorners(aElement) == true && aFlags == eIgnoreStrokeDashing,
   * since when aElement has no corners the rendered linecap style depends on
   * whether or not the stroke is dashed.
   */
  static void GetStrokeOptions(AutoStrokeOptions* aStrokeOptions,
                               nsSVGElement* aElement,
                               nsStyleContext* aStyleContext,
                               mozilla::SVGContextPaint* aContextPaint,
                               StrokeOptionFlags aFlags = eAllStrokeOptions);

  /**
   * Returns the current computed value of the CSS property 'stroke-width' for
   * the given element. aStyleContext may be provided as an optimization. 
   * aContextPaint is also optional.
   *
   * Note that this function does NOT take account of the value of the 'stroke'
   * and 'stroke-opacity' properties to, say, return zero if they are "none" or
   * "0", respectively.
   */
  static Float GetStrokeWidth(nsSVGElement* aElement,
                              nsStyleContext* aStyleContext,
                              mozilla::SVGContextPaint* aContextPaint);

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

  static Matrix GetCTM(nsSVGElement *aElement, bool aScreenCTM);

  /**
   * Gets the tight bounds-space stroke bounds of the non-scaling-stroked rect
   * aRect.
   * @param aToBoundsSpace transforms from source space to the space aBounds
   *        should be computed in.  Must be rectilinear.
   * @param aToNonScalingStrokeSpace transforms from source
   *        space to the space in which non-scaling stroke should be applied.
   *        Must be rectilinear.
   */
  static void
  RectilinearGetStrokeBounds(const Rect& aRect,
                             const Matrix& aToBoundsSpace,
                             const Matrix& aToNonScalingStrokeSpace,
                             float aStrokeWidth,
                             Rect* aBounds);

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

  static Matrix
  GetViewBoxTransform(float aViewportWidth, float aViewportHeight,
                      float aViewboxX, float aViewboxY,
                      float aViewboxWidth, float aViewboxHeight,
                      const SVGAnimatedPreserveAspectRatio &aPreserveAspectRatio);

  static Matrix
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
  static already_AddRefed<mozilla::gfx::Path>
  GetPath(const nsAString& aPathString);

  /**
   *  Returns true if aContent is one of the elements whose stroke is guaranteed
   *  to have no corners: circle or ellipse
   */
  static bool ShapeTypeHasNoCorners(const nsIContent* aContent);
};

#endif
