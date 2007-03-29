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

#include "nsSVGLength.h"
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
#include "nsIDOMSVGAnimPresAspRatio.h"
#include "nsIDOMSVGPresAspectRatio.h"
#include "nsSVGMatrix.h"
#include "nsSVGFilterFrame.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGMaskFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGLength2.h"
#include "nsGenericElement.h"
#include "nsAttrValue.h"
#include "nsSVGGeometryFrame.h"
#include "nsIScriptError.h"
#include "cairo.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "gfxImageSurface.h"
#include "gfxMatrix.h"
#include "nsStubMutationObserver.h"

class nsSVGFilterProperty : public nsStubMutationObserver {
public:
  nsSVGFilterProperty(nsSVGFilterFrame *aFilter, nsIFrame *aFrame);

  nsRect GetRect() { return mFilterRect; }
  nsSVGFilterFrame *GetFilterFrame() { return mFilter; }
  void RemoveMutationObserver();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIMutationObserver
  virtual void AttributeChanged(nsIDocument* aDocument, nsIContent* aContent,
                                PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                                PRInt32 aModType);
  virtual void ContentAppended(nsIDocument* aDocument, nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer);
  virtual void ContentInserted(nsIDocument* aDocument, nsIContent* aContainer,
                               nsIContent* aChild, PRInt32 aIndexInContainer);
  virtual void ContentRemoved(nsIDocument* aDocument, nsIContent* aContainer,
                              nsIContent* aChild, PRInt32 aIndexInContainer);
  virtual void ParentChainChanged(nsIContent *aContent);

private:
  void DoUpdate();

  nsWeakPtr mObservedFilter;
  nsSVGFilterFrame *mFilter;
  nsIFrame *mFrame;  // frame being filtered
  nsRect mFilterRect;
};

NS_IMPL_ISUPPORTS1(nsSVGFilterProperty, nsIMutationObserver)

nsSVGFilterProperty::nsSVGFilterProperty(nsSVGFilterFrame *aFilter,
                                         nsIFrame *aFrame)
  : mFilter(aFilter), mFrame(aFrame)
{
  mFilterRect = mFilter->GetInvalidationRegion(mFrame);

  nsIFrame *filter = nsnull;
  CallQueryInterface(mFilter, &filter);

  nsCOMPtr<nsIContent> filterContent = filter->GetContent();
  mObservedFilter = do_GetWeakReference(filterContent);

  filterContent->AddMutationObserver(this);
}

void
nsSVGFilterProperty::RemoveMutationObserver()
{
  nsCOMPtr<nsIContent> filter = do_QueryReferent(mObservedFilter);
  if (filter)
    filter->RemoveMutationObserver(this);
}

void
nsSVGFilterProperty::DoUpdate()
{
  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(mFrame);
  if (outerSVGFrame) {
    outerSVGFrame->InvalidateRect(mFilterRect);
    mFilterRect = mFilter->GetInvalidationRegion(mFrame);
    outerSVGFrame->InvalidateRect(mFilterRect);
  }
}

void
nsSVGFilterProperty::AttributeChanged(nsIDocument *aDocument,
                                      nsIContent *aContent,
                                      PRInt32 aNameSpaceID,
                                      nsIAtom *aAttribute,
                                      PRInt32 aModType)
{
  DoUpdate();
}

void
nsSVGFilterProperty::ContentAppended(nsIDocument *aDocument,
                                     nsIContent *aContainer,
                                     PRInt32 aNewIndexInContainer)
{
  DoUpdate();
}

void
nsSVGFilterProperty::ContentInserted(nsIDocument *aDocument,
                                     nsIContent *aContainer,
                                     nsIContent *aChild,
                                     PRInt32 aIndexInContainer)
{
  DoUpdate();
}

void
nsSVGFilterProperty::ContentRemoved(nsIDocument *aDocument,
                                    nsIContent *aContainer,
                                    nsIContent *aChild,
                                    PRInt32 aIndexInContainer)
{
  DoUpdate();
}

void
nsSVGFilterProperty::ParentChainChanged(nsIContent *aContent)
{
  if (aContent->IsInDoc())
    return;

  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(mFrame);
  if (outerSVGFrame)
    outerSVGFrame->InvalidateRect(mFilterRect);

  RemoveMutationObserver();
  mFrame->RemoveStateBits(NS_STATE_SVG_FILTERED);
  mFrame->DeleteProperty(nsGkAtoms::filter);
}

cairo_surface_t *nsSVGUtils::mCairoComputationalSurface = nsnull;
gfxASurface     *nsSVGUtils::mThebesComputationalSurface = nsnull;

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

PR_STATIC_CALLBACK(int)
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

void
nsSVGUtils::UnPremultiplyImageDataAlpha(PRUint8 *data, 
                                        PRInt32 stride,
                                        const nsRect &rect)
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
                                      const nsRect &rect)
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
                                        const nsRect &rect)
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
                                          const nsRect &rect)
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
  float val = 0.0f;

  switch (aCoord.GetUnit()) {
  case eStyleUnit_Factor:
    // user units
    val = aCoord.GetFactorValue();
    break;

  case eStyleUnit_Coord:
    val = nsPresContext::AppUnitsToFloatCSSPixels(aCoord.GetCoordValue());
    break;

  case eStyleUnit_Percent: {
      nsCOMPtr<nsISVGLength> length;
      NS_NewSVGLength(getter_AddRefs(length),
                      aCoord.GetPercentValue() * 100.0f,
                      nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);

      if (!length)
        break;

      nsWeakPtr weakCtx =
        do_GetWeakReference(NS_STATIC_CAST(nsGenericElement*, aContent));
      length->SetContext(weakCtx, nsSVGUtils::XY);
      length->GetValue(&val);
      break;
    }
  default:
    break;
  }

  return val;
}

nsresult nsSVGUtils::GetReferencedFrame(nsIFrame **aRefFrame, nsIURI* aURI, nsIContent *aContent, 
                                        nsIPresShell *aPresShell)
{
  *aRefFrame = nsnull;

  nsIContent* content = nsContentUtils::GetReferencedElement(aURI, aContent);
  if (!content)
    return NS_ERROR_FAILURE;

  // Get the Primary Frame
  NS_ASSERTION(aPresShell, "Get referenced SVG frame -- no pres shell provided");
  if (!aPresShell)
    return NS_ERROR_FAILURE;

  *aRefFrame = aPresShell->GetPrimaryFrameFor(content);
  if (!(*aRefFrame)) return NS_ERROR_FAILURE;
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
    nsISVGChildFrame* SVGFrame = nsnull;
    CallQueryInterface(kid, &SVGFrame);
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
nsSVGUtils::FindFilterInvalidation(nsIFrame *aFrame)
{
  nsRect rect;

  while (aFrame) {
    if (aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG)
      break;

    if (aFrame->GetStateBits() & NS_STATE_SVG_FILTERED) {
      nsSVGFilterProperty *property;
      property = NS_STATIC_CAST(nsSVGFilterProperty *,
                                aFrame->GetProperty(nsGkAtoms::filter));
      rect = property->GetRect();
    }
    aFrame = aFrame->GetParent();
  }

  return rect;
}

float
nsSVGUtils::ObjectSpace(nsIDOMSVGRect *aRect, nsSVGLength2 *aLength)
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
    axis = sqrt(width * width + height * height)/sqrt(2.0f);
  }
  }

  if (aLength->GetSpecifiedUnitType() ==
      nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE) {
    fraction = aLength->GetAnimValInSpecifiedUnits() / 100;
  } else
    fraction = aLength->GetAnimValue(NS_STATIC_CAST(nsSVGSVGElement*,
                                                    nsnull));

  return fraction * axis;
}

float
nsSVGUtils::UserSpace(nsSVGElement *aSVGElement, nsSVGLength2 *aLength)
{
  return aLength->GetAnimValue(aSVGElement);
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
  float delta = fmod(a2 - a1, NS_STATIC_CAST(float, 2*M_PI));
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
      return NS_STATIC_CAST(nsSVGOuterSVGFrame*, aFrame);
    }
    aFrame = aFrame->GetParent();
  }

  return nsnull;
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGUtils::GetViewBoxTransform(float aViewportWidth, float aViewportHeight,
                                float aViewboxX, float aViewboxY,
                                float aViewboxWidth, float aViewboxHeight,
                                nsIDOMSVGAnimatedPreserveAspectRatio *aPreserveAspectRatio,
                                PRBool aIgnoreAlign)
{
  PRUint16 align, meetOrSlice;
  {
    nsCOMPtr<nsIDOMSVGPreserveAspectRatio> par;
    aPreserveAspectRatio->GetAnimVal(getter_AddRefs(par));
    NS_ASSERTION(par, "could not get preserveAspectRatio");
    par->GetAlign(&align);
    par->GetMeetOrSlice(&meetOrSlice);
  }

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
    if (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
        a < d ||
        meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE &&
        d < a) {
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
      meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
      d < a ||
      meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE &&
      a < d) {
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
  if (!aFrame->IsLeaf()) {
    nsSVGContainerFrame *containerFrame = NS_STATIC_CAST(nsSVGContainerFrame*,
                                                         aFrame);
    return containerFrame->GetCanvasTM();
  }

  nsSVGGeometryFrame *geometryFrame = NS_STATIC_CAST(nsSVGGeometryFrame*,
                                                     aFrame);
  nsCOMPtr<nsIDOMSVGMatrix> matrix;
  nsIDOMSVGMatrix *retval;
  geometryFrame->GetCanvasTM(getter_AddRefs(matrix));
  retval = matrix.get();
  NS_IF_ADDREF(retval);
  return retval;
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
// Effect helper functions

static void
AddEffectProperties(nsIFrame *aFrame)
{
  const nsStyleSVGReset *style = aFrame->GetStyleSVGReset();

  if (style->mFilter && !(aFrame->GetStateBits() & NS_STATE_SVG_FILTERED)) {
    nsSVGFilterFrame *filter;
    NS_GetSVGFilterFrame(&filter, style->mFilter, aFrame->GetContent());
    if (filter) {
      nsSVGFilterProperty *property = new nsSVGFilterProperty(filter, aFrame);
      if (!property) {
        NS_ERROR("Could not create filter property");
        return;
      }
      NS_ADDREF(property); // addref to allow QI - FilterPropertyDtor releases
      aFrame->SetProperty(nsGkAtoms::filter,
                          NS_STATIC_CAST(nsISupports*, property),
                          nsPropertyTable::SupportsDtorFunc);
      aFrame->AddStateBits(NS_STATE_SVG_FILTERED);
    }
  }

  if (style->mClipPath && !(aFrame->GetStateBits() & NS_STATE_SVG_CLIPPED_MASK)) {
    nsSVGClipPathFrame *clip;
    NS_GetSVGClipPathFrame(&clip, style->mClipPath, aFrame->GetContent());
    if (clip) {
      aFrame->SetProperty(nsGkAtoms::clipPath, clip);

      PRBool trivialClip;
      clip->IsTrivial(&trivialClip);
      if (trivialClip)
        aFrame->AddStateBits(NS_STATE_SVG_CLIPPED_TRIVIAL);
      else
        aFrame->AddStateBits(NS_STATE_SVG_CLIPPED_COMPLEX);
    }
  }

  if (style->mMask && !(aFrame->GetStateBits() & NS_STATE_SVG_MASKED)) {
    nsSVGMaskFrame *mask;
    NS_GetSVGMaskFrame(&mask, style->mMask, aFrame->GetContent());
    if (mask) {
      aFrame->SetProperty(nsGkAtoms::mask, mask);
      aFrame->AddStateBits(NS_STATE_SVG_MASKED);
    }
  }
}

static cairo_pattern_t *
GetComplexClipSurface(nsSVGRenderState *aContext, nsIFrame *aFrame)
{
  cairo_pattern_t *pattern = nsnull;

  if (aFrame->GetStateBits() & NS_STATE_SVG_CLIPPED_COMPLEX) {
    nsISVGChildFrame *svgChildFrame;
    CallQueryInterface(aFrame, &svgChildFrame);

    nsSVGClipPathFrame *clip;
    clip = NS_STATIC_CAST(nsSVGClipPathFrame *,
                          aFrame->GetProperty(nsGkAtoms::clipPath));

    cairo_t *ctx = aContext->GetGfxContext()->GetCairo();

    cairo_push_group(ctx);

    nsCOMPtr<nsIDOMSVGMatrix> matrix = nsSVGUtils::GetCanvasTM(aFrame);
    nsresult rv = clip->ClipPaint(aContext, svgChildFrame, matrix);
    pattern = cairo_pop_group(ctx);

    if (NS_FAILED(rv) && pattern) {
      cairo_pattern_destroy(pattern);
      pattern = nsnull;
    }
  }

  return pattern;
}

static cairo_pattern_t *
GetMaskSurface(nsSVGRenderState *aContext, nsIFrame *aFrame, float opacity)
{
  if (aFrame->GetStateBits() & NS_STATE_SVG_MASKED) {
    nsISVGChildFrame *svgChildFrame;
    CallQueryInterface(aFrame, &svgChildFrame);

    nsSVGMaskFrame *mask;
    mask = NS_STATIC_CAST(nsSVGMaskFrame *,
                          aFrame->GetProperty(nsGkAtoms::mask));

    nsCOMPtr<nsIDOMSVGMatrix> matrix = nsSVGUtils::GetCanvasTM(aFrame);
    return mask->ComputeMaskAlpha(aContext, svgChildFrame, matrix, opacity);
  }

  return nsnull;
}


// ************************************************************

void
nsSVGUtils::PaintChildWithEffects(nsSVGRenderState *aContext,
                                  nsRect *aDirtyRect,
                                  nsIFrame *aFrame)
{
  nsISVGChildFrame *svgChildFrame;
  CallQueryInterface(aFrame, &svgChildFrame);

  if (!svgChildFrame)
    return;

  float opacity = aFrame->GetStyleDisplay()->mOpacity;
  if (opacity == 0.0f)
    return;

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */

  AddEffectProperties(aFrame);
  nsFrameState state = aFrame->GetStateBits();

  /* Check if we need to draw anything */
  if (aDirtyRect) {
    if (state & NS_STATE_SVG_FILTERED) {
      if (!aDirtyRect->Intersects(FindFilterInvalidation(aFrame)))
        return;
    } else if (svgChildFrame->HasValidCoveredRect()) {
      if (!aDirtyRect->Intersects(aFrame->GetRect()))
        return;
    }
  }

  /* SVG defines the following rendering model:
   *
   *  1. Render geometry
   *  2. Apply filter
   *  3. Apply clipping, masking, group opacity
   *
   * We follow this, but perform a couple optimizations:
   *
   * + Use cairo's clipPath when representable natively (single object
   *   clip region).
   *
   * + Merge opacity and masking if both used together.
   */

  if (opacity != 1.0 && nsSVGUtils::CanOptimizeOpacity(aFrame))
    opacity = 1.0;

  gfxContext *gfx = aContext->GetGfxContext();
  cairo_t *ctx = nsnull;

  /* Check if we need to do additional operations on this child's
   * rendering, which necessitates rendering into another surface. */
  if (opacity != 1.0 ||
      state & (NS_STATE_SVG_CLIPPED_COMPLEX | NS_STATE_SVG_MASKED)) {
    ctx = gfx->GetCairo();
    cairo_save(ctx);
    cairo_push_group(ctx);
  }

  /* If this frame has only a trivial clipPath, set up cairo's clipping now so
   * we can just do normal painting and get it clipped appropriately.
   */
  if (state & NS_STATE_SVG_CLIPPED_TRIVIAL) {
    nsSVGClipPathFrame *clip;
    clip = NS_STATIC_CAST(nsSVGClipPathFrame *,
                          aFrame->GetProperty(nsGkAtoms::clipPath));

    gfx->Save();
    nsCOMPtr<nsIDOMSVGMatrix> matrix = GetCanvasTM(aFrame);
    clip->ClipPaint(aContext, svgChildFrame, matrix);
  }

  /* Paint the child */
  if (state & NS_STATE_SVG_FILTERED) {
    nsSVGFilterProperty *property;
    property = NS_STATIC_CAST(nsSVGFilterProperty *,
                              aFrame->GetProperty(nsGkAtoms::filter));
    property->GetFilterFrame()->FilterPaint(aContext, svgChildFrame);
  } else {
    svgChildFrame->PaintSVG(aContext, aDirtyRect);
  }

  if (state & NS_STATE_SVG_CLIPPED_TRIVIAL) {
    gfx->Restore();
  }

  /* No more effects, we're done. */
  if (!ctx)
    return;

  cairo_pop_group_to_source(ctx);

  cairo_pattern_t *maskSurface     = GetMaskSurface(aContext, aFrame, opacity);
  cairo_pattern_t *clipMaskSurface = GetComplexClipSurface(aContext, aFrame);

  if (clipMaskSurface) {
    // Still more set after clipping, so clip to another surface
    if (maskSurface || opacity != 1.0) {
      cairo_push_group(ctx);
      cairo_mask(ctx, clipMaskSurface);
      cairo_pop_group_to_source(ctx);
    } else {
      cairo_mask(ctx, clipMaskSurface);
    }
  }

  if (maskSurface) {
    cairo_mask(ctx, maskSurface);
  } else if (opacity != 1.0) {
    cairo_paint_with_alpha(ctx, opacity);
  }

  cairo_restore(ctx);

  if (maskSurface)
    cairo_pattern_destroy(maskSurface);
  if (clipMaskSurface)
    cairo_pattern_destroy(clipMaskSurface);
}

void
nsSVGUtils::StyleEffects(nsIFrame *aFrame)
{
  nsFrameState state = aFrame->GetStateBits();

  /* clear out all effects */

  if (state & NS_STATE_SVG_CLIPPED_MASK) {
    aFrame->DeleteProperty(nsGkAtoms::clipPath);
  }

  if (state & NS_STATE_SVG_FILTERED) {
    nsSVGFilterProperty *property;
    property = NS_STATIC_CAST(nsSVGFilterProperty *,
                              aFrame->GetProperty(nsGkAtoms::filter));
    if (property)
      property->RemoveMutationObserver();
    aFrame->DeleteProperty(nsGkAtoms::filter);
  }

  if (state & NS_STATE_SVG_MASKED) {
    aFrame->DeleteProperty(nsGkAtoms::mask);
  }

  aFrame->RemoveStateBits(NS_STATE_SVG_CLIPPED_MASK |
                          NS_STATE_SVG_FILTERED |
                          NS_STATE_SVG_MASKED);
}

PRBool
nsSVGUtils::HitTestClip(nsIFrame *aFrame, float x, float y)
{
  PRBool clipHit = PR_TRUE;

  nsISVGChildFrame* SVGFrame;
  CallQueryInterface(aFrame, &SVGFrame);

  if (aFrame->GetStateBits() & NS_STATE_SVG_CLIPPED_MASK) {
    nsSVGClipPathFrame *clip;
    clip = NS_STATIC_CAST(nsSVGClipPathFrame *,
                          aFrame->GetProperty(nsGkAtoms::clipPath));
    nsCOMPtr<nsIDOMSVGMatrix> matrix = GetCanvasTM(aFrame);
    clip->ClipHitTest(SVGFrame, matrix, x, y, &clipHit);
  }

  return clipHit;
}

void
nsSVGUtils::HitTestChildren(nsIFrame *aFrame, float x, float y,
                            nsIFrame **aResult)
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

  *aResult = nsnull;

  nsIFrame* current = nsnull;
  nsIFrame* next = aFrame->GetFirstChild(nsnull);

  // reverse sibling pointers
  while (next) {
    nsIFrame* temp = next->GetNextSibling();
    next->SetNextSibling(current);
    current = next;
    next = temp;    
  }

  // now do the backwards traversal
  while (current) {
    nsISVGChildFrame* SVGFrame;
    CallQueryInterface(current, &SVGFrame);
    if (SVGFrame) {
      if (NS_SUCCEEDED(SVGFrame->GetFrameForPointSVG(x, y, aResult)) &&
          *aResult)
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

  if (*aResult && !HitTestClip(aFrame, x, y))
    *aResult = nsnull;
}

nsRect
nsSVGUtils::GetCoveredRegion(const nsFrameList &aFrames)
{
  nsRect rect;

  for (nsIFrame* kid = aFrames.FirstChild();
       kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* child = nsnull;
    CallQueryInterface(kid, &child);
    if (child) {
      nsRect childRect = child->GetCoveredRegion();
      rect.UnionRect(rect, childRect);
    }
  }

  return rect;
}

nsRect
nsSVGUtils::ToBoundingPixelRect(double xmin, double ymin,
                                double xmax, double ymax)
{
  return nsRect(nscoord(floor(xmin)),
                nscoord(floor(ymin)),
                nscoord(ceil(xmax) - floor(xmin)),
                nscoord(ceil(ymax) - floor(ymin)));
}

nsRect
nsSVGUtils::ToBoundingPixelRect(const gfxRect& rect)
{
  return nsRect(nscoord(floor(rect.X())),
                nscoord(floor(rect.Y())),
                nscoord(ceil(rect.XMost()) - floor(rect.X())),
                nscoord(ceil(rect.YMost()) - floor(rect.Y())));
}

cairo_surface_t *
nsSVGUtils::GetCairoComputationalSurface()
{
  if (!mCairoComputationalSurface)
    mCairoComputationalSurface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);

  return mCairoComputationalSurface;
}

gfxASurface *
nsSVGUtils::GetThebesComputationalSurface()
{
  if (!mThebesComputationalSurface) {
    mThebesComputationalSurface =
      new gfxImageSurface(GetCairoComputationalSurface());
    // we want to keep this surface around
    NS_IF_ADDREF(mThebesComputationalSurface);
  }

  return mThebesComputationalSurface;
}

cairo_matrix_t
nsSVGUtils::ConvertSVGMatrixToCairo(nsIDOMSVGMatrix *aMatrix)
{
  float A, B, C, D, E, F;
  aMatrix->GetA(&A);
  aMatrix->GetB(&B);
  aMatrix->GetC(&C);
  aMatrix->GetD(&D);
  aMatrix->GetE(&E);
  aMatrix->GetF(&F);
  cairo_matrix_t m = { A, B, C, D, E, F };
  return m;
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
    cairo_matrix_t matrix = ConvertSVGMatrixToCairo(aMatrix);
    cairo_t *ctx = cairo_create(GetCairoComputationalSurface());
    if (cairo_status(ctx) != CAIRO_STATUS_SUCCESS) {
      cairo_destroy(ctx);
      return PR_FALSE;
    }
    cairo_set_tolerance(ctx, 1.0);

    cairo_set_matrix(ctx, &matrix);
    cairo_new_path(ctx);
    cairo_rectangle(ctx, aRX, aRY, aRWidth, aRHeight);
    cairo_identity_matrix(ctx);

    if (!cairo_in_fill(ctx, aX, aY))
      result = PR_FALSE;

    cairo_destroy(ctx);
  }

  return result;
}

void
nsSVGUtils::UserToDeviceBBox(cairo_t *ctx,
                             double *xmin, double *ymin,
                             double *xmax, double *ymax)
{
  double x[3], y[3];
  x[0] = *xmin;  y[0] = *ymax;
  x[1] = *xmax;  y[1] = *ymax;
  x[2] = *xmax;  y[2] = *ymin;

  cairo_user_to_device(ctx, xmin, ymin);
  *xmax = *xmin;
  *ymax = *ymin;
  for (int i = 0; i < 3; i++) {
    cairo_user_to_device(ctx, &x[i], &y[i]);
    *xmin = PR_MIN(*xmin, x[i]);
    *xmax = PR_MAX(*xmax, x[i]);
    *ymin = PR_MIN(*ymin, y[i]);
    *ymax = PR_MAX(*ymax, y[i]);
  }
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

PRBool
nsSVGUtils::CanOptimizeOpacity(nsIFrame *aFrame)
{
  if (!(aFrame->GetStateBits() & NS_STATE_SVG_FILTERED)) {
    nsIAtom *type = aFrame->GetType();
    if (type == nsGkAtoms::svgImageFrame)
      return PR_TRUE;
    if (type == nsGkAtoms::svgPathGeometryFrame) {
      nsSVGGeometryFrame *geom = NS_STATIC_CAST(nsSVGGeometryFrame*, aFrame);
      if (!(geom->HasFill() && geom->HasStroke()))
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

// ----------------------------------------------------------------------

nsSVGRenderState::nsSVGRenderState(nsIRenderingContext *aContext) :
  mRenderMode(NORMAL), mRenderingContext(aContext)
{
  mGfxContext = NS_STATIC_CAST(gfxContext*,
                               aContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT));
}

nsSVGRenderState::nsSVGRenderState(gfxContext *aContext) :
  mRenderMode(NORMAL), mRenderingContext(nsnull), mGfxContext(aContext)
{
}
