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

#include "nsSVGFilterFrame.h"
#include "nsIDocument.h"
#include "nsISVGValueUtils.h"
#include "nsSVGMatrix.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsGkAtoms.h"
#include "nsSVGUtils.h"
#include "nsSVGFilterElement.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGFilters.h"
#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"

nsIFrame*
NS_NewSVGFilterFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(aContent);
  if (!filter) {
    NS_ERROR("Can't create frame! Content is not an SVG filter");
    return nsnull;
  }

  return new (aPresShell) nsSVGFilterFrame(aContext);
}

nsIContent *
NS_GetSVGFilterElement(nsIURI *aURI, nsIContent *aContent)
{
  nsIContent* content = nsContentUtils::GetReferencedElement(aURI, aContent);

  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(content);
  if (filter)
    return content;

  return nsnull;
}

static nsIntRect
MapDeviceRectToFilterSpace(const gfxMatrix& aMatrix,
                           const gfxIntSize& aFilterSize,
                           const nsRect* aDeviceRect)
{
  nsIntRect rect(0, 0, aFilterSize.width, aFilterSize.height);
  if (aDeviceRect) {
    gfxRect r = aMatrix.TransformBounds(gfxRect(aDeviceRect->x, aDeviceRect->y,
                                                aDeviceRect->width, aDeviceRect->height));
    r.RoundOut();
    nsIntRect intRect;
    if (NS_SUCCEEDED(nsSVGUtils::GfxRectToIntRect(r, &intRect))) {
      rect = intRect;
    }
  }
  return rect;
}

nsresult
nsSVGFilterFrame::CreateInstance(nsISVGChildFrame *aTarget,
                                 const nsRect *aDirtyOutputRect,
                                 const nsRect *aDirtyInputRect,
                                 nsSVGFilterInstance **aInstance)
{
  *aInstance = nsnull;

  nsIFrame *frame;
  CallQueryInterface(aTarget, &frame);

  nsCOMPtr<nsIDOMSVGMatrix> ctm = nsSVGUtils::GetCanvasTM(frame);

  nsSVGElement *target = static_cast<nsSVGElement*>(frame->GetContent());

  aTarget->SetMatrixPropagation(PR_FALSE);
  aTarget->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                            nsISVGChildFrame::TRANSFORM_CHANGED);

  nsSVGFilterElement *filter = static_cast<nsSVGFilterElement*>(mContent);

  float x, y, width, height;
  nsCOMPtr<nsIDOMSVGRect> bbox;
  aTarget->GetBBox(getter_AddRefs(bbox));

  nsSVGLength2 *tmpX, *tmpY, *tmpWidth, *tmpHeight;
  tmpX = &filter->mLengthAttributes[nsSVGFilterElement::X];
  tmpY = &filter->mLengthAttributes[nsSVGFilterElement::Y];
  tmpWidth = &filter->mLengthAttributes[nsSVGFilterElement::WIDTH];
  tmpHeight = &filter->mLengthAttributes[nsSVGFilterElement::HEIGHT];

  PRUint16 units =
    filter->mEnumAttributes[nsSVGFilterElement::FILTERUNITS].GetAnimValue();

  // Compute filter effects region as per spec
  if (units == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    if (!bbox)
      return NS_OK;

    bbox->GetX(&x);
    x += nsSVGUtils::ObjectSpace(bbox, tmpX);
    bbox->GetY(&y);
    y += nsSVGUtils::ObjectSpace(bbox, tmpY);
    width = nsSVGUtils::ObjectSpace(bbox, tmpWidth);
    height = nsSVGUtils::ObjectSpace(bbox, tmpHeight);
  } else {
    x = nsSVGUtils::UserSpace(target, tmpX);
    y = nsSVGUtils::UserSpace(target, tmpY);
    width = nsSVGUtils::UserSpace(target, tmpWidth);
    height = nsSVGUtils::UserSpace(target, tmpHeight);
  }
  
  PRBool resultOverflows;
  gfxIntSize filterRes;

  // Compute size of filter buffer
  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::filterRes)) {
    PRInt32 filterResX, filterResY;
    filter->GetAnimatedIntegerValues(&filterResX, &filterResY, nsnull);

    filterRes =
      nsSVGUtils::ConvertToSurfaceSize(gfxSize(filterResX, filterResY),
                                       &resultOverflows);
  } else {
    float scale = nsSVGUtils::MaxExpansion(ctm);
#ifdef DEBUG_tor
    fprintf(stderr, "scale: %f\n", scale);
#endif

    filterRes =
      nsSVGUtils::ConvertToSurfaceSize(gfxSize(width, height) * scale,
                                       &resultOverflows);
  }

  // 0 disables rendering, < 0 is error
  if (filterRes.width <= 0 || filterRes.height <= 0)
    return NS_OK;

#ifdef DEBUG_tor
  fprintf(stderr, "filter bbox: %f,%f  %fx%f\n", x, y, width, height);
  fprintf(stderr, "filterRes: %u %u\n", filterRes.width, filterRes.height);
#endif

  // 'fini' is the matrix we will finally use to transform filter space
  // to surface space for drawing
  nsCOMPtr<nsIDOMSVGMatrix> scale, fini;
  NS_NewSVGMatrix(getter_AddRefs(scale),
                  width / filterRes.width, 0.0f,
                  0.0f, height / filterRes.height,
                  x, y);
  ctm->Multiply(scale, getter_AddRefs(fini));
  
  gfxMatrix finiM = nsSVGUtils::ConvertSVGMatrixToThebes(fini);
  // fini is always invertible.
  finiM.Invert();

  nsIntRect dirtyOutputRect =
    MapDeviceRectToFilterSpace(finiM, filterRes, aDirtyOutputRect);
  nsIntRect dirtyInputRect =
    MapDeviceRectToFilterSpace(finiM, filterRes, aDirtyInputRect);

  // Setup instance data
  PRUint16 primitiveUnits =
    filter->mEnumAttributes[nsSVGFilterElement::PRIMITIVEUNITS].GetAnimValue();
  *aInstance = new nsSVGFilterInstance(aTarget, mContent, bbox,
                                       gfxRect(x, y, width, height),
                                       nsIntSize(filterRes.width, filterRes.height),
                                       fini,
                                       dirtyOutputRect, dirtyInputRect,
                                       primitiveUnits);
  return *aInstance ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

static void
RestoreTargetState(nsISVGChildFrame *aTarget)
{
  aTarget->SetOverrideCTM(nsnull);
  aTarget->SetMatrixPropagation(PR_TRUE);
  aTarget->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                            nsISVGChildFrame::TRANSFORM_CHANGED);
}

nsresult
nsSVGFilterFrame::FilterPaint(nsSVGRenderState *aContext,
                              nsISVGChildFrame *aTarget,
                              const nsRect *aDirtyRect)
{
  nsAutoPtr<nsSVGFilterInstance> instance;
  nsresult rv = CreateInstance(aTarget, aDirtyRect, nsnull, getter_Transfers(instance));

  if (NS_SUCCEEDED(rv) && instance) {
    // Transformation from user space to filter space
    nsCOMPtr<nsIDOMSVGMatrix> filterTransform =
      instance->GetUserSpaceToFilterSpaceTransform();
    aTarget->SetOverrideCTM(filterTransform);
    aTarget->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                              nsISVGChildFrame::TRANSFORM_CHANGED);

    nsRefPtr<gfxASurface> result;
    rv = instance->Render(getter_AddRefs(result));
    if (NS_SUCCEEDED(rv) && result) {
      nsSVGUtils::CompositeSurfaceMatrix(aContext->GetGfxContext(),
        result, instance->GetFilterSpaceToDeviceSpaceTransform(), 1.0);
    }
  }

  RestoreTargetState(aTarget);

  if (NS_FAILED(rv)) {
    aTarget->PaintSVG(aContext, nsnull);
  }
  return rv;
}

nsRect
nsSVGFilterFrame::GetInvalidationRegion(nsIFrame *aTarget, const nsRect& aRect)
{
  nsISVGChildFrame *svg;
  CallQueryInterface(aTarget, &svg);

  nsRect result = aRect;

  nsAutoPtr<nsSVGFilterInstance> instance;
  nsresult rv = CreateInstance(svg, nsnull, &aRect, getter_Transfers(instance));
  if (NS_SUCCEEDED(rv)) {
    if (!instance) {
      // The filter draws nothing, so nothing is dirty.
      result = nsRect();
    } else {
      // We've passed in the source's dirty area so the instance knows about it.
      // Now we can ask the instance to compute the area of the filter output
      // that's dirty.
      nsIntRect filterSpaceDirtyRect;
      rv = instance->ComputeOutputDirtyRect(&filterSpaceDirtyRect);
      if (NS_SUCCEEDED(rv)) {
        gfxMatrix m = nsSVGUtils::ConvertSVGMatrixToThebes(
          instance->GetFilterSpaceToDeviceSpaceTransform());
        gfxRect r(filterSpaceDirtyRect.x, filterSpaceDirtyRect.y,
                  filterSpaceDirtyRect.width, filterSpaceDirtyRect.height);
        r = m.TransformBounds(r);
        r.RoundOut();
        nsIntRect deviceRect;
        rv = nsSVGUtils::GfxRectToIntRect(r, &deviceRect);
        if (NS_SUCCEEDED(rv)) {
          result = deviceRect;
        }
      }
    }
  }

  RestoreTargetState(svg);

  return result;
}

nsIAtom *
nsSVGFilterFrame::GetType() const
{
  return nsGkAtoms::svgFilterFrame;
}
