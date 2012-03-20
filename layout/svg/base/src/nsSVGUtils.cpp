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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// include nsSVGUtils.h first to ensure definition of M_SQRT1_2 is picked up
#include "nsSVGUtils.h"

#include "nsIDOMDocument.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsRenderingContext.h"
#include "nsStyleCoord.h"
#include "nsPresContext.h"
#include "nsSVGSVGElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsGkAtoms.h"
#include "nsIURI.h"
#include "nsStyleStruct.h"
#include "nsIPresShell.h"
#include "nsNetUtil.h"
#include "nsFrameList.h"
#include "nsISVGChildFrame.h"
#include "nsContentDLF.h"
#include "nsContentUtils.h"
#include "nsSVGFilterFrame.h"
#include "nsINameSpaceManager.h"
#include "nsDOMError.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsSVGInnerSVGFrame.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGMaskFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGTextContainerFrame.h"
#include "nsSVGLength2.h"
#include "nsGenericElement.h"
#include "nsSVGGraphicElement.h"
#include "nsAttrValue.h"
#include "nsIScriptError.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "nsSVGForeignObjectFrame.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsSVGEffects.h"
#include "nsMathUtils.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGFilterPaintCallback.h"
#include "nsSVGGeometryFrame.h"
#include "nsComputedDOMStyle.h"
#include "nsSVGPathGeometryFrame.h"
#include "nsSVGPathGeometryElement.h"
#include "prdtoa.h"
#include "mozilla/dom/Element.h"
#include "gfxUtils.h"
#include "mozilla/Preferences.h"

#include "mozilla/gfx/2D.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

// c = n / 255
// (c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1 / 2.4) - 0.055) * 255 + 0.5
static const PRUint8 glinearRGBTosRGBMap[256] = {
  0,  13,  22,  28,  34,  38,  42,  46,
 50,  53,  56,  59,  61,  64,  66,  69,
 71,  73,  75,  77,  79,  81,  83,  85,
 86,  88,  90,  92,  93,  95,  96,  98,
 99, 101, 102, 104, 105, 106, 108, 109,
110, 112, 113, 114, 115, 117, 118, 119,
120, 121, 122, 124, 125, 126, 127, 128,
129, 130, 131, 132, 133, 134, 135, 136,
137, 138, 139, 140, 141, 142, 143, 144,
145, 146, 147, 148, 148, 149, 150, 151,
152, 153, 154, 155, 155, 156, 157, 158,
159, 159, 160, 161, 162, 163, 163, 164,
165, 166, 167, 167, 168, 169, 170, 170,
171, 172, 173, 173, 174, 175, 175, 176,
177, 178, 178, 179, 180, 180, 181, 182,
182, 183, 184, 185, 185, 186, 187, 187,
188, 189, 189, 190, 190, 191, 192, 192,
193, 194, 194, 195, 196, 196, 197, 197,
198, 199, 199, 200, 200, 201, 202, 202,
203, 203, 204, 205, 205, 206, 206, 207,
208, 208, 209, 209, 210, 210, 211, 212,
212, 213, 213, 214, 214, 215, 215, 216,
216, 217, 218, 218, 219, 219, 220, 220,
221, 221, 222, 222, 223, 223, 224, 224,
225, 226, 226, 227, 227, 228, 228, 229,
229, 230, 230, 231, 231, 232, 232, 233,
233, 234, 234, 235, 235, 236, 236, 237,
237, 238, 238, 238, 239, 239, 240, 240,
241, 241, 242, 242, 243, 243, 244, 244,
245, 245, 246, 246, 246, 247, 247, 248,
248, 249, 249, 250, 250, 251, 251, 251,
252, 252, 253, 253, 254, 254, 255, 255
};

// c = n / 255
// c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4)) * 255 + 0.5
static const PRUint8 gsRGBToLinearRGBMap[256] = {
  0,   0,   0,   0,   0,   0,   0,   1,
  1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   2,   2,   2,   2,   2,   2,
  2,   2,   3,   3,   3,   3,   3,   3,
  4,   4,   4,   4,   4,   5,   5,   5,
  5,   6,   6,   6,   6,   7,   7,   7,
  8,   8,   8,   8,   9,   9,   9,  10,
 10,  10,  11,  11,  12,  12,  12,  13,
 13,  13,  14,  14,  15,  15,  16,  16,
 17,  17,  17,  18,  18,  19,  19,  20,
 20,  21,  22,  22,  23,  23,  24,  24,
 25,  25,  26,  27,  27,  28,  29,  29,
 30,  30,  31,  32,  32,  33,  34,  35,
 35,  36,  37,  37,  38,  39,  40,  41,
 41,  42,  43,  44,  45,  45,  46,  47,
 48,  49,  50,  51,  51,  52,  53,  54,
 55,  56,  57,  58,  59,  60,  61,  62,
 63,  64,  65,  66,  67,  68,  69,  70,
 71,  72,  73,  74,  76,  77,  78,  79,
 80,  81,  82,  84,  85,  86,  87,  88,
 90,  91,  92,  93,  95,  96,  97,  99,
100, 101, 103, 104, 105, 107, 108, 109,
111, 112, 114, 115, 116, 118, 119, 121,
122, 124, 125, 127, 128, 130, 131, 133,
134, 136, 138, 139, 141, 142, 144, 146,
147, 149, 151, 152, 154, 156, 157, 159,
161, 163, 164, 166, 168, 170, 171, 173,
175, 177, 179, 181, 183, 184, 186, 188,
190, 192, 194, 196, 198, 200, 202, 204,
206, 208, 210, 212, 214, 216, 218, 220,
222, 224, 226, 229, 231, 233, 235, 237,
239, 242, 244, 246, 248, 250, 253, 255
};

static bool gSMILEnabled;
static const char SMIL_PREF_STR[] = "svg.smil.enabled";

static int
SMILPrefChanged(const char *aPref, void *aClosure)
{
  bool prefVal = Preferences::GetBool(SMIL_PREF_STR);
  gSMILEnabled = prefVal;
  return 0;
}

bool
NS_SMILEnabled()
{
  static bool sInitialized = false;
  
  if (!sInitialized) {
    /* check and register ourselves with the pref */
    gSMILEnabled = Preferences::GetBool(SMIL_PREF_STR);
    Preferences::RegisterCallback(SMILPrefChanged, SMIL_PREF_STR);

    sInitialized = true;
  }

  return gSMILEnabled;
}

// we only take the address of this:
static mozilla::gfx::UserDataKey sSVGAutoRenderStateKey;

SVGAutoRenderState::SVGAutoRenderState(nsRenderingContext *aContext,
                                       RenderMode aMode)
  : mContext(aContext)
  , mOriginalRenderState(nsnull)
  , mMode(aMode)
  , mPaintingToWindow(false)
{
  mOriginalRenderState = aContext->RemoveUserData(&sSVGAutoRenderStateKey);
  // We always remove ourselves from aContext before it dies, so
  // passing nsnull as the destroy function is okay.
  aContext->AddUserData(&sSVGAutoRenderStateKey, this, nsnull);
}

SVGAutoRenderState::~SVGAutoRenderState()
{
  mContext->RemoveUserData(&sSVGAutoRenderStateKey);
  if (mOriginalRenderState) {
    mContext->AddUserData(&sSVGAutoRenderStateKey, mOriginalRenderState, nsnull);
  }
}

void
SVGAutoRenderState::SetPaintingToWindow(bool aPaintingToWindow)
{
  mPaintingToWindow = aPaintingToWindow;
}

/* static */ SVGAutoRenderState::RenderMode
SVGAutoRenderState::GetRenderMode(nsRenderingContext *aContext)
{
  void *state = aContext->GetUserData(&sSVGAutoRenderStateKey);
  if (state) {
    return static_cast<SVGAutoRenderState*>(state)->mMode;
  }
  return NORMAL;
}

/* static */ bool
SVGAutoRenderState::IsPaintingToWindow(nsRenderingContext *aContext)
{
  void *state = aContext->GetUserData(&sSVGAutoRenderStateKey);
  if (state) {
    return static_cast<SVGAutoRenderState*>(state)->mPaintingToWindow;
  }
  return false;
}

nsSVGSVGElement*
nsSVGUtils::GetOuterSVGElement(nsSVGElement *aSVGElement)
{
  nsIContent *element = nsnull;
  nsIContent *ancestor = aSVGElement->GetFlattenedTreeParent();

  while (ancestor && ancestor->IsSVG() &&
                     ancestor->Tag() != nsGkAtoms::foreignObject) {
    element = ancestor;
    ancestor = element->GetFlattenedTreeParent();
  }

  if (element && element->Tag() == nsGkAtoms::svg) {
    return static_cast<nsSVGSVGElement*>(element);
  }
  return nsnull;
}

float
nsSVGUtils::GetFontSize(Element *aElement)
{
  if (!aElement)
    return 1.0f;

  nsRefPtr<nsStyleContext> styleContext = 
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(aElement,
                                                         nsnull, nsnull);
  if (!styleContext) {
    // ReportToConsole
    NS_WARNING("Couldn't get style context for content in GetFontStyle");
    return 1.0f;
  }

  return GetFontSize(styleContext);
}

float
nsSVGUtils::GetFontSize(nsIFrame *aFrame)
{
  NS_ABORT_IF_FALSE(aFrame, "NULL frame in GetFontSize");
  return GetFontSize(aFrame->GetStyleContext());
}

float
nsSVGUtils::GetFontSize(nsStyleContext *aStyleContext)
{
  NS_ABORT_IF_FALSE(aStyleContext, "NULL style context in GetFontSize");

  nsPresContext *presContext = aStyleContext->PresContext();
  NS_ABORT_IF_FALSE(presContext, "NULL pres context in GetFontSize");

  nscoord fontSize = aStyleContext->GetStyleFont()->mSize;
  return nsPresContext::AppUnitsToFloatCSSPixels(fontSize) / 
         presContext->TextZoom();
}

float
nsSVGUtils::GetFontXHeight(Element *aElement)
{
  if (!aElement)
    return 1.0f;

  nsRefPtr<nsStyleContext> styleContext = 
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(aElement,
                                                         nsnull, nsnull);
  if (!styleContext) {
    // ReportToConsole
    NS_WARNING("Couldn't get style context for content in GetFontStyle");
    return 1.0f;
  }

  return GetFontXHeight(styleContext);
}
  
float
nsSVGUtils::GetFontXHeight(nsIFrame *aFrame)
{
  NS_ABORT_IF_FALSE(aFrame, "NULL frame in GetFontXHeight");
  return GetFontXHeight(aFrame->GetStyleContext());
}

float
nsSVGUtils::GetFontXHeight(nsStyleContext *aStyleContext)
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

void
nsSVGUtils::UnPremultiplyImageDataAlpha(PRUint8 *data, 
                                        PRInt32 stride,
                                        const nsIntRect &rect)
{
  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      PRUint8 *pixel = data + stride * y + 4 * x;

      PRUint8 a = pixel[GFX_ARGB32_OFFSET_A];
      if (a == 255)
        continue;

      if (a) {
        pixel[GFX_ARGB32_OFFSET_B] = (255 * pixel[GFX_ARGB32_OFFSET_B]) / a;
        pixel[GFX_ARGB32_OFFSET_G] = (255 * pixel[GFX_ARGB32_OFFSET_G]) / a;
        pixel[GFX_ARGB32_OFFSET_R] = (255 * pixel[GFX_ARGB32_OFFSET_R]) / a;
      } else {
        pixel[GFX_ARGB32_OFFSET_B] = 0;
        pixel[GFX_ARGB32_OFFSET_G] = 0;
        pixel[GFX_ARGB32_OFFSET_R] = 0;
      }
    }
  }
}

void
nsSVGUtils::PremultiplyImageDataAlpha(PRUint8 *data, 
                                      PRInt32 stride,
                                      const nsIntRect &rect)
{
  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      PRUint8 *pixel = data + stride * y + 4 * x;

      PRUint8 a = pixel[GFX_ARGB32_OFFSET_A];
      if (a == 255)
        continue;

      FAST_DIVIDE_BY_255(pixel[GFX_ARGB32_OFFSET_B],
                         pixel[GFX_ARGB32_OFFSET_B] * a);
      FAST_DIVIDE_BY_255(pixel[GFX_ARGB32_OFFSET_G],
                         pixel[GFX_ARGB32_OFFSET_G] * a);
      FAST_DIVIDE_BY_255(pixel[GFX_ARGB32_OFFSET_R],
                         pixel[GFX_ARGB32_OFFSET_R] * a);
    }
  }
}

void
nsSVGUtils::ConvertImageDataToLinearRGB(PRUint8 *data, 
                                        PRInt32 stride,
                                        const nsIntRect &rect)
{
  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      PRUint8 *pixel = data + stride * y + 4 * x;

      pixel[GFX_ARGB32_OFFSET_B] =
        gsRGBToLinearRGBMap[pixel[GFX_ARGB32_OFFSET_B]];
      pixel[GFX_ARGB32_OFFSET_G] =
        gsRGBToLinearRGBMap[pixel[GFX_ARGB32_OFFSET_G]];
      pixel[GFX_ARGB32_OFFSET_R] =
        gsRGBToLinearRGBMap[pixel[GFX_ARGB32_OFFSET_R]];
    }
  }
}

void
nsSVGUtils::ConvertImageDataFromLinearRGB(PRUint8 *data, 
                                          PRInt32 stride,
                                          const nsIntRect &rect)
{
  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      PRUint8 *pixel = data + stride * y + 4 * x;

      pixel[GFX_ARGB32_OFFSET_B] =
        glinearRGBTosRGBMap[pixel[GFX_ARGB32_OFFSET_B]];
      pixel[GFX_ARGB32_OFFSET_G] =
        glinearRGBTosRGBMap[pixel[GFX_ARGB32_OFFSET_G]];
      pixel[GFX_ARGB32_OFFSET_R] =
        glinearRGBTosRGBMap[pixel[GFX_ARGB32_OFFSET_R]];
    }
  }
}

nsresult
nsSVGUtils::ReportToConsole(nsIDocument* doc,
                            const char* aWarning,
                            const PRUnichar **aParams,
                            PRUint32 aParamsLength)
{
  return nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                         "SVG", doc,
                                         nsContentUtils::eSVG_PROPERTIES,
                                         aWarning,
                                         aParams, aParamsLength);
}

float
nsSVGUtils::CoordToFloat(nsPresContext *aPresContext,
                         nsSVGElement *aContent,
                         const nsStyleCoord &aCoord)
{
  switch (aCoord.GetUnit()) {
  case eStyleUnit_Factor:
    // user units
    return aCoord.GetFactorValue();

  case eStyleUnit_Coord:
    return nsPresContext::AppUnitsToFloatCSSPixels(aCoord.GetCoordValue());

  case eStyleUnit_Percent: {
      nsSVGSVGElement* ctx = aContent->GetCtx();
      return ctx ? aCoord.GetPercentValue() * ctx->GetLength(nsSVGUtils::XY) : 0.0f;
    }
  default:
    return 0.0f;
  }
}

bool
nsSVGUtils::EstablishesViewport(nsIContent *aContent)
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
nsSVGUtils::GetNearestViewportElement(nsIContent *aContent)
{
  nsIContent *element = aContent->GetFlattenedTreeParent();

  while (element && element->IsSVG()) {
    if (EstablishesViewport(element)) {
      if (element->Tag() == nsGkAtoms::foreignObject) {
        return nsnull;
      }
      return nsCOMPtr<nsIDOMSVGElement>(do_QueryInterface(element)).forget();
    }
    element = element->GetFlattenedTreeParent();
  }
  return nsnull;
}

static gfxMatrix
GetCTMInternal(nsSVGElement *aElement, bool aScreenCTM, bool aHaveRecursed)
{
  nsIDocument* currentDoc = aElement->GetCurrentDoc();

  gfxMatrix matrix = aElement->PrependLocalTransformsTo(gfxMatrix(),
    aHaveRecursed ? nsSVGElement::eAllTransforms : nsSVGElement::eUserSpaceToParent);
  nsSVGElement *element = aElement;
  nsIContent *ancestor = aElement->GetFlattenedTreeParent();

  while (ancestor && ancestor->IsSVG() &&
                     ancestor->Tag() != nsGkAtoms::foreignObject) {
    element = static_cast<nsSVGElement*>(ancestor);
    matrix *= element->PrependLocalTransformsTo(gfxMatrix()); // i.e. *A*ppend
    if (!aScreenCTM && nsSVGUtils::EstablishesViewport(element)) {
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
  if (!ancestor || !ancestor->IsElement()) {
    return matrix;
  }
  if (ancestor->IsSVG()) {
    if (element->Tag() != nsGkAtoms::svg) {
      return gfxMatrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
    }
    return
      matrix * GetCTMInternal(static_cast<nsSVGElement*>(ancestor), true, true);
  }
  // XXX this does not take into account CSS transform, or that the non-SVG
  // content that we've hit may itself be inside an SVG foreignObject higher up
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
nsSVGUtils::GetCTM(nsSVGElement *aElement, bool aScreenCTM)
{
  nsIDocument* currentDoc = aElement->GetCurrentDoc();
  if (currentDoc) {
    // Flush all pending notifications so that our frames are up to date
    currentDoc->FlushPendingNotifications(Flush_Layout);
  }
  return GetCTMInternal(aElement, aScreenCTM, false);
}

nsSVGDisplayContainerFrame*
nsSVGUtils::GetNearestSVGViewport(nsIFrame *aFrame)
{
  NS_ASSERTION(aFrame->IsFrameOfType(nsIFrame::eSVG), "SVG frame expected");
  if (aFrame->GetType() == nsGkAtoms::svgOuterSVGFrame) {
    return nsnull;
  }
  while ((aFrame = aFrame->GetParent())) {
    NS_ASSERTION(aFrame->IsFrameOfType(nsIFrame::eSVG), "SVG frame expected");
    if (aFrame->GetType() == nsGkAtoms::svgInnerSVGFrame ||
        aFrame->GetType() == nsGkAtoms::svgOuterSVGFrame) {
      return do_QueryFrame(aFrame);
    }
  }
  NS_NOTREACHED("This is not reached. It's only needed to compile.");
  return nsnull;
}

nsRect
nsSVGUtils::FindFilterInvalidation(nsIFrame *aFrame, const nsRect& aRect)
{
  PRInt32 appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  nsIntRect rect = aRect.ToOutsidePixels(appUnitsPerDevPixel);

  while (aFrame) {
    if (aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG)
      break;

    nsSVGFilterFrame *filter = nsSVGEffects::GetFilterFrame(aFrame);
    if (filter) {
      // When we are under AttributeChanged, we can no longer get the old bbox
      // by calling GetBBox(), and we need that to set up the filter region
      // with the correct position. :-(
      //rect = filter->GetInvalidationBBox(aFrame, rect);

      // XXX [perf] As a horrible workaround, for now we just invalidate the
      // entire area of the nearest viewport establishing frame that doesnt
      // have overflow:visible. See bug 463939.
      nsSVGDisplayContainerFrame* viewportFrame = GetNearestSVGViewport(aFrame);
      while (viewportFrame && !viewportFrame->GetStyleDisplay()->IsScrollableOverflow()) {
        viewportFrame = GetNearestSVGViewport(viewportFrame);
      }
      if (!viewportFrame) {
        viewportFrame = GetOuterSVGFrame(aFrame);
      }
      if (viewportFrame->GetType() == nsGkAtoms::svgOuterSVGFrame) {
        nsRect r = viewportFrame->GetVisualOverflowRect();
        // GetOverflowRect is relative to our border box, but we need it
        // relative to our content box.
        r.MoveBy(viewportFrame->GetPosition() - viewportFrame->GetContentRect().TopLeft());
        return r;
      }
      NS_ASSERTION(viewportFrame->GetType() == nsGkAtoms::svgInnerSVGFrame,
                   "Wrong frame type");
      nsSVGInnerSVGFrame* innerSvg = do_QueryFrame(static_cast<nsIFrame*>(viewportFrame));
      nsSVGDisplayContainerFrame* innerSvgParent = do_QueryFrame(viewportFrame->GetParent());
      float x, y, width, height;
      static_cast<nsSVGSVGElement*>(innerSvg->GetContent())->
        GetAnimatedLengthValues(&x, &y, &width, &height, nsnull);
      gfxRect bounds = GetCanvasTM(innerSvgParent).
                         TransformBounds(gfxRect(x, y, width, height));
      bounds.RoundOut();
      nsIntRect r;
      if (gfxUtils::GfxRectToIntRect(bounds, &r)) {
        rect = r;
      } else {
        NS_NOTREACHED("Not going to invalidate the correct area");
      }
      aFrame = viewportFrame;
    }
    aFrame = aFrame->GetParent();
  }

  nsRect r = rect.ToAppUnits(appUnitsPerDevPixel);
  if (aFrame) {
    NS_ASSERTION(aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG,
                 "outer SVG frame expected");
    r.MoveBy(aFrame->GetContentRect().TopLeft() - aFrame->GetPosition());
  }
  return r;
}

#ifdef DEBUG
bool
nsSVGUtils::OuterSVGIsCallingUpdateBounds(nsIFrame *aFrame)
{
  return nsSVGUtils::GetOuterSVGFrame(aFrame)->IsCallingUpdateBounds();
}
#endif

void
nsSVGUtils::InvalidateBounds(nsIFrame *aFrame, bool aDuringUpdate)
{
  NS_ABORT_IF_FALSE(aFrame->IsFrameOfType(nsIFrame::eSVG),
                    "Passed bad frame!");

  NS_ASSERTION(aDuringUpdate == OuterSVGIsCallingUpdateBounds(aFrame),
               "aDuringUpdate lies!");

  // Rendering observers must be notified about changes to the frames that they
  // are observing _before_ UpdateBounds is called on the SVG frame tree, so we
  // only need to notify observers if we're not under an UpdateBounds call.
  // In fact, it would actually be wrong to notify observers while under
  // UpdateBounds because the observers will try to mark themselves as dirty
  // and, since UpdateBounds would be in the process of _removeing_ dirty bits
  // from frames, that would mess things up.
  if (!aDuringUpdate) {
    NS_ASSERTION(!OuterSVGIsCallingUpdateBounds(aFrame),
                 "Must not InvalidateRenderingObservers() under "
                 "nsISVGChildFrame::UpdateBounds!");

    nsSVGEffects::InvalidateRenderingObservers(aFrame);
  }

  // Must come after InvalidateRenderingObservers
  if (aFrame->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD) {
    return;
  }

  // XXXjwatt: can this come before InvalidateRenderingObservers?
  if (aFrame->GetStateBits() &
      (NS_FRAME_IS_DIRTY | NS_FRAME_FIRST_REFLOW)) {
    // Nothing to do if we're already dirty, or if the outer-<svg>
    // hasn't yet had its initial reflow.
    return;
  }

  // XXXsvgreflow we want to reduce the bounds when passing through inner-<svg>
  // and <use>, etc.

  nsSVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame(aFrame);
  NS_ASSERTION(outerSVGFrame, "no outer svg frame");
  if (outerSVGFrame) {
    nsISVGChildFrame *svgFrame = do_QueryFrame(aFrame);
    if (!svgFrame)
      return;

    // Note that filters can paint even if the element being filtered has empty
    // bounds, so we don't return early for that. Specifically, filters can be
    // given an explicit size that doesn't depend on the bbox of the element
    // being filtered, and then feFlood can be used to fill that area with paint.
    nsRect rect = FindFilterInvalidation(aFrame, svgFrame->GetCoveredRegion());
    outerSVGFrame->Invalidate(rect);
  }
}

static void
MarkDirtyBitsOnDescendants(nsIFrame *aFrame)
{
  NS_ABORT_IF_FALSE(aFrame->IsFrameOfType(nsIFrame::eSVG),
                    "Passed bad frame!");

  nsIFrame* kid = aFrame->GetFirstPrincipalChild();
  while (kid) {
    nsISVGChildFrame* svgkid = do_QueryFrame(kid);
    if (svgkid &&
        !(kid->GetStateBits() &
          (NS_STATE_SVG_NONDISPLAY_CHILD | NS_FRAME_IS_DIRTY))) {
      MarkDirtyBitsOnDescendants(kid);
      kid->AddStateBits(NS_FRAME_IS_DIRTY);
    }
    kid = kid->GetNextSibling();
  }
}

void
nsSVGUtils::ScheduleBoundsUpdate(nsIFrame *aFrame)
{
  NS_ABORT_IF_FALSE(aFrame->IsFrameOfType(nsIFrame::eSVG),
                    "Passed bad frame!");

  // If this is triggered, the callers should be fixed to call us before
  // UpdateBounds is called. If we try to mark dirty bits on frames while we're
  // in the process of removing them, things will get messed up.
  NS_ASSERTION(!OuterSVGIsCallingUpdateBounds(aFrame),
               "Do not call under nsISVGChildFrame::UpdateBounds!");

  // We don't call nsSVGEffects::InvalidateRenderingObservers here because
  // we should only be called under InvalidateAndScheduleBoundsUpdate (which
  // calls InvalidateBounds) or nsSVGDisplayContainerFrame::InsertFrames
  // (at which point the frame has no observers).

  if (aFrame->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD) {
    return;
  }

  if (aFrame->GetStateBits() &
      (NS_FRAME_IS_DIRTY | NS_FRAME_FIRST_REFLOW)) {
    // Nothing to do if we're already dirty, or if the outer-<svg>
    // hasn't yet had its initial reflow.
    return;
  }

  // XXXsvgreflow once we store bounds on containers, we will not need to
  // mark our descendants dirty.
  MarkDirtyBitsOnDescendants(aFrame);

  nsSVGOuterSVGFrame *outerSVGFrame = nsnull;

  // We must not add dirty bits to the nsSVGOuterSVGFrame or else
  // PresShell::FrameNeedsReflow won't work when we pass it in below.
  if (aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG) {
    outerSVGFrame = static_cast<nsSVGOuterSVGFrame*>(aFrame);
  } else {
    aFrame->AddStateBits(NS_FRAME_IS_DIRTY);

    nsIFrame *f = aFrame->GetParent();
    while (f && !(f->GetStateBits() & NS_STATE_IS_OUTER_SVG)) {
      if (f->GetStateBits() &
          (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) {
        return;
      }
      f->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
      f = f->GetParent();
      NS_ABORT_IF_FALSE(f->IsFrameOfType(nsIFrame::eSVG),
                        "NS_STATE_IS_OUTER_SVG check above not valid!");
    }

    outerSVGFrame = static_cast<nsSVGOuterSVGFrame*>(f);

    NS_ABORT_IF_FALSE(outerSVGFrame &&
                      outerSVGFrame->GetType() == nsGkAtoms::svgOuterSVGFrame,
                      "Did not find nsSVGOuterSVGFrame!");
  }

  if (outerSVGFrame->GetStateBits() & NS_FRAME_IN_REFLOW) {
    // We're currently under an nsSVGOuterSVGFrame::Reflow call so there is no
    // need to call PresShell::FrameNeedsReflow, since we have an
    // nsSVGOuterSVGFrame::DidReflow call pending.
    return;
  }

  nsFrameState dirtyBit =
    (outerSVGFrame == aFrame ? NS_FRAME_IS_DIRTY : NS_FRAME_HAS_DIRTY_CHILDREN);

  aFrame->PresContext()->PresShell()->FrameNeedsReflow(
    outerSVGFrame, nsIPresShell::eResize, dirtyBit);
}

void
nsSVGUtils::InvalidateAndScheduleBoundsUpdate(nsIFrame *aFrame)
{
  // If this is triggered, the callers should be fixed to call us much
  // earlier. If we try to mark dirty bits on frames while we're in the
  // process of removing them, things will get messed up.
  NS_ASSERTION(!OuterSVGIsCallingUpdateBounds(aFrame),
               "Must not call under nsISVGChildFrame::UpdateBounds!");

  InvalidateBounds(aFrame, false);
  ScheduleBoundsUpdate(aFrame);
}

bool
nsSVGUtils::NeedsUpdatedBounds(nsIFrame *aFrame)
{
  NS_ABORT_IF_FALSE(aFrame->IsFrameOfType(nsIFrame::eSVG),
                    "SVG uses bits differently!");

  // The flags we test here may change, hence why we have this separate
  // function.
  return NS_SUBTREE_DIRTY(aFrame);
}

void
nsSVGUtils::NotifyAncestorsOfFilterRegionChange(nsIFrame *aFrame)
{
  NS_ABORT_IF_FALSE(!(aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG),
                    "Not expecting to be called on the outer SVG Frame");

  aFrame = aFrame->GetParent();

  while (aFrame) {
    if (aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG)
      return;

    nsSVGFilterProperty *property = nsSVGEffects::GetFilterProperty(aFrame);
    if (property) {
      property->Invalidate();
    }
    aFrame = aFrame->GetParent();
  }
}

double
nsSVGUtils::ComputeNormalizedHypotenuse(double aWidth, double aHeight)
{
  return sqrt((aWidth*aWidth + aHeight*aHeight)/2);
}

float
nsSVGUtils::ObjectSpace(const gfxRect &aRect, const nsSVGLength2 *aLength)
{
  float fraction, axis;

  switch (aLength->GetCtxType()) {
  case X:
    axis = aRect.Width();
    break;
  case Y:
    axis = aRect.Height();
    break;
  case XY:
    axis = float(ComputeNormalizedHypotenuse(aRect.Width(), aRect.Height()));
    break;
  default:
    NS_NOTREACHED("unexpected ctx type");
    axis = 0.0f;
    break;
  }

  if (aLength->IsPercentage()) {
    fraction = aLength->GetAnimValInSpecifiedUnits() / 100;
  } else {
    fraction = aLength->GetAnimValue(static_cast<nsSVGSVGElement*>
                                                (nsnull));
  }

  return fraction * axis;
}

float
nsSVGUtils::UserSpace(nsSVGElement *aSVGElement, const nsSVGLength2 *aLength)
{
  return aLength->GetAnimValue(aSVGElement);
}

float
nsSVGUtils::UserSpace(nsIFrame *aNonSVGContext, const nsSVGLength2 *aLength)
{
  return aLength->GetAnimValue(aNonSVGContext);
}

float
nsSVGUtils::AngleBisect(float a1, float a2)
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

nsSVGOuterSVGFrame *
nsSVGUtils::GetOuterSVGFrame(nsIFrame *aFrame)
{
  while (aFrame) {
    if (aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG) {
      return static_cast<nsSVGOuterSVGFrame*>(aFrame);
    }
    aFrame = aFrame->GetParent();
  }

  return nsnull;
}

nsIFrame*
nsSVGUtils::GetOuterSVGFrameAndCoveredRegion(nsIFrame* aFrame, nsRect* aRect)
{
  nsISVGChildFrame* svg = do_QueryFrame(aFrame);
  if (!svg)
    return nsnull;
  *aRect = (aFrame->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD) ?
             nsRect(0, 0, 0, 0) : svg->GetCoveredRegion();
  return GetOuterSVGFrame(aFrame);
}

gfxMatrix
nsSVGUtils::GetViewBoxTransform(const nsSVGElement* aElement,
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
nsSVGUtils::GetViewBoxTransform(const nsSVGElement* aElement,
                                float aViewportWidth, float aViewportHeight,
                                float aViewboxX, float aViewboxY,
                                float aViewboxWidth, float aViewboxHeight,
                                const SVGPreserveAspectRatio &aPreserveAspectRatio)
{
  NS_ASSERTION(aViewportWidth  >= 0, "viewport width must be nonnegative!");
  NS_ASSERTION(aViewportHeight >= 0, "viewport height must be nonnegative!");
  NS_ASSERTION(aViewboxWidth  > 0, "viewBox width must be greater than zero!");
  NS_ASSERTION(aViewboxHeight > 0, "viewBox height must be greater than zero!");

  PRUint16 align = aPreserveAspectRatio.GetAlign();
  PRUint16 meetOrSlice = aPreserveAspectRatio.GetMeetOrSlice();

  // default to the defaults
  if (align == nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_UNKNOWN)
    align = nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID;
  if (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_UNKNOWN)
    meetOrSlice = nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET;

  float a, d, e, f;
  a = aViewportWidth / aViewboxWidth;
  d = aViewportHeight / aViewboxHeight;
  e = 0.0f;
  f = 0.0f;

  if (align != nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE &&
      a != d) {
    if ((meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
        a < d) ||
        (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE &&
        d < a)) {
      d = a;
      switch (align) {
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMIN:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN:
        break;
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
        f = (aViewportHeight - a * aViewboxHeight) / 2.0f;
        break;
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
        f = aViewportHeight - a * aViewboxHeight;
        break;
      default:
        NS_NOTREACHED("Unknown value for align");
      }
    }
    else if (
      (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
      d < a) ||
      (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE &&
      a < d)) {
      a = d;
      switch (align) {
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMIN:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX:
        break;
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
        e = (aViewportWidth - a * aViewboxWidth) / 2.0f;
        break;
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
      case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
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

gfxMatrix
nsSVGUtils::GetCanvasTM(nsIFrame *aFrame)
{
  // XXX yuck, we really need a common interface for GetCanvasTM

  if (!aFrame->IsFrameOfType(nsIFrame::eSVG)) {
    return nsSVGIntegrationUtils::GetInitialMatrix(aFrame);
  }

  nsIAtom* type = aFrame->GetType();
  if (type == nsGkAtoms::svgForeignObjectFrame) {
    return static_cast<nsSVGForeignObjectFrame*>(aFrame)->GetCanvasTM();
  }

  nsSVGContainerFrame *containerFrame = do_QueryFrame(aFrame);
  if (containerFrame) {
    return containerFrame->GetCanvasTM();
  }

  return static_cast<nsSVGGeometryFrame*>(aFrame)->GetCanvasTM();
}

void 
nsSVGUtils::NotifyChildrenOfSVGChange(nsIFrame *aFrame, PRUint32 aFlags)
{
  nsIFrame *kid = aFrame->GetFirstPrincipalChild();

  while (kid) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      SVGFrame->NotifySVGChanged(aFlags); 
    } else {
      NS_ASSERTION(kid->IsFrameOfType(nsIFrame::eSVG), "SVG frame expected");
      // recurse into the children of container frames e.g. <clipPath>, <mask>
      // in case they have child frames with transformation matrices
      NotifyChildrenOfSVGChange(kid, aFlags);
    }
    kid = kid->GetNextSibling();
  }
}

// ************************************************************

class SVGPaintCallback : public nsSVGFilterPaintCallback
{
public:
  virtual void Paint(nsRenderingContext *aContext, nsIFrame *aTarget,
                     const nsIntRect* aDirtyRect)
  {
    nsISVGChildFrame *svgChildFrame = do_QueryFrame(aTarget);
    NS_ASSERTION(svgChildFrame, "Expected SVG frame here");

    nsIntRect* dirtyRect = nsnull;
    nsIntRect tmpDirtyRect;

    // aDirtyRect is in user-space pixels, we need to convert to
    // outer-SVG-frame-relative device pixels.
    if (aDirtyRect) {
      gfxMatrix userToDeviceSpace = nsSVGUtils::GetCanvasTM(aTarget);
      if (userToDeviceSpace.IsSingular()) {
        return;
      }
      gfxRect dirtyBounds = userToDeviceSpace.TransformBounds(
        gfxRect(aDirtyRect->x, aDirtyRect->y, aDirtyRect->width, aDirtyRect->height));
      dirtyBounds.RoundOut();
      if (gfxUtils::GfxRectToIntRect(dirtyBounds, &tmpDirtyRect)) {
        dirtyRect = &tmpDirtyRect;
      }
    }

    svgChildFrame->PaintSVG(aContext, dirtyRect);
  }
};

void
nsSVGUtils::PaintFrameWithEffects(nsRenderingContext *aContext,
                                  const nsIntRect *aDirtyRect,
                                  nsIFrame *aFrame)
{
  nsISVGChildFrame *svgChildFrame = do_QueryFrame(aFrame);
  if (!svgChildFrame)
    return;

  float opacity = aFrame->GetStyleDisplay()->mOpacity;
  if (opacity == 0.0f)
    return;

  const nsIContent* content = aFrame->GetContent();
  if (content->IsSVG() &&
      !static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
    return;
  }

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */

  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(aFrame);

  bool isOK = true;
  nsSVGFilterFrame *filterFrame = effectProperties.GetFilterFrame(&isOK);

  /* Check if we need to draw anything. HasValidCoveredRect only returns
   * true for path geometry and glyphs, so basically we're traversing
   * all containers and we can only skip leaves here.
   */
  if (aDirtyRect && svgChildFrame->HasValidCoveredRect()) {
    if (filterFrame) {
      if (!aDirtyRect->Intersects(filterFrame->GetFilterBBox(aFrame, nsnull)))
        return;
    } else {
      nsRect leafBounds = nsSVGUtils::TransformFrameRectToOuterSVG(
        aFrame->GetRect(), GetCanvasTM(aFrame), aFrame->PresContext());
      nsRect rect = aDirtyRect->ToAppUnits(aFrame->PresContext()->AppUnitsPerDevPixel());
      if (!rect.Intersects(leafBounds))
        return;
    }
  }

  /* SVG defines the following rendering model:
   *
   *  1. Render fill
   *  2. Render stroke
   *  3. Render markers
   *  4. Apply filter
   *  5. Apply clipping, masking, group opacity
   *
   * We follow this, but perform a couple of optimizations:
   *
   * + Use cairo's clipPath when representable natively (single object
   *   clip region).
   *
   * + Merge opacity and masking if both used together.
   */

  if (opacity != 1.0f && CanOptimizeOpacity(aFrame))
    opacity = 1.0f;

  gfxContext *gfx = aContext->ThebesContext();
  bool complexEffects = false;

  nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame(&isOK);
  nsSVGMaskFrame *maskFrame = effectProperties.GetMaskFrame(&isOK);

  bool isTrivialClip = clipPathFrame ? clipPathFrame->IsTrivial() : true;

  if (!isOK) {
    // Some resource is invalid. We shouldn't paint anything.
    return;
  }
  
  gfxMatrix matrix;
  if (clipPathFrame || maskFrame)
    matrix = GetCanvasTM(aFrame);

  /* Check if we need to do additional operations on this child's
   * rendering, which necessitates rendering into another surface. */
  if (opacity != 1.0f || maskFrame || (clipPathFrame && !isTrivialClip)) {
    complexEffects = true;
    gfx->Save();
    gfx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
  }

  /* If this frame has only a trivial clipPath, set up cairo's clipping now so
   * we can just do normal painting and get it clipped appropriately.
   */
  if (clipPathFrame && isTrivialClip) {
    gfx->Save();
    clipPathFrame->ClipPaint(aContext, aFrame, matrix);
  }

  /* Paint the child */
  if (filterFrame) {
    SVGPaintCallback paintCallback;
    filterFrame->FilterPaint(aContext, aFrame, &paintCallback, aDirtyRect);
  } else {
    svgChildFrame->PaintSVG(aContext, aDirtyRect);
  }

  if (clipPathFrame && isTrivialClip) {
    gfx->Restore();
  }

  /* No more effects, we're done. */
  if (!complexEffects)
    return;

  gfx->PopGroupToSource();

  nsRefPtr<gfxPattern> maskSurface =
    maskFrame ? maskFrame->ComputeMaskAlpha(aContext, aFrame,
                                            matrix, opacity) : nsnull;

  nsRefPtr<gfxPattern> clipMaskSurface;
  if (clipPathFrame && !isTrivialClip) {
    gfx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);

    nsresult rv = clipPathFrame->ClipPaint(aContext, aFrame, matrix);
    clipMaskSurface = gfx->PopGroup();

    if (NS_SUCCEEDED(rv) && clipMaskSurface) {
      // Still more set after clipping, so clip to another surface
      if (maskSurface || opacity != 1.0f) {
        gfx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
        gfx->Mask(clipMaskSurface);
        gfx->PopGroupToSource();
      } else {
        gfx->Mask(clipMaskSurface);
      }
    }
  }

  if (maskSurface) {
    gfx->Mask(maskSurface);
  } else if (opacity != 1.0f) {
    gfx->Paint(opacity);
  }

  gfx->Restore();
}

bool
nsSVGUtils::HitTestClip(nsIFrame *aFrame, const nsPoint &aPoint)
{
  nsSVGEffects::EffectProperties props =
    nsSVGEffects::GetEffectProperties(aFrame);
  if (!props.mClipPath)
    return true;

  bool isOK = true;
  nsSVGClipPathFrame *clipPathFrame = props.GetClipPathFrame(&isOK);
  if (!clipPathFrame || !isOK) {
    // clipPath is not a valid resource, so nothing gets painted, so
    // hit-testing must fail.
    return false;
  }

  return clipPathFrame->ClipHitTest(aFrame, GetCanvasTM(aFrame), aPoint);
}

nsIFrame *
nsSVGUtils::HitTestChildren(nsIFrame *aFrame, const nsPoint &aPoint)
{
  // Traverse the list in reverse order, so that if we get a hit we know that's
  // the topmost frame that intersects the point; then we can just return it.
  nsIFrame* result = nsnull;
  for (nsIFrame* current = aFrame->PrincipalChildList().LastChild();
       current;
       current = current->GetPrevSibling()) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(current);
    if (SVGFrame) {
      const nsIContent* content = current->GetContent();
      if (content->IsSVG() &&
          !static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
        continue;
      }
      result = SVGFrame->GetFrameForPoint(aPoint);
      if (result)
        break;
    }
  }

  if (result && !HitTestClip(aFrame, aPoint))
    result = nsnull;

  return result;
}

nsRect
nsSVGUtils::GetCoveredRegion(const nsFrameList &aFrames)
{
  nsRect rect;

  for (nsIFrame* kid = aFrames.FirstChild();
       kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* child = do_QueryFrame(kid);
    if (child) {
      nsRect childRect = child->GetCoveredRegion();
      rect.UnionRect(rect, childRect);
    }
  }

  return rect;
}

nsPoint
nsSVGUtils::TransformOuterSVGPointToChildFrame(nsPoint aPoint,
                                               const gfxMatrix& aFrameToCanvasTM,
                                               nsPresContext* aPresContext)
{
  NS_ABORT_IF_FALSE(!aFrameToCanvasTM.IsSingular(),
                    "Callers must not pass a singular matrix");
  gfxMatrix canvasDevToFrameUserSpace = aFrameToCanvasTM;
  canvasDevToFrameUserSpace.Invert();
  gfxPoint devPt = gfxPoint(aPoint.x, aPoint.y) /
    aPresContext->AppUnitsPerDevPixel();
  gfxPoint userPt = canvasDevToFrameUserSpace.Transform(devPt).Round();
  gfxPoint appPt = userPt * aPresContext->AppUnitsPerCSSPixel();
  userPt.x = clamped(appPt.x, gfxFloat(nscoord_MIN), gfxFloat(nscoord_MAX));
  userPt.y = clamped(appPt.y, gfxFloat(nscoord_MIN), gfxFloat(nscoord_MAX));
  // now guaranteed to be safe:
  return nsPoint(nscoord(userPt.x), nscoord(userPt.y));
}

nsRect
nsSVGUtils::TransformFrameRectToOuterSVG(const nsRect& aRect,
                                         const gfxMatrix& aMatrix,
                                         nsPresContext* aPresContext)
{
  gfxRect r(aRect.x, aRect.y, aRect.width, aRect.height);
  r.Scale(1.0 / nsPresContext::AppUnitsPerCSSPixel());
  return nsLayoutUtils::RoundGfxRectToAppRect(
    aMatrix.TransformBounds(r), aPresContext->AppUnitsPerDevPixel());
}

gfxIntSize
nsSVGUtils::ConvertToSurfaceSize(const gfxSize& aSize,
                                 bool *aResultOverflows)
{
  gfxIntSize surfaceSize(ClampToInt(ceil(aSize.width)), ClampToInt(ceil(aSize.height)));

  *aResultOverflows = surfaceSize.width != ceil(aSize.width) ||
    surfaceSize.height != ceil(aSize.height);

  if (!gfxASurface::CheckSurfaceSize(surfaceSize)) {
    surfaceSize.width = NS_MIN(NS_SVG_OFFSCREEN_MAX_DIMENSION,
                               surfaceSize.width);
    surfaceSize.height = NS_MIN(NS_SVG_OFFSCREEN_MAX_DIMENSION,
                                surfaceSize.height);
    *aResultOverflows = true;
  }

  return surfaceSize;
}

bool
nsSVGUtils::HitTestRect(const gfxMatrix &aMatrix,
                        float aRX, float aRY, float aRWidth, float aRHeight,
                        float aX, float aY)
{
  if (aMatrix.IsSingular()) {
    return false;
  }
  gfxContext ctx(gfxPlatform::GetPlatform()->ScreenReferenceSurface());
  ctx.SetMatrix(aMatrix);
  ctx.NewPath();
  ctx.Rectangle(gfxRect(aRX, aRY, aRWidth, aRHeight));
  ctx.IdentityMatrix();
  return ctx.PointInFill(gfxPoint(aX, aY));
}

gfxRect
nsSVGUtils::GetClipRectForFrame(nsIFrame *aFrame,
                                float aX, float aY, float aWidth, float aHeight)
{
  const nsStyleDisplay* disp = aFrame->GetStyleDisplay();

  if (!(disp->mClipFlags & NS_STYLE_CLIP_RECT)) {
    NS_ASSERTION(disp->mClipFlags == NS_STYLE_CLIP_AUTO,
                 "We don't know about this type of clip.");
    return gfxRect(aX, aY, aWidth, aHeight);
  }

  if (disp->mOverflowX == NS_STYLE_OVERFLOW_HIDDEN ||
      disp->mOverflowY == NS_STYLE_OVERFLOW_HIDDEN) {

    nsIntRect clipPxRect =
      disp->mClip.ToOutsidePixels(aFrame->PresContext()->AppUnitsPerDevPixel());
    gfxRect clipRect =
      gfxRect(clipPxRect.x, clipPxRect.y, clipPxRect.width, clipPxRect.height);

    if (NS_STYLE_CLIP_RIGHT_AUTO & disp->mClipFlags) {
      clipRect.width = aWidth - clipRect.X();
    }
    if (NS_STYLE_CLIP_BOTTOM_AUTO & disp->mClipFlags) {
      clipRect.height = aHeight - clipRect.Y();
    }

    if (disp->mOverflowX != NS_STYLE_OVERFLOW_HIDDEN) {
      clipRect.x = aX;
      clipRect.width = aWidth;
    }
    if (disp->mOverflowY != NS_STYLE_OVERFLOW_HIDDEN) {
      clipRect.y = aY;
      clipRect.height = aHeight;
    }
     
    return clipRect;
  }
  return gfxRect(aX, aY, aWidth, aHeight);
}

void
nsSVGUtils::CompositeSurfaceMatrix(gfxContext *aContext,
                                   gfxASurface *aSurface,
                                   const gfxMatrix &aCTM, float aOpacity)
{
  if (aCTM.IsSingular())
    return;
  
  if (aContext->IsCairo()) {
    aContext->Save();
    aContext->Multiply(aCTM);
    aContext->SetSource(aSurface);
    aContext->Paint(aOpacity);
    aContext->Restore();
  } else {
    DrawTarget *dt = aContext->GetDrawTarget();
    Matrix oldMat = dt->GetTransform();
    RefPtr<SourceSurface> surf =
      gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(dt, aSurface);
    dt->SetTransform(oldMat * ToMatrix(aCTM));

    gfxSize size = aSurface->GetSize();
    NS_ASSERTION(size.width >= 0 && size.height >= 0, "Failure to get size for aSurface.");

    gfxPoint pt = aSurface->GetDeviceOffset();

    dt->FillRect(Rect(-pt.x, -pt.y, size.width, size.height),
                 SurfacePattern(surf, EXTEND_CLAMP,
                                Matrix(1.0f, 0, 0, 1.0f, -pt.x, -pt.y)),
                 DrawOptions(aOpacity));

    dt->SetTransform(oldMat);
  }
}

void
nsSVGUtils::CompositePatternMatrix(gfxContext *aContext,
                                   gfxPattern *aPattern,
                                   const gfxMatrix &aCTM, float aWidth, float aHeight, float aOpacity)
{
  if (aCTM.IsSingular())
    return;

  aContext->Save();
  SetClipRect(aContext, aCTM, gfxRect(0, 0, aWidth, aHeight));
  aContext->Multiply(aCTM);
  aContext->SetPattern(aPattern);
  aContext->Paint(aOpacity);
  aContext->Restore();
}

void
nsSVGUtils::SetClipRect(gfxContext *aContext,
                        const gfxMatrix &aCTM,
                        const gfxRect &aRect)
{
  if (aCTM.IsSingular())
    return;

  gfxMatrix oldMatrix = aContext->CurrentMatrix();
  aContext->Multiply(aCTM);
  aContext->Clip(aRect);
  aContext->SetMatrix(oldMatrix);
}

void
nsSVGUtils::ClipToGfxRect(nsIntRect* aRect, const gfxRect& aGfxRect)
{
  gfxRect r = aGfxRect;
  r.RoundOut();
  gfxRect r2(aRect->x, aRect->y, aRect->width, aRect->height);
  r = r.Intersect(r2);
  *aRect = nsIntRect(PRInt32(r.X()), PRInt32(r.Y()),
                     PRInt32(r.Width()), PRInt32(r.Height()));
}

gfxRect
nsSVGUtils::GetBBox(nsIFrame *aFrame, PRUint32 aFlags)
{
  if (aFrame->GetContent()->IsNodeOfType(nsINode::eTEXT)) {
    aFrame = aFrame->GetParent();
  }
  gfxRect bbox;
  nsISVGChildFrame *svg = do_QueryFrame(aFrame);
  if (svg) {
    // It is possible to apply a gradient, pattern, clipping path, mask or
    // filter to text. When one of these facilities is applied to text
    // the bounding box is the entire text element in all
    // cases.
    nsSVGTextContainerFrame* metrics = do_QueryFrame(
      GetFirstNonAAncestorFrame(aFrame));
    if (metrics) {
      while (aFrame->GetType() != nsGkAtoms::svgTextFrame) {
        aFrame = aFrame->GetParent();
      }
      svg = do_QueryFrame(aFrame);
    }
    nsIContent* content = aFrame->GetContent();
    if (content->IsSVG() &&
        !static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
      return bbox;
    }
    gfxMatrix matrix;
    if (aFrame->GetType() == nsGkAtoms::svgForeignObjectFrame) {
      // The spec says getBBox "Returns the tight bounding box in *current user
      // space*". So we should really be doing this for all elements, but that
      // needs investigation to check that we won't break too much content.
      NS_ABORT_IF_FALSE(content->IsSVG(), "bad cast");
      nsSVGElement *element = static_cast<nsSVGElement*>(content);
      matrix = element->PrependLocalTransformsTo(matrix,
                          nsSVGElement::eChildToUserSpace);
    }
    bbox = svg->GetBBoxContribution(matrix, aFlags);
  } else {
    bbox = nsSVGIntegrationUtils::GetSVGBBoxForNonSVGFrame(aFrame);
  }
  NS_ASSERTION(bbox.Width() >= 0.0 && bbox.Height() >= 0.0, "Invalid bbox!");
  return bbox;
}

gfxRect
nsSVGUtils::GetRelativeRect(PRUint16 aUnits, const nsSVGLength2 *aXYWH,
                            const gfxRect &aBBox, nsIFrame *aFrame)
{
  float x, y, width, height;
  if (aUnits == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    x = aBBox.X() + ObjectSpace(aBBox, &aXYWH[0]);
    y = aBBox.Y() + ObjectSpace(aBBox, &aXYWH[1]);
    width = ObjectSpace(aBBox, &aXYWH[2]);
    height = ObjectSpace(aBBox, &aXYWH[3]);
  } else {
    x = UserSpace(aFrame, &aXYWH[0]);
    y = UserSpace(aFrame, &aXYWH[1]);
    width = UserSpace(aFrame, &aXYWH[2]);
    height = UserSpace(aFrame, &aXYWH[3]);
  }
  return gfxRect(x, y, width, height);
}

bool
nsSVGUtils::CanOptimizeOpacity(nsIFrame *aFrame)
{
  nsIAtom *type = aFrame->GetType();
  if (type != nsGkAtoms::svgImageFrame &&
      type != nsGkAtoms::svgPathGeometryFrame) {
    return false;
  }
  if (aFrame->GetStyleSVGReset()->mFilter) {
    return false;
  }
  // XXX The SVG WG is intending to allow fill, stroke and markers on <image>
  if (type == nsGkAtoms::svgImageFrame) {
    return true;
  }
  const nsStyleSVG *style = aFrame->GetStyleSVG();
  if (style->mMarkerStart || style->mMarkerMid || style->mMarkerEnd) {
    return false;
  }
  if (style->mFill.mType == eStyleSVGPaintType_None ||
      style->mFillOpacity <= 0 ||
      !static_cast<nsSVGPathGeometryFrame*>(aFrame)->HasStroke()) {
    return true;
  }
  return false;
}

float
nsSVGUtils::MaxExpansion(const gfxMatrix &aMatrix)
{
  // maximum expansion derivation from
  // http://lists.cairographics.org/archives/cairo/2004-October/001980.html
  // and also implemented in cairo_matrix_transformed_circle_major_axis
  double a = aMatrix.xx;
  double b = aMatrix.yx;
  double c = aMatrix.xy;
  double d = aMatrix.yy;
  double f = (a * a + b * b + c * c + d * d) / 2;
  double g = (a * a + b * b - c * c - d * d) / 2;
  double h = a * c + b * d;
  return sqrt(f + sqrt(g * g + h * h));
}

gfxMatrix
nsSVGUtils::AdjustMatrixForUnits(const gfxMatrix &aMatrix,
                                 nsSVGEnum *aUnits,
                                 nsIFrame *aFrame)
{
  if (aFrame &&
      aUnits->GetAnimValue() ==
      nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    gfxRect bbox = GetBBox(aFrame);
    return gfxMatrix().Scale(bbox.Width(), bbox.Height()) *
           gfxMatrix().Translate(gfxPoint(bbox.X(), bbox.Y())) *
           aMatrix;
  }
  return aMatrix;
}

nsIFrame*
nsSVGUtils::GetFirstNonAAncestorFrame(nsIFrame* aStartFrame)
{
  for (nsIFrame *ancestorFrame = aStartFrame; ancestorFrame;
       ancestorFrame = ancestorFrame->GetParent()) {
    if (ancestorFrame->GetType() != nsGkAtoms::svgAFrame) {
      return ancestorFrame;
    }
  }
  return nsnull;
}

#ifdef DEBUG
void
nsSVGUtils::WritePPM(const char *fname, gfxImageSurface *aSurface)
{
  FILE *f = fopen(fname, "wb");
  if (!f)
    return;

  gfxIntSize size = aSurface->GetSize();
  fprintf(f, "P6\n%d %d\n255\n", size.width, size.height);
  unsigned char *data = aSurface->Data();
  PRInt32 stride = aSurface->Stride();
  for (int y=0; y<size.height; y++) {
    for (int x=0; x<size.width; x++) {
      fwrite(data + y * stride + 4 * x + GFX_ARGB32_OFFSET_R, 1, 1, f);
      fwrite(data + y * stride + 4 * x + GFX_ARGB32_OFFSET_G, 1, 1, f);
      fwrite(data + y * stride + 4 * x + GFX_ARGB32_OFFSET_B, 1, 1, f);
    }
  }
  fclose(f);
}
#endif

// The logic here comes from _cairo_stroke_style_max_distance_from_path
static gfxRect
PathExtentsToMaxStrokeExtents(const gfxRect& aPathExtents,
                              nsSVGGeometryFrame* aFrame,
                              double styleExpansionFactor,
                              const gfxMatrix& aMatrix)
{
  double style_expansion =
    styleExpansionFactor * aFrame->GetStrokeWidth();

  double dx = style_expansion * (fabs(aMatrix.xx) + fabs(aMatrix.xy));
  double dy = style_expansion * (fabs(aMatrix.yy) + fabs(aMatrix.yx));

  gfxRect strokeExtents = aPathExtents;
  strokeExtents.Inflate(dx, dy);
  return strokeExtents;
}

/*static*/ gfxRect
nsSVGUtils::PathExtentsToMaxStrokeExtents(const gfxRect& aPathExtents,
                                          nsSVGGeometryFrame* aFrame,
                                          const gfxMatrix& aMatrix)
{
  return ::PathExtentsToMaxStrokeExtents(aPathExtents, aFrame, 0.5, aMatrix);
}

/*static*/ gfxRect
nsSVGUtils::PathExtentsToMaxStrokeExtents(const gfxRect& aPathExtents,
                                          nsSVGPathGeometryFrame* aFrame,
                                          const gfxMatrix& aMatrix)
{
  double styleExpansionFactor = 0.5;

  if (static_cast<nsSVGPathGeometryElement*>(aFrame->GetContent())->IsMarkable()) {
    const nsStyleSVG* style = aFrame->GetStyleSVG();

    if (style->mStrokeLinecap == NS_STYLE_STROKE_LINECAP_SQUARE) {
      styleExpansionFactor = M_SQRT1_2;
    }

    if (style->mStrokeLinejoin == NS_STYLE_STROKE_LINEJOIN_MITER &&
        styleExpansionFactor < style->mStrokeMiterlimit &&
        aFrame->GetContent()->Tag() != nsGkAtoms::line) {
      styleExpansionFactor = style->mStrokeMiterlimit;
    }
  }

  return ::PathExtentsToMaxStrokeExtents(aPathExtents,
                                         aFrame,
                                         styleExpansionFactor,
                                         aMatrix);
}

// ----------------------------------------------------------------------

/* static */ bool
nsSVGUtils::RootSVGElementHasViewbox(const nsIContent *aRootSVGElem)
{
  if (!aRootSVGElem->IsSVG(nsGkAtoms::svg)) {
    NS_ABORT_IF_FALSE(false, "Expecting an SVG <svg> node");
    return false;
  }

  const nsSVGSVGElement *svgSvgElem =
    static_cast<const nsSVGSVGElement*>(aRootSVGElem);

  return svgSvgElem->HasValidViewbox();
}

/* static */ void
nsSVGUtils::GetFallbackOrPaintColor(gfxContext *aContext, nsStyleContext *aStyleContext,
                                    nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                    float *aOpacity, nscolor *color)
{
  const nsStyleSVGPaint &paint = aStyleContext->GetStyleSVG()->*aFillOrStroke;
  nsStyleContext *styleIfVisited = aStyleContext->GetStyleIfVisited();
  bool isServer = paint.mType == eStyleSVGPaintType_Server;
  *color = isServer ? paint.mFallbackColor : paint.mPaint.mColor;
  if (styleIfVisited) {
    const nsStyleSVGPaint &paintIfVisited =
      styleIfVisited->GetStyleSVG()->*aFillOrStroke;
    // To prevent Web content from detecting if a user has visited a URL
    // (via URL loading triggered by paint servers or performance
    // differences between paint servers or between a paint server and a
    // color), we do not allow whether links are visited to change which
    // paint server is used or switch between paint servers and simple
    // colors.  A :visited style may only override a simple color with
    // another simple color.
    if (paintIfVisited.mType == eStyleSVGPaintType_Color &&
        paint.mType == eStyleSVGPaintType_Color) {
      nscolor colorIfVisited = paintIfVisited.mPaint.mColor;
      nscolor colors[2] = { *color, colorIfVisited };
      *color = nsStyleContext::CombineVisitedColors(colors,
                                         aStyleContext->RelevantLinkVisited());
    }
  }
}
