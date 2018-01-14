/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object to wrap rendering objects that should be scrollable */

#include "nsGfxScrollFrame.h"

#include "ActiveLayerTracker.h"
#include "base/compiler_specific.h"
#include "DisplayItemClip.h"
#include "nsCOMPtr.h"
#include "nsIContentViewer.h"
#include "nsPresContext.h"
#include "nsView.h"
#include "nsIScrollable.h"
#include "nsContainerFrame.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsIDocumentInlines.h"
#include "nsFontMetrics.h"
#include "nsBoxLayoutState.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsScrollbarFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsITextControlFrame.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsPresState.h"
#include "nsIHTMLDocument.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsBidiPresUtils.h"
#include "nsBidiUtils.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include <stdint.h>
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Telemetry.h"
#include "FrameLayerBuilder.h"
#include "nsSMILKeySpline.h"
#include "nsSubDocumentFrame.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsIObjectLoadingContent.h"
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
#include "ScrollAnimationPhysics.h"
#include "ScrollAnimationBezierPhysics.h"
#include "ScrollAnimationMSDPhysics.h"
#include "ScrollSnap.h"
#include "UnitTransforms.h"
#include "nsPluginFrame.h"
#include "nsSliderFrame.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include <mozilla/layers/AxisPhysicsModel.h>
#include <mozilla/layers/AxisPhysicsMSDModel.h>
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/ScrollLinkedEffectDetector.h"
#include "mozilla/Unused.h"
#include "LayersLogging.h"  // for Stringify
#include <algorithm>
#include <cstdlib> // for std::abs(int/long)
#include <cmath> // for std::abs(float/double)

#define PAINT_SKIP_LOG(...)
// #define PAINT_SKIP_LOG(...) printf_stderr("PSKIP: " __VA_ARGS__)

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::layout;

static uint32_t
GetOverflowChange(const nsRect& aCurScrolledRect, const nsRect& aPrevScrolledRect)
{
  uint32_t result = 0;
  if (aPrevScrolledRect.x != aCurScrolledRect.x ||
      aPrevScrolledRect.width != aCurScrolledRect.width) {
    result |= nsIScrollableFrame::HORIZONTAL;
  }
  if (aPrevScrolledRect.y != aCurScrolledRect.y ||
      aPrevScrolledRect.height != aCurScrolledRect.height) {
    result |= nsIScrollableFrame::VERTICAL;
  }
  return result;
}

//----------------------------------------------------------------------

//----------nsHTMLScrollFrame-------------------------------------------

nsHTMLScrollFrame*
NS_NewHTMLScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, bool aIsRoot)
{
  return new (aPresShell) nsHTMLScrollFrame(aContext, aIsRoot);
}

NS_IMPL_FRAMEARENA_HELPERS(nsHTMLScrollFrame)

nsHTMLScrollFrame::nsHTMLScrollFrame(nsStyleContext* aContext,
                                     nsIFrame::ClassID aID,
                                     bool aIsRoot)
  : nsContainerFrame(aContext, aID)
  , mHelper(ALLOW_THIS_IN_INITIALIZER_LIST(this), aIsRoot)
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
nsHTMLScrollFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  DestroyAbsoluteFrames(aDestructRoot, aPostDestroyData);
  mHelper.Destroy(aPostDestroyData);
  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
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

/**
 HTML scrolling implementation

 All other things being equal, we prefer layouts with fewer scrollbars showing.
*/

namespace mozilla {

struct MOZ_STACK_CLASS ScrollReflowInput {
  const ReflowInput& mReflowInput;
  nsBoxLayoutState mBoxState;
  ScrollbarStyles mStyles;
  nsMargin mComputedBorder;

  // === Filled in by ReflowScrolledFrame ===
  nsOverflowAreas mContentsOverflowAreas;
  MOZ_INIT_OUTSIDE_CTOR
  bool mReflowedContentsWithHScrollbar;
  MOZ_INIT_OUTSIDE_CTOR
  bool mReflowedContentsWithVScrollbar;

  // === Filled in when TryLayout succeeds ===
  // The size of the inside-border area
  nsSize mInsideBorderSize;
  // Whether we decided to show the horizontal scrollbar
  MOZ_INIT_OUTSIDE_CTOR
  bool mShowHScrollbar;
  // Whether we decided to show the vertical scrollbar
  MOZ_INIT_OUTSIDE_CTOR
  bool mShowVScrollbar;

  ScrollReflowInput(nsIScrollableFrame* aFrame,
                    const ReflowInput& aReflowInput) :
    mReflowInput(aReflowInput),
    // mBoxState is just used for scrollbars so we don't need to
    // worry about the reflow depth here
    mBoxState(aReflowInput.mFrame->PresContext(), aReflowInput.mRenderingContext, 0),
    mStyles(aFrame->GetScrollbarStyles()) {
  }
};

} // namespace mozilla

// XXXldb Can this go away?
static nsSize ComputeInsideBorderSize(ScrollReflowInput* aState,
                                      const nsSize& aDesiredInsideBorderSize)
{
  // aDesiredInsideBorderSize is the frame size; i.e., it includes
  // borders and padding (but the scrolled child doesn't have
  // borders). The scrolled child has the same padding as us.
  nscoord contentWidth = aState->mReflowInput.ComputedWidth();
  if (contentWidth == NS_UNCONSTRAINEDSIZE) {
    contentWidth = aDesiredInsideBorderSize.width -
      aState->mReflowInput.ComputedPhysicalPadding().LeftRight();
  }
  nscoord contentHeight = aState->mReflowInput.ComputedHeight();
  if (contentHeight == NS_UNCONSTRAINEDSIZE) {
    contentHeight = aDesiredInsideBorderSize.height -
      aState->mReflowInput.ComputedPhysicalPadding().TopBottom();
  }

  contentWidth  = aState->mReflowInput.ApplyMinMaxWidth(contentWidth);
  contentHeight = aState->mReflowInput.ApplyMinMaxHeight(contentHeight);
  return nsSize(contentWidth + aState->mReflowInput.ComputedPhysicalPadding().LeftRight(),
                contentHeight + aState->mReflowInput.ComputedPhysicalPadding().TopBottom());
}

static void
GetScrollbarMetrics(nsBoxLayoutState& aState, nsIFrame* aBox, nsSize* aMin,
                    nsSize* aPref, bool aVertical)
{
  NS_ASSERTION(aState.GetRenderingContext(),
               "Must have rendering context in layout state for size "
               "computations");

  if (aMin) {
    *aMin = aBox->GetXULMinSize(aState);
    nsBox::AddMargin(aBox, *aMin);
    if (aMin->width < 0) {
      aMin->width = 0;
    }
    if (aMin->height < 0) {
      aMin->height = 0;
    }
  }

  if (aPref) {
    *aPref = aBox->GetXULPrefSize(aState);
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
 * computed fields of the ScrollReflowInput.
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
nsHTMLScrollFrame::TryLayout(ScrollReflowInput* aState,
                             ReflowOutput* aKidMetrics,
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
    if (aAssumeHScroll != aState->mReflowedContentsWithHScrollbar) {
      nsLayoutUtils::MarkIntrinsicISizesDirtyIfDependentOnBSize(
          mHelper.mScrolledFrame);
    }
    aKidMetrics->mOverflowAreas.Clear();
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
  nsIPresShell* presShell = PresShell();
  if (mHelper.mIsRoot && presShell->IsScrollPositionClampingScrollPortSizeSet()) {
    nsSize compositionSize = nsLayoutUtils::CalculateCompositionSizeForFrame(this, false);
    float resolution = presShell->GetResolution();
    compositionSize.width /= resolution;
    compositionSize.height /= resolution;
    visualScrollPortSize = nsSize(std::max(0, compositionSize.width - vScrollbarDesiredWidth),
                                  std::max(0, compositionSize.height - hScrollbarDesiredHeight));
  }

  if (!aForce) {
    nsRect scrolledRect =
      mHelper.GetUnsnappedScrolledRectInternal(aState->mContentsOverflowAreas.ScrollableOverflow(),
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
  if (!IsScrollbarOnRight()) {
    scrollPortOrigin.x += vScrollbarActualWidth;
  }
  mHelper.mScrollPort = nsRect(scrollPortOrigin, scrollPortSize);
  return true;
}

// XXX Height/BSize mismatch needs to be addressed here; check the caller!
// Currently this will only behave as expected for horizontal writing modes.
// (See bug 1175509.)
bool
nsHTMLScrollFrame::ScrolledContentDependsOnHeight(ScrollReflowInput* aState)
{
  // Return true if ReflowScrolledFrame is going to do something different
  // based on the presence of a horizontal scrollbar.
  return mHelper.mScrolledFrame->HasAnyStateBits(
      NS_FRAME_CONTAINS_RELATIVE_BSIZE | NS_FRAME_DESCENDANT_INTRINSIC_ISIZE_DEPENDS_ON_BSIZE) ||
    aState->mReflowInput.ComputedBSize() != NS_UNCONSTRAINEDSIZE ||
    aState->mReflowInput.ComputedMinBSize() > 0 ||
    aState->mReflowInput.ComputedMaxBSize() != NS_UNCONSTRAINEDSIZE;
}

void
nsHTMLScrollFrame::ReflowScrolledFrame(ScrollReflowInput* aState,
                                       bool aAssumeHScroll,
                                       bool aAssumeVScroll,
                                       ReflowOutput* aMetrics,
                                       bool aFirstPass)
{
  WritingMode wm = mHelper.mScrolledFrame->GetWritingMode();

  // these could be NS_UNCONSTRAINEDSIZE ... std::min arithmetic should
  // be OK
  LogicalMargin padding = aState->mReflowInput.ComputedLogicalPadding();
  nscoord availISize =
    aState->mReflowInput.ComputedISize() + padding.IStartEnd(wm);

  nscoord computedBSize = aState->mReflowInput.ComputedBSize();
  nscoord computedMinBSize = aState->mReflowInput.ComputedMinBSize();
  nscoord computedMaxBSize = aState->mReflowInput.ComputedMaxBSize();
  if (!ShouldPropagateComputedBSizeToScrolledContent()) {
    computedBSize = NS_UNCONSTRAINEDSIZE;
    computedMinBSize = 0;
    computedMaxBSize = NS_UNCONSTRAINEDSIZE;
  }

  if (wm.IsVertical()) {
    if (aAssumeVScroll) {
      nsSize vScrollbarPrefSize;
      GetScrollbarMetrics(aState->mBoxState, mHelper.mVScrollbarBox,
                          nullptr, &vScrollbarPrefSize, false);
      if (computedBSize != NS_UNCONSTRAINEDSIZE) {
        computedBSize = std::max(0, computedBSize - vScrollbarPrefSize.width);
      }
      computedMinBSize = std::max(0, computedMinBSize - vScrollbarPrefSize.width);
      if (computedMaxBSize != NS_UNCONSTRAINEDSIZE) {
        computedMaxBSize = std::max(0, computedMaxBSize - vScrollbarPrefSize.width);
      }
    }

    if (aAssumeHScroll) {
      nsSize hScrollbarPrefSize;
      GetScrollbarMetrics(aState->mBoxState, mHelper.mHScrollbarBox,
                          nullptr, &hScrollbarPrefSize, true);
      availISize = std::max(0, availISize - hScrollbarPrefSize.height);
    }
  } else {
    if (aAssumeHScroll) {
      nsSize hScrollbarPrefSize;
      GetScrollbarMetrics(aState->mBoxState, mHelper.mHScrollbarBox,
                          nullptr, &hScrollbarPrefSize, false);
      if (computedBSize != NS_UNCONSTRAINEDSIZE) {
        computedBSize = std::max(0, computedBSize - hScrollbarPrefSize.height);
      }
      computedMinBSize = std::max(0, computedMinBSize - hScrollbarPrefSize.height);
      if (computedMaxBSize != NS_UNCONSTRAINEDSIZE) {
        computedMaxBSize = std::max(0, computedMaxBSize - hScrollbarPrefSize.height);
      }
    }

    if (aAssumeVScroll) {
      nsSize vScrollbarPrefSize;
      GetScrollbarMetrics(aState->mBoxState, mHelper.mVScrollbarBox,
                          nullptr, &vScrollbarPrefSize, true);
      availISize = std::max(0, availISize - vScrollbarPrefSize.width);
    }
  }

  nsPresContext* presContext = PresContext();

  // Pass false for aInit so we can pass in the correct padding.
  ReflowInput
    kidReflowInput(presContext, aState->mReflowInput,
                   mHelper.mScrolledFrame,
                   LogicalSize(wm, availISize, NS_UNCONSTRAINEDSIZE),
                   nullptr, ReflowInput::CALLER_WILL_INIT);
  const nsMargin physicalPadding = padding.GetPhysicalMargin(wm);
  kidReflowInput.Init(presContext, nullptr, nullptr,
                      &physicalPadding);
  kidReflowInput.mFlags.mAssumingHScrollbar = aAssumeHScroll;
  kidReflowInput.mFlags.mAssumingVScrollbar = aAssumeVScroll;
  kidReflowInput.SetComputedBSize(computedBSize);
  kidReflowInput.ComputedMinBSize() = computedMinBSize;
  kidReflowInput.ComputedMaxBSize() = computedMaxBSize;
  if (aState->mReflowInput.IsBResizeForWM(kidReflowInput.GetWritingMode())) {
    kidReflowInput.SetBResize(true);
  }

  // Temporarily set mHasHorizontalScrollbar/mHasVerticalScrollbar to
  // reflect our assumptions while we reflow the child.
  bool didHaveHorizontalScrollbar = mHelper.mHasHorizontalScrollbar;
  bool didHaveVerticalScrollbar = mHelper.mHasVerticalScrollbar;
  mHelper.mHasHorizontalScrollbar = aAssumeHScroll;
  mHelper.mHasVerticalScrollbar = aAssumeVScroll;

  nsReflowStatus status;
  // No need to pass a true container-size to ReflowChild or
  // FinishReflowChild, because it's only used there when positioning
  // the frame (i.e. if NS_FRAME_NO_MOVE_FRAME isn't set)
  const nsSize dummyContainerSize;
  ReflowChild(mHelper.mScrolledFrame, presContext, *aMetrics,
              kidReflowInput, wm, LogicalPoint(wm), dummyContainerSize,
              NS_FRAME_NO_MOVE_FRAME, status);

  mHelper.mHasHorizontalScrollbar = didHaveHorizontalScrollbar;
  mHelper.mHasVerticalScrollbar = didHaveVerticalScrollbar;

  // Don't resize or position the view (if any) because we're going to resize
  // it to the correct size anyway in PlaceScrollArea. Allowing it to
  // resize here would size it to the natural height of the frame,
  // which will usually be different from the scrollport height;
  // invalidating the difference will cause unnecessary repainting.
  FinishReflowChild(mHelper.mScrolledFrame, presContext,
                    *aMetrics, &kidReflowInput, wm, LogicalPoint(wm),
                    dummyContainerSize,
                    NS_FRAME_NO_MOVE_FRAME | NS_FRAME_NO_SIZE_VIEW);

  // XXX Some frames (e.g., nsPluginFrame, nsFrameFrame, nsTextFrame) don't bother
  // setting their mOverflowArea. This is wrong because every frame should
  // always set mOverflowArea. In fact nsPluginFrame and nsFrameFrame don't
  // support the 'outline' property because of this. Rather than fix the world
  // right now, just fix up the overflow area if necessary. Note that we don't
  // check HasOverflowRect() because it could be set even though the
  // overflow area doesn't include the frame bounds.
  aMetrics->UnionOverflowAreasWithDesiredBounds();

  auto* disp = StyleDisplay();
  if (MOZ_UNLIKELY(disp->mOverflowClipBoxBlock ==
                     NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX ||
                   disp->mOverflowClipBoxInline ==
                     NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX)) {
    nsOverflowAreas childOverflow;
    nsLayoutUtils::UnionChildOverflow(mHelper.mScrolledFrame, childOverflow);
    nsRect childScrollableOverflow = childOverflow.ScrollableOverflow();
    if (disp->mOverflowClipBoxBlock == NS_STYLE_OVERFLOW_CLIP_BOX_PADDING_BOX) {
      padding.BStart(wm) = nscoord(0);
      padding.BEnd(wm) = nscoord(0);
    }
    if (disp->mOverflowClipBoxInline == NS_STYLE_OVERFLOW_CLIP_BOX_PADDING_BOX) {
      padding.IStart(wm) = nscoord(0);
      padding.IEnd(wm) = nscoord(0);
    }
    childScrollableOverflow.Inflate(padding.GetPhysicalMargin(wm));
    nsRect contentArea =
      wm.IsVertical() ? nsRect(0, 0, computedBSize, availISize)
                      : nsRect(0, 0, availISize, computedBSize);
    if (!contentArea.Contains(childScrollableOverflow)) {
      aMetrics->mOverflowAreas.ScrollableOverflow() = childScrollableOverflow;
    }
  }

  aState->mContentsOverflowAreas = aMetrics->mOverflowAreas;
  aState->mReflowedContentsWithHScrollbar = aAssumeHScroll;
  aState->mReflowedContentsWithVScrollbar = aAssumeVScroll;
}

bool
nsHTMLScrollFrame::GuessHScrollbarNeeded(const ScrollReflowInput& aState)
{
  if (aState.mStyles.mHorizontal != NS_STYLE_OVERFLOW_AUTO)
    // no guessing required
    return aState.mStyles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL;

  return mHelper.mHasHorizontalScrollbar;
}

bool
nsHTMLScrollFrame::GuessVScrollbarNeeded(const ScrollReflowInput& aState)
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
    nsIFrame *f = mHelper.mScrolledFrame->PrincipalChildList().FirstChild();
    if (f && f->IsSVGOuterSVGFrame() &&
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
nsHTMLScrollFrame::ReflowContents(ScrollReflowInput* aState,
                                  const ReflowOutput& aDesiredSize)
{
  ReflowOutput kidDesiredSize(aDesiredSize.GetWritingMode(), aDesiredSize.mFlags);
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
      mHelper.GetUnsnappedScrolledRectInternal(kidDesiredSize.ScrollableOverflow(),
                                               insideBorderSize);
    if (nsRect(nsPoint(0, 0), insideBorderSize).Contains(scrolledRect)) {
      // Let's pretend we had no scrollbars coming in here
      kidDesiredSize.mOverflowAreas.Clear();
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
nsHTMLScrollFrame::PlaceScrollArea(ScrollReflowInput& aState,
                                   const nsPoint& aScrollPosition)
{
  nsIFrame *scrolledFrame = mHelper.mScrolledFrame;
  // Set the x,y of the scrolled frame to the correct value
  scrolledFrame->SetPosition(mHelper.mScrollPort.TopLeft() - aScrollPosition);

  // Recompute our scrollable overflow, taking perspective children into
  // account. Note that this only recomputes the overflow areas stored on the
  // helper (which are used to compute scrollable length and scrollbar thumb
  // sizes) but not the overflow areas stored on the frame. This seems to work
  // for now, but it's possible that we may need to update both in the future.
  AdjustForPerspective(aState.mContentsOverflowAreas.ScrollableOverflow());

  nsRect scrolledArea;
  // Preserve the width or height of empty rects
  nsSize portSize = mHelper.mScrollPort.Size();
  nsRect scrolledRect =
    mHelper.GetUnsnappedScrolledRectInternal(aState.mContentsOverflowAreas.ScrollableOverflow(),
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
nsHTMLScrollFrame::GetIntrinsicVScrollbarWidth(gfxContext *aRenderingContext)
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
nsHTMLScrollFrame::GetMinISize(gfxContext *aRenderingContext)
{
  nscoord result = mHelper.mScrolledFrame->GetMinISize(aRenderingContext);
  DISPLAY_MIN_WIDTH(this, result);
  return result + GetIntrinsicVScrollbarWidth(aRenderingContext);
}

/* virtual */ nscoord
nsHTMLScrollFrame::GetPrefISize(gfxContext *aRenderingContext)
{
  nscoord result = mHelper.mScrolledFrame->GetPrefISize(aRenderingContext);
  DISPLAY_PREF_WIDTH(this, result);
  return NSCoordSaturatingAdd(result, GetIntrinsicVScrollbarWidth(aRenderingContext));
}

nsresult
nsHTMLScrollFrame::GetXULPadding(nsMargin& aMargin)
{
  // Our padding hangs out on the inside of the scrollframe, but XUL doesn't
  // reaize that.  If we're stuck inside a XUL box, we need to claim no
  // padding.
  // @see also nsXULScrollFrame::GetXULPadding.
  aMargin.SizeTo(0,0,0,0);
  return NS_OK;
}

bool
nsHTMLScrollFrame::IsXULCollapsed()
{
  // We're never collapsed in the box sense.
  return false;
}

// Return the <browser> if the scrollframe is for the root frame directly
// inside a <browser>.
static Element*
GetBrowserRoot(nsIContent* aContent)
{
  if (aContent) {
    nsIDocument* doc = aContent->GetUncomposedDoc();
    if (nsPIDOMWindowOuter* win = doc->GetWindow()) {
      Element* frameElement = win->GetFrameElementInternal();
      if (frameElement &&
          frameElement->NodeInfo()->Equals(nsGkAtoms::browser, kNameSpaceID_XUL))
        return frameElement;
    }
  }

  return nullptr;
}

// When we have perspective set on the outer scroll frame, and transformed
// children (possibly with preserve-3d) then the effective transform on the
// child depends on the offset to the scroll frame, which changes as we scroll.
// This perspective transform can cause the element to move relative to the
// scrolled inner frame, which would cause the scrollable length changes during
// scrolling if we didn't account for it. Since we don't want scrollHeight/Width
// and the size of scrollbar thumbs to change during scrolling, we compute the
// scrollable overflow by determining the scroll position at which the child
// becomes completely visible within the scrollport rather than using the union
// of the overflow areas at their current position.
void
GetScrollableOverflowForPerspective(nsIFrame* aScrolledFrame,
                                    nsIFrame* aCurrentFrame,
                                    const nsRect aScrollPort,
                                    nsPoint aOffset,
                                    nsRect& aScrolledFrameOverflowArea)
{
  // Iterate over all children except pop-ups.
  FrameChildListIDs skip = nsIFrame::kSelectPopupList | nsIFrame::kPopupList;
  for (nsIFrame::ChildListIterator childLists(aCurrentFrame);
       !childLists.IsDone(); childLists.Next()) {
    if (skip.Contains(childLists.CurrentID())) {
      continue;
    }

    for (nsIFrame* child : childLists.CurrentList()) {
      nsPoint offset = aOffset;

      // When we reach a direct child of the scroll, then we record the offset
      // to convert from that frame's coordinate into the scroll frame's
      // coordinates. Preserve-3d descendant frames use the same offset as their
      // ancestors, since TransformRect already converts us into the coordinate
      // space of the preserve-3d root.
      if (aScrolledFrame == aCurrentFrame) {
        offset = child->GetPosition();
      }

      if (child->Extend3DContext()) {
        // If we're a preserve-3d frame, then recurse and include our
        // descendants since overflow of preserve-3d frames is only included
        // in the post-transform overflow area of the preserve-3d root frame.
        GetScrollableOverflowForPerspective(aScrolledFrame, child, aScrollPort,
                                            offset, aScrolledFrameOverflowArea);
      }

      // If we're transformed, then we want to consider the possibility that
      // this frame might move relative to the scrolled frame when scrolling.
      // For preserve-3d, leaf frames have correct overflow rects relative to
      // themselves. preserve-3d 'nodes' (intermediate frames and the root) have
      // only their untransformed children included in their overflow relative
      // to self, which is what we want to include here.
      if (child->IsTransformed()) {
        // Compute the overflow rect for this leaf transform frame in the
        // coordinate space of the scrolled frame.
        nsPoint scrollPos = aScrolledFrame->GetPosition();
        nsRect preScroll = nsDisplayTransform::TransformRect(
          child->GetScrollableOverflowRectRelativeToSelf(), child);

        // Temporarily override the scroll position of the scrolled frame by
        // 10 CSS pixels, and then recompute what the overflow rect would be.
        // This scroll position may not be valid, but that shouldn't matter
        // for our calculations.
        aScrolledFrame->SetPosition(scrollPos + nsPoint(600, 600));
        nsRect postScroll = nsDisplayTransform::TransformRect(
          child->GetScrollableOverflowRectRelativeToSelf(), child);
        aScrolledFrame->SetPosition(scrollPos);

        // Compute how many app units the overflow rects moves by when we adjust
        // the scroll position by 1 app unit.
        double rightDelta =
          (postScroll.XMost() - preScroll.XMost() + 600.0) / 600.0;
        double bottomDelta =
          (postScroll.YMost() - preScroll.YMost() + 600.0) / 600.0;

        // We can't ever have negative scrolling.
        NS_ASSERTION(rightDelta > 0.0f && bottomDelta > 0.0f,
                     "Scrolling can't be reversed!");

        // Move preScroll into the coordinate space of the scrollport.
        preScroll += offset + scrollPos;

        // For each of the four edges of preScroll, figure out how far they
        // extend beyond the scrollport. Ignore negative values since that means
        // that side is already scrolled in to view and we don't need to add
        // overflow to account for it.
        nsMargin overhang(std::max(0, aScrollPort.Y() - preScroll.Y()),
                          std::max(0, preScroll.XMost() - aScrollPort.XMost()),
                          std::max(0, preScroll.YMost() - aScrollPort.YMost()),
                          std::max(0, aScrollPort.X() - preScroll.X()));

        // Scale according to rightDelta/bottomDelta to adjust for the different
        // scroll rates.
        overhang.top /= bottomDelta;
        overhang.right /= rightDelta;
        overhang.bottom /= bottomDelta;
        overhang.left /= rightDelta;

        // Take the minimum overflow rect that would allow the current scroll
        // position, using the size of the scroll port and offset by the
        // inverse of the scroll position.
        nsRect overflow = aScrollPort - scrollPos;

        // Expand it by our margins to get an overflow rect that would allow all
        // edges of our transformed content to be scrolled into view.
        overflow.Inflate(overhang);

        // Merge it with the combined overflow
        aScrolledFrameOverflowArea.UnionRect(aScrolledFrameOverflowArea,
                                             overflow);
      } else if (aCurrentFrame == aScrolledFrame) {
        aScrolledFrameOverflowArea.UnionRect(
          aScrolledFrameOverflowArea,
          child->GetScrollableOverflowRectRelativeToParent());
      }
    }
  }
}

void
nsHTMLScrollFrame::AdjustForPerspective(nsRect& aScrollableOverflow)
{
  // If we have perspective that is being applied to our children, then
  // the effective transform on the child depends on the relative position
  // of the child to us and changes during scrolling.
  if (!ChildrenHavePerspective()) {
    return;
  }
  aScrollableOverflow.SetEmpty();
  GetScrollableOverflowForPerspective(mHelper.mScrolledFrame,
                                      mHelper.mScrolledFrame,
                                      mHelper.mScrollPort,
                                      nsPoint(), aScrollableOverflow);
}

void
nsHTMLScrollFrame::Reflow(nsPresContext*           aPresContext,
                          ReflowOutput&     aDesiredSize,
                          const ReflowInput& aReflowInput,
                          nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsHTMLScrollFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  mHelper.HandleScrollbarStyleSwitching();

  ScrollReflowInput state(this, aReflowInput);
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
  if (!aReflowInput.ShouldReflowAllKids()) {
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

    Element* browserRoot = GetBrowserRoot(mContent);
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

  state.mComputedBorder = aReflowInput.ComputedPhysicalBorderPadding() -
    aReflowInput.ComputedPhysicalPadding();

  ReflowContents(&state, aDesiredSize);

  aDesiredSize.Width() = state.mInsideBorderSize.width +
    state.mComputedBorder.LeftRight();
  aDesiredSize.Height() = state.mInsideBorderSize.height +
    state.mComputedBorder.TopBottom();

  // Set the size of the frame now since computing the perspective-correct
  // overflow (within PlaceScrollArea) can rely on it.
  SetSize(aDesiredSize.GetWritingMode(),
          aDesiredSize.Size(aDesiredSize.GetWritingMode()));

  // Restore the old scroll position, for now, even if that's not valid anymore
  // because we changed size. We'll fix it up in a post-reflow callback, because
  // our current size may only be temporary (e.g. we're compute XUL desired sizes).
  PlaceScrollArea(state, oldScrollPosition);
  if (!mHelper.mPostedReflowCallback) {
    // Make sure we'll try scrolling to restored position
    PresShell()->PostReflowCallback(&mHelper);
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
    if (!mHelper.mSuppressScrollbarUpdate) {
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

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  if (mHelper.IsIgnoringViewportClipping()) {
    aDesiredSize.mOverflowAreas.UnionWith(
      state.mContentsOverflowAreas + mHelper.mScrolledFrame->GetPosition());
  }

  mHelper.UpdateSticky();
  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowInput, aStatus);

  if (!InInitialReflow() && !mHelper.mHadNonInitialReflow) {
    mHelper.mHadNonInitialReflow = true;
  }

  if (mHelper.mIsRoot && !oldScrolledAreaBounds.IsEqualEdges(newScrolledAreaBounds)) {
    mHelper.PostScrolledAreaEvent();
  }

  mHelper.UpdatePrevScrolledRect();

  aStatus.Reset(); // This type of frame can't be split.
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
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
  if (IsTableCaption()) {
    return GetRect().IsEmpty() ? a11y::eNoType : a11y::eHTMLCaptionType;
  }

  // Create an accessible regardless of focusable state because the state can be
  // changed during frame life cycle without any notifications to accessibility.
  if (mContent->IsRootOfNativeAnonymousSubtree() ||
      GetScrollbarStyles().IsHiddenInBothDirections()) {
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
  return new (aPresShell) nsXULScrollFrame(aContext, aIsRoot,
                                           aClipAllDescendants);
}

NS_IMPL_FRAMEARENA_HELPERS(nsXULScrollFrame)

nsXULScrollFrame::nsXULScrollFrame(nsStyleContext* aContext,
                                   bool aIsRoot,
                                   bool aClipAllDescendants)
  : nsBoxFrame(aContext, kClassID, aIsRoot)
  , mHelper(ALLOW_THIS_IN_INITIALIZER_LIST(this), aIsRoot)
{
  SetXULLayoutManager(nullptr);
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
    nsSize size = mVScrollbarBox->GetXULPrefSize(*aState);
    nsBox::AddMargin(mVScrollbarBox, size);
    if (IsScrollbarOnRight())
      result.left = size.width;
    else
      result.right = size.width;
  }

  if (mHScrollbarBox) {
    nsSize size = mHScrollbarBox->GetXULPrefSize(*aState);
    nsBox::AddMargin(mHScrollbarBox, size);
    // We don't currently support any scripts that would require a scrollbar
    // at the top. (Are there any?)
    result.bottom = size.height;
  }

  return result;
}

nscoord
ScrollFrameHelper::GetNondisappearingScrollbarWidth(nsBoxLayoutState* aState,
                                                    WritingMode aWM)
{
  NS_ASSERTION(aState && aState->GetRenderingContext(),
               "Must have rendering context in layout state for size "
               "computations");

  bool verticalWM = aWM.IsVertical();
  if (LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
    // We're using overlay scrollbars, so we need to get the width that
    // non-disappearing scrollbars would have.
    nsITheme* theme = aState->PresContext()->GetTheme();
    if (theme &&
        theme->ThemeSupportsWidget(aState->PresContext(),
                                   verticalWM ? mHScrollbarBox
                                              : mVScrollbarBox,
                                   NS_THEME_SCROLLBAR_NON_DISAPPEARING)) {
      LayoutDeviceIntSize size;
      bool canOverride = true;
      theme->GetMinimumWidgetSize(aState->PresContext(),
                                  verticalWM ? mHScrollbarBox
                                             : mVScrollbarBox,
                                  NS_THEME_SCROLLBAR_NON_DISAPPEARING,
                                  &size,
                                  &canOverride);
      return aState->PresContext()->
             DevPixelsToAppUnits(verticalWM ? size.height : size.width);
    }
  }

  nsMargin sizes(GetDesiredScrollbarSizes(aState));
  return verticalWM ? sizes.TopBottom() : sizes.LeftRight();
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

#if defined(MOZ_WIDGET_ANDROID)
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
#endif

void
ScrollFrameHelper::SetScrollableByAPZ(bool aScrollable)
{
  mScrollableByAPZ = aScrollable;
}

void
ScrollFrameHelper::SetZoomableByAPZ(bool aZoomable)
{
  if (mZoomableByAPZ != aZoomable) {
    // We might be changing the result of WantAsyncScroll() so schedule a
    // paint to make sure we pick up the result of that change.
    mZoomableByAPZ = aZoomable;
    mOuter->SchedulePaint();
  }
}

void
ScrollFrameHelper::SetHasOutOfFlowContentInsideFilter()
{
  mHasOutOfFlowContentInsideFilter = true;
}

bool
ScrollFrameHelper::WantAsyncScroll() const
{
  // If zooming is allowed, and this is a frame that's allowed to zoom, then
  // we want it to be async-scrollable or zooming will not be permitted.
  if (mZoomableByAPZ) {
    return true;
  }

  ScrollbarStyles styles = GetScrollbarStylesFromFrame();
  nscoord oneDevPixel = GetScrolledFrame()->PresContext()->AppUnitsPerDevPixel();
  nsRect scrollRange = GetScrollRange();
  bool isVScrollable = (scrollRange.height >= oneDevPixel) &&
                       (styles.mVertical != NS_STYLE_OVERFLOW_HIDDEN);
  bool isHScrollable = (scrollRange.width >= oneDevPixel) &&
                       (styles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN);

#if defined(MOZ_WIDGET_ANDROID)
  // Mobile platforms need focus to scroll.
  bool canScrollWithoutScrollbars = IsFocused(mOuter->GetContent());
#else
  bool canScrollWithoutScrollbars = true;
#endif

  // The check for scroll bars was added in bug 825692 to prevent layerization
  // of text inputs for performance reasons.
  bool isVAsyncScrollable = isVScrollable && (mVScrollbarBox || canScrollWithoutScrollbars);
  bool isHAsyncScrollable = isHScrollable && (mHScrollbarBox || canScrollWithoutScrollbars);
  return isVAsyncScrollable || isHAsyncScrollable;
}

static nsRect
GetOnePixelRangeAroundPoint(const nsPoint& aPoint, bool aIsHorizontal)
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
ScrollFrameHelper::ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                                nsIScrollbarMediator::ScrollSnapMode aSnap)
{
  ScrollByUnit(aScrollbar, nsIScrollableFrame::SMOOTH, aDirection,
               nsIScrollableFrame::PAGES, aSnap);
}

void
ScrollFrameHelper::ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                                 nsIScrollbarMediator::ScrollSnapMode aSnap)
{
  ScrollByUnit(aScrollbar, nsIScrollableFrame::INSTANT, aDirection,
               nsIScrollableFrame::WHOLE, aSnap);
}

void
ScrollFrameHelper::ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                                nsIScrollbarMediator::ScrollSnapMode aSnap)
{
  bool isHorizontal = aScrollbar->IsXULHorizontal();
  nsIntPoint delta;
  if (isHorizontal) {
    const double kScrollMultiplier =
      Preferences::GetInt("toolkit.scrollbox.horizontalScrollDistance",
                          NS_DEFAULT_HORIZONTAL_SCROLL_DISTANCE);
    delta.x = aDirection * kScrollMultiplier;
    if (GetLineScrollAmount().width * delta.x > GetPageScrollAmount().width) {
      // The scroll frame is so small that the delta would be more
      // than an entire page.  Scroll by one page instead to maintain
      // context.
      ScrollByPage(aScrollbar, aDirection);
      return;
    }
  } else {
    const double kScrollMultiplier =
      Preferences::GetInt("toolkit.scrollbox.verticalScrollDistance",
                          NS_DEFAULT_VERTICAL_SCROLL_DISTANCE);
    delta.y = aDirection * kScrollMultiplier;
    if (GetLineScrollAmount().height * delta.y > GetPageScrollAmount().height) {
      // The scroll frame is so small that the delta would be more
      // than an entire page.  Scroll by one page instead to maintain
      // context.
      ScrollByPage(aScrollbar, aDirection);
      return;
    }
  }

  nsIntPoint overflow;
  ScrollBy(delta, nsIScrollableFrame::LINES, nsIScrollableFrame::SMOOTH,
           &overflow, nsGkAtoms::other, nsIScrollableFrame::NOT_MOMENTUM,
           aSnap);
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
  bool isHorizontal = aScrollbar->IsXULHorizontal();
  nsPoint current = GetScrollPosition();
  nsPoint dest = current;
  if (isHorizontal) {
    dest.x = IsPhysicalLTR() ? aNewPos : aNewPos - GetScrollRange().width;
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
ScrollFrameHelper::ScrollbarReleased(nsScrollbarFrame* aScrollbar)
{
  // Scrollbar scrolling does not result in fling gestures, clear any
  // accumulated velocity
  mVelocityQueue.Reset();

  // Perform scroll snapping, if needed.  Scrollbar movement uses the same
  // smooth scrolling animation as keyboard scrolling.
  ScrollSnap(mDestination, nsIScrollableFrame::SMOOTH);
}

void
ScrollFrameHelper::ScrollByUnit(nsScrollbarFrame* aScrollbar,
                                nsIScrollableFrame::ScrollMode aMode,
                                int32_t aDirection,
                                nsIScrollableFrame::ScrollUnit aUnit,
                                nsIScrollbarMediator::ScrollSnapMode aSnap)
{
  MOZ_ASSERT(aScrollbar != nullptr);
  bool isHorizontal = aScrollbar->IsXULHorizontal();
  nsIntPoint delta;
  if (isHorizontal) {
    delta.x = aDirection;
  } else {
    delta.y = aDirection;
  }
  nsIntPoint overflow;
  ScrollBy(delta, aUnit, aMode, &overflow, nsGkAtoms::other,
           nsIScrollableFrame::NOT_MOMENTUM, aSnap);
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
nsXULScrollFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  mHelper.Destroy(aPostDestroyData);
  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void
nsXULScrollFrame::SetInitialChildList(ChildListID     aListID,
                                      nsFrameList&    aChildList)
{
  nsBoxFrame::SetInitialChildList(aListID, aChildList);
  if (aListID == kPrincipalList) {
    mHelper.ReloadChildFrames();
  }
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
nsXULScrollFrame::GetXULPadding(nsMargin& aMargin)
{
  aMargin.SizeTo(0,0,0,0);
  return NS_OK;
}

nscoord
nsXULScrollFrame::GetXULBoxAscent(nsBoxLayoutState& aState)
{
  if (!mHelper.mScrolledFrame)
    return 0;

  nscoord ascent = mHelper.mScrolledFrame->GetXULBoxAscent(aState);
  nsMargin m(0,0,0,0);
  GetXULBorderAndPadding(m);
  ascent += m.top;
  GetXULMargin(m);
  ascent += m.top;

  return ascent;
}

nsSize
nsXULScrollFrame::GetXULPrefSize(nsBoxLayoutState& aState)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsSize pref = mHelper.mScrolledFrame->GetXULPrefSize(aState);

  ScrollbarStyles styles = GetScrollbarStyles();

  // scrolled frames don't have their own margins

  if (mHelper.mVScrollbarBox &&
      styles.mVertical == NS_STYLE_OVERFLOW_SCROLL) {
    nsSize vSize = mHelper.mVScrollbarBox->GetXULPrefSize(aState);
    nsBox::AddMargin(mHelper.mVScrollbarBox, vSize);
    pref.width += vSize.width;
  }

  if (mHelper.mHScrollbarBox &&
      styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
    nsSize hSize = mHelper.mHScrollbarBox->GetXULPrefSize(aState);
    nsBox::AddMargin(mHelper.mHScrollbarBox, hSize);
    pref.height += hSize.height;
  }

  AddBorderAndPadding(pref);
  bool widthSet, heightSet;
  nsIFrame::AddXULPrefSize(this, pref, widthSet, heightSet);
  return pref;
}

nsSize
nsXULScrollFrame::GetXULMinSize(nsBoxLayoutState& aState)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsSize min = mHelper.mScrolledFrame->GetXULMinSizeForScrollArea(aState);

  ScrollbarStyles styles = GetScrollbarStyles();

  if (mHelper.mVScrollbarBox &&
      styles.mVertical == NS_STYLE_OVERFLOW_SCROLL) {
     nsSize vSize = mHelper.mVScrollbarBox->GetXULMinSize(aState);
     AddMargin(mHelper.mVScrollbarBox, vSize);
     min.width += vSize.width;
     if (min.height < vSize.height)
        min.height = vSize.height;
  }

  if (mHelper.mHScrollbarBox &&
      styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
     nsSize hSize = mHelper.mHScrollbarBox->GetXULMinSize(aState);
     AddMargin(mHelper.mHScrollbarBox, hSize);
     min.height += hSize.height;
     if (min.width < hSize.width)
        min.width = hSize.width;
  }

  AddBorderAndPadding(min);
  bool widthSet, heightSet;
  nsIFrame::AddXULMinSize(aState, this, min, widthSet, heightSet);
  return min;
}

nsSize
nsXULScrollFrame::GetXULMaxSize(nsBoxLayoutState& aState)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsSize maxSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE);

  AddBorderAndPadding(maxSize);
  bool widthSet, heightSet;
  nsIFrame::AddXULMaxSize(this, maxSize, widthSet, heightSet);
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
nsXULScrollFrame::DoXULLayout(nsBoxLayoutState& aState)
{
  uint32_t flags = aState.LayoutFlags();
  nsresult rv = XULLayout(aState);
  aState.SetLayoutFlags(flags);

  nsBox::DoXULLayout(aState);
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

// AsyncSmoothMSDScroll has ref counting.
class ScrollFrameHelper::AsyncSmoothMSDScroll final : public nsARefreshObserver {
public:
  AsyncSmoothMSDScroll(const nsPoint &aInitialPosition,
                       const nsPoint &aInitialDestination,
                       const nsSize &aInitialVelocity,
                       const nsRect &aRange,
                       const mozilla::TimeStamp &aStartTime,
                       nsPresContext* aPresContext)
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
    , mOneDevicePixelInAppUnits(aPresContext->DevPixelsToAppUnits(1))
  {
    Telemetry::SetHistogramRecordingEnabled(
      Telemetry::FX_REFRESH_DRIVER_SYNC_SCROLL_FRAME_DELAY_MS, true);
  }

  NS_INLINE_DECL_REFCOUNTING(AsyncSmoothMSDScroll, override)

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
    return mXAxisModel.IsFinished(mOneDevicePixelInAppUnits) &&
           mYAxisModel.IsFinished(mOneDevicePixelInAppUnits);
  }

  virtual void WillRefresh(mozilla::TimeStamp aTime) override {
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

    if (!RefreshDriver(aCallee)->AddRefreshObserver(this, FlushType::Style)) {
      return false;
    }

    mCallee = aCallee;
    return true;
  }

private:
  // Private destructor, to discourage deletion outside of Release():
  ~AsyncSmoothMSDScroll() {
    RemoveObserver();
    Telemetry::SetHistogramRecordingEnabled(
      Telemetry::FX_REFRESH_DRIVER_SYNC_SCROLL_FRAME_DELAY_MS, false);
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
      RefreshDriver(mCallee)->RemoveRefreshObserver(this, FlushType::Style);
    }
  }

  mozilla::layers::AxisPhysicsMSDModel mXAxisModel, mYAxisModel;
  nsRect mRange;
  mozilla::TimeStamp mLastRefreshTime;
  ScrollFrameHelper *mCallee;
  nscoord mOneDevicePixelInAppUnits;
};

// AsyncScroll has ref counting.
class ScrollFrameHelper::AsyncScroll final
  : public nsARefreshObserver
{
public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  explicit AsyncScroll()
    : mCallee(nullptr)
  {
    Telemetry::SetHistogramRecordingEnabled(
      Telemetry::FX_REFRESH_DRIVER_SYNC_SCROLL_FRAME_DELAY_MS, true);
  }

private:
  // Private destructor, to discourage deletion outside of Release():
  ~AsyncScroll() {
    RemoveObserver();
    Telemetry::SetHistogramRecordingEnabled(
      Telemetry::FX_REFRESH_DRIVER_SYNC_SCROLL_FRAME_DELAY_MS, false);
  }

public:
  void InitSmoothScroll(TimeStamp aTime,
                        nsPoint aInitialPosition, nsPoint aDestination,
                        nsAtom *aOrigin, const nsRect& aRange,
                        const nsSize& aCurrentVelocity);
  void Init(const nsRect& aRange) {
    mAnimationPhysics = nullptr;
    mRange = aRange;
  }

  bool IsSmoothScroll() { return mAnimationPhysics != nullptr; }

  bool IsFinished(const TimeStamp& aTime) const {
    MOZ_RELEASE_ASSERT(mAnimationPhysics);
    return mAnimationPhysics->IsFinished(aTime);
  }

  nsPoint PositionAt(const TimeStamp& aTime) const {
    MOZ_RELEASE_ASSERT(mAnimationPhysics);
    return mAnimationPhysics->PositionAt(aTime);
  }

  nsSize VelocityAt(const TimeStamp& aTime) const {
    MOZ_RELEASE_ASSERT(mAnimationPhysics);
    return mAnimationPhysics->VelocityAt(aTime);
  }

  // Most recent scroll origin.
  RefPtr<nsAtom> mOrigin;

  // Allowed destination positions around mDestination
  nsRect mRange;

private:
  void InitPreferences(TimeStamp aTime, nsAtom *aOrigin);

  UniquePtr<ScrollAnimationPhysics> mAnimationPhysics;

// The next section is observer/callback management
// Bodies of WillRefresh and RefreshDriver contain ScrollFrameHelper specific code.
public:
  NS_INLINE_DECL_REFCOUNTING(AsyncScroll, override)

  /*
   * Set a refresh observer for smooth scroll iterations (and start observing).
   * Should be used at most once during the lifetime of this object.
   * Return value: true on success, false otherwise.
   */
  bool SetRefreshObserver(ScrollFrameHelper *aCallee) {
    NS_ASSERTION(aCallee && !mCallee, "AsyncScroll::SetRefreshObserver - Invalid usage.");

    if (!RefreshDriver(aCallee)->AddRefreshObserver(this, FlushType::Style)) {
      return false;
    }

    mCallee = aCallee;
    APZCCallbackHelper::SuppressDisplayport(true, mCallee->mOuter->PresShell());
    return true;
  }

  virtual void WillRefresh(mozilla::TimeStamp aTime) override {
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
      RefreshDriver(mCallee)->RemoveRefreshObserver(this, FlushType::Style);
      APZCCallbackHelper::SuppressDisplayport(false, mCallee->mOuter->PresShell());
    }
  }
};

/*
 * Calculate duration, possibly dynamically according to events rate and event origin.
 * (also maintain previous timestamps - which are only used here).
 */
static ScrollAnimationBezierPhysicsSettings
ComputeBezierAnimationSettingsForOrigin(nsAtom *aOrigin)
{
  int32_t minMS = 0;
  int32_t maxMS = 0;
  bool isOriginSmoothnessEnabled = false;
  double intervalRatio = 1;

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
    minMS = Preferences::GetInt(prefMin.get(), kDefaultMinMS);
    maxMS = Preferences::GetInt(prefMax.get(), kDefaultMaxMS);

    static const int32_t kSmoothScrollMaxAllowedAnimationDurationMS = 10000;
    maxMS = clamped(maxMS, 0, kSmoothScrollMaxAllowedAnimationDurationMS);
    minMS = clamped(minMS, 0, maxMS);
  }

  // Keep the animation duration longer than the average event intervals
  //   (to "connect" consecutive scroll animations before the scroll comes to a stop).
  static const double kDefaultDurationToIntervalRatio = 2; // Duplicated at all.js
  intervalRatio = Preferences::GetInt("general.smoothScroll.durationToIntervalRatio",
                                                      kDefaultDurationToIntervalRatio * 100) / 100.0;

  // Duration should be at least as long as the intervals -> ratio is at least 1
  intervalRatio = std::max(1.0, intervalRatio);

  return ScrollAnimationBezierPhysicsSettings { minMS, maxMS, intervalRatio };
}

void
ScrollFrameHelper::AsyncScroll::InitSmoothScroll(TimeStamp aTime,
                                                 nsPoint aInitialPosition,
                                                 nsPoint aDestination,
                                                 nsAtom *aOrigin,
                                                 const nsRect& aRange,
                                                 const nsSize& aCurrentVelocity)
{
  if (!aOrigin || aOrigin == nsGkAtoms::restore) {
    // We don't have special prefs for "restore", just treat it as "other".
    // "restore" scrolls are (for now) always instant anyway so unless something
    // changes we should never have aOrigin == nsGkAtoms::restore here.
    aOrigin = nsGkAtoms::other;
  }
  // Likewise we should never get APZ-triggered scrolls here, and if that changes
  // something is likely broken somewhere.
  MOZ_ASSERT(aOrigin != nsGkAtoms::apz);

  // Read preferences only on first iteration or for a different event origin.
  if (!mAnimationPhysics || aOrigin != mOrigin) {
    mOrigin = aOrigin;
    if (gfxPrefs::SmoothScrollMSDPhysicsEnabled()) {
      mAnimationPhysics = MakeUnique<ScrollAnimationMSDPhysics>(aInitialPosition);
    } else {
      ScrollAnimationBezierPhysicsSettings settings =
        ComputeBezierAnimationSettingsForOrigin(mOrigin);
      mAnimationPhysics =
        MakeUnique<ScrollAnimationBezierPhysics>(aInitialPosition, settings);
    }
  }

  mRange = aRange;

  mAnimationPhysics->Update(aTime, aDestination, aCurrentVelocity);
}

bool
ScrollFrameHelper::IsSmoothScrollingEnabled()
{
  return Preferences::GetBool(SMOOTH_SCROLL_PREF_NAME, false);
}

class ScrollFrameActivityTracker final : public nsExpirationTracker<ScrollFrameHelper,4> {
public:
  // Wait for 3-4s between scrolls before we remove our layers.
  // That's 4 generations of 1s each.
  enum { TIMEOUT_MS = 1000 };
  explicit ScrollFrameActivityTracker(nsIEventTarget* aEventTarget)
    : nsExpirationTracker<ScrollFrameHelper,4>(TIMEOUT_MS,
                                               "ScrollFrameActivityTracker",
                                               aEventTarget)
  {}
  ~ScrollFrameActivityTracker() {
    AgeAllGenerations();
  }

  virtual void NotifyExpired(ScrollFrameHelper *aObject) override {
    RemoveObject(aObject);
    aObject->MarkNotRecentlyScrolled();
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
  , mReferenceFrameDuringPainting(nullptr)
  , mAsyncScroll(nullptr)
  , mAsyncSmoothMSDScroll(nullptr)
  , mLastScrollOrigin(nsGkAtoms::other)
  , mAllowScrollOriginDowngrade(false)
  , mLastSmoothScrollOrigin(nullptr)
  , mScrollGeneration(++sScrollGenerationCounter)
  , mDestination(0, 0)
  , mScrollPosAtLastPaint(0, 0)
  , mRestorePos(-1, -1)
  , mLastPos(-1, -1)
  , mScrollPosForLayerPixelAlignment(-1, -1)
  , mLastUpdateFramesPos(-1, -1)
  , mHadDisplayPortAtLastFrameUpdate(false)
  , mDisplayPortAtLastFrameUpdate()
  , mNeverHasVerticalScrollbar(false)
  , mNeverHasHorizontalScrollbar(false)
  , mHasVerticalScrollbar(false)
  , mHasHorizontalScrollbar(false)
  , mFrameIsUpdatingScrollbar(false)
  , mDidHistoryRestore(false)
  , mIsRoot(aIsRoot)
  , mClipAllDescendants(aIsRoot)
  , mSuppressScrollbarUpdate(false)
  , mSkippedScrollbarLayout(false)
  , mHadNonInitialReflow(false)
  , mHorizontalOverflow(false)
  , mVerticalOverflow(false)
  , mPostedReflowCallback(false)
  , mMayHaveDirtyFixedChildren(false)
  , mUpdateScrollbarAttributes(false)
  , mHasBeenScrolledRecently(false)
  , mCollapsedResizer(false)
  , mWillBuildScrollableLayer(false)
  , mIsScrollParent(false)
  , mIsScrollableLayerInRootContainer(false)
  , mHasBeenScrolled(false)
  , mIgnoreMomentumScroll(false)
  , mTransformingByAPZ(false)
  , mScrollableByAPZ(false)
  , mZoomableByAPZ(false)
  , mHasOutOfFlowContentInsideFilter(false)
  , mSuppressScrollbarRepaints(false)
  , mVelocityQueue(aOuter->PresContext())
{
  if (LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) != 0) {
    mScrollbarActivity = new ScrollbarActivity(do_QueryFrame(aOuter));
  }

  EnsureFrameVisPrefsCached();

  if (IsAlwaysActive() &&
      gfxPrefs::LayersTilesEnabled() &&
      !nsLayoutUtils::UsesAsyncScrolling(mOuter) &&
      mOuter->GetContent()) {
    // If we have tiling but no APZ, then set a 0-margin display port on
    // active scroll containers so that we paint by whole tile increments
    // when scrolling.
    nsLayoutUtils::SetDisplayPortMargins(mOuter->GetContent(),
                                         mOuter->PresShell(),
                                         ScreenMargin(),
                                         0,
                                         nsLayoutUtils::RepaintMode::DoNotRepaint);
    nsLayoutUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(
        mOuter, nsLayoutUtils::RepaintMode::DoNotRepaint);
  }

}

ScrollFrameHelper::~ScrollFrameHelper()
{
  if (mScrollEvent) {
    mScrollEvent->Revoke();
  }
  if (mScrollEndEvent) {
    mScrollEndEvent->Revoke();
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
  MOZ_ASSERT(aInstance != nullptr, "aInstance must not be null");
  MOZ_ASSERT(aInstance->mAsyncScroll,
    "Did not expect AsyncScrollCallback without an active async scroll.");

  if (!aInstance || !aInstance->mAsyncScroll) {
    return;  // XXX wallpaper bug 1107353 for now.
  }

  nsRect range = aInstance->mAsyncScroll->mRange;
  if (aInstance->mAsyncScroll->IsSmoothScroll()) {
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
ScrollFrameHelper::CompleteAsyncScroll(const nsRect &aRange, nsAtom* aOrigin)
{
  // Apply desired destination range since this is the last step of scrolling.
  mAsyncSmoothMSDScroll = nullptr;
  mAsyncScroll = nullptr;
  AutoWeakFrame weakFrame(mOuter);
  ScrollToImpl(mDestination, aRange, aOrigin);
  if (!weakFrame.IsAlive()) {
    return;
  }
  // We are done scrolling, set our destination to wherever we actually ended
  // up scrolling to.
  mDestination = GetScrollPosition();
  PostScrollEndEvent();
}

bool
ScrollFrameHelper::HasPluginFrames()
{
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  if (XRE_IsContentProcess()) {
    nsPresContext* presContext = mOuter->PresContext();
    nsRootPresContext* rootPresContext = presContext->GetRootPresContext();
    if (!rootPresContext || rootPresContext->NeedToComputePluginGeometryUpdates()) {
      return true;
    }
  }
#endif
  return false;
}

bool
ScrollFrameHelper::HasPerspective() const
{
  const nsStyleDisplay* disp = mOuter->StyleDisplay();
  return disp->mChildPerspective.GetUnit() != eStyleUnit_None;
}

bool
ScrollFrameHelper::HasBgAttachmentLocal() const
{
  const nsStyleBackground* bg = mOuter->StyleBackground();
  return bg->HasLocalBackground();
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
                                                nsAtom *aOrigin)
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
                                          nsAtom *aOrigin,
                                          const nsRect* aRange,
                                          nsIScrollbarMediator::ScrollSnapMode aSnap)
{
  if (aOrigin != nsGkAtoms::restore) {
    // If we're doing a non-restore scroll, we don't want to later
    // override it by restoring our saved scroll position.
    mRestorePos.x = mRestorePos.y = -1;
  }

  if (aSnap == nsIScrollableFrame::ENABLE_SNAP) {
    GetSnapPointForDestination(nsIScrollableFrame::DEVICE_PIXELS,
                               mDestination,
                               aScrollPosition);
  }

  nsRect scrollRange = GetScrollRangeForClamping();
  mDestination = scrollRange.ClampPoint(aScrollPosition);
  if (mDestination != aScrollPosition && aOrigin == nsGkAtoms::restore &&
      GetPageLoadingState() != LoadingState::Loading) {
    // If we're doing a restore but the scroll position is clamped, promote
    // the origin from one that APZ can clobber to one that it can't clobber.
    aOrigin = nsGkAtoms::other;
  }

  nsRect range = aRange ? *aRange : nsRect(aScrollPosition, nsSize(0, 0));

  if (aMode != nsIScrollableFrame::SMOOTH_MSD) {
    // If we get a non-smooth-scroll, reset the cached APZ scroll destination,
    // so that we know to process the next smooth-scroll destined for APZ.
    mApzSmoothScrollDestination = Nothing();
  }

  if (aMode == nsIScrollableFrame::INSTANT) {
    // Asynchronous scrolling is not allowed, so we'll kill any existing
    // async-scrolling process and do an instant scroll.
    CompleteAsyncScroll(range, aOrigin);
    return;
  }

  nsPresContext* presContext = mOuter->PresContext();
  TimeStamp now = presContext->RefreshDriver()->IsTestControllingRefreshesEnabled()
                ? presContext->RefreshDriver()->MostRecentRefresh()
                : TimeStamp::Now();
  bool isSmoothScroll = (aMode == nsIScrollableFrame::SMOOTH) &&
                          IsSmoothScrollingEnabled();

  nsSize currentVelocity(0, 0);

  if (gfxPrefs::ScrollBehaviorEnabled()) {
    if (aMode == nsIScrollableFrame::SMOOTH_MSD) {
      mIgnoreMomentumScroll = true;
      if (!mAsyncSmoothMSDScroll) {
        nsPoint sv = mVelocityQueue.GetVelocity();
        currentVelocity.width = sv.x;
        currentVelocity.height = sv.y;
        if (mAsyncScroll) {
          if (mAsyncScroll->IsSmoothScroll()) {
            currentVelocity = mAsyncScroll->VelocityAt(now);
          }
          mAsyncScroll = nullptr;
        }

        if (nsLayoutUtils::AsyncPanZoomEnabled(mOuter) && WantAsyncScroll()) {
          if (mApzSmoothScrollDestination == Some(mDestination) &&
              mScrollGeneration == sScrollGenerationCounter) {
            // If we already sent APZ a smooth-scroll request to this
            // destination with this generation (i.e. it was the last request
            // we sent), then don't send another one because it is redundant.
            // This is to avoid a scenario where pages do repeated scrollBy
            // calls, incrementing the generation counter, and blocking APZ from
            // syncing the scroll offset back to the main thread.
            // Note that if we get two smooth-scroll requests to the same
            // destination with some other scroll in between,
            // mApzSmoothScrollDestination will get reset to Nothing() and so
            // we shouldn't have the problem where this check discards a
            // legitimate smooth-scroll.
            // Note: if there are two separate scrollframes both getting smooth
            // scrolled at the same time, sScrollGenerationCounter can get
            // incremented and this early-exit won't get taken. Bug 1231177 is
            // on file for this.
            return;
          }

          // The animation will be handled in the compositor, pass the
          // information needed to start the animation and skip the main-thread
          // animation for this scroll.
          mLastSmoothScrollOrigin = aOrigin;
          mApzSmoothScrollDestination = Some(mDestination);
          mScrollGeneration = ++sScrollGenerationCounter;

          if (!nsLayoutUtils::HasDisplayPort(mOuter->GetContent())) {
            // If this frame doesn't have a displayport then there won't be an
            // APZC instance for it and so there won't be anything to process
            // this smooth scroll request. We should set a displayport on this
            // frame to force an APZC which can handle the request.
            nsLayoutUtils::CalculateAndSetDisplayPortMargins(
              mOuter->GetScrollTargetFrame(),
              nsLayoutUtils::RepaintMode::DoNotRepaint);
            nsIFrame* frame = do_QueryFrame(mOuter->GetScrollTargetFrame());
            nsLayoutUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(
              frame,
              nsLayoutUtils::RepaintMode::DoNotRepaint);
          }

          // Schedule a paint to ensure that the frame metrics get updated on
          // the compositor thread.
          mOuter->SchedulePaint();
          return;
        }

        mAsyncSmoothMSDScroll =
          new AsyncSmoothMSDScroll(GetScrollPosition(), mDestination,
                                   currentVelocity, GetScrollRangeForClamping(),
                                   now, presContext);

        if (!mAsyncSmoothMSDScroll->SetRefreshObserver(this)) {
          // Observer setup failed. Scroll the normal way.
          CompleteAsyncScroll(range, aOrigin);
          return;
        }
      } else {
        // A previous smooth MSD scroll is still in progress, so we just need to
        // update its range and destination.
        mAsyncSmoothMSDScroll->SetRange(GetScrollRangeForClamping());
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
    mAsyncScroll = new AsyncScroll();
    if (!mAsyncScroll->SetRefreshObserver(this)) {
      // Observer setup failed. Scroll the normal way.
      CompleteAsyncScroll(range, aOrigin);
      return;
    }
  }

  if (isSmoothScroll) {
    mAsyncScroll->InitSmoothScroll(now, GetScrollPosition(), mDestination,
                                   aOrigin, range, currentVelocity);
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

bool ScrollFrameHelper::IsIgnoringViewportClipping() const
{
  if (!mIsRoot)
    return false;
  nsSubDocumentFrame* subdocFrame = static_cast<nsSubDocumentFrame*>
    (nsLayoutUtils::GetCrossDocParentFrame(mOuter->PresShell()->GetRootFrame()));
  return subdocFrame && !subdocFrame->ShouldClipSubdocument();
}

void ScrollFrameHelper::MarkScrollbarsDirtyForReflow() const
{
  nsIPresShell* presShell = mOuter->PresShell();
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
    (nsLayoutUtils::GetCrossDocParentFrame(mOuter->PresShell()->GetRootFrame()));
  return !subdocFrame || subdocFrame->ShouldClampScrollPosition();
}

bool ScrollFrameHelper::IsAlwaysActive() const
{
  if (nsDisplayItem::ForceActiveLayers()) {
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

/*static*/ void
RemoveDisplayPortCallback(nsITimer* aTimer, void* aClosure)
{
  ScrollFrameHelper* helper = static_cast<ScrollFrameHelper*>(aClosure);

  // This function only ever gets called from the expiry timer, so it must
  // be non-null here. Set it to null here so that we don't keep resetting
  // it unnecessarily in MarkRecentlyScrolled().
  MOZ_ASSERT(helper->mDisplayPortExpiryTimer);
  helper->mDisplayPortExpiryTimer = nullptr;

  if (!helper->AllowDisplayPortExpiration() || helper->mIsScrollParent) {
    // If this is a scroll parent for some other scrollable frame, don't
    // expire the displayport because it would break scroll handoff. Once the
    // descendant scrollframes have their displayports expired, they will
    // trigger the displayport expiration on this scrollframe as well, and
    // mIsScrollParent will presumably be false when that kicks in.
    return;
  }

  // Remove the displayport from this scrollframe if it's been a while
  // since it's scrolled, except if it needs to be always active. Note that
  // there is one scrollframe that doesn't fall under this general rule, and
  // that is the one that nsLayoutUtils::MaybeCreateDisplayPort decides to put
  // a displayport on (i.e. the first scrollframe that WantAsyncScroll()s).
  // If that scrollframe is this one, we remove the displayport anyway, and
  // as part of the next paint MaybeCreateDisplayPort will put another
  // displayport back on it. Although the displayport will "flicker" off and
  // back on, the layer itself should never disappear, because this all
  // happens between actual painting. If the displayport is reset to a
  // different position that's ok; this scrollframe hasn't been scrolled
  // recently and so the reset should be correct.
  nsLayoutUtils::RemoveDisplayPort(helper->mOuter->GetContent());
  nsLayoutUtils::ExpireDisplayPortOnAsyncScrollableAncestor(helper->mOuter);
  helper->mOuter->SchedulePaint();
  // Be conservative and unflag this this scrollframe as being scrollable by
  // APZ. If it is still scrollable this will get flipped back soon enough.
  helper->mScrollableByAPZ = false;
}

void ScrollFrameHelper::MarkNotRecentlyScrolled()
{
  if (!mHasBeenScrolledRecently)
    return;

  mHasBeenScrolledRecently = false;
  mOuter->SchedulePaint();
}

void ScrollFrameHelper::MarkRecentlyScrolled()
{
  mHasBeenScrolledRecently = true;
  if (IsAlwaysActive()) {
    return;
  }

  if (mActivityExpirationState.IsTracked()) {
    gScrollFrameActivityTracker->MarkUsed(this);
  } else {
    if (!gScrollFrameActivityTracker) {
      gScrollFrameActivityTracker = new ScrollFrameActivityTracker(
        SystemGroup::EventTargetFor(TaskCategory::Other));
    }
    gScrollFrameActivityTracker->AddObject(this);
  }

  // If we just scrolled and there's a displayport expiry timer in place,
  // reset the timer.
  ResetDisplayPortExpiryTimer();
}

void ScrollFrameHelper::ResetDisplayPortExpiryTimer()
{
  if (mDisplayPortExpiryTimer) {
    mDisplayPortExpiryTimer->InitWithNamedFuncCallback(
      RemoveDisplayPortCallback,
      this,
      gfxPrefs::APZDisplayPortExpiryTime(),
      nsITimer::TYPE_ONE_SHOT,
      "ScrollFrameHelper::ResetDisplayPortExpiryTimer");
  }
}

bool ScrollFrameHelper::AllowDisplayPortExpiration()
{
  if (IsAlwaysActive()) {
    return false;
  }
  if (mIsRoot && mOuter->PresContext()->IsRoot()) {
    return false;
  }
  return true;
}

void ScrollFrameHelper::TriggerDisplayPortExpiration()
{
  if (!AllowDisplayPortExpiration()) {
    return;
  }

  if (!gfxPrefs::APZDisplayPortExpiryTime()) {
    // a zero time disables the expiry
    return;
  }

  if (!mDisplayPortExpiryTimer) {
    mDisplayPortExpiryTimer = NS_NewTimer();
  }
  ResetDisplayPortExpiryTimer();
}

void ScrollFrameHelper::ScrollVisual()
{
  // Mark this frame as having been scrolled. If this is the root
  // scroll frame of a content document, then IsAlwaysActive()
  // will return true from now on and MarkNotRecentlyScrolled() won't
  // have any effect.
  mHasBeenScrolled = true;

  AdjustViews(mScrolledFrame);
  // We need to call this after fixing up the view positions
  // to be consistent with the frame hierarchy.
  MarkRecentlyScrolled();
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
  self->mOuter->PresShell()->SynthesizeMouseMove(true);
}


void
ScrollFrameHelper::ScheduleSyntheticMouseMove()
{
  if (!mScrollActivityTimer) {
    mScrollActivityTimer = NS_NewTimer(
      mOuter->PresContext()->Document()->EventTargetFor(TaskCategory::Other));
    if (!mScrollActivityTimer) {
      return;
    }
  }

  mScrollActivityTimer->InitWithNamedFuncCallback(
    ScrollActivityCallback,
    this,
    100,
    nsITimer::TYPE_ONE_SHOT,
    "ScrollFrameHelper::ScheduleSyntheticMouseMove");
}

void
ScrollFrameHelper::NotifyApproximateFrameVisibilityUpdate(bool aIgnoreDisplayPort)
{
  mLastUpdateFramesPos = GetScrollPosition();
  if (aIgnoreDisplayPort) {
    mHadDisplayPortAtLastFrameUpdate = false;
    mDisplayPortAtLastFrameUpdate = nsRect();
  } else {
    mHadDisplayPortAtLastFrameUpdate =
      nsLayoutUtils::GetDisplayPort(mOuter->GetContent(),
                                    &mDisplayPortAtLastFrameUpdate);
  }
}

bool
ScrollFrameHelper::GetDisplayPortAtLastApproximateFrameVisibilityUpdate(nsRect* aDisplayPort)
{
  if (mHadDisplayPortAtLastFrameUpdate) {
    *aDisplayPort = mDisplayPortAtLastFrameUpdate;
  }
  return mHadDisplayPortAtLastFrameUpdate;
}

void
ScrollFrameHelper::ScrollToImpl(nsPoint aPt, const nsRect& aRange, nsAtom* aOrigin)
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

  bool needFrameVisibilityUpdate = mLastUpdateFramesPos == nsPoint(-1,-1);

  nsPoint dist(std::abs(pt.x - mLastUpdateFramesPos.x),
               std::abs(pt.y - mLastUpdateFramesPos.y));
  nsSize scrollPortSize = GetScrollPositionClampingScrollPortSize();
  nscoord horzAllowance = std::max(scrollPortSize.width / std::max(sHorzScrollFraction, 1),
                                   nsPresContext::AppUnitsPerCSSPixel());
  nscoord vertAllowance = std::max(scrollPortSize.height / std::max(sVertScrollFraction, 1),
                                   nsPresContext::AppUnitsPerCSSPixel());
  if (dist.x >= horzAllowance || dist.y >= vertAllowance) {
    needFrameVisibilityUpdate = true;
  }

  // notify the listeners.
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->ScrollPositionWillChange(pt.x, pt.y);
  }

  nsRect oldDisplayPort;
  nsIContent* content = mOuter->GetContent();
  nsLayoutUtils::GetHighResolutionDisplayPort(content, &oldDisplayPort);
  oldDisplayPort.MoveBy(-mScrolledFrame->GetPosition());

  // Update frame position for scrolling
  mScrolledFrame->SetPosition(mScrollPort.TopLeft() - pt);

  // If |mLastScrollOrigin| is already set to something that can clobber APZ's
  // scroll offset, then we don't want to change it to something that can't.
  // If we allowed this, then we could end up in a state where APZ ignores
  // legitimate scroll offset updates because the origin has been masked by
  // a later change within the same refresh driver tick.
  bool isScrollOriginDowngrade =
    nsLayoutUtils::CanScrollOriginClobberApz(mLastScrollOrigin) &&
    !nsLayoutUtils::CanScrollOriginClobberApz(aOrigin);
  bool allowScrollOriginChange = mAllowScrollOriginDowngrade ||
    !isScrollOriginDowngrade;
  if (allowScrollOriginChange) {
    mLastScrollOrigin = aOrigin;
    mAllowScrollOriginDowngrade = false;
  }
  mLastSmoothScrollOrigin = nullptr;
  mScrollGeneration = ++sScrollGenerationCounter;

  ScrollVisual();

  bool schedulePaint = true;
  if (nsLayoutUtils::AsyncPanZoomEnabled(mOuter) &&
      !nsLayoutUtils::ShouldDisableApzForElement(content) &&
      gfxPrefs::APZPaintSkipping()) {
    // If APZ is enabled with paint-skipping, there are certain conditions in
    // which we can skip paints:
    // 1) If APZ triggered this scroll, and the tile-aligned displayport is
    //    unchanged.
    // 2) If non-APZ triggered this scroll, but we can handle it by just asking
    //    APZ to update the scroll position. Again we make this conditional on
    //    the tile-aligned displayport being unchanged.
    // We do the displayport check first since it's common to all scenarios,
    // and then if the displayport is unchanged, we check if APZ triggered,
    // or can handle, this scroll. If so, we set schedulePaint to false and
    // skip the paint.
    // Because of bug 1264297, we also don't do paint-skipping for elements with
    // perspective, because the displayport may not have captured everything
    // that needs to be painted. So even if the final tile-aligned displayport
    // is the same, we force a repaint for these elements. Bug 1254260 tracks
    // fixing this properly.
    nsRect displayPort;
    bool usingDisplayPort =
      nsLayoutUtils::GetHighResolutionDisplayPort(content, &displayPort);
    displayPort.MoveBy(-mScrolledFrame->GetPosition());

    PAINT_SKIP_LOG("New scrollpos %s usingDP %d dpEqual %d scrollableByApz %d plugins"
        "%d perspective %d bglocal %d filter %d\n",
        Stringify(CSSPoint::FromAppUnits(GetScrollPosition())).c_str(),
        usingDisplayPort, displayPort.IsEqualEdges(oldDisplayPort),
        mScrollableByAPZ, HasPluginFrames(), HasPerspective(),
        HasBgAttachmentLocal(), mHasOutOfFlowContentInsideFilter);
    if (usingDisplayPort && displayPort.IsEqualEdges(oldDisplayPort) &&
        !HasPerspective() && !HasBgAttachmentLocal() &&
        !mHasOutOfFlowContentInsideFilter) {
      bool haveScrollLinkedEffects = content->GetComposedDoc()->HasScrollLinkedEffect();
      bool apzDisabled = haveScrollLinkedEffects && gfxPrefs::APZDisableForScrollLinkedEffects();
      if (!apzDisabled && !HasPluginFrames()) {
        if (LastScrollOrigin() == nsGkAtoms::apz) {
          schedulePaint = false;
          PAINT_SKIP_LOG("Skipping due to APZ scroll\n");
        } else if (mScrollableByAPZ) {
          nsIWidget* widget = presContext->GetNearestWidget();
          LayerManager* manager = widget ? widget->GetLayerManager() : nullptr;
          if (manager) {
            mozilla::layers::FrameMetrics::ViewID id;
            bool success = nsLayoutUtils::FindIDFor(content, &id);
            MOZ_ASSERT(success); // we have a displayport, we better have an ID

            // Schedule an empty transaction to carry over the scroll offset update,
            // instead of a full transaction. This empty transaction might still get
            // squashed into a full transaction if something happens to trigger one.
            success = manager->SetPendingScrollUpdateForNextTransaction(id,
                { mScrollGeneration, CSSPoint::FromAppUnits(GetScrollPosition()) });
            if (success) {
              schedulePaint = false;
              mOuter->SchedulePaint(nsIFrame::PAINT_COMPOSITE_ONLY);
              PAINT_SKIP_LOG("Skipping due to APZ-forwarded main-thread scroll\n");
            } else {
              PAINT_SKIP_LOG("Failed to set pending scroll update on layer manager\n");
            }
          }
        }
      }
    }
  }

  if (schedulePaint) {
    mOuter->SchedulePaint();

    if (needFrameVisibilityUpdate) {
      presContext->PresShell()->ScheduleApproximateFrameVisibilityUpdateNow();
    }
  }

  if (mOuter->ChildrenHavePerspective()) {
    // The overflow areas of descendants may depend on the scroll position,
    // so ensure they get updated.

    // First we recompute the overflow areas of the transformed children
    // that use the perspective. FinishAndStoreOverflow only calls this
    // if the size changes, so we need to do it manually.
    mOuter->RecomputePerspectiveChildrenOverflow(mOuter);

    // Update the overflow for the scrolled frame to take any changes from the
    // children into account.
    mScrolledFrame->UpdateOverflow();

    // Update the overflow for the outer so that we recompute scrollbars.
    mOuter->UpdateOverflow();
  }

  ScheduleSyntheticMouseMove();

  { // scope the AutoScrollbarRepaintSuppression
    AutoScrollbarRepaintSuppression repaintSuppression(this, !schedulePaint);
    AutoWeakFrame weakFrame(mOuter);
    UpdateScrollbarPosition();
    if (!weakFrame.IsAlive()) {
      return;
    }
  }

  presContext->RecordInteractionTime(
    nsPresContext::InteractionType::eScrollInteraction,
    TimeStamp::Now());

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
  int32_t maxZIndex = -1;
  for (nsDisplayItem* item = aList->GetBottom(); item; item = item->GetAbove()) {
    maxZIndex = std::max(maxZIndex, item->ZIndex());
  }
  return maxZIndex;
}

// Finds the max z-index of the items in aList that meet the following conditions
//   1) have z-index auto or z-index >= 0.
//   2) aFrame is a proper ancestor of the item's frame.
// Returns -1 if there is no such item.
static int32_t
MaxZIndexInListOfItemsContainedInFrame(nsDisplayList* aList, nsIFrame* aFrame)
{
  int32_t maxZIndex = -1;
  for (nsDisplayItem* item = aList->GetBottom(); item; item = item->GetAbove()) {
    nsIFrame* itemFrame = item->Frame();
    // Perspective items return the scroll frame as their Frame(), so consider
    // their TransformFrame() instead.
    if (item->GetType() == DisplayItemType::TYPE_PERSPECTIVE) {
      itemFrame = static_cast<nsDisplayPerspective*>(item)->TransformFrame();
    }
    if (nsLayoutUtils::IsProperAncestorFrame(aFrame, itemFrame)) {
      maxZIndex = std::max(maxZIndex, item->ZIndex());
    }
  }
  return maxZIndex;
}

template<class T>
static void
AppendInternalItemToTop(const nsDisplayListSet& aLists,
                        T* aItem,
                        int32_t aZIndex)
{
  if (aZIndex >= 0) {
    aItem->SetOverrideZIndex(aZIndex);
    aLists.PositionedDescendants()->AppendToTop(aItem);
  } else {
    aLists.Content()->AppendToTop(aItem);
  }
}

static const uint32_t APPEND_OWN_LAYER = 0x1;
static const uint32_t APPEND_POSITIONED = 0x2;
static const uint32_t APPEND_SCROLLBAR_CONTAINER = 0x4;

static void
AppendToTop(nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists,
            nsDisplayList* aSource, nsIFrame* aSourceFrame, uint32_t aFlags)
{
  if (aSource->IsEmpty())
    return;

  nsDisplayWrapList* newItem;
  const ActiveScrolledRoot* asr = aBuilder->CurrentActiveScrolledRoot();
  if (aFlags & APPEND_OWN_LAYER) {
    FrameMetrics::ViewID scrollTarget = FrameMetrics::NULL_SCROLL_ID;
    nsDisplayOwnLayerFlags flags = aBuilder->GetCurrentScrollbarFlags();
    // The flags here should be at most one scrollbar direction and nothing else
    MOZ_ASSERT(flags == nsDisplayOwnLayerFlags::eNone ||
               flags == nsDisplayOwnLayerFlags::eVerticalScrollbar ||
               flags == nsDisplayOwnLayerFlags::eHorizontalScrollbar);

    if (aFlags & APPEND_SCROLLBAR_CONTAINER) {
      scrollTarget = aBuilder->GetCurrentScrollbarTarget();
      // The flags here should be exactly one scrollbar direction
      MOZ_ASSERT(flags != nsDisplayOwnLayerFlags::eNone);
      flags |= nsDisplayOwnLayerFlags::eScrollbarContainer;
    }

    newItem = new (aBuilder) nsDisplayOwnLayer(aBuilder, aSourceFrame, aSource, asr, flags, scrollTarget);
  } else {
    newItem = new (aBuilder) nsDisplayWrapList(aBuilder, aSourceFrame, aSource, asr);
  }

  if (aFlags & APPEND_POSITIONED) {
    // We want overlay scrollbars to always be on top of the scrolled content,
    // but we don't want them to unnecessarily cover overlapping elements from
    // outside our scroll frame.
    int32_t zIndex = MaxZIndexInList(aLists.PositionedDescendants(), aBuilder);
    AppendInternalItemToTop(aLists, newItem, zIndex);
  } else {
    aLists.BorderBackground()->AppendToTop(newItem);
  }
}

struct HoveredStateComparator
{
  static bool Hovered(const nsIFrame* aFrame)
  {
      return aFrame->GetContent()->IsElement() &&
             aFrame->GetContent()->AsElement()->HasAttr(kNameSpaceID_None,
                                                        nsGkAtoms::hover);
  }

  bool Equals(nsIFrame* A, nsIFrame* B) const {
    return Hovered(A) == Hovered(B);
  }

  bool LessThan(nsIFrame* A, nsIFrame* B) const {
    return !Hovered(A) && Hovered(B);
  }
};

void
ScrollFrameHelper::AppendScrollPartsTo(nsDisplayListBuilder*   aBuilder,
                                       const nsDisplayListSet& aLists,
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

  AutoTArray<nsIFrame*, 3> scrollParts;
  for (nsIFrame* kid : mOuter->PrincipalChildList()) {
    if (kid == mScrolledFrame ||
        (kid->IsAbsPosContainingBlock() || overlayScrollbars) != aPositioned) {
      continue;
    }

    scrollParts.AppendElement(kid);
  }
  if (scrollParts.IsEmpty()) {
    return;
  }

  // We can't check will-change budget during display list building phase.
  // This means that we will build scroll bar layers for out of budget
  // will-change: scroll position.
  mozilla::layers::FrameMetrics::ViewID scrollTargetId = IsMaybeScrollingActive()
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
    nsDisplayOwnLayerFlags flags = nsDisplayOwnLayerFlags::eNone;
    uint32_t appendToTopFlags = 0;
    if (scrollParts[i] == mVScrollbarBox) {
      flags |= nsDisplayOwnLayerFlags::eVerticalScrollbar;
      appendToTopFlags |= APPEND_SCROLLBAR_CONTAINER;
    }
    if (scrollParts[i] == mHScrollbarBox) {
      flags |= nsDisplayOwnLayerFlags::eHorizontalScrollbar;
      appendToTopFlags |= APPEND_SCROLLBAR_CONTAINER;
    }

    // The display port doesn't necessarily include the scrollbars, so just
    // include all of the scrollbars if we are in a RCD-RSF. We only do
    // this for the root scrollframe of the root content document, which is
    // zoomable, and where the scrollbar sizes are bounded by the widget.
    nsRect visible = mIsRoot && mOuter->PresContext()->IsRootContentDocument()
                     ? scrollParts[i]->GetVisualOverflowRectRelativeToParent()
                     : aBuilder->GetVisibleRect();
    nsRect dirty = mIsRoot && mOuter->PresContext()->IsRootContentDocument()
                     ? scrollParts[i]->GetVisualOverflowRectRelativeToParent()
                     : aBuilder->GetDirtyRect();

    // Always create layers for overlay scrollbars so that we don't create a
    // giant layer covering the whole scrollport if both scrollbars are visible.
    bool isOverlayScrollbar = (flags != nsDisplayOwnLayerFlags::eNone) && overlayScrollbars;
    bool createLayer = aCreateLayer || isOverlayScrollbar ||
                       gfxPrefs::AlwaysLayerizeScrollbarTrackTestOnly();

    nsDisplayListCollection partList(aBuilder);
    {
      nsDisplayListBuilder::AutoBuildingDisplayList
        buildingForChild(aBuilder, mOuter,
                         visible, dirty, true);

      nsDisplayListBuilder::AutoCurrentScrollbarInfoSetter
        infoSetter(aBuilder, scrollTargetId, flags, createLayer);
      mOuter->BuildDisplayListForChild(
        aBuilder, scrollParts[i], partList,
        nsIFrame::DISPLAY_CHILD_FORCE_STACKING_CONTEXT);
    }

    if (createLayer) {
      appendToTopFlags |= APPEND_OWN_LAYER;
    }
    if (aPositioned) {
      appendToTopFlags |= APPEND_POSITIONED;
    }

    {
      nsDisplayListBuilder::AutoBuildingDisplayList
        buildingForChild(aBuilder, scrollParts[i],
                         visible + mOuter->GetOffsetTo(scrollParts[i]),
                         dirty + mOuter->GetOffsetTo(scrollParts[i]), true);
      nsDisplayListBuilder::AutoCurrentScrollbarInfoSetter
        infoSetter(aBuilder, scrollTargetId, flags, createLayer);
      // DISPLAY_CHILD_FORCE_STACKING_CONTEXT put everything into
      // partList.PositionedDescendants().
      ::AppendToTop(aBuilder, aLists,
                    partList.PositionedDescendants(), scrollParts[i],
                    appendToTopFlags);
    }
  }
}

/* static */ bool ScrollFrameHelper::sFrameVisPrefsCached = false;
/* static */ uint32_t ScrollFrameHelper::sHorzExpandScrollPort = 0;
/* static */ uint32_t ScrollFrameHelper::sVertExpandScrollPort = 1;
/* static */ int32_t ScrollFrameHelper::sHorzScrollFraction = 2;
/* static */ int32_t ScrollFrameHelper::sVertScrollFraction = 2;

/* static */ void
ScrollFrameHelper::EnsureFrameVisPrefsCached()
{
  if (!sFrameVisPrefsCached) {
    Preferences::AddUintVarCache(&sHorzExpandScrollPort,
      "layout.framevisibility.numscrollportwidths", (uint32_t)0);
    Preferences::AddUintVarCache(&sVertExpandScrollPort,
      "layout.framevisibility.numscrollportheights", 1);

    Preferences::AddIntVarCache(&sHorzScrollFraction,
      "layout.framevisibility.amountscrollbeforeupdatehorizontal", 2);
    Preferences::AddIntVarCache(&sVertScrollFraction,
      "layout.framevisibility.amountscrollbeforeupdatevertical", 2);

    sFrameVisPrefsCached = true;
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
ClipItemsExceptCaret(nsDisplayList* aList,
                     nsDisplayListBuilder* aBuilder,
                     nsIFrame* aClipFrame,
                     const DisplayItemClipChain* aExtraClip,
                     nsDataHashtable<nsPtrHashKey<const DisplayItemClipChain>, const DisplayItemClipChain*>& aCache)
{
  for (nsDisplayItem* i = aList->GetBottom(); i; i = i->GetAbove()) {
    if (!ShouldBeClippedByFrame(aClipFrame, i->Frame())) {
      continue;
    }

    if (i->GetType() != DisplayItemType::TYPE_CARET) {
      const DisplayItemClipChain* clip = i->GetClipChain();
      const DisplayItemClipChain* intersection = nullptr;
      if (aCache.Get(clip, &intersection)) {
        i->SetClipChain(intersection, true);
      } else {
        i->IntersectClip(aBuilder, aExtraClip, true);
        aCache.Put(clip, i->GetClipChain());
      }
    }
    nsDisplayList* children = i->GetSameCoordinateSystemChildren();
    if (children) {
      ClipItemsExceptCaret(children, aBuilder, aClipFrame, aExtraClip, aCache);
    }
  }
}

static void
ClipListsExceptCaret(nsDisplayListCollection* aLists,
                     nsDisplayListBuilder* aBuilder,
                     nsIFrame* aClipFrame,
                     const DisplayItemClipChain* aExtraClip)
{
  nsDataHashtable<nsPtrHashKey<const DisplayItemClipChain>, const DisplayItemClipChain*> cache;
  ClipItemsExceptCaret(aLists->BorderBackground(), aBuilder, aClipFrame, aExtraClip, cache);
  ClipItemsExceptCaret(aLists->BlockBorderBackgrounds(), aBuilder, aClipFrame, aExtraClip, cache);
  ClipItemsExceptCaret(aLists->Floats(), aBuilder, aClipFrame, aExtraClip, cache);
  ClipItemsExceptCaret(aLists->PositionedDescendants(), aBuilder, aClipFrame, aExtraClip, cache);
  ClipItemsExceptCaret(aLists->Outlines(), aBuilder, aClipFrame, aExtraClip, cache);
  ClipItemsExceptCaret(aLists->Content(), aBuilder, aClipFrame, aExtraClip, cache);
}

void
ScrollFrameHelper::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                    const nsDisplayListSet& aLists)
{
  SetAndNullOnExit<const nsIFrame> tmpBuilder(mReferenceFrameDuringPainting, aBuilder->GetCurrentReferenceFrame());
  if (aBuilder->IsForFrameVisibility()) {
    NotifyApproximateFrameVisibilityUpdate(false);
  }

  mOuter->DisplayBorderBackgroundOutline(aBuilder, aLists);

  if (aBuilder->IsPaintingToWindow()) {
    mScrollPosAtLastPaint = GetScrollPosition();
    if (IsMaybeScrollingActive()) {
      if (mScrollPosForLayerPixelAlignment == nsPoint(-1,-1)) {
        mScrollPosForLayerPixelAlignment = mScrollPosAtLastPaint;
      }
    } else {
      mScrollPosForLayerPixelAlignment = nsPoint(-1,-1);
    }
  }

  // It's safe to get this value before the DecideScrollableLayer call below
  // because that call cannot create a displayport for root scroll frames,
  // and hence it cannot create an ignore scroll frame.
  bool ignoringThisScrollFrame =
    aBuilder->GetIgnoreScrollFrame() == mOuter || IsIgnoringViewportClipping();

  // Overflow clipping can never clip frames outside our subtree, so there
  // is no need to worry about whether we are a moving frame that might clip
  // non-moving frames.
  // Not all our descendants will be clipped by overflow clipping, but all
  // the ones that aren't clipped will be out of flow frames that have already
  // had dirty rects saved for them by their parent frames calling
  // MarkOutOfFlowChildrenForDisplayList, so it's safe to restrict our
  // dirty rect here.
  nsRect visibleRect = aBuilder->GetVisibleRect();
  nsRect dirtyRect = aBuilder->GetDirtyRect();
  if (!ignoringThisScrollFrame) {
    visibleRect = visibleRect.Intersect(mScrollPort);
    dirtyRect = dirtyRect.Intersect(mScrollPort);
  }

  bool dirtyRectHasBeenOverriden = false;
  Unused << DecideScrollableLayer(aBuilder, &visibleRect, &dirtyRect,
              /* aSetBase = */ !mIsRoot, &dirtyRectHasBeenOverriden);

  if (aBuilder->IsForFrameVisibility()) {
    // We expand the dirty rect to catch frames just outside of the scroll port.
    // We use the dirty rect instead of the whole scroll port to prevent
    // too much expansion in the presence of very large (bigger than the
    // viewport) scroll ports.
    dirtyRect = ExpandRectToNearlyVisible(dirtyRect);
    visibleRect = dirtyRect;
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

  nsIScrollableFrame* sf = do_QueryFrame(mOuter);
  MOZ_ASSERT(sf);

  if (ignoringThisScrollFrame) {
    // Root scrollframes have FrameMetrics and clipping on their container
    // layers, so don't apply clipping again.
    mAddClipRectToLayer = false;

    // If we are a root scroll frame that has a display port we want to add
    // scrollbars, they will be children of the scrollable layer, but they get
    // adjusted by the APZC automatically.
    bool addScrollBars = mIsRoot && mWillBuildScrollableLayer && aBuilder->IsPaintingToWindow();

    if (addScrollBars) {
      // Add classic scrollbars.
      AppendScrollPartsTo(aBuilder, aLists, createLayersForScrollbars, false);
    }

    {
      nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter asrSetter(aBuilder);
      if (aBuilder->IsPaintingToWindow() &&
          gfxPrefs::LayoutUseContainersForRootFrames() && mIsRoot) {
        asrSetter.EnterScrollFrame(sf);
        aBuilder->SetActiveScrolledRootForRootScrollframe(aBuilder->CurrentActiveScrolledRoot());
      }

      nsDisplayListBuilder::AutoBuildingDisplayList
        building(aBuilder, mOuter, visibleRect, dirtyRect, aBuilder->IsAtRootOfPseudoStackingContext());

      // Don't clip the scrolled child, and don't paint scrollbars/scrollcorner.
      // The scrolled frame shouldn't have its own background/border, so we
      // can just pass aLists directly.
      mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame, aLists);
    }

    if (addScrollBars) {
      // Add overlay scrollbars.
      AppendScrollPartsTo(aBuilder, aLists, createLayersForScrollbars, true);
    }

    return;
  }

  // Root scrollframes have FrameMetrics and clipping on their container
  // layers, so don't apply clipping again.
  mAddClipRectToLayer =
    !(mIsRoot && mOuter->PresShell()->GetIsViewportOverridden());

  // Whether we might want to build a scrollable layer for this scroll frame
  // at some point in the future. This controls whether we add the information
  // to the layer tree (a scroll info layer if necessary, and add the right
  // area to the dispatch to content layer event regions) necessary to activate
  // a scroll frame so it creates a scrollable layer.
  bool couldBuildLayer = false;
  if (aBuilder->IsPaintingToWindow()) {
    if (mWillBuildScrollableLayer) {
      couldBuildLayer = true;
    } else {
      couldBuildLayer =
        nsLayoutUtils::AsyncPanZoomEnabled(mOuter) &&
        WantAsyncScroll() &&
        // If we are using containers for root frames, and we are the root
        // scroll frame for the display root, then we don't need a scroll
        // info layer. nsDisplayList::PaintForFrame already calls
        // ComputeFrameMetrics for us.
        (!(gfxPrefs::LayoutUseContainersForRootFrames() && mIsRoot) ||
         (aBuilder->RootReferenceFrame()->PresContext() != mOuter->PresContext()));
    }
  }

  // Now display the scrollbars and scrollcorner. These parts are drawn
  // in the border-background layer, on top of our own background and
  // borders and underneath borders and backgrounds of later elements
  // in the tree.
  // Note that this does not apply for overlay scrollbars; those are drawn
  // in the positioned-elements layer on top of everything else by the call
  // to AppendScrollPartsTo(..., true) further down.
  AppendScrollPartsTo(aBuilder, aLists, createLayersForScrollbars, false);

  const nsStyleDisplay* disp = mOuter->StyleDisplay();
  if (disp && (disp->mWillChangeBitField & NS_STYLE_WILL_CHANGE_SCROLL)) {
    aBuilder->AddToWillChangeBudget(mOuter, GetScrollPositionClampingScrollPortSize());
  }

  mScrollParentID = aBuilder->GetCurrentScrollParentId();

  Maybe<nsRect> contentBoxClip;
  Maybe<const DisplayItemClipChain*> extraContentBoxClipForNonCaretContent;
  if (MOZ_UNLIKELY(disp->mOverflowClipBoxBlock ==
                     NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX ||
                   disp->mOverflowClipBoxInline ==
                      NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX)) {
    WritingMode wm = mScrolledFrame->GetWritingMode();
    bool cbH = (wm.IsVertical() ? disp->mOverflowClipBoxBlock
                                : disp->mOverflowClipBoxInline) ==
               NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX;
    bool cbV = (wm.IsVertical() ? disp->mOverflowClipBoxInline
                                : disp->mOverflowClipBoxBlock) ==
               NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX;
    // We only clip if there is *scrollable* overflow, to avoid clipping
    // *visual* overflow unnecessarily.
    nsRect clipRect = mScrollPort + aBuilder->ToReferenceFrame(mOuter);
    nsRect so = mScrolledFrame->GetScrollableOverflowRect();
    if ((cbH && (clipRect.width != so.width || so.x < 0)) ||
        (cbV && (clipRect.height != so.height || so.y < 0))) {
      nsMargin padding = mOuter->GetUsedPadding();
      if (!cbH) {
        padding.left = padding.right = nscoord(0);
      }
      if (!cbV) {
        padding.top = padding.bottom = nscoord(0);
      }
      clipRect.Deflate(padding);

      // The non-inflated clip needs to be set on all non-caret items.
      // We prepare an extra DisplayItemClipChain here that will be intersected
      // with those items after they've been created.
      const ActiveScrolledRoot* asr = aBuilder->CurrentActiveScrolledRoot();

      DisplayItemClip newClip;
      newClip.SetTo(clipRect);

      const DisplayItemClipChain* extraClip =
        aBuilder->AllocateDisplayItemClipChain(newClip, asr, nullptr);

      extraContentBoxClipForNonCaretContent = Some(extraClip);

      nsIFrame* caretFrame = aBuilder->GetCaretFrame();
      // Avoid clipping it in a zero-height line box (heuristic only).
      if (caretFrame && caretFrame->GetRect().height != 0) {
        nsRect caretRect = aBuilder->GetCaretRect();
        // Allow the caret to stick out of the content box clip by half the
        // caret height on the top, and its full width on the right.
        nsRect inflatedClip = clipRect;
        inflatedClip.Inflate(nsMargin(caretRect.height / 2, caretRect.width, 0, 0));
        contentBoxClip = Some(inflatedClip);
      }
    }
  }

  nsDisplayListCollection scrolledContent(aBuilder);
  {
    // Note that setting the current scroll parent id here means that positioned children
    // of this scroll info layer will pick up the scroll info layer as their scroll handoff
    // parent. This is intentional because that is what happens for positioned children
    // of scroll layers, and we want to maintain consistent behaviour between scroll layers
    // and scroll info layers.
    nsDisplayListBuilder::AutoCurrentScrollParentIdSetter idSetter(
        aBuilder,
        couldBuildLayer && mScrolledFrame->GetContent()
            ? nsLayoutUtils::FindOrCreateIDFor(mScrolledFrame->GetContent())
            : aBuilder->GetCurrentScrollParentId());

    nsRect clipRect = mScrollPort + aBuilder->ToReferenceFrame(mOuter);
    // Our override of GetBorderRadii ensures we never have a radius at
    // the corners where we have a scrollbar.
    nscoord radii[8];
    bool haveRadii = mOuter->GetPaddingBoxBorderRadii(radii);
    if (mIsRoot) {
      clipRect.SizeTo(nsLayoutUtils::CalculateCompositionSizeForFrame(mOuter));
      if (mOuter->PresContext()->IsRootContentDocument()) {
        double res = mOuter->PresShell()->GetResolution();
        clipRect.width = NSToCoordRound(clipRect.width / res);
        clipRect.height = NSToCoordRound(clipRect.height / res);
      }
    }

    DisplayListClipState::AutoSaveRestore clipState(aBuilder);
    if (mClipAllDescendants) {
      clipState.ClipContentDescendants(clipRect, haveRadii ? radii : nullptr);
    } else {
      clipState.ClipContainingBlockDescendants(clipRect, haveRadii ? radii : nullptr);
    }

    Maybe<DisplayListClipState::AutoSaveRestore> contentBoxClipState;;
    if (contentBoxClip) {
      contentBoxClipState.emplace(aBuilder);
      if (mClipAllDescendants) {
        contentBoxClipState->ClipContentDescendants(*contentBoxClip);
      } else {
        contentBoxClipState->ClipContainingBlockDescendants(*contentBoxClip);
      }
    }

    nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter asrSetter(aBuilder);
    if (mWillBuildScrollableLayer && aBuilder->IsPaintingToWindow()) {
      asrSetter.EnterScrollFrame(sf);
    }

    if (mIsScrollableLayerInRootContainer) {
      aBuilder->SetActiveScrolledRootForRootScrollframe(aBuilder->CurrentActiveScrolledRoot());
    }

    {
      // Clip our contents to the unsnapped scrolled rect. This makes sure that
      // we don't have display items over the subpixel seam at the edge of the
      // scrolled area.
      DisplayListClipState::AutoSaveRestore scrolledRectClipState(aBuilder);
      nsRect scrolledRectClip =
        GetUnsnappedScrolledRectInternal(mScrolledFrame->GetScrollableOverflowRect(),
                                         mScrollPort.Size()) + mScrolledFrame->GetPosition();
      if (mWillBuildScrollableLayer && aBuilder->IsPaintingToWindow()) {
        // Clip the contents to the display port.
        // The dirty rect already acts kind of like a clip, in that
        // FrameLayerBuilder intersects item bounds and opaque regions with
        // it, but it doesn't have the consistent snapping behavior of a
        // true clip.
        // For a case where this makes a difference, imagine the following
        // scenario: The display port has an edge that falls on a fractional
        // layer pixel, and there's an opaque display item that covers the
        // whole display port up until that fractional edge, and there is a
        // transparent display item that overlaps the edge. We want to prevent
        // this transparent item from enlarging the scrolled layer's visible
        // region beyond its opaque region. The dirty rect doesn't do that -
        // it gets rounded out, whereas a true clip gets rounded to nearest
        // pixels.
        // If there is no display port, we don't need this because the clip
        // from the scroll port is still applied.
        scrolledRectClip = scrolledRectClip.Intersect(visibleRect);
      }
      scrolledRectClipState.ClipContainingBlockDescendants(
        scrolledRectClip + aBuilder->ToReferenceFrame(mOuter));

      nsDisplayListBuilder::AutoBuildingDisplayList
        building(aBuilder, mOuter, visibleRect, dirtyRect, aBuilder->IsAtRootOfPseudoStackingContext());

      mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame, scrolledContent);

      if (dirtyRectHasBeenOverriden && gfxPrefs::LayoutDisplayListShowArea()) {
        nsDisplaySolidColor* color =
          new (aBuilder) nsDisplaySolidColor(aBuilder, mOuter,
                                             dirtyRect + aBuilder->GetCurrentFrameOffsetToReferenceFrame(),
                                             NS_RGBA(0, 0, 255, 64), false);
        color->SetOverrideZIndex(INT32_MAX);
        scrolledContent.PositionedDescendants()->AppendToTop(color);
      }
    }

    if (extraContentBoxClipForNonCaretContent) {
      // The items were built while the inflated content box clip was in
      // effect, so that the caret wasn't clipped unnecessarily. We apply
      // the non-inflated clip to the non-caret items now, by intersecting
      // it with their existing clip.
      ClipListsExceptCaret(&scrolledContent, aBuilder, mScrolledFrame,
                           *extraContentBoxClipForNonCaretContent);
    }

    if (aBuilder->IsPaintingToWindow()) {
      mIsScrollParent = idSetter.ShouldForceLayerForScrollParent();
    }
    if (idSetter.ShouldForceLayerForScrollParent() &&
        !gfxPrefs::LayoutUseContainersForRootFrames())
    {
      // Note that forcing layerization of scroll parents follows the scroll
      // handoff chain which is subject to the out-of-flow-frames caveat noted
      // above (where the idSetter variable is created).
      //
      // This is not compatible when using containes for root scrollframes.
      MOZ_ASSERT(couldBuildLayer && mScrolledFrame->GetContent() &&
        aBuilder->IsPaintingToWindow());
      if (!mWillBuildScrollableLayer) {
        // Set a displayport so next paint we don't have to force layerization
        // after the fact.
        nsLayoutUtils::SetDisplayPortMargins(mOuter->GetContent(),
                                             mOuter->PresShell(),
                                             ScreenMargin(),
                                             0,
                                             nsLayoutUtils::RepaintMode::DoNotRepaint);
        // Call DecideScrollableLayer to recompute mWillBuildScrollableLayer and
        // recompute the current animated geometry root if needed.
        // It's too late to change the dirty rect so pass a copy.
        nsRect copyOfDirtyRect = dirtyRect;
        nsRect copyOfVisibleRect = visibleRect;
        Unused << DecideScrollableLayer(aBuilder, &copyOfVisibleRect, &copyOfDirtyRect,
                    /* aSetBase = */ false, nullptr);
        if (mWillBuildScrollableLayer) {
          asrSetter.InsertScrollFrame(sf);
        }
      }
    }
  }

  if (mWillBuildScrollableLayer && aBuilder->IsPaintingToWindow()) {
    aBuilder->ForceLayerForScrollParent();
  }

  if (couldBuildLayer) {
    // Make sure that APZ will dispatch events back to content so we can create
    // a displayport for this frame. We'll add the item later on.
    if (!mWillBuildScrollableLayer) {
      int32_t zIndex =
        MaxZIndexInListOfItemsContainedInFrame(scrolledContent.PositionedDescendants(), mOuter);
      if (aBuilder->BuildCompositorHitTestInfo()) {
        CompositorHitTestInfo info = CompositorHitTestInfo::eVisibleToHitTest
                                   | CompositorHitTestInfo::eDispatchToContent;
        nsDisplayCompositorHitTestInfo* hitInfo =
            new (aBuilder) nsDisplayCompositorHitTestInfo(aBuilder, mScrolledFrame, info, 1,
                Some(mScrollPort + aBuilder->ToReferenceFrame(mOuter)));
        AppendInternalItemToTop(scrolledContent, hitInfo, zIndex);
      }
      if (aBuilder->IsBuildingLayerEventRegions()) {
        nsDisplayLayerEventRegions* inactiveRegionItem =
            new (aBuilder) nsDisplayLayerEventRegions(aBuilder, mScrolledFrame, 1);
        inactiveRegionItem->AddInactiveScrollPort(mScrolledFrame, mScrollPort + aBuilder->ToReferenceFrame(mOuter));
        AppendInternalItemToTop(scrolledContent, inactiveRegionItem, zIndex);
      }
    }

    if (aBuilder->ShouldBuildScrollInfoItemsForHoisting()) {
      aBuilder->AppendNewScrollInfoItemForHoisting(
        new (aBuilder) nsDisplayScrollInfoLayer(aBuilder, mScrolledFrame,
                                                mOuter));
    }
  }
  // Now display overlay scrollbars and the resizer, if we have one.
  AppendScrollPartsTo(aBuilder, scrolledContent, createLayersForScrollbars, true);

  scrolledContent.MoveTo(aLists);
}

bool
ScrollFrameHelper::DecideScrollableLayer(nsDisplayListBuilder* aBuilder,
                                         nsRect* aVisibleRect,
                                         nsRect* aDirtyRect,
                                         bool aSetBase,
                                         bool* aDirtyRectHasBeenOverriden)
{
  // Save and check if this changes so we can recompute the current agr.
  bool oldWillBuildScrollableLayer = mWillBuildScrollableLayer;

  nsIContent* content = mOuter->GetContent();
  bool usingDisplayPort = nsLayoutUtils::HasDisplayPort(content);
  if (aBuilder->IsPaintingToWindow()) {
    if (aSetBase) {
      nsRect displayportBase = *aVisibleRect;
      nsPresContext* pc = mOuter->PresContext();
      if (mIsRoot && (pc->IsRootContentDocument() || !pc->GetParentPresContext())) {
        displayportBase =
          nsRect(nsPoint(0, 0), nsLayoutUtils::CalculateCompositionSizeForFrame(mOuter));
      } else {
        // Make the displayport base equal to the visible rect restricted to
        // the scrollport and the root composition bounds, relative to the
        // scrollport.
        displayportBase = aVisibleRect->Intersect(mScrollPort);

        // Only restrict to the root composition bounds if necessary,
        // as the required coordinate transformation is expensive.
        if (usingDisplayPort) {
          const nsPresContext* rootPresContext =
            pc->GetToplevelContentDocumentPresContext();
          if (!rootPresContext) {
            rootPresContext = pc->GetRootPresContext();
          }
          if (rootPresContext) {
            const nsIPresShell* const rootPresShell = rootPresContext->PresShell();
            nsIFrame* rootFrame = rootPresShell->GetRootScrollFrame();
            if (!rootFrame) {
              rootFrame = rootPresShell->GetRootFrame();
            }
            if (rootFrame) {
              nsRect rootCompBounds =
                nsRect(nsPoint(0, 0), nsLayoutUtils::CalculateCompositionSizeForFrame(rootFrame));

              // If rootFrame is the RCD-RSF then CalculateCompositionSizeForFrame
              // did not take the document's resolution into account, so we must.
              if (rootPresContext->IsRootContentDocument() &&
                  rootFrame == rootPresShell->GetRootScrollFrame()) {
                rootCompBounds = rootCompBounds.RemoveResolution(rootPresShell->GetResolution());
              }

              // We want to convert the root composition bounds from the coordinate
              // space of |rootFrame| to the coordinate space of |mOuter|. We do
              // that with the TransformRect call below. However, since we care
              // about the root composition bounds relative to what the user is
              // actually seeing, we also need to incorporate the APZ callback
              // transforms into this. Most of the time those transforms are
              // negligible, but in some cases (e.g. when a zoom is applied on
              // an overflow:hidden document) it is not (see bug 1280013).
              // XXX: Eventually we may want to create a modified version of
              // TransformRect that includes the APZ callback transforms
              // directly.
              nsLayoutUtils::TransformRect(rootFrame, mOuter, rootCompBounds);
              rootCompBounds += CSSPoint::ToAppUnits(
                  nsLayoutUtils::GetCumulativeApzCallbackTransform(mOuter));

              // We want to limit displayportBase to be no larger than rootCompBounds on
              // either axis, but we don't want to just blindly intersect the two, because
              // rootCompBounds might be offset from where displayportBase is (see bug
              // 1327095 comment 8). Instead, we translate rootCompBounds so as to
              // maximize the overlap with displayportBase, and *then* do the intersection.
              if (rootCompBounds.x > displayportBase.x && rootCompBounds.XMost() > displayportBase.XMost()) {
                // rootCompBounds is at a greater x-position for both left and right, so translate it such
                // that the XMost() values are the same. This will line up the right edge of the two rects,
                // and might mean that rootCompbounds.x is smaller than displayportBase.x. We can avoid that
                // by taking the min of the x delta and XMost() delta, but it doesn't really matter because
                // the intersection between the two rects below will end up the same.
                rootCompBounds.x -= (rootCompBounds.XMost() - displayportBase.XMost());
              } else if (rootCompBounds.x < displayportBase.x && rootCompBounds.XMost() < displayportBase.XMost()) {
                // Analaogous code for when the rootCompBounds is at a smaller x-position.
                rootCompBounds.x = displayportBase.x;
              }
              // Do the same for y-axis
              if (rootCompBounds.y > displayportBase.y && rootCompBounds.YMost() > displayportBase.YMost()) {
                rootCompBounds.y -= (rootCompBounds.YMost() - displayportBase.YMost());
              } else if (rootCompBounds.y < displayportBase.y && rootCompBounds.YMost() < displayportBase.YMost()) {
                rootCompBounds.y = displayportBase.y;
              }

              // Now we can do the intersection
              displayportBase = displayportBase.Intersect(rootCompBounds);
            }
          }
        }

        displayportBase -= mScrollPort.TopLeft();
      }

      nsLayoutUtils::SetDisplayPortBase(mOuter->GetContent(), displayportBase);
    }

    // If we don't have aSetBase == true then should have already
    // been called with aSetBase == true which should have set a
    // displayport base.
    MOZ_ASSERT(content->GetProperty(nsGkAtoms::DisplayPortBase));
    nsRect displayPort;
    usingDisplayPort =
      nsLayoutUtils::GetDisplayPort(content, &displayPort, RelativeTo::ScrollFrame);

    if (usingDisplayPort) {
      // Override the dirty rectangle if the displayport has been set.
      *aVisibleRect = displayPort;
      if (!aBuilder->IsPartialUpdate() || aBuilder->InInvalidSubtree()) {
        *aDirtyRect = displayPort;
        if (aDirtyRectHasBeenOverriden) {
          *aDirtyRectHasBeenOverriden = true;
        }
      } else if (mOuter->HasOverrideDirtyRegion()) {
        nsRect* rect =
          mOuter->GetProperty(nsDisplayListBuilder::DisplayListBuildingDisplayPortRect());
        if (rect) {
          *aDirtyRect = *rect;
          if (aDirtyRectHasBeenOverriden) {
            *aDirtyRectHasBeenOverriden = true;
          }
        }
      }
    } else if (mIsRoot) {
      // The displayPort getter takes care of adjusting for resolution. So if
      // we have resolution but no displayPort then we need to adjust for
      // resolution here.
      nsIPresShell* presShell = mOuter->PresShell();
      *aVisibleRect = aVisibleRect->RemoveResolution(
        presShell->ScaleToResolution() ? presShell->GetResolution () : 1.0f);
      *aDirtyRect = aDirtyRect->RemoveResolution(
        presShell->ScaleToResolution() ? presShell->GetResolution () : 1.0f);
    }
  }

  // Since making new layers is expensive, only create a scrollable layer
  // for some scroll frames.
  // When a displayport is being used, force building of a layer so that
  // the compositor can find the scrollable layer for async scrolling.
  // If the element is marked 'scrollgrab', also force building of a layer
  // so that APZ can implement scroll grabbing.
  mWillBuildScrollableLayer = usingDisplayPort || nsContentUtils::HasScrollgrab(content);

  // The cached animated geometry root for the display builder is out of
  // date if we just introduced a new animated geometry root.
  if (oldWillBuildScrollableLayer != mWillBuildScrollableLayer) {
    aBuilder->RecomputeCurrentAnimatedGeometryRoot();
  }

  if (gfxPrefs::LayoutUseContainersForRootFrames() && mWillBuildScrollableLayer && mIsRoot) {
    mIsScrollableLayerInRootContainer = true;
  }

  return mWillBuildScrollableLayer;
}


Maybe<ScrollMetadata>
ScrollFrameHelper::ComputeScrollMetadata(Layer* aLayer,
                                         LayerManager* aLayerManager,
                                         const nsIFrame* aContainerReferenceFrame,
                                         const ContainerLayerParameters& aParameters,
                                         const DisplayItemClip* aClip) const
{
  if (!mWillBuildScrollableLayer || mIsScrollableLayerInRootContainer) {
    return Nothing();
  }

  nsPoint toReferenceFrame = mOuter->GetOffsetToCrossDoc(aContainerReferenceFrame);

  Maybe<nsRect> parentLayerClip;
  // For containerful frames, the clip is on the container layer.
  if (aClip &&
      (!gfxPrefs::LayoutUseContainersForRootFrames() || mAddClipRectToLayer)) {
    parentLayerClip = Some(aClip->GetClipRect());
  }

  bool isRootContent = mIsRoot && mOuter->PresContext()->IsRootContentDocument();
  bool thisScrollFrameUsesAsyncScrolling = nsLayoutUtils::UsesAsyncScrolling(mOuter);
  if (!thisScrollFrameUsesAsyncScrolling) {
    if (parentLayerClip) {
      // If APZ is not enabled, we still need the displayport to be clipped
      // in the compositor.
      ParentLayerIntRect displayportClip =
        ViewAs<ParentLayerPixel>(
          parentLayerClip->ScaleToNearestPixels(
            aParameters.mXScale,
            aParameters.mYScale,
            mScrolledFrame->PresContext()->AppUnitsPerDevPixel()));

      ParentLayerIntRect layerClip;
      if (const ParentLayerIntRect* origClip = aLayer->GetClipRect().ptrOr(nullptr)) {
        layerClip = displayportClip.Intersect(*origClip);
      } else {
        layerClip = displayportClip;
      }
      aLayer->SetClipRect(Some(layerClip));
    }

    // Return early, since if we don't use APZ we don't need FrameMetrics.
    return Nothing();
  }

  MOZ_ASSERT(mScrolledFrame->GetContent());

  nsRect scrollport = mScrollPort + toReferenceFrame;

  return Some(nsLayoutUtils::ComputeScrollMetadata(
    mScrolledFrame, mOuter, mOuter->GetContent(),
    aContainerReferenceFrame, aLayerManager, mScrollParentID,
    scrollport, parentLayerClip, isRootContent, aParameters));
}

bool
ScrollFrameHelper::IsRectNearlyVisible(const nsRect& aRect) const
{
  // Use the right rect depending on if a display port is set.
  nsRect displayPort;
  bool usingDisplayport =
    nsLayoutUtils::GetDisplayPort(mOuter->GetContent(), &displayPort, RelativeTo::ScrollFrame);
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
    return ScrollbarStyles(disp);
  }

  ScrollbarStyles result = presContext->GetViewportScrollbarStylesOverride();
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
  nsIPresShell* presShell = mOuter->PresShell();
  if (mIsRoot && presShell->IsScrollPositionClampingScrollPortSizeSet()) {
    return presShell->GetScrollPositionClampingScrollPortSize();
  }
  return mScrollPort.Size();
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
                            nsAtom *aOrigin,
                            nsIScrollableFrame::ScrollMomentum aMomentum,
                            nsIScrollbarMediator::ScrollSnapMode aSnap)
{
  // When a smooth scroll is being processed on a frame, mouse wheel and trackpad
  // momentum scroll event updates must notcancel the SMOOTH or SMOOTH_MSD
  // scroll animations, enabling Javascript that depends on them to be responsive
  // without forcing the user to wait for the fling animations to completely stop.
  switch (aMomentum) {
  case nsIScrollableFrame::NOT_MOMENTUM:
    mIgnoreMomentumScroll = false;
    break;
  case nsIScrollableFrame::SYNTHESIZED_MOMENTUM_EVENT:
    if (mIgnoreMomentumScroll) {
      return;
    }
    break;
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
    if (aSnap == nsIScrollableFrame::ENABLE_SNAP) {
      GetSnapPointForDestination(aUnit, mDestination, pos);
    }
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

  nsPoint newPos = mDestination + nsPoint(aDelta.x*deltaMultiplier.width, aDelta.y*deltaMultiplier.height);

  if (aSnap == nsIScrollableFrame::ENABLE_SNAP) {
    ScrollbarStyles styles = GetScrollbarStylesFromFrame();
    if (styles.mScrollSnapTypeY != NS_STYLE_SCROLL_SNAP_TYPE_NONE ||
        styles.mScrollSnapTypeX != NS_STYLE_SCROLL_SNAP_TYPE_NONE) {
      nscoord appUnitsPerDevPixel = mOuter->PresContext()->AppUnitsPerDevPixel();
      deltaMultiplier = nsSize(appUnitsPerDevPixel, appUnitsPerDevPixel);
      negativeTolerance = 0.1f;
      positiveTolerance = 0;
      nsIScrollableFrame::ScrollUnit snapUnit = aUnit;
      if (aOrigin == nsGkAtoms::mouseWheel) {
        // When using a clicky scroll wheel, snap point selection works the same
        // as keyboard up/down/left/right navigation, but with varying amounts
        // of scroll delta.
        snapUnit = nsIScrollableFrame::LINES;
      }
      GetSnapPointForDestination(snapUnit, mDestination, newPos);
    }
  }

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
  AutoWeakFrame weakFrame(mOuter);
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

  if (aUnit == nsIScrollableFrame::DEVICE_PIXELS &&
      !nsLayoutUtils::AsyncPanZoomEnabled(mOuter)) {
    // When APZ is disabled, we must track the velocity
    // on the main thread; otherwise, the APZC will manage this.
    mVelocityQueue.Sample(GetScrollPosition());
  }
}

void
ScrollFrameHelper::ScrollSnap(nsIScrollableFrame::ScrollMode aMode)
{
  float flingSensitivity = gfxPrefs::ScrollSnapPredictionSensitivity();
  int maxVelocity = gfxPrefs::ScrollSnapPredictionMaxVelocity();
  maxVelocity = nsPresContext::CSSPixelsToAppUnits(maxVelocity);
  int maxOffset = maxVelocity * flingSensitivity;
  nsPoint velocity = mVelocityQueue.GetVelocity();
  // Multiply each component individually to avoid integer multiply
  nsPoint predictedOffset = nsPoint(velocity.x * flingSensitivity,
                                    velocity.y * flingSensitivity);
  predictedOffset.Clamp(maxOffset);
  nsPoint pos = GetScrollPosition();
  nsPoint destinationPos = pos + predictedOffset;
  ScrollSnap(destinationPos, aMode);
}

void
ScrollFrameHelper::ScrollSnap(const nsPoint &aDestination,
                              nsIScrollableFrame::ScrollMode aMode)
{
  nsRect scrollRange = GetScrollRangeForClamping();
  nsPoint pos = GetScrollPosition();
  nsPoint snapDestination = scrollRange.ClampPoint(aDestination);
  if (GetSnapPointForDestination(nsIScrollableFrame::DEVICE_PIXELS,
                                                 pos,
                                                 snapDestination)) {
    ScrollTo(snapDestination, aMode);
  }
}

nsSize
ScrollFrameHelper::GetLineScrollAmount() const
{
  RefPtr<nsFontMetrics> fm =
    nsLayoutUtils::GetInflatedFontMetricsForFrame(mOuter);
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
  AutoTArray<TopAndBottom, 50> list;
  nsFrameList fixedFrames = aViewportFrame->GetChildList(nsIFrame::kFixedList);
  for (nsFrameList::Enumerator iterator(fixedFrames); !iterator.AtEnd();
       iterator.Next()) {
    nsIFrame* f = iterator.get();
    nsRect r = f->GetRectRelativeToSelf();
    r = nsLayoutUtils::TransformFrameRectToAncestor(f, r, aViewportFrame);
    r = r.Intersect(aScrollPort);
    if ((r.width >= aScrollPort.width / 2 ||
         r.width >= NSIntPixelsToAppUnits(800, AppUnitsPerCSSPixel())) &&
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
    nsIFrame* root = mOuter->PresShell()->GetRootFrame();
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
      LoadingState state = GetPageLoadingState();
      if (state == LoadingState::Stopped && !NS_SUBTREE_DIRTY(mOuter)) {
        return;
      }
      nsPoint scrollToPos = mRestorePos;
      if (!IsPhysicalLTR()) {
        // convert from logical to physical scroll position
        scrollToPos.x = mScrollPort.x -
          (mScrollPort.XMost() - scrollToPos.x - mScrolledFrame->GetRect().width);
      }
      AutoWeakFrame weakFrame(mOuter);
      // It's very important to pass nsGkAtoms::restore here, so
      // ScrollToWithOrigin won't clear out mRestorePos.
      ScrollToWithOrigin(scrollToPos, nsIScrollableFrame::INSTANT,
                         nsGkAtoms::restore, nullptr);
      if (!weakFrame.IsAlive()) {
        return;
      }
      if (state == LoadingState::Loading || NS_SUBTREE_DIRTY(mOuter)) {
        // If we're trying to do a history scroll restore, then we want to
        // keep trying this until we succeed, because the page can be loading
        // incrementally. So re-get the scroll position for the next iteration,
        // it might not be exactly equal to mRestorePos due to rounding and
        // clamping.
        mLastPos = GetLogicalScrollPosition();
        return;
      }
    }
    // If we get here, either we reached the desired position (mLastPos ==
    // mRestorePos) or we're not trying to do a history scroll restore, so
    // we can stop after the scroll attempt above.
    mRestorePos.y = -1;
    mLastPos.x = -1;
    mLastPos.y = -1;
  } else {
    // user moved the position, so we won't need to restore
    mLastPos.x = -1;
    mLastPos.y = -1;
  }
}

auto
ScrollFrameHelper::GetPageLoadingState() -> LoadingState
{
  bool loadCompleted = false, stopped = false;
  nsCOMPtr<nsIDocShell> ds = mOuter->GetContent()->GetComposedDoc()->GetDocShell();
  if (ds) {
    nsCOMPtr<nsIContentViewer> cv;
    ds->GetContentViewer(getter_AddRefs(cv));
    cv->GetLoadCompleted(&loadCompleted);
    cv->GetIsStopped(&stopped);
  }
  return loadCompleted ? (stopped ? LoadingState::Stopped : LoadingState::Loaded)
                       : LoadingState::Loading;
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
  InternalScrollPortEvent::OrientType orient;
  if (both) {
    orient = InternalScrollPortEvent::eBoth;
    mHorizontalOverflow = newHorizontalOverflow;
    mVerticalOverflow = newVerticalOverflow;
  } else if (vertChanged) {
    orient = InternalScrollPortEvent::eVertical;
    mVerticalOverflow = newVerticalOverflow;
    if (horizChanged) {
      // We need to dispatch a separate horizontal DOM event. Do that the next
      // time around since dispatching the vertical DOM event might destroy
      // the frame.
      PostOverflowEvent();
    }
  } else {
    orient = InternalScrollPortEvent::eHorizontal;
    mHorizontalOverflow = newHorizontalOverflow;
  }

  InternalScrollPortEvent event(true,
    (orient == InternalScrollPortEvent::eHorizontal ? mHorizontalOverflow :
                                                      mVerticalOverflow) ?
    eScrollPortOverflow : eScrollPortUnderflow, nullptr);
  event.mOrient = orient;
  return EventDispatcher::Dispatch(mOuter->GetContent(),
                                   mOuter->PresContext(), &event);
}

void
ScrollFrameHelper::PostScrollEndEvent()
{
  if (mScrollEndEvent) {
    return;
  }

  // The ScrollEndEvent constructor registers itself with the refresh driver.
  mScrollEndEvent = new ScrollEndEvent(this);
}

void
ScrollFrameHelper::FireScrollEndEvent()
{
  MOZ_ASSERT(mOuter->GetContent());
  MOZ_ASSERT(mScrollEndEvent);
  mScrollEndEvent->Revoke();
  mScrollEndEvent = nullptr;

  nsContentUtils::DispatchEventOnlyToChrome(mOuter->GetContent()->OwnerDoc(),
                                            mOuter->GetContent(),
                                            NS_LITERAL_STRING("scrollend"),
                                            true /* aCanBubble */,
                                            false /* aCancelable */);
}

void
ScrollFrameHelper::ReloadChildFrames()
{
  mScrolledFrame = nullptr;
  mHScrollbarBox = nullptr;
  mVScrollbarBox = nullptr;
  mScrollCornerBox = nullptr;
  mResizerBox = nullptr;

  for (nsIFrame* frame : mOuter->PrincipalChildList()) {
    nsIContent* content = frame->GetContent();
    if (content == mOuter->GetContent()) {
      NS_ASSERTION(!mScrolledFrame, "Already found the scrolled frame");
      mScrolledFrame = frame;
    } else {
      nsAutoString value;
      if (content->IsElement()) {
        content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::orient,
                                      value);
      }
      if (!value.IsEmpty()) {
        // probably a scrollbar then
        if (value.LowerCaseEqualsLiteral("horizontal")) {
          NS_ASSERTION(!mHScrollbarBox, "Found multiple horizontal scrollbars?");
          mHScrollbarBox = frame;
        } else {
          NS_ASSERTION(!mVScrollbarBox, "Found multiple vertical scrollbars?");
          mVScrollbarBox = frame;
        }
      } else if (content->IsXULElement(nsGkAtoms::resizer)) {
        NS_ASSERTION(!mResizerBox, "Found multiple resizers");
        mResizerBox = frame;
      } else if (content->IsXULElement(nsGkAtoms::scrollcorner)) {
        // probably a scrollcorner
        NS_ASSERTION(!mScrollCornerBox, "Found multiple scrollcorners");
        mScrollCornerBox = frame;
      }
    }
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

  // Check if the frame is resizable. Note:
  // "The effect of the resize property on generated content is undefined.
  //  Implementations should not apply the resize property to generated
  //  content." [1]
  // For info on what is generated content, see [2].
  // [1]: https://drafts.csswg.org/css-ui/#resize
  // [2]: https://www.w3.org/TR/CSS2/generate.html#content
  int8_t resizeStyle = mOuter->StyleDisplay()->mResize;
  bool isResizable = resizeStyle != NS_STYLE_RESIZE_NONE &&
                     !mOuter->HasAnyStateBits(NS_FRAME_GENERATED_CONTENT);

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
    HTMLTextAreaElement* textAreaElement =
      HTMLTextAreaElement::FromContent(parent->GetContent());
    if (!textAreaElement) {
      mNeverHasVerticalScrollbar = mNeverHasHorizontalScrollbar = true;
      return NS_OK;
    }
  }

  nsNodeInfoManager *nodeInfoManager =
    presContext->Document()->NodeInfoManager();
  RefPtr<NodeInfo> nodeInfo;
  nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollbar, nullptr,
                                          kNameSpaceID_XUL,
                                          nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  if (canHaveHorizontal) {
    RefPtr<NodeInfo> ni = nodeInfo;
    NS_TrustedNewXULElement(getter_AddRefs(mHScrollbarContent), ni.forget());
#ifdef DEBUG
    // Scrollbars can get restyled by theme changes.  Whether such a restyle
    // will actually reconstruct them correctly if it involves a frame
    // reconstruct... I don't know.  :(
    mHScrollbarContent->SetProperty(nsGkAtoms::restylableAnonymousNode,
                                    reinterpret_cast<void*>(true));
#endif // DEBUG

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
    RefPtr<NodeInfo> ni = nodeInfo;
    NS_TrustedNewXULElement(getter_AddRefs(mVScrollbarContent), ni.forget());
#ifdef DEBUG
    // Scrollbars can get restyled by theme changes.  Whether such a restyle
    // will actually reconstruct them correctly if it involves a frame
    // reconstruct... I don't know.  :(
    mVScrollbarContent->SetProperty(nsGkAtoms::restylableAnonymousNode,
                                    reinterpret_cast<void*>(true));
#endif // DEBUG

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
    RefPtr<NodeInfo> nodeInfo;
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
      Element* browserRoot = GetBrowserRoot(mOuter->GetContent());
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
ScrollFrameHelper::Destroy(PostDestroyData& aPostDestroyData)
{
  if (mScrollbarActivity) {
    mScrollbarActivity->Destroy();
    mScrollbarActivity = nullptr;
  }

  // Unbind the content created in CreateAnonymousContent later...
  aPostDestroyData.AddAnonymousContent(mHScrollbarContent.forget());
  aPostDestroyData.AddAnonymousContent(mVScrollbarContent.forget());
  aPostDestroyData.AddAnonymousContent(mScrollCornerContent.forget());
  aPostDestroyData.AddAnonymousContent(mResizerContent.forget());

  if (mPostedReflowCallback) {
    mOuter->PresShell()->CancelReflowCallback(this);
    mPostedReflowCallback = false;
  }

  if (mDisplayPortExpiryTimer) {
    mDisplayPortExpiryTimer->Cancel();
    mDisplayPortExpiryTimer = nullptr;
  }
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

/**
 * Called when we want to update the scrollbar position, either because scrolling happened
 * or the user moved the scrollbar position and we need to undo that (e.g., when the user
 * clicks to scroll and we're using smooth scrolling, so we need to put the thumb back
 * to its initial position for the start of the smooth sequence).
 */
void
ScrollFrameHelper::UpdateScrollbarPosition()
{
  AutoWeakFrame weakFrame(mOuter);
  mFrameIsUpdatingScrollbar = true;

  nsPoint pt = GetScrollPosition();
  if (mVScrollbarBox) {
    SetCoordAttribute(mVScrollbarBox->GetContent()->AsElement(),
                      nsGkAtoms::curpos, pt.y - GetScrolledRect().y);
    if (!weakFrame.IsAlive()) {
      return;
    }
  }
  if (mHScrollbarBox) {
    SetCoordAttribute(mHScrollbarBox->GetContent()->AsElement(), nsGkAtoms::curpos,
                      pt.x - GetScrolledRect().x);
    if (!weakFrame.IsAlive()) {
      return;
    }
  }

  mFrameIsUpdatingScrollbar = false;
}

void ScrollFrameHelper::CurPosAttributeChanged(nsIContent* aContent,
                                               bool aDoScroll)
{
  NS_ASSERTION(aContent, "aContent must not be null");
  NS_ASSERTION((mHScrollbarBox && mHScrollbarBox->GetContent() == aContent) ||
               (mVScrollbarBox && mVScrollbarBox->GetContent() == aContent),
               "unexpected child");
  MOZ_ASSERT(aContent->IsElement());

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
    RefPtr<ScrollbarActivity> scrollbarActivity(mScrollbarActivity);
    scrollbarActivity->ActivityOccurred();
  }

  const bool isSmooth =
    aContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::smooth);
  if (isSmooth) {
    // Make sure an attribute-setting callback occurs even if the view
    // didn't actually move yet.  We need to make sure other listeners
    // see that the scroll position is not (yet) what they thought it
    // was.
    AutoWeakFrame weakFrame(mOuter);
    UpdateScrollbarPosition();
    if (!weakFrame.IsAlive()) {
      return;
    }
  }

  if (aDoScroll) {
    ScrollToWithOrigin(dest,
                       isSmooth ? nsIScrollableFrame::SMOOTH : nsIScrollableFrame::INSTANT,
                       nsGkAtoms::scrollbars, &allowedRange);
  }
  // 'this' might be destroyed here
}

/* ============= Scroll events ========== */

ScrollFrameHelper::ScrollEvent::ScrollEvent(ScrollFrameHelper* aHelper)
  : Runnable("ScrollFrameHelper::ScrollEvent")
  , mHelper(aHelper)
{
  mHelper->mOuter->PresContext()->RefreshDriver()->PostScrollEvent(this);
}

NS_IMETHODIMP
ScrollFrameHelper::ScrollEvent::Run()
{
  if (mHelper) {
    mHelper->FireScrollEvent();
  }
  return NS_OK;
}

ScrollFrameHelper::ScrollEndEvent::ScrollEndEvent(ScrollFrameHelper* aHelper)
  : Runnable("ScrollFrameHelper::ScrollEndEvent")
  , mHelper(aHelper)
{
  mHelper->mOuter->PresContext()->RefreshDriver()->PostScrollEvent(this);
}

NS_IMETHODIMP
ScrollFrameHelper::ScrollEndEvent::Run()
{
  if (mHelper) {
    mHelper->FireScrollEndEvent();
  }
  return NS_OK;
}

void
ScrollFrameHelper::FireScrollEvent()
{
  AUTO_PROFILER_TRACING("Paint", "FireScrollEvent");
  MOZ_ASSERT(mScrollEvent);
  mScrollEvent->Revoke();
  mScrollEvent = nullptr;

  ActiveLayerTracker::SetCurrentScrollHandlerFrame(mOuter);
  WidgetGUIEvent event(true, eScroll, nullptr);
  nsEventStatus status = nsEventStatus_eIgnore;
  nsIContent* content = mOuter->GetContent();
  nsPresContext* prescontext = mOuter->PresContext();
  // Fire viewport scroll events at the document (where they
  // will bubble to the window)
  mozilla::layers::ScrollLinkedEffectDetector detector(content->GetComposedDoc());
  if (mIsRoot) {
    nsIDocument* doc = content->GetUncomposedDoc();
    if (doc) {
      prescontext->SetTelemetryScrollY(GetScrollPosition().y);
      EventDispatcher::Dispatch(doc, prescontext, &event, nullptr,  &status);
    }
  } else {
    // scroll events fired at elements don't bubble (although scroll events
    // fired at documents do, to the window)
    event.mFlags.mBubbles = false;
    EventDispatcher::Dispatch(content, prescontext, &event, nullptr, &status);
  }
  ActiveLayerTracker::SetCurrentScrollHandlerFrame(nullptr);
}

void
ScrollFrameHelper::PostScrollEvent()
{
  if (mScrollEvent) {
    return;
  }

  // The ScrollEvent constructor registers itself with the refresh driver.
  mScrollEvent = new ScrollEvent(this);
}

NS_IMETHODIMP
ScrollFrameHelper::AsyncScrollPortEvent::Run()
{
  if (mHelper) {
    mHelper->mOuter->PresContext()->GetPresShell()->
      FlushPendingNotifications(FlushType::InterruptibleLayout);
  }
  return mHelper ? mHelper->FireScrollPortEvent() : NS_OK;
}

bool
nsXULScrollFrame::AddHorizontalScrollbar(nsBoxLayoutState& aState, bool aOnBottom)
{
  if (!mHelper.mHScrollbarBox) {
    return true;
  }

  return AddRemoveScrollbar(aState, aOnBottom, true, true);
}

bool
nsXULScrollFrame::AddVerticalScrollbar(nsBoxLayoutState& aState, bool aOnRight)
{
  if (!mHelper.mVScrollbarBox) {
    return true;
  }

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

     nsSize hSize = mHelper.mHScrollbarBox->GetXULPrefSize(aState);
     nsBox::AddMargin(mHelper.mHScrollbarBox, hSize);

     mHelper.SetScrollbarVisibility(mHelper.mHScrollbarBox, aAdd);

     // We can't directly pass mHasHorizontalScrollbar as the bool outparam for
     // AddRemoveScrollbar() because it's a bool:1 bitfield. Hence this var:
     bool hasHorizontalScrollbar;
     bool fit = AddRemoveScrollbar(hasHorizontalScrollbar,
                                   mHelper.mScrollPort.y,
                                   mHelper.mScrollPort.height,
                                   hSize.height, aOnRightOrBottom, aAdd);
     mHelper.mHasHorizontalScrollbar = hasHorizontalScrollbar;
     if (!fit) {
       mHelper.SetScrollbarVisibility(mHelper.mHScrollbarBox, !aAdd);
     }
     return fit;
  } else {
     if (mHelper.mNeverHasVerticalScrollbar || !mHelper.mVScrollbarBox)
       return false;

     nsSize vSize = mHelper.mVScrollbarBox->GetXULPrefSize(aState);
     nsBox::AddMargin(mHelper.mVScrollbarBox, vSize);

     mHelper.SetScrollbarVisibility(mHelper.mVScrollbarBox, aAdd);

     // We can't directly pass mHasVerticalScrollbar as the bool outparam for
     // AddRemoveScrollbar() because it's a bool:1 bitfield. Hence this var:
     bool hasVerticalScrollbar;
     bool fit = AddRemoveScrollbar(hasVerticalScrollbar,
                                   mHelper.mScrollPort.x,
                                   mHelper.mScrollPort.width,
                                   vSize.width, aOnRightOrBottom, aAdd);
     mHelper.mHasVerticalScrollbar = hasVerticalScrollbar;
     if (!fit) {
       mHelper.SetScrollbarVisibility(mHelper.mVScrollbarBox, !aAdd);
     }
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

  nsSize minSize = mHelper.mScrolledFrame->GetXULMinSize(aState);

  if (minSize.height > childRect.height)
    childRect.height = minSize.height;

  if (minSize.width > childRect.width)
    childRect.width = minSize.width;

  // TODO: Handle transformed children that inherit perspective
  // from this frame. See AdjustForPerspective for how we handle
  // this for HTML scroll frames.

  aState.SetLayoutFlags(flags);
  ClampAndSetBounds(aState, childRect, aScrollPosition);
  mHelper.mScrolledFrame->XULLayout(aState);

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
  if (mAsyncScrollPortEvent.IsPending()) {
    return;
  }

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
  if (!rpc) {
    return;
  }

  mAsyncScrollPortEvent = new AsyncScrollPortEvent(this);
  rpc->AddWillPaintObserver(mAsyncScrollPortEvent.get());
}

nsIFrame*
ScrollFrameHelper::GetFrameForDir() const
{
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
      if (bodyElement) {
        root = bodyElement; // we can trust the document to hold on to it
      }
    }

    if (root) {
      nsIFrame *rootsFrame = root->GetPrimaryFrame();
      if (rootsFrame) {
        frame = rootsFrame;
      }
    }
  }

  return frame;
}

bool
ScrollFrameHelper::IsScrollbarOnRight() const
{
  nsPresContext *presContext = mOuter->PresContext();

  // The position of the scrollbar in top-level windows depends on the pref
  // layout.scrollbar.side. For non-top-level elements, it depends only on the
  // directionaliy of the element (equivalent to a value of "1" for the pref).
  if (!mIsRoot) {
    return IsPhysicalLTR();
  }
  switch (presContext->GetCachedIntPref(kPresContext_ScrollbarSide)) {
    default:
    case 0: // UI directionality
      return presContext->GetCachedIntPref(kPresContext_BidiDirection)
             == IBMBIDI_TEXTDIRECTION_LTR;
    case 1: // Document / content directionality
      return IsPhysicalLTR();
    case 2: // Always right
      return true;
    case 3: // Always left
      return false;
  }
}

bool
ScrollFrameHelper::IsMaybeScrollingActive() const
{
  const nsStyleDisplay* disp = mOuter->StyleDisplay();
  if (disp && (disp->mWillChangeBitField & NS_STYLE_WILL_CHANGE_SCROLL)) {
    return true;
  }

  nsIContent* content = mOuter->GetContent();
  return mHasBeenScrolledRecently ||
         IsAlwaysActive() ||
         nsLayoutUtils::HasDisplayPort(content) ||
         nsContentUtils::HasScrollgrab(content);
}

bool
ScrollFrameHelper::IsScrollingActive(nsDisplayListBuilder* aBuilder) const
{
  const nsStyleDisplay* disp = mOuter->StyleDisplay();
  if (disp && (disp->mWillChangeBitField & NS_STYLE_WILL_CHANGE_SCROLL) &&
    aBuilder->IsInWillChangeBudget(mOuter, GetScrollPositionClampingScrollPortSize())) {
    return true;
  }

  nsIContent* content = mOuter->GetContent();
  return mHasBeenScrolledRecently ||
         IsAlwaysActive() ||
         nsLayoutUtils::HasDisplayPort(content) ||
         nsContentUtils::HasScrollgrab(content);
}

/**
 * Reflow the scroll area if it needs it and return its size. Also determine if the reflow will
 * cause any of the scrollbars to need to be reflowed.
 */
nsresult
nsXULScrollFrame::XULLayout(nsBoxLayoutState& aState)
{
  bool scrollbarRight = IsScrollbarOnRight();
  bool scrollbarBottom = true;

  // get the content rect
  nsRect clientRect(0,0,0,0);
  GetXULClientRect(clientRect);

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
    if (scrolledRect.height <= mHelper.mScrollPort.height ||
        styles.mVertical != NS_STYLE_OVERFLOW_AUTO) {
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
        if (AddVerticalScrollbar(aState, scrollbarRight)) {
          needsLayout = true;
        }
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
        if (AddHorizontalScrollbar(aState, scrollbarBottom)) {

          // if we added a horizontal scrollbar and we did not have a vertical
          // there is a chance that by adding the horizontal scrollbar we will
          // suddenly need a vertical scrollbar. Is a special case but it's
          // important.
          //
          // But before we do that we need to relayout, since it's
          // possible that the contents will flex as a result of adding a
          // horizontal scrollbar and avoid the need for a vertical
          // scrollbar.
          //
          // So instead of setting needsLayout to true here, do the
          // layout immediately, and then consider whether to add the
          // vertical scrollbar (and then maybe layout again).
          {
            nsBoxLayoutState resizeState(aState);
            LayoutScrollArea(resizeState, oldScrollPosition);
            needsLayout = false;
          }

          // Refresh scrolledRect because we called LayoutScrollArea.
          scrolledRect = mHelper.GetScrolledRect();

          if (styles.mVertical == NS_STYLE_OVERFLOW_AUTO &&
              !mHelper.mHasVerticalScrollbar &&
              scrolledRect.height > mHelper.mScrollPort.height) {
            if (AddVerticalScrollbar(aState, scrollbarRight)) {
              needsLayout = true;
            }
          }
        }

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

  if (!mHelper.mSuppressScrollbarUpdate) {
    mHelper.LayoutScrollbars(aState, clientRect, oldScrollAreaBounds);
  }
  if (!mHelper.mPostedReflowCallback) {
    // Make sure we'll try scrolling to restored position
    PresShell()->PostReflowCallback(&mHelper);
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

  mHelper.UpdatePrevScrolledRect();

  mHelper.PostOverflowEvent();
  return NS_OK;
}

void
ScrollFrameHelper::FinishReflowForScrollbar(Element* aElement,
                                            nscoord aMinXY, nscoord aMaxXY,
                                            nscoord aCurPosXY,
                                            nscoord aPageIncrement,
                                            nscoord aIncrement)
{
  // Scrollbars assume zero is the minimum position, so translate for them.
  SetCoordAttribute(aElement, nsGkAtoms::curpos, aCurPosXY - aMinXY);
  SetScrollbarEnabled(aElement, aMaxXY - aMinXY);
  SetCoordAttribute(aElement, nsGkAtoms::maxpos, aMaxXY - aMinXY);
  SetCoordAttribute(aElement, nsGkAtoms::pageincrement, aPageIncrement);
  SetCoordAttribute(aElement, nsGkAtoms::increment, aIncrement);
}

bool
ScrollFrameHelper::ReflowFinished()
{
  mPostedReflowCallback = false;

  bool doScroll = true;
  if (NS_SUBTREE_DIRTY(mOuter)) {
    // We will get another call after the next reflow and scrolling
    // later is less janky.
    doScroll = false;
  }

  nsAutoScriptBlocker scriptBlocker;

  if (doScroll) {
    ScrollToRestoredPosition();

    // Clamp current scroll position to new bounds. Normally this won't
    // do anything.
    nsPoint currentScrollPos = GetScrollPosition();
    ScrollToImpl(currentScrollPos, nsRect(currentScrollPos, nsSize(0, 0)));
    if (!mAsyncScroll && !mAsyncSmoothMSDScroll && !mApzSmoothScrollDestination) {
      // We need to have mDestination track the current scroll position,
      // in case it falls outside the new reflow area. mDestination is used
      // by ScrollBy as its starting position.
      mDestination = GetScrollPosition();
    }
  }

  if (!mUpdateScrollbarAttributes) {
    return false;
  }
  mUpdateScrollbarAttributes = false;

  // Update scrollbar attributes.
  nsPresContext* presContext = mOuter->PresContext();

  if (mMayHaveDirtyFixedChildren) {
    mMayHaveDirtyFixedChildren = false;
    nsIFrame* parentFrame = mOuter->GetParent();
    for (nsIFrame* fixedChild =
           parentFrame->GetChildList(nsIFrame::kFixedList).FirstChild();
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

  // FIXME(emilio): Why this instead of mHScrollbarContent / mVScrollbarContent?
  RefPtr<Element> vScroll =
    mVScrollbarBox ? mVScrollbarBox->GetContent()->AsElement() : nullptr;
  RefPtr<Element> hScroll =
    mHScrollbarBox ? mHScrollbarBox->GetContent()->AsElement() : nullptr;

  // Note, in some cases mOuter may get deleted while finishing reflow
  // for scrollbars. XXXmats is this still true now that we have a script
  // blocker in this scope? (if not, remove the weak frame checks below).
  if (vScroll || hScroll) {
    AutoWeakFrame weakFrame(mOuter);
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
  CurPosAttributeChanged(mVScrollbarBox ? mVScrollbarBox->GetContent()->AsElement()
                                        : mHScrollbarBox->GetContent()->AsElement(),
                         doScroll);
  return doScroll;
}

void
ScrollFrameHelper::ReflowCallbackCanceled()
{
  mPostedReflowCallback = false;
}

bool
ScrollFrameHelper::ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas)
{
  nsIScrollableFrame* sf = do_QueryFrame(mOuter);
  ScrollbarStyles ss = sf->GetScrollbarStyles();

  // Reflow when the change in overflow leads to one of our scrollbars
  // changing or might require repositioning the scrolled content due to
  // reduced extents.
  nsRect scrolledRect = GetScrolledRect();
  uint32_t overflowChange = GetOverflowChange(scrolledRect, mPrevScrolledRect);
  mPrevScrolledRect = scrolledRect;

  bool needReflow = false;
  nsPoint scrollPosition = GetScrollPosition();
  if (overflowChange & nsIScrollableFrame::HORIZONTAL) {
    if (ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN || scrollPosition.x) {
      needReflow = true;
    }
  }
  if (overflowChange & nsIScrollableFrame::VERTICAL) {
    if (ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN || scrollPosition.y) {
      needReflow = true;
    }
  }

  if (needReflow) {
    // If there are scrollbars, or we're not at the beginning of the pane,
    // the scroll position may change. In this case, mark the frame as
    // needing reflow. Don't use NS_FRAME_IS_DIRTY as dirty as that means
    // we have to reflow the frame and all its descendants, and we don't
    // have to do that here. Only this frame needs to be reflowed.
    mOuter->PresShell()->FrameNeedsReflow(
      mOuter, nsIPresShell::eResize, NS_FRAME_HAS_DIRTY_CHILDREN);
    // Ensure that next time nsHTMLScrollFrame::Reflow runs, we don't skip
    // updating the scrollbars. (Because the overflow area of the scrolled
    // frame has probably just been updated, Reflow won't see it change.)
    mSkippedScrollbarLayout = true;
    return false;  // reflowing will update overflow
  }
  PostOverflowEvent();
  return mOuter->nsContainerFrame::ComputeCustomOverflow(aOverflowAreas);
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
ScrollFrameHelper::UpdatePrevScrolledRect()
{
  mPrevScrolledRect = GetScrolledRect();
}

void
ScrollFrameHelper::AdjustScrollbarRectForResizer(
                         nsIFrame* aFrame, nsPresContext* aPresContext,
                         nsRect& aRect, bool aHasResizer, bool aVertical)
{
  if ((aVertical ? aRect.width : aRect.height) == 0) {
    return;
  }

  // if a content resizer is present, use its size. Otherwise, check if the
  // widget has a resizer.
  nsRect resizerRect;
  if (aHasResizer) {
    resizerRect = mResizerBox->GetRect();
  }
  else {
    nsPoint offset;
    nsIWidget* widget = aFrame->GetNearestWidget(offset);
    LayoutDeviceIntRect widgetRect;
    if (!widget || !widget->ShowsResizeIndicator(&widgetRect)) {
      return;
    }

    resizerRect = nsRect(aPresContext->DevPixelsToAppUnits(widgetRect.x) - offset.x,
                         aPresContext->DevPixelsToAppUnits(widgetRect.y) - offset.y,
                         aPresContext->DevPixelsToAppUnits(widgetRect.width),
                         aPresContext->DevPixelsToAppUnits(widgetRect.height));
  }

  if (resizerRect.Contains(aRect.BottomRight() - nsPoint(1, 1))) {
    if (aVertical) {
      aRect.height = std::max(0, resizerRect.y - aRect.y);
    } else {
      aRect.width = std::max(0, resizerRect.x - aRect.x);
    }
  } else if (resizerRect.Contains(aRect.BottomLeft() + nsPoint(1, -1))) {
    if (aVertical) {
      aRect.height = std::max(0, resizerRect.y - aRect.y);
    } else {
      nscoord xmost = aRect.XMost();
      aRect.x = std::max(aRect.x, resizerRect.XMost());
      aRect.width = xmost - aRect.x;
    }
  }
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
  NS_ASSERTION(!mSuppressScrollbarUpdate,
               "This should have been suppressed");

  nsIPresShell* presShell = mOuter->PresShell();

  bool hasResizer = HasResizer();
  bool scrollbarOnLeft = !IsScrollbarOnRight();
  bool overlayScrollBarsWithZoom =
    mIsRoot && LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars) &&
    presShell->IsScrollPositionClampingScrollPortSizeSet();

  nsSize scrollPortClampingSize = mScrollPort.Size();
  double res = 1.0;
  if (overlayScrollBarsWithZoom) {
    scrollPortClampingSize = presShell->GetScrollPositionClampingScrollPortSize();
    res = presShell->GetCumulativeResolution();
  }

  // place the scrollcorner
  if (mScrollCornerBox || mResizerBox) {
    NS_PRECONDITION(!mScrollCornerBox || mScrollCornerBox->IsXULBoxFrame(), "Must be a box frame!");

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
      nsSize resizerMinSize = mResizerBox->GetXULMinSize(aState);

      nscoord vScrollbarWidth = mVScrollbarBox ?
        mVScrollbarBox->GetXULPrefSize(aState).width : defaultSize;
      r.width = std::max(std::max(r.width, vScrollbarWidth), resizerMinSize.width);
      if (aContentArea.x == mScrollPort.x && !scrollbarOnLeft) {
        r.x = aContentArea.XMost() - r.width;
      }

      nscoord hScrollbarHeight = mHScrollbarBox ?
        mHScrollbarBox->GetXULPrefSize(aState).height : defaultSize;
      r.height = std::max(std::max(r.height, hScrollbarHeight), resizerMinSize.height);
      if (aContentArea.y == mScrollPort.y) {
        r.y = aContentArea.YMost() - r.height;
      }

      nsBoxFrame::LayoutChildAt(aState, mResizerBox, r);
    } else if (mResizerBox) {
      // otherwise lay out the resizer with an empty rectangle
      nsBoxFrame::LayoutChildAt(aState, mResizerBox, nsRect());
    }
  }

  nsPresContext* presContext = mScrolledFrame->PresContext();
  nsRect vRect;
  if (mVScrollbarBox) {
    NS_PRECONDITION(mVScrollbarBox->IsXULBoxFrame(), "Must be a box frame!");
    vRect = mScrollPort;
    if (overlayScrollBarsWithZoom) {
      vRect.height = NSToCoordRound(res * scrollPortClampingSize.height);
    }
    vRect.width = aContentArea.width - mScrollPort.width;
    vRect.x = scrollbarOnLeft ? aContentArea.x : mScrollPort.x + NSToCoordRound(res * scrollPortClampingSize.width);
    if (mHasVerticalScrollbar) {
      nsMargin margin;
      mVScrollbarBox->GetXULMargin(margin);
      vRect.Deflate(margin);
    }
    AdjustScrollbarRectForResizer(mOuter, presContext, vRect, hasResizer, true);
  }

  nsRect hRect;
  if (mHScrollbarBox) {
    NS_PRECONDITION(mHScrollbarBox->IsXULBoxFrame(), "Must be a box frame!");
    hRect = mScrollPort;
    if (overlayScrollBarsWithZoom) {
      hRect.width = NSToCoordRound(res * scrollPortClampingSize.width);
    }
    hRect.height = aContentArea.height - mScrollPort.height;
    hRect.y = mScrollPort.y + NSToCoordRound(res * scrollPortClampingSize.height);
    if (mHasHorizontalScrollbar) {
      nsMargin margin;
      mHScrollbarBox->GetXULMargin(margin);
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
    aState.PresShell()->PostReflowCallback(this);
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
ScrollFrameHelper::SetScrollbarEnabled(Element* aElement, nscoord aMaxPos)
{
  DebugOnly<nsWeakPtr> weakShell(
    do_GetWeakReference(mOuter->PresShell()));
  if (aMaxPos) {
    aElement->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
  } else {
    aElement->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled,
                      NS_LITERAL_STRING("true"), true);
  }
  MOZ_ASSERT(ShellIsAlive(weakShell), "pres shell was destroyed by scrolling");
}

void
ScrollFrameHelper::SetCoordAttribute(Element* aElement, nsAtom* aAtom,
                                     nscoord aSize)
{
  DebugOnly<nsWeakPtr> weakShell(
    do_GetWeakReference(mOuter->PresShell()));
  // convert to pixels
  int32_t pixelSize = nsPresContext::AppUnitsToIntCSSPixels(aSize);

  // only set the attribute if it changed.

  nsAutoString newValue;
  newValue.AppendInt(pixelSize);

  if (aElement->AttrValueIs(kNameSpaceID_None, aAtom, newValue, eCaseMatters)) {
    return;
  }

  AutoWeakFrame weakFrame(mOuter);
  RefPtr<Element> kungFuDeathGrip = aElement;
  aElement->SetAttr(kNameSpaceID_None, aAtom, newValue, true);
  MOZ_ASSERT(ShellIsAlive(weakShell), "pres shell was destroyed by scrolling");
  if (!weakFrame.IsAlive()) {
    return;
  }

  if (mScrollbarActivity) {
    RefPtr<ScrollbarActivity> scrollbarActivity(mScrollbarActivity);
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
                aRadii[eCornerTopLeftX],
                aRadii[eCornerTopLeftY]);
  }

  if (sb.top > 0 || sb.right > 0) {
    ReduceRadii(border.right, border.top,
                aRadii[eCornerTopRightX],
                aRadii[eCornerTopRightY]);
  }

  if (sb.right > 0 || sb.bottom > 0) {
    ReduceRadii(border.right, border.bottom,
                aRadii[eCornerBottomRightX],
                aRadii[eCornerBottomRightY]);
  }

  if (sb.bottom > 0 || sb.left > 0) {
    ReduceRadii(border.left, border.bottom,
                aRadii[eCornerBottomLeftX],
                aRadii[eCornerBottomLeftY]);
  }

  return true;
}

static nscoord
SnapCoord(nscoord aCoord, double aRes, nscoord aAppUnitsPerPixel)
{
  double snappedToLayerPixels = NS_round((aRes*aCoord)/aAppUnitsPerPixel);
  return NSToCoordRoundWithClamp(snappedToLayerPixels*aAppUnitsPerPixel/aRes);
}

nsRect
ScrollFrameHelper::GetScrolledRect() const
{
  nsRect result =
    GetUnsnappedScrolledRectInternal(mScrolledFrame->GetScrollableOverflowRect(),
                                     mScrollPort.Size());

  if (result.width < mScrollPort.width) {
    NS_WARNING("Scrolled rect smaller than scrollport?");
  }
  if (result.height < mScrollPort.height) {
    NS_WARNING("Scrolled rect smaller than scrollport?");
  }

  // Expand / contract the result by up to half a layer pixel so that scrolling
  // to the right / bottom edge does not change the layer pixel alignment of
  // the scrolled contents.

  if (result.x == 0 && result.y == 0 &&
      result.width == mScrollPort.width &&
      result.height == mScrollPort.height) {
    // The edges that we would snap are already aligned with the scroll port,
    // so we can skip all the work below.
    return result;
  }

  // For that, we first convert the scroll port and the scrolled rect to rects
  // relative to the reference frame, since that's the space where painting does
  // snapping.
  nsSize scrollPortSize = GetScrollPositionClampingScrollPortSize();
  const nsIFrame* referenceFrame =
    mReferenceFrameDuringPainting ? mReferenceFrameDuringPainting : nsLayoutUtils::GetReferenceFrame(mOuter);
  nsPoint toReferenceFrame = mOuter->GetOffsetToCrossDoc(referenceFrame);
  nsRect scrollPort(mScrollPort.TopLeft() + toReferenceFrame, scrollPortSize);
  nsRect scrolledRect = result + scrollPort.TopLeft();

  if (scrollPort.Overflows() || scrolledRect.Overflows()) {
    return result;
  }

  // Now, snap the bottom right corner of both of these rects.
  // We snap to layer pixels, so we need to respect the layer's scale.
  nscoord appUnitsPerDevPixel = mScrolledFrame->PresContext()->AppUnitsPerDevPixel();
  gfxSize scale = FrameLayerBuilder::GetPaintedLayerScaleForFrame(mScrolledFrame);
  if (scale.IsEmpty()) {
    scale = gfxSize(1.0f, 1.0f);
  }

  // Compute bounds for the scroll position, and computed the snapped scrolled
  // rect from the scroll position bounds.
  nscoord snappedScrolledAreaBottom = SnapCoord(scrolledRect.YMost(), scale.height, appUnitsPerDevPixel);
  nscoord snappedScrollPortBottom = SnapCoord(scrollPort.YMost(), scale.height, appUnitsPerDevPixel);
  nscoord maximumScrollOffsetY = snappedScrolledAreaBottom - snappedScrollPortBottom;
  result.SetBottomEdge(scrollPort.height + maximumScrollOffsetY);

  if (GetScrolledFrameDir() == NS_STYLE_DIRECTION_LTR) {
    nscoord snappedScrolledAreaRight = SnapCoord(scrolledRect.XMost(), scale.width, appUnitsPerDevPixel);
    nscoord snappedScrollPortRight = SnapCoord(scrollPort.XMost(), scale.width, appUnitsPerDevPixel);
    nscoord maximumScrollOffsetX = snappedScrolledAreaRight - snappedScrollPortRight;
    result.SetRightEdge(scrollPort.width + maximumScrollOffsetX);
  } else {
    // In RTL, the scrolled area's right edge is at scrollPort.XMost(),
    // and the scrolled area's x position is zero or negative. We want
    // the right edge to stay flush with the scroll port, so we snap the
    // left edge.
    nscoord snappedScrolledAreaLeft = SnapCoord(scrolledRect.x, scale.width, appUnitsPerDevPixel);
    nscoord snappedScrollPortLeft = SnapCoord(scrollPort.x, scale.width, appUnitsPerDevPixel);
    nscoord minimumScrollOffsetX = snappedScrolledAreaLeft - snappedScrollPortLeft;
    result.SetLeftEdge(minimumScrollOffsetX);
  }

  return result;
}


uint8_t
ScrollFrameHelper::GetScrolledFrameDir() const
{
  // If the scrolled frame has unicode-bidi: plaintext, the paragraph
  // direction set by the text content overrides the direction of the frame
  if (mScrolledFrame->StyleTextReset()->mUnicodeBidi &
      NS_STYLE_UNICODE_BIDI_PLAINTEXT) {
    nsIFrame* childFrame = mScrolledFrame->PrincipalChildList().FirstChild();
    if (childFrame) {
      return (nsBidiPresUtils::ParagraphDirection(childFrame) == NSBIDI_LTR)
             ? NS_STYLE_DIRECTION_LTR : NS_STYLE_DIRECTION_RTL;
    }
  }

  return IsBidiLTR() ? NS_STYLE_DIRECTION_LTR : NS_STYLE_DIRECTION_RTL;
}

nsRect
ScrollFrameHelper::GetUnsnappedScrolledRectInternal(const nsRect& aScrolledFrameOverflowArea,
                                                    const nsSize& aScrollPortSize) const
{
  return nsLayoutUtils::GetScrolledRect(mScrolledFrame,
                                        aScrolledFrameOverflowArea,
                                        aScrollPortSize, GetScrolledFrameDir());
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
ScrollFrameHelper::GetCoordAttribute(nsIFrame* aBox, nsAtom* aAtom,
                                         nscoord aDefaultValue,
                                         nscoord* aRangeStart,
                                         nscoord* aRangeLength)
{
  if (aBox) {
    nsIContent* content = aBox->GetContent();

    nsAutoString value;
    if (content->IsElement()) {
      content->AsElement()->GetAttr(kNameSpaceID_None, aAtom, value);
    }
    if (!value.IsEmpty()) {
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
  // a previous scroll state, and we're not in the middle of a smooth scroll.
  bool isInSmoothScroll = IsProcessingAsyncScroll() || mLastSmoothScrollOrigin;
  if (!mHasBeenScrolled && !mDidHistoryRestore && !isInSmoothScroll) {
    return nullptr;
  }

  nsPresState* state = new nsPresState();
  bool allowScrollOriginDowngrade =
    !nsLayoutUtils::CanScrollOriginClobberApz(mLastScrollOrigin) ||
    mAllowScrollOriginDowngrade;
  // Save mRestorePos instead of our actual current scroll position, if it's
  // valid and we haven't moved since the last update of mLastPos (same check
  // that ScrollToRestoredPosition uses). This ensures if a reframe occurs
  // while we're in the process of loading content to scroll to a restored
  // position, we'll keep trying after the reframe. Similarly, if we're in the
  // middle of a smooth scroll, store the destination so that when we restore
  // we'll jump straight to the end of the scroll animation, rather than
  // effectively dropping it. Note that the mRestorePos will override the
  // smooth scroll destination if both are present.
  nsPoint pt = GetLogicalScrollPosition();
  if (isInSmoothScroll) {
    pt = mDestination;
    allowScrollOriginDowngrade = false;
  }
  if (mRestorePos.y != -1 && pt == mLastPos) {
    pt = mRestorePos;
  }
  state->SetScrollState(pt);
  state->SetAllowScrollOriginDowngrade(allowScrollOriginDowngrade);
  if (mIsRoot) {
    // Only save resolution properties for root scroll frames
    nsIPresShell* shell = mOuter->PresShell();
    state->SetResolution(shell->GetResolution());
    state->SetScaleToResolution(shell->ScaleToResolution());
  }
  return state;
}

void
ScrollFrameHelper::RestoreState(nsPresState* aState)
{
  mRestorePos = aState->GetScrollPosition();
  MOZ_ASSERT(mLastScrollOrigin == nsGkAtoms::other);
  mAllowScrollOriginDowngrade = aState->GetAllowScrollOriginDowngrade();
  mDidHistoryRestore = true;
  mLastPos = mScrolledFrame ? GetLogicalScrollPosition() : nsPoint(0,0);

  // Resolution properties should only exist on root scroll frames.
  MOZ_ASSERT(mIsRoot || (!aState->GetScaleToResolution() &&
                         aState->GetResolution() == 1.0));

  if (mIsRoot) {
    nsIPresShell* presShell = mOuter->PresShell();
    if (aState->GetScaleToResolution()) {
      presShell->SetResolutionAndScaleTo(aState->GetResolution());
    } else {
      presShell->SetResolution(aState->GetResolution());
    }
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

  InternalScrollAreaEvent event(true, eScrolledAreaChanged, nullptr);
  nsPresContext *prescontext = mOuter->PresContext();
  nsIContent* content = mOuter->GetContent();

  event.mArea = mScrolledFrame->GetScrollableOverflowRectRelativeToParent();

  nsIDocument *doc = content->GetUncomposedDoc();
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

/**
 * Collect the scroll-snap-coordinates of frames in the subtree rooted at
 * |aFrame|, relative to |aScrolledFrame|, into |aOutCoords|.
 */
void
CollectScrollSnapCoordinates(nsIFrame* aFrame, nsIFrame* aScrolledFrame,
                             nsTArray<nsPoint>& aOutCoords)
{
  nsIFrame::ChildListIterator childLists(aFrame);
  for (; !childLists.IsDone(); childLists.Next()) {
    nsFrameList::Enumerator childFrames(childLists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* f = childFrames.get();

      const nsStyleDisplay* styleDisplay = f->StyleDisplay();
      size_t coordCount = styleDisplay->mScrollSnapCoordinate.Length();

      if (coordCount) {
        nsRect frameRect = f->GetRect();
        nsPoint offset = f->GetOffsetTo(aScrolledFrame);
        nsRect edgesRect = nsRect(offset, frameRect.Size());
        for (size_t coordNum = 0; coordNum < coordCount; coordNum++) {
          const Position& coordPosition =
            f->StyleDisplay()->mScrollSnapCoordinate[coordNum];
          nsPoint coordPoint = edgesRect.TopLeft();
          coordPoint += nsPoint(coordPosition.mXPosition.mLength,
                                coordPosition.mYPosition.mLength);
          if (coordPosition.mXPosition.mHasPercent) {
            coordPoint.x += NSToCoordRound(coordPosition.mXPosition.mPercent *
                                           frameRect.width);
          }
          if (coordPosition.mYPosition.mHasPercent) {
            coordPoint.y += NSToCoordRound(coordPosition.mYPosition.mPercent *
                                           frameRect.height);
          }

          aOutCoords.AppendElement(coordPoint);
        }
      }

      CollectScrollSnapCoordinates(f, aScrolledFrame, aOutCoords);
    }
  }
}

layers::ScrollSnapInfo
ComputeScrollSnapInfo(const ScrollFrameHelper& aScrollFrame)
{
  ScrollSnapInfo result;

  ScrollbarStyles styles = aScrollFrame.GetScrollbarStylesFromFrame();

  if (styles.mScrollSnapTypeY == NS_STYLE_SCROLL_SNAP_TYPE_NONE &&
      styles.mScrollSnapTypeX == NS_STYLE_SCROLL_SNAP_TYPE_NONE) {
    // We won't be snapping, short-circuit the computation.
    return result;
  }

  result.mScrollSnapTypeX = styles.mScrollSnapTypeX;
  result.mScrollSnapTypeY = styles.mScrollSnapTypeY;

  nsSize scrollPortSize = aScrollFrame.GetScrollPortRect().Size();

  result.mScrollSnapDestination = nsPoint(styles.mScrollSnapDestinationX.mLength,
                                          styles.mScrollSnapDestinationY.mLength);
  if (styles.mScrollSnapDestinationX.mHasPercent) {
    result.mScrollSnapDestination.x +=
        NSToCoordFloorClamped(styles.mScrollSnapDestinationX.mPercent *
                              scrollPortSize.width);
  }
  if (styles.mScrollSnapDestinationY.mHasPercent) {
    result.mScrollSnapDestination.y +=
        NSToCoordFloorClamped(styles.mScrollSnapDestinationY.mPercent *
                              scrollPortSize.height);
  }

  if (styles.mScrollSnapPointsX.GetUnit() != eStyleUnit_None) {
    result.mScrollSnapIntervalX = Some(
      styles.mScrollSnapPointsX.ComputeCoordPercentCalc(scrollPortSize.width));
  }
  if (styles.mScrollSnapPointsY.GetUnit() != eStyleUnit_None) {
    result.mScrollSnapIntervalY = Some(
      styles.mScrollSnapPointsY.ComputeCoordPercentCalc(scrollPortSize.height));
  }

  CollectScrollSnapCoordinates(aScrollFrame.GetScrolledFrame(),
                               aScrollFrame.GetScrolledFrame(),
                               result.mScrollSnapCoordinates);

  return result;
}

layers::ScrollSnapInfo
ScrollFrameHelper::GetScrollSnapInfo() const
{
  // TODO(botond): Should we cache it?
  return ComputeScrollSnapInfo(*this);
}

bool
ScrollFrameHelper::GetSnapPointForDestination(nsIScrollableFrame::ScrollUnit aUnit,
                                              nsPoint aStartPos,
                                              nsPoint &aDestination)
{
  Maybe<nsPoint> snapPoint = ScrollSnapUtils::GetSnapPointForDestination(
      GetScrollSnapInfo(), aUnit, mScrollPort.Size(),
      GetScrollRangeForClamping(), aStartPos, aDestination);
  if (snapPoint) {
    aDestination = snapPoint.ref();
    return true;
  }
  return false;
}

bool
ScrollFrameHelper::UsesContainerScrolling() const
{
  if (gfxPrefs::LayoutUseContainersForRootFrames()) {
    return mIsRoot;
  }
  return false;
}

bool
ScrollFrameHelper::DragScroll(WidgetEvent* aEvent)
{
  // Dragging is allowed while within a 20 pixel border. Note that device pixels
  // are used so that the same margin is used even when zoomed in or out.
  nscoord margin = 20 * mOuter->PresContext()->AppUnitsPerDevPixel();

  // Don't drag scroll for small scrollareas.
  if (mScrollPort.width < margin * 2 || mScrollPort.height < margin * 2) {
    return false;
  }

  // If willScroll is computed as false, then the frame is already scrolled as
  // far as it can go in both directions. Return false so that an ancestor
  // scrollframe can scroll instead.
  bool willScroll = false;
  nsPoint pnt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, mOuter);
  nsPoint scrollPoint = GetScrollPosition();
  nsRect rangeRect = GetScrollRangeForClamping();

  // Only drag scroll when a scrollbar is present.
  nsPoint offset;
  if (mHasHorizontalScrollbar) {
    if (pnt.x >= mScrollPort.x && pnt.x <= mScrollPort.x + margin) {
      offset.x = -margin;
      if (scrollPoint.x > 0) {
        willScroll = true;
      }
    } else if (pnt.x >= mScrollPort.XMost() - margin && pnt.x <= mScrollPort.XMost()) {
      offset.x = margin;
      if (scrollPoint.x < rangeRect.width) {
        willScroll = true;
      }
    }
  }

  if (mHasVerticalScrollbar) {
    if (pnt.y >= mScrollPort.y && pnt.y <= mScrollPort.y + margin) {
      offset.y = -margin;
      if (scrollPoint.y > 0) {
        willScroll = true;
      }
    } else if (pnt.y >= mScrollPort.YMost() - margin && pnt.y <= mScrollPort.YMost()) {
      offset.y = margin;
      if (scrollPoint.y < rangeRect.height) {
        willScroll = true;
      }
    }
  }

  if (offset.x || offset.y) {
    ScrollTo(GetScrollPosition() + offset, nsIScrollableFrame::NORMAL);
  }

  return willScroll;
}

static void
AsyncScrollbarDragRejected(nsIFrame* aScrollbar)
{
  if (!aScrollbar) {
    return;
  }

  for (nsIFrame::ChildListIterator childLists(aScrollbar);
       !childLists.IsDone();
       childLists.Next()) {
    for (nsIFrame* frame : childLists.CurrentList()) {
      if (nsSliderFrame* sliderFrame = do_QueryFrame(frame)) {
        sliderFrame->AsyncScrollbarDragRejected();
      }
    }
  }
}

void
ScrollFrameHelper::AsyncScrollbarDragRejected()
{
  // We don't get told which scrollbar requested the async drag,
  // so we notify both.
  ::AsyncScrollbarDragRejected(mHScrollbarBox);
  ::AsyncScrollbarDragRejected(mVScrollbarBox);
}
