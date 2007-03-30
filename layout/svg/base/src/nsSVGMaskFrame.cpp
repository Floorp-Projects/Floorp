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
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGMaskElement.h"
#include "nsIDOMSVGMatrix.h"
#include "gfxContext.h"
#include "nsIDOMSVGRect.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGMaskFrame(aContext);
}

nsSVGMaskFrame *
NS_GetSVGMaskFrame(nsIURI *aURI, nsIContent *aContent)
{
  // Get the PresShell
  nsIDocument *myDoc = aContent->GetCurrentDoc();
  if (!myDoc) {
    NS_WARNING("No document for this content!");
    return nsnull;
  }
  nsIPresShell *presShell = myDoc->GetShellAt(0);
  if (!presShell) {
    NS_WARNING("no presshell");
    return nsnull;
  }

  // Find the referenced frame
  nsIFrame *cpframe;
  if (!NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&cpframe, aURI, aContent, presShell)))
    return nsnull;

  nsIAtom* frameType = cpframe->GetType();
  if (frameType != nsGkAtoms::svgMaskFrame)
    return nsnull;

  return NS_STATIC_CAST(nsSVGMaskFrame *, cpframe);
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


cairo_pattern_t *
nsSVGMaskFrame::ComputeMaskAlpha(nsSVGRenderState *aContext,
                                 nsISVGChildFrame* aParent,
                                 nsIDOMSVGMatrix* aMatrix,
                                 float aOpacity)
{
  gfxContext *gfx = aContext->GetGfxContext();
  cairo_t *ctx = gfx->GetCairo();

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

    gfx->Save();
    nsSVGUtils::SetClipRect(gfx, aMatrix, x, y, width, height);
  }

  mMaskParent = aParent,
  mMaskParentMatrix = aMatrix;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsSVGUtils::PaintChildWithEffects(aContext, nsnull, kid);
  }

  gfx->Restore();

  cairo_pattern_t *pattern = cairo_pop_group(ctx);
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
  if (cairo_status(transferCtx) != CAIRO_STATUS_SUCCESS) {
    cairo_destroy(transferCtx);
    cairo_surface_destroy(image);
    cairo_pattern_destroy(pattern);
    return nsnull;
  }

  cairo_set_source_surface(transferCtx, surface, 0, 0);
  cairo_paint(transferCtx);

  cairo_destroy(transferCtx);
  cairo_pattern_destroy(pattern);

  PRUint32 width  = cairo_image_surface_get_width(image);
  PRUint32 height = cairo_image_surface_get_height(image);
  PRUint8 *data   = cairo_image_surface_get_data(image);
  PRInt32  stride = cairo_image_surface_get_stride(image);

  nsRect rect(0, 0, width, height);
  nsSVGUtils::UnPremultiplyImageDataAlpha(data, stride, rect);
  nsSVGUtils::ConvertImageDataToLinearRGB(data, stride, rect);

  for (PRUint32 y = 0; y < height; y++)
    for (PRUint32 x = 0; x < width; x++) {
      PRUint8 *pixel = data + stride * y + 4 * x;

      /* linearRGB -> intensity */
      PRUint8 alpha =
        NS_STATIC_CAST(PRUint8,
                       (pixel[GFX_ARGB32_OFFSET_R] * 0.2125 +
                        pixel[GFX_ARGB32_OFFSET_G] * 0.7154 +
                        pixel[GFX_ARGB32_OFFSET_B] * 0.0721) *
                       (pixel[GFX_ARGB32_OFFSET_A] / 255.0) * aOpacity);

      memset(pixel, alpha, 4);
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
  return nsGkAtoms::svgMaskFrame;
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

