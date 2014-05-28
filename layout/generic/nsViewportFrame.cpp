/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object that is the root of the frame tree, which contains
 * the document's scrollbars and contains fixed-positioned elements
 */

#include "nsViewportFrame.h"
#include "nsGkAtoms.h"
#include "nsIScrollableFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsAbsoluteContainingBlock.h"
#include "GeckoProfiler.h"

using namespace mozilla;

ViewportFrame*
NS_NewViewportFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) ViewportFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(ViewportFrame)
NS_QUERYFRAME_HEAD(ViewportFrame)
  NS_QUERYFRAME_ENTRY(ViewportFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void
ViewportFrame::Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow)
{
  Super::Init(aContent, aParent, aPrevInFlow);

  nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(this);
  if (parent) {
    nsFrameState state = parent->GetStateBits();

    mState |= state & (NS_FRAME_IN_POPUP);
  }
}

void
ViewportFrame::SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList)
{
#ifdef DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
#endif
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
}

void
ViewportFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  PROFILER_LABEL("ViewportFrame", "BuildDisplayList");
  nsIFrame* kid = mFrames.FirstChild();
  if (!kid)
    return;

  // make the kid's BorderBackground our own. This ensures that the canvas
  // frame's background becomes our own background and therefore appears
  // below negative z-index elements.
  BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
}

void
ViewportFrame::AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList ||
               aListID == GetAbsoluteListID(), "unexpected child list");
  NS_ASSERTION(aListID != GetAbsoluteListID() ||
               GetChildList(aListID).IsEmpty(), "Shouldn't have any kids!");
  nsContainerFrame::AppendFrames(aListID, aFrameList);
}

void
ViewportFrame::InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList ||
               aListID == GetAbsoluteListID(), "unexpected child list");
  NS_ASSERTION(aListID != GetAbsoluteListID() ||
               GetChildList(aListID).IsEmpty(), "Shouldn't have any kids!");
  nsContainerFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
}

void
ViewportFrame::RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList ||
               aListID == GetAbsoluteListID(), "unexpected child list");
  nsContainerFrame::RemoveFrame(aListID, aOldFrame);
}

/* virtual */ nscoord
ViewportFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetMinWidth(aRenderingContext);

  return result;
}

/* virtual */ nscoord
ViewportFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetPrefWidth(aRenderingContext);

  return result;
}

nsPoint
ViewportFrame::AdjustReflowStateForScrollbars(nsHTMLReflowState* aReflowState) const
{
  // Get our prinicpal child frame and see if we're scrollable
  nsIFrame* kidFrame = mFrames.FirstChild();
  nsIScrollableFrame* scrollingFrame = do_QueryFrame(kidFrame);

  if (scrollingFrame) {
    nsMargin scrollbars = scrollingFrame->GetActualScrollbarSizes();
    aReflowState->SetComputedWidth(aReflowState->ComputedWidth() -
                                   scrollbars.LeftRight());
    aReflowState->AvailableWidth() -= scrollbars.LeftRight();
    aReflowState->SetComputedHeightWithoutResettingResizeFlags(
      aReflowState->ComputedHeight() - scrollbars.TopBottom());
    return nsPoint(scrollbars.left, scrollbars.top);
  }
  return nsPoint(0, 0);
}

nsRect
ViewportFrame::AdjustReflowStateAsContainingBlock(nsHTMLReflowState* aReflowState) const
{
#ifdef DEBUG
  nsPoint offset =
#endif
    AdjustReflowStateForScrollbars(aReflowState);

  NS_ASSERTION(GetAbsoluteContainingBlock()->GetChildList().IsEmpty() ||
               (offset.x == 0 && offset.y == 0),
               "We don't handle correct positioning of fixed frames with "
               "scrollbars in odd positions");

  // If a scroll position clamping scroll-port size has been set, layout
  // fixed position elements to this size instead of the computed size.
  nsRect rect(0, 0, aReflowState->ComputedWidth(), aReflowState->ComputedHeight());
  nsIPresShell* ps = PresContext()->PresShell();
  if (ps->IsScrollPositionClampingScrollPortSizeSet()) {
    rect.SizeTo(ps->GetScrollPositionClampingScrollPortSize());
  }

  // Make sure content document fixed-position margins are respected.
  rect.Deflate(ps->GetContentDocumentFixedPositionMargins());
  return rect;
}

void
ViewportFrame::Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("ViewportFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("ViewportFrame::Reflow");

  // Initialize OUT parameters
  aStatus = NS_FRAME_COMPLETE;

  // Because |Reflow| sets mComputedHeight on the child to
  // availableHeight.
  AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);

  // Set our size up front, since some parts of reflow depend on it
  // being already set.  Note that the computed height may be
  // unconstrained; that's ok.  Consumers should watch out for that.
  SetSize(nsSize(aReflowState.ComputedWidth(), aReflowState.ComputedHeight()));
 
  // Reflow the main content first so that the placeholders of the
  // fixed-position frames will be in the right places on an initial
  // reflow.
  nscoord kidHeight = 0;

  if (mFrames.NotEmpty()) {
    // Deal with a non-incremental reflow or an incremental reflow
    // targeted at our one-and-only principal child frame.
    if (aReflowState.ShouldReflowAllKids() ||
        aReflowState.mFlags.mVResize ||
        NS_SUBTREE_DIRTY(mFrames.FirstChild())) {
      // Reflow our one-and-only principal child frame
      nsIFrame*           kidFrame = mFrames.FirstChild();
      nsHTMLReflowMetrics kidDesiredSize(aReflowState);
      nsSize              availableSpace(aReflowState.AvailableWidth(),
                                         aReflowState.AvailableHeight());
      nsHTMLReflowState   kidReflowState(aPresContext, aReflowState,
                                         kidFrame, availableSpace);

      // Reflow the frame
      kidReflowState.SetComputedHeight(aReflowState.ComputedHeight());
      ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                  0, 0, 0, aStatus);
      kidHeight = kidDesiredSize.Height();

      FinishReflowChild(kidFrame, aPresContext, kidDesiredSize, nullptr, 0, 0, 0);
    } else {
      kidHeight = mFrames.FirstChild()->GetSize().height;
    }
  }

  NS_ASSERTION(aReflowState.AvailableWidth() != NS_UNCONSTRAINEDSIZE,
               "shouldn't happen anymore");

  // Return the max size as our desired size
  aDesiredSize.Width() = aReflowState.AvailableWidth();
  // Being flowed initially at an unconstrained height means we should
  // return our child's intrinsic size.
  aDesiredSize.Height() = aReflowState.ComputedHeight() != NS_UNCONSTRAINEDSIZE
                          ? aReflowState.ComputedHeight()
                          : kidHeight;
  aDesiredSize.SetOverflowAreasToDesiredBounds();

  if (mFrames.NotEmpty()) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, mFrames.FirstChild());
  }

  if (IsAbsoluteContainer()) {
    // Make a copy of the reflow state and change the computed width and height
    // to reflect the available space for the fixed items
    nsHTMLReflowState reflowState(aReflowState);

    if (reflowState.AvailableHeight() == NS_UNCONSTRAINEDSIZE) {
      // We have an intrinsic-height document with abs-pos/fixed-pos children.
      // Set the available height and mComputedHeight to our chosen height.
      reflowState.AvailableHeight() = aDesiredSize.Height();
      // Not having border/padding simplifies things
      NS_ASSERTION(reflowState.ComputedPhysicalBorderPadding() == nsMargin(0,0,0,0),
                   "Viewports can't have border/padding");
      reflowState.SetComputedHeight(aDesiredSize.Height());
    }

    nsRect rect = AdjustReflowStateAsContainingBlock(&reflowState);

    // Just reflow all the fixed-pos frames.
    GetAbsoluteContainingBlock()->Reflow(this, aPresContext, reflowState, aStatus,
                                         rect,
                                         false, true, true, // XXX could be optimized
                                         &aDesiredSize.mOverflowAreas);
  }

  // If we were dirty then do a repaint
  if (GetStateBits() & NS_FRAME_IS_DIRTY) {
    InvalidateFrame();
  }

  // Clipping is handled by the document container (e.g., nsSubDocumentFrame),
  // so we don't need to change our overflow areas.
  bool overflowChanged = FinishAndStoreOverflow(&aDesiredSize);
  if (overflowChanged) {
    // We may need to alert our container to get it to pick up the
    // overflow change.
    nsSubDocumentFrame* container = static_cast<nsSubDocumentFrame*>
      (nsLayoutUtils::GetCrossDocParentFrame(this));
    if (container && !container->ShouldClipSubdocument()) {
      container->PresContext()->PresShell()->
        FrameNeedsReflow(container, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
    }
  }

  NS_FRAME_TRACE_REFLOW_OUT("ViewportFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
}

nsIAtom*
ViewportFrame::GetType() const
{
  return nsGkAtoms::viewportFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
ViewportFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Viewport"), aResult);
}
#endif
