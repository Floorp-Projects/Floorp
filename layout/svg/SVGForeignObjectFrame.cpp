/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGForeignObjectFrame.h"

// Keep others in (case-insensitive) order:
#include "ImgDrawResult.h"
#include "gfxContext.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/PresShell.h"
#include "mozilla/SVGContainerFrame.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/SVGOuterSVGFrame.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/dom/SVGForeignObjectElement.h"
#include "nsDisplayList.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsLayoutUtils.h"
#include "nsRegion.h"
#include "SVGGeometryProperty.h"

using namespace mozilla::dom;
using namespace mozilla::image;
namespace SVGT = SVGGeometryProperty::Tags;

//----------------------------------------------------------------------
// Implementation

nsContainerFrame* NS_NewSVGForeignObjectFrame(mozilla::PresShell* aPresShell,
                                              mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGForeignObjectFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_IMPL_FRAMEARENA_HELPERS(SVGForeignObjectFrame)

SVGForeignObjectFrame::SVGForeignObjectFrame(ComputedStyle* aStyle,
                                             nsPresContext* aPresContext)
    : nsContainerFrame(aStyle, aPresContext, kClassID), mInReflow(false) {
  AddStateBits(NS_FRAME_REFLOW_ROOT | NS_FRAME_MAY_BE_TRANSFORMED |
               NS_FRAME_SVG_LAYOUT);
}

//----------------------------------------------------------------------
// nsIFrame methods

NS_QUERYFRAME_HEAD(SVGForeignObjectFrame)
  NS_QUERYFRAME_ENTRY(ISVGDisplayableFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void SVGForeignObjectFrame::Init(nsIContent* aContent,
                                 nsContainerFrame* aParent,
                                 nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::foreignObject),
               "Content is not an SVG foreignObject!");

  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);
  AddStateBits(aParent->GetStateBits() & NS_STATE_SVG_CLIPPATH_CHILD);
  AddStateBits(NS_FRAME_FONT_INFLATION_CONTAINER |
               NS_FRAME_FONT_INFLATION_FLOW_ROOT);
  AddStateBits(NS_FRAME_MAY_BE_TRANSFORMED);
  if (!(mState & NS_FRAME_IS_NONDISPLAY)) {
    SVGUtils::GetOuterSVGFrame(this)->RegisterForeignObject(this);
  }
}

void SVGForeignObjectFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                        PostDestroyData& aPostDestroyData) {
  // Only unregister if we registered in the first place:
  if (!(mState & NS_FRAME_IS_NONDISPLAY)) {
    SVGUtils::GetOuterSVGFrame(this)->UnregisterForeignObject(this);
  }
  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

nsresult SVGForeignObjectFrame::AttributeChanged(int32_t aNameSpaceID,
                                                 nsAtom* aAttribute,
                                                 int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::transform) {
      // We don't invalidate for transform changes (the layers code does that).
      // Also note that SVGTransformableElement::GetAttributeChangeHint will
      // return nsChangeHint_UpdateOverflow for "transform" attribute changes
      // and cause DoApplyRenderingChangeToTree to make the SchedulePaint call.
      mCanvasTM = nullptr;
    } else if (aAttribute == nsGkAtoms::viewBox ||
               aAttribute == nsGkAtoms::preserveAspectRatio) {
      nsLayoutUtils::PostRestyleEvent(
          mContent->AsElement(), RestyleHint{0},
          nsChangeHint_InvalidateRenderingObservers);
    }
  }

  return NS_OK;
}

void SVGForeignObjectFrame::DidSetComputedStyle(
    ComputedStyle* aOldComputedStyle) {
  nsContainerFrame::DidSetComputedStyle(aOldComputedStyle);

  if (aOldComputedStyle) {
    if (StyleSVGReset()->mX != aOldComputedStyle->StyleSVGReset()->mX ||
        StyleSVGReset()->mY != aOldComputedStyle->StyleSVGReset()->mY) {
      // Invalidate cached transform matrix.
      mCanvasTM = nullptr;
      SVGUtils::ScheduleReflowSVG(this);
    }
  }
}

void SVGForeignObjectFrame::Reflow(nsPresContext* aPresContext,
                                   ReflowOutput& aDesiredSize,
                                   const ReflowInput& aReflowInput,
                                   nsReflowStatus& aStatus) {
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  MOZ_ASSERT(!HasAnyStateBits(NS_FRAME_IS_NONDISPLAY),
             "Should not have been called");

  // Only InvalidateAndScheduleBoundsUpdate marks us with NS_FRAME_IS_DIRTY,
  // so if that bit is still set we still have a resize pending. If we hit
  // this assertion, then we should get the presShell to skip reflow roots
  // that have a dirty parent since a reflow is going to come via the
  // reflow root's parent anyway.
  NS_ASSERTION(!HasAnyStateBits(NS_FRAME_IS_DIRTY),
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
}

void SVGForeignObjectFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                             const nsDisplayListSet& aLists) {
  if (!static_cast<const SVGElement*>(GetContent())->HasValidDimensions()) {
    return;
  }
  nsDisplayList newList(aBuilder);
  nsDisplayListSet set(&newList, &newList, &newList, &newList, &newList,
                       &newList);
  DisplayOutline(aBuilder, set);
  BuildDisplayListForNonBlockChildren(aBuilder, set);
  aLists.Content()->AppendNewToTop<nsDisplayForeignObject>(aBuilder, this,
                                                           &newList);
}

bool SVGForeignObjectFrame::IsSVGTransformed(
    Matrix* aOwnTransform, Matrix* aFromParentTransform) const {
  bool foundTransform = false;

  // Check if our parent has children-only transforms:
  nsIFrame* parent = GetParent();
  if (parent &&
      parent->IsFrameOfType(nsIFrame::eSVG | nsIFrame::eSVGContainer)) {
    foundTransform =
        static_cast<SVGContainerFrame*>(parent)->HasChildrenOnlyTransform(
            aFromParentTransform);
  }

  SVGElement* content = static_cast<SVGElement*>(GetContent());
  SVGAnimatedTransformList* transformList = content->GetAnimatedTransformList();
  if ((transformList && transformList->HasTransform()) ||
      content->GetAnimateMotionTransform()) {
    if (aOwnTransform) {
      *aOwnTransform = gfx::ToMatrix(
          content->PrependLocalTransformsTo(gfxMatrix(), eUserSpaceToParent));
    }
    foundTransform = true;
  }
  return foundTransform;
}

void SVGForeignObjectFrame::PaintSVG(gfxContext& aContext,
                                     const gfxMatrix& aTransform,
                                     imgDrawingParams& aImgParams,
                                     const nsIntRect* aDirtyRect) {
  NS_ASSERTION(
      !NS_SVGDisplayListPaintingEnabled() || (mState & NS_FRAME_IS_NONDISPLAY),
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

  nsRect kidDirtyRect = kid->InkOverflowRect();

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
                               nsLayoutUtils::RoundGfxRectToAppRect(
                                   transDirtyRect, AppUnitsPerCSSPixel()));

    // XXX after bug 614732 is fixed, we will compare mRect with aDirtyRect,
    // not with kidDirtyRect. I.e.
    // int32_t appUnitsPerDevPx = PresContext()->AppUnitsPerDevPixel();
    // mRect.ToOutsidePixels(appUnitsPerDevPx).Intersects(*aDirtyRect)
    if (kidDirtyRect.IsEmpty()) {
      return;
    }
  }

  aContext.Save();

  if (StyleDisplay()->IsScrollableOverflow()) {
    float x, y, width, height;
    SVGGeometryProperty::ResolveAll<SVGT::X, SVGT::Y, SVGT::Width,
                                    SVGT::Height>(
        static_cast<SVGElement*>(GetContent()), &x, &y, &width, &height);

    gfxRect clipRect =
        SVGUtils::GetClipRectForFrame(this, 0.0f, 0.0f, width, height);
    SVGUtils::SetClipRect(&aContext, aTransform, clipRect);
  }

  // SVG paints in CSS px, but normally frames paint in dev pixels. Here we
  // multiply a CSS-px-to-dev-pixel factor onto aTransform so our children
  // paint correctly.
  float cssPxPerDevPx = nsPresContext::AppUnitsToFloatCSSPixels(
      PresContext()->AppUnitsPerDevPixel());
  gfxMatrix canvasTMForChildren = aTransform;
  canvasTMForChildren.PreScale(cssPxPerDevPx, cssPxPerDevPx);

  aContext.Multiply(canvasTMForChildren);

  using PaintFrameFlags = nsLayoutUtils::PaintFrameFlags;
  PaintFrameFlags flags = PaintFrameFlags::InTransform;
  if (SVGAutoRenderState::IsPaintingToWindow(aContext.GetDrawTarget())) {
    flags |= PaintFrameFlags::ToWindow;
  }
  if (aImgParams.imageFlags & imgIContainer::FLAG_SYNC_DECODE) {
    flags |= PaintFrameFlags::SyncDecodeImages;
  }
  if (aImgParams.imageFlags & imgIContainer::FLAG_HIGH_QUALITY_SCALING) {
    flags |= PaintFrameFlags::UseHighQualityScaling;
  }
  nsLayoutUtils::PaintFrame(&aContext, kid, nsRegion(kidDirtyRect),
                            NS_RGBA(0, 0, 0, 0),
                            nsDisplayListBuilderMode::Painting, flags);

  aContext.Restore();
}

nsIFrame* SVGForeignObjectFrame::GetFrameForPoint(const gfxPoint& aPoint) {
  NS_ASSERTION(!NS_SVGDisplayListHitTestingEnabled() ||
                   (mState & NS_FRAME_IS_NONDISPLAY),
               "If display lists are enabled, only hit-testing of a "
               "clipPath's contents should take this code path");

  if (IsDisabled() || HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
    return nullptr;
  }

  nsIFrame* kid = PrincipalChildList().FirstChild();
  if (!kid) {
    return nullptr;
  }

  float x, y, width, height;
  SVGGeometryProperty::ResolveAll<SVGT::X, SVGT::Y, SVGT::Width, SVGT::Height>(
      static_cast<SVGElement*>(GetContent()), &x, &y, &width, &height);

  if (!gfxRect(x, y, width, height).Contains(aPoint) ||
      !SVGUtils::HitTestClip(this, aPoint)) {
    return nullptr;
  }

  // Convert the point to app units relative to the top-left corner of the
  // viewport that's established by the foreignObject element:

  gfxPoint pt = (aPoint + gfxPoint(x, y)) * AppUnitsPerCSSPixel();
  nsPoint point = nsPoint(NSToIntRound(pt.x), NSToIntRound(pt.y));

  return nsLayoutUtils::GetFrameForPoint(RelativeTo{kid}, point);
}

void SVGForeignObjectFrame::ReflowSVG() {
  NS_ASSERTION(SVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  MOZ_ASSERT(!HasAnyStateBits(NS_FRAME_IS_NONDISPLAY),
             "ReflowSVG mechanism not designed for this");

  if (!SVGUtils::NeedsReflowSVG(this)) {
    return;
  }

  // We update mRect before the DoReflow call so that DoReflow uses the
  // correct dimensions:

  float x, y, w, h;
  SVGGeometryProperty::ResolveAll<SVGT::X, SVGT::Y, SVGT::Width, SVGT::Height>(
      static_cast<SVGElement*>(GetContent()), &x, &y, &w, &h);

  // If mRect's width or height are negative, reflow blows up! We must clamp!
  if (w < 0.0f) w = 0.0f;
  if (h < 0.0f) h = 0.0f;

  mRect = nsLayoutUtils::RoundGfxRectToAppRect(gfxRect(x, y, w, h),
                                               AppUnitsPerCSSPixel());

  // Fully mark our kid dirty so that it gets resized if necessary
  // (NS_FRAME_HAS_DIRTY_CHILDREN isn't enough in that case):
  nsIFrame* kid = PrincipalChildList().FirstChild();
  kid->MarkSubtreeDirty();

  // Make sure to not allow interrupts if we're not being reflown as a root:
  nsPresContext::InterruptPreventer noInterrupts(PresContext());

  DoReflow();

  if (mState & NS_FRAME_FIRST_REFLOW) {
    // Make sure we have our filter property (if any) before calling
    // FinishAndStoreOverflow (subsequent filter changes are handled off
    // nsChangeHint_UpdateEffects):
    SVGObserverUtils::UpdateEffects(this);
  }

  // If we have a filter, we need to invalidate ourselves because filter
  // output can change even if none of our descendants need repainting.
  if (StyleEffects()->HasFilters()) {
    InvalidateFrame();
  }

  auto* anonKid = PrincipalChildList().FirstChild();
  nsRect overflow = anonKid->InkOverflowRect();

  OverflowAreas overflowAreas(overflow, overflow);
  FinishAndStoreOverflow(overflowAreas, mRect.Size());

  // Now unset the various reflow bits:
  RemoveStateBits(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
                  NS_FRAME_HAS_DIRTY_CHILDREN);
}

void SVGForeignObjectFrame::NotifySVGChanged(uint32_t aFlags) {
  MOZ_ASSERT(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
             "Invalidation logic may need adjusting");

  bool needNewBounds = false;  // i.e. mRect or ink overflow rect
  bool needReflow = false;
  bool needNewCanvasTM = false;

  if (aFlags & COORD_CONTEXT_CHANGED) {
    // Coordinate context changes affect mCanvasTM if we have a
    // percentage 'x' or 'y'
    if (StyleSVGReset()->mX.HasPercent() || StyleSVGReset()->mY.HasPercent()) {
      needNewBounds = true;
      needNewCanvasTM = true;
    }

    // Our coordinate context's width/height has changed. If we have a
    // percentage width/height our dimensions will change so we must reflow.
    if (StylePosition()->mWidth.HasPercent() ||
        StylePosition()->mHeight.HasPercent()) {
      needNewBounds = true;
      needReflow = true;
    }
  }

  if (aFlags & TRANSFORM_CHANGED) {
    if (mCanvasTM && mCanvasTM->IsSingular()) {
      needNewBounds = true;  // old bounds are bogus
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
    SVGUtils::ScheduleReflowSVG(this);
  }

  // If we're called while the PresShell is handling reflow events then we
  // must have been called as a result of the NotifyViewportChange() call in
  // our SVGOuterSVGFrame's Reflow() method. We must not call RequestReflow
  // at this point (i.e. during reflow) because it could confuse the
  // PresShell and prevent it from reflowing us properly in future. Besides
  // that, SVGOuterSVGFrame::DidReflow will take care of reflowing us
  // synchronously, so there's no need.
  if (needReflow && !PresShell()->IsReflowLocked()) {
    RequestReflow(IntrinsicDirty::Resize);
  }

  if (needNewCanvasTM) {
    // Do this after calling InvalidateAndScheduleBoundsUpdate in case we
    // change the code and it needs to use it.
    mCanvasTM = nullptr;
  }
}

SVGBBox SVGForeignObjectFrame::GetBBoxContribution(
    const Matrix& aToBBoxUserspace, uint32_t aFlags) {
  SVGForeignObjectElement* content =
      static_cast<SVGForeignObjectElement*>(GetContent());

  float x, y, w, h;
  SVGGeometryProperty::ResolveAll<SVGT::X, SVGT::Y, SVGT::Width, SVGT::Height>(
      content, &x, &y, &w, &h);

  if (w < 0.0f) w = 0.0f;
  if (h < 0.0f) h = 0.0f;

  if (aToBBoxUserspace.IsSingular()) {
    // XXX ReportToConsole
    return SVGBBox();
  }
  return aToBBoxUserspace.TransformBounds(gfx::Rect(0.0, 0.0, w, h));
}

//----------------------------------------------------------------------

gfxMatrix SVGForeignObjectFrame::GetCanvasTM() {
  if (!mCanvasTM) {
    NS_ASSERTION(GetParent(), "null parent");

    auto* parent = static_cast<SVGContainerFrame*>(GetParent());
    auto* content = static_cast<SVGForeignObjectElement*>(GetContent());

    gfxMatrix tm = content->PrependLocalTransformsTo(parent->GetCanvasTM());

    mCanvasTM = MakeUnique<gfxMatrix>(tm);
  }
  return *mCanvasTM;
}

//----------------------------------------------------------------------
// Implementation helpers

void SVGForeignObjectFrame::RequestReflow(IntrinsicDirty aType) {
  if (HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    // If we haven't had a ReflowSVG() yet, nothing to do.
    return;
  }

  nsIFrame* kid = PrincipalChildList().FirstChild();
  if (!kid) {
    return;
  }

  PresShell()->FrameNeedsReflow(kid, aType, NS_FRAME_IS_DIRTY);
}

void SVGForeignObjectFrame::DoReflow() {
  MarkInReflow();
  // Skip reflow if we're zero-sized, unless this is our first reflow.
  if (IsDisabled() && !HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    return;
  }

  nsPresContext* presContext = PresContext();
  nsIFrame* kid = PrincipalChildList().FirstChild();
  if (!kid) {
    return;
  }

  // initiate a synchronous reflow here and now:
  RefPtr<gfxContext> renderingContext =
      presContext->PresShell()->CreateReferenceRenderingContext();

  mInReflow = true;

  WritingMode wm = kid->GetWritingMode();
  ReflowInput reflowInput(presContext, kid, renderingContext,
                          LogicalSize(wm, ISize(wm), NS_UNCONSTRAINEDSIZE));
  ReflowOutput desiredSize(reflowInput);
  nsReflowStatus status;

  // We don't use mRect.height above because that tells the child to do
  // page/column breaking at that height.
  NS_ASSERTION(
      reflowInput.ComputedPhysicalBorderPadding() == nsMargin(0, 0, 0, 0) &&
          reflowInput.ComputedPhysicalMargin() == nsMargin(0, 0, 0, 0),
      "style system should ensure that :-moz-svg-foreign-content "
      "does not get styled");
  NS_ASSERTION(reflowInput.ComputedISize() == ISize(wm),
               "reflow input made child wrong size");
  reflowInput.SetComputedBSize(BSize(wm));

  ReflowChild(kid, presContext, desiredSize, reflowInput, 0, 0,
              ReflowChildFlags::NoMoveFrame, status);
  NS_ASSERTION(mRect.width == desiredSize.Width() &&
                   mRect.height == desiredSize.Height(),
               "unexpected size");
  FinishReflowChild(kid, presContext, desiredSize, &reflowInput, 0, 0,
                    ReflowChildFlags::NoMoveFrame);

  mInReflow = false;
}

nsRect SVGForeignObjectFrame::GetInvalidRegion() {
  MOZ_ASSERT(!NS_SVGDisplayListPaintingEnabled(),
             "Only called by nsDisplayOuterSVG code");

  nsIFrame* kid = PrincipalChildList().FirstChild();
  if (kid->HasInvalidFrameInSubtree()) {
    gfxRect r(mRect.x, mRect.y, mRect.width, mRect.height);
    r.Scale(1.0 / AppUnitsPerCSSPixel());
    nsRect rect = SVGUtils::ToCanvasBounds(r, GetCanvasTM(), PresContext());
    rect = SVGUtils::GetPostFilterInkOverflowRect(this, rect);
    return rect;
  }
  return nsRect();
}

void SVGForeignObjectFrame::AppendDirectlyOwnedAnonBoxes(
    nsTArray<OwnedAnonBox>& aResult) {
  MOZ_ASSERT(PrincipalChildList().FirstChild(), "Must have our anon box");
  aResult.AppendElement(OwnedAnonBox(PrincipalChildList().FirstChild()));
}

}  // namespace mozilla
