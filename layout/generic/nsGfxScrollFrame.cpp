/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object to wrap rendering objects that should be scrollable */

#include "nsGfxScrollFrame.h"

#include "nsIXULRuntime.h"
#include "base/compiler_specific.h"
#include "DisplayItemClip.h"
#include "Layers.h"
#include "nsCOMPtr.h"
#include "nsIContentViewer.h"
#include "nsPresContext.h"
#include "nsView.h"
#include "nsViewportInfo.h"
#include "nsContainerFrame.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "mozilla/intl/BidiEmbeddingLevel.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/gfx/gfxVars.h"
#include "nsFontMetrics.h"
#include "nsBoxLayoutState.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsScrollbarFrame.h"
#include "nsINode.h"
#include "nsIScrollbarMediator.h"
#include "nsITextControlFrame.h"
#include "nsILayoutHistoryState.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsStyleTransformMatrix.h"
#include "mozilla/PresState.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsHTMLDocument.h"
#include "nsLayoutUtils.h"
#include "nsBidiPresUtils.h"
#include "nsBidiUtils.h"
#include "nsDocShell.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/DisplayPortUtils.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/ScrollbarPreferences.h"
#include "mozilla/ScrollingMetrics.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SVGOuterSVGFrame.h"
#include "mozilla/ViewportUtils.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/HTMLMarqueeElement.h"
#include "mozilla/dom/ScrollTimeline.h"
#include <stdint.h>
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Telemetry.h"
#include "nsSubDocumentFrame.h"
#include "mozilla/Attributes.h"
#include "ScrollbarActivity.h"
#include "nsRefreshDriver.h"
#include "nsStyleConsts.h"
#include "nsIScrollPositionListener.h"
#include "StickyScrollContainer.h"
#include "nsIFrameInlines.h"
#include "gfxPlatform.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_general.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_mousewheel.h"
#include "mozilla/ToString.h"
#include "ScrollAnimationPhysics.h"
#include "ScrollAnimationBezierPhysics.h"
#include "ScrollAnimationMSDPhysics.h"
#include "ScrollSnap.h"
#include "UnitTransforms.h"
#include "nsSliderFrame.h"
#include "ViewportFrame.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZPublicUtils.h"
#include "mozilla/layers/AxisPhysicsModel.h"
#include "mozilla/layers/AxisPhysicsMSDModel.h"
#include "mozilla/layers/ScrollingInteractionContext.h"
#include "mozilla/layers/ScrollLinkedEffectDetector.h"
#include "mozilla/Unused.h"
#include "MobileViewportManager.h"
#include "VisualViewport.h"
#include "WindowRenderer.h"
#include <algorithm>
#include <cstdlib>  // for std::abs(int/long)
#include <cmath>    // for std::abs(float/double)
#include <tuple>    // for std::tie

static mozilla::LazyLogModule sApzPaintSkipLog("apz.paintskip");
#define PAINT_SKIP_LOG(...) \
  MOZ_LOG(sApzPaintSkipLog, LogLevel::Debug, (__VA_ARGS__))
static mozilla::LazyLogModule sScrollRestoreLog("scrollrestore");
#define SCROLLRESTORE_LOG(...) \
  MOZ_LOG(sScrollRestoreLog, LogLevel::Debug, (__VA_ARGS__))
static mozilla::LazyLogModule sRootScrollbarsLog("rootscrollbars");
#define ROOT_SCROLLBAR_LOG(...)                                  \
  if (mHelper.mIsRoot) {                                         \
    MOZ_LOG(sRootScrollbarsLog, LogLevel::Debug, (__VA_ARGS__)); \
  }
static mozilla::LazyLogModule sDisplayportLog("apz.displayport");

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::layout;
using nsStyleTransformMatrix::TransformReferenceBox;

static ScrollDirections GetOverflowChange(const nsRect& aCurScrolledRect,
                                          const nsRect& aPrevScrolledRect) {
  ScrollDirections result;
  if (aPrevScrolledRect.x != aCurScrolledRect.x ||
      aPrevScrolledRect.width != aCurScrolledRect.width) {
    result += ScrollDirection::eHorizontal;
  }
  if (aPrevScrolledRect.y != aCurScrolledRect.y ||
      aPrevScrolledRect.height != aCurScrolledRect.height) {
    result += ScrollDirection::eVertical;
  }
  return result;
}

/**
 * This class handles the dispatching of scroll events to content.
 *
 * Scroll events are posted to the refresh driver via
 * nsRefreshDriver::PostScrollEvent(), and they are fired during a refresh
 * driver tick, after running requestAnimationFrame callbacks but before
 * the style flush. This allows rAF callbacks to perform scrolling and have
 * that scrolling be reflected on the same refresh driver tick, while at
 * the same time allowing scroll event listeners to make style changes and
 * have those style changes be reflected on the same refresh driver tick.
 *
 * ScrollEvents cannot be refresh observers, because none of the existing
 * categories of refresh observers (FlushType::Style, FlushType::Layout,
 * and FlushType::Display) are run at the desired time in a refresh driver
 * tick. They behave similarly to refresh observers in that their presence
 * causes the refresh driver to tick.
 *
 * ScrollEvents are one-shot runnables; the refresh driver drops them after
 * running them.
 */
class ScrollFrameHelper::ScrollEvent : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE
  explicit ScrollEvent(ScrollFrameHelper* aHelper, bool aDelayed);
  void Revoke() { mHelper = nullptr; }

 private:
  ScrollFrameHelper* mHelper;
};

class ScrollFrameHelper::ScrollEndEvent : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE
  explicit ScrollEndEvent(ScrollFrameHelper* aHelper);
  void Revoke() { mHelper = nullptr; }

 private:
  ScrollFrameHelper* mHelper;
};

class ScrollFrameHelper::AsyncScrollPortEvent : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE
  explicit AsyncScrollPortEvent(ScrollFrameHelper* helper)
      : Runnable("ScrollFrameHelper::AsyncScrollPortEvent"), mHelper(helper) {}
  void Revoke() { mHelper = nullptr; }

 private:
  ScrollFrameHelper* mHelper;
};

class ScrollFrameHelper::ScrolledAreaEvent : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE
  explicit ScrolledAreaEvent(ScrollFrameHelper* helper)
      : Runnable("ScrollFrameHelper::ScrolledAreaEvent"), mHelper(helper) {}
  void Revoke() { mHelper = nullptr; }

 private:
  ScrollFrameHelper* mHelper;
};

//----------------------------------------------------------------------

//----------nsHTMLScrollFrame-------------------------------------------

nsHTMLScrollFrame* NS_NewHTMLScrollFrame(PresShell* aPresShell,
                                         ComputedStyle* aStyle, bool aIsRoot) {
  return new (aPresShell)
      nsHTMLScrollFrame(aStyle, aPresShell->GetPresContext(), aIsRoot);
}

NS_IMPL_FRAMEARENA_HELPERS(nsHTMLScrollFrame)

nsHTMLScrollFrame::nsHTMLScrollFrame(ComputedStyle* aStyle,
                                     nsPresContext* aPresContext,
                                     nsIFrame::ClassID aID, bool aIsRoot)
    : nsContainerFrame(aStyle, aPresContext, aID),
      mHelper(ALLOW_THIS_IN_INITIALIZER_LIST(this), aIsRoot) {}

void nsHTMLScrollFrame::ScrollbarActivityStarted() const {
  if (mHelper.mScrollbarActivity) {
    mHelper.mScrollbarActivity->ActivityStarted();
  }
}

void nsHTMLScrollFrame::ScrollbarActivityStopped() const {
  if (mHelper.mScrollbarActivity) {
    mHelper.mScrollbarActivity->ActivityStopped();
  }
}

nsresult nsHTMLScrollFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  return mHelper.CreateAnonymousContent(aElements);
}

void nsHTMLScrollFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
  mHelper.AppendAnonymousContentTo(aElements, aFilter);
}

void nsHTMLScrollFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                    PostDestroyData& aPostDestroyData) {
  DestroyAbsoluteFrames(aDestructRoot, aPostDestroyData);
  mHelper.Destroy(aPostDestroyData);
  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void nsHTMLScrollFrame::SetInitialChildList(ChildListID aListID,
                                            nsFrameList& aChildList) {
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
  mHelper.ReloadChildFrames();
}

void nsHTMLScrollFrame::AppendFrames(ChildListID aListID,
                                     nsFrameList& aFrameList) {
  NS_ASSERTION(aListID == kPrincipalList, "Only main list supported");
  mFrames.AppendFrames(nullptr, aFrameList);
  mHelper.ReloadChildFrames();
}

void nsHTMLScrollFrame::InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                                     const nsLineList::iterator* aPrevFrameLine,
                                     nsFrameList& aFrameList) {
  NS_ASSERTION(aListID == kPrincipalList, "Only main list supported");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");
  mFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);
  mHelper.ReloadChildFrames();
}

void nsHTMLScrollFrame::RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) {
  NS_ASSERTION(aListID == kPrincipalList, "Only main list supported");
  mFrames.DestroyFrame(aOldFrame);
  mHelper.ReloadChildFrames();
}

static void GetScrollbarMetrics(nsBoxLayoutState& aState, nsIFrame* aBox,
                                nsSize* aMin, nsSize* aPref) {
  NS_ASSERTION(aState.GetRenderingContext(),
               "Must have rendering context in layout state for size "
               "computations");

  if (aMin) {
    *aMin = aBox->GetXULMinSize(aState);
    nsIFrame::AddXULMargin(aBox, *aMin);
    if (aMin->width < 0) {
      aMin->width = 0;
    }
    if (aMin->height < 0) {
      aMin->height = 0;
    }
  }

  if (aPref) {
    *aPref = aBox->GetXULPrefSize(aState);
    nsIFrame::AddXULMargin(aBox, *aPref);
    if (aPref->width < 0) {
      aPref->width = 0;
    }
    if (aPref->height < 0) {
      aPref->height = 0;
    }
  }
}

/**
 HTML scrolling implementation

 All other things being equal, we prefer layouts with fewer scrollbars showing.
*/

namespace mozilla {

enum class ShowScrollbar : uint8_t {
  Auto,
  Always,
  // Never is a misnomer. We can still get a scrollbar if we need to scroll the
  // visual viewport inside the layout viewport. Thus this enum is best thought
  // of as value used by layout, which does not know about the visual viewport.
  // The visual viewport does not affect any layout sizes, so this is sound.
  Never,
};

static ShowScrollbar ShouldShowScrollbar(StyleOverflow aOverflow) {
  switch (aOverflow) {
    case StyleOverflow::Scroll:
      return ShowScrollbar::Always;
    case StyleOverflow::Hidden:
      return ShowScrollbar::Never;
    default:
    case StyleOverflow::Auto:
      return ShowScrollbar::Auto;
  }
}

struct MOZ_STACK_CLASS ScrollReflowInput {
  // === Filled in by the constructor. Members in this section shouldn't change
  // their values after the constructor. ===
  const ReflowInput& mReflowInput;
  nsBoxLayoutState mBoxState;
  ShowScrollbar mHScrollbar;
  // If the horizontal scrollbar is allowed (even if mHScrollbar ==
  // ShowScrollbar::Never) provided that it is for scrolling the visual viewport
  // inside the layout viewport only.
  bool mHScrollbarAllowedForScrollingVVInsideLV = true;
  ShowScrollbar mVScrollbar;
  // If the vertical scrollbar is allowed (even if mVScrollbar ==
  // ShowScrollbar::Never) provided that it is for scrolling the visual viewport
  // inside the layout viewport only.
  bool mVScrollbarAllowedForScrollingVVInsideLV = true;
  nsMargin mComputedBorder;

  // === Filled in by ReflowScrolledFrame ===
  OverflowAreas mContentsOverflowAreas;
  // The scrollbar gutter sizes used in the most recent reflow of
  // mHelper.mScrolledFrame. The writing-mode is the same as the scroll
  // container.
  LogicalMargin mScrollbarGutterFromLastReflow;
  // True if the most recent reflow of mHelper.mScrolledFrame is with the
  // horizontal scrollbar.
  bool mReflowedContentsWithHScrollbar = false;
  // True if the most recent reflow of mHelper.mScrolledFrame is with the
  // vertical scrollbar.
  bool mReflowedContentsWithVScrollbar = false;

  // === Filled in when TryLayout succeeds ===
  // The size of the inside-border area
  nsSize mInsideBorderSize;
  // Whether we decided to show the horizontal scrollbar in the most recent
  // TryLayout.
  bool mShowHScrollbar = false;
  // Whether we decided to show the vertical scrollbar in the most recent
  // TryLayout.
  bool mShowVScrollbar = false;
  // If mShow(H|V)Scrollbar is true then
  // mOnlyNeed(V|H)ScrollbarToScrollVVInsideLV indicates if the only reason we
  // need that scrollbar is to scroll the visual viewport inside the layout
  // viewport. These scrollbars are special in that even if they are layout
  // scrollbars they do not take up any layout space.
  bool mOnlyNeedHScrollbarToScrollVVInsideLV = false;
  bool mOnlyNeedVScrollbarToScrollVVInsideLV = false;

  ScrollReflowInput(nsHTMLScrollFrame* aFrame, const ReflowInput& aReflowInput);

  nscoord VScrollbarMinHeight() const { return mVScrollbarMinSize.height; }
  nscoord VScrollbarPrefWidth() const { return mVScrollbarPrefSize.width; }
  nscoord HScrollbarMinWidth() const { return mHScrollbarMinSize.width; }
  nscoord HScrollbarPrefHeight() const { return mHScrollbarPrefSize.height; }

  // Returns the sizes occupied by the scrollbar gutters. If aShowVScroll or
  // aShowHScroll is true, the sizes occupied by the scrollbars are also
  // included.
  nsMargin ScrollbarGutter(bool aShowVScrollbar, bool aShowHScrollbar,
                           bool aScrollbarOnRight) const {
    nsMargin gutter = mScrollbarGutter;
    if (aShowVScrollbar && gutter.right == 0 && gutter.left == 0) {
      const nscoord w = VScrollbarPrefWidth();
      if (aScrollbarOnRight) {
        gutter.right = w;
      } else {
        gutter.left = w;
      }
    }
    if (aShowHScrollbar && gutter.bottom == 0) {
      // The horizontal scrollbar is always at the bottom side.
      gutter.bottom = HScrollbarPrefHeight();
    }
    return gutter;
  }

 private:
  // Filled in by the constructor. Put variables here to keep them unchanged
  // after initializing them in the constructor.
  nsSize mVScrollbarMinSize;
  nsSize mVScrollbarPrefSize;
  nsSize mHScrollbarMinSize;
  nsSize mHScrollbarPrefSize;
  // The scrollbar gutter sizes resolved from the scrollbar-gutter and
  // scrollbar-width property.
  nsMargin mScrollbarGutter;
};

ScrollReflowInput::ScrollReflowInput(nsHTMLScrollFrame* aFrame,
                                     const ReflowInput& aReflowInput)
    : mReflowInput(aReflowInput),
      // mBoxState is just used for scrollbars so we don't need to
      // worry about the reflow depth here
      mBoxState(aReflowInput.mFrame->PresContext(),
                aReflowInput.mRenderingContext),
      mComputedBorder(aReflowInput.ComputedPhysicalBorderPadding() -
                      aReflowInput.ComputedPhysicalPadding()),
      mScrollbarGutterFromLastReflow(aFrame->GetWritingMode()) {
  ScrollStyles styles = aFrame->GetScrollStyles();
  mHScrollbar = ShouldShowScrollbar(styles.mHorizontal);
  mVScrollbar = ShouldShowScrollbar(styles.mVertical);

  if (nsIFrame* hScrollbarBox = aFrame->GetScrollbarBox(false)) {
    nsScrollbarFrame* scrollbar = do_QueryFrame(hScrollbarBox);
    scrollbar->SetScrollbarMediatorContent(mReflowInput.mFrame->GetContent());

    GetScrollbarMetrics(mBoxState, hScrollbarBox, &mHScrollbarMinSize,
                        &mHScrollbarPrefSize);
  } else {
    mHScrollbar = ShowScrollbar::Never;
    mHScrollbarAllowedForScrollingVVInsideLV = false;
  }
  if (nsIFrame* vScrollbarBox = aFrame->GetScrollbarBox(true)) {
    nsScrollbarFrame* scrollbar = do_QueryFrame(vScrollbarBox);
    scrollbar->SetScrollbarMediatorContent(mReflowInput.mFrame->GetContent());

    GetScrollbarMetrics(mBoxState, vScrollbarBox, &mVScrollbarMinSize,
                        &mVScrollbarPrefSize);
  } else {
    mVScrollbar = ShowScrollbar::Never;
    mVScrollbarAllowedForScrollingVVInsideLV = false;
  }

  const auto* scrollbarStyle =
      nsLayoutUtils::StyleForScrollbar(mReflowInput.mFrame);
  // Hide the scrollbar when the scrollbar-width is set to none.
  //
  // Note: In some cases this is unnecessary, because scrollbar-width:none
  // makes us suppress scrollbars in CreateAnonymousContent. But if this frame
  // initially had a non-'none' scrollbar-width and dynamically changed to
  // 'none', then we'll need to handle it here.
  if (scrollbarStyle->StyleUIReset()->ScrollbarWidth() ==
      StyleScrollbarWidth::None) {
    mHScrollbar = ShowScrollbar::Never;
    mHScrollbarAllowedForScrollingVVInsideLV = false;
    mVScrollbar = ShowScrollbar::Never;
    mVScrollbarAllowedForScrollingVVInsideLV = false;
  } else if (const auto& scrollbarGutterStyle =
                 scrollbarStyle->StyleDisplay()->mScrollbarGutter) {
    const auto stable =
        bool(scrollbarGutterStyle & StyleScrollbarGutter::STABLE);
    const auto bothEdges =
        bool(scrollbarGutterStyle & StyleScrollbarGutter::BOTH_EDGES);

    if (mReflowInput.GetWritingMode().IsVertical()) {
      const nscoord h = HScrollbarPrefHeight();
      if (bothEdges) {
        mScrollbarGutter.top = mScrollbarGutter.bottom = h;
      } else if (stable) {
        // The horizontal scrollbar gutter is always at the bottom side.
        mScrollbarGutter.bottom = h;
      }
    } else {
      const nscoord w = VScrollbarPrefWidth();
      if (bothEdges) {
        mScrollbarGutter.left = mScrollbarGutter.right = w;
      } else if (stable) {
        if (aFrame->IsScrollbarOnRight()) {
          mScrollbarGutter.right = w;
        } else {
          mScrollbarGutter.left = w;
        }
      }
    }
  }
}

}  // namespace mozilla

// XXXldb Can this go away?
static nsSize ComputeInsideBorderSize(const ScrollReflowInput& aState,
                                      const nsSize& aDesiredInsideBorderSize) {
  // aDesiredInsideBorderSize is the frame size; i.e., it includes
  // borders and padding (but the scrolled child doesn't have
  // borders). The scrolled child has the same padding as us.
  nscoord contentWidth = aState.mReflowInput.ComputedWidth();
  if (contentWidth == NS_UNCONSTRAINEDSIZE) {
    contentWidth = aDesiredInsideBorderSize.width -
                   aState.mReflowInput.ComputedPhysicalPadding().LeftRight();
  }
  nscoord contentHeight = aState.mReflowInput.ComputedHeight();
  if (contentHeight == NS_UNCONSTRAINEDSIZE) {
    contentHeight = aDesiredInsideBorderSize.height -
                    aState.mReflowInput.ComputedPhysicalPadding().TopBottom();
  }

  contentWidth = aState.mReflowInput.ApplyMinMaxWidth(contentWidth);
  contentHeight = aState.mReflowInput.ApplyMinMaxHeight(contentHeight);
  return nsSize(
      contentWidth + aState.mReflowInput.ComputedPhysicalPadding().LeftRight(),
      contentHeight +
          aState.mReflowInput.ComputedPhysicalPadding().TopBottom());
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
 * 3) the style is SCROLL, or the kid's overflow-area XMost is
 * greater than the scrollport width
 *
 * @param aForce if true, then we just assume the layout is consistent.
 */
bool nsHTMLScrollFrame::TryLayout(ScrollReflowInput& aState,
                                  ReflowOutput* aKidMetrics,
                                  bool aAssumeHScroll, bool aAssumeVScroll,
                                  bool aForce) {
  if ((aState.mVScrollbar == ShowScrollbar::Never && aAssumeVScroll) ||
      (aState.mHScrollbar == ShowScrollbar::Never && aAssumeHScroll)) {
    NS_ASSERTION(!aForce, "Shouldn't be forcing a hidden scrollbar to show!");
    return false;
  }

  const auto wm = GetWritingMode();
  const nsMargin scrollbarGutter = aState.ScrollbarGutter(
      aAssumeVScroll, aAssumeHScroll, IsScrollbarOnRight());
  const LogicalMargin logicalScrollbarGutter(wm, scrollbarGutter);

  const bool inlineEndsGutterChanged =
      aState.mScrollbarGutterFromLastReflow.IStartEnd(wm) !=
      logicalScrollbarGutter.IStartEnd(wm);
  const bool blockEndsGutterChanged =
      aState.mScrollbarGutterFromLastReflow.BStartEnd(wm) !=
      logicalScrollbarGutter.BStartEnd(wm);
  const bool shouldReflowScrolledFrame =
      inlineEndsGutterChanged ||
      (blockEndsGutterChanged && ScrolledContentDependsOnBSize(aState));

  if (shouldReflowScrolledFrame) {
    if (blockEndsGutterChanged) {
      nsLayoutUtils::MarkIntrinsicISizesDirtyIfDependentOnBSize(
          mHelper.mScrolledFrame);
    }
    aKidMetrics->mOverflowAreas.Clear();
    ROOT_SCROLLBAR_LOG(
        "TryLayout reflowing scrolled frame with scrollbars h=%d, v=%d\n",
        aAssumeHScroll, aAssumeVScroll);
    ReflowScrolledFrame(aState, aAssumeHScroll, aAssumeVScroll, aKidMetrics);
  }

  const nsSize scrollbarGutterSize(scrollbarGutter.LeftRight(),
                                   scrollbarGutter.TopBottom());

  // First, compute our inside-border size and scrollport size
  // XXXldb Can we depend more on ComputeSize here?
  nsSize kidSize =
      aState.mReflowInput.mStyleDisplay->GetContainSizeAxes().ContainSize(
          aKidMetrics->PhysicalSize(), wm);
  const nsSize desiredInsideBorderSize = kidSize + scrollbarGutterSize;
  aState.mInsideBorderSize =
      ComputeInsideBorderSize(aState, desiredInsideBorderSize);

  nsSize layoutSize = mHelper.mIsUsingMinimumScaleSize
                          ? mHelper.mMinimumScaleSize
                          : aState.mInsideBorderSize;

  const nsSize scrollPortSize =
      Max(nsSize(0, 0), layoutSize - scrollbarGutterSize);
  if (mHelper.mIsUsingMinimumScaleSize) {
    mHelper.mICBSize =
        Max(nsSize(0, 0), aState.mInsideBorderSize - scrollbarGutterSize);
  }

  nsSize visualViewportSize = scrollPortSize;
  ROOT_SCROLLBAR_LOG("TryLayout with VV %s\n",
                     ToString(visualViewportSize).c_str());
  mozilla::PresShell* presShell = PresShell();
  // Note: we check for a non-null MobileViepwortManager here, but ideally we
  // should be able to drop that clause as well. It's just that in some cases
  // with extension popups the composition size comes back as stale, because
  // the content viewer is only resized after the popup contents are reflowed.
  // That case also happens to have no APZ and no MVM, so we use that as a
  // way to detect the scenario. Bug 1648669 tracks removing this clause.
  if (mHelper.mIsRoot && presShell->GetMobileViewportManager()) {
    visualViewportSize = nsLayoutUtils::CalculateCompositionSizeForFrame(
        this, false, &layoutSize);
    visualViewportSize =
        Max(nsSize(0, 0), visualViewportSize - scrollbarGutterSize);

    float resolution = presShell->GetResolution();
    visualViewportSize.width /= resolution;
    visualViewportSize.height /= resolution;
    ROOT_SCROLLBAR_LOG("TryLayout now with VV %s\n",
                       ToString(visualViewportSize).c_str());
  }

  nsRect overflowRect = aState.mContentsOverflowAreas.ScrollableOverflow();
  // If the content height expanded by the minimum-scale will be taller than
  // the scrollable overflow area, we need to expand the area here to tell
  // properly whether we need to render the overlay vertical scrollbar.
  // NOTE: This expanded size should NOT be used for non-overley scrollbars
  // cases since putting the vertical non-overlay scrollbar will make the
  // content width narrow a little bit, which in turn the minimum scale value
  // becomes a bit bigger than before, then the vertical scrollbar is no longer
  // needed, which means the content width becomes the original width, then the
  // minimum-scale is changed to the original one, and so forth.
  if (mHelper.UsesOverlayScrollbars() && mHelper.mIsUsingMinimumScaleSize &&
      mHelper.mMinimumScaleSize.height > overflowRect.YMost()) {
    overflowRect.height +=
        mHelper.mMinimumScaleSize.height - overflowRect.YMost();
  }
  nsRect scrolledRect =
      mHelper.GetUnsnappedScrolledRectInternal(overflowRect, scrollPortSize);
  ROOT_SCROLLBAR_LOG(
      "TryLayout scrolledRect:%s overflowRect:%s scrollportSize:%s\n",
      ToString(scrolledRect).c_str(), ToString(overflowRect).c_str(),
      ToString(scrollPortSize).c_str());
  nscoord oneDevPixel = aState.mBoxState.PresContext()->DevPixelsToAppUnits(1);

  bool showHScrollbar = aAssumeHScroll;
  bool showVScrollbar = aAssumeVScroll;
  if (!aForce) {
    nsSize sizeToCompare = visualViewportSize;
    if (gfxPlatform::UseDesktopZoomingScrollbars()) {
      sizeToCompare = scrollPortSize;
    }

    // No need to compute showHScrollbar if we got ShowScrollbar::Never.
    if (aState.mHScrollbar != ShowScrollbar::Never) {
      showHScrollbar =
          aState.mHScrollbar == ShowScrollbar::Always ||
          scrolledRect.XMost() >= sizeToCompare.width + oneDevPixel ||
          scrolledRect.x <= -oneDevPixel;
      // TODO(emilio): This should probably check this scrollbar's minimum size
      // in both axes, for consistency?
      if (aState.mHScrollbar == ShowScrollbar::Auto &&
          scrollPortSize.width < aState.HScrollbarMinWidth()) {
        showHScrollbar = false;
      }
      ROOT_SCROLLBAR_LOG("TryLayout wants H Scrollbar: %d =? %d\n",
                         showHScrollbar, aAssumeHScroll);
    }

    // No need to compute showVScrollbar if we got ShowScrollbar::Never.
    if (aState.mVScrollbar != ShowScrollbar::Never) {
      showVScrollbar =
          aState.mVScrollbar == ShowScrollbar::Always ||
          scrolledRect.YMost() >= sizeToCompare.height + oneDevPixel ||
          scrolledRect.y <= -oneDevPixel;
      // TODO(emilio): This should probably check this scrollbar's minimum size
      // in both axes, for consistency?
      if (aState.mVScrollbar == ShowScrollbar::Auto &&
          scrollPortSize.height < aState.VScrollbarMinHeight()) {
        showVScrollbar = false;
      }
      ROOT_SCROLLBAR_LOG("TryLayout wants V Scrollbar: %d =? %d\n",
                         showVScrollbar, aAssumeVScroll);
    }

    if (showHScrollbar != aAssumeHScroll || showVScrollbar != aAssumeVScroll) {
      const nsMargin wantedScrollbarGutter = aState.ScrollbarGutter(
          showVScrollbar, showHScrollbar, IsScrollbarOnRight());
      // We report an inconsistent layout only when the desired visibility of
      // the scrollbars can change the size of the scrollbar gutters.
      if (scrollbarGutter != wantedScrollbarGutter) {
        return false;
      }
    }
  }

  // If we reach here, the layout is consistent. Record the desired visibility
  // of the scrollbars.
  aState.mShowHScrollbar = showHScrollbar;
  aState.mShowVScrollbar = showVScrollbar;
  const nsPoint scrollPortOrigin(
      aState.mComputedBorder.left + scrollbarGutter.left,
      aState.mComputedBorder.top + scrollbarGutter.top);
  mHelper.SetScrollPort(nsRect(scrollPortOrigin, scrollPortSize));

  if (mHelper.mIsRoot && gfxPlatform::UseDesktopZoomingScrollbars()) {
    bool vvChanged = true;
    // This loop can run at most twice since we can only add a scrollbar once.
    // At this point we've already decided that this layout is consistent so we
    // will return true. Scrollbars added here never take up layout space even
    // if they are layout scrollbars so any changes made here will not make us
    // return false.
    while (vvChanged) {
      vvChanged = false;
      if (!aState.mShowHScrollbar &&
          aState.mHScrollbarAllowedForScrollingVVInsideLV) {
        if (mHelper.ScrollPort().width >=
                visualViewportSize.width + oneDevPixel &&
            visualViewportSize.width >= aState.HScrollbarMinWidth()) {
          vvChanged = true;
          visualViewportSize.height -= aState.HScrollbarPrefHeight();
          aState.mShowHScrollbar = true;
          aState.mOnlyNeedHScrollbarToScrollVVInsideLV = true;
          ROOT_SCROLLBAR_LOG("TryLayout added H scrollbar for VV, VV now %s\n",
                             ToString(visualViewportSize).c_str());
        }
      }

      if (!aState.mShowVScrollbar &&
          aState.mVScrollbarAllowedForScrollingVVInsideLV) {
        if (mHelper.ScrollPort().height >=
                visualViewportSize.height + oneDevPixel &&
            visualViewportSize.height >= aState.VScrollbarMinHeight()) {
          vvChanged = true;
          visualViewportSize.width -= aState.VScrollbarPrefWidth();
          aState.mShowVScrollbar = true;
          aState.mOnlyNeedVScrollbarToScrollVVInsideLV = true;
          ROOT_SCROLLBAR_LOG("TryLayout added V scrollbar for VV, VV now %s\n",
                             ToString(visualViewportSize).c_str());
        }
      }
    }
  }

  return true;
}

bool nsHTMLScrollFrame::ScrolledContentDependsOnBSize(
    const ScrollReflowInput& aState) const {
  return mHelper.mScrolledFrame->HasAnyStateBits(
             NS_FRAME_CONTAINS_RELATIVE_BSIZE |
             NS_FRAME_DESCENDANT_INTRINSIC_ISIZE_DEPENDS_ON_BSIZE) ||
         aState.mReflowInput.ComputedBSize() != NS_UNCONSTRAINEDSIZE ||
         aState.mReflowInput.ComputedMinBSize() > 0 ||
         aState.mReflowInput.ComputedMaxBSize() != NS_UNCONSTRAINEDSIZE;
}

void nsHTMLScrollFrame::ReflowScrolledFrame(ScrollReflowInput& aState,
                                            bool aAssumeHScroll,
                                            bool aAssumeVScroll,
                                            ReflowOutput* aMetrics) {
  const WritingMode wm = GetWritingMode();

  // these could be NS_UNCONSTRAINEDSIZE ... std::min arithmetic should
  // be OK
  LogicalMargin padding = aState.mReflowInput.ComputedLogicalPadding(wm);
  nscoord availISize =
      aState.mReflowInput.ComputedISize() + padding.IStartEnd(wm);

  nscoord computedBSize = aState.mReflowInput.ComputedBSize();
  nscoord computedMinBSize = aState.mReflowInput.ComputedMinBSize();
  nscoord computedMaxBSize = aState.mReflowInput.ComputedMaxBSize();
  if (!ShouldPropagateComputedBSizeToScrolledContent()) {
    computedBSize = NS_UNCONSTRAINEDSIZE;
    computedMinBSize = 0;
    computedMaxBSize = NS_UNCONSTRAINEDSIZE;
  }

  const LogicalMargin scrollbarGutter(
      wm, aState.ScrollbarGutter(aAssumeVScroll, aAssumeHScroll,
                                 IsScrollbarOnRight()));
  if (const nscoord inlineEndsGutter = scrollbarGutter.IStartEnd(wm);
      inlineEndsGutter > 0) {
    availISize = std::max(0, availISize - inlineEndsGutter);
  }
  if (const nscoord blockEndsGutter = scrollbarGutter.BStartEnd(wm);
      blockEndsGutter > 0) {
    if (computedBSize != NS_UNCONSTRAINEDSIZE) {
      computedBSize = std::max(0, computedBSize - blockEndsGutter);
    }
    computedMinBSize = std::max(0, computedMinBSize - blockEndsGutter);
    if (computedMaxBSize != NS_UNCONSTRAINEDSIZE) {
      computedMaxBSize = std::max(0, computedMaxBSize - blockEndsGutter);
    }
  }

  nsPresContext* presContext = PresContext();

  // Pass InitFlags::CallerWillInit so we can pass in the correct padding.
  ReflowInput kidReflowInput(presContext, aState.mReflowInput,
                             mHelper.mScrolledFrame,
                             LogicalSize(wm, availISize, NS_UNCONSTRAINEDSIZE),
                             Nothing(), ReflowInput::InitFlag::CallerWillInit);
  const WritingMode kidWM = kidReflowInput.GetWritingMode();
  kidReflowInput.Init(presContext, Nothing(), Nothing(),
                      Some(padding.ConvertTo(kidWM, wm)));
  kidReflowInput.mFlags.mAssumingHScrollbar = aAssumeHScroll;
  kidReflowInput.mFlags.mAssumingVScrollbar = aAssumeVScroll;
  kidReflowInput.mFlags.mTreatBSizeAsIndefinite =
      aState.mReflowInput.mFlags.mTreatBSizeAsIndefinite;
  kidReflowInput.SetComputedBSize(computedBSize);
  kidReflowInput.ComputedMinBSize() = computedMinBSize;
  kidReflowInput.ComputedMaxBSize() = computedMaxBSize;
  if (aState.mReflowInput.IsBResizeForWM(kidWM)) {
    kidReflowInput.SetBResize(true);
  }
  if (aState.mReflowInput.IsBResizeForPercentagesForWM(kidWM)) {
    kidReflowInput.mFlags.mIsBResizeForPercentages = true;
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
  // the frame (i.e. if ReflowChildFlags::NoMoveFrame isn't set)
  const nsSize dummyContainerSize;
  ReflowChild(mHelper.mScrolledFrame, presContext, *aMetrics, kidReflowInput,
              wm, LogicalPoint(wm), dummyContainerSize,
              ReflowChildFlags::NoMoveFrame, status);

  mHelper.mHasHorizontalScrollbar = didHaveHorizontalScrollbar;
  mHelper.mHasVerticalScrollbar = didHaveVerticalScrollbar;

  // Don't resize or position the view (if any) because we're going to resize
  // it to the correct size anyway in PlaceScrollArea. Allowing it to
  // resize here would size it to the natural height of the frame,
  // which will usually be different from the scrollport height;
  // invalidating the difference will cause unnecessary repainting.
  FinishReflowChild(
      mHelper.mScrolledFrame, presContext, *aMetrics, &kidReflowInput, wm,
      LogicalPoint(wm), dummyContainerSize,
      ReflowChildFlags::NoMoveFrame | ReflowChildFlags::NoSizeView);

  if (mHelper.mScrolledFrame->HasAnyStateBits(
          NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
    // Propagate NS_FRAME_CONTAINS_RELATIVE_BSIZE from our inner scrolled frame
    // to ourselves so that our containing block is aware of it.
    //
    // Note: If the scrolled frame has any child whose block-size depends on the
    // containing block's block-size, the NS_FRAME_CONTAINS_RELATIVE_BSIZE bit
    // is set on the scrolled frame when initializing the child's ReflowInput in
    // ReflowInput::InitResizeFlags(). Therefore, we propagate the bit here
    // after we reflowed the scrolled frame.
    AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
  }

  // XXX Some frames (e.g. nsFrameFrame, nsTextFrame) don't
  // bother setting their mOverflowArea. This is wrong because every frame
  // should always set mOverflowArea. In fact nsFrameFrame doesn't
  // support the 'outline' property because of this. Rather than fix the
  // world right now, just fix up the overflow area if necessary. Note that we
  // don't check HasOverflowRect() because it could be set even though the
  // overflow area doesn't include the frame bounds.
  aMetrics->UnionOverflowAreasWithDesiredBounds();

  auto* disp = StyleDisplay();
  if (MOZ_UNLIKELY(disp->mOverflowClipBoxInline ==
                   StyleOverflowClipBox::ContentBox)) {
    // The scrolled frame is scrollable in the inline axis with
    // `overflow-clip-box:content-box`. To prevent its content from being
    // clipped at the scroll container's padding edges, we inflate its
    // children's scrollable overflow area with its inline padding, and union
    // its scrollable overflow area with its children's inflated scrollable
    // overflow area.
    OverflowAreas childOverflow;
    mHelper.mScrolledFrame->UnionChildOverflow(childOverflow);
    nsRect childScrollableOverflow = childOverflow.ScrollableOverflow();

    const LogicalMargin inlinePadding =
        padding.ApplySkipSides(LogicalSides(wm, eLogicalSideBitsBBoth));
    childScrollableOverflow.Inflate(inlinePadding.GetPhysicalMargin(wm));

    nsRect& so = aMetrics->ScrollableOverflow();
    so = so.UnionEdges(childScrollableOverflow);
  }

  aState.mContentsOverflowAreas = aMetrics->mOverflowAreas;
  aState.mScrollbarGutterFromLastReflow = scrollbarGutter;
  aState.mReflowedContentsWithHScrollbar = aAssumeHScroll;
  aState.mReflowedContentsWithVScrollbar = aAssumeVScroll;
}

bool nsHTMLScrollFrame::GuessHScrollbarNeeded(const ScrollReflowInput& aState) {
  if (aState.mHScrollbar != ShowScrollbar::Auto) {
    // no guessing required
    return aState.mHScrollbar == ShowScrollbar::Always;
  }
  // We only care about scrollbars that might take up space when trying to guess
  // if we need a scrollbar, so we ignore scrollbars only created to scroll the
  // visual viewport inside the layout viewport because they take up no layout
  // space.
  return mHelper.mHasHorizontalScrollbar &&
         !mHelper.mOnlyNeedHScrollbarToScrollVVInsideLV;
}

bool nsHTMLScrollFrame::GuessVScrollbarNeeded(const ScrollReflowInput& aState) {
  if (aState.mVScrollbar != ShowScrollbar::Auto) {
    // no guessing required
    return aState.mVScrollbar == ShowScrollbar::Always;
  }

  // If we've had at least one non-initial reflow, then just assume
  // the state of the vertical scrollbar will be what we determined
  // last time.
  if (mHelper.mHadNonInitialReflow) {
    // We only care about scrollbars that might take up space when trying to
    // guess if we need a scrollbar, so we ignore scrollbars only created to
    // scroll the visual viewport inside the layout viewport because they take
    // up no layout space.
    return mHelper.mHasVerticalScrollbar &&
           !mHelper.mOnlyNeedVScrollbarToScrollVVInsideLV;
  }

  // If this is the initial reflow, guess false because usually
  // we have very little content by then.
  if (InInitialReflow()) return false;

  if (mHelper.mIsRoot) {
    nsIFrame* f = mHelper.mScrolledFrame->PrincipalChildList().FirstChild();
    if (f && f->IsSVGOuterSVGFrame() &&
        static_cast<SVGOuterSVGFrame*>(f)->VerticalScrollbarNotNeeded()) {
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

bool nsHTMLScrollFrame::InInitialReflow() const {
  // We're in an initial reflow if NS_FRAME_FIRST_REFLOW is set, unless we're a
  // root scrollframe.  In that case we want to skip this clause altogether.
  // The guess here is that there are lots of overflow:auto divs out there that
  // end up auto-sizing so they don't overflow, and that the root basically
  // always needs a scrollbar if it did last time we loaded this page (good
  // assumption, because our initial reflow is no longer synchronous).
  return !mHelper.mIsRoot && HasAnyStateBits(NS_FRAME_FIRST_REFLOW);
}

void nsHTMLScrollFrame::ReflowContents(ScrollReflowInput& aState,
                                       const ReflowOutput& aDesiredSize) {
  const WritingMode desiredWm = aDesiredSize.GetWritingMode();
  ReflowOutput kidDesiredSize(desiredWm);
  ReflowScrolledFrame(aState, GuessHScrollbarNeeded(aState),
                      GuessVScrollbarNeeded(aState), &kidDesiredSize);

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
  if ((aState.mReflowedContentsWithHScrollbar ||
       aState.mReflowedContentsWithVScrollbar) &&
      aState.mVScrollbar != ShowScrollbar::Always &&
      aState.mHScrollbar != ShowScrollbar::Always) {
    nsSize kidSize =
        aState.mReflowInput.mStyleDisplay->GetContainSizeAxes().ContainSize(
            kidDesiredSize.PhysicalSize(), desiredWm);
    nsSize insideBorderSize = ComputeInsideBorderSize(aState, kidSize);
    nsRect scrolledRect = mHelper.GetUnsnappedScrolledRectInternal(
        kidDesiredSize.ScrollableOverflow(), insideBorderSize);
    if (nsRect(nsPoint(0, 0), insideBorderSize).Contains(scrolledRect)) {
      // Let's pretend we had no scrollbars coming in here
      kidDesiredSize.mOverflowAreas.Clear();
      ReflowScrolledFrame(aState, false, false, &kidDesiredSize);
    }
  }

  if (IsRootScrollFrameOfDocument()) {
    mHelper.UpdateMinimumScaleSize(
        aState.mContentsOverflowAreas.ScrollableOverflow(),
        kidDesiredSize.PhysicalSize());
  }

  // Try vertical scrollbar settings that leave the vertical scrollbar
  // unchanged. Do this first because changing the vertical scrollbar setting is
  // expensive, forcing a reflow always.

  // Try leaving the horizontal scrollbar unchanged first. This will be more
  // efficient.
  ROOT_SCROLLBAR_LOG("Trying layout1 with %d, %d\n",
                     aState.mReflowedContentsWithHScrollbar,
                     aState.mReflowedContentsWithVScrollbar);
  if (TryLayout(aState, &kidDesiredSize, aState.mReflowedContentsWithHScrollbar,
                aState.mReflowedContentsWithVScrollbar, false)) {
    return;
  }
  ROOT_SCROLLBAR_LOG("Trying layout2 with %d, %d\n",
                     !aState.mReflowedContentsWithHScrollbar,
                     aState.mReflowedContentsWithVScrollbar);
  if (TryLayout(aState, &kidDesiredSize,
                !aState.mReflowedContentsWithHScrollbar,
                aState.mReflowedContentsWithVScrollbar, false)) {
    return;
  }

  // OK, now try toggling the vertical scrollbar. The performance advantage
  // of trying the status-quo horizontal scrollbar state
  // does not exist here (we'll have to reflow due to the vertical scrollbar
  // change), so always try no horizontal scrollbar first.
  bool newVScrollbarState = !aState.mReflowedContentsWithVScrollbar;
  ROOT_SCROLLBAR_LOG("Trying layout3 with %d, %d\n", false, newVScrollbarState);
  if (TryLayout(aState, &kidDesiredSize, false, newVScrollbarState, false)) {
    return;
  }
  ROOT_SCROLLBAR_LOG("Trying layout4 with %d, %d\n", true, newVScrollbarState);
  if (TryLayout(aState, &kidDesiredSize, true, newVScrollbarState, false)) {
    return;
  }

  // OK, we're out of ideas. Try again enabling whatever scrollbars we can
  // enable and force the layout to stick even if it's inconsistent.
  // This just happens sometimes.
  ROOT_SCROLLBAR_LOG("Giving up, adding both scrollbars...\n");
  TryLayout(aState, &kidDesiredSize, aState.mHScrollbar != ShowScrollbar::Never,
            aState.mVScrollbar != ShowScrollbar::Never, true);
}

void nsHTMLScrollFrame::PlaceScrollArea(ScrollReflowInput& aState,
                                        const nsPoint& aScrollPosition) {
  nsIFrame* scrolledFrame = mHelper.mScrolledFrame;
  // Set the x,y of the scrolled frame to the correct value
  scrolledFrame->SetPosition(mHelper.ScrollPort().TopLeft() - aScrollPosition);

  // Recompute our scrollable overflow, taking perspective children into
  // account. Note that this only recomputes the overflow areas stored on the
  // helper (which are used to compute scrollable length and scrollbar thumb
  // sizes) but not the overflow areas stored on the frame. This seems to work
  // for now, but it's possible that we may need to update both in the future.
  AdjustForPerspective(aState.mContentsOverflowAreas.ScrollableOverflow());

  // Preserve the width or height of empty rects
  const nsSize portSize = mHelper.ScrollPort().Size();
  nsRect scrolledRect = mHelper.GetUnsnappedScrolledRectInternal(
      aState.mContentsOverflowAreas.ScrollableOverflow(), portSize);
  nsRect scrolledArea =
      scrolledRect.UnionEdges(nsRect(nsPoint(0, 0), portSize));

  // Store the new overflow area. Note that this changes where an outline
  // of the scrolled frame would be painted, but scrolled frames can't have
  // outlines (the outline would go on this scrollframe instead).
  // Using FinishAndStoreOverflow is needed so the overflow rect gets set
  // correctly.  It also messes with the overflow rect in the 'clip' case, but
  // scrolled frames can't have 'overflow' either.
  // This needs to happen before SyncFrameViewAfterReflow so
  // HasOverflowRect() will return the correct value.
  OverflowAreas overflow(scrolledArea, scrolledArea);
  scrolledFrame->FinishAndStoreOverflow(overflow, scrolledFrame->GetSize());

  // Note that making the view *exactly* the size of the scrolled area
  // is critical, since the view scrolling code uses the size of the
  // scrolled view to clamp scroll requests.
  // Normally the scrolledFrame won't have a view but in some cases it
  // might create its own.
  nsContainerFrame::SyncFrameViewAfterReflow(
      scrolledFrame->PresContext(), scrolledFrame, scrolledFrame->GetView(),
      scrolledArea, ReflowChildFlags::Default);
}

nscoord nsHTMLScrollFrame::IntrinsicScrollbarGutterSizeAtInlineEdges(
    gfxContext* aRenderingContext) {
  const bool isVerticalWM = GetWritingMode().IsVertical();
  nsIFrame* inlineEndScrollbarBox =
      isVerticalWM ? mHelper.mHScrollbarBox : mHelper.mVScrollbarBox;
  if (!inlineEndScrollbarBox) {
    // No scrollbar box frame means no intrinsic size.
    return 0;
  }

  const auto* styleForScrollbar = nsLayoutUtils::StyleForScrollbar(this);
  if (styleForScrollbar->StyleUIReset()->ScrollbarWidth() ==
      StyleScrollbarWidth::None) {
    // Scrollbar shouldn't appear at all with "scrollbar-width: none".
    return 0;
  }

  const auto& styleScrollbarGutter =
      styleForScrollbar->StyleDisplay()->mScrollbarGutter;
  ScrollStyles ss = GetScrollStyles();
  const StyleOverflow& inlineEndStyleOverflow =
      isVerticalWM ? ss.mHorizontal : ss.mVertical;

  // Return the scrollbar-gutter size only if we have "overflow:scroll" or
  // non-auto "scrollbar-gutter", so early-return here if the conditions aren't
  // satisfied.
  if (inlineEndStyleOverflow != StyleOverflow::Scroll &&
      styleScrollbarGutter == StyleScrollbarGutter::AUTO) {
    return 0;
  }

  // No need to worry about reflow depth here since it's just for scrollbars.
  nsBoxLayoutState bls(PresContext(), aRenderingContext, 0);
  nsSize scrollbarPrefSize;
  GetScrollbarMetrics(bls, inlineEndScrollbarBox, nullptr, &scrollbarPrefSize);
  const nscoord scrollbarSize =
      isVerticalWM ? scrollbarPrefSize.height : scrollbarPrefSize.width;
  const auto bothEdges =
      bool(styleScrollbarGutter & StyleScrollbarGutter::BOTH_EDGES);
  return bothEdges ? scrollbarSize * 2 : scrollbarSize;
}

// Legacy, this sucks!
static bool IsMarqueeScrollbox(const nsIFrame& aScrollFrame) {
  if (!aScrollFrame.GetContent()) {
    return false;
  }
  if (MOZ_LIKELY(!aScrollFrame.GetContent()->IsInUAWidget())) {
    return false;
  }
  MOZ_ASSERT(aScrollFrame.GetParent() &&
             aScrollFrame.GetParent()->GetContent());
  return aScrollFrame.GetParent() &&
         HTMLMarqueeElement::FromNodeOrNull(
             aScrollFrame.GetParent()->GetContent());
}

/* virtual */
nscoord nsHTMLScrollFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result = [&] {
    if (StyleDisplay()->GetContainSizeAxes().mIContained) {
      return 0;
    }
    if (MOZ_UNLIKELY(IsMarqueeScrollbox(*this))) {
      return 0;
    }
    return mHelper.mScrolledFrame->GetMinISize(aRenderingContext);
  }();

  DISPLAY_MIN_INLINE_SIZE(this, result);
  return result + IntrinsicScrollbarGutterSizeAtInlineEdges(aRenderingContext);
}

/* virtual */
nscoord nsHTMLScrollFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result =
      StyleDisplay()->GetContainSizeAxes().mIContained
          ? 0
          : mHelper.mScrolledFrame->GetPrefISize(aRenderingContext);
  DISPLAY_PREF_INLINE_SIZE(this, result);
  return NSCoordSaturatingAdd(
      result, IntrinsicScrollbarGutterSizeAtInlineEdges(aRenderingContext));
}

nsresult nsHTMLScrollFrame::GetXULPadding(nsMargin& aMargin) {
  // Our padding hangs out on the inside of the scrollframe, but XUL doesn't
  // reaize that.  If we're stuck inside a XUL box, we need to claim no
  // padding.
  // @see also nsXULScrollFrame::GetXULPadding.
  aMargin.SizeTo(0, 0, 0, 0);
  return NS_OK;
}

bool nsHTMLScrollFrame::IsXULCollapsed() {
  // We're never collapsed in the box sense.
  return false;
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
static void GetScrollableOverflowForPerspective(
    nsIFrame* aScrolledFrame, nsIFrame* aCurrentFrame, const nsRect aScrollPort,
    nsPoint aOffset, nsRect& aScrolledFrameOverflowArea) {
  // Iterate over all children except pop-ups.
  for (const auto& [list, listID] : aCurrentFrame->ChildLists()) {
    if (listID == nsIFrame::kPopupList) {
      continue;
    }

    for (nsIFrame* child : list) {
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
        nsRect preScroll, postScroll;
        {
          // TODO: Can we reuse the reference box?
          TransformReferenceBox refBox(child);
          preScroll = nsDisplayTransform::TransformRect(
              child->ScrollableOverflowRectRelativeToSelf(), child, refBox);
        }

        // Temporarily override the scroll position of the scrolled frame by
        // 10 CSS pixels, and then recompute what the overflow rect would be.
        // This scroll position may not be valid, but that shouldn't matter
        // for our calculations.
        {
          aScrolledFrame->SetPosition(scrollPos + nsPoint(600, 600));
          TransformReferenceBox refBox(child);
          postScroll = nsDisplayTransform::TransformRect(
              child->ScrollableOverflowRectRelativeToSelf(), child, refBox);
          aScrolledFrame->SetPosition(scrollPos);
        }

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
        overhang.top = NSCoordSaturatingMultiply(
            overhang.top, static_cast<float>(1 / bottomDelta));
        overhang.right = NSCoordSaturatingMultiply(
            overhang.right, static_cast<float>(1 / rightDelta));
        overhang.bottom = NSCoordSaturatingMultiply(
            overhang.bottom, static_cast<float>(1 / bottomDelta));
        overhang.left = NSCoordSaturatingMultiply(
            overhang.left, static_cast<float>(1 / rightDelta));

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
            child->ScrollableOverflowRectRelativeToParent());
      }
    }
  }
}

nscoord nsHTMLScrollFrame::GetLogicalBaseline(WritingMode aWritingMode) const {
  // This function implements some of the spec text here:
  //  https://drafts.csswg.org/css-align/#baseline-export
  //
  // Specifically: if our scrolled frame is a block, we just use the inherited
  // GetLogicalBaseline() impl, which synthesizes a baseline from the
  // margin-box. Otherwise, we defer to our scrolled frame, considering it
  // to be scrolled to its initial scroll position.
  if (mHelper.mScrolledFrame->IsBlockFrameOrSubclass() ||
      StyleDisplay()->IsContainLayout()) {
    return nsContainerFrame::GetLogicalBaseline(aWritingMode);
  }

  // OK, here's where we defer to our scrolled frame. We have to add our
  // border BStart thickness to whatever it returns, to produce an offset in
  // our frame-rect's coordinate system. (We don't have to add padding,
  // because the scrolled frame handles our padding.)
  LogicalMargin border = GetLogicalUsedBorder(aWritingMode);

  return border.BStart(aWritingMode) +
         mHelper.mScrolledFrame->GetLogicalBaseline(aWritingMode);
}

void nsHTMLScrollFrame::AdjustForPerspective(nsRect& aScrollableOverflow) {
  // If we have perspective that is being applied to our children, then
  // the effective transform on the child depends on the relative position
  // of the child to us and changes during scrolling.
  if (!ChildrenHavePerspective()) {
    return;
  }
  aScrollableOverflow.SetEmpty();
  GetScrollableOverflowForPerspective(
      mHelper.mScrolledFrame, mHelper.mScrolledFrame, mHelper.ScrollPort(),
      nsPoint(), aScrollableOverflow);
}

void nsHTMLScrollFrame::Reflow(nsPresContext* aPresContext,
                               ReflowOutput& aDesiredSize,
                               const ReflowInput& aReflowInput,
                               nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsHTMLScrollFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  mHelper.HandleScrollbarStyleSwitching();

  ScrollReflowInput state(this, aReflowInput);

  //------------ Handle Incremental Reflow -----------------
  bool reflowHScrollbar = true;
  bool reflowVScrollbar = true;
  bool reflowScrollCorner = true;
  if (!aReflowInput.ShouldReflowAllKids()) {
    auto NeedsReflow = [](const nsIFrame* aFrame) {
      return aFrame && aFrame->IsSubtreeDirty();
    };

    reflowHScrollbar = NeedsReflow(mHelper.mHScrollbarBox);
    reflowVScrollbar = NeedsReflow(mHelper.mVScrollbarBox);
    reflowScrollCorner = NeedsReflow(mHelper.mScrollCornerBox) ||
                         NeedsReflow(mHelper.mResizerBox);
  }

  if (mHelper.mIsRoot) {
    reflowScrollCorner = false;
  }

  const nsRect oldScrollPort = mHelper.ScrollPort();
  nsRect oldScrolledAreaBounds =
      mHelper.mScrolledFrame->ScrollableOverflowRectRelativeToParent();
  nsPoint oldScrollPosition = mHelper.GetScrollPosition();

  ReflowContents(state, aDesiredSize);

  nsSize layoutSize = mHelper.mIsUsingMinimumScaleSize
                          ? mHelper.mMinimumScaleSize
                          : state.mInsideBorderSize;
  aDesiredSize.Width() = layoutSize.width + state.mComputedBorder.LeftRight();
  aDesiredSize.Height() = layoutSize.height + state.mComputedBorder.TopBottom();

  // Set the size of the frame now since computing the perspective-correct
  // overflow (within PlaceScrollArea) can rely on it.
  SetSize(aDesiredSize.GetWritingMode(),
          aDesiredSize.Size(aDesiredSize.GetWritingMode()));

  // Restore the old scroll position, for now, even if that's not valid anymore
  // because we changed size. We'll fix it up in a post-reflow callback, because
  // our current size may only be temporary (e.g. we're compute XUL desired
  // sizes).
  PlaceScrollArea(state, oldScrollPosition);
  if (!mHelper.mPostedReflowCallback) {
    // Make sure we'll try scrolling to restored position
    PresShell()->PostReflowCallback(&mHelper);
    mHelper.mPostedReflowCallback = true;
  }

  bool didOnlyHScrollbar = mHelper.mOnlyNeedHScrollbarToScrollVVInsideLV;
  bool didOnlyVScrollbar = mHelper.mOnlyNeedVScrollbarToScrollVVInsideLV;
  mHelper.mOnlyNeedHScrollbarToScrollVVInsideLV =
      state.mOnlyNeedHScrollbarToScrollVVInsideLV;
  mHelper.mOnlyNeedVScrollbarToScrollVVInsideLV =
      state.mOnlyNeedVScrollbarToScrollVVInsideLV;

  bool didHaveHScrollbar = mHelper.mHasHorizontalScrollbar;
  bool didHaveVScrollbar = mHelper.mHasVerticalScrollbar;
  mHelper.mHasHorizontalScrollbar = state.mShowHScrollbar;
  mHelper.mHasVerticalScrollbar = state.mShowVScrollbar;
  const nsRect& newScrollPort = mHelper.ScrollPort();
  nsRect newScrolledAreaBounds =
      mHelper.mScrolledFrame->ScrollableOverflowRectRelativeToParent();
  if (mHelper.mSkippedScrollbarLayout || reflowHScrollbar || reflowVScrollbar ||
      reflowScrollCorner || HasAnyStateBits(NS_FRAME_IS_DIRTY) ||
      didHaveHScrollbar != state.mShowHScrollbar ||
      didHaveVScrollbar != state.mShowVScrollbar ||
      didOnlyHScrollbar != mHelper.mOnlyNeedHScrollbarToScrollVVInsideLV ||
      didOnlyVScrollbar != mHelper.mOnlyNeedVScrollbarToScrollVVInsideLV ||
      !oldScrollPort.IsEqualEdges(newScrollPort) ||
      !oldScrolledAreaBounds.IsEqualEdges(newScrolledAreaBounds)) {
    if (!mHelper.mSuppressScrollbarUpdate) {
      mHelper.mSkippedScrollbarLayout = false;
      ScrollFrameHelper::SetScrollbarVisibility(mHelper.mHScrollbarBox,
                                                state.mShowHScrollbar);
      ScrollFrameHelper::SetScrollbarVisibility(mHelper.mVScrollbarBox,
                                                state.mShowVScrollbar);
      // place and reflow scrollbars
      const nsRect insideBorderArea(
          nsPoint(state.mComputedBorder.left, state.mComputedBorder.top),
          layoutSize);
      mHelper.LayoutScrollbars(state.mBoxState, insideBorderArea,
                               oldScrollPort);
    } else {
      mHelper.mSkippedScrollbarLayout = true;
    }
  }
  if (mHelper.mIsRoot) {
    if (RefPtr<MobileViewportManager> manager =
            PresShell()->GetMobileViewportManager()) {
      // Note that this runs during layout, and when we get here the root
      // scrollframe has already been laid out. It may have added or removed
      // scrollbars as a result of that layout, so we need to ensure the
      // visual viewport is updated to account for that before we read the
      // visual viewport size.
      manager->UpdateVisualViewportSizeForPotentialScrollbarChange();
    } else if (oldScrollPort.Size() != newScrollPort.Size()) {
      // We want to make sure to send a visual viewport resize event if the
      // scrollport changed sizes for root scroll frames. The
      // MobileViewportManager will do that, but if we don't have one (ie we
      // aren't a root content document for example) we have to send one
      // ourselves.
      if (auto* window = nsGlobalWindowInner::Cast(
              aPresContext->Document()->GetInnerWindow())) {
        window->VisualViewport()->PostResizeEvent();
      }
    }
  }

  // Note that we need to do this after the
  // UpdateVisualViewportSizeForPotentialScrollbarChange call above because that
  // is what updates the visual viewport size and we need it to be up to date.
  if (mHelper.mIsRoot && !mHelper.UsesOverlayScrollbars() &&
      (didHaveHScrollbar != state.mShowHScrollbar ||
       didHaveVScrollbar != state.mShowVScrollbar ||
       didOnlyHScrollbar != mHelper.mOnlyNeedHScrollbarToScrollVVInsideLV ||
       didOnlyVScrollbar != mHelper.mOnlyNeedVScrollbarToScrollVVInsideLV) &&
      PresShell()->IsVisualViewportOffsetSet()) {
    // Removing layout/classic scrollbars can make a previously valid vvoffset
    // invalid. For example, if we are zoomed in on an overflow hidden document
    // and then zoom back out, when apz reaches the initial resolution (ie 1.0)
    // it won't know that we can remove the scrollbars, so the vvoffset can
    // validly be upto the width/height of the scrollbars. After we reflow and
    // remove the scrollbars the only valid vvoffset is (0,0). We could wait
    // until we send the new frame metrics to apz and then have it reply with
    // the new corrected vvoffset but having an inconsistent vvoffset causes
    // problems so trigger the vvoffset to be re-set and re-clamped in
    // ReflowFinished.
    mHelper.mReclampVVOffsetInReflowFinished = true;
  }

  aDesiredSize.SetOverflowAreasToDesiredBounds();

  mHelper.UpdateSticky();
  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowInput,
                                 aStatus);

  if (!InInitialReflow() && !mHelper.mHadNonInitialReflow) {
    mHelper.mHadNonInitialReflow = true;
  }

  if (mHelper.mIsRoot &&
      !oldScrolledAreaBounds.IsEqualEdges(newScrolledAreaBounds)) {
    mHelper.PostScrolledAreaEvent();
  }

  mHelper.UpdatePrevScrolledRect();

  aStatus.Reset();  // This type of frame can't be split.
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
  mHelper.PostOverflowEvent();
}

void nsHTMLScrollFrame::DidReflow(nsPresContext* aPresContext,
                                  const ReflowInput* aReflowInput) {
  nsContainerFrame::DidReflow(aPresContext, aReflowInput);
  PresShell()->PostPendingScrollAnchorAdjustment(Anchor());
}

////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_FRAME_DUMP
nsresult nsHTMLScrollFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"HTMLScroll"_ns, aResult);
}
#endif

#ifdef ACCESSIBILITY
a11y::AccType nsHTMLScrollFrame::AccessibleType() {
  if (IsTableCaption()) {
    return GetRect().IsEmpty() ? a11y::eNoType : a11y::eHTMLCaptionType;
  }

  // Create an accessible regardless of focusable state because the state can be
  // changed during frame life cycle without any notifications to accessibility.
  if (mContent->IsRootOfNativeAnonymousSubtree() ||
      GetScrollStyles().IsHiddenInBothDirections()) {
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
  NS_QUERYFRAME_ENTRY(nsHTMLScrollFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

//----------nsXULScrollFrame-------------------------------------------

nsXULScrollFrame* NS_NewXULScrollFrame(PresShell* aPresShell,
                                       ComputedStyle* aStyle, bool aIsRoot) {
  return new (aPresShell)
      nsXULScrollFrame(aStyle, aPresShell->GetPresContext(), aIsRoot);
}

NS_IMPL_FRAMEARENA_HELPERS(nsXULScrollFrame)

nsXULScrollFrame::nsXULScrollFrame(ComputedStyle* aStyle,
                                   nsPresContext* aPresContext, bool aIsRoot)
    : nsBoxFrame(aStyle, aPresContext, kClassID, aIsRoot),
      mHelper(ALLOW_THIS_IN_INITIALIZER_LIST(this), aIsRoot) {
  SetXULLayoutManager(nullptr);
}

void nsXULScrollFrame::ScrollbarActivityStarted() const {
  if (mHelper.mScrollbarActivity) {
    mHelper.mScrollbarActivity->ActivityStarted();
  }
}

void nsXULScrollFrame::ScrollbarActivityStopped() const {
  if (mHelper.mScrollbarActivity) {
    mHelper.mScrollbarActivity->ActivityStopped();
  }
}

nsMargin ScrollFrameHelper::GetDesiredScrollbarSizes(nsBoxLayoutState* aState) {
  NS_ASSERTION(aState && aState->GetRenderingContext(),
               "Must have rendering context in layout state for size "
               "computations");

  nsMargin result(0, 0, 0, 0);

  if (mVScrollbarBox) {
    nsSize size = mVScrollbarBox->GetXULPrefSize(*aState);
    nsIFrame::AddXULMargin(mVScrollbarBox, size);
    if (IsScrollbarOnRight())
      result.left = size.width;
    else
      result.right = size.width;
  }

  if (mHScrollbarBox) {
    nsSize size = mHScrollbarBox->GetXULPrefSize(*aState);
    nsIFrame::AddXULMargin(mHScrollbarBox, size);
    // We don't currently support any scripts that would require a scrollbar
    // at the top. (Are there any?)
    result.bottom = size.height;
  }

  return result;
}

nscoord nsIScrollableFrame::GetNondisappearingScrollbarWidth(nsPresContext* aPc,
                                                             WritingMode aWM) {
  // We use this to size the combobox dropdown button. For that, we need to have
  // the proper big, non-overlay scrollbar size, regardless of whether we're
  // using e.g. scrollbar-width: thin, or overlay scrollbars.
  auto sizes = aPc->Theme()->GetScrollbarSizes(aPc, StyleScrollbarWidth::Auto,
                                               nsITheme::Overlay::No);
  return aPc->DevPixelsToAppUnits(aWM.IsVertical() ? sizes.mHorizontal
                                                   : sizes.mVertical);
}

void ScrollFrameHelper::HandleScrollbarStyleSwitching() {
  // Check if we switched between scrollbar styles.
  if (mScrollbarActivity && !UsesOverlayScrollbars()) {
    mScrollbarActivity->Destroy();
    mScrollbarActivity = nullptr;
  } else if (!mScrollbarActivity && UsesOverlayScrollbars()) {
    mScrollbarActivity = new ScrollbarActivity(do_QueryFrame(mOuter));
  }
}

#if defined(MOZ_WIDGET_ANDROID)
static bool IsFocused(nsIContent* aContent) {
  // Some content elements, like the GetContent() of a scroll frame
  // for a text input field, are inside anonymous subtrees, but the focus
  // manager always reports a non-anonymous element as the focused one, so
  // walk up the tree until we reach a non-anonymous element.
  while (aContent && aContent->IsInNativeAnonymousSubtree()) {
    aContent = aContent->GetParent();
  }

  return aContent ? nsContentUtils::IsFocusedContent(aContent) : false;
}
#endif

void ScrollFrameHelper::SetScrollableByAPZ(bool aScrollable) {
  mScrollableByAPZ = aScrollable;
}

void ScrollFrameHelper::SetZoomableByAPZ(bool aZoomable) {
  if (!nsLayoutUtils::UsesAsyncScrolling(mOuter)) {
    // If APZ is disabled on this window, then we're never actually going to
    // do any zooming. So we don't need to do any of the setup for it. Note
    // that this function gets called from ZoomConstraintsClient even if APZ
    // is disabled to indicate the zoomability of content.
    aZoomable = false;
  }
  if (mZoomableByAPZ != aZoomable) {
    // We might be changing the result of DecideScrollableLayer() so schedule a
    // paint to make sure we pick up the result of that change.
    mZoomableByAPZ = aZoomable;
    mOuter->SchedulePaint();
  }
}

void ScrollFrameHelper::SetHasOutOfFlowContentInsideFilter() {
  mHasOutOfFlowContentInsideFilter = true;
}

bool ScrollFrameHelper::WantAsyncScroll() const {
  ScrollStyles styles = GetScrollStylesFromFrame();
  nscoord oneDevPixel =
      GetScrolledFrame()->PresContext()->AppUnitsPerDevPixel();
  nsRect scrollRange = GetLayoutScrollRange();

  // If the page has a visual viewport size that's different from
  // the layout viewport size at the current zoom level, we need to be
  // able to scroll the visual viewport inside the layout viewport
  // even if the page is not zoomable.
  if (!GetVisualScrollRange().IsEqualInterior(scrollRange)) {
    return true;
  }

  bool isVScrollable = (scrollRange.height >= oneDevPixel) &&
                       (styles.mVertical != StyleOverflow::Hidden);
  bool isHScrollable = (scrollRange.width >= oneDevPixel) &&
                       (styles.mHorizontal != StyleOverflow::Hidden);

#if defined(MOZ_WIDGET_ANDROID)
  // Mobile platforms need focus to scroll text inputs.
  bool canScrollWithoutScrollbars =
      !IsForTextControlWithNoScrollbars() || IsFocused(mOuter->GetContent());
#else
  bool canScrollWithoutScrollbars = true;
#endif

  // The check for scroll bars was added in bug 825692 to prevent layerization
  // of text inputs for performance reasons.
  bool isVAsyncScrollable =
      isVScrollable && (mVScrollbarBox || canScrollWithoutScrollbars);
  bool isHAsyncScrollable =
      isHScrollable && (mHScrollbarBox || canScrollWithoutScrollbars);
  return isVAsyncScrollable || isHAsyncScrollable;
}

static nsRect GetOnePixelRangeAroundPoint(const nsPoint& aPoint,
                                          bool aIsHorizontal) {
  nsRect allowedRange(aPoint, nsSize());
  nscoord halfPixel = nsPresContext::CSSPixelsToAppUnits(0.5f);
  if (aIsHorizontal) {
    allowedRange.x = aPoint.x - halfPixel;
    allowedRange.width = halfPixel * 2 - 1;
  } else {
    allowedRange.y = aPoint.y - halfPixel;
    allowedRange.height = halfPixel * 2 - 1;
  }
  return allowedRange;
}

void ScrollFrameHelper::ScrollByPage(nsScrollbarFrame* aScrollbar,
                                     int32_t aDirection,
                                     ScrollSnapFlags aSnapFlags) {
  ScrollByUnit(aScrollbar, ScrollMode::Smooth, aDirection, ScrollUnit::PAGES,
               aSnapFlags);
}

void ScrollFrameHelper::ScrollByWhole(nsScrollbarFrame* aScrollbar,
                                      int32_t aDirection,
                                      ScrollSnapFlags aSnapFlags) {
  ScrollByUnit(aScrollbar, ScrollMode::Instant, aDirection, ScrollUnit::WHOLE,
               aSnapFlags);
}

void ScrollFrameHelper::ScrollByLine(nsScrollbarFrame* aScrollbar,
                                     int32_t aDirection,
                                     ScrollSnapFlags aSnapFlags) {
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
  ScrollBy(delta, ScrollUnit::LINES, ScrollMode::Smooth, &overflow,
           ScrollOrigin::Other, nsIScrollableFrame::NOT_MOMENTUM, aSnapFlags);
}

void ScrollFrameHelper::RepeatButtonScroll(nsScrollbarFrame* aScrollbar) {
  aScrollbar->MoveToNewPosition(nsScrollbarFrame::ImplementsScrollByUnit::Yes);
}

void ScrollFrameHelper::ThumbMoved(nsScrollbarFrame* aScrollbar,
                                   nscoord aOldPos, nscoord aNewPos) {
  MOZ_ASSERT(aScrollbar != nullptr);
  bool isHorizontal = aScrollbar->IsXULHorizontal();
  nsPoint current = GetScrollPosition();
  nsPoint dest = current;
  if (isHorizontal) {
    dest.x = IsPhysicalLTR() ? aNewPos : aNewPos - GetLayoutScrollRange().width;
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

  ScrollTo(dest, ScrollMode::Instant, ScrollOrigin::Other, &allowedRange);
}

void ScrollFrameHelper::ScrollbarReleased(nsScrollbarFrame* aScrollbar) {
  // Scrollbar scrolling does not result in fling gestures, clear any
  // accumulated velocity
  mVelocityQueue.Reset();

  // Perform scroll snapping, if needed.  Scrollbar movement uses the same
  // smooth scrolling animation as keyboard scrolling.
  ScrollSnap(mDestination, ScrollMode::Smooth);
}

void ScrollFrameHelper::ScrollByUnit(nsScrollbarFrame* aScrollbar,
                                     ScrollMode aMode, int32_t aDirection,
                                     ScrollUnit aUnit,
                                     ScrollSnapFlags aSnapFlags) {
  MOZ_ASSERT(aScrollbar != nullptr);
  bool isHorizontal = aScrollbar->IsXULHorizontal();
  nsIntPoint delta;
  if (isHorizontal) {
    delta.x = aDirection;
  } else {
    delta.y = aDirection;
  }
  nsIntPoint overflow;
  ScrollBy(delta, aUnit, aMode, &overflow, ScrollOrigin::Other,
           nsIScrollableFrame::NOT_MOMENTUM, aSnapFlags);
}

nsresult nsXULScrollFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  return mHelper.CreateAnonymousContent(aElements);
}

void nsXULScrollFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
  mHelper.AppendAnonymousContentTo(aElements, aFilter);
}

void nsXULScrollFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                   PostDestroyData& aPostDestroyData) {
  mHelper.Destroy(aPostDestroyData);
  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void nsXULScrollFrame::SetInitialChildList(ChildListID aListID,
                                           nsFrameList& aChildList) {
  nsBoxFrame::SetInitialChildList(aListID, aChildList);
  if (aListID == kPrincipalList) {
    mHelper.ReloadChildFrames();
  }
}

void nsXULScrollFrame::AppendFrames(ChildListID aListID,
                                    nsFrameList& aFrameList) {
  nsBoxFrame::AppendFrames(aListID, aFrameList);
  mHelper.ReloadChildFrames();
}

void nsXULScrollFrame::InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                                    const nsLineList::iterator* aPrevFrameLine,
                                    nsFrameList& aFrameList) {
  nsBoxFrame::InsertFrames(aListID, aPrevFrame, aPrevFrameLine, aFrameList);
  mHelper.ReloadChildFrames();
}

void nsXULScrollFrame::RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) {
  nsBoxFrame::RemoveFrame(aListID, aOldFrame);
  mHelper.ReloadChildFrames();
}

nsresult nsXULScrollFrame::GetXULPadding(nsMargin& aMargin) {
  aMargin.SizeTo(0, 0, 0, 0);
  return NS_OK;
}

nscoord nsXULScrollFrame::GetXULBoxAscent(nsBoxLayoutState& aState) {
  if (!mHelper.mScrolledFrame) return 0;

  nscoord ascent = mHelper.mScrolledFrame->GetXULBoxAscent(aState);
  nsMargin m(0, 0, 0, 0);
  GetXULBorderAndPadding(m);
  ascent += m.top;
  GetXULMargin(m);
  ascent += m.top;

  return ascent;
}

nsSize nsXULScrollFrame::GetXULPrefSize(nsBoxLayoutState& aState) {
  nsSize pref = mHelper.mScrolledFrame->GetXULPrefSize(aState);

  ScrollStyles styles = GetScrollStyles();

  // scrolled frames don't have their own margins

  if (mHelper.mVScrollbarBox && styles.mVertical == StyleOverflow::Scroll) {
    nsSize vSize = mHelper.mVScrollbarBox->GetXULPrefSize(aState);
    nsIFrame::AddXULMargin(mHelper.mVScrollbarBox, vSize);
    pref.width += vSize.width;
  }

  if (mHelper.mHScrollbarBox && styles.mHorizontal == StyleOverflow::Scroll) {
    nsSize hSize = mHelper.mHScrollbarBox->GetXULPrefSize(aState);
    nsIFrame::AddXULMargin(mHelper.mHScrollbarBox, hSize);
    pref.height += hSize.height;
  }

  AddXULBorderAndPadding(pref);
  bool widthSet, heightSet;
  nsIFrame::AddXULPrefSize(this, pref, widthSet, heightSet);
  return pref;
}

nsSize nsXULScrollFrame::GetXULMinSize(nsBoxLayoutState& aState) {
  nsSize min = mHelper.mScrolledFrame->GetXULMinSizeForScrollArea(aState);

  ScrollStyles styles = GetScrollStyles();

  if (mHelper.mVScrollbarBox && styles.mVertical == StyleOverflow::Scroll) {
    nsSize vSize = mHelper.mVScrollbarBox->GetXULMinSize(aState);
    AddXULMargin(mHelper.mVScrollbarBox, vSize);
    min.width += vSize.width;
    if (min.height < vSize.height) min.height = vSize.height;
  }

  if (mHelper.mHScrollbarBox && styles.mHorizontal == StyleOverflow::Scroll) {
    nsSize hSize = mHelper.mHScrollbarBox->GetXULMinSize(aState);
    AddXULMargin(mHelper.mHScrollbarBox, hSize);
    min.height += hSize.height;
    if (min.width < hSize.width) min.width = hSize.width;
  }

  AddXULBorderAndPadding(min);
  bool widthSet, heightSet;
  nsIFrame::AddXULMinSize(this, min, widthSet, heightSet);
  return min;
}

nsSize nsXULScrollFrame::GetXULMaxSize(nsBoxLayoutState& aState) {
  nsSize maxSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  AddXULBorderAndPadding(maxSize);
  bool widthSet, heightSet;
  nsIFrame::AddXULMaxSize(this, maxSize, widthSet, heightSet);
  return maxSize;
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsXULScrollFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"XULScroll"_ns, aResult);
}
#endif

NS_IMETHODIMP
nsXULScrollFrame::DoXULLayout(nsBoxLayoutState& aState) {
  if (nsIFrame* hScrollbarBox = GetScrollbarBox(false)) {
    nsScrollbarFrame* scrollbar = do_QueryFrame(hScrollbarBox);
    scrollbar->SetScrollbarMediatorContent(mContent);
  }
  if (nsIFrame* vScrollbarBox = GetScrollbarBox(true)) {
    nsScrollbarFrame* scrollbar = do_QueryFrame(vScrollbarBox);
    scrollbar->SetScrollbarMediatorContent(mContent);
  }
  ReflowChildFlags flags = aState.LayoutFlags();
  nsresult rv = XULLayout(aState);
  aState.SetLayoutFlags(flags);
  return rv;
}

NS_QUERYFRAME_HEAD(nsXULScrollFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsIScrollableFrame)
  NS_QUERYFRAME_ENTRY(nsIStatefulFrame)
  NS_QUERYFRAME_ENTRY(nsIScrollbarMediator)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

//-------------------- Helper ----------------------

// AsyncSmoothMSDScroll has ref counting.
class ScrollFrameHelper::AsyncSmoothMSDScroll final
    : public nsARefreshObserver {
 public:
  AsyncSmoothMSDScroll(const nsPoint& aInitialPosition,
                       const nsPoint& aInitialDestination,
                       const nsSize& aInitialVelocity, const nsRect& aRange,
                       const mozilla::TimeStamp& aStartTime,
                       nsPresContext* aPresContext)
      : mXAxisModel(aInitialPosition.x, aInitialDestination.x,
                    aInitialVelocity.width,
                    StaticPrefs::layout_css_scroll_behavior_spring_constant(),
                    StaticPrefs::layout_css_scroll_behavior_damping_ratio()),
        mYAxisModel(aInitialPosition.y, aInitialDestination.y,
                    aInitialVelocity.height,
                    StaticPrefs::layout_css_scroll_behavior_spring_constant(),
                    StaticPrefs::layout_css_scroll_behavior_damping_ratio()),
        mRange(aRange),
        mLastRefreshTime(aStartTime),
        mCallee(nullptr),
        mOneDevicePixelInAppUnits(aPresContext->DevPixelsToAppUnits(1)) {
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
    return nsPoint(NSToCoordRound(mXAxisModel.GetPosition()),
                   NSToCoordRound(mYAxisModel.GetPosition()));
  }

  void SetDestination(const nsPoint& aDestination) {
    mXAxisModel.SetDestination(static_cast<int32_t>(aDestination.x));
    mYAxisModel.SetDestination(static_cast<int32_t>(aDestination.y));
  }

  void SetRange(const nsRect& aRange) { mRange = aRange; }

  nsRect GetRange() { return mRange; }

  void Simulate(const TimeDuration& aDeltaTime) {
    mXAxisModel.Simulate(aDeltaTime);
    mYAxisModel.Simulate(aDeltaTime);

    nsPoint desired = GetPosition();
    nsPoint clamped = mRange.ClampPoint(desired);
    if (desired.x != clamped.x) {
      // The scroll has hit the "wall" at the left or right edge of the allowed
      // scroll range.
      // Absorb the impact to avoid bounceback effect.
      mXAxisModel.SetVelocity(0.0);
      mXAxisModel.SetPosition(clamped.x);
    }

    if (desired.y != clamped.y) {
      // The scroll has hit the "wall" at the left or right edge of the allowed
      // scroll range.
      // Absorb the impact to avoid bounceback effect.
      mYAxisModel.SetVelocity(0.0);
      mYAxisModel.SetPosition(clamped.y);
    }
  }

  bool IsFinished() {
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
   */
  void SetRefreshObserver(ScrollFrameHelper* aCallee) {
    NS_ASSERTION(aCallee && !mCallee,
                 "AsyncSmoothMSDScroll::SetRefreshObserver - Invalid usage.");

    RefreshDriver(aCallee)->AddRefreshObserver(this, FlushType::Style,
                                               "Smooth scroll (MSD) animation");
    mCallee = aCallee;
  }

  /**
   * The mCallee holds a strong ref to us since the refresh driver doesn't.
   * Our dtor and mCallee's Destroy() method both call RemoveObserver() -
   * whichever comes first removes us from the refresh driver.
   */
  void RemoveObserver() {
    if (mCallee) {
      RefreshDriver(mCallee)->RemoveRefreshObserver(this, FlushType::Style);
      mCallee = nullptr;
    }
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

  mozilla::layers::AxisPhysicsMSDModel mXAxisModel, mYAxisModel;
  nsRect mRange;
  mozilla::TimeStamp mLastRefreshTime;
  ScrollFrameHelper* mCallee;
  nscoord mOneDevicePixelInAppUnits;
};

// AsyncScroll has ref counting.
class ScrollFrameHelper::AsyncScroll final : public nsARefreshObserver {
 public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  explicit AsyncScroll()
      : mOrigin(ScrollOrigin::NotSpecified), mCallee(nullptr) {
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
  void InitSmoothScroll(TimeStamp aTime, nsPoint aInitialPosition,
                        nsPoint aDestination, ScrollOrigin aOrigin,
                        const nsRect& aRange, const nsSize& aCurrentVelocity);
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
  ScrollOrigin mOrigin;

  // Allowed destination positions around mDestination
  nsRect mRange;

 private:
  void InitPreferences(TimeStamp aTime, nsAtom* aOrigin);

  UniquePtr<ScrollAnimationPhysics> mAnimationPhysics;

  // The next section is observer/callback management
  // Bodies of WillRefresh and RefreshDriver contain ScrollFrameHelper specific
  // code.
 public:
  NS_INLINE_DECL_REFCOUNTING(AsyncScroll, override)

  /*
   * Set a refresh observer for smooth scroll iterations (and start observing).
   * Should be used at most once during the lifetime of this object.
   */
  void SetRefreshObserver(ScrollFrameHelper* aCallee) {
    NS_ASSERTION(aCallee && !mCallee,
                 "AsyncScroll::SetRefreshObserver - Invalid usage.");

    RefreshDriver(aCallee)->AddRefreshObserver(this, FlushType::Style,
                                               "Smooth scroll animation");
    mCallee = aCallee;
    PresShell* presShell = mCallee->mOuter->PresShell();
    MOZ_ASSERT(presShell);
    presShell->SuppressDisplayport(true);
  }

  virtual void WillRefresh(mozilla::TimeStamp aTime) override {
    // The callback may release "this".
    // We don't access members after returning, so no need for KungFuDeathGrip.
    ScrollFrameHelper::AsyncScrollCallback(mCallee, aTime);
  }

  /**
   * The mCallee holds a strong ref to us since the refresh driver doesn't.
   * Our dtor and mCallee's Destroy() method both call RemoveObserver() -
   * whichever comes first removes us from the refresh driver.
   */
  void RemoveObserver() {
    if (mCallee) {
      RefreshDriver(mCallee)->RemoveRefreshObserver(this, FlushType::Style);
      PresShell* presShell = mCallee->mOuter->PresShell();
      MOZ_ASSERT(presShell);
      presShell->SuppressDisplayport(false);
      mCallee = nullptr;
    }
  }

 private:
  ScrollFrameHelper* mCallee;

  nsRefreshDriver* RefreshDriver(ScrollFrameHelper* aCallee) {
    return aCallee->mOuter->PresContext()->RefreshDriver();
  }
};

void ScrollFrameHelper::AsyncScroll::InitSmoothScroll(
    TimeStamp aTime, nsPoint aInitialPosition, nsPoint aDestination,
    ScrollOrigin aOrigin, const nsRect& aRange,
    const nsSize& aCurrentVelocity) {
  switch (aOrigin) {
    case ScrollOrigin::NotSpecified:
    case ScrollOrigin::Restore:
    case ScrollOrigin::Relative:
      // We don't have special prefs for "restore", just treat it as "other".
      // "restore" scrolls are (for now) always instant anyway so unless
      // something changes we should never have aOrigin ==
      // ScrollOrigin::Restore here.
      aOrigin = ScrollOrigin::Other;
      break;
    case ScrollOrigin::Apz:
      // Likewise we should never get APZ-triggered scrolls here, and if that
      // changes something is likely broken somewhere.
      MOZ_ASSERT(false);
      break;
    default:
      break;
  };

  // Read preferences only on first iteration or for a different event origin.
  if (!mAnimationPhysics || aOrigin != mOrigin) {
    mOrigin = aOrigin;
    if (StaticPrefs::general_smoothScroll_msdPhysics_enabled()) {
      mAnimationPhysics =
          MakeUnique<ScrollAnimationMSDPhysics>(aInitialPosition);
    } else {
      ScrollAnimationBezierPhysicsSettings settings =
          layers::apz::ComputeBezierAnimationSettingsForOrigin(mOrigin);
      mAnimationPhysics =
          MakeUnique<ScrollAnimationBezierPhysics>(aInitialPosition, settings);
    }
  }

  mRange = aRange;

  mAnimationPhysics->Update(aTime, aDestination, aCurrentVelocity);
}

bool ScrollFrameHelper::IsSmoothScrollingEnabled() {
  return StaticPrefs::general_smoothScroll();
}

class ScrollFrameActivityTracker final
    : public nsExpirationTracker<ScrollFrameHelper, 4> {
 public:
  // Wait for 3-4s between scrolls before we remove our layers.
  // That's 4 generations of 1s each.
  enum { TIMEOUT_MS = 1000 };
  explicit ScrollFrameActivityTracker(nsIEventTarget* aEventTarget)
      : nsExpirationTracker<ScrollFrameHelper, 4>(
            TIMEOUT_MS, "ScrollFrameActivityTracker", aEventTarget) {}
  ~ScrollFrameActivityTracker() { AgeAllGenerations(); }

  virtual void NotifyExpired(ScrollFrameHelper* aObject) override {
    RemoveObject(aObject);
    aObject->MarkNotRecentlyScrolled();
  }
};

static StaticAutoPtr<ScrollFrameActivityTracker> gScrollFrameActivityTracker;

ScrollFrameHelper::ScrollFrameHelper(nsContainerFrame* aOuter, bool aIsRoot)
    : mHScrollbarBox(nullptr),
      mVScrollbarBox(nullptr),
      mScrolledFrame(nullptr),
      mScrollCornerBox(nullptr),
      mResizerBox(nullptr),
      mOuter(aOuter),
      mReferenceFrameDuringPainting(nullptr),
      mAsyncScroll(nullptr),
      mAsyncSmoothMSDScroll(nullptr),
      mLastScrollOrigin(ScrollOrigin::None),
      mDestination(0, 0),
      mRestorePos(-1, -1),
      mLastPos(-1, -1),
      mApzScrollPos(0, 0),
      mLastUpdateFramesPos(-1, -1),
      mDisplayPortAtLastFrameUpdate(),
      mScrollParentID(mozilla::layers::ScrollableLayerGuid::NULL_SCROLL_ID),
      mAnchor(this),
      mCurrentAPZScrollAnimationType(APZScrollAnimationType::No),
      mAllowScrollOriginDowngrade(false),
      mHadDisplayPortAtLastFrameUpdate(false),
      mHasVerticalScrollbar(false),
      mHasHorizontalScrollbar(false),
      mOnlyNeedVScrollbarToScrollVVInsideLV(false),
      mOnlyNeedHScrollbarToScrollVVInsideLV(false),
      mFrameIsUpdatingScrollbar(false),
      mDidHistoryRestore(false),
      mIsRoot(aIsRoot),
      mSuppressScrollbarUpdate(false),
      mSkippedScrollbarLayout(false),
      mHadNonInitialReflow(false),
      mFirstReflow(true),
      mHorizontalOverflow(false),
      mVerticalOverflow(false),
      mPostedReflowCallback(false),
      mMayHaveDirtyFixedChildren(false),
      mUpdateScrollbarAttributes(false),
      mHasBeenScrolledRecently(false),
      mWillBuildScrollableLayer(false),
      mIsParentToActiveScrollFrames(false),
      mHasBeenScrolled(false),
      mIgnoreMomentumScroll(false),
      mTransformingByAPZ(false),
      mScrollableByAPZ(false),
      mZoomableByAPZ(false),
      mHasOutOfFlowContentInsideFilter(false),
      mSuppressScrollbarRepaints(false),
      mIsUsingMinimumScaleSize(false),
      mMinimumScaleSizeChanged(false),
      mProcessingScrollEvent(false),
      mApzAnimationRequested(false),
      mReclampVVOffsetInReflowFinished(false),
      mMayScheduleScrollAnimations(false),
#ifdef MOZ_WIDGET_ANDROID
      mHasVerticalOverflowForDynamicToolbar(false),
#endif
      mVelocityQueue(aOuter->PresContext()) {
  AppendScrollUpdate(ScrollPositionUpdate::NewScrollframe(nsPoint()));

  if (UsesOverlayScrollbars()) {
    mScrollbarActivity = new ScrollbarActivity(do_QueryFrame(aOuter));
  }

  if (mIsRoot) {
    mZoomableByAPZ = mOuter->PresShell()->GetZoomableByAPZ();
  }
}

ScrollFrameHelper::~ScrollFrameHelper() {
  if (mScrollEvent) {
    mScrollEvent->Revoke();
  }
  if (mScrollEndEvent) {
    mScrollEndEvent->Revoke();
  }
}

/*
 * Callback function from AsyncSmoothMSDScroll, used in
 * ScrollFrameHelper::ScrollTo
 */
void ScrollFrameHelper::AsyncSmoothMSDScrollCallback(
    ScrollFrameHelper* aInstance, mozilla::TimeDuration aDeltaTime) {
  NS_ASSERTION(aInstance != nullptr, "aInstance must not be null");
  NS_ASSERTION(aInstance->mAsyncSmoothMSDScroll,
               "Did not expect AsyncSmoothMSDScrollCallback without an active "
               "MSD scroll.");

  nsRect range = aInstance->mAsyncSmoothMSDScroll->GetRange();
  aInstance->mAsyncSmoothMSDScroll->Simulate(aDeltaTime);

  if (!aInstance->mAsyncSmoothMSDScroll->IsFinished()) {
    nsPoint destination = aInstance->mAsyncSmoothMSDScroll->GetPosition();
    // Allow this scroll operation to land on any pixel boundary within the
    // allowed scroll range for this frame.
    // If the MSD is under-dampened or the destination is changed rapidly,
    // it is expected (and desired) that the scrolling may overshoot.
    nsRect intermediateRange = nsRect(destination, nsSize()).UnionEdges(range);
    aInstance->ScrollToImpl(destination, intermediateRange);
    // 'aInstance' might be destroyed here
    return;
  }

  aInstance->CompleteAsyncScroll(range);
}

/*
 * Callback function from AsyncScroll, used in ScrollFrameHelper::ScrollTo
 */
void ScrollFrameHelper::AsyncScrollCallback(ScrollFrameHelper* aInstance,
                                            mozilla::TimeStamp aTime) {
  MOZ_ASSERT(aInstance != nullptr, "aInstance must not be null");
  MOZ_ASSERT(
      aInstance->mAsyncScroll,
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

void ScrollFrameHelper::CompleteAsyncScroll(const nsRect& aRange,
                                            ScrollOrigin aOrigin) {
  // Apply desired destination range since this is the last step of scrolling.
  RemoveObservers();
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

bool ScrollFrameHelper::HasBgAttachmentLocal() const {
  const nsStyleBackground* bg = mOuter->StyleBackground();
  return bg->HasLocalBackground();
}

void ScrollFrameHelper::ScrollTo(nsPoint aScrollPosition, ScrollMode aMode,
                                 ScrollOrigin aOrigin, const nsRect* aRange,
                                 ScrollSnapFlags aSnapFlags,
                                 ScrollTriggeredByScript aTriggeredByScript) {
  if (aOrigin == ScrollOrigin::NotSpecified) {
    aOrigin = ScrollOrigin::Other;
  }
  ScrollToWithOrigin(aScrollPosition, aMode, aOrigin, aRange, aSnapFlags,
                     aTriggeredByScript);
}

void ScrollFrameHelper::ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                                          ScrollMode aMode) {
  CSSIntPoint currentCSSPixels = GetScrollPositionCSSPixels();
  // Transmogrify this scroll to a relative one if there's any on-going
  // animation in APZ triggered by __user__.
  // Bug 1740164: We will apply it for cases there's no animation in APZ.

  auto scrollAnimationState = ScrollAnimationState();
  bool isScrollAnimating =
      scrollAnimationState.contains(AnimationState::MainThread) ||
      scrollAnimationState.contains(AnimationState::APZPending) ||
      scrollAnimationState.contains(AnimationState::APZRequested);
  if (mCurrentAPZScrollAnimationType ==
          APZScrollAnimationType::TriggeredByUserInput &&
      !isScrollAnimating) {
    CSSIntPoint delta = aScrollPosition - currentCSSPixels;
    // This transmogrification need to be an intended end position scroll
    // operation.
    ScrollByCSSPixels(delta, aMode, ScrollSnapFlags::IntendedEndPosition);
    return;
  }

  nscoord halfPixel = nsPresContext::CSSPixelsToAppUnits(0.5f);
  nsPoint pt = CSSPoint::ToAppUnits(aScrollPosition);
  nsRect range(pt.x - halfPixel, pt.y - halfPixel, 2 * halfPixel - 1,
               2 * halfPixel - 1);
  // XXX I don't think the following blocks are needed anymore, now that
  // ScrollToImpl simply tries to scroll an integer number of layer
  // pixels from the current position
  nsPoint current = GetScrollPosition();
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
  ScrollTo(pt, aMode, ScrollOrigin::Other, &range,
           // This ScrollToCSSPixels is used for Element.scrollTo,
           // Element.scrollTop, Element.scrollLeft and for Window.scrollTo.
           ScrollSnapFlags::IntendedEndPosition, ScrollTriggeredByScript::Yes);
  // 'this' might be destroyed here
}

void ScrollFrameHelper::ScrollToCSSPixelsForApz(
    const CSSPoint& aScrollPosition) {
  nsPoint pt = CSSPoint::ToAppUnits(aScrollPosition);
  nscoord halfRange = nsPresContext::CSSPixelsToAppUnits(1000);
  nsRect range(pt.x - halfRange, pt.y - halfRange, 2 * halfRange - 1,
               2 * halfRange - 1);
  ScrollToWithOrigin(pt, ScrollMode::Instant, ScrollOrigin::Apz, &range);
  // 'this' might be destroyed here
}

CSSIntPoint ScrollFrameHelper::GetScrollPositionCSSPixels() {
  return CSSIntPoint::FromAppUnitsRounded(GetScrollPosition());
}

/*
 * this method wraps calls to ScrollToImpl(), either in one shot or
 * incrementally, based on the setting of the smoothness scroll pref
 */
void ScrollFrameHelper::ScrollToWithOrigin(
    nsPoint aScrollPosition, ScrollMode aMode, ScrollOrigin aOrigin,
    const nsRect* aRange, ScrollSnapFlags aSnapFlags,
    ScrollTriggeredByScript aTriggeredByScript) {
  // None is never a valid scroll origin to be passed in.
  MOZ_ASSERT(aOrigin != ScrollOrigin::None);

  if (aOrigin != ScrollOrigin::Restore) {
    // If we're doing a non-restore scroll, we don't want to later
    // override it by restoring our saved scroll position.
    SCROLLRESTORE_LOG("%p: Clearing mRestorePos (cur=%s, dst=%s)\n", this,
                      ToString(GetScrollPosition()).c_str(),
                      ToString(aScrollPosition).c_str());
    mRestorePos.x = mRestorePos.y = -1;
  }

  bool willSnap = false;
  if (aSnapFlags != ScrollSnapFlags::Disabled) {
    willSnap = GetSnapPointForDestination(ScrollUnit::DEVICE_PIXELS, aSnapFlags,
                                          mDestination, aScrollPosition);
  }

  nsRect scrollRange = GetLayoutScrollRange();
  mDestination = scrollRange.ClampPoint(aScrollPosition);
  if (mDestination != aScrollPosition && aOrigin == ScrollOrigin::Restore &&
      GetPageLoadingState() != LoadingState::Loading) {
    // If we're doing a restore but the scroll position is clamped, promote
    // the origin from one that APZ can clobber to one that it can't clobber.
    aOrigin = ScrollOrigin::Other;
  }

  nsRect range =
      aRange && !willSnap ? *aRange : nsRect(aScrollPosition, nsSize(0, 0));

  if (aMode == ScrollMode::Instant) {
    // Asynchronous scrolling is not allowed, so we'll kill any existing
    // async-scrolling process and do an instant scroll.
    CompleteAsyncScroll(range, aOrigin);
    mApzSmoothScrollDestination = Nothing();
    return;
  }

  if (aMode != ScrollMode::SmoothMsd) {
    // If we get a non-smooth-scroll, reset the cached APZ scroll destination,
    // so that we know to process the next smooth-scroll destined for APZ.
    mApzSmoothScrollDestination = Nothing();
  }

  nsPresContext* presContext = mOuter->PresContext();
  TimeStamp now =
      presContext->RefreshDriver()->IsTestControllingRefreshesEnabled()
          ? presContext->RefreshDriver()->MostRecentRefresh()
          : TimeStamp::Now();

  nsSize currentVelocity(0, 0);

  if (aMode == ScrollMode::SmoothMsd) {
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
        ApzSmoothScrollTo(mDestination, aOrigin, aTriggeredByScript);
        return;
      }

      mAsyncSmoothMSDScroll = new AsyncSmoothMSDScroll(
          GetScrollPosition(), mDestination, currentVelocity,
          GetLayoutScrollRange(), now, presContext);

      mAsyncSmoothMSDScroll->SetRefreshObserver(this);
    } else {
      // A previous smooth MSD scroll is still in progress, so we just need to
      // update its range and destination.
      mAsyncSmoothMSDScroll->SetRange(GetLayoutScrollRange());
      mAsyncSmoothMSDScroll->SetDestination(mDestination);
    }

    return;
  }

  if (mAsyncSmoothMSDScroll) {
    currentVelocity = mAsyncSmoothMSDScroll->GetVelocity();
    mAsyncSmoothMSDScroll = nullptr;
  }

  if (!mAsyncScroll) {
    mAsyncScroll = new AsyncScroll();
    mAsyncScroll->SetRefreshObserver(this);
  }

  const bool isSmoothScroll =
      aMode == ScrollMode::Smooth && IsSmoothScrollingEnabled();
  if (isSmoothScroll) {
    mAsyncScroll->InitSmoothScroll(now, GetScrollPosition(), mDestination,
                                   aOrigin, range, currentVelocity);
  } else {
    mAsyncScroll->Init(range);
  }
}

// We can't use nsContainerFrame::PositionChildViews here because
// we don't want to invalidate views that have moved.
static void AdjustViews(nsIFrame* aFrame) {
  nsView* view = aFrame->GetView();
  if (view) {
    nsPoint pt;
    aFrame->GetParent()->GetClosestView(&pt);
    pt += aFrame->GetPosition();
    view->SetPosition(pt.x, pt.y);

    return;
  }

  if (!aFrame->HasAnyStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    return;
  }

  // Call AdjustViews recursively for all child frames except the popup list as
  // the views for popups are not scrolled.
  for (const auto& [list, listID] : aFrame->ChildLists()) {
    if (listID == nsIFrame::kPopupList) {
      continue;
    }
    for (nsIFrame* child : list) {
      AdjustViews(child);
    }
  }
}

void ScrollFrameHelper::MarkScrollbarsDirtyForReflow() const {
  PresShell* presShell = mOuter->PresShell();
  if (mVScrollbarBox) {
    presShell->FrameNeedsReflow(mVScrollbarBox, IntrinsicDirty::StyleChange,
                                NS_FRAME_IS_DIRTY);
  }
  if (mHScrollbarBox) {
    presShell->FrameNeedsReflow(mHScrollbarBox, IntrinsicDirty::StyleChange,
                                NS_FRAME_IS_DIRTY);
  }
}

void ScrollFrameHelper::InvalidateScrollbars() const {
  if (mHScrollbarBox) {
    mHScrollbarBox->InvalidateFrameSubtree();
  }
  if (mVScrollbarBox) {
    mVScrollbarBox->InvalidateFrameSubtree();
  }
}

bool ScrollFrameHelper::IsAlwaysActive() const {
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
  ScrollStyles styles = GetScrollStylesFromFrame();
  return (styles.mHorizontal != StyleOverflow::Hidden &&
          styles.mVertical != StyleOverflow::Hidden);
}

static void RemoveDisplayPortCallback(nsITimer* aTimer, void* aClosure) {
  ScrollFrameHelper* helper = static_cast<ScrollFrameHelper*>(aClosure);

  // This function only ever gets called from the expiry timer, so it must
  // be non-null here. Set it to null here so that we don't keep resetting
  // it unnecessarily in MarkRecentlyScrolled().
  MOZ_ASSERT(helper->mDisplayPortExpiryTimer);
  helper->mDisplayPortExpiryTimer = nullptr;

  if (!helper->AllowDisplayPortExpiration() ||
      helper->mIsParentToActiveScrollFrames) {
    // If this is a scroll parent for some other scrollable frame, don't
    // expire the displayport because it would break scroll handoff. Once the
    // descendant scrollframes have their displayports expired, they will
    // trigger the displayport expiration on this scrollframe as well, and
    // mIsParentToActiveScrollFrames will presumably be false when that kicks
    // in.
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

  nsIContent* content = helper->mOuter->GetContent();

  if (ScrollFrameHelper::ShouldActivateAllScrollFrames()) {
    // If we are activating all scroll frames then we only want to remove the
    // regular display port and downgrade to a minimal display port.
    MOZ_ASSERT(!content->GetProperty(nsGkAtoms::MinimalDisplayPort));
    content->SetProperty(nsGkAtoms::MinimalDisplayPort,
                         reinterpret_cast<void*>(true));
  } else {
    content->RemoveProperty(nsGkAtoms::MinimalDisplayPort);
    DisplayPortUtils::RemoveDisplayPort(content);
    // Be conservative and unflag this this scrollframe as being scrollable by
    // APZ. If it is still scrollable this will get flipped back soon enough.
    helper->mScrollableByAPZ = false;
  }

  DisplayPortUtils::ExpireDisplayPortOnAsyncScrollableAncestor(helper->mOuter);

  helper->mOuter->SchedulePaint();
}

void ScrollFrameHelper::MarkEverScrolled() {
  // Mark this frame as having been scrolled. If this is the root
  // scroll frame of a content document, then IsAlwaysActive()
  // will return true from now on and MarkNotRecentlyScrolled() won't
  // have any effect.
  mHasBeenScrolled = true;
}

void ScrollFrameHelper::MarkNotRecentlyScrolled() {
  if (!mHasBeenScrolledRecently) return;

  mHasBeenScrolledRecently = false;
  mOuter->SchedulePaint();
}

void ScrollFrameHelper::MarkRecentlyScrolled() {
  mHasBeenScrolledRecently = true;
  if (IsAlwaysActive()) {
    return;
  }

  if (mActivityExpirationState.IsTracked()) {
    gScrollFrameActivityTracker->MarkUsed(this);
  } else {
    if (!gScrollFrameActivityTracker) {
      gScrollFrameActivityTracker =
          new ScrollFrameActivityTracker(GetMainThreadSerialEventTarget());
    }
    gScrollFrameActivityTracker->AddObject(this);
  }

  // If we just scrolled and there's a displayport expiry timer in place,
  // reset the timer.
  ResetDisplayPortExpiryTimer();
}

void ScrollFrameHelper::ResetDisplayPortExpiryTimer() {
  if (mDisplayPortExpiryTimer) {
    mDisplayPortExpiryTimer->InitWithNamedFuncCallback(
        RemoveDisplayPortCallback, this,
        StaticPrefs::apz_displayport_expiry_ms(), nsITimer::TYPE_ONE_SHOT,
        "ScrollFrameHelper::ResetDisplayPortExpiryTimer");
  }
}

bool ScrollFrameHelper::AllowDisplayPortExpiration() {
  if (IsAlwaysActive()) {
    return false;
  }
  if (mIsRoot && mOuter->PresContext()->IsRoot()) {
    return false;
  }
  if (ShouldActivateAllScrollFrames() &&
      mOuter->GetContent()->GetProperty(nsGkAtoms::MinimalDisplayPort)) {
    return false;
  }
  return true;
}

void ScrollFrameHelper::TriggerDisplayPortExpiration() {
  if (!AllowDisplayPortExpiration()) {
    return;
  }

  if (!StaticPrefs::apz_displayport_expiry_ms()) {
    // a zero time disables the expiry
    return;
  }

  if (!mDisplayPortExpiryTimer) {
    mDisplayPortExpiryTimer = NS_NewTimer();
  }
  ResetDisplayPortExpiryTimer();
}

void ScrollFrameHelper::ScrollVisual() {
  MarkEverScrolled();

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
static nscoord ClampAndAlignWithPixels(nscoord aDesired, nscoord aBoundLower,
                                       nscoord aBoundUpper, nscoord aDestLower,
                                       nscoord aDestUpper,
                                       nscoord aAppUnitsPerPixel, double aRes,
                                       nscoord aCurrent) {
  // Intersect scroll range with allowed range, by clamping the ends
  // of aRange to be within bounds
  nscoord destLower = clamped(aDestLower, aBoundLower, aBoundUpper);
  nscoord destUpper = clamped(aDestUpper, aBoundLower, aBoundUpper);

  nscoord desired = clamped(aDesired, destLower, destUpper);

  double currentLayerVal = (aRes * aCurrent) / aAppUnitsPerPixel;
  double desiredLayerVal = (aRes * desired) / aAppUnitsPerPixel;
  double delta = desiredLayerVal - currentLayerVal;
  double nearestLayerVal = NS_round(delta) + currentLayerVal;

  // Convert back from PaintedLayer space to appunits relative to the top-left
  // of the scrolled frame.
  nscoord aligned =
      aRes == 0.0
          ? 0.0
          : NSToCoordRoundWithClamp(nearestLayerVal * aAppUnitsPerPixel / aRes);

  // Use a bound if it is within the allowed range and closer to desired than
  // the nearest pixel-aligned value.
  if (aBoundUpper == destUpper &&
      static_cast<decltype(Abs(desired))>(aBoundUpper - desired) <
          Abs(desired - aligned)) {
    return aBoundUpper;
  }

  if (aBoundLower == destLower &&
      static_cast<decltype(Abs(desired))>(desired - aBoundLower) <
          Abs(aligned - desired)) {
    return aBoundLower;
  }

  // Accept the nearest pixel-aligned value if it is within the allowed range.
  if (aligned >= destLower && aligned <= destUpper) {
    return aligned;
  }

  // Check if opposite pixel boundary fits into allowed range.
  double oppositeLayerVal =
      nearestLayerVal + ((nearestLayerVal < desiredLayerVal) ? 1.0 : -1.0);
  nscoord opposite = aRes == 0.0
                         ? 0.0
                         : NSToCoordRoundWithClamp(oppositeLayerVal *
                                                   aAppUnitsPerPixel / aRes);
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
static nsPoint ClampAndAlignWithLayerPixels(const nsPoint& aPt,
                                            const nsRect& aBounds,
                                            const nsRect& aRange,
                                            const nsPoint& aCurrent,
                                            nscoord aAppUnitsPerPixel,
                                            const MatrixScales& aScale) {
  return nsPoint(
      ClampAndAlignWithPixels(aPt.x, aBounds.x, aBounds.XMost(), aRange.x,
                              aRange.XMost(), aAppUnitsPerPixel, aScale.xScale,
                              aCurrent.x),
      ClampAndAlignWithPixels(aPt.y, aBounds.y, aBounds.YMost(), aRange.y,
                              aRange.YMost(), aAppUnitsPerPixel, aScale.yScale,
                              aCurrent.y));
}

/* static */
void ScrollFrameHelper::ScrollActivityCallback(nsITimer* aTimer,
                                               void* anInstance) {
  ScrollFrameHelper* self = static_cast<ScrollFrameHelper*>(anInstance);

  // Fire the synth mouse move.
  self->mScrollActivityTimer->Cancel();
  self->mScrollActivityTimer = nullptr;
  self->mOuter->PresShell()->SynthesizeMouseMove(true);
}

void ScrollFrameHelper::ScheduleSyntheticMouseMove() {
  if (!mScrollActivityTimer) {
    mScrollActivityTimer = NS_NewTimer(
        mOuter->PresContext()->Document()->EventTargetFor(TaskCategory::Other));
    if (!mScrollActivityTimer) {
      return;
    }
  }

  mScrollActivityTimer->InitWithNamedFuncCallback(
      ScrollActivityCallback, this, 100, nsITimer::TYPE_ONE_SHOT,
      "ScrollFrameHelper::ScheduleSyntheticMouseMove");
}

void ScrollFrameHelper::NotifyApproximateFrameVisibilityUpdate(
    bool aIgnoreDisplayPort) {
  mLastUpdateFramesPos = GetScrollPosition();
  if (aIgnoreDisplayPort) {
    mHadDisplayPortAtLastFrameUpdate = false;
    mDisplayPortAtLastFrameUpdate = nsRect();
  } else {
    mHadDisplayPortAtLastFrameUpdate = DisplayPortUtils::GetDisplayPort(
        mOuter->GetContent(), &mDisplayPortAtLastFrameUpdate);
  }
}

bool ScrollFrameHelper::GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
    nsRect* aDisplayPort) {
  if (mHadDisplayPortAtLastFrameUpdate) {
    *aDisplayPort = mDisplayPortAtLastFrameUpdate;
  }
  return mHadDisplayPortAtLastFrameUpdate;
}

MatrixScales GetPaintedLayerScaleForFrame(nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame, "need a frame");

  nsPresContext* presCtx = aFrame->PresContext()->GetRootPresContext();

  if (!presCtx) {
    presCtx = aFrame->PresContext();
    MOZ_ASSERT(presCtx);
  }

  ParentLayerToScreenScale2D transformToAncestorScale =
      ParentLayerToParentLayerScale(
          presCtx->PresShell()->GetCumulativeResolution()) *
      nsLayoutUtils::GetTransformToAncestorScaleCrossProcessForFrameMetrics(
          aFrame);

  return transformToAncestorScale.ToUnknownScale();
}

void ScrollFrameHelper::ScrollToImpl(
    nsPoint aPt, const nsRect& aRange, ScrollOrigin aOrigin,
    ScrollTriggeredByScript aTriggeredByScript) {
  // None is never a valid scroll origin to be passed in.
  MOZ_ASSERT(aOrigin != ScrollOrigin::None);

  // Figure out the effective origin for this scroll request.
  if (aOrigin == ScrollOrigin::NotSpecified) {
    // If no origin was specified, we still want to set it to something that's
    // non-unknown, so that we can use eUnknown to distinguish if the frame was
    // scrolled at all. Default it to some generic placeholder.
    aOrigin = ScrollOrigin::Other;
  }

  // If this scroll is |relative|, but we've already had a user scroll that
  // was not relative, promote this origin to |other|. This ensures that we
  // may only transmit a relative update to APZ if all scrolls since the last
  // transaction or repaint request have been relative.
  if (aOrigin == ScrollOrigin::Relative &&
      (mLastScrollOrigin != ScrollOrigin::None &&
       mLastScrollOrigin != ScrollOrigin::NotSpecified &&
       mLastScrollOrigin != ScrollOrigin::Relative &&
       mLastScrollOrigin != ScrollOrigin::Apz)) {
    aOrigin = ScrollOrigin::Other;
  }

  // If the origin is a downgrade, and downgrades are allowed, process the
  // downgrade even if we're going to early-exit because we're already at
  // the correct scroll position. This ensures that if there wasn't a main-
  // thread scroll update pending before a frame reconstruction (as indicated
  // by mAllowScrollOriginDowngrade=true), then after the frame reconstruction
  // the origin is downgraded to "restore" even if the layout scroll offset to
  // be restored is (0,0) (which will take the early-exit below). This is
  // important so that restoration of a *visual* scroll offset (which might be
  // to something other than (0,0)) isn't clobbered.
  bool isScrollOriginDowngrade =
      nsLayoutUtils::CanScrollOriginClobberApz(mLastScrollOrigin) &&
      !nsLayoutUtils::CanScrollOriginClobberApz(aOrigin);
  bool allowScrollOriginChange =
      mAllowScrollOriginDowngrade && isScrollOriginDowngrade;

  if (allowScrollOriginChange) {
    mLastScrollOrigin = aOrigin;
    mAllowScrollOriginDowngrade = false;
  }

  nsPresContext* presContext = mOuter->PresContext();
  nscoord appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  // 'scale' is our estimate of the scale factor that will be applied
  // when rendering the scrolled content to its own PaintedLayer.
  MatrixScales scale = GetPaintedLayerScaleForFrame(mScrolledFrame);
  nsPoint curPos = GetScrollPosition();

  // Try to align aPt with curPos so they have an integer number of layer
  // pixels between them. This gives us the best chance of scrolling without
  // having to invalidate due to changes in subpixel rendering.
  // Note that when we actually draw into a PaintedLayer, the coordinates
  // that get mapped onto the layer buffer pixels are from the display list,
  // which are relative to the display root frame's top-left increasing down,
  // whereas here our coordinates are scroll positions which increase upward
  // and are relative to the scrollport top-left. This difference doesn't
  // actually matter since all we are about is that there be an integer number
  // of layer pixels between pt and curPos.
  nsPoint pt = ClampAndAlignWithLayerPixels(aPt, GetLayoutScrollRange(), aRange,
                                            curPos, appUnitsPerDevPixel, scale);
  if (pt == curPos) {
    // Even if we are bailing out due to no-op main-thread scroll position
    // change, we might need to cancel an APZ smooth scroll that we already
    // kicked off. It might be reasonable to eventually remove the
    // mApzSmoothScrollDestination clause from this if statement, as that
    // may simplify this a bit and should be fine from the APZ side.
    if (mApzSmoothScrollDestination && aOrigin != ScrollOrigin::Clamp) {
      if (aOrigin == ScrollOrigin::Relative) {
        AppendScrollUpdate(
            ScrollPositionUpdate::NewRelativeScroll(mApzScrollPos, pt));
        mApzScrollPos = pt;
      } else if (aOrigin != ScrollOrigin::Apz) {
        ScrollOrigin origin =
            (mAllowScrollOriginDowngrade || !isScrollOriginDowngrade)
                ? aOrigin
                : mLastScrollOrigin;
        AppendScrollUpdate(ScrollPositionUpdate::NewScroll(origin, pt));
      }
    }
    return;
  }

  // If we are scrolling the RCD-RSF, and a visual scroll update is pending,
  // cancel it; otherwise, it will clobber this scroll.
  if (IsRootScrollFrameOfDocument() &&
      presContext->IsRootContentDocumentCrossProcess()) {
    PresShell* ps = presContext->GetPresShell();
    if (const auto& visualScrollUpdate = ps->GetPendingVisualScrollUpdate()) {
      if (visualScrollUpdate->mVisualScrollOffset != aPt) {
        // Only clobber if the scroll was originated by the main thread.
        // Respect the priority of origins (an "eRestore" layout scroll should
        // not clobber an "eMainThread" visual scroll.)
        bool shouldClobber =
            aOrigin == ScrollOrigin::Other ||
            (aOrigin == ScrollOrigin::Restore &&
             visualScrollUpdate->mUpdateType == FrameMetrics::eRestore);
        if (shouldClobber) {
          ps->AcknowledgePendingVisualScrollUpdate();
          ps->ClearPendingVisualScrollUpdate();
        }
      }
    }
  }

  bool needFrameVisibilityUpdate = mLastUpdateFramesPos == nsPoint(-1, -1);

  nsPoint dist(std::abs(pt.x - mLastUpdateFramesPos.x),
               std::abs(pt.y - mLastUpdateFramesPos.y));
  nsSize visualViewportSize = GetVisualViewportSize();
  nscoord horzAllowance = std::max(
      visualViewportSize.width /
          std::max(
              StaticPrefs::
                  layout_framevisibility_amountscrollbeforeupdatehorizontal(),
              1),
      AppUnitsPerCSSPixel());
  nscoord vertAllowance = std::max(
      visualViewportSize.height /
          std::max(
              StaticPrefs::
                  layout_framevisibility_amountscrollbeforeupdatevertical(),
              1),
      AppUnitsPerCSSPixel());
  if (dist.x >= horzAllowance || dist.y >= vertAllowance) {
    needFrameVisibilityUpdate = true;
  }

  // notify the listeners.
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->ScrollPositionWillChange(pt.x, pt.y);
  }

  nsRect oldDisplayPort;
  nsIContent* content = mOuter->GetContent();
  DisplayPortUtils::GetDisplayPort(content, &oldDisplayPort);
  oldDisplayPort.MoveBy(-mScrolledFrame->GetPosition());

  // Update frame position for scrolling
  mScrolledFrame->SetPosition(mScrollPort.TopLeft() - pt);

  // If |mLastScrollOrigin| is already set to something that can clobber APZ's
  // scroll offset, then we don't want to change it to something that can't.
  // If we allowed this, then we could end up in a state where APZ ignores
  // legitimate scroll offset updates because the origin has been masked by
  // a later change within the same refresh driver tick.
  allowScrollOriginChange =
      (mAllowScrollOriginDowngrade || !isScrollOriginDowngrade);

  if (allowScrollOriginChange) {
    mLastScrollOrigin = aOrigin;
    mAllowScrollOriginDowngrade = false;
  }

  if (aOrigin == ScrollOrigin::Relative) {
    MOZ_ASSERT(!isScrollOriginDowngrade);
    MOZ_ASSERT(mLastScrollOrigin == ScrollOrigin::Relative);
    AppendScrollUpdate(
        ScrollPositionUpdate::NewRelativeScroll(mApzScrollPos, pt));
    mApzScrollPos = pt;
  } else if (aOrigin != ScrollOrigin::Apz) {
    AppendScrollUpdate(ScrollPositionUpdate::NewScroll(mLastScrollOrigin, pt));
  }

  if (mLastScrollOrigin == ScrollOrigin::Apz) {
    mApzScrollPos = GetScrollPosition();
  }

  ScrollVisual();
  mAnchor.UserScrolled();

  // Only report user-triggered scrolling interactions
  bool jsOnStack = nsContentUtils::GetCurrentJSContext() != nullptr;
  bool scrollingToAnchor = ScrollingInteractionContext::IsScrollingToAnchor();
  if (!jsOnStack && !scrollingToAnchor) {
    nsPoint distanceScrolled(std::abs(pt.x - curPos.x),
                             std::abs(pt.y - curPos.y));
    ScrollingMetrics::OnScrollingInteraction(
        CSSPoint::FromAppUnits(distanceScrolled).Length());
  }

  bool schedulePaint = true;
  if (nsLayoutUtils::AsyncPanZoomEnabled(mOuter) &&
      !nsLayoutUtils::ShouldDisableApzForElement(content) &&
      !content->GetProperty(nsGkAtoms::MinimalDisplayPort) &&
      StaticPrefs::apz_paint_skipping_enabled()) {
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
        DisplayPortUtils::GetDisplayPort(content, &displayPort);
    displayPort.MoveBy(-mScrolledFrame->GetPosition());

    PAINT_SKIP_LOG(
        "New scrollpos %s usingDP %d dpEqual %d scrollableByApz "
        "%d perspective %d bglocal %d filter %d\n",
        ToString(CSSPoint::FromAppUnits(GetScrollPosition())).c_str(),
        usingDisplayPort, displayPort.IsEqualEdges(oldDisplayPort),
        mScrollableByAPZ, HasPerspective(), HasBgAttachmentLocal(),
        mHasOutOfFlowContentInsideFilter);
    if (usingDisplayPort && displayPort.IsEqualEdges(oldDisplayPort) &&
        !HasPerspective() && !HasBgAttachmentLocal() &&
        !mHasOutOfFlowContentInsideFilter) {
      bool haveScrollLinkedEffects =
          content->GetComposedDoc()->HasScrollLinkedEffect();
      bool apzDisabled = haveScrollLinkedEffects &&
                         StaticPrefs::apz_disable_for_scroll_linked_effects();
      if (!apzDisabled) {
        if (LastScrollOrigin() == ScrollOrigin::Apz) {
          schedulePaint = false;
          PAINT_SKIP_LOG("Skipping due to APZ scroll\n");
        } else if (mScrollableByAPZ) {
          nsIWidget* widget = presContext->GetNearestWidget();
          WindowRenderer* renderer =
              widget ? widget->GetWindowRenderer() : nullptr;
          if (renderer) {
            mozilla::layers::ScrollableLayerGuid::ViewID id;
            bool success = nsLayoutUtils::FindIDFor(content, &id);
            MOZ_ASSERT(success);  // we have a displayport, we better have an ID

            // Schedule an empty transaction to carry over the scroll offset
            // update, instead of a full transaction. This empty transaction
            // might still get squashed into a full transaction if something
            // happens to trigger one.
            MOZ_ASSERT(!mScrollUpdates.IsEmpty());
            success = renderer->AddPendingScrollUpdateForNextTransaction(
                id, mScrollUpdates.LastElement());
            if (success) {
              schedulePaint = false;
              mOuter->SchedulePaint(nsIFrame::PAINT_COMPOSITE_ONLY);
              PAINT_SKIP_LOG(
                  "Skipping due to APZ-forwarded main-thread scroll\n");
            } else {
              PAINT_SKIP_LOG(
                  "Failed to set pending scroll update on layer manager\n");
            }
          }
        }
      }
    }
  }

  // If the new scroll offset is going to clobber APZ's scroll offset, for
  // the RCD-RSF this will have the effect of updating the visual viewport
  // offset in a way that keeps the relative offset between the layout and
  // visual viewports constant. This will cause APZ to send us a new visual
  // viewport offset, but instead of waiting for  that, just set the value
  // we expect APZ will set ourselves, to minimize the chances of
  // inconsistencies from querying a stale value.
  if (mIsRoot && nsLayoutUtils::CanScrollOriginClobberApz(aOrigin)) {
    AutoWeakFrame weakFrame(mOuter);
    AutoScrollbarRepaintSuppression repaintSuppression(this, weakFrame,
                                                       !schedulePaint);

    nsPoint visualViewportOffset = curPos;
    if (presContext->PresShell()->IsVisualViewportOffsetSet()) {
      visualViewportOffset =
          presContext->PresShell()->GetVisualViewportOffset();
    }
    nsPoint relativeOffset = visualViewportOffset - curPos;

    presContext->PresShell()->SetVisualViewportOffset(pt + relativeOffset,
                                                      curPos);
    if (!weakFrame.IsAlive()) {
      return;
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

  PresShell::AutoAssertNoFlush noFlush(*mOuter->PresShell());

  {  // scope the AutoScrollbarRepaintSuppression
    AutoWeakFrame weakFrame(mOuter);
    AutoScrollbarRepaintSuppression repaintSuppression(this, weakFrame,
                                                       !schedulePaint);
    UpdateScrollbarPosition();
    if (!weakFrame.IsAlive()) {
      return;
    }
  }

  presContext->RecordInteractionTime(
      nsPresContext::InteractionType::ScrollInteraction, TimeStamp::Now());

  PostScrollEvent();
  // If this is a viewport scroll, this could affect the relative offset
  // between layout and visual viewport, so we might have to fire a visual
  // viewport scroll event as well.
  if (mIsRoot) {
    if (auto* window = nsGlobalWindowInner::Cast(
            mOuter->PresContext()->Document()->GetInnerWindow())) {
      window->VisualViewport()->PostScrollEvent(
          presContext->PresShell()->GetVisualViewportOffset(), curPos);
    }
  }

  // Schedule the scroll-timelines linked to its scrollable frame.
  // if `pt == curPos`, we early return, so the position must be changed at
  // this moment. Therefore, we can schedule scroll animations directly.
  ScheduleScrollAnimations();

  // notify the listeners.
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->ScrollPositionDidChange(pt.x, pt.y);
  }

  if (nsCOMPtr<nsIDocShell> docShell = presContext->GetDocShell()) {
    docShell->NotifyScrollObservers();
  }
}

static Maybe<int32_t> MaxZIndexInList(nsDisplayList* aList,
                                      nsDisplayListBuilder* aBuilder) {
  Maybe<int32_t> maxZIndex = Nothing();
  for (nsDisplayItem* item : *aList) {
    int32_t zIndex = item->ZIndex();
    if (zIndex < 0) {
      continue;
    }
    if (!maxZIndex) {
      maxZIndex = Some(zIndex);
    } else {
      maxZIndex = Some(std::max(maxZIndex.value(), zIndex));
    }
  }
  return maxZIndex;
}

template <class T>
static void AppendInternalItemToTop(const nsDisplayListSet& aLists, T* aItem,
                                    const Maybe<int32_t>& aZIndex) {
  if (aZIndex) {
    aItem->SetOverrideZIndex(aZIndex.value());
    aLists.PositionedDescendants()->AppendToTop(aItem);
  } else {
    aLists.Content()->AppendToTop(aItem);
  }
}

static const uint32_t APPEND_OWN_LAYER = 0x1;
static const uint32_t APPEND_POSITIONED = 0x2;
static const uint32_t APPEND_SCROLLBAR_CONTAINER = 0x4;
static const uint32_t APPEND_OVERLAY = 0x8;
static const uint32_t APPEND_TOP = 0x10;

static void AppendToTop(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists, nsDisplayList* aSource,
                        nsIFrame* aSourceFrame, uint32_t aFlags) {
  if (aSource->IsEmpty()) {
    return;
  }

  nsDisplayWrapList* newItem;
  const ActiveScrolledRoot* asr = aBuilder->CurrentActiveScrolledRoot();
  if (aFlags & APPEND_OWN_LAYER) {
    ScrollbarData scrollbarData;
    if (aFlags & APPEND_SCROLLBAR_CONTAINER) {
      scrollbarData = ScrollbarData::CreateForScrollbarContainer(
          aBuilder->GetCurrentScrollbarDirection(),
          aBuilder->GetCurrentScrollbarTarget());
      // Direction should be set
      MOZ_ASSERT(scrollbarData.mDirection.isSome());
    }

    newItem = MakeDisplayItemWithIndex<nsDisplayOwnLayer>(
        aBuilder, aSourceFrame,
        /* aIndex = */ nsDisplayOwnLayer::OwnLayerForScrollbar, aSource, asr,
        nsDisplayOwnLayerFlags::None, scrollbarData, true, false);
  } else {
    // Build the wrap list with an index of 1, since the scrollbar frame itself
    // might have already built an nsDisplayWrapList.
    newItem = MakeDisplayItemWithIndex<nsDisplayWrapper>(
        aBuilder, aSourceFrame, 1, aSource, asr, false);
  }
  if (!newItem) {
    return;
  }

  if (aFlags & APPEND_POSITIONED) {
    // We want overlay scrollbars to always be on top of the scrolled content,
    // but we don't want them to unnecessarily cover overlapping elements from
    // outside our scroll frame.
    Maybe<int32_t> zIndex = Nothing();
    if (aFlags & APPEND_TOP) {
      zIndex = Some(INT32_MAX);
    } else if (aFlags & APPEND_OVERLAY) {
      zIndex = MaxZIndexInList(aLists.PositionedDescendants(), aBuilder);
    } else if (aSourceFrame->StylePosition()->mZIndex.IsInteger()) {
      zIndex = Some(aSourceFrame->StylePosition()->mZIndex.integer._0);
    }
    AppendInternalItemToTop(aLists, newItem, zIndex);
  } else {
    aLists.BorderBackground()->AppendToTop(newItem);
  }
}

struct HoveredStateComparator {
  static bool Hovered(const nsIFrame* aFrame) {
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

void ScrollFrameHelper::AppendScrollPartsTo(nsDisplayListBuilder* aBuilder,
                                            const nsDisplayListSet& aLists,
                                            bool aCreateLayer,
                                            bool aPositioned) {
  const bool overlayScrollbars = UsesOverlayScrollbars();

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
  const mozilla::layers::ScrollableLayerGuid::ViewID scrollTargetId =
      IsScrollingActive()
          ? nsLayoutUtils::FindOrCreateIDFor(mScrolledFrame->GetContent())
          : mozilla::layers::ScrollableLayerGuid::NULL_SCROLL_ID;

  scrollParts.Sort(HoveredStateComparator());

  DisplayListClipState::AutoSaveRestore clipState(aBuilder);
  // Don't let scrollparts extent outside our frame's border-box, if these are
  // viewport scrollbars. They would create layerization problems. This wouldn't
  // normally be an issue but themes can add overflow areas to scrollbar parts.
  if (mIsRoot) {
    nsRect scrollPartsClip(aBuilder->ToReferenceFrame(mOuter),
                           TrueOuterSize(aBuilder));
    clipState.ClipContentDescendants(scrollPartsClip);
  }

  for (uint32_t i = 0; i < scrollParts.Length(); ++i) {
    MOZ_ASSERT(scrollParts[i]);
    Maybe<ScrollDirection> scrollDirection;
    uint32_t appendToTopFlags = 0;
    if (scrollParts[i] == mVScrollbarBox) {
      scrollDirection.emplace(ScrollDirection::eVertical);
      appendToTopFlags |= APPEND_SCROLLBAR_CONTAINER;
    }
    if (scrollParts[i] == mHScrollbarBox) {
      MOZ_ASSERT(!scrollDirection.isSome());
      scrollDirection.emplace(ScrollDirection::eHorizontal);
      appendToTopFlags |= APPEND_SCROLLBAR_CONTAINER;
    }

    // The display port doesn't necessarily include the scrollbars, so just
    // include all of the scrollbars if we are in a RCD-RSF. We only do
    // this for the root scrollframe of the root content document, which is
    // zoomable, and where the scrollbar sizes are bounded by the widget.
    const nsRect visible =
        mIsRoot && mOuter->PresContext()->IsRootContentDocumentCrossProcess()
            ? scrollParts[i]->InkOverflowRectRelativeToParent()
            : aBuilder->GetVisibleRect();
    if (visible.IsEmpty()) {
      continue;
    }
    const nsRect dirty =
        mIsRoot && mOuter->PresContext()->IsRootContentDocumentCrossProcess()
            ? scrollParts[i]->InkOverflowRectRelativeToParent()
            : aBuilder->GetDirtyRect();

    // Always create layers for overlay scrollbars so that we don't create a
    // giant layer covering the whole scrollport if both scrollbars are visible.
    const bool isOverlayScrollbar =
        scrollDirection.isSome() && overlayScrollbars;
    const bool createLayer =
        aCreateLayer || isOverlayScrollbar ||
        StaticPrefs::layout_scrollbars_always_layerize_track();

    nsDisplayListCollection partList(aBuilder);
    {
      nsDisplayListBuilder::AutoBuildingDisplayList buildingForChild(
          aBuilder, mOuter, visible, dirty);

      nsDisplayListBuilder::AutoCurrentScrollbarInfoSetter infoSetter(
          aBuilder, scrollTargetId, scrollDirection, createLayer);
      mOuter->BuildDisplayListForChild(
          aBuilder, scrollParts[i], partList,
          nsIFrame::DisplayChildFlag::ForceStackingContext);
    }

    // DisplayChildFlag::ForceStackingContext put everything into
    // partList.PositionedDescendants().
    if (partList.PositionedDescendants()->IsEmpty()) {
      continue;
    }

    if (createLayer) {
      appendToTopFlags |= APPEND_OWN_LAYER;
    }
    if (aPositioned) {
      appendToTopFlags |= APPEND_POSITIONED;
    }

    if (isOverlayScrollbar || scrollParts[i] == mResizerBox) {
      if (isOverlayScrollbar && mIsRoot) {
        appendToTopFlags |= APPEND_TOP;
      } else {
        appendToTopFlags |= APPEND_OVERLAY;
        aBuilder->SetDisablePartialUpdates(true);
      }
    }

    {
      nsDisplayListBuilder::AutoBuildingDisplayList buildingForChild(
          aBuilder, scrollParts[i],
          visible + mOuter->GetOffsetTo(scrollParts[i]),
          dirty + mOuter->GetOffsetTo(scrollParts[i]));
      if (scrollParts[i]->IsTransformed()) {
        nsPoint toOuterReferenceFrame;
        const nsIFrame* outerReferenceFrame = aBuilder->FindReferenceFrameFor(
            scrollParts[i]->GetParent(), &toOuterReferenceFrame);
        toOuterReferenceFrame += scrollParts[i]->GetPosition();

        buildingForChild.SetReferenceFrameAndCurrentOffset(
            outerReferenceFrame, toOuterReferenceFrame);
      }
      nsDisplayListBuilder::AutoCurrentScrollbarInfoSetter infoSetter(
          aBuilder, scrollTargetId, scrollDirection, createLayer);

      ::AppendToTop(aBuilder, aLists, partList.PositionedDescendants(),
                    scrollParts[i], appendToTopFlags);
    }
  }
}

nsRect ScrollFrameHelper::ExpandRectToNearlyVisible(const nsRect& aRect) const {
  // We don't want to expand a rect in a direction that we can't scroll, so we
  // check the scroll range.
  nsRect scrollRange = GetLayoutScrollRange();
  nsPoint scrollPos = GetScrollPosition();
  nsMargin expand(0, 0, 0, 0);

  nscoord vertShift =
      StaticPrefs::layout_framevisibility_numscrollportheights() * aRect.height;
  if (scrollRange.y < scrollPos.y) {
    expand.top = vertShift;
  }
  if (scrollPos.y < scrollRange.YMost()) {
    expand.bottom = vertShift;
  }

  nscoord horzShift =
      StaticPrefs::layout_framevisibility_numscrollportwidths() * aRect.width;
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

static bool ShouldBeClippedByFrame(nsIFrame* aClipFrame,
                                   nsIFrame* aClippedFrame) {
  return nsLayoutUtils::IsProperAncestorFrame(aClipFrame, aClippedFrame);
}

static void ClipItemsExceptCaret(
    nsDisplayList* aList, nsDisplayListBuilder* aBuilder, nsIFrame* aClipFrame,
    const DisplayItemClipChain* aExtraClip,
    nsTHashMap<nsPtrHashKey<const DisplayItemClipChain>,
               const DisplayItemClipChain*>& aCache) {
  for (nsDisplayItem* i : *aList) {
    if (!ShouldBeClippedByFrame(aClipFrame, i->Frame())) {
      continue;
    }

    const DisplayItemType type = i->GetType();
    if (type != DisplayItemType::TYPE_CARET &&
        type != DisplayItemType::TYPE_CONTAINER) {
      const DisplayItemClipChain* clip = i->GetClipChain();
      const DisplayItemClipChain* intersection = nullptr;
      if (aCache.Get(clip, &intersection)) {
        i->SetClipChain(intersection, true);
      } else {
        i->IntersectClip(aBuilder, aExtraClip, true);
        aCache.InsertOrUpdate(clip, i->GetClipChain());
      }
    }
    nsDisplayList* children = i->GetSameCoordinateSystemChildren();
    if (children) {
      ClipItemsExceptCaret(children, aBuilder, aClipFrame, aExtraClip, aCache);
    }
  }
}

static void ClipListsExceptCaret(nsDisplayListCollection* aLists,
                                 nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aClipFrame,
                                 const DisplayItemClipChain* aExtraClip) {
  nsTHashMap<nsPtrHashKey<const DisplayItemClipChain>,
             const DisplayItemClipChain*>
      cache;
  ClipItemsExceptCaret(aLists->BorderBackground(), aBuilder, aClipFrame,
                       aExtraClip, cache);
  ClipItemsExceptCaret(aLists->BlockBorderBackgrounds(), aBuilder, aClipFrame,
                       aExtraClip, cache);
  ClipItemsExceptCaret(aLists->Floats(), aBuilder, aClipFrame, aExtraClip,
                       cache);
  ClipItemsExceptCaret(aLists->PositionedDescendants(), aBuilder, aClipFrame,
                       aExtraClip, cache);
  ClipItemsExceptCaret(aLists->Outlines(), aBuilder, aClipFrame, aExtraClip,
                       cache);
  ClipItemsExceptCaret(aLists->Content(), aBuilder, aClipFrame, aExtraClip,
                       cache);
}

// This is similar to a "save-restore" RAII class for
// DisplayListBuilder::ContainsBlendMode(), with a slight enhancement.
// If this class is put on the stack and then unwound, the DL builder's
// ContainsBlendMode flag will be effectively the same as if this class wasn't
// put on the stack. However, if the CaptureContainsBlendMode method is called,
// there will be a difference - the blend mode in the descendant display lists
// will be "captured" and extracted.
// The main goal here is to allow conditionally capturing the flag that
// indicates whether or not a blend mode was encountered in the descendant part
// of the display list.
class MOZ_RAII AutoContainsBlendModeCapturer {
  nsDisplayListBuilder& mBuilder;
  bool mSavedContainsBlendMode;

 public:
  explicit AutoContainsBlendModeCapturer(nsDisplayListBuilder& aBuilder)
      : mBuilder(aBuilder),
        mSavedContainsBlendMode(aBuilder.ContainsBlendMode()) {
    mBuilder.SetContainsBlendMode(false);
  }

  bool CaptureContainsBlendMode() {
    // "Capture" the flag by extracting and clearing the ContainsBlendMode flag
    // on the builder.
    bool capturedBlendMode = mBuilder.ContainsBlendMode();
    mBuilder.SetContainsBlendMode(false);
    return capturedBlendMode;
  }

  ~AutoContainsBlendModeCapturer() {
    // If CaptureContainsBlendMode() was called, the descendant blend mode was
    // "captured" and so uncapturedContainsBlendMode will be false. If
    // CaptureContainsBlendMode() wasn't called, then no capture occurred, and
    // uncapturedContainsBlendMode may be true if there was a descendant blend
    // mode. In that case, we set the flag on the DL builder so that we restore
    // state to what it would have been without this RAII class on the stack.
    bool uncapturedContainsBlendMode = mBuilder.ContainsBlendMode();
    mBuilder.SetContainsBlendMode(mSavedContainsBlendMode ||
                                  uncapturedContainsBlendMode);
  }
};

// Finds the max z-index of the items in aList that meet the following
// conditions
//   1) have z-index auto or z-index >= 0.
//   2) aFrame is a proper ancestor of the item's frame.
// Returns -1 if there is no such item.
static int32_t MaxZIndexInListOfItemsContainedInFrame(nsDisplayList* aList,
                                                      nsIFrame* aFrame) {
  int32_t maxZIndex = -1;
  for (nsDisplayItem* item : *aList) {
    nsIFrame* itemFrame = item->Frame();
    // Perspective items return the scroll frame as their Frame(), so consider
    // their TransformFrame() instead.
    if (nsLayoutUtils::IsProperAncestorFrame(aFrame, itemFrame)) {
      maxZIndex = std::max(maxZIndex, item->ZIndex());
    }
  }
  return maxZIndex;
}

void ScrollFrameHelper::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayListSet& aLists) {
  SetAndNullOnExit<const nsIFrame> tmpBuilder(
      mReferenceFrameDuringPainting, aBuilder->GetCurrentReferenceFrame());
  if (aBuilder->IsForFrameVisibility()) {
    NotifyApproximateFrameVisibilityUpdate(false);
  }

  mOuter->DisplayBorderBackgroundOutline(aBuilder, aLists);

  bool isRootContent =
      mIsRoot && mOuter->PresContext()->IsRootContentDocumentCrossProcess();

  nsRect effectiveScrollPort = mScrollPort;
  if (isRootContent && mOuter->PresContext()->HasDynamicToolbar()) {
    // Expand the scroll port to the size including the area covered by dynamic
    // toolbar in the case where the dynamic toolbar is being used since
    // position:fixed elements attached to this root scroller might be taller
    // than its scroll port (e.g 100vh). Even if the dynamic toolbar covers the
    // taller area, it doesn't mean the area is clipped by the toolbar because
    // the dynamic toolbar is laid out outside of our topmost window and it
    // transitions without changing our topmost window size.
    effectiveScrollPort.SizeTo(nsLayoutUtils::ExpandHeightForDynamicToolbar(
        mOuter->PresContext(), effectiveScrollPort.Size()));
  }

  // It's safe to get this value before the DecideScrollableLayer call below
  // because that call cannot create a displayport for root scroll frames,
  // and hence it cannot create an ignore scroll frame.
  bool ignoringThisScrollFrame = aBuilder->GetIgnoreScrollFrame() == mOuter;

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
    visibleRect = visibleRect.Intersect(effectiveScrollPort);
    dirtyRect = dirtyRect.Intersect(effectiveScrollPort);
  }

  bool dirtyRectHasBeenOverriden = false;
  Unused << DecideScrollableLayer(aBuilder, &visibleRect, &dirtyRect,
                                  /* aSetBase = */ !mIsRoot,
                                  &dirtyRectHasBeenOverriden);

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
  bool createLayersForScrollbars = isRootContent;

  nsIScrollableFrame* sf = do_QueryFrame(mOuter);
  MOZ_ASSERT(sf);

  if (ignoringThisScrollFrame) {
    // If we are a root scroll frame that has a display port we want to add
    // scrollbars, they will be children of the scrollable layer, but they get
    // adjusted by the APZC automatically.
    bool addScrollBars =
        mIsRoot && mWillBuildScrollableLayer && aBuilder->IsPaintingToWindow();

    nsDisplayListCollection set(aBuilder);

    if (addScrollBars) {
      // Add classic scrollbars.
      AppendScrollPartsTo(aBuilder, set, createLayersForScrollbars, false);
    }

    {
      nsDisplayListBuilder::AutoBuildingDisplayList building(
          aBuilder, mOuter, visibleRect, dirtyRect);

      // Don't clip the scrolled child, and don't paint scrollbars/scrollcorner.
      // The scrolled frame shouldn't have its own background/border, so we
      // can just pass aLists directly.
      mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame, set);
    }

    if (nsDisplayWrapList* topLayerWrapList =
            MaybeCreateTopLayerItems(aBuilder, nullptr)) {
      set.PositionedDescendants()->AppendToTop(topLayerWrapList);
    }

    if (addScrollBars) {
      // Add overlay scrollbars.
      AppendScrollPartsTo(aBuilder, set, createLayersForScrollbars, true);
    }

    set.MoveTo(aLists);
    return;
  }

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
      couldBuildLayer = mOuter->StyleVisibility()->IsVisible() &&
                        nsLayoutUtils::AsyncPanZoomEnabled(mOuter) &&
                        WantAsyncScroll();
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
  if (aBuilder->IsForPainting() &&
      disp->mWillChange.bits & StyleWillChangeBits::SCROLL) {
    aBuilder->AddToWillChangeBudget(mOuter, GetVisualViewportSize());
  }

  mScrollParentID = aBuilder->GetCurrentScrollParentId();

  Maybe<nsRect> contentBoxClip;
  Maybe<const DisplayItemClipChain*> extraContentBoxClipForNonCaretContent;
  if (MOZ_UNLIKELY(
          disp->mOverflowClipBoxBlock == StyleOverflowClipBox::ContentBox ||
          disp->mOverflowClipBoxInline == StyleOverflowClipBox::ContentBox)) {
    WritingMode wm = mScrolledFrame->GetWritingMode();
    bool cbH = (wm.IsVertical() ? disp->mOverflowClipBoxBlock
                                : disp->mOverflowClipBoxInline) ==
               StyleOverflowClipBox::ContentBox;
    bool cbV = (wm.IsVertical() ? disp->mOverflowClipBoxInline
                                : disp->mOverflowClipBoxBlock) ==
               StyleOverflowClipBox::ContentBox;
    // We only clip if there is *scrollable* overflow, to avoid clipping
    // *ink* overflow unnecessarily.
    nsRect clipRect = effectiveScrollPort + aBuilder->ToReferenceFrame(mOuter);
    nsMargin padding = mOuter->GetUsedPadding();
    if (!cbH) {
      padding.left = padding.right = nscoord(0);
    }
    if (!cbV) {
      padding.top = padding.bottom = nscoord(0);
    }
    clipRect.Deflate(padding);

    nsRect so = mScrolledFrame->ScrollableOverflowRect();
    if ((cbH && (clipRect.width != so.width || so.x < 0)) ||
        (cbV && (clipRect.height != so.height || so.y < 0))) {
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
        inflatedClip.Inflate(
            nsMargin(caretRect.height / 2, caretRect.width, 0, 0));
        contentBoxClip = Some(inflatedClip);
      }
    }
  }

  nsDisplayListCollection set(aBuilder);
  AutoContainsBlendModeCapturer blendCapture(*aBuilder);

  bool willBuildAsyncZoomContainer =
      mWillBuildScrollableLayer && aBuilder->ShouldBuildAsyncZoomContainer() &&
      isRootContent;

  nsRect scrollPortClip =
      effectiveScrollPort + aBuilder->ToReferenceFrame(mOuter);
  nsRect clipRect = scrollPortClip;
  // Our override of GetBorderRadii ensures we never have a radius at
  // the corners where we have a scrollbar.
  nscoord radii[8];
  bool haveRadii = mOuter->GetPaddingBoxBorderRadii(radii);
  if (mIsRoot) {
    clipRect.SizeTo(nsLayoutUtils::CalculateCompositionSizeForFrame(mOuter));
    // The composition size is essentially in visual coordinates.
    // If we are hit-testing in layout coordinates, transform the clip rect
    // to layout coordinates to match.
    if (aBuilder->IsRelativeToLayoutViewport() && isRootContent) {
      clipRect = ViewportUtils::VisualToLayout(clipRect, mOuter->PresShell());
    }
  }

  {
    // Note that setting the current scroll parent id here means that positioned
    // children of this scroll info layer will pick up the scroll info layer as
    // their scroll handoff parent. This is intentional because that is what
    // happens for positioned children of scroll layers, and we want to maintain
    // consistent behaviour between scroll layers and scroll info layers.
    nsDisplayListBuilder::AutoCurrentScrollParentIdSetter idSetter(
        aBuilder,
        couldBuildLayer && mScrolledFrame->GetContent()
            ? nsLayoutUtils::FindOrCreateIDFor(mScrolledFrame->GetContent())
            : aBuilder->GetCurrentScrollParentId());

    DisplayListClipState::AutoSaveRestore clipState(aBuilder);
    // If we're building an async zoom container, clip the contents inside
    // to the layout viewport (scrollPortClip). The composition bounds clip
    // (clipRect) will be applied to the zoom container itself below.
    nsRect clipRectForContents =
        willBuildAsyncZoomContainer ? scrollPortClip : clipRect;
    if (mIsRoot) {
      clipState.ClipContentDescendants(clipRectForContents,
                                       haveRadii ? radii : nullptr);
    } else {
      clipState.ClipContainingBlockDescendants(clipRectForContents,
                                               haveRadii ? radii : nullptr);
    }

    Maybe<DisplayListClipState::AutoSaveRestore> contentBoxClipState;
    ;
    if (contentBoxClip) {
      contentBoxClipState.emplace(aBuilder);
      if (mIsRoot) {
        contentBoxClipState->ClipContentDescendants(*contentBoxClip);
      } else {
        contentBoxClipState->ClipContainingBlockDescendants(*contentBoxClip);
      }
    }

    nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter asrSetter(
        aBuilder);
    if (mWillBuildScrollableLayer && aBuilder->IsPaintingToWindow()) {
      asrSetter.EnterScrollFrame(sf);
    }

    if (mWillBuildScrollableLayer && aBuilder->BuildCompositorHitTestInfo()) {
      // Create a hit test info item for the scrolled content that's not
      // clipped to the displayport. This ensures that within the bounds
      // of the scroll frame, the scrolled content is always hit, even
      // if we are checkerboarding.
      CompositorHitTestInfo info =
          mScrolledFrame->GetCompositorHitTestInfo(aBuilder);

      if (info != CompositorHitTestInvisibleToHit) {
        auto* hitInfo =
            MakeDisplayItemWithIndex<nsDisplayCompositorHitTestInfo>(
                aBuilder, mScrolledFrame, 1);
        if (hitInfo) {
          aBuilder->SetCompositorHitTestInfo(info);
          set.BorderBackground()->AppendToTop(hitInfo);
        }
      }
    }

    {
      // Clip our contents to the unsnapped scrolled rect. This makes sure
      // that we don't have display items over the subpixel seam at the edge
      // of the scrolled area.
      DisplayListClipState::AutoSaveRestore scrolledRectClipState(aBuilder);
      nsRect scrolledRectClip =
          GetUnsnappedScrolledRectInternal(
              mScrolledFrame->ScrollableOverflowRect(), mScrollPort.Size()) +
          mScrolledFrame->GetPosition();
      bool clippedToDisplayPort = false;
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
        clippedToDisplayPort = scrolledRectClip.IsEqualEdges(visibleRect);
      }
      scrolledRectClipState.ClipContainingBlockDescendants(
          scrolledRectClip + aBuilder->ToReferenceFrame(mOuter));
      if (clippedToDisplayPort) {
        // We have to do this after the ClipContainingBlockDescendants call
        // above, otherwise that call will clobber the flag set by this call
        // to SetClippedToDisplayPort.
        scrolledRectClipState.SetClippedToDisplayPort();
      }

      nsRect visibleRectForChildren = visibleRect;
      nsRect dirtyRectForChildren = dirtyRect;

      // If we are entering the RCD-RSF, we are crossing the async zoom
      // container boundary, and need to convert from visual to layout
      // coordinates.
      if (willBuildAsyncZoomContainer && aBuilder->IsForEventDelivery()) {
        MOZ_ASSERT(ViewportUtils::IsZoomedContentRoot(mScrolledFrame));
        visibleRectForChildren = ViewportUtils::VisualToLayout(
            visibleRectForChildren, mOuter->PresShell());
        dirtyRectForChildren = ViewportUtils::VisualToLayout(
            dirtyRectForChildren, mOuter->PresShell());
      }

      nsDisplayListBuilder::AutoBuildingDisplayList building(
          aBuilder, mOuter, visibleRectForChildren, dirtyRectForChildren);

      mOuter->BuildDisplayListForChild(aBuilder, mScrolledFrame, set);

      if (dirtyRectHasBeenOverriden &&
          StaticPrefs::layout_display_list_show_rebuild_area()) {
        nsDisplaySolidColor* color = MakeDisplayItem<nsDisplaySolidColor>(
            aBuilder, mOuter,
            dirtyRect + aBuilder->GetCurrentFrameOffsetToReferenceFrame(),
            NS_RGBA(0, 0, 255, 64), false);
        if (color) {
          color->SetOverrideZIndex(INT32_MAX);
          set.PositionedDescendants()->AppendToTop(color);
        }
      }
    }

    if (extraContentBoxClipForNonCaretContent) {
      // The items were built while the inflated content box clip was in
      // effect, so that the caret wasn't clipped unnecessarily. We apply
      // the non-inflated clip to the non-caret items now, by intersecting
      // it with their existing clip.
      ClipListsExceptCaret(&set, aBuilder, mScrolledFrame,
                           *extraContentBoxClipForNonCaretContent);
    }

    if (aBuilder->IsPaintingToWindow()) {
      mIsParentToActiveScrollFrames =
          ShouldActivateAllScrollFrames()
              ? idSetter.GetContainsNonMinimalDisplayPort()
              : idSetter.ShouldForceLayerForScrollParent();
    }
    if (idSetter.ShouldForceLayerForScrollParent()) {
      // Note that forcing layerization of scroll parents follows the scroll
      // handoff chain which is subject to the out-of-flow-frames caveat noted
      // above (where the idSetter variable is created).
      MOZ_ASSERT(couldBuildLayer && mScrolledFrame->GetContent() &&
                 aBuilder->IsPaintingToWindow());
      if (!mWillBuildScrollableLayer) {
        // Set a displayport so next paint we don't have to force layerization
        // after the fact. It's ok to pass DoNotRepaint here, since we've
        // already painted the change and we're just optimizing it to be
        // detected earlier. We also won't confuse RetainedDisplayLists
        // with the silent change, since we explicitly request partial updates
        // to be disabled on the next paint.
        DisplayPortUtils::SetDisplayPortMargins(
            mOuter->GetContent(), mOuter->PresShell(),
            DisplayPortMargins::Empty(mOuter->GetContent()),
            DisplayPortUtils::ClearMinimalDisplayPortProperty::Yes, 0,
            DisplayPortUtils::RepaintMode::DoNotRepaint);
        // Call DecideScrollableLayer to recompute mWillBuildScrollableLayer
        // and recompute the current animated geometry root if needed. It's
        // too late to change the dirty rect so pass a copy.
        nsRect copyOfDirtyRect = dirtyRect;
        nsRect copyOfVisibleRect = visibleRect;
        Unused << DecideScrollableLayer(aBuilder, &copyOfVisibleRect,
                                        &copyOfDirtyRect,
                                        /* aSetBase = */ false, nullptr);
        if (mWillBuildScrollableLayer) {
          asrSetter.InsertScrollFrame(sf);
          aBuilder->SetDisablePartialUpdates(true);
        }
      }
    }
  }

  // Create any required items for the 'top layer' and check if they'll be
  // opaque over the entire area of the viewport. If they are, then we can
  // skip building display items for the rest of the page.
  bool topLayerIsOpaque = false;
  if (nsDisplayWrapList* topLayerWrapList =
          MaybeCreateTopLayerItems(aBuilder, &topLayerIsOpaque)) {
    // If the top layer content is opaque, and we're the root content document
    // in the process, we can drop the display items behind it. We only support
    // doing this for the root content document in the process, since the top
    // layer content might have fixed position items that have a scrolltarget
    // referencing the APZ data for the document. APZ builds this data
    // implicitly for the root content document in the process, but subdocuments
    // etc need their display items to generate it, so we can't cull those.
    if (topLayerIsOpaque &&
        mOuter->PresContext()->IsRootContentDocumentInProcess()) {
      set.DeleteAll(aBuilder);
    }
    set.PositionedDescendants()->AppendToTop(topLayerWrapList);
  }

  nsDisplayList rootResultList(aBuilder);
  bool serializedList = false;
  auto SerializeList = [&] {
    if (!serializedList) {
      serializedList = true;
      set.SerializeWithCorrectZOrder(&rootResultList, mOuter->GetContent());
    }
  };
  if (mIsRoot) {
    if (nsIFrame* rootStyleFrame = GetFrameForStyle()) {
      bool usingBackdropFilter =
          rootStyleFrame->StyleEffects()->HasBackdropFilters() &&
          rootStyleFrame->IsVisibleForPainting();

      if (rootStyleFrame->StyleEffects()->HasFilters()) {
        SerializeList();
        rootResultList.AppendNewToTop<nsDisplayFilters>(
            aBuilder, mOuter, &rootResultList, rootStyleFrame,
            usingBackdropFilter);
      }

      if (usingBackdropFilter) {
        SerializeList();
        DisplayListClipState::AutoSaveRestore clipState(aBuilder);
        nsRect backdropRect = mOuter->GetRectRelativeToSelf() +
                              aBuilder->ToReferenceFrame(mOuter);
        rootResultList.AppendNewToTop<nsDisplayBackdropFilters>(
            aBuilder, mOuter, &rootResultList, backdropRect, rootStyleFrame);
      }
    }
  }

  if (willBuildAsyncZoomContainer) {
    MOZ_ASSERT(mIsRoot);

    // Wrap all our scrolled contents in an nsDisplayAsyncZoom. This will be
    // the layer that gets scaled for APZ zooming. It does not have the
    // scrolled ASR, but it does have the composition bounds clip applied to
    // it. The children have the layout viewport clip applied to them (above).
    // Effectively we are double clipping to the viewport, at potentially
    // different async scales.
    SerializeList();

    if (blendCapture.CaptureContainsBlendMode()) {
      // The async zoom contents contain a mix-blend mode, so let's wrap all
      // those contents into a blend container, and then wrap the blend
      // container in the async zoom container. Otherwise the blend container
      // ends up outside the zoom container which results in blend failure for
      // WebRender.
      nsDisplayItem* blendContainer =
          nsDisplayBlendContainer::CreateForMixBlendMode(
              aBuilder, mOuter, &rootResultList,
              aBuilder->CurrentActiveScrolledRoot());
      rootResultList.AppendToTop(blendContainer);

      // Blend containers can be created or omitted during partial updates
      // depending on the dirty rect. So we basically can't do partial updates
      // if there's a blend container involved. There is equivalent code to this
      // in the BuildDisplayListForStackingContext function as well, with a more
      // detailed comment explaining things better.
      if (aBuilder->IsRetainingDisplayList()) {
        if (aBuilder->IsPartialUpdate()) {
          aBuilder->SetPartialBuildFailed(true);
        } else {
          aBuilder->SetDisablePartialUpdates(true);
        }
      }
    }

    mozilla::layers::FrameMetrics::ViewID viewID =
        nsLayoutUtils::FindOrCreateIDFor(mScrolledFrame->GetContent());

    DisplayListClipState::AutoSaveRestore clipState(aBuilder);
    clipState.ClipContentDescendants(clipRect, haveRadii ? radii : nullptr);

    rootResultList.AppendNewToTop<nsDisplayAsyncZoom>(
        aBuilder, mOuter, &rootResultList,
        aBuilder->CurrentActiveScrolledRoot(), viewID);
  }

  if (serializedList) {
    set.Content()->AppendToTop(&rootResultList);
  }

  nsDisplayListCollection scrolledContent(aBuilder);
  set.MoveTo(scrolledContent);

  if (mWillBuildScrollableLayer && aBuilder->IsPaintingToWindow()) {
    aBuilder->ForceLayerForScrollParent();
  }

  // We want to call SetContainsNonMinimalDisplayPort if
  // mWillBuildScrollableLayer is true for any reason other than having a
  // minimal display port.
  if (aBuilder->IsPaintingToWindow()) {
    if (DisplayPortUtils::HasNonMinimalDisplayPort(mOuter->GetContent()) ||
        mZoomableByAPZ || nsContentUtils::HasScrollgrab(mOuter->GetContent())) {
      aBuilder->SetContainsNonMinimalDisplayPort();
    }
  }

  if (couldBuildLayer) {
    CompositorHitTestInfo info(CompositorHitTestFlags::eVisibleToHitTest,
                               CompositorHitTestFlags::eInactiveScrollframe);
    // If the scroll frame has non-default overscroll-behavior, instruct
    // APZ to require a target confirmation before processing events that
    // hit this scroll frame (that is, to drop the events if a
    // confirmation does not arrive within the timeout period). Otherwise,
    // APZ's fallback behaviour of scrolling the enclosing scroll frame
    // would violate the specified overscroll-behavior.
    auto overscroll = GetOverscrollBehaviorInfo();
    if (overscroll.mBehaviorX != OverscrollBehavior::Auto ||
        overscroll.mBehaviorY != OverscrollBehavior::Auto) {
      info += CompositorHitTestFlags::eRequiresTargetConfirmation;
    }

    nsRect area = effectiveScrollPort + aBuilder->ToReferenceFrame(mOuter);

    // Make sure that APZ will dispatch events back to content so we can
    // create a displayport for this frame. We'll add the item later on.
    if (!mWillBuildScrollableLayer && aBuilder->BuildCompositorHitTestInfo()) {
      int32_t zIndex = MaxZIndexInListOfItemsContainedInFrame(
          scrolledContent.PositionedDescendants(), mOuter);
      if (aBuilder->IsPartialUpdate()) {
        for (nsDisplayItem* item : mScrolledFrame->DisplayItems()) {
          if (item->GetType() ==
              DisplayItemType::TYPE_COMPOSITOR_HITTEST_INFO) {
            auto* hitTestItem =
                static_cast<nsDisplayCompositorHitTestInfo*>(item);
            if (hitTestItem->GetHitTestInfo().Info().contains(
                    CompositorHitTestFlags::eInactiveScrollframe)) {
              zIndex = std::max(zIndex, hitTestItem->ZIndex());
              item->SetCantBeReused();
            }
          }
        }
      }
      // Make sure the z-index of the inactive item is at least zero.
      // Otherwise, it will end up behind non-positioned items in the scrolled
      // content.
      zIndex = std::max(zIndex, 0);
      nsDisplayCompositorHitTestInfo* hitInfo =
          MakeDisplayItemWithIndex<nsDisplayCompositorHitTestInfo>(
              aBuilder, mScrolledFrame, 1, area, info);
      if (hitInfo) {
        AppendInternalItemToTop(scrolledContent, hitInfo, Some(zIndex));
        aBuilder->SetCompositorHitTestInfo(info);
      }
    }

    if (aBuilder->ShouldBuildScrollInfoItemsForHoisting()) {
      aBuilder->AppendNewScrollInfoItemForHoisting(
          MakeDisplayItem<nsDisplayScrollInfoLayer>(aBuilder, mScrolledFrame,
                                                    mOuter, info, area));
    }
  }

  // Now display overlay scrollbars and the resizer, if we have one.
  AppendScrollPartsTo(aBuilder, scrolledContent, createLayersForScrollbars,
                      true);

  scrolledContent.MoveTo(aLists);
}

nsDisplayWrapList* ScrollFrameHelper::MaybeCreateTopLayerItems(
    nsDisplayListBuilder* aBuilder, bool* aIsOpaque) {
  if (!mIsRoot) {
    return nullptr;
  }

  ViewportFrame* viewport = do_QueryFrame(mOuter->GetParent());
  if (!viewport) {
    return nullptr;
  }

  return viewport->BuildDisplayListForTopLayer(aBuilder, aIsOpaque);
}

nsRect ScrollFrameHelper::RestrictToRootDisplayPort(
    const nsRect& aDisplayportBase) {
  // This function clips aDisplayportBase so that it is no larger than the
  // root frame's displayport (or the root composition bounds, if we can't
  // obtain the root frame's displayport). This is useful for ensuring that
  // the displayport of a tall scrollframe doesn't gobble up all the memory.

  nsPresContext* pc = mOuter->PresContext();
  const nsPresContext* rootPresContext =
      pc->GetInProcessRootContentDocumentPresContext();
  if (!rootPresContext) {
    rootPresContext = pc->GetRootPresContext();
  }
  if (!rootPresContext) {
    return aDisplayportBase;
  }
  const mozilla::PresShell* const rootPresShell = rootPresContext->PresShell();
  nsIFrame* rootFrame = rootPresShell->GetRootScrollFrame();
  if (!rootFrame) {
    rootFrame = rootPresShell->GetRootFrame();
  }
  if (!rootFrame) {
    return aDisplayportBase;
  }

  // Make sure we aren't trying to restrict to our own displayport, which is a
  // circular dependency.
  MOZ_ASSERT(!mIsRoot || rootPresContext != pc);

  nsRect rootDisplayPort;
  bool hasDisplayPort =
      rootFrame->GetContent() && DisplayPortUtils::GetDisplayPort(
                                     rootFrame->GetContent(), &rootDisplayPort);
  if (hasDisplayPort) {
    // The display port of the root frame already factors in it's callback
    // transform, so subtract it out here, the GetCumulativeApzCallbackTransform
    // call below will add it back.
    MOZ_LOG(sDisplayportLog, LogLevel::Verbose,
            ("RestrictToRootDisplayPort: Existing root displayport is %s\n",
             ToString(rootDisplayPort).c_str()));
    if (nsIContent* content = rootFrame->GetContent()) {
      if (void* property =
              content->GetProperty(nsGkAtoms::apzCallbackTransform)) {
        rootDisplayPort -=
            CSSPoint::ToAppUnits(*static_cast<CSSPoint*>(property));
      }
    }
  } else {
    // If we don't have a display port on the root frame let's fall back to
    // the root composition bounds instead.
    nsRect rootCompBounds =
        nsRect(nsPoint(0, 0),
               nsLayoutUtils::CalculateCompositionSizeForFrame(rootFrame));

    // If rootFrame is the RCD-RSF then
    // CalculateCompositionSizeForFrame did not take the document's
    // resolution into account, so we must.
    if (rootPresContext->IsRootContentDocumentCrossProcess() &&
        rootFrame == rootPresShell->GetRootScrollFrame()) {
      MOZ_LOG(
          sDisplayportLog, LogLevel::Verbose,
          ("RestrictToRootDisplayPort: Removing resolution %f from root "
           "composition bounds %s\n",
           rootPresShell->GetResolution(), ToString(rootCompBounds).c_str()));
      rootCompBounds =
          rootCompBounds.RemoveResolution(rootPresShell->GetResolution());
    }

    rootDisplayPort = rootCompBounds;
  }
  MOZ_LOG(sDisplayportLog, LogLevel::Verbose,
          ("RestrictToRootDisplayPort: Intermediate root displayport %s\n",
           ToString(rootDisplayPort).c_str()));

  // We want to convert the root display port from the
  // coordinate space of |rootFrame| to the coordinate space of
  // |mOuter|. We do that with the TransformRect call below.
  // However, since we care about the root display port
  // relative to what the user is actually seeing, we also need to
  // incorporate the APZ callback transforms into this. Most of the
  // time those transforms are negligible, but in some cases (e.g.
  // when a zoom is applied on an overflow:hidden document) it is
  // not (see bug 1280013).
  // XXX: Eventually we may want to create a modified version of
  // TransformRect that includes the APZ callback transforms
  // directly.
  nsLayoutUtils::TransformRect(rootFrame, mOuter, rootDisplayPort);
  MOZ_LOG(sDisplayportLog, LogLevel::Verbose,
          ("RestrictToRootDisplayPort: Transformed root displayport %s\n",
           ToString(rootDisplayPort).c_str()));
  rootDisplayPort += CSSPoint::ToAppUnits(
      nsLayoutUtils::GetCumulativeApzCallbackTransform(mOuter));
  MOZ_LOG(sDisplayportLog, LogLevel::Verbose,
          ("RestrictToRootDisplayPort: Final root displayport %s\n",
           ToString(rootDisplayPort).c_str()));

  // We want to limit aDisplayportBase to be no larger than
  // rootDisplayPort on either axis, but we don't want to just
  // blindly intersect the two, because rootDisplayPort might be
  // offset from where aDisplayportBase is (see bug 1327095 comment
  // 8). Instead, we translate rootDisplayPort so as to maximize the
  // overlap with aDisplayportBase, and *then* do the intersection.
  if (rootDisplayPort.x > aDisplayportBase.x &&
      rootDisplayPort.XMost() > aDisplayportBase.XMost()) {
    // rootDisplayPort is at a greater x-position for both left and
    // right, so translate it such that the XMost() values are the
    // same. This will line up the right edge of the two rects, and
    // might mean that rootDisplayPort.x is smaller than
    // aDisplayportBase.x. We can avoid that by taking the min of the
    // x delta and XMost() delta, but it doesn't really matter
    // because the intersection between the two rects below will end
    // up the same.
    rootDisplayPort.x -= (rootDisplayPort.XMost() - aDisplayportBase.XMost());
  } else if (rootDisplayPort.x < aDisplayportBase.x &&
             rootDisplayPort.XMost() < aDisplayportBase.XMost()) {
    // Analaogous code for when the rootDisplayPort is at a smaller
    // x-position.
    rootDisplayPort.x = aDisplayportBase.x;
  }
  // Do the same for y-axis
  if (rootDisplayPort.y > aDisplayportBase.y &&
      rootDisplayPort.YMost() > aDisplayportBase.YMost()) {
    rootDisplayPort.y -= (rootDisplayPort.YMost() - aDisplayportBase.YMost());
  } else if (rootDisplayPort.y < aDisplayportBase.y &&
             rootDisplayPort.YMost() < aDisplayportBase.YMost()) {
    rootDisplayPort.y = aDisplayportBase.y;
  }
  MOZ_LOG(
      sDisplayportLog, LogLevel::Verbose,
      ("RestrictToRootDisplayPort: Root displayport translated to %s to "
       "better enclose %s\n",
       ToString(rootDisplayPort).c_str(), ToString(aDisplayportBase).c_str()));

  // Now we can do the intersection
  return aDisplayportBase.Intersect(rootDisplayPort);
}

/* static */ bool ScrollFrameHelper::ShouldActivateAllScrollFrames() {
  return (StaticPrefs::apz_wr_activate_all_scroll_frames() ||
          (StaticPrefs::apz_wr_activate_all_scroll_frames_when_fission() &&
           FissionAutostart()));
}

bool ScrollFrameHelper::DecideScrollableLayer(
    nsDisplayListBuilder* aBuilder, nsRect* aVisibleRect, nsRect* aDirtyRect,
    bool aSetBase, bool* aDirtyRectHasBeenOverriden) {
  nsIContent* content = mOuter->GetContent();
  // For hit testing purposes with fission we want to create a
  // minimal display port for every scroll frame that could be active. (We only
  // do this when aSetBase is true because we only want to do this the first
  // time this function is called for the same scroll frame.)
  if (ShouldActivateAllScrollFrames() &&
      !DisplayPortUtils::HasDisplayPort(content) &&
      nsLayoutUtils::AsyncPanZoomEnabled(mOuter) && WantAsyncScroll() &&
      aBuilder->IsPaintingToWindow() && aSetBase) {
    // SetDisplayPortMargins calls TriggerDisplayPortExpiration which starts a
    // display port expiry timer for display ports that do expire. However
    // minimal display ports do not expire, so the display port has to be
    // marked before the SetDisplayPortMargins call so the expiry timer
    // doesn't get started.
    content->SetProperty(nsGkAtoms::MinimalDisplayPort,
                         reinterpret_cast<void*>(true));

    DisplayPortUtils::SetDisplayPortMargins(
        content, mOuter->PresShell(), DisplayPortMargins::Empty(content),
        DisplayPortUtils::ClearMinimalDisplayPortProperty::No, 0,
        DisplayPortUtils::RepaintMode::DoNotRepaint);
  }

  bool usingDisplayPort = DisplayPortUtils::HasDisplayPort(content);
  if (aBuilder->IsPaintingToWindow()) {
    if (aSetBase) {
      nsRect displayportBase = *aVisibleRect;
      nsPresContext* pc = mOuter->PresContext();

      bool isContentRootDoc = pc->IsRootContentDocumentCrossProcess();
      bool isChromeRootDoc =
          !pc->Document()->IsContentDocument() && !pc->GetParentPresContext();

      if (mIsRoot && (isContentRootDoc || isChromeRootDoc)) {
        displayportBase =
            nsRect(nsPoint(0, 0),
                   nsLayoutUtils::CalculateCompositionSizeForFrame(mOuter));
      } else {
        // Make the displayport base equal to the visible rect restricted to
        // the scrollport and the root composition bounds, relative to the
        // scrollport.
        displayportBase = aVisibleRect->Intersect(mScrollPort);

        mozilla::layers::ScrollableLayerGuid::ViewID viewID =
            mozilla::layers::ScrollableLayerGuid::NULL_SCROLL_ID;
        if (MOZ_LOG_TEST(sDisplayportLog, LogLevel::Verbose)) {
          nsLayoutUtils::FindIDFor(mOuter->GetContent(), &viewID);
          MOZ_LOG(
              sDisplayportLog, LogLevel::Verbose,
              ("Scroll id %" PRIu64 " has visible rect %s, scroll port %s\n",
               viewID, ToString(*aVisibleRect).c_str(),
               ToString(mScrollPort).c_str()));
        }

        // Only restrict to the root displayport bounds if necessary,
        // as the required coordinate transformation is expensive.
        // And don't call RestrictToRootDisplayPort if we would be trying to
        // restrict to our own display port, which doesn't make sense (ie if we
        // are a root scroll frame in a process root prescontext).
        if (usingDisplayPort && (!mIsRoot || pc->GetParentPresContext())) {
          displayportBase = RestrictToRootDisplayPort(displayportBase);
          MOZ_LOG(sDisplayportLog, LogLevel::Verbose,
                  ("Scroll id %" PRIu64 " has restricted base %s\n", viewID,
                   ToString(displayportBase).c_str()));
        }
        displayportBase -= mScrollPort.TopLeft();
      }

      DisplayPortUtils::SetDisplayPortBase(mOuter->GetContent(),
                                           displayportBase);
    }

    // If we don't have aSetBase == true then should have already
    // been called with aSetBase == true which should have set a
    // displayport base.
    MOZ_ASSERT(content->GetProperty(nsGkAtoms::DisplayPortBase));
    nsRect displayPort;
    usingDisplayPort = DisplayPortUtils::GetDisplayPort(
        content, &displayPort,
        DisplayPortOptions().With(DisplayportRelativeTo::ScrollFrame));

    auto OverrideDirtyRect = [&](const nsRect& aRect) {
      *aDirtyRect = aRect;
      if (aDirtyRectHasBeenOverriden) {
        *aDirtyRectHasBeenOverriden = true;
      }
    };

    if (usingDisplayPort) {
      // Override the dirty rectangle if the displayport has been set.
      *aVisibleRect = displayPort;
      if (aBuilder->IsReusingStackingContextItems() ||
          !aBuilder->IsPartialUpdate() || aBuilder->InInvalidSubtree() ||
          mOuter->IsFrameModified()) {
        OverrideDirtyRect(displayPort);
      } else if (mOuter->HasOverrideDirtyRegion()) {
        nsRect* rect = mOuter->GetProperty(
            nsDisplayListBuilder::DisplayListBuildingDisplayPortRect());
        if (rect) {
          OverrideDirtyRect(*rect);
        }
      }
    } else if (mIsRoot) {
      // The displayPort getter takes care of adjusting for resolution. So if
      // we have resolution but no displayPort then we need to adjust for
      // resolution here.
      PresShell* presShell = mOuter->PresShell();
      *aVisibleRect =
          aVisibleRect->RemoveResolution(presShell->GetResolution());
      *aDirtyRect = aDirtyRect->RemoveResolution(presShell->GetResolution());
    }
  }

  // Since making new layers is expensive, only create a scrollable layer
  // for some scroll frames.
  // When a displayport is being used, force building of a layer so that
  // the compositor can find the scrollable layer for async scrolling.
  // If the element is marked 'scrollgrab', also force building of a layer
  // so that APZ can implement scroll grabbing.
  mWillBuildScrollableLayer = usingDisplayPort ||
                              nsContentUtils::HasScrollgrab(content) ||
                              mZoomableByAPZ;
  return mWillBuildScrollableLayer;
}

void ScrollFrameHelper::NotifyApzTransaction() {
  mAllowScrollOriginDowngrade = true;
  mApzScrollPos = GetScrollPosition();
  mApzAnimationRequested = IsLastScrollUpdateAnimating();
  mScrollUpdates.Clear();
  if (mIsRoot) {
    mOuter->PresShell()->SetResolutionUpdated(false);
  }
}

Maybe<ScrollMetadata> ScrollFrameHelper::ComputeScrollMetadata(
    WebRenderLayerManager* aLayerManager, const nsIFrame* aItemFrame,
    const nsPoint& aOffsetToReferenceFrame) const {
  if (!mWillBuildScrollableLayer) {
    return Nothing();
  }

  bool isRootContent =
      mIsRoot && mOuter->PresContext()->IsRootContentDocumentCrossProcess();

  MOZ_ASSERT(mScrolledFrame->GetContent());

  return Some(nsLayoutUtils::ComputeScrollMetadata(
      mScrolledFrame, mOuter, mOuter->GetContent(), aItemFrame,
      aOffsetToReferenceFrame, aLayerManager, mScrollParentID,
      mScrollPort.Size(), isRootContent));
}

bool ScrollFrameHelper::IsRectNearlyVisible(const nsRect& aRect) const {
  // Use the right rect depending on if a display port is set.
  nsRect displayPort;
  bool usingDisplayport = DisplayPortUtils::GetDisplayPort(
      mOuter->GetContent(), &displayPort,
      DisplayPortOptions().With(DisplayportRelativeTo::ScrollFrame));

  if (mIsRoot && !usingDisplayport &&
      mOuter->PresContext()->IsRootContentDocumentInProcess() &&
      !mOuter->PresContext()->IsRootContentDocumentCrossProcess()) {
    // In the case of the root scroller of OOP iframes, there are cases where
    // any display port value isn't set, e.g. the iframe element is out of view
    // in the parent document. In such cases we'd consider the iframe is not
    // visible.
    return false;
  }

  return aRect.Intersects(
      ExpandRectToNearlyVisible(usingDisplayport ? displayPort : mScrollPort));
}

OverscrollBehaviorInfo ScrollFrameHelper::GetOverscrollBehaviorInfo() const {
  nsIFrame* frame = GetFrameForStyle();
  if (!frame) {
    return {};
  }

  auto& disp = *frame->StyleDisplay();
  return OverscrollBehaviorInfo::FromStyleConstants(disp.mOverscrollBehaviorX,
                                                    disp.mOverscrollBehaviorY);
}

ScrollStyles ScrollFrameHelper::GetScrollStylesFromFrame() const {
  nsPresContext* presContext = mOuter->PresContext();
  if (!presContext->IsDynamic() &&
      !(mIsRoot && presContext->HasPaginatedScrolling())) {
    return ScrollStyles(StyleOverflow::Hidden, StyleOverflow::Hidden);
  }

  if (!mIsRoot) {
    return ScrollStyles(*mOuter->StyleDisplay(),
                        ScrollStyles::MapOverflowToValidScrollStyle);
  }

  ScrollStyles result = presContext->GetViewportScrollStylesOverride();
  if (nsDocShell* ds = presContext->GetDocShell()) {
    switch (ds->ScrollbarPreference()) {
      case ScrollbarPreference::Auto:
        break;
      case ScrollbarPreference::Never:
        result.mHorizontal = result.mVertical = StyleOverflow::Hidden;
        break;
    }
  }
  return result;
}

nsRect ScrollFrameHelper::GetLayoutScrollRange() const {
  return GetScrollRange(mScrollPort.width, mScrollPort.height);
}

nsRect ScrollFrameHelper::GetScrollRange(nscoord aWidth,
                                         nscoord aHeight) const {
  nsRect range = GetScrolledRect();
  range.width = std::max(range.width - aWidth, 0);
  range.height = std::max(range.height - aHeight, 0);
  return range;
}

nsRect ScrollFrameHelper::GetVisualScrollRange() const {
  nsSize visualViewportSize = GetVisualViewportSize();
  return GetScrollRange(visualViewportSize.width, visualViewportSize.height);
}

nsSize ScrollFrameHelper::GetVisualViewportSize() const {
  PresShell* presShell = mOuter->PresShell();
  if (mIsRoot && presShell->IsVisualViewportSizeSet()) {
    return presShell->GetVisualViewportSize();
  }
  return mScrollPort.Size();
}

nsPoint ScrollFrameHelper::GetVisualViewportOffset() const {
  PresShell* presShell = mOuter->PresShell();
  if (mIsRoot) {
    if (auto pendingUpdate = presShell->GetPendingVisualScrollUpdate()) {
      return pendingUpdate->mVisualScrollOffset;
    }
    return presShell->GetVisualViewportOffset();
  }
  return GetScrollPosition();
}

bool ScrollFrameHelper::SetVisualViewportOffset(const nsPoint& aOffset,
                                                bool aRepaint) {
  MOZ_ASSERT(mIsRoot);
  AutoWeakFrame weakFrame(mOuter);
  AutoScrollbarRepaintSuppression repaintSuppression(this, weakFrame,
                                                     !aRepaint);

  bool retVal = mOuter->PresShell()->SetVisualViewportOffset(
      aOffset, GetScrollPosition());
  if (!weakFrame.IsAlive()) {
    return false;
  }
  return retVal;
}

nsRect ScrollFrameHelper::GetVisualOptimalViewingRect() const {
  PresShell* presShell = mOuter->PresShell();
  nsRect rect = mScrollPort;
  if (mIsRoot && presShell->IsVisualViewportSizeSet() &&
      presShell->IsVisualViewportOffsetSet()) {
    rect = nsRect(mScrollPort.TopLeft() - GetScrollPosition() +
                      presShell->GetVisualViewportOffset(),
                  presShell->GetVisualViewportSize());
  }
  // NOTE: We intentionally resolve scroll-padding percentages against the
  // scrollport even when the visual viewport is set, see
  // https://github.com/w3c/csswg-drafts/issues/4393.
  rect.Deflate(GetScrollPadding());
  return rect;
}

static void AdjustDestinationForWholeDelta(const nsIntPoint& aDelta,
                                           const nsRect& aScrollRange,
                                           nsPoint& aPoint) {
  if (aDelta.x < 0) {
    aPoint.x = aScrollRange.X();
  } else if (aDelta.x > 0) {
    aPoint.x = aScrollRange.XMost();
  }
  if (aDelta.y < 0) {
    aPoint.y = aScrollRange.Y();
  } else if (aDelta.y > 0) {
    aPoint.y = aScrollRange.YMost();
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
static void CalcRangeForScrollBy(int32_t aDelta, nscoord aPos,
                                 float aNegTolerance, float aPosTolerance,
                                 nscoord aMultiplier, nscoord* aLower,
                                 nscoord* aUpper) {
  if (!aDelta) {
    *aLower = *aUpper = aPos;
    return;
  }
  *aLower = aPos - NSToCoordRound(aMultiplier *
                                  (aDelta > 0 ? aNegTolerance : aPosTolerance));
  *aUpper = aPos + NSToCoordRound(aMultiplier *
                                  (aDelta > 0 ? aPosTolerance : aNegTolerance));
}

void ScrollFrameHelper::ScrollBy(nsIntPoint aDelta, ScrollUnit aUnit,
                                 ScrollMode aMode, nsIntPoint* aOverflow,
                                 ScrollOrigin aOrigin,
                                 nsIScrollableFrame::ScrollMomentum aMomentum,
                                 ScrollSnapFlags aSnapFlags) {
  // When a smooth scroll is being processed on a frame, mouse wheel and
  // trackpad momentum scroll event updates must notcancel the SMOOTH or
  // SMOOTH_MSD scroll animations, enabling Javascript that depends on them to
  // be responsive without forcing the user to wait for the fling animations to
  // completely stop.
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
  if (aOrigin == ScrollOrigin::NotSpecified) {
    aOrigin = ScrollOrigin::Other;
  }
  bool isGenericOrigin = (aOrigin == ScrollOrigin::Other);

  bool askApzToDoTheScroll = false;
  if ((aSnapFlags == ScrollSnapFlags::Disabled || !NeedsScrollSnap()) &&
      gfxPlatform::UseDesktopZoomingScrollbars() &&
      nsLayoutUtils::AsyncPanZoomEnabled(mOuter) &&
      !nsLayoutUtils::ShouldDisableApzForElement(mOuter->GetContent()) &&
      (WantAsyncScroll() || mZoomableByAPZ)) {
    askApzToDoTheScroll = true;
  }

  switch (aUnit) {
    case ScrollUnit::DEVICE_PIXELS: {
      nscoord appUnitsPerDevPixel =
          mOuter->PresContext()->AppUnitsPerDevPixel();
      deltaMultiplier = nsSize(appUnitsPerDevPixel, appUnitsPerDevPixel);
      if (isGenericOrigin) {
        aOrigin = ScrollOrigin::Pixels;
      }
      negativeTolerance = positiveTolerance = 0.5f;
      break;
    }
    case ScrollUnit::LINES: {
      deltaMultiplier = GetLineScrollAmount();
      if (isGenericOrigin) {
        aOrigin = ScrollOrigin::Lines;
      }
      negativeTolerance = positiveTolerance = 0.1f;
      break;
    }
    case ScrollUnit::PAGES: {
      deltaMultiplier = GetPageScrollAmount();
      if (isGenericOrigin) {
        aOrigin = ScrollOrigin::Pages;
      }
      negativeTolerance = 0.05f;
      positiveTolerance = 0;
      break;
    }
    case ScrollUnit::WHOLE: {
      if (askApzToDoTheScroll) {
        MOZ_ASSERT(aDelta.x >= -1 && aDelta.x <= 1 && aDelta.y >= -1 &&
                   aDelta.y <= 1);
        deltaMultiplier = GetScrollRangeForUserInputEvents().Size();
        break;
      } else {
        nsPoint pos = GetScrollPosition();
        AdjustDestinationForWholeDelta(aDelta, GetLayoutScrollRange(), pos);
        if (aSnapFlags != ScrollSnapFlags::Disabled) {
          GetSnapPointForDestination(aUnit, aSnapFlags, mDestination, pos);
        }
        ScrollTo(pos, aMode, ScrollOrigin::Other);
        // 'this' might be destroyed here
        if (aOverflow) {
          *aOverflow = nsIntPoint(0, 0);
        }
        return;
      }
    }
    default:
      NS_ERROR("Invalid scroll mode");
      return;
  }

  if (askApzToDoTheScroll) {
    nsPoint delta(
        NSCoordSaturatingNonnegativeMultiply(aDelta.x, deltaMultiplier.width),
        NSCoordSaturatingNonnegativeMultiply(aDelta.y, deltaMultiplier.height));

    AppendScrollUpdate(
        ScrollPositionUpdate::NewPureRelativeScroll(aOrigin, aMode, delta));

    nsIContent* content = mOuter->GetContent();
    if (!DisplayPortUtils::HasNonMinimalNonZeroDisplayPort(content)) {
      if (MOZ_LOG_TEST(sDisplayportLog, LogLevel::Debug)) {
        mozilla::layers::ScrollableLayerGuid::ViewID viewID =
            mozilla::layers::ScrollableLayerGuid::NULL_SCROLL_ID;
        nsLayoutUtils::FindIDFor(content, &viewID);
        MOZ_LOG(
            sDisplayportLog, LogLevel::Debug,
            ("ScrollBy setting displayport on scrollId=%" PRIu64 "\n", viewID));
      }

      DisplayPortUtils::CalculateAndSetDisplayPortMargins(
          mOuter->GetScrollTargetFrame(),
          DisplayPortUtils::RepaintMode::Repaint);
      nsIFrame* frame = do_QueryFrame(mOuter->GetScrollTargetFrame());
      DisplayPortUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(
          frame);
    }

    mOuter->SchedulePaint();
    return;
  }

  nsPoint newPos(NSCoordSaturatingAdd(mDestination.x,
                                      NSCoordSaturatingNonnegativeMultiply(
                                          aDelta.x, deltaMultiplier.width)),
                 NSCoordSaturatingAdd(mDestination.y,
                                      NSCoordSaturatingNonnegativeMultiply(
                                          aDelta.y, deltaMultiplier.height)));

  if (aSnapFlags != ScrollSnapFlags::Disabled) {
    if (NeedsScrollSnap()) {
      nscoord appUnitsPerDevPixel =
          mOuter->PresContext()->AppUnitsPerDevPixel();
      deltaMultiplier = nsSize(appUnitsPerDevPixel, appUnitsPerDevPixel);
      negativeTolerance = 0.1f;
      positiveTolerance = 0;
      ScrollUnit snapUnit = aUnit;
      if (aOrigin == ScrollOrigin::MouseWheel) {
        // When using a clicky scroll wheel, snap point selection works the same
        // as keyboard up/down/left/right navigation, but with varying amounts
        // of scroll delta.
        snapUnit = ScrollUnit::LINES;
      }
      GetSnapPointForDestination(snapUnit, aSnapFlags, mDestination, newPos);
    }
  }

  // Calculate desired range values.
  nscoord rangeLowerX, rangeUpperX, rangeLowerY, rangeUpperY;
  CalcRangeForScrollBy(aDelta.x, newPos.x, negativeTolerance, positiveTolerance,
                       deltaMultiplier.width, &rangeLowerX, &rangeUpperX);
  CalcRangeForScrollBy(aDelta.y, newPos.y, negativeTolerance, positiveTolerance,
                       deltaMultiplier.height, &rangeLowerY, &rangeUpperY);
  nsRect range(rangeLowerX, rangeLowerY, rangeUpperX - rangeLowerX,
               rangeUpperY - rangeLowerY);
  AutoWeakFrame weakFrame(mOuter);
  ScrollToWithOrigin(newPos, aMode, aOrigin, &range);
  if (!weakFrame.IsAlive()) {
    return;
  }

  if (aOverflow) {
    nsPoint clampAmount = newPos - mDestination;
    float appUnitsPerDevPixel = mOuter->PresContext()->AppUnitsPerDevPixel();
    *aOverflow =
        nsIntPoint(NSAppUnitsToIntPixels(clampAmount.x, appUnitsPerDevPixel),
                   NSAppUnitsToIntPixels(clampAmount.y, appUnitsPerDevPixel));
  }

  if (aUnit == ScrollUnit::DEVICE_PIXELS &&
      !nsLayoutUtils::AsyncPanZoomEnabled(mOuter)) {
    // When APZ is disabled, we must track the velocity
    // on the main thread; otherwise, the APZC will manage this.
    mVelocityQueue.Sample(GetScrollPosition());
  }
}

void ScrollFrameHelper::ScrollByCSSPixels(const CSSIntPoint& aDelta,
                                          ScrollMode aMode,
                                          mozilla::ScrollSnapFlags aSnapFlags) {
  nsPoint current = GetScrollPosition();
  // `current` value above might be a value which was aligned to in
  // layer-pixels, so starting from such points will make the difference between
  // the given position in script (e.g. scrollTo) and the aligned position
  // larger, in the worst case the difference can be observed in CSS pixels.
  // To avoid it, we use the current position in CSS pixels as the start
  // position.  Hopefully it exactly matches the position where it was given by
  // the previous scrolling operation, but there may be some edge cases where
  // the current position in CSS pixels differs from the given position, the
  // cases should be fixed in bug 1556685.
  CSSIntPoint currentCSSPixels = GetScrollPositionCSSPixels();
  nsPoint pt = CSSPoint::ToAppUnits(currentCSSPixels + aDelta);

  nscoord halfPixel = nsPresContext::CSSPixelsToAppUnits(0.5f);
  nsRect range(pt.x - halfPixel, pt.y - halfPixel, 2 * halfPixel - 1,
               2 * halfPixel - 1);
  // XXX I don't think the following blocks are needed anymore, now that
  // ScrollToImpl simply tries to scroll an integer number of layer
  // pixels from the current position
  if (aDelta.x == 0.0f) {
    pt.x = current.x;
    range.x = pt.x;
    range.width = 0;
  }
  if (aDelta.y == 0.0f) {
    pt.y = current.y;
    range.y = pt.y;
    range.height = 0;
  }
  ScrollToWithOrigin(pt, aMode, ScrollOrigin::Relative, &range, aSnapFlags,
                     ScrollTriggeredByScript::Yes);
  // 'this' might be destroyed here
}

void ScrollFrameHelper::ScrollSnap(ScrollMode aMode) {
  float flingSensitivity =
      StaticPrefs::layout_css_scroll_snap_prediction_sensitivity();
  int maxVelocity =
      StaticPrefs::layout_css_scroll_snap_prediction_max_velocity();
  maxVelocity = nsPresContext::CSSPixelsToAppUnits(maxVelocity);
  int maxOffset = maxVelocity * flingSensitivity;
  nsPoint velocity = mVelocityQueue.GetVelocity();
  // Multiply each component individually to avoid integer multiply
  nsPoint predictedOffset =
      nsPoint(velocity.x * flingSensitivity, velocity.y * flingSensitivity);
  predictedOffset.Clamp(maxOffset);
  nsPoint pos = GetScrollPosition();
  nsPoint destinationPos = pos + predictedOffset;
  ScrollSnap(destinationPos, aMode);
}

void ScrollFrameHelper::ScrollSnap(const nsPoint& aDestination,
                                   ScrollMode aMode) {
  nsRect scrollRange = GetLayoutScrollRange();
  nsPoint pos = GetScrollPosition();
  nsPoint snapDestination = scrollRange.ClampPoint(aDestination);
  ScrollSnapFlags snapFlags = ScrollSnapFlags::IntendedEndPosition;
  if (mVelocityQueue.GetVelocity() != nsPoint()) {
    snapFlags |= ScrollSnapFlags::IntendedDirection;
  }
  if (GetSnapPointForDestination(ScrollUnit::DEVICE_PIXELS, snapFlags, pos,
                                 snapDestination)) {
    ScrollTo(snapDestination, aMode, ScrollOrigin::Other);
  }
}

nsSize ScrollFrameHelper::GetLineScrollAmount() const {
  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetInflatedFontMetricsForFrame(mOuter);
  NS_ASSERTION(fm, "FontMetrics is null, assuming fontHeight == 1 appunit");
  int32_t appUnitsPerDevPixel = mOuter->PresContext()->AppUnitsPerDevPixel();
  nscoord minScrollAmountInAppUnits =
      std::max(1, StaticPrefs::mousewheel_min_line_scroll_amount()) *
      appUnitsPerDevPixel;
  nscoord horizontalAmount = fm ? fm->AveCharWidth() : 0;
  nscoord verticalAmount = fm ? fm->MaxHeight() : 0;
  return nsSize(std::max(horizontalAmount, minScrollAmountInAppUnits),
                std::max(verticalAmount, minScrollAmountInAppUnits));
}

/**
 * Compute the scrollport size excluding any fixed-pos and sticky-pos (that are
 * stuck) headers and footers. A header or footer is an box that spans that
 * entire width of the viewport and touches the top (or bottom, respectively) of
 * the viewport. We also want to consider fixed/sticky elements that stack or
 * overlap to effectively create a larger header or footer. Headers and footers
 * that cover more than a third of the the viewport are ignored since they
 * probably aren't true headers and footers and we don't want to restrict
 * scrolling too much in such cases. This is a bit conservative --- some
 * pages use elements as headers or footers that don't span the entire width
 * of the viewport --- but it should be a good start.
 *
 * If aViewportFrame is non-null then the scroll frame is the root scroll
 * frame and we should consider fixed-pos items.
 */
struct TopAndBottom {
  TopAndBottom(nscoord aTop, nscoord aBottom) : top(aTop), bottom(aBottom) {}

  nscoord top, bottom;
};
struct TopComparator {
  bool Equals(const TopAndBottom& A, const TopAndBottom& B) const {
    return A.top == B.top;
  }
  bool LessThan(const TopAndBottom& A, const TopAndBottom& B) const {
    return A.top < B.top;
  }
};
struct ReverseBottomComparator {
  bool Equals(const TopAndBottom& A, const TopAndBottom& B) const {
    return A.bottom == B.bottom;
  }
  bool LessThan(const TopAndBottom& A, const TopAndBottom& B) const {
    return A.bottom > B.bottom;
  }
};

static void AddToListIfHeaderFooter(nsIFrame* aFrame,
                                    nsIFrame* aScrollPortFrame,
                                    const nsRect& aScrollPort,
                                    nsTArray<TopAndBottom>& aList) {
  nsRect r = aFrame->GetRectRelativeToSelf();
  r = nsLayoutUtils::TransformFrameRectToAncestor(aFrame, r, aScrollPortFrame);
  r = r.Intersect(aScrollPort);
  if ((r.width >= aScrollPort.width / 2 ||
       r.width >= NSIntPixelsToAppUnits(800, AppUnitsPerCSSPixel())) &&
      r.height <= aScrollPort.height / 3) {
    aList.AppendElement(TopAndBottom(r.y, r.YMost()));
  }
}

static nsSize GetScrollPortSizeExcludingHeadersAndFooters(
    nsIFrame* aScrollFrame, nsIFrame* aViewportFrame,
    const nsRect& aScrollPort) {
  AutoTArray<TopAndBottom, 10> list;
  if (aViewportFrame) {
    nsFrameList fixedFrames =
        aViewportFrame->GetChildList(nsIFrame::kFixedList);
    for (nsFrameList::Enumerator iterator(fixedFrames); !iterator.AtEnd();
         iterator.Next()) {
      AddToListIfHeaderFooter(iterator.get(), aViewportFrame, aScrollPort,
                              list);
    }
  }

  // Add sticky frames that are currently in "fixed" positions
  StickyScrollContainer* ssc =
      StickyScrollContainer::GetStickyScrollContainerForScrollFrame(
          aScrollFrame);
  if (ssc) {
    const nsTArray<nsIFrame*>& stickyFrames = ssc->GetFrames();
    for (nsIFrame* f : stickyFrames) {
      // If it's acting like fixed position.
      if (ssc->IsStuckInYDirection(f)) {
        AddToListIfHeaderFooter(f, aScrollFrame, aScrollPort, list);
      }
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

  headerBottom = std::min(aScrollPort.height / 3, headerBottom);
  footerTop = std::max(aScrollPort.height - aScrollPort.height / 3, footerTop);

  return nsSize(aScrollPort.width, footerTop - headerBottom);
}

nsSize ScrollFrameHelper::GetPageScrollAmount() const {
  nsSize effectiveScrollPortSize;

  if (GetVisualViewportSize() != mScrollPort.Size()) {
    // We want to use the visual viewport size if one is set.
    // The headers/footers adjustment is too complicated to do if there is a
    // visual viewport that differs from the layout viewport, this is probably
    // okay.
    effectiveScrollPortSize = GetVisualViewportSize();
  } else {
    // Reduce effective scrollport height by the height of any
    // fixed-pos/sticky-pos headers or footers
    effectiveScrollPortSize = GetScrollPortSizeExcludingHeadersAndFooters(
        mOuter, mIsRoot ? mOuter->PresShell()->GetRootFrame() : nullptr,
        mScrollPort);
  }

  nsSize lineScrollAmount = GetLineScrollAmount();

  // The page increment is the size of the page, minus the smaller of
  // 10% of the size or 2 lines.
  return nsSize(effectiveScrollPortSize.width -
                    std::min(effectiveScrollPortSize.width / 10,
                             2 * lineScrollAmount.width),
                effectiveScrollPortSize.height -
                    std::min(effectiveScrollPortSize.height / 10,
                             2 * lineScrollAmount.height));
}

/**
 * this code is resposible for restoring the scroll position back to some
 * saved position. if the user has not moved the scroll position manually
 * we keep scrolling down until we get to our original position. keep in
 * mind that content could incrementally be coming in. we only want to stop
 * when we reach our new position.
 */
void ScrollFrameHelper::ScrollToRestoredPosition() {
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

  // The layout offset we want to restore is the same as the visual offset
  // (for now, may change in bug 1499210), but clamped to the layout scroll
  // range (which can be a subset of the visual scroll range).
  // Note that we can't do the clamping when initializing mRestorePos in
  // RestoreState(), since the scrollable rect (which the clamping depends
  // on) can change over the course of the restoration process.
  nsPoint layoutRestorePos = GetLayoutScrollRange().ClampPoint(mRestorePos);
  nsPoint visualRestorePos = GetVisualScrollRange().ClampPoint(mRestorePos);

  // Continue restoring until both the layout and visual scroll positions
  // reach the destination. (Note that the two can only be different for
  // the root content document's root scroll frame, and when zoomed in).
  // This is necessary to avoid situations where the two offsets get stuck
  // at different values and nothing reconciles them (see bug 1519621 comment
  // 8).
  nsPoint logicalLayoutScrollPos = GetLogicalScrollPosition();

  SCROLLRESTORE_LOG(
      "%p: ScrollToRestoredPosition (mRestorePos=%s, mLastPos=%s, "
      "layoutRestorePos=%s, visualRestorePos=%s, "
      "logicalLayoutScrollPos=%s, "
      "GetLogicalVisualViewportOffset()=%s)\n",
      this, ToString(mRestorePos).c_str(), ToString(mLastPos).c_str(),
      ToString(layoutRestorePos).c_str(), ToString(visualRestorePos).c_str(),
      ToString(logicalLayoutScrollPos).c_str(),
      ToString(GetLogicalVisualViewportOffset()).c_str());

  // if we didn't move, we still need to restore
  if (GetLogicalVisualViewportOffset() == mLastPos ||
      logicalLayoutScrollPos == mLastPos) {
    // if our desired position is different to the scroll position, scroll.
    // remember that we could be incrementally loading so we may enter
    // and scroll many times.
    if (mRestorePos != mLastPos /* GetLogicalVisualViewportOffset() */ ||
        layoutRestorePos != logicalLayoutScrollPos) {
      LoadingState state = GetPageLoadingState();
      if (state == LoadingState::Stopped && !mOuter->IsSubtreeDirty()) {
        return;
      }
      nsPoint visualScrollToPos = visualRestorePos;
      nsPoint layoutScrollToPos = layoutRestorePos;
      if (!IsPhysicalLTR()) {
        // convert from logical to physical scroll position
        visualScrollToPos.x -=
            (GetVisualViewportSize().width - mScrolledFrame->GetRect().width);
        layoutScrollToPos.x -=
            (GetVisualViewportSize().width - mScrolledFrame->GetRect().width);
      }
      AutoWeakFrame weakFrame(mOuter);
      // It's very important to pass ScrollOrigin::Restore here, so
      // ScrollToWithOrigin won't clear out mRestorePos.
      ScrollToWithOrigin(layoutScrollToPos, ScrollMode::Instant,
                         ScrollOrigin::Restore, nullptr);
      if (!weakFrame.IsAlive()) {
        return;
      }
      if (mIsRoot) {
        mOuter->PresShell()->ScrollToVisual(
            visualScrollToPos, FrameMetrics::eRestore, ScrollMode::Instant);
      }
      if (state == LoadingState::Loading || mOuter->IsSubtreeDirty()) {
        // If we're trying to do a history scroll restore, then we want to
        // keep trying this until we succeed, because the page can be loading
        // incrementally. So re-get the scroll position for the next iteration,
        // it might not be exactly equal to mRestorePos due to rounding and
        // clamping.
        mLastPos = GetLogicalVisualViewportOffset();
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

auto ScrollFrameHelper::GetPageLoadingState() -> LoadingState {
  bool loadCompleted = false, stopped = false;
  nsCOMPtr<nsIDocShell> ds =
      mOuter->GetContent()->GetComposedDoc()->GetDocShell();
  if (ds) {
    nsCOMPtr<nsIContentViewer> cv;
    ds->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      loadCompleted = cv->GetLoadCompleted();
      stopped = cv->GetIsStopped();
    }
  }
  return loadCompleted
             ? (stopped ? LoadingState::Stopped : LoadingState::Loaded)
             : LoadingState::Loading;
}

ScrollFrameHelper::OverflowState ScrollFrameHelper::GetOverflowState() const {
  nsSize scrollportSize = mScrollPort.Size();
  nsSize childSize = GetScrolledRect().Size();

  OverflowState result = OverflowState::None;

  if (childSize.height > scrollportSize.height) {
    result |= OverflowState::Vertical;
  }

  if (childSize.width > scrollportSize.width) {
    result |= OverflowState::Horizontal;
  }

  return result;
}

nsresult ScrollFrameHelper::FireScrollPortEvent() {
  mAsyncScrollPortEvent.Forget();

  // TODO(emilio): why do we need the whole WillPaintObserver infrastructure and
  // can't use AddScriptRunner & co? I guess it made sense when we used
  // WillPaintObserver for scroll events too, or when this used to flush.
  //
  // Should we remove this?

  OverflowState overflowState = GetOverflowState();

  bool newVerticalOverflow = !!(overflowState & OverflowState::Vertical);
  bool vertChanged = mVerticalOverflow != newVerticalOverflow;

  bool newHorizontalOverflow = !!(overflowState & OverflowState::Horizontal);
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

  InternalScrollPortEvent event(
      true,
      (orient == InternalScrollPortEvent::eHorizontal ? mHorizontalOverflow
                                                      : mVerticalOverflow)
          ? eScrollPortOverflow
          : eScrollPortUnderflow,
      nullptr);
  event.mOrient = orient;

  RefPtr<nsIContent> content = mOuter->GetContent();
  RefPtr<nsPresContext> presContext = mOuter->PresContext();
  return EventDispatcher::Dispatch(content, presContext, &event);
}

void ScrollFrameHelper::PostScrollEndEvent() {
  if (mScrollEndEvent) {
    return;
  }

  // The ScrollEndEvent constructor registers itself with the refresh driver.
  mScrollEndEvent = new ScrollEndEvent(this);
}

void ScrollFrameHelper::FireScrollEndEvent() {
  MOZ_ASSERT(mOuter->GetContent());
  MOZ_ASSERT(mScrollEndEvent);
  mScrollEndEvent->Revoke();
  mScrollEndEvent = nullptr;

  nsContentUtils::DispatchEventOnlyToChrome(
      mOuter->GetContent()->OwnerDoc(), mOuter->GetContent(), u"scrollend"_ns,
      CanBubble::eYes, Cancelable::eNo);
}

void ScrollFrameHelper::ReloadChildFrames() {
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
          NS_ASSERTION(!mHScrollbarBox,
                       "Found multiple horizontal scrollbars?");
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

already_AddRefed<Element> ScrollFrameHelper::MakeScrollbar(
    NodeInfo* aNodeInfo, bool aVertical, AnonymousContentKey& aKey) {
  MOZ_ASSERT(aNodeInfo);
  MOZ_ASSERT(
      aNodeInfo->Equals(nsGkAtoms::scrollbar, nullptr, kNameSpaceID_XUL));

  static constexpr nsLiteralString kOrientValues[2] = {
      u"horizontal"_ns,
      u"vertical"_ns,
  };

  aKey = AnonymousContentKey::Type_Scrollbar;
  if (aVertical) {
    aKey |= AnonymousContentKey::Flag_Vertical;
  }

  RefPtr<Element> e;
  NS_TrustedNewXULElement(getter_AddRefs(e), do_AddRef(aNodeInfo));

#ifdef DEBUG
  // Scrollbars can get restyled by theme changes.  Whether such a restyle
  // will actually reconstruct them correctly if it involves a frame
  // reconstruct... I don't know.  :(
  e->SetProperty(nsGkAtoms::restylableAnonymousNode,
                 reinterpret_cast<void*>(true));
#endif  // DEBUG

  e->SetAttr(kNameSpaceID_None, nsGkAtoms::orient, kOrientValues[aVertical],
             false);
  e->SetAttr(kNameSpaceID_None, nsGkAtoms::clickthrough, u"always"_ns, false);

  if (mIsRoot) {
    e->SetProperty(nsGkAtoms::docLevelNativeAnonymousContent,
                   reinterpret_cast<void*>(true));
    e->SetAttr(kNameSpaceID_None, nsGkAtoms::root_, u"true"_ns, false);

    // Don't bother making style caching take [root="true"] styles into account.
    aKey = AnonymousContentKey::None;
  }

  return e.forget();
}

bool ScrollFrameHelper::IsForTextControlWithNoScrollbars() const {
  // FIXME(emilio): we should probably make the scroller inside <input> an
  // internal pseudo-element, and then this would be simpler.
  //
  // Also, this could just use scrollbar-width these days.
  auto* content = mOuter->GetContent();
  if (!content) {
    return false;
  }
  auto* input = content->GetClosestNativeAnonymousSubtreeRootParent();
  return input && input->IsHTMLElement(nsGkAtoms::input);
}

auto ScrollFrameHelper::GetCurrentAnonymousContent() const
    -> EnumSet<AnonymousContentType> {
  EnumSet<AnonymousContentType> result;
  if (mHScrollbarContent) {
    result += AnonymousContentType::HorizontalScrollbar;
  }
  if (mVScrollbarContent) {
    result += AnonymousContentType::VerticalScrollbar;
  }
  if (mResizerContent) {
    result += AnonymousContentType::Resizer;
  }
  return result;
}

auto ScrollFrameHelper::GetNeededAnonymousContent() const
    -> EnumSet<AnonymousContentType> {
  nsPresContext* pc = mOuter->PresContext();

  // Don't create scrollbars if we're an SVG document being used as an image,
  // or if we're printing/print previewing.
  // (In the printing case, we allow scrollbars if this is the child of the
  // viewport & paginated scrolling is enabled, because then we must be the
  // scroll frame for the print preview window, & that does need scrollbars.)
  if (pc->Document()->IsBeingUsedAsImage() ||
      (!pc->IsDynamic() && !(mIsRoot && pc->HasPaginatedScrolling()))) {
    return {};
  }

  if (IsForTextControlWithNoScrollbars()) {
    return {};
  }

  EnumSet<AnonymousContentType> result;
  // If we're the scrollframe for the root, then we want to construct our
  // scrollbar frames no matter what.  That way later dynamic changes to
  // propagated overflow styles will show or hide scrollbars on the viewport
  // without requiring frame reconstruction of the viewport (good!).
  //
  // TODO(emilio): Figure out if we can remove this special-case now that we
  // have more targeted optimizations.
  if (mIsRoot) {
    result += AnonymousContentType::HorizontalScrollbar;
    result += AnonymousContentType::VerticalScrollbar;
    // If scrollbar-width is none, don't generate scrollbars.
  } else if (mOuter->StyleUIReset()->ScrollbarWidth() !=
             StyleScrollbarWidth::None) {
    nsIScrollableFrame* scrollable = do_QueryFrame(mOuter);
    ScrollStyles styles = scrollable->GetScrollStyles();
    if (styles.mHorizontal != StyleOverflow::Hidden) {
      result += AnonymousContentType::HorizontalScrollbar;
    }
    if (styles.mVertical != StyleOverflow::Hidden) {
      result += AnonymousContentType::VerticalScrollbar;
    }

    // If we have scrollbar-gutter, construct the scrollbar frames to query its
    // size to reserve the gutter space at the inline start or end edges.
    if (mOuter->StyleDisplay()->mScrollbarGutter &
        StyleScrollbarGutter::STABLE) {
      result += mOuter->GetWritingMode().IsVertical()
                    ? AnonymousContentType::HorizontalScrollbar
                    : AnonymousContentType::VerticalScrollbar;
    }
  }

  // Check if the frame is resizable. Note:
  // "The effect of the resize property on generated content is undefined.
  //  Implementations should not apply the resize property to generated
  //  content." [1]
  // For info on what is generated content, see [2].
  // [1]: https://drafts.csswg.org/css-ui/#resize
  // [2]: https://www.w3.org/TR/CSS2/generate.html#content
  auto resizeStyle = mOuter->StyleDisplay()->mResize;
  if (resizeStyle != StyleResize::None &&
      !mOuter->HasAnyStateBits(NS_FRAME_GENERATED_CONTENT)) {
    result += AnonymousContentType::Resizer;
  }

  return result;
}

nsresult ScrollFrameHelper::CreateAnonymousContent(
    nsTArray<nsIAnonymousContentCreator::ContentInfo>& aElements) {
  typedef nsIAnonymousContentCreator::ContentInfo ContentInfo;

  nsPresContext* presContext = mOuter->PresContext();
  nsNodeInfoManager* nodeInfoManager =
      presContext->Document()->NodeInfoManager();

  auto neededAnonContent = GetNeededAnonymousContent();
  if (neededAnonContent.isEmpty()) {
    return NS_OK;
  }

  {
    RefPtr<NodeInfo> nodeInfo = nodeInfoManager->GetNodeInfo(
        nsGkAtoms::scrollbar, nullptr, kNameSpaceID_XUL, nsINode::ELEMENT_NODE);
    NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

    if (neededAnonContent.contains(AnonymousContentType::HorizontalScrollbar)) {
      AnonymousContentKey key;
      mHScrollbarContent = MakeScrollbar(nodeInfo, /* aVertical */ false, key);
      aElements.AppendElement(ContentInfo(mHScrollbarContent, key));
    }

    if (neededAnonContent.contains(AnonymousContentType::VerticalScrollbar)) {
      AnonymousContentKey key;
      mVScrollbarContent = MakeScrollbar(nodeInfo, /* aVertical */ true, key);
      aElements.AppendElement(ContentInfo(mVScrollbarContent, key));
    }
  }

  if (neededAnonContent.contains(AnonymousContentType::Resizer)) {
    MOZ_ASSERT(!mIsRoot, "Root scroll frame shouldn't be resizable");

    RefPtr<NodeInfo> nodeInfo;
    nodeInfo = nodeInfoManager->GetNodeInfo(
        nsGkAtoms::resizer, nullptr, kNameSpaceID_XUL, nsINode::ELEMENT_NODE);
    NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

    NS_TrustedNewXULElement(getter_AddRefs(mResizerContent), nodeInfo.forget());

    nsAutoString dir;
    switch (mOuter->StyleDisplay()->mResize) {
      case StyleResize::Horizontal:
        if (IsScrollbarOnRight()) {
          dir.AssignLiteral("right");
        } else {
          dir.AssignLiteral("left");
        }
        break;
      case StyleResize::Vertical:
        dir.AssignLiteral("bottom");
        if (!IsScrollbarOnRight()) {
          mResizerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::flip, u""_ns,
                                   false);
        }
        break;
      case StyleResize::Both:
        if (IsScrollbarOnRight()) {
          dir.AssignLiteral("bottomright");
        } else {
          dir.AssignLiteral("bottomleft");
        }
        break;
      default:
        NS_WARNING("only resizable types should have resizers");
    }
    mResizerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, dir, false);
    mResizerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::clickthrough,
                             u"always"_ns, false);
    aElements.AppendElement(mResizerContent);
  }

  if (neededAnonContent.contains(AnonymousContentType::HorizontalScrollbar) &&
      neededAnonContent.contains(AnonymousContentType::VerticalScrollbar)) {
    AnonymousContentKey key = AnonymousContentKey::Type_ScrollCorner;

    RefPtr<NodeInfo> nodeInfo =
        nodeInfoManager->GetNodeInfo(nsGkAtoms::scrollcorner, nullptr,
                                     kNameSpaceID_XUL, nsINode::ELEMENT_NODE);
    NS_TrustedNewXULElement(getter_AddRefs(mScrollCornerContent),
                            nodeInfo.forget());
    if (mIsRoot) {
      mScrollCornerContent->SetProperty(
          nsGkAtoms::docLevelNativeAnonymousContent,
          reinterpret_cast<void*>(true));
      mScrollCornerContent->SetAttr(kNameSpaceID_None, nsGkAtoms::root_,
                                    u"true"_ns, false);

      // Don't bother making style caching take [root="true"] styles into
      // account.
      key = AnonymousContentKey::None;
    }
    aElements.AppendElement(ContentInfo(mScrollCornerContent, key));
  }

  // Don't cache styles if we are a child of a <select> element, since we have
  // some UA style sheet rules that depend on the <select>'s attributes.
  if (mOuter->GetContent()->IsHTMLElement(nsGkAtoms::select)) {
    for (auto& info : aElements) {
      info.mKey = AnonymousContentKey::None;
    }
  }

  return NS_OK;
}

void ScrollFrameHelper::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
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

void ScrollFrameHelper::Destroy(PostDestroyData& aPostDestroyData) {
  if (mIsRoot) {
    mOuter->PresShell()->ResetVisualViewportOffset();
  }

  mAnchor.Destroy();

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
  if (gScrollFrameActivityTracker && gScrollFrameActivityTracker->IsEmpty()) {
    gScrollFrameActivityTracker = nullptr;
  }

  if (mScrollActivityTimer) {
    mScrollActivityTimer->Cancel();
    mScrollActivityTimer = nullptr;
  }
  RemoveObservers();
}

void ScrollFrameHelper::RemoveObservers() {
  if (mAsyncScroll) {
    mAsyncScroll->RemoveObserver();
    mAsyncScroll = nullptr;
  }
  if (mAsyncSmoothMSDScroll) {
    mAsyncSmoothMSDScroll->RemoveObserver();
    mAsyncSmoothMSDScroll = nullptr;
  }
}

/**
 * Called when we want to update the scrollbar position, either because
 * scrolling happened or the user moved the scrollbar position and we need to
 * undo that (e.g., when the user clicks to scroll and we're using smooth
 * scrolling, so we need to put the thumb back to its initial position for the
 * start of the smooth sequence).
 */
void ScrollFrameHelper::UpdateScrollbarPosition() {
  AutoWeakFrame weakFrame(mOuter);
  mFrameIsUpdatingScrollbar = true;

  nsPoint pt = GetScrollPosition();
  nsRect scrollRange = GetVisualScrollRange();

  if (gfxPlatform::UseDesktopZoomingScrollbars()) {
    pt = GetVisualViewportOffset();
    scrollRange = GetScrollRangeForUserInputEvents();
  }

  if (mVScrollbarBox) {
    SetCoordAttribute(mVScrollbarBox->GetContent()->AsElement(),
                      nsGkAtoms::curpos, pt.y - scrollRange.y);
    if (!weakFrame.IsAlive()) {
      return;
    }
  }
  if (mHScrollbarBox) {
    SetCoordAttribute(mHScrollbarBox->GetContent()->AsElement(),
                      nsGkAtoms::curpos, pt.x - scrollRange.x);
    if (!weakFrame.IsAlive()) {
      return;
    }
  }

  mFrameIsUpdatingScrollbar = false;
}

void ScrollFrameHelper::CurPosAttributeChanged(nsIContent* aContent,
                                               bool aDoScroll) {
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
  if (mFrameIsUpdatingScrollbar) return;

  nsRect scrollRange = GetVisualScrollRange();

  nsPoint current = GetScrollPosition() - scrollRange.TopLeft();

  if (gfxPlatform::UseDesktopZoomingScrollbars()) {
    scrollRange = GetScrollRangeForUserInputEvents();
    current = GetVisualViewportOffset() - scrollRange.TopLeft();
  }

  nsPoint dest;
  nsRect allowedRange;
  dest.x = GetCoordAttribute(mHScrollbarBox, nsGkAtoms::curpos, current.x,
                             &allowedRange.x, &allowedRange.width);
  dest.y = GetCoordAttribute(mVScrollbarBox, nsGkAtoms::curpos, current.y,
                             &allowedRange.y, &allowedRange.height);
  current += scrollRange.TopLeft();
  dest += scrollRange.TopLeft();
  allowedRange += scrollRange.TopLeft();

  // Don't try to scroll if we're already at an acceptable place.
  // Don't call Contains here since Contains returns false when the point is
  // on the bottom or right edge of the rectangle.
  if (allowedRange.ClampPoint(current) == current) {
    return;
  }

  if (mScrollbarActivity &&
      (mHasHorizontalScrollbar || mHasVerticalScrollbar)) {
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
                       isSmooth ? ScrollMode::Smooth : ScrollMode::Instant,
                       ScrollOrigin::Scrollbars, &allowedRange);
  }
  // 'this' might be destroyed here
}

/* ============= Scroll events ========== */

ScrollFrameHelper::ScrollEvent::ScrollEvent(ScrollFrameHelper* aHelper,
                                            bool aDelayed)
    : Runnable("ScrollFrameHelper::ScrollEvent"), mHelper(aHelper) {
  mHelper->mOuter->PresContext()->RefreshDriver()->PostScrollEvent(this,
                                                                   aDelayed);
}

// TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230, bug 1535398)
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
ScrollFrameHelper::ScrollEvent::Run() {
  if (mHelper) {
    mHelper->FireScrollEvent();
  }
  return NS_OK;
}

ScrollFrameHelper::ScrollEndEvent::ScrollEndEvent(ScrollFrameHelper* aHelper)
    : Runnable("ScrollFrameHelper::ScrollEndEvent"), mHelper(aHelper) {
  mHelper->mOuter->PresContext()->RefreshDriver()->PostScrollEvent(this);
}

NS_IMETHODIMP
ScrollFrameHelper::ScrollEndEvent::Run() {
  if (mHelper) {
    mHelper->FireScrollEndEvent();
  }
  return NS_OK;
}

void ScrollFrameHelper::FireScrollEvent() {
  RefPtr<nsIContent> content = mOuter->GetContent();
  RefPtr<nsPresContext> presContext = mOuter->PresContext();
  AUTO_PROFILER_TRACING_MARKER_DOCSHELL("Paint", "FireScrollEvent", GRAPHICS,
                                        presContext->GetDocShell());

  MOZ_ASSERT(mScrollEvent);
  mScrollEvent->Revoke();
  mScrollEvent = nullptr;

  // If event handling is suppressed, keep posting the scroll event to the
  // refresh driver until it is unsuppressed. The event is marked as delayed so
  // that the refresh driver does not continue ticking.
  if (content->GetComposedDoc() &&
      content->GetComposedDoc()->EventHandlingSuppressed()) {
    content->GetComposedDoc()->SetHasDelayedRefreshEvent();
    PostScrollEvent(/* aDelayed = */ true);
    return;
  }

  bool oldProcessing = mProcessingScrollEvent;
  AutoWeakFrame weakFrame(mOuter);
  auto RestoreProcessingScrollEvent = mozilla::MakeScopeExit([&] {
    if (weakFrame.IsAlive()) {  // Otherwise `this` will be dead too.
      mProcessingScrollEvent = oldProcessing;
    }
  });

  mProcessingScrollEvent = true;

  WidgetGUIEvent event(true, eScroll, nullptr);
  nsEventStatus status = nsEventStatus_eIgnore;
  // Fire viewport scroll events at the document (where they
  // will bubble to the window)
  mozilla::layers::ScrollLinkedEffectDetector detector(
      content->GetComposedDoc(),
      presContext->RefreshDriver()->MostRecentRefresh());
  if (mIsRoot) {
    if (RefPtr<Document> doc = content->GetUncomposedDoc()) {
      // TODO: Bug 1506441
      EventDispatcher::Dispatch(MOZ_KnownLive(ToSupports(doc)), presContext,
                                &event, nullptr, &status);
    }
  } else {
    // scroll events fired at elements don't bubble (although scroll events
    // fired at documents do, to the window)
    event.mFlags.mBubbles = false;
    EventDispatcher::Dispatch(content, presContext, &event, nullptr, &status);
  }
}

void ScrollFrameHelper::PostScrollEvent(bool aDelayed) {
  if (mScrollEvent) {
    return;
  }

  // The ScrollEvent constructor registers itself with the refresh driver.
  mScrollEvent = new ScrollEvent(this, aDelayed);
}

// TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230, bug 1535398)
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
ScrollFrameHelper::AsyncScrollPortEvent::Run() {
  return mHelper ? mHelper->FireScrollPortEvent() : NS_OK;
}

bool nsXULScrollFrame::AddHorizontalScrollbar(nsBoxLayoutState& aState,
                                              bool aOnBottom) {
  if (!mHelper.mHScrollbarBox) {
    return true;
  }

  return AddRemoveScrollbar(aState, aOnBottom, true, true);
}

bool nsXULScrollFrame::AddVerticalScrollbar(nsBoxLayoutState& aState,
                                            bool aOnRight) {
  if (!mHelper.mVScrollbarBox) {
    return true;
  }

  return AddRemoveScrollbar(aState, aOnRight, false, true);
}

void nsXULScrollFrame::RemoveHorizontalScrollbar(nsBoxLayoutState& aState,
                                                 bool aOnBottom) {
  // removing a scrollbar should always fit
  DebugOnly<bool> result = AddRemoveScrollbar(aState, aOnBottom, true, false);
  NS_ASSERTION(result, "Removing horizontal scrollbar failed to fit??");
}

void nsXULScrollFrame::RemoveVerticalScrollbar(nsBoxLayoutState& aState,
                                               bool aOnRight) {
  // removing a scrollbar should always fit
  DebugOnly<bool> result = AddRemoveScrollbar(aState, aOnRight, false, false);
  NS_ASSERTION(result, "Removing vertical scrollbar failed to fit??");
}

bool nsXULScrollFrame::AddRemoveScrollbar(nsBoxLayoutState& aState,
                                          bool aOnRightOrBottom,
                                          bool aHorizontal, bool aAdd) {
  if (aHorizontal) {
    if (!mHelper.mHScrollbarBox) {
      return false;
    }

    nsSize hSize = mHelper.mHScrollbarBox->GetXULPrefSize(aState);
    nsIFrame::AddXULMargin(mHelper.mHScrollbarBox, hSize);

    nsRect newScrollPort = mHelper.ScrollPort();
    MOZ_ASSERT(newScrollPort.height != NS_UNCONSTRAINEDSIZE,
               "The scroll port shouldn't have unconstrained height!");

    // Removing a scrollbar should always fit.
    const bool fit = !aAdd || newScrollPort.height >= hSize.height;
    if (fit) {
      if (aAdd) {
        newScrollPort.height -= hSize.height;
      } else {
        newScrollPort.height += hSize.height;
      }
      // No need to adjust newScrollPort.y because we don't support scrollbar on
      // top side.
      MOZ_ASSERT(aOnRightOrBottom,
                 "We don't support a horizontal scrollbar on top side!");
      mHelper.SetScrollPort(newScrollPort);
    }

    const bool showHScrollbar = aAdd && fit;
    mHelper.mHasHorizontalScrollbar = showHScrollbar;
    ScrollFrameHelper::SetScrollbarVisibility(mHelper.mHScrollbarBox,
                                              showHScrollbar);
    return fit;
  } else {
    if (!mHelper.mVScrollbarBox) {
      return false;
    }

    nsSize vSize = mHelper.mVScrollbarBox->GetXULPrefSize(aState);
    nsIFrame::AddXULMargin(mHelper.mVScrollbarBox, vSize);

    nsRect newScrollPort = mHelper.ScrollPort();
    MOZ_ASSERT(newScrollPort.width != NS_UNCONSTRAINEDSIZE,
               "The scroll port shouldn't have unconstrained width!");

    // Removing a scrollbar should always fit.
    const bool fit = !aAdd || newScrollPort.width >= vSize.width;
    const bool scrollbarOnLeft = !aOnRightOrBottom;
    if (fit) {
      if (aAdd) {
        newScrollPort.width -= vSize.width;
        if (scrollbarOnLeft) {
          newScrollPort.x += vSize.width;
        }
      } else {
        newScrollPort.width += vSize.width;
        if (scrollbarOnLeft) {
          newScrollPort.x -= vSize.width;
        }
      }
      mHelper.SetScrollPort(newScrollPort);
    }

    const bool showVScrollbar = aAdd && fit;
    mHelper.mHasVerticalScrollbar = showVScrollbar;
    ScrollFrameHelper::SetScrollbarVisibility(mHelper.mVScrollbarBox,
                                              showVScrollbar);
    return fit;
  }
}

void nsXULScrollFrame::LayoutScrollArea(nsBoxLayoutState& aState,
                                        const nsPoint& aScrollPosition) {
  ReflowChildFlags oldflags = aState.LayoutFlags();
  nsRect childRect(mHelper.ScrollPort().TopLeft() - aScrollPosition,
                   mHelper.ScrollPort().Size());
  ReflowChildFlags flags = ReflowChildFlags::NoMoveView;

  nsSize minSize = mHelper.mScrolledFrame->GetXULMinSize(aState);

  if (minSize.height > childRect.height) childRect.height = minSize.height;

  if (minSize.width > childRect.width) childRect.width = minSize.width;

  // TODO: Handle transformed children that inherit perspective
  // from this frame. See AdjustForPerspective for how we handle
  // this for HTML scroll frames.

  aState.SetLayoutFlags(flags);
  ClampAndSetBounds(aState, childRect, aScrollPosition);
  mHelper.mScrolledFrame->XULLayout(aState);

  childRect = mHelper.mScrolledFrame->GetRect();

  const nscoord scrollPortWidth = mHelper.ScrollPort().width;
  const nscoord scrollPortHeight = mHelper.ScrollPort().height;
  if (childRect.width < scrollPortWidth ||
      childRect.height < scrollPortHeight) {
    childRect.width = std::max(childRect.width, scrollPortWidth);
    childRect.height = std::max(childRect.height, scrollPortHeight);

    // remove overflow areas when we update the bounds,
    // because we've already accounted for it
    // REVIEW: Have we accounted for both?
    ClampAndSetBounds(aState, childRect, aScrollPosition, true);
  }

  aState.SetLayoutFlags(oldflags);
}

void ScrollFrameHelper::PostOverflowEvent() {
  if (mAsyncScrollPortEvent.IsPending()) {
    return;
  }

  OverflowState overflowState = GetOverflowState();

  bool newVerticalOverflow = !!(overflowState & OverflowState::Vertical);
  bool vertChanged = mVerticalOverflow != newVerticalOverflow;

  bool newHorizontalOverflow = !!(overflowState & OverflowState::Horizontal);
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

nsIFrame* ScrollFrameHelper::GetFrameForStyle() const {
  nsIFrame* styleFrame = nullptr;
  if (mIsRoot) {
    if (const Element* rootElement =
            mOuter->PresContext()->Document()->GetRootElement()) {
      styleFrame = rootElement->GetPrimaryFrame();
    }
  } else {
    styleFrame = mOuter;
  }

  return styleFrame;
}

bool ScrollFrameHelper::NeedsScrollSnap() const {
  nsIFrame* scrollSnapFrame = GetFrameForStyle();
  if (!scrollSnapFrame) {
    return false;
  }
  return scrollSnapFrame->StyleDisplay()->mScrollSnapType.strictness !=
         StyleScrollSnapStrictness::None;
}

bool ScrollFrameHelper::IsScrollbarOnRight() const {
  nsPresContext* presContext = mOuter->PresContext();

  // The position of the scrollbar in top-level windows depends on the pref
  // layout.scrollbar.side. For non-top-level elements, it depends only on the
  // directionaliy of the element (equivalent to a value of "1" for the pref).
  if (!mIsRoot) {
    return IsPhysicalLTR();
  }
  switch (presContext->GetCachedIntPref(kPresContext_ScrollbarSide)) {
    default:
    case 0:  // UI directionality
      return presContext->GetCachedIntPref(kPresContext_BidiDirection) ==
             IBMBIDI_TEXTDIRECTION_LTR;
    case 1:  // Document / content directionality
      return IsPhysicalLTR();
    case 2:  // Always right
      return true;
    case 3:  // Always left
      return false;
  }
}

bool ScrollFrameHelper::IsScrollingActive() const {
  const nsStyleDisplay* disp = mOuter->StyleDisplay();
  if (disp->mWillChange.bits & StyleWillChangeBits::SCROLL) {
    return true;
  }

  nsIContent* content = mOuter->GetContent();
  return mHasBeenScrolledRecently || IsAlwaysActive() ||
         DisplayPortUtils::HasDisplayPort(content) ||
         nsContentUtils::HasScrollgrab(content);
}

/**
 * Reflow the scroll area if it needs it and return its size. Also determine if
 * the reflow will cause any of the scrollbars to need to be reflowed.
 */
nsresult nsXULScrollFrame::XULLayout(nsBoxLayoutState& aState) {
  bool scrollbarRight = IsScrollbarOnRight();
  bool scrollbarBottom = true;

  // get the content rect
  nsRect clientRect(0, 0, 0, 0);
  GetXULClientRect(clientRect);

  const nsRect oldScrollPort = mHelper.ScrollPort();
  nsPoint oldScrollPosition = mHelper.GetLogicalScrollPosition();

  // the scroll area size starts off as big as our content area
  mHelper.SetScrollPort(clientRect);

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

  ScrollStyles styles = GetScrollStyles();

  // Look at our style do we always have vertical or horizontal scrollbars?
  if (styles.mHorizontal == StyleOverflow::Scroll)
    mHelper.mHasHorizontalScrollbar = true;
  if (styles.mVertical == StyleOverflow::Scroll)
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
  if (styles.mVertical != StyleOverflow::Scroll) {
    // These are only good until the call to LayoutScrollArea.
    nsRect scrolledRect = mHelper.GetScrolledRect();

    // There are two cases to consider
    if (scrolledRect.height <= mHelper.ScrollPort().height ||
        styles.mVertical != StyleOverflow::Auto) {
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
  if (styles.mHorizontal != StyleOverflow::Scroll) {
    // These are only good until the call to LayoutScrollArea.
    nsRect scrolledRect = mHelper.GetScrolledRect();

    // if the child is wider that the scroll area
    // and we don't have a scrollbar add one.
    if ((scrolledRect.width > mHelper.ScrollPort().width) &&
        styles.mHorizontal == StyleOverflow::Auto) {
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

          if (styles.mVertical == StyleOverflow::Auto &&
              !mHelper.mHasVerticalScrollbar &&
              scrolledRect.height > mHelper.ScrollPort().height) {
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
    GetScrollbarMetrics(aState, mHelper.mHScrollbarBox, &hMinSize, nullptr);
  }
  nsSize vMinSize(0, 0);
  if (mHelper.mVScrollbarBox && mHelper.mHasVerticalScrollbar) {
    GetScrollbarMetrics(aState, mHelper.mVScrollbarBox, &vMinSize, nullptr);
  }

  // Disable scrollbars that are too small
  // Disable horizontal scrollbar first. If we have to disable only one
  // scrollbar, we'd rather keep the vertical scrollbar.
  // Note that we always give horizontal scrollbars their preferred height,
  // never their min-height. So check that there's room for the preferred
  // height.
  if (mHelper.mHasHorizontalScrollbar &&
      (hMinSize.width > clientRect.width - vMinSize.width ||
       hMinSize.height > clientRect.height)) {
    RemoveHorizontalScrollbar(aState, scrollbarBottom);
    needsLayout = true;
  }
  // Now disable vertical scrollbar if necessary
  if (mHelper.mHasVerticalScrollbar &&
      (vMinSize.height > clientRect.height - hMinSize.height ||
       vMinSize.width > clientRect.width)) {
    RemoveVerticalScrollbar(aState, scrollbarRight);
    needsLayout = true;
  }

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
    nsBoxLayoutState resizeState(aState);
    LayoutScrollArea(resizeState, oldScrollPosition);
  }

  if (!mHelper.mSuppressScrollbarUpdate) {
    mHelper.LayoutScrollbars(aState, clientRect, oldScrollPort);
  }
  if (!mHelper.mPostedReflowCallback) {
    // Make sure we'll try scrolling to restored position
    PresShell()->PostReflowCallback(&mHelper);
    mHelper.mPostedReflowCallback = true;
  }
  if (!HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    mHelper.mHadNonInitialReflow = true;
  }

  mHelper.UpdateSticky();

  // Set up overflow areas for block frames for the benefit of
  // text-overflow.
  nsIFrame* f = mHelper.mScrolledFrame->GetContentInsertionFrame();
  if (f && f->IsBlockFrameOrSubclass()) {
    nsRect origRect = f->GetRect();
    nsRect clippedRect = origRect;
    clippedRect.MoveBy(mHelper.ScrollPort().TopLeft());
    clippedRect.IntersectRect(clippedRect, mHelper.ScrollPort());
    OverflowAreas overflow = f->GetOverflowAreas();
    f->FinishAndStoreOverflow(overflow, clippedRect.Size());
    clippedRect.MoveTo(origRect.TopLeft());
    f->SetRect(clippedRect);
  }

  mHelper.UpdatePrevScrolledRect();

  mHelper.PostOverflowEvent();
  return NS_OK;
}

void ScrollFrameHelper::FinishReflowForScrollbar(Element* aElement,
                                                 nscoord aMinXY, nscoord aMaxXY,
                                                 nscoord aCurPosXY,
                                                 nscoord aPageIncrement,
                                                 nscoord aIncrement) {
  // Scrollbars assume zero is the minimum position, so translate for them.
  SetCoordAttribute(aElement, nsGkAtoms::curpos, aCurPosXY - aMinXY);
  SetScrollbarEnabled(aElement, aMaxXY - aMinXY);
  SetCoordAttribute(aElement, nsGkAtoms::maxpos, aMaxXY - aMinXY);
  SetCoordAttribute(aElement, nsGkAtoms::pageincrement, aPageIncrement);
  SetCoordAttribute(aElement, nsGkAtoms::increment, aIncrement);
}

class MOZ_RAII AutoMinimumScaleSizeChangeDetector final {
 public:
  explicit AutoMinimumScaleSizeChangeDetector(
      ScrollFrameHelper* aScrollFrameHelper)
      : mHelper(aScrollFrameHelper) {
    MOZ_ASSERT(mHelper);
    MOZ_ASSERT(mHelper->mIsRoot);

    mPreviousMinimumScaleSize = aScrollFrameHelper->mMinimumScaleSize;
    mPreviousIsUsingMinimumScaleSize =
        aScrollFrameHelper->mIsUsingMinimumScaleSize;
  }
  ~AutoMinimumScaleSizeChangeDetector() {
    if (mPreviousMinimumScaleSize != mHelper->mMinimumScaleSize ||
        mPreviousIsUsingMinimumScaleSize != mHelper->mIsUsingMinimumScaleSize) {
      mHelper->mMinimumScaleSizeChanged = true;
    }
  }

 private:
  ScrollFrameHelper* mHelper;

  nsSize mPreviousMinimumScaleSize;
  bool mPreviousIsUsingMinimumScaleSize;
};

nsSize ScrollFrameHelper::TrueOuterSize(nsDisplayListBuilder* aBuilder) const {
  if (!mOuter->PresShell()->UsesMobileViewportSizing()) {
    return mOuter->GetSize();
  }

  RefPtr<MobileViewportManager> manager =
      mOuter->PresShell()->GetMobileViewportManager();
  MOZ_ASSERT(manager);

  LayoutDeviceIntSize displaySize = manager->DisplaySize();

  MOZ_ASSERT(aBuilder);
  // In case of WebRender, we expand the outer size to include the dynamic
  // toolbar area here.
  // In case of non WebRender, we expand the size dynamically in
  // MoveScrollbarForLayerMargin in AsyncCompositionManager.cpp.
  WebRenderLayerManager* layerManager = aBuilder->GetWidgetLayerManager();
  if (layerManager) {
    displaySize.height += ViewAs<LayoutDevicePixel>(
        mOuter->PresContext()->GetDynamicToolbarMaxHeight(),
        PixelCastJustification::LayoutDeviceIsScreenForBounds);
  }

  return LayoutDeviceSize::ToAppUnits(
      displaySize, mOuter->PresContext()->AppUnitsPerDevPixel());
}

void ScrollFrameHelper::UpdateMinimumScaleSize(
    const nsRect& aScrollableOverflow, const nsSize& aICBSize) {
  MOZ_ASSERT(mIsRoot);

  AutoMinimumScaleSizeChangeDetector minimumScaleSizeChangeDetector(this);

  mIsUsingMinimumScaleSize = false;

  if (!mOuter->PresShell()->UsesMobileViewportSizing()) {
    return;
  }

  nsPresContext* pc = mOuter->PresContext();
  MOZ_ASSERT(pc->IsRootContentDocumentCrossProcess(),
             "The pres context should be for the root content document");

  RefPtr<MobileViewportManager> manager =
      mOuter->PresShell()->GetMobileViewportManager();
  MOZ_ASSERT(manager);

  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      manager->DisplaySize(),
      PixelCastJustification::LayoutDeviceIsScreenForBounds);
  if (displaySize.width == 0 || displaySize.height == 0) {
    return;
  }
  if (aScrollableOverflow.IsEmpty()) {
    // Bail if the scrollable overflow rect is empty, as we're going to be
    // dividing by it.
    return;
  }

  Document* doc = pc->Document();
  MOZ_ASSERT(doc, "The document should be valid");
  if (doc->GetFullscreenElement()) {
    // Don't use the minimum scale size in the case of fullscreen state.
    // FIXME: 1508177: We will no longer need this.
    return;
  }

  nsViewportInfo viewportInfo = doc->GetViewportInfo(displaySize);
  if (!viewportInfo.IsZoomAllowed()) {
    // Don't apply the minimum scale size if user-scalable=no is specified.
    return;
  }

  // The intrinsic minimum scale is the scale that fits the entire content
  // width into the visual viewport.
  CSSToScreenScale intrinsicMinScale(
      displaySize.width / CSSRect::FromAppUnits(aScrollableOverflow).XMost());

  // The scale used to compute the minimum-scale size is the larger of the
  // intrinsic minimum and the min-scale from the meta viewport tag.
  CSSToScreenScale minScale =
      std::max(intrinsicMinScale, viewportInfo.GetMinZoom());

  // The minimum-scale size is the size of the visual viewport when zoomed
  // to be the minimum scale.
  mMinimumScaleSize = CSSSize::ToAppUnits(ScreenSize(displaySize) / minScale);

  // Ensure the minimum-scale size is never smaller than the ICB size.
  // That could happen if a page has a meta viewport tag with large explicitly
  // specified viewport dimensions (making the ICB large) and also a large
  // minimum scale (making the min-scale size small).
  mMinimumScaleSize = Max(aICBSize, mMinimumScaleSize);

  mIsUsingMinimumScaleSize = true;
}

bool ScrollFrameHelper::ReflowFinished() {
  mPostedReflowCallback = false;

  TryScheduleScrollAnimations();

  if (mIsRoot) {
    if (mMinimumScaleSizeChanged &&
        mOuter->PresShell()->UsesMobileViewportSizing() &&
        !mOuter->PresShell()->IsResolutionUpdatedByApz()) {
      PresShell* presShell = mOuter->PresShell();
      RefPtr<MobileViewportManager> manager =
          presShell->GetMobileViewportManager();
      MOZ_ASSERT(manager);

      manager->ShrinkToDisplaySizeIfNeeded();
      mMinimumScaleSizeChanged = false;
    }

    if (!UsesOverlayScrollbars()) {
      // Layout scrollbars may have added or removed during reflow, so let's
      // update the visual viewport accordingly. Note that this may be a no-op
      // because we might have recomputed the visual viewport size during the
      // reflow itself, just before laying out the fixed-pos items. But there
      // might be cases where that code doesn't run, so this is a sort of
      // backstop to ensure we do that recomputation.
      if (RefPtr<MobileViewportManager> manager =
              mOuter->PresShell()->GetMobileViewportManager()) {
        manager->UpdateVisualViewportSizeForPotentialScrollbarChange();
      }
    }

#if defined(MOZ_WIDGET_ANDROID)
    const bool hasVerticalOverflow =
        GetOverflowState() & OverflowState::Vertical &&
        GetScrollStylesFromFrame().mVertical != StyleOverflow::Hidden;
    if (!mFirstReflow && mHasVerticalOverflowForDynamicToolbar &&
        !hasVerticalOverflow) {
      mOuter->PresShell()->MaybeNotifyShowDynamicToolbar();
    }
    mHasVerticalOverflowForDynamicToolbar = hasVerticalOverflow;
#endif  // defined(MOZ_WIDGET_ANDROID)
  }

  bool doScroll = true;
  if (mOuter->IsSubtreeDirty()) {
    // We will get another call after the next reflow and scrolling
    // later is less janky.
    doScroll = false;
  }

  if (mFirstReflow) {
    nsPoint currentScrollPos = GetScrollPosition();
    if (!mScrollUpdates.IsEmpty() &&
        mScrollUpdates.LastElement().GetOrigin() == ScrollOrigin::None &&
        currentScrollPos != nsPoint()) {
      // With frame reconstructions, the reconstructed frame may have a nonzero
      // scroll position by the end of the reflow, but without going through
      // RestoreState. In particular this can happen with RTL XUL scrollframes,
      // see https://bugzilla.mozilla.org/show_bug.cgi?id=1664638#c14.
      // Upon construction, the ScrollFrameHelper constructor will have inserted
      // a ScrollPositionUpdate into mScrollUpdates with origin None and a zero
      // scroll position, but here we update that to hold the correct scroll
      // position. Otherwise APZ may end up resetting the scroll position to
      // zero incorrectly. If we ever hit this codepath, it must be on a reflow
      // immediately following the scrollframe construction, so there should be
      // exactly one ScrollPositionUpdate in mScrollUpdates.
      MOZ_ASSERT(mScrollUpdates.Length() == 1);
      MOZ_ASSERT(mScrollUpdates.LastElement().GetGeneration() ==
                 mScrollGeneration);
      MOZ_ASSERT(mScrollUpdates.LastElement().GetDestination() == CSSPoint());
      SCROLLRESTORE_LOG("%p: updating initial SPU to pos %s\n", this,
                        ToString(currentScrollPos).c_str());
      mScrollUpdates.Clear();
      AppendScrollUpdate(
          ScrollPositionUpdate::NewScrollframe(currentScrollPos));
    }

    mFirstReflow = false;
  }

  nsAutoScriptBlocker scriptBlocker;

  if (mReclampVVOffsetInReflowFinished) {
    MOZ_ASSERT(mIsRoot && mOuter->PresShell()->IsVisualViewportOffsetSet());
    mReclampVVOffsetInReflowFinished = false;
    AutoWeakFrame weakFrame(mOuter);
    mOuter->PresShell()->SetVisualViewportOffset(
        mOuter->PresShell()->GetVisualViewportOffset(), GetScrollPosition());
    NS_ENSURE_TRUE(weakFrame.IsAlive(), false);
  }

  if (doScroll) {
    ScrollToRestoredPosition();

    // Clamp current scroll position to new bounds. Normally this won't
    // do anything.
    nsPoint currentScrollPos = GetScrollPosition();
    ScrollToImpl(currentScrollPos, nsRect(currentScrollPos, nsSize(0, 0)),
                 ScrollOrigin::Clamp);
    if (ScrollAnimationState().isEmpty()) {
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
  if (mMayHaveDirtyFixedChildren) {
    mMayHaveDirtyFixedChildren = false;
    nsIFrame* parentFrame = mOuter->GetParent();
    for (nsIFrame* fixedChild =
             parentFrame->GetChildList(nsIFrame::kFixedList).FirstChild();
         fixedChild; fixedChild = fixedChild->GetNextSibling()) {
      // force a reflow of the fixed child
      mOuter->PresShell()->FrameNeedsReflow(fixedChild, IntrinsicDirty::Resize,
                                            NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }

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
    nsSize visualViewportSize = GetVisualViewportSize();
    nsRect scrollRange = GetVisualScrollRange();
    nsPoint scrollPos = GetScrollPosition();
    nsSize lineScrollAmount = GetLineScrollAmount();

    if (gfxPlatform::UseDesktopZoomingScrollbars()) {
      scrollRange = GetScrollRangeForUserInputEvents();
      scrollPos = GetVisualViewportOffset();
    }

    AutoWeakFrame weakFrame(mOuter);
    if (vScroll) {
      const double kScrollMultiplier =
          Preferences::GetInt("toolkit.scrollbox.verticalScrollDistance",
                              NS_DEFAULT_VERTICAL_SCROLL_DISTANCE);
      nscoord increment = lineScrollAmount.height * kScrollMultiplier;
      // We normally use (visualViewportSize.height - increment) for height of
      // page scrolling.  However, it is too small when increment is very large.
      // (If increment is larger than visualViewportSize.height, direction of
      // scrolling will be opposite). To avoid it, we use
      // (float(visualViewportSize.height) * 0.8) as lower bound value of height
      // of page scrolling. (bug 383267)
      // XXX shouldn't we use GetPageScrollAmount here?
      nscoord pageincrement = nscoord(visualViewportSize.height - increment);
      nscoord pageincrementMin =
          nscoord(float(visualViewportSize.height) * 0.8);
      FinishReflowForScrollbar(
          vScroll, scrollRange.y, scrollRange.YMost(), scrollPos.y,
          std::max(pageincrement, pageincrementMin), increment);
    }
    if (hScroll) {
      const double kScrollMultiplier =
          Preferences::GetInt("toolkit.scrollbox.horizontalScrollDistance",
                              NS_DEFAULT_HORIZONTAL_SCROLL_DISTANCE);
      nscoord increment = lineScrollAmount.width * kScrollMultiplier;
      FinishReflowForScrollbar(
          hScroll, scrollRange.x, scrollRange.XMost(), scrollPos.x,
          nscoord(float(visualViewportSize.width) * 0.8), increment);
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
  if (!mHScrollbarBox && !mVScrollbarBox) return false;
  CurPosAttributeChanged(mVScrollbarBox
                             ? mVScrollbarBox->GetContent()->AsElement()
                             : mHScrollbarBox->GetContent()->AsElement(),
                         doScroll);
  return doScroll;
}

void ScrollFrameHelper::ReflowCallbackCanceled() {
  mPostedReflowCallback = false;
}

bool ScrollFrameHelper::ComputeCustomOverflow(OverflowAreas& aOverflowAreas) {
  nsIScrollableFrame* sf = do_QueryFrame(mOuter);
  ScrollStyles ss = sf->GetScrollStyles();

  // Reflow when the change in overflow leads to one of our scrollbars
  // changing or might require repositioning the scrolled content due to
  // reduced extents.
  nsRect scrolledRect = GetScrolledRect();
  ScrollDirections overflowChange =
      GetOverflowChange(scrolledRect, mPrevScrolledRect);
  mPrevScrolledRect = scrolledRect;

  bool needReflow = false;
  nsPoint scrollPosition = GetScrollPosition();
  if (overflowChange.contains(ScrollDirection::eHorizontal)) {
    if (ss.mHorizontal != StyleOverflow::Hidden || scrollPosition.x) {
      needReflow = true;
    }
  }
  if (overflowChange.contains(ScrollDirection::eVertical)) {
    if (ss.mVertical != StyleOverflow::Hidden || scrollPosition.y) {
      needReflow = true;
    }
  }

  if (needReflow) {
    // If there are scrollbars, or we're not at the beginning of the pane,
    // the scroll position may change. In this case, mark the frame as
    // needing reflow. Don't use NS_FRAME_IS_DIRTY as dirty as that means
    // we have to reflow the frame and all its descendants, and we don't
    // have to do that here. Only this frame needs to be reflowed.
    mOuter->PresShell()->FrameNeedsReflow(mOuter, IntrinsicDirty::Resize,
                                          NS_FRAME_HAS_DIRTY_CHILDREN);
    // Ensure that next time nsHTMLScrollFrame::Reflow runs, we don't skip
    // updating the scrollbars. (Because the overflow area of the scrolled
    // frame has probably just been updated, Reflow won't see it change.)
    mSkippedScrollbarLayout = true;
    return false;  // reflowing will update overflow
  }
  PostOverflowEvent();
  return mOuter->nsContainerFrame::ComputeCustomOverflow(aOverflowAreas);
}

void ScrollFrameHelper::UpdateSticky() {
  StickyScrollContainer* ssc =
      StickyScrollContainer::GetStickyScrollContainerForScrollFrame(mOuter);
  if (ssc) {
    nsIScrollableFrame* scrollFrame = do_QueryFrame(mOuter);
    ssc->UpdatePositions(scrollFrame->GetScrollPosition(), mOuter);
  }
}

void ScrollFrameHelper::UpdatePrevScrolledRect() {
  // The layout scroll range is determinated by the scrolled rect and the scroll
  // port, so if the scrolled rect is updated, we may have to schedule the
  // associated scroll-linked animations' restyles.
  nsRect currScrolledRect = GetScrolledRect();
  if (!currScrolledRect.IsEqualEdges(mPrevScrolledRect)) {
    mMayScheduleScrollAnimations = true;
  }
  mPrevScrolledRect = currScrolledRect;
}

void ScrollFrameHelper::AdjustScrollbarRectForResizer(
    nsIFrame* aFrame, nsPresContext* aPresContext, nsRect& aRect,
    bool aHasResizer, ScrollDirection aDirection) {
  if ((aDirection == ScrollDirection::eVertical ? aRect.width : aRect.height) ==
      0) {
    return;
  }

  // if a content resizer is present, use its size. Otherwise, check if the
  // widget has a resizer.
  nsRect resizerRect;
  if (aHasResizer) {
    resizerRect = mResizerBox->GetRect();
  } else {
    nsPoint offset;
    nsIWidget* widget = aFrame->GetNearestWidget(offset);
    LayoutDeviceIntRect widgetRect;
    if (!widget || !widget->ShowsResizeIndicator(&widgetRect)) {
      return;
    }

    resizerRect =
        nsRect(aPresContext->DevPixelsToAppUnits(widgetRect.x) - offset.x,
               aPresContext->DevPixelsToAppUnits(widgetRect.y) - offset.y,
               aPresContext->DevPixelsToAppUnits(widgetRect.width),
               aPresContext->DevPixelsToAppUnits(widgetRect.height));
  }

  if (resizerRect.Contains(aRect.BottomRight() - nsPoint(1, 1))) {
    switch (aDirection) {
      case ScrollDirection::eVertical:
        aRect.height = std::max(0, resizerRect.y - aRect.y);
        break;
      case ScrollDirection::eHorizontal:
        aRect.width = std::max(0, resizerRect.x - aRect.x);
        break;
    }
  } else if (resizerRect.Contains(aRect.BottomLeft() + nsPoint(1, -1))) {
    switch (aDirection) {
      case ScrollDirection::eVertical:
        aRect.height = std::max(0, resizerRect.y - aRect.y);
        break;
      case ScrollDirection::eHorizontal: {
        nscoord xmost = aRect.XMost();
        aRect.x = std::max(aRect.x, resizerRect.XMost());
        aRect.width = xmost - aRect.x;
        break;
      }
    }
  }
}

static void AdjustOverlappingScrollbars(nsRect& aVRect, nsRect& aHRect) {
  if (aVRect.IsEmpty() || aHRect.IsEmpty()) return;

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

void ScrollFrameHelper::LayoutScrollbars(nsBoxLayoutState& aState,
                                         const nsRect& aInsideBorderArea,
                                         const nsRect& aOldScrollPort) {
  NS_ASSERTION(!mSuppressScrollbarUpdate, "This should have been suppressed");

  bool scrollbarOnLeft = !IsScrollbarOnRight();
  const bool overlayScrollbars = UsesOverlayScrollbars();
  const bool overlayScrollBarsOnRoot = overlayScrollbars && mIsRoot;
  const bool showVScrollbar = mVScrollbarBox && mHasVerticalScrollbar;
  const bool showHScrollbar = mHScrollbarBox && mHasHorizontalScrollbar;

  nsSize compositionSize = mScrollPort.Size();
  if (overlayScrollBarsOnRoot) {
    compositionSize = nsLayoutUtils::CalculateCompositionSizeForFrame(
        mOuter, false, &compositionSize);
  }

  nsPresContext* presContext = mScrolledFrame->PresContext();
  nsRect vRect;
  if (showVScrollbar) {
    MOZ_ASSERT(mVScrollbarBox->IsXULBoxFrame(), "Must be a box frame!");
    vRect.height =
        overlayScrollBarsOnRoot ? compositionSize.height : mScrollPort.height;
    vRect.y = mScrollPort.y;
    if (scrollbarOnLeft) {
      vRect.width = mScrollPort.x - aInsideBorderArea.x;
      vRect.x = aInsideBorderArea.x;
    } else {
      vRect.width = aInsideBorderArea.XMost() - mScrollPort.XMost();
      vRect.x = mScrollPort.x + compositionSize.width;
    }

    // For overlay scrollbars the margin returned from this GetXULMargin call
    // is a negative margin that moves the scrollbar from just outside the
    // scrollport (and hence not visible) to just inside the scrollport (and
    // hence visible). For non-overlay scrollbars it is a 0 margin.
    nsMargin margin;
    mVScrollbarBox->GetXULMargin(margin);

    if (!overlayScrollbars && mOnlyNeedVScrollbarToScrollVVInsideLV) {
      // There is no space reserved for the layout scrollbar, it is currently
      // not visible because it is positioned just outside the scrollport. But
      // we know that it needs to be made visible so we shift it back in.
      nsSize vScrollbarPrefSize(0, 0);
      GetScrollbarMetrics(aState, mVScrollbarBox, nullptr, &vScrollbarPrefSize);
      if (scrollbarOnLeft) {
        margin.right -= vScrollbarPrefSize.width;
      } else {
        margin.left -= vScrollbarPrefSize.width;
      }
    }

    vRect.Deflate(margin);
  }

  nsRect hRect;
  if (showHScrollbar) {
    MOZ_ASSERT(mHScrollbarBox->IsXULBoxFrame(), "Must be a box frame!");
    hRect.width =
        overlayScrollBarsOnRoot ? compositionSize.width : mScrollPort.width;
    hRect.x = mScrollPort.x;
    hRect.height = aInsideBorderArea.YMost() - mScrollPort.YMost();
    hRect.y = mScrollPort.y + compositionSize.height;

    // For overlay scrollbars the margin returned from this GetXULMargin call
    // is a negative margin that moves the scrollbar from just outside the
    // scrollport (and hence not visible) to just inside the scrollport (and
    // hence visible). For non-overlay scrollbars it is a 0 margin.
    nsMargin margin;
    mHScrollbarBox->GetXULMargin(margin);

    if (!overlayScrollbars && mOnlyNeedHScrollbarToScrollVVInsideLV) {
      // There is no space reserved for the layout scrollbar, it is currently
      // not visible because it is positioned just outside the scrollport. But
      // we know that it needs to be made visible so we shift it back in.
      nsSize hScrollbarPrefSize(0, 0);
      GetScrollbarMetrics(aState, mHScrollbarBox, nullptr, &hScrollbarPrefSize);
      margin.top -= hScrollbarPrefSize.height;
    }

    hRect.Deflate(margin);
  }

  const bool hasVisualOnlyScrollbarsOnBothDirections =
      !overlayScrollbars && showHScrollbar &&
      mOnlyNeedHScrollbarToScrollVVInsideLV && showVScrollbar &&
      mOnlyNeedVScrollbarToScrollVVInsideLV;

  // place the scrollcorner
  if (mScrollCornerBox) {
    MOZ_ASSERT(mScrollCornerBox->IsXULBoxFrame(), "Must be a box frame!");

    nsRect r(0, 0, 0, 0);
    if (scrollbarOnLeft) {
      // scrollbar (if any) on left
      r.width = showVScrollbar ? mScrollPort.x - aInsideBorderArea.x : 0;
      r.x = aInsideBorderArea.x;
    } else {
      // scrollbar (if any) on right
      r.width =
          showVScrollbar ? aInsideBorderArea.XMost() - mScrollPort.XMost() : 0;
      r.x = aInsideBorderArea.XMost() - r.width;
    }
    NS_ASSERTION(r.width >= 0, "Scroll area should be inside client rect");

    if (showHScrollbar) {
      // scrollbar (if any) on bottom
      // Note we don't support the horizontal scrollbar at the top side.
      r.height = aInsideBorderArea.YMost() - mScrollPort.YMost();
      NS_ASSERTION(r.height >= 0, "Scroll area should be inside client rect");
    }
    r.y = aInsideBorderArea.YMost() - r.height;

    // If we have layout scrollbars and both scrollbars are present and both are
    // only needed to scroll the VV inside the LV then we need a scrollcorner
    // but the above calculation will result in an empty rect, so adjust it.
    if (r.IsEmpty() && hasVisualOnlyScrollbarsOnBothDirections) {
      r.width = vRect.width;
      r.height = hRect.height;
      r.x = scrollbarOnLeft ? mScrollPort.x : mScrollPort.XMost() - r.width;
      r.y = mScrollPort.YMost() - r.height;
    }

    nsBoxFrame::LayoutChildAt(aState, mScrollCornerBox, r);
  }

  if (mResizerBox) {
    // If a resizer is present, get its size.
    //
    // TODO(emilio): Should this really account for scrollbar-width?
    nsPresContext* pc = aState.PresContext();
    auto scrollbarWidth = nsLayoutUtils::StyleForScrollbar(mOuter)
                              ->StyleUIReset()
                              ->ScrollbarWidth();
    auto sizes = pc->Theme()->GetScrollbarSizes(pc, scrollbarWidth,
                                                nsITheme::Overlay::No);
    nsSize resizerMinSize = mResizerBox->GetXULMinSize(aState);

    nsRect r;
    nscoord vScrollbarWidth = pc->DevPixelsToAppUnits(sizes.mVertical);
    r.width =
        std::max(std::max(r.width, vScrollbarWidth), resizerMinSize.width);
    r.x = scrollbarOnLeft ? aInsideBorderArea.x
                          : aInsideBorderArea.XMost() - r.width;

    nscoord hScrollbarHeight = pc->DevPixelsToAppUnits(sizes.mHorizontal);
    r.height =
        std::max(std::max(r.height, hScrollbarHeight), resizerMinSize.height);
    r.y = aInsideBorderArea.YMost() - r.height;

    nsBoxFrame::LayoutChildAt(aState, mResizerBox, r);
  }

  // Note that AdjustScrollbarRectForResizer has to be called after the
  // resizer has been laid out immediately above this because it gets the rect
  // of the resizer frame.
  if (mVScrollbarBox) {
    AdjustScrollbarRectForResizer(mOuter, presContext, vRect, mResizerBox,
                                  ScrollDirection::eVertical);
  }
  if (mHScrollbarBox) {
    AdjustScrollbarRectForResizer(mOuter, presContext, hRect, mResizerBox,
                                  ScrollDirection::eHorizontal);
  }

  // Layout scrollbars can overlap at this point if they are both present and
  // both only needed to scroll the VV inside the LV.
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::AllowOverlayScrollbarsOverlap) ||
      hasVisualOnlyScrollbarsOnBothDirections) {
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
  if (aOldScrollPort.Size() != mScrollPort.Size() &&
      !mOuter->HasAnyStateBits(NS_FRAME_IS_DIRTY) && mIsRoot) {
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
static bool ShellIsAlive(nsWeakPtr& aWeakPtr) {
  RefPtr<PresShell> presShell = do_QueryReferent(aWeakPtr);
  return !!presShell;
}
#endif

void ScrollFrameHelper::SetScrollbarEnabled(Element* aElement,
                                            nscoord aMaxPos) {
  DebugOnly<nsWeakPtr> weakShell(do_GetWeakReference(mOuter->PresShell()));
  if (aMaxPos) {
    aElement->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
  } else {
    aElement->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, u"true"_ns, true);
  }
  MOZ_ASSERT(ShellIsAlive(weakShell), "pres shell was destroyed by scrolling");
}

void ScrollFrameHelper::SetCoordAttribute(Element* aElement, nsAtom* aAtom,
                                          nscoord aSize) {
  DebugOnly<nsWeakPtr> weakShell(do_GetWeakReference(mOuter->PresShell()));
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

  if (mScrollbarActivity &&
      (mHasHorizontalScrollbar || mHasVerticalScrollbar)) {
    RefPtr<ScrollbarActivity> scrollbarActivity(mScrollbarActivity);
    scrollbarActivity->ActivityOccurred();
  }
}

static void ReduceRadii(nscoord aXBorder, nscoord aYBorder, nscoord& aXRadius,
                        nscoord& aYRadius) {
  // In order to ensure that the inside edge of the border has no
  // curvature, we need at least one of its radii to be zero.
  if (aXRadius <= aXBorder || aYRadius <= aYBorder) return;

  // For any corner where we reduce the radii, preserve the corner's shape.
  double ratio =
      std::max(double(aXBorder) / aXRadius, double(aYBorder) / aYRadius);
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
bool ScrollFrameHelper::GetBorderRadii(const nsSize& aFrameSize,
                                       const nsSize& aBorderArea,
                                       Sides aSkipSides,
                                       nscoord aRadii[8]) const {
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
    ReduceRadii(border.left, border.top, aRadii[eCornerTopLeftX],
                aRadii[eCornerTopLeftY]);
  }

  if (sb.top > 0 || sb.right > 0) {
    ReduceRadii(border.right, border.top, aRadii[eCornerTopRightX],
                aRadii[eCornerTopRightY]);
  }

  if (sb.right > 0 || sb.bottom > 0) {
    ReduceRadii(border.right, border.bottom, aRadii[eCornerBottomRightX],
                aRadii[eCornerBottomRightY]);
  }

  if (sb.bottom > 0 || sb.left > 0) {
    ReduceRadii(border.left, border.bottom, aRadii[eCornerBottomLeftX],
                aRadii[eCornerBottomLeftY]);
  }

  return true;
}

static nscoord SnapCoord(nscoord aCoord, double aRes,
                         nscoord aAppUnitsPerPixel) {
  double snappedToLayerPixels = NS_round((aRes * aCoord) / aAppUnitsPerPixel);
  return NSToCoordRoundWithClamp(snappedToLayerPixels * aAppUnitsPerPixel /
                                 aRes);
}

nsRect ScrollFrameHelper::GetScrolledRect() const {
  nsRect result = GetUnsnappedScrolledRectInternal(
      mScrolledFrame->ScrollableOverflowRect(), mScrollPort.Size());

#if 0
  // This happens often enough.
  if (result.width < mScrollPort.width || result.height < mScrollPort.height) {
    NS_WARNING("Scrolled rect smaller than scrollport?");
  }
#endif

  // Expand / contract the result by up to half a layer pixel so that scrolling
  // to the right / bottom edge does not change the layer pixel alignment of
  // the scrolled contents.

  if (result.x == 0 && result.y == 0 && result.width == mScrollPort.width &&
      result.height == mScrollPort.height) {
    // The edges that we would snap are already aligned with the scroll port,
    // so we can skip all the work below.
    return result;
  }

  // For that, we first convert the scroll port and the scrolled rect to rects
  // relative to the reference frame, since that's the space where painting does
  // snapping.
  nsSize visualViewportSize = GetVisualViewportSize();
  const nsIFrame* referenceFrame =
      mReferenceFrameDuringPainting ? mReferenceFrameDuringPainting
                                    : nsLayoutUtils::GetReferenceFrame(mOuter);
  nsPoint toReferenceFrame = mOuter->GetOffsetToCrossDoc(referenceFrame);
  nsRect scrollPort(mScrollPort.TopLeft() + toReferenceFrame,
                    visualViewportSize);
  nsRect scrolledRect = result + scrollPort.TopLeft();

  if (scrollPort.Overflows() || scrolledRect.Overflows()) {
    return result;
  }

  // Now, snap the bottom right corner of both of these rects.
  // We snap to layer pixels, so we need to respect the layer's scale.
  nscoord appUnitsPerDevPixel =
      mScrolledFrame->PresContext()->AppUnitsPerDevPixel();
  MatrixScales scale = GetPaintedLayerScaleForFrame(mScrolledFrame);
  if (scale.xScale == 0 || scale.yScale == 0) {
    scale = MatrixScales();
  }

  // Compute bounds for the scroll position, and computed the snapped scrolled
  // rect from the scroll position bounds.
  nscoord snappedScrolledAreaBottom =
      SnapCoord(scrolledRect.YMost(), scale.yScale, appUnitsPerDevPixel);
  nscoord snappedScrollPortBottom =
      SnapCoord(scrollPort.YMost(), scale.yScale, appUnitsPerDevPixel);
  nscoord maximumScrollOffsetY =
      snappedScrolledAreaBottom - snappedScrollPortBottom;
  result.SetBottomEdge(scrollPort.height + maximumScrollOffsetY);

  if (GetScrolledFrameDir() == StyleDirection::Ltr) {
    nscoord snappedScrolledAreaRight =
        SnapCoord(scrolledRect.XMost(), scale.xScale, appUnitsPerDevPixel);
    nscoord snappedScrollPortRight =
        SnapCoord(scrollPort.XMost(), scale.xScale, appUnitsPerDevPixel);
    nscoord maximumScrollOffsetX =
        snappedScrolledAreaRight - snappedScrollPortRight;
    result.SetRightEdge(scrollPort.width + maximumScrollOffsetX);
  } else {
    // In RTL, the scrolled area's right edge is at scrollPort.XMost(),
    // and the scrolled area's x position is zero or negative. We want
    // the right edge to stay flush with the scroll port, so we snap the
    // left edge.
    nscoord snappedScrolledAreaLeft =
        SnapCoord(scrolledRect.x, scale.xScale, appUnitsPerDevPixel);
    nscoord snappedScrollPortLeft =
        SnapCoord(scrollPort.x, scale.xScale, appUnitsPerDevPixel);
    nscoord minimumScrollOffsetX =
        snappedScrolledAreaLeft - snappedScrollPortLeft;
    result.SetLeftEdge(minimumScrollOffsetX);
  }

  return result;
}

StyleDirection ScrollFrameHelper::GetScrolledFrameDir() const {
  // If the scrolled frame has unicode-bidi: plaintext, the paragraph
  // direction set by the text content overrides the direction of the frame
  if (mScrolledFrame->StyleTextReset()->mUnicodeBidi &
      NS_STYLE_UNICODE_BIDI_PLAINTEXT) {
    if (nsIFrame* child = mScrolledFrame->PrincipalChildList().FirstChild()) {
      return nsBidiPresUtils::ParagraphDirection(child) ==
                     mozilla::intl::BidiDirection::LTR
                 ? StyleDirection::Ltr
                 : StyleDirection::Rtl;
    }
  }
  return IsBidiLTR() ? StyleDirection::Ltr : StyleDirection::Rtl;
}

nsRect ScrollFrameHelper::GetUnsnappedScrolledRectInternal(
    const nsRect& aScrolledFrameOverflowArea,
    const nsSize& aScrollPortSize) const {
  return nsLayoutUtils::GetScrolledRect(mScrolledFrame,
                                        aScrolledFrameOverflowArea,
                                        aScrollPortSize, GetScrolledFrameDir());
}

nsMargin ScrollFrameHelper::GetActualScrollbarSizes(
    nsIScrollableFrame::ScrollbarSizesOptions
        aOptions /* = nsIScrollableFrame::ScrollbarSizesOptions::NONE */)
    const {
  nsRect r = mOuter->GetPaddingRectRelativeToSelf();

  nsMargin m(mScrollPort.y - r.y, r.XMost() - mScrollPort.XMost(),
             r.YMost() - mScrollPort.YMost(), mScrollPort.x - r.x);

  if (aOptions == nsIScrollableFrame::ScrollbarSizesOptions::
                      INCLUDE_VISUAL_VIEWPORT_SCROLLBARS &&
      !UsesOverlayScrollbars()) {
    // If we are using layout scrollbars and they only exist to scroll the
    // visual viewport then they do not take up any layout space (so the
    // scrollport is the same as the padding rect) but they do cover everything
    // below them so some callers may want to include this special type of
    // scrollbars in the returned value.
    if (mHScrollbarBox && mHasHorizontalScrollbar &&
        mOnlyNeedHScrollbarToScrollVVInsideLV) {
      m.bottom += mHScrollbarBox->GetRect().height;
    }
    if (mVScrollbarBox && mHasVerticalScrollbar &&
        mOnlyNeedVScrollbarToScrollVVInsideLV) {
      if (IsScrollbarOnRight()) {
        m.right += mVScrollbarBox->GetRect().width;
      } else {
        m.left += mVScrollbarBox->GetRect().width;
      }
    }
  }

  return m;
}

void ScrollFrameHelper::SetScrollbarVisibility(nsIFrame* aScrollbar,
                                               bool aVisible) {
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

nscoord ScrollFrameHelper::GetCoordAttribute(nsIFrame* aBox, nsAtom* aAtom,
                                             nscoord aDefaultValue,
                                             nscoord* aRangeStart,
                                             nscoord* aRangeLength) {
  if (aBox) {
    nsIContent* content = aBox->GetContent();

    nsAutoString value;
    if (content->IsElement()) {
      content->AsElement()->GetAttr(kNameSpaceID_None, aAtom, value);
    }
    if (!value.IsEmpty()) {
      nsresult error;
      // convert it to appunits
      nscoord result =
          nsPresContext::CSSPixelsToAppUnits(value.ToInteger(&error));
      nscoord halfPixel = nsPresContext::CSSPixelsToAppUnits(0.5f);
      // Any nscoord value that would round to the attribute value when
      // converted to CSS pixels is allowed.
      *aRangeStart = result - halfPixel;
      *aRangeLength = halfPixel * 2 - 1;
      return result;
    }
  }

  // Only this exact default value is allowed.
  *aRangeStart = aDefaultValue;
  *aRangeLength = 0;
  return aDefaultValue;
}

bool ScrollFrameHelper::IsLastScrollUpdateAnimating() const {
  if (!mScrollUpdates.IsEmpty()) {
    switch (mScrollUpdates.LastElement().GetMode()) {
      case ScrollMode::Smooth:
      case ScrollMode::SmoothMsd:
        return true;
      case ScrollMode::Instant:
      case ScrollMode::Normal:
        break;
    }
  }
  return false;
}

using AnimationState = nsIScrollableFrame::AnimationState;
EnumSet<AnimationState> ScrollFrameHelper::ScrollAnimationState() const {
  EnumSet<AnimationState> retval;
  if (IsApzAnimationInProgress()) {
    retval += AnimationState::APZInProgress;
  }
  if (mApzAnimationRequested) {
    retval += AnimationState::APZRequested;
  }
  if (IsLastScrollUpdateAnimating()) {
    retval += AnimationState::APZPending;
  }
  if (mAsyncScroll || mAsyncSmoothMSDScroll) {
    retval += AnimationState::MainThread;
  }
  return retval;
}

void ScrollFrameHelper::ResetScrollInfoIfNeeded(
    const MainThreadScrollGeneration& aGeneration,
    const APZScrollGeneration& aGenerationOnApz,
    APZScrollAnimationType aAPZScrollAnimationType) {
  if (aGeneration == mScrollGeneration) {
    mLastScrollOrigin = ScrollOrigin::None;
    mApzAnimationRequested = false;
  }

  mScrollGenerationOnApz = aGenerationOnApz;
  // We can reset this regardless of scroll generation, as this is only set
  // here, as a response to APZ requesting a repaint.
  mCurrentAPZScrollAnimationType = aAPZScrollAnimationType;
}

UniquePtr<PresState> ScrollFrameHelper::SaveState() const {
  nsIScrollbarMediator* mediator = do_QueryFrame(GetScrolledFrame());
  if (mediator) {
    // child handles its own scroll state, so don't bother saving state here
    return nullptr;
  }

  // Don't store a scroll state if we never have been scrolled or restored
  // a previous scroll state, and we're not in the middle of a smooth scroll.
  auto scrollAnimationState = ScrollAnimationState();
  bool isScrollAnimating =
      scrollAnimationState.contains(AnimationState::MainThread) ||
      scrollAnimationState.contains(AnimationState::APZPending) ||
      scrollAnimationState.contains(AnimationState::APZRequested);
  if (!mHasBeenScrolled && !mDidHistoryRestore && !isScrollAnimating) {
    return nullptr;
  }

  UniquePtr<PresState> state = NewPresState();
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
  nsPoint pt = GetLogicalVisualViewportOffset();
  if (isScrollAnimating) {
    pt = mDestination;
    allowScrollOriginDowngrade = false;
  }
  SCROLLRESTORE_LOG("%p: SaveState, pt=%s, mLastPos=%s, mRestorePos=%s\n", this,
                    ToString(pt).c_str(), ToString(mLastPos).c_str(),
                    ToString(mRestorePos).c_str());
  if (mRestorePos.y != -1 && pt == mLastPos) {
    pt = mRestorePos;
  }
  state->scrollState() = pt;
  state->allowScrollOriginDowngrade() = allowScrollOriginDowngrade;
  if (mIsRoot) {
    // Only save resolution properties for root scroll frames
    state->resolution() = mOuter->PresShell()->GetResolution();
  }
  return state;
}

void ScrollFrameHelper::RestoreState(PresState* aState) {
  mRestorePos = aState->scrollState();
  MOZ_ASSERT(mLastScrollOrigin == ScrollOrigin::None);
  mAllowScrollOriginDowngrade = aState->allowScrollOriginDowngrade();
  // When restoring state, we promote mLastScrollOrigin to a stronger value
  // from the default of eNone, to restore the behaviour that existed when
  // the state was saved. If mLastScrollOrigin was a weaker value previously,
  // then mAllowScrollOriginDowngrade will be true, and so the combination of
  // mAllowScrollOriginDowngrade and the stronger mLastScrollOrigin will allow
  // the same types of scrolls as before. It might be possible to also just
  // save and restore the mAllowScrollOriginDowngrade and mLastScrollOrigin
  // values directly without this sort of fiddling. Something to try in the
  // future or if we tinker with this code more.
  mLastScrollOrigin = ScrollOrigin::Other;
  mDidHistoryRestore = true;
  mLastPos = mScrolledFrame ? GetLogicalVisualViewportOffset() : nsPoint(0, 0);
  SCROLLRESTORE_LOG("%p: RestoreState, set mRestorePos=%s mLastPos=%s\n", this,
                    ToString(mRestorePos).c_str(), ToString(mLastPos).c_str());

  // Resolution properties should only exist on root scroll frames.
  MOZ_ASSERT(mIsRoot || aState->resolution() == 1.0);

  if (mIsRoot) {
    mOuter->PresShell()->SetResolutionAndScaleTo(
        aState->resolution(), ResolutionChangeOrigin::MainThreadRestore);
  }
}

void ScrollFrameHelper::PostScrolledAreaEvent() {
  if (mScrolledAreaEvent.IsPending()) {
    return;
  }
  mScrolledAreaEvent = new ScrolledAreaEvent(this);
  nsContentUtils::AddScriptRunner(mScrolledAreaEvent.get());
}

////////////////////////////////////////////////////////////////////////////////
// ScrolledArea change event dispatch

// TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230, bug 1535398)
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
ScrollFrameHelper::ScrolledAreaEvent::Run() {
  if (mHelper) {
    mHelper->FireScrolledAreaEvent();
  }
  return NS_OK;
}

void ScrollFrameHelper::FireScrolledAreaEvent() {
  mScrolledAreaEvent.Forget();

  InternalScrollAreaEvent event(true, eScrolledAreaChanged, nullptr);
  RefPtr<nsPresContext> presContext = mOuter->PresContext();
  nsIContent* content = mOuter->GetContent();

  event.mArea = mScrolledFrame->ScrollableOverflowRectRelativeToParent();
  if (RefPtr<Document> doc = content->GetUncomposedDoc()) {
    // TODO: Bug 1506441
    EventDispatcher::Dispatch(MOZ_KnownLive(ToSupports(doc)), presContext,
                              &event, nullptr);
  }
}

ScrollDirections nsIScrollableFrame::GetAvailableScrollingDirections() const {
  nscoord oneDevPixel =
      GetScrolledFrame()->PresContext()->AppUnitsPerDevPixel();
  ScrollDirections directions;
  nsRect scrollRange = GetScrollRange();
  if (scrollRange.width >= oneDevPixel) {
    directions += ScrollDirection::eHorizontal;
  }
  if (scrollRange.height >= oneDevPixel) {
    directions += ScrollDirection::eVertical;
  }
  return directions;
}

nsRect ScrollFrameHelper::GetScrollRangeForUserInputEvents() const {
  // This function computes a scroll range based on a scrolled rect and scroll
  // port defined as follows:
  //   scrollable rect = overflow:hidden ? layout viewport : scrollable rect
  //   scroll port = have visual viewport ? visual viewport : layout viewport
  // The results in the same notion of scroll range that APZ uses (the combined
  // effect of FrameMetrics::CalculateScrollRange() and
  // nsLayoutUtils::CalculateScrollableRectForFrame).

  ScrollStyles ss = GetScrollStylesFromFrame();

  nsPoint scrollPos = GetScrollPosition();

  nsRect scrolledRect = GetScrolledRect();
  if (StyleOverflow::Hidden == ss.mHorizontal) {
    scrolledRect.width = mScrollPort.width;
    scrolledRect.x = scrollPos.x;
  }
  if (StyleOverflow::Hidden == ss.mVertical) {
    scrolledRect.height = mScrollPort.height;
    scrolledRect.y = scrollPos.y;
  }

  nsSize scrollPort = GetVisualViewportSize();

  nsRect scrollRange = scrolledRect;
  scrollRange.width = std::max(scrolledRect.width - scrollPort.width, 0);
  scrollRange.height = std::max(scrolledRect.height - scrollPort.height, 0);

  return scrollRange;
}

ScrollDirections
ScrollFrameHelper::GetAvailableScrollingDirectionsForUserInputEvents() const {
  nsRect scrollRange = GetScrollRangeForUserInputEvents();

  // We check if there is at least one half of a screen pixel of scroll range to
  // roughly match what apz does when it checks if the change in scroll position
  // in screen pixels round to zero or not.
  // (https://searchfox.org/mozilla-central/rev/2f09184ec781a2667feec87499d4b81b32b6c48e/gfx/layers/apz/src/AsyncPanZoomController.cpp#3210)
  // This isn't quite half a screen pixel, it doesn't take into account CSS
  // transforms, but should be good enough.
  float halfScreenPixel =
      GetScrolledFrame()->PresContext()->AppUnitsPerDevPixel() /
      (mOuter->PresShell()->GetCumulativeResolution() * 2.f);
  ScrollDirections directions;
  if (scrollRange.width >= halfScreenPixel) {
    directions += ScrollDirection::eHorizontal;
  }
  if (scrollRange.height >= halfScreenPixel) {
    directions += ScrollDirection::eVertical;
  }
  return directions;
}

static nsRect InflateByScrollMargin(const nsRect& aTargetRect,
                                    const nsMargin& aScrollMargin,
                                    const nsRect& aScrolledRect) {
  // Inflate the rect by scroll-margin.
  nsRect result = aTargetRect;
  result.Inflate(aScrollMargin);

  // But don't be beyond the limit boundary.
  return result.Intersect(aScrolledRect);
}

/**
 * Append scroll positions for valid snap positions into |aSnapInfo| if
 * applicable.
 */
static void AppendScrollPositionsForSnap(const nsIFrame* aFrame,
                                         const nsIFrame* aScrolledFrame,
                                         const nsRect& aScrolledRect,
                                         const nsMargin& aScrollPadding,
                                         WritingMode aWritingModeOnScroller,
                                         ScrollSnapInfo& aSnapInfo) {
  nsRect targetRect = nsLayoutUtils::TransformFrameRectToAncestor(
      aFrame, aFrame->GetRectRelativeToSelf(), aScrolledFrame);

  // The snap area contains scroll-margin values.
  // https://drafts.csswg.org/css-scroll-snap-1/#scroll-snap-area
  nsMargin scrollMargin = aFrame->StyleMargin()->GetScrollMargin();
  nsRect snapArea =
      InflateByScrollMargin(targetRect, scrollMargin, aScrolledRect);

  // These snap range shouldn't be involved with scroll-margin since we just
  // need the visible range of the target element.
  if (snapArea.width > aSnapInfo.mSnapportSize.width) {
    aSnapInfo.mXRangeWiderThanSnapport.AppendElement(
        ScrollSnapInfo::ScrollSnapRange(snapArea.X(), snapArea.XMost()));
  }
  if (snapArea.height > aSnapInfo.mSnapportSize.height) {
    aSnapInfo.mYRangeWiderThanSnapport.AppendElement(
        ScrollSnapInfo::ScrollSnapRange(snapArea.Y(), snapArea.YMost()));
  }

  // Shift target rect position by the scroll padding to get the padded
  // position thus we don't need to take account scroll-padding values in
  // ScrollSnapUtils::GetSnapPointForDestination() when it gets called from
  // the compositor thread.
  snapArea.y -= aScrollPadding.top;
  snapArea.x -= aScrollPadding.left;

  LogicalRect logicalTargetRect(aWritingModeOnScroller, snapArea,
                                aSnapInfo.mSnapportSize);
  LogicalSize logicalSnapportRect(aWritingModeOnScroller,
                                  aSnapInfo.mSnapportSize);

  Maybe<nscoord> blockDirectionPosition;
  Maybe<nscoord> inlineDirectionPosition;

  const nsStyleDisplay* styleDisplay = aFrame->StyleDisplay();
  nscoord containerBSize = logicalSnapportRect.BSize(aWritingModeOnScroller);
  switch (styleDisplay->mScrollSnapAlign.block) {
    case StyleScrollSnapAlignKeyword::None:
      break;
    case StyleScrollSnapAlignKeyword::Start:
      blockDirectionPosition.emplace(
          aWritingModeOnScroller.IsVerticalRL()
              ? -logicalTargetRect.BStart(aWritingModeOnScroller)
              : logicalTargetRect.BStart(aWritingModeOnScroller));
      break;
    case StyleScrollSnapAlignKeyword::End:
      if (aWritingModeOnScroller.IsVerticalRL()) {
        blockDirectionPosition.emplace(
            containerBSize - logicalTargetRect.BEnd(aWritingModeOnScroller));
      } else {
        // What we need here is the scroll position instead of the snap position
        // itself, so we need, for example, the top edge of the scroll port
        // on horizontal-tb when the frame is positioned at the bottom edge of
        // the scroll port. For this reason we subtract containerBSize from
        // BEnd of the target.
        blockDirectionPosition.emplace(
            logicalTargetRect.BEnd(aWritingModeOnScroller) - containerBSize);
      }
      break;
    case StyleScrollSnapAlignKeyword::Center: {
      nscoord targetCenter = (logicalTargetRect.BStart(aWritingModeOnScroller) +
                              logicalTargetRect.BEnd(aWritingModeOnScroller)) /
                             2;
      nscoord halfSnapportSize = containerBSize / 2;
      // Get the center of the target to align with the center of the snapport
      // depending on direction.
      if (aWritingModeOnScroller.IsVerticalRL()) {
        blockDirectionPosition.emplace(halfSnapportSize - targetCenter);
      } else {
        blockDirectionPosition.emplace(targetCenter - halfSnapportSize);
      }
      break;
    }
  }

  nscoord containerISize = logicalSnapportRect.ISize(aWritingModeOnScroller);
  switch (styleDisplay->mScrollSnapAlign.inline_) {
    case StyleScrollSnapAlignKeyword::None:
      break;
    case StyleScrollSnapAlignKeyword::Start:
      inlineDirectionPosition.emplace(
          aWritingModeOnScroller.IsInlineReversed()
              ? -logicalTargetRect.IStart(aWritingModeOnScroller)
              : logicalTargetRect.IStart(aWritingModeOnScroller));
      break;
    case StyleScrollSnapAlignKeyword::End:
      if (aWritingModeOnScroller.IsInlineReversed()) {
        inlineDirectionPosition.emplace(
            containerISize - logicalTargetRect.IEnd(aWritingModeOnScroller));
      } else {
        // Same as above BEnd case, we subtract containerISize.
        inlineDirectionPosition.emplace(
            logicalTargetRect.IEnd(aWritingModeOnScroller) - containerISize);
      }
      break;
    case StyleScrollSnapAlignKeyword::Center: {
      nscoord targetCenter = (logicalTargetRect.IStart(aWritingModeOnScroller) +
                              logicalTargetRect.IEnd(aWritingModeOnScroller)) /
                             2;
      nscoord halfSnapportSize = containerISize / 2;
      // Get the center of the target to align with the center of the snapport
      // depending on direction.
      if (aWritingModeOnScroller.IsInlineReversed()) {
        inlineDirectionPosition.emplace(halfSnapportSize - targetCenter);
      } else {
        inlineDirectionPosition.emplace(targetCenter - halfSnapportSize);
      }
      break;
    }
  }

  if (blockDirectionPosition || inlineDirectionPosition) {
    aSnapInfo.mSnapTargets.AppendElement(
        aWritingModeOnScroller.IsVertical()
            ? ScrollSnapInfo::SnapTarget(std::move(blockDirectionPosition),
                                         std::move(inlineDirectionPosition),
                                         std::move(snapArea),
                                         styleDisplay->mScrollSnapStop)
            : ScrollSnapInfo::SnapTarget(std::move(inlineDirectionPosition),
                                         std::move(blockDirectionPosition),
                                         std::move(snapArea),
                                         styleDisplay->mScrollSnapStop));
  }
}

/**
 * Collect the scroll positions corresponding to snap positions of frames in the
 * subtree rooted at |aFrame|, relative to |aScrolledFrame|, into |aSnapInfo|.
 */
static void CollectScrollPositionsForSnap(nsIFrame* aFrame,
                                          nsIFrame* aScrolledFrame,
                                          const nsRect& aScrolledRect,
                                          const nsMargin& aScrollPadding,
                                          WritingMode aWritingModeOnScroller,
                                          ScrollSnapInfo& aSnapInfo) {
  // Snap positions only affect the nearest ancestor scroll container on the
  // element's containing block chain.
  nsIScrollableFrame* sf = do_QueryFrame(aFrame);
  if (sf) {
    return;
  }

  for (const auto& childList : aFrame->ChildLists()) {
    for (nsIFrame* f : childList.mList) {
      const nsStyleDisplay* styleDisplay = f->StyleDisplay();
      if (styleDisplay->mScrollSnapAlign.inline_ !=
              StyleScrollSnapAlignKeyword::None ||
          styleDisplay->mScrollSnapAlign.block !=
              StyleScrollSnapAlignKeyword::None) {
        AppendScrollPositionsForSnap(f, aScrolledFrame, aScrolledRect,
                                     aScrollPadding, aWritingModeOnScroller,
                                     aSnapInfo);
      }
      CollectScrollPositionsForSnap(f, aScrolledFrame, aScrolledRect,
                                    aScrollPadding, aWritingModeOnScroller,
                                    aSnapInfo);
    }
  }
}

static nscoord ResolveScrollPaddingStyleValue(
    const StyleRect<mozilla::NonNegativeLengthPercentageOrAuto>&
        aScrollPaddingStyle,
    Side aSide, const nsSize& aScrollPortSize) {
  if (aScrollPaddingStyle.Get(aSide).IsAuto()) {
    // https://drafts.csswg.org/css-scroll-snap-1/#valdef-scroll-padding-auto
    return 0;
  }

  nscoord percentageBasis;
  switch (aSide) {
    case eSideTop:
    case eSideBottom:
      percentageBasis = aScrollPortSize.height;
      break;
    case eSideLeft:
    case eSideRight:
      percentageBasis = aScrollPortSize.width;
      break;
  }

  return aScrollPaddingStyle.Get(aSide).AsLengthPercentage().Resolve(
      percentageBasis);
}

static nsMargin ResolveScrollPaddingStyle(
    const StyleRect<mozilla::NonNegativeLengthPercentageOrAuto>&
        aScrollPaddingStyle,
    const nsSize& aScrollPortSize) {
  return nsMargin(ResolveScrollPaddingStyleValue(aScrollPaddingStyle, eSideTop,
                                                 aScrollPortSize),
                  ResolveScrollPaddingStyleValue(aScrollPaddingStyle,
                                                 eSideRight, aScrollPortSize),
                  ResolveScrollPaddingStyleValue(aScrollPaddingStyle,
                                                 eSideBottom, aScrollPortSize),
                  ResolveScrollPaddingStyleValue(aScrollPaddingStyle, eSideLeft,
                                                 aScrollPortSize));
}

nsMargin ScrollFrameHelper::GetScrollPadding() const {
  nsIFrame* styleFrame = GetFrameForStyle();
  if (!styleFrame) {
    return nsMargin();
  }

  // The spec says percentage values are relative to the scroll port size.
  // https://drafts.csswg.org/css-scroll-snap-1/#scroll-padding
  return ResolveScrollPaddingStyle(styleFrame->StylePadding()->mScrollPadding,
                                   GetScrollPortRect().Size());
}

layers::ScrollSnapInfo ScrollFrameHelper::ComputeScrollSnapInfo() const {
  ScrollSnapInfo result;

  nsIFrame* scrollSnapFrame = GetFrameForStyle();
  if (!scrollSnapFrame) {
    return result;
  }

  const nsStyleDisplay* disp = scrollSnapFrame->StyleDisplay();
  if (disp->mScrollSnapType.strictness == StyleScrollSnapStrictness::None) {
    // We won't be snapping, short-circuit the computation.
    return result;
  }

  WritingMode writingMode = mOuter->GetWritingMode();
  result.InitializeScrollSnapStrictness(writingMode, disp);

  nsRect snapport = GetScrollPortRect();
  nsMargin scrollPadding = GetScrollPadding();
  snapport.Deflate(scrollPadding);

  result.mSnapportSize = snapport.Size();
  CollectScrollPositionsForSnap(mScrolledFrame, mScrolledFrame,
                                GetScrolledRect(), scrollPadding, writingMode,
                                result);
  return result;
}

layers::ScrollSnapInfo ScrollFrameHelper::GetScrollSnapInfo() const {
  // TODO(botond): Should we cache it?
  return ComputeScrollSnapInfo();
}

bool ScrollFrameHelper::GetSnapPointForDestination(ScrollUnit aUnit,
                                                   ScrollSnapFlags aSnapFlags,
                                                   const nsPoint& aStartPos,
                                                   nsPoint& aDestination) {
  Maybe<nsPoint> snapPoint = ScrollSnapUtils::GetSnapPointForDestination(
      GetScrollSnapInfo(), aUnit, aSnapFlags, GetLayoutScrollRange(), aStartPos,
      aDestination);
  if (snapPoint) {
    aDestination = snapPoint.ref();
    return true;
  }
  return false;
}

bool ScrollFrameHelper::UsesOverlayScrollbars() const {
  return mOuter->PresContext()->UseOverlayScrollbars();
}

bool ScrollFrameHelper::DragScroll(WidgetEvent* aEvent) {
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
  nsPoint pnt =
      nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, RelativeTo{mOuter});
  nsPoint scrollPoint = GetScrollPosition();
  nsRect rangeRect = GetLayoutScrollRange();

  // Only drag scroll when a scrollbar is present.
  nsPoint offset;
  if (mHasHorizontalScrollbar) {
    if (pnt.x >= mScrollPort.x && pnt.x <= mScrollPort.x + margin) {
      offset.x = -margin;
      if (scrollPoint.x > 0) {
        willScroll = true;
      }
    } else if (pnt.x >= mScrollPort.XMost() - margin &&
               pnt.x <= mScrollPort.XMost()) {
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
    } else if (pnt.y >= mScrollPort.YMost() - margin &&
               pnt.y <= mScrollPort.YMost()) {
      offset.y = margin;
      if (scrollPoint.y < rangeRect.height) {
        willScroll = true;
      }
    }
  }

  if (offset.x || offset.y) {
    ScrollTo(GetScrollPosition() + offset, ScrollMode::Normal,
             ScrollOrigin::Other);
  }

  return willScroll;
}

static nsSliderFrame* GetSliderFrame(nsIFrame* aScrollbarFrame) {
  if (!aScrollbarFrame) {
    return nullptr;
  }

  for (const auto& childList : aScrollbarFrame->ChildLists()) {
    for (nsIFrame* frame : childList.mList) {
      if (nsSliderFrame* sliderFrame = do_QueryFrame(frame)) {
        return sliderFrame;
      }
    }
  }
  return nullptr;
}

static void AsyncScrollbarDragInitiated(uint64_t aDragBlockId,
                                        nsIFrame* aScrollbar) {
  if (nsSliderFrame* sliderFrame = GetSliderFrame(aScrollbar)) {
    sliderFrame->AsyncScrollbarDragInitiated(aDragBlockId);
  }
}

void ScrollFrameHelper::AsyncScrollbarDragInitiated(
    uint64_t aDragBlockId, ScrollDirection aDirection) {
  switch (aDirection) {
    case ScrollDirection::eVertical:
      ::AsyncScrollbarDragInitiated(aDragBlockId, mVScrollbarBox);
      break;
    case ScrollDirection::eHorizontal:
      ::AsyncScrollbarDragInitiated(aDragBlockId, mHScrollbarBox);
      break;
  }
}

static void AsyncScrollbarDragRejected(nsIFrame* aScrollbar) {
  if (nsSliderFrame* sliderFrame = GetSliderFrame(aScrollbar)) {
    sliderFrame->AsyncScrollbarDragRejected();
  }
}

void ScrollFrameHelper::AsyncScrollbarDragRejected() {
  // We don't get told which scrollbar requested the async drag,
  // so we notify both.
  ::AsyncScrollbarDragRejected(mHScrollbarBox);
  ::AsyncScrollbarDragRejected(mVScrollbarBox);
}

void ScrollFrameHelper::ApzSmoothScrollTo(
    const nsPoint& aDestination, ScrollOrigin aOrigin,
    ScrollTriggeredByScript aTriggeredByScript) {
  if (mApzSmoothScrollDestination == Some(aDestination)) {
    // If we already sent APZ a smooth-scroll request to this
    // destination (i.e. it was the last request
    // we sent), then don't send another one because it is redundant.
    // This is to avoid a scenario where pages do repeated scrollBy
    // calls, incrementing the generation counter, and blocking APZ from
    // syncing the scroll offset back to the main thread.
    // Note that if we get two smooth-scroll requests to the same
    // destination with some other scroll in between,
    // mApzSmoothScrollDestination will get reset to Nothing() and so
    // we shouldn't have the problem where this check discards a
    // legitimate smooth-scroll.
    return;
  }

  // The animation will be handled in the compositor, pass the
  // information needed to start the animation and skip the main-thread
  // animation for this scroll.
  MOZ_ASSERT(aOrigin != ScrollOrigin::None);
  mApzSmoothScrollDestination = Some(aDestination);
  AppendScrollUpdate(ScrollPositionUpdate::NewSmoothScroll(
      aOrigin, aDestination, aTriggeredByScript));

  nsIContent* content = mOuter->GetContent();
  if (!DisplayPortUtils::HasNonMinimalNonZeroDisplayPort(content)) {
    // If this frame doesn't have a displayport then there won't be an
    // APZC instance for it and so there won't be anything to process
    // this smooth scroll request. We should set a displayport on this
    // frame to force an APZC which can handle the request.
    if (MOZ_LOG_TEST(sDisplayportLog, LogLevel::Debug)) {
      mozilla::layers::ScrollableLayerGuid::ViewID viewID =
          mozilla::layers::ScrollableLayerGuid::NULL_SCROLL_ID;
      nsLayoutUtils::FindIDFor(content, &viewID);
      MOZ_LOG(
          sDisplayportLog, LogLevel::Debug,
          ("ApzSmoothScrollTo setting displayport on scrollId=%" PRIu64 "\n",
           viewID));
    }

    DisplayPortUtils::CalculateAndSetDisplayPortMargins(
        mOuter->GetScrollTargetFrame(), DisplayPortUtils::RepaintMode::Repaint);
    nsIFrame* frame = do_QueryFrame(mOuter->GetScrollTargetFrame());
    DisplayPortUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(frame);
  }

  // Schedule a paint to ensure that the frame metrics get updated on
  // the compositor thread.
  mOuter->SchedulePaint();
}

bool ScrollFrameHelper::SmoothScrollVisual(
    const nsPoint& aVisualViewportOffset,
    FrameMetrics::ScrollOffsetUpdateType aUpdateType) {
  bool canDoApzSmoothScroll =
      nsLayoutUtils::AsyncPanZoomEnabled(mOuter) && WantAsyncScroll();
  if (!canDoApzSmoothScroll) {
    return false;
  }

  // Clamp the destination to the visual scroll range.
  // There is a potential issue here, where |mDestination| is usually
  // clamped to the layout scroll range, and so e.g. a subsequent
  // window.scrollBy() may have an undesired effect. However, as this function
  // is only called internally, this should not be a problem in practice.
  // If it turns out to be, the fix would be:
  //   - add a new "destination" field that doesn't have to be clamped to
  //     the layout scroll range
  //   - clamp mDestination to the layout scroll range here
  //   - make sure ComputeScrollMetadata() picks up the former as the
  //     smooth scroll destination to send to APZ.
  mDestination = GetVisualScrollRange().ClampPoint(aVisualViewportOffset);

  // Perform the scroll.
  ApzSmoothScrollTo(mDestination,
                    aUpdateType == FrameMetrics::eRestore
                        ? ScrollOrigin::Restore
                        : ScrollOrigin::Other,
                    ScrollTriggeredByScript::No);
  return true;
}

bool ScrollFrameHelper::IsSmoothScroll(dom::ScrollBehavior aBehavior) const {
  if (aBehavior == dom::ScrollBehavior::Smooth) {
    return true;
  }

  nsIFrame* styleFrame = GetFrameForStyle();
  if (!styleFrame) {
    return false;
  }
  return (aBehavior == dom::ScrollBehavior::Auto &&
          styleFrame->StyleDisplay()->mScrollBehavior ==
              StyleScrollBehavior::Smooth);
}

nsTArray<ScrollPositionUpdate> ScrollFrameHelper::GetScrollUpdates() const {
  return mScrollUpdates.Clone();
}

void ScrollFrameHelper::AppendScrollUpdate(
    const ScrollPositionUpdate& aUpdate) {
  mScrollGeneration = aUpdate.GetGeneration();
  mScrollUpdates.AppendElement(aUpdate);
}

void ScrollFrameHelper::ScheduleScrollAnimations() {
  MOZ_ASSERT(mOuter);
  nsIContent* content = mOuter->GetContent();
  MOZ_ASSERT(content && content->IsElement(),
             "The nsIScrollableFrame should have the element.");

  const auto* set =
      ScrollTimelineSet::GetScrollTimelineSet(content->AsElement());
  if (!set) {
    // We don't have scroll timelines associated with this frame.
    return;
  }

  set->ScheduleAnimations();
}
