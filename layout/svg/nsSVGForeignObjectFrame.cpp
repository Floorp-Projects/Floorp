/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGForeignObjectFrame.h"

// Keep others in (case-insensitive) order:
#include "DrawResult.h"
#include "gfxContext.h"
#include "nsDisplayList.h"
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
using namespace mozilla::image;

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
  : nsContainerFrame(aContext, kClassID)
  , mInReflow(false)
{
  AddStateBits(NS_FRAME_REFLOW_ROOT | NS_FRAME_MAY_BE_TRANSFORMED |
               NS_FRAME_SVG_LAYOUT);
}

//----------------------------------------------------------------------
// nsIFrame methods

NS_QUERYFRAME_HEAD(nsSVGForeignObjectFrame)
  NS_QUERYFRAME_ENTRY(nsSVGDisplayableFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void
nsSVGForeignObjectFrame::Init(nsIContent*       aContent,
                              nsContainerFrame* aParent,
                              nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::foreignObject),
               "Content is not an SVG foreignObject!");

  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);
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
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

nsresult
nsSVGForeignObjectFrame::AttributeChanged(int32_t  aNameSpaceID,
                                          nsIAtom *aAttribute,
                                          int32_t  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::width ||
        aAttribute == nsGkAtoms::height) {
      nsLayoutUtils::PostRestyleEvent(
        mContent->AsElement(), nsRestyleHint(0),
        nsChangeHint_InvalidateRenderingObservers);
      nsSVGUtils::ScheduleReflowSVG(this);
      // XXXjwatt: why mark intrinsic widths dirty? can't we just use eResize?
      RequestReflow(nsIPresShell::eStyleChange);
    } else if (aAttribute == nsGkAtoms::x ||
               aAttribute == nsGkAtoms::y) {
      // make sure our cached transform matrix gets (lazily) updated
      mCanvasTM = nullptr;
      nsLayoutUtils::PostRestyleEvent(
        mContent->AsElement(), nsRestyleHint(0),
        nsChangeHint_InvalidateRenderingObservers);
      nsSVGUtils::ScheduleReflowSVG(this);
    } else if (aAttribute == nsGkAtoms::transform) {
      // We don't invalidate for transform changes (the layers code does that).
      // Also note that SVGTransformableElement::GetAttributeChangeHint will
      // return nsChangeHint_UpdateOverflow for "transform" attribute changes
      // and cause DoApplyRenderingChangeToTree to make the SchedulePaint call.
      mCanvasTM = nullptr;
    } else if (aAttribute == nsGkAtoms::viewBox ||
               aAttribute == nsGkAtoms::preserveAspectRatio) {
      nsLayoutUtils::PostRestyleEvent(
        mContent->AsElement(), nsRestyleHint(0),
        nsChangeHint_InvalidateRenderingObservers);
    }
  }

  return NS_OK;
}

void
nsSVGForeignObjectFrame::Reflow(nsPresContext*           aPresContext,
                                ReflowOutput&     aDesiredSize,
                                const ReflowInput& aReflowInput,
                                nsReflowStatus&          aStatus)
{
  MOZ_ASSERT(!(GetStateBits() & NS_FRAME_IS_NONDISPLAY),
             "Should not have been called");

  // Only InvalidateAndScheduleBoundsUpdate marks us with NS_FRAME_IS_DIRTY,
  // so if that bit is still set we still have a resize pending. If we hit
  // this assertion, then we should get the presShell to skip reflow roots
  // that have a dirty parent since a reflow is going to come via the
  // reflow root's parent anyway.
  NS_ASSERTION(!(GetStateBits() & NS_FRAME_IS_DIRTY),
               "Reflowing while a resize is pending is wasteful");

  // ReflowSVG makes sure mRect is up to date before we're called.

  NS_ASSERTION(!aReflowInput.mParentReflowInput,
               "should only get reflow from being reflow root");
  NS_ASSERTION(aReflowInput.ComputedWidth() == GetSize().width &&
               aReflowInput.ComputedHeight() == GetSize().height,
               "reflow roots should be reflowed at existing size and "
               "svg.css should ensure we have no padding/border/margin");

  DoReflow();

  WritingMode wm = aReflowInput.GetWritingMode();
  LogicalSize finalSize(wm, aReflowInput.ComputedISize(),
                        aReflowInput.ComputedBSize());
  aDesiredSize.SetSize(wm, finalSize);
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  aStatus.Reset();
}

void
nsSVGForeignObjectFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                          const nsRect&           aDirtyRect,
                                          const nsDisplayListSet& aLists)
{
  if (!static_cast<const nsSVGElement*>(mContent)->HasValidDimensions()) {
    return;
  }
  DisplayOutline(aBuilder, aLists);
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
      *aOwnTransform = gfx::ToMatrix(content->PrependLocalTransformsTo(
                                       gfxMatrix(),
                                       eUserSpaceToParent));
    }
    foundTransform = true;
  }
  return foundTransform;
}

void
nsSVGForeignObjectFrame::PaintSVG(gfxContext& aContext,
                                  const gfxMatrix& aTransform,
                                  imgDrawingParams& aImgParams,
                                  const nsIntRect* aDirtyRect)
{
  NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
               (mState & NS_FRAME_IS_NONDISPLAY),
               "If display lists are enabled, only painting of non-display "
               "SVG should take this code path");

  if (IsDisabled()) {
    return;
  }

  nsIFrame* kid = PrincipalChildList().FirstChild();
  if (!kid) {
    return;
  }

  if (aTransform.IsSingular()) {
    NS_WARNING("Can't render foreignObject element!");
    return;
  }

  nsRect kidDirtyRect = kid->GetVisualOverflowRect();

  /* Check if we need to draw anything. */
  if (aDirtyRect) {
    NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
                 (mState & NS_FRAME_IS_NONDISPLAY),
                 "Display lists handle dirty rect intersection test");
    // Transform the dirty rect into app units in our userspace.
    gfxMatrix invmatrix = aTransform;
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
      return;
  }

  aContext.Save();

  if (StyleDisplay()->IsScrollableOverflow()) {
    float x, y, width, height;
    static_cast<nsSVGElement*>(mContent)->
      GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

    gfxRect clipRect =
      nsSVGUtils::GetClipRectForFrame(this, 0.0f, 0.0f, width, height);
    nsSVGUtils::SetClipRect(&aContext, aTransform, clipRect);
  }

  // SVG paints in CSS px, but normally frames paint in dev pixels. Here we
  // multiply a CSS-px-to-dev-pixel factor onto aTransform so our children
  // paint correctly.
  float cssPxPerDevPx = PresContext()->
    AppUnitsToFloatCSSPixels(PresContext()->AppUnitsPerDevPixel());
  gfxMatrix canvasTMForChildren = aTransform;
  canvasTMForChildren.Scale(cssPxPerDevPx, cssPxPerDevPx);

  aContext.Multiply(canvasTMForChildren);

  using PaintFrameFlags = nsLayoutUtils::PaintFrameFlags;
  PaintFrameFlags flags = PaintFrameFlags::PAINT_IN_TRANSFORM;
  if (SVGAutoRenderState::IsPaintingToWindow(aContext.GetDrawTarget())) {
    flags |= PaintFrameFlags::PAINT_TO_WINDOW;
  }
  if (aImgParams.imageFlags & imgIContainer::FLAG_SYNC_DECODE) {
    flags |= PaintFrameFlags::PAINT_SYNC_DECODE_IMAGES;
  }
  nsRenderingContext rendCtx(&aContext);
  Unused << nsLayoutUtils::PaintFrame(&rendCtx, kid, nsRegion(kidDirtyRect),
                                      NS_RGBA(0,0,0,0),
                                      nsDisplayListBuilderMode::PAINTING,
                                      flags);

  aContext.Restore();
}

nsIFrame*
nsSVGForeignObjectFrame::GetFrameForPoint(const gfxPoint& aPoint)
{
  NS_ASSERTION(!NS_SVGDisplayListHitTestingEnabled() ||
               (mState & NS_FRAME_IS_NONDISPLAY),
               "If display lists are enabled, only hit-testing of a "
               "clipPath's contents should take this code path");

  if (IsDisabled() || (GetStateBits() & NS_FRAME_IS_NONDISPLAY))
    return nullptr;

  nsIFrame* kid = PrincipalChildList().FirstChild();
  if (!kid)
    return nullptr;

  float x, y, width, height;
  static_cast<nsSVGElement*>(mContent)->
    GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

  if (!gfxRect(x, y, width, height).Contains(aPoint) ||
      !nsSVGUtils::HitTestClip(this, aPoint)) {
    return nullptr;
  }

  // Convert the point to app units relative to the top-left corner of the
  // viewport that's established by the foreignObject element:

  gfxPoint pt = (aPoint + gfxPoint(x, y)) * nsPresContext::AppUnitsPerCSSPixel();
  nsPoint point = nsPoint(NSToIntRound(pt.x), NSToIntRound(pt.y));

  return nsLayoutUtils::GetFrameForPoint(kid, point);
}

void
nsSVGForeignObjectFrame::ReflowSVG()
{
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  MOZ_ASSERT(!(GetStateBits() & NS_FRAME_IS_NONDISPLAY),
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
  nsIFrame* kid = PrincipalChildList().FirstChild();
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
  if (StyleEffects()->HasFilters()) {
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
  MOZ_ASSERT(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
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
nsSVGForeignObjectFrame::GetCanvasTM()
{
  if (!mCanvasTM) {
    NS_ASSERTION(GetParent(), "null parent");

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(GetParent());
    SVGForeignObjectElement *content =
      static_cast<SVGForeignObjectElement*>(mContent);

    gfxMatrix tm = content->PrependLocalTransformsTo(parent->GetCanvasTM());

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

  nsIFrame* kid = PrincipalChildList().FirstChild();
  if (!kid)
    return;

  PresContext()->PresShell()->FrameNeedsReflow(kid, aType, NS_FRAME_IS_DIRTY);
}

void
nsSVGForeignObjectFrame::DoReflow()
{
  MarkInReflow();
  // Skip reflow if we're zero-sized, unless this is our first reflow.
  if (IsDisabled() &&
      !(GetStateBits() & NS_FRAME_FIRST_REFLOW))
    return;

  nsPresContext *presContext = PresContext();
  nsIFrame* kid = PrincipalChildList().FirstChild();
  if (!kid)
    return;

  // initiate a synchronous reflow here and now:  
  nsRenderingContext renderingContext(
    presContext->PresShell()->CreateReferenceRenderingContext());

  mInReflow = true;

  WritingMode wm = kid->GetWritingMode();
  ReflowInput reflowInput(presContext, kid,
                                &renderingContext,
                                LogicalSize(wm, ISize(wm),
                                            NS_UNCONSTRAINEDSIZE));
  ReflowOutput desiredSize(reflowInput);
  nsReflowStatus status;

  // We don't use mRect.height above because that tells the child to do
  // page/column breaking at that height.
  NS_ASSERTION(reflowInput.ComputedPhysicalBorderPadding() == nsMargin(0, 0, 0, 0) &&
               reflowInput.ComputedPhysicalMargin() == nsMargin(0, 0, 0, 0),
               "style system should ensure that :-moz-svg-foreign-content "
               "does not get styled");
  NS_ASSERTION(reflowInput.ComputedISize() == ISize(wm),
               "reflow state made child wrong size");
  reflowInput.SetComputedBSize(BSize(wm));

  ReflowChild(kid, presContext, desiredSize, reflowInput, 0, 0,
              NS_FRAME_NO_MOVE_FRAME, status);
  NS_ASSERTION(mRect.width == desiredSize.Width() &&
               mRect.height == desiredSize.Height(), "unexpected size");
  FinishReflowChild(kid, presContext, desiredSize, &reflowInput, 0, 0,
                    NS_FRAME_NO_MOVE_FRAME);
  
  mInReflow = false;
}

nsRect
nsSVGForeignObjectFrame::GetInvalidRegion()
{
  MOZ_ASSERT(!NS_SVGDisplayListPaintingEnabled(),
             "Only called by nsDisplayOuterSVG code");

  nsIFrame* kid = PrincipalChildList().FirstChild();
  if (kid->HasInvalidFrameInSubtree()) {
    gfxRect r(mRect.x, mRect.y, mRect.width, mRect.height);
    r.Scale(1.0 / nsPresContext::AppUnitsPerCSSPixel());
    nsRect rect = nsSVGUtils::ToCanvasBounds(r, GetCanvasTM(), PresContext());
    rect = nsSVGUtils::GetPostFilterVisualOverflowRect(this, rect);
    return rect;
  }
  return nsRect();
}

void
nsSVGForeignObjectFrame::DoUpdateStyleOfOwnedAnonBoxes(
  ServoStyleSet& aStyleSet,
  nsStyleChangeList& aChangeList,
  nsChangeHint aHintForThisFrame)
{
  MOZ_ASSERT(PrincipalChildList().FirstChild(), "Must have our anon box");
  UpdateStyleOfChildAnonBox(PrincipalChildList().FirstChild(),
                            aStyleSet, aChangeList, aHintForThisFrame);
}
