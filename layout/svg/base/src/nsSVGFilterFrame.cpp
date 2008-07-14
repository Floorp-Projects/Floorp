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

void
nsSVGFilterFrame::FilterFailCleanup(nsSVGRenderState *aContext,
                                    nsISVGChildFrame *aTarget)
{
  aTarget->SetOverrideCTM(nsnull);
  aTarget->SetMatrixPropagation(PR_TRUE);
  aTarget->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                            nsISVGChildFrame::TRANSFORM_CHANGED);
  aTarget->PaintSVG(aContext, nsnull);
}

nsresult
nsSVGFilterFrame::FilterPaint(nsSVGRenderState *aContext,
                              nsISVGChildFrame *aTarget)
{
  nsCOMPtr<nsIDOMSVGFilterElement> aFilter = do_QueryInterface(mContent);
  NS_ASSERTION(aFilter, "Wrong content element (not filter)");

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
    if (!bbox) {
      aTarget->SetMatrixPropagation(PR_TRUE);
      aTarget->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                                nsISVGChildFrame::TRANSFORM_CHANGED);
      return NS_OK;
    }

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
  if (filterRes.width <= 0 || filterRes.height <= 0) {
    aTarget->SetMatrixPropagation(PR_TRUE);
    aTarget->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                              nsISVGChildFrame::TRANSFORM_CHANGED);
    return NS_OK;
  }

#ifdef DEBUG_tor
  fprintf(stderr, "filter bbox: %f,%f  %fx%f\n", x, y, width, height);
  fprintf(stderr, "filterRes: %u %u\n", filterRes.width, filterRes.height);
#endif

  // Transformation from user space to filter space
  nsCOMPtr<nsIDOMSVGMatrix> filterTransform;
  NS_NewSVGMatrix(getter_AddRefs(filterTransform),
                  filterRes.width / width,      0.0f,
                  0.0f,                         filterRes.height / height,
                  -x * filterRes.width / width, -y * filterRes.height / height);
  aTarget->SetOverrideCTM(filterTransform);
  aTarget->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                            nsISVGChildFrame::TRANSFORM_CHANGED);

  // Setup instance data
  PRUint16 primitiveUnits =
    filter->mEnumAttributes[nsSVGFilterElement::PRIMITIVEUNITS].GetAnimValue();
  nsSVGFilterInstance instance(aTarget, mContent, bbox,
                               gfxRect(x, y, width, height),
                               nsIntSize(filterRes.width, filterRes.height),
                               primitiveUnits);

  nsRefPtr<gfxASurface> result;
  nsresult rv = instance.Render(getter_AddRefs(result));
  if (NS_FAILED(rv)) {
    FilterFailCleanup(aContext, aTarget);
    return NS_OK;
  }

  if (result) {
    nsCOMPtr<nsIDOMSVGMatrix> scale, fini;
    NS_NewSVGMatrix(getter_AddRefs(scale),
                    width / filterRes.width, 0.0f,
                    0.0f, height / filterRes.height,
                    x, y);

    ctm->Multiply(scale, getter_AddRefs(fini));

    nsSVGUtils::CompositeSurfaceMatrix(aContext->GetGfxContext(),
                                       result, fini, 1.0);
  }

  aTarget->SetOverrideCTM(nsnull);
  aTarget->SetMatrixPropagation(PR_TRUE);
  aTarget->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                            nsISVGChildFrame::TRANSFORM_CHANGED);

  return NS_OK;
}

nsRect
nsSVGFilterFrame::GetInvalidationRegion(nsIFrame *aTarget)
{
  nsSVGElement *targetContent =
    static_cast<nsSVGElement*>(aTarget->GetContent());
  nsISVGChildFrame *svg;

  nsCOMPtr<nsIDOMSVGMatrix> ctm = nsSVGUtils::GetCanvasTM(aTarget);

  CallQueryInterface(aTarget, &svg);

  nsSVGFilterElement *filter = static_cast<nsSVGFilterElement*>(mContent);

  PRUint16 type =
    filter->mEnumAttributes[nsSVGFilterElement::FILTERUNITS].GetAnimValue();

  float x, y, width, height;
  nsCOMPtr<nsIDOMSVGRect> bbox;

  svg->SetMatrixPropagation(PR_FALSE);
  svg->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                        nsISVGChildFrame::TRANSFORM_CHANGED);

  svg->GetBBox(getter_AddRefs(bbox));

  svg->SetMatrixPropagation(PR_TRUE);
  svg->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                        nsISVGChildFrame::TRANSFORM_CHANGED);

  nsSVGLength2 *tmpX, *tmpY, *tmpWidth, *tmpHeight;
  tmpX = &filter->mLengthAttributes[nsSVGFilterElement::X];
  tmpY = &filter->mLengthAttributes[nsSVGFilterElement::Y];
  tmpWidth = &filter->mLengthAttributes[nsSVGFilterElement::WIDTH];
  tmpHeight = &filter->mLengthAttributes[nsSVGFilterElement::HEIGHT];

  if (type == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    if (!bbox)
      return nsRect();

    bbox->GetX(&x);
    x += nsSVGUtils::ObjectSpace(bbox, tmpX);
    bbox->GetY(&y);
    y += nsSVGUtils::ObjectSpace(bbox, tmpY);
    width = nsSVGUtils::ObjectSpace(bbox, tmpWidth);
    height = nsSVGUtils::ObjectSpace(bbox, tmpHeight);
  } else {
    x = nsSVGUtils::UserSpace(targetContent, tmpX);
    y = nsSVGUtils::UserSpace(targetContent, tmpY);
    width = nsSVGUtils::UserSpace(targetContent, tmpWidth);
    height = nsSVGUtils::UserSpace(targetContent, tmpHeight);
  }

#ifdef DEBUG_tor
  fprintf(stderr, "invalidate box: %f,%f %fx%f\n", x, y, width, height);
#endif

  // transform back
  float xx[4], yy[4];
  xx[0] = x;          yy[0] = y;
  xx[1] = x + width;  yy[1] = y;
  xx[2] = x + width;  yy[2] = y + height;
  xx[3] = x;          yy[3] = y + height;

  nsSVGUtils::TransformPoint(ctm, &xx[0], &yy[0]);
  nsSVGUtils::TransformPoint(ctm, &xx[1], &yy[1]);
  nsSVGUtils::TransformPoint(ctm, &xx[2], &yy[2]);
  nsSVGUtils::TransformPoint(ctm, &xx[3], &yy[3]);

  float xmin, xmax, ymin, ymax;
  xmin = xmax = xx[0];
  ymin = ymax = yy[0];
  for (int i=1; i<4; i++) {
    if (xx[i] < xmin)
      xmin = xx[i];
    if (yy[i] < ymin)
      ymin = yy[i];
    if (xx[i] > xmax)
      xmax = xx[i];
    if (yy[i] > ymax)
      ymax = yy[i];
  }

#ifdef DEBUG_tor
  fprintf(stderr, "xform bound: %f %f  %f %f\n", xmin, ymin, xmax, ymax);
#endif

  return nsSVGUtils::ToBoundingPixelRect(xmin, ymin, xmax, ymax);
}

nsIAtom *
nsSVGFilterFrame::GetType() const
{
  return nsGkAtoms::svgFilterFrame;
}
