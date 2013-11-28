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
#include "nsSVGElement.h"
#include "mozilla/dom/SVGFilterElement.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGFilterPaintCallback.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGUtils.h"
#include "nsContentUtils.h"

using namespace mozilla::dom;

nsIFrame*
NS_NewSVGFilterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGFilterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGFilterFrame)

/**
 * Converts an nsRect that is relative to a filtered frame's origin (i.e. the
 * top-left corner of its border box) into filter space.
 * Returns the entire filter region (a rect the width/height of aFilterRes) if
 * aFrameRect is null, or if the result is too large to be stored in an
 * nsIntRect.
 */
static nsIntRect
MapFrameRectToFilterSpace(const nsRect* aRect,
                          int32_t aAppUnitsPerCSSPx,
                          const gfxMatrix& aFrameSpaceInCSSPxToFilterSpace,
                          const gfxIntSize& aFilterRes)
{
  nsIntRect rect(0, 0, aFilterRes.width, aFilterRes.height);
  if (aRect) {
    if (aRect->IsEmpty()) {
      return nsIntRect();
    }
    gfxRect rectInCSSPx =
      nsLayoutUtils::RectToGfxRect(*aRect, aAppUnitsPerCSSPx);
    gfxRect rectInFilterSpace =
      aFrameSpaceInCSSPxToFilterSpace.TransformBounds(rectInCSSPx);
    rectInFilterSpace.RoundOut();
    nsIntRect intRect;
    if (gfxUtils::GfxRectToIntRect(rectInFilterSpace, &intRect)) {
      rect = intRect;
    }
  }
  return rect;
}

/**
 * Returns the transform from frame space to the coordinate space that
 * GetCanvasTM transforms to. "Frame space" is the origin of a frame, aka the
 * top-left corner of its border box, aka the top left corner of its mRect.
 */
static gfxMatrix
GetUserToFrameSpaceInCSSPxTransform(nsIFrame *aFrame)
{
  gfxMatrix userToFrameSpaceInCSSPx;

  if ((aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT)) {
    int32_t appUnitsPerCSSPx = aFrame->PresContext()->AppUnitsPerCSSPixel();
    // As currently implemented by Mozilla for the purposes of filters, user
    // space is the coordinate system established by GetCanvasTM(), since
    // that's what we use to set filterToDeviceSpace above. In other words,
    // for SVG, user space is actually the coordinate system aTarget
    // establishes for _its_ children (i.e. after taking account of any x/y
    // and viewBox attributes), not the coordinate system that is established
    // for it by its 'transform' attribute (or by its _parent_) as it's
    // normally defined. (XXX We should think about fixing this.) The only
    // frame type for which these extra transforms are not simply an x/y
    // translation is nsSVGInnerSVGFrame, hence we treat it specially here.
    if (aFrame->GetType() == nsGkAtoms::svgInnerSVGFrame) {
      userToFrameSpaceInCSSPx =
        static_cast<nsSVGElement*>(aFrame->GetContent())->
          PrependLocalTransformsTo(gfxMatrix());
    } else {
      gfxPoint targetsUserSpaceOffset =
        nsLayoutUtils::RectToGfxRect(aFrame->GetRect(), appUnitsPerCSSPx).
                         TopLeft();
      userToFrameSpaceInCSSPx.Translate(-targetsUserSpaceOffset);
    }
  }
  // else, for all other frames, leave as the identity matrix
  return userToFrameSpaceInCSSPx;
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

class MOZ_STACK_CLASS nsAutoFilterInstance {
public:
  nsAutoFilterInstance(nsIFrame *aTarget,
                       nsSVGFilterFrame *aFilterFrame,
                       nsSVGFilterPaintCallback *aPaint,
                       const nsRect *aPostFilterDirtyRect,
                       const nsRect *aPreFilterDirtyRect,
                       const nsRect *aOverridePreFilterVisualOverflowRect,
                       const gfxRect *aOverrideBBox = nullptr,
                       nsIFrame* aTransformRoot = nullptr);
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
                                           const nsRect *aPostFilterDirtyRect,
                                           const nsRect *aPreFilterDirtyRect,
                                           const nsRect *aPreFilterVisualOverflowRectOverride,
                                           const gfxRect *aOverrideBBox,
                                           nsIFrame* aTransformRoot)
{
  const SVGFilterElement *filter = aFilterFrame->GetFilterContent();

  uint16_t filterUnits =
    aFilterFrame->GetEnumValue(SVGFilterElement::FILTERUNITS);
  uint16_t primitiveUnits =
    aFilterFrame->GetEnumValue(SVGFilterElement::PRIMITIVEUNITS);

  gfxRect bbox = aOverrideBBox ? *aOverrideBBox : nsSVGUtils::GetBBox(aTarget);

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
  XYWH[0] = *aFilterFrame->GetLengthValue(SVGFilterElement::ATTR_X);
  XYWH[1] = *aFilterFrame->GetLengthValue(SVGFilterElement::ATTR_Y);
  XYWH[2] = *aFilterFrame->GetLengthValue(SVGFilterElement::ATTR_WIDTH);
  XYWH[3] = *aFilterFrame->GetLengthValue(SVGFilterElement::ATTR_HEIGHT);
  // The filter region in user space, in user units:
  gfxRect filterRegion = nsSVGUtils::GetRelativeRect(filterUnits,
    XYWH, bbox, aTarget);

  if (filterRegion.Width() <= 0 || filterRegion.Height() <= 0) {
    // 0 disables rendering, < 0 is error. dispatch error console warning
    // or error as appropriate.
    return;
  }

  // Calculate filterRes (the width and height of the pixel buffer of the
  // temporary offscreen surface that we would/will create to paint into when
  // painting the entire filtered element) and, if necessary, adjust
  // filterRegion out slightly so that it aligns with pixel boundaries of this
  // buffer:

  gfxIntSize filterRes;
  const nsSVGIntegerPair* filterResAttrs =
    aFilterFrame->GetIntegerPairValue(SVGFilterElement::FILTERRES);
  if (filterResAttrs->IsExplicitlySet()) {
    int32_t filterResX = filterResAttrs->GetAnimValue(nsSVGIntegerPair::eFirst);
    int32_t filterResY = filterResAttrs->GetAnimValue(nsSVGIntegerPair::eSecond);
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
    gfxMatrix canvasTM =
      nsSVGUtils::GetCanvasTM(aTarget, nsISVGChildFrame::FOR_OUTERSVG_TM);
    if (canvasTM.IsSingular()) {
      // nothing to draw
      return;
    }

    gfxSize scale = canvasTM.ScaleFactors(true);
    filterRegion.Scale(scale.width, scale.height);
    filterRegion.RoundOut();
    // We don't care if this overflows, because we can handle upscaling/
    // downscaling to filterRes
    bool overflow;
    filterRes = nsSVGUtils::ConvertToSurfaceSize(filterRegion.Size(),
                                                 &overflow);
    filterRegion.Scale(1.0 / scale.width, 1.0 / scale.height);
  }

  // Get various transforms:

  gfxMatrix filterToUserSpace(filterRegion.Width() / filterRes.width, 0.0f,
                              0.0f, filterRegion.Height() / filterRes.height,
                              filterRegion.X(), filterRegion.Y());

  // Only used (so only set) when we paint:
  gfxMatrix filterToDeviceSpace;
  if (aPaint) {
    filterToDeviceSpace = filterToUserSpace *
              nsSVGUtils::GetCanvasTM(aTarget, nsISVGChildFrame::FOR_PAINTING);
  }

  // Convert the passed in rects from frame to filter space:

  int32_t appUnitsPerCSSPx = aTarget->PresContext()->AppUnitsPerCSSPixel();

  gfxMatrix filterToFrameSpaceInCSSPx =
    filterToUserSpace * GetUserToFrameSpaceInCSSPxTransform(aTarget);
  // filterToFrameSpaceInCSSPx is always invertible
  gfxMatrix frameSpaceInCSSPxTofilterSpace = filterToFrameSpaceInCSSPx;
  frameSpaceInCSSPxTofilterSpace.Invert();

  nsIntRect postFilterDirtyRect =
    MapFrameRectToFilterSpace(aPostFilterDirtyRect, appUnitsPerCSSPx,
                              frameSpaceInCSSPxTofilterSpace, filterRes);
  nsIntRect preFilterDirtyRect =
    MapFrameRectToFilterSpace(aPreFilterDirtyRect, appUnitsPerCSSPx,
                              frameSpaceInCSSPxTofilterSpace, filterRes);
  nsIntRect preFilterVisualOverflowRect;
  if (aPreFilterVisualOverflowRectOverride) {
    preFilterVisualOverflowRect =
      MapFrameRectToFilterSpace(aPreFilterVisualOverflowRectOverride,
                                appUnitsPerCSSPx,
                                frameSpaceInCSSPxTofilterSpace, filterRes);
  } else {
    nsRect preFilterVOR = aTarget->GetPreEffectsVisualOverflowRect();
    preFilterVisualOverflowRect =
      MapFrameRectToFilterSpace(&preFilterVOR, appUnitsPerCSSPx,
                                frameSpaceInCSSPxTofilterSpace, filterRes);
  }

  // Setup instance data
  mInstance =
    new nsSVGFilterInstance(aTarget, aPaint, filter, bbox, filterRegion,
                            nsIntSize(filterRes.width, filterRes.height),
                            filterToDeviceSpace, filterToFrameSpaceInCSSPx,
                            preFilterVisualOverflowRect, postFilterDirtyRect,
                            preFilterDirtyRect, primitiveUnits,
                            aTransformRoot);
}

uint16_t
nsSVGFilterFrame::GetEnumValue(uint32_t aIndex, nsIContent *aDefault)
{
  nsSVGEnum& thisEnum =
    static_cast<SVGFilterElement *>(mContent)->mEnumAttributes[aIndex];

  if (thisEnum.IsExplicitlySet())
    return thisEnum.GetAnimValue();

  AutoFilterReferencer filterRef(this);

  nsSVGFilterFrame *next = GetReferencedFilterIfNotInUse();
  return next ? next->GetEnumValue(aIndex, aDefault) :
    static_cast<SVGFilterElement *>(aDefault)->
      mEnumAttributes[aIndex].GetAnimValue();
}

const nsSVGIntegerPair *
nsSVGFilterFrame::GetIntegerPairValue(uint32_t aIndex, nsIContent *aDefault)
{
  const nsSVGIntegerPair *thisIntegerPair =
    &static_cast<SVGFilterElement *>(mContent)->mIntegerPairAttributes[aIndex];

  if (thisIntegerPair->IsExplicitlySet())
    return thisIntegerPair;

  AutoFilterReferencer filterRef(this);

  nsSVGFilterFrame *next = GetReferencedFilterIfNotInUse();
  return next ? next->GetIntegerPairValue(aIndex, aDefault) :
    &static_cast<SVGFilterElement *>(aDefault)->mIntegerPairAttributes[aIndex];
}

const nsSVGLength2 *
nsSVGFilterFrame::GetLengthValue(uint32_t aIndex, nsIContent *aDefault)
{
  const nsSVGLength2 *thisLength =
    &static_cast<SVGFilterElement *>(mContent)->mLengthAttributes[aIndex];

  if (thisLength->IsExplicitlySet())
    return thisLength;

  AutoFilterReferencer filterRef(this);

  nsSVGFilterFrame *next = GetReferencedFilterIfNotInUse();
  return next ? next->GetLengthValue(aIndex, aDefault) :
    &static_cast<SVGFilterElement *>(aDefault)->mLengthAttributes[aIndex];
}

const SVGFilterElement *
nsSVGFilterFrame::GetFilterContent(nsIContent *aDefault)
{
  for (nsIContent* child = mContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    nsRefPtr<nsSVGFE> primitive;
    CallQueryInterface(child, (nsSVGFE**)getter_AddRefs(primitive));
    if (primitive) {
      return static_cast<SVGFilterElement *>(mContent);
    }
  }

  AutoFilterReferencer filterRef(this);

  nsSVGFilterFrame *next = GetReferencedFilterIfNotInUse();
  return next ? next->GetFilterContent(aDefault) :
    static_cast<SVGFilterElement *>(aDefault);
}

nsSVGFilterFrame *
nsSVGFilterFrame::GetReferencedFilter()
{
  if (mNoHRefURI)
    return nullptr;

  nsSVGPaintingProperty *property = static_cast<nsSVGPaintingProperty*>
    (Properties().Get(nsSVGEffects::HrefProperty()));

  if (!property) {
    // Fetch our Filter element's xlink:href attribute
    SVGFilterElement *filter = static_cast<SVGFilterElement *>(mContent);
    nsAutoString href;
    filter->mStringAttributes[SVGFilterElement::HREF].GetAnimValue(href, filter);
    if (href.IsEmpty()) {
      mNoHRefURI = true;
      return nullptr; // no URL
    }

    // Convert href to an nsIURI
    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = mContent->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              mContent->GetCurrentDoc(), base);

    property =
      nsSVGEffects::GetPaintingProperty(targetURI, this, nsSVGEffects::HrefProperty());
    if (!property)
      return nullptr;
  }

  nsIFrame *result = property->GetReferencedFrame();
  if (!result)
    return nullptr;

  nsIAtom* frameType = result->GetType();
  if (frameType != nsGkAtoms::svgFilterFrame)
    return nullptr;

  return static_cast<nsSVGFilterFrame*>(result);
}

nsSVGFilterFrame *
nsSVGFilterFrame::GetReferencedFilterIfNotInUse()
{
  nsSVGFilterFrame *referenced = GetReferencedFilter();
  if (!referenced)
    return nullptr;

  if (referenced->mLoopFlag) {
    // XXXjwatt: we should really send an error to the JavaScript Console here:
    NS_WARNING("Filter reference loop detected while inheriting attribute!");
    return nullptr;
  }

  return referenced;
}

NS_IMETHODIMP
nsSVGFilterFrame::AttributeChanged(int32_t  aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   int32_t  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width ||
       aAttribute == nsGkAtoms::height ||
       aAttribute == nsGkAtoms::filterRes ||
       aAttribute == nsGkAtoms::filterUnits ||
       aAttribute == nsGkAtoms::primitiveUnits)) {
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  } else if (aNameSpaceID == kNameSpaceID_XLink &&
             aAttribute == nsGkAtoms::href) {
    // Blow away our reference, if any
    Properties().Delete(nsSVGEffects::HrefProperty());
    mNoHRefURI = false;
    // And update whoever references us
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  }
  return nsSVGFilterFrameBase::AttributeChanged(aNameSpaceID,
                                                aAttribute, aModType);
}

nsresult
nsSVGFilterFrame::PaintFilteredFrame(nsRenderingContext *aContext,
                                     nsIFrame *aFilteredFrame,
                                     nsSVGFilterPaintCallback *aPaintCallback,
                                     const nsRect *aDirtyArea,
                                     nsIFrame* aTransformRoot)
{
  nsAutoFilterInstance instance(aFilteredFrame, this, aPaintCallback,
                                aDirtyArea, nullptr, nullptr, nullptr,
                                aTransformRoot);
  if (!instance.get()) {
    return NS_OK;
  }
  return instance.get()->Render(aContext->ThebesContext());
}

static nsRect
TransformFilterSpaceToFrameSpace(nsSVGFilterInstance *aInstance,
                                 nsIntRect *aRect)
{
  if (aRect->IsEmpty()) {
    return nsRect();
  }
  gfxMatrix m = aInstance->GetFilterSpaceToFrameSpaceInCSSPxTransform();
  gfxRect r(aRect->x, aRect->y, aRect->width, aRect->height);
  r = m.TransformBounds(r);
  return nsLayoutUtils::RoundGfxRectToAppRect(r, aInstance->AppUnitsPerCSSPixel());
}

nsRect
nsSVGFilterFrame::GetPostFilterDirtyArea(nsIFrame *aFilteredFrame,
                                         const nsRect& aPreFilterDirtyRect)
{
  if (aPreFilterDirtyRect.IsEmpty()) {
    return nsRect();
  }

  nsAutoFilterInstance instance(aFilteredFrame, this, nullptr, nullptr,
                                &aPreFilterDirtyRect, nullptr);
  if (!instance.get()) {
    return nsRect();
  }
  // We've passed in the source's dirty area so the instance knows about it.
  // Now we can ask the instance to compute the area of the filter output
  // that's dirty.
  nsIntRect dirtyRect;
  nsresult rv = instance.get()->ComputePostFilterDirtyRect(&dirtyRect);
  if (NS_SUCCEEDED(rv)) {
    return TransformFilterSpaceToFrameSpace(instance.get(), &dirtyRect);
  }
  return nsRect();
}

nsRect
nsSVGFilterFrame::GetPreFilterNeededArea(nsIFrame *aFilteredFrame,
                                         const nsRect& aPostFilterDirtyRect)
{
  nsAutoFilterInstance instance(aFilteredFrame, this, nullptr,
                                &aPostFilterDirtyRect, nullptr, nullptr);
  if (!instance.get()) {
    return nsRect();
  }
  // Now we can ask the instance to compute the area of the source
  // that's needed.
  nsIntRect neededRect;
  nsresult rv = instance.get()->ComputeSourceNeededRect(&neededRect);
  if (NS_SUCCEEDED(rv)) {
    return TransformFilterSpaceToFrameSpace(instance.get(), &neededRect);
  }
  return nsRect();
}

nsRect
nsSVGFilterFrame::GetPostFilterBounds(nsIFrame *aFilteredFrame,
                                      const gfxRect *aOverrideBBox,
                                      const nsRect *aPreFilterBounds)
{
  MOZ_ASSERT(!(aFilteredFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) ||
             !(aFilteredFrame->GetStateBits() & NS_FRAME_IS_NONDISPLAY),
             "Non-display SVG do not maintain visual overflow rects");

  nsAutoFilterInstance instance(aFilteredFrame, this, nullptr, nullptr,
                                aPreFilterBounds, aPreFilterBounds,
                                aOverrideBBox);
  if (!instance.get()) {
    return nsRect();
  }
  nsIntRect bbox;
  nsresult rv = instance.get()->ComputePostFilterExtents(&bbox);
  if (NS_SUCCEEDED(rv)) {
    return TransformFilterSpaceToFrameSpace(instance.get(), &bbox);
  }
  return nsRect();
}

#ifdef DEBUG
void
nsSVGFilterFrame::Init(nsIContent* aContent,
                       nsIFrame* aParent,
                       nsIFrame* aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVG(nsGkAtoms::filter),
               "Content is not an SVG filter");

  nsSVGFilterFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
nsSVGFilterFrame::GetType() const
{
  return nsGkAtoms::svgFilterFrame;
}
