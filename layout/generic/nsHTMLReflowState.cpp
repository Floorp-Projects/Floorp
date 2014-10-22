/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* struct containing the input to nsIFrame::Reflow */

#include "nsHTMLReflowState.h"

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
#include "nsFlexContainerFrame.h"
#include "nsImageFrame.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsIPercentHeightObserver.h"
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
  parentReflowState = nullptr;
  AvailableISize() = aAvailableSpace.ISize(mWritingMode);
  AvailableBSize() = aAvailableSpace.BSize(mWritingMode);
  mFloatManager = nullptr;
  mLineLayout = nullptr;
  memset(&mFlags, 0, sizeof(mFlags));
  mDiscoveredClearance = nullptr;
  mPercentHeightObserver = nullptr;

  if (aFlags & DUMMY_PARENT_REFLOW_STATE) {
    mFlags.mDummyParentReflowState = true;
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

// NOTE: If we ever want to use nsCSSOffsetState for a flex item or a grid
// item, we need to make it take the containing-block height as well as the
// width, since flex items and grid items resolve vertical percent margins
// and padding against the containing-block height, rather than its width.
nsCSSOffsetState::nsCSSOffsetState(nsIFrame *aFrame,
                                   nsRenderingContext *aRenderingContext,
                                   nscoord aContainingBlockWidth)
  : frame(aFrame)
  , rendContext(aRenderingContext)
  , mWritingMode(aFrame->GetWritingMode())
{
  MOZ_ASSERT(!aFrame->IsFlexOrGridItem(),
             "We're about to resolve vertical percent margin & padding "
             "values against CB width, which is incorrect for flex/grid items");
  InitOffsets(aContainingBlockWidth, aContainingBlockWidth, frame->GetType());
}

// Initialize a reflow state for a child frame's reflow. Some state
// is copied from the parent reflow state; the remaining state is
// computed.
nsHTMLReflowState::nsHTMLReflowState(nsPresContext*           aPresContext,
                                     const nsHTMLReflowState& aParentReflowState,
                                     nsIFrame*                aFrame,
                                     const LogicalSize&       aAvailableSpace,
                                     nscoord                  aContainingBlockWidth,
                                     nscoord                  aContainingBlockHeight,
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
  NS_PRECONDITION((aContainingBlockWidth == -1) ==
                    (aContainingBlockHeight == -1),
                  "cb width and height should only be non-default together");
  NS_PRECONDITION(!mFlags.mSpecialHeightReflow ||
                  !NS_SUBTREE_DIRTY(aFrame),
                  "frame should be clean when getting special height reflow");

  parentReflowState = &aParentReflowState;

  // If the parent is dirty, then the child is as well.
  // XXX Are the other cases where the parent reflows a child a second
  // time, as a resize?
  if (!mFlags.mSpecialHeightReflow)
    frame->AddStateBits(parentReflowState->frame->GetStateBits() &
                        NS_FRAME_IS_DIRTY);

  AvailableISize() = aAvailableSpace.ISize(mWritingMode);
  AvailableBSize() = aAvailableSpace.BSize(mWritingMode);

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
  mFlags.mHasClearance = false;
  mFlags.mIsColumnBalancing = false;
  mFlags.mIsFlexContainerMeasuringHeight = false;
  mFlags.mDummyParentReflowState = false;

  mDiscoveredClearance = nullptr;
  mPercentHeightObserver = (aParentReflowState.mPercentHeightObserver &&
                            aParentReflowState.mPercentHeightObserver->NeedsToObserve(*this))
                           ? aParentReflowState.mPercentHeightObserver : nullptr;

  if ((aFlags & DUMMY_PARENT_REFLOW_STATE) ||
      (parentReflowState->mFlags.mDummyParentReflowState &&
       frame->GetType() == nsGkAtoms::tableFrame)) {
    mFlags.mDummyParentReflowState = true;
  }

  if (!(aFlags & CALLER_WILL_INIT)) {
    Init(aPresContext, aContainingBlockWidth, aContainingBlockHeight);
  }
}

inline nscoord
nsCSSOffsetState::ComputeWidthValue(nscoord aContainingBlockWidth,
                                    nscoord aContentEdgeToBoxSizing,
                                    nscoord aBoxSizingToMarginEdge,
                                    const nsStyleCoord& aCoord)
{
  return nsLayoutUtils::ComputeWidthValue(rendContext, frame,
                                          aContainingBlockWidth,
                                          aContentEdgeToBoxSizing,
                                          aBoxSizingToMarginEdge,
                                          aCoord);
}

nscoord
nsCSSOffsetState::ComputeWidthValue(nscoord aContainingBlockWidth,
                                    uint8_t aBoxSizing,
                                    const nsStyleCoord& aCoord)
{
  nscoord inside = 0, outside = ComputedPhysicalBorderPadding().LeftRight() +
                                ComputedPhysicalMargin().LeftRight();
  switch (aBoxSizing) {
    case NS_STYLE_BOX_SIZING_BORDER:
      inside = ComputedPhysicalBorderPadding().LeftRight();
      break;
    case NS_STYLE_BOX_SIZING_PADDING:
      inside = ComputedPhysicalPadding().LeftRight();
      break;
  }
  outside -= inside;

  return ComputeWidthValue(aContainingBlockWidth, inside,
                           outside, aCoord);
}

nscoord
nsCSSOffsetState::ComputeHeightValue(nscoord aContainingBlockHeight,
                                     uint8_t aBoxSizing,
                                     const nsStyleCoord& aCoord)
{
  nscoord inside = 0;
  switch (aBoxSizing) {
    case NS_STYLE_BOX_SIZING_BORDER:
      inside = ComputedPhysicalBorderPadding().TopBottom();
      break;
    case NS_STYLE_BOX_SIZING_PADDING:
      inside = ComputedPhysicalPadding().TopBottom();
      break;
  }
  return nsLayoutUtils::ComputeHeightValue(aContainingBlockHeight, 
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
    if (frameType != nsGkAtoms::viewportFrame) { // Or check GetParent()?
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
    InitResizeFlags(frame->PresContext(), frame->GetType());
  }
}

void
nsHTMLReflowState::Init(nsPresContext* aPresContext,
                        nscoord         aContainingBlockWidth,
                        nscoord         aContainingBlockHeight,
                        const nsMargin* aBorder,
                        const nsMargin* aPadding)
{
  if (AvailableISize() == NS_UNCONSTRAINEDSIZE) {
    // Look up the parent chain for an orthogonal inline limit,
    // and reset AvailableISize() if found.
    for (const nsHTMLReflowState *parent = parentReflowState;
         parent != nullptr; parent = parent->parentReflowState) {
      if (parent->GetWritingMode().IsOrthogonalTo(mWritingMode) &&
          parent->mOrthogonalLimit != NS_UNCONSTRAINEDSIZE) {
        AvailableISize() = parent->mOrthogonalLimit;
        break;
      }
    }
  }

  NS_WARN_IF_FALSE(AvailableISize() != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained inline-size; this should only result from "
                   "very large sizes, not attempts at intrinsic inline-size "
                   "calculation");

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

  InitConstraints(aPresContext, aContainingBlockWidth, aContainingBlockHeight,
                  aBorder, aPadding, type);

  InitResizeFlags(aPresContext, type);

  nsIFrame *parent = frame->GetParent();
  if (parent &&
      (parent->GetStateBits() & NS_FRAME_IN_CONSTRAINED_HEIGHT) &&
      !(parent->GetType() == nsGkAtoms::scrollFrame &&
        parent->StyleDisplay()->mOverflowY != NS_STYLE_OVERFLOW_HIDDEN)) {
    frame->AddStateBits(NS_FRAME_IN_CONSTRAINED_HEIGHT);
  } else if (type == nsGkAtoms::svgForeignObjectFrame) {
    // An SVG foreignObject frame is inherently constrained height.
    frame->AddStateBits(NS_FRAME_IN_CONSTRAINED_HEIGHT);
  } else if ((mStylePosition->mHeight.GetUnit() != eStyleUnit_Auto ||
              mStylePosition->mMaxHeight.GetUnit() != eStyleUnit_None) &&
              // Don't set NS_FRAME_IN_CONSTRAINED_HEIGHT on body or html
              // elements.
             (frame->GetContent() &&
            !(frame->GetContent()->IsHTML(nsGkAtoms::body) ||
              frame->GetContent()->IsHTML(nsGkAtoms::html)))) {

    // If our height was specified as a percentage, then this could
    // actually resolve to 'auto', based on:
    // http://www.w3.org/TR/CSS21/visudet.html#the-height-property
    nsIFrame* containingBlk = frame;
    while (containingBlk) {
      const nsStylePosition* stylePos = containingBlk->StylePosition();
      if ((stylePos->mHeight.IsCoordPercentCalcUnit() &&
           !stylePos->mHeight.HasPercent()) ||
          (stylePos->mMaxHeight.IsCoordPercentCalcUnit() &&
           !stylePos->mMaxHeight.HasPercent())) {
        frame->AddStateBits(NS_FRAME_IN_CONSTRAINED_HEIGHT);
        break;
      } else if ((stylePos->mHeight.IsCoordPercentCalcUnit() &&
                  stylePos->mHeight.HasPercent()) ||
                 (stylePos->mMaxHeight.IsCoordPercentCalcUnit() &&
                  stylePos->mMaxHeight.HasPercent())) {
        if (!(containingBlk = containingBlk->GetContainingBlock())) {
          // If we've reached the top of the tree, then we don't have
          // a constrained height.
          frame->RemoveStateBits(NS_FRAME_IN_CONSTRAINED_HEIGHT);
          break;
        }

        continue;
      } else {
        frame->RemoveStateBits(NS_FRAME_IN_CONSTRAINED_HEIGHT);
        break;
      }
    }
  } else {
    frame->RemoveStateBits(NS_FRAME_IN_CONSTRAINED_HEIGHT);
  }

  NS_WARN_IF_FALSE((mFrameType == NS_CSS_FRAME_TYPE_INLINE &&
                    !frame->IsFrameOfType(nsIFrame::eReplaced)) ||
                   type == nsGkAtoms::textFrame ||
                   ComputedISize() != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained inline-size; this should only result from "
                   "very large sizes, not attempts at intrinsic inline-size "
                   "calculation");
}

void nsHTMLReflowState::InitCBReflowState()
{
  if (!parentReflowState) {
    mCBReflowState = nullptr;
    return;
  }

  if (parentReflowState->frame == frame->GetContainingBlock()) {
    // Inner table frames need to use the containing block of the outer
    // table frame.
    if (frame->GetType() == nsGkAtoms::tableFrame) {
      mCBReflowState = parentReflowState->mCBReflowState;
    } else {
      mCBReflowState = parentReflowState;
    }
  } else {
    mCBReflowState = parentReflowState->mCBReflowState;
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
  bool isHResize = (frame->GetSize().width !=
                     ComputedWidth() + ComputedPhysicalBorderPadding().LeftRight()) ||
                     aPresContext->PresShell()->IsReflowOnZoomPending();

  if ((frame->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT) &&
      nsLayoutUtils::FontSizeInflationEnabled(aPresContext)) {
    // Create our font inflation data if we don't have it already, and
    // give it our current width information.
    bool dirty = nsFontInflationData::UpdateFontInflationDataWidthFor(*this) &&
                 // Avoid running this at the box-to-block interface
                 // (where we shouldn't be inflating anyway, and where
                 // reflow state construction is probably to construct a
                 // dummy parent reflow state anyway).
                 !mFlags.mDummyParentReflowState;

    if (dirty || (!frame->GetParent() && isHResize)) {
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
      // we go through InitResizeFlags we set mHResize to true, and then
      // the second time we'd set it to false even without the
      // NS_FRAME_IS_DIRTY bit already set.
      if (frame->GetType() == nsGkAtoms::svgForeignObjectFrame) {
        // Foreign object frames use dirty bits in a special way.
        frame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
        nsIFrame *kid = frame->GetFirstPrincipalChild();
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

      nsAutoTArray<nsIFrame*, 32> stack;
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

  mFlags.mHResize = !(frame->GetStateBits() & NS_FRAME_IS_DIRTY) &&
                    isHResize;

  // XXX Should we really need to null check mCBReflowState?  (We do for
  // at least nsBoxFrame).
  if (IS_TABLE_CELL(aFrameType) &&
      (mFlags.mSpecialHeightReflow ||
       (frame->FirstInFlow()->GetStateBits() &
         NS_TABLE_CELL_HAD_SPECIAL_REFLOW)) &&
      (frame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT)) {
    // Need to set the bit on the cell so that
    // mCBReflowState->mFlags.mVResize is set correctly below when
    // reflowing descendant.
    mFlags.mVResize = true;
  } else if (mCBReflowState && !nsLayoutUtils::IsNonWrapperBlock(frame)) {
    // XXX Is this problematic for relatively positioned inlines acting
    // as containing block for absolutely positioned elements?
    // Possibly; in that case we should at least be checking
    // NS_SUBTREE_DIRTY, I'd think.
    mFlags.mVResize = mCBReflowState->mFlags.mVResize;
  } else if (ComputedHeight() == NS_AUTOHEIGHT) {
    if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
        mCBReflowState) {
      mFlags.mVResize = mCBReflowState->mFlags.mVResize;
    } else {
      mFlags.mVResize = mFlags.mHResize;
    }
    mFlags.mVResize = mFlags.mVResize || NS_SUBTREE_DIRTY(frame);
  } else {
    // not 'auto' height
    mFlags.mVResize = frame->GetSize().height !=
                        ComputedHeight() + ComputedPhysicalBorderPadding().TopBottom();
  }

  bool dependsOnCBHeight =
    (mStylePosition->HeightDependsOnContainer() &&
     // FIXME: condition this on not-abspos?
     mStylePosition->mHeight.GetUnit() != eStyleUnit_Auto) ||
    mStylePosition->MinHeightDependsOnContainer() ||
    mStylePosition->MaxHeightDependsOnContainer() ||
    mStylePosition->OffsetHasPercent(NS_SIDE_TOP) ||
    mStylePosition->mOffset.GetBottomUnit() != eStyleUnit_Auto ||
    frame->IsBoxFrame();

  if (mStyleText->mLineHeight.GetUnit() == eStyleUnit_Enumerated) {
    NS_ASSERTION(mStyleText->mLineHeight.GetIntValue() ==
                 NS_STYLE_LINE_HEIGHT_BLOCK_HEIGHT,
                 "bad line-height value");

    // line-height depends on block height
    frame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
    // but only on containing blocks if this frame is not a suitable block
    dependsOnCBHeight |= !nsLayoutUtils::IsNonWrapperBlock(frame);
  }

  // If we're the descendant of a table cell that performs special height
  // reflows and we could be the child that requires them, always set
  // the vertical resize in case this is the first pass before the
  // special height reflow.  However, don't do this if it actually is
  // the special height reflow, since in that case it will already be
  // set correctly above if we need it set.
  if (!mFlags.mVResize && mCBReflowState &&
      (IS_TABLE_CELL(mCBReflowState->frame->GetType()) || 
       mCBReflowState->mFlags.mHeightDependsOnAncestorCell) &&
      !mCBReflowState->mFlags.mSpecialHeightReflow && 
      dependsOnCBHeight) {
    mFlags.mVResize = true;
    mFlags.mHeightDependsOnAncestorCell = true;
  }

  // Set NS_FRAME_CONTAINS_RELATIVE_HEIGHT if it's needed.

  // It would be nice to check that |mComputedHeight != NS_AUTOHEIGHT|
  // &&ed with the percentage height check.  However, this doesn't get
  // along with table special height reflows, since a special height
  // reflow (a quirk that makes such percentage heights work on children
  // of table cells) can cause not just a single percentage height to
  // become fixed, but an entire descendant chain of percentage heights
  // to become fixed.
  if (dependsOnCBHeight && mCBReflowState) {
    const nsHTMLReflowState *rs = this;
    bool hitCBReflowState = false;
    do {
      rs = rs->parentReflowState;
      if (!rs) {
        break;
      }
        
      if (rs->frame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT)
        break; // no need to go further
      rs->frame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
      
      // Keep track of whether we've hit the containing block, because
      // we need to go at least that far.
      if (rs == mCBReflowState) {
        hitCBReflowState = true;
      }

    } while (!hitCBReflowState ||
             (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
              !IsQuirkContainingBlockHeight(rs, rs->frame->GetType())));
    // Note: We actually don't need to set the
    // NS_FRAME_CONTAINS_RELATIVE_HEIGHT bit for the cases
    // where we hit the early break statements in
    // CalcQuirkContainingBlockHeight. But it doesn't hurt
    // us to set the bit in these cases.
    
  }
  if (frame->GetStateBits() & NS_FRAME_IS_DIRTY) {
    // If we're reflowing everything, then we'll find out if we need
    // to re-set this.
    frame->RemoveStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
  }
}

/* static */
nscoord
nsHTMLReflowState::GetContainingBlockContentWidth(const nsHTMLReflowState* aReflowState)
{
  const nsHTMLReflowState* rs = aReflowState->mCBReflowState;
  if (!rs)
    return 0;
  return rs->ComputedWidth();
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
    case NS_STYLE_DISPLAY_GRID:
      frameType = NS_CSS_FRAME_TYPE_BLOCK;
      break;

    case NS_STYLE_DISPLAY_INLINE:
    case NS_STYLE_DISPLAY_INLINE_BLOCK:
    case NS_STYLE_DISPLAY_INLINE_TABLE:
    case NS_STYLE_DISPLAY_INLINE_BOX:
    case NS_STYLE_DISPLAY_INLINE_XUL_GRID:
    case NS_STYLE_DISPLAY_INLINE_STACK:
    case NS_STYLE_DISPLAY_INLINE_FLEX:
    case NS_STYLE_DISPLAY_INLINE_GRID:
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
nsHTMLReflowState::ComputeRelativeOffsets(uint8_t aCBDirection,
                                          nsIFrame* aFrame,
                                          nscoord aContainingBlockWidth,
                                          nscoord aContainingBlockHeight,
                                          nsMargin& aComputedOffsets)
{
  const nsStylePosition* position = aFrame->StylePosition();

  // Compute the 'left' and 'right' values. 'Left' moves the boxes to the right,
  // and 'right' moves the boxes to the left. The computed values are always:
  // left=-right
  bool    leftIsAuto = eStyleUnit_Auto == position->mOffset.GetLeftUnit();
  bool    rightIsAuto = eStyleUnit_Auto == position->mOffset.GetRightUnit();

  // If neither 'left' not 'right' are auto, then we're over-constrained and
  // we ignore one of them
  if (!leftIsAuto && !rightIsAuto) {
    if (aCBDirection == NS_STYLE_DIRECTION_RTL) {
      leftIsAuto = true;
    } else {
      rightIsAuto = true;
    }
  }

  if (leftIsAuto) {
    if (rightIsAuto) {
      // If both are 'auto' (their initial values), the computed values are 0
      aComputedOffsets.left = aComputedOffsets.right = 0;
    } else {
      // 'Right' isn't 'auto' so compute its value
      aComputedOffsets.right = nsLayoutUtils::
        ComputeCBDependentValue(aContainingBlockWidth,
                                position->mOffset.GetRight());

      // Computed value for 'left' is minus the value of 'right'
      aComputedOffsets.left = -aComputedOffsets.right;
    }

  } else {
    NS_ASSERTION(rightIsAuto, "unexpected specified constraint");
    
    // 'Left' isn't 'auto' so compute its value
    aComputedOffsets.left = nsLayoutUtils::
      ComputeCBDependentValue(aContainingBlockWidth,
                              position->mOffset.GetLeft());

    // Computed value for 'right' is minus the value of 'left'
    aComputedOffsets.right = -aComputedOffsets.left;
  }

  // Compute the 'top' and 'bottom' values. The 'top' and 'bottom' properties
  // move relatively positioned elements up and down. They also must be each 
  // other's negative
  bool    topIsAuto = eStyleUnit_Auto == position->mOffset.GetTopUnit();
  bool    bottomIsAuto = eStyleUnit_Auto == position->mOffset.GetBottomUnit();

  // Check for percentage based values and a containing block height that
  // depends on the content height. Treat them like 'auto'
  if (NS_AUTOHEIGHT == aContainingBlockHeight) {
    if (position->OffsetHasPercent(NS_SIDE_TOP)) {
      topIsAuto = true;
    }
    if (position->OffsetHasPercent(NS_SIDE_BOTTOM)) {
      bottomIsAuto = true;
    }
  }

  // If neither is 'auto', 'bottom' is ignored
  if (!topIsAuto && !bottomIsAuto) {
    bottomIsAuto = true;
  }

  if (topIsAuto) {
    if (bottomIsAuto) {
      // If both are 'auto' (their initial values), the computed values are 0
      aComputedOffsets.top = aComputedOffsets.bottom = 0;
    } else {
      // 'Bottom' isn't 'auto' so compute its value
      aComputedOffsets.bottom = nsLayoutUtils::
        ComputeHeightDependentValue(aContainingBlockHeight,
                                    position->mOffset.GetBottom());
      
      // Computed value for 'top' is minus the value of 'bottom'
      aComputedOffsets.top = -aComputedOffsets.bottom;
    }

  } else {
    NS_ASSERTION(bottomIsAuto, "unexpected specified constraint");
    
    // 'Top' isn't 'auto' so compute its value
    aComputedOffsets.top = nsLayoutUtils::
      ComputeHeightDependentValue(aContainingBlockHeight,
                                  position->mOffset.GetTop());

    // Computed value for 'bottom' is minus the value of 'top'
    aComputedOffsets.bottom = -aComputedOffsets.top;
  }

  // Store the offset
  FrameProperties props = aFrame->Properties();
  nsMargin* offsets = static_cast<nsMargin*>
    (props.Get(nsIFrame::ComputedOffsetProperty()));
  if (offsets) {
    *offsets = aComputedOffsets;
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
nsHTMLReflowState::GetHypotheticalBoxContainer(nsIFrame* aFrame,
                                               nscoord& aCBLeftEdge,
                                               nscoord& aCBWidth)
{
  aFrame = aFrame->GetContainingBlock();
  NS_ASSERTION(aFrame != frame, "How did that happen?");

  /* Now aFrame is the containing block we want */

  /* Check whether the containing block is currently being reflowed.
     If so, use the info from the reflow state. */
  const nsHTMLReflowState* state;
  if (aFrame->GetStateBits() & NS_FRAME_IN_REFLOW) {
    for (state = parentReflowState; state && state->frame != aFrame;
         state = state->parentReflowState) {
      /* do nothing */
    }
  } else {
    state = nullptr;
  }
  
  if (state) {
    aCBLeftEdge = state->ComputedPhysicalBorderPadding().left;
    aCBWidth = state->ComputedWidth();
  } else {
    /* Didn't find a reflow state for aFrame.  Just compute the information we
       want, on the assumption that aFrame already knows its size.  This really
       ought to be true by now. */
    NS_ASSERTION(!(aFrame->GetStateBits() & NS_FRAME_IN_REFLOW),
                 "aFrame shouldn't be in reflow; we'll lie if it is");
    nsMargin borderPadding = aFrame->GetUsedBorderAndPadding();
    aCBLeftEdge = borderPadding.left;
    aCBWidth = aFrame->GetSize().width - borderPadding.LeftRight();
  }

  return aFrame;
}

// When determining the hypothetical box that would have been if the element
// had been in the flow we may not be able to exactly determine both the left
// and right edges. For example, if the element is a non-replaced inline-level
// element we would have to reflow it in order to determine it desired width.
// In that case depending on the progression direction either the left or
// right edge would be marked as not being exact
struct nsHypotheticalBox {
  // offsets from left edge of containing block (which is a padding edge)
  nscoord       mLeft, mRight;
  // offset from top edge of containing block (which is a padding edge)
  nscoord       mTop;
#ifdef DEBUG
  bool          mLeftIsExact, mRightIsExact;
#endif

  nsHypotheticalBox() {
#ifdef DEBUG
    mLeftIsExact = mRightIsExact = false;
#endif
  }
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
 * aInsideBoxSizing returns the part of the horizontal padding, border,
 * and margin that goes inside the edge given by box-sizing;
 * aOutsideBoxSizing returns the rest.
 */
void
nsHTMLReflowState::CalculateHorizBorderPaddingMargin(
                       nscoord aContainingBlockWidth,
                       nscoord* aInsideBoxSizing,
                       nscoord* aOutsideBoxSizing)
{
  const nsMargin& border = mStyleBorder->GetComputedBorder();
  nsMargin padding, margin;

  // See if the style system can provide us the padding directly
  if (!mStylePadding->GetPadding(padding)) {
    // We have to compute the left and right values
    padding.left = nsLayoutUtils::
      ComputeCBDependentValue(aContainingBlockWidth,
                              mStylePadding->mPadding.GetLeft());
    padding.right = nsLayoutUtils::
      ComputeCBDependentValue(aContainingBlockWidth,
                              mStylePadding->mPadding.GetRight());
  }

  // See if the style system can provide us the margin directly
  if (!mStyleMargin->GetMargin(margin)) {
    // We have to compute the left and right values
    if (eStyleUnit_Auto == mStyleMargin->mMargin.GetLeftUnit()) {
      // XXX FIXME (or does CalculateBlockSideMargins do this?)
      margin.left = 0;  // just ignore
    } else {
      margin.left = nsLayoutUtils::
        ComputeCBDependentValue(aContainingBlockWidth,
                                mStyleMargin->mMargin.GetLeft());
    }
    if (eStyleUnit_Auto == mStyleMargin->mMargin.GetRightUnit()) {
      // XXX FIXME (or does CalculateBlockSideMargins do this?)
      margin.right = 0;  // just ignore
    } else {
      margin.right = nsLayoutUtils::
        ComputeCBDependentValue(aContainingBlockWidth,
                                mStyleMargin->mMargin.GetRight());
    }
  }

  nscoord outside =
    padding.LeftRight() + border.LeftRight() + margin.LeftRight();
  nscoord inside = 0;
  switch (mStylePosition->mBoxSizing) {
    case NS_STYLE_BOX_SIZING_BORDER:
      inside += border.LeftRight();
      // fall through
    case NS_STYLE_BOX_SIZING_PADDING:
      inside += padding.LeftRight();
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
  for (nsIFrame* f = aFrame->GetFirstPrincipalChild(); f; f = f->GetNextSibling()) {
    bool allEmpty = AreAllEarlierInFlowFramesEmpty(f, aDescendant, aFound);
    if (*aFound || !allEmpty) {
      return allEmpty;
    }
  }
  *aFound = false;
  return true;
}

// Calculate the hypothetical box that the element would have if it were in
// the flow. The values returned are relative to the padding edge of the
// absolute containing block
// aContainingBlock is the placeholder's containing block (XXX rename it?)
// cbrs->frame is the actual containing block
void
nsHTMLReflowState::CalculateHypotheticalBox(nsPresContext*    aPresContext,
                                            nsIFrame*         aPlaceholderFrame,
                                            nsIFrame*         aContainingBlock,
                                            nscoord           aBlockLeftContentEdge,
                                            nscoord           aBlockContentWidth,
                                            const nsHTMLReflowState* cbrs,
                                            nsHypotheticalBox& aHypotheticalBox,
                                            nsIAtom*          aFrameType)
{
  NS_ASSERTION(mStyleDisplay->mOriginalDisplay != NS_STYLE_DISPLAY_NONE,
               "mOriginalDisplay has not been properly initialized");
  
  // If it's a replaced element and it has a 'auto' value for 'width', see if we
  // can get the intrinsic size. This will allow us to exactly determine both the
  // left and right edges
  bool isAutoWidth = mStylePosition->mWidth.GetUnit() == eStyleUnit_Auto;
  nsSize      intrinsicSize;
  bool        knowIntrinsicSize = false;
  if (NS_FRAME_IS_REPLACED(mFrameType) && isAutoWidth) {
    // See if we can get the intrinsic size of the element
    knowIntrinsicSize = GetIntrinsicSizeFor(frame, intrinsicSize, aFrameType);
  }

  // See if we can calculate what the box width would have been if the
  // element had been in the flow
  nscoord boxWidth;
  bool    knowBoxWidth = false;
  if ((NS_STYLE_DISPLAY_INLINE == mStyleDisplay->mOriginalDisplay) &&
      !NS_FRAME_IS_REPLACED(mFrameType)) {
    // For non-replaced inline-level elements the 'width' property doesn't apply,
    // so we don't know what the width would have been without reflowing it

  } else {
    // It's either a replaced inline-level element or a block-level element

    // Determine the total amount of horizontal border/padding/margin that
    // the element would have had if it had been in the flow. Note that we
    // ignore any 'auto' and 'inherit' values
    nscoord insideBoxSizing, outsideBoxSizing;
    CalculateHorizBorderPaddingMargin(aBlockContentWidth,
                                      &insideBoxSizing, &outsideBoxSizing);

    if (NS_FRAME_IS_REPLACED(mFrameType) && isAutoWidth) {
      // It's a replaced element with an 'auto' width so the box width is
      // its intrinsic size plus any border/padding/margin
      if (knowIntrinsicSize) {
        boxWidth = intrinsicSize.width + outsideBoxSizing + insideBoxSizing;
        knowBoxWidth = true;
      }

    } else if (isAutoWidth) {
      // The box width is the containing block width
      boxWidth = aBlockContentWidth;
      knowBoxWidth = true;
    
    } else {
      // We need to compute it. It's important we do this, because if it's
      // percentage based this computed value may be different from the computed
      // value calculated using the absolute containing block width
      boxWidth = ComputeWidthValue(aBlockContentWidth,
                                   insideBoxSizing, outsideBoxSizing,
                                   mStylePosition->mWidth) + 
                 insideBoxSizing + outsideBoxSizing;
      knowBoxWidth = true;
    }
  }
  
  // Get the 'direction' of the block
  const nsStyleVisibility* blockVis = aContainingBlock->StyleVisibility();

  // Get the placeholder x-offset and y-offset in the coordinate
  // space of its containing block
  // XXXbz the placeholder is not fully reflowed yet if our containing block is
  // relatively positioned...
  nsPoint placeholderOffset = aPlaceholderFrame->GetOffsetTo(aContainingBlock);

  // First, determine the hypothetical box's mTop.  We want to check the
  // content insertion frame of aContainingBlock for block-ness, but make
  // sure to compute all coordinates in the coordinate system of
  // aContainingBlock.
  nsBlockFrame* blockFrame =
    nsLayoutUtils::GetAsBlock(aContainingBlock->GetContentInsertionFrame());
  if (blockFrame) {
    nscoord blockYOffset = blockFrame->GetOffsetTo(aContainingBlock).y;
    bool isValid;
    nsBlockInFlowLineIterator iter(blockFrame, aPlaceholderFrame, &isValid);
    if (!isValid) {
      // Give up.  We're probably dealing with somebody using
      // position:absolute inside native-anonymous content anyway.
      aHypotheticalBox.mTop = placeholderOffset.y;
    } else {
      NS_ASSERTION(iter.GetContainer() == blockFrame,
                   "Found placeholder in wrong block!");
      nsBlockFrame::line_iterator lineBox = iter.GetLine();

      // How we determine the hypothetical box depends on whether the element
      // would have been inline-level or block-level
      if (mStyleDisplay->IsOriginalDisplayInlineOutsideStyle()) {
        // Use the top of the inline box which the placeholder lives in
        // as the hypothetical box's top.
        aHypotheticalBox.mTop = lineBox->GetPhysicalBounds().y + blockYOffset;
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
            aHypotheticalBox.mTop = lineBox->GetPhysicalBounds().y + blockYOffset;
          } else {
            // The top of the hypothetical box is just below the line
            // containing the placeholder.
            aHypotheticalBox.mTop = lineBox->GetPhysicalBounds().YMost() + blockYOffset;
          }
        } else {
          // Just use the placeholder's y-offset wrt the containing block
          aHypotheticalBox.mTop = placeholderOffset.y;
        }
      }
    }
  } else {
    // The containing block is not a block, so it's probably something
    // like a XUL box, etc.
    // Just use the placeholder's y-offset
    aHypotheticalBox.mTop = placeholderOffset.y;
  }

  // Second, determine the hypothetical box's mLeft & mRight
  // To determine the left and right offsets we need to look at the block's 'direction'
  if (NS_STYLE_DIRECTION_LTR == blockVis->mDirection) {
    // How we determine the hypothetical box depends on whether the element
    // would have been inline-level or block-level
    if (mStyleDisplay->IsOriginalDisplayInlineOutsideStyle()) {
      // The placeholder represents the left edge of the hypothetical box
      aHypotheticalBox.mLeft = placeholderOffset.x;
    } else {
      aHypotheticalBox.mLeft = aBlockLeftContentEdge;
    }
#ifdef DEBUG
    aHypotheticalBox.mLeftIsExact = true;
#endif

    if (knowBoxWidth) {
      aHypotheticalBox.mRight = aHypotheticalBox.mLeft + boxWidth;
#ifdef DEBUG
      aHypotheticalBox.mRightIsExact = true;
#endif
    } else {
      // We can't compute the right edge because we don't know the desired
      // width. So instead use the right content edge of the block parent,
      // but remember it's not exact
      aHypotheticalBox.mRight = aBlockLeftContentEdge + aBlockContentWidth;
#ifdef DEBUG
      aHypotheticalBox.mRightIsExact = false;
#endif
    }

  } else {
    // The placeholder represents the right edge of the hypothetical box
    if (mStyleDisplay->IsOriginalDisplayInlineOutsideStyle()) {
      aHypotheticalBox.mRight = placeholderOffset.x;
    } else {
      aHypotheticalBox.mRight = aBlockLeftContentEdge + aBlockContentWidth;
    }
#ifdef DEBUG
    aHypotheticalBox.mRightIsExact = true;
#endif
    
    if (knowBoxWidth) {
      aHypotheticalBox.mLeft = aHypotheticalBox.mRight - boxWidth;
#ifdef DEBUG
      aHypotheticalBox.mLeftIsExact = true;
#endif
    } else {
      // We can't compute the left edge because we don't know the desired
      // width. So instead use the left content edge of the block parent,
      // but remember it's not exact
      aHypotheticalBox.mLeft = aBlockLeftContentEdge;
#ifdef DEBUG
      aHypotheticalBox.mLeftIsExact = false;
#endif
    }

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
    // In this case, cbrs->frame will always be an ancestor of
    // aContainingBlock, so can just walk our way up the frame tree.
    // Make sure to not add positions of frames whose parent is a
    // scrollFrame, since we're doing fixed positioning, which assumes
    // everything is scrolled to (0,0).
    cbOffset.MoveTo(0, 0);
    do {
      NS_ASSERTION(aContainingBlock,
                   "Should hit cbrs->frame before we run off the frame tree!");
      cbOffset += aContainingBlock->GetPositionIgnoringScrolling();
      aContainingBlock = aContainingBlock->GetParent();
    } while (aContainingBlock != cbrs->frame);
  } else {
    // XXXldb We need to either ignore scrolling for the absolute
    // positioning case too (and take the incompatibility) or figure out
    // how to make these positioned elements actually *move* when we
    // scroll, and thus avoid the resulting incremental reflow bugs.
    cbOffset = aContainingBlock->GetOffsetTo(cbrs->frame);
  }
  aHypotheticalBox.mLeft += cbOffset.x;
  aHypotheticalBox.mTop += cbOffset.y;
  aHypotheticalBox.mRight += cbOffset.x;
  
  // The specified offsets are relative to the absolute containing block's
  // padding edge and our current values are relative to the border edge, so
  // translate.
  nsMargin border = cbrs->ComputedPhysicalBorderPadding() - cbrs->ComputedPhysicalPadding();
  aHypotheticalBox.mLeft -= border.left;
  aHypotheticalBox.mRight -= border.left;
  aHypotheticalBox.mTop -= border.top;
}

void
nsHTMLReflowState::InitAbsoluteConstraints(nsPresContext* aPresContext,
                                           const nsHTMLReflowState* cbrs,
                                           nscoord containingBlockWidth,
                                           nscoord containingBlockHeight,
                                           nsIAtom* aFrameType)
{
  NS_PRECONDITION(containingBlockHeight != NS_AUTOHEIGHT,
                  "containing block height must be constrained");

  NS_ASSERTION(aFrameType != nsGkAtoms::tableFrame,
               "InitAbsoluteConstraints should not be called on table frames");
  NS_ASSERTION(frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW,
               "Why are we here?");

  // Get the placeholder frame
  nsIFrame*     placeholderFrame;

  placeholderFrame = aPresContext->PresShell()->GetPlaceholderFrameFor(frame);
  NS_ASSERTION(nullptr != placeholderFrame, "no placeholder frame");

  // If both 'left' and 'right' are 'auto' or both 'top' and 'bottom' are
  // 'auto', then compute the hypothetical box of where the element would
  // have been if it had been in the flow
  nsHypotheticalBox hypotheticalBox;
  if (((eStyleUnit_Auto == mStylePosition->mOffset.GetLeftUnit()) &&
       (eStyleUnit_Auto == mStylePosition->mOffset.GetRightUnit())) ||
      ((eStyleUnit_Auto == mStylePosition->mOffset.GetTopUnit()) &&
       (eStyleUnit_Auto == mStylePosition->mOffset.GetBottomUnit()))) {
    // Find the nearest containing block frame to the placeholder frame,
    // and return its left edge and width.
    nscoord cbLeftEdge, cbWidth;
    nsIFrame* cbFrame = GetHypotheticalBoxContainer(placeholderFrame,
                                                    cbLeftEdge,
                                                    cbWidth);

    CalculateHypotheticalBox(aPresContext, placeholderFrame, cbFrame,
                             cbLeftEdge, cbWidth, cbrs, hypotheticalBox, aFrameType);
  }

  // Initialize the 'left' and 'right' computed offsets
  // XXX Handle new 'static-position' value...
  bool          leftIsAuto = false, rightIsAuto = false;
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetLeftUnit()) {
    ComputedPhysicalOffsets().left = 0;
    leftIsAuto = true;
  } else {
    ComputedPhysicalOffsets().left = nsLayoutUtils::
      ComputeCBDependentValue(containingBlockWidth,
                              mStylePosition->mOffset.GetLeft());
  }
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetRightUnit()) {
    ComputedPhysicalOffsets().right = 0;
    rightIsAuto = true;
  } else {
    ComputedPhysicalOffsets().right = nsLayoutUtils::
      ComputeCBDependentValue(containingBlockWidth,
                              mStylePosition->mOffset.GetRight());
  }

  // Use the horizontal component of the hypothetical box in the cases
  // where it's needed.
  if (leftIsAuto && rightIsAuto) {
    // Use the direction of the original ("static-position") containing block
    // to dictate whether 'left' or 'right' is treated like 'static-position'.
    if (NS_STYLE_DIRECTION_LTR == placeholderFrame->GetContainingBlock()
                                    ->StyleVisibility()->mDirection) {
      NS_ASSERTION(hypotheticalBox.mLeftIsExact, "should always have "
                   "exact value on containing block's start side");
      ComputedPhysicalOffsets().left = hypotheticalBox.mLeft;
      leftIsAuto = false;
    } else {
      NS_ASSERTION(hypotheticalBox.mRightIsExact, "should always have "
                   "exact value on containing block's start side");
      ComputedPhysicalOffsets().right = containingBlockWidth - hypotheticalBox.mRight;
      rightIsAuto = false;
    }
  }

  // Initialize the 'top' and 'bottom' computed offsets
  bool        topIsAuto = false, bottomIsAuto = false;
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetTopUnit()) {
    ComputedPhysicalOffsets().top = 0;
    topIsAuto = true;
  } else {
    ComputedPhysicalOffsets().top = nsLayoutUtils::
      ComputeHeightDependentValue(containingBlockHeight,
                                  mStylePosition->mOffset.GetTop());
  }
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetBottomUnit()) {
    ComputedPhysicalOffsets().bottom = 0;        
    bottomIsAuto = true;
  } else {
    ComputedPhysicalOffsets().bottom = nsLayoutUtils::
      ComputeHeightDependentValue(containingBlockHeight,
                                  mStylePosition->mOffset.GetBottom());
  }

  if (topIsAuto && bottomIsAuto) {
    // Treat 'top' like 'static-position'
    ComputedPhysicalOffsets().top = hypotheticalBox.mTop;
    topIsAuto = false;
  }

  bool widthIsAuto = eStyleUnit_Auto == mStylePosition->mWidth.GetUnit();
  bool heightIsAuto = eStyleUnit_Auto == mStylePosition->mHeight.GetUnit();

  uint32_t computeSizeFlags = 0;
  if (leftIsAuto || rightIsAuto) {
    computeSizeFlags |= nsIFrame::eShrinkWrap;
  }

  {
    AutoMaybeDisableFontInflation an(frame);

    WritingMode wm = GetWritingMode();
    // Size of the containing block in our writing mode
    LogicalSize cbSize(wm, nsSize(containingBlockWidth, containingBlockHeight));
    LogicalSize size =
      frame->ComputeSize(rendContext, wm, cbSize,
                         cbSize.ISize(wm), // XXX or AvailableISize()?
                         ComputedLogicalMargin().Size(wm) +
                           ComputedLogicalOffsets().Size(wm),
                         ComputedLogicalBorderPadding().Size(wm) -
                           ComputedLogicalPadding().Size(wm),
                         ComputedLogicalPadding().Size(wm),
                         computeSizeFlags);
    ComputedISize() = size.ISize(wm);
    ComputedBSize() = size.BSize(wm);
  }
  NS_ASSERTION(ComputedISize() >= 0, "Bogus inline-size");
  NS_ASSERTION(ComputedBSize() == NS_UNCONSTRAINEDSIZE ||
               ComputedBSize() >= 0, "Bogus block-size");

  // XXX Now that we have ComputeSize, can we condense many of the
  // branches off of widthIsAuto?

  if (leftIsAuto) {
    // We know 'right' is not 'auto' anymore thanks to the hypothetical
    // box code above.
    // Solve for 'left'.
    if (widthIsAuto) {
      // XXXldb This, and the corresponding code in
      // nsAbsoluteContainingBlock.cpp, could probably go away now that
      // we always compute widths.
      ComputedPhysicalOffsets().left = NS_AUTOOFFSET;
    } else {
      ComputedPhysicalOffsets().left = containingBlockWidth - ComputedPhysicalMargin().left -
        ComputedPhysicalBorderPadding().left - ComputedWidth() - ComputedPhysicalBorderPadding().right - 
        ComputedPhysicalMargin().right - ComputedPhysicalOffsets().right;

    }
  } else if (rightIsAuto) {
    // We know 'left' is not 'auto' anymore thanks to the hypothetical
    // box code above.
    // Solve for 'right'.
    if (widthIsAuto) {
      // XXXldb This, and the corresponding code in
      // nsAbsoluteContainingBlock.cpp, could probably go away now that
      // we always compute widths.
      ComputedPhysicalOffsets().right = NS_AUTOOFFSET;
    } else {
      ComputedPhysicalOffsets().right = containingBlockWidth - ComputedPhysicalOffsets().left -
        ComputedPhysicalMargin().left - ComputedPhysicalBorderPadding().left - ComputedWidth() -
        ComputedPhysicalBorderPadding().right - ComputedPhysicalMargin().right;
    }
  } else {
    // Neither 'left' nor 'right' is 'auto'.  However, the width might
    // still not fill all the available space (even though we didn't
    // shrink-wrap) in case:
    //  * width was specified
    //  * we're dealing with a replaced element
    //  * width was constrained by min-width or max-width.

    nscoord availMarginSpace = containingBlockWidth -
                               ComputedPhysicalOffsets().LeftRight() -
                               ComputedPhysicalMargin().LeftRight() -
                               ComputedPhysicalBorderPadding().LeftRight() -
                               ComputedWidth();
    bool marginLeftIsAuto =
      eStyleUnit_Auto == mStyleMargin->mMargin.GetLeftUnit();
    bool marginRightIsAuto =
      eStyleUnit_Auto == mStyleMargin->mMargin.GetRightUnit();

    if (marginLeftIsAuto) {
      if (marginRightIsAuto) {
        if (availMarginSpace < 0) {
          // Note that this case is different from the neither-'auto'
          // case below, where the spec says to ignore 'left'/'right'.
          if (cbrs &&
              NS_STYLE_DIRECTION_RTL == cbrs->mStyleVisibility->mDirection) {
            // Ignore the specified value for 'margin-left'.
            ComputedPhysicalMargin().left = availMarginSpace;
          } else {
            // Ignore the specified value for 'margin-right'.
            ComputedPhysicalMargin().right = availMarginSpace;
          }
        } else {
          // Both 'margin-left' and 'margin-right' are 'auto', so they get
          // equal values
          ComputedPhysicalMargin().left = availMarginSpace / 2;
          ComputedPhysicalMargin().right = availMarginSpace - ComputedPhysicalMargin().left;
        }
      } else {
        // Just 'margin-left' is 'auto'
        ComputedPhysicalMargin().left = availMarginSpace;
      }
    } else {
      if (marginRightIsAuto) {
        // Just 'margin-right' is 'auto'
        ComputedPhysicalMargin().right = availMarginSpace;
      } else {
        // We're over-constrained so use the direction of the containing
        // block to dictate which value to ignore.  (And note that the
        // spec says to ignore 'left' or 'right' rather than
        // 'margin-left' or 'margin-right'.)
        // Note that this case is different from the both-'auto' case
        // above, where the spec says to ignore
        // 'margin-left'/'margin-right'.
        if (cbrs &&
            NS_STYLE_DIRECTION_RTL == cbrs->mStyleVisibility->mDirection) {
          // Ignore the specified value for 'left'.
          ComputedPhysicalOffsets().left += availMarginSpace;
        } else {
          // Ignore the specified value for 'right'.
          ComputedPhysicalOffsets().right += availMarginSpace;
        }
      }
    }
  }

  if (topIsAuto) {
    // solve for 'top'
    if (heightIsAuto) {
      ComputedPhysicalOffsets().top = NS_AUTOOFFSET;
    } else {
      ComputedPhysicalOffsets().top = containingBlockHeight - ComputedPhysicalMargin().top -
        ComputedPhysicalBorderPadding().top - ComputedHeight() - ComputedPhysicalBorderPadding().bottom - 
        ComputedPhysicalMargin().bottom - ComputedPhysicalOffsets().bottom;
    }
  } else if (bottomIsAuto) {
    // solve for 'bottom'
    if (heightIsAuto) {
      ComputedPhysicalOffsets().bottom = NS_AUTOOFFSET;
    } else {
      ComputedPhysicalOffsets().bottom = containingBlockHeight - ComputedPhysicalOffsets().top -
        ComputedPhysicalMargin().top - ComputedPhysicalBorderPadding().top - ComputedHeight() -
        ComputedPhysicalBorderPadding().bottom - ComputedPhysicalMargin().bottom;
    }
  } else {
    // Neither 'top' nor 'bottom' is 'auto'.
    nscoord autoHeight = containingBlockHeight -
                         ComputedPhysicalOffsets().TopBottom() -
                         ComputedPhysicalMargin().TopBottom() -
                         ComputedPhysicalBorderPadding().TopBottom();
    if (autoHeight < 0) {
      autoHeight = 0;
    }

    if (ComputedHeight() == NS_UNCONSTRAINEDSIZE) {
      // For non-replaced elements with 'height' auto, the 'height'
      // fills the remaining space.
      ComputedHeight() = autoHeight;

      // XXX Do these need box-sizing adjustments?
      if (ComputedHeight() > ComputedMaxHeight())
        ComputedHeight() = ComputedMaxHeight();
      if (ComputedHeight() < ComputedMinHeight())
        ComputedHeight() = ComputedMinHeight();
    }

    // The height might still not fill all the available space in case:
    //  * height was specified
    //  * we're dealing with a replaced element
    //  * height was constrained by min-height or max-height.
    nscoord availMarginSpace = autoHeight - ComputedHeight();
    bool marginTopIsAuto =
      eStyleUnit_Auto == mStyleMargin->mMargin.GetTopUnit();
    bool marginBottomIsAuto =
      eStyleUnit_Auto == mStyleMargin->mMargin.GetBottomUnit();

    if (marginTopIsAuto) {
      if (marginBottomIsAuto) {
        if (availMarginSpace < 0) {
          // FIXME: Note that the spec doesn't actually say we should do this!
          ComputedPhysicalMargin().bottom = availMarginSpace;
        } else {
          // Both 'margin-top' and 'margin-bottom' are 'auto', so they get
          // equal values
          ComputedPhysicalMargin().top = availMarginSpace / 2;
          ComputedPhysicalMargin().bottom = availMarginSpace - ComputedPhysicalMargin().top;
        }
      } else {
        // Just 'margin-top' is 'auto'
        ComputedPhysicalMargin().top = availMarginSpace;
      }
    } else {
      if (marginBottomIsAuto) {
        // Just 'margin-bottom' is 'auto'
        ComputedPhysicalMargin().bottom = availMarginSpace;
      } else {
        // We're over-constrained so ignore the specified value for
        // 'bottom'.  (And note that the spec says to ignore 'bottom'
        // rather than 'margin-bottom'.)
        ComputedPhysicalOffsets().bottom += availMarginSpace;
      }
    }
  }
}

nscoord 
GetVerticalMarginBorderPadding(const nsHTMLReflowState* aReflowState)
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
 *  When we encounter scrolledContent block frames, we skip over them, since they are guaranteed to not be useful for computing the containing block.
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
  for (; rs; rs = rs->parentReflowState) {
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

      result -= GetVerticalMarginBorderPadding(firstAncestorRS); 
      result -= GetVerticalMarginBorderPadding(secondAncestorRS); 

#ifdef DEBUG
      // make sure the first ancestor is the HTML and the second is the BODY
      if (firstAncestorRS) {
        nsIContent* frameContent = firstAncestorRS->frame->GetContent();
        if (frameContent) {
          nsIAtom *contentTag = frameContent->Tag();
          NS_ASSERTION(contentTag == nsGkAtoms::html, "First ancestor is not HTML");
        }
      }
      if (secondAncestorRS) {
        nsIContent* frameContent = secondAncestorRS->frame->GetContent();
        if (frameContent) {
          nsIAtom *contentTag = frameContent->Tag();
          NS_ASSERTION(contentTag == nsGkAtoms::body, "Second ancestor is not BODY");
        }
      }
#endif
      
    }
    // if we got to the html frame (a block child of the canvas) ...
    else if (nsGkAtoms::blockFrame == frameType &&
             rs->parentReflowState &&
             nsGkAtoms::canvasFrame ==
               rs->parentReflowState->frame->GetType()) {
      // ... then subtract out margin/border/padding for the BODY element
      result -= GetVerticalMarginBorderPadding(secondAncestorRS);
    }
    break;
  }

  // Make sure not to return a negative height here!
  return std::max(result, 0);
}

// Called by InitConstraints() to compute the containing block rectangle for
// the element. Handles the special logic for absolutely positioned elements
void
nsHTMLReflowState::ComputeContainingBlockRectangle(nsPresContext*          aPresContext,
                                                   const nsHTMLReflowState* aContainingBlockRS,
                                                   nscoord&                 aContainingBlockWidth,
                                                   nscoord&                 aContainingBlockHeight)
{
  // Unless the element is absolutely positioned, the containing block is
  // formed by the content edge of the nearest block-level ancestor
  aContainingBlockWidth = aContainingBlockRS->ComputedWidth();
  aContainingBlockHeight = aContainingBlockRS->ComputedHeight();

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
      nsMargin computedBorder = aContainingBlockRS->ComputedPhysicalBorderPadding() -
        aContainingBlockRS->ComputedPhysicalPadding();
      aContainingBlockWidth = aContainingBlockRS->frame->GetRect().width -
        computedBorder.LeftRight();
      NS_ASSERTION(aContainingBlockWidth >= 0,
                   "Negative containing block width!");
      aContainingBlockHeight = aContainingBlockRS->frame->GetRect().height -
        computedBorder.TopBottom();
      NS_ASSERTION(aContainingBlockHeight >= 0,
                   "Negative containing block height!");
    } else {
      // If the ancestor is block-level, the containing block is formed by the
      // padding edge of the ancestor
      aContainingBlockWidth += aContainingBlockRS->ComputedPhysicalPadding().LeftRight();
      aContainingBlockHeight += aContainingBlockRS->ComputedPhysicalPadding().TopBottom();
    }
  } else {
    // an element in quirks mode gets a containing block based on looking for a
    // parent with a non-auto height if the element has a percent height
    // Note: We don't emulate this quirk for percents in calc().
    if (NS_AUTOHEIGHT == aContainingBlockHeight) {
      if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
          mStylePosition->mHeight.GetUnit() == eStyleUnit_Percent) {
        aContainingBlockHeight = CalcQuirkContainingBlockHeight(aContainingBlockRS);
      }
    }
  }
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
IsSideCaption(nsIFrame* aFrame, const nsStyleDisplay* aStyleDisplay)
{
  if (aStyleDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE_CAPTION)
    return false;
  uint8_t captionSide = aFrame->StyleTableBorder()->mCaptionSide;
  return captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
         captionSide == NS_STYLE_CAPTION_SIDE_RIGHT;
}

static nsFlexContainerFrame*
GetFlexContainer(nsIFrame* aFrame)
{
  nsIFrame* parent = aFrame->GetParent();
  if (!parent ||
      parent->GetType() != nsGkAtoms::flexContainerFrame) {
    return nullptr;
  }

  return static_cast<nsFlexContainerFrame*>(parent);
}

// Flex items resolve percentage margin & padding against the flex
// container's height (which is the containing block height).
// For everything else: the CSS21 spec requires that margin and padding
// percentage values are calculated with respect to the *width* of the
// containing block, even for margin & padding in the vertical axis.
static nscoord
VerticalOffsetPercentBasis(const nsIFrame* aFrame,
                           nscoord aContainingBlockWidth,
                           nscoord aContainingBlockHeight)
{
  if (!aFrame->IsFlexOrGridItem()) {
    return aContainingBlockWidth;
  }

  if (aContainingBlockHeight == NS_AUTOHEIGHT) {
    return 0;
  }

  return aContainingBlockHeight;
}

// XXX refactor this code to have methods for each set of properties
// we are computing: width,height,line-height; margin; offsets

void
nsHTMLReflowState::InitConstraints(nsPresContext* aPresContext,
                                   nscoord         aContainingBlockWidth,
                                   nscoord         aContainingBlockHeight,
                                   const nsMargin* aBorder,
                                   const nsMargin* aPadding,
                                   nsIAtom* aFrameType)
{
  DISPLAY_INIT_CONSTRAINTS(frame, this,
                           aContainingBlockWidth, aContainingBlockHeight,
                           aBorder, aPadding);

  // If this is a reflow root, then set the computed width and
  // height equal to the available space
  if (nullptr == parentReflowState || mFlags.mDummyParentReflowState) {
    // XXXldb This doesn't mean what it used to!
    InitOffsets(aContainingBlockWidth,
                VerticalOffsetPercentBasis(frame, aContainingBlockWidth,
                                           aContainingBlockHeight),
                aFrameType, aBorder, aPadding);
    // Override mComputedMargin since reflow roots start from the
    // frame's boundary, which is inside the margin.
    ComputedPhysicalMargin().SizeTo(0, 0, 0, 0);
    ComputedPhysicalOffsets().SizeTo(0, 0, 0, 0);

    ComputedWidth() = AvailableWidth() - ComputedPhysicalBorderPadding().LeftRight();
    if (ComputedWidth() < 0)
      ComputedWidth() = 0;
    if (AvailableHeight() != NS_UNCONSTRAINEDSIZE) {
      ComputedHeight() = AvailableHeight() - ComputedPhysicalBorderPadding().TopBottom();
      if (ComputedHeight() < 0)
        ComputedHeight() = 0;
    } else {
      ComputedHeight() = NS_UNCONSTRAINEDSIZE;
    }

    ComputedMinWidth() = ComputedMinHeight() = 0;
    ComputedMaxWidth() = ComputedMaxHeight() = NS_UNCONSTRAINEDSIZE;
  } else {
    // Get the containing block reflow state
    const nsHTMLReflowState* cbrs = mCBReflowState;
    NS_ASSERTION(nullptr != cbrs, "no containing block");

    // If we weren't given a containing block width and height, then
    // compute one
    if (aContainingBlockWidth == -1) {
      ComputeContainingBlockRectangle(aPresContext, cbrs, aContainingBlockWidth, 
                                      aContainingBlockHeight);
    }

    // See if the containing block height is based on the size of its
    // content
    nsIAtom* fType;
    if (NS_AUTOHEIGHT == aContainingBlockHeight) {
      // See if the containing block is a cell frame which needs
      // to use the mComputedHeight of the cell instead of what the cell block passed in.
      // XXX It seems like this could lead to bugs with min-height and friends
      if (cbrs->parentReflowState) {
        fType = cbrs->frame->GetType();
        if (IS_TABLE_CELL(fType)) {
          // use the cell's computed height 
          aContainingBlockHeight = cbrs->ComputedHeight();
        }
      }
    }

    // XXX Might need to also pass the CB height (not width) for page boxes,
    // too, if we implement them.
    InitOffsets(aContainingBlockWidth,
                VerticalOffsetPercentBasis(frame, aContainingBlockWidth,
                                           aContainingBlockHeight),
                aFrameType, aBorder, aPadding);

    const nsStyleCoord &height = mStylePosition->mHeight;
    nsStyleUnit heightUnit = height.GetUnit();

    // Check for a percentage based height and a containing block height
    // that depends on the content height
    // XXX twiddling heightUnit doesn't help anymore
    // FIXME Shouldn't we fix that?
    if (height.HasPercent()) {
      if (NS_AUTOHEIGHT == aContainingBlockHeight) {
        // this if clause enables %-height on replaced inline frames,
        // such as images.  See bug 54119.  The else clause "heightUnit = eStyleUnit_Auto;"
        // used to be called exclusively.
        if (NS_FRAME_REPLACED(NS_CSS_FRAME_TYPE_INLINE) == mFrameType ||
            NS_FRAME_REPLACED_CONTAINS_BLOCK(
                NS_CSS_FRAME_TYPE_INLINE) == mFrameType) {
          // Get the containing block reflow state
          NS_ASSERTION(nullptr != cbrs, "no containing block");
          // in quirks mode, get the cb height using the special quirk method
          if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode()) {
            if (!IS_TABLE_CELL(fType)) {
              aContainingBlockHeight = CalcQuirkContainingBlockHeight(cbrs);
              if (aContainingBlockHeight == NS_AUTOHEIGHT) {
                heightUnit = eStyleUnit_Auto;
              }
            }
            else {
              heightUnit = eStyleUnit_Auto;
            }
          }
          // in standard mode, use the cb height.  if it's "auto", as will be the case
          // by default in BODY, use auto height as per CSS2 spec.
          else 
          {
            if (NS_AUTOHEIGHT != cbrs->ComputedHeight())
              aContainingBlockHeight = cbrs->ComputedHeight();
            else
              heightUnit = eStyleUnit_Auto;
          }
        }
        else {
          // default to interpreting the height like 'auto'
          heightUnit = eStyleUnit_Auto;
        }
      }
    }

    // Compute our offsets if the element is relatively positioned.  We need
    // the correct containing block width and height here, which is why we need
    // to do it after all the quirks-n-such above. (If the element is sticky
    // positioned, we need to wait until the scroll container knows its size,
    // so we compute offsets from StickyScrollContainer::UpdatePositions.)
    if (mStyleDisplay->IsRelativelyPositioned(frame) &&
        NS_STYLE_POSITION_RELATIVE == mStyleDisplay->mPosition) {
      uint8_t direction = NS_STYLE_DIRECTION_LTR;
      if (cbrs && NS_STYLE_DIRECTION_RTL == cbrs->mStyleVisibility->mDirection) {
        direction = NS_STYLE_DIRECTION_RTL;
      }
      ComputeRelativeOffsets(direction, frame, aContainingBlockWidth,
          aContainingBlockHeight, ComputedPhysicalOffsets());
    } else {
      // Initialize offsets to 0
      ComputedPhysicalOffsets().SizeTo(0, 0, 0, 0);
    }

    // Calculate the computed values for min and max properties.  Note that
    // this MUST come after we've computed our border and padding.
    ComputeMinMaxValues(aContainingBlockWidth, aContainingBlockHeight, cbrs);

    // Calculate the computed width and height. This varies by frame type

    if (NS_CSS_FRAME_TYPE_INTERNAL_TABLE == mFrameType) {
      // Internal table elements. The rules vary depending on the type.
      // Calculate the computed width
      bool rowOrRowGroup = false;
      const nsStyleCoord &width = mStylePosition->mWidth;
      nsStyleUnit widthUnit = width.GetUnit();
      if ((NS_STYLE_DISPLAY_TABLE_ROW == mStyleDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == mStyleDisplay->mDisplay)) {
        // 'width' property doesn't apply to table rows and row groups
        widthUnit = eStyleUnit_Auto;
        rowOrRowGroup = true;
      }

      // calc() with percentages acts like auto on internal table elements
      if (eStyleUnit_Auto == widthUnit ||
          (width.IsCalcUnit() && width.CalcHasPercent())) {
        ComputedWidth() = AvailableWidth();

        if ((ComputedWidth() != NS_UNCONSTRAINEDSIZE) && !rowOrRowGroup){
          // Internal table elements don't have margins. Only tables and
          // cells have border and padding
          ComputedWidth() -= ComputedPhysicalBorderPadding().left +
            ComputedPhysicalBorderPadding().right;
          if (ComputedWidth() < 0)
            ComputedWidth() = 0;
        }
        NS_ASSERTION(ComputedWidth() >= 0, "Bogus computed width");
      
      } else {
        NS_ASSERTION(widthUnit == mStylePosition->mWidth.GetUnit(),
                     "unexpected width unit change");
        ComputedWidth() = ComputeWidthValue(aContainingBlockWidth,
                                           mStylePosition->mBoxSizing,
                                           mStylePosition->mWidth);
      }

      // Calculate the computed height
      if ((NS_STYLE_DISPLAY_TABLE_COLUMN == mStyleDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == mStyleDisplay->mDisplay)) {
        // 'height' property doesn't apply to table columns and column groups
        heightUnit = eStyleUnit_Auto;
      }
      // calc() with percentages acts like 'auto' on internal table elements
      if (eStyleUnit_Auto == heightUnit ||
          (height.IsCalcUnit() && height.CalcHasPercent())) {
        ComputedHeight() = NS_AUTOHEIGHT;
      } else {
        NS_ASSERTION(heightUnit == mStylePosition->mHeight.GetUnit(),
                     "unexpected height unit change");
        ComputedHeight() = ComputeHeightValue(aContainingBlockHeight, 
                                             mStylePosition->mBoxSizing,
                                             mStylePosition->mHeight);
      }

      // Doesn't apply to table elements
      ComputedMinWidth() = ComputedMinHeight() = 0;
      ComputedMaxWidth() = ComputedMaxHeight() = NS_UNCONSTRAINEDSIZE;

    } else if (NS_FRAME_GET_TYPE(mFrameType) == NS_CSS_FRAME_TYPE_ABSOLUTE) {
      // XXX not sure if this belongs here or somewhere else - cwk
      InitAbsoluteConstraints(aPresContext, cbrs, aContainingBlockWidth,
                              aContainingBlockHeight, aFrameType);
    } else {
      AutoMaybeDisableFontInflation an(frame);

      bool isBlock = NS_CSS_FRAME_TYPE_BLOCK == NS_FRAME_GET_TYPE(mFrameType);
      uint32_t computeSizeFlags = isBlock ? 0 : nsIFrame::eShrinkWrap;

      // Make sure legend frames with display:block and width:auto still
      // shrink-wrap.
      if (isBlock &&
          ((aFrameType == nsGkAtoms::legendFrame &&
            frame->StyleContext()->GetPseudo() != nsCSSAnonBoxes::scrolledContent) ||
           (aFrameType == nsGkAtoms::scrollFrame &&
            frame->GetContentInsertionFrame()->GetType() == nsGkAtoms::legendFrame))) {
        computeSizeFlags |= nsIFrame::eShrinkWrap;
      }

      const nsFlexContainerFrame* flexContainerFrame = GetFlexContainer(frame);
      if (flexContainerFrame) {
        computeSizeFlags |= nsIFrame::eShrinkWrap;

        // If we're inside of a flex container that needs to measure our
        // auto height, pass that information along to ComputeSize().
        if (mFlags.mIsFlexContainerMeasuringHeight) {
          computeSizeFlags |= nsIFrame::eUseAutoHeight;
        }
      } else {
        MOZ_ASSERT(!mFlags.mIsFlexContainerMeasuringHeight,
                   "We're not in a flex container, so the flag "
                   "'mIsFlexContainerMeasuringHeight' shouldn't be set");
      }

      WritingMode wm = GetWritingMode();
      LogicalSize size =
        frame->ComputeSize(rendContext, wm,
                           LogicalSize(wm, nsSize(aContainingBlockWidth,
                                                  aContainingBlockHeight)),
                           AvailableISize(),
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

      // Exclude inline tables and flex items from the block margin calculations
      if (isBlock &&
          !IsSideCaption(frame, mStyleDisplay) &&
          mStyleDisplay->mDisplay != NS_STYLE_DISPLAY_INLINE_TABLE &&
          !flexContainerFrame) {
        CalculateBlockSideMargins(AvailableWidth(), ComputedWidth(), aFrameType);
      }
    }
  }
}

static void
UpdateProp(FrameProperties& aProps,
           const FramePropertyDescriptor* aProperty,
           bool aNeeded,
           nsMargin& aNewValue)
{
  if (aNeeded) {
    nsMargin* propValue = static_cast<nsMargin*>(aProps.Get(aProperty));
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
nsCSSOffsetState::InitOffsets(nscoord aHorizontalPercentBasis,
                              nscoord aVerticalPercentBasis,
                              nsIAtom* aFrameType,
                              const nsMargin *aBorder,
                              const nsMargin *aPadding)
{
  DISPLAY_INIT_OFFSETS(frame, this,
                       aHorizontalPercentBasis,
                       aVerticalPercentBasis,
                       aBorder, aPadding);

  // Since we are in reflow, we don't need to store these properties anymore
  // unless they are dependent on width, in which case we store the new value.
  nsPresContext *presContext = frame->PresContext();
  FrameProperties props(presContext->PropertyTable(), frame);
  props.Delete(nsIFrame::UsedBorderProperty());

  // Compute margins from the specified margin style information. These
  // become the default computed values, and may be adjusted below
  // XXX fix to provide 0,0 for the top&bottom margins for
  // inline-non-replaced elements
  bool needMarginProp = ComputeMargin(aHorizontalPercentBasis,
                                      aVerticalPercentBasis);
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
    needPaddingProp = ComputePadding(aHorizontalPercentBasis,
                                     aVerticalPercentBasis, aFrameType);
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
      ComputedPhysicalBorderPadding() = tableFrame->GetIncludedOuterBCBorder();
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
nsHTMLReflowState::CalculateBlockSideMargins(nscoord aAvailWidth,
                                             nscoord aComputedWidth,
                                             nsIAtom* aFrameType)
{
  NS_WARN_IF_FALSE(NS_UNCONSTRAINEDSIZE != aComputedWidth &&
                   NS_UNCONSTRAINEDSIZE != aAvailWidth,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");

  nscoord sum = ComputedPhysicalMargin().left + ComputedPhysicalBorderPadding().left +
    aComputedWidth + ComputedPhysicalBorderPadding().right + ComputedPhysicalMargin().right;
  if (sum == aAvailWidth)
    // The sum is already correct
    return;

  // Determine the left and right margin values. The width value
  // remains constant while we do this.

  // Calculate how much space is available for margins
  nscoord availMarginSpace = aAvailWidth - sum;

  // If the available margin space is negative, then don't follow the
  // usual overconstraint rules.
  if (availMarginSpace < 0) {
    if (mCBReflowState &&
        mCBReflowState->mStyleVisibility->mDirection == NS_STYLE_DIRECTION_RTL) {
      ComputedPhysicalMargin().left += availMarginSpace;
    } else {
      ComputedPhysicalMargin().right += availMarginSpace;
    }
    return;
  }

  // The css2 spec clearly defines how block elements should behave
  // in section 10.3.3.
  bool isAutoLeftMargin =
    eStyleUnit_Auto == mStyleMargin->mMargin.GetLeftUnit();
  bool isAutoRightMargin =
    eStyleUnit_Auto == mStyleMargin->mMargin.GetRightUnit();
  if (!isAutoLeftMargin && !isAutoRightMargin) {
    // Neither margin is 'auto' so we're over constrained. Use the
    // 'direction' property of the parent to tell which margin to
    // ignore
    // First check if there is an HTML alignment that we should honor
    const nsHTMLReflowState* prs = parentReflowState;
    if (aFrameType == nsGkAtoms::tableFrame) {
      NS_ASSERTION(prs->frame->GetType() == nsGkAtoms::tableOuterFrame,
                   "table not inside outer table");
      // Center the table within the outer table based on the alignment
      // of the outer table's parent.
      prs = prs->parentReflowState;
    }
    if (prs &&
        (prs->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_LEFT ||
         prs->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER ||
         prs->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT)) {
      isAutoLeftMargin =
        prs->mStyleText->mTextAlign != NS_STYLE_TEXT_ALIGN_MOZ_LEFT;
      isAutoRightMargin =
        prs->mStyleText->mTextAlign != NS_STYLE_TEXT_ALIGN_MOZ_RIGHT;
    }
    // Otherwise apply the CSS rules, and ignore one margin by forcing
    // it to 'auto', depending on 'direction'.
    else if (mCBReflowState &&
             NS_STYLE_DIRECTION_RTL == mCBReflowState->mStyleVisibility->mDirection) {
      isAutoLeftMargin = true;
    }
    else {
      isAutoRightMargin = true;
    }
  }

  // Logic which is common to blocks and tables
  // The computed margins need not be zero because the 'auto' could come from
  // overconstraint or from HTML alignment so values need to be accumulated

  if (isAutoLeftMargin) {
    if (isAutoRightMargin) {
      // Both margins are 'auto' so the computed addition should be equal
      nscoord forLeft = availMarginSpace / 2;
      ComputedPhysicalMargin().left  += forLeft;
      ComputedPhysicalMargin().right += availMarginSpace - forLeft;
    } else {
      ComputedPhysicalMargin().left += availMarginSpace;
    }
  } else if (isAutoRightMargin) {
    ComputedPhysicalMargin().right += availMarginSpace;
  }
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
                  nscoord aBlockHeight,
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
    if (aBlockHeight != NS_AUTOHEIGHT) {
      return aBlockHeight;
    }
  }

  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForStyleContext(aStyleContext,
                                               getter_AddRefs(fm),
                                               aFontSizeInflation);
  return GetNormalLineHeight(fm);
}

nscoord
nsHTMLReflowState::CalcLineHeight() const
{
  nscoord blockHeight =
    nsLayoutUtils::IsNonWrapperBlock(frame) ? ComputedHeight() :
    (mCBReflowState ? mCBReflowState->ComputedHeight() : NS_AUTOHEIGHT);

  return CalcLineHeight(frame->GetContent(), frame->StyleContext(), blockHeight,
                        nsLayoutUtils::FontSizeInflationFor(frame));
}

/* static */ nscoord
nsHTMLReflowState::CalcLineHeight(nsIContent* aContent,
                                  nsStyleContext* aStyleContext,
                                  nscoord aBlockHeight,
                                  float aFontSizeInflation)
{
  NS_PRECONDITION(aStyleContext, "Must have a style context");

  nscoord lineHeight =
    ComputeLineHeight(aStyleContext, aBlockHeight, aFontSizeInflation);

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
nsCSSOffsetState::ComputeMargin(nscoord aHorizontalPercentBasis,
                                nscoord aVerticalPercentBasis)
{
  // SVG text frames have no margin.
  if (frame->IsSVGText()) {
    return false;
  }

  // If style style can provide us the margin directly, then use it.
  const nsStyleMargin *styleMargin = frame->StyleMargin();
  bool isCBDependent = !styleMargin->GetMargin(ComputedPhysicalMargin());
  if (isCBDependent) {
    // We have to compute the value
    ComputedPhysicalMargin().left = nsLayoutUtils::
      ComputeCBDependentValue(aHorizontalPercentBasis,
                              styleMargin->mMargin.GetLeft());
    ComputedPhysicalMargin().right = nsLayoutUtils::
      ComputeCBDependentValue(aHorizontalPercentBasis,
                              styleMargin->mMargin.GetRight());

    ComputedPhysicalMargin().top = nsLayoutUtils::
      ComputeCBDependentValue(aVerticalPercentBasis,
                              styleMargin->mMargin.GetTop());
    ComputedPhysicalMargin().bottom = nsLayoutUtils::
      ComputeCBDependentValue(aVerticalPercentBasis,
                              styleMargin->mMargin.GetBottom());
  }

  nscoord marginAdjustment = FontSizeInflationListMarginAdjustment(frame);

  if (marginAdjustment > 0) {
    const nsStyleVisibility* visibility = frame->StyleVisibility();
    if (visibility->mDirection == NS_STYLE_DIRECTION_RTL) {
      ComputedPhysicalMargin().right = ComputedPhysicalMargin().right + marginAdjustment;
    } else {
      ComputedPhysicalMargin().left = ComputedPhysicalMargin().left + marginAdjustment;
    }
  }

  return isCBDependent;
}

bool
nsCSSOffsetState::ComputePadding(nscoord aHorizontalPercentBasis,
                                 nscoord aVerticalPercentBasis,
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
    // We have to compute the value
    // clamp negative calc() results to 0
    ComputedPhysicalPadding().left = std::max(0, nsLayoutUtils::
      ComputeCBDependentValue(aHorizontalPercentBasis,
                              stylePadding->mPadding.GetLeft()));
    ComputedPhysicalPadding().right = std::max(0, nsLayoutUtils::
      ComputeCBDependentValue(aHorizontalPercentBasis,
                              stylePadding->mPadding.GetRight()));

    ComputedPhysicalPadding().top = std::max(0, nsLayoutUtils::
      ComputeCBDependentValue(aVerticalPercentBasis,
                              stylePadding->mPadding.GetTop()));
    ComputedPhysicalPadding().bottom = std::max(0, nsLayoutUtils::
      ComputeCBDependentValue(aVerticalPercentBasis,
                              stylePadding->mPadding.GetBottom()));
  }
  return isCBDependent;
}

void
nsHTMLReflowState::ComputeMinMaxValues(nscoord aContainingBlockWidth,
                                       nscoord aContainingBlockHeight,
                                       const nsHTMLReflowState* aContainingBlockRS)
{
  // NOTE: min-width:auto resolves to 0, except on a flex item. (But
  // even there, it's supposed to be ignored (i.e. treated as 0) until
  // the flex container explicitly resolves & considers it.)
  if (eStyleUnit_Auto == mStylePosition->mMinWidth.GetUnit()) {
    ComputedMinWidth() = 0;
  } else {
    ComputedMinWidth() = ComputeWidthValue(aContainingBlockWidth,
                                          mStylePosition->mBoxSizing,
                                          mStylePosition->mMinWidth);
  }

  if (eStyleUnit_None == mStylePosition->mMaxWidth.GetUnit()) {
    // Specified value of 'none'
    ComputedMaxWidth() = NS_UNCONSTRAINEDSIZE;  // no limit
  } else {
    ComputedMaxWidth() = ComputeWidthValue(aContainingBlockWidth,
                                          mStylePosition->mBoxSizing,
                                          mStylePosition->mMaxWidth);
  }

  // If the computed value of 'min-width' is greater than the value of
  // 'max-width', 'max-width' is set to the value of 'min-width'
  if (ComputedMinWidth() > ComputedMaxWidth()) {
    ComputedMaxWidth() = ComputedMinWidth();
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
  const nsStyleCoord &minHeight = mStylePosition->mMinHeight;
  if (eStyleUnit_Auto == minHeight.GetUnit() ||
      (NS_AUTOHEIGHT == aContainingBlockHeight &&
       minHeight.HasPercent()) ||
      (mFrameType == NS_CSS_FRAME_TYPE_INTERNAL_TABLE &&
       minHeight.IsCalcUnit() && minHeight.CalcHasPercent()) ||
      mFlags.mIsFlexContainerMeasuringHeight) {
    ComputedMinHeight() = 0;
  } else {
    ComputedMinHeight() = ComputeHeightValue(aContainingBlockHeight, 
                                            mStylePosition->mBoxSizing, 
                                            minHeight);
  }
  const nsStyleCoord &maxHeight = mStylePosition->mMaxHeight;
  nsStyleUnit maxHeightUnit = maxHeight.GetUnit();
  if (eStyleUnit_None == maxHeightUnit) {
    // Specified value of 'none'
    ComputedMaxHeight() = NS_UNCONSTRAINEDSIZE;  // no limit
  } else {
    // Check for percentage based values and a containing block height that
    // depends on the content height. Treat them like 'none'
    // Likewise, check for calc() with percentages on internal table elements;
    // that's treated as 'auto' too.
    // Likewise, if we're a child of a flex container who's measuring our
    // intrinsic height, then we want to disregard our max-height.
    if ((NS_AUTOHEIGHT == aContainingBlockHeight && 
         maxHeight.HasPercent()) ||
        (mFrameType == NS_CSS_FRAME_TYPE_INTERNAL_TABLE &&
         maxHeight.IsCalcUnit() && maxHeight.CalcHasPercent()) ||
        mFlags.mIsFlexContainerMeasuringHeight) {
      ComputedMaxHeight() = NS_UNCONSTRAINEDSIZE;
    } else {
      ComputedMaxHeight() = ComputeHeightValue(aContainingBlockHeight, 
                                              mStylePosition->mBoxSizing,
                                              maxHeight);
    }
  }

  // If the computed value of 'min-height' is greater than the value of
  // 'max-height', 'max-height' is set to the value of 'min-height'
  if (ComputedMinHeight() > ComputedMaxHeight()) {
    ComputedMaxHeight() = ComputedMinHeight();
  }
}

void
nsHTMLReflowState::SetTruncated(const nsHTMLReflowMetrics& aMetrics,
                                nsReflowStatus* aStatus) const
{
  if (AvailableHeight() != NS_UNCONSTRAINEDSIZE &&
      AvailableHeight() < aMetrics.Height() &&
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
