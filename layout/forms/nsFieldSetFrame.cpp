/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFieldSetFrame.h"

#include "mozilla/gfx/2D.h"
#include "nsCSSAnonBoxes.h"
#include "nsLayoutUtils.h"
#include "nsLegendFrame.h"
#include "nsCSSRendering.h"
#include <algorithm>
#include "nsIFrame.h"
#include "nsPresContext.h"
#include "RestyleManager.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsDisplayList.h"
#include "nsRenderingContext.h"
#include "nsIScrollableFrame.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;
using namespace mozilla::layout;

nsContainerFrame*
NS_NewFieldSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFieldSetFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsFieldSetFrame)

nsFieldSetFrame::nsFieldSetFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
  , mLegendRect(GetWritingMode())
{
  mLegendSpace  = 0;
}

nsIAtom*
nsFieldSetFrame::GetType() const
{
  return nsGkAtoms::fieldSetFrame;
}

nsRect
nsFieldSetFrame::VisualBorderRectRelativeToSelf() const
{
  WritingMode wm = GetWritingMode();
  css::Side legendSide = wm.PhysicalSide(eLogicalSideBStart);
  nscoord legendBorder = StyleBorder()->GetComputedBorderWidth(legendSide);
  LogicalRect r(wm, LogicalPoint(wm, 0, 0), GetLogicalSize(wm));
  nsSize containerSize = r.Size(wm).GetPhysicalSize(wm);
  if (legendBorder < mLegendRect.BSize(wm)) {
    nscoord off = (mLegendRect.BSize(wm) - legendBorder) / 2;
    r.BStart(wm) += off;
    r.BSize(wm) -= off;
  }
  return r.GetPhysicalRect(wm, containerSize);
}

nsIFrame*
nsFieldSetFrame::GetInner() const
{
  nsIFrame* last = mFrames.LastChild();
  if (last &&
      last->StyleContext()->GetPseudo() == nsCSSAnonBoxes::fieldsetContent) {
    return last;
  }
  MOZ_ASSERT(mFrames.LastChild() == mFrames.FirstChild());
  return nullptr;
}

nsIFrame*
nsFieldSetFrame::GetLegend() const
{
  if (mFrames.FirstChild() == GetInner()) {
    MOZ_ASSERT(mFrames.LastChild() == mFrames.FirstChild());
    return nullptr;
  }
  MOZ_ASSERT(mFrames.FirstChild() &&
             mFrames.FirstChild()->GetContentInsertionFrame()->GetType() ==
               nsGkAtoms::legendFrame);
  return mFrames.FirstChild();
}

class nsDisplayFieldSetBorderBackground : public nsDisplayItem {
public:
  nsDisplayFieldSetBorderBackground(nsDisplayListBuilder* aBuilder,
                                    nsFieldSetFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayFieldSetBorderBackground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayFieldSetBorderBackground() {
    MOZ_COUNT_DTOR(nsDisplayFieldSetBorderBackground);
  }
#endif

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override;
  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion *aInvalidRegion) override;
  NS_DISPLAY_DECL_NAME("FieldSetBorderBackground", TYPE_FIELDSET_BORDER_BACKGROUND)
};

void nsDisplayFieldSetBorderBackground::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                                                HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  // aPt is guaranteed to be in this item's bounds. We do the hit test based on the
  // frame bounds even though our background doesn't cover the whole frame.
  // It's not clear whether this is correct.
  aOutFrames->AppendElement(mFrame);
}

void
nsDisplayFieldSetBorderBackground::Paint(nsDisplayListBuilder* aBuilder,
                                         nsRenderingContext* aCtx)
{
  DrawResult result = static_cast<nsFieldSetFrame*>(mFrame)->
    PaintBorderBackground(aBuilder, *aCtx, ToReferenceFrame(), mVisibleRect);

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
}

nsDisplayItemGeometry*
nsDisplayFieldSetBorderBackground::AllocateGeometry(nsDisplayListBuilder* aBuilder)
{
  return new nsDisplayItemGenericImageGeometry(this, aBuilder);
}

void
nsDisplayFieldSetBorderBackground::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                             const nsDisplayItemGeometry* aGeometry,
                                                             nsRegion *aInvalidRegion)
{
  auto geometry =
    static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
}

void
nsFieldSetFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists) {
  // Paint our background and border in a special way.
  // REVIEW: We don't really need to check frame emptiness here; if it's empty,
  // the background/border display item won't do anything, and if it isn't empty,
  // we need to paint the outline
  if (!(GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) &&
      IsVisibleForPainting(aBuilder)) {
    if (StyleBorder()->mBoxShadow) {
      aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayBoxShadowOuter(aBuilder, this));
    }

    // don't bother checking to see if we really have a border or background.
    // we usually will have a border.
    aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
      nsDisplayFieldSetBorderBackground(aBuilder, this));
  
    DisplayOutlineUnconditional(aBuilder, aLists);

    DO_GLOBAL_REFLOW_COUNT_DSP("nsFieldSetFrame");
  }

  if (GetPrevInFlow()) {
    DisplayOverflowContainers(aBuilder, aDirtyRect, aLists);
  }

  nsDisplayListCollection contentDisplayItems;
  if (nsIFrame* inner = GetInner()) {
    // Collect the inner frame's display items into their own collection.
    // We need to be calling BuildDisplayList on it before the legend in
    // case it contains out-of-flow frames whose placeholders are in the
    // legend. However, we want the inner frame's display items to be
    // after the legend's display items in z-order, so we need to save them
    // and append them later.
    BuildDisplayListForChild(aBuilder, inner, aDirtyRect, contentDisplayItems);
  }
  if (nsIFrame* legend = GetLegend()) {
    // The legend's background goes on our BlockBorderBackgrounds list because
    // it's a block child.
    nsDisplayListSet set(aLists, aLists.BlockBorderBackgrounds());
    BuildDisplayListForChild(aBuilder, legend, aDirtyRect, set);
  }
  // Put the inner frame's display items on the master list. Note that this
  // moves its border/background display items to our BorderBackground() list,
  // which isn't really correct, but it's OK because the inner frame is
  // anonymous and can't have its own border and background.
  contentDisplayItems.MoveTo(aLists);
}

DrawResult
nsFieldSetFrame::PaintBorderBackground(
  nsDisplayListBuilder* aBuilder,
  nsRenderingContext& aRenderingContext,
  nsPoint aPt,
  const nsRect& aDirtyRect)
{
  // if the border is smaller than the legend. Move the border down
  // to be centered on the legend.
  // FIXME: This means border-radius clamping is incorrect; we should
  // override nsIFrame::GetBorderRadii.
  WritingMode wm = GetWritingMode();
  nsRect rect = VisualBorderRectRelativeToSelf();
  nscoord off = wm.IsVertical() ? rect.x : rect.y;
  rect += aPt;
  nsPresContext* presContext = PresContext();

  uint32_t bgFlags = aBuilder->GetBackgroundPaintFlags();
  PaintBorderFlags borderFlags = aBuilder->ShouldSyncDecodeImages()
                               ? PaintBorderFlags::SYNC_DECODE_IMAGES
                               : PaintBorderFlags();

  DrawResult result =
    nsCSSRendering::PaintBackground(presContext, aRenderingContext, this,
                                    aDirtyRect, rect, bgFlags);

  nsCSSRendering::PaintBoxShadowInner(presContext, aRenderingContext,
                                      this, rect);

  if (nsIFrame* legend = GetLegend()) {
    css::Side legendSide = wm.PhysicalSide(eLogicalSideBStart);
    nscoord legendBorderWidth =
      StyleBorder()->GetComputedBorderWidth(legendSide);

    // Use the rect of the legend frame, not mLegendRect, so we draw our
    // border under the legend's inline-start and -end margins.
    LogicalRect legendRect(wm, legend->GetRect() + aPt, rect.Size());

    // Compute clipRect using logical coordinates, so that the legend space
    // will be clipped out of the appropriate physical side depending on mode.
    LogicalRect clipRect = LogicalRect(wm, rect, rect.Size());
    DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();
    gfxContext* gfx = aRenderingContext.ThebesContext();
    int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();

    // draw inline-start portion of the block-start side of the border
    clipRect.ISize(wm) = legendRect.IStart(wm) - clipRect.IStart(wm);
    clipRect.BSize(wm) = legendBorderWidth;

    gfx->Save();
    gfx->Clip(NSRectToSnappedRect(clipRect.GetPhysicalRect(wm, rect.Size()),
                                  appUnitsPerDevPixel, *drawTarget));
    result &=
      nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                  aDirtyRect, rect, mStyleContext, borderFlags);
    gfx->Restore();

    // draw inline-end portion of the block-start side of the border
    clipRect = LogicalRect(wm, rect, rect.Size());
    clipRect.ISize(wm) = clipRect.IEnd(wm) - legendRect.IEnd(wm);
    clipRect.IStart(wm) = legendRect.IEnd(wm);
    clipRect.BSize(wm) = legendBorderWidth;

    gfx->Save();
    gfx->Clip(NSRectToSnappedRect(clipRect.GetPhysicalRect(wm, rect.Size()),
                                  appUnitsPerDevPixel, *drawTarget));
    result &=
      nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                  aDirtyRect, rect, mStyleContext, borderFlags);
    gfx->Restore();

    // draw remainder of the border (omitting the block-start side)
    clipRect = LogicalRect(wm, rect, rect.Size());
    clipRect.BStart(wm) += legendBorderWidth;
    clipRect.BSize(wm) = BSize(wm) - (off + legendBorderWidth);

    gfx->Save();
    gfx->Clip(NSRectToSnappedRect(clipRect.GetPhysicalRect(wm, rect.Size()),
                                  appUnitsPerDevPixel, *drawTarget));
    result &=
      nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                  aDirtyRect, rect, mStyleContext, borderFlags);
    gfx->Restore();
  } else {
    result &=
      nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                  aDirtyRect, nsRect(aPt, mRect.Size()),
                                  mStyleContext, borderFlags);
  }

  return result;
}

nscoord
nsFieldSetFrame::GetIntrinsicISize(nsRenderingContext* aRenderingContext,
                                   nsLayoutUtils::IntrinsicISizeType aType)
{
  nscoord legendWidth = 0;
  nscoord contentWidth = 0;
  if (nsIFrame* legend = GetLegend()) {
    legendWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, legend, aType);
  }

  if (nsIFrame* inner = GetInner()) {
    // Ignore padding on the inner, since the padding will be applied to the
    // outer instead, and the padding computed for the inner is wrong
    // for percentage padding.
    contentWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, inner, aType,
                                           nsLayoutUtils::IGNORE_PADDING);
  }

  return std::max(legendWidth, contentWidth);
}


nscoord
nsFieldSetFrame::GetMinISize(nsRenderingContext* aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);

  result = GetIntrinsicISize(aRenderingContext, nsLayoutUtils::MIN_ISIZE);
  return result;
}

nscoord
nsFieldSetFrame::GetPrefISize(nsRenderingContext* aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);

  result = GetIntrinsicISize(aRenderingContext, nsLayoutUtils::PREF_ISIZE);
  return result;
}

/* virtual */
LogicalSize
nsFieldSetFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                             WritingMode aWM,
                             const LogicalSize& aCBSize,
                             nscoord aAvailableISize,
                             const LogicalSize& aMargin,
                             const LogicalSize& aBorder,
                             const LogicalSize& aPadding,
                             ComputeSizeFlags aFlags)
{
  LogicalSize result =
    nsContainerFrame::ComputeSize(aRenderingContext, aWM,
                                  aCBSize, aAvailableISize,
                                  aMargin, aBorder, aPadding, aFlags);

  // XXX The code below doesn't make sense if the caller's writing mode
  // is orthogonal to this frame's. Not sure yet what should happen then;
  // for now, just bail out.
  if (aWM.IsVertical() != GetWritingMode().IsVertical()) {
    return result;
  }

  // Fieldsets never shrink below their min width.

  // If we're a container for font size inflation, then shrink
  // wrapping inside of us should not apply font size inflation.
  AutoMaybeDisableFontInflation an(this);

  nscoord minISize = GetMinISize(aRenderingContext);
  if (minISize > result.ISize(aWM)) {
    result.ISize(aWM) = minISize;
  }

  return result;
}

void
nsFieldSetFrame::Reflow(nsPresContext*           aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsFieldSetFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_PRECONDITION(aReflowState.ComputedISize() != NS_INTRINSICSIZE,
                  "Should have a precomputed inline-size!");

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  nsOverflowAreas ocBounds;
  nsReflowStatus ocStatus = NS_FRAME_COMPLETE;
  if (GetPrevInFlow()) {
    ReflowOverflowContainerChildren(aPresContext, aReflowState, ocBounds, 0,
                                    ocStatus);
  }

  //------------ Handle Incremental Reflow -----------------
  bool reflowInner;
  bool reflowLegend;
  nsIFrame* legend = GetLegend();
  nsIFrame* inner = GetInner();
  if (aReflowState.ShouldReflowAllKids()) {
    reflowInner = inner != nullptr;
    reflowLegend = legend != nullptr;
  } else {
    reflowInner = inner && NS_SUBTREE_DIRTY(inner);
    reflowLegend = legend && NS_SUBTREE_DIRTY(legend);
  }

  // We don't allow fieldsets to break vertically. If we did, we'd
  // need logic here to push and pull overflow frames.
  // Since we're not applying our padding in this frame, we need to add it here
  // to compute the available width for our children.
  WritingMode wm = GetWritingMode();
  WritingMode innerWM = inner ? inner->GetWritingMode() : wm;
  WritingMode legendWM = legend ? legend->GetWritingMode() : wm;
  LogicalSize innerAvailSize = aReflowState.ComputedSizeWithPadding(innerWM);
  LogicalSize legendAvailSize = aReflowState.ComputedSizeWithPadding(legendWM);
  innerAvailSize.BSize(innerWM) = legendAvailSize.BSize(legendWM) =
    NS_UNCONSTRAINEDSIZE;
  NS_ASSERTION(!inner ||
      nsLayoutUtils::IntrinsicForContainer(aReflowState.rendContext,
                                           inner,
                                           nsLayoutUtils::MIN_ISIZE) <=
               innerAvailSize.ISize(innerWM),
               "Bogus availSize.ISize; should be bigger");
  NS_ASSERTION(!legend ||
      nsLayoutUtils::IntrinsicForContainer(aReflowState.rendContext,
                                           legend,
                                           nsLayoutUtils::MIN_ISIZE) <=
               legendAvailSize.ISize(legendWM),
               "Bogus availSize.ISize; should be bigger");

  // get our border and padding
  LogicalMargin border = aReflowState.ComputedLogicalBorderPadding() -
                         aReflowState.ComputedLogicalPadding();

  // Figure out how big the legend is if there is one.
  // get the legend's margin
  LogicalMargin legendMargin(wm);
  // reflow the legend only if needed
  Maybe<nsHTMLReflowState> legendReflowState;
  if (legend) {
    legendReflowState.emplace(aPresContext, aReflowState, legend,
                                legendAvailSize);
  }
  if (reflowLegend) {
    nsHTMLReflowMetrics legendDesiredSize(aReflowState);

    // We'll move the legend to its proper place later, so the position
    // and containerSize passed here are unimportant.
    const nsSize dummyContainerSize;
    ReflowChild(legend, aPresContext, legendDesiredSize, *legendReflowState,
                wm, LogicalPoint(wm), dummyContainerSize,
                NS_FRAME_NO_MOVE_FRAME, aStatus);
#ifdef NOISY_REFLOW
    printf("  returned (%d, %d)\n",
           legendDesiredSize.Width(), legendDesiredSize.Height());
#endif
    // figure out the legend's rectangle
    legendMargin = legend->GetLogicalUsedMargin(wm);
    mLegendRect =
      LogicalRect(wm, 0, 0,
                  legendDesiredSize.ISize(wm) + legendMargin.IStartEnd(wm),
                  legendDesiredSize.BSize(wm) + legendMargin.BStartEnd(wm));
    nscoord oldSpace = mLegendSpace;
    mLegendSpace = 0;
    if (mLegendRect.BSize(wm) > border.BStart(wm)) {
      // center the border on the legend
      mLegendSpace = mLegendRect.BSize(wm) - border.BStart(wm);
    } else {
      mLegendRect.BStart(wm) =
        (border.BStart(wm) - mLegendRect.BSize(wm)) / 2;
    }

    // if the legend space changes then we need to reflow the
    // content area as well.
    if (mLegendSpace != oldSpace && inner) {
      reflowInner = true;
    }

    FinishReflowChild(legend, aPresContext, legendDesiredSize,
                      legendReflowState.ptr(), wm, LogicalPoint(wm),
                      dummyContainerSize, NS_FRAME_NO_MOVE_FRAME);
  } else if (!legend) {
    mLegendRect.SetEmpty();
    mLegendSpace = 0;
  } else {
    // mLegendSpace and mLegendRect haven't changed, but we need
    // the used margin when placing the legend.
    legendMargin = legend->GetLogicalUsedMargin(wm);
  }

  // This containerSize is incomplete as yet: it does not include the size
  // of the |inner| frame itself.
  nsSize containerSize = (LogicalSize(wm, 0, mLegendSpace) +
                          border.Size(wm)).GetPhysicalSize(wm);
  // reflow the content frame only if needed
  if (reflowInner) {
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, inner,
                                     innerAvailSize, nullptr,
                                     nsHTMLReflowState::CALLER_WILL_INIT);
    // Override computed padding, in case it's percentage padding
    kidReflowState.Init(aPresContext, nullptr, nullptr,
                        &aReflowState.ComputedPhysicalPadding());
    // Our child is "height:100%" but we actually want its height to be reduced
    // by the amount of content-height the legend is eating up, unless our
    // height is unconstrained (in which case the child's will be too).
    if (aReflowState.ComputedBSize() != NS_UNCONSTRAINEDSIZE) {
      kidReflowState.SetComputedBSize(
         std::max(0, aReflowState.ComputedBSize() - mLegendSpace));
    }

    if (aReflowState.ComputedMinBSize() > 0) {
      kidReflowState.ComputedMinBSize() =
        std::max(0, aReflowState.ComputedMinBSize() - mLegendSpace);
    }

    if (aReflowState.ComputedMaxBSize() != NS_UNCONSTRAINEDSIZE) {
      kidReflowState.ComputedMaxBSize() =
        std::max(0, aReflowState.ComputedMaxBSize() - mLegendSpace);
    }

    nsHTMLReflowMetrics kidDesiredSize(kidReflowState,
                                       aDesiredSize.mFlags);
    // Reflow the frame
    NS_ASSERTION(kidReflowState.ComputedPhysicalMargin() == nsMargin(0,0,0,0),
                 "Margins on anonymous fieldset child not supported!");
    LogicalPoint pt(wm, border.IStart(wm), border.BStart(wm) + mLegendSpace);

    // We don't know the correct containerSize until we have reflowed |inner|,
    // so we use a dummy value for now; FinishReflowChild will fix the position
    // if necessary.
    const nsSize dummyContainerSize;
    ReflowChild(inner, aPresContext, kidDesiredSize, kidReflowState,
                wm, pt, dummyContainerSize, 0, aStatus);

    // Update containerSize to account for size of the inner frame, so that
    // FinishReflowChild can position it correctly.
    containerSize += kidDesiredSize.PhysicalSize();
    FinishReflowChild(inner, aPresContext, kidDesiredSize,
                      &kidReflowState, wm, pt, containerSize, 0);
    NS_FRAME_TRACE_REFLOW_OUT("FieldSet::Reflow", aStatus);
  } else if (inner) {
    // |inner| didn't need to be reflowed but we do need to include its size
    // in containerSize.
    containerSize += inner->GetSize();
  }

  LogicalRect contentRect(wm);
  if (inner) {
    // We don't support margins on inner, so our content rect is just the
    // inner's border-box. (We don't really care about container size at this
    // point, as we'll figure out the actual positioning later.)
    contentRect = inner->GetLogicalRect(wm, containerSize);
  }

  // Our content rect must fill up the available width
  LogicalSize availSize = aReflowState.ComputedSizeWithPadding(wm);
  if (availSize.ISize(wm) > contentRect.ISize(wm)) {
    contentRect.ISize(wm) = innerAvailSize.ISize(wm);
  }

  if (legend) {
    // The legend is positioned inline-wards within the inner's content rect
    // (so that padding on the fieldset affects the legend position).
    LogicalRect innerContentRect = contentRect;
    innerContentRect.Deflate(wm, aReflowState.ComputedLogicalPadding());
    // If the inner content rect is larger than the legend, we can align the
    // legend.
    if (innerContentRect.ISize(wm) > mLegendRect.ISize(wm)) {
      int32_t align = static_cast<nsLegendFrame*>
        (legend->GetContentInsertionFrame())->GetAlign();
      if (!wm.IsBidiLTR()) {
        if (align == NS_STYLE_TEXT_ALIGN_LEFT ||
            align == NS_STYLE_TEXT_ALIGN_MOZ_LEFT) {
          align = NS_STYLE_TEXT_ALIGN_END;
        } else if (align == NS_STYLE_TEXT_ALIGN_RIGHT ||
                   align == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT) {
          align = NS_STYLE_TEXT_ALIGN_DEFAULT;
        }
      }
      switch (align) {
        case NS_STYLE_TEXT_ALIGN_END:
          mLegendRect.IStart(wm) =
            innerContentRect.IEnd(wm) - mLegendRect.ISize(wm);
          break;
        case NS_STYLE_TEXT_ALIGN_CENTER:
        case NS_STYLE_TEXT_ALIGN_MOZ_CENTER:
          // Note: rounding removed; there doesn't seem to be any need
          mLegendRect.IStart(wm) = innerContentRect.IStart(wm) +
            (innerContentRect.ISize(wm) - mLegendRect.ISize(wm)) / 2;
          break;
        default:
          mLegendRect.IStart(wm) = innerContentRect.IStart(wm);
          break;
      }
    } else {
      // otherwise make place for the legend
      mLegendRect.IStart(wm) = innerContentRect.IStart(wm);
      innerContentRect.ISize(wm) = mLegendRect.ISize(wm);
      contentRect.ISize(wm) = mLegendRect.ISize(wm) +
        aReflowState.ComputedLogicalPadding().IStartEnd(wm);
    }

    // place the legend
    LogicalRect actualLegendRect = mLegendRect;
    actualLegendRect.Deflate(wm, legendMargin);
    LogicalPoint actualLegendPos(actualLegendRect.Origin(wm));

    // Note that legend's writing mode may be different from the fieldset's,
    // so we need to convert offsets before applying them to it (bug 1134534).
    LogicalMargin offsets =
      legendReflowState->ComputedLogicalOffsets().
        ConvertTo(wm, legendReflowState->GetWritingMode());
    nsHTMLReflowState::ApplyRelativePositioning(legend, wm, offsets,
                                                &actualLegendPos,
                                                containerSize);

    legend->SetPosition(wm, actualLegendPos, containerSize);
    nsContainerFrame::PositionFrameView(legend);
    nsContainerFrame::PositionChildViews(legend);
  }

  // Return our size and our result.
  LogicalSize finalSize(wm, contentRect.ISize(wm) + border.IStartEnd(wm),
                        mLegendSpace + border.BStartEnd(wm) +
                        (inner ? inner->BSize(wm) : 0));
  aDesiredSize.SetSize(wm, finalSize);
  aDesiredSize.SetOverflowAreasToDesiredBounds();

  if (legend) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, legend);
  }
  if (inner) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, inner);
  }

  // Merge overflow container bounds and status.
  aDesiredSize.mOverflowAreas.UnionWith(ocBounds);
  NS_MergeReflowStatusInto(&aStatus, ocStatus);

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowState, aStatus);

  InvalidateFrame();

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
}

#ifdef DEBUG
void
nsFieldSetFrame::SetInitialChildList(ChildListID    aListID,
                                     nsFrameList&   aChildList)
{
  nsContainerFrame::SetInitialChildList(kPrincipalList, aChildList);
  MOZ_ASSERT(GetInner());
}
void
nsFieldSetFrame::AppendFrames(ChildListID    aListID,
                              nsFrameList&   aFrameList)
{
  MOZ_CRASH("nsFieldSetFrame::AppendFrames not supported");
}

void
nsFieldSetFrame::InsertFrames(ChildListID    aListID,
                              nsIFrame*      aPrevFrame,
                              nsFrameList&   aFrameList)
{
  MOZ_CRASH("nsFieldSetFrame::InsertFrames not supported");
}

void
nsFieldSetFrame::RemoveFrame(ChildListID    aListID,
                             nsIFrame*      aOldFrame)
{
  MOZ_CRASH("nsFieldSetFrame::RemoveFrame not supported");
}
#endif

#ifdef ACCESSIBILITY
a11y::AccType
nsFieldSetFrame::AccessibleType()
{
  return a11y::eHTMLGroupboxType;
}
#endif

nscoord
nsFieldSetFrame::GetLogicalBaseline(WritingMode aWritingMode) const
{
  nsIFrame* inner = GetInner();
  return inner->BStart(aWritingMode, GetParent()->GetSize()) +
    inner->GetLogicalBaseline(aWritingMode);
}
