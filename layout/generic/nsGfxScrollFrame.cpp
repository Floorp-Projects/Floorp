/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object to wrap rendering objects that should be scrollable */

#include "nsGfxScrollFrame.h"

#include "base/compiler_specific.h"
#include "DisplayItemClip.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsView.h"
#include "nsIScrollable.h"
#include "nsContainerFrame.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsContentList.h"
#include "nsIDocumentInlines.h"
#include "nsFontMetrics.h"
#include "nsBoxLayoutState.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsScrollbarFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsITextControlFrame.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsAutoPtr.h"
#include "nsPresState.h"
#include "nsIHTMLDocument.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsBidiUtils.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/dom/Element.h"
#include <stdint.h>
#include "mozilla/MathAlgorithms.h"
#include "FrameLayerBuilder.h"
#include "nsSMILKeySpline.h"
#include "nsSubDocumentFrame.h"
#include "nsSVGOuterSVGFrame.h"
#include "mozilla/Attributes.h"
#include "ScrollbarActivity.h"
#include "nsRefreshDriver.h"
#include "nsThemeConstants.h"
#include "nsSVGIntegrationUtils.h"
#include "nsIScrollPositionListener.h"
#include "StickyScrollContainer.h"
#include "nsIFrameInlines.h"
#include "gfxPlatform.h"
#include "gfxPrefs.h"
#include <mozilla/layers/AxisPhysicsModel.h>
#include <mozilla/layers/AxisPhysicsMSDModel.h>
#include <algorithm>
#include <cstdlib> // for std::abs(int/long)
#include <cmath> // for std::abs(float/double)

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layout;

static bool
BuildScrollContainerLayers()
{
  static bool sContainerlessScrollingEnabled;
  static bool sContainerlessScrollingPrefCached = false;

  if (!sContainerlessScrollingPrefCached) {
    sContainerlessScrollingPrefCached = true;
    Preferences::AddBoolVarCache(&sContainerlessScrollingEnabled,
                                 "layout.async-containerless-scrolling.enabled",
                                 true);
  }

  return !sContainerlessScrollingEnabled;
}

//----------------------------------------------------------------------

//----------nsHTMLScrollFrame-------------------------------------------

nsHTMLScrollFrame*
NS_NewHTMLScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, bool aIsRoot)
{
  return new (aPresShell) nsHTMLScrollFrame(aPresShell, aContext, aIsRoot);
}

NS_IMPL_FRAMEARENA_HELPERS(nsHTMLScrollFrame)

nsHTMLScrollFrame::nsHTMLScrollFrame(nsIPresShell* aShell, nsStyleContext* aContext, bool aIsRoot)
  : nsContainerFrame(aContext),
    mHelper(ALLOW_THIS_IN_INITIALIZER_LIST(this), aIsRoot)
{
}

void
nsHTMLScrollFrame::ScrollbarActivityStarted() const
{
  if (mHelper.mScrollbarActivity) {
    mHelper.mScrollbarActivity->ActivityStarted();
  }
}

void
nsHTMLScrollFrame::ScrollbarActivityStopped() const
{
  if (mHelper.mScrollbarActivity) {
    mHelper.mScrollbarActivity->ActivityStopped();
  }
}

nsresult
nsHTMLScrollFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  return mHelper.CreateAnonymousContent(aElements);
}

void
nsHTMLScrollFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                            uint32_t aFilter)
{
  mHelper.AppendAnonymousContentTo(aElements, aFilter);
}

void
nsHTMLScrollFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  DestroyAbsoluteFrames(aDestructRoot);
  mHelper.Destroy();
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

void
nsHTMLScrollFrame::SetInitialChildList(ChildListID  aListID,
                                       nsFrameList& aChildList)
{
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
  mHelper.ReloadChildFrames();
}


void
nsHTMLScrollFrame::AppendFrames(ChildListID  aListID,
                                nsFrameList& aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "Only main list supported");
  mFrames.AppendFrames(nullptr, aFrameList);
  mHelper.ReloadChildFrames();
}

void
nsHTMLScrollFrame::InsertFrames(ChildListID aListID,
                                nsIFrame* aPrevFrame,
                                nsFrameList& aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "Only main list supported");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");
  mFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);
  mHelper.ReloadChildFrames();
}

void
nsHTMLScrollFrame::RemoveFrame(ChildListID aListID,
                               nsIFrame* aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList, "Only main list supported");
  mFrames.DestroyFrame(aOldFrame);
  mHelper.ReloadChildFrames();
}

nsSplittableType
nsHTMLScrollFrame::GetSplittableType() const
{
  return NS_FRAME_NOT_SPLITTABLE;
}

nsIAtom*
nsHTMLScrollFrame::GetType() const
{
  return nsGkAtoms::scrollFrame;
}

/**
 HTML scrolling implementation

 All other things being equal, we prefer layouts with fewer scrollbars showing.
*/

struct MOZ_STACK_CLASS ScrollReflowState {
  const nsHTMLReflowState& mReflowState;
  nsBoxLayoutState mBoxState;
  ScrollbarStyles mStyles;
  nsMargin mComputedBorder;

  // === Filled in by ReflowScrolledFrame ===
  nsOverflowAreas mContentsOverflowAreas;
  bool mReflowedContentsWithHScrollbar;
  bool mReflowedContentsWithVScrollbar;

  // === Filled in when TryLayout succeeds ===
  // The size of the inside-border area
  nsSize mInsideBorderSize;
  // Whether we decided to show the horizontal scrollbar
  bool mShowHScrollbar;
  // Whether we decided to show the vertical scrollbar
  bool mShowVScrollbar;

  ScrollReflowState(nsIScrollableFrame* aFrame,
                    const nsHTMLReflowState& aState) :
    mReflowState(aState),
    // mBoxState is just used for scrollbars so we don't need to
    // worry about the reflow depth here
    mBoxState(aState.frame->PresContext(), aState.rendContext, 0),
    mStyles(aFrame->GetScrollbarStyles()) {
  }
};

// XXXldb Can this go away?
static nsSize ComputeInsideBorderSize(ScrollReflowState* aState,
                                      const nsSize& aDesiredInsideBorderSize)
{
  // aDesiredInsideBorderSize is the frame size; i.e., it includes
  // borders and padding (but the scrolled child doesn't have
  // borders). The scrolled child has the same padding as us.
  nscoord contentWidth = aState->mReflowState.ComputedWidth();
  if (contentWidth == NS_UNCONSTRAINEDSIZE) {
    contentWidth = aDesiredInsideBorderSize.width -
      aState->mReflowState.ComputedPhysicalPadding().LeftRight();
  }
  nscoord contentHeight = aState->mReflowState.ComputedHeight();
  if (contentHeight == NS_UNCONSTRAINEDSIZE) {
    contentHeight = aDesiredInsideBorderSize.height -
      aState->mReflowState.ComputedPhysicalPadding().TopBottom();
  }

  contentWidth  = aState->mReflowState.ApplyMinMaxWidth(contentWidth);
  contentHeight = aState->mReflowState.ApplyMinMaxHeight(contentHeight);
  return nsSize(contentWidth + aState->mReflowState.ComputedPhysicalPadding().LeftRight(),
                contentHeight + aState->mReflowState.ComputedPhysicalPadding().TopBottom());
}

static void
GetScrollbarMetrics(nsBoxLayoutState& aState, nsIFrame* aBox, nsSize* aMin,
                    nsSize* aPref, bool aVertical)
{
  NS_ASSERTION(aState.GetRenderingContext(),
               "Must have rendering context in layout state for size "
               "computations");

  if (aMin) {
    *aMin = aBox->GetMinSize(aState);
    nsBox::AddMargin(aBox, *aMin);
    if (aMin->width < 0) {
      aMin->width = 0;
    }
    if (aMin->height < 0) {
      aMin->height = 0;
    }
  }

  if (aPref) {
    *aPref = aBox->GetPrefSize(aState);
    nsBox::AddMargin(aBox, *aPref);
    if (aPref->width < 0) {
      aPref->width = 0;
    }
    if (aPref->height < 0) {
      aPref->height = 0;
    }
  }
}

/**
 * Assuming that we know the metrics for our wrapped frame and
 * whether the horizontal and/or vertical scrollbars are present,
 * compute the resulting layout and return true if the layout is
 * consistent. If the layout is consistent then we fill in the
 * computed fields of the ScrollReflowState.
 *
 * The layout is consistent when both scrollbars are showing if and only
 * if they should be showing. A horizontal scrollbar should be showing if all
 * following conditions are met:
 * 1) the style is not HIDDEN
 * 2) our inside-border height is at least the scrollbar height (i.e., the
 * scrollbar fits vertically)
 * 3) our scrollport width (the inside-border width minus the width allocated for a
 * vertical scrollbar, if showing) is at least the scrollbar's min-width
 * (i.e., the scrollbar fits horizontally)
 * 4) the style is SCROLL, or the kid's overflow-area XMost is
 * greater than the scrollport width
 *
 * @param aForce if true, then we just assume the layout is consistent.
 */
bool
nsHTMLScrollFrame::TryLayout(ScrollReflowState* aState,
                             nsHTMLReflowMetrics* aKidMetrics,
                             bool aAssumeHScroll, bool aAssumeVScroll,
                             bool aForce)
{
  if ((aState->mStyles.mVertical == NS_STYLE_OVERFLOW_HIDDEN && aAssumeVScroll) ||
      (aState->mStyles.mHorizontal == NS_STYLE_OVERFLOW_HIDDEN && aAssumeHScroll)) {
    NS_ASSERTION(!aForce, "Shouldn't be forcing a hidden scrollbar to show!");
    return false;
  }

  if (aAssumeVScroll != aState->mReflowedContentsWithVScrollbar ||
      (aAssumeHScroll != aState->mReflowedContentsWithHScrollbar &&
       ScrolledContentDependsOnHeight(aState))) {
    ReflowScrolledFrame(aState, aAssumeHScroll, aAssumeVScroll, aKidMetrics,
                        false);
  }

  nsSize vScrollbarMinSize(0, 0);
  nsSize vScrollbarPrefSize(0, 0);
  if (mHelper.mVScrollbarBox) {
    GetScrollbarMetrics(aState->mBoxState, mHelper.mVScrollbarBox,
                        &vScrollbarMinSize,
                        aAssumeVScroll ? &vScrollbarPrefSize : nullptr, true);
    nsScrollbarFrame* scrollbar = do_QueryFrame(mHelper.mVScrollbarBox);
    scrollbar->SetScrollbarMediatorContent(mContent);
  }
  nscoord vScrollbarDesiredWidth = aAssumeVScroll ? vScrollbarPrefSize.width : 0;
  nscoord vScrollbarMinHeight = aAssumeVScroll ? vScrollbarMinSize.height : 0;

  nsSize hScrollbarMinSize(0, 0);
  nsSize hScrollbarPrefSize(0, 0);
  if (mHelper.mHScrollbarBox) {
    GetScrollbarMetrics(aState->mBoxState, mHelper.mHScrollbarBox,
                        &hScrollbarMinSize,
                        aAssumeHScroll ? &hScrollbarPrefSize : nullptr, false);
    nsScrollbarFrame* scrollbar = do_QueryFrame(mHelper.mHScrollbarBox);
    scrollbar->SetScrollbarMediatorContent(mContent);
  }
  nscoord hScrollbarDesiredHeight = aAssumeHScroll ? hScrollbarPrefSize.height : 0;
  nscoord hScrollbarMinWidth = aAssumeHScroll ? hScrollbarMinSize.width : 0;

  // First, compute our inside-border size and scrollport size
  // XXXldb Can we depend more on ComputeSize here?
  nsSize desiredInsideBorderSize;
  desiredInsideBorderSize.width = vScrollbarDesiredWidth +
    std::max(aKidMetrics->Width(), hScrollbarMinWidth);
  desiredInsideBorderSize.height = hScrollbarDesiredHeight +
    std::max(aKidMetrics->Height(), vScrollbarMinHeight);
  aState->mInsideBorderSize =
    ComputeInsideBorderSize(aState, desiredInsideBorderSize);
  nsSize scrollPortSize = nsSize(std::max(0, aState->mInsideBorderSize.width - vScrollbarDesiredWidth),
                                 std::max(0, aState->mInsideBorderSize.height - hScrollbarDesiredHeight));
  nsSize visualScrollPortSize = scrollPortSize;
  nsIPresShell* presShell = PresContext()->PresShell();
  if (mHelper.mIsRoot && presShell->IsScrollPositionClampingScrollPortSizeSet()) {
    visualScrollPortSize = presShell->GetScrollPositionClampingScrollPortSize();
  }

  if (!aForce) {
    nsRect scrolledRect =
      mHelper.GetScrolledRectInternal(aState->mContentsOverflowAreas.ScrollableOverflow(),
                                     scrollPortSize);
    nscoord oneDevPixel = aState->mBoxState.PresContext()->DevPixelsToAppUnits(1);

    // If the style is HIDDEN then we already know that aAssumeHScroll is false
    if (aState->mStyles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN) {
      bool wantHScrollbar =
        aState->mStyles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL ||
        scrolledRect.XMost() >= visualScrollPortSize.width + oneDevPixel ||
        scrolledRect.x <= -oneDevPixel;
      if (scrollPortSize.width < hScrollbarMinSize.width)
        wantHScrollbar = false;
      if (wantHScrollbar != aAssumeHScroll)
        return false;
    }

    // If the style is HIDDEN then we already know that aAssumeVScroll is false
    if (aState->mStyles.mVertical != NS_STYLE_OVERFLOW_HIDDEN) {
      bool wantVScrollbar =
        aState->mStyles.mVertical == NS_STYLE_OVERFLOW_SCROLL ||
        scrolledRect.YMost() >= visualScrollPortSize.height + oneDevPixel ||
        scrolledRect.y <= -oneDevPixel;
      if (scrollPortSize.height < vScrollbarMinSize.height)
        wantVScrollbar = false;
      if (wantVScrollbar != aAssumeVScroll)
        return false;
    }
  }

  nscoord vScrollbarActualWidth = aState->mInsideBorderSize.width - scrollPortSize.width;

  aState->mShowHScrollbar = aAssumeHScroll;
  aState->mShowVScrollbar = aAssumeVScroll;
  nsPoint scrollPortOrigin(aState->mComputedBorder.left,
                           aState->mComputedBorder.top);
  if (!mHelper.IsScrollbarOnRight()) {
    scrollPortOrigin.x += vScrollbarActualWidth;
  }
  mHelper.mScrollPort = nsRect(scrollPortOrigin, scrollPortSize);
  return true;
}

bool
nsHTMLScrollFrame::ScrolledContentDependsOnHeight(ScrollReflowState* aState)
{
  // Return true if ReflowScrolledFrame is going to do something different
  // based on the presence of a horizontal scrollbar.
  return (mHelper.mScrolledFrame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT) ||
    aState->mReflowState.ComputedHeight() != NS_UNCONSTRAINEDSIZE ||
    aState->mReflowState.ComputedMinHeight() > 0 ||
    aState->mReflowState.ComputedMaxHeight() != NS_UNCONSTRAINEDSIZE;
}

void
nsHTMLScrollFrame::ReflowScrolledFrame(ScrollReflowState* aState,
                                       bool aAssumeHScroll,
                                       bool aAssumeVScroll,
                                       nsHTMLReflowMetrics* aMetrics,
                                       bool aFirstPass)
{
  // these could be NS_UNCONSTRAINEDSIZE ... std::min arithmetic should
  // be OK
  const nsMargin& padding = aState->mReflowState.ComputedPhysicalPadding();
  nscoord availWidth = aState->mReflowState.ComputedWidth() + padding.LeftRight();

  nscoord computedHeight = aState->mReflowState.ComputedHeight();
  nscoord computedMinHeight = aState->mReflowState.ComputedMinHeight();
  nscoord computedMaxHeight = aState->mReflowState.ComputedMaxHeight();
  if (!ShouldPropagateComputedHeightToScrolledContent()) {
    computedHeight = NS_UNCONSTRAINEDSIZE;
    computedMinHeight = 0;
    computedMaxHeight = NS_UNCONSTRAINEDSIZE;
  }
  if (aAssumeHScroll) {
    nsSize hScrollbarPrefSize;
    GetScrollbarMetrics(aState->mBoxState, mHelper.mHScrollbarBox,
                        nullptr, &hScrollbarPrefSize, false);
    if (computedHeight != NS_UNCONSTRAINEDSIZE) {
      computedHeight = std::max(0, computedHeight - hScrollbarPrefSize.height);
    }
    computedMinHeight = std::max(0, computedMinHeight - hScrollbarPrefSize.height);
    if (computedMaxHeight != NS_UNCONSTRAINEDSIZE) {
      computedMaxHeight = std::max(0, computedMaxHeight - hScrollbarPrefSize.height);
    }
  }

  if (aAssumeVScroll) {
    nsSize vScrollbarPrefSize;
    GetScrollbarMetrics(aState->mBoxState, mHelper.mVScrollbarBox,
                        nullptr, &vScrollbarPrefSize, true);
    availWidth = std::max(0, availWidth - vScrollbarPrefSize.width);
  }

  nsPresContext* presContext = PresContext();

  // Pass false for aInit so we can pass in the correct padding.
  nsHTMLReflowState
    kidReflowState(presContext, aState->mReflowState,
                   mHelper.mScrolledFrame,
                   LogicalSize(mHelper.mScrolledFrame->GetWritingMode(),
                               nsSize(availWidth, NS_UNCONSTRAINEDSIZE)),
                   -1, -1, nsHTMLReflowState::CALLER_WILL_INIT);
  kidReflowState.Init(presContext, -1, -1, nullptr,
                      &padding);
  kidReflowState.mFlags.mAssumingHScrollbar = aAssumeHScroll;
  kidReflowState.mFlags.mAssumingVScrollbar = aAssumeVScroll;
  kidReflowState.SetComputedHeight(computedHeight);
  kidReflowState.ComputedMinHeight() = computedMinHeight;
  kidReflowState.ComputedMaxHeight() = computedMaxHeight;

  // Temporarily set mHasHorizontalScrollbar/mHasVerticalScrollbar to
  // reflect our assumptions while we reflow the child.
  bool didHaveHorizontalScrollbar = mHelper.mHasHorizontalScrollbar;
  bool didHaveVerticalScrollbar = mHelper.mHasVerticalScrollbar;
  mHelper.mHasHorizontalScrollbar = aAssumeHScroll;
  mHelper.mHasVerticalScrollbar = aAssumeVScroll;

  nsReflowStatus status;
  ReflowChild(mHelper.mScrolledFrame, presContext, *aMetrics,
              kidReflowState, 0, 0,
              NS_FRAME_NO_MOVE_FRAME, status);

  mHelper.mHasHorizontalScrollbar = didHaveHorizontalScrollbar;
  mHelper.mHasVerticalScrollbar = didHaveVerticalScrollbar;

  // Don't resize or position the view (if any) because we're going to resize
  // it to the correct size anyway in PlaceScrollArea. Allowing it to
  // resize here would size it to the natural height of the frame,
  // which will usually be different from the scrollport height;
  // invalidating the difference will cause unnecessary repainting.
  FinishReflowChild(mHelper.mScrolledFrame, presContext,
                    *aMetrics, &kidReflowState, 0, 0,
                    NS_FRAME_NO_MOVE_FRAME | NS_FRAME_NO_SIZE_VIEW);

  // XXX Some frames (e.g., nsPluginFrame, nsFrameFrame, nsTextFrame) don't bother
  // setting their mOverflowArea. This is wrong because every frame should
  // always set mOverflowArea. In fact nsPluginFrame and nsFrameFrame don't
  // support the 'outline' property because of this. Rather than fix the world
  // right now, just fix up the overflow area if necessary. Note that we don't
  // check HasOverflowRect() because it could be set even though the
  // overflow area doesn't include the frame bounds.
  aMetrics->UnionOverflowAreasWithDesiredBounds();

  if (MOZ_UNLIKELY(StyleDisplay()->mOverflowClipBox ==
                     NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX)) {
    nsOverflowAreas childOverflow;
    nsLayoutUtils::UnionChildOverflow(mHelper.mScrolledFrame, childOverflow);
    nsRect childScrollableOverflow = childOverflow.ScrollableOverflow();
    childScrollableOverflow.Inflate(padding);
    nsRect contentArea = nsRect(0, 0, availWidth, computedHeight);
    if (!contentArea.Contains(childScrollableOverflow)) {
      aMetrics->mOverflowAreas.ScrollableOverflow() = childScrollableOverflow;
    }
  }

  aState->mContentsOverflowAreas = aMetrics->mOverflowAreas;
  aState->mReflowedContentsWithHScrollbar = aAssumeHScroll;
  aState->mReflowedContentsWithVScrollbar = aAssumeVScroll;
}

bool
nsHTMLScrollFrame::GuessHScrollbarNeeded(const ScrollReflowState& aState)
{
  if (aState.mStyles.mHorizontal != NS_STYLE_OVERFLOW_AUTO)
    // no guessing required
    return aState.mStyles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL;

  return mHelper.mHasHorizontalScrollbar;
}

bool
nsHTMLScrollFrame::GuessVScrollbarNeeded(const ScrollReflowState& aState)
{
  if (aState.mStyles.mVertical != NS_STYLE_OVERFLOW_AUTO)
    // no guessing required
    return aState.mStyles.mVertical == NS_STYLE_OVERFLOW_SCROLL;

  // If we've had at least one non-initial reflow, then just assume
  // the state of the vertical scrollbar will be what we determined
  // last time.
  if (mHelper.mHadNonInitialReflow) {
    return mHelper.mHasVerticalScrollbar;
  }

  // If this is the initial reflow, guess false because usually
  // we have very little content by then.
  if (InInitialReflow())
    return false;

  if (mHelper.mIsRoot) {
    nsIFrame *f = mHelper.mScrolledFrame->GetFirstPrincipalChild();
    if (f && f->GetType() == nsGkAtoms::svgOuterSVGFrame &&
        static_cast<nsSVGOuterSVGFrame*>(f)->VerticalScrollbarNotNeeded()) {
      // Common SVG case - avoid a bad guess.
      return false;
    }
    // Assume that there will be a scrollbar; it seems to me
    // that 'most pages' do have a scrollbar, and anyway, it's cheaper
    // to do an extra reflow for the pages that *don't* need a
    // scrollbar (because on average they will have less content).
    return true;
  }

  // For non-viewports, just guess that we don't need a scrollbar.
  // XXX I wonder if statistically this is the right idea; I'm
  // basically guessing that there are a lot of overflow:auto DIVs
  // that get their intrinsic size and don't overflow
  return false;
}

bool
nsHTMLScrollFrame::InInitialReflow() const
{
  // We're in an initial reflow if NS_FRAME_FIRST_REFLOW is set, unless we're a
  // root scrollframe.  In that case we want to skip this clause altogether.
  // The guess here is that there are lots of overflow:auto divs out there that
  // end up auto-sizing so they don't overflow, and that the root basically
  // always needs a scrollbar if it did last time we loaded this page (good
  // assumption, because our initial reflow is no longer synchronous).
  return !mHelper.mIsRoot && (GetStateBits() & NS_FRAME_FIRST_REFLOW);
}

void
nsHTMLScrollFrame::ReflowContents(ScrollReflowState* aState,
                                  const nsHTMLReflowMetrics& aDesiredSize)
{
  nsHTMLReflowMetrics kidDesiredSize(aDesiredSize.GetWritingMode(), aDesiredSize.mFlags);
  ReflowScrolledFrame(aState, GuessHScrollbarNeeded(*aState),
                      GuessVScrollbarNeeded(*aState), &kidDesiredSize, true);

  // There's an important special case ... if the child appears to fit
  // in the inside-border rect (but overflows the scrollport), we
  // should try laying it out without a vertical scrollbar. It will
  // usually fit because making the available-width wider will not
  // normally make the child taller. (The only situation I can think
  // of is when you have a line containing %-width inline replaced
  // elements whose percentages sum to more than 100%, so increasing
  // the available width makes the line break where it was fitting
  // before.) If we don't treat this case specially, then we will
  // decide that showing scrollbars is OK because the content
  // overflows when we're showing scrollbars and we won't try to
  // remove the vertical scrollbar.

  // Detecting when we enter this special case is important for when
  // people design layouts that exactly fit the container "most of the
  // time".

  // XXX Is this check really sufficient to catch all the incremental cases
  // where the ideal case doesn't have a scrollbar?
  if ((aState->mReflowedContentsWithHScrollbar || aState->mReflowedContentsWithVScrollbar) &&
      aState->mStyles.mVertical != NS_STYLE_OVERFLOW_SCROLL &&
      aState->mStyles.mHorizontal != NS_STYLE_OVERFLOW_SCROLL) {
    nsSize insideBorderSize =
      ComputeInsideBorderSize(aState,
                              nsSize(kidDesiredSize.Width(), kidDesiredSize.Height()));
    nsRect scrolledRect =
      mHelper.GetScrolledRectInternal(kidDesiredSize.ScrollableOverflow(),
                                     insideBorderSize);
    if (nsRect(nsPoint(0, 0), insideBorderSize).Contains(scrolledRect)) {
      // Let's pretend we had no scrollbars coming in here
      ReflowScrolledFrame(aState, false, false, &kidDesiredSize, false);
    }
  }

  // Try vertical scrollbar settings that leave the vertical scrollbar unchanged.
  // Do this first because changing the vertical scrollbar setting is expensive,
  // forcing a reflow always.

  // Try leaving the horizontal scrollbar unchanged first. This will be more
  // efficient.
  if (TryLayout(aState, &kidDesiredSize, aState->mReflowedContentsWithHScrollbar,
                aState->mReflowedContentsWithVScrollbar, false))
    return;
  if (TryLayout(aState, &kidDesiredSize, !aState->mReflowedContentsWithHScrollbar,
                aState->mReflowedContentsWithVScrollbar, false))
    return;

  // OK, now try toggling the vertical scrollbar. The performance advantage
  // of trying the status-quo horizontal scrollbar state
  // does not exist here (we'll have to reflow due to the vertical scrollbar
  // change), so always try no horizontal scrollbar first.
  bool newVScrollbarState = !aState->mReflowedContentsWithVScrollbar;
  if (TryLayout(aState, &kidDesiredSize, false, newVScrollbarState, false))
    return;
  if (TryLayout(aState, &kidDesiredSize, true, newVScrollbarState, false))
    return;

  // OK, we're out of ideas. Try again enabling whatever scrollbars we can
  // enable and force the layout to stick even if it's inconsistent.
  // This just happens sometimes.
  TryLayout(aState, &kidDesiredSize,
            aState->mStyles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN,
            aState->mStyles.mVertical != NS_STYLE_OVERFLOW_HIDDEN,
            true);
}

void
nsHTMLScrollFrame::PlaceScrollArea(const ScrollReflowState& aState,
                                   const nsPoint& aScrollPosition)
{
  nsIFrame *scrolledFrame = mHelper.mScrolledFrame;
  // Set the x,y of the scrolled frame to the correct value
  scrolledFrame->SetPosition(mHelper.mScrollPort.TopLeft() - aScrollPosition);

  nsRect scrolledArea;
  // Preserve the width or height of empty rects
  nsSize portSize = mHelper.mScrollPort.Size();
  nsRect scrolledRect =
    mHelper.GetScrolledRectInternal(aState.mContentsOverflowAreas.ScrollableOverflow(),
                                   portSize);
  scrolledArea.UnionRectEdges(scrolledRect,
                              nsRect(nsPoint(0,0), portSize));

  // Store the new overflow area. Note that this changes where an outline
  // of the scrolled frame would be painted, but scrolled frames can't have
  // outlines (the outline would go on this scrollframe instead).
  // Using FinishAndStoreOverflow is needed so the overflow rect
  // gets set correctly.  It also messes with the overflow rect in the
  // -moz-hidden-unscrollable case, but scrolled frames can't have
  // 'overflow' either.
  // This needs to happen before SyncFrameViewAfterReflow so
  // HasOverflowRect() will return the correct value.
  nsOverflowAreas overflow(scrolledArea, scrolledArea);
  scrolledFrame->FinishAndStoreOverflow(overflow,
                                        scrolledFrame->GetSize());

  // Note that making the view *exactly* the size of the scrolled area
  // is critical, since the view scrolling code uses the size of the
  // scrolled view to clamp scroll requests.
  // Normally the scrolledFrame won't have a view but in some cases it
  // might create its own.
  nsContainerFrame::SyncFrameViewAfterReflow(scrolledFrame->PresContext(),
                                             scrolledFrame,
                                             scrolledFrame->GetView(),
                                             scrolledArea,
                                             0);
}

nscoord
nsHTMLScrollFrame::GetIntrinsicVScrollbarWidth(nsRenderingContext *aRenderingContext)
{
  ScrollbarStyles ss = GetScrollbarStyles();
  if (ss.mVertical != NS_STYLE_OVERFLOW_SCROLL || !mHelper.mVScrollbarBox)
    return 0;

  // Don't need to worry about reflow depth here since it's
  // just for scrollbars
  nsBoxLayoutState bls(PresContext(), aRenderingContext, 0);
  nsSize vScrollbarPrefSize(0, 0);
  GetScrollbarMetrics(bls, mHelper.mVScrollbarBox,
                      nullptr, &vScrollbarPrefSize, true);
  return vScrollbarPrefSize.width;
}

/* virtual */ nscoord
nsHTMLScrollFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord result = mHelper.mScrolledFrame->GetMinISize(aRenderingContext);
  DISPLAY_MIN_WIDTH(this, result);
  return result + GetIntrinsicVScrollbarWidth(aRenderingContext);
}

/* virtual */ nscoord
nsHTMLScrollFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  nscoord result = mHelper.mScrolledFrame->GetPrefISize(aRenderingContext);
  DISPLAY_PREF_WIDTH(this, result);
  return NSCoordSaturatingAdd(result, GetIntrinsicVScrollbarWidth(aRenderingContext));
}

nsresult
nsHTMLScrollFrame::GetPadding(nsMargin& aMargin)
{
  // Our padding hangs out on the inside of the scrollframe, but XUL doesn't
  // reaize that.  If we're stuck inside a XUL box, we need to claim no
  // padding.
  // @see also nsXULScrollFrame::GetPadding.
  aMargin.SizeTo(0,0,0,0);
  return NS_OK;
}

bool
nsHTMLScrollFrame::IsCollapsed()
{
  // We're never collapsed in the box sense.
  return false;
}

// Return the <browser> if the scrollframe is for the root frame directly
// inside a <browser>.
static nsIContent*
GetBrowserRoot(nsIContent* aContent)
{
  if (aContent) {
    nsIDocument* doc = aContent->GetCurrentDoc();
    nsPIDOMWindow* win = doc->GetWindow();
    if (win) {
      nsCOMPtr<Element> frameElement = win->GetFrameElementInternal();
      if (frameElement &&
          frameElement->NodeInfo()->Equals(nsGkAtoms::browser, kNameSpaceID_XUL))
        return frameElement;
    }
  }

  return nullptr;
}

void
nsHTMLScrollFrame::Reflow(nsPresContext*           aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLScrollFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  mHelper.HandleScrollbarStyleSwitching();

  ScrollReflowState state(this, aReflowState);
  // sanity check: ensure that if we have no scrollbar, we treat it
  // as hidden.
  if (!mHelper.mVScrollbarBox || mHelper.mNeverHasVerticalScrollbar)
    state.mStyles.mVertical = NS_STYLE_OVERFLOW_HIDDEN;
  if (!mHelper.mHScrollbarBox || mHelper.mNeverHasHorizontalScrollbar)
    state.mStyles.mHorizontal = NS_STYLE_OVERFLOW_HIDDEN;

  //------------ Handle Incremental Reflow -----------------
  bool reflowHScrollbar = true;
  bool reflowVScrollbar = true;
  bool reflowScrollCorner = true;
  if (!aReflowState.ShouldReflowAllKids()) {
    #define NEEDS_REFLOW(frame_) \
      ((frame_) && NS_SUBTREE_DIRTY(frame_))

    reflowHScrollbar = NEEDS_REFLOW(mHelper.mHScrollbarBox);
    reflowVScrollbar = NEEDS_REFLOW(mHelper.mVScrollbarBox);
    reflowScrollCorner = NEEDS_REFLOW(mHelper.mScrollCornerBox) ||
                         NEEDS_REFLOW(mHelper.mResizerBox);

    #undef NEEDS_REFLOW
  }

  if (mHelper.mIsRoot) {
    mHelper.mCollapsedResizer = true;

    nsIContent* browserRoot = GetBrowserRoot(mContent);
    if (browserRoot) {
      bool showResizer = browserRoot->HasAttr(kNameSpaceID_None, nsGkAtoms::showresizer);
      reflowScrollCorner = showResizer == mHelper.mCollapsedResizer;
      mHelper.mCollapsedResizer = !showResizer;
    }
  }

  nsRect oldScrollAreaBounds = mHelper.mScrollPort;
  nsRect oldScrolledAreaBounds =
    mHelper.mScrolledFrame->GetScrollableOverflowRectRelativeToParent();
  nsPoint oldScrollPosition = mHelper.GetScrollPosition();

  state.mComputedBorder = aReflowState.ComputedPhysicalBorderPadding() -
    aReflowState.ComputedPhysicalPadding();

  ReflowContents(&state, aDesiredSize);

  // Restore the old scroll position, for now, even if that's not valid anymore
  // because we changed size. We'll fix it up in a post-reflow callback, because
  // our current size may only be temporary (e.g. we're compute XUL desired sizes).
  PlaceScrollArea(state, oldScrollPosition);
  if (!mHelper.mPostedReflowCallback) {
    // Make sure we'll try scrolling to restored position
    PresContext()->PresShell()->PostReflowCallback(&mHelper);
    mHelper.mPostedReflowCallback = true;
  }

  bool didHaveHScrollbar = mHelper.mHasHorizontalScrollbar;
  bool didHaveVScrollbar = mHelper.mHasVerticalScrollbar;
  mHelper.mHasHorizontalScrollbar = state.mShowHScrollbar;
  mHelper.mHasVerticalScrollbar = state.mShowVScrollbar;
  nsRect newScrollAreaBounds = mHelper.mScrollPort;
  nsRect newScrolledAreaBounds =
    mHelper.mScrolledFrame->GetScrollableOverflowRectRelativeToParent();
  if (mHelper.mSkippedScrollbarLayout ||
      reflowHScrollbar || reflowVScrollbar || reflowScrollCorner ||
      (GetStateBits() & NS_FRAME_IS_DIRTY) ||
      didHaveHScrollbar != state.mShowHScrollbar ||
      didHaveVScrollbar != state.mShowVScrollbar ||
      !oldScrollAreaBounds.IsEqualEdges(newScrollAreaBounds) ||
      !oldScrolledAreaBounds.IsEqualEdges(newScrolledAreaBounds)) {
    if (!mHelper.mSupppressScrollbarUpdate) {
      mHelper.mSkippedScrollbarLayout = false;
      mHelper.SetScrollbarVisibility(mHelper.mHScrollbarBox, state.mShowHScrollbar);
      mHelper.SetScrollbarVisibility(mHelper.mVScrollbarBox, state.mShowVScrollbar);
      // place and reflow scrollbars
      nsRect insideBorderArea =
        nsRect(nsPoint(state.mComputedBorder.left, state.mComputedBorder.top),
               state.mInsideBorderSize);
      mHelper.LayoutScrollbars(state.mBoxState, insideBorderArea,
                              oldScrollAreaBounds);
    } else {
      mHelper.mSkippedScrollbarLayout = true;
    }
  }

  aDesiredSize.Width() = state.mInsideBorderSize.width +
    state.mComputedBorder.LeftRight();
  aDesiredSize.Height() = state.mInsideBorderSize.height +
    state.mComputedBorder.TopBottom();

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  if (mHelper.IsIgnoringViewportClipping()) {
    aDesiredSize.mOverflowAreas.UnionWith(
      state.mContentsOverflowAreas + mHelper.mScrolledFrame->GetPosition());
  }

  mHelper.UpdateSticky();
  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowState, aStatus);

  if (!InInitialReflow() && !mHelper.mHadNonInitialReflow) {
    mHelper.mHadNonInitialReflow = true;
  }

  if (mHelper.mIsRoot && !oldScrolledAreaBounds.IsEqualEdges(newScrolledAreaBounds)) {
    mHelper.PostScrolledAreaEvent();
  }

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  mHelper.PostOverflowEvent();
}


////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_FRAME_DUMP
nsresult
nsHTMLScrollFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("HTMLScroll"), aResult);
}
#endif

#ifdef ACCESSIBILITY
a11y::AccType
nsHTMLScrollFrame::AccessibleType()
{
  // Create an accessible regardless of focusable state because the state can be
  // changed during frame life cycle without any notifications to accessibility.
  if (mContent->IsRootOfNativeAnonymousSubtree() ||
      GetScrollbarStyles() ==
        ScrollbarStyles(NS_STYLE_OVERFLOW_HIDDEN, NS_STYLE_OVERFLOW_HIDDEN) ) {
    return a11y::eNoType;
  }

  return a11y::eHyperTextType;
}
#endif

NS_QUERYFRAME_HEAD(nsHTMLScrollFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsIScrollableFrame)
  NS_QUERYFRAME_ENTRY(nsIStatefulFrame)
  NS_QUERYFRAME_ENTRY(nsIScrollbarMediator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

//----------nsXULScrollFrame-------------------------------------------

nsXULScrollFrame*
NS_NewXULScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext,
                     bool aIsRoot, bool aClipAllDescendants)
{
  return new (aPresShell) nsXULScrollFrame(aPresShell, aContext, aIsRoot,
                                           aClipAllDescendants);
}

NS_IMPL_FRAMEARENA_HELPERS(nsXULScrollFrame)

nsXULScrollFrame::nsXULScrollFrame(nsIPresShell* aShell, nsStyleContext* aContext,
                                   bool aIsRoot, bool aClipAllDescendants)
  : nsBoxFrame(aShell, aContext, aIsRoot),
    mHelper(ALLOW_THIS_IN_INITIALIZER_LIST(this), aIsRoot)
{
  SetLayoutManager(nullptr);
  mHelper.mClipAllDescendants = aClipAllDescendants;
}

void
nsXULScrollFrame::ScrollbarActivityStarted() const
{
  if (mHelper.mScrollbarActivity) {
    mHelper.mScrollbarActivity->ActivityStarted();
  }
}

void
nsXULScrollFrame::ScrollbarActivityStopped() const
{
  if (mHelper.mScrollbarActivity) {
    mHelper.mScrollbarActivity->ActivityStopped();
  }
}

nsMargin
ScrollFrameHelper::GetDesiredScrollbarSizes(nsBoxLayoutState* aState)
{
  NS_ASSERTION(aState && aState->GetRenderingContext(),
               "Must have rendering context in layout state for size "
               "computations");

  nsMargin result(0, 0, 0, 0);

  if (mVScrollbarBox) {
    nsSize size = mVScrollbarBox->GetPrefSize(*aState);
    nsBox::AddMargin(mVScrollbarBox, size);
    if (IsScrollbarOnRight())
      result.left = size.width;
    else
      result.right = size.width;
  }

  if (mHScrollbarBox) {
    nsSize size = mHScrollbarBox->GetPrefSize(*aState);
    nsBox::AddMargin(mHScrollbarBox, size);
    // We don't currently support any scripts that would require a scrollbar
    // at the top. (Are there any?)
    result.bottom = size.height;
  }

  return result;
}

nscoord
ScrollFrameHelper::GetNondisappearingScrollbarWidth(nsBoxLayoutState* aState)
{
  NS_ASSERTION(aState && aState->GetRenderingContext(),
               "Must have rendering context in layout state for size "
               "computations");

  if (LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
    // We're using overlay scrollbars, so we need to get the width that
    // non-disappearing scrollbars would have.
    nsITheme* theme = aState->PresContext()->GetTheme();
    if (theme &&
        theme->ThemeSupportsWidget(aState->PresContext(),
                                   mVScrollbarBox,
                                   NS_THEME_SCROLLBAR_NON_DISAPPEARING)) {
      nsIntSize size;
      bool canOverride = true;
      theme->GetMinimumWidgetSize(aState->PresContext(),
                                  mVScrollbarBox,
                                  NS_THEME_SCROLLBAR_NON_DISAPPEARING,
                                  &size,
                                  &canOverride);
      if (size.width) {
        return aState->PresContext()->DevPixelsToAppUnits(size.width);
      }
    }
  }

  return GetDesiredScrollbarSizes(aState).LeftRight();
}

void
ScrollFrameHelper::HandleScrollbarStyleSwitching()
{
  // Check if we switched between scrollbar styles.
  if (mScrollbarActivity &&
      LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) == 0) {
    mScrollbarActivity->Destroy();
    mScrollbarActivity = nullptr;
    mOuter->PresContext()->ThemeChanged();
  }
  else if (!mScrollbarActivity &&
           LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
    mScrollbarActivity = new ScrollbarActivity(do_QueryFrame(mOuter));
    mOuter->PresContext()->ThemeChanged();
  }
}

static bool IsFocused(nsIContent* aContent)
{
  // Some content elements, like the GetContent() of a scroll frame
  // for a text input field, are inside anonymous subtrees, but the focus
  // manager always reports a non-anonymous element as the focused one, so
  // walk up the tree until we reach a non-anonymous element.
  while (aContent && aContent->IsInAnonymousSubtree()) {
    aContent = aContent->GetParent();
  }

  return aContent ? nsContentUtils::IsFocusedContent(aContent) : false;
}

bool
ScrollFrameHelper::WantAsyncScroll() const
{
  nsRect scrollRange = GetScrollRange();
  ScrollbarStyles styles = GetScrollbarStylesFromFrame();
  bool isFocused = IsFocused(mOuter->GetContent());
  bool isVScrollable = (scrollRange.height > 0)
                    && (styles.mVertical != NS_STYLE_OVERFLOW_HIDDEN);
  bool isHScrollable = (scrollRange.width > 0)
                    && (styles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN);
  // The check for scroll bars was added in bug 825692 to prevent layerization
  // of text inputs for performance reasons. However, if a text input is
  // focused we want to layerize it so we can async scroll it (bug 946408).
  bool isVAsyncScrollable = isVScrollable && (mVScrollbarBox || isFocused);
  bool isHAsyncScrollable = isHScrollable && (mHScrollbarBox || isFocused);
  return isVAsyncScrollable || isHAsyncScrollable;
}

static nsRect
GetOnePixelRangeAroundPoint(nsPoint aPoint, bool aIsHorizontal)
{
  nsRect allowedRange(aPoint, nsSize());
  nscoord halfPixel = nsPresContext::CSSPixelsToAppUnits(0.5f);
  if (aIsHorizontal) {
    allowedRange.x = aPoint.x - halfPixel;
    allowedRange.width = halfPixel*2 - 1;
  } else {
    allowedRange.y = aPoint.y - halfPixel;
    allowedRange.height = halfPixel*2 - 1;
  }
  return allowedRange;
}

void
ScrollFrameHelper::ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection)
{
  ScrollByUnit(aScrollbar, nsIScrollableFrame::SMOOTH, aDirection,
               nsIScrollableFrame::PAGES);
}

void
ScrollFrameHelper::ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection)
{
  ScrollByUnit(aScrollbar, nsIScrollableFrame::INSTANT, aDirection,
               nsIScrollableFrame::WHOLE);
}

void
ScrollFrameHelper::ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection)
{
  bool isHorizontal = aScrollbar->IsHorizontal();
  nsIntPoint delta;
  if (isHorizontal) {
    const double kScrollMultiplier =
      Preferences::GetInt("toolkit.scrollbox.verticalScrollDistance",
                          NS_DEFAULT_HORIZONTAL_SCROLL_DISTANCE);
    delta.x = aDirection * kScrollMultiplier;
  } else {
    const double kScrollMultiplier =
      Preferences::GetInt("toolkit.scrollbox.horizontalScrollDistance",
                          NS_DEFAULT_VERTICAL_SCROLL_DISTANCE);
    delta.y = aDirection * kScrollMultiplier;
  }
  nsIntPoint overflow;
  ScrollBy(delta, nsIScrollableFrame::LINES, nsIScrollableFrame::SMOOTH,
           &overflow, nsGkAtoms::other);
}

void
ScrollFrameHelper::RepeatButtonScroll(nsScrollbarFrame* aScrollbar)
{
  aScrollbar->MoveToNewPosition();
}

void
ScrollFrameHelper::ThumbMoved(nsScrollbarFrame* aScrollbar,
                              nscoord aOldPos,
                              nscoord aNewPos)
{
  MOZ_ASSERT(aScrollbar != nullptr);
  bool isHorizontal = aScrollbar->IsHorizontal();
  nsPoint current = GetScrollPosition();
  nsPoint dest = current;
  if (isHorizontal) {
    dest.x = aNewPos;
  } else {
    dest.y = aNewPos;
  }
  nsRect allowedRange = GetOnePixelRangeAroundPoint(dest, isHorizontal);

  // Don't try to scroll if we're already at an acceptable place.
  // Don't call Contains here since Contains returns false when the point is
  // on the bottom or right edge of the rectangle.
  if (allowedRange.ClampPoint(current) == current) {
    return;
  }

  ScrollTo(dest, nsIScrollableFrame::INSTANT, &allowedRange);
}

void
ScrollFrameHelper::ScrollByUnit(nsScrollbarFrame* aScrollbar,
                                nsIScrollableFrame::ScrollMode aMode,
                                int32_t aDirection,
                                nsIScrollableFrame::ScrollUnit aUnit)
{
  MOZ_ASSERT(aScrollbar != nullptr);
  bool isHorizontal = aScrollbar->IsHorizontal();
  nsIntPoint delta;
  if (isHorizontal) {
    delta.x = aDirection;
  } else {
    delta.y = aDirection;
  }
  nsIntPoint overflow;
  ScrollBy(delta, aUnit, aMode, &overflow, nsGkAtoms::other);
}

nsresult
nsXULScrollFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  return mHelper.CreateAnonymousContent(aElements);
}

void
nsXULScrollFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                           uint32_t aFilter)
{
  mHelper.AppendAnonymousContentTo(aElements, aFilter);
}

void
nsXULScrollFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  mHelper.Destroy();
  nsBoxFrame::DestroyFrom(aDestructRoot);
}

void
nsXULScrollFrame::SetInitialChildList(ChildListID     aListID,
                                      nsFrameList&    aChildList)
{
  nsBoxFrame::SetInitialChildList(aListID, aChildList);
  mHelper.ReloadChildFrames();
}


void
nsXULScrollFrame::AppendFrames(ChildListID     aListID,
                               nsFrameList&    aFrameList)
{
  nsBoxFrame::AppendFrames(aListID, aFrameList);
  mHelper.ReloadChildFrames();
}

void
nsXULScrollFrame::InsertFrames(ChildListID     aListID,
                               nsIFrame*       aPrevFrame,
                               nsFrameList&    aFrameList)
{
  nsBoxFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
  mHelper.ReloadChildFrames();
}

void
nsXULScrollFrame::RemoveFrame(ChildListID     aListID,
                              nsIFrame*       aOldFrame)
{
  nsBoxFrame::RemoveFrame(aListID, aOldFrame);
  mHelper.ReloadChildFrames();
}

nsSplittableType
nsXULScrollFrame::GetSplittableType() const
{
  return NS_FRAME_NOT_SPLITTABLE;
}

nsresult
nsXULScrollFrame::GetPadding(nsMargin& aMargin)
{
  aMargin.SizeTo(0,0,0,0);
  return NS_OK;
}

nsIAtom*
nsXULScrollFrame::GetType() const
{
  return nsGkAtoms::scrollFrame;
}

nscoord
nsXULScrollFrame::GetBoxAscent(nsBoxLayoutState& aState)
{
  if (!mHelper.mScrolledFrame)
    return 0;

  nscoord ascent = mHelper.mScrolledFrame->GetBoxAscent(aState);
  nsMargin m(0,0,0,0);
  GetBorderAndPadding(m);
  ascent += m.top;
  GetMargin(m);
  ascent += m.top;

  return ascent;
}

nsSize
nsXULScrollFrame::GetPrefSize(nsBoxLayoutState& aState)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsSize pref = mHelper.mScrolledFrame->GetPrefSize(aState);

  ScrollbarStyles styles = GetScrollbarStyles();

  // scrolled frames don't have their own margins

  if (mHelper.mVScrollbarBox &&
      styles.mVertical == NS_STYLE_OVERFLOW_SCROLL) {
    nsSize vSize = mHelper.mVScrollbarBox->GetPrefSize(aState);
    nsBox::AddMargin(mHelper.mVScrollbarBox, vSize);
    pref.width += vSize.width;
  }

  if (mHelper.mHScrollbarBox &&
      styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
    nsSize hSize = mHelper.mHScrollbarBox->GetPrefSize(aState);
    nsBox::AddMargin(mHelper.mHScrollbarBox, hSize);
    pref.height += hSize.height;
  }

  AddBorderAndPadding(pref);
  bool widthSet, heightSet;
  nsIFrame::AddCSSPrefSize(this, pref, widthSet, heightSet);
  return pref;
}

nsSize
nsXULScrollFrame::GetMinSize(nsBoxLayoutState& aState)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsSize min = mHelper.mScrolledFrame->GetMinSizeForScrollArea(aState);

  ScrollbarStyles styles = GetScrollbarStyles();

  if (mHelper.mVScrollbarBox &&
      styles.mVertical == NS_STYLE_OVERFLOW_SCROLL) {
     nsSize vSize = mHelper.mVScrollbarBox->GetMinSize(aState);
     AddMargin(mHelper.mVScrollbarBox, vSize);
     min.width += vSize.width;
     if (min.height < vSize.height)
        min.height = vSize.height;
  }

  if (mHelper.mHScrollbarBox &&
      styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
     nsSize hSize = mHelper.mHScrollbarBox->GetMinSize(aState);
     AddMargin(mHelper.mHScrollbarBox, hSize);
     min.height += hSize.height;
     if (min.width < hSize.width)
        min.width = hSize.width;
  }

  AddBorderAndPadding(min);
  bool widthSet, heightSet;
  nsIFrame::AddCSSMinSize(aState, this, min, widthSet, heightSet);
  return min;
}

nsSize
nsXULScrollFrame::GetMaxSize(nsBoxLayoutState& aState)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsSize maxSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE);

  AddBorderAndPadding(maxSize);
  bool widthSet, heightSet;
  nsIFrame::AddCSSMaxSize(this, maxSize, widthSet, heightSet);
  return maxSize;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsXULScrollFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("XULScroll"), aResult);
}
#endif

NS_IMETHODIMP
nsXULScrollFrame::DoLayout(nsBoxLayoutState& aState)
{
  uint32_t flags = aState.LayoutFlags();
  nsresult rv = Layout(aState);
  aState.SetLayoutFlags(flags);

  nsBox::DoLayout(aState);
  return rv;
}

NS_QUERYFRAME_HEAD(nsXULScrollFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsIScrollableFrame)
  NS_QUERYFRAME_ENTRY(nsIStatefulFrame)
  NS_QUERYFRAME_ENTRY(nsIScrollbarMediator)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

//-------------------- Helper ----------------------

#define SMOOTH_SCROLL_PREF_NAME "general.smoothScroll"

const double kCurrentVelocityWeighting = 0.25;
const double kStopDecelerationWeighting = 0.4;

// AsyncSmoothMSDScroll has ref counting.
class ScrollFrameHelper::AsyncSmoothMSDScroll MOZ_FINAL : public nsARefreshObserver {
public:
  AsyncSmoothMSDScroll(const nsPoint &aInitialPosition,
                       const nsPoint &aInitialDestination,
                       const nsSize &aInitialVelocity,
                       const nsRect &aRange,
                       const mozilla::TimeStamp &aStartTime)
    : mXAxisModel(aInitialPosition.x, aInitialDestination.x,
                  aInitialVelocity.width,
                  gfxPrefs::ScrollBehaviorSpringConstant(),
                  gfxPrefs::ScrollBehaviorDampingRatio())
    , mYAxisModel(aInitialPosition.y, aInitialDestination.y,
                  aInitialVelocity.height,
                  gfxPrefs::ScrollBehaviorSpringConstant(),
                  gfxPrefs::ScrollBehaviorDampingRatio())
    , mRange(aRange)
    , mLastRefreshTime(aStartTime)
    , mCallee(nullptr)
  {
  }

  NS_INLINE_DECL_REFCOUNTING(AsyncSmoothMSDScroll)

  nsSize GetVelocity() {
    // In nscoords per second
    return nsSize(mXAxisModel.GetVelocity(), mYAxisModel.GetVelocity());
  }

  nsPoint GetPosition() {
    // In nscoords
    return nsPoint(NSToCoordRound(mXAxisModel.GetPosition()), NSToCoordRound(mYAxisModel.GetPosition()));
  }

  void SetDestination(const nsPoint &aDestination) {
    mXAxisModel.SetDestination(static_cast<int32_t>(aDestination.x));
    mYAxisModel.SetDestination(static_cast<int32_t>(aDestination.y));
  }

  void SetRange(const nsRect &aRange)
  {
    mRange = aRange;
  }

  nsRect GetRange()
  {
    return mRange;
  }

  void Simulate(const TimeDuration& aDeltaTime)
  {
    mXAxisModel.Simulate(aDeltaTime);
    mYAxisModel.Simulate(aDeltaTime);

    nsPoint desired = GetPosition();
    nsPoint clamped = mRange.ClampPoint(desired);
    if(desired.x != clamped.x) {
      // The scroll has hit the "wall" at the left or right edge of the allowed
      // scroll range.
      // Absorb the impact to avoid bounceback effect.
      mXAxisModel.SetVelocity(0.0);
      mXAxisModel.SetPosition(clamped.x);
    }

    if(desired.y != clamped.y) {
      // The scroll has hit the "wall" at the left or right edge of the allowed
      // scroll range.
      // Absorb the impact to avoid bounceback effect.
      mYAxisModel.SetVelocity(0.0);
      mYAxisModel.SetPosition(clamped.y);
    }
  }

  bool IsFinished()
  {
    return mXAxisModel.IsFinished() && mYAxisModel.IsFinished();
  }

  virtual void WillRefresh(mozilla::TimeStamp aTime) MOZ_OVERRIDE {
    mozilla::TimeDuration deltaTime = aTime - mLastRefreshTime;
    mLastRefreshTime = aTime;

    // The callback may release "this".
    // We don't access members after returning, so no need for KungFuDeathGrip.
    ScrollFrameHelper::AsyncSmoothMSDScrollCallback(mCallee, deltaTime);
  }

  /*
   * Set a refresh observer for smooth scroll iterations (and start observing).
   * Should be used at most once during the lifetime of this object.
   * Return value: true on success, false otherwise.
   */
  bool SetRefreshObserver(ScrollFrameHelper *aCallee) {
    NS_ASSERTION(aCallee && !mCallee, "AsyncSmoothMSDScroll::SetRefreshObserver - Invalid usage.");

    if (!RefreshDriver(aCallee)->AddRefreshObserver(this, Flush_Style)) {
      return false;
    }

    mCallee = aCallee;
    return true;
  }

private:
  // Private destructor, to discourage deletion outside of Release():
  ~AsyncSmoothMSDScroll() {
    RemoveObserver();
  }

  nsRefreshDriver* RefreshDriver(ScrollFrameHelper* aCallee) {
    return aCallee->mOuter->PresContext()->RefreshDriver();
  }

  /*
   * The refresh driver doesn't hold a reference to its observers,
   *   so releasing this object can (and is) used to remove the observer on DTOR.
   * Currently, this object is released once the scrolling ends.
   */
  void RemoveObserver() {
    if (mCallee) {
      RefreshDriver(mCallee)->RemoveRefreshObserver(this, Flush_Style);
    }
  }

  mozilla::layers::AxisPhysicsMSDModel mXAxisModel, mYAxisModel;
  nsRect mRange;
  mozilla::TimeStamp mLastRefreshTime;
  ScrollFrameHelper *mCallee;
};

// AsyncScroll has ref counting.
class ScrollFrameHelper::AsyncScroll MOZ_FINAL : public nsARefreshObserver {
public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  explicit AsyncScroll(nsPoint aStartPos)
    : mIsFirstIteration(true)
    , mStartPos(aStartPos)
    , mCallee(nullptr)
  {}

private:
  // Private destructor, to discourage deletion outside of Release():
  ~AsyncScroll() {
    RemoveObserver();
  }

public:
  nsPoint PositionAt(TimeStamp aTime);
  nsSize VelocityAt(TimeStamp aTime); // In nscoords per second

  void InitSmoothScroll(TimeStamp aTime, nsPoint aDestination,
                        nsIAtom *aOrigin, const nsRect& aRange,
                        const nsSize& aCurrentVelocity);
  void Init(const nsRect& aRange) {
    mRange = aRange;
  }

  bool IsFinished(TimeStamp aTime) {
    return aTime > mStartTime + mDuration; // XXX or if we've hit the wall
  }

  TimeStamp mStartTime;

  // mPrevEventTime holds previous 3 timestamps for intervals averaging (to
  // reduce duration fluctuations). When AsyncScroll is constructed and no
  // previous timestamps are available (indicated with mIsFirstIteration),
  // initialize mPrevEventTime using imaginary previous timestamps with maximum
  // relevant intervals between them.
  TimeStamp mPrevEventTime[3];
  bool mIsFirstIteration;

  // Cached Preferences values to avoid re-reading them when extending an existing
  // animation for the same event origin (can be as frequent as every 10(!)ms for
  // a quick roll of the mouse wheel).
  // These values are minimum and maximum animation duration per event origin,
  // and a global ratio which defines how longer is the animation's duration
  // compared to the average recent events intervals (such that for a relatively
  // consistent events rate, the next event arrives before current animation ends)
  nsCOMPtr<nsIAtom> mOrigin;
  int32_t mOriginMinMS;
  int32_t mOriginMaxMS;
  double  mIntervalRatio;

  TimeDuration mDuration;
  nsPoint mStartPos;
  nsPoint mDestination;
  // Allowed destination positions around mDestination
  nsRect mRange;
  nsSMILKeySpline mTimingFunctionX;
  nsSMILKeySpline mTimingFunctionY;
  bool mIsSmoothScroll;

protected:
  double ProgressAt(TimeStamp aTime) {
    return clamped((aTime - mStartTime) / mDuration, 0.0, 1.0);
  }

  nscoord VelocityComponent(double aTimeProgress,
                            nsSMILKeySpline& aTimingFunction,
                            nscoord aStart, nscoord aDestination);

  // Initializes the timing function in such a way that the current velocity is
  // preserved.
  void InitTimingFunction(nsSMILKeySpline& aTimingFunction,
                          nscoord aCurrentPos, nscoord aCurrentVelocity,
                          nscoord aDestination);

  TimeDuration CalcDurationForEventTime(TimeStamp aTime, nsIAtom *aOrigin);

// The next section is observer/callback management
// Bodies of WillRefresh and RefreshDriver contain ScrollFrameHelper specific code.
public:
  NS_INLINE_DECL_REFCOUNTING(AsyncScroll)

  /*
   * Set a refresh observer for smooth scroll iterations (and start observing).
   * Should be used at most once during the lifetime of this object.
   * Return value: true on success, false otherwise.
   */
  bool SetRefreshObserver(ScrollFrameHelper *aCallee) {
    NS_ASSERTION(aCallee && !mCallee, "AsyncScroll::SetRefreshObserver - Invalid usage.");

    if (!RefreshDriver(aCallee)->AddRefreshObserver(this, Flush_Style)) {
      return false;
    }

    mCallee = aCallee;
    return true;
  }

  virtual void WillRefresh(mozilla::TimeStamp aTime) MOZ_OVERRIDE {
    // The callback may release "this".
    // We don't access members after returning, so no need for KungFuDeathGrip.
    ScrollFrameHelper::AsyncScrollCallback(mCallee, aTime);
  }

private:
  ScrollFrameHelper *mCallee;

  nsRefreshDriver* RefreshDriver(ScrollFrameHelper* aCallee) {
    return aCallee->mOuter->PresContext()->RefreshDriver();
  }

  /*
   * The refresh driver doesn't hold a reference to its observers,
   *   so releasing this object can (and is) used to remove the observer on DTOR.
   * Currently, this object is released once the scrolling ends.
   */
  void RemoveObserver() {
    if (mCallee) {
      RefreshDriver(mCallee)->RemoveRefreshObserver(this, Flush_Style);
    }
  }
};

nsPoint
ScrollFrameHelper::AsyncScroll::PositionAt(TimeStamp aTime) {
  double progressX = mTimingFunctionX.GetSplineValue(ProgressAt(aTime));
  double progressY = mTimingFunctionY.GetSplineValue(ProgressAt(aTime));
  return nsPoint(NSToCoordRound((1 - progressX) * mStartPos.x + progressX * mDestination.x),
                 NSToCoordRound((1 - progressY) * mStartPos.y + progressY * mDestination.y));
}

nsSize
ScrollFrameHelper::AsyncScroll::VelocityAt(TimeStamp aTime) {
  double timeProgress = ProgressAt(aTime);
  return nsSize(VelocityComponent(timeProgress, mTimingFunctionX,
                                  mStartPos.x, mDestination.x),
                VelocityComponent(timeProgress, mTimingFunctionY,
                                  mStartPos.y, mDestination.y));
}

/*
 * Calculate duration, possibly dynamically according to events rate and event origin.
 * (also maintain previous timestamps - which are only used here).
 */
TimeDuration
ScrollFrameHelper::
AsyncScroll::CalcDurationForEventTime(TimeStamp aTime, nsIAtom *aOrigin) {
  if (!aOrigin){
    aOrigin = nsGkAtoms::other;
  }

  // Read preferences only on first iteration or for a different event origin.
  if (mIsFirstIteration || aOrigin != mOrigin) {
    mOrigin = aOrigin;
    mOriginMinMS = mOriginMaxMS = 0;
    bool isOriginSmoothnessEnabled = false;
    mIntervalRatio = 1;

    // Default values for all preferences are defined in all.js
    static const int32_t kDefaultMinMS = 150, kDefaultMaxMS = 150;
    static const bool kDefaultIsSmoothEnabled = true;

    nsAutoCString originName;
    aOrigin->ToUTF8String(originName);
    nsAutoCString prefBase = NS_LITERAL_CSTRING("general.smoothScroll.") + originName;

    isOriginSmoothnessEnabled = Preferences::GetBool(prefBase.get(), kDefaultIsSmoothEnabled);
    if (isOriginSmoothnessEnabled) {
      nsAutoCString prefMin = prefBase + NS_LITERAL_CSTRING(".durationMinMS");
      nsAutoCString prefMax = prefBase + NS_LITERAL_CSTRING(".durationMaxMS");
      mOriginMinMS = Preferences::GetInt(prefMin.get(), kDefaultMinMS);
      mOriginMaxMS = Preferences::GetInt(prefMax.get(), kDefaultMaxMS);

      static const int32_t kSmoothScrollMaxAllowedAnimationDurationMS = 10000;
      mOriginMaxMS = clamped(mOriginMaxMS, 0, kSmoothScrollMaxAllowedAnimationDurationMS);
      mOriginMinMS = clamped(mOriginMinMS, 0, mOriginMaxMS);
    }

    // Keep the animation duration longer than the average event intervals
    //   (to "connect" consecutive scroll animations before the scroll comes to a stop).
    static const double kDefaultDurationToIntervalRatio = 2; // Duplicated at all.js
    mIntervalRatio = Preferences::GetInt("general.smoothScroll.durationToIntervalRatio",
                                         kDefaultDurationToIntervalRatio * 100) / 100.0;

    // Duration should be at least as long as the intervals -> ratio is at least 1
    mIntervalRatio = std::max(1.0, mIntervalRatio);

    if (mIsFirstIteration) {
      // Starting a new scroll (i.e. not when extending an existing scroll animation),
      //   create imaginary prev timestamps with maximum relevant intervals between them.

      // Longest relevant interval (which results in maximum duration)
      TimeDuration maxDelta = TimeDuration::FromMilliseconds(mOriginMaxMS / mIntervalRatio);
      mPrevEventTime[0] = aTime              - maxDelta;
      mPrevEventTime[1] = mPrevEventTime[0]  - maxDelta;
      mPrevEventTime[2] = mPrevEventTime[1]  - maxDelta;
    }
  }

  // Average last 3 delta durations (rounding errors up to 2ms are negligible for us)
  int32_t eventsDeltaMs = (aTime - mPrevEventTime[2]).ToMilliseconds() / 3;
  mPrevEventTime[2] = mPrevEventTime[1];
  mPrevEventTime[1] = mPrevEventTime[0];
  mPrevEventTime[0] = aTime;

  // Modulate duration according to events rate (quicker events -> shorter durations).
  // The desired effect is to use longer duration when scrolling slowly, such that
  // it's easier to follow, but reduce the duration to make it feel more snappy when
  // scrolling quickly. To reduce fluctuations of the duration, we average event
  // intervals using the recent 4 timestamps (now + three prev -> 3 intervals).
  int32_t durationMS = clamped<int32_t>(eventsDeltaMs * mIntervalRatio, mOriginMinMS, mOriginMaxMS);

  return TimeDuration::FromMilliseconds(durationMS);
}

void
ScrollFrameHelper::AsyncScroll::InitSmoothScroll(TimeStamp aTime,
                                                     nsPoint aDestination,
                                                     nsIAtom *aOrigin,
                                                     const nsRect& aRange,
                                                     const nsSize& aCurrentVelocity) {
  mRange = aRange;
  TimeDuration duration = CalcDurationForEventTime(aTime, aOrigin);
  nsSize currentVelocity = aCurrentVelocity;
  if (!mIsFirstIteration) {
    // If an additional event has not changed the destination, then do not let
    // another minimum duration reset slow things down.  If it would then
    // instead continue with the existing timing function.
    if (aDestination == mDestination &&
        aTime + duration > mStartTime + mDuration)
      return;

    currentVelocity = VelocityAt(aTime);
    mStartPos = PositionAt(aTime);
  }
  mStartTime = aTime;
  mDuration = duration;
  mDestination = aDestination;
  InitTimingFunction(mTimingFunctionX, mStartPos.x, currentVelocity.width,
                     aDestination.x);
  InitTimingFunction(mTimingFunctionY, mStartPos.y, currentVelocity.height,
                     aDestination.y);
  mIsFirstIteration = false;
}


nscoord
ScrollFrameHelper::AsyncScroll::VelocityComponent(double aTimeProgress,
                                                      nsSMILKeySpline& aTimingFunction,
                                                      nscoord aStart,
                                                      nscoord aDestination)
{
  double dt, dxy;
  aTimingFunction.GetSplineDerivativeValues(aTimeProgress, dt, dxy);
  if (dt == 0)
    return dxy >= 0 ? nscoord_MAX : nscoord_MIN;

  const TimeDuration oneSecond = TimeDuration::FromSeconds(1);
  double slope = dxy / dt;
  return NSToCoordRound(slope * (aDestination - aStart) / (mDuration / oneSecond));
}

void
ScrollFrameHelper::AsyncScroll::InitTimingFunction(nsSMILKeySpline& aTimingFunction,
                                                       nscoord aCurrentPos,
                                                       nscoord aCurrentVelocity,
                                                       nscoord aDestination)
{
  if (aDestination == aCurrentPos || kCurrentVelocityWeighting == 0) {
    aTimingFunction.Init(0, 0, 1 - kStopDecelerationWeighting, 1);
    return;
  }

  const TimeDuration oneSecond = TimeDuration::FromSeconds(1);
  double slope = aCurrentVelocity * (mDuration / oneSecond) / (aDestination - aCurrentPos);
  double normalization = sqrt(1.0 + slope * slope);
  double dt = 1.0 / normalization * kCurrentVelocityWeighting;
  double dxy = slope / normalization * kCurrentVelocityWeighting;
  aTimingFunction.Init(dt, dxy, 1 - kStopDecelerationWeighting, 1);
}

static bool
IsSmoothScrollingEnabled()
{
  return Preferences::GetBool(SMOOTH_SCROLL_PREF_NAME, false);
}

class ScrollFrameActivityTracker MOZ_FINAL : public nsExpirationTracker<ScrollFrameHelper,4> {
public:
  // Wait for 3-4s between scrolls before we remove our layers.
  // That's 4 generations of 1s each.
  enum { TIMEOUT_MS = 1000 };
  ScrollFrameActivityTracker()
    : nsExpirationTracker<ScrollFrameHelper,4>(TIMEOUT_MS) {}
  ~ScrollFrameActivityTracker() {
    AgeAllGenerations();
  }

  virtual void NotifyExpired(ScrollFrameHelper *aObject) {
    RemoveObject(aObject);
    aObject->MarkInactive();
  }
};

static ScrollFrameActivityTracker *gScrollFrameActivityTracker = nullptr;

// There are situations when a scroll frame is destroyed and then re-created
// for the same content element. In this case we want to increment the scroll
// generation between the old and new scrollframes. If the new one knew about
// the old one then it could steal the old generation counter and increment it
// but it doesn't have that reference so instead we use a static global to
// ensure the new one gets a fresh value.
static uint32_t sScrollGenerationCounter = 0;

ScrollFrameHelper::ScrollFrameHelper(nsContainerFrame* aOuter,
                                             bool aIsRoot)
  : mHScrollbarBox(nullptr)
  , mVScrollbarBox(nullptr)
  , mScrolledFrame(nullptr)
  , mScrollCornerBox(nullptr)
  , mResizerBox(nullptr)
  , mOuter(aOuter)
  , mAsyncScroll(nullptr)
  , mAsyncSmoothMSDScroll(nullptr)
  , mLastScrollOrigin(nsGkAtoms::other)
  , mLastSmoothScrollOrigin(nullptr)
  , mScrollGeneration(++sScrollGenerationCounter)
  , mDestination(0, 0)
  , mScrollPosAtLastPaint(0, 0)
  , mRestorePos(-1, -1)
  , mLastPos(-1, -1)
  , mResolution(1.0, 1.0)
  , mScrollPosForLayerPixelAlignment(-1, -1)
  , mLastUpdateImagesPos(-1, -1)
  , mNeverHasVerticalScrollbar(false)
  , mNeverHasHorizontalScrollbar(false)
  , mHasVerticalScrollbar(false)
  , mHasHorizontalScrollbar(false)
  , mFrameIsUpdatingScrollbar(false)
  , mDidHistoryRestore(false)
  , mIsRoot(aIsRoot)
  , mClipAllDescendants(aIsRoot)
  , mSupppressScrollbarUpdate(false)
  , mSkippedScrollbarLayout(false)
  , mHadNonInitialReflow(false)
  , mHorizontalOverflow(false)
  , mVerticalOverflow(false)
  , mPostedReflowCallback(false)
  , mMayHaveDirtyFixedChildren(false)
  , mUpdateScrollbarAttributes(false)
  , mCollapsedResizer(false)
  , mShouldBuildScrollableLayer(false)
  , mHasBeenScrolled(false)
  , mIsResolutionSet(false)
{
  mScrollingActive = IsAlwaysActive();

  if (LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
    mScrollbarActivity = new ScrollbarActivity(do_QueryFrame(aOuter));
  }

  EnsureImageVisPrefsCached();

  if (mScrollingActive &&
      gfxPlatform::GetPlatform()->UseTiling() &&
      !nsLayoutUtils::UsesAsyncScrolling() &&
      mOuter->GetContent()) {
    // If we have tiling but no APZ, then set a 0-margin display port on
    // active scroll containers so that we paint by whole tile increments
    // when scrolling.
    nsLayoutUtils::SetDisplayPortMargins(mOuter->GetContent(),
                                         mOuter->PresContext()->PresShell(),
                                         LayerMargin(),
                                         gfxPrefs::LayersTileWidth(), gfxPrefs::LayersTileHeight(),
                                         0,
                                         nsLayoutUtils::RepaintMode::DoNotRepaint);
  }

}

ScrollFrameHelper::~ScrollFrameHelper()
{
  if (mActivityExpirationState.IsTracked()) {
    gScrollFrameActivityTracker->RemoveObject(this);
  }
  if (gScrollFrameActivityTracker &&
      gScrollFrameActivityTracker->IsEmpty()) {
    delete gScrollFrameActivityTracker;
    gScrollFrameActivityTracker = nullptr;
  }

  if (mScrollActivityTimer) {
    mScrollActivityTimer->Cancel();
    mScrollActivityTimer = nullptr;
  }
}

/*
 * Callback function from AsyncSmoothMSDScroll, used in ScrollFrameHelper::ScrollTo
 */
void
ScrollFrameHelper::AsyncSmoothMSDScrollCallback(ScrollFrameHelper* aInstance,
                                                mozilla::TimeDuration aDeltaTime)
{
  NS_ASSERTION(aInstance != nullptr, "aInstance must not be null");
  NS_ASSERTION(aInstance->mAsyncSmoothMSDScroll,
    "Did not expect AsyncSmoothMSDScrollCallback without an active MSD scroll.");

  nsRect range = aInstance->mAsyncSmoothMSDScroll->GetRange();
  aInstance->mAsyncSmoothMSDScroll->Simulate(aDeltaTime);

  if (!aInstance->mAsyncSmoothMSDScroll->IsFinished()) {
    nsPoint destination = aInstance->mAsyncSmoothMSDScroll->GetPosition();
    // Allow this scroll operation to land on any pixel boundary within the
    // allowed scroll range for this frame.
    // If the MSD is under-dampened or the destination is changed rapidly,
    // it is expected (and desired) that the scrolling may overshoot.
    nsRect intermediateRange =
      nsRect(destination, nsSize()).UnionEdges(range);
    aInstance->ScrollToImpl(destination, intermediateRange);
    // 'aInstance' might be destroyed here
    return;
  }

  aInstance->CompleteAsyncScroll(range);
}

/*
 * Callback function from AsyncScroll, used in ScrollFrameHelper::ScrollTo
 */
void
ScrollFrameHelper::AsyncScrollCallback(ScrollFrameHelper* aInstance,
                                       mozilla::TimeStamp aTime)
{
  NS_ASSERTION(aInstance != nullptr, "aInstance must not be null");
  NS_ASSERTION(aInstance->mAsyncScroll,
    "Did not expect AsyncScrollCallback without an active async scroll.");

  nsRect range = aInstance->mAsyncScroll->mRange;
  if (aInstance->mAsyncScroll->mIsSmoothScroll) {
    if (!aInstance->mAsyncScroll->IsFinished(aTime)) {
      nsPoint destination = aInstance->mAsyncScroll->PositionAt(aTime);
      // Allow this scroll operation to land on any pixel boundary between the
      // current position and the final allowed range.  (We don't want
      // intermediate steps to be more constrained than the final step!)
      nsRect intermediateRange =
        nsRect(aInstance->GetScrollPosition(), nsSize()).UnionEdges(range);
      aInstance->ScrollToImpl(destination, intermediateRange);
      // 'aInstance' might be destroyed here
      return;
    }
  }

  aInstance->CompleteAsyncScroll(range);
}

void
ScrollFrameHelper::CompleteAsyncScroll(const nsRect &aRange, nsIAtom* aOrigin)
{
  // Apply desired destination range since this is the last step of scrolling.
  mAsyncSmoothMSDScroll = nullptr;
  mAsyncScroll = nullptr;
  nsWeakFrame weakFrame(mOuter);
  ScrollToImpl(mDestination, aRange, aOrigin);
  if (!weakFrame.IsAlive()) {
    return;
  }
  // We are done scrolling, set our destination to wherever we actually ended
  // up scrolling to.
  mDestination = GetScrollPosition();
}

void
ScrollFrameHelper::ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                                     nsIScrollableFrame::ScrollMode aMode)
{
  nsPoint current = GetScrollPosition();
  CSSIntPoint currentCSSPixels = GetScrollPositionCSSPixels();
  nsPoint pt = CSSPoint::ToAppUnits(aScrollPosition);
  nscoord halfPixel = nsPresContext::CSSPixelsToAppUnits(0.5f);
  nsRect range(pt.x - halfPixel, pt.y - halfPixel, 2*halfPixel - 1, 2*halfPixel - 1);
  // XXX I don't think the following blocks are needed anymore, now that
  // ScrollToImpl simply tries to scroll an integer number of layer
  // pixels from the current position
  if (currentCSSPixels.x == aScrollPosition.x) {
    pt.x = current.x;
    range.x = pt.x;
    range.width = 0;
  }
  if (currentCSSPixels.y == aScrollPosition.y) {
    pt.y = current.y;
    range.y = pt.y;
    range.height = 0;
  }
  ScrollTo(pt, aMode, &range);
  // 'this' might be destroyed here
}

void
ScrollFrameHelper::ScrollToCSSPixelsApproximate(const CSSPoint& aScrollPosition,
                                                nsIAtom *aOrigin)
{
  nsPoint pt = CSSPoint::ToAppUnits(aScrollPosition);
  nscoord halfRange = nsPresContext::CSSPixelsToAppUnits(1000);
  nsRect range(pt.x - halfRange, pt.y - halfRange, 2*halfRange - 1, 2*halfRange - 1);
  ScrollToWithOrigin(pt, nsIScrollableFrame::INSTANT, aOrigin, &range);
  // 'this' might be destroyed here
}

CSSIntPoint
ScrollFrameHelper::GetScrollPositionCSSPixels()
{
  return CSSIntPoint::FromAppUnitsRounded(GetScrollPosition());
}

/*
 * this method wraps calls to ScrollToImpl(), either in one shot or incrementally,
 *  based on the setting of the smoothness scroll pref
 */
void
ScrollFrameHelper::ScrollToWithOrigin(nsPoint aScrollPosition,
                                          nsIScrollableFrame::ScrollMode aMode,
                                          nsIAtom *aOrigin,
                                          const nsRect* aRange)
{
  nsRect scrollRange = GetScrollRangeForClamping();
  mDestination = scrollRange.ClampPoint(aScrollPosition);

  nsRect range = aRange ? *aRange : nsRect(aScrollPosition, nsSize(0, 0));

  if (aMode == nsIScrollableFrame::INSTANT) {
    // Asynchronous scrolling is not allowed, so we'll kill any existing
    // async-scrolling process and do an instant scroll.
    CompleteAsyncScroll(range, aOrigin);
    return;
  }

  nsPresContext* presContext = mOuter->PresContext();
  TimeStamp now = presContext->RefreshDriver()->MostRecentRefresh();
  bool isSmoothScroll = (aMode == nsIScrollableFrame::SMOOTH) &&
                          IsSmoothScrollingEnabled();

  nsSize currentVelocity(0, 0);

  if (gfxPrefs::ScrollBehaviorEnabled()) {
    if (aMode == nsIScrollableFrame::SMOOTH_MSD) {
      if (!mAsyncSmoothMSDScroll) {
        if (mAsyncScroll) {
          if (mAsyncScroll->mIsSmoothScroll) {
            currentVelocity = mAsyncScroll->VelocityAt(now);
          }
          mAsyncScroll = nullptr;
        }

        if (gfxPrefs::AsyncPanZoomEnabled()) {
          // The animation will be handled in the compositor, pass the
          // information needed to start the animation and skip the main-thread
          // animation for this scroll.
          mLastSmoothScrollOrigin = aOrigin;
          mScrollGeneration = ++sScrollGenerationCounter;

          // Schedule a paint to ensure that the frame metrics get updated on
          // the compositor thread.
          mOuter->SchedulePaint();
          return;
        }

        mAsyncSmoothMSDScroll =
          new AsyncSmoothMSDScroll(GetScrollPosition(), mDestination,
                                   currentVelocity, GetScrollRangeForClamping(),
                                   now);

        if (!mAsyncSmoothMSDScroll->SetRefreshObserver(this)) {
          // Observer setup failed. Scroll the normal way.
          CompleteAsyncScroll(range, aOrigin);
          return;
        }
      } else {
        // A previous smooth MSD scroll is still in progress, so we just need to
        // update its destination.
        mAsyncSmoothMSDScroll->SetDestination(mDestination);
      }

      return;
    } else {
      if (mAsyncSmoothMSDScroll) {
        currentVelocity = mAsyncSmoothMSDScroll->GetVelocity();
        mAsyncSmoothMSDScroll = nullptr;
      }
    }
  }

  if (!mAsyncScroll) {
    mAsyncScroll = new AsyncScroll(GetScrollPosition());
    if (!mAsyncScroll->SetRefreshObserver(this)) {
      // Observer setup failed. Scroll the normal way.
      CompleteAsyncScroll(range, aOrigin);
      return;
    }
  }

  mAsyncScroll->mIsSmoothScroll = isSmoothScroll;

  if (isSmoothScroll) {
    mAsyncScroll->InitSmoothScroll(now, mDestination, aOrigin, range, currentVelocity);
  } else {
    mAsyncScroll->Init(range);
  }
}

// We can't use nsContainerFrame::PositionChildViews here because
// we don't want to invalidate views that have moved.
static void AdjustViews(nsIFrame* aFrame)
{
  nsView* view = aFrame->GetView();
  if (view) {
    nsPoint pt;
    aFrame->GetParent()->GetClosestView(&pt);
    pt += aFrame->GetPosition();
    view->SetPosition(pt.x, pt.y);

    return;
  }

  if (!(aFrame->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    return;
  }

  // Call AdjustViews recursively for all child frames except the popup list as
  // the views for popups are not scrolled.
  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    if (lists.CurrentID() == nsIFrame::kPopupList) {
      continue;
    }
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      AdjustViews(childFrames.get());
    }
  }
}

static bool
NeedToInvalidateOnScroll(nsIFrame* aFrame)
{
  return (aFrame->GetStateBits() & NS_SCROLLFRAME_INVALIDATE_CONTENTS_ON_SCROLL) != 0;
}

bool ScrollFrameHelper::IsIgnoringViewportClipping() const
{
  if (!mIsRoot)
    return false;
  nsSubDocumentFrame* subdocFrame = static_cast<nsSubDocumentFrame*>
    (nsLayoutUtils::GetCrossDocParentFrame(mOuter->PresContext()->PresShell()->GetRootFrame()));
  return subdocFrame && !subdocFrame->ShouldClipSubdocument();
}

void ScrollFrameHelper::MarkScrollbarsDirtyForReflow() const
{
  nsIPresShell* presShell = mOuter->PresContext()->PresShell();
  if (mVScrollbarBox) {
    presShell->FrameNeedsReflow(mVScrollbarBox, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
  }
  if (mHScrollbarBox) {
    presShell->FrameNeedsReflow(mHScrollbarBox, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
  }
}

bool ScrollFrameHelper::ShouldClampScrollPosition() const
{
  if (!mIsRoot)
    return true;
  nsSubDocumentFrame* subdocFrame = static_cast<nsSubDocumentFrame*>
    (nsLayoutUtils::GetCrossDocParentFrame(mOuter->PresContext()->PresShell()->GetRootFrame()));
  return !subdocFrame || subdocFrame->ShouldClampScrollPosition();
}

bool ScrollFrameHelper::IsAlwaysActive() const
{
  if (nsDisplayItem::ForceActiveLayers()) {
    return true;
  }

  const nsStyleDisplay* disp = mOuter->StyleDisplay();
  if (disp && (disp->mWillChangeBitField & NS_STYLE_WILL_CHANGE_SCROLL)) {
    return true;
  }

  // Unless this is the root scrollframe for a non-chrome document
  // which is the direct child of a chrome document, we default to not
  // being "active".
  if (!(mIsRoot && mOuter->PresContext()->IsRootContentDocument())) {
     return false;
  }

  // If we have scrolled before, then we should stay active.
  if (mHasBeenScrolled) {
    return true;
  }

  // If we're overflow:hidden, then start as inactive until
  // we get scrolled manually.
  ScrollbarStyles styles = GetScrollbarStylesFromFrame();
  return (styles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN &&
          styles.mVertical != NS_STYLE_OVERFLOW_HIDDEN);
}

void ScrollFrameHelper::MarkInactive()
{
  if (IsAlwaysActive() || !mScrollingActive)
    return;

  mScrollingActive = false;
  mOuter->SchedulePaint();
}

void ScrollFrameHelper::MarkActive()
{
  mScrollingActive = true;
  if (IsAlwaysActive())
    return;

  if (mActivityExpirationState.IsTracked()) {
    gScrollFrameActivityTracker->MarkUsed(this);
  } else {
    if (!gScrollFrameActivityTracker) {
      gScrollFrameActivityTracker = new ScrollFrameActivityTracker();
    }
    gScrollFrameActivityTracker->AddObject(this);
  }
}

void ScrollFrameHelper::ScrollVisual(nsPoint aOldScrolledFramePos)
{
  // Mark this frame as having been scrolled. If this is the root
  // scroll frame of a content document, then IsAlwaysActive()
  // will return true from now on and MarkInactive() won't
  // have any effect.
  mHasBeenScrolled = true;

  AdjustViews(mScrolledFrame);
  // We need to call this after fixing up the view positions
  // to be consistent with the frame hierarchy.
  bool needToInvalidateOnScroll = NeedToInvalidateOnScroll(mOuter);
  mOuter->RemoveStateBits(NS_SCROLLFRAME_INVALIDATE_CONTENTS_ON_SCROLL);
  if (IsScrollingActive() && needToInvalidateOnScroll) {
    MarkInactive();
  }
  if (!needToInvalidateOnScroll) {
    MarkActive();
  }

  mOuter->SchedulePaint();
}

/**
 * Clamp desired scroll position aDesired and range [aDestLower, aDestUpper]
 * to [aBoundLower, aBoundUpper] and then select the appunit value from among
 * aBoundLower, aBoundUpper and those such that (aDesired - aCurrent) *
 * aRes/aAppUnitsPerPixel is an integer (or as close as we can get
 * modulo rounding to appunits) that is in [aDestLower, aDestUpper] and
 * closest to aDesired.  If no such value exists, return the nearest in
 * [aDestLower, aDestUpper].
 */
static nscoord
ClampAndAlignWithPixels(nscoord aDesired,
                        nscoord aBoundLower, nscoord aBoundUpper,
                        nscoord aDestLower, nscoord aDestUpper,
                        nscoord aAppUnitsPerPixel, double aRes,
                        nscoord aCurrent)
{
  // Intersect scroll range with allowed range, by clamping the ends
  // of aRange to be within bounds
  nscoord destLower = clamped(aDestLower, aBoundLower, aBoundUpper);
  nscoord destUpper = clamped(aDestUpper, aBoundLower, aBoundUpper);

  nscoord desired = clamped(aDesired, destLower, destUpper);

  double currentLayerVal = (aRes*aCurrent)/aAppUnitsPerPixel;
  double desiredLayerVal = (aRes*desired)/aAppUnitsPerPixel;
  double delta = desiredLayerVal - currentLayerVal;
  double nearestLayerVal = NS_round(delta) + currentLayerVal;

  // Convert back from PaintedLayer space to appunits relative to the top-left
  // of the scrolled frame.
  nscoord aligned =
    NSToCoordRoundWithClamp(nearestLayerVal*aAppUnitsPerPixel/aRes);

  // Use a bound if it is within the allowed range and closer to desired than
  // the nearest pixel-aligned value.
  if (aBoundUpper == destUpper &&
      static_cast<decltype(Abs(desired))>(aBoundUpper - desired) <
      Abs(desired - aligned))
    return aBoundUpper;

  if (aBoundLower == destLower &&
      static_cast<decltype(Abs(desired))>(desired - aBoundLower) <
      Abs(aligned - desired))
    return aBoundLower;

  // Accept the nearest pixel-aligned value if it is within the allowed range. 
  if (aligned >= destLower && aligned <= destUpper)
    return aligned;

  // Check if opposite pixel boundary fits into allowed range.
  double oppositeLayerVal =
    nearestLayerVal + ((nearestLayerVal < desiredLayerVal) ? 1.0 : -1.0);
  nscoord opposite =
    NSToCoordRoundWithClamp(oppositeLayerVal*aAppUnitsPerPixel/aRes);
  if (opposite >= destLower && opposite <= destUpper) {
    return opposite;
  }

  // No alignment available.
  return desired;
}

/**
 * Clamp desired scroll position aPt to aBounds and then snap
 * it to the same layer pixel edges as aCurrent, keeping it within aRange
 * during snapping. aCurrent is the current scroll position.
 */
static nsPoint
ClampAndAlignWithLayerPixels(const nsPoint& aPt,
                             const nsRect& aBounds,
                             const nsRect& aRange,
                             const nsPoint& aCurrent,
                             nscoord aAppUnitsPerPixel,
                             const gfxSize& aScale)
{
  return nsPoint(ClampAndAlignWithPixels(aPt.x, aBounds.x, aBounds.XMost(),
                                         aRange.x, aRange.XMost(),
                                         aAppUnitsPerPixel, aScale.width,
                                         aCurrent.x),
                 ClampAndAlignWithPixels(aPt.y, aBounds.y, aBounds.YMost(),
                                         aRange.y, aRange.YMost(),
                                         aAppUnitsPerPixel, aScale.height,
                                         aCurrent.y));
}

/* static */ void
ScrollFrameHelper::ScrollActivityCallback(nsITimer *aTimer, void* anInstance)
{
  ScrollFrameHelper* self = static_cast<ScrollFrameHelper*>(anInstance);

  // Fire the synth mouse move.
  self->mScrollActivityTimer->Cancel();
  self->mScrollActivityTimer = nullptr;
  self->mOuter->PresContext()->PresShell()->SynthesizeMouseMove(true);
}


void
ScrollFrameHelper::ScheduleSyntheticMouseMove()
{
  if (!mScrollActivityTimer) {
    mScrollActivityTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (!mScrollActivityTimer)
      return;
  }

  mScrollActivityTimer->InitWithFuncCallback(
        ScrollActivityCallback, this, 100, nsITimer::TYPE_ONE_SHOT);
}

void
ScrollFrameHelper::ScrollToImpl(nsPoint aPt, const nsRect& aRange, nsIAtom* aOrigin)
{
  if (aOrigin == nullptr) {
    // If no origin was specified, we still want to set it to something that's
    // non-null, so that we can use nullness to distinguish if the frame was scrolled
    // at all. Default it to some generic placeholder.
    aOrigin = nsGkAtoms::other;
  }

  nsPresContext* presContext = mOuter->PresContext();
  nscoord appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  // 'scale' is our estimate of the scale factor that will be applied
  // when rendering the scrolled content to its own PaintedLayer.
  gfxSize scale = FrameLayerBuilder::GetPaintedLayerScaleForFrame(mScrolledFrame);
  nsPoint curPos = GetScrollPosition();
  nsPoint alignWithPos = mScrollPosForLayerPixelAlignment == nsPoint(-1,-1)
      ? curPos : mScrollPosForLayerPixelAlignment;
  // Try to align aPt with curPos so they have an integer number of layer
  // pixels between them. This gives us the best chance of scrolling without
  // having to invalidate due to changes in subpixel rendering.
  // Note that when we actually draw into a PaintedLayer, the coordinates
  // that get mapped onto the layer buffer pixels are from the display list,
  // which are relative to the display root frame's top-left increasing down,
  // whereas here our coordinates are scroll positions which increase upward
  // and are relative to the scrollport top-left. This difference doesn't actually
  // matter since all we are about is that there be an integer number of
  // layer pixels between pt and curPos.
  nsPoint pt =
    ClampAndAlignWithLayerPixels(aPt,
                                 GetScrollRangeForClamping(),
                                 aRange,
                                 alignWithPos,
                                 appUnitsPerDevPixel,
                                 scale);
  if (pt == curPos) {
    return;
  }

  bool needImageVisibilityUpdate = (mLastUpdateImagesPos == nsPoint(-1,-1));

  nsPoint dist(std::abs(pt.x - mLastUpdateImagesPos.x),
               std::abs(pt.y - mLastUpdateImagesPos.y));
  nsSize scrollPortSize = GetScrollPositionClampingScrollPortSize();
  nscoord horzAllowance = std::max(scrollPortSize.width / std::max(sHorzScrollFraction, 1),
                                   nsPresContext::AppUnitsPerCSSPixel());
  nscoord vertAllowance = std::max(scrollPortSize.height / std::max(sVertScrollFraction, 1),
                                   nsPresContext::AppUnitsPerCSSPixel());
  if (dist.x >= horzAllowance || dist.y >= vertAllowance) {
    needImageVisibilityUpdate = true;
  }

  if (needImageVisibilityUpdate) {
    presContext->PresShell()->ScheduleImageVisibilityUpdate();
  }

  // notify the listeners.
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->ScrollPositionWillChange(pt.x, pt.y);
  }

  nsPoint oldScrollFramePos = mScrolledFrame->GetPosition();
  // Update frame position for scrolling
  mScrolledFrame->SetPosition(mScrollPort.TopLeft() - pt);
  mLastScrollOrigin = aOrigin;
  mLastSmoothScrollOrigin = nullptr;
  mScrollGeneration = ++sScrollGenerationCounter;

  // We pass in the amount to move visually
  ScrollVisual(oldScrollFramePos);

  ScheduleSyntheticMouseMove();
  nsWeakFrame weakFrame(mOuter);
  UpdateScrollbarPosition();
  if (!weakFrame.IsAlive()) {
    return;
  }
  PostScrollEvent();

  // notify the listeners.
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->ScrollPositionDidChange(pt.x, pt.y);
  }

  nsCOMPtr<nsIDocShell> docShell = presContext->GetDocShell();
  if (docShell) {
    docShell->NotifyScrollObservers();
  }
}

static int32_t
MaxZIndexInList(nsDisplayList* aList, nsDisplayListBuilder* aBuilder)
{
  int32_t maxZIndex = 0;
  for (nsDisplayItem* item = aList->GetBottom(); item; item = item->GetAbove()) {
    maxZIndex = std::max(maxZIndex, item->ZIndex());
  }
  return maxZIndex;
}

static void
AppendToTop(nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists,
            nsDisplayList* aSource, nsIFrame* aSourceFrame, bool aOwnLayer,
            bool aPositioned)
{
  if (aSource->IsEmpty())
    return;

  nsDisplayWrapList* newItem = aOwnLayer?
    new (aBuilder) nsDisplayOwnLayer(aBuilder, aSourceFrame, aSource) :
    new (aBuilder) nsDisplayWrapList(aBuilder, aSourceFrame, aSource);

  if (aPositioned) {
    // We want overlay scrollbars to always be on top of the scrolled content,
    // but we don't want them to unnecessarily cover overlapping elements from
    // outside our scroll frame.
    nsDisplayList* positionedDescendants = aLists.PositionedDescendants();
    if (!positionedDescendants->IsEmpty()) {
      newItem->SetOverrideZIndex(MaxZIndexInList(positionedDescendants, aBuilder));
      positionedDescendants->AppendNewToTop(newItem);
    } else {
      aLists.Outlines()->AppendNewToTop(newItem);
    }
  } else {
    aLists.BorderBackground()->AppendNewToTop(newItem);
  }
}

struct HoveredStateComparator
{
  bool Equals(nsIFrame* A, nsIFrame* B) const {
    bool aHovered = A->GetContent()->HasAttr(kNameSpaceID_None,
                                             nsGkAtoms::hover);
    bool bHovered = B->GetContent()->HasAttr(kNameSpaceID_None,
                                             nsGkAtoms::hover);
    return aHovered == bHovered;
  }
  bool LessThan(nsIFrame* A, nsIFrame* B) const {
    bool aHovered = A->GetContent()->HasAttr(kNameSpaceID_None,
                                             nsGkAtoms::hover);
    bool bHovered = B->GetContent()->HasAttr(kNameSpaceID_None,
                                             nsGkAtoms::hover);
    return !aHovered && bHovered;
  }
};

void
ScrollFrameHelper::AppendScrollPartsTo(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists,
                                           bool                    aUsingDisplayPort,
                                           bool                    aCreateLayer,
                                           bool                    aPositioned)
{
  nsITheme* theme = mOuter->PresContext()->GetTheme();
  if (theme &&
      theme->ShouldHideScrollbars()) {
    return;
  }

  bool overlayScrollbars =
    LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) != 0;

  nsAutoTArray<nsIFrame*, 3> scrollParts;
  for (nsIFrame* kid = mOuter->GetFirstPrincipalChild(); kid; kid = kid->GetNextSibling()) {
    if (kid == mScrolledFrame ||
        (kid->IsPositioned() || overlayScrollbars) != aPositioned)
      continue;

    scrollParts.AppendElement(kid);
  }
  if (scrollParts.IsEmpty()) {
    return;
  }

  mozilla::layers::FrameMetrics::ViewID scrollTargetId = IsScrollingActive()
    ? nsLayoutUtils::FindOrCreateIDFor(mScrolledFrame->GetContent())
    : mozilla::layers::FrameMetrics::NULL_SCROLL_ID;

  scrollParts.Sort(HoveredStateComparator());

  DisplayListClipState::AutoSaveRestore clipState(aBuilder);
  // Don't let scrollparts extent outside our frame's border-box, if these are
  // viewport scrollbars. They would create layerization problems. This wouldn't
  // normally be an issue but themes can add overflow areas to scrollbar parts.
  if (mIsRoot) {
    clipState.ClipContentDescendants(
        mOuter->GetRectRelativeToSelf() + aBuilder->ToReferenceFrame(mOuter));
  }

  for (uint32_t i = 0; i < scrollParts.Length(); ++i) {
    uint32_t flags = 0;
    if (scrollParts[i] == mVScrollbarBox) {
      flags |= nsDisplayOwnLayer::VERTICAL_SCROLLBAR;
    }
    if (scrollParts[i] == mHScrollbarBox) {
      flags |= nsDisplayOwnLayer::HORIZONTAL_SCROLLBAR;
    }

    // The display port doesn't necessarily include the scrollbars, so just
    // include all of the scrollbars if we have a display port.
    nsRect dirty = aUsingDisplayPort ?
      scrollParts[i]->GetVisualOverflowRectRelativeToParent() : aDirtyRect;
    nsDisplayListBuilder::AutoBuildingDisplayList
      buildingForChild(aBuilder, scrollParts[i],
                       dirty + mOuter->GetOffsetTo(scrollParts[i]), true);
    nsDisplayListBuilder::AutoCurrentScrollbarInfoSetter
      infoSetter(aBuilder, scrollTargetId, flags);
    nsDisplayListCollection partList;
    mOuter->BuildDisplayListForChild(
      aBuilder, scrollParts[i], dirty, partList,
      nsIFrame::DISPLAY_CHILD_FORCE_STACKING_CONTEXT);

    // Always create layers for overlay scrollbars so that we don't create a
    // giant layer covering the whole scrollport if both scrollbars are visible.
    bool isOverlayScrollbar = (flags != 0) && overlayScrollbars;
    bool createLayer = aCreateLayer || isOverlayScrollbar;

    // DISPLAY_CHILD_FORCE_STACKING_CONTEXT put everything into
    // partList.PositionedDescendants().
    ::AppendToTop(aBuilder, aLists,
                  partList.PositionedDescendants(), scrollParts[i],
                  createLayer, aPositioned);
  }
}

class ScrollLayerWrapper : public nsDisplayWrapper
{
public:
  ScrollLayerWrapper(nsIFrame* aScrollFrame, nsIFrame* aScrolledFrame)
    : mCount(0)
    , mProps(aScrolledFrame->Properties())
    , mScrollFrame(aScrollFrame)
    , mScrolledFrame(aScrolledFrame)
  {
    SetCount(0);
  }

  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame,
                                  nsDisplayList* aList) MOZ_OVERRIDE {
    SetCount(++mCount);
    return new (aBuilder) nsDisplayScrollLayer(aBuilder, aList, mScrolledFrame, mScrolledFrame, mScrollFrame);
  }

  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) MOZ_OVERRIDE {

    SetCount(++mCount);
    return new (aBuilder) nsDisplayScrollLayer(aBuilder, aItem, aItem->Frame(), mScrolledFrame, mScrollFrame);
  }

protected:
  void SetCount(intptr_t aCount) {
    mProps.Set(nsIFrame::ScrollLayerCount(), reinterpret_cast<void*>(aCount));
  }

  intptr_t mCount;
  FrameProperties mProps;
  nsIFrame* mScrollFrame;
  nsIFrame* mScrolledFrame;
};

/* static */ bool ScrollFrameHelper::sImageVisPrefsCached = false;
/* static */ uint32_t ScrollFrameHelper::sHorzExpandScrollPort = 0;
/* static */ uint32_t ScrollFrameHelper::sVertExpandScrollPort = 1;
/* static */ int32_t ScrollFrameHelper::sHorzScrollFraction = 2;
/* static */ int32_t ScrollFrameHelper::sVertScrollFraction = 2;

/* static */ void
ScrollFrameHelper::EnsureImageVisPrefsCached()
{
  if (!sImageVisPrefsCached) {
    Preferences::AddUintVarCache(&sHorzExpandScrollPort,
      "layout.imagevisibility.numscrollportwidths", (uint32_t)0);
    Preferences::AddUintVarCache(&sVertExpandScrollPort,
      "layout.imagevisibility.numscrollportheights", 1);

    Preferences::AddIntVarCache(&sHorzScrollFraction,
      "layout.imagevisibility.amountscrollbeforeupdatehorizontal", 2);
    Preferences::AddIntVarCache(&sVertScrollFraction,
      "layout.imagevisibility.amountscrollbeforeupdatevertical", 2);

    sImageVisPrefsCached = true;
  }
}

nsRect
ScrollFrameHelper::ExpandRectToNearlyVisible(const nsRect& aRect) const
{
  // We don't want to expand a rect in a direction that we can't scroll, so we
  // check the scroll range.
  nsRect scrollRange = GetScrollRangeForClamping();
  nsPoint scrollPos = GetScrollPosition();
  nsMargin expand(0, 0, 0, 0);

  nscoord vertShift = sVertExpandScrollPort * aRect.height;
  if (scrollRange.y < scrollPos.y) {
    expand.top = vertShift;
  }
  if (scrollPos.y < scrollRange.YMost()) {
    expand.bottom = vertShift;
  }

  nscoord horzShift = sHorzExpandScrollPort * aRect.width;
  if (scrollRange.x < scrollPos.x) {
    expand.left = horzShift;
  }
  if (scrollPos.x < scrollRange.XMost()) {
    expand.right = horzShift;
  }

  nsRect rect = aRect;
  rect.Inflate(expand);
  return rect;
}

static bool
ShouldBeClippedByFrame(nsIFrame* aClipFrame, nsIFrame* aClippedFrame)
{
  return nsLayoutUtils::IsProperAncestorFrame(aClipFrame, aClippedFrame);
}

static void
ClipItemsExceptCaret(nsDisplayList* aList, nsDisplayListBuilder* aBuilder,
                     nsIFrame* aClipFrame, const DisplayItemClip& aClip)
{
  nsDisplayItem* i = aList->GetBottom();
  for (; i; i = i->GetAbove()) {
    if (!::ShouldBeClippedByFrame(aClipFrame, i->Frame())) {
      continue;
    }

    bool unused;
    nsRect bounds = i->GetBounds(aBuilder, &unused);
    bool isAffectedByClip = aClip.IsRectAffectedByClip(bounds);
    if (isAffectedByClip && nsDisplayItem::TYPE_CARET == i->GetType()) {
      // Don't clip the caret if it overflows vertically only, and by half
      // its height at most.  This is to avoid clipping it when the line-height
      // is small.
      auto half = bounds.height / 2;
      bounds.y += half;
      bounds.height -= half;
      isAffectedByClip = aClip.IsRectAffectedByClip(bounds);
      if (isAffectedByClip) {
        // Don't clip the caret if it's just outside on the right side.
        nsRect rightSide(bounds.x - 1, bounds.y, 1, bounds.height);
        isAffectedByClip = aClip.IsRectAffectedByClip(rightSide);
        // Also, avoid clipping it in a zero-height line box (heuristic only).
        if (isAffectedByClip) {
          isAffectedByClip = i->Frame()->GetRect().height != 0;
        }
      }
    }
    if (isAffectedByClip) {
      DisplayItemClip newClip;
      newClip.IntersectWith(i->GetClip());
      newClip.IntersectWith(aClip);
      i->SetClip(aBuilder, newClip);
    }
    nsDisplayList* children = i->GetSameCoordinateSystemChildren();
    if (children) {
      ClipItemsExceptCaret(children, aBuilder, aClipFrame, aClip);
    }
  }
}

static void
ClipListsExceptCaret(nsDisplayListCollection* aLists,
                     nsDisplayListBuilder* aBuilder,
                     nsIFrame* aClipFrame,
                     const DisplayItemClip& aClip)
{
  ::ClipItemsExceptCaret(aLists->BorderBackground(), aBuilder, aClipFrame, aClip);
  ::ClipItemsExceptCaret(aLists->BlockBorderBackgrounds(), aBuilder, aClipFrame, aClip);
  ::ClipItemsExceptCaret(aLists->Floats(), aBuilder, aClipFrame, aClip);
  ::ClipItemsExceptCaret(aLists->PositionedDescendants(), aBuilder, aClipFrame, aClip);
  ::ClipItemsExceptCaret(aLists->Outlines(), aBuilder, aClipFrame, aClip);
  ::ClipItemsExceptCaret(aLists->Content(), aBuilder, aClipFrame, aClip);
}

void
ScrollFrameHelper::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                    const nsRect&           aDirtyRect,
                                    const nsDisplayListSet& aLists)
{
  if (aBuilder->IsForImageVisibility()) {
    mLastUpdateImagesPos = GetScrollPosition();
  }

  mOuter->DisplayBorderBackgroundOutline(aBuilder, aLists);

  if (aBuilder->IsPaintingToWindow()) {
    mScrollPosAtLastPaint = GetScrollPosition();
    if (IsScrollingActive() && NeedToInvalidateOnScroll(mOuter)) {
      MarkInactive();
    }
    if (IsScrollingActive()) {
      if (mScrollPosForLayerPixelAlignment == nsPoint(-1,-1)) {
        mScrollPosForLayerPixelAlignment = mScrollPosAtLastPaint;
      }
    } else {
      mScrollPosForLayerPixelAlignment = nsPoint(-1,-1);
    }
  }

  // We put non-overlay scrollbars in their own layers when this is the root
  // scroll frame and we are a toplevel content document. In this situation,
  // the scrollbar(s) would normally be assigned their own layer anyway, since
  // they're not scrolled with the rest of the document. But when both
  // scrollbars are visible, the layer's visible rectangle would be the size
  // of the viewport, so most layer implementations would create a layer buffer
  // that's much larger than necessary. Creating independent layers for each
  // scrollbar works around the problem.
  bool createLayersForScrollbars = mIsRoot &&
    mOuter->PresContext()->IsRootContentDocument();

  if (aBuilder->GetIgnoreScrollFrame() == mOuter || IsIgnoringViewportClipping()) {

    // If we are a root scroll frame that has a display port we want to add
    // scrollbars, they will be children of the scrollable layer, but they get
    // adjusted by the APZC automatically.
    bool usingDisplayPort = nsLayoutUtils::GetDisplayPort(mOuter->GetContent());
    bool addScrollBars = mIsRoot && usingDisplayPort && !aBuilder->IsForEventDelivery();

    if (addScrollBars) {
      // Add classic scrollbars.
      AppendScrollPartsTo(aBuilder, aDirtyRect, aLists, usingDisplayPort,
                          createLayersForScrollbars, false);
    }

    // Don't clip the scrolled child, and don't paint scrollbars/scrollcorner.
    // The scrolled frame shouldn't have its own background/border, so we
    // can just pass aLists directly.
    mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame,
                                     aDirtyRect, aLists);

    if (addScrollBars) {
      // Add overlay scrollbars.
      AppendScrollPartsTo(aBuilder, aDirtyRect, aLists, usingDisplayPort,
                          createLayersForScrollbars, true);
    }

    return;
  }

  // Overflow clipping can never clip frames outside our subtree, so there
  // is no need to worry about whether we are a moving frame that might clip
  // non-moving frames.
  // Not all our descendants will be clipped by overflow clipping, but all
  // the ones that aren't clipped will be out of flow frames that have already
  // had dirty rects saved for them by their parent frames calling
  // MarkOutOfFlowChildrenForDisplayList, so it's safe to restrict our
  // dirty rect here.
  nsRect dirtyRect = aDirtyRect.Intersect(mScrollPort);

  nsRect displayPort;
  bool usingDisplayport = false;
  if (!aBuilder->IsForEventDelivery()) {
    if (!mIsRoot) {
      // For a non-root scroll frame, override the value of the display port
      // base rect, and possibly create a display port if there isn't one
      // already. For root scroll frame, nsLayoutUtils::PaintFrame or
      // nsSubDocumentFrame::BuildDisplayList takes care of this.
      nsRect displayportBase = dirtyRect;
      usingDisplayport = nsLayoutUtils::GetOrMaybeCreateDisplayPort(
          *aBuilder, mOuter, displayportBase, &displayPort);
    } else {
      // For a root frmae, just get the value of the existing of the display
      // port, if any.
      usingDisplayport = nsLayoutUtils::GetDisplayPort(mOuter->GetContent(), &displayPort);
    }

    bool usingLowPrecision = gfxPrefs::UseLowPrecisionBuffer();
    if (usingDisplayport && usingLowPrecision) {
      // If we have low-res painting enabled we should check the critical displayport too
      nsRect critDp;
      nsLayoutUtils::GetCriticalDisplayPort(mOuter->GetContent(), &critDp);
    }

    // Override the dirty rectangle if the displayport has been set.
    if (usingDisplayport) {
      dirtyRect = displayPort;
    }
  }

  // Now display the scrollbars and scrollcorner. These parts are drawn
  // in the border-background layer, on top of our own background and
  // borders and underneath borders and backgrounds of later elements
  // in the tree.
  // Note that this does not apply for overlay scrollbars; those are drawn
  // in the positioned-elements layer on top of everything else by the call
  // to AppendScrollPartsTo(..., true) further down.
  AppendScrollPartsTo(aBuilder, aDirtyRect, aLists, usingDisplayport,
                      createLayersForScrollbars, false);

  if (aBuilder->IsForImageVisibility()) {
    // We expand the dirty rect to catch images just outside of the scroll port.
    // We use the dirty rect instead of the whole scroll port to prevent
    // too much expansion in the presence of very large (bigger than the
    // viewport) scroll ports.
    dirtyRect = ExpandRectToNearlyVisible(dirtyRect);
  }

  // Since making new layers is expensive, only use nsDisplayScrollLayer
  // if the area is scrollable and we're the content process (unless we're on
  // B2G, where we support async scrolling for scrollable elements in the
  // parent process as well).
  // When a displayport is being used, force building of a layer so that
  // CompositorParent can always find the scrollable layer for the root content
  // document.
  // If the element is marked 'scrollgrab', also force building of a layer
  // so that APZ can implement scroll grabbing.
  mShouldBuildScrollableLayer = usingDisplayport || nsContentUtils::HasScrollgrab(mOuter->GetContent());
  bool shouldBuildLayer = false;
  if (mShouldBuildScrollableLayer) {
    shouldBuildLayer = true;
  } else {
    shouldBuildLayer =
      nsLayoutUtils::WantSubAPZC() &&
      WantAsyncScroll() &&
      // If we are the root scroll frame for the display root then we don't need a scroll
      // info layer to make a ComputeFrameMetrics call for us as
      // nsDisplayList::PaintForFrame already calls ComputeFrameMetrics for us.
      (!mIsRoot || aBuilder->RootReferenceFrame()->PresContext() != mOuter->PresContext());
  }

  mScrollParentID = aBuilder->GetCurrentScrollParentId();

  nsDisplayListCollection scrolledContent;
  {
    // Note that setting the current scroll parent id here means that positioned children
    // of this scroll info layer will pick up the scroll info layer as their scroll handoff
    // parent. This is intentional because that is what happens for positioned children
    // of scroll layers, and we want to maintain consistent behaviour between scroll layers
    // and scroll info layers.
    nsDisplayListBuilder::AutoCurrentScrollParentIdSetter idSetter(
        aBuilder,
        shouldBuildLayer && mScrolledFrame->GetContent()
            ? nsLayoutUtils::FindOrCreateIDFor(mScrolledFrame->GetContent())
            : aBuilder->GetCurrentScrollParentId());
    DisplayListClipState::AutoSaveRestore clipState(aBuilder);

    if (usingDisplayport) {
      // If we are using a display port, then ignore any pre-existing clip
      // passed down from our parents. The pre-existing clip would just defeat
      // the purpose of a display port which is to paint regions that are not
      // currently visible so that they can be brought into view asynchronously.
      // Notes:
      //   - The pre-existing clip state will be restored when the
      //     AutoSaveRestore goes out of scope, so there is no permanent change
      //     to this clip state.
      //   - We still set a clip to the scroll port further below where we
      //     build the scroll wrapper. This doesn't prevent us from painting
      //     the entire displayport, but it lets the compositor know to
      //     clip to the scroll port after compositing.
      clipState.Clear();
    } else {
      nsRect clip = mScrollPort + aBuilder->ToReferenceFrame(mOuter);
      nscoord radii[8];
      bool haveRadii = mOuter->GetPaddingBoxBorderRadii(radii);
      // Our override of GetBorderRadii ensures we never have a radius at
      // the corners where we have a scrollbar.
      if (mClipAllDescendants) {
        clipState.ClipContentDescendants(clip, haveRadii ? radii : nullptr);
      } else {
        clipState.ClipContainingBlockDescendants(clip, haveRadii ? radii : nullptr);
      }
    }

    mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame, dirtyRect, scrolledContent);
  }

  if (MOZ_UNLIKELY(mOuter->StyleDisplay()->mOverflowClipBox ==
                     NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX)) {
    // We only clip if there is *scrollable* overflow, to avoid clipping
    // *visual* overflow unnecessarily.
    nsRect clipRect = mScrollPort + aBuilder->ToReferenceFrame(mOuter);
    nsRect so = mScrolledFrame->GetScrollableOverflowRect();
    if (clipRect.width != so.width || clipRect.height != so.height ||
        so.x < 0 || so.y < 0) {
      // The 'scrolledContent' items are clipped to the padding-box at this point.
      // Now clip them again to the content-box, except the nsDisplayCaret item
      // which we allow to overflow the content-box in various situations --
      // see ::ClipItemsExceptCaret.
      clipRect.Deflate(mOuter->GetUsedPadding());
      DisplayItemClip clip;
      clip.SetTo(clipRect);
      ::ClipListsExceptCaret(&scrolledContent, aBuilder, mScrolledFrame, clip);
    }
  }

  if (shouldBuildLayer) {
    // ScrollLayerWrapper must always be created because it initializes the
    // scroll layer count. The display lists depend on this.
    ScrollLayerWrapper wrapper(mOuter, mScrolledFrame);

    if (mShouldBuildScrollableLayer && BuildScrollContainerLayers()) {
      DisplayListClipState::AutoSaveRestore clipState(aBuilder);

      // For root scrollframes in documents where the CSS viewport has been
      // modified, the CSS viewport no longer corresponds to what is visible,
      // so we don't want to clip the content to it. For root scrollframes
      // in documents where the CSS viewport is NOT modified, the mScrollPort
      // is the same as the CSS viewport, modulo scrollbars.
      if (!(mIsRoot && mOuter->PresContext()->PresShell()->GetIsViewportOverridden())) {
        nsRect clip = mScrollPort + aBuilder->ToReferenceFrame(mOuter);
        if (mClipAllDescendants) {
          clipState.ClipContentDescendants(clip);
        } else {
          clipState.ClipContainingBlockDescendants(clip);
        }
      }

      // Once a displayport is set, assume that scrolling needs to be fast
      // so create a layer with all the content inside. The compositor
      // process will be able to scroll the content asynchronously.
      wrapper.WrapListsInPlace(aBuilder, mOuter, scrolledContent);
    }

    // In case we are not using displayport or the nsDisplayScrollLayers are
    // flattened during visibility computation, we still need to export the
    // metadata about this scroll box to the compositor process.
    nsDisplayScrollInfoLayer* layerItem = new (aBuilder) nsDisplayScrollInfoLayer(
      aBuilder, mScrolledFrame, mOuter);
    nsDisplayList* positionedDescendants = scrolledContent.PositionedDescendants();
    if (!positionedDescendants->IsEmpty()) {
      layerItem->SetOverrideZIndex(MaxZIndexInList(positionedDescendants, aBuilder));
      positionedDescendants->AppendNewToTop(layerItem);
    } else {
      aLists.Outlines()->AppendNewToTop(layerItem);
    }
  }
  // Now display overlay scrollbars and the resizer, if we have one.
  AppendScrollPartsTo(aBuilder, aDirtyRect, scrolledContent, usingDisplayport,
                      createLayersForScrollbars, true);
  scrolledContent.MoveTo(aLists);
}

void
ScrollFrameHelper::ComputeFrameMetrics(Layer* aLayer,
                                       nsIFrame* aContainerReferenceFrame,
                                       const ContainerLayerParameters& aParameters,
                                       nsRect* aClipRect,
                                       nsTArray<FrameMetrics>* aOutput) const
{
  nsRect viewport = mScrollPort +
    mOuter->GetOffsetToCrossDoc(aContainerReferenceFrame);
  if (!(mIsRoot && mOuter->PresContext()->PresShell()->GetIsViewportOverridden())) {
    *aClipRect = viewport;
  }

  if (!mShouldBuildScrollableLayer || BuildScrollContainerLayers()) {
    return;
  }

  MOZ_ASSERT(mScrolledFrame->GetContent());

  *aOutput->AppendElement() =
      nsDisplayScrollLayer::ComputeFrameMetrics(mScrolledFrame, mOuter,
        aContainerReferenceFrame, aLayer, mScrollParentID,
        viewport, false, false, aParameters);
}

bool
ScrollFrameHelper::IsRectNearlyVisible(const nsRect& aRect) const
{
  // Use the right rect depending on if a display port is set.
  nsRect displayPort;
  bool usingDisplayport = nsLayoutUtils::GetDisplayPort(mOuter->GetContent(), &displayPort);
  return aRect.Intersects(ExpandRectToNearlyVisible(usingDisplayport ? displayPort : mScrollPort));
}

static void HandleScrollPref(nsIScrollable *aScrollable, int32_t aOrientation,
                             uint8_t& aValue)
{
  int32_t pref;
  aScrollable->GetDefaultScrollbarPreferences(aOrientation, &pref);
  switch (pref) {
    case nsIScrollable::Scrollbar_Auto:
      // leave |aValue| untouched
      break;
    case nsIScrollable::Scrollbar_Never:
      aValue = NS_STYLE_OVERFLOW_HIDDEN;
      break;
    case nsIScrollable::Scrollbar_Always:
      aValue = NS_STYLE_OVERFLOW_SCROLL;
      break;
  }
}

ScrollbarStyles
ScrollFrameHelper::GetScrollbarStylesFromFrame() const
{
  nsPresContext* presContext = mOuter->PresContext();
  if (!presContext->IsDynamic() &&
      !(mIsRoot && presContext->HasPaginatedScrolling())) {
    return ScrollbarStyles(NS_STYLE_OVERFLOW_HIDDEN, NS_STYLE_OVERFLOW_HIDDEN);
  }

  if (!mIsRoot) {
    const nsStyleDisplay* disp = mOuter->StyleDisplay();
    return ScrollbarStyles(disp->mOverflowX, disp->mOverflowY);
  }

  ScrollbarStyles result = presContext->GetViewportOverflowOverride();
  nsCOMPtr<nsISupports> container = presContext->GetContainerWeak();
  nsCOMPtr<nsIScrollable> scrollable = do_QueryInterface(container);
  if (scrollable) {
    HandleScrollPref(scrollable, nsIScrollable::ScrollOrientation_X,
                     result.mHorizontal);
    HandleScrollPref(scrollable, nsIScrollable::ScrollOrientation_Y,
                     result.mVertical);
  }
  return result;
}

nsRect
ScrollFrameHelper::GetScrollRange() const
{
  return GetScrollRange(mScrollPort.width, mScrollPort.height);
}

nsRect
ScrollFrameHelper::GetScrollRange(nscoord aWidth, nscoord aHeight) const
{
  nsRect range = GetScrolledRect();
  range.width = std::max(range.width - aWidth, 0);
  range.height = std::max(range.height - aHeight, 0);
  return range;
}

nsRect
ScrollFrameHelper::GetScrollRangeForClamping() const
{
  if (!ShouldClampScrollPosition()) {
    return nsRect(nscoord_MIN/2, nscoord_MIN/2,
                  nscoord_MAX - nscoord_MIN/2, nscoord_MAX - nscoord_MIN/2);
  }
  nsSize scrollPortSize = GetScrollPositionClampingScrollPortSize();
  return GetScrollRange(scrollPortSize.width, scrollPortSize.height);
}

nsSize
ScrollFrameHelper::GetScrollPositionClampingScrollPortSize() const
{
  nsIPresShell* presShell = mOuter->PresContext()->PresShell();
  if (mIsRoot && presShell->IsScrollPositionClampingScrollPortSizeSet()) {
    return presShell->GetScrollPositionClampingScrollPortSize();
  }
  return mScrollPort.Size();
}

gfxSize
ScrollFrameHelper::GetResolution() const
{
  return mResolution;
}

void
ScrollFrameHelper::SetResolution(const gfxSize& aResolution)
{
  mResolution = aResolution;
  mIsResolutionSet = true;
}

static void
AdjustForWholeDelta(int32_t aDelta, nscoord* aCoord)
{
  if (aDelta < 0) {
    *aCoord = nscoord_MIN;
  } else if (aDelta > 0) {
    *aCoord = nscoord_MAX;
  }
}

/**
 * Calculate lower/upper scrollBy range in given direction.
 * @param aDelta specifies scrollBy direction, if 0 then range will be 0 size
 * @param aPos desired destination in AppUnits
 * @param aNeg/PosTolerance defines relative range distance
 *   below and above of aPos point
 * @param aMultiplier used for conversion of tolerance into appUnis
 */
static void
CalcRangeForScrollBy(int32_t aDelta, nscoord aPos,
                     float aNegTolerance,
                     float aPosTolerance,
                     nscoord aMultiplier,
                     nscoord* aLower, nscoord* aUpper)
{
  if (!aDelta) {
    *aLower = *aUpper = aPos;
    return;
  }
  *aLower = aPos - NSToCoordRound(aMultiplier * (aDelta > 0 ? aNegTolerance : aPosTolerance));
  *aUpper = aPos + NSToCoordRound(aMultiplier * (aDelta > 0 ? aPosTolerance : aNegTolerance));
}

void
ScrollFrameHelper::ScrollBy(nsIntPoint aDelta,
                                nsIScrollableFrame::ScrollUnit aUnit,
                                nsIScrollableFrame::ScrollMode aMode,
                                nsIntPoint* aOverflow,
                                nsIAtom *aOrigin,
                                bool aIsMomentum)
{
  // When a smooth scroll is being processed on a frame, mouse wheel and trackpad
  // momentum scroll event updates must notcancel the SMOOTH or SMOOTH_MSD
  // scroll animations, enabling Javascript that depends on them to be responsive
  // without forcing the user to wait for the fling animations to completely stop.
  if (aIsMomentum && IsProcessingAsyncScroll()) {
    return;
  }

  if (mAsyncSmoothMSDScroll != nullptr) {
    // When CSSOM-View scroll-behavior smooth scrolling is interrupted,
    // the scroll is not completed to avoid non-smooth snapping to the
    // prior smooth scroll's destination.
    mDestination = GetScrollPosition();
  }

  nsSize deltaMultiplier;
  float negativeTolerance;
  float positiveTolerance;
  if (!aOrigin){
    aOrigin = nsGkAtoms::other;
  }
  bool isGenericOrigin = (aOrigin == nsGkAtoms::other);
  switch (aUnit) {
  case nsIScrollableFrame::DEVICE_PIXELS: {
    nscoord appUnitsPerDevPixel =
      mOuter->PresContext()->AppUnitsPerDevPixel();
    deltaMultiplier = nsSize(appUnitsPerDevPixel, appUnitsPerDevPixel);
    if (isGenericOrigin){
      aOrigin = nsGkAtoms::pixels;
    }
    negativeTolerance = positiveTolerance = 0.5f;
    break;
  }
  case nsIScrollableFrame::LINES: {
    deltaMultiplier = GetLineScrollAmount();
    if (isGenericOrigin){
      aOrigin = nsGkAtoms::lines;
    }
    negativeTolerance = positiveTolerance = 0.1f;
    break;
  }
  case nsIScrollableFrame::PAGES: {
    deltaMultiplier = GetPageScrollAmount();
    if (isGenericOrigin){
      aOrigin = nsGkAtoms::pages;
    }
    negativeTolerance = 0.05f;
    positiveTolerance = 0;
    break;
  }
  case nsIScrollableFrame::WHOLE: {
    nsPoint pos = GetScrollPosition();
    AdjustForWholeDelta(aDelta.x, &pos.x);
    AdjustForWholeDelta(aDelta.y, &pos.y);
    ScrollTo(pos, aMode);
    // 'this' might be destroyed here
    if (aOverflow) {
      *aOverflow = nsIntPoint(0, 0);
    }
    return;
  }
  default:
    NS_ERROR("Invalid scroll mode");
    return;
  }

  nsPoint newPos = mDestination +
    nsPoint(aDelta.x*deltaMultiplier.width, aDelta.y*deltaMultiplier.height);
  // Calculate desired range values.
  nscoord rangeLowerX, rangeUpperX, rangeLowerY, rangeUpperY;
  CalcRangeForScrollBy(aDelta.x, newPos.x, negativeTolerance, positiveTolerance,
                       deltaMultiplier.width, &rangeLowerX, &rangeUpperX);
  CalcRangeForScrollBy(aDelta.y, newPos.y, negativeTolerance, positiveTolerance,
                       deltaMultiplier.height, &rangeLowerY, &rangeUpperY);
  nsRect range(rangeLowerX,
               rangeLowerY,
               rangeUpperX - rangeLowerX,
               rangeUpperY - rangeLowerY);
  nsWeakFrame weakFrame(mOuter);
  ScrollToWithOrigin(newPos, aMode, aOrigin, &range);
  if (!weakFrame.IsAlive()) {
    return;
  }

  if (aOverflow) {
    nsPoint clampAmount = newPos - mDestination;
    float appUnitsPerDevPixel = mOuter->PresContext()->AppUnitsPerDevPixel();
    *aOverflow = nsIntPoint(
        NSAppUnitsToIntPixels(clampAmount.x, appUnitsPerDevPixel),
        NSAppUnitsToIntPixels(clampAmount.y, appUnitsPerDevPixel));
  }
}

nsSize
ScrollFrameHelper::GetLineScrollAmount() const
{
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(mOuter, getter_AddRefs(fm),
    nsLayoutUtils::FontSizeInflationFor(mOuter));
  NS_ASSERTION(fm, "FontMetrics is null, assuming fontHeight == 1 appunit");
  static nscoord sMinLineScrollAmountInPixels = -1;
  if (sMinLineScrollAmountInPixels < 0) {
    Preferences::AddIntVarCache(&sMinLineScrollAmountInPixels,
                                "mousewheel.min_line_scroll_amount", 1);
  }
  int32_t appUnitsPerDevPixel = mOuter->PresContext()->AppUnitsPerDevPixel();
  nscoord minScrollAmountInAppUnits =
    std::max(1, sMinLineScrollAmountInPixels) * appUnitsPerDevPixel;
  nscoord horizontalAmount = fm ? fm->AveCharWidth() : 0;
  nscoord verticalAmount = fm ? fm->MaxHeight() : 0;
  return nsSize(std::max(horizontalAmount, minScrollAmountInAppUnits),
                std::max(verticalAmount, minScrollAmountInAppUnits));
}

/**
 * Compute the scrollport size excluding any fixed-pos headers and
 * footers. A header or footer is an box that spans that entire width
 * of the viewport and touches the top (or bottom, respectively) of the
 * viewport. We also want to consider fixed elements that stack or overlap
 * to effectively create a larger header or footer. Headers and footers that
 * cover more than a third of the the viewport are ignored since they
 * probably aren't true headers and footers and we don't want to restrict
 * scrolling too much in such cases. This is a bit conservative --- some
 * pages use elements as headers or footers that don't span the entire width
 * of the viewport --- but it should be a good start.
 */
struct TopAndBottom
{
  TopAndBottom(nscoord aTop, nscoord aBottom) : top(aTop), bottom(aBottom) {}

  nscoord top, bottom;
};
struct TopComparator
{
  bool Equals(const TopAndBottom& A, const TopAndBottom& B) const {
    return A.top == B.top;
  }
  bool LessThan(const TopAndBottom& A, const TopAndBottom& B) const {
    return A.top < B.top;
  }
};
struct ReverseBottomComparator
{
  bool Equals(const TopAndBottom& A, const TopAndBottom& B) const {
    return A.bottom == B.bottom;
  }
  bool LessThan(const TopAndBottom& A, const TopAndBottom& B) const {
    return A.bottom > B.bottom;
  }
};
static nsSize
GetScrollPortSizeExcludingHeadersAndFooters(nsIFrame* aViewportFrame,
                                            const nsRect& aScrollPort)
{
  nsTArray<TopAndBottom> list;
  nsFrameList fixedFrames = aViewportFrame->GetChildList(nsIFrame::kFixedList);
  for (nsFrameList::Enumerator iterator(fixedFrames); !iterator.AtEnd();
       iterator.Next()) {
    nsIFrame* f = iterator.get();
    nsRect r = f->GetRect().Intersect(aScrollPort);
    if (r.x == 0 && r.width == aScrollPort.width &&
        r.height <= aScrollPort.height/3) {
      list.AppendElement(TopAndBottom(r.y, r.YMost()));
    }
  }

  list.Sort(TopComparator());
  nscoord headerBottom = 0;
  for (uint32_t i = 0; i < list.Length(); ++i) {
    if (list[i].top <= headerBottom) {
      headerBottom = std::max(headerBottom, list[i].bottom);
    }
  }

  list.Sort(ReverseBottomComparator());
  nscoord footerTop = aScrollPort.height;
  for (uint32_t i = 0; i < list.Length(); ++i) {
    if (list[i].bottom >= footerTop) {
      footerTop = std::min(footerTop, list[i].top);
    }
  }

  headerBottom = std::min(aScrollPort.height/3, headerBottom);
  footerTop = std::max(aScrollPort.height - aScrollPort.height/3, footerTop);

  return nsSize(aScrollPort.width, footerTop - headerBottom);
}

nsSize
ScrollFrameHelper::GetPageScrollAmount() const
{
  nsSize lineScrollAmount = GetLineScrollAmount();
  nsSize effectiveScrollPortSize;
  if (mIsRoot) {
    // Reduce effective scrollport height by the height of any fixed-pos
    // headers or footers
    nsIFrame* root = mOuter->PresContext()->PresShell()->GetRootFrame();
    effectiveScrollPortSize =
      GetScrollPortSizeExcludingHeadersAndFooters(root, mScrollPort);
  } else {
    effectiveScrollPortSize = mScrollPort.Size();
  }
  // The page increment is the size of the page, minus the smaller of
  // 10% of the size or 2 lines.
  return nsSize(
    effectiveScrollPortSize.width -
      std::min(effectiveScrollPortSize.width/10, 2*lineScrollAmount.width),
    effectiveScrollPortSize.height -
      std::min(effectiveScrollPortSize.height/10, 2*lineScrollAmount.height));
}

  /**
   * this code is resposible for restoring the scroll position back to some
   * saved position. if the user has not moved the scroll position manually
   * we keep scrolling down until we get to our original position. keep in
   * mind that content could incrementally be coming in. we only want to stop
   * when we reach our new position.
   */
void
ScrollFrameHelper::ScrollToRestoredPosition()
{
  if (mRestorePos.y == -1 || mLastPos.x == -1 || mLastPos.y == -1) {
    return;
  }
  // make sure our scroll position did not change for where we last put
  // it. if it does then the user must have moved it, and we no longer
  // need to restore.
  //
  // In the RTL case, we check whether the scroll position changed using the
  // logical scroll position, but we scroll to the physical scroll position in
  // all cases

  // if we didn't move, we still need to restore
  if (GetLogicalScrollPosition() == mLastPos) {
    // if our desired position is different to the scroll position, scroll.
    // remember that we could be incrementally loading so we may enter
    // and scroll many times.
    if (mRestorePos != mLastPos /* GetLogicalScrollPosition() */) {
      nsPoint scrollToPos = mRestorePos;
      if (!IsLTR())
        // convert from logical to physical scroll position
        scrollToPos.x = mScrollPort.x -
          (mScrollPort.XMost() - scrollToPos.x - mScrolledFrame->GetRect().width);
      nsWeakFrame weakFrame(mOuter);
      ScrollTo(scrollToPos, nsIScrollableFrame::INSTANT);
      if (!weakFrame.IsAlive()) {
        return;
      }
      // Re-get the scroll position, it might not be exactly equal to
      // mRestorePos due to rounding and clamping.
      mLastPos = GetLogicalScrollPosition();
    } else {
      // if we reached the position then stop
      mRestorePos.y = -1;
      mLastPos.x = -1;
      mLastPos.y = -1;
    }
  } else {
    // user moved the position, so we won't need to restore
    mLastPos.x = -1;
    mLastPos.y = -1;
  }
}

nsresult
ScrollFrameHelper::FireScrollPortEvent()
{
  mAsyncScrollPortEvent.Forget();

  // Keep this in sync with PostOverflowEvent().
  nsSize scrollportSize = mScrollPort.Size();
  nsSize childSize = GetScrolledRect().Size();

  bool newVerticalOverflow = childSize.height > scrollportSize.height;
  bool vertChanged = mVerticalOverflow != newVerticalOverflow;

  bool newHorizontalOverflow = childSize.width > scrollportSize.width;
  bool horizChanged = mHorizontalOverflow != newHorizontalOverflow;

  if (!vertChanged && !horizChanged) {
    return NS_OK;
  }

  // If both either overflowed or underflowed then we dispatch only one
  // DOM event.
  bool both = vertChanged && horizChanged &&
                newVerticalOverflow == newHorizontalOverflow;
  InternalScrollPortEvent::orientType orient;
  if (both) {
    orient = InternalScrollPortEvent::both;
    mHorizontalOverflow = newHorizontalOverflow;
    mVerticalOverflow = newVerticalOverflow;
  }
  else if (vertChanged) {
    orient = InternalScrollPortEvent::vertical;
    mVerticalOverflow = newVerticalOverflow;
    if (horizChanged) {
      // We need to dispatch a separate horizontal DOM event. Do that the next
      // time around since dispatching the vertical DOM event might destroy
      // the frame.
      PostOverflowEvent();
    }
  }
  else {
    orient = InternalScrollPortEvent::horizontal;
    mHorizontalOverflow = newHorizontalOverflow;
  }

  InternalScrollPortEvent event(true,
    (orient == InternalScrollPortEvent::horizontal ? mHorizontalOverflow :
                                                     mVerticalOverflow) ?
    NS_SCROLLPORT_OVERFLOW : NS_SCROLLPORT_UNDERFLOW, nullptr);
  event.orient = orient;
  return EventDispatcher::Dispatch(mOuter->GetContent(),
                                   mOuter->PresContext(), &event);
}

void
ScrollFrameHelper::ReloadChildFrames()
{
  mScrolledFrame = nullptr;
  mHScrollbarBox = nullptr;
  mVScrollbarBox = nullptr;
  mScrollCornerBox = nullptr;
  mResizerBox = nullptr;

  nsIFrame* frame = mOuter->GetFirstPrincipalChild();
  while (frame) {
    nsIContent* content = frame->GetContent();
    if (content == mOuter->GetContent()) {
      NS_ASSERTION(!mScrolledFrame, "Already found the scrolled frame");
      mScrolledFrame = frame;
    } else {
      nsAutoString value;
      content->GetAttr(kNameSpaceID_None, nsGkAtoms::orient, value);
      if (!value.IsEmpty()) {
        // probably a scrollbar then
        if (value.LowerCaseEqualsLiteral("horizontal")) {
          NS_ASSERTION(!mHScrollbarBox, "Found multiple horizontal scrollbars?");
          mHScrollbarBox = frame;
        } else {
          NS_ASSERTION(!mVScrollbarBox, "Found multiple vertical scrollbars?");
          mVScrollbarBox = frame;
        }
      } else if (content->Tag() == nsGkAtoms::resizer) {
        NS_ASSERTION(!mResizerBox, "Found multiple resizers");
        mResizerBox = frame;
      } else if (content->Tag() == nsGkAtoms::scrollcorner) {
        // probably a scrollcorner
        NS_ASSERTION(!mScrollCornerBox, "Found multiple scrollcorners");
        mScrollCornerBox = frame;
      }
    }

    frame = frame->GetNextSibling();
  }
}

nsresult
ScrollFrameHelper::CreateAnonymousContent(
  nsTArray<nsIAnonymousContentCreator::ContentInfo>& aElements)
{
  nsPresContext* presContext = mOuter->PresContext();
  nsIFrame* parent = mOuter->GetParent();

  // Don't create scrollbars if we're an SVG document being used as an image,
  // or if we're printing/print previewing.
  // (In the printing case, we allow scrollbars if this is the child of the
  // viewport & paginated scrolling is enabled, because then we must be the
  // scroll frame for the print preview window, & that does need scrollbars.)
  if (presContext->Document()->IsBeingUsedAsImage() ||
      (!presContext->IsDynamic() &&
       !(mIsRoot && presContext->HasPaginatedScrolling()))) {
    mNeverHasVerticalScrollbar = mNeverHasHorizontalScrollbar = true;
    return NS_OK;
  }

  // Check if the frame is resizable.
  int8_t resizeStyle = mOuter->StyleDisplay()->mResize;
  bool isResizable = resizeStyle != NS_STYLE_RESIZE_NONE;

  nsIScrollableFrame *scrollable = do_QueryFrame(mOuter);

  // If we're the scrollframe for the root, then we want to construct
  // our scrollbar frames no matter what.  That way later dynamic
  // changes to propagated overflow styles will show or hide
  // scrollbars on the viewport without requiring frame reconstruction
  // of the viewport (good!).
  bool canHaveHorizontal;
  bool canHaveVertical;
  if (!mIsRoot) {
    ScrollbarStyles styles = scrollable->GetScrollbarStyles();
    canHaveHorizontal = styles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN;
    canHaveVertical = styles.mVertical != NS_STYLE_OVERFLOW_HIDDEN;
    if (!canHaveHorizontal && !canHaveVertical && !isResizable) {
      // Nothing to do.
      return NS_OK;
    }
  } else {
    canHaveHorizontal = true;
    canHaveVertical = true;
  }

  // The anonymous <div> used by <inputs> never gets scrollbars.
  nsITextControlFrame* textFrame = do_QueryFrame(parent);
  if (textFrame) {
    // Make sure we are not a text area.
    nsCOMPtr<nsIDOMHTMLTextAreaElement> textAreaElement(do_QueryInterface(parent->GetContent()));
    if (!textAreaElement) {
      mNeverHasVerticalScrollbar = mNeverHasHorizontalScrollbar = true;
      return NS_OK;
    }
  }

  nsNodeInfoManager *nodeInfoManager =
    presContext->Document()->NodeInfoManager();
  nsRefPtr<NodeInfo> nodeInfo;
  nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollbar, nullptr,
                                          kNameSpaceID_XUL,
                                          nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  if (canHaveHorizontal) {
    nsRefPtr<NodeInfo> ni = nodeInfo;
    NS_TrustedNewXULElement(getter_AddRefs(mHScrollbarContent), ni.forget());
    mHScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                NS_LITERAL_STRING("horizontal"), false);
    mHScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::clickthrough,
                                NS_LITERAL_STRING("always"), false);
    if (mIsRoot) {
      mHScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::root_,
                                  NS_LITERAL_STRING("true"), false);
    }
    if (!aElements.AppendElement(mHScrollbarContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (canHaveVertical) {
    nsRefPtr<NodeInfo> ni = nodeInfo;
    NS_TrustedNewXULElement(getter_AddRefs(mVScrollbarContent), ni.forget());
    mVScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                NS_LITERAL_STRING("vertical"), false);
    mVScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::clickthrough,
                                NS_LITERAL_STRING("always"), false);
    if (mIsRoot) {
      mVScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::root_,
                                  NS_LITERAL_STRING("true"), false);
    }
    if (!aElements.AppendElement(mVScrollbarContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (isResizable) {
    nsRefPtr<NodeInfo> nodeInfo;
    nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::resizer, nullptr,
                                            kNameSpaceID_XUL,
                                            nsIDOMNode::ELEMENT_NODE);
    NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

    NS_TrustedNewXULElement(getter_AddRefs(mResizerContent), nodeInfo.forget());

    nsAutoString dir;
    switch (resizeStyle) {
      case NS_STYLE_RESIZE_HORIZONTAL:
        if (IsScrollbarOnRight()) {
          dir.AssignLiteral("right");
        }
        else {
          dir.AssignLiteral("left");
        }
        break;
      case NS_STYLE_RESIZE_VERTICAL:
        dir.AssignLiteral("bottom");
        break;
      case NS_STYLE_RESIZE_BOTH:
        dir.AssignLiteral("bottomend");
        break;
      default:
        NS_WARNING("only resizable types should have resizers");
    }
    mResizerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, dir, false);

    if (mIsRoot) {
      nsIContent* browserRoot = GetBrowserRoot(mOuter->GetContent());
      mCollapsedResizer = !(browserRoot &&
                            browserRoot->HasAttr(kNameSpaceID_None, nsGkAtoms::showresizer));
    }
    else {
      mResizerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::element,
                                    NS_LITERAL_STRING("_parent"), false);
    }

    mResizerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::clickthrough,
                                  NS_LITERAL_STRING("always"), false);

    if (!aElements.AppendElement(mResizerContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (canHaveHorizontal && canHaveVertical) {
    nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollcorner, nullptr,
                                            kNameSpaceID_XUL,
                                            nsIDOMNode::ELEMENT_NODE);
    NS_TrustedNewXULElement(getter_AddRefs(mScrollCornerContent), nodeInfo.forget());
    if (!aElements.AppendElement(mScrollCornerContent))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void
ScrollFrameHelper::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                                uint32_t aFilter)
{
  if (mHScrollbarContent) {
    aElements.AppendElement(mHScrollbarContent);
  }

  if (mVScrollbarContent) {
    aElements.AppendElement(mVScrollbarContent);
  }

  if (mScrollCornerContent) {
    aElements.AppendElement(mScrollCornerContent);
  }

  if (mResizerContent) {
    aElements.AppendElement(mResizerContent);
  }
}

void
ScrollFrameHelper::Destroy()
{
  if (mScrollbarActivity) {
    mScrollbarActivity->Destroy();
    mScrollbarActivity = nullptr;
  }

  // Unbind any content created in CreateAnonymousContent from the tree
  nsContentUtils::DestroyAnonymousContent(&mHScrollbarContent);
  nsContentUtils::DestroyAnonymousContent(&mVScrollbarContent);
  nsContentUtils::DestroyAnonymousContent(&mScrollCornerContent);
  nsContentUtils::DestroyAnonymousContent(&mResizerContent);

  if (mPostedReflowCallback) {
    mOuter->PresContext()->PresShell()->CancelReflowCallback(this);
    mPostedReflowCallback = false;
  }
}

/**
 * Called when we want to update the scrollbar position, either because scrolling happened
 * or the user moved the scrollbar position and we need to undo that (e.g., when the user
 * clicks to scroll and we're using smooth scrolling, so we need to put the thumb back
 * to its initial position for the start of the smooth sequence).
 */
void
ScrollFrameHelper::UpdateScrollbarPosition()
{
  nsWeakFrame weakFrame(mOuter);
  mFrameIsUpdatingScrollbar = true;

  nsPoint pt = GetScrollPosition();
  if (mVScrollbarBox) {
    SetCoordAttribute(mVScrollbarBox->GetContent(), nsGkAtoms::curpos,
                      pt.y - GetScrolledRect().y);
    if (!weakFrame.IsAlive()) {
      return;
    }
  }
  if (mHScrollbarBox) {
    SetCoordAttribute(mHScrollbarBox->GetContent(), nsGkAtoms::curpos,
                      pt.x - GetScrolledRect().x);
    if (!weakFrame.IsAlive()) {
      return;
    }
  }

  mFrameIsUpdatingScrollbar = false;
}

void ScrollFrameHelper::CurPosAttributeChanged(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "aContent must not be null");
  NS_ASSERTION((mHScrollbarBox && mHScrollbarBox->GetContent() == aContent) ||
               (mVScrollbarBox && mVScrollbarBox->GetContent() == aContent),
               "unexpected child");

  // Attribute changes on the scrollbars happen in one of three ways:
  // 1) The scrollbar changed the attribute in response to some user event
  // 2) We changed the attribute in response to a ScrollPositionDidChange
  // callback from the scrolling view
  // 3) We changed the attribute to adjust the scrollbars for the start
  // of a smooth scroll operation
  //
  // In cases 2 and 3 we do not need to scroll because we're just
  // updating our scrollbar.
  if (mFrameIsUpdatingScrollbar)
    return;

  nsRect scrolledRect = GetScrolledRect();

  nsPoint current = GetScrollPosition() - scrolledRect.TopLeft();
  nsPoint dest;
  nsRect allowedRange;
  dest.x = GetCoordAttribute(mHScrollbarBox, nsGkAtoms::curpos, current.x,
                             &allowedRange.x, &allowedRange.width);
  dest.y = GetCoordAttribute(mVScrollbarBox, nsGkAtoms::curpos, current.y,
                             &allowedRange.y, &allowedRange.height);
  current += scrolledRect.TopLeft();
  dest += scrolledRect.TopLeft();
  allowedRange += scrolledRect.TopLeft();

  // Don't try to scroll if we're already at an acceptable place.
  // Don't call Contains here since Contains returns false when the point is
  // on the bottom or right edge of the rectangle.
  if (allowedRange.ClampPoint(current) == current) {
    return;
  }

  if (mScrollbarActivity) {
    nsRefPtr<ScrollbarActivity> scrollbarActivity(mScrollbarActivity);
    scrollbarActivity->ActivityOccurred();
  }

  bool isSmooth = aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::smooth);
  if (isSmooth) {
    // Make sure an attribute-setting callback occurs even if the view
    // didn't actually move yet.  We need to make sure other listeners
    // see that the scroll position is not (yet) what they thought it
    // was.
    nsWeakFrame weakFrame(mOuter);
    UpdateScrollbarPosition();
    if (!weakFrame.IsAlive()) {
      return;
    }
  }
  ScrollToWithOrigin(dest,
                     isSmooth ? nsIScrollableFrame::SMOOTH : nsIScrollableFrame::INSTANT,
                     nsGkAtoms::scrollbars, &allowedRange);
  // 'this' might be destroyed here
}

/* ============= Scroll events ========== */

NS_IMETHODIMP
ScrollFrameHelper::ScrollEvent::Run()
{
  if (mHelper)
    mHelper->FireScrollEvent();
  return NS_OK;
}

void
ScrollFrameHelper::FireScrollEvent()
{
  mScrollEvent.Forget();

  WidgetGUIEvent event(true, NS_SCROLL_EVENT, nullptr);
  nsEventStatus status = nsEventStatus_eIgnore;
  nsIContent* content = mOuter->GetContent();
  nsPresContext* prescontext = mOuter->PresContext();
  // Fire viewport scroll events at the document (where they
  // will bubble to the window)
  if (mIsRoot) {
    nsIDocument* doc = content->GetCurrentDoc();
    if (doc) {
      EventDispatcher::Dispatch(doc, prescontext, &event, nullptr,  &status);
    }
  } else {
    // scroll events fired at elements don't bubble (although scroll events
    // fired at documents do, to the window)
    event.mFlags.mBubbles = false;
    EventDispatcher::Dispatch(content, prescontext, &event, nullptr, &status);
  }
}

void
ScrollFrameHelper::PostScrollEvent()
{
  if (mScrollEvent.IsPending())
    return;

  nsRootPresContext* rpc = mOuter->PresContext()->GetRootPresContext();
  if (!rpc)
    return;
  mScrollEvent = new ScrollEvent(this);
  rpc->AddWillPaintObserver(mScrollEvent.get());
}

NS_IMETHODIMP
ScrollFrameHelper::AsyncScrollPortEvent::Run()
{
  if (mHelper) {
    mHelper->mOuter->PresContext()->GetPresShell()->
      FlushPendingNotifications(Flush_InterruptibleLayout);
  }
  return mHelper ? mHelper->FireScrollPortEvent() : NS_OK;
}

bool
nsXULScrollFrame::AddHorizontalScrollbar(nsBoxLayoutState& aState, bool aOnBottom)
{
  if (!mHelper.mHScrollbarBox)
    return true;

  return AddRemoveScrollbar(aState, aOnBottom, true, true);
}

bool
nsXULScrollFrame::AddVerticalScrollbar(nsBoxLayoutState& aState, bool aOnRight)
{
  if (!mHelper.mVScrollbarBox)
    return true;

  return AddRemoveScrollbar(aState, aOnRight, false, true);
}

void
nsXULScrollFrame::RemoveHorizontalScrollbar(nsBoxLayoutState& aState, bool aOnBottom)
{
  // removing a scrollbar should always fit
  DebugOnly<bool> result = AddRemoveScrollbar(aState, aOnBottom, true, false);
  NS_ASSERTION(result, "Removing horizontal scrollbar failed to fit??");
}

void
nsXULScrollFrame::RemoveVerticalScrollbar(nsBoxLayoutState& aState, bool aOnRight)
{
  // removing a scrollbar should always fit
  DebugOnly<bool> result = AddRemoveScrollbar(aState, aOnRight, false, false);
  NS_ASSERTION(result, "Removing vertical scrollbar failed to fit??");
}

bool
nsXULScrollFrame::AddRemoveScrollbar(nsBoxLayoutState& aState,
                                     bool aOnRightOrBottom, bool aHorizontal, bool aAdd)
{
  if (aHorizontal) {
     if (mHelper.mNeverHasHorizontalScrollbar || !mHelper.mHScrollbarBox)
       return false;

     nsSize hSize = mHelper.mHScrollbarBox->GetPrefSize(aState);
     nsBox::AddMargin(mHelper.mHScrollbarBox, hSize);

     mHelper.SetScrollbarVisibility(mHelper.mHScrollbarBox, aAdd);

     bool hasHorizontalScrollbar;
     bool fit = AddRemoveScrollbar(hasHorizontalScrollbar,
                                     mHelper.mScrollPort.y,
                                     mHelper.mScrollPort.height,
                                     hSize.height, aOnRightOrBottom, aAdd);
     mHelper.mHasHorizontalScrollbar = hasHorizontalScrollbar;    // because mHasHorizontalScrollbar is a bool
     if (!fit)
        mHelper.SetScrollbarVisibility(mHelper.mHScrollbarBox, !aAdd);

     return fit;
  } else {
     if (mHelper.mNeverHasVerticalScrollbar || !mHelper.mVScrollbarBox)
       return false;

     nsSize vSize = mHelper.mVScrollbarBox->GetPrefSize(aState);
     nsBox::AddMargin(mHelper.mVScrollbarBox, vSize);

     mHelper.SetScrollbarVisibility(mHelper.mVScrollbarBox, aAdd);

     bool hasVerticalScrollbar;
     bool fit = AddRemoveScrollbar(hasVerticalScrollbar,
                                     mHelper.mScrollPort.x,
                                     mHelper.mScrollPort.width,
                                     vSize.width, aOnRightOrBottom, aAdd);
     mHelper.mHasVerticalScrollbar = hasVerticalScrollbar;    // because mHasVerticalScrollbar is a bool
     if (!fit)
        mHelper.SetScrollbarVisibility(mHelper.mVScrollbarBox, !aAdd);

     return fit;
  }
}

bool
nsXULScrollFrame::AddRemoveScrollbar(bool& aHasScrollbar, nscoord& aXY,
                                     nscoord& aSize, nscoord aSbSize,
                                     bool aOnRightOrBottom, bool aAdd)
{
   nscoord size = aSize;
   nscoord xy = aXY;

   if (size != NS_INTRINSICSIZE) {
     if (aAdd) {
        size -= aSbSize;
        if (!aOnRightOrBottom && size >= 0)
          xy += aSbSize;
     } else {
        size += aSbSize;
        if (!aOnRightOrBottom)
          xy -= aSbSize;
     }
   }

   // not enough room? Yes? Return true.
   if (size >= 0) {
       aHasScrollbar = aAdd;
       aSize = size;
       aXY = xy;
       return true;
   }

   aHasScrollbar = false;
   return false;
}

void
nsXULScrollFrame::LayoutScrollArea(nsBoxLayoutState& aState,
                                   const nsPoint& aScrollPosition)
{
  uint32_t oldflags = aState.LayoutFlags();
  nsRect childRect = nsRect(mHelper.mScrollPort.TopLeft() - aScrollPosition,
                            mHelper.mScrollPort.Size());
  int32_t flags = NS_FRAME_NO_MOVE_VIEW;

  nsSize minSize = mHelper.mScrolledFrame->GetMinSize(aState);

  if (minSize.height > childRect.height)
    childRect.height = minSize.height;

  if (minSize.width > childRect.width)
    childRect.width = minSize.width;

  aState.SetLayoutFlags(flags);
  ClampAndSetBounds(aState, childRect, aScrollPosition);
  mHelper.mScrolledFrame->Layout(aState);

  childRect = mHelper.mScrolledFrame->GetRect();

  if (childRect.width < mHelper.mScrollPort.width ||
      childRect.height < mHelper.mScrollPort.height)
  {
    childRect.width = std::max(childRect.width, mHelper.mScrollPort.width);
    childRect.height = std::max(childRect.height, mHelper.mScrollPort.height);

    // remove overflow areas when we update the bounds,
    // because we've already accounted for it
    // REVIEW: Have we accounted for both?
    ClampAndSetBounds(aState, childRect, aScrollPosition, true);
  }

  aState.SetLayoutFlags(oldflags);

}

void ScrollFrameHelper::PostOverflowEvent()
{
  if (mAsyncScrollPortEvent.IsPending())
    return;

  // Keep this in sync with FireScrollPortEvent().
  nsSize scrollportSize = mScrollPort.Size();
  nsSize childSize = GetScrolledRect().Size();

  bool newVerticalOverflow = childSize.height > scrollportSize.height;
  bool vertChanged = mVerticalOverflow != newVerticalOverflow;

  bool newHorizontalOverflow = childSize.width > scrollportSize.width;
  bool horizChanged = mHorizontalOverflow != newHorizontalOverflow;

  if (!vertChanged && !horizChanged) {
    return;
  }

  nsRootPresContext* rpc = mOuter->PresContext()->GetRootPresContext();
  if (!rpc)
    return;

  mAsyncScrollPortEvent = new AsyncScrollPortEvent(this);
  rpc->AddWillPaintObserver(mAsyncScrollPortEvent.get());
}

bool
ScrollFrameHelper::IsLTR() const
{
  //TODO make bidi code set these from preferences

  nsIFrame *frame = mOuter;
  // XXX This is a bit on the slow side.
  if (mIsRoot) {
    // If we're the root scrollframe, we need the root element's style data.
    nsPresContext *presContext = mOuter->PresContext();
    nsIDocument *document = presContext->Document();
    Element *root = document->GetRootElement();

    // But for HTML and XHTML we want the body element.
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(document);
    if (htmlDoc) {
      Element *bodyElement = document->GetBodyElement();
      if (bodyElement)
        root = bodyElement; // we can trust the document to hold on to it
    }

    if (root) {
      nsIFrame *rootsFrame = root->GetPrimaryFrame();
      if (rootsFrame)
        frame = rootsFrame;
    }
  }

  return frame->StyleVisibility()->mDirection != NS_STYLE_DIRECTION_RTL;
}

bool
ScrollFrameHelper::IsScrollbarOnRight() const
{
  nsPresContext *presContext = mOuter->PresContext();

  // The position of the scrollbar in top-level windows depends on the pref
  // layout.scrollbar.side. For non-top-level elements, it depends only on the
  // directionaliy of the element (equivalent to a value of "1" for the pref).
  if (!mIsRoot)
    return IsLTR();
  switch (presContext->GetCachedIntPref(kPresContext_ScrollbarSide)) {
    default:
    case 0: // UI directionality
      return presContext->GetCachedIntPref(kPresContext_BidiDirection)
             == IBMBIDI_TEXTDIRECTION_LTR;
    case 1: // Document / content directionality
      return IsLTR();
    case 2: // Always right
      return true;
    case 3: // Always left
      return false;
  }
}

/**
 * Reflow the scroll area if it needs it and return its size. Also determine if the reflow will
 * cause any of the scrollbars to need to be reflowed.
 */
nsresult
nsXULScrollFrame::Layout(nsBoxLayoutState& aState)
{
  bool scrollbarRight = mHelper.IsScrollbarOnRight();
  bool scrollbarBottom = true;

  // get the content rect
  nsRect clientRect(0,0,0,0);
  GetClientRect(clientRect);

  nsRect oldScrollAreaBounds = mHelper.mScrollPort;
  nsPoint oldScrollPosition = mHelper.GetLogicalScrollPosition();

  // the scroll area size starts off as big as our content area
  mHelper.mScrollPort = clientRect;

  /**************
   Our basic strategy here is to first try laying out the content with
   the scrollbars in their current state. We're hoping that that will
   just "work"; the content will overflow wherever there's a scrollbar
   already visible. If that does work, then there's no need to lay out
   the scrollarea. Otherwise we fix up the scrollbars; first we add a
   vertical one to scroll the content if necessary, or remove it if
   it's not needed. Then we reflow the content if the scrollbar
   changed.  Then we add a horizontal scrollbar if necessary (or
   remove if not needed), and if that changed, we reflow the content
   again. At this point, any scrollbars that are needed to scroll the
   content have been added.

   In the second phase we check to see if any scrollbars are too small
   to display, and if so, we remove them. We check the horizontal
   scrollbar first; removing it might make room for the vertical
   scrollbar, and if we have room for just one scrollbar we'll save
   the vertical one.

   Finally we position and size the scrollbars and scrollcorner (the
   square that is needed in the corner of the window when two
   scrollbars are visible), and reflow any fixed position views
   (if we're the viewport and we added or removed a scrollbar).
   **************/

  ScrollbarStyles styles = GetScrollbarStyles();

  // Look at our style do we always have vertical or horizontal scrollbars?
  if (styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL)
     mHelper.mHasHorizontalScrollbar = true;
  if (styles.mVertical == NS_STYLE_OVERFLOW_SCROLL)
     mHelper.mHasVerticalScrollbar = true;

  if (mHelper.mHasHorizontalScrollbar)
     AddHorizontalScrollbar(aState, scrollbarBottom);

  if (mHelper.mHasVerticalScrollbar)
     AddVerticalScrollbar(aState, scrollbarRight);

  // layout our the scroll area
  LayoutScrollArea(aState, oldScrollPosition);

  // now look at the content area and see if we need scrollbars or not
  bool needsLayout = false;

  // if we have 'auto' scrollbars look at the vertical case
  if (styles.mVertical != NS_STYLE_OVERFLOW_SCROLL) {
    // These are only good until the call to LayoutScrollArea.
    nsRect scrolledRect = mHelper.GetScrolledRect();

    // There are two cases to consider
      if (scrolledRect.height <= mHelper.mScrollPort.height
          || styles.mVertical != NS_STYLE_OVERFLOW_AUTO) {
        if (mHelper.mHasVerticalScrollbar) {
          // We left room for the vertical scrollbar, but it's not needed;
          // remove it.
          RemoveVerticalScrollbar(aState, scrollbarRight);
          needsLayout = true;
        }
      } else {
        if (!mHelper.mHasVerticalScrollbar) {
          // We didn't leave room for the vertical scrollbar, but it turns
          // out we needed it
          if (AddVerticalScrollbar(aState, scrollbarRight))
            needsLayout = true;
        }
    }

    // ok layout at the right size
    if (needsLayout) {
       nsBoxLayoutState resizeState(aState);
       LayoutScrollArea(resizeState, oldScrollPosition);
       needsLayout = false;
    }
  }


  // if scrollbars are auto look at the horizontal case
  if (styles.mHorizontal != NS_STYLE_OVERFLOW_SCROLL)
  {
    // These are only good until the call to LayoutScrollArea.
    nsRect scrolledRect = mHelper.GetScrolledRect();

    // if the child is wider that the scroll area
    // and we don't have a scrollbar add one.
    if ((scrolledRect.width > mHelper.mScrollPort.width)
        && styles.mHorizontal == NS_STYLE_OVERFLOW_AUTO) {

      if (!mHelper.mHasHorizontalScrollbar) {
           // no scrollbar?
          if (AddHorizontalScrollbar(aState, scrollbarBottom))
             needsLayout = true;

           // if we added a horizontal scrollbar and we did not have a vertical
           // there is a chance that by adding the horizontal scrollbar we will
           // suddenly need a vertical scrollbar. Is a special case but its
           // important.
           //if (!mHasVerticalScrollbar && scrolledRect.height > scrollAreaRect.height - sbSize.height)
           //  printf("****Gfx Scrollbar Special case hit!!*****\n");

      }
    } else {
        // if the area is smaller or equal to and we have a scrollbar then
        // remove it.
      if (mHelper.mHasHorizontalScrollbar) {
        RemoveHorizontalScrollbar(aState, scrollbarBottom);
        needsLayout = true;
      }
    }
  }

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
     nsBoxLayoutState resizeState(aState);
     LayoutScrollArea(resizeState, oldScrollPosition);
     needsLayout = false;
  }

  // get the preferred size of the scrollbars
  nsSize hMinSize(0, 0);
  if (mHelper.mHScrollbarBox && mHelper.mHasHorizontalScrollbar) {
    GetScrollbarMetrics(aState, mHelper.mHScrollbarBox, &hMinSize, nullptr, false);
  }
  nsSize vMinSize(0, 0);
  if (mHelper.mVScrollbarBox && mHelper.mHasVerticalScrollbar) {
    GetScrollbarMetrics(aState, mHelper.mVScrollbarBox, &vMinSize, nullptr, true);
  }

  // Disable scrollbars that are too small
  // Disable horizontal scrollbar first. If we have to disable only one
  // scrollbar, we'd rather keep the vertical scrollbar.
  // Note that we always give horizontal scrollbars their preferred height,
  // never their min-height. So check that there's room for the preferred height.
  if (mHelper.mHasHorizontalScrollbar &&
      (hMinSize.width > clientRect.width - vMinSize.width
       || hMinSize.height > clientRect.height)) {
    RemoveHorizontalScrollbar(aState, scrollbarBottom);
    needsLayout = true;
  }
  // Now disable vertical scrollbar if necessary
  if (mHelper.mHasVerticalScrollbar &&
      (vMinSize.height > clientRect.height - hMinSize.height
       || vMinSize.width > clientRect.width)) {
    RemoveVerticalScrollbar(aState, scrollbarRight);
    needsLayout = true;
  }

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
    nsBoxLayoutState resizeState(aState);
    LayoutScrollArea(resizeState, oldScrollPosition);
  }

  if (!mHelper.mSupppressScrollbarUpdate) {
    mHelper.LayoutScrollbars(aState, clientRect, oldScrollAreaBounds);
  }
  if (!mHelper.mPostedReflowCallback) {
    // Make sure we'll try scrolling to restored position
    PresContext()->PresShell()->PostReflowCallback(&mHelper);
    mHelper.mPostedReflowCallback = true;
  }
  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    mHelper.mHadNonInitialReflow = true;
  }

  mHelper.UpdateSticky();

  // Set up overflow areas for block frames for the benefit of
  // text-overflow.
  nsIFrame* f = mHelper.mScrolledFrame->GetContentInsertionFrame();
  if (nsLayoutUtils::GetAsBlock(f)) {
    nsRect origRect = f->GetRect();
    nsRect clippedRect = origRect;
    clippedRect.MoveBy(mHelper.mScrollPort.TopLeft());
    clippedRect.IntersectRect(clippedRect, mHelper.mScrollPort);
    nsOverflowAreas overflow = f->GetOverflowAreas();
    f->FinishAndStoreOverflow(overflow, clippedRect.Size());
    clippedRect.MoveTo(origRect.TopLeft());
    f->SetRect(clippedRect);
  }

  mHelper.PostOverflowEvent();
  return NS_OK;
}

void
ScrollFrameHelper::FinishReflowForScrollbar(nsIContent* aContent,
                                                nscoord aMinXY, nscoord aMaxXY,
                                                nscoord aCurPosXY,
                                                nscoord aPageIncrement,
                                                nscoord aIncrement)
{
  // Scrollbars assume zero is the minimum position, so translate for them.
  SetCoordAttribute(aContent, nsGkAtoms::curpos, aCurPosXY - aMinXY);
  SetScrollbarEnabled(aContent, aMaxXY - aMinXY);
  SetCoordAttribute(aContent, nsGkAtoms::maxpos, aMaxXY - aMinXY);
  SetCoordAttribute(aContent, nsGkAtoms::pageincrement, aPageIncrement);
  SetCoordAttribute(aContent, nsGkAtoms::increment, aIncrement);
}

bool
ScrollFrameHelper::ReflowFinished()
{
  nsAutoScriptBlocker scriptBlocker;
  mPostedReflowCallback = false;

  ScrollToRestoredPosition();

  // Clamp current scroll position to new bounds. Normally this won't
  // do anything.
  nsPoint currentScrollPos = GetScrollPosition();
  ScrollToImpl(currentScrollPos, nsRect(currentScrollPos, nsSize(0, 0)));
  if (!mAsyncScroll && !mAsyncSmoothMSDScroll) {
    // We need to have mDestination track the current scroll position,
    // in case it falls outside the new reflow area. mDestination is used
    // by ScrollBy as its starting position.
    mDestination = GetScrollPosition();
  }

  if (NS_SUBTREE_DIRTY(mOuter) || !mUpdateScrollbarAttributes)
    return false;

  mUpdateScrollbarAttributes = false;

  // Update scrollbar attributes.
  nsPresContext* presContext = mOuter->PresContext();

  if (mMayHaveDirtyFixedChildren) {
    mMayHaveDirtyFixedChildren = false;
    nsIFrame* parentFrame = mOuter->GetParent();
    for (nsIFrame* fixedChild =
           parentFrame->GetFirstChild(nsIFrame::kFixedList);
         fixedChild; fixedChild = fixedChild->GetNextSibling()) {
      // force a reflow of the fixed child
      presContext->PresShell()->
        FrameNeedsReflow(fixedChild, nsIPresShell::eResize,
                         NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }

  nsRect scrolledContentRect = GetScrolledRect();
  nsSize scrollClampingScrollPort = GetScrollPositionClampingScrollPortSize();
  nscoord minX = scrolledContentRect.x;
  nscoord maxX = scrolledContentRect.XMost() - scrollClampingScrollPort.width;
  nscoord minY = scrolledContentRect.y;
  nscoord maxY = scrolledContentRect.YMost() - scrollClampingScrollPort.height;

  // Suppress handling of the curpos attribute changes we make here.
  NS_ASSERTION(!mFrameIsUpdatingScrollbar, "We shouldn't be reentering here");
  mFrameIsUpdatingScrollbar = true;

  nsCOMPtr<nsIContent> vScroll =
    mVScrollbarBox ? mVScrollbarBox->GetContent() : nullptr;
  nsCOMPtr<nsIContent> hScroll =
    mHScrollbarBox ? mHScrollbarBox->GetContent() : nullptr;

  // Note, in some cases mOuter may get deleted while finishing reflow
  // for scrollbars. XXXmats is this still true now that we have a script
  // blocker in this scope? (if not, remove the weak frame checks below).
  if (vScroll || hScroll) {
    nsWeakFrame weakFrame(mOuter);
    nsPoint scrollPos = GetScrollPosition();
    nsSize lineScrollAmount = GetLineScrollAmount();
    if (vScroll) {
      const double kScrollMultiplier =
        Preferences::GetInt("toolkit.scrollbox.verticalScrollDistance",
                            NS_DEFAULT_VERTICAL_SCROLL_DISTANCE);
      nscoord increment = lineScrollAmount.height * kScrollMultiplier;
      // We normally use (scrollArea.height - increment) for height
      // of page scrolling.  However, it is too small when
      // increment is very large. (If increment is larger than
      // scrollArea.height, direction of scrolling will be opposite).
      // To avoid it, we use (float(scrollArea.height) * 0.8) as
      // lower bound value of height of page scrolling. (bug 383267)
      // XXX shouldn't we use GetPageScrollAmount here?
      nscoord pageincrement = nscoord(scrollClampingScrollPort.height - increment);
      nscoord pageincrementMin = nscoord(float(scrollClampingScrollPort.height) * 0.8);
      FinishReflowForScrollbar(vScroll, minY, maxY, scrollPos.y,
                               std::max(pageincrement, pageincrementMin),
                               increment);
    }
    if (hScroll) {
      const double kScrollMultiplier =
        Preferences::GetInt("toolkit.scrollbox.horizontalScrollDistance",
                            NS_DEFAULT_HORIZONTAL_SCROLL_DISTANCE);
      nscoord increment = lineScrollAmount.width * kScrollMultiplier;
      FinishReflowForScrollbar(hScroll, minX, maxX, scrollPos.x,
                               nscoord(float(scrollClampingScrollPort.width) * 0.8),
                               increment);
    }
    NS_ENSURE_TRUE(weakFrame.IsAlive(), false);
  }

  mFrameIsUpdatingScrollbar = false;
  // We used to rely on the curpos attribute changes above to scroll the
  // view.  However, for scrolling to the left of the viewport, we
  // rescale the curpos attribute, which means that operations like
  // resizing the window while it is scrolled all the way to the left
  // hold the curpos attribute constant at 0 while still requiring
  // scrolling.  So we suppress the effect of the changes above with
  // mFrameIsUpdatingScrollbar and call CurPosAttributeChanged here.
  // (It actually even works some of the time without this, thanks to
  // nsSliderFrame::AttributeChanged's handling of maxpos, but not when
  // we hide the scrollbar on a large size change, such as
  // maximization.)
  if (!mHScrollbarBox && !mVScrollbarBox)
    return false;
  CurPosAttributeChanged(mVScrollbarBox ? mVScrollbarBox->GetContent()
                                        : mHScrollbarBox->GetContent());
  return true;
}

void
ScrollFrameHelper::ReflowCallbackCanceled()
{
  mPostedReflowCallback = false;
}

bool
ScrollFrameHelper::UpdateOverflow()
{
  nsIScrollableFrame* sf = do_QueryFrame(mOuter);
  ScrollbarStyles ss = sf->GetScrollbarStyles();

  if (ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN ||
      ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN ||
      GetScrollPosition() != nsPoint()) {
    // If there are scrollbars, or we're not at the beginning of the pane,
    // the scroll position may change. In this case, mark the frame as
    // needing reflow. Don't use NS_FRAME_IS_DIRTY as dirty as that means
    // we have to reflow the frame and all its descendants, and we don't
    // have to do that here. Only this frame needs to be reflowed.
    mOuter->PresContext()->PresShell()->FrameNeedsReflow(
      mOuter, nsIPresShell::eResize, NS_FRAME_HAS_DIRTY_CHILDREN);
    // Ensure that next time nsHTMLScrollFrame::Reflow runs, we don't skip
    // updating the scrollbars. (Because the overflow area of the scrolled
    // frame has probably just been updated, Reflow won't see it change.)
    mSkippedScrollbarLayout = true;
    return false;  // reflowing will update overflow
  }
  PostOverflowEvent();
  return mOuter->nsContainerFrame::UpdateOverflow();
}

void
ScrollFrameHelper::UpdateSticky()
{
  StickyScrollContainer* ssc = StickyScrollContainer::
    GetStickyScrollContainerForScrollFrame(mOuter);
  if (ssc) {
    nsIScrollableFrame* scrollFrame = do_QueryFrame(mOuter);
    ssc->UpdatePositions(scrollFrame->GetScrollPosition(), mOuter);
  }
}

void
ScrollFrameHelper::AdjustScrollbarRectForResizer(
                         nsIFrame* aFrame, nsPresContext* aPresContext,
                         nsRect& aRect, bool aHasResizer, bool aVertical)
{
  if ((aVertical ? aRect.width : aRect.height) == 0)
    return;

  // if a content resizer is present, use its size. Otherwise, check if the
  // widget has a resizer.
  nsRect resizerRect;
  if (aHasResizer) {
    resizerRect = mResizerBox->GetRect();
  }
  else {
    nsPoint offset;
    nsIWidget* widget = aFrame->GetNearestWidget(offset);
    nsIntRect widgetRect;
    if (!widget || !widget->ShowsResizeIndicator(&widgetRect))
      return;

    resizerRect = nsRect(aPresContext->DevPixelsToAppUnits(widgetRect.x) - offset.x,
                         aPresContext->DevPixelsToAppUnits(widgetRect.y) - offset.y,
                         aPresContext->DevPixelsToAppUnits(widgetRect.width),
                         aPresContext->DevPixelsToAppUnits(widgetRect.height));
  }

  if (!resizerRect.Contains(aRect.BottomRight() - nsPoint(1, 1)))
    return;

  if (aVertical)
    aRect.height = std::max(0, resizerRect.y - aRect.y);
  else
    aRect.width = std::max(0, resizerRect.x - aRect.x);
}

static void
AdjustOverlappingScrollbars(nsRect& aVRect, nsRect& aHRect)
{
  if (aVRect.IsEmpty() || aHRect.IsEmpty())
    return;

  const nsRect oldVRect = aVRect;
  const nsRect oldHRect = aHRect;
  if (oldVRect.Contains(oldHRect.BottomRight() - nsPoint(1, 1))) {
    aHRect.width = std::max(0, oldVRect.x - oldHRect.x);
  } else if (oldVRect.Contains(oldHRect.BottomLeft() - nsPoint(0, 1))) {
    nscoord overlap = std::min(oldHRect.width, oldVRect.XMost() - oldHRect.x);
    aHRect.x += overlap;
    aHRect.width -= overlap;
  }
  if (oldHRect.Contains(oldVRect.BottomRight() - nsPoint(1, 1))) {
    aVRect.height = std::max(0, oldHRect.y - oldVRect.y);
  }
}

void
ScrollFrameHelper::LayoutScrollbars(nsBoxLayoutState& aState,
                                        const nsRect& aContentArea,
                                        const nsRect& aOldScrollArea)
{
  NS_ASSERTION(!mSupppressScrollbarUpdate,
               "This should have been suppressed");

  bool hasResizer = HasResizer();
  bool scrollbarOnLeft = !IsScrollbarOnRight();

  // place the scrollcorner
  if (mScrollCornerBox || mResizerBox) {
    NS_PRECONDITION(!mScrollCornerBox || mScrollCornerBox->IsBoxFrame(), "Must be a box frame!");

    nsRect r(0, 0, 0, 0);
    if (aContentArea.x != mScrollPort.x || scrollbarOnLeft) {
      // scrollbar (if any) on left
      r.x = aContentArea.x;
      r.width = mScrollPort.x - aContentArea.x;
      NS_ASSERTION(r.width >= 0, "Scroll area should be inside client rect");
    } else {
      // scrollbar (if any) on right
      r.width = aContentArea.XMost() - mScrollPort.XMost();
      r.x = aContentArea.XMost() - r.width;
      NS_ASSERTION(r.width >= 0, "Scroll area should be inside client rect");
    }
    if (aContentArea.y != mScrollPort.y) {
      NS_ERROR("top scrollbars not supported");
    } else {
      // scrollbar (if any) on bottom
      r.height = aContentArea.YMost() - mScrollPort.YMost();
      r.y = aContentArea.YMost() - r.height;
      NS_ASSERTION(r.height >= 0, "Scroll area should be inside client rect");
    }

    if (mScrollCornerBox) {
      nsBoxFrame::LayoutChildAt(aState, mScrollCornerBox, r);
    }

    if (hasResizer) {
      // if a resizer is present, get its size. Assume a default size of 15 pixels.
      nscoord defaultSize = nsPresContext::CSSPixelsToAppUnits(15);
      nsSize resizerMinSize = mResizerBox->GetMinSize(aState);

      nscoord vScrollbarWidth = mVScrollbarBox ?
        mVScrollbarBox->GetPrefSize(aState).width : defaultSize;
      r.width = std::max(std::max(r.width, vScrollbarWidth), resizerMinSize.width);
      if (aContentArea.x == mScrollPort.x && !scrollbarOnLeft) {
        r.x = aContentArea.XMost() - r.width;
      }

      nscoord hScrollbarHeight = mHScrollbarBox ?
        mHScrollbarBox->GetPrefSize(aState).height : defaultSize;
      r.height = std::max(std::max(r.height, hScrollbarHeight), resizerMinSize.height);
      if (aContentArea.y == mScrollPort.y) {
        r.y = aContentArea.YMost() - r.height;
      }

      nsBoxFrame::LayoutChildAt(aState, mResizerBox, r);
    }
    else if (mResizerBox) {
      // otherwise lay out the resizer with an empty rectangle
      nsBoxFrame::LayoutChildAt(aState, mResizerBox, nsRect());
    }
  }

  nsPresContext* presContext = mScrolledFrame->PresContext();
  nsRect vRect;
  if (mVScrollbarBox) {
    NS_PRECONDITION(mVScrollbarBox->IsBoxFrame(), "Must be a box frame!");
    vRect = mScrollPort;
    vRect.width = aContentArea.width - mScrollPort.width;
    vRect.x = scrollbarOnLeft ? aContentArea.x : mScrollPort.XMost();
    if (mHasVerticalScrollbar) {
      nsMargin margin;
      mVScrollbarBox->GetMargin(margin);
      vRect.Deflate(margin);
    }
    AdjustScrollbarRectForResizer(mOuter, presContext, vRect, hasResizer, true);
  }

  nsRect hRect;
  if (mHScrollbarBox) {
    NS_PRECONDITION(mHScrollbarBox->IsBoxFrame(), "Must be a box frame!");
    hRect = mScrollPort;
    hRect.height = aContentArea.height - mScrollPort.height;
    hRect.y = true ? mScrollPort.YMost() : aContentArea.y;
    if (mHasHorizontalScrollbar) {
      nsMargin margin;
      mHScrollbarBox->GetMargin(margin);
      hRect.Deflate(margin);
    }
    AdjustScrollbarRectForResizer(mOuter, presContext, hRect, hasResizer, false);
  }

  if (!LookAndFeel::GetInt(LookAndFeel::eIntID_AllowOverlayScrollbarsOverlap)) {
    AdjustOverlappingScrollbars(vRect, hRect);
  }
  if (mVScrollbarBox) {
    nsBoxFrame::LayoutChildAt(aState, mVScrollbarBox, vRect);
  }
  if (mHScrollbarBox) {
    nsBoxFrame::LayoutChildAt(aState, mHScrollbarBox, hRect);
  }

  // may need to update fixed position children of the viewport,
  // if the client area changed size because of an incremental
  // reflow of a descendant.  (If the outer frame is dirty, the fixed
  // children will be re-laid out anyway)
  if (aOldScrollArea.Size() != mScrollPort.Size() &&
      !(mOuter->GetStateBits() & NS_FRAME_IS_DIRTY) &&
      mIsRoot) {
    mMayHaveDirtyFixedChildren = true;
  }

  // post reflow callback to modify scrollbar attributes
  mUpdateScrollbarAttributes = true;
  if (!mPostedReflowCallback) {
    aState.PresContext()->PresShell()->PostReflowCallback(this);
    mPostedReflowCallback = true;
  }
}

#if DEBUG
static bool ShellIsAlive(nsWeakPtr& aWeakPtr)
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(aWeakPtr));
  return !!shell;
}
#endif

void
ScrollFrameHelper::SetScrollbarEnabled(nsIContent* aContent, nscoord aMaxPos)
{
  DebugOnly<nsWeakPtr> weakShell(
    do_GetWeakReference(mOuter->PresContext()->PresShell()));
  if (aMaxPos) {
    aContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
  } else {
    aContent->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled,
                      NS_LITERAL_STRING("true"), true);
  }
  MOZ_ASSERT(ShellIsAlive(weakShell), "pres shell was destroyed by scrolling");
}

void
ScrollFrameHelper::SetCoordAttribute(nsIContent* aContent, nsIAtom* aAtom,
                                         nscoord aSize)
{
  DebugOnly<nsWeakPtr> weakShell(
    do_GetWeakReference(mOuter->PresContext()->PresShell()));
  // convert to pixels
  aSize = nsPresContext::AppUnitsToIntCSSPixels(aSize);

  // only set the attribute if it changed.

  nsAutoString newValue;
  newValue.AppendInt(aSize);

  if (aContent->AttrValueIs(kNameSpaceID_None, aAtom, newValue, eCaseMatters))
    return;

  nsWeakFrame weakFrame(mOuter);
  nsCOMPtr<nsIContent> kungFuDeathGrip = aContent;
  aContent->SetAttr(kNameSpaceID_None, aAtom, newValue, true);
  MOZ_ASSERT(ShellIsAlive(weakShell), "pres shell was destroyed by scrolling");
  if (!weakFrame.IsAlive()) {
    return;
  }

  if (mScrollbarActivity) {
    nsRefPtr<ScrollbarActivity> scrollbarActivity(mScrollbarActivity);
    scrollbarActivity->ActivityOccurred();
  }
}

static void
ReduceRadii(nscoord aXBorder, nscoord aYBorder,
            nscoord& aXRadius, nscoord& aYRadius)
{
  // In order to ensure that the inside edge of the border has no
  // curvature, we need at least one of its radii to be zero.
  if (aXRadius <= aXBorder || aYRadius <= aYBorder)
    return;

  // For any corner where we reduce the radii, preserve the corner's shape.
  double ratio = std::max(double(aXBorder) / aXRadius,
                        double(aYBorder) / aYRadius);
  aXRadius *= ratio;
  aYRadius *= ratio;
}

/**
 * Implement an override for nsIFrame::GetBorderRadii to ensure that
 * the clipping region for the border radius does not clip the scrollbars.
 *
 * In other words, we require that the border radius be reduced until the
 * inner border radius at the inner edge of the border is 0 wherever we
 * have scrollbars.
 */
bool
ScrollFrameHelper::GetBorderRadii(const nsSize& aFrameSize,
                                  const nsSize& aBorderArea,
                                  Sides aSkipSides,
                                  nscoord aRadii[8]) const
{
  if (!mOuter->nsContainerFrame::GetBorderRadii(aFrameSize, aBorderArea,
                                                aSkipSides, aRadii)) {
    return false;
  }

  // Since we can use GetActualScrollbarSizes (rather than
  // GetDesiredScrollbarSizes) since this doesn't affect reflow, we
  // probably should.
  nsMargin sb = GetActualScrollbarSizes();
  nsMargin border = mOuter->GetUsedBorder();

  if (sb.left > 0 || sb.top > 0) {
    ReduceRadii(border.left, border.top,
                aRadii[NS_CORNER_TOP_LEFT_X],
                aRadii[NS_CORNER_TOP_LEFT_Y]);
  }

  if (sb.top > 0 || sb.right > 0) {
    ReduceRadii(border.right, border.top,
                aRadii[NS_CORNER_TOP_RIGHT_X],
                aRadii[NS_CORNER_TOP_RIGHT_Y]);
  }

  if (sb.right > 0 || sb.bottom > 0) {
    ReduceRadii(border.right, border.bottom,
                aRadii[NS_CORNER_BOTTOM_RIGHT_X],
                aRadii[NS_CORNER_BOTTOM_RIGHT_Y]);
  }

  if (sb.bottom > 0 || sb.left > 0) {
    ReduceRadii(border.left, border.bottom,
                aRadii[NS_CORNER_BOTTOM_LEFT_X],
                aRadii[NS_CORNER_BOTTOM_LEFT_Y]);
  }

  return true;
}

nsRect
ScrollFrameHelper::GetScrolledRect() const
{
  nsRect result =
    GetScrolledRectInternal(mScrolledFrame->GetScrollableOverflowRect(),
                            mScrollPort.Size());

  if (result.width < mScrollPort.width) {
    NS_WARNING("Scrolled rect smaller than scrollport?");
  }
  if (result.height < mScrollPort.height) {
    NS_WARNING("Scrolled rect smaller than scrollport?");
  }
  return result;
}

nsRect
ScrollFrameHelper::GetScrolledRectInternal(const nsRect& aScrolledFrameOverflowArea,
                                               const nsSize& aScrollPortSize) const
{
  return nsLayoutUtils::GetScrolledRect(mScrolledFrame,
      aScrolledFrameOverflowArea, aScrollPortSize,
      IsLTR() ? NS_STYLE_DIRECTION_LTR : NS_STYLE_DIRECTION_RTL);
}

nsMargin
ScrollFrameHelper::GetActualScrollbarSizes() const
{
  nsRect r = mOuter->GetPaddingRect() - mOuter->GetPosition();

  return nsMargin(mScrollPort.y - r.y,
                  r.XMost() - mScrollPort.XMost(),
                  r.YMost() - mScrollPort.YMost(),
                  mScrollPort.x - r.x);
}

void
ScrollFrameHelper::SetScrollbarVisibility(nsIFrame* aScrollbar, bool aVisible)
{
  nsScrollbarFrame* scrollbar = do_QueryFrame(aScrollbar);
  if (scrollbar) {
    // See if we have a mediator.
    nsIScrollbarMediator* mediator = scrollbar->GetScrollbarMediator();
    if (mediator) {
      // Inform the mediator of the visibility change.
      mediator->VisibilityChanged(aVisible);
    }
  }
}

nscoord
ScrollFrameHelper::GetCoordAttribute(nsIFrame* aBox, nsIAtom* aAtom,
                                         nscoord aDefaultValue,
                                         nscoord* aRangeStart,
                                         nscoord* aRangeLength)
{
  if (aBox) {
    nsIContent* content = aBox->GetContent();

    nsAutoString value;
    content->GetAttr(kNameSpaceID_None, aAtom, value);
    if (!value.IsEmpty())
    {
      nsresult error;
      // convert it to appunits
      nscoord result = nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      nscoord halfPixel = nsPresContext::CSSPixelsToAppUnits(0.5f);
      // Any nscoord value that would round to the attribute value when converted
      // to CSS pixels is allowed.
      *aRangeStart = result - halfPixel;
      *aRangeLength = halfPixel*2 - 1;
      return result;
    }
  }

  // Only this exact default value is allowed.
  *aRangeStart = aDefaultValue;
  *aRangeLength = 0;
  return aDefaultValue;
}

nsPresState*
ScrollFrameHelper::SaveState() const
{
  nsIScrollbarMediator* mediator = do_QueryFrame(GetScrolledFrame());
  if (mediator) {
    // child handles its own scroll state, so don't bother saving state here
    return nullptr;
  }

  // Don't store a scroll state if we never have been scrolled or restored
  // a previous scroll state.
  if (!mHasBeenScrolled && !mDidHistoryRestore) {
    return nullptr;
  }

  nsPresState* state = new nsPresState();
  // Save mRestorePos instead of our actual current scroll position, if it's
  // valid and we haven't moved since the last update of mLastPos (same check
  // that ScrollToRestoredPosition uses). This ensures if a reframe occurs
  // while we're in the process of loading content to scroll to a restored
  // position, we'll keep trying after the reframe.
  nsPoint pt = GetLogicalScrollPosition();
  if (mRestorePos.y != -1 && pt == mLastPos) {
    pt = mRestorePos;
  }
  state->SetScrollState(pt);
  state->SetResolution(mResolution);
  return state;
}

void
ScrollFrameHelper::RestoreState(nsPresState* aState)
{
  mRestorePos = aState->GetScrollState();
  mDidHistoryRestore = true;
  mLastPos = mScrolledFrame ? GetLogicalScrollPosition() : nsPoint(0,0);
  mResolution = aState->GetResolution();
  mIsResolutionSet = true;

  if (mIsRoot) {
    mOuter->PresContext()->PresShell()->SetResolution(mResolution.width, mResolution.height);
  }
}

void
ScrollFrameHelper::PostScrolledAreaEvent()
{
  if (mScrolledAreaEvent.IsPending()) {
    return;
  }
  mScrolledAreaEvent = new ScrolledAreaEvent(this);
  nsContentUtils::AddScriptRunner(mScrolledAreaEvent.get());
}

////////////////////////////////////////////////////////////////////////////////
// ScrolledArea change event dispatch

NS_IMETHODIMP
ScrollFrameHelper::ScrolledAreaEvent::Run()
{
  if (mHelper) {
    mHelper->FireScrolledAreaEvent();
  }
  return NS_OK;
}

void
ScrollFrameHelper::FireScrolledAreaEvent()
{
  mScrolledAreaEvent.Forget();

  InternalScrollAreaEvent event(true, NS_SCROLLEDAREACHANGED, nullptr);
  nsPresContext *prescontext = mOuter->PresContext();
  nsIContent* content = mOuter->GetContent();

  event.mArea = mScrolledFrame->GetScrollableOverflowRectRelativeToParent();

  nsIDocument *doc = content->GetCurrentDoc();
  if (doc) {
    EventDispatcher::Dispatch(doc, prescontext, &event, nullptr);
  }
}

uint32_t
nsIScrollableFrame::GetPerceivedScrollingDirections() const
{
  nscoord oneDevPixel = GetScrolledFrame()->PresContext()->AppUnitsPerDevPixel();
  uint32_t directions = GetScrollbarVisibility();
  nsRect scrollRange = GetScrollRange();
  if (scrollRange.width >= oneDevPixel) {
    directions |= HORIZONTAL;
  }
  if (scrollRange.height >= oneDevPixel) {
    directions |= VERTICAL;
  }
  return directions;
}
