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
#include "nsCanvasFrame.h"
#include "nsAbsoluteContainingBlock.h"
#include "GeckoProfiler.h"
#include "nsIMozBrowserFrame.h"

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
ViewportFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  PROFILER_LABEL("ViewportFrame", "BuildDisplayList",
    js::ProfileEntry::Category::GRAPHICS);

  if (nsIFrame* kid = mFrames.FirstChild()) {
    // make the kid's BorderBackground our own. This ensures that the canvas
    // frame's background becomes our own background and therefore appears
    // below negative z-index elements.
    BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
  }

  nsDisplayList topLayerList;
  BuildDisplayListForTopLayer(aBuilder, &topLayerList);
  if (!topLayerList.IsEmpty()) {
    // Wrap the whole top layer in a single item with maximum z-index,
    // and append it at the very end, so that it stays at the topmost.
    nsDisplayWrapList* wrapList =
      new (aBuilder) nsDisplayWrapList(aBuilder, this, &topLayerList);
    wrapList->SetOverrideZIndex(
      std::numeric_limits<decltype(wrapList->ZIndex())>::max());
    aLists.PositionedDescendants()->AppendNewToTop(wrapList);
  }
}

#ifdef DEBUG
/**
 * Returns whether we are going to put an element in the top layer for
 * fullscreen. This function should matches the CSS rule in ua.css.
 */
static bool
ShouldInTopLayerForFullscreen(Element* aElement)
{
  if (!aElement->GetParent()) {
    return false;
  }
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(aElement);
  if (browserFrame && browserFrame->GetReallyIsBrowserOrApp()) {
    return false;
  }
  return true;
}
#endif // DEBUG

static void
BuildDisplayListForTopLayerFrame(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame,
                                 nsDisplayList* aList)
{
  nsRect dirty;
  nsDisplayListBuilder::OutOfFlowDisplayData*
    savedOutOfFlowData = nsDisplayListBuilder::GetOutOfFlowData(aFrame);
  if (savedOutOfFlowData) {
    dirty = savedOutOfFlowData->mDirtyRect;
  }
  nsDisplayList list;
  aFrame->BuildDisplayListForStackingContext(aBuilder, dirty, &list);
  aList->AppendToTop(&list);
}

void
ViewportFrame::BuildDisplayListForTopLayer(nsDisplayListBuilder* aBuilder,
                                           nsDisplayList* aList)
{
  nsIDocument* doc = PresContext()->Document();
  nsTArray<Element*> fullscreenStack = doc->GetFullscreenStack();
  for (Element* elem : fullscreenStack) {
    if (nsIFrame* frame = elem->GetPrimaryFrame()) {
      // There are two cases where an element in fullscreen is not in
      // the top layer:
      // 1. When building display list for purpose other than painting,
      //    it is possible that there is inconsistency between the style
      //    info and the content tree.
      // 2. This is an element which we are not going to put in the top
      //    layer for fullscreen. See ShouldInTopLayerForFullscreen().
      // In both cases, we want to skip the frame here and paint it in
      // the normal path.
      if (frame->StyleDisplay()->mTopLayer == NS_STYLE_TOP_LAYER_NONE) {
        MOZ_ASSERT(!aBuilder->IsForPainting() ||
                   !ShouldInTopLayerForFullscreen(elem));
        continue;
      }
      MOZ_ASSERT(ShouldInTopLayerForFullscreen(elem));
      // Inner SVG, MathML elements, as well as children of some XUL
      // elements are not allowed to be out-of-flow. They should not
      // be handled as top layer element here.
      if (!(frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
        MOZ_ASSERT(!elem->GetParent()->IsHTMLElement(), "HTML element "
                   "should always be out-of-flow if in the top layer");
        continue;
      }
      MOZ_ASSERT(frame->GetParent() == this);
      BuildDisplayListForTopLayerFrame(aBuilder, frame, aList);
    }
  }

  nsIPresShell* shell = PresContext()->PresShell();
  if (nsCanvasFrame* canvasFrame = shell->GetCanvasFrame()) {
    if (Element* container = canvasFrame->GetCustomContentContainer()) {
      if (nsIFrame* frame = container->GetPrimaryFrame()) {
        BuildDisplayListForTopLayerFrame(aBuilder, frame, aList);
      }
    }
  }
}

#ifdef DEBUG
void
ViewportFrame::SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList)
{
  nsFrame::VerifyDirtyBitSet(aChildList);
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
}

void
ViewportFrame::AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  NS_ASSERTION(GetChildList(aListID).IsEmpty(), "Shouldn't have any kids!");
  nsContainerFrame::AppendFrames(aListID, aFrameList);
}

void
ViewportFrame::InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  NS_ASSERTION(GetChildList(aListID).IsEmpty(), "Shouldn't have any kids!");
  nsContainerFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
}

void
ViewportFrame::RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  nsContainerFrame::RemoveFrame(aListID, aOldFrame);
}
#endif

/* virtual */ nscoord
ViewportFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetMinISize(aRenderingContext);

  return result;
}

/* virtual */ nscoord
ViewportFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetPrefISize(aRenderingContext);

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

  return rect;
}

void
ViewportFrame::Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("ViewportFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("ViewportFrame::Reflow");

  // Initialize OUT parameters
  aStatus = NS_FRAME_COMPLETE;

  // Because |Reflow| sets ComputedBSize() on the child to our
  // ComputedBSize().
  AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);

  // Set our size up front, since some parts of reflow depend on it
  // being already set.  Note that the computed height may be
  // unconstrained; that's ok.  Consumers should watch out for that.
  SetSize(nsSize(aReflowState.ComputedWidth(), aReflowState.ComputedHeight()));

  // Reflow the main content first so that the placeholders of the
  // fixed-position frames will be in the right places on an initial
  // reflow.
  nscoord kidBSize = 0;
  WritingMode wm = aReflowState.GetWritingMode();

  if (mFrames.NotEmpty()) {
    // Deal with a non-incremental reflow or an incremental reflow
    // targeted at our one-and-only principal child frame.
    if (aReflowState.ShouldReflowAllKids() ||
        aReflowState.IsVResize() ||
        NS_SUBTREE_DIRTY(mFrames.FirstChild())) {
      // Reflow our one-and-only principal child frame
      nsIFrame*           kidFrame = mFrames.FirstChild();
      nsHTMLReflowMetrics kidDesiredSize(aReflowState);
      WritingMode         wm = kidFrame->GetWritingMode();
      LogicalSize         availableSpace = aReflowState.AvailableSize(wm);
      nsHTMLReflowState   kidReflowState(aPresContext, aReflowState,
                                         kidFrame, availableSpace);

      // Reflow the frame
      kidReflowState.SetComputedBSize(aReflowState.ComputedBSize());
      ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                  0, 0, 0, aStatus);
      kidBSize = kidDesiredSize.BSize(wm);

      FinishReflowChild(kidFrame, aPresContext, kidDesiredSize, nullptr, 0, 0, 0);
    } else {
      kidBSize = LogicalSize(wm, mFrames.FirstChild()->GetSize()).BSize(wm);
    }
  }

  NS_ASSERTION(aReflowState.AvailableISize() != NS_UNCONSTRAINEDSIZE,
               "shouldn't happen anymore");

  // Return the max size as our desired size
  LogicalSize maxSize(wm, aReflowState.AvailableISize(),
                      // Being flowed initially at an unconstrained block size
                      // means we should return our child's intrinsic size.
                      aReflowState.ComputedBSize() != NS_UNCONSTRAINEDSIZE
                        ? aReflowState.ComputedBSize()
                        : kidBSize);
  aDesiredSize.SetSize(wm, maxSize);
  aDesiredSize.SetOverflowAreasToDesiredBounds();

  if (HasAbsolutelyPositionedChildren()) {
    // Make a copy of the reflow state and change the computed width and height
    // to reflect the available space for the fixed items
    nsHTMLReflowState reflowState(aReflowState);

    if (reflowState.AvailableBSize() == NS_UNCONSTRAINEDSIZE) {
      // We have an intrinsic-height document with abs-pos/fixed-pos children.
      // Set the available height and mComputedHeight to our chosen height.
      reflowState.AvailableBSize() = maxSize.BSize(wm);
      // Not having border/padding simplifies things
      NS_ASSERTION(reflowState.ComputedPhysicalBorderPadding() == nsMargin(0,0,0,0),
                   "Viewports can't have border/padding");
      reflowState.SetComputedBSize(maxSize.BSize(wm));
    }

    nsRect rect = AdjustReflowStateAsContainingBlock(&reflowState);
    nsOverflowAreas* overflowAreas = &aDesiredSize.mOverflowAreas;
    nsIScrollableFrame* rootScrollFrame =
                    aPresContext->PresShell()->GetRootScrollFrameAsScrollable();
    if (rootScrollFrame && !rootScrollFrame->IsIgnoringViewportClipping()) {
      overflowAreas = nullptr;
    }
    GetAbsoluteContainingBlock()->Reflow(this, aPresContext, reflowState, aStatus,
                                         rect,
                                         false, true, true, // XXX could be optimized
                                         overflowAreas);
  }

  if (mFrames.NotEmpty()) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, mFrames.FirstChild());
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

bool
ViewportFrame::UpdateOverflow()
{
  nsIScrollableFrame* rootScrollFrame =
    PresContext()->PresShell()->GetRootScrollFrameAsScrollable();
  if (rootScrollFrame && !rootScrollFrame->IsIgnoringViewportClipping()) {
    return false;
  }

  return nsFrame::UpdateOverflow();
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
