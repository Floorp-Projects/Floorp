/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
// This is also necessary to ensure our definition of M_SQRT1_2 is picked up
#include "SVGContentUtils.h"

// Keep others in (case-insensitive) order:
#include "gfxMatrix.h"
#include "mozilla/Preferences.h"
#include "nsComputedDOMStyle.h"
#include "nsFontMetrics.h"
#include "nsIDOMSVGElement.h"
#include "nsIFrame.h"
#include "nsIScriptError.h"
#include "nsLayoutUtils.h"
#include "nsSVGAnimationElement.h"
#include "nsSVGSVGElement.h"
#include "SVGAnimatedPreserveAspectRatio.h"

using namespace mozilla;
using namespace mozilla::dom;

nsSVGSVGElement*
SVGContentUtils::GetOuterSVGElement(nsSVGElement *aSVGElement)
{
  nsIContent *element = nullptr;
  nsIContent *ancestor = aSVGElement->GetFlattenedTreeParent();

  while (ancestor && ancestor->IsSVG() &&
                     ancestor->Tag() != nsGkAtoms::foreignObject) {
    element = ancestor;
    ancestor = element->GetFlattenedTreeParent();
  }

  if (element && element->Tag() == nsGkAtoms::svg) {
    return static_cast<nsSVGSVGElement*>(element);
  }
  return nullptr;
}

void
SVGContentUtils::ActivateByHyperlink(nsIContent *aContent)
{
  NS_ABORT_IF_FALSE(aContent->IsNodeOfType(nsINode::eANIMATION),
                    "Expecting an animation element");

  static_cast<nsSVGAnimationElement*>(aContent)->ActivateByHyperlink();
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
  NS_ABORT_IF_FALSE(aFrame, "NULL frame in GetFontSize");
  return GetFontSize(aFrame->GetStyleContext());
}

float
SVGContentUtils::GetFontSize(nsStyleContext *aStyleContext)
{
  NS_ABORT_IF_FALSE(aStyleContext, "NULL style context in GetFontSize");

  nsPresContext *presContext = aStyleContext->PresContext();
  NS_ABORT_IF_FALSE(presContext, "NULL pres context in GetFontSize");

  nscoord fontSize = aStyleContext->GetStyleFont()->mSize;
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
  NS_ABORT_IF_FALSE(aFrame, "NULL frame in GetFontXHeight");
  return GetFontXHeight(aFrame->GetStyleContext());
}

float
SVGContentUtils::GetFontXHeight(nsStyleContext *aStyleContext)
{
  NS_ABORT_IF_FALSE(aStyleContext, "NULL style context in GetFontXHeight");

  nsPresContext *presContext = aStyleContext->PresContext();
  NS_ABORT_IF_FALSE(presContext, "NULL pres context in GetFontXHeight");

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
                                 const PRUnichar **aParams,
                                 uint32_t aParamsLength)
{
  return nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                         "SVG", doc,
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
  return aContent && aContent->IsSVG() &&
           (aContent->Tag() == nsGkAtoms::svg ||
            aContent->Tag() == nsGkAtoms::foreignObject ||
            aContent->Tag() == nsGkAtoms::symbol);
}

already_AddRefed<nsIDOMSVGElement>
SVGContentUtils::GetNearestViewportElement(nsIContent *aContent)
{
  nsIContent *element = aContent->GetFlattenedTreeParent();

  while (element && element->IsSVG()) {
    if (EstablishesViewport(element)) {
      if (element->Tag() == nsGkAtoms::foreignObject) {
        return nullptr;
      }
      return nsCOMPtr<nsIDOMSVGElement>(do_QueryInterface(element)).forget();
    }
    element = element->GetFlattenedTreeParent();
  }
  return nullptr;
}

static gfxMatrix
GetCTMInternal(nsSVGElement *aElement, bool aScreenCTM, bool aHaveRecursed)
{
  gfxMatrix matrix = aElement->PrependLocalTransformsTo(gfxMatrix(),
    aHaveRecursed ? nsSVGElement::eAllTransforms : nsSVGElement::eUserSpaceToParent);
  nsSVGElement *element = aElement;
  nsIContent *ancestor = aElement->GetFlattenedTreeParent();

  while (ancestor && ancestor->IsSVG() &&
                     ancestor->Tag() != nsGkAtoms::foreignObject) {
    element = static_cast<nsSVGElement*>(ancestor);
    matrix *= element->PrependLocalTransformsTo(gfxMatrix()); // i.e. *A*ppend
    if (!aScreenCTM && SVGContentUtils::EstablishesViewport(element)) {
      if (!element->NodeInfo()->Equals(nsGkAtoms::svg, kNameSpaceID_SVG) &&
          !element->NodeInfo()->Equals(nsGkAtoms::symbol, kNameSpaceID_SVG)) {
        NS_ERROR("New (SVG > 1.1) SVG viewport establishing element?");
        return gfxMatrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
      }
      // XXX spec seems to say x,y translation should be undone for IsInnerSVG
      return matrix;
    }
    ancestor = ancestor->GetFlattenedTreeParent();
  }
  if (!aScreenCTM) {
    // didn't find a nearestViewportElement
    return gfxMatrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
  }
  if (element->Tag() != nsGkAtoms::svg) {
    // Not a valid SVG fragment
    return gfxMatrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
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
    return matrix;
  }
  if (ancestor->IsSVG()) {
    return
      matrix * GetCTMInternal(static_cast<nsSVGElement*>(ancestor), true, true);
  }

  // XXX this does not take into account CSS transform, or that the non-SVG
  // content that we've hit may itself be inside an SVG foreignObject higher up
  nsIDocument* currentDoc = aElement->GetCurrentDoc();
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
  return matrix * gfxMatrix().Translate(gfxPoint(x, y));
}

gfxMatrix
SVGContentUtils::GetCTM(nsSVGElement *aElement, bool aScreenCTM)
{
  nsIDocument* currentDoc = aElement->GetCurrentDoc();
  if (currentDoc) {
    // Flush all pending notifications so that our frames are up to date
    currentDoc->FlushPendingNotifications(Flush_Layout);
  }
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
    delta += 2*M_PI;
  }
  /* delta is now the angle from a1 around to a2, in the range [0, 2*M_PI) */
  float r = a1 + delta/2;
  if (delta >= M_PI) {
    /* the arc from a2 to a1 is smaller, so use the ray on that side */
    r += M_PI;
  }
  return r;
}

gfxMatrix
SVGContentUtils::GetViewBoxTransform(const nsSVGElement* aElement,
                                     float aViewportWidth, float aViewportHeight,
                                     float aViewboxX, float aViewboxY,
                                     float aViewboxWidth, float aViewboxHeight,
                                     const SVGAnimatedPreserveAspectRatio &aPreserveAspectRatio)
{
  return GetViewBoxTransform(aElement,
                             aViewportWidth, aViewportHeight,
                             aViewboxX, aViewboxY,
                             aViewboxWidth, aViewboxHeight,
                             aPreserveAspectRatio.GetAnimValue());
}

gfxMatrix
SVGContentUtils::GetViewBoxTransform(const nsSVGElement* aElement,
                                     float aViewportWidth, float aViewportHeight,
                                     float aViewboxX, float aViewboxY,
                                     float aViewboxWidth, float aViewboxHeight,
                                     const SVGPreserveAspectRatio &aPreserveAspectRatio)
{
  NS_ASSERTION(aViewportWidth  >= 0, "viewport width must be nonnegative!");
  NS_ASSERTION(aViewportHeight >= 0, "viewport height must be nonnegative!");
  NS_ASSERTION(aViewboxWidth  > 0, "viewBox width must be greater than zero!");
  NS_ASSERTION(aViewboxHeight > 0, "viewBox height must be greater than zero!");

  uint16_t align = aPreserveAspectRatio.GetAlign();
  uint16_t meetOrSlice = aPreserveAspectRatio.GetMeetOrSlice();

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
  
  return gfxMatrix(a, 0.0f, 0.0f, d, e, f);
}
