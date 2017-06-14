/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGViewportFrame.h"

// Keep others in (case-insensitive) order:
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "nsIFrame.h"
#include "nsSVGDisplayableFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGIntegrationUtils.h"
#include "mozilla/dom/SVGViewportElement.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

//----------------------------------------------------------------------
// nsSVGDisplayableFrame methods

void
nsSVGViewportFrame::PaintSVG(gfxContext& aContext,
                             const gfxMatrix& aTransform,
                             imgDrawingParams& aImgParams,
                             const nsIntRect *aDirtyRect)
{
  NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
               (mState & NS_FRAME_IS_NONDISPLAY),
               "If display lists are enabled, only painting of non-display "
               "SVG should take this code path");

  gfxContextAutoSaveRestore autoSR;

  if (StyleDisplay()->IsScrollableOverflow()) {
    float x, y, width, height;
    static_cast<SVGViewportElement*>(mContent)->
      GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

    if (width <= 0 || height <= 0) {
      return;
    }

    autoSR.SetContext(&aContext);
    gfxRect clipRect =
      nsSVGUtils::GetClipRectForFrame(this, x, y, width, height);
    nsSVGUtils::SetClipRect(&aContext, aTransform, clipRect);
  }

  nsSVGDisplayContainerFrame::PaintSVG(aContext, aTransform, aImgParams,
                                       aDirtyRect);
}

void
nsSVGViewportFrame::ReflowSVG()
{
  // mRect must be set before FinishAndStoreOverflow is called in order
  // for our overflow areas to be clipped correctly.
  float x, y, width, height;
  static_cast<SVGViewportElement*>(mContent)->
    GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);
  mRect = nsLayoutUtils::RoundGfxRectToAppRect(
                           gfxRect(x, y, width, height),
                           PresContext()->AppUnitsPerCSSPixel());

  // If we have a filter, we need to invalidate ourselves because filter
  // output can change even if none of our descendants need repainting.
  if (StyleEffects()->HasFilters()) {
    InvalidateFrame();
  }

  nsSVGDisplayContainerFrame::ReflowSVG();
}

void
nsSVGViewportFrame::NotifySVGChanged(uint32_t aFlags)
{
  MOZ_ASSERT(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
             "Invalidation logic may need adjusting");

  if (aFlags & COORD_CONTEXT_CHANGED) {

    SVGViewportElement *svg = static_cast<SVGViewportElement*>(mContent);

    bool xOrYIsPercentage =
      svg->mLengthAttributes[SVGViewportElement::ATTR_X].IsPercentage() ||
      svg->mLengthAttributes[SVGViewportElement::ATTR_Y].IsPercentage();
    bool widthOrHeightIsPercentage =
      svg->mLengthAttributes[SVGViewportElement::ATTR_WIDTH].IsPercentage() ||
      svg->mLengthAttributes[SVGViewportElement::ATTR_HEIGHT].IsPercentage();

    if (xOrYIsPercentage || widthOrHeightIsPercentage) {
      // Ancestor changes can't affect how we render from the perspective of
      // any rendering observers that we may have, so we don't need to
      // invalidate them. We also don't need to invalidate ourself, since our
      // changed ancestor will have invalidated its entire area, which includes
      // our area.
      // For perf reasons we call this before calling NotifySVGChanged() below.
      nsSVGUtils::ScheduleReflowSVG(this);
    }

    // Coordinate context changes affect mCanvasTM if we have a
    // percentage 'x' or 'y', or if we have a percentage 'width' or 'height' AND
    // a 'viewBox'.

    if (!(aFlags & TRANSFORM_CHANGED) &&
        (xOrYIsPercentage ||
         (widthOrHeightIsPercentage && svg->HasViewBoxRect()))) {
      aFlags |= TRANSFORM_CHANGED;
    }

    if (svg->HasViewBoxRect() || !widthOrHeightIsPercentage) {
      // Remove COORD_CONTEXT_CHANGED, since we establish the coordinate
      // context for our descendants and this notification won't change its
      // dimensions:
      aFlags &= ~COORD_CONTEXT_CHANGED;

      if (!aFlags) {
        return; // No notification flags left
      }
    }
  }

  if (aFlags & TRANSFORM_CHANGED) {
    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nullptr;
  }

  nsSVGDisplayContainerFrame::NotifySVGChanged(aFlags);
}

SVGBBox
nsSVGViewportFrame::GetBBoxContribution(const Matrix &aToBBoxUserspace,
                                        uint32_t aFlags)
{
  // XXXjwatt It seems like authors would want the result to be clipped by the
  // viewport we establish if IsScrollableOverflow() is true.  We should
  // consider doing that.  See bug 1350755.

  SVGBBox bbox;

  if (aFlags & nsSVGUtils::eForGetClientRects) {
    // XXXjwatt For consistency with the old code this code includes the
    // viewport we establish in the result, but only includes the bounds of our
    // descendants if they are not clipped to that viewport.  However, this is
    // both inconsistent with Chrome and with the specs.  See bug 1350755.
    // Ideally getClientRects/getBoundingClientRect should be consistent with
    // getBBox.
    float x, y, w, h;
    static_cast<SVGViewportElement*>(mContent)->
      GetAnimatedLengthValues(&x, &y, &w, &h, nullptr);
    if (w < 0.0f) w = 0.0f;
    if (h < 0.0f) h = 0.0f;
    Rect viewport(x, y, w, h);
    bbox = aToBBoxUserspace.TransformBounds(viewport);
    if (StyleDisplay()->IsScrollableOverflow()) {
      return bbox;
    }
    // Else we're not clipping to our viewport so we fall through and include
    // the bounds of our children.
  }

  SVGBBox descendantsBbox =
    nsSVGDisplayContainerFrame::GetBBoxContribution(aToBBoxUserspace, aFlags);

  bbox.UnionEdges(descendantsBbox);

  return bbox;
}

nsresult
nsSVGViewportFrame::AttributeChanged(int32_t  aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     int32_t  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      !(GetStateBits() & NS_FRAME_IS_NONDISPLAY)) {

    SVGViewportElement* content = static_cast<SVGViewportElement*>(mContent);

    if (aAttribute == nsGkAtoms::width ||
        aAttribute == nsGkAtoms::height) {
      nsLayoutUtils::PostRestyleEvent(
        mContent->AsElement(), nsRestyleHint(0),
        nsChangeHint_InvalidateRenderingObservers);
      nsSVGUtils::ScheduleReflowSVG(this);

      if (content->HasViewBoxOrSyntheticViewBox()) {
        // make sure our cached transform matrix gets (lazily) updated
        mCanvasTM = nullptr;
        content->ChildrenOnlyTransformChanged();
        nsSVGUtils::NotifyChildrenOfSVGChange(this, TRANSFORM_CHANGED);
      } else {
        uint32_t flags = COORD_CONTEXT_CHANGED;
        if (mCanvasTM && mCanvasTM->IsSingular()) {
          mCanvasTM = nullptr;
          flags |= TRANSFORM_CHANGED;
        }
        nsSVGUtils::NotifyChildrenOfSVGChange(this, flags);
      }

    } else if (aAttribute == nsGkAtoms::transform ||
               aAttribute == nsGkAtoms::preserveAspectRatio ||
               aAttribute == nsGkAtoms::viewBox ||
               aAttribute == nsGkAtoms::x ||
               aAttribute == nsGkAtoms::y) {
      // make sure our cached transform matrix gets (lazily) updated
      mCanvasTM = nullptr;

      nsSVGUtils::NotifyChildrenOfSVGChange(
          this, aAttribute == nsGkAtoms::viewBox ?
                  TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED : TRANSFORM_CHANGED);

      // We don't invalidate for transform changes (the layers code does that).
      // Also note that SVGTransformableElement::GetAttributeChangeHint will
      // return nsChangeHint_UpdateOverflow for "transform" attribute changes
      // and cause DoApplyRenderingChangeToTree to make the SchedulePaint call.

      if (aAttribute == nsGkAtoms::x || aAttribute == nsGkAtoms::y) {
        nsLayoutUtils::PostRestyleEvent(
          mContent->AsElement(), nsRestyleHint(0),
          nsChangeHint_InvalidateRenderingObservers);
        nsSVGUtils::ScheduleReflowSVG(this);
      } else if (aAttribute == nsGkAtoms::viewBox ||
                 (aAttribute == nsGkAtoms::preserveAspectRatio &&
                  content->HasViewBoxOrSyntheticViewBox())) {
        content->ChildrenOnlyTransformChanged();
        // SchedulePaint sets a global state flag so we only need to call it once
        // (on ourself is fine), not once on each child (despite bug 828240).
        SchedulePaint();
      }
    }
  }

  return NS_OK;
}

nsIFrame*
nsSVGViewportFrame::GetFrameForPoint(const gfxPoint& aPoint)
{
  NS_ASSERTION(!NS_SVGDisplayListHitTestingEnabled() ||
               (mState & NS_FRAME_IS_NONDISPLAY),
               "If display lists are enabled, only hit-testing of non-display "
               "SVG should take this code path");

  if (StyleDisplay()->IsScrollableOverflow()) {
    Rect clip;
    static_cast<nsSVGElement*>(mContent)->
      GetAnimatedLengthValues(&clip.x, &clip.y,
                              &clip.width, &clip.height, nullptr);
    if (!clip.Contains(ToPoint(aPoint))) {
      return nullptr;
    }
  }

  return nsSVGDisplayContainerFrame::GetFrameForPoint(aPoint);
}

//----------------------------------------------------------------------
// nsISVGSVGFrame methods:

void
nsSVGViewportFrame::NotifyViewportOrTransformChanged(uint32_t aFlags)
{
  // The dimensions of inner-<svg> frames are purely defined by their "width"
  // and "height" attributes, and transform changes can only occur as a result
  // of changes to their "width", "height", "viewBox" or "preserveAspectRatio"
  // attributes. Changes to all of these attributes are handled in
  // AttributeChanged(), so we should never be called.
  NS_ERROR("Not called for nsSVGViewportFrame");
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

gfxMatrix
nsSVGViewportFrame::GetCanvasTM()
{
  if (!mCanvasTM) {
    NS_ASSERTION(GetParent(), "null parent");

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(GetParent());
    SVGViewportElement *content = static_cast<SVGViewportElement*>(mContent);

    gfxMatrix tm = content->PrependLocalTransformsTo(parent->GetCanvasTM());

    mCanvasTM = new gfxMatrix(tm);
  }
  return *mCanvasTM;
}

bool
nsSVGViewportFrame::HasChildrenOnlyTransform(gfx::Matrix *aTransform) const
{
  SVGViewportElement *content = static_cast<SVGViewportElement*>(mContent);

  if (content->HasViewBoxOrSyntheticViewBox()) {
    // XXX Maybe return false if the transform is the identity transform?
    if (aTransform) {
      *aTransform = content->GetViewBoxTransform();
    }
    return true;
  }
  return false;
}
