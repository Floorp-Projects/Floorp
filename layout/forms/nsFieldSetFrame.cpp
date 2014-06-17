/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFieldSetFrame.h"

#include "nsCSSAnonBoxes.h"
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
using namespace mozilla::layout;

nsContainerFrame*
NS_NewFieldSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFieldSetFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsFieldSetFrame)

nsFieldSetFrame::nsFieldSetFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
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
  nscoord topBorder = StyleBorder()->GetComputedBorderWidth(NS_SIDE_TOP);
  nsRect r(nsPoint(0,0), GetSize());
  if (topBorder < mLegendRect.height) {
    nscoord yoff = (mLegendRect.height - topBorder) / 2;
    r.y += yoff;
    r.height -= yoff;
  }
  return r;
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
                       nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) MOZ_OVERRIDE;
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion *aInvalidRegion) MOZ_OVERRIDE;
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
  static_cast<nsFieldSetFrame*>(mFrame)->
    PaintBorderBackground(*aCtx, ToReferenceFrame(),
                          mVisibleRect, aBuilder->GetBackgroundPaintFlags());
}

void
nsDisplayFieldSetBorderBackground::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                             const nsDisplayItemGeometry* aGeometry,
                                                             nsRegion *aInvalidRegion)
{
  AddInvalidRegionForSyncDecodeBackgroundImages(aBuilder, aGeometry, aInvalidRegion);

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

void
nsFieldSetFrame::PaintBorderBackground(nsRenderingContext& aRenderingContext,
    nsPoint aPt, const nsRect& aDirtyRect, uint32_t aBGFlags)
{
  // if the border is smaller than the legend. Move the border down
  // to be centered on the legend. 
  // FIXME: This means border-radius clamping is incorrect; we should
  // override nsIFrame::GetBorderRadii.
  nsRect rect = VisualBorderRectRelativeToSelf();
  nscoord yoff = rect.y;
  rect += aPt;
  nsPresContext* presContext = PresContext();

  nsCSSRendering::PaintBackground(presContext, aRenderingContext, this,
                                  aDirtyRect, rect, aBGFlags);

  nsCSSRendering::PaintBoxShadowInner(presContext, aRenderingContext,
                                      this, rect, aDirtyRect);

   if (nsIFrame* legend = GetLegend()) {
     nscoord topBorder = StyleBorder()->GetComputedBorderWidth(NS_SIDE_TOP);

    // Use the rect of the legend frame, not mLegendRect, so we draw our
    // border under the legend's left and right margins.
    nsRect legendRect = legend->GetRect() + aPt;
    
    // we should probably use PaintBorderEdges to do this but for now just use clipping
    // to achieve the same effect.

    // draw left side
    nsRect clipRect(rect);
    clipRect.width = legendRect.x - rect.x;
    clipRect.height = topBorder;

    aRenderingContext.PushState();
    aRenderingContext.IntersectClip(clipRect);
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, rect, mStyleContext);

    aRenderingContext.PopState();


    // draw right side
    clipRect = rect;
    clipRect.x = legendRect.XMost();
    clipRect.width = rect.XMost() - legendRect.XMost();
    clipRect.height = topBorder;

    aRenderingContext.PushState();
    aRenderingContext.IntersectClip(clipRect);
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, rect, mStyleContext);

    aRenderingContext.PopState();

    
    // draw bottom
    clipRect = rect;
    clipRect.y += topBorder;
    clipRect.height = mRect.height - (yoff + topBorder);
    
    aRenderingContext.PushState();
    aRenderingContext.IntersectClip(clipRect);
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, rect, mStyleContext);

    aRenderingContext.PopState();
  } else {

    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect,
                                nsRect(aPt, mRect.Size()),
                                mStyleContext);
  }
}

nscoord
nsFieldSetFrame::GetIntrinsicWidth(nsRenderingContext* aRenderingContext,
                                   nsLayoutUtils::IntrinsicWidthType aType)
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
nsFieldSetFrame::GetMinWidth(nsRenderingContext* aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);

  result = GetIntrinsicWidth(aRenderingContext, nsLayoutUtils::MIN_WIDTH);
  return result;
}

nscoord
nsFieldSetFrame::GetPrefWidth(nsRenderingContext* aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);

  result = GetIntrinsicWidth(aRenderingContext, nsLayoutUtils::PREF_WIDTH);
  return result;
}

/* virtual */ nsSize
nsFieldSetFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             uint32_t aFlags)
{
  nsSize result =
    nsContainerFrame::ComputeSize(aRenderingContext, aCBSize, aAvailableWidth,
                                  aMargin, aBorder, aPadding, aFlags);

  // Fieldsets never shrink below their min width.

  // If we're a container for font size inflation, then shrink
  // wrapping inside of us should not apply font size inflation.
  AutoMaybeDisableFontInflation an(this);

  nscoord minWidth = GetMinWidth(aRenderingContext);
  if (minWidth > result.width)
    result.width = minWidth;

  return result;
}

void
nsFieldSetFrame::Reflow(nsPresContext*           aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFieldSetFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_PRECONDITION(aReflowState.ComputedWidth() != NS_INTRINSICSIZE,
                  "Should have a precomputed width!");      
  
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
  nsSize availSize(aReflowState.ComputedWidth() + aReflowState.ComputedPhysicalPadding().LeftRight(),
                   NS_UNCONSTRAINEDSIZE);
  NS_ASSERTION(!inner ||
      nsLayoutUtils::IntrinsicForContainer(aReflowState.rendContext,
                                           inner,
                                           nsLayoutUtils::MIN_WIDTH) <=
               availSize.width,
               "Bogus availSize.width; should be bigger");
  NS_ASSERTION(!legend ||
      nsLayoutUtils::IntrinsicForContainer(aReflowState.rendContext,
                                           legend,
                                           nsLayoutUtils::MIN_WIDTH) <=
               availSize.width,
               "Bogus availSize.width; should be bigger");

  // get our border and padding
  nsMargin border = aReflowState.ComputedPhysicalBorderPadding() - aReflowState.ComputedPhysicalPadding();

  // Figure out how big the legend is if there is one. 
  // get the legend's margin
  nsMargin legendMargin(0,0,0,0);
  // reflow the legend only if needed
  Maybe<nsHTMLReflowState> legendReflowState;
  if (legend) {
    legendReflowState.construct(aPresContext, aReflowState, legend, availSize);
  }
  if (reflowLegend) {
    nsHTMLReflowMetrics legendDesiredSize(aReflowState);

    ReflowChild(legend, aPresContext, legendDesiredSize, legendReflowState.ref(),
                0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
#ifdef NOISY_REFLOW
    printf("  returned (%d, %d)\n", legendDesiredSize.Width(), legendDesiredSize.Height());
#endif
    // figure out the legend's rectangle
    legendMargin = legend->GetUsedMargin();
    mLegendRect.width  = legendDesiredSize.Width() + legendMargin.left + legendMargin.right;
    mLegendRect.height = legendDesiredSize.Height() + legendMargin.top + legendMargin.bottom;
    mLegendRect.x = 0;
    mLegendRect.y = 0;

    nscoord oldSpace = mLegendSpace;
    mLegendSpace = 0;
    if (mLegendRect.height > border.top) {
      // center the border on the legend
      mLegendSpace = mLegendRect.height - border.top;
    } else {
      mLegendRect.y = (border.top - mLegendRect.height)/2;
    }

    // if the legend space changes then we need to reflow the 
    // content area as well.
    if (mLegendSpace != oldSpace && inner) {
      reflowInner = true;
    }

    FinishReflowChild(legend, aPresContext, legendDesiredSize,
                      &legendReflowState.ref(), 0, 0, NS_FRAME_NO_MOVE_FRAME);    
  } else if (!legend) {
    mLegendRect.SetEmpty();
    mLegendSpace = 0;
  } else {
    // mLegendSpace and mLegendRect haven't changed, but we need
    // the used margin when placing the legend.
    legendMargin = legend->GetUsedMargin();
  }

  // reflow the content frame only if needed
  if (reflowInner) {
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, inner,
                                     availSize, -1, -1, nsHTMLReflowState::CALLER_WILL_INIT);
    // Override computed padding, in case it's percentage padding
    kidReflowState.Init(aPresContext, -1, -1, nullptr,
                        &aReflowState.ComputedPhysicalPadding());
    // Our child is "height:100%" but we actually want its height to be reduced
    // by the amount of content-height the legend is eating up, unless our
    // height is unconstrained (in which case the child's will be too).
    if (aReflowState.ComputedHeight() != NS_UNCONSTRAINEDSIZE) {
      kidReflowState.SetComputedHeight(
         std::max(0, aReflowState.ComputedHeight() - mLegendSpace));
    }

    if (aReflowState.ComputedMinHeight() > 0) {
      kidReflowState.ComputedMinHeight() =
        std::max(0, aReflowState.ComputedMinHeight() - mLegendSpace);
    }

    if (aReflowState.ComputedMaxHeight() != NS_UNCONSTRAINEDSIZE) {
      kidReflowState.ComputedMaxHeight() =
        std::max(0, aReflowState.ComputedMaxHeight() - mLegendSpace);
    }

    nsHTMLReflowMetrics kidDesiredSize(kidReflowState,
                                       aDesiredSize.mFlags);
    // Reflow the frame
    NS_ASSERTION(kidReflowState.ComputedPhysicalMargin() == nsMargin(0,0,0,0),
                 "Margins on anonymous fieldset child not supported!");
    nsPoint pt(border.left, border.top + mLegendSpace);
    ReflowChild(inner, aPresContext, kidDesiredSize, kidReflowState,
                pt.x, pt.y, 0, aStatus);

    FinishReflowChild(inner, aPresContext, kidDesiredSize,
                      &kidReflowState, pt.x, pt.y, 0);
    NS_FRAME_TRACE_REFLOW_OUT("FieldSet::Reflow", aStatus);
  }

  nsRect contentRect;
  if (inner) {
    // We don't support margins on inner, so our content rect is just the
    // inner's border-box.
    contentRect = inner->GetRect();
  }

  // Our content rect must fill up the available width
  if (availSize.width > contentRect.width) {
    contentRect.width = availSize.width;
  }

  if (legend) {
    // the legend is postioned horizontally within the inner's content rect
    // (so that padding on the fieldset affects the legend position).
    nsRect innerContentRect = contentRect;
    innerContentRect.Deflate(aReflowState.ComputedPhysicalPadding());
    // if the inner content rect is larger than the legend, we can align the legend
    if (innerContentRect.width > mLegendRect.width) {
      int32_t align = static_cast<nsLegendFrame*>
        (legend->GetContentInsertionFrame())->GetAlign();

      switch (align) {
        case NS_STYLE_TEXT_ALIGN_RIGHT:
          mLegendRect.x = innerContentRect.XMost() - mLegendRect.width;
          break;
        case NS_STYLE_TEXT_ALIGN_CENTER:
          // Note: rounding removed; there doesn't seem to be any need
          mLegendRect.x = innerContentRect.width / 2 - mLegendRect.width / 2 + innerContentRect.x;
          break;
        default:
          mLegendRect.x = innerContentRect.x;
          break;
      }
    } else {
      // otherwise make place for the legend
      mLegendRect.x = innerContentRect.x;
      innerContentRect.width = mLegendRect.width;
      contentRect.width = mLegendRect.width + aReflowState.ComputedPhysicalPadding().LeftRight();
    }

    // place the legend
    nsRect actualLegendRect(mLegendRect);
    actualLegendRect.Deflate(legendMargin);
    nsPoint actualLegendPos(actualLegendRect.TopLeft());
    legendReflowState.ref().ApplyRelativePositioning(&actualLegendPos);
    legend->SetPosition(actualLegendPos);
    nsContainerFrame::PositionFrameView(legend);
    nsContainerFrame::PositionChildViews(legend);
  }

  // Return our size and our result.
  aDesiredSize.Height() = mLegendSpace + border.TopBottom() +
                          (inner ? inner->GetRect().height : 0);
  aDesiredSize.Width() = contentRect.width + border.LeftRight();
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  if (legend)
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, legend);
  if (inner)
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, inner);

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
  return inner->BStart(aWritingMode, GetParent()->GetSize().width) +
    inner->GetLogicalBaseline(aWritingMode);
}
