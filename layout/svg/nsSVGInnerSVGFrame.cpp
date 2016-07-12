/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGInnerSVGFrame.h"

// Keep others in (case-insensitive) order:
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "nsIFrame.h"
#include "nsISVGChildFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGIntegrationUtils.h"
#include "mozilla/dom/SVGSVGElement.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

nsIFrame*
NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGInnerSVGFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGInnerSVGFrame)

//----------------------------------------------------------------------
// nsIFrame methods

NS_QUERYFRAME_HEAD(nsSVGInnerSVGFrame)
  NS_QUERYFRAME_ENTRY(nsSVGInnerSVGFrame)
  NS_QUERYFRAME_ENTRY(nsISVGSVGFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGDisplayContainerFrame)

#ifdef DEBUG
void
nsSVGInnerSVGFrame::Init(nsIContent*       aContent,
                         nsContainerFrame* aParent,
                         nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::svg),
               "Content is not an SVG 'svg' element!");

  nsSVGDisplayContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
nsSVGInnerSVGFrame::GetType() const
{
  return nsGkAtoms::svgInnerSVGFrame;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

DrawResult
nsSVGInnerSVGFrame::PaintSVG(gfxContext& aContext,
                             const gfxMatrix& aTransform,
                             const nsIntRect *aDirtyRect)
{
  NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
               (mState & NS_FRAME_IS_NONDISPLAY),
               "If display lists are enabled, only painting of non-display "
               "SVG should take this code path");

  gfxContextAutoSaveRestore autoSR;

  if (StyleDisplay()->IsScrollableOverflow()) {
    float x, y, width, height;
    static_cast<SVGSVGElement*>(mContent)->
      GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

    if (width <= 0 || height <= 0) {
      return DrawResult::SUCCESS;
    }

    autoSR.SetContext(&aContext);
    gfxRect clipRect =
      nsSVGUtils::GetClipRectForFrame(this, x, y, width, height);
    nsSVGUtils::SetClipRect(&aContext, aTransform, clipRect);
  }

  return nsSVGDisplayContainerFrame::PaintSVG(aContext, aTransform, aDirtyRect);
}

nsRect
nsSVGInnerSVGFrame::GetCoveredRegion()
{
  float x, y, w, h;
  static_cast<SVGSVGElement*>(mContent)->
    GetAnimatedLengthValues(&x, &y, &w, &h, nullptr);
  if (w < 0.0f) w = 0.0f;
  if (h < 0.0f) h = 0.0f;
  // GetCanvasTM includes the x,y translation
  nsRect bounds = nsSVGUtils::ToCanvasBounds(gfxRect(0.0, 0.0, w, h),
                                             GetCanvasTM(),
                                             PresContext());

  if (!StyleDisplay()->IsScrollableOverflow()) {
    bounds.UnionRect(bounds, nsSVGUtils::GetCoveredRegion(mFrames));
  }
  return bounds;
}

void
nsSVGInnerSVGFrame::ReflowSVG()
{
  // mRect must be set before FinishAndStoreOverflow is called in order
  // for our overflow areas to be clipped correctly.
  float x, y, width, height;
  static_cast<SVGSVGElement*>(mContent)->
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
nsSVGInnerSVGFrame::NotifySVGChanged(uint32_t aFlags)
{
  MOZ_ASSERT(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
             "Invalidation logic may need adjusting");

  if (aFlags & COORD_CONTEXT_CHANGED) {

    SVGSVGElement *svg = static_cast<SVGSVGElement*>(mContent);

    bool xOrYIsPercentage =
      svg->mLengthAttributes[SVGSVGElement::ATTR_X].IsPercentage() ||
      svg->mLengthAttributes[SVGSVGElement::ATTR_Y].IsPercentage();
    bool widthOrHeightIsPercentage =
      svg->mLengthAttributes[SVGSVGElement::ATTR_WIDTH].IsPercentage() ||
      svg->mLengthAttributes[SVGSVGElement::ATTR_HEIGHT].IsPercentage();

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

nsresult
nsSVGInnerSVGFrame::AttributeChanged(int32_t  aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     int32_t  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      !(GetStateBits() & NS_FRAME_IS_NONDISPLAY)) {

    SVGSVGElement* content = static_cast<SVGSVGElement*>(mContent);

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
nsSVGInnerSVGFrame::GetFrameForPoint(const gfxPoint& aPoint)
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
nsSVGInnerSVGFrame::NotifyViewportOrTransformChanged(uint32_t aFlags)
{
  // The dimensions of inner-<svg> frames are purely defined by their "width"
  // and "height" attributes, and transform changes can only occur as a result
  // of changes to their "width", "height", "viewBox" or "preserveAspectRatio"
  // attributes. Changes to all of these attributes are handled in
  // AttributeChanged(), so we should never be called.
  NS_ERROR("Not called for nsSVGInnerSVGFrame");
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

gfxMatrix
nsSVGInnerSVGFrame::GetCanvasTM()
{
  if (!mCanvasTM) {
    NS_ASSERTION(GetParent(), "null parent");

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(GetParent());
    SVGSVGElement *content = static_cast<SVGSVGElement*>(mContent);

    gfxMatrix tm = content->PrependLocalTransformsTo(parent->GetCanvasTM());

    mCanvasTM = new gfxMatrix(tm);
  }
  return *mCanvasTM;
}

bool
nsSVGInnerSVGFrame::HasChildrenOnlyTransform(gfx::Matrix *aTransform) const
{
  SVGSVGElement *content = static_cast<SVGSVGElement*>(mContent);

  if (content->HasViewBoxOrSyntheticViewBox()) {
    // XXX Maybe return false if the transform is the identity transform?
    if (aTransform) {
      *aTransform = content->GetViewBoxTransform();
    }
    return true;
  }
  return false;
}
