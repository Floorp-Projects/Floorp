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
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIDocument.h"
#include "nsSVGMaskFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGMaskElement.h"
#include "nsIDOMSVGMatrix.h"
#include "nsISVGCairoCanvas.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGMaskFrame(aContext);
}

nsresult
NS_GetSVGMaskFrame(nsSVGMaskFrame **aResult,
                   nsIURI *aURI, nsIContent *aContent)
{
  *aResult = nsnull;

  // Get the PresShell
  nsIDocument *myDoc = aContent->GetCurrentDoc();
  if (!myDoc) {
    NS_WARNING("No document for this content!");
    return NS_ERROR_FAILURE;
  }
  nsIPresShell *presShell = myDoc->GetShellAt(0);
  if (!presShell) {
    NS_WARNING("no presshell");
    return NS_ERROR_FAILURE;
  }

  // Find the referenced frame
  nsIFrame *cpframe;
  if (!NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&cpframe, aURI, aContent, presShell)))
    return NS_ERROR_FAILURE;

  nsIAtom* frameType = cpframe->GetType();
  if (frameType != nsLayoutAtoms::svgMaskFrame)
    return NS_ERROR_FAILURE;

  *aResult = (nsSVGMaskFrame *)cpframe;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGMaskFrame::InitSVG()
{
  nsresult rv = nsSVGMaskFrameBase::InitSVG();
  if (NS_FAILED(rv))
    return rv;

  mMaskParentMatrix = nsnull;

  nsCOMPtr<nsIDOMSVGMaskElement> mask = do_QueryInterface(mContent);
  NS_ASSERTION(mask, "wrong content element");

  return NS_OK;
}


/* sRGB -> linearRGB mapping table */
static unsigned char rgb2lin[256] = {
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   2,   2, 
  2,   2,   2,   2,   2,   3,   3,   3, 
  3,   3,   4,   4,   4,   4,   4,   5, 
  5,   5,   5,   6,   6,   6,   6,   7, 
  7,   7,   8,   8,   8,   9,   9,   9, 
 10,  10,  10,  11,  11,  11,  12,  12, 
 13,  13,  13,  14,  14,  15,  15,  16, 
 16,  16,  17,  17,  18,  18,  19,  19, 
 20,  20,  21,  22,  22,  23,  23,  24, 
 24,  25,  26,  26,  27,  27,  28,  29, 
 29,  30,  31,  31,  32,  33,  33,  34, 
 35,  36,  36,  37,  38,  38,  39,  40, 
 41,  42,  42,  43,  44,  45,  46,  47, 
 47,  48,  49,  50,  51,  52,  53,  54, 
 55,  55,  56,  57,  58,  59,  60,  61, 
 62,  63,  64,  65,  66,  67,  68,  70, 
 71,  72,  73,  74,  75,  76,  77,  78, 
 80,  81,  82,  83,  84,  85,  87,  88, 
 89,  90,  92,  93,  94,  95,  97,  98, 
 99, 101, 102, 103, 105, 106, 107, 109, 
110, 112, 113, 114, 116, 117, 119, 120, 
122, 123, 125, 126, 128, 129, 131, 132, 
134, 135, 137, 139, 140, 142, 144, 145, 
147, 148, 150, 152, 153, 155, 157, 159, 
160, 162, 164, 166, 167, 169, 171, 173, 
175, 176, 178, 180, 182, 184, 186, 188, 
190, 192, 193, 195, 197, 199, 201, 203, 
205, 207, 209, 211, 213, 215, 218, 220, 
222, 224, 226, 228, 230, 232, 235, 237, 
239, 241, 243, 245, 248, 250, 252, 255
};

cairo_pattern_t *
nsSVGMaskFrame::ComputeMaskAlpha(nsISVGRendererCanvas* aCanvas,
                                 nsISVGChildFrame* aParent,
                                 nsIDOMSVGMatrix* aMatrix,
                                 float aOpacity)
{
  nsCOMPtr<nsISVGCairoCanvas> cairoCanvas = do_QueryInterface(aCanvas);
  cairo_t *ctx = cairoCanvas->GetContext();
  cairo_pattern_t *pattern = nsnull;

  cairo_push_group(ctx);

  {
    nsIFrame *frame;
    CallQueryInterface(aParent, &frame);
    nsSVGElement *parent = NS_STATIC_CAST(nsSVGElement*, frame->GetContent());

    float x, y, width, height;

    PRUint16 units = GetMaskUnits();

    nsSVGMaskElement *mask = NS_STATIC_CAST(nsSVGMaskElement*, mContent);
    nsSVGLength2 *tmpX, *tmpY, *tmpWidth, *tmpHeight;
    tmpX = &mask->mLengthAttributes[nsSVGMaskElement::X];
    tmpY = &mask->mLengthAttributes[nsSVGMaskElement::Y];
    tmpWidth = &mask->mLengthAttributes[nsSVGMaskElement::WIDTH];
    tmpHeight = &mask->mLengthAttributes[nsSVGMaskElement::HEIGHT];

    if (units == nsIDOMSVGMaskElement::SVG_MUNITS_OBJECTBOUNDINGBOX) {

      aParent->SetMatrixPropagation(PR_FALSE);
      aParent->NotifyCanvasTMChanged(PR_TRUE);

      nsCOMPtr<nsIDOMSVGRect> bbox;
      aParent->GetBBox(getter_AddRefs(bbox));

      aParent->SetMatrixPropagation(PR_TRUE);
      aParent->NotifyCanvasTMChanged(PR_TRUE);

      if (!bbox)
        return nsnull;

#ifdef DEBUG_tor
      bbox->GetX(&x);
      bbox->GetY(&y);
      bbox->GetWidth(&width);
      bbox->GetHeight(&height);

      fprintf(stderr, "mask bbox: %f,%f %fx%f\n", x, y, width, height);
#endif

      bbox->GetX(&x);
      x += nsSVGUtils::ObjectSpace(bbox, tmpX);
      bbox->GetY(&y);
      y += nsSVGUtils::ObjectSpace(bbox, tmpY);
      width = nsSVGUtils::ObjectSpace(bbox, tmpWidth);
      height = nsSVGUtils::ObjectSpace(bbox, tmpHeight);
    } else {
      x = nsSVGUtils::UserSpace(parent, tmpX);
      y = nsSVGUtils::UserSpace(parent, tmpY);
      width = nsSVGUtils::UserSpace(parent, tmpWidth);
      height = nsSVGUtils::UserSpace(parent, tmpHeight);
    }

#ifdef DEBUG_tor
    fprintf(stderr, "mask clip: %f,%f %fx%f\n", x, y, width, height);
#endif

    if (NS_FAILED(aCanvas->SetClipRect(aMatrix, x, y, width, height))) {
      pattern = cairo_pop_group(ctx);
      if (pattern)
        cairo_pattern_destroy(pattern);
      return nsnull;
    }
  }

  mMaskParent = aParent,
  mMaskParentMatrix = aMatrix;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsSVGUtils::PaintChildWithEffects(aCanvas, nsnull, kid);
  }

  pattern = cairo_pop_group(ctx);
  if (!pattern)
    return nsnull;

  cairo_matrix_t patternMatrix;
  cairo_pattern_get_matrix(pattern, &patternMatrix);

  cairo_surface_t *surface = nsnull;
  cairo_pattern_get_surface(pattern, &surface);

  double x1, y1, x2, y2;
  cairo_clip_extents(ctx, &x1, &y1, &x2, &y2);

  PRUint32 clipWidth = PRUint32(ceil(x2) - floor(x1));
  PRUint32 clipHeight = PRUint32(ceil(y2) - floor(y1));

  cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                      clipWidth, clipHeight);
  if (!image)
    return nsnull;

  cairo_t *transferCtx = cairo_create(image);
  if (!transferCtx)
    return nsnull;

  cairo_set_source_surface(transferCtx, surface, 0, 0);
  cairo_paint(transferCtx);

  cairo_destroy(transferCtx);
  cairo_pattern_destroy(pattern);

  /* Now convert to intensity (sRGB -> linearRGB -> intensity) and
     store in alpha channel.  Reuse the transfer image instead of
     allocating another CAIRO_FORMAT_A8 image. */

  PRUint32 width  = cairo_image_surface_get_width(image);
  PRUint32 height = cairo_image_surface_get_height(image);
  PRUint8 *data   = cairo_image_surface_get_data(image);
  PRInt32  stride = cairo_image_surface_get_stride(image);

  for (PRUint32 y = 0; y < height; y++)
    for (PRUint32 x = 0; x < width; x++) {
      PRUint32 a;
      float r, g, b, intensity;

      /* un-premultiply and sRGB -> linearRGB conversion */
      a = data[stride * y + 4 * x + 3];
      if (a) {
        b = rgb2lin[(255 * (PRUint32)data[stride * y + 4 * x])     / a] / 255.0;
        g = rgb2lin[(255 * (PRUint32)data[stride * y + 4 * x + 1]) / a] / 255.0;
        r = rgb2lin[(255 * (PRUint32)data[stride * y + 4 * x + 2]) / a] / 255.0;
      } else {
        b = g = r = 0.0f;
      }

      /* linearRGB -> intensity */
      intensity = (r * 0.2125 + g * 0.7154 + b * 0.0721) * (a / 255.0) * aOpacity;

      data[stride * y + 4 * x] = 255;
      data[stride * y + 4 * x + 1] = 255;
      data[stride * y + 4 * x + 2] = 255;
      data[stride * y + 4 * x + 3] = (unsigned char)(intensity * 255);
    }

  cairo_pattern_t *retval = cairo_pattern_create_for_surface(image);
  cairo_surface_destroy(image);

  if (retval)
    cairo_pattern_set_matrix(retval, &patternMatrix);

  return retval;
}

nsIAtom *
nsSVGMaskFrame::GetType() const
{
  return nsLayoutAtoms::svgMaskFrame;
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGMaskFrame::GetCanvasTM()
{
  NS_ASSERTION(mMaskParentMatrix, "null parent matrix");

  nsCOMPtr<nsIDOMSVGMatrix> canvasTM = mMaskParentMatrix;

  /* object bounding box? */
  PRUint16 units = GetMaskContentUnits();

  if (mMaskParent &&
      units == nsIDOMSVGMaskElement::SVG_MUNITS_OBJECTBOUNDINGBOX) {
    nsCOMPtr<nsIDOMSVGRect> rect;
    nsresult rv = mMaskParent->GetBBox(getter_AddRefs(rect));

    if (NS_SUCCEEDED(rv)) {
      float minx, miny, width, height;
      rect->GetX(&minx);
      rect->GetY(&miny);
      rect->GetWidth(&width);
      rect->GetHeight(&height);

      nsCOMPtr<nsIDOMSVGMatrix> tmp, fini;
      canvasTM->Translate(minx, miny, getter_AddRefs(tmp));
      tmp->ScaleNonUniform(width, height, getter_AddRefs(fini));
      canvasTM = fini;
    }
  }

  nsIDOMSVGMatrix* retval = canvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

// -------------------------------------------------------------------------
// Helper functions
// -------------------------------------------------------------------------

PRUint16
nsSVGMaskFrame::GetMaskUnits()
{
  PRUint16 rv;

  nsSVGMaskElement *maskElement = NS_STATIC_CAST(nsSVGMaskElement*, mContent);
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> units;
  maskElement->GetMaskUnits(getter_AddRefs(units));
  units->GetAnimVal(&rv);
  return rv;
}

PRUint16
nsSVGMaskFrame::GetMaskContentUnits()
{
  PRUint16 rv;

  nsSVGMaskElement *maskElement = NS_STATIC_CAST(nsSVGMaskElement*, mContent);
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> units;
  maskElement->GetMaskContentUnits(getter_AddRefs(units));
  units->GetAnimVal(&rv);
  return rv;
}

