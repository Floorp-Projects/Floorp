/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGForeignObjectFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxContext.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsLayoutUtils.h"
#include "nsRegion.h"
#include "nsRenderingContext.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGEffects.h"
#include "mozilla/dom/SVGForeignObjectElement.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsSVGUtils.h"
#include "mozilla/AutoRestore.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
// Implementation

nsContainerFrame*
NS_NewSVGForeignObjectFrame(nsIPresShell   *aPresShell,
                            nsStyleContext *aContext)
{
  return new (aPresShell) nsSVGForeignObjectFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGForeignObjectFrame)

nsSVGForeignObjectFrame::nsSVGForeignObjectFrame(nsStyleContext* aContext)
  : nsSVGForeignObjectFrameBase(aContext),
    mInReflow(false)
{
  AddStateBits(NS_FRAME_REFLOW_ROOT | NS_FRAME_MAY_BE_TRANSFORMED |
               NS_FRAME_SVG_LAYOUT);
}

//----------------------------------------------------------------------
// nsIFrame methods

NS_QUERYFRAME_HEAD(nsSVGForeignObjectFrame)
  NS_QUERYFRAME_ENTRY(nsISVGChildFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGForeignObjectFrameBase)

void
nsSVGForeignObjectFrame::Init(nsIContent*       aContent,
                              nsContainerFrame* aParent,
                              nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVG(nsGkAtoms::foreignObject),
               "Content is not an SVG foreignObject!");

  nsSVGForeignObjectFrameBase::Init(aContent, aParent, aPrevInFlow);
  AddStateBits(aParent->GetStateBits() & NS_STATE_SVG_CLIPPATH_CHILD);
  AddStateBits(NS_FRAME_FONT_INFLATION_CONTAINER |
               NS_FRAME_FONT_INFLATION_FLOW_ROOT);
  if (!(mState & NS_FRAME_IS_NONDISPLAY)) {
    nsSVGUtils::GetOuterSVGFrame(this)->RegisterForeignObject(this);
  }
}

void nsSVGForeignObjectFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // Only unregister if we registered in the first place:
  if (!(mState & NS_FRAME_IS_NONDISPLAY)) {
      nsSVGUtils::GetOuterSVGFrame(this)->UnregisterForeignObject(this);
  }
  nsSVGForeignObjectFrameBase::DestroyFrom(aDestructRoot);
}

nsIAtom *
nsSVGForeignObjectFrame::GetType() const
{
  return nsGkAtoms::svgForeignObjectFrame;
}

nsresult
nsSVGForeignObjectFrame::AttributeChanged(int32_t  aNameSpaceID,
                                          nsIAtom *aAttribute,
                                          int32_t  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::width ||
        aAttribute == nsGkAtoms::height) {
      nsSVGEffects::InvalidateRenderingObservers(this);
      nsSVGUtils::ScheduleReflowSVG(this);
      // XXXjwatt: why mark intrinsic widths dirty? can't we just use eResize?
      RequestReflow(nsIPresShell::eStyleChange);
    } else if (aAttribute == nsGkAtoms::x ||
               aAttribute == nsGkAtoms::y) {
      // make sure our cached transform matrix gets (lazily) updated
      mCanvasTM = nullptr;
      nsSVGEffects::InvalidateRenderingObservers(this);
      nsSVGUtils::ScheduleReflowSVG(this);
    } else if (aAttribute == nsGkAtoms::transform) {
      // We don't invalidate for transform changes (the layers code does that).
      // Also note that SVGTransformableElement::GetAttributeChangeHint will
      // return nsChangeHint_UpdateOverflow for "transform" attribute changes
      // and cause DoApplyRenderingChangeToTree to make the SchedulePaint call.
      mCanvasTM = nullptr;
    } else if (aAttribute == nsGkAtoms::viewBox ||
               aAttribute == nsGkAtoms::preserveAspectRatio) {
      nsSVGEffects::InvalidateRenderingObservers(this);
    }
  }

  return NS_OK;
}

void
nsSVGForeignObjectFrame::Reflow(nsPresContext*           aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus)
{
  NS_ABORT_IF_FALSE(!(GetStateBits() & NS_FRAME_IS_NONDISPLAY),
                    "Should not have been called");

  // Only InvalidateAndScheduleBoundsUpdate marks us with NS_FRAME_IS_DIRTY,
  // so if that bit is still set we still have a resize pending. If we hit
  // this assertion, then we should get the presShell to skip reflow roots
  // that have a dirty parent since a reflow is going to come via the
  // reflow root's parent anyway.
  NS_ASSERTION(!(GetStateBits() & NS_FRAME_IS_DIRTY),
               "Reflowing while a resize is pending is wasteful");

  // ReflowSVG makes sure mRect is up to date before we're called.

  NS_ASSERTION(!aReflowState.parentReflowState,
               "should only get reflow from being reflow root");
  NS_ASSERTION(aReflowState.ComputedWidth() == GetSize().width &&
               aReflowState.ComputedHeight() == GetSize().height,
               "reflow roots should be reflowed at existing size and "
               "svg.css should ensure we have no padding/border/margin");

  DoReflow();

  aDesiredSize.Width() = aReflowState.ComputedWidth();
  aDesiredSize.Height() = aReflowState.ComputedHeight();
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  aStatus = NS_FRAME_COMPLETE;
}

void
nsSVGForeignObjectFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                          const nsRect&           aDirtyRect,
                                          const nsDisplayListSet& aLists)
{
  if (!static_cast<const nsSVGElement*>(mContent)->HasValidDimensions()) {
    return;
  }
  BuildDisplayListForNonBlockChildren(aBuilder, aDirtyRect, aLists);
}

bool
nsSVGForeignObjectFrame::IsSVGTransformed(Matrix *aOwnTransform,
                                          Matrix *aFromParentTransform) const
{
  bool foundTransform = false;

  // Check if our parent has children-only transforms:
  nsIFrame *parent = GetParent();
  if (parent &&
      parent->IsFrameOfType(nsIFrame::eSVG | nsIFrame::eSVGContainer)) {
    foundTransform = static_cast<nsSVGContainerFrame*>(parent)->
                       HasChildrenOnlyTransform(aFromParentTransform);
  }

  nsSVGElement *content = static_cast<nsSVGElement*>(mContent);
  nsSVGAnimatedTransformList* transformList =
    content->GetAnimatedTransformList();
  if ((transformList && transformList->HasTransform()) ||
      content->GetAnimateMotionTransform()) {
    if (aOwnTransform) {
      *aOwnTransform = gfx::ToMatrix(content->PrependLocalTransformsTo(gfxMatrix(),
                                  nsSVGElement::eUserSpaceToParent));
    }
    foundTransform = true;
  }
  return foundTransform;
}

nsresult
nsSVGForeignObjectFrame::PaintSVG(nsRenderingContext *aContext,
                                  const nsIntRect *aDirtyRect,
                                  nsIFrame* aTransformRoot)
{
  NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
               (mState & NS_FRAME_IS_NONDISPLAY),
               "If display lists are enabled, only painting of non-display "
               "SVG should take this code path");

  if (IsDisabled())
    return NS_OK;

  nsIFrame* kid = GetFirstPrincipalChild();
  if (!kid)
    return NS_OK;

  gfxMatrix canvasTM = GetCanvasTM(FOR_PAINTING, aTransformRoot);

  if (canvasTM.IsSingular()) {
    NS_WARNING("Can't render foreignObject element!");
    return NS_ERROR_FAILURE;
  }

  nsRect kidDirtyRect = kid->GetVisualOverflowRect();

  /* Check if we need to draw anything. */
  if (aDirtyRect) {
    NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
                 (mState & NS_FRAME_IS_NONDISPLAY),
                 "Display lists handle dirty rect intersection test");
    // Transform the dirty rect into app units in our userspace.
    gfxMatrix invmatrix = canvasTM;
    DebugOnly<bool> ok = invmatrix.Invert();
    NS_ASSERTION(ok, "inverse of non-singular matrix should be non-singular");

    gfxRect transDirtyRect = gfxRect(aDirtyRect->x, aDirtyRect->y,
                                     aDirtyRect->width, aDirtyRect->height);
    transDirtyRect = invmatrix.TransformBounds(transDirtyRect);

    kidDirtyRect.IntersectRect(kidDirtyRect,
      nsLayoutUtils::RoundGfxRectToAppRect(transDirtyRect,
                       PresContext()->AppUnitsPerCSSPixel()));

    // XXX after bug 614732 is fixed, we will compare mRect with aDirtyRect,
    // not with kidDirtyRect. I.e.
    // int32_t appUnitsPerDevPx = PresContext()->AppUnitsPerDevPixel();
    // mRect.ToOutsidePixels(appUnitsPerDevPx).Intersects(*aDirtyRect)
    if (kidDirtyRect.IsEmpty())
      return NS_OK;
  }

  gfxContext *gfx = aContext->ThebesContext();

  gfx->Save();

  if (StyleDisplay()->IsScrollableOverflow()) {
    float x, y, width, height;
    static_cast<nsSVGElement*>(mContent)->
      GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

    gfxRect clipRect =
      nsSVGUtils::GetClipRectForFrame(this, 0.0f, 0.0f, width, height);
    nsSVGUtils::SetClipRect(gfx, canvasTM, clipRect);
  }

  // SVG paints in CSS px, but normally frames paint in dev pixels. Here we
  // multiply a CSS-px-to-dev-pixel factor onto canvasTM so our children paint
  // correctly.
  float cssPxPerDevPx = PresContext()->
    AppUnitsToFloatCSSPixels(PresContext()->AppUnitsPerDevPixel());
  gfxMatrix canvasTMForChildren = canvasTM;
  canvasTMForChildren.Scale(cssPxPerDevPx, cssPxPerDevPx);

  gfx->Multiply(canvasTMForChildren);

  uint32_t flags = nsLayoutUtils::PAINT_IN_TRANSFORM;
  if (SVGAutoRenderState::IsPaintingToWindow(aContext)) {
    flags |= nsLayoutUtils::PAINT_TO_WINDOW;
  }
  nsresult rv = nsLayoutUtils::PaintFrame(aContext, kid, nsRegion(kidDirtyRect),
                                          NS_RGBA(0,0,0,0), flags);

  gfx->Restore();

  return rv;
}

nsIFrame*
nsSVGForeignObjectFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  NS_ASSERTION(!NS_SVGDisplayListHitTestingEnabled() ||
               (mState & NS_FRAME_IS_NONDISPLAY),
               "If display lists are enabled, only hit-testing of a "
               "clipPath's contents should take this code path");

  if (IsDisabled() || (GetStateBits() & NS_FRAME_IS_NONDISPLAY))
    return nullptr;

  nsIFrame* kid = GetFirstPrincipalChild();
  if (!kid)
    return nullptr;

  float x, y, width, height;
  static_cast<nsSVGElement*>(mContent)->
    GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

  gfxMatrix tm = GetCanvasTM(FOR_HIT_TESTING);
  if (!tm.Invert()) {
    return nullptr;
  }

  // Convert aPoint from app units in canvas space to user space:

  gfxPoint pt = gfxPoint(aPoint.x, aPoint.y) / PresContext()->AppUnitsPerCSSPixel();
  pt = tm.Transform(pt);

  if (!gfxRect(0.0f, 0.0f, width, height).Contains(pt))
    return nullptr;

  // Convert pt to app units in *local* space:

  pt = pt * nsPresContext::AppUnitsPerCSSPixel();
  nsPoint point = nsPoint(NSToIntRound(pt.x), NSToIntRound(pt.y));

  nsIFrame *frame = nsLayoutUtils::GetFrameForPoint(kid, point);
  if (frame && nsSVGUtils::HitTestClip(this, aPoint))
    return frame;

  return nullptr;
}

nsRect
nsSVGForeignObjectFrame::GetCoveredRegion()
{
  float x, y, w, h;
  static_cast<SVGForeignObjectElement*>(mContent)->
    GetAnimatedLengthValues(&x, &y, &w, &h, nullptr);
  if (w < 0.0f) w = 0.0f;
  if (h < 0.0f) h = 0.0f;
  // GetCanvasTM includes the x,y translation
  return nsSVGUtils::ToCanvasBounds(gfxRect(0.0, 0.0, w, h),
                                    GetCanvasTM(FOR_OUTERSVG_TM),
                                    PresContext());
}

void
nsSVGForeignObjectFrame::ReflowSVG()
{
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  NS_ABORT_IF_FALSE(!(GetStateBits() & NS_FRAME_IS_NONDISPLAY),
                    "ReflowSVG mechanism not designed for this");

  if (!nsSVGUtils::NeedsReflowSVG(this)) {
    return;
  }

  // We update mRect before the DoReflow call so that DoReflow uses the
  // correct dimensions:

  float x, y, w, h;
  static_cast<SVGForeignObjectElement*>(mContent)->
    GetAnimatedLengthValues(&x, &y, &w, &h, nullptr);

  // If mRect's width or height are negative, reflow blows up! We must clamp!
  if (w < 0.0f) w = 0.0f;
  if (h < 0.0f) h = 0.0f;

  mRect = nsLayoutUtils::RoundGfxRectToAppRect(
                           gfxRect(x, y, w, h),
                           PresContext()->AppUnitsPerCSSPixel());

  // Fully mark our kid dirty so that it gets resized if necessary
  // (NS_FRAME_HAS_DIRTY_CHILDREN isn't enough in that case):
  nsIFrame* kid = GetFirstPrincipalChild();
  kid->AddStateBits(NS_FRAME_IS_DIRTY);

  // Make sure to not allow interrupts if we're not being reflown as a root:
  nsPresContext::InterruptPreventer noInterrupts(PresContext());

  DoReflow();

  if (mState & NS_FRAME_FIRST_REFLOW) {
    // Make sure we have our filter property (if any) before calling
    // FinishAndStoreOverflow (subsequent filter changes are handled off
    // nsChangeHint_UpdateEffects):
    nsSVGEffects::UpdateEffects(this);
  }

  // If we have a filter, we need to invalidate ourselves because filter
  // output can change even if none of our descendants need repainting.
  if (StyleSVGReset()->HasFilters()) {
    InvalidateFrame();
  }

  // TODO: once we support |overflow:visible| on foreignObject, then we will
  // need to take account of our descendants here.
  nsRect overflow = nsRect(nsPoint(0,0), mRect.Size());
  nsOverflowAreas overflowAreas(overflow, overflow);
  FinishAndStoreOverflow(overflowAreas, mRect.Size());

  // Now unset the various reflow bits:
  mState &= ~(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
              NS_FRAME_HAS_DIRTY_CHILDREN);
}

void
nsSVGForeignObjectFrame::NotifySVGChanged(uint32_t aFlags)
{
  NS_ABORT_IF_FALSE(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
                    "Invalidation logic may need adjusting");

  bool needNewBounds = false; // i.e. mRect or visual overflow rect
  bool needReflow = false;
  bool needNewCanvasTM = false;

  if (aFlags & COORD_CONTEXT_CHANGED) {
    SVGForeignObjectElement *fO =
      static_cast<SVGForeignObjectElement*>(mContent);
    // Coordinate context changes affect mCanvasTM if we have a
    // percentage 'x' or 'y'
    if (fO->mLengthAttributes[SVGForeignObjectElement::ATTR_X].IsPercentage() ||
        fO->mLengthAttributes[SVGForeignObjectElement::ATTR_Y].IsPercentage()) {
      needNewBounds = true;
      needNewCanvasTM = true;
    }
    // Our coordinate context's width/height has changed. If we have a
    // percentage width/height our dimensions will change so we must reflow.
    if (fO->mLengthAttributes[SVGForeignObjectElement::ATTR_WIDTH].IsPercentage() ||
        fO->mLengthAttributes[SVGForeignObjectElement::ATTR_HEIGHT].IsPercentage()) {
      needNewBounds = true;
      needReflow = true;
    }
  }

  if (aFlags & TRANSFORM_CHANGED) {
    if (mCanvasTM && mCanvasTM->IsSingular()) {
      needNewBounds = true; // old bounds are bogus
    }
    needNewCanvasTM = true;
    // In an ideal world we would reflow when our CTM changes. This is because
    // glyph metrics do not necessarily scale uniformly with change in scale
    // and, as a result, CTM changes may require text to break at different
    // points. The problem would be how to keep performance acceptable when
    // e.g. the transform of an ancestor is animated.
    // We also seem to get some sort of infinite loop post bug 421584 if we
    // reflow.
  }

  if (needNewBounds) {
    // Ancestor changes can't affect how we render from the perspective of
    // any rendering observers that we may have, so we don't need to
    // invalidate them. We also don't need to invalidate ourself, since our
    // changed ancestor will have invalidated its entire area, which includes
    // our area.
    nsSVGUtils::ScheduleReflowSVG(this);
  }

  // If we're called while the PresShell is handling reflow events then we
  // must have been called as a result of the NotifyViewportChange() call in
  // our nsSVGOuterSVGFrame's Reflow() method. We must not call RequestReflow
  // at this point (i.e. during reflow) because it could confuse the
  // PresShell and prevent it from reflowing us properly in future. Besides
  // that, nsSVGOuterSVGFrame::DidReflow will take care of reflowing us
  // synchronously, so there's no need.
  if (needReflow && !PresContext()->PresShell()->IsReflowLocked()) {
    RequestReflow(nsIPresShell::eResize);
  }

  if (needNewCanvasTM) {
    // Do this after calling InvalidateAndScheduleBoundsUpdate in case we
    // change the code and it needs to use it.
    mCanvasTM = nullptr;
  }
}

SVGBBox
nsSVGForeignObjectFrame::GetBBoxContribution(const Matrix &aToBBoxUserspace,
                                             uint32_t aFlags)
{
  SVGForeignObjectElement *content =
    static_cast<SVGForeignObjectElement*>(mContent);

  float x, y, w, h;
  content->GetAnimatedLengthValues(&x, &y, &w, &h, nullptr);

  if (w < 0.0f) w = 0.0f;
  if (h < 0.0f) h = 0.0f;

  if (aToBBoxUserspace.IsSingular()) {
    // XXX ReportToConsole
    return SVGBBox();
  }
  return aToBBoxUserspace.TransformBounds(gfx::Rect(0.0, 0.0, w, h));
}

//----------------------------------------------------------------------

gfxMatrix
nsSVGForeignObjectFrame::GetCanvasTM(uint32_t aFor, nsIFrame* aTransformRoot)
{
  if (!(GetStateBits() & NS_FRAME_IS_NONDISPLAY) && !aTransformRoot) {
    if (aFor == FOR_PAINTING && NS_SVGDisplayListPaintingEnabled()) {
      return nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(this);
    }
    if (aFor == FOR_HIT_TESTING && NS_SVGDisplayListHitTestingEnabled()) {
      return gfxMatrix();
    }
  }
  if (!mCanvasTM) {
    NS_ASSERTION(GetParent(), "null parent");

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(GetParent());
    SVGForeignObjectElement *content =
      static_cast<SVGForeignObjectElement*>(mContent);

    gfxMatrix tm = content->PrependLocalTransformsTo(
        this == aTransformRoot ? gfxMatrix() :
                                 parent->GetCanvasTM(aFor, aTransformRoot));

    mCanvasTM = new gfxMatrix(tm);
  }
  return *mCanvasTM;
}

//----------------------------------------------------------------------
// Implementation helpers

void nsSVGForeignObjectFrame::RequestReflow(nsIPresShell::IntrinsicDirty aType)
{
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW)
    // If we haven't had a ReflowSVG() yet, nothing to do.
    return;

  nsIFrame* kid = GetFirstPrincipalChild();
  if (!kid)
    return;

  PresContext()->PresShell()->FrameNeedsReflow(kid, aType, NS_FRAME_IS_DIRTY);
}

void
nsSVGForeignObjectFrame::DoReflow()
{
  // Skip reflow if we're zero-sized, unless this is our first reflow.
  if (IsDisabled() &&
      !(GetStateBits() & NS_FRAME_FIRST_REFLOW))
    return;

  nsPresContext *presContext = PresContext();
  nsIFrame* kid = GetFirstPrincipalChild();
  if (!kid)
    return;

  // initiate a synchronous reflow here and now:  
  nsRefPtr<nsRenderingContext> renderingContext =
    presContext->PresShell()->CreateReferenceRenderingContext();

  mInReflow = true;

  nsHTMLReflowState reflowState(presContext, kid,
                                renderingContext,
                                nsSize(mRect.width, NS_UNCONSTRAINEDSIZE));
  nsHTMLReflowMetrics desiredSize(reflowState);
  nsReflowStatus status;

  // We don't use mRect.height above because that tells the child to do
  // page/column breaking at that height.
  NS_ASSERTION(reflowState.ComputedPhysicalBorderPadding() == nsMargin(0, 0, 0, 0) &&
               reflowState.ComputedPhysicalMargin() == nsMargin(0, 0, 0, 0),
               "style system should ensure that :-moz-svg-foreign-content "
               "does not get styled");
  NS_ASSERTION(reflowState.ComputedWidth() == mRect.width,
               "reflow state made child wrong size");
  reflowState.SetComputedHeight(mRect.height);

  ReflowChild(kid, presContext, desiredSize, reflowState, 0, 0,
              NS_FRAME_NO_MOVE_FRAME, status);
  NS_ASSERTION(mRect.width == desiredSize.Width() &&
               mRect.height == desiredSize.Height(), "unexpected size");
  FinishReflowChild(kid, presContext, desiredSize, &reflowState, 0, 0,
                    NS_FRAME_NO_MOVE_FRAME);
  
  mInReflow = false;
}

nsRect
nsSVGForeignObjectFrame::GetInvalidRegion()
{
  nsIFrame* kid = GetFirstPrincipalChild();
  if (kid->HasInvalidFrameInSubtree()) {
    gfxRect r(mRect.x, mRect.y, mRect.width, mRect.height);
    r.Scale(1.0 / nsPresContext::AppUnitsPerCSSPixel());
    nsRect rect = nsSVGUtils::ToCanvasBounds(r, GetCanvasTM(FOR_PAINTING), PresContext());
    rect = nsSVGUtils::GetPostFilterVisualOverflowRect(this, rect);
    return rect;
  }
  return nsRect();
}


