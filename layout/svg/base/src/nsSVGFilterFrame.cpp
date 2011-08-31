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
#include "nsSVGOuterSVGFrame.h"
#include "nsGkAtoms.h"
#include "nsSVGUtils.h"
#include "nsSVGFilterElement.h"
#include "nsSVGFilters.h"
#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "nsSVGFilterPaintCallback.h"
#include "nsSVGRect.h"
#include "nsSVGFilterInstance.h"
#include "gfxUtils.h"

nsIFrame*
NS_NewSVGFilterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGFilterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGFilterFrame)

static nsIntRect
MapDeviceRectToFilterSpace(const gfxMatrix& aMatrix,
                           const gfxIntSize& aFilterSize,
                           const nsIntRect* aDeviceRect)
{
  nsIntRect rect(0, 0, aFilterSize.width, aFilterSize.height);
  if (aDeviceRect) {
    gfxRect r = aMatrix.TransformBounds(gfxRect(aDeviceRect->x, aDeviceRect->y,
                                                aDeviceRect->width, aDeviceRect->height));
    r.RoundOut();
    nsIntRect intRect;
    if (gfxUtils::GfxRectToIntRect(r, &intRect)) {
      rect = intRect;
    }
  }
  return rect;
}

class NS_STACK_CLASS nsAutoFilterInstance {
public:
  nsAutoFilterInstance(nsIFrame *aTarget,
                       nsSVGFilterFrame *aFilterFrame,
                       nsSVGFilterPaintCallback *aPaint,
                       const nsIntRect *aDirtyOutputRect,
                       const nsIntRect *aDirtyInputRect,
                       const nsIntRect *aOverrideSourceBBox);
  ~nsAutoFilterInstance();

  // If this returns null, then draw nothing. Either the filter draws
  // nothing or it is "in error".
  nsSVGFilterInstance* get() { return mInstance; }

private:
  nsAutoPtr<nsSVGFilterInstance> mInstance;
  // Store mTarget separately even though mInstance has it, because if
  // mInstance creation fails we still need to be able to clean up
  nsISVGChildFrame*              mTarget;
};

nsAutoFilterInstance::nsAutoFilterInstance(nsIFrame *aTarget,
                                           nsSVGFilterFrame *aFilterFrame,
                                           nsSVGFilterPaintCallback *aPaint,
                                           const nsIntRect *aDirtyOutputRect,
                                           const nsIntRect *aDirtyInputRect,
                                           const nsIntRect *aOverrideSourceBBox)
{
  mTarget = do_QueryFrame(aTarget);

  nsSVGFilterElement *filter =
    static_cast<nsSVGFilterElement*>(aFilterFrame->GetContent());

  PRUint16 filterUnits =
    filter->mEnumAttributes[nsSVGFilterElement::FILTERUNITS].GetAnimValue();
  PRUint16 primitiveUnits =
    filter->mEnumAttributes[nsSVGFilterElement::PRIMITIVEUNITS].GetAnimValue();

  gfxRect bbox;
  if (aOverrideSourceBBox) {
    bbox = gfxRect(aOverrideSourceBBox->x, aOverrideSourceBBox->y,
                   aOverrideSourceBBox->width, aOverrideSourceBBox->height);
  } else {
    bbox = nsSVGUtils::GetBBox(aTarget);
  }

  // Get the filter region (in the filtered element's user space):

  // XXX if filterUnits is set (or has defaulted) to objectBoundingBox, we
  // should send a warning to the error console if the author has used lengths
  // with units. This is a common mistake and can result in filterRes being
  // *massive* below (because we ignore the units and interpret the number as
  // a factor of the bbox width/height). We should also send a warning if the
  // user uses a number without units (a future SVG spec should really
  // deprecate that, since it's too confusing for a bare number to be sometimes
  // interpreted as a fraction of the bounding box and sometimes as user-space
  // units). So really only percentage values should be used in this case.
  
  gfxRect filterRegion = nsSVGUtils::GetRelativeRect(filterUnits,
    filter->mLengthAttributes, bbox, aTarget);

  if (filterRegion.Width() <= 0 || filterRegion.Height() <= 0) {
    // 0 disables rendering, < 0 is error. dispatch error console warning
    // or error as appropriate.
    return;
  }

  gfxMatrix userToDeviceSpace = nsSVGUtils::GetCanvasTM(aTarget);
  if (userToDeviceSpace.IsSingular()) {
    // nothing to draw
    return;
  }
  
  // Calculate filterRes (the width and height of the pixel buffer of the
  // temporary offscreen surface that we'll paint into):

  gfxIntSize filterRes;
  const nsSVGIntegerPair& filterResAttrs =
    filter->mIntegerPairAttributes[nsSVGFilterElement::FILTERRES];
  if (filterResAttrs.IsExplicitlySet()) {
    PRInt32 filterResX = filterResAttrs.GetAnimValue(nsSVGIntegerPair::eFirst);
    PRInt32 filterResY = filterResAttrs.GetAnimValue(nsSVGIntegerPair::eSecond);
    if (filterResX <= 0 || filterResY <= 0) {
      // 0 disables rendering, < 0 is error. dispatch error console warning?
      return;
    }

    filterRegion.Scale(filterResX, filterResY);
    filterRegion.RoundOut();
    filterRegion.Scale(1.0 / filterResX, 1.0 / filterResY);
    // We don't care if this overflows, because we can handle upscaling/
    // downscaling to filterRes
    PRBool overflow;
    filterRes =
      nsSVGUtils::ConvertToSurfaceSize(gfxSize(filterResX, filterResY),
                                       &overflow);
    // XXX we could send a warning to the error console if the author specified
    // filterRes doesn't align well with our outer 'svg' device space.
  } else {
    // Match filterRes as closely as possible to the pixel density of the nearest
    // outer 'svg' device space:
    float scale = nsSVGUtils::MaxExpansion(userToDeviceSpace);

    filterRegion.Scale(scale);
    filterRegion.RoundOut();
    // We don't care if this overflows, because we can handle upscaling/
    // downscaling to filterRes
    PRBool overflow;
    filterRes = nsSVGUtils::ConvertToSurfaceSize(filterRegion.Size(),
                                                 &overflow);
    filterRegion.Scale(1.0 / scale);
  }

  // XXX we haven't taken account of the fact that filterRegion may be
  // partially or entirely outside the current clip region. :-/

  // Convert the dirty rects to filter space, and create our nsSVGFilterInstance:

  gfxMatrix filterToUserSpace(filterRegion.Width() / filterRes.width, 0.0f,
                              0.0f, filterRegion.Height() / filterRes.height,
                              filterRegion.X(), filterRegion.Y());
  gfxMatrix filterToDeviceSpace = filterToUserSpace * userToDeviceSpace;
  
  // filterToDeviceSpace is always invertible
  gfxMatrix deviceToFilterSpace = filterToDeviceSpace;
  deviceToFilterSpace.Invert();

  nsIntRect dirtyOutputRect =
    MapDeviceRectToFilterSpace(deviceToFilterSpace, filterRes, aDirtyOutputRect);
  nsIntRect dirtyInputRect =
    MapDeviceRectToFilterSpace(deviceToFilterSpace, filterRes, aDirtyInputRect);

  // Setup instance data
  mInstance = new nsSVGFilterInstance(aTarget, aPaint, filter, bbox, filterRegion,
                                      nsIntSize(filterRes.width, filterRes.height),
                                      filterToDeviceSpace,
                                      dirtyOutputRect, dirtyInputRect,
                                      primitiveUnits);
}

nsAutoFilterInstance::~nsAutoFilterInstance()
{
}

nsresult
nsSVGFilterFrame::FilterPaint(nsSVGRenderState *aContext,
                              nsIFrame *aTarget,
                              nsSVGFilterPaintCallback *aPaintCallback,
                              const nsIntRect *aDirtyRect)
{
  nsAutoFilterInstance instance(aTarget, this, aPaintCallback,
    aDirtyRect, nsnull, nsnull);
  if (!instance.get())
    return NS_OK;

  nsRefPtr<gfxASurface> result;
  nsresult rv = instance.get()->Render(getter_AddRefs(result));
  if (NS_SUCCEEDED(rv) && result) {
    nsSVGUtils::CompositeSurfaceMatrix(aContext->GetGfxContext(),
      result, instance.get()->GetFilterSpaceToDeviceSpaceTransform(), 1.0);
  }
  return rv;
}

static nsresult
TransformFilterSpaceToDeviceSpace(nsSVGFilterInstance *aInstance, nsIntRect *aRect)
{
  gfxMatrix m = aInstance->GetFilterSpaceToDeviceSpaceTransform();
  gfxRect r(aRect->x, aRect->y, aRect->width, aRect->height);
  r = m.TransformBounds(r);
  r.RoundOut();
  nsIntRect deviceRect;
  if (!gfxUtils::GfxRectToIntRect(r, &deviceRect))
    return NS_ERROR_FAILURE;
  *aRect = deviceRect;
  return NS_OK;
}

nsIntRect
nsSVGFilterFrame::GetInvalidationBBox(nsIFrame *aTarget, const nsIntRect& aRect)
{
  nsAutoFilterInstance instance(aTarget, this, nsnull, nsnull, &aRect, nsnull);
  if (!instance.get())
    return nsIntRect();

  // We've passed in the source's dirty area so the instance knows about it.
  // Now we can ask the instance to compute the area of the filter output
  // that's dirty.
  nsIntRect dirtyRect;
  nsresult rv = instance.get()->ComputeOutputDirtyRect(&dirtyRect);
  if (NS_SUCCEEDED(rv)) {
    rv = TransformFilterSpaceToDeviceSpace(instance.get(), &dirtyRect);
    if (NS_SUCCEEDED(rv))
      return dirtyRect;
  }

  return nsIntRect();
}

nsIntRect
nsSVGFilterFrame::GetSourceForInvalidArea(nsIFrame *aTarget, const nsIntRect& aRect)
{
  nsAutoFilterInstance instance(aTarget, this, nsnull, &aRect, nsnull, nsnull);
  if (!instance.get())
    return nsIntRect();

  // Now we can ask the instance to compute the area of the source
  // that's needed.
  nsIntRect neededRect;
  nsresult rv = instance.get()->ComputeSourceNeededRect(&neededRect);
  if (NS_SUCCEEDED(rv)) {
    rv = TransformFilterSpaceToDeviceSpace(instance.get(), &neededRect);
    if (NS_SUCCEEDED(rv))
      return neededRect;
  }

  return nsIntRect();
}

nsIntRect
nsSVGFilterFrame::GetFilterBBox(nsIFrame *aTarget, const nsIntRect *aSourceBBox)
{
  nsAutoFilterInstance instance(aTarget, this, nsnull, nsnull, nsnull, aSourceBBox);
  if (!instance.get())
    return nsIntRect();

  // We've passed in the source's bounding box so the instance knows about
  // it. Now we can ask the instance to compute the bounding box of
  // the filter output.
  nsIntRect bbox;
  nsresult rv = instance.get()->ComputeOutputBBox(&bbox);
  if (NS_SUCCEEDED(rv)) {
    rv = TransformFilterSpaceToDeviceSpace(instance.get(), &bbox);
    if (NS_SUCCEEDED(rv))
      return bbox;
  }
  
  return nsIntRect();
}
  
#ifdef DEBUG
NS_IMETHODIMP
nsSVGFilterFrame::Init(nsIContent* aContent,
                       nsIFrame* aParent,
                       nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(aContent);
  NS_ASSERTION(filter, "Content is not an SVG filter");

  return nsSVGFilterFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
nsSVGFilterFrame::GetType() const
{
  return nsGkAtoms::svgFilterFrame;
}
