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

#include "nsIDOMDocument.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGSVGElement.h"
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
#include "nsSVGUtils.h"
#include "nsISVGGlyphFragmentLeaf.h"
#include "nsNetUtil.h"
#include "nsIDOMSVGRect.h"
#include "nsFrameList.h"
#include "nsISVGChildFrame.h"
#include "nsContentDLF.h"
#include "nsContentUtils.h"
#include "nsSVGFilterFrame.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMSVGPoint.h"
#include "nsSVGPoint.h"
#include "nsDOMError.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsSVGPreserveAspectRatio.h"
#include "nsSVGMatrix.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGMaskFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGLength2.h"
#include "nsGenericElement.h"
#include "nsAttrValue.h"
#include "nsSVGGeometryFrame.h"
#include "nsIScriptError.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "nsSVGForeignObjectFrame.h"
#include "nsIFontMetrics.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsSVGRect.h"
#include "nsSVGEffects.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGFilterPaintCallback.h"

gfxASurface *nsSVGUtils::mThebesComputationalSurface = nsnull;

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

static PRBool gSVGEnabled;
static const char SVG_PREF_STR[] = "svg.enabled";

static int
SVGPrefChanged(const char *aPref, void *aClosure)
{
  PRBool prefVal = nsContentUtils::GetBoolPref(SVG_PREF_STR);
  if (prefVal == gSVGEnabled)
    return 0;

  gSVGEnabled = prefVal;
  if (gSVGEnabled)
    nsContentDLF::RegisterSVG();
  else
    nsContentDLF::UnregisterSVG();

  return 0;
}

PRBool
NS_SVGEnabled()
{
  static PRBool sInitialized = PR_FALSE;
  
  if (!sInitialized) {
    /* check and register ourselves with the pref */
    gSVGEnabled = nsContentUtils::GetBoolPref(SVG_PREF_STR);
    nsContentUtils::RegisterPrefCallback(SVG_PREF_STR, SVGPrefChanged, nsnull);

    sInitialized = PR_TRUE;
  }

  return gSVGEnabled;
}

static nsIFrame*
GetFrameForContent(nsIContent* aContent)
{
  if (!aContent)
    return nsnull;

  nsIDocument *doc = aContent->GetCurrentDoc();
  if (!doc)
    return nsnull;

  return nsGenericElement::GetPrimaryFrameFor(aContent, doc);
}

float
nsSVGUtils::GetFontSize(nsIContent *aContent)
{
  nsIFrame* frame = GetFrameForContent(aContent);
  if (!frame) {
    NS_WARNING("no frame in GetFontSize()");
    return 1.0f;
  }

  return GetFontSize(frame);
}

float
nsSVGUtils::GetFontSize(nsIFrame *aFrame)
{
  return nsPresContext::AppUnitsToFloatCSSPixels(aFrame->GetStyleFont()->mSize) /
         aFrame->PresContext()->TextZoom();
}

float
nsSVGUtils::GetFontXHeight(nsIContent *aContent)
{
  nsIFrame* frame = GetFrameForContent(aContent);
  if (!frame) {
    NS_WARNING("no frame in GetFontXHeight()");
    return 1.0f;
  }

  return GetFontXHeight(frame);
}
  
float
nsSVGUtils::GetFontXHeight(nsIFrame *aFrame)
{
  nsCOMPtr<nsIFontMetrics> fontMetrics;
  nsLayoutUtils::GetFontMetricsForFrame(aFrame, getter_AddRefs(fontMetrics));

  if (!fontMetrics) {
    NS_WARNING("no FontMetrics in GetFontXHeight()");
    return 1.0f;
  }

  nscoord xHeight;
  fontMetrics->GetXHeight(xHeight);
  return nsPresContext::AppUnitsToFloatCSSPixels(xHeight) /
         aFrame->PresContext()->TextZoom();
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
  return nsContentUtils::ReportToConsole(nsContentUtils::eSVG_PROPERTIES,
                                         aWarning,
                                         aParams, aParamsLength,
                                         doc ? doc->GetDocumentURI() : nsnull,
                                         EmptyString(), 0, 0,
                                         nsIScriptError::warningFlag,
                                         "SVG");
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

nsresult
nsSVGUtils::GetNearestViewportElement(nsIContent *aContent,
                                      nsIDOMSVGElement * *aNearestViewportElement)
{
  *aNearestViewportElement = nsnull;

  nsBindingManager *bindingManager = nsnull;
  // XXXbz I _think_ this is right.  We want to be using the binding manager
  // that would have attached the bindings that gives us our anonymous
  // ancestors. That's the binding manager for the document we actually belong
  // to, which is our owner doc.
  nsIDocument* ownerDoc = aContent->GetOwnerDoc();
  if (ownerDoc) {
    bindingManager = ownerDoc->BindingManager();
  }

  nsCOMPtr<nsIContent> element = aContent;
  nsCOMPtr<nsIContent> ancestor;
  unsigned short ancestorCount = 0;

  while (1) {

    ancestor = nsnull;
    if (bindingManager) {
      // check for an anonymous ancestor first
      ancestor = bindingManager->GetInsertionParent(element);
    }
    if (!ancestor) {
      // if we didn't find an anonymous ancestor, use the explicit one
      ancestor = element->GetParent();
    }

    nsCOMPtr<nsIDOMSVGFitToViewBox> fitToViewBox = do_QueryInterface(element);

    if (fitToViewBox && (ancestor || ancestorCount)) {
      // right interface and not the outermost SVG element
      nsCOMPtr<nsIDOMSVGElement> SVGElement = do_QueryInterface(element);
      SVGElement.swap(*aNearestViewportElement);
      return NS_OK;
    }

    if (!ancestor) {
      // reached the top of our parent chain
      break;
    }

    element = ancestor;
    ancestorCount++;
  }

  return NS_OK;
}

nsresult
nsSVGUtils::GetFarthestViewportElement(nsIContent *aContent,
                                       nsIDOMSVGElement * *aFarthestViewportElement)
{
  *aFarthestViewportElement = nsnull;

  nsBindingManager *bindingManager = nsnull;
  // XXXbz I _think_ this is right.  We want to be using the binding manager
  // that would have attached the bindings that gives us our anonymous
  // ancestors. That's the binding manager for the document we actually belong
  // to, which is our owner doc.
  nsIDocument* ownerDoc = aContent->GetOwnerDoc();
  if (ownerDoc) {
    bindingManager = ownerDoc->BindingManager();
  }

  nsCOMPtr<nsIContent> element = aContent;
  nsCOMPtr<nsIContent> ancestor;
  nsCOMPtr<nsIDOMSVGElement> SVGElement;
  unsigned short ancestorCount = 0;

  while (1) {

    ancestor = nsnull;
    if (bindingManager) {
      // check for an anonymous ancestor first
      ancestor = bindingManager->GetInsertionParent(element);
    }
    if (!ancestor) {
      // if we didn't find an anonymous ancestor, use the explicit one
      ancestor = element->GetParent();
    }

    nsCOMPtr<nsIDOMSVGFitToViewBox> fitToViewBox = do_QueryInterface(element);

    if (fitToViewBox) {
      // right interface
      SVGElement = do_QueryInterface(element);
    }

    if (!ancestor) {
      // reached the top of our parent chain
      break;
    }

    element = ancestor;
    ancestorCount++;
  }

  if (ancestorCount == 0 || !SVGElement) {
    // outermost SVG element or no viewport found
    return NS_OK;
  }

  SVGElement.swap(*aFarthestViewportElement);
  return NS_OK;
}

nsresult
nsSVGUtils::GetBBox(nsFrameList *aFrames, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  float minx, miny, maxx, maxy;
  minx = miny = FLT_MAX;
  maxx = maxy = -1.0 * FLT_MAX;

  nsCOMPtr<nsIDOMSVGRect> unionRect;

  nsIFrame* kid = aFrames->FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      nsCOMPtr<nsIDOMSVGRect> box;
      SVGFrame->GetBBox(getter_AddRefs(box));

      if (box) {
        float bminx, bminy, bmaxx, bmaxy, width, height;
        box->GetX(&bminx);
        box->GetY(&bminy);
        box->GetWidth(&width);
        box->GetHeight(&height);
        bmaxx = bminx+width;
        bmaxy = bminy+height;

        if (!unionRect)
          unionRect = box;
        minx = PR_MIN(minx, bminx);
        miny = PR_MIN(miny, bminy);
        maxx = PR_MAX(maxx, bmaxx);
        maxy = PR_MAX(maxy, bmaxy);
      }
    }
    kid = kid->GetNextSibling();
  }

  if (unionRect) {
    unionRect->SetX(minx);
    unionRect->SetY(miny);
    unionRect->SetWidth(maxx - minx);
    unionRect->SetHeight(maxy - miny);
    *_retval = unionRect;
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsRect
nsSVGUtils::FindFilterInvalidation(nsIFrame *aFrame, const nsRect& aRect)
{
  PRInt32 appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  nsIntRect rect = nsRect::ToOutsidePixels(aRect, appUnitsPerDevPixel);

  while (aFrame) {
    if (aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG)
      break;

    nsSVGFilterFrame *filter = nsSVGEffects::GetFilterFrame(aFrame);
    if (filter) {
      rect = filter->GetInvalidationBBox(aFrame, rect);
    }
    aFrame = aFrame->GetParent();
  }

  return nsIntRect::ToAppUnits(rect, appUnitsPerDevPixel);
}

void
nsSVGUtils::InvalidateCoveredRegion(nsIFrame *aFrame)
{
  if (aFrame->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)
    return;

  nsSVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(aFrame);
  NS_ASSERTION(outerSVGFrame, "no outer svg frame");
  if (outerSVGFrame)
    outerSVGFrame->InvalidateCoveredRegion(aFrame);
}

void
nsSVGUtils::UpdateGraphic(nsISVGChildFrame *aSVGFrame)
{
  nsIFrame *frame = do_QueryFrame(aSVGFrame);

  nsSVGEffects::InvalidateRenderingObservers(frame);

  if (frame->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)
    return;

  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(frame);
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return;
  }

  if (outerSVGFrame->IsRedrawSuspended()) {
    frame->AddStateBits(NS_STATE_SVG_DIRTY);
  } else {
    frame->RemoveStateBits(NS_STATE_SVG_DIRTY);

    PRBool changed = outerSVGFrame->UpdateAndInvalidateCoveredRegion(frame);
    if (changed) {
      NotifyAncestorsOfFilterRegionChange(frame);
    }
  }
}

void
nsSVGUtils::NotifyAncestorsOfFilterRegionChange(nsIFrame *aFrame)
{
  if (aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG) {
    // It would be better if we couldn't get here
    return;
  }

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
nsSVGUtils::ObjectSpace(nsIDOMSVGRect *aRect, const nsSVGLength2 *aLength)
{
  float fraction, axis;

  switch (aLength->GetCtxType()) {
  case X:
    aRect->GetWidth(&axis);
    break;
  case Y:
    aRect->GetHeight(&axis);
    break;
  case XY:
  {
    float width, height;
    aRect->GetWidth(&width);
    aRect->GetHeight(&height);
    axis = float(ComputeNormalizedHypotenuse(width, height));
  }
  }

  if (aLength->IsPercentage()) {
    fraction = aLength->GetAnimValInSpecifiedUnits() / 100;
  } else
    fraction = aLength->GetAnimValue(static_cast<nsSVGSVGElement*>
                                                (nsnull));

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

void
nsSVGUtils::TransformPoint(nsIDOMSVGMatrix *matrix, 
                           float *x, float *y)
{
  nsCOMPtr<nsIDOMSVGPoint> point;
  NS_NewSVGPoint(getter_AddRefs(point), *x, *y);
  if (!point)
    return;

  nsCOMPtr<nsIDOMSVGPoint> xfpoint;
  point->MatrixTransform(matrix, getter_AddRefs(xfpoint));
  if (!xfpoint)
    return;

  xfpoint->GetX(x);
  xfpoint->GetY(y);
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
  *aRect = svg->GetCoveredRegion();
  return GetOuterSVGFrame(aFrame);
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGUtils::GetViewBoxTransform(float aViewportWidth, float aViewportHeight,
                                float aViewboxX, float aViewboxY,
                                float aViewboxWidth, float aViewboxHeight,
                                const nsSVGPreserveAspectRatio &aPreserveAspectRatio,
                                PRBool aIgnoreAlign)
{
  NS_ASSERTION(aViewboxWidth > 0, "viewBox width must be greater than zero!");
  NS_ASSERTION(aViewboxHeight > 0, "viewBox height must be greater than zero!");

  PRUint16 align = aPreserveAspectRatio.GetAnimValue().GetAlign();
  PRUint16 meetOrSlice = aPreserveAspectRatio.GetAnimValue().GetMeetOrSlice();

  // default to the defaults
  if (align == nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_UNKNOWN)
    align = nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID;
  if (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_UNKNOWN)
    meetOrSlice = nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET;

  // alignment disabled for this matrix setup
  if (aIgnoreAlign)
    align = nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMIN;
    
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
  
  nsIDOMSVGMatrix *retval;
  NS_NewSVGMatrix(&retval, a, 0.0f, 0.0f, d, e, f);
  return retval;
}


// This is ugly and roc will want to kill me...

already_AddRefed<nsIDOMSVGMatrix>
nsSVGUtils::GetCanvasTM(nsIFrame *aFrame)
{
  if (!aFrame->IsFrameOfType(nsIFrame::eSVG))
    return nsSVGIntegrationUtils::GetInitialMatrix(aFrame);

  if (!aFrame->IsLeaf()) {
    // foreignObject is the one non-leaf svg frame that isn't a SVGContainer
    if (aFrame->GetType() == nsGkAtoms::svgForeignObjectFrame) {
      nsSVGForeignObjectFrame *foreignFrame =
        static_cast<nsSVGForeignObjectFrame*>(aFrame);
      return foreignFrame->GetCanvasTM();
    }
    nsSVGContainerFrame *containerFrame = static_cast<nsSVGContainerFrame*>
                                                     (aFrame);
    return containerFrame->GetCanvasTM();
  }

  nsSVGGeometryFrame *geometryFrame = static_cast<nsSVGGeometryFrame*>
                                                 (aFrame);
  nsCOMPtr<nsIDOMSVGMatrix> matrix;
  nsIDOMSVGMatrix *retval;
  geometryFrame->GetCanvasTM(getter_AddRefs(matrix));
  retval = matrix.get();
  NS_IF_ADDREF(retval);
  return retval;
}

void 
nsSVGUtils::NotifyChildrenOfSVGChange(nsIFrame *aFrame, PRUint32 aFlags)
{
  nsIFrame *aKid = aFrame->GetFirstChild(nsnull);

  while (aKid) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(aKid);
    if (SVGFrame) {
      SVGFrame->NotifySVGChanged(aFlags); 
    } else {
      NS_ASSERTION(aKid->IsFrameOfType(nsIFrame::eSVG), "SVG frame expected");
      // recurse into the children of container frames e.g. <clipPath>, <mask>
      // in case they have child frames with transformation matrices
      nsSVGUtils::NotifyChildrenOfSVGChange(aKid, aFlags);
    }
    aKid = aKid->GetNextSibling();
  }
}

void
nsSVGUtils::AddObserver(nsISupports *aObserver, nsISupports *aTarget)
{
  nsISVGValueObserver *observer = nsnull;
  nsISVGValue *v = nsnull;
  CallQueryInterface(aObserver, &observer);
  CallQueryInterface(aTarget, &v);
  if (observer && v)
    v->AddObserver(observer);
}

void
nsSVGUtils::RemoveObserver(nsISupports *aObserver, nsISupports *aTarget)
{
  nsISVGValueObserver *observer = nsnull;
  nsISVGValue *v = nsnull;
  CallQueryInterface(aObserver, &observer);
  CallQueryInterface(aTarget, &v);
  if (observer && v)
    v->RemoveObserver(observer);
}

// ************************************************************

class SVGPaintCallback : public nsSVGFilterPaintCallback
{
public:
  virtual void Paint(nsSVGRenderState *aContext, nsIFrame *aTarget,
                     const nsIntRect* aDirtyRect)
  {
    nsISVGChildFrame *svgChildFrame = do_QueryFrame(aTarget);
    NS_ASSERTION(svgChildFrame, "Expected SVG frame here");
    NS_ASSERTION(!svgChildFrame->GetMatrixPropagation(),
                 "This should have been set to false already");

    nsIntRect* dirtyRect = nsnull;
    nsIntRect tmpDirtyRect;

    // aDirtyRect is in user-space pixels, we need to convert to
    // outer-SVG-frame-relative device pixels.
    if (aDirtyRect) {
      // Temporarily set SetMatrixPropagation so we can find out what
      // the actual CTM is.
      svgChildFrame->SetMatrixPropagation(PR_TRUE);
      nsCOMPtr<nsIDOMSVGMatrix> ctm = nsSVGUtils::GetCanvasTM(aTarget);
      NS_ASSERTION(ctm, "graphic source didn't specify a ctm");
      svgChildFrame->SetMatrixPropagation(PR_FALSE);

      gfxMatrix matrix = nsSVGUtils::ConvertSVGMatrixToThebes(ctm);
      gfxRect dirtyBounds = matrix.TransformBounds(
        gfxRect(aDirtyRect->x, aDirtyRect->y, aDirtyRect->width, aDirtyRect->height));
      dirtyBounds.RoundOut();
      if (NS_SUCCEEDED(nsSVGUtils::GfxRectToIntRect(dirtyBounds, &tmpDirtyRect))) {
        dirtyRect = &tmpDirtyRect;
      }
    }

    svgChildFrame->PaintSVG(aContext, dirtyRect);
  }
};

void
nsSVGUtils::PaintFrameWithEffects(nsSVGRenderState *aContext,
                                  const nsIntRect *aDirtyRect,
                                  nsIFrame *aFrame)
{
  nsISVGChildFrame *svgChildFrame = do_QueryFrame(aFrame);
  if (!svgChildFrame)
    return;

  float opacity = aFrame->GetStyleDisplay()->mOpacity;
  if (opacity == 0.0f)
    return;

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */

  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(aFrame);

  PRBool isOK = PR_TRUE;
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
      nsRect rect = nsIntRect::ToAppUnits(*aDirtyRect, aFrame->PresContext()->AppUnitsPerDevPixel());
      if (!rect.Intersects(aFrame->GetRect()))
        return;
    }
  }

  /* SVG defines the following rendering model:
   *
   *  1. Render geometry
   *  2. Apply filter
   *  3. Apply clipping, masking, group opacity
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

  gfxContext *gfx = aContext->GetGfxContext();
  PRBool complexEffects = PR_FALSE;

  nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame(&isOK);
  nsSVGMaskFrame *maskFrame = effectProperties.GetMaskFrame(&isOK);

  PRBool isTrivialClip = clipPathFrame ? clipPathFrame->IsTrivial() : PR_TRUE;

  if (!isOK) {
    // Some resource is missing. We shouldn't paint anything.
    return;
  }
  
  nsCOMPtr<nsIDOMSVGMatrix> matrix =
    (clipPathFrame || maskFrame) ? GetCanvasTM(aFrame) : nsnull;

  /* Check if we need to do additional operations on this child's
   * rendering, which necessitates rendering into another surface. */
  if (opacity != 1.0f || maskFrame || (clipPathFrame && !isTrivialClip)) {
    complexEffects = PR_TRUE;
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

PRBool
nsSVGUtils::HitTestClip(nsIFrame *aFrame, const nsPoint &aPoint)
{
  nsSVGEffects::EffectProperties props =
    nsSVGEffects::GetEffectProperties(aFrame);
  if (!props.mClipPath)
    return PR_TRUE;

  nsSVGClipPathFrame *clipPathFrame = props.GetClipPathFrame(nsnull);
  if (!clipPathFrame) {
    // clipPath is not a valid resource, so nothing gets painted, so
    // hit-testing must fail.
    return PR_FALSE;
  }

  nsCOMPtr<nsIDOMSVGMatrix> matrix = GetCanvasTM(aFrame);
  return clipPathFrame->ClipHitTest(aFrame, matrix, aPoint);
}

nsIFrame *
nsSVGUtils::HitTestChildren(nsIFrame *aFrame, const nsPoint &aPoint)
{
  // XXX: The frame's children are linked in a singly-linked list in document
  // order. If we were to hit test the children in this order we would need to
  // hit test *every* SVG frame, since even if we get a hit, later SVG frames
  // may lie on top of the matching frame. We really want to traverse SVG
  // frames in reverse order so we can stop at the first match. Since we don't
  // have a doubly-linked list, for the time being we traverse the
  // singly-linked list backwards by first reversing the nextSibling pointers
  // in place, and then restoring them when done.
  //
  // Note: While the child list pointers are reversed, any method which walks
  // the list would only encounter a single child!

  nsIFrame* current = nsnull;
  nsIFrame* next = aFrame->GetFirstChild(nsnull);

  nsIFrame* result = nsnull;

  // reverse sibling pointers
  while (next) {
    nsIFrame* temp = next->GetNextSibling();
    next->SetNextSibling(current);
    current = next;
    next = temp;    
  }

  // now do the backwards traversal
  while (current) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(current);
    if (SVGFrame) {
       result = SVGFrame->GetFrameForPoint(aPoint);
       if (result)
         break;
    }
    // restore current frame's sibling pointer
    nsIFrame* temp = current->GetNextSibling();
    current->SetNextSibling(next);
    next = current;
    current = temp;
  }

  // restore remaining pointers
  while (current) {
    nsIFrame* temp = current->GetNextSibling();
    current->SetNextSibling(next);
    next = current;
    current = temp;
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

nsRect
nsSVGUtils::ToAppPixelRect(nsPresContext *aPresContext,
                           double xmin, double ymin,
                           double xmax, double ymax)
{
  return ToAppPixelRect(aPresContext,
                        gfxRect(xmin, ymin, xmax - xmin, ymax - ymin));
}

nsRect
nsSVGUtils::ToAppPixelRect(nsPresContext *aPresContext, const gfxRect& rect)
{
  return nsRect(aPresContext->DevPixelsToAppUnits(NSToIntFloor(rect.X())),
                aPresContext->DevPixelsToAppUnits(NSToIntFloor(rect.Y())),
                aPresContext->DevPixelsToAppUnits(NSToIntCeil(rect.XMost()) - NSToIntFloor(rect.X())),
                aPresContext->DevPixelsToAppUnits(NSToIntCeil(rect.YMost()) - NSToIntFloor(rect.Y())));
}

gfxIntSize
nsSVGUtils::ConvertToSurfaceSize(const gfxSize& aSize, PRBool *aResultOverflows)
{
  gfxIntSize surfaceSize =
    gfxIntSize(PRInt32(aSize.width + 0.5), PRInt32(aSize.height + 0.5));

  *aResultOverflows = (aSize.width >= PR_INT32_MAX + 0.5 ||
                       aSize.height >= PR_INT32_MAX + 0.5 ||
                       aSize.width <= PR_INT32_MIN - 0.5 ||
                       aSize.height <= PR_INT32_MIN - 0.5);

  if (*aResultOverflows ||
      !gfxASurface::CheckSurfaceSize(surfaceSize)) {
    surfaceSize.width = PR_MIN(NS_SVG_OFFSCREEN_MAX_DIMENSION,
                               surfaceSize.width);
    surfaceSize.height = PR_MIN(NS_SVG_OFFSCREEN_MAX_DIMENSION,
                                surfaceSize.height);
    *aResultOverflows = PR_TRUE;
  }
  return surfaceSize;
}

gfxASurface *
nsSVGUtils::GetThebesComputationalSurface()
{
  if (!mThebesComputationalSurface) {
    nsRefPtr<gfxImageSurface> surface =
      new gfxImageSurface(gfxIntSize(1, 1), gfxASurface::ImageFormatARGB32);
    NS_ASSERTION(surface && !surface->CairoStatus(),
                 "Could not create offscreen surface");
    mThebesComputationalSurface = surface;
    // we want to keep this surface around
    NS_IF_ADDREF(mThebesComputationalSurface);
  }

  return mThebesComputationalSurface;
}

gfxMatrix
nsSVGUtils::ConvertSVGMatrixToThebes(nsIDOMSVGMatrix *aMatrix)
{
  float A, B, C, D, E, F;
  aMatrix->GetA(&A);
  aMatrix->GetB(&B);
  aMatrix->GetC(&C);
  aMatrix->GetD(&D);
  aMatrix->GetE(&E);
  aMatrix->GetF(&F);
  return gfxMatrix(A, B, C, D, E, F);
}

PRBool
nsSVGUtils::HitTestRect(nsIDOMSVGMatrix *aMatrix,
                        float aRX, float aRY, float aRWidth, float aRHeight,
                        float aX, float aY)
{
  PRBool result = PR_TRUE;

  if (aMatrix) {
    gfxContext ctx(GetThebesComputationalSurface());
    ctx.SetMatrix(ConvertSVGMatrixToThebes(aMatrix));

    ctx.NewPath();
    ctx.Rectangle(gfxRect(aRX, aRY, aRWidth, aRHeight));
    ctx.IdentityMatrix();

    if (!ctx.PointInFill(gfxPoint(aX, aY)))
      result = PR_FALSE;
  }

  return result;
}

void
nsSVGUtils::CompositeSurfaceMatrix(gfxContext *aContext,
                                   gfxASurface *aSurface,
                                   nsIDOMSVGMatrix *aCTM, float aOpacity)
{
  gfxMatrix matrix = ConvertSVGMatrixToThebes(aCTM);
  if (matrix.IsSingular())
    return;

  aContext->Save();

  aContext->Multiply(matrix);

  aContext->SetSource(aSurface);
  aContext->Paint(aOpacity);

  aContext->Restore();
}

void
nsSVGUtils::CompositePatternMatrix(gfxContext *aContext,
                                   gfxPattern *aPattern,
                                   nsIDOMSVGMatrix *aCTM, float aWidth, float aHeight, float aOpacity)
{
  gfxMatrix matrix = ConvertSVGMatrixToThebes(aCTM);
  if (matrix.IsSingular())
    return;

  aContext->Save();

  SetClipRect(aContext, aCTM, 0, 0, aWidth, aHeight);

  aContext->Multiply(matrix);

  aContext->SetPattern(aPattern);
  aContext->Paint(aOpacity);

  aContext->Restore();
}

void
nsSVGUtils::SetClipRect(gfxContext *aContext,
                        nsIDOMSVGMatrix *aCTM, float aX, float aY,
                        float aWidth, float aHeight)
{
  gfxMatrix matrix = ConvertSVGMatrixToThebes(aCTM);
  if (matrix.IsSingular())
    return;

  gfxMatrix oldMatrix = aContext->CurrentMatrix();
  aContext->Multiply(matrix);
  aContext->Clip(gfxRect(aX, aY, aWidth, aHeight));
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

nsresult
nsSVGUtils::GfxRectToIntRect(const gfxRect& aIn, nsIntRect* aOut)
{
  *aOut = nsIntRect(PRInt32(aIn.X()), PRInt32(aIn.Y()),
                    PRInt32(aIn.Width()), PRInt32(aIn.Height()));
  return gfxRect(aOut->x, aOut->y, aOut->width, aOut->height) == aIn
    ? NS_OK : NS_ERROR_FAILURE;
}

already_AddRefed<nsIDOMSVGRect>
nsSVGUtils::GetBBox(nsIFrame *aFrame)
{
  nsISVGChildFrame *svg = do_QueryFrame(aFrame);
  if (!svg) {
    nsIDOMSVGRect *rect = nsnull;
    gfxRect r = nsSVGIntegrationUtils::GetSVGBBoxForNonSVGFrame(aFrame);
    NS_NewSVGRect(&rect, r);
    return rect;
  }

  PRBool needToDisablePropagation = svg->GetMatrixPropagation();
  if (needToDisablePropagation) {
    svg->SetMatrixPropagation(PR_FALSE);
    svg->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                          nsISVGChildFrame::TRANSFORM_CHANGED);
  }
  
  nsCOMPtr<nsIDOMSVGRect> bbox;
  svg->GetBBox(getter_AddRefs(bbox));
  
  if (needToDisablePropagation) {
    svg->SetMatrixPropagation(PR_TRUE);
    svg->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                          nsISVGChildFrame::TRANSFORM_CHANGED);
  }

  return bbox.forget();
}

gfxRect
nsSVGUtils::GetRelativeRect(PRUint16 aUnits, const nsSVGLength2 *aXYWH,
                            nsIDOMSVGRect *aBBox, nsIFrame *aFrame)
{
  float x, y, width, height;
  if (aUnits == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    aBBox->GetX(&x);
    x += ObjectSpace(aBBox, &aXYWH[0]);
    aBBox->GetY(&y);
    y += ObjectSpace(aBBox, &aXYWH[1]);
    width = ObjectSpace(aBBox, &aXYWH[2]);
    height = ObjectSpace(aBBox, &aXYWH[3]);
  } else {
    x = nsSVGUtils::UserSpace(aFrame, &aXYWH[0]);
    y = nsSVGUtils::UserSpace(aFrame, &aXYWH[1]);
    width = nsSVGUtils::UserSpace(aFrame, &aXYWH[2]);
    height = nsSVGUtils::UserSpace(aFrame, &aXYWH[3]);
  }
  return gfxRect(x, y, width, height);
}

PRBool
nsSVGUtils::CanOptimizeOpacity(nsIFrame *aFrame)
{
  if (!aFrame->GetStyleSVGReset()->mFilter) {
    nsIAtom *type = aFrame->GetType();
    if (type == nsGkAtoms::svgImageFrame)
      return PR_TRUE;
    if (type == nsGkAtoms::svgPathGeometryFrame) {
      const nsStyleSVG *style = aFrame->GetStyleSVG();
      if (style->mFill.mType == eStyleSVGPaintType_None &&
          style->mStroke.mType == eStyleSVGPaintType_None)
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

float
nsSVGUtils::MaxExpansion(nsIDOMSVGMatrix *aMatrix)
{
  float a, b, c, d;
  aMatrix->GetA(&a);
  aMatrix->GetB(&b);
  aMatrix->GetC(&c);
  aMatrix->GetD(&d);

  // maximum expansion derivation from
  // http://lists.cairographics.org/archives/cairo/2004-October/001980.html
  float f = (a * a + b * b + c * c + d * d) / 2;
  float g = (a * a + b * b - c * c - d * d) / 2;
  float h = a * c + b * d;
  return sqrt(f + sqrt(g * g + h * h));
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGUtils::AdjustMatrixForUnits(nsIDOMSVGMatrix *aMatrix,
                                 nsSVGEnum *aUnits,
                                 nsIFrame *aFrame)
{
  nsCOMPtr<nsIDOMSVGMatrix> fini = aMatrix;

  if (aFrame &&
      aUnits->GetAnimValue() == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    float minx, miny, width, height;

    PRBool gotRect = PR_FALSE;
    if (aFrame->IsFrameOfType(nsIFrame::eSVG)) {
      nsISVGChildFrame *svgFrame = do_QueryFrame(aFrame);
      nsCOMPtr<nsIDOMSVGRect> rect;
      svgFrame->GetBBox(getter_AddRefs(rect));
      if (rect) {
        gotRect = PR_TRUE;
        rect->GetX(&minx);
        rect->GetY(&miny);
        rect->GetWidth(&width);
        rect->GetHeight(&height);
        // Correct for scaling in outersvg CTM
        nsPresContext *presCtx = aFrame->PresContext();
        float scaleInv =
          presCtx->AppUnitsToGfxUnits(presCtx->AppUnitsPerCSSPixel());
        minx /= scaleInv;
        miny /= scaleInv;
        width /= scaleInv;
        height /= scaleInv;
      }
    } else {
      gotRect = PR_TRUE;
      gfxRect r = nsSVGIntegrationUtils::GetSVGBBoxForNonSVGFrame(aFrame);
      minx = r.X();
      miny = r.Y();
      width = r.Width();
      height = r.Height();
    }

    if (gotRect) {
      nsCOMPtr<nsIDOMSVGMatrix> tmp;
      aMatrix->Translate(minx, miny, getter_AddRefs(tmp));
      tmp->ScaleNonUniform(width, height, getter_AddRefs(fini));
    }
  }

  nsIDOMSVGMatrix* retval = fini.get();
  NS_IF_ADDREF(retval);
  return retval;
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

// ----------------------------------------------------------------------

nsSVGRenderState::nsSVGRenderState(nsIRenderingContext *aContext) :
  mRenderMode(NORMAL), mRenderingContext(aContext)
{
  mGfxContext = aContext->ThebesContext();
}

nsSVGRenderState::nsSVGRenderState(gfxASurface *aSurface) :
  mRenderMode(NORMAL)
{
  mGfxContext = new gfxContext(aSurface);
}

nsIRenderingContext*
nsSVGRenderState::GetRenderingContext(nsIFrame *aFrame)
{
  if (!mRenderingContext) {
    nsIDeviceContext* devCtx = aFrame->PresContext()->DeviceContext();
    devCtx->CreateRenderingContextInstance(*getter_AddRefs(mRenderingContext));
    if (!mRenderingContext)
      return nsnull;
    mRenderingContext->Init(devCtx, mGfxContext);
  }
  return mRenderingContext;
}
