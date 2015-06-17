/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
// This is also necessary to ensure our definition of M_SQRT1_2 is picked up
#include "SVGContentUtils.h"

// Keep others in (case-insensitive) order:
#include "gfx2DGlue.h"
#include "gfxMatrix.h"
#include "gfxPlatform.h"
#include "gfxSVGGlyphs.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/RefPtr.h"
#include "nsComputedDOMStyle.h"
#include "nsFontMetrics.h"
#include "nsIFrame.h"
#include "nsIScriptError.h"
#include "nsLayoutUtils.h"
#include "SVGAnimationElement.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsContentUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/FloatingPoint.h"
#include "nsStyleContext.h"
#include "nsSVGPathDataParser.h"
#include "SVGPathData.h"
#include "SVGPathElement.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

SVGSVGElement*
SVGContentUtils::GetOuterSVGElement(nsSVGElement *aSVGElement)
{
  nsIContent *element = nullptr;
  nsIContent *ancestor = aSVGElement->GetFlattenedTreeParent();

  while (ancestor && ancestor->IsSVGElement() &&
                     !ancestor->IsSVGElement(nsGkAtoms::foreignObject)) {
    element = ancestor;
    ancestor = element->GetFlattenedTreeParent();
  }

  if (element && element->IsSVGElement(nsGkAtoms::svg)) {
    return static_cast<SVGSVGElement*>(element);
  }
  return nullptr;
}

void
SVGContentUtils::ActivateByHyperlink(nsIContent *aContent)
{
  MOZ_ASSERT(aContent->IsNodeOfType(nsINode::eANIMATION),
             "Expecting an animation element");

  static_cast<SVGAnimationElement*>(aContent)->ActivateByHyperlink();
}

enum DashState {
  eDashedStroke,
  eContinuousStroke, //< all dashes, no gaps
  eNoStroke          //< all gaps, no dashes
};

static DashState
GetStrokeDashData(SVGContentUtils::AutoStrokeOptions* aStrokeOptions,
                  nsSVGElement* aElement,
                  const nsStyleSVG* aStyleSVG,
                  gfxTextContextPaint *aContextPaint)
{
  size_t dashArrayLength;
  Float totalLengthOfDashes = 0.0, totalLengthOfGaps = 0.0;
  Float pathScale = 1.0;

  if (aContextPaint && aStyleSVG->mStrokeDasharrayFromObject) {
    const FallibleTArray<gfxFloat>& dashSrc = aContextPaint->GetStrokeDashArray();
    dashArrayLength = dashSrc.Length();
    if (dashArrayLength <= 0) {
      return eContinuousStroke;
    }
    Float* dashPattern = aStrokeOptions->InitDashPattern(dashArrayLength);
    if (!dashPattern) {
      return eContinuousStroke;
    }
    for (size_t i = 0; i < dashArrayLength; i++) {
      if (dashSrc[i] < 0.0) {
        return eContinuousStroke; // invalid
      }
      dashPattern[i] = Float(dashSrc[i]);
      (i % 2 ? totalLengthOfGaps : totalLengthOfDashes) += dashSrc[i];
    }
  } else {
    const nsStyleCoord *dasharray = aStyleSVG->mStrokeDasharray;
    dashArrayLength = aStyleSVG->mStrokeDasharrayLength;
    if (dashArrayLength <= 0) {
      return eContinuousStroke;
    }
    if (aElement->IsSVGElement(nsGkAtoms::path)) {
      pathScale = static_cast<SVGPathElement*>(aElement)->
        GetPathLengthScale(SVGPathElement::eForStroking);
      if (pathScale <= 0) {
        return eContinuousStroke;
      }
    }
    Float* dashPattern = aStrokeOptions->InitDashPattern(dashArrayLength);
    if (!dashPattern) {
      return eContinuousStroke;
    }
    for (uint32_t i = 0; i < dashArrayLength; i++) {
      Float dashLength =
        SVGContentUtils::CoordToFloat(aElement, dasharray[i]) * pathScale;
      if (dashLength < 0.0) {
        return eContinuousStroke; // invalid
      }
      dashPattern[i] = dashLength;
      (i % 2 ? totalLengthOfGaps : totalLengthOfDashes) += dashLength;
    }
  }

  // Now that aStrokeOptions.mDashPattern is fully initialized (we didn't
  // return early above) we can safely set mDashLength:
  aStrokeOptions->mDashLength = dashArrayLength;

  if ((dashArrayLength % 2) == 1) {
    // If we have a dash pattern with an odd number of lengths the pattern
    // repeats a second time, per the SVG spec., and as implemented by Moz2D.
    // When deciding whether to return eNoStroke or eContinuousStroke below we
    // need to take into account that in the repeat pattern the dashes become
    // gaps, and the gaps become dashes.
    Float origTotalLengthOfDashes = totalLengthOfDashes;
    totalLengthOfDashes += totalLengthOfGaps;
    totalLengthOfGaps += origTotalLengthOfDashes;
  }

  // Stroking using dashes is much slower than stroking a continuous line
  // (see bug 609361 comment 40), and much, much slower than not stroking the
  // line at all. Here we check for cases when the dash pattern causes the
  // stroke to essentially be continuous or to be nonexistent in which case
  // we can avoid expensive stroking operations (the underlying platform
  // graphics libraries don't seem to optimize for this).
  if (totalLengthOfGaps <= 0) {
    return eContinuousStroke;
  }
  // We can only return eNoStroke if the value of stroke-linecap isn't
  // adding caps to zero length dashes.
  if (totalLengthOfDashes <= 0 &&
      aStyleSVG->mStrokeLinecap == NS_STYLE_STROKE_LINECAP_BUTT) {
    return eNoStroke;
  }

  if (aContextPaint && aStyleSVG->mStrokeDashoffsetFromObject) {
    aStrokeOptions->mDashOffset = Float(aContextPaint->GetStrokeDashOffset());
  } else {
    aStrokeOptions->mDashOffset =
      SVGContentUtils::CoordToFloat(aElement, aStyleSVG->mStrokeDashoffset) *
      pathScale;
  }

  return eDashedStroke;
}

void
SVGContentUtils::GetStrokeOptions(AutoStrokeOptions* aStrokeOptions,
                                  nsSVGElement* aElement,
                                  nsStyleContext* aStyleContext,
                                  gfxTextContextPaint *aContextPaint,
                                  StrokeOptionFlags aFlags)
{
  nsRefPtr<nsStyleContext> styleContext;
  if (aStyleContext) {
    styleContext = aStyleContext;
  } else {
    styleContext =
      nsComputedDOMStyle::GetStyleContextForElementNoFlush(aElement, nullptr,
                                                           nullptr);
  }

  if (!styleContext) {
    return;
  }

  const nsStyleSVG* styleSVG = styleContext->StyleSVG();

  if (aFlags != eIgnoreStrokeDashing) {
    DashState dashState =
      GetStrokeDashData(aStrokeOptions, aElement, styleSVG, aContextPaint);

    if (dashState == eNoStroke) {
      // Hopefully this will shortcircuit any stroke operations:
      aStrokeOptions->mLineWidth = 0;
      return;
    }
    if (dashState == eContinuousStroke && aStrokeOptions->mDashPattern) {
      // Prevent our caller from wasting time looking at a pattern without gaps:
      aStrokeOptions->DiscardDashPattern();
    }
  }

  aStrokeOptions->mLineWidth =
    GetStrokeWidth(aElement, styleContext, aContextPaint);

  aStrokeOptions->mMiterLimit = Float(styleSVG->mStrokeMiterlimit);

  switch (styleSVG->mStrokeLinejoin) {
  case NS_STYLE_STROKE_LINEJOIN_MITER:
    aStrokeOptions->mLineJoin = JoinStyle::MITER_OR_BEVEL;
    break;
  case NS_STYLE_STROKE_LINEJOIN_ROUND:
    aStrokeOptions->mLineJoin = JoinStyle::ROUND;
    break;
  case NS_STYLE_STROKE_LINEJOIN_BEVEL:
    aStrokeOptions->mLineJoin = JoinStyle::BEVEL;
    break;
  }

  if (ShapeTypeHasNoCorners(aElement)) {
    aStrokeOptions->mLineCap = CapStyle::BUTT;
  }
  else {
    switch (styleSVG->mStrokeLinecap) {
      case NS_STYLE_STROKE_LINECAP_BUTT:
        aStrokeOptions->mLineCap = CapStyle::BUTT;
        break;
      case NS_STYLE_STROKE_LINECAP_ROUND:
        aStrokeOptions->mLineCap = CapStyle::ROUND;
        break;
      case NS_STYLE_STROKE_LINECAP_SQUARE:
        aStrokeOptions->mLineCap = CapStyle::SQUARE;
        break;
    }
  }
}

Float
SVGContentUtils::GetStrokeWidth(nsSVGElement* aElement,
                                nsStyleContext* aStyleContext,
                                gfxTextContextPaint *aContextPaint)
{
  nsRefPtr<nsStyleContext> styleContext;
  if (aStyleContext) {
    styleContext = aStyleContext;
  } else {
    styleContext =
      nsComputedDOMStyle::GetStyleContextForElementNoFlush(aElement, nullptr,
                                                           nullptr);
  }

  if (!styleContext) {
    return 0.0f;
  }

  const nsStyleSVG* styleSVG = styleContext->StyleSVG();

  if (aContextPaint && styleSVG->mStrokeWidthFromObject) {
    return aContextPaint->GetStrokeWidth();
  }

  return SVGContentUtils::CoordToFloat(aElement, styleSVG->mStrokeWidth);
}

float
SVGContentUtils::GetFontSize(Element *aElement)
{
  if (!aElement)
    return 1.0f;

  nsRefPtr<nsStyleContext> styleContext = 
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(aElement,
                                                         nullptr, nullptr);
  if (!styleContext) {
    // ReportToConsole
    NS_WARNING("Couldn't get style context for content in GetFontStyle");
    return 1.0f;
  }

  return GetFontSize(styleContext);
}

float
SVGContentUtils::GetFontSize(nsIFrame *aFrame)
{
  MOZ_ASSERT(aFrame, "NULL frame in GetFontSize");
  return GetFontSize(aFrame->StyleContext());
}

float
SVGContentUtils::GetFontSize(nsStyleContext *aStyleContext)
{
  MOZ_ASSERT(aStyleContext, "NULL style context in GetFontSize");

  nsPresContext *presContext = aStyleContext->PresContext();
  MOZ_ASSERT(presContext, "NULL pres context in GetFontSize");

  nscoord fontSize = aStyleContext->StyleFont()->mSize;
  return nsPresContext::AppUnitsToFloatCSSPixels(fontSize) / 
         presContext->TextZoom();
}

float
SVGContentUtils::GetFontXHeight(Element *aElement)
{
  if (!aElement)
    return 1.0f;

  nsRefPtr<nsStyleContext> styleContext = 
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(aElement,
                                                         nullptr, nullptr);
  if (!styleContext) {
    // ReportToConsole
    NS_WARNING("Couldn't get style context for content in GetFontStyle");
    return 1.0f;
  }

  return GetFontXHeight(styleContext);
}
  
float
SVGContentUtils::GetFontXHeight(nsIFrame *aFrame)
{
  MOZ_ASSERT(aFrame, "NULL frame in GetFontXHeight");
  return GetFontXHeight(aFrame->StyleContext());
}

float
SVGContentUtils::GetFontXHeight(nsStyleContext *aStyleContext)
{
  MOZ_ASSERT(aStyleContext, "NULL style context in GetFontXHeight");

  nsPresContext *presContext = aStyleContext->PresContext();
  MOZ_ASSERT(presContext, "NULL pres context in GetFontXHeight");

  nsRefPtr<nsFontMetrics> fontMetrics;
  nsLayoutUtils::GetFontMetricsForStyleContext(aStyleContext,
                                               getter_AddRefs(fontMetrics));

  if (!fontMetrics) {
    // ReportToConsole
    NS_WARNING("no FontMetrics in GetFontXHeight()");
    return 1.0f;
  }

  nscoord xHeight = fontMetrics->XHeight();
  return nsPresContext::AppUnitsToFloatCSSPixels(xHeight) /
         presContext->TextZoom();
}
nsresult
SVGContentUtils::ReportToConsole(nsIDocument* doc,
                                 const char* aWarning,
                                 const char16_t **aParams,
                                 uint32_t aParamsLength)
{
  return nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                         NS_LITERAL_CSTRING("SVG"), doc,
                                         nsContentUtils::eSVG_PROPERTIES,
                                         aWarning,
                                         aParams, aParamsLength);
}

bool
SVGContentUtils::EstablishesViewport(nsIContent *aContent)
{
  // Although SVG 1.1 states that <image> is an element that establishes a
  // viewport, this is really only for the document it references, not
  // for any child content, which is what this function is used for.
  return aContent && aContent->IsAnyOfSVGElements(nsGkAtoms::svg,
                                                  nsGkAtoms::foreignObject,
                                                  nsGkAtoms::symbol);
}

nsSVGElement*
SVGContentUtils::GetNearestViewportElement(nsIContent *aContent)
{
  nsIContent *element = aContent->GetFlattenedTreeParent();

  while (element && element->IsSVGElement()) {
    if (EstablishesViewport(element)) {
      if (element->IsSVGElement(nsGkAtoms::foreignObject)) {
        return nullptr;
      }
      return static_cast<nsSVGElement*>(element);
    }
    element = element->GetFlattenedTreeParent();
  }
  return nullptr;
}

static gfx::Matrix
GetCTMInternal(nsSVGElement *aElement, bool aScreenCTM, bool aHaveRecursed)
{
  gfxMatrix matrix = aElement->PrependLocalTransformsTo(gfxMatrix(),
    aHaveRecursed ? nsSVGElement::eAllTransforms : nsSVGElement::eUserSpaceToParent);
  nsSVGElement *element = aElement;
  nsIContent *ancestor = aElement->GetFlattenedTreeParent();

  while (ancestor && ancestor->IsSVGElement() &&
                     !ancestor->IsSVGElement(nsGkAtoms::foreignObject)) {
    element = static_cast<nsSVGElement*>(ancestor);
    matrix *= element->PrependLocalTransformsTo(gfxMatrix()); // i.e. *A*ppend
    if (!aScreenCTM && SVGContentUtils::EstablishesViewport(element)) {
      if (!element->NodeInfo()->Equals(nsGkAtoms::svg, kNameSpaceID_SVG) &&
          !element->NodeInfo()->Equals(nsGkAtoms::symbol, kNameSpaceID_SVG)) {
        NS_ERROR("New (SVG > 1.1) SVG viewport establishing element?");
        return gfx::Matrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
      }
      // XXX spec seems to say x,y translation should be undone for IsInnerSVG
      return gfx::ToMatrix(matrix);
    }
    ancestor = ancestor->GetFlattenedTreeParent();
  }
  if (!aScreenCTM) {
    // didn't find a nearestViewportElement
    return gfx::Matrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
  }
  if (!element->IsSVGElement(nsGkAtoms::svg)) {
    // Not a valid SVG fragment
    return gfx::Matrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
  }
  if (element == aElement && !aHaveRecursed) {
    // We get here when getScreenCTM() is called on an outer-<svg>.
    // Consistency with other elements would have us include only the
    // eFromUserSpace transforms, but we include the eAllTransforms
    // transforms in this case since that's what we've been doing for
    // a while, and it keeps us consistent with WebKit and Opera (if not
    // really with the ambiguous spec).
    matrix = aElement->PrependLocalTransformsTo(gfxMatrix());
  }
  if (!ancestor || !ancestor->IsElement()) {
    return gfx::ToMatrix(matrix);
  }
  if (ancestor->IsSVGElement()) {
    return
      gfx::ToMatrix(matrix) * GetCTMInternal(static_cast<nsSVGElement*>(ancestor), true, true);
  }

  // XXX this does not take into account CSS transform, or that the non-SVG
  // content that we've hit may itself be inside an SVG foreignObject higher up
  nsIDocument* currentDoc = aElement->GetComposedDoc();
  float x = 0.0f, y = 0.0f;
  if (currentDoc && element->NodeInfo()->Equals(nsGkAtoms::svg, kNameSpaceID_SVG)) {
    nsIPresShell *presShell = currentDoc->GetShell();
    if (presShell) {
      nsIFrame* frame = element->GetPrimaryFrame();
      nsIFrame* ancestorFrame = presShell->GetRootFrame();
      if (frame && ancestorFrame) {
        nsPoint point = frame->GetOffsetTo(ancestorFrame);
        x = nsPresContext::AppUnitsToFloatCSSPixels(point.x);
        y = nsPresContext::AppUnitsToFloatCSSPixels(point.y);
      }
    }
  }
  return ToMatrix(matrix).PostTranslate(x, y);
}

gfx::Matrix
SVGContentUtils::GetCTM(nsSVGElement *aElement, bool aScreenCTM)
{
  return GetCTMInternal(aElement, aScreenCTM, false);
}

double
SVGContentUtils::ComputeNormalizedHypotenuse(double aWidth, double aHeight)
{
  return sqrt((aWidth*aWidth + aHeight*aHeight)/2);
}

float
SVGContentUtils::AngleBisect(float a1, float a2)
{
  float delta = fmod(a2 - a1, static_cast<float>(2*M_PI));
  if (delta < 0) {
    delta += static_cast<float>(2*M_PI);
  }
  /* delta is now the angle from a1 around to a2, in the range [0, 2*M_PI) */
  float r = a1 + delta/2;
  if (delta >= M_PI) {
    /* the arc from a2 to a1 is smaller, so use the ray on that side */
    r += static_cast<float>(M_PI);
  }
  return r;
}

gfx::Matrix
SVGContentUtils::GetViewBoxTransform(float aViewportWidth, float aViewportHeight,
                                     float aViewboxX, float aViewboxY,
                                     float aViewboxWidth, float aViewboxHeight,
                                     const SVGAnimatedPreserveAspectRatio &aPreserveAspectRatio)
{
  return GetViewBoxTransform(aViewportWidth, aViewportHeight,
                             aViewboxX, aViewboxY,
                             aViewboxWidth, aViewboxHeight,
                             aPreserveAspectRatio.GetAnimValue());
}

gfx::Matrix
SVGContentUtils::GetViewBoxTransform(float aViewportWidth, float aViewportHeight,
                                     float aViewboxX, float aViewboxY,
                                     float aViewboxWidth, float aViewboxHeight,
                                     const SVGPreserveAspectRatio &aPreserveAspectRatio)
{
  NS_ASSERTION(aViewportWidth  >= 0, "viewport width must be nonnegative!");
  NS_ASSERTION(aViewportHeight >= 0, "viewport height must be nonnegative!");
  NS_ASSERTION(aViewboxWidth  > 0, "viewBox width must be greater than zero!");
  NS_ASSERTION(aViewboxHeight > 0, "viewBox height must be greater than zero!");

  SVGAlign align = aPreserveAspectRatio.GetAlign();
  SVGMeetOrSlice meetOrSlice = aPreserveAspectRatio.GetMeetOrSlice();

  // default to the defaults
  if (align == SVG_PRESERVEASPECTRATIO_UNKNOWN)
    align = SVG_PRESERVEASPECTRATIO_XMIDYMID;
  if (meetOrSlice == SVG_MEETORSLICE_UNKNOWN)
    meetOrSlice = SVG_MEETORSLICE_MEET;

  float a, d, e, f;
  a = aViewportWidth / aViewboxWidth;
  d = aViewportHeight / aViewboxHeight;
  e = 0.0f;
  f = 0.0f;

  if (align != SVG_PRESERVEASPECTRATIO_NONE &&
      a != d) {
    if ((meetOrSlice == SVG_MEETORSLICE_MEET && a < d) ||
        (meetOrSlice == SVG_MEETORSLICE_SLICE && d < a)) {
      d = a;
      switch (align) {
      case SVG_PRESERVEASPECTRATIO_XMINYMIN:
      case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
      case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
        break;
      case SVG_PRESERVEASPECTRATIO_XMINYMID:
      case SVG_PRESERVEASPECTRATIO_XMIDYMID:
      case SVG_PRESERVEASPECTRATIO_XMAXYMID:
        f = (aViewportHeight - a * aViewboxHeight) / 2.0f;
        break;
      case SVG_PRESERVEASPECTRATIO_XMINYMAX:
      case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
      case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
        f = aViewportHeight - a * aViewboxHeight;
        break;
      default:
        NS_NOTREACHED("Unknown value for align");
      }
    }
    else if (
      (meetOrSlice == SVG_MEETORSLICE_MEET &&
      d < a) ||
      (meetOrSlice == SVG_MEETORSLICE_SLICE &&
      a < d)) {
      a = d;
      switch (align) {
      case SVG_PRESERVEASPECTRATIO_XMINYMIN:
      case SVG_PRESERVEASPECTRATIO_XMINYMID:
      case SVG_PRESERVEASPECTRATIO_XMINYMAX:
        break;
      case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
      case SVG_PRESERVEASPECTRATIO_XMIDYMID:
      case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
        e = (aViewportWidth - a * aViewboxWidth) / 2.0f;
        break;
      case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
      case SVG_PRESERVEASPECTRATIO_XMAXYMID:
      case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
        e = aViewportWidth - a * aViewboxWidth;
        break;
      default:
        NS_NOTREACHED("Unknown value for align");
      }
    }
    else NS_NOTREACHED("Unknown value for meetOrSlice");
  }

  if (aViewboxX) e += -a * aViewboxX;
  if (aViewboxY) f += -d * aViewboxY;

  return gfx::Matrix(a, 0.0f, 0.0f, d, e, f);
}

static bool
ParseNumber(RangedPtr<const char16_t>& aIter,
            const RangedPtr<const char16_t>& aEnd,
            double& aValue)
{
  int32_t sign;
  if (!SVGContentUtils::ParseOptionalSign(aIter, aEnd, sign)) {
    return false;
  }

  // Absolute value of the integer part of the mantissa.
  double intPart = 0.0;

  bool gotDot = *aIter == '.';

  if (!gotDot) {
    if (!SVGContentUtils::IsDigit(*aIter)) {
      return false;
    }
    do {
      intPart = 10.0 * intPart + SVGContentUtils::DecimalDigitValue(*aIter);
      ++aIter;
    } while (aIter != aEnd && SVGContentUtils::IsDigit(*aIter));

    if (aIter != aEnd) {
      gotDot = *aIter == '.';
    }
  }

  // Fractional part of the mantissa.
  double fracPart = 0.0;

  if (gotDot) {
    ++aIter;
    if (aIter == aEnd || !SVGContentUtils::IsDigit(*aIter)) {
      return false;
    }

    // Power of ten by which we need to divide the fraction
    double divisor = 1.0;

    do {
      fracPart = 10.0 * fracPart + SVGContentUtils::DecimalDigitValue(*aIter);
      divisor *= 10.0;
      ++aIter;
    } while (aIter != aEnd && SVGContentUtils::IsDigit(*aIter));

    fracPart /= divisor;
  }

  bool gotE = false;
  int32_t exponent = 0;
  int32_t expSign;

  if (aIter != aEnd && (*aIter == 'e' || *aIter == 'E')) {

    RangedPtr<const char16_t> expIter(aIter);

    ++expIter;
    if (expIter != aEnd) {
      expSign = *expIter == '-' ? -1 : 1;
      if (*expIter == '-' || *expIter == '+') {
        ++expIter;
      }
      if (expIter != aEnd && SVGContentUtils::IsDigit(*expIter)) {
        // At this point we're sure this is an exponent
        // and not the start of a unit such as em or ex.
        gotE = true;
      }
    }

    if (gotE) {
      aIter = expIter;
      do {
        exponent = 10.0 * exponent + SVGContentUtils::DecimalDigitValue(*aIter);
        ++aIter;
      } while (aIter != aEnd && SVGContentUtils::IsDigit(*aIter));
    }
  }

  // Assemble the number
  aValue = sign * (intPart + fracPart);
  if (gotE) {
    aValue *= pow(10.0, expSign * exponent);
  }
  return true;
}

template<class floatType>
bool
SVGContentUtils::ParseNumber(RangedPtr<const char16_t>& aIter,
                             const RangedPtr<const char16_t>& aEnd,
                             floatType& aValue)
{
  RangedPtr<const char16_t> iter(aIter);

  double value;
  if (!::ParseNumber(iter, aEnd, value)) {
    return false;
  }
  floatType floatValue = floatType(value);
  if (!IsFinite(floatValue)) {
    return false;
  }
  aValue = floatValue;
  aIter = iter;
  return true;
}

template bool
SVGContentUtils::ParseNumber<float>(RangedPtr<const char16_t>& aIter,
                                    const RangedPtr<const char16_t>& aEnd,
                                    float& aValue);

template bool
SVGContentUtils::ParseNumber<double>(RangedPtr<const char16_t>& aIter,
                                     const RangedPtr<const char16_t>& aEnd,
                                     double& aValue);

RangedPtr<const char16_t>
SVGContentUtils::GetStartRangedPtr(const nsAString& aString)
{
  return RangedPtr<const char16_t>(aString.Data(), aString.Length());
}

RangedPtr<const char16_t>
SVGContentUtils::GetEndRangedPtr(const nsAString& aString)
{
  return RangedPtr<const char16_t>(aString.Data() + aString.Length(),
                                    aString.Data(), aString.Length());
}

template<class floatType>
bool
SVGContentUtils::ParseNumber(const nsAString& aString, 
                             floatType& aValue)
{
  RangedPtr<const char16_t> iter = GetStartRangedPtr(aString);
  const RangedPtr<const char16_t> end = GetEndRangedPtr(aString);

  return ParseNumber(iter, end, aValue) && iter == end;
}

template bool
SVGContentUtils::ParseNumber<float>(const nsAString& aString, 
                                    float& aValue);
template bool
SVGContentUtils::ParseNumber<double>(const nsAString& aString, 
                                     double& aValue);

/* static */
bool
SVGContentUtils::ParseInteger(RangedPtr<const char16_t>& aIter,
                              const RangedPtr<const char16_t>& aEnd,
                              int32_t& aValue)
{
  RangedPtr<const char16_t> iter(aIter);

  int32_t sign;
  if (!ParseOptionalSign(iter, aEnd, sign)) {
    return false;
  }

  if (!IsDigit(*iter)) {
    return false;
  }

  int64_t value = 0;

  do {
    if (value <= std::numeric_limits<int32_t>::max()) {
      value = 10 * value + DecimalDigitValue(*iter);
    }
    ++iter;
  } while (iter != aEnd && IsDigit(*iter));

  aIter = iter;
  aValue = int32_t(clamped(sign * value,
                           int64_t(std::numeric_limits<int32_t>::min()),
                           int64_t(std::numeric_limits<int32_t>::max())));
  return true;
}

/* static */
bool
SVGContentUtils::ParseInteger(const nsAString& aString,
                              int32_t& aValue)
{
  RangedPtr<const char16_t> iter = GetStartRangedPtr(aString);
  const RangedPtr<const char16_t> end = GetEndRangedPtr(aString);

  return ParseInteger(iter, end, aValue) && iter == end;
}

float
SVGContentUtils::CoordToFloat(nsSVGElement *aContent,
                              const nsStyleCoord &aCoord)
{
  switch (aCoord.GetUnit()) {
  case eStyleUnit_Factor:
    // user units
    return aCoord.GetFactorValue();

  case eStyleUnit_Coord:
    return nsPresContext::AppUnitsToFloatCSSPixels(aCoord.GetCoordValue());

  case eStyleUnit_Percent: {
    SVGSVGElement* ctx = aContent->GetCtx();
    return ctx ? aCoord.GetPercentValue() * ctx->GetLength(SVGContentUtils::XY) : 0.0f;
  }
  default:
    return 0.0f;
  }
}

already_AddRefed<gfx::Path>
SVGContentUtils::GetPath(const nsAString& aPathString)
{
  SVGPathData pathData;
  nsSVGPathDataParser parser(aPathString, &pathData);
  if (!parser.Parse()) {
    return NULL;
  }

  RefPtr<DrawTarget> drawTarget =
    gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  RefPtr<PathBuilder> builder =
    drawTarget->CreatePathBuilder(FillRule::FILL_WINDING);

  return pathData.BuildPath(builder, NS_STYLE_STROKE_LINECAP_BUTT, 1);
}

bool
SVGContentUtils::ShapeTypeHasNoCorners(const nsIContent* aContent) {
  return aContent && aContent->IsAnyOfSVGElements(nsGkAtoms::circle,
                                                  nsGkAtoms::ellipse);
}
