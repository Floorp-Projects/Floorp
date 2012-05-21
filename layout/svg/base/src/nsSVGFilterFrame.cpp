/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGFilterFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxASurface.h"
#include "gfxUtils.h"
#include "nsGkAtoms.h"
#include "nsRenderingContext.h"
#include "nsSVGEffects.h"
#include "nsSVGFilterElement.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGFilterPaintCallback.h"
#include "nsSVGUtils.h"

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

class nsSVGFilterFrame::AutoFilterReferencer
{
public:
  AutoFilterReferencer(nsSVGFilterFrame *aFrame)
    : mFrame(aFrame)
  {
    // Reference loops should normally be detected in advance and handled, so
    // we're not expecting to encounter them here
    NS_ABORT_IF_FALSE(!mFrame->mLoopFlag, "Undetected reference loop!");
    mFrame->mLoopFlag = true;
  }
  ~AutoFilterReferencer() {
    mFrame->mLoopFlag = false;
  }
private:
  nsSVGFilterFrame *mFrame;
};

class NS_STACK_CLASS nsAutoFilterInstance {
public:
  nsAutoFilterInstance(nsIFrame *aTarget,
                       nsSVGFilterFrame *aFilterFrame,
                       nsSVGFilterPaintCallback *aPaint,
                       const nsIntRect *aDirtyOutputRect,
                       const nsIntRect *aDirtyInputRect,
                       const nsIntRect *aOverrideSourceBBox,
                       const gfxMatrix *aOverrideUserToDeviceSpace = nsnull);
  ~nsAutoFilterInstance() {}

  // If this returns null, then draw nothing. Either the filter draws
  // nothing or it is "in error".
  nsSVGFilterInstance* get() { return mInstance; }

private:
  nsAutoPtr<nsSVGFilterInstance> mInstance;
};

nsAutoFilterInstance::nsAutoFilterInstance(nsIFrame *aTarget,
                                           nsSVGFilterFrame *aFilterFrame,
                                           nsSVGFilterPaintCallback *aPaint,
                                           const nsIntRect *aDirtyOutputRect,
                                           const nsIntRect *aDirtyInputRect,
                                           const nsIntRect *aOverrideSourceBBox,
                                           const gfxMatrix *aOverrideUserToDeviceSpace)
{
  const nsSVGFilterElement *filter = aFilterFrame->GetFilterContent();

  PRUint16 filterUnits =
    aFilterFrame->GetEnumValue(nsSVGFilterElement::FILTERUNITS);
  PRUint16 primitiveUnits =
    aFilterFrame->GetEnumValue(nsSVGFilterElement::PRIMITIVEUNITS);

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
  
  nsSVGLength2 XYWH[4];
  NS_ABORT_IF_FALSE(sizeof(filter->mLengthAttributes) == sizeof(XYWH),
                    "XYWH size incorrect");
  memcpy(XYWH, filter->mLengthAttributes, sizeof(filter->mLengthAttributes));
  XYWH[0] = *aFilterFrame->GetLengthValue(nsSVGFilterElement::X);
  XYWH[1] = *aFilterFrame->GetLengthValue(nsSVGFilterElement::Y);
  XYWH[2] = *aFilterFrame->GetLengthValue(nsSVGFilterElement::WIDTH);
  XYWH[3] = *aFilterFrame->GetLengthValue(nsSVGFilterElement::HEIGHT);
  gfxRect filterRegion = nsSVGUtils::GetRelativeRect(filterUnits,
    XYWH, bbox, aTarget);

  if (filterRegion.Width() <= 0 || filterRegion.Height() <= 0) {
    // 0 disables rendering, < 0 is error. dispatch error console warning
    // or error as appropriate.
    return;
  }

  gfxMatrix userToDeviceSpace;
  if (aOverrideUserToDeviceSpace) {
    userToDeviceSpace = *aOverrideUserToDeviceSpace;
  } else {
    userToDeviceSpace = nsSVGUtils::GetCanvasTM(aTarget);
  }
  
  // Calculate filterRes (the width and height of the pixel buffer of the
  // temporary offscreen surface that we'll paint into):

  gfxIntSize filterRes;
  const nsSVGIntegerPair* filterResAttrs =
    aFilterFrame->GetIntegerPairValue(nsSVGFilterElement::FILTERRES);
  if (filterResAttrs->IsExplicitlySet()) {
    PRInt32 filterResX = filterResAttrs->GetAnimValue(nsSVGIntegerPair::eFirst);
    PRInt32 filterResY = filterResAttrs->GetAnimValue(nsSVGIntegerPair::eSecond);
    if (filterResX <= 0 || filterResY <= 0) {
      // 0 disables rendering, < 0 is error. dispatch error console warning?
      return;
    }

    filterRegion.Scale(filterResX, filterResY);
    filterRegion.RoundOut();
    filterRegion.Scale(1.0 / filterResX, 1.0 / filterResY);
    // We don't care if this overflows, because we can handle upscaling/
    // downscaling to filterRes
    bool overflow;
    filterRes =
      nsSVGUtils::ConvertToSurfaceSize(gfxSize(filterResX, filterResY),
                                       &overflow);
    // XXX we could send a warning to the error console if the author specified
    // filterRes doesn't align well with our outer 'svg' device space.
  } else {
    // Match filterRes as closely as possible to the pixel density of the nearest
    // outer 'svg' device space:
    gfxMatrix canvasTM = nsSVGUtils::GetCanvasTM(aTarget);
    if (canvasTM.IsSingular()) {
      // nothing to draw
      return;
    }
    float scale = nsSVGUtils::MaxExpansion(canvasTM);

    filterRegion.Scale(scale);
    filterRegion.RoundOut();
    // We don't care if this overflows, because we can handle upscaling/
    // downscaling to filterRes
    bool overflow;
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
  nsIntRect targetBoundsDeviceSpace;
  nsISVGChildFrame* svgTarget = do_QueryFrame(aTarget);
  if (svgTarget) {
    if (aOverrideUserToDeviceSpace) {
      // If aOverrideUserToDeviceSpace is specified, it is a simple
      // CSS-px-to-dev-px transform passed by nsSVGFilterFrame::GetFilterBBox()
      // when requesting the filter expansion of the overflow rects in frame
      // space. In this case GetCoveredRegion() is not what we want since it is
      // in outer-<svg> space, GetFilterBBox passes in the pre-filter bounds of
      // the frame in frame space for us to use instead.
      NS_ASSERTION(aDirtyInputRect, "Who passed aOverrideUserToDeviceSpace?");
      targetBoundsDeviceSpace = *aDirtyInputRect;
    } else {
      targetBoundsDeviceSpace =
        svgTarget->GetCoveredRegion().ToOutsidePixels(aTarget->
          PresContext()->AppUnitsPerDevPixel());
    }
  }
  nsIntRect targetBoundsFilterSpace =
    MapDeviceRectToFilterSpace(deviceToFilterSpace, filterRes, &targetBoundsDeviceSpace);

  // Setup instance data
  mInstance = new nsSVGFilterInstance(aTarget, aPaint, filter, bbox, filterRegion,
                                      nsIntSize(filterRes.width, filterRes.height),
                                      filterToDeviceSpace, targetBoundsFilterSpace,
                                      dirtyOutputRect, dirtyInputRect,
                                      primitiveUnits);
}

PRUint16
nsSVGFilterFrame::GetEnumValue(PRUint32 aIndex, nsIContent *aDefault)
{
  nsSVGEnum& thisEnum =
    static_cast<nsSVGFilterElement *>(mContent)->mEnumAttributes[aIndex];

  if (thisEnum.IsExplicitlySet())
    return thisEnum.GetAnimValue();

  AutoFilterReferencer filterRef(this);

  nsSVGFilterFrame *next = GetReferencedFilterIfNotInUse();
  return next ? next->GetEnumValue(aIndex, aDefault) :
    static_cast<nsSVGFilterElement *>(aDefault)->
      mEnumAttributes[aIndex].GetAnimValue();
}

const nsSVGIntegerPair *
nsSVGFilterFrame::GetIntegerPairValue(PRUint32 aIndex, nsIContent *aDefault)
{
  const nsSVGIntegerPair *thisIntegerPair =
    &static_cast<nsSVGFilterElement *>(mContent)->mIntegerPairAttributes[aIndex];

  if (thisIntegerPair->IsExplicitlySet())
    return thisIntegerPair;

  AutoFilterReferencer filterRef(this);

  nsSVGFilterFrame *next = GetReferencedFilterIfNotInUse();
  return next ? next->GetIntegerPairValue(aIndex, aDefault) :
    &static_cast<nsSVGFilterElement *>(aDefault)->mIntegerPairAttributes[aIndex];
}

const nsSVGLength2 *
nsSVGFilterFrame::GetLengthValue(PRUint32 aIndex, nsIContent *aDefault)
{
  const nsSVGLength2 *thisLength =
    &static_cast<nsSVGFilterElement *>(mContent)->mLengthAttributes[aIndex];

  if (thisLength->IsExplicitlySet())
    return thisLength;

  AutoFilterReferencer filterRef(this);

  nsSVGFilterFrame *next = GetReferencedFilterIfNotInUse();
  return next ? next->GetLengthValue(aIndex, aDefault) :
    &static_cast<nsSVGFilterElement *>(aDefault)->mLengthAttributes[aIndex];
}

const nsSVGFilterElement *
nsSVGFilterFrame::GetFilterContent(nsIContent *aDefault)
{
  for (nsIContent* child = mContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    nsRefPtr<nsSVGFE> primitive;
    CallQueryInterface(child, (nsSVGFE**)getter_AddRefs(primitive));
    if (primitive) {
      return static_cast<nsSVGFilterElement *>(mContent);
    }
  }

  AutoFilterReferencer filterRef(this);

  nsSVGFilterFrame *next = GetReferencedFilterIfNotInUse();
  return next ? next->GetFilterContent(aDefault) :
    static_cast<nsSVGFilterElement *>(aDefault);
}

nsSVGFilterFrame *
nsSVGFilterFrame::GetReferencedFilter()
{
  if (mNoHRefURI)
    return nsnull;

  nsSVGPaintingProperty *property = static_cast<nsSVGPaintingProperty*>
    (Properties().Get(nsSVGEffects::HrefProperty()));

  if (!property) {
    // Fetch our Filter element's xlink:href attribute
    nsSVGFilterElement *filter = static_cast<nsSVGFilterElement *>(mContent);
    nsAutoString href;
    filter->mStringAttributes[nsSVGFilterElement::HREF].GetAnimValue(href, filter);
    if (href.IsEmpty()) {
      mNoHRefURI = true;
      return nsnull; // no URL
    }

    // Convert href to an nsIURI
    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = mContent->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              mContent->GetCurrentDoc(), base);

    property =
      nsSVGEffects::GetPaintingProperty(targetURI, this, nsSVGEffects::HrefProperty());
    if (!property)
      return nsnull;
  }

  nsIFrame *result = property->GetReferencedFrame();
  if (!result)
    return nsnull;

  nsIAtom* frameType = result->GetType();
  if (frameType != nsGkAtoms::svgFilterFrame)
    return nsnull;

  return static_cast<nsSVGFilterFrame*>(result);
}

nsSVGFilterFrame *
nsSVGFilterFrame::GetReferencedFilterIfNotInUse()
{
  nsSVGFilterFrame *referenced = GetReferencedFilter();
  if (!referenced)
    return nsnull;

  if (referenced->mLoopFlag) {
    // XXXjwatt: we should really send an error to the JavaScript Console here:
    NS_WARNING("Filter reference loop detected while inheriting attribute!");
    return nsnull;
  }

  return referenced;
}

NS_IMETHODIMP
nsSVGFilterFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   PRInt32  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width ||
       aAttribute == nsGkAtoms::height ||
       aAttribute == nsGkAtoms::filterRes ||
       aAttribute == nsGkAtoms::filterUnits ||
       aAttribute == nsGkAtoms::primitiveUnits)) {
    nsSVGEffects::InvalidateRenderingObservers(this);
  } else if (aNameSpaceID == kNameSpaceID_XLink &&
             aAttribute == nsGkAtoms::href) {
    // Blow away our reference, if any
    Properties().Delete(nsSVGEffects::HrefProperty());
    mNoHRefURI = false;
    // And update whoever references us
    nsSVGEffects::InvalidateRenderingObservers(this);
  }
  return nsSVGFilterFrameBase::AttributeChanged(aNameSpaceID,
                                                aAttribute, aModType);
}

nsresult
nsSVGFilterFrame::FilterPaint(nsRenderingContext *aContext,
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
    nsSVGUtils::CompositeSurfaceMatrix(aContext->ThebesContext(),
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
nsSVGFilterFrame::GetFilterBBox(nsIFrame *aTarget,
                                const nsIntRect *aOverrideBBox,
                                const nsIntRect *aPreFilterBounds)
{
  bool overrideCTM = false;
  gfxMatrix ctm;

  if (aTarget->GetStateBits() & NS_FRAME_SVG_LAYOUT) {
    // For most filter operations on SVG frames we want information in
    // outer-<svg> device space, but in this case we want the visual overflow
    // rect relative to aTarget itself. For that we need to prevent the filter
    // code using GetCanvasTM().
    overrideCTM = true;
    PRInt32 appUnitsPerDevPixel = aTarget->PresContext()->AppUnitsPerDevPixel();
    float devPxPerCSSPx =
      1 / nsPresContext::AppUnitsToFloatCSSPixels(appUnitsPerDevPixel);
    ctm.Scale(devPxPerCSSPx, devPxPerCSSPx);
  }

  nsAutoFilterInstance instance(aTarget, this, nsnull, nsnull, aPreFilterBounds,
                                aOverrideBBox, overrideCTM ? &ctm : nsnull);
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
