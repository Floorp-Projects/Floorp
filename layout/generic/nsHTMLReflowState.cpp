/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* struct containing the input to nsIFrame::Reflow */

#include "nsHTMLReflowState.h"

#include "LayoutLogging.h"
#include "nsStyleConsts.h"
#include "nsCSSAnonBoxes.h"
#include "nsFrame.h"
#include "nsIContent.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsFontMetrics.h"
#include "nsBlockFrame.h"
#include "nsLineBox.h"
#include "nsImageFrame.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsIPercentBSizeObserver.h"
#include "nsLayoutUtils.h"
#include "mozilla/Preferences.h"
#include "nsFontInflationData.h"
#include "StickyScrollContainer.h"
#include "nsIFrameInlines.h"
#include "CounterStyleManager.h"
#include <algorithm>
#include "mozilla/dom/HTMLInputElement.h"

#ifdef DEBUG
#undef NOISY_VERTICAL_ALIGN
#else
#undef NOISY_VERTICAL_ALIGN
#endif

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;
using namespace mozilla::layout;

enum eNormalLineHeightControl {
  eUninitialized = -1,
  eNoExternalLeading = 0,   // does not include external leading 
  eIncludeExternalLeading,  // use whatever value font vendor provides
  eCompensateLeading        // compensate leading if leading provided by font vendor is not enough
};

static eNormalLineHeightControl sNormalLineHeightControl = eUninitialized;

// Initialize a <b>root</b> reflow state with a rendering context to
// use for measuring things.
nsHTMLReflowState::nsHTMLReflowState(nsPresContext*       aPresContext,
                                     nsIFrame*            aFrame,
                                     nsRenderingContext*  aRenderingContext,
                                     const LogicalSize&   aAvailableSpace,
                                     uint32_t             aFlags)
  : nsCSSOffsetState(aFrame, aRenderingContext)
  , mBlockDelta(0)
  , mOrthogonalLimit(NS_UNCONSTRAINEDSIZE)
  , mReflowDepth(0)
{
  NS_PRECONDITION(aRenderingContext, "no rendering context");
  MOZ_ASSERT(aPresContext, "no pres context");
  MOZ_ASSERT(aFrame, "no frame");
  MOZ_ASSERT(aPresContext == aFrame->PresContext(), "wrong pres context");
  mParentReflowState = nullptr;
  AvailableISize() = aAvailableSpace.ISize(mWritingMode);
  AvailableBSize() = aAvailableSpace.BSize(mWritingMode);
  mFloatManager = nullptr;
  mLineLayout = nullptr;
  mDiscoveredClearance = nullptr;
  mPercentBSizeObserver = nullptr;

  if (aFlags & DUMMY_PARENT_REFLOW_STATE) {
    mFlags.mDummyParentReflowState = true;
  }
  if (aFlags & COMPUTE_SIZE_SHRINK_WRAP) {
    mFlags.mShrinkWrap = true;
  }
  if (aFlags & COMPUTE_SIZE_USE_AUTO_BSIZE) {
    mFlags.mUseAutoBSize = true;
  }
  if (aFlags & STATIC_POS_IS_CB_ORIGIN) {
    mFlags.mStaticPosIsCBOrigin = true;
  }

  if (!(aFlags & CALLER_WILL_INIT)) {
    Init(aPresContext);
  }
}

static bool CheckNextInFlowParenthood(nsIFrame* aFrame, nsIFrame* aParent)
{
  nsIFrame* frameNext = aFrame->GetNextInFlow();
  nsIFrame* parentNext = aParent->GetNextInFlow();
  return frameNext && parentNext && frameNext->GetParent() == parentNext;
}

/**
 * Adjusts the margin for a list (ol, ul), if necessary, depending on
 * font inflation settings. Unfortunately, because bullets from a list are
 * placed in the margin area, we only have ~40px in which to place the
 * bullets. When they are inflated, however, this causes problems, since
 * the text takes up more space than is available in the margin.
 *
 * This method will return a small amount (in app units) by which the
 * margin can be adjusted, so that the space is available for list
 * bullets to be rendered with font inflation enabled.
 */
static  nscoord
FontSizeInflationListMarginAdjustment(const nsIFrame* aFrame)
{
  float inflation = nsLayoutUtils::FontSizeInflationFor(aFrame);
  if (aFrame->IsFrameOfType(nsIFrame::eBlockFrame)) {
    const nsBlockFrame* blockFrame = static_cast<const nsBlockFrame*>(aFrame);

    // We only want to adjust the margins if we're dealing with an ordered
    // list.
    if (inflation > 1.0f &&
        blockFrame->HasBullet() &&
        inflation > 1.0f) {

      auto listStyleType = aFrame->StyleList()->GetCounterStyle()->GetStyle();
      if (listStyleType != NS_STYLE_LIST_STYLE_NONE &&
          listStyleType != NS_STYLE_LIST_STYLE_DISC &&
          listStyleType != NS_STYLE_LIST_STYLE_CIRCLE &&
          listStyleType != NS_STYLE_LIST_STYLE_SQUARE &&
          listStyleType != NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED &&
          listStyleType != NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN) {
        // The HTML spec states that the default padding for ordered lists
        // begins at 40px, indicating that we have 40px of space to place a
        // bullet. When performing font inflation calculations, we add space
        // equivalent to this, but simply inflated at the same amount as the
        // text, in app units.
        return nsPresContext::CSSPixelsToAppUnits(40) * (inflation - 1);
      }

    }
  }

  return 0;
}

// NOTE: If we ever want to use nsCSSOffsetState for a flex item or a
// grid item, we need to make it take the containing-block block-size as
// well as the inline-size, since flex items and grid items resolve
// block-direction percent margins and padding against the
// containing-block block-size, rather than its inline-size.
nsCSSOffsetState::nsCSSOffsetState(nsIFrame *aFrame,
                                   nsRenderingContext *aRenderingContext,
                                   WritingMode aContainingBlockWritingMode,
                                   nscoord aContainingBlockISize)
  : frame(aFrame)
  , rendContext(aRenderingContext)
  , mWritingMode(aFrame->GetWritingMode())
{
  MOZ_ASSERT(!aFrame->IsFlexOrGridItem(),
             "We're about to resolve percent margin & padding "
             "values against CB inline size, which is incorrect for "
             "flex/grid items. "
             "Additionally for grid items, this path doesn't handle baseline "
             "padding contribution - see nsCSSOffsetState::InitOffsets");
  LogicalSize cbSize(aContainingBlockWritingMode, aContainingBlockISize,
                     aContainingBlockISize);
  ReflowStateFlags flags;
  InitOffsets(aContainingBlockWritingMode, cbSize, frame->GetType(), flags);
}

// Initialize a reflow state for a child frame's reflow. Some state
// is copied from the parent reflow state; the remaining state is
// computed.
nsHTMLReflowState::nsHTMLReflowState(
                     nsPresContext*           aPresContext,
                     const nsHTMLReflowState& aParentReflowState,
                     nsIFrame*                aFrame,
                     const LogicalSize&       aAvailableSpace,
                     const LogicalSize*       aContainingBlockSize,
                     uint32_t                 aFlags)
  : nsCSSOffsetState(aFrame, aParentReflowState.rendContext)
  , mBlockDelta(0)
  , mOrthogonalLimit(NS_UNCONSTRAINEDSIZE)
  , mReflowDepth(aParentReflowState.mReflowDepth + 1)
  , mFlags(aParentReflowState.mFlags)
{
  MOZ_ASSERT(aPresContext, "no pres context");
  MOZ_ASSERT(aFrame, "no frame");
  MOZ_ASSERT(aPresContext == aFrame->PresContext(), "wrong pres context");
  NS_PRECONDITION(!mFlags.mSpecialBSizeReflow ||
                  !NS_SUBTREE_DIRTY(aFrame),
                  "frame should be clean when getting special bsize reflow");

  mParentReflowState = &aParentReflowState;

  // If the parent is dirty, then the child is as well.
  // XXX Are the other cases where the parent reflows a child a second
  // time, as a resize?
  if (!mFlags.mSpecialBSizeReflow)
    frame->AddStateBits(mParentReflowState->frame->GetStateBits() &
                        NS_FRAME_IS_DIRTY);

  AvailableISize() = aAvailableSpace.ISize(mWritingMode);
  AvailableBSize() = aAvailableSpace.BSize(mWritingMode);

  if (mWritingMode.IsOrthogonalTo(aParentReflowState.GetWritingMode())) {
    // If we're setting up for an orthogonal flow, and the parent reflow state
    // had a constrained ComputedBSize, we can use that as our AvailableISize
    // in preference to leaving it unconstrained.
    if (AvailableISize() == NS_UNCONSTRAINEDSIZE &&
        aParentReflowState.ComputedBSize() != NS_UNCONSTRAINEDSIZE) {
      AvailableISize() = aParentReflowState.ComputedBSize();
    }
  }

  mFloatManager = aParentReflowState.mFloatManager;
  if (frame->IsFrameOfType(nsIFrame::eLineParticipant))
    mLineLayout = aParentReflowState.mLineLayout;
  else
    mLineLayout = nullptr;

  // Note: mFlags was initialized as a copy of aParentReflowState.mFlags up in
  // this constructor's init list, so the only flags that we need to explicitly
  // initialize here are those that may need a value other than our parent's.
  mFlags.mNextInFlowUntouched = aParentReflowState.mFlags.mNextInFlowUntouched &&
    CheckNextInFlowParenthood(aFrame, aParentReflowState.frame);
  mFlags.mAssumingHScrollbar = mFlags.mAssumingVScrollbar = false;
  mFlags.mIsColumnBalancing = false;
  mFlags.mIsFlexContainerMeasuringHeight = false;
  mFlags.mDummyParentReflowState = false;
  mFlags.mShrinkWrap = !!(aFlags & COMPUTE_SIZE_SHRINK_WRAP);
  mFlags.mUseAutoBSize = !!(aFlags & COMPUTE_SIZE_USE_AUTO_BSIZE);
  mFlags.mStaticPosIsCBOrigin = !!(aFlags & STATIC_POS_IS_CB_ORIGIN);

  mDiscoveredClearance = nullptr;
  mPercentBSizeObserver = (aParentReflowState.mPercentBSizeObserver &&
                            aParentReflowState.mPercentBSizeObserver->NeedsToObserve(*this))
                           ? aParentReflowState.mPercentBSizeObserver : nullptr;

  if ((aFlags & DUMMY_PARENT_REFLOW_STATE) ||
      (mParentReflowState->mFlags.mDummyParentReflowState &&
       frame->GetType() == nsGkAtoms::tableFrame)) {
    mFlags.mDummyParentReflowState = true;
  }

  if (!(aFlags & CALLER_WILL_INIT)) {
    Init(aPresContext, aContainingBlockSize);
  }
}

inline nscoord
nsCSSOffsetState::ComputeISizeValue(nscoord aContainingBlockISize,
                                    nscoord aContentEdgeToBoxSizing,
                                    nscoord aBoxSizingToMarginEdge,
                                    const nsStyleCoord& aCoord) const
{
  return nsLayoutUtils::ComputeISizeValue(rendContext, frame,
                                          aContainingBlockISize,
                                          aContentEdgeToBoxSizing,
                                          aBoxSizingToMarginEdge,
                                          aCoord);
}

nscoord
nsCSSOffsetState::ComputeISizeValue(nscoord aContainingBlockISize,
                                    StyleBoxSizing aBoxSizing,
                                    const nsStyleCoord& aCoord) const
{
  WritingMode wm = GetWritingMode();
  nscoord inside = 0, outside = ComputedLogicalBorderPadding().IStartEnd(wm) +
                                ComputedLogicalMargin().IStartEnd(wm);
  switch (aBoxSizing) {
    case StyleBoxSizing::Border:
      inside = ComputedLogicalBorderPadding().IStartEnd(wm);
      break;
    case StyleBoxSizing::Padding:
      inside = ComputedLogicalPadding().IStartEnd(wm);
      break;
    case StyleBoxSizing::Content:
      // nothing
      break;
  }
  outside -= inside;

  return ComputeISizeValue(aContainingBlockISize, inside,
                           outside, aCoord);
}

nscoord
nsCSSOffsetState::ComputeBSizeValue(nscoord aContainingBlockBSize,
                                    StyleBoxSizing aBoxSizing,
                                    const nsStyleCoord& aCoord) const
{
  WritingMode wm = GetWritingMode();
  nscoord inside = 0;
  switch (aBoxSizing) {
    case StyleBoxSizing::Border:
      inside = ComputedLogicalBorderPadding().BStartEnd(wm);
      break;
    case StyleBoxSizing::Padding:
      inside = ComputedLogicalPadding().BStartEnd(wm);
      break;
    case StyleBoxSizing::Content:
      // nothing
      break;
  }
  return nsLayoutUtils::ComputeBSizeValue(aContainingBlockBSize,
                                          inside, aCoord);
}

void
nsHTMLReflowState::SetComputedWidth(nscoord aComputedWidth)
{
  NS_ASSERTION(frame, "Must have a frame!");
  // It'd be nice to assert that |frame| is not in reflow, but this fails for
  // two reasons:
  //
  // 1) Viewport frames reset the computed width on a copy of their reflow
  //    state when reflowing fixed-pos kids.  In that case we actually don't
  //    want to mess with the resize flags, because comparing the frame's rect
  //    to the munged computed width is pointless.
  // 2) nsFrame::BoxReflow creates a reflow state for its parent.  This reflow
  //    state is not used to reflow the parent, but just as a parent for the
  //    frame's own reflow state.  So given a nsBoxFrame inside some non-XUL
  //    (like a text control, for example), we'll end up creating a reflow
  //    state for the parent while the parent is reflowing.

  NS_PRECONDITION(aComputedWidth >= 0, "Invalid computed width");
  if (ComputedWidth() != aComputedWidth) {
    ComputedWidth() = aComputedWidth;
    nsIAtom* frameType = frame->GetType();
    if (frameType != nsGkAtoms::viewportFrame || // Or check GetParent()?
        mWritingMode.IsVertical()) {
      InitResizeFlags(frame->PresContext(), frameType);
    }
  }
}

void
nsHTMLReflowState::SetComputedHeight(nscoord aComputedHeight)
{
  NS_ASSERTION(frame, "Must have a frame!");
  // It'd be nice to assert that |frame| is not in reflow, but this fails
  // because:
  //
  //    nsFrame::BoxReflow creates a reflow state for its parent.  This reflow
  //    state is not used to reflow the parent, but just as a parent for the
  //    frame's own reflow state.  So given a nsBoxFrame inside some non-XUL
  //    (like a text control, for example), we'll end up creating a reflow
  //    state for the parent while the parent is reflowing.

  NS_PRECONDITION(aComputedHeight >= 0, "Invalid computed height");
  if (ComputedHeight() != aComputedHeight) {
    ComputedHeight() = aComputedHeight;
    nsIAtom* frameType = frame->GetType();
    if (frameType != nsGkAtoms::viewportFrame || !mWritingMode.IsVertical()) {
      InitResizeFlags(frame->PresContext(), frameType);
    }
  }
}

void
nsHTMLReflowState::Init(nsPresContext*     aPresContext,
                        const LogicalSize* aContainingBlockSize,
                        const nsMargin*    aBorder,
                        const nsMargin*    aPadding)
{
  if (AvailableISize() == NS_UNCONSTRAINEDSIZE) {
    // Look up the parent chain for an orthogonal inline limit,
    // and reset AvailableISize() if found.
    for (const nsHTMLReflowState *parent = mParentReflowState;
         parent != nullptr; parent = parent->mParentReflowState) {
      if (parent->GetWritingMode().IsOrthogonalTo(mWritingMode) &&
          parent->mOrthogonalLimit != NS_UNCONSTRAINEDSIZE) {
        AvailableISize() = parent->mOrthogonalLimit;
        break;
      }
    }
  }

  LAYOUT_WARN_IF_FALSE(AvailableISize() != NS_UNCONSTRAINEDSIZE,
                       "have unconstrained inline-size; this should only "
                       "result from very large sizes, not attempts at "
                       "intrinsic inline-size calculation");

  mStylePosition = frame->StylePosition();
  mStyleDisplay = frame->StyleDisplay();
  mStyleVisibility = frame->StyleVisibility();
  mStyleBorder = frame->StyleBorder();
  mStyleMargin = frame->StyleMargin();
  mStylePadding = frame->StylePadding();
  mStyleText = frame->StyleText();

  nsIAtom* type = frame->GetType();

  InitFrameType(type);
  InitCBReflowState();

  LogicalSize cbSize(mWritingMode, -1, -1);
  if (aContainingBlockSize) {
    cbSize = *aContainingBlockSize;
  }

  InitConstraints(aPresContext, cbSize, aBorder, aPadding, type);

  InitResizeFlags(aPresContext, type);

  nsIFrame *parent = frame->GetParent();
  if (parent &&
      (parent->GetStateBits() & NS_FRAME_IN_CONSTRAINED_BSIZE) &&
      !(parent->GetType() == nsGkAtoms::scrollFrame &&
        parent->StyleDisplay()->mOverflowY != NS_STYLE_OVERFLOW_HIDDEN)) {
    frame->AddStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE);
  } else if (type == nsGkAtoms::svgForeignObjectFrame) {
    // An SVG foreignObject frame is inherently constrained block-size.
    frame->AddStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE);
  } else {
    const nsStyleCoord& bSizeCoord = mStylePosition->BSize(mWritingMode);
    const nsStyleCoord& maxBSizeCoord = mStylePosition->MaxBSize(mWritingMode);
    if ((bSizeCoord.GetUnit() != eStyleUnit_Auto ||
         maxBSizeCoord.GetUnit() != eStyleUnit_None) &&
         // Don't set NS_FRAME_IN_CONSTRAINED_BSIZE on body or html elements.
         (frame->GetContent() &&
        !(frame->GetContent()->IsAnyOfHTMLElements(nsGkAtoms::body,
                                                   nsGkAtoms::html)))) {

      // If our block-size was specified as a percentage, then this could
      // actually resolve to 'auto', based on:
      // http://www.w3.org/TR/CSS21/visudet.html#the-height-property
      nsIFrame* containingBlk = frame;
      while (containingBlk) {
        const nsStylePosition* stylePos = containingBlk->StylePosition();
        const nsStyleCoord& bSizeCoord = stylePos->BSize(mWritingMode);
        const nsStyleCoord& maxBSizeCoord = stylePos->MaxBSize(mWritingMode);
        if ((bSizeCoord.IsCoordPercentCalcUnit() &&
             !bSizeCoord.HasPercent()) ||
            (maxBSizeCoord.IsCoordPercentCalcUnit() &&
             !maxBSizeCoord.HasPercent())) {
          frame->AddStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE);
          break;
        } else if ((bSizeCoord.IsCoordPercentCalcUnit() &&
                    bSizeCoord.HasPercent()) ||
                   (maxBSizeCoord.IsCoordPercentCalcUnit() &&
                    maxBSizeCoord.HasPercent())) {
          if (!(containingBlk = containingBlk->GetContainingBlock())) {
            // If we've reached the top of the tree, then we don't have
            // a constrained block-size.
            frame->RemoveStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE);
            break;
          }

          continue;
        } else {
          frame->RemoveStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE);
          break;
        }
      }
    } else {
      frame->RemoveStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE);
    }
  }

  if (mParentReflowState &&
      mParentReflowState->GetWritingMode().IsOrthogonalTo(mWritingMode)) {
    // Orthogonal frames are always reflowed with an unconstrained
    // dimension to avoid incomplete reflow across an orthogonal
    // boundary. Normally this is the block-size, but for column sets
    // with auto-height it's the inline-size, so that they can add
    // columns in the container's block direction
    if (type == nsGkAtoms::columnSetFrame &&
        eStyleUnit_Auto == mStylePosition->ISize(mWritingMode).GetUnit()) {
      ComputedISize() = NS_UNCONSTRAINEDSIZE;
    } else {
      AvailableBSize() = NS_UNCONSTRAINEDSIZE;
    }
  }

  LAYOUT_WARN_IF_FALSE((mFrameType == NS_CSS_FRAME_TYPE_INLINE &&
                        !frame->IsFrameOfType(nsIFrame::eReplaced)) ||
                       type == nsGkAtoms::textFrame ||
                       ComputedISize() != NS_UNCONSTRAINEDSIZE,
                       "have unconstrained inline-size; this should only "
                       "result from very large sizes, not attempts at "
                       "intrinsic inline-size calculation");
}

void nsHTMLReflowState::InitCBReflowState()
{
  if (!mParentReflowState) {
    mCBReflowState = nullptr;
    return;
  }

  if (mParentReflowState->frame == frame->GetContainingBlock()) {
    // Inner table frames need to use the containing block of the outer
    // table frame.
    if (frame->GetType() == nsGkAtoms::tableFrame) {
      mCBReflowState = mParentReflowState->mCBReflowState;
    } else {
      mCBReflowState = mParentReflowState;
    }
  } else {
    mCBReflowState = mParentReflowState->mCBReflowState;
  }
}

/* Check whether CalcQuirkContainingBlockHeight would stop on the
 * given reflow state, using its block as a height.  (essentially 
 * returns false for any case in which CalcQuirkContainingBlockHeight 
 * has a "continue" in its main loop.)
 *
 * XXX Maybe refactor CalcQuirkContainingBlockHeight so it uses 
 * this function as well
 */
static bool
IsQuirkContainingBlockHeight(const nsHTMLReflowState* rs, nsIAtom* aFrameType)
{
  if (nsGkAtoms::blockFrame == aFrameType ||
#ifdef MOZ_XUL
      nsGkAtoms::XULLabelFrame == aFrameType ||
#endif
      nsGkAtoms::scrollFrame == aFrameType) {
    // Note: This next condition could change due to a style change,
    // but that would cause a style reflow anyway, which means we're ok.
    if (NS_AUTOHEIGHT == rs->ComputedHeight()) {
      if (!rs->frame->IsAbsolutelyPositioned()) {
        return false;
      }
    }
  }
  return true;
}


void
nsHTMLReflowState::InitResizeFlags(nsPresContext* aPresContext, nsIAtom* aFrameType)
{
  const WritingMode wm = mWritingMode; // just a shorthand
  bool isIResize =
    frame->ISize(wm) !=
      ComputedISize() + ComputedLogicalBorderPadding().IStartEnd(wm);

  if ((frame->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT) &&
      nsLayoutUtils::FontSizeInflationEnabled(aPresContext)) {
    // Create our font inflation data if we don't have it already, and
    // give it our current width information.
    bool dirty = nsFontInflationData::UpdateFontInflationDataISizeFor(*this) &&
                 // Avoid running this at the box-to-block interface
                 // (where we shouldn't be inflating anyway, and where
                 // reflow state construction is probably to construct a
                 // dummy parent reflow state anyway).
                 !mFlags.mDummyParentReflowState;

    if (dirty || (!frame->GetParent() && isIResize)) {
      // When font size inflation is enabled, a change in either:
      //  * the effective width of a font inflation flow root
      //  * the width of the frame
      // needs to cause a dirty reflow since they change the font size
      // inflation calculations, which in turn change the size of text,
      // line-heights, etc.  This is relatively similar to a classic
      // case of style change reflow, except that because inflation
      // doesn't affect the intrinsic sizing codepath, there's no need
      // to invalidate intrinsic sizes.
      //
      // Note that this makes horizontal resizing a good bit more
      // expensive.  However, font size inflation is targeted at a set of
      // devices (zoom-and-pan devices) where the main use case for
      // horizontal resizing needing to be efficient (window resizing) is
      // not present.  It does still increase the cost of dynamic changes
      // caused by script where a style or content change in one place
      // causes a resize in another (e.g., rebalancing a table).

      // FIXME: This isn't so great for the cases where
      // nsHTMLReflowState::SetComputedWidth is called, if the first time
      // we go through InitResizeFlags we set IsHResize() to true, and then
      // the second time we'd set it to false even without the
      // NS_FRAME_IS_DIRTY bit already set.
      if (frame->GetType() == nsGkAtoms::svgForeignObjectFrame) {
        // Foreign object frames use dirty bits in a special way.
        frame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
        nsIFrame *kid = frame->PrincipalChildList().FirstChild();
        if (kid) {
          kid->AddStateBits(NS_FRAME_IS_DIRTY);
        }
      } else {
        frame->AddStateBits(NS_FRAME_IS_DIRTY);
      }

      // Mark intrinsic widths on all descendants dirty.  We need to do
      // this (1) since we're changing the size of text and need to
      // clear text runs on text frames and (2) since we actually are
      // changing some intrinsic widths, but only those that live inside
      // of containers.

      // It makes sense to do this for descendants but not ancestors
      // (which is unusual) because we're only changing the unusual
      // inflation-dependent intrinsic widths (i.e., ones computed with
      // nsPresContext::mInflationDisabledForShrinkWrap set to false),
      // which should never affect anything outside of their inflation
      // flow root (or, for that matter, even their inflation
      // container).

      // This is also different from what PresShell::FrameNeedsReflow
      // does because it doesn't go through placeholders.  It doesn't
      // need to because we're actually doing something that cares about
      // frame tree geometry (the width on an ancestor) rather than
      // style.

      AutoTArray<nsIFrame*, 32> stack;
      stack.AppendElement(frame);

      do {
        nsIFrame *f = stack.ElementAt(stack.Length() - 1);
        stack.RemoveElementAt(stack.Length() - 1);

        nsIFrame::ChildListIterator lists(f);
        for (; !lists.IsDone(); lists.Next()) {
          nsFrameList::Enumerator childFrames(lists.CurrentList());
          for (; !childFrames.AtEnd(); childFrames.Next()) {
            nsIFrame* kid = childFrames.get();
            kid->MarkIntrinsicISizesDirty();
            stack.AppendElement(kid);
          }
        }
      } while (stack.Length() != 0);
    }
  }

  SetIResize(!(frame->GetStateBits() & NS_FRAME_IS_DIRTY) &&
             isIResize);

  // XXX Should we really need to null check mCBReflowState?  (We do for
  // at least nsBoxFrame).
  if (IS_TABLE_CELL(aFrameType) &&
      (mFlags.mSpecialBSizeReflow ||
       (frame->FirstInFlow()->GetStateBits() &
         NS_TABLE_CELL_HAD_SPECIAL_REFLOW)) &&
      (frame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
    // Need to set the bit on the cell so that
    // mCBReflowState->IsBResize() is set correctly below when
    // reflowing descendant.
    SetBResize(true);
  } else if (mCBReflowState && frame->IsBlockWrapper()) {
    // XXX Is this problematic for relatively positioned inlines acting
    // as containing block for absolutely positioned elements?
    // Possibly; in that case we should at least be checking
    // NS_SUBTREE_DIRTY, I'd think.
    SetBResize(mCBReflowState->IsBResizeForWM(wm));
  } else if (mCBReflowState && !nsLayoutUtils::GetAsBlock(frame)) {
    // Some non-block frames (e.g. table frames) aggressively optimize out their
    // BSize recomputation when they don't have the BResize flag set.  This
    // means that if they go from having a computed non-auto height to having an
    // auto height and don't have that flag set, they will not actually compute
    // their auto height and will just remain at whatever size they already
    // were.  We can end up in that situation if the child has a percentage
    // specified height and the parent changes from non-auto height to auto
    // height.  When that happens, the parent will typically have the BResize
    // flag set, and we want to propagate that flag to the kid.
    //
    // Ideally it seems like we'd do this for blocks too, of course... but we'd
    // really want to restrict it to the percentage height case or something, to
    // avoid extra reflows in common cases.  Maybe we should be examining
    // mStylePosition->BSize(wm).GetUnit() for that purpose?
    //
    // Note that we _also_ need to set the BResize flag if we have auto
    // ComputedBSize() and a dirty subtree, since that might require us to
    // change BSize due to kids having been added or removed.
    SetBResize(mCBReflowState->IsBResizeForWM(wm));
    if (ComputedBSize() == NS_AUTOHEIGHT) {
      SetBResize(IsBResize() || NS_SUBTREE_DIRTY(frame));
    }
  } else if (ComputedBSize() == NS_AUTOHEIGHT) {
    if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
        mCBReflowState) {
      SetBResize(mCBReflowState->IsBResizeForWM(wm));
    } else {
      SetBResize(IsIResize());
    }
    SetBResize(IsBResize() || NS_SUBTREE_DIRTY(frame));
  } else {
    // not 'auto' block-size
    SetBResize(frame->BSize(wm) !=
               ComputedBSize() + ComputedLogicalBorderPadding().BStartEnd(wm));
  }

  bool dependsOnCBBSize =
    (mStylePosition->BSizeDependsOnContainer(wm) &&
     // FIXME: condition this on not-abspos?
     mStylePosition->BSize(wm).GetUnit() != eStyleUnit_Auto) ||
    mStylePosition->MinBSizeDependsOnContainer(wm) ||
    mStylePosition->MaxBSizeDependsOnContainer(wm) ||
    mStylePosition->OffsetHasPercent(wm.PhysicalSide(eLogicalSideBStart)) ||
    mStylePosition->mOffset.GetBEndUnit(wm) != eStyleUnit_Auto ||
    frame->IsXULBoxFrame();

  if (mStyleText->mLineHeight.GetUnit() == eStyleUnit_Enumerated) {
    NS_ASSERTION(mStyleText->mLineHeight.GetIntValue() ==
                 NS_STYLE_LINE_HEIGHT_BLOCK_HEIGHT,
                 "bad line-height value");

    // line-height depends on block bsize
    frame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
    // but only on containing blocks if this frame is not a suitable block
    dependsOnCBBSize |= !nsLayoutUtils::IsNonWrapperBlock(frame);
  }

  // If we're the descendant of a table cell that performs special bsize
  // reflows and we could be the child that requires them, always set
  // the block-axis resize in case this is the first pass before the
  // special bsize reflow.  However, don't do this if it actually is
  // the special bsize reflow, since in that case it will already be
  // set correctly above if we need it set.
  if (!IsBResize() && mCBReflowState &&
      (IS_TABLE_CELL(mCBReflowState->frame->GetType()) || 
       mCBReflowState->mFlags.mHeightDependsOnAncestorCell) &&
      !mCBReflowState->mFlags.mSpecialBSizeReflow && 
      dependsOnCBBSize) {
    SetBResize(true);
    mFlags.mHeightDependsOnAncestorCell = true;
  }

  // Set NS_FRAME_CONTAINS_RELATIVE_BSIZE if it's needed.

  // It would be nice to check that |ComputedBSize != NS_AUTOHEIGHT|
  // &&ed with the percentage bsize check.  However, this doesn't get
  // along with table special bsize reflows, since a special bsize
  // reflow (a quirk that makes such percentage height work on children
  // of table cells) can cause not just a single percentage height to
  // become fixed, but an entire descendant chain of percentage height
  // to become fixed.
  if (dependsOnCBBSize && mCBReflowState) {
    const nsHTMLReflowState *rs = this;
    bool hitCBReflowState = false;
    do {
      rs = rs->mParentReflowState;
      if (!rs) {
        break;
      }
        
      if (rs->frame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_BSIZE)
        break; // no need to go further
      rs->frame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
      
      // Keep track of whether we've hit the containing block, because
      // we need to go at least that far.
      if (rs == mCBReflowState) {
        hitCBReflowState = true;
      }

      // XXX What about orthogonal flows? It doesn't make sense to
      // keep propagating this bit across an orthogonal boundary,
      // where the meaning of BSize changes. Bug 1175517.
    } while (!hitCBReflowState ||
             (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
              !IsQuirkContainingBlockHeight(rs, rs->frame->GetType())));
    // Note: We actually don't need to set the
    // NS_FRAME_CONTAINS_RELATIVE_BSIZE bit for the cases
    // where we hit the early break statements in
    // CalcQuirkContainingBlockHeight. But it doesn't hurt
    // us to set the bit in these cases.
    
  }
  if (frame->GetStateBits() & NS_FRAME_IS_DIRTY) {
    // If we're reflowing everything, then we'll find out if we need
    // to re-set this.
    frame->RemoveStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
  }
}

nscoord
nsHTMLReflowState::GetContainingBlockContentISize(WritingMode aWritingMode) const
{
  if (!mCBReflowState) {
    return 0;
  }
  return mCBReflowState->GetWritingMode().IsOrthogonalTo(aWritingMode)
    ? mCBReflowState->ComputedBSize()
    : mCBReflowState->ComputedISize();
}

void
nsHTMLReflowState::InitFrameType(nsIAtom* aFrameType)
{
  const nsStyleDisplay *disp = mStyleDisplay;
  nsCSSFrameType frameType;

  // Section 9.7 of the CSS2 spec indicates that absolute position
  // takes precedence over float which takes precedence over display.
  // XXXldb nsRuleNode::ComputeDisplayData should take care of this, right?
  // Make sure the frame was actually moved out of the flow, and don't
  // just assume what the style says, because we might not have had a
  // useful float/absolute containing block

  DISPLAY_INIT_TYPE(frame, this);

  if (aFrameType == nsGkAtoms::tableFrame) {
    mFrameType = NS_CSS_FRAME_TYPE_BLOCK;
    return;
  }

  NS_ASSERTION(frame->StyleDisplay()->IsAbsolutelyPositionedStyle() ==
                 disp->IsAbsolutelyPositionedStyle(),
               "Unexpected position style");
  NS_ASSERTION(frame->StyleDisplay()->IsFloatingStyle() ==
                 disp->IsFloatingStyle(), "Unexpected float style");
  if (frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    if (disp->IsAbsolutelyPositioned(frame)) {
      frameType = NS_CSS_FRAME_TYPE_ABSOLUTE;
      //XXXfr hack for making frames behave properly when in overflow container lists
      //      see bug 154892; need to revisit later
      if (frame->GetPrevInFlow())
        frameType = NS_CSS_FRAME_TYPE_BLOCK;
    }
    else if (disp->IsFloating(frame)) {
      frameType = NS_CSS_FRAME_TYPE_FLOATING;
    } else {
      NS_ASSERTION(disp->mDisplay == NS_STYLE_DISPLAY_POPUP,
                   "unknown out of flow frame type");
      frameType = NS_CSS_FRAME_TYPE_UNKNOWN;
    }
  }
  else {
    switch (GetDisplay()) {
    case NS_STYLE_DISPLAY_BLOCK:
    case NS_STYLE_DISPLAY_LIST_ITEM:
    case NS_STYLE_DISPLAY_TABLE:
    case NS_STYLE_DISPLAY_TABLE_CAPTION:
    case NS_STYLE_DISPLAY_FLEX:
    case NS_STYLE_DISPLAY_WEBKIT_BOX:
    case NS_STYLE_DISPLAY_GRID:
    case NS_STYLE_DISPLAY_RUBY_TEXT_CONTAINER:
      frameType = NS_CSS_FRAME_TYPE_BLOCK;
      break;

    case NS_STYLE_DISPLAY_INLINE:
    case NS_STYLE_DISPLAY_INLINE_BLOCK:
    case NS_STYLE_DISPLAY_INLINE_TABLE:
    case NS_STYLE_DISPLAY_INLINE_BOX:
    case NS_STYLE_DISPLAY_INLINE_XUL_GRID:
    case NS_STYLE_DISPLAY_INLINE_STACK:
    case NS_STYLE_DISPLAY_INLINE_FLEX:
    case NS_STYLE_DISPLAY_WEBKIT_INLINE_BOX:
    case NS_STYLE_DISPLAY_INLINE_GRID:
    case NS_STYLE_DISPLAY_RUBY:
    case NS_STYLE_DISPLAY_RUBY_BASE:
    case NS_STYLE_DISPLAY_RUBY_TEXT:
    case NS_STYLE_DISPLAY_RUBY_BASE_CONTAINER:
      frameType = NS_CSS_FRAME_TYPE_INLINE;
      break;

    case NS_STYLE_DISPLAY_TABLE_CELL:
    case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
    case NS_STYLE_DISPLAY_TABLE_COLUMN:
    case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
    case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
    case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
    case NS_STYLE_DISPLAY_TABLE_ROW:
      frameType = NS_CSS_FRAME_TYPE_INTERNAL_TABLE;
      break;

    case NS_STYLE_DISPLAY_NONE:
    default:
      frameType = NS_CSS_FRAME_TYPE_UNKNOWN;
      break;
    }
  }

  // See if the frame is replaced
  if (frame->IsFrameOfType(nsIFrame::eReplacedContainsBlock)) {
    frameType = NS_FRAME_REPLACED_CONTAINS_BLOCK(frameType);
  } else if (frame->IsFrameOfType(nsIFrame::eReplaced)) {
    frameType = NS_FRAME_REPLACED(frameType);
  }

  mFrameType = frameType;
}

/* static */ void
nsHTMLReflowState::ComputeRelativeOffsets(WritingMode aWM,
                                          nsIFrame* aFrame,
                                          const LogicalSize& aCBSize,
                                          nsMargin& aComputedOffsets)
{
  LogicalMargin offsets(aWM);
  mozilla::css::Side inlineStart = aWM.PhysicalSide(eLogicalSideIStart);
  mozilla::css::Side inlineEnd   = aWM.PhysicalSide(eLogicalSideIEnd);
  mozilla::css::Side blockStart  = aWM.PhysicalSide(eLogicalSideBStart);
  mozilla::css::Side blockEnd    = aWM.PhysicalSide(eLogicalSideBEnd);

  const nsStylePosition* position = aFrame->StylePosition();

  // Compute the 'inlineStart' and 'inlineEnd' values. 'inlineStart'
  // moves the boxes to the end of the line, and 'inlineEnd' moves the
  // boxes to the start of the line. The computed values are always:
  // inlineStart=-inlineEnd
  bool inlineStartIsAuto =
    eStyleUnit_Auto == position->mOffset.GetUnit(inlineStart);
  bool inlineEndIsAuto =
    eStyleUnit_Auto == position->mOffset.GetUnit(inlineEnd);

  // If neither 'inlineStart' nor 'inlineEnd' is auto, then we're
  // over-constrained and we ignore one of them
  if (!inlineStartIsAuto && !inlineEndIsAuto) {
    inlineEndIsAuto = true;
  }

  if (inlineStartIsAuto) {
    if (inlineEndIsAuto) {
      // If both are 'auto' (their initial values), the computed values are 0
      offsets.IStart(aWM) = offsets.IEnd(aWM) = 0;
    } else {
      // 'inlineEnd' isn't 'auto' so compute its value
      offsets.IEnd(aWM) = nsLayoutUtils::
        ComputeCBDependentValue(aCBSize.ISize(aWM),
                                position->mOffset.Get(inlineEnd));

      // Computed value for 'inlineStart' is minus the value of 'inlineEnd'
      offsets.IStart(aWM) = -offsets.IEnd(aWM);
    }

  } else {
    NS_ASSERTION(inlineEndIsAuto, "unexpected specified constraint");

    // 'InlineStart' isn't 'auto' so compute its value
    offsets.IStart(aWM) = nsLayoutUtils::
      ComputeCBDependentValue(aCBSize.ISize(aWM),
                              position->mOffset.Get(inlineStart));

    // Computed value for 'inlineEnd' is minus the value of 'inlineStart'
    offsets.IEnd(aWM) = -offsets.IStart(aWM);
  }

  // Compute the 'blockStart' and 'blockEnd' values. The 'blockStart'
  // and 'blockEnd' properties move relatively positioned elements in
  // the block progression direction. They also must be each other's
  // negative
  bool blockStartIsAuto =
    eStyleUnit_Auto == position->mOffset.GetUnit(blockStart);
  bool blockEndIsAuto =
    eStyleUnit_Auto == position->mOffset.GetUnit(blockEnd);

  // Check for percentage based values and a containing block block-size
  // that depends on the content block-size. Treat them like 'auto'
  if (NS_AUTOHEIGHT == aCBSize.BSize(aWM)) {
    if (position->OffsetHasPercent(blockStart)) {
      blockStartIsAuto = true;
    }
    if (position->OffsetHasPercent(blockEnd)) {
      blockEndIsAuto = true;
    }
  }

  // If neither is 'auto', 'block-end' is ignored
  if (!blockStartIsAuto && !blockEndIsAuto) {
    blockEndIsAuto = true;
  }

  if (blockStartIsAuto) {
    if (blockEndIsAuto) {
      // If both are 'auto' (their initial values), the computed values are 0
      offsets.BStart(aWM) = offsets.BEnd(aWM) = 0;
    } else {
      // 'blockEnd' isn't 'auto' so compute its value
      offsets.BEnd(aWM) = nsLayoutUtils::
        ComputeBSizeDependentValue(aCBSize.BSize(aWM),
                                   position->mOffset.Get(blockEnd));

      // Computed value for 'blockStart' is minus the value of 'blockEnd'
      offsets.BStart(aWM) = -offsets.BEnd(aWM);
    }

  } else {
    NS_ASSERTION(blockEndIsAuto, "unexpected specified constraint");

    // 'blockStart' isn't 'auto' so compute its value
    offsets.BStart(aWM) = nsLayoutUtils::
      ComputeBSizeDependentValue(aCBSize.BSize(aWM),
                                 position->mOffset.Get(blockStart));

    // Computed value for 'blockEnd' is minus the value of 'blockStart'
    offsets.BEnd(aWM) = -offsets.BStart(aWM);
  }

  // Convert the offsets to physical coordinates and store them on the frame
  aComputedOffsets = offsets.GetPhysicalMargin(aWM);
  FrameProperties props = aFrame->Properties();
  nsMargin* physicalOffsets = static_cast<nsMargin*>
    (props.Get(nsIFrame::ComputedOffsetProperty()));
  if (physicalOffsets) {
    *physicalOffsets = aComputedOffsets;
  } else {
    props.Set(nsIFrame::ComputedOffsetProperty(),
              new nsMargin(aComputedOffsets));
  }
}

/* static */ void
nsHTMLReflowState::ApplyRelativePositioning(nsIFrame* aFrame,
                                            const nsMargin& aComputedOffsets,
                                            nsPoint* aPosition)
{
  if (!aFrame->IsRelativelyPositioned()) {
    NS_ASSERTION(!aFrame->Properties().Get(nsIFrame::NormalPositionProperty()),
                 "We assume that changing the 'position' property causes "
                 "frame reconstruction.  If that ever changes, this code "
                 "should call "
                 "props.Delete(nsIFrame::NormalPositionProperty())");
    return;
  }

  // Store the normal position
  FrameProperties props = aFrame->Properties();
  nsPoint* normalPosition = static_cast<nsPoint*>
    (props.Get(nsIFrame::NormalPositionProperty()));
  if (normalPosition) {
    *normalPosition = *aPosition;
  } else {
    props.Set(nsIFrame::NormalPositionProperty(), new nsPoint(*aPosition));
  }

  const nsStyleDisplay* display = aFrame->StyleDisplay();
  if (NS_STYLE_POSITION_RELATIVE == display->mPosition) {
    *aPosition += nsPoint(aComputedOffsets.left, aComputedOffsets.top);
  } else if (NS_STYLE_POSITION_STICKY == display->mPosition &&
             !aFrame->GetNextContinuation() &&
             !aFrame->GetPrevContinuation() &&
             !(aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT)) {
    // Sticky positioning for elements with multiple frames needs to be
    // computed all at once. We can't safely do that here because we might be
    // partway through (re)positioning the frames, so leave it until the scroll
    // container reflows and calls StickyScrollContainer::UpdatePositions.
    // For single-frame sticky positioned elements, though, go ahead and apply
    // it now to avoid unnecessary overflow updates later.
    StickyScrollContainer* ssc =
      StickyScrollContainer::GetStickyScrollContainerForFrame(aFrame);
    if (ssc) {
      *aPosition = ssc->ComputePosition(aFrame);
    }
  }
}

nsIFrame*
nsHTMLReflowState::GetHypotheticalBoxContainer(nsIFrame*    aFrame,
                                               nscoord&     aCBIStartEdge,
                                               LogicalSize& aCBSize) const
{
  aFrame = aFrame->GetContainingBlock();
  NS_ASSERTION(aFrame != frame, "How did that happen?");

  /* Now aFrame is the containing block we want */

  /* Check whether the containing block is currently being reflowed.
     If so, use the info from the reflow state. */
  const nsHTMLReflowState* state;
  if (aFrame->GetStateBits() & NS_FRAME_IN_REFLOW) {
    for (state = mParentReflowState; state && state->frame != aFrame;
         state = state->mParentReflowState) {
      /* do nothing */
    }
  } else {
    state = nullptr;
  }

  if (state) {
    WritingMode wm = state->GetWritingMode();
    NS_ASSERTION(wm == aFrame->GetWritingMode(), "unexpected writing mode");
    aCBIStartEdge = state->ComputedLogicalBorderPadding().IStart(wm);
    aCBSize = state->ComputedSize(wm);
  } else {
    /* Didn't find a reflow state for aFrame.  Just compute the information we
       want, on the assumption that aFrame already knows its size.  This really
       ought to be true by now. */
    NS_ASSERTION(!(aFrame->GetStateBits() & NS_FRAME_IN_REFLOW),
                 "aFrame shouldn't be in reflow; we'll lie if it is");
    WritingMode wm = aFrame->GetWritingMode();
    LogicalMargin borderPadding = aFrame->GetLogicalUsedBorderAndPadding(wm);
    aCBIStartEdge = borderPadding.IStart(wm);
    aCBSize = aFrame->GetLogicalSize(wm) - borderPadding.Size(wm);
  }

  return aFrame;
}

struct nsHypotheticalPosition {
  // offset from inline-start edge of containing block (which is a padding edge)
  nscoord       mIStart;
  // offset from block-start edge of containing block (which is a padding edge)
  nscoord       mBStart;
  WritingMode   mWritingMode;
};

static bool
GetIntrinsicSizeFor(nsIFrame* aFrame, nsSize& aIntrinsicSize, nsIAtom* aFrameType)
{
  // See if it is an image frame
  bool success = false;

  // Currently the only type of replaced frame that we can get the intrinsic
  // size for is an image frame
  // XXX We should add back the GetReflowMetrics() function and one of the
  // things should be the intrinsic size...
  if (aFrameType == nsGkAtoms::imageFrame) {
    nsImageFrame* imageFrame = (nsImageFrame*)aFrame;

    if (NS_SUCCEEDED(imageFrame->GetIntrinsicImageSize(aIntrinsicSize))) {
      success = (aIntrinsicSize != nsSize(0, 0));
    }
  }
  return success;
}

/**
 * aInsideBoxSizing returns the part of the padding, border, and margin
 * in the aAxis dimension that goes inside the edge given by box-sizing;
 * aOutsideBoxSizing returns the rest.
 */
void
nsHTMLReflowState::CalculateBorderPaddingMargin(
                       LogicalAxis aAxis,
                       nscoord aContainingBlockSize,
                       nscoord* aInsideBoxSizing,
                       nscoord* aOutsideBoxSizing) const
{
  WritingMode wm = GetWritingMode();
  mozilla::css::Side startSide =
    wm.PhysicalSide(MakeLogicalSide(aAxis, eLogicalEdgeStart));
  mozilla::css::Side endSide =
    wm.PhysicalSide(MakeLogicalSide(aAxis, eLogicalEdgeEnd));

  nsMargin styleBorder = mStyleBorder->GetComputedBorder();
  nscoord borderStartEnd =
    styleBorder.Side(startSide) + styleBorder.Side(endSide);

  nscoord paddingStartEnd, marginStartEnd;

  // See if the style system can provide us the padding directly
  nsMargin stylePadding;
  if (mStylePadding->GetPadding(stylePadding)) {
    paddingStartEnd =
      stylePadding.Side(startSide) + stylePadding.Side(endSide);
  } else {
    // We have to compute the start and end values
    nscoord start, end;
    start = nsLayoutUtils::
      ComputeCBDependentValue(aContainingBlockSize,
                              mStylePadding->mPadding.Get(startSide));
    end = nsLayoutUtils::
      ComputeCBDependentValue(aContainingBlockSize,
                              mStylePadding->mPadding.Get(endSide));
    paddingStartEnd = start + end;
  }

  // See if the style system can provide us the margin directly
  nsMargin styleMargin;
  if (mStyleMargin->GetMargin(styleMargin)) {
    marginStartEnd =
      styleMargin.Side(startSide) + styleMargin.Side(endSide);
  } else {
    nscoord start, end;
    // We have to compute the start and end values
    if (eStyleUnit_Auto == mStyleMargin->mMargin.GetUnit(startSide)) {
      // XXX FIXME (or does CalculateBlockSideMargins do this?)
      start = 0;  // just ignore
    } else {
      start = nsLayoutUtils::
        ComputeCBDependentValue(aContainingBlockSize,
                                mStyleMargin->mMargin.Get(startSide));
    }
    if (eStyleUnit_Auto == mStyleMargin->mMargin.GetUnit(endSide)) {
      // XXX FIXME (or does CalculateBlockSideMargins do this?)
      end = 0;  // just ignore
    } else {
      end = nsLayoutUtils::
        ComputeCBDependentValue(aContainingBlockSize,
                                mStyleMargin->mMargin.Get(endSide));
    }
    marginStartEnd = start + end;
  }

  nscoord outside = paddingStartEnd + borderStartEnd + marginStartEnd;
  nscoord inside = 0;
  switch (mStylePosition->mBoxSizing) {
    case StyleBoxSizing::Border:
      inside += borderStartEnd;
      MOZ_FALLTHROUGH;
    case StyleBoxSizing::Padding:
      inside += paddingStartEnd;
      MOZ_FALLTHROUGH;
    case StyleBoxSizing::Content:
      // nothing
      break;
  }
  outside -= inside;
  *aInsideBoxSizing = inside;
  *aOutsideBoxSizing = outside;
  return;
}

/**
 * Returns true iff a pre-order traversal of the normal child
 * frames rooted at aFrame finds no non-empty frame before aDescendant.
 */
static bool AreAllEarlierInFlowFramesEmpty(nsIFrame* aFrame,
  nsIFrame* aDescendant, bool* aFound) {
  if (aFrame == aDescendant) {
    *aFound = true;
    return true;
  }
  if (!aFrame->IsSelfEmpty()) {
    *aFound = false;
    return false;
  }
  for (nsIFrame* f : aFrame->PrincipalChildList()) {
    bool allEmpty = AreAllEarlierInFlowFramesEmpty(f, aDescendant, aFound);
    if (*aFound || !allEmpty) {
      return allEmpty;
    }
  }
  *aFound = false;
  return true;
}

// Calculate the position of the hypothetical box that the element would have
// if it were in the flow.
// The values returned are relative to the padding edge of the absolute
// containing block. The writing-mode of the hypothetical box position will
// have the same block direction as the absolute containing block, but may
// differ in inline-bidi direction.
// In the code below, |cbrs->frame| is the absolute containing block, while
// |containingBlock| is the nearest block container of the placeholder frame,
// which may be different from the absolute containing block.
void
nsHTMLReflowState::CalculateHypotheticalPosition
                     (nsPresContext*           aPresContext,
                      nsIFrame*                aPlaceholderFrame,
                      const nsHTMLReflowState* cbrs,
                      nsHypotheticalPosition&  aHypotheticalPos,
                      nsIAtom*                 aFrameType) const
{
  NS_ASSERTION(mStyleDisplay->mOriginalDisplay != NS_STYLE_DISPLAY_NONE,
               "mOriginalDisplay has not been properly initialized");

  // Find the nearest containing block frame to the placeholder frame,
  // and its inline-start edge and width.
  nscoord blockIStartContentEdge;
  // Dummy writing mode for blockContentSize, will be changed as needed by
  // GetHypotheticalBoxContainer.
  WritingMode cbwm = cbrs->GetWritingMode();
  LogicalSize blockContentSize(cbwm);
  nsIFrame* containingBlock =
    GetHypotheticalBoxContainer(aPlaceholderFrame, blockIStartContentEdge,
                                blockContentSize);
  // Now blockContentSize is in containingBlock's writing mode.

  // If it's a replaced element and it has a 'auto' value for
  //'inline size', see if we can get the intrinsic size. This will allow
  // us to exactly determine both the inline edges
  WritingMode wm = containingBlock->GetWritingMode();

  nsStyleCoord styleISize = mStylePosition->ISize(wm);
  bool isAutoISize = styleISize.GetUnit() == eStyleUnit_Auto;
  nsSize      intrinsicSize;
  bool        knowIntrinsicSize = false;
  if (NS_FRAME_IS_REPLACED(mFrameType) && isAutoISize) {
    // See if we can get the intrinsic size of the element
    knowIntrinsicSize = GetIntrinsicSizeFor(frame, intrinsicSize, aFrameType);
  }

  // See if we can calculate what the box inline size would have been if
  // the element had been in the flow
  nscoord boxISize;
  bool    knowBoxISize = false;
  if ((NS_STYLE_DISPLAY_INLINE == mStyleDisplay->mOriginalDisplay) &&
      !NS_FRAME_IS_REPLACED(mFrameType)) {
    // For non-replaced inline-level elements the 'inline size' property
    // doesn't apply, so we don't know what the inline size would have
    // been without reflowing it

  } else {
    // It's either a replaced inline-level element or a block-level element

    // Determine the total amount of inline direction
    // border/padding/margin that the element would have had if it had
    // been in the flow. Note that we ignore any 'auto' and 'inherit'
    // values
    nscoord insideBoxSizing, outsideBoxSizing;
    CalculateBorderPaddingMargin(eLogicalAxisInline,
                                 blockContentSize.ISize(wm),
                                 &insideBoxSizing, &outsideBoxSizing);

    if (NS_FRAME_IS_REPLACED(mFrameType) && isAutoISize) {
      // It's a replaced element with an 'auto' inline size so the box
      // inline size is its intrinsic size plus any border/padding/margin
      if (knowIntrinsicSize) {
        boxISize = LogicalSize(wm, intrinsicSize).ISize(wm) +
                   outsideBoxSizing + insideBoxSizing;
        knowBoxISize = true;
      }

    } else if (isAutoISize) {
      // The box inline size is the containing block inline size
      boxISize = blockContentSize.ISize(wm);
      knowBoxISize = true;

    } else {
      // We need to compute it. It's important we do this, because if it's
      // percentage based this computed value may be different from the computed
      // value calculated using the absolute containing block width
      boxISize = ComputeISizeValue(blockContentSize.ISize(wm),
                                   insideBoxSizing, outsideBoxSizing,
                                   styleISize) +
                 insideBoxSizing + outsideBoxSizing;
      knowBoxISize = true;
    }
  }

  // Get the placeholder x-offset and y-offset in the coordinate
  // space of its containing block
  // XXXbz the placeholder is not fully reflowed yet if our containing block is
  // relatively positioned...
  nsSize containerSize = containingBlock->GetStateBits() & NS_FRAME_IN_REFLOW
    ? cbrs->ComputedSizeAsContainerIfConstrained()
    : containingBlock->GetSize();
  LogicalPoint
    placeholderOffset(wm, aPlaceholderFrame->GetOffsetTo(containingBlock),
                      containerSize);

  // First, determine the hypothetical box's mBStart.  We want to check the
  // content insertion frame of containingBlock for block-ness, but make
  // sure to compute all coordinates in the coordinate system of
  // containingBlock.
  nsBlockFrame* blockFrame =
    nsLayoutUtils::GetAsBlock(containingBlock->GetContentInsertionFrame());
  if (blockFrame) {
    // Use a null containerSize to convert a LogicalPoint functioning as a
    // vector into a physical nsPoint vector.
    const nsSize nullContainerSize;
    LogicalPoint blockOffset(wm, blockFrame->GetOffsetTo(containingBlock),
                             nullContainerSize);
    bool isValid;
    nsBlockInFlowLineIterator iter(blockFrame, aPlaceholderFrame, &isValid);
    if (!isValid) {
      // Give up.  We're probably dealing with somebody using
      // position:absolute inside native-anonymous content anyway.
      aHypotheticalPos.mBStart = placeholderOffset.B(wm);
    } else {
      NS_ASSERTION(iter.GetContainer() == blockFrame,
                   "Found placeholder in wrong block!");
      nsBlockFrame::line_iterator lineBox = iter.GetLine();

      // How we determine the hypothetical box depends on whether the element
      // would have been inline-level or block-level
      LogicalRect lineBounds =
        lineBox->GetBounds().ConvertTo(wm, lineBox->mWritingMode,
                                       lineBox->mContainerSize);
      if (mStyleDisplay->IsOriginalDisplayInlineOutsideStyle()) {
        // Use the block-start of the inline box which the placeholder lives in
        // as the hypothetical box's block-start.
        aHypotheticalPos.mBStart = lineBounds.BStart(wm) + blockOffset.B(wm);
      } else {
        // The element would have been block-level which means it would
        // be below the line containing the placeholder frame, unless
        // all the frames before it are empty.  In that case, it would
        // have been just before this line.
        // XXXbz the line box is not fully reflowed yet if our
        // containing block is relatively positioned...
        if (lineBox != iter.End()) {
          nsIFrame * firstFrame = lineBox->mFirstChild;
          bool found = false;
          bool allEmpty = true;
          while (firstFrame) { // See bug 223064
            allEmpty = AreAllEarlierInFlowFramesEmpty(firstFrame,
              aPlaceholderFrame, &found);
            if (found || !allEmpty)
              break;
            firstFrame = firstFrame->GetNextSibling();
          }
          NS_ASSERTION(firstFrame, "Couldn't find placeholder!");

          if (allEmpty) {
            // The top of the hypothetical box is the top of the line
            // containing the placeholder, since there is nothing in the
            // line before our placeholder except empty frames.
            aHypotheticalPos.mBStart =
              lineBounds.BStart(wm) + blockOffset.B(wm);
          } else {
            // The top of the hypothetical box is just below the line
            // containing the placeholder.
            aHypotheticalPos.mBStart =
              lineBounds.BEnd(wm) + blockOffset.B(wm);
          }
        } else {
          // Just use the placeholder's block-offset wrt the containing block
          aHypotheticalPos.mBStart = placeholderOffset.B(wm);
        }
      }
    }
  } else {
    // The containing block is not a block, so it's probably something
    // like a XUL box, etc.
    // Just use the placeholder's block-offset
    aHypotheticalPos.mBStart = placeholderOffset.B(wm);
  }

  // Second, determine the hypothetical box's mIStart.
  // How we determine the hypothetical box depends on whether the element
  // would have been inline-level or block-level
  if (mStyleDisplay->IsOriginalDisplayInlineOutsideStyle()) {
    // The placeholder represents the left edge of the hypothetical box
    aHypotheticalPos.mIStart = placeholderOffset.I(wm);
  } else {
    aHypotheticalPos.mIStart = blockIStartContentEdge;
  }

  // The current coordinate space is that of the nearest block to the placeholder.
  // Convert to the coordinate space of the absolute containing block
  // One weird thing here is that for fixed-positioned elements we want to do
  // the conversion incorrectly; specifically we want to ignore any scrolling
  // that may have happened;
  nsPoint cbOffset;
  if (mStyleDisplay->mPosition == NS_STYLE_POSITION_FIXED &&
      // Exclude cases inside -moz-transform where fixed is like absolute.
      nsLayoutUtils::IsReallyFixedPos(frame)) {
    // In this case, cbrs->frame will likely be an ancestor of
    // containingBlock, so can just walk our way up the frame tree.
    // Make sure to not add positions of frames whose parent is a
    // scrollFrame, since we're doing fixed positioning, which assumes
    // everything is scrolled to (0,0).
    cbOffset.MoveTo(0, 0);
    do {
      cbOffset += containingBlock->GetPositionIgnoringScrolling();
      nsContainerFrame* parent = containingBlock->GetParent();
      if (!parent) {
        // Oops, our absolute containing block isn't an ancestor of the
        // placeholder's containing block. This can happen if the placeholder
        // is pushed to a different page in a printing context.  'cbOffset' is
        // currently relative to the root frame (containingBlock) - so just
        // subtract the offset to the absolute containing block to make it
        // relative to that.
        cbOffset -= containingBlock->GetOffsetTo(cbrs->frame);
        break;
      }
      containingBlock = parent;
    } while (containingBlock != cbrs->frame);
  } else {
    // XXXldb We need to either ignore scrolling for the absolute
    // positioning case too (and take the incompatibility) or figure out
    // how to make these positioned elements actually *move* when we
    // scroll, and thus avoid the resulting incremental reflow bugs.
    cbOffset = containingBlock->GetOffsetTo(cbrs->frame);
  }
  nsSize cbrsSize = cbrs->ComputedSizeAsContainerIfConstrained();
  LogicalPoint logCBOffs(wm, cbOffset, cbrsSize - containerSize);
  aHypotheticalPos.mIStart += logCBOffs.I(wm);
  aHypotheticalPos.mBStart += logCBOffs.B(wm);

  // The specified offsets are relative to the absolute containing block's
  // padding edge and our current values are relative to the border edge, so
  // translate.
  LogicalMargin border =
    cbrs->ComputedLogicalBorderPadding() - cbrs->ComputedLogicalPadding();
  border = border.ConvertTo(wm, cbrs->GetWritingMode());
  aHypotheticalPos.mIStart -= border.IStart(wm);
  aHypotheticalPos.mBStart -= border.BStart(wm);

  // At this point, we have computed aHypotheticalPos using the writing mode
  // of the placeholder's containing block.

  if (cbwm.GetBlockDir() != wm.GetBlockDir()) {
    // If the block direction we used in calculating aHypotheticalPos does not
    // match the absolute containing block's, we need to convert here so that
    // aHypotheticalPos is usable in relation to the absolute containing block.
    // This requires computing or measuring the abspos frame's block-size,
    // which is not otherwise required/used here (as aHypotheticalPos
    // records only the block-start coordinate).

    // This is similar to the inline-size calculation for a replaced
    // inline-level element or a block-level element (above), except that
    // 'auto' sizing is handled differently in the block direction for non-
    // replaced elements and replaced elements lacking an intrinsic size.

    // Determine the total amount of block direction
    // border/padding/margin that the element would have had if it had
    // been in the flow. Note that we ignore any 'auto' and 'inherit'
    // values.
    nscoord insideBoxSizing, outsideBoxSizing;
    CalculateBorderPaddingMargin(eLogicalAxisBlock,
                                 blockContentSize.BSize(wm),
                                 &insideBoxSizing, &outsideBoxSizing);

    nscoord boxBSize;
    nsStyleCoord styleBSize = mStylePosition->BSize(wm);
    bool isAutoBSize = styleBSize.GetUnit() == eStyleUnit_Auto;
    if (isAutoBSize) {
      if (NS_FRAME_IS_REPLACED(mFrameType) && knowIntrinsicSize) {
        // It's a replaced element with an 'auto' block size so the box
        // block size is its intrinsic size plus any border/padding/margin
        boxBSize = LogicalSize(wm, intrinsicSize).BSize(wm) +
                   outsideBoxSizing + insideBoxSizing;
      } else {
        // XXX Bug 1191801
        // Figure out how to get the correct boxBSize here (need to reflow the
        // positioned frame?)
        boxBSize = 0;
      }
    } else {
      // We need to compute it. It's important we do this, because if it's
      // percentage-based this computed value may be different from the
      // computed value calculated using the absolute containing block height.
      boxBSize = nsLayoutUtils::ComputeBSizeValue(blockContentSize.BSize(wm),
                                                  insideBoxSizing, styleBSize) +
                 insideBoxSizing + outsideBoxSizing;
    }

    LogicalSize boxSize(wm, knowBoxISize ? boxISize : 0, boxBSize);

    LogicalPoint origin(wm, aHypotheticalPos.mIStart,
                        aHypotheticalPos.mBStart);
    origin = origin.ConvertTo(cbwm, wm, cbrsSize -
                              boxSize.GetPhysicalSize(wm));

    aHypotheticalPos.mIStart = origin.I(cbwm);
    aHypotheticalPos.mBStart = origin.B(cbwm);
    aHypotheticalPos.mWritingMode = cbwm;
  } else {
    aHypotheticalPos.mWritingMode = wm;
  }
}

void
nsHTMLReflowState::InitAbsoluteConstraints(nsPresContext* aPresContext,
                                           const nsHTMLReflowState* cbrs,
                                           const LogicalSize& aCBSize,
                                           nsIAtom* aFrameType)
{
  WritingMode wm = GetWritingMode();
  WritingMode cbwm = cbrs->GetWritingMode();
  NS_PRECONDITION(aCBSize.BSize(cbwm) != NS_AUTOHEIGHT,
                  "containing block bsize must be constrained");

  NS_ASSERTION(aFrameType != nsGkAtoms::tableFrame,
               "InitAbsoluteConstraints should not be called on table frames");
  NS_ASSERTION(frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW,
               "Why are we here?");

  const auto& styleOffset = mStylePosition->mOffset;
  bool iStartIsAuto = styleOffset.GetIStartUnit(cbwm) == eStyleUnit_Auto;
  bool iEndIsAuto   = styleOffset.GetIEndUnit(cbwm) == eStyleUnit_Auto;
  bool bStartIsAuto = styleOffset.GetBStartUnit(cbwm) == eStyleUnit_Auto;
  bool bEndIsAuto   = styleOffset.GetBEndUnit(cbwm) == eStyleUnit_Auto;

  // If both 'left' and 'right' are 'auto' or both 'top' and 'bottom' are
  // 'auto', then compute the hypothetical box position where the element would
  // have been if it had been in the flow
  nsHypotheticalPosition hypotheticalPos;
  if ((iStartIsAuto && iEndIsAuto) || (bStartIsAuto && bEndIsAuto)) {
    if (mFlags.mStaticPosIsCBOrigin) {
      hypotheticalPos.mWritingMode = cbwm;
      hypotheticalPos.mIStart = nscoord(0);
      hypotheticalPos.mBStart = nscoord(0);
    } else {
      nsIFrame* placeholderFrame =
        aPresContext->PresShell()->GetPlaceholderFrameFor(frame);
      NS_ASSERTION(placeholderFrame, "no placeholder frame");
      CalculateHypotheticalPosition(aPresContext, placeholderFrame, cbrs,
                                    hypotheticalPos, aFrameType);
    }
  }

  // Initialize the 'left' and 'right' computed offsets
  // XXX Handle new 'static-position' value...

  // Size of the containing block in its writing mode
  LogicalSize cbSize = aCBSize;
  LogicalMargin offsets = ComputedLogicalOffsets().ConvertTo(cbwm, wm);

  if (iStartIsAuto) {
    offsets.IStart(cbwm) = 0;
  } else {
    offsets.IStart(cbwm) = nsLayoutUtils::
      ComputeCBDependentValue(cbSize.ISize(cbwm), styleOffset.GetIStart(cbwm));
  }
  if (iEndIsAuto) {
    offsets.IEnd(cbwm) = 0;
  } else {
    offsets.IEnd(cbwm) = nsLayoutUtils::
      ComputeCBDependentValue(cbSize.ISize(cbwm), styleOffset.GetIEnd(cbwm));
  }

  if (iStartIsAuto && iEndIsAuto) {
    if (cbwm.IsBidiLTR() != hypotheticalPos.mWritingMode.IsBidiLTR()) {
      offsets.IEnd(cbwm) = hypotheticalPos.mIStart;
      iEndIsAuto = false;
    } else {
      offsets.IStart(cbwm) = hypotheticalPos.mIStart;
      iStartIsAuto = false;
    }
  }

  if (bStartIsAuto) {
    offsets.BStart(cbwm) = 0;
  } else {
    offsets.BStart(cbwm) = nsLayoutUtils::
      ComputeBSizeDependentValue(cbSize.BSize(cbwm),
                                 styleOffset.GetBStart(cbwm));
  }
  if (bEndIsAuto) {
    offsets.BEnd(cbwm) = 0;
  } else {
    offsets.BEnd(cbwm) = nsLayoutUtils::
      ComputeBSizeDependentValue(cbSize.BSize(cbwm),
                                 styleOffset.GetBEnd(cbwm));
  }

  if (bStartIsAuto && bEndIsAuto) {
    // Treat 'top' like 'static-position'
    offsets.BStart(cbwm) = hypotheticalPos.mBStart;
    bStartIsAuto = false;
  }

  SetComputedLogicalOffsets(offsets.ConvertTo(wm, cbwm));

  typedef nsIFrame::ComputeSizeFlags ComputeSizeFlags;
  ComputeSizeFlags computeSizeFlags = ComputeSizeFlags::eDefault;
  if (mFlags.mShrinkWrap) {
    computeSizeFlags =
      ComputeSizeFlags(computeSizeFlags | ComputeSizeFlags::eShrinkWrap);
  }
  if (mFlags.mUseAutoBSize) {
    computeSizeFlags =
      ComputeSizeFlags(computeSizeFlags | ComputeSizeFlags::eUseAutoBSize);
  }
  if (wm.IsOrthogonalTo(cbwm)) {
    if (bStartIsAuto || bEndIsAuto) {
      computeSizeFlags =
        ComputeSizeFlags(computeSizeFlags | ComputeSizeFlags::eShrinkWrap);
    }
  } else {
    if (iStartIsAuto || iEndIsAuto) {
      computeSizeFlags =
        ComputeSizeFlags(computeSizeFlags | ComputeSizeFlags::eShrinkWrap);
    }
  }

  LogicalSize computedSize(wm);
  {
    AutoMaybeDisableFontInflation an(frame);

    computedSize =
      frame->ComputeSize(rendContext, wm, cbSize.ConvertTo(wm, cbwm),
                         cbSize.ConvertTo(wm, cbwm).ISize(wm), // XXX or AvailableISize()?
                         ComputedLogicalMargin().Size(wm) +
                           ComputedLogicalOffsets().Size(wm),
                         ComputedLogicalBorderPadding().Size(wm) -
                           ComputedLogicalPadding().Size(wm),
                         ComputedLogicalPadding().Size(wm),
                         computeSizeFlags);
    ComputedISize() = computedSize.ISize(wm);
    ComputedBSize() = computedSize.BSize(wm);
    NS_ASSERTION(ComputedISize() >= 0, "Bogus inline-size");
    NS_ASSERTION(ComputedBSize() == NS_UNCONSTRAINEDSIZE ||
                 ComputedBSize() >= 0, "Bogus block-size");
  }
  computedSize = computedSize.ConvertTo(cbwm, wm);

  // XXX Now that we have ComputeSize, can we condense many of the
  // branches off of widthIsAuto?

  LogicalMargin margin = ComputedLogicalMargin().ConvertTo(cbwm, wm);
  const LogicalMargin borderPadding =
    ComputedLogicalBorderPadding().ConvertTo(cbwm, wm);

  bool iSizeIsAuto = eStyleUnit_Auto == mStylePosition->ISize(cbwm).GetUnit();
  if (iStartIsAuto) {
    // We know 'right' is not 'auto' anymore thanks to the hypothetical
    // box code above.
    // Solve for 'left'.
    if (iSizeIsAuto) {
      // XXXldb This, and the corresponding code in
      // nsAbsoluteContainingBlock.cpp, could probably go away now that
      // we always compute widths.
      offsets.IStart(cbwm) = NS_AUTOOFFSET;
    } else {
      offsets.IStart(cbwm) =
        cbSize.ISize(cbwm) - offsets.IEnd(cbwm) -
        computedSize.ISize(cbwm) - margin.IStartEnd(cbwm) -
        borderPadding.IStartEnd(cbwm);
    }
  } else if (iEndIsAuto) {
    // We know 'left' is not 'auto' anymore thanks to the hypothetical
    // box code above.
    // Solve for 'right'.
    if (iSizeIsAuto) {
      // XXXldb This, and the corresponding code in
      // nsAbsoluteContainingBlock.cpp, could probably go away now that
      // we always compute widths.
      offsets.IEnd(cbwm) = NS_AUTOOFFSET;
    } else {
      offsets.IEnd(cbwm) =
        cbSize.ISize(cbwm) - offsets.IStart(cbwm) -
        computedSize.ISize(cbwm) - margin.IStartEnd(cbwm) -
        borderPadding.IStartEnd(cbwm);
    }
  } else {
    // Neither 'inline-start' nor 'inline-end' is 'auto'.

    if (wm.IsOrthogonalTo(cbwm)) {
      // For orthogonal blocks, we need to handle the case where the block had
      // unconstrained block-size, which mapped to unconstrained inline-size
      // in the containing block's writing mode.
      nscoord autoISize = cbSize.ISize(cbwm) - margin.IStartEnd(cbwm) -
        borderPadding.IStartEnd(cbwm) - offsets.IStartEnd(cbwm);
      if (autoISize < 0) {
        autoISize = 0;
      }

      if (computedSize.ISize(cbwm) == NS_UNCONSTRAINEDSIZE) {
        // For non-replaced elements with block-size auto, the block-size
        // fills the remaining space.
        computedSize.ISize(cbwm) = autoISize;

        // XXX Do these need box-sizing adjustments?
        LogicalSize maxSize = ComputedMaxSize(cbwm);
        LogicalSize minSize = ComputedMinSize(cbwm);
        if (computedSize.ISize(cbwm) > maxSize.ISize(cbwm)) {
          computedSize.ISize(cbwm) = maxSize.ISize(cbwm);
        }
        if (computedSize.ISize(cbwm) < minSize.ISize(cbwm)) {
          computedSize.ISize(cbwm) = minSize.ISize(cbwm);
        }
      }
    }

    // However, the inline-size might
    // still not fill all the available space (even though we didn't
    // shrink-wrap) in case:
    //  * inline-size was specified
    //  * we're dealing with a replaced element
    //  * width was constrained by min- or max-inline-size.

    nscoord availMarginSpace =
      aCBSize.ISize(cbwm) - offsets.IStartEnd(cbwm) - margin.IStartEnd(cbwm) -
      borderPadding.IStartEnd(cbwm) - computedSize.ISize(cbwm);
    bool marginIStartIsAuto =
      eStyleUnit_Auto == mStyleMargin->mMargin.GetIStartUnit(cbwm);
    bool marginIEndIsAuto =
      eStyleUnit_Auto == mStyleMargin->mMargin.GetIEndUnit(cbwm);

    if (marginIStartIsAuto) {
      if (marginIEndIsAuto) {
        if (availMarginSpace < 0) {
          // Note that this case is different from the neither-'auto'
          // case below, where the spec says to ignore 'left'/'right'.
          // Ignore the specified value for 'margin-right'.
          margin.IEnd(cbwm) = availMarginSpace;
        } else {
          // Both 'margin-left' and 'margin-right' are 'auto', so they get
          // equal values
          margin.IStart(cbwm) = availMarginSpace / 2;
          margin.IEnd(cbwm) = availMarginSpace - margin.IStart(cbwm);
        }
      } else {
        // Just 'margin-left' is 'auto'
        margin.IStart(cbwm) = availMarginSpace;
      }
    } else {
      if (marginIEndIsAuto) {
        // Just 'margin-right' is 'auto'
        margin.IEnd(cbwm) = availMarginSpace;
      } else {
        // We're over-constrained so use the direction of the containing
        // block to dictate which value to ignore.  (And note that the
        // spec says to ignore 'left' or 'right' rather than
        // 'margin-left' or 'margin-right'.)
        // Note that this case is different from the both-'auto' case
        // above, where the spec says to ignore
        // 'margin-left'/'margin-right'.
        // Ignore the specified value for 'right'.
        offsets.IEnd(cbwm) += availMarginSpace;
      }
    }
  }

  bool bSizeIsAuto = eStyleUnit_Auto == mStylePosition->BSize(cbwm).GetUnit();
  if (bStartIsAuto) {
    // solve for block-start
    if (bSizeIsAuto) {
      offsets.BStart(cbwm) = NS_AUTOOFFSET;
    } else {
      offsets.BStart(cbwm) = cbSize.BSize(cbwm) - margin.BStartEnd(cbwm) -
        borderPadding.BStartEnd(cbwm) - computedSize.BSize(cbwm) -
        offsets.BEnd(cbwm);
    }
  } else if (bEndIsAuto) {
    // solve for block-end
    if (bSizeIsAuto) {
      offsets.BEnd(cbwm) = NS_AUTOOFFSET;
    } else {
      offsets.BEnd(cbwm) = cbSize.BSize(cbwm) - margin.BStartEnd(cbwm) -
        borderPadding.BStartEnd(cbwm) - computedSize.BSize(cbwm) -
        offsets.BStart(cbwm);
    }
  } else {
    // Neither block-start nor -end is 'auto'.
    nscoord autoBSize = cbSize.BSize(cbwm) - margin.BStartEnd(cbwm) -
      borderPadding.BStartEnd(cbwm) - offsets.BStartEnd(cbwm);
    if (autoBSize < 0) {
      autoBSize = 0;
    }

    if (computedSize.BSize(cbwm) == NS_UNCONSTRAINEDSIZE) {
      // For non-replaced elements with block-size auto, the block-size
      // fills the remaining space.
      computedSize.BSize(cbwm) = autoBSize;

      // XXX Do these need box-sizing adjustments?
      LogicalSize maxSize = ComputedMaxSize(cbwm);
      LogicalSize minSize = ComputedMinSize(cbwm);
      if (computedSize.BSize(cbwm) > maxSize.BSize(cbwm)) {
        computedSize.BSize(cbwm) = maxSize.BSize(cbwm);
      }
      if (computedSize.BSize(cbwm) < minSize.BSize(cbwm)) {
        computedSize.BSize(cbwm) = minSize.BSize(cbwm);
      }
    }

    // The block-size might still not fill all the available space in case:
    //  * bsize was specified
    //  * we're dealing with a replaced element
    //  * bsize was constrained by min- or max-bsize.
    nscoord availMarginSpace = autoBSize - computedSize.BSize(cbwm);
    bool marginBStartIsAuto =
      eStyleUnit_Auto == mStyleMargin->mMargin.GetBStartUnit(cbwm);
    bool marginBEndIsAuto =
      eStyleUnit_Auto == mStyleMargin->mMargin.GetBEndUnit(cbwm);

    if (marginBStartIsAuto) {
      if (marginBEndIsAuto) {
        // Both 'margin-top' and 'margin-bottom' are 'auto', so they get
        // equal values
        margin.BStart(cbwm) = availMarginSpace / 2;
        margin.BEnd(cbwm) = availMarginSpace - margin.BStart(cbwm);
      } else {
        // Just margin-block-start is 'auto'
        margin.BStart(cbwm) = availMarginSpace;
      }
    } else {
      if (marginBEndIsAuto) {
        // Just margin-block-end is 'auto'
        margin.BEnd(cbwm) = availMarginSpace;
      } else {
        // We're over-constrained so ignore the specified value for
        // block-end.  (And note that the spec says to ignore 'bottom'
        // rather than 'margin-bottom'.)
        offsets.BEnd(cbwm) += availMarginSpace;
      }
    }
  }
  ComputedBSize() = computedSize.ConvertTo(wm, cbwm).BSize(wm);
  ComputedISize() = computedSize.ConvertTo(wm, cbwm).ISize(wm);

  SetComputedLogicalOffsets(offsets.ConvertTo(wm, cbwm));
  SetComputedLogicalMargin(margin.ConvertTo(wm, cbwm));
}

// This will not be converted to abstract coordinates because it's only
// used in CalcQuirkContainingBlockHeight
nscoord
GetBlockMarginBorderPadding(const nsHTMLReflowState* aReflowState)
{
  nscoord result = 0;
  if (!aReflowState) return result;

  // zero auto margins
  nsMargin margin = aReflowState->ComputedPhysicalMargin();
  if (NS_AUTOMARGIN == margin.top) 
    margin.top = 0;
  if (NS_AUTOMARGIN == margin.bottom) 
    margin.bottom = 0;

  result += margin.top + margin.bottom;
  result += aReflowState->ComputedPhysicalBorderPadding().top + 
            aReflowState->ComputedPhysicalBorderPadding().bottom;

  return result;
}

/* Get the height based on the viewport of the containing block specified 
 * in aReflowState when the containing block has mComputedHeight == NS_AUTOHEIGHT
 * This will walk up the chain of containing blocks looking for a computed height
 * until it finds the canvas frame, or it encounters a frame that is not a block,
 * area, or scroll frame. This handles compatibility with IE (see bug 85016 and bug 219693)
 *
 * When we encounter scrolledContent block frames, we skip over them,
 * since they are guaranteed to not be useful for computing the containing block.
 *
 * See also IsQuirkContainingBlockHeight.
 */
static nscoord
CalcQuirkContainingBlockHeight(const nsHTMLReflowState* aCBReflowState)
{
  const nsHTMLReflowState* firstAncestorRS = nullptr; // a candidate for html frame
  const nsHTMLReflowState* secondAncestorRS = nullptr; // a candidate for body frame
  
  // initialize the default to NS_AUTOHEIGHT as this is the containings block
  // computed height when this function is called. It is possible that we 
  // don't alter this height especially if we are restricted to one level
  nscoord result = NS_AUTOHEIGHT; 
                             
  const nsHTMLReflowState* rs = aCBReflowState;
  for (; rs; rs = rs->mParentReflowState) {
    nsIAtom* frameType = rs->frame->GetType();
    // if the ancestor is auto height then skip it and continue up if it 
    // is the first block frame and possibly the body/html
    if (nsGkAtoms::blockFrame == frameType ||
#ifdef MOZ_XUL
        nsGkAtoms::XULLabelFrame == frameType ||
#endif
        nsGkAtoms::scrollFrame == frameType) {

      secondAncestorRS = firstAncestorRS;
      firstAncestorRS = rs;

      // If the current frame we're looking at is positioned, we don't want to
      // go any further (see bug 221784).  The behavior we want here is: 1) If
      // not auto-height, use this as the percentage base.  2) If auto-height,
      // keep looking, unless the frame is positioned.
      if (NS_AUTOHEIGHT == rs->ComputedHeight()) {
        if (rs->frame->IsAbsolutelyPositioned()) {
          break;
        } else {
          continue;
        }
      }
    }
    else if (nsGkAtoms::canvasFrame == frameType) {
      // Always continue on to the height calculation
    }
    else if (nsGkAtoms::pageContentFrame == frameType) {
      nsIFrame* prevInFlow = rs->frame->GetPrevInFlow();
      // only use the page content frame for a height basis if it is the first in flow
      if (prevInFlow) 
        break;
    }
    else {
      break;
    }

    // if the ancestor is the page content frame then the percent base is 
    // the avail height, otherwise it is the computed height
    result = (nsGkAtoms::pageContentFrame == frameType)
             ? rs->AvailableHeight() : rs->ComputedHeight();
    // if unconstrained - don't sutract borders - would result in huge height
    if (NS_AUTOHEIGHT == result) return result;

    // if we got to the canvas or page content frame, then subtract out 
    // margin/border/padding for the BODY and HTML elements
    if ((nsGkAtoms::canvasFrame == frameType) || 
        (nsGkAtoms::pageContentFrame == frameType)) {

      result -= GetBlockMarginBorderPadding(firstAncestorRS);
      result -= GetBlockMarginBorderPadding(secondAncestorRS);

#ifdef DEBUG
      // make sure the first ancestor is the HTML and the second is the BODY
      if (firstAncestorRS) {
        nsIContent* frameContent = firstAncestorRS->frame->GetContent();
        if (frameContent) {
          NS_ASSERTION(frameContent->IsHTMLElement(nsGkAtoms::html),
                       "First ancestor is not HTML");
        }
      }
      if (secondAncestorRS) {
        nsIContent* frameContent = secondAncestorRS->frame->GetContent();
        if (frameContent) {
          NS_ASSERTION(frameContent->IsHTMLElement(nsGkAtoms::body),
                       "Second ancestor is not BODY");
        }
      }
#endif
      
    }
    // if we got to the html frame (a block child of the canvas) ...
    else if (nsGkAtoms::blockFrame == frameType &&
             rs->mParentReflowState &&
             nsGkAtoms::canvasFrame ==
               rs->mParentReflowState->frame->GetType()) {
      // ... then subtract out margin/border/padding for the BODY element
      result -= GetBlockMarginBorderPadding(secondAncestorRS);
    }
    break;
  }

  // Make sure not to return a negative height here!
  return std::max(result, 0);
}

// Called by InitConstraints() to compute the containing block rectangle for
// the element. Handles the special logic for absolutely positioned elements
LogicalSize
nsHTMLReflowState::ComputeContainingBlockRectangle(
                     nsPresContext*           aPresContext,
                     const nsHTMLReflowState* aContainingBlockRS) const
{
  // Unless the element is absolutely positioned, the containing block is
  // formed by the content edge of the nearest block-level ancestor
  LogicalSize cbSize = aContainingBlockRS->ComputedSize();

  WritingMode wm = aContainingBlockRS->GetWritingMode();

  // mFrameType for abs-pos tables is NS_CSS_FRAME_TYPE_BLOCK, so we need to
  // special case them here.
  if (NS_FRAME_GET_TYPE(mFrameType) == NS_CSS_FRAME_TYPE_ABSOLUTE ||
      (frame->GetType() == nsGkAtoms::tableFrame &&
       frame->IsAbsolutelyPositioned() &&
       (frame->GetParent()->GetStateBits() & NS_FRAME_OUT_OF_FLOW))) {
    // See if the ancestor is block-level or inline-level
    if (NS_FRAME_GET_TYPE(aContainingBlockRS->mFrameType) == NS_CSS_FRAME_TYPE_INLINE) {
      // Base our size on the actual size of the frame.  In cases when this is
      // completely bogus (eg initial reflow), this code shouldn't even be
      // called, since the code in nsInlineFrame::Reflow will pass in
      // the containing block dimensions to our constructor.
      // XXXbz we should be taking the in-flows into account too, but
      // that's very hard.

      LogicalMargin computedBorder =
        aContainingBlockRS->ComputedLogicalBorderPadding() -
        aContainingBlockRS->ComputedLogicalPadding();
      cbSize.ISize(wm) = aContainingBlockRS->frame->ISize(wm) -
                         computedBorder.IStartEnd(wm);
      NS_ASSERTION(cbSize.ISize(wm) >= 0,
                   "Negative containing block isize!");
      cbSize.BSize(wm) = aContainingBlockRS->frame->BSize(wm) -
                         computedBorder.BStartEnd(wm);
      NS_ASSERTION(cbSize.BSize(wm) >= 0,
                   "Negative containing block bsize!");
    } else {
      // If the ancestor is block-level, the containing block is formed by the
      // padding edge of the ancestor
      cbSize.ISize(wm) +=
        aContainingBlockRS->ComputedLogicalPadding().IStartEnd(wm);
      cbSize.BSize(wm) +=
        aContainingBlockRS->ComputedLogicalPadding().BStartEnd(wm);
    }
  } else {
    // an element in quirks mode gets a containing block based on looking for a
    // parent with a non-auto height if the element has a percent height
    // Note: We don't emulate this quirk for percents in calc() or in
    // vertical writing modes.
    if (!wm.IsVertical() &&
        NS_AUTOHEIGHT == cbSize.BSize(wm)) {
      if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
          mStylePosition->mHeight.GetUnit() == eStyleUnit_Percent) {
        cbSize.BSize(wm) = CalcQuirkContainingBlockHeight(aContainingBlockRS);
      }
    }
  }

  return cbSize.ConvertTo(GetWritingMode(), wm);
}

static eNormalLineHeightControl GetNormalLineHeightCalcControl(void)
{
  if (sNormalLineHeightControl == eUninitialized) {
    // browser.display.normal_lineheight_calc_control is not user
    // changeable, so no need to register callback for it.
    int32_t val =
      Preferences::GetInt("browser.display.normal_lineheight_calc_control",
                          eNoExternalLeading);
    sNormalLineHeightControl = static_cast<eNormalLineHeightControl>(val);
  }
  return sNormalLineHeightControl;
}

static inline bool
IsSideCaption(nsIFrame* aFrame, const nsStyleDisplay* aStyleDisplay,
              WritingMode aWM)
{
  if (aStyleDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE_CAPTION) {
    return false;
  }
  uint8_t captionSide = aFrame->StyleTableBorder()->mCaptionSide;
  return captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
         captionSide == NS_STYLE_CAPTION_SIDE_RIGHT;
}

// Flex/grid items resolve block-axis percentage margin & padding against the
// containing block block-size (also for abs/fixed-pos child frames).
// For everything else: the CSS21 spec requires that margin and padding
// percentage values are calculated with respect to the inline-size of the
// containing block, even for margin & padding in the block axis.
static LogicalSize
OffsetPercentBasis(const nsIFrame*    aFrame,
                   WritingMode        aWM,
                   const LogicalSize& aContainingBlockSize)
{
  LogicalSize offsetPercentBasis = aContainingBlockSize;
  if (MOZ_LIKELY(!aFrame->GetParent() ||
                 !aFrame->GetParent()->IsFlexOrGridContainer())) {
    offsetPercentBasis.BSize(aWM) = offsetPercentBasis.ISize(aWM);
  } else if (offsetPercentBasis.BSize(aWM) == NS_AUTOHEIGHT) {
    offsetPercentBasis.BSize(aWM) = 0;
  }

  return offsetPercentBasis;
}

// XXX refactor this code to have methods for each set of properties
// we are computing: width,height,line-height; margin; offsets

void
nsHTMLReflowState::InitConstraints(nsPresContext*     aPresContext,
                                   const LogicalSize& aContainingBlockSize,
                                   const nsMargin*    aBorder,
                                   const nsMargin*    aPadding,
                                   nsIAtom*           aFrameType)
{
  WritingMode wm = GetWritingMode();
  DISPLAY_INIT_CONSTRAINTS(frame, this,
                           aContainingBlockSize.ISize(wm),
                           aContainingBlockSize.BSize(wm),
                           aBorder, aPadding);

  // If this is a reflow root, then set the computed width and
  // height equal to the available space
  if (nullptr == mParentReflowState || mFlags.mDummyParentReflowState) {
    // XXXldb This doesn't mean what it used to!
    InitOffsets(wm, OffsetPercentBasis(frame, wm, aContainingBlockSize),
                aFrameType, mFlags, aBorder, aPadding);
    // Override mComputedMargin since reflow roots start from the
    // frame's boundary, which is inside the margin.
    ComputedPhysicalMargin().SizeTo(0, 0, 0, 0);
    ComputedPhysicalOffsets().SizeTo(0, 0, 0, 0);

    ComputedISize() =
      AvailableISize() - ComputedLogicalBorderPadding().IStartEnd(wm);
    if (ComputedISize() < 0) {
      ComputedISize() = 0;
    }
    if (AvailableBSize() != NS_UNCONSTRAINEDSIZE) {
      ComputedBSize() =
        AvailableBSize() - ComputedLogicalBorderPadding().BStartEnd(wm);
      if (ComputedBSize() < 0) {
        ComputedBSize() = 0;
      }
    } else {
      ComputedBSize() = NS_UNCONSTRAINEDSIZE;
    }

    ComputedMinWidth() = ComputedMinHeight() = 0;
    ComputedMaxWidth() = ComputedMaxHeight() = NS_UNCONSTRAINEDSIZE;
  } else {
    // Get the containing block reflow state
    const nsHTMLReflowState* cbrs = mCBReflowState;
    NS_ASSERTION(nullptr != cbrs, "no containing block");

    // If we weren't given a containing block width and height, then
    // compute one
    LogicalSize cbSize = (aContainingBlockSize == LogicalSize(wm, -1, -1))
      ? ComputeContainingBlockRectangle(aPresContext, cbrs)
      : aContainingBlockSize;

    // See if the containing block height is based on the size of its
    // content
    nsIAtom* fType;
    if (NS_AUTOHEIGHT == cbSize.BSize(wm)) {
      // See if the containing block is a cell frame which needs
      // to use the mComputedHeight of the cell instead of what the cell block passed in.
      // XXX It seems like this could lead to bugs with min-height and friends
      if (cbrs->mParentReflowState) {
        fType = cbrs->frame->GetType();
        if (IS_TABLE_CELL(fType)) {
          // use the cell's computed block size
          cbSize.BSize(wm) = cbrs->ComputedSize(wm).BSize(wm);
        }
      }
    }

    // XXX Might need to also pass the CB height (not width) for page boxes,
    // too, if we implement them.

    // For calculating positioning offsets, margins, borders and
    // padding, we use the writing mode of the containing block
    WritingMode cbwm = cbrs->GetWritingMode();
    InitOffsets(cbwm, OffsetPercentBasis(frame, cbwm,
                                         cbSize.ConvertTo(cbwm, wm)),
                aFrameType, mFlags, aBorder, aPadding);

    // For calculating the size of this box, we use its own writing mode
    const nsStyleCoord &blockSize = mStylePosition->BSize(wm);
    nsStyleUnit blockSizeUnit = blockSize.GetUnit();

    // Check for a percentage based block size and a containing block
    // block size that depends on the content block size
    // XXX twiddling blockSizeUnit doesn't help anymore
    // FIXME Shouldn't we fix that?
    if (blockSize.HasPercent()) {
      if (NS_AUTOHEIGHT == cbSize.BSize(wm)) {
        // this if clause enables %-blockSize on replaced inline frames,
        // such as images.  See bug 54119.  The else clause "blockSizeUnit = eStyleUnit_Auto;"
        // used to be called exclusively.
        if (NS_FRAME_REPLACED(NS_CSS_FRAME_TYPE_INLINE) == mFrameType ||
            NS_FRAME_REPLACED_CONTAINS_BLOCK(
                NS_CSS_FRAME_TYPE_INLINE) == mFrameType) {
          // Get the containing block reflow state
          NS_ASSERTION(nullptr != cbrs, "no containing block");
          // in quirks mode, get the cb height using the special quirk method
          if (!wm.IsVertical() &&
              eCompatibility_NavQuirks == aPresContext->CompatibilityMode()) {
            if (!IS_TABLE_CELL(fType)) {
              cbSize.BSize(wm) = CalcQuirkContainingBlockHeight(cbrs);
              if (cbSize.BSize(wm) == NS_AUTOHEIGHT) {
                blockSizeUnit = eStyleUnit_Auto;
              }
            }
            else {
              blockSizeUnit = eStyleUnit_Auto;
            }
          }
          // in standard mode, use the cb block size.  if it's "auto",
          // as will be the case by default in BODY, use auto block size
          // as per CSS2 spec.
          else
          {
            nscoord computedBSize = cbrs->ComputedSize(wm).BSize(wm);
            if (NS_AUTOHEIGHT != computedBSize) {
              cbSize.BSize(wm) = computedBSize;
            }
            else {
              blockSizeUnit = eStyleUnit_Auto;
            }
          }
        }
        else {
          // default to interpreting the blockSize like 'auto'
          blockSizeUnit = eStyleUnit_Auto;
        }
      }
    }

    // Compute our offsets if the element is relatively positioned.  We
    // need the correct containing block inline-size and block-size
    // here, which is why we need to do it after all the quirks-n-such
    // above. (If the element is sticky positioned, we need to wait
    // until the scroll container knows its size, so we compute offsets
    // from StickyScrollContainer::UpdatePositions.)
    if (mStyleDisplay->IsRelativelyPositioned(frame) &&
        NS_STYLE_POSITION_RELATIVE == mStyleDisplay->mPosition) {
      ComputeRelativeOffsets(cbwm, frame, cbSize.ConvertTo(cbwm, wm),
                             ComputedPhysicalOffsets());
    } else {
      // Initialize offsets to 0
      ComputedPhysicalOffsets().SizeTo(0, 0, 0, 0);
    }

    // Calculate the computed values for min and max properties.  Note that
    // this MUST come after we've computed our border and padding.
    ComputeMinMaxValues(cbSize);

    // Calculate the computed inlineSize and blockSize.
    // This varies by frame type.

    if (NS_CSS_FRAME_TYPE_INTERNAL_TABLE == mFrameType) {
      // Internal table elements. The rules vary depending on the type.
      // Calculate the computed isize
      bool rowOrRowGroup = false;
      const nsStyleCoord &inlineSize = mStylePosition->ISize(wm);
      nsStyleUnit inlineSizeUnit = inlineSize.GetUnit();
      if ((NS_STYLE_DISPLAY_TABLE_ROW == mStyleDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == mStyleDisplay->mDisplay)) {
        // 'inlineSize' property doesn't apply to table rows and row groups
        inlineSizeUnit = eStyleUnit_Auto;
        rowOrRowGroup = true;
      }

      // calc() with percentages acts like auto on internal table elements
      if (eStyleUnit_Auto == inlineSizeUnit ||
          (inlineSize.IsCalcUnit() && inlineSize.CalcHasPercent())) {
        ComputedISize() = AvailableISize();

        if ((ComputedISize() != NS_UNCONSTRAINEDSIZE) && !rowOrRowGroup){
          // Internal table elements don't have margins. Only tables and
          // cells have border and padding
          ComputedISize() -= ComputedLogicalBorderPadding().IStartEnd(wm);
          if (ComputedISize() < 0)
            ComputedISize() = 0;
        }
        NS_ASSERTION(ComputedISize() >= 0, "Bogus computed isize");

      } else {
        NS_ASSERTION(inlineSizeUnit == inlineSize.GetUnit(),
                     "unexpected inline size unit change");
        ComputedISize() = ComputeISizeValue(cbSize.ISize(wm),
                                            mStylePosition->mBoxSizing,
                                            inlineSize);
      }

      // Calculate the computed block size
      if ((NS_STYLE_DISPLAY_TABLE_COLUMN == mStyleDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == mStyleDisplay->mDisplay)) {
        // 'blockSize' property doesn't apply to table columns and column groups
        blockSizeUnit = eStyleUnit_Auto;
      }
      // calc() with percentages acts like 'auto' on internal table elements
      if (eStyleUnit_Auto == blockSizeUnit ||
          (blockSize.IsCalcUnit() && blockSize.CalcHasPercent())) {
        ComputedBSize() = NS_AUTOHEIGHT;
      } else {
        NS_ASSERTION(blockSizeUnit == blockSize.GetUnit(),
                     "unexpected block size unit change");
        ComputedBSize() = ComputeBSizeValue(cbSize.BSize(wm),
                                            mStylePosition->mBoxSizing,
                                            blockSize);
      }

      // Doesn't apply to table elements
      ComputedMinWidth() = ComputedMinHeight() = 0;
      ComputedMaxWidth() = ComputedMaxHeight() = NS_UNCONSTRAINEDSIZE;

    } else if (NS_FRAME_GET_TYPE(mFrameType) == NS_CSS_FRAME_TYPE_ABSOLUTE) {
      // XXX not sure if this belongs here or somewhere else - cwk
      InitAbsoluteConstraints(aPresContext, cbrs, cbSize.ConvertTo(cbrs->GetWritingMode(), wm), aFrameType);
    } else {
      AutoMaybeDisableFontInflation an(frame);

      bool isBlock = NS_CSS_FRAME_TYPE_BLOCK == NS_FRAME_GET_TYPE(mFrameType);
      typedef nsIFrame::ComputeSizeFlags ComputeSizeFlags;
      ComputeSizeFlags computeSizeFlags =
        isBlock ? ComputeSizeFlags::eDefault : ComputeSizeFlags::eShrinkWrap;
      if (mFlags.mShrinkWrap) {
        computeSizeFlags =
          ComputeSizeFlags(computeSizeFlags | ComputeSizeFlags::eShrinkWrap);
      }
      if (mFlags.mUseAutoBSize) {
        computeSizeFlags =
          ComputeSizeFlags(computeSizeFlags | ComputeSizeFlags::eUseAutoBSize);
      }

      nsIFrame* parent = frame->GetParent();
      nsIAtom* parentFrameType = parent ? parent->GetType() : nullptr;
      if (parentFrameType == nsGkAtoms::gridContainerFrame) {
        // Shrink-wrap grid items that will be aligned (rather than stretched)
        // in its inline axis.
        auto inlineAxisAlignment = wm.IsOrthogonalTo(cbwm) ?
          mStylePosition->ComputedAlignSelf(frame->StyleContext()->GetParent()) :
          mStylePosition->ComputedJustifySelf(frame->StyleContext()->GetParent());
        if ((inlineAxisAlignment != NS_STYLE_ALIGN_STRETCH &&
             inlineAxisAlignment != NS_STYLE_ALIGN_NORMAL) ||
            mStyleMargin->mMargin.GetIStartUnit(wm) == eStyleUnit_Auto ||
            mStyleMargin->mMargin.GetIEndUnit(wm) == eStyleUnit_Auto) {
          computeSizeFlags =
            ComputeSizeFlags(computeSizeFlags | ComputeSizeFlags::eShrinkWrap);
        }
      } else {
        // Make sure legend frames with display:block and width:auto still
        // shrink-wrap.
        // Also shrink-wrap blocks that are orthogonal to their container.
        if (isBlock &&
            ((aFrameType == nsGkAtoms::legendFrame &&
              frame->StyleContext()->GetPseudo() != nsCSSAnonBoxes::scrolledContent) ||
             (aFrameType == nsGkAtoms::scrollFrame &&
              frame->GetContentInsertionFrame()->GetType() == nsGkAtoms::legendFrame) ||
             (mCBReflowState &&
              mCBReflowState->GetWritingMode().IsOrthogonalTo(mWritingMode)))) {
          computeSizeFlags =
            ComputeSizeFlags(computeSizeFlags | ComputeSizeFlags::eShrinkWrap);
        }

        if (parentFrameType == nsGkAtoms::flexContainerFrame) {
          computeSizeFlags =
            ComputeSizeFlags(computeSizeFlags | ComputeSizeFlags::eShrinkWrap);

          // If we're inside of a flex container that needs to measure our
          // auto height, pass that information along to ComputeSize().
          if (mFlags.mIsFlexContainerMeasuringHeight) {
            computeSizeFlags =
              ComputeSizeFlags(computeSizeFlags | ComputeSizeFlags::eUseAutoBSize);
          }
        } else {
          MOZ_ASSERT(!mFlags.mIsFlexContainerMeasuringHeight,
                     "We're not in a flex container, so the flag "
                     "'mIsFlexContainerMeasuringHeight' shouldn't be set");
        }
      }

      if (cbSize.ISize(wm) == NS_UNCONSTRAINEDSIZE) {
        // For orthogonal flows, where we found a parent orthogonal-limit
        // for AvailableISize() in Init(), we'll use the same here as well.
        cbSize.ISize(wm) = AvailableISize();
      }

      LogicalSize size =
        frame->ComputeSize(rendContext, wm, cbSize, AvailableISize(),
                           ComputedLogicalMargin().Size(wm),
                           ComputedLogicalBorderPadding().Size(wm) -
                             ComputedLogicalPadding().Size(wm),
                           ComputedLogicalPadding().Size(wm),
                           computeSizeFlags);

      ComputedISize() = size.ISize(wm);
      ComputedBSize() = size.BSize(wm);
      NS_ASSERTION(ComputedISize() >= 0, "Bogus inline-size");
      NS_ASSERTION(ComputedBSize() == NS_UNCONSTRAINEDSIZE ||
                   ComputedBSize() >= 0, "Bogus block-size");

      // Exclude inline tables, side captions, flex and grid items from block
      // margin calculations.
      if (isBlock &&
          !IsSideCaption(frame, mStyleDisplay, cbwm) &&
          mStyleDisplay->mDisplay != NS_STYLE_DISPLAY_INLINE_TABLE &&
          parentFrameType != nsGkAtoms::flexContainerFrame &&
          parentFrameType != nsGkAtoms::gridContainerFrame) {
        CalculateBlockSideMargins(aFrameType);
      }
    }
  }
}

static void
UpdateProp(FrameProperties& aProps,
           const FramePropertyDescriptor<nsMargin>* aProperty,
           bool aNeeded,
           nsMargin& aNewValue)
{
  if (aNeeded) {
    nsMargin* propValue = aProps.Get(aProperty);
    if (propValue) {
      *propValue = aNewValue;
    } else {
      aProps.Set(aProperty, new nsMargin(aNewValue));
    }
  } else {
    aProps.Delete(aProperty);
  }
}

void
nsCSSOffsetState::InitOffsets(WritingMode aWM,
                              const LogicalSize& aPercentBasis,
                              nsIAtom* aFrameType,
                              ReflowStateFlags aFlags,
                              const nsMargin* aBorder,
                              const nsMargin* aPadding)
{
  DISPLAY_INIT_OFFSETS(frame, this, aPercentBasis, aBorder, aPadding);

  // Since we are in reflow, we don't need to store these properties anymore
  // unless they are dependent on width, in which case we store the new value.
  nsPresContext *presContext = frame->PresContext();
  FrameProperties props(presContext->PropertyTable(), frame);
  props.Delete(nsIFrame::UsedBorderProperty());

  // Compute margins from the specified margin style information. These
  // become the default computed values, and may be adjusted below
  // XXX fix to provide 0,0 for the top&bottom margins for
  // inline-non-replaced elements
  bool needMarginProp = ComputeMargin(aWM, aPercentBasis);
  // XXX We need to include 'auto' horizontal margins in this too!
  // ... but if we did that, we'd need to fix nsFrame::GetUsedMargin
  // to use it even when the margins are all zero (since sometimes
  // they get treated as auto)
  ::UpdateProp(props, nsIFrame::UsedMarginProperty(), needMarginProp,
               ComputedPhysicalMargin());


  const nsStyleDisplay *disp = frame->StyleDisplay();
  bool isThemed = frame->IsThemed(disp);
  bool needPaddingProp;
  nsIntMargin widget;
  if (isThemed &&
      presContext->GetTheme()->GetWidgetPadding(presContext->DeviceContext(),
                                                frame, disp->mAppearance,
                                                &widget)) {
    ComputedPhysicalPadding().top = presContext->DevPixelsToAppUnits(widget.top);
    ComputedPhysicalPadding().right = presContext->DevPixelsToAppUnits(widget.right);
    ComputedPhysicalPadding().bottom = presContext->DevPixelsToAppUnits(widget.bottom);
    ComputedPhysicalPadding().left = presContext->DevPixelsToAppUnits(widget.left);
    needPaddingProp = false;
  }
  else if (frame->IsSVGText()) {
    ComputedPhysicalPadding().SizeTo(0, 0, 0, 0);
    needPaddingProp = false;
  }
  else if (aPadding) { // padding is an input arg
    ComputedPhysicalPadding() = *aPadding;
    needPaddingProp = frame->StylePadding()->IsWidthDependent() ||
	  (frame->GetStateBits() & NS_FRAME_REFLOW_ROOT);
  }
  else {
    needPaddingProp = ComputePadding(aWM, aPercentBasis, aFrameType);
  }

  // Add [align|justify]-content:baseline padding contribution.
  typedef const FramePropertyDescriptor<SmallValueHolder<nscoord>>* Prop;
  auto ApplyBaselinePadding = [this, &needPaddingProp]
         (LogicalAxis aAxis, Prop aProp) {
    bool found;
    nscoord val = frame->Properties().Get(aProp, &found);
    if (found) {
      NS_ASSERTION(val != nscoord(0), "zero in this property is useless");
      WritingMode wm = GetWritingMode();
      LogicalSide side;
      if (val > 0) {
        side = MakeLogicalSide(aAxis, eLogicalEdgeStart);
      } else {
        side = MakeLogicalSide(aAxis, eLogicalEdgeEnd);
        val = -val;
      }
      mComputedPadding.Side(wm.PhysicalSide(side)) += val;
      needPaddingProp = true;
    }
  };
  if (!aFlags.mUseAutoBSize) {
    ApplyBaselinePadding(eLogicalAxisBlock, nsIFrame::BBaselinePadProperty());
  }
  if (!aFlags.mShrinkWrap) {
    ApplyBaselinePadding(eLogicalAxisInline, nsIFrame::IBaselinePadProperty());
  }

  if (isThemed) {
    nsIntMargin widget;
    presContext->GetTheme()->GetWidgetBorder(presContext->DeviceContext(),
                                             frame, disp->mAppearance,
                                             &widget);
    ComputedPhysicalBorderPadding().top =
      presContext->DevPixelsToAppUnits(widget.top);
    ComputedPhysicalBorderPadding().right =
      presContext->DevPixelsToAppUnits(widget.right);
    ComputedPhysicalBorderPadding().bottom =
      presContext->DevPixelsToAppUnits(widget.bottom);
    ComputedPhysicalBorderPadding().left =
      presContext->DevPixelsToAppUnits(widget.left);
  }
  else if (frame->IsSVGText()) {
    ComputedPhysicalBorderPadding().SizeTo(0, 0, 0, 0);
  }
  else if (aBorder) {  // border is an input arg
    ComputedPhysicalBorderPadding() = *aBorder;
  }
  else {
    ComputedPhysicalBorderPadding() = frame->StyleBorder()->GetComputedBorder();
  }
  ComputedPhysicalBorderPadding() += ComputedPhysicalPadding();

  if (aFrameType == nsGkAtoms::tableFrame) {
    nsTableFrame *tableFrame = static_cast<nsTableFrame*>(frame);

    if (tableFrame->IsBorderCollapse()) {
      // border-collapsed tables don't use any of their padding, and
      // only part of their border.  We need to do this here before we
      // try to do anything like handling 'auto' widths,
      // 'box-sizing', or 'auto' margins.
      ComputedPhysicalPadding().SizeTo(0,0,0,0);
      SetComputedLogicalBorderPadding(
        tableFrame->GetIncludedOuterBCBorder(mWritingMode));
    }

    // The margin is inherited to the outer table frame via
    // the ::-moz-table-outer rule in ua.css.
    ComputedPhysicalMargin().SizeTo(0, 0, 0, 0);
  } else if (aFrameType == nsGkAtoms::scrollbarFrame) {
    // scrollbars may have had their width or height smashed to zero
    // by the associated scrollframe, in which case we must not report
    // any padding or border.
    nsSize size(frame->GetSize());
    if (size.width == 0 || size.height == 0) {
      ComputedPhysicalPadding().SizeTo(0,0,0,0);
      ComputedPhysicalBorderPadding().SizeTo(0,0,0,0);
    }
  }
  ::UpdateProp(props, nsIFrame::UsedPaddingProperty(), needPaddingProp,
               ComputedPhysicalPadding());
}

// This code enforces section 10.3.3 of the CSS2 spec for this formula:
//
// 'margin-left' + 'border-left-width' + 'padding-left' + 'width' +
//   'padding-right' + 'border-right-width' + 'margin-right'
//   = width of containing block 
//
// Note: the width unit is not auto when this is called
void
nsHTMLReflowState::CalculateBlockSideMargins(nsIAtom* aFrameType)
{
  // Calculations here are done in the containing block's writing mode,
  // which is where margins will eventually be applied: we're calculating
  // margins that will be used by the container in its inline direction,
  // which in the case of an orthogonal contained block will correspond to
  // the block direction of this reflow state. So in the orthogonal-flow
  // case, "CalculateBlock*Side*Margins" will actually end up adjusting
  // the BStart/BEnd margins; those are the "sides" of the block from its
  // container's point of view.
  WritingMode cbWM =
    mCBReflowState ? mCBReflowState->GetWritingMode(): GetWritingMode();

  nscoord availISizeCBWM = AvailableSize(cbWM).ISize(cbWM);
  nscoord computedISizeCBWM = ComputedSize(cbWM).ISize(cbWM);
  if (computedISizeCBWM == NS_UNCONSTRAINEDSIZE) {
    // For orthogonal flows, where we found a parent orthogonal-limit
    // for AvailableISize() in Init(), we'll use the same here as well.
    computedISizeCBWM = availISizeCBWM;
  }

  LAYOUT_WARN_IF_FALSE(NS_UNCONSTRAINEDSIZE != computedISizeCBWM &&
                       NS_UNCONSTRAINEDSIZE != availISizeCBWM,
                       "have unconstrained inline-size; this should only "
                       "result from very large sizes, not attempts at "
                       "intrinsic inline-size calculation");

  LogicalMargin margin =
    ComputedLogicalMargin().ConvertTo(cbWM, mWritingMode);
  LogicalMargin borderPadding =
    ComputedLogicalBorderPadding().ConvertTo(cbWM, mWritingMode);
  nscoord sum = margin.IStartEnd(cbWM) +
    borderPadding.IStartEnd(cbWM) + computedISizeCBWM;
  if (sum == availISizeCBWM) {
    // The sum is already correct
    return;
  }

  // Determine the start and end margin values. The isize value
  // remains constant while we do this.

  // Calculate how much space is available for margins
  nscoord availMarginSpace = availISizeCBWM - sum;

  // If the available margin space is negative, then don't follow the
  // usual overconstraint rules.
  if (availMarginSpace < 0) {
    margin.IEnd(cbWM) += availMarginSpace;
    SetComputedLogicalMargin(margin.ConvertTo(mWritingMode, cbWM));
    return;
  }

  // The css2 spec clearly defines how block elements should behave
  // in section 10.3.3.
  const nsStyleSides& styleSides = mStyleMargin->mMargin;
  bool isAutoStartMargin = eStyleUnit_Auto == styleSides.GetIStartUnit(cbWM);
  bool isAutoEndMargin = eStyleUnit_Auto == styleSides.GetIEndUnit(cbWM);
  if (!isAutoStartMargin && !isAutoEndMargin) {
    // Neither margin is 'auto' so we're over constrained. Use the
    // 'direction' property of the parent to tell which margin to
    // ignore
    // First check if there is an HTML alignment that we should honor
    const nsHTMLReflowState* prs = mParentReflowState;
    if (aFrameType == nsGkAtoms::tableFrame) {
      NS_ASSERTION(prs->frame->GetType() == nsGkAtoms::tableOuterFrame,
                   "table not inside outer table");
      // Center the table within the outer table based on the alignment
      // of the outer table's parent.
      prs = prs->mParentReflowState;
    }
    if (prs &&
        (prs->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_LEFT ||
         prs->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER ||
         prs->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT)) {
      if (prs->mWritingMode.IsBidiLTR()) {
        isAutoStartMargin =
          prs->mStyleText->mTextAlign != NS_STYLE_TEXT_ALIGN_MOZ_LEFT;
        isAutoEndMargin =
          prs->mStyleText->mTextAlign != NS_STYLE_TEXT_ALIGN_MOZ_RIGHT;
      } else {
        isAutoStartMargin =
          prs->mStyleText->mTextAlign != NS_STYLE_TEXT_ALIGN_MOZ_RIGHT;
        isAutoEndMargin =
          prs->mStyleText->mTextAlign != NS_STYLE_TEXT_ALIGN_MOZ_LEFT;
      }
    }
    // Otherwise apply the CSS rules, and ignore one margin by forcing
    // it to 'auto', depending on 'direction'.
    else {
      isAutoEndMargin = true;
    }
  }

  // Logic which is common to blocks and tables
  // The computed margins need not be zero because the 'auto' could come from
  // overconstraint or from HTML alignment so values need to be accumulated

  if (isAutoStartMargin) {
    if (isAutoEndMargin) {
      // Both margins are 'auto' so the computed addition should be equal
      nscoord forStart = availMarginSpace / 2;
      margin.IStart(cbWM) += forStart;
      margin.IEnd(cbWM) += availMarginSpace - forStart;
    } else {
      margin.IStart(cbWM) += availMarginSpace;
    }
  } else if (isAutoEndMargin) {
    margin.IEnd(cbWM) += availMarginSpace;
  }
  SetComputedLogicalMargin(margin.ConvertTo(mWritingMode, cbWM));
}

#define NORMAL_LINE_HEIGHT_FACTOR 1.2f    // in term of emHeight 
// For "normal" we use the font's normal line height (em height + leading).
// If both internal leading and  external leading specified by font itself
// are zeros, we should compensate this by creating extra (external) leading 
// in eCompensateLeading mode. This is necessary because without this 
// compensation, normal line height might looks too tight. 

// For risk management, we use preference to control the behavior, and 
// eNoExternalLeading is the old behavior.
static nscoord
GetNormalLineHeight(nsFontMetrics* aFontMetrics)
{
  NS_PRECONDITION(nullptr != aFontMetrics, "no font metrics");

  nscoord normalLineHeight;

  nscoord externalLeading = aFontMetrics->ExternalLeading();
  nscoord internalLeading = aFontMetrics->InternalLeading();
  nscoord emHeight = aFontMetrics->EmHeight();
  switch (GetNormalLineHeightCalcControl()) {
  case eIncludeExternalLeading:
    normalLineHeight = emHeight+ internalLeading + externalLeading;
    break;
  case eCompensateLeading:
    if (!internalLeading && !externalLeading)
      normalLineHeight = NSToCoordRound(emHeight * NORMAL_LINE_HEIGHT_FACTOR);
    else
      normalLineHeight = emHeight+ internalLeading + externalLeading;
    break;
  default:
    //case eNoExternalLeading:
    normalLineHeight = emHeight + internalLeading;
  }
  return normalLineHeight;
}

static inline nscoord
ComputeLineHeight(nsStyleContext* aStyleContext,
                  nscoord aBlockBSize,
                  float aFontSizeInflation)
{
  const nsStyleCoord& lhCoord = aStyleContext->StyleText()->mLineHeight;

  if (lhCoord.GetUnit() == eStyleUnit_Coord) {
    nscoord result = lhCoord.GetCoordValue();
    if (aFontSizeInflation != 1.0f) {
      result = NSToCoordRound(result * aFontSizeInflation);
    }
    return result;
  }

  if (lhCoord.GetUnit() == eStyleUnit_Factor)
    // For factor units the computed value of the line-height property 
    // is found by multiplying the factor by the font's computed size
    // (adjusted for min-size prefs and text zoom).
    return NSToCoordRound(lhCoord.GetFactorValue() * aFontSizeInflation *
                          aStyleContext->StyleFont()->mFont.size);

  NS_ASSERTION(lhCoord.GetUnit() == eStyleUnit_Normal ||
               lhCoord.GetUnit() == eStyleUnit_Enumerated,
               "bad line-height unit");
  
  if (lhCoord.GetUnit() == eStyleUnit_Enumerated) {
    NS_ASSERTION(lhCoord.GetIntValue() == NS_STYLE_LINE_HEIGHT_BLOCK_HEIGHT,
                 "bad line-height value");
    if (aBlockBSize != NS_AUTOHEIGHT) {
      return aBlockBSize;
    }
  }

  RefPtr<nsFontMetrics> fm = nsLayoutUtils::
    GetFontMetricsForStyleContext(aStyleContext, aFontSizeInflation);
  return GetNormalLineHeight(fm);
}

nscoord
nsHTMLReflowState::CalcLineHeight() const
{
  nscoord blockBSize =
    nsLayoutUtils::IsNonWrapperBlock(frame) ? ComputedBSize() :
    (mCBReflowState ? mCBReflowState->ComputedBSize() : NS_AUTOHEIGHT);

  return CalcLineHeight(frame->GetContent(), frame->StyleContext(), blockBSize,
                        nsLayoutUtils::FontSizeInflationFor(frame));
}

/* static */ nscoord
nsHTMLReflowState::CalcLineHeight(nsIContent* aContent,
                                  nsStyleContext* aStyleContext,
                                  nscoord aBlockBSize,
                                  float aFontSizeInflation)
{
  NS_PRECONDITION(aStyleContext, "Must have a style context");

  nscoord lineHeight =
    ComputeLineHeight(aStyleContext, aBlockBSize, aFontSizeInflation);

  NS_ASSERTION(lineHeight >= 0, "ComputeLineHeight screwed up");

  HTMLInputElement* input = HTMLInputElement::FromContentOrNull(aContent);
  if (input && input->IsSingleLineTextControl()) {
    // For Web-compatibility, single-line text input elements cannot
    // have a line-height smaller than one.
    nscoord lineHeightOne =
      aFontSizeInflation * aStyleContext->StyleFont()->mFont.size;
    if (lineHeight < lineHeightOne) {
      lineHeight = lineHeightOne;
    }
  }

  return lineHeight;
}

bool
nsCSSOffsetState::ComputeMargin(WritingMode aWM,
                                const LogicalSize& aPercentBasis)
{
  // SVG text frames have no margin.
  if (frame->IsSVGText()) {
    return false;
  }

  // If style style can provide us the margin directly, then use it.
  const nsStyleMargin *styleMargin = frame->StyleMargin();

  bool isCBDependent = !styleMargin->GetMargin(ComputedPhysicalMargin());
  if (isCBDependent) {
    // We have to compute the value. Note that this calculation is
    // performed according to the writing mode of the containing block
    // (http://dev.w3.org/csswg/css-writing-modes-3/#orthogonal-flows)
    LogicalMargin m(aWM);
    m.IStart(aWM) = nsLayoutUtils::
      ComputeCBDependentValue(aPercentBasis.ISize(aWM),
                              styleMargin->mMargin.GetIStart(aWM));
    m.IEnd(aWM) = nsLayoutUtils::
      ComputeCBDependentValue(aPercentBasis.ISize(aWM),
                              styleMargin->mMargin.GetIEnd(aWM));

    m.BStart(aWM) = nsLayoutUtils::
      ComputeCBDependentValue(aPercentBasis.BSize(aWM),
                              styleMargin->mMargin.GetBStart(aWM));
    m.BEnd(aWM) = nsLayoutUtils::
      ComputeCBDependentValue(aPercentBasis.BSize(aWM),
                              styleMargin->mMargin.GetBEnd(aWM));

    SetComputedLogicalMargin(aWM, m);
  }

  // ... but font-size-inflation-based margin adjustment uses the
  // frame's writing mode
  nscoord marginAdjustment = FontSizeInflationListMarginAdjustment(frame);

  if (marginAdjustment > 0) {
    LogicalMargin m = ComputedLogicalMargin();
    m.IStart(mWritingMode) += marginAdjustment;
    SetComputedLogicalMargin(m);
  }

  return isCBDependent;
}

bool
nsCSSOffsetState::ComputePadding(WritingMode aWM,
                                 const LogicalSize& aPercentBasis,
                                 nsIAtom* aFrameType)
{
  // If style can provide us the padding directly, then use it.
  const nsStylePadding *stylePadding = frame->StylePadding();
  bool isCBDependent = !stylePadding->GetPadding(ComputedPhysicalPadding());
  // a table row/col group, row/col doesn't have padding
  // XXXldb Neither do border-collapse tables.
  if (nsGkAtoms::tableRowGroupFrame == aFrameType ||
      nsGkAtoms::tableColGroupFrame == aFrameType ||
      nsGkAtoms::tableRowFrame      == aFrameType ||
      nsGkAtoms::tableColFrame      == aFrameType) {
    ComputedPhysicalPadding().SizeTo(0,0,0,0);
  }
  else if (isCBDependent) {
    // We have to compute the value. This calculation is performed
    // according to the writing mode of the containing block
    // (http://dev.w3.org/csswg/css-writing-modes-3/#orthogonal-flows)
    // clamp negative calc() results to 0
    LogicalMargin p(aWM);
    p.IStart(aWM) = std::max(0, nsLayoutUtils::
      ComputeCBDependentValue(aPercentBasis.ISize(aWM),
                              stylePadding->mPadding.GetIStart(aWM)));
    p.IEnd(aWM) = std::max(0, nsLayoutUtils::
      ComputeCBDependentValue(aPercentBasis.ISize(aWM),
                              stylePadding->mPadding.GetIEnd(aWM)));

    p.BStart(aWM) = std::max(0, nsLayoutUtils::
      ComputeCBDependentValue(aPercentBasis.BSize(aWM),
                              stylePadding->mPadding.GetBStart(aWM)));
    p.BEnd(aWM) = std::max(0, nsLayoutUtils::
      ComputeCBDependentValue(aPercentBasis.BSize(aWM),
                              stylePadding->mPadding.GetBEnd(aWM)));

    SetComputedLogicalPadding(aWM, p);
  }
  return isCBDependent;
}

void
nsHTMLReflowState::ComputeMinMaxValues(const LogicalSize&aCBSize)
{
  WritingMode wm = GetWritingMode();

  const nsStyleCoord& minISize = mStylePosition->MinISize(wm);
  const nsStyleCoord& maxISize = mStylePosition->MaxISize(wm);
  const nsStyleCoord& minBSize = mStylePosition->MinBSize(wm);
  const nsStyleCoord& maxBSize = mStylePosition->MaxBSize(wm);

  // NOTE: min-width:auto resolves to 0, except on a flex item. (But
  // even there, it's supposed to be ignored (i.e. treated as 0) until
  // the flex container explicitly resolves & considers it.)
  if (eStyleUnit_Auto == minISize.GetUnit()) {
    ComputedMinISize() = 0;
  } else {
    ComputedMinISize() = ComputeISizeValue(aCBSize.ISize(wm),
                                           mStylePosition->mBoxSizing,
                                           minISize);
  }

  if (eStyleUnit_None == maxISize.GetUnit()) {
    // Specified value of 'none'
    ComputedMaxISize() = NS_UNCONSTRAINEDSIZE;  // no limit
  } else {
    ComputedMaxISize() = ComputeISizeValue(aCBSize.ISize(wm),
                                           mStylePosition->mBoxSizing,
                                           maxISize);
  }

  // If the computed value of 'min-width' is greater than the value of
  // 'max-width', 'max-width' is set to the value of 'min-width'
  if (ComputedMinISize() > ComputedMaxISize()) {
    ComputedMaxISize() = ComputedMinISize();
  }

  // Check for percentage based values and a containing block height that
  // depends on the content height. Treat them like 'auto'
  // Likewise, check for calc() with percentages on internal table elements;
  // that's treated as 'auto' too.
  // Likewise, if we're a child of a flex container who's measuring our
  // intrinsic height, then we want to disregard our min-height.

  // NOTE: min-height:auto resolves to 0, except on a flex item. (But
  // even there, it's supposed to be ignored (i.e. treated as 0) until
  // the flex container explicitly resolves & considers it.)
  if (eStyleUnit_Auto == minBSize.GetUnit() ||
      (NS_AUTOHEIGHT == aCBSize.BSize(wm) &&
       minBSize.HasPercent()) ||
      (mFrameType == NS_CSS_FRAME_TYPE_INTERNAL_TABLE &&
       minBSize.IsCalcUnit() && minBSize.CalcHasPercent()) ||
      mFlags.mIsFlexContainerMeasuringHeight) {
    ComputedMinBSize() = 0;
  } else {
    ComputedMinBSize() = ComputeBSizeValue(aCBSize.BSize(wm),
                                           mStylePosition->mBoxSizing,
                                           minBSize);
  }
  nsStyleUnit maxBSizeUnit = maxBSize.GetUnit();
  if (eStyleUnit_None == maxBSizeUnit) {
    // Specified value of 'none'
    ComputedMaxBSize() = NS_UNCONSTRAINEDSIZE;  // no limit
  } else {
    // Check for percentage based values and a containing block height that
    // depends on the content height. Treat them like 'none'
    // Likewise, check for calc() with percentages on internal table elements;
    // that's treated as 'auto' too.
    // Likewise, if we're a child of a flex container who's measuring our
    // intrinsic height, then we want to disregard our max-height.
    if ((NS_AUTOHEIGHT == aCBSize.BSize(wm) &&
         maxBSize.HasPercent()) ||
        (mFrameType == NS_CSS_FRAME_TYPE_INTERNAL_TABLE &&
         maxBSize.IsCalcUnit() && maxBSize.CalcHasPercent()) ||
        mFlags.mIsFlexContainerMeasuringHeight) {
      ComputedMaxBSize() = NS_UNCONSTRAINEDSIZE;
    } else {
      ComputedMaxBSize() = ComputeBSizeValue(aCBSize.BSize(wm),
                                             mStylePosition->mBoxSizing,
                                             maxBSize);
    }
  }

  // If the computed value of 'min-height' is greater than the value of
  // 'max-height', 'max-height' is set to the value of 'min-height'
  if (ComputedMinBSize() > ComputedMaxBSize()) {
    ComputedMaxBSize() = ComputedMinBSize();
  }
}

void
nsHTMLReflowState::SetTruncated(const nsHTMLReflowMetrics& aMetrics,
                                nsReflowStatus* aStatus) const
{
  const WritingMode containerWM = aMetrics.GetWritingMode();
  if (GetWritingMode().IsOrthogonalTo(containerWM)) {
    // Orthogonal flows are always reflowed with an unconstrained dimension,
    // so should never end up truncated (see nsHTMLReflowState::Init()).
    *aStatus &= ~NS_FRAME_TRUNCATED;
  } else if (AvailableBSize() != NS_UNCONSTRAINEDSIZE &&
             AvailableBSize() < aMetrics.BSize(containerWM) &&
             !mFlags.mIsTopOfPage) {
    *aStatus |= NS_FRAME_TRUNCATED;
  } else {
    *aStatus &= ~NS_FRAME_TRUNCATED;
  }
}

bool
nsHTMLReflowState::IsFloating() const
{
  return mStyleDisplay->IsFloating(frame);
}

uint8_t
nsHTMLReflowState::GetDisplay() const
{
  return mStyleDisplay->GetDisplay(frame);
}
