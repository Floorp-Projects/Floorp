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

#include "nsSVGMaskFrame.h"

#include "nsIDocument.h"
#include "nsRenderingContext.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGMaskElement.h"
#include "nsSVGEffects.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGMaskFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGMaskFrame)

already_AddRefed<gfxPattern>
nsSVGMaskFrame::ComputeMaskAlpha(nsRenderingContext *aContext,
                                 nsIFrame* aParent,
                                 const gfxMatrix &aMatrix,
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

  nsSVGMaskElement *mask = static_cast<nsSVGMaskElement*>(mContent);

  PRUint16 units =
    mask->mEnumAttributes[nsSVGMaskElement::MASKUNITS].GetAnimValue();
  gfxRect bbox;
  if (units == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    bbox = nsSVGUtils::GetBBox(aParent);
  }

  gfxRect maskArea = nsSVGUtils::GetRelativeRect(units,
    &mask->mLengthAttributes[nsSVGMaskElement::X], bbox, aParent);

  gfxContext *gfx = aContext->ThebesContext();

  gfx->Save();
  nsSVGUtils::SetClipRect(gfx, aMatrix, maskArea);
  gfxRect clipExtents = gfx->GetClipExtents();
  clipExtents.RoundOut();
  gfx->Restore();

#ifdef DEBUG_tor
  fprintf(stderr, "clip extent: %f,%f %fx%f\n",
          clipExtents.X(), clipExtents.Y(),
          clipExtents.Width(), clipExtents.Height());
#endif

  bool resultOverflows;
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
  image->SetDeviceOffset(-clipExtents.TopLeft());

  nsRenderingContext tmpCtx;
  tmpCtx.Init(this->PresContext()->DeviceContext(), image);

  mMaskParent = aParent;
  if (mMaskParentMatrix) {
    *mMaskParentMatrix = aMatrix;
  } else {
    mMaskParentMatrix = new gfxMatrix(aMatrix);
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    // The CTM of each frame referencing us can be different
    nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      SVGFrame->NotifySVGChanged(
                          nsISVGChildFrame::DO_NOT_NOTIFY_RENDERING_OBSERVERS |
                          nsISVGChildFrame::TRANSFORM_CHANGED);
    }
    nsSVGUtils::PaintFrameWithEffects(&tmpCtx, nsnull, kid);
  }

  PRUint8 *data   = image->Data();
  PRInt32  stride = image->Stride();

  nsIntRect rect(0, 0, surfaceSize.width, surfaceSize.height);
  nsSVGUtils::UnPremultiplyImageDataAlpha(data, stride, rect);
  if (GetStyleSVG()->mColorInterpolation ==
      NS_STYLE_COLOR_INTERPOLATION_LINEARRGB) {
    nsSVGUtils::ConvertImageDataToLinearRGB(data, stride, rect);
  }

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

/* virtual */ void
nsSVGMaskFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsSVGEffects::InvalidateRenderingObservers(this);
  nsSVGMaskFrameBase::DidSetStyleContext(aOldStyleContext);
}

NS_IMETHODIMP
nsSVGMaskFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 PRInt32  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width ||
       aAttribute == nsGkAtoms::height||
       aAttribute == nsGkAtoms::maskUnits ||
       aAttribute == nsGkAtoms::maskContentUnits)) {
    nsSVGEffects::InvalidateRenderingObservers(this);
  }

  return nsSVGMaskFrameBase::AttributeChanged(aNameSpaceID,
                                              aAttribute, aModType);
}

#ifdef DEBUG
NS_IMETHODIMP
nsSVGMaskFrame::Init(nsIContent* aContent,
                     nsIFrame* aParent,
                     nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGMaskElement> mask = do_QueryInterface(aContent);
  NS_ASSERTION(mask, "Content is not an SVG mask");

  return nsSVGMaskFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
nsSVGMaskFrame::GetType() const
{
  return nsGkAtoms::svgMaskFrame;
}

gfxMatrix
nsSVGMaskFrame::GetCanvasTM()
{
  NS_ASSERTION(mMaskParentMatrix, "null parent matrix");

  nsSVGMaskElement *mask = static_cast<nsSVGMaskElement*>(mContent);

  return nsSVGUtils::AdjustMatrixForUnits(
    mMaskParentMatrix ? *mMaskParentMatrix : gfxMatrix(),
    &mask->mEnumAttributes[nsSVGMaskElement::MASKCONTENTUNITS],
    mMaskParent);
}

