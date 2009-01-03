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
#include "nsSVGContainerFrame.h"
#include "nsSVGMaskElement.h"
#include "nsIDOMSVGMatrix.h"
#include "gfxContext.h"
#include "nsIDOMSVGRect.h"
#include "gfxImageSurface.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGMaskElement> mask = do_QueryInterface(aContent);

  if (!mask) {
    NS_ERROR("Can't create frame! Content is not an SVG mask");
    return nsnull;
  }

  return new (aPresShell) nsSVGMaskFrame(aContext);
}

already_AddRefed<gfxPattern>
nsSVGMaskFrame::ComputeMaskAlpha(nsSVGRenderState *aContext,
                                 nsIFrame* aParent,
                                 nsIDOMSVGMatrix* aMatrix,
                                 float aOpacity)
{
  // If the flag is set when we get here, it means this mask frame
  // has already been used painting the current mask, and the document
  // has a mask reference loop.
  if (mInUse) {
    NS_WARNING("Mask loop detected!");
    return nsnull;
  }
  AutoMaskReferencer maskRef(this);

  gfxContext *gfx = aContext->GetGfxContext();

  gfx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);

  {
    nsSVGMaskElement *mask = static_cast<nsSVGMaskElement*>(mContent);

    PRUint16 units =
      mask->mEnumAttributes[nsSVGMaskElement::MASKUNITS].GetAnimValue();
    nsCOMPtr<nsIDOMSVGRect> bbox;
    if (units == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
      bbox = nsSVGUtils::GetBBox(aParent);
      if (!bbox)
        return nsnull;
    }

    gfxRect maskArea = nsSVGUtils::GetRelativeRect(units,
      &mask->mLengthAttributes[nsSVGMaskElement::X], bbox, aParent);

    gfx->Save();
    nsSVGUtils::SetClipRect(gfx, aMatrix, maskArea.X(), maskArea.Y(),
                            maskArea.Width(), maskArea.Height());
  }

  mMaskParent = aParent;
  mMaskParentMatrix = aMatrix;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsSVGUtils::PaintFrameWithEffects(aContext, nsnull, kid);
  }

  gfxRect clipExtents = gfx->GetClipExtents();
  gfx->Restore();

  nsRefPtr<gfxPattern> pattern = gfx->PopGroup();
  if (!pattern || pattern->CairoStatus())
    return nsnull;

#ifdef DEBUG_tor
  fprintf(stderr, "clip extent: %f,%f %fx%f\n",
          clipExtents.X(), clipExtents.Y(),
          clipExtents.Width(), clipExtents.Height());
#endif

  PRBool resultOverflows;
  gfxIntSize surfaceSize =
    nsSVGUtils::ConvertToSurfaceSize(gfxSize(clipExtents.Width(),
                                             clipExtents.Height()),
                                     &resultOverflows);

  // 0 disables mask, < 0 is an error
  if (surfaceSize.width <= 0 || surfaceSize.height <= 0)
    return nsnull;

  if (resultOverflows)
    return nsnull;

  nsRefPtr<gfxImageSurface> image =
    new gfxImageSurface(surfaceSize, gfxASurface::ImageFormatARGB32);
  if (!image || image->CairoStatus())
    return nsnull;
  image->SetDeviceOffset(-clipExtents.pos);

  gfxContext transferCtx(image);
  transferCtx.SetOperator(gfxContext::OPERATOR_SOURCE);
  transferCtx.SetPattern(pattern);
  transferCtx.Paint();

  PRUint8 *data   = image->Data();
  PRInt32  stride = image->Stride();

  nsIntRect rect(0, 0, surfaceSize.width, surfaceSize.height);
  nsSVGUtils::UnPremultiplyImageDataAlpha(data, stride, rect);
  nsSVGUtils::ConvertImageDataToLinearRGB(data, stride, rect);

  for (PRInt32 y = 0; y < surfaceSize.height; y++)
    for (PRInt32 x = 0; x < surfaceSize.width; x++) {
      PRUint8 *pixel = data + stride * y + 4 * x;

      /* linearRGB -> intensity */
      PRUint8 alpha =
        static_cast<PRUint8>
                   ((pixel[GFX_ARGB32_OFFSET_R] * 0.2125 +
                        pixel[GFX_ARGB32_OFFSET_G] * 0.7154 +
                        pixel[GFX_ARGB32_OFFSET_B] * 0.0721) *
                       (pixel[GFX_ARGB32_OFFSET_A] / 255.0) * aOpacity);

      memset(pixel, alpha, 4);
    }

  gfxPattern *retval = new gfxPattern(image);
  NS_IF_ADDREF(retval);
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

  nsSVGMaskElement *mask = static_cast<nsSVGMaskElement*>(mContent);

  return nsSVGUtils::AdjustMatrixForUnits(mMaskParentMatrix,
                                          &mask->mEnumAttributes[nsSVGMaskElement::MASKCONTENTUNITS],
                                          mMaskParent);
}

