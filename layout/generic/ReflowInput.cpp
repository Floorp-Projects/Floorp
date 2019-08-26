/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* struct containing the input to nsIFrame::Reflow */

#include "mozilla/ReflowInput.h"

#include "LayoutLogging.h"
#include "nsStyleConsts.h"
#include "nsCSSAnonBoxes.h"
#include "nsFrame.h"
#include "nsIContent.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
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
#include "nsGridContainerFrame.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;
using namespace mozilla::layout;

enum eNormalLineHeightControl {
  eUninitialized = -1,
  eNoExternalLeading = 0,   // does not include external leading
  eIncludeExternalLeading,  // use whatever value font vendor provides
  eCompensateLeading  // compensate leading if leading provided by font vendor
                      // is not enough
};

static eNormalLineHeightControl sNormalLineHeightControl = eUninitialized;

// Initialize a <b>root</b> reflow input with a rendering context to
// use for measuring things.
ReflowInput::ReflowInput(nsPresContext* aPresContext, nsIFrame* aFrame,
                         gfxContext* aRenderingContext,
                         const LogicalSize& aAvailableSpace, uint32_t aFlags)
    : SizeComputationInput(aFrame, aRenderingContext) {
  MOZ_ASSERT(aRenderingContext, "no rendering context");
  MOZ_ASSERT(aPresContext, "no pres context");
  MOZ_ASSERT(aFrame, "no frame");
  MOZ_ASSERT(aPresContext == aFrame->PresContext(), "wrong pres context");
  AvailableISize() = aAvailableSpace.ISize(mWritingMode);
  AvailableBSize() = aAvailableSpace.BSize(mWritingMode);

  if (aFlags & DUMMY_PARENT_REFLOW_INPUT) {
    mFlags.mDummyParentReflowInput = true;
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
  if (aFlags & I_CLAMP_MARGIN_BOX_MIN_SIZE) {
    mFlags.mIClampMarginBoxMinSize = true;
  }
  if (aFlags & B_CLAMP_MARGIN_BOX_MIN_SIZE) {
    mFlags.mBClampMarginBoxMinSize = true;
  }
  if (aFlags & I_APPLY_AUTO_MIN_SIZE) {
    mFlags.mApplyAutoMinSize = true;
  }

  if (!(aFlags & CALLER_WILL_INIT)) {
    Init(aPresContext);
  }
}

static bool CheckNextInFlowParenthood(nsIFrame* aFrame, nsIFrame* aParent) {
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
static nscoord FontSizeInflationListMarginAdjustment(const nsIFrame* aFrame) {
  if (!aFrame->IsBlockFrameOrSubclass()) {
    return 0;
  }

  // We only want to adjust the margins if we're dealing with an ordered list.
  const nsBlockFrame* blockFrame = static_cast<const nsBlockFrame*>(aFrame);
  if (!blockFrame->HasMarker()) {
    return 0;
  }

  float inflation = nsLayoutUtils::FontSizeInflationFor(aFrame);
  if (inflation <= 1.0f) {
    return 0;
  }

  // The HTML spec states that the default padding for ordered lists
  // begins at 40px, indicating that we have 40px of space to place a
  // bullet. When performing font inflation calculations, we add space
  // equivalent to this, but simply inflated at the same amount as the
  // text, in app units.
  auto margin = nsPresContext::CSSPixelsToAppUnits(40) * (inflation - 1);

  auto* list = aFrame->StyleList();
  if (!list->mCounterStyle.IsAtom()) {
    return margin;
  }

  // NOTE(emilio): @counter-style can override some of the styles from this
  // list, and we won't add margin to the counter.
  //
  // See https://github.com/w3c/csswg-drafts/issues/3584
  nsAtom* type = list->mCounterStyle.AsAtom();
  if (type != nsGkAtoms::none && type != nsGkAtoms::disc &&
      type != nsGkAtoms::circle && type != nsGkAtoms::square &&
      type != nsGkAtoms::disclosure_closed &&
      type != nsGkAtoms::disclosure_open) {
    return margin;
  }

  return 0;
}

SizeComputationInput::SizeComputationInput(
    nsIFrame* aFrame, gfxContext* aRenderingContext,
    WritingMode aContainingBlockWritingMode, nscoord aContainingBlockISize)
    : mFrame(aFrame),
      mRenderingContext(aRenderingContext),
      mWritingMode(aFrame->GetWritingMode()) {
  ReflowInputFlags flags;
  InitOffsets(aContainingBlockWritingMode, aContainingBlockISize,
              mFrame->Type(), flags);
}

// Initialize a reflow input for a child frame's reflow. Some state
// is copied from the parent reflow input; the remaining state is
// computed.
ReflowInput::ReflowInput(nsPresContext* aPresContext,
                         const ReflowInput& aParentReflowInput,
                         nsIFrame* aFrame, const LogicalSize& aAvailableSpace,
                         const Maybe<LogicalSize>& aContainingBlockSize,
                         uint32_t aFlags)
    : SizeComputationInput(aFrame, aParentReflowInput.mRenderingContext),
      mParentReflowInput(&aParentReflowInput),
      mFloatManager(aParentReflowInput.mFloatManager),
      mLineLayout(mFrame->IsFrameOfType(nsIFrame::eLineParticipant)
                      ? aParentReflowInput.mLineLayout
                      : nullptr),
      mPercentBSizeObserver(
          (aParentReflowInput.mPercentBSizeObserver &&
           aParentReflowInput.mPercentBSizeObserver->NeedsToObserve(*this))
              ? aParentReflowInput.mPercentBSizeObserver
              : nullptr),
      mFlags(aParentReflowInput.mFlags),
      mReflowDepth(aParentReflowInput.mReflowDepth + 1) {
  MOZ_ASSERT(aPresContext, "no pres context");
  MOZ_ASSERT(aFrame, "no frame");
  MOZ_ASSERT(aPresContext == aFrame->PresContext(), "wrong pres context");
  MOZ_ASSERT(!mFlags.mSpecialBSizeReflow || !NS_SUBTREE_DIRTY(aFrame),
             "frame should be clean when getting special bsize reflow");

  AvailableISize() = aAvailableSpace.ISize(mWritingMode);
  AvailableBSize() = aAvailableSpace.BSize(mWritingMode);

  if (mWritingMode.IsOrthogonalTo(aParentReflowInput.GetWritingMode())) {
    // If we're setting up for an orthogonal flow, and the parent reflow input
    // had a constrained ComputedBSize, we can use that as our AvailableISize
    // in preference to leaving it unconstrained.
    if (AvailableISize() == NS_UNCONSTRAINEDSIZE &&
        aParentReflowInput.ComputedBSize() != NS_UNCONSTRAINEDSIZE) {
      AvailableISize() = aParentReflowInput.ComputedBSize();
    }
  }

  // Note: mFlags was initialized as a copy of aParentReflowInput.mFlags up in
  // this constructor's init list, so the only flags that we need to explicitly
  // initialize here are those that may need a value other than our parent's.
  mFlags.mNextInFlowUntouched =
      aParentReflowInput.mFlags.mNextInFlowUntouched &&
      CheckNextInFlowParenthood(aFrame, aParentReflowInput.mFrame);
  mFlags.mAssumingHScrollbar = mFlags.mAssumingVScrollbar = false;
  mFlags.mIsColumnBalancing = false;
  mFlags.mColumnSetWrapperHasNoBSizeLeft = false;
  mFlags.mIsFlexContainerMeasuringBSize = false;
  mFlags.mDummyParentReflowInput = false;
  mFlags.mShrinkWrap = !!(aFlags & COMPUTE_SIZE_SHRINK_WRAP);
  mFlags.mUseAutoBSize = !!(aFlags & COMPUTE_SIZE_USE_AUTO_BSIZE);
  mFlags.mStaticPosIsCBOrigin = !!(aFlags & STATIC_POS_IS_CB_ORIGIN);
  mFlags.mIOffsetsNeedCSSAlign = mFlags.mBOffsetsNeedCSSAlign = false;
  mFlags.mIClampMarginBoxMinSize = !!(aFlags & I_CLAMP_MARGIN_BOX_MIN_SIZE);
  mFlags.mBClampMarginBoxMinSize = !!(aFlags & B_CLAMP_MARGIN_BOX_MIN_SIZE);
  mFlags.mApplyAutoMinSize = !!(aFlags & I_APPLY_AUTO_MIN_SIZE);
  mFlags.mApplyLineClamp = false;

  if ((aFlags & DUMMY_PARENT_REFLOW_INPUT) ||
      (mParentReflowInput->mFlags.mDummyParentReflowInput &&
       mFrame->IsTableFrame())) {
    mFlags.mDummyParentReflowInput = true;
  }

  if (!(aFlags & CALLER_WILL_INIT)) {
    Init(aPresContext, aContainingBlockSize);
  }
}

template <typename SizeOrMaxSize>
inline nscoord SizeComputationInput::ComputeISizeValue(
    nscoord aContainingBlockISize, nscoord aContentEdgeToBoxSizing,
    nscoord aBoxSizingToMarginEdge, const SizeOrMaxSize& aSize) const {
  return mFrame->ComputeISizeValue(mRenderingContext, aContainingBlockISize,
                                   aContentEdgeToBoxSizing,
                                   aBoxSizingToMarginEdge, aSize);
}

template <typename SizeOrMaxSize>
nscoord SizeComputationInput::ComputeISizeValue(
    nscoord aContainingBlockISize, StyleBoxSizing aBoxSizing,
    const SizeOrMaxSize& aSize) const {
  WritingMode wm = GetWritingMode();
  nscoord inside = 0, outside = ComputedLogicalBorderPadding().IStartEnd(wm) +
                                ComputedLogicalMargin().IStartEnd(wm);
  if (aBoxSizing == StyleBoxSizing::Border) {
    inside = ComputedLogicalBorderPadding().IStartEnd(wm);
  }
  outside -= inside;

  return ComputeISizeValue(aContainingBlockISize, inside, outside, aSize);
}

nscoord SizeComputationInput::ComputeBSizeValue(
    nscoord aContainingBlockBSize, StyleBoxSizing aBoxSizing,
    const LengthPercentage& aSize) const {
  WritingMode wm = GetWritingMode();
  nscoord inside = 0;
  if (aBoxSizing == StyleBoxSizing::Border) {
    inside = ComputedLogicalBorderPadding().BStartEnd(wm);
  }
  return nsLayoutUtils::ComputeBSizeValue(aContainingBlockBSize, inside, aSize);
}

void ReflowInput::SetComputedWidth(nscoord aComputedWidth) {
  NS_ASSERTION(mFrame, "Must have a frame!");
  // It'd be nice to assert that |frame| is not in reflow, but this fails for
  // two reasons:
  //
  // 1) Viewport frames reset the computed width on a copy of their reflow
  //    input when reflowing fixed-pos kids.  In that case we actually don't
  //    want to mess with the resize flags, because comparing the frame's rect
  //    to the munged computed width is pointless.
  // 2) nsFrame::BoxReflow creates a reflow input for its parent.  This reflow
  //    input is not used to reflow the parent, but just as a parent for the
  //    frame's own reflow input.  So given a nsBoxFrame inside some non-XUL
  //    (like a text control, for example), we'll end up creating a reflow
  //    input for the parent while the parent is reflowing.

  MOZ_ASSERT(aComputedWidth >= 0, "Invalid computed width");
  if (ComputedWidth() != aComputedWidth) {
    ComputedWidth() = aComputedWidth;
    LayoutFrameType frameType = mFrame->Type();
    if (frameType != LayoutFrameType::Viewport ||  // Or check GetParent()?
        mWritingMode.IsVertical()) {
      InitResizeFlags(mFrame->PresContext(), frameType);
    }
  }
}

void ReflowInput::SetComputedHeight(nscoord aComputedHeight) {
  NS_ASSERTION(mFrame, "Must have a frame!");
  // It'd be nice to assert that |frame| is not in reflow, but this fails
  // because:
  //
  //    nsFrame::BoxReflow creates a reflow input for its parent.  This reflow
  //    input is not used to reflow the parent, but just as a parent for the
  //    frame's own reflow input.  So given a nsBoxFrame inside some non-XUL
  //    (like a text control, for example), we'll end up creating a reflow
  //    input for the parent while the parent is reflowing.

  MOZ_ASSERT(aComputedHeight >= 0, "Invalid computed height");
  if (ComputedHeight() != aComputedHeight) {
    ComputedHeight() = aComputedHeight;
    LayoutFrameType frameType = mFrame->Type();
    if (frameType != LayoutFrameType::Viewport || !mWritingMode.IsVertical()) {
      InitResizeFlags(mFrame->PresContext(), frameType);
    }
  }
}

void ReflowInput::Init(nsPresContext* aPresContext,
                       const Maybe<LogicalSize>& aContainingBlockSize,
                       const nsMargin* aBorder, const nsMargin* aPadding) {
  if (AvailableISize() == NS_UNCONSTRAINEDSIZE) {
    // Look up the parent chain for an orthogonal inline limit,
    // and reset AvailableISize() if found.
    for (const ReflowInput* parent = mParentReflowInput; parent != nullptr;
         parent = parent->mParentReflowInput) {
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

  mStylePosition = mFrame->StylePosition();
  mStyleDisplay = mFrame->StyleDisplay();
  mStyleVisibility = mFrame->StyleVisibility();
  mStyleBorder = mFrame->StyleBorder();
  mStyleMargin = mFrame->StyleMargin();
  mStylePadding = mFrame->StylePadding();
  mStyleText = mFrame->StyleText();

  InitCBReflowInput();

  LayoutFrameType type = mFrame->Type();
  if (type == mozilla::LayoutFrameType::Placeholder) {
    // Placeholders have a no-op Reflow method that doesn't need the rest of
    // this initialization, so we bail out early.
    ComputedBSize() = ComputedISize() = 0;
    return;
  }

  InitFrameType(type);
  InitConstraints(aPresContext, aContainingBlockSize, aBorder, aPadding, type);

  InitResizeFlags(aPresContext, type);
  InitDynamicReflowRoot();

  nsIFrame* parent = mFrame->GetParent();
  if (parent && (parent->GetStateBits() & NS_FRAME_IN_CONSTRAINED_BSIZE) &&
      !(parent->IsScrollFrame() &&
        parent->StyleDisplay()->mOverflowY != StyleOverflow::Hidden)) {
    mFrame->AddStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE);
  } else if (type == LayoutFrameType::SVGForeignObject) {
    // An SVG foreignObject frame is inherently constrained block-size.
    mFrame->AddStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE);
  } else {
    const auto& bSizeCoord = mStylePosition->BSize(mWritingMode);
    const auto& maxBSizeCoord = mStylePosition->MaxBSize(mWritingMode);
    if ((!bSizeCoord.BehavesLikeInitialValueOnBlockAxis() ||
         !maxBSizeCoord.BehavesLikeInitialValueOnBlockAxis()) &&
        // Don't set NS_FRAME_IN_CONSTRAINED_BSIZE on body or html elements.
        (mFrame->GetContent() && !(mFrame->GetContent()->IsAnyOfHTMLElements(
                                     nsGkAtoms::body, nsGkAtoms::html)))) {
      // If our block-size was specified as a percentage, then this could
      // actually resolve to 'auto', based on:
      // http://www.w3.org/TR/CSS21/visudet.html#the-height-property
      nsIFrame* containingBlk = mFrame;
      while (containingBlk) {
        const nsStylePosition* stylePos = containingBlk->StylePosition();
        const auto& bSizeCoord = stylePos->BSize(mWritingMode);
        const auto& maxBSizeCoord = stylePos->MaxBSize(mWritingMode);
        if ((bSizeCoord.IsLengthPercentage() && !bSizeCoord.HasPercent()) ||
            (maxBSizeCoord.IsLengthPercentage() &&
             !maxBSizeCoord.HasPercent())) {
          mFrame->AddStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE);
          break;
        } else if (bSizeCoord.HasPercent() || maxBSizeCoord.HasPercent()) {
          if (!(containingBlk = containingBlk->GetContainingBlock())) {
            // If we've reached the top of the tree, then we don't have
            // a constrained block-size.
            mFrame->RemoveStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE);
            break;
          }

          continue;
        } else {
          mFrame->RemoveStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE);
          break;
        }
      }
    } else {
      mFrame->RemoveStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE);
    }
  }

  if (mParentReflowInput &&
      mParentReflowInput->GetWritingMode().IsOrthogonalTo(mWritingMode)) {
    // Orthogonal frames are always reflowed with an unconstrained
    // dimension to avoid incomplete reflow across an orthogonal
    // boundary. Normally this is the block-size, but for column sets
    // with auto-height it's the inline-size, so that they can add
    // columns in the container's block direction
    if (type == LayoutFrameType::ColumnSet &&
        mStylePosition->ISize(mWritingMode).IsAuto()) {
      ComputedISize() = NS_UNCONSTRAINEDSIZE;
    } else {
      AvailableBSize() = NS_UNCONSTRAINEDSIZE;
    }
  }

  if (mStyleDisplay->IsContainSize()) {
    // In the case that a box is size contained, we want to ensure
    // that it is also monolithic. We do this by unsetting
    // AvailableBSize() to avoid fragmentaiton.
    AvailableBSize() = NS_UNCONSTRAINEDSIZE;
  }

  LAYOUT_WARN_IF_FALSE((mFrameType == NS_CSS_FRAME_TYPE_INLINE &&
                        !mFrame->IsFrameOfType(nsIFrame::eReplaced)) ||
                           type == LayoutFrameType::Text ||
                           ComputedISize() != NS_UNCONSTRAINEDSIZE,
                       "have unconstrained inline-size; this should only "
                       "result from very large sizes, not attempts at "
                       "intrinsic inline-size calculation");
}

void ReflowInput::InitCBReflowInput() {
  if (!mParentReflowInput) {
    mCBReflowInput = nullptr;
    return;
  }
  if (mParentReflowInput->mFlags.mDummyParentReflowInput) {
    mCBReflowInput = mParentReflowInput;
    return;
  }

  if (mParentReflowInput->mFrame ==
      mFrame->GetContainingBlock(0, mStyleDisplay)) {
    // Inner table frames need to use the containing block of the outer
    // table frame.
    if (mFrame->IsTableFrame()) {
      mCBReflowInput = mParentReflowInput->mCBReflowInput;
    } else {
      mCBReflowInput = mParentReflowInput;
    }
  } else {
    mCBReflowInput = mParentReflowInput->mCBReflowInput;
  }
}

/* Check whether CalcQuirkContainingBlockHeight would stop on the
 * given reflow input, using its block as a height.  (essentially
 * returns false for any case in which CalcQuirkContainingBlockHeight
 * has a "continue" in its main loop.)
 *
 * XXX Maybe refactor CalcQuirkContainingBlockHeight so it uses
 * this function as well
 */
static bool IsQuirkContainingBlockHeight(const ReflowInput* rs,
                                         LayoutFrameType aFrameType) {
  if (LayoutFrameType::Block == aFrameType ||
#ifdef MOZ_XUL
      LayoutFrameType::XULLabel == aFrameType ||
#endif
      LayoutFrameType::Scroll == aFrameType) {
    // Note: This next condition could change due to a style change,
    // but that would cause a style reflow anyway, which means we're ok.
    if (NS_UNCONSTRAINEDSIZE == rs->ComputedHeight()) {
      if (!rs->mFrame->IsAbsolutelyPositioned(rs->mStyleDisplay)) {
        return false;
      }
    }
  }
  return true;
}

void ReflowInput::InitResizeFlags(nsPresContext* aPresContext,
                                  LayoutFrameType aFrameType) {
  SetBResize(false);
  SetIResize(false);
  mFlags.mIsBResizeForPercentages = false;

  const WritingMode wm = mWritingMode;  // just a shorthand
  // We should report that we have a resize in the inline dimension if
  // *either* the border-box size or the content-box size in that
  // dimension has changed.  It might not actually be necessary to do
  // this if the border-box size has changed and the content-box size
  // has not changed, but since we've historically used the flag to mean
  // border-box size change, continue to do that.  (It's possible for
  // the content-box size to change without a border-box size change or
  // a style change given (1) a fixed width (possibly fixed by max-width
  // or min-width), (2) box-sizing:border-box or padding-box, and
  // (3) percentage padding.)
  //
  // However, we don't actually have the information at this point to
  // tell whether the content-box size has changed, since both style
  // data and the UsedPaddingProperty() have already been updated.  So,
  // instead, we explicitly check for the case where it's possible for
  // the content-box size to have changed without either (a) a change in
  // the border-box size or (b) an nsChangeHint_NeedDirtyReflow change
  // hint due to change in border or padding.  Thus we test using the
  // conditions from the previous paragraph, except without testing (1)
  // since it's complicated to test properly and less likely to help
  // with optimizing cases away.
  bool isIResize =
      // is the border-box resizing?
      mFrame->ISize(wm) !=
          ComputedISize() + ComputedLogicalBorderPadding().IStartEnd(wm) ||
      // or is the content-box resizing?  (see comment above)
      (mStylePosition->mBoxSizing != StyleBoxSizing::Content &&
       mStylePadding->IsWidthDependent());

  if ((mFrame->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT) &&
      nsLayoutUtils::FontSizeInflationEnabled(aPresContext)) {
    // Create our font inflation data if we don't have it already, and
    // give it our current width information.
    bool dirty = nsFontInflationData::UpdateFontInflationDataISizeFor(*this) &&
                 // Avoid running this at the box-to-block interface
                 // (where we shouldn't be inflating anyway, and where
                 // reflow input construction is probably to construct a
                 // dummy parent reflow input anyway).
                 !mFlags.mDummyParentReflowInput;

    if (dirty || (!mFrame->GetParent() && isIResize)) {
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
      // ReflowInput::SetComputedWidth is called, if the first time
      // we go through InitResizeFlags we set IsHResize() to true, and then
      // the second time we'd set it to false even without the
      // NS_FRAME_IS_DIRTY bit already set.
      if (mFrame->IsSVGForeignObjectFrame()) {
        // Foreign object frames use dirty bits in a special way.
        mFrame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
        nsIFrame* kid = mFrame->PrincipalChildList().FirstChild();
        if (kid) {
          kid->MarkSubtreeDirty();
        }
      } else {
        mFrame->MarkSubtreeDirty();
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
      stack.AppendElement(mFrame);

      do {
        nsIFrame* f = stack.PopLastElement();

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

  SetIResize(!(mFrame->GetStateBits() & NS_FRAME_IS_DIRTY) && isIResize);

  // XXX Should we really need to null check mCBReflowInput?  (We do for
  // at least nsBoxFrame).
  if (mFrame->HasBSizeChange()) {
    // When we have an nsChangeHint_UpdateComputedBSize, we'll set a bit
    // on the frame to indicate we're resizing.  This might catch cases,
    // such as a change between auto and a length, where the box doesn't
    // actually resize but children with percentages resize (since those
    // percentages become auto if their containing block is auto).
    SetBResize(true);
    mFlags.mIsBResizeForPercentages = true;
    // We don't clear the HasBSizeChange state here, since sometimes we
    // construct reflow states (e.g., in
    // nsBlockReflowContext::ComputeCollapsedBStartMargin) without
    // reflowing the frame.  Instead, we clear it in nsFrame::DidReflow.
  } else if (mCBReflowInput &&
             mCBReflowInput->IsBResizeForPercentagesForWM(wm) &&
             (mStylePosition->BSize(wm).HasPercent() ||
              mStylePosition->MinBSize(wm).HasPercent() ||
              mStylePosition->MaxBSize(wm).HasPercent())) {
    // We have a percentage (or calc-with-percentage) block-size, and the
    // value it's relative to has changed.
    SetBResize(true);
    mFlags.mIsBResizeForPercentages = true;
  } else if (aFrameType == LayoutFrameType::TableCell &&
             (mFlags.mSpecialBSizeReflow ||
              (mFrame->FirstInFlow()->GetStateBits() &
               NS_TABLE_CELL_HAD_SPECIAL_REFLOW)) &&
             (mFrame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
    // Need to set the bit on the cell so that
    // mCBReflowInput->IsBResize() is set correctly below when
    // reflowing descendant.
    SetBResize(true);
    mFlags.mIsBResizeForPercentages = true;
  } else if (mCBReflowInput && mFrame->IsBlockWrapper()) {
    // XXX Is this problematic for relatively positioned inlines acting
    // as containing block for absolutely positioned elements?
    // Possibly; in that case we should at least be checking
    // NS_SUBTREE_DIRTY, I'd think.
    SetBResize(mCBReflowInput->IsBResizeForWM(wm));
    mFlags.mIsBResizeForPercentages =
        mCBReflowInput->IsBResizeForPercentagesForWM(wm);
  } else if (ComputedBSize() == NS_UNCONSTRAINEDSIZE) {
    // We have an 'auto' block-size.
    if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
        mCBReflowInput) {
      // FIXME: This should probably also check IsIResize().
      SetBResize(mCBReflowInput->IsBResizeForWM(wm));
    } else {
      SetBResize(IsIResize());
    }
    SetBResize(IsBResize() || NS_SUBTREE_DIRTY(mFrame));
  } else {
    // We have a non-'auto' block-size, i.e., a length.  Set the BResize
    // flag to whether the size is actually different.
    SetBResize(mFrame->BSize(wm) !=
               ComputedBSize() + ComputedLogicalBorderPadding().BStartEnd(wm));
  }

  bool dependsOnCBBSize =
      (mStylePosition->BSizeDependsOnContainer(wm) &&
       // FIXME: condition this on not-abspos?
       !mStylePosition->BSize(wm).IsAuto()) ||
      mStylePosition->MinBSizeDependsOnContainer(wm) ||
      mStylePosition->MaxBSizeDependsOnContainer(wm) ||
      mStylePosition->OffsetHasPercent(wm.PhysicalSide(eLogicalSideBStart)) ||
      !mStylePosition->mOffset.GetBEnd(wm).IsAuto() || mFrame->IsXULBoxFrame();

  if (mStyleText->mLineHeight.IsMozBlockHeight()) {
    // line-height depends on block bsize
    mFrame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
    // but only on containing blocks if this frame is not a suitable block
    dependsOnCBBSize |= !nsLayoutUtils::IsNonWrapperBlock(mFrame);
  }

  // If we're the descendant of a table cell that performs special bsize
  // reflows and we could be the child that requires them, always set
  // the block-axis resize in case this is the first pass before the
  // special bsize reflow.  However, don't do this if it actually is
  // the special bsize reflow, since in that case it will already be
  // set correctly above if we need it set.
  if (!IsBResize() && mCBReflowInput &&
      (mCBReflowInput->mFrame->IsTableCellFrame() ||
       mCBReflowInput->mFlags.mHeightDependsOnAncestorCell) &&
      !mCBReflowInput->mFlags.mSpecialBSizeReflow && dependsOnCBBSize) {
    SetBResize(true);
    mFlags.mHeightDependsOnAncestorCell = true;
  }

  // Set NS_FRAME_CONTAINS_RELATIVE_BSIZE if it's needed.

  // It would be nice to check that |ComputedBSize != NS_UNCONSTRAINEDSIZE|
  // &&ed with the percentage bsize check.  However, this doesn't get
  // along with table special bsize reflows, since a special bsize
  // reflow (a quirk that makes such percentage height work on children
  // of table cells) can cause not just a single percentage height to
  // become fixed, but an entire descendant chain of percentage height
  // to become fixed.
  if (dependsOnCBBSize && mCBReflowInput) {
    const ReflowInput* rs = this;
    bool hitCBReflowInput = false;
    do {
      rs = rs->mParentReflowInput;
      if (!rs) {
        break;
      }

      if (rs->mFrame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_BSIZE) {
        break;  // no need to go further
      }
      rs->mFrame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);

      // Keep track of whether we've hit the containing block, because
      // we need to go at least that far.
      if (rs == mCBReflowInput) {
        hitCBReflowInput = true;
      }

      // XXX What about orthogonal flows? It doesn't make sense to
      // keep propagating this bit across an orthogonal boundary,
      // where the meaning of BSize changes. Bug 1175517.
    } while (!hitCBReflowInput ||
             (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
              !IsQuirkContainingBlockHeight(rs, rs->mFrame->Type())));
    // Note: We actually don't need to set the
    // NS_FRAME_CONTAINS_RELATIVE_BSIZE bit for the cases
    // where we hit the early break statements in
    // CalcQuirkContainingBlockHeight. But it doesn't hurt
    // us to set the bit in these cases.
  }
  if (mFrame->GetStateBits() & NS_FRAME_IS_DIRTY) {
    // If we're reflowing everything, then we'll find out if we need
    // to re-set this.
    mFrame->RemoveStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
  }
}

template <typename SizeOrMaxSize>
static inline bool IsIntrinsicKeyword(const SizeOrMaxSize& aSize) {
  if (!aSize.IsExtremumLength()) {
    return false;
  }

  // All of the keywords except for '-moz-available' depend on intrinsic sizes.
  return aSize.AsExtremumLength() != StyleExtremumLength::MozAvailable;
}

static bool AreDynamicReflowRootsEnabled() {
  static bool sAreDynamicReflowRootsEnabled;
  static bool sIsPrefCached = false;

  if (!sIsPrefCached) {
    sIsPrefCached = true;
    Preferences::AddBoolVarCache(&sAreDynamicReflowRootsEnabled,
                                 "layout.dynamic-reflow-roots.enabled");
  }
  return sAreDynamicReflowRootsEnabled;
}

void ReflowInput::InitDynamicReflowRoot() {
  auto display = mStyleDisplay->mDisplay;
  if (mFrame->IsFrameOfType(nsIFrame::eLineParticipant) ||
      nsStyleDisplay::IsRubyDisplayType(display) ||
      mFrameType == NS_CSS_FRAME_TYPE_INTERNAL_TABLE ||
      display == StyleDisplay::Table || display == StyleDisplay::InlineTable ||
      (mFrame->GetParent() && mFrame->GetParent()->IsXULBoxFrame())) {
    // We have a display type where 'width' and 'height' don't actually
    // set the width or height (i.e., the size depends on content).
    NS_ASSERTION(!(mFrame->GetStateBits() & NS_FRAME_DYNAMIC_REFLOW_ROOT),
                 "should not have dynamic reflow root bit");
    return;
  }

  bool canBeDynamicReflowRoot = AreDynamicReflowRootsEnabled();

  // We can't do this if our used 'width' and 'height' might be influenced by
  // content.
  // FIXME: For display:block, we should probably optimize inline-size
  // being auto.
  // FIXME: Other flex and grid cases?
  const auto& width = mStylePosition->mWidth;
  const auto& height = mStylePosition->mHeight;
  if (canBeDynamicReflowRoot &&
      (!width.IsLengthPercentage() || width.HasPercent() ||
       !height.IsLengthPercentage() || height.HasPercent() ||
       IsIntrinsicKeyword(mStylePosition->mMinWidth) ||
       IsIntrinsicKeyword(mStylePosition->mMaxWidth) ||
       IsIntrinsicKeyword(mStylePosition->mMinHeight) ||
       IsIntrinsicKeyword(mStylePosition->mMaxHeight) ||
       ((mStylePosition->mMinWidth.IsAuto() ||
         mStylePosition->mMinHeight.IsAuto()) &&
        mFrame->IsFlexOrGridItem()))) {
    canBeDynamicReflowRoot = false;
  }

  if (canBeDynamicReflowRoot && mFrame->IsFlexItem()) {
    // If our flex-basis is 'auto', it'll defer to 'width' (or 'height') which
    // we've already checked. Otherwise, it preempts them, so we need to
    // perform the same "could-this-value-be-influenced-by-content" checks that
    // we performed for 'width' and 'height' above.
    const auto& flexBasis = mStylePosition->mFlexBasis;
    if (!flexBasis.IsAuto()) {
      if (!flexBasis.IsSize() || !flexBasis.AsSize().IsLengthPercentage() ||
          flexBasis.AsSize().HasPercent()) {
        canBeDynamicReflowRoot = false;
      }
    }
  }

  if (canBeDynamicReflowRoot && !mFrame->IsFixedPosContainingBlock()) {
    // We can't treat this frame as a reflow root, since dynamic changes
    // to absolutely-positioned frames inside of it require that we
    // reflow the placeholder before we reflow the absolutely positioned
    // frame.
    // FIXME:  Alternatively, we could sort the reflow roots in
    // PresShell::ProcessReflowCommands by depth in the tree, from
    // deepest to least deep.  However, for performance (FIXME) we
    // should really be sorting them in the opposite order!
    canBeDynamicReflowRoot = false;
  }

  // If we participate in a container's block reflow context, or margins
  // can collapse through us, we can't be a dynamic reflow root.
  if (canBeDynamicReflowRoot && mFrame->IsBlockFrameOrSubclass() &&
      !mFrame->HasAllStateBits(NS_BLOCK_FLOAT_MGR | NS_BLOCK_MARGIN_ROOT)) {
    canBeDynamicReflowRoot = false;
  }

  // Subgrids are never reflow roots, but 'contain:layout/paint' prevents
  // creating a subgrid in the first place.
  if (canBeDynamicReflowRoot &&
      (mStylePosition->mGridTemplateColumns.IsSubgrid() ||
       mStylePosition->mGridTemplateRows.IsSubgrid()) &&
      !(mStyleDisplay->IsContainLayout() || mStyleDisplay->IsContainPaint())) {
    // NOTE: we could check that 'display' of our content's primary frame is
    // '[inline-]grid' here but that's probably not worth it in practice.
    canBeDynamicReflowRoot = false;
  }

  if (canBeDynamicReflowRoot) {
    mFrame->AddStateBits(NS_FRAME_DYNAMIC_REFLOW_ROOT);
  } else {
    mFrame->RemoveStateBits(NS_FRAME_DYNAMIC_REFLOW_ROOT);
  }
}

nscoord ReflowInput::GetContainingBlockContentISize(
    WritingMode aWritingMode) const {
  if (!mCBReflowInput) {
    return 0;
  }
  return mCBReflowInput->GetWritingMode().IsOrthogonalTo(aWritingMode)
             ? mCBReflowInput->ComputedBSize()
             : mCBReflowInput->ComputedISize();
}

void ReflowInput::InitFrameType(LayoutFrameType aFrameType) {
  const nsStyleDisplay* disp = mStyleDisplay;
  nsCSSFrameType frameType;

  DISPLAY_INIT_TYPE(mFrame, this);

  if (aFrameType == LayoutFrameType::Table) {
    mFrameType = NS_CSS_FRAME_TYPE_BLOCK;
    return;
  }

  NS_ASSERTION(mFrame->StyleDisplay()->IsAbsolutelyPositionedStyle() ==
                   disp->IsAbsolutelyPositionedStyle(),
               "Unexpected position style");
  NS_ASSERTION(
      mFrame->StyleDisplay()->IsFloatingStyle() == disp->IsFloatingStyle(),
      "Unexpected float style");
  if (mFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    if (disp->IsAbsolutelyPositioned(mFrame)) {
      frameType = NS_CSS_FRAME_TYPE_ABSOLUTE;
      // XXXfr hack for making frames behave properly when in overflow container
      // lists
      //      see bug 154892; need to revisit later
      if (mFrame->GetPrevInFlow()) frameType = NS_CSS_FRAME_TYPE_BLOCK;
    } else if (disp->IsFloating(mFrame)) {
      frameType = NS_CSS_FRAME_TYPE_FLOATING;
    } else {
      NS_ASSERTION(disp->mDisplay == StyleDisplay::MozPopup,
                   "unknown out of flow frame type");
      frameType = NS_CSS_FRAME_TYPE_UNKNOWN;
    }
  } else {
    switch (disp->DisplayOutside()) {
      case StyleDisplayOutside::Block:
      case StyleDisplayOutside::TableCaption:
        frameType = NS_CSS_FRAME_TYPE_BLOCK;
        break;

      case StyleDisplayOutside::Inline:
        frameType = NS_CSS_FRAME_TYPE_INLINE;
        break;

      case StyleDisplayOutside::InternalTable:
        frameType = NS_CSS_FRAME_TYPE_INTERNAL_TABLE;
        break;

      case StyleDisplayOutside::InternalRuby:
        switch (disp->DisplayInside()) {
          case StyleDisplayInside::RubyTextContainer:
            frameType = NS_CSS_FRAME_TYPE_BLOCK;
            break;
          case StyleDisplayInside::RubyBase:
          case StyleDisplayInside::RubyText:
          case StyleDisplayInside::RubyBaseContainer:
            frameType = NS_CSS_FRAME_TYPE_INLINE;
            break;
          default:
            MOZ_ASSERT_UNREACHABLE("unexpected inside for InternalRuby");
        }
        break;

      default:
        frameType = NS_CSS_FRAME_TYPE_UNKNOWN;
        break;
    }
  }

  // See if the frame is replaced
  if (mFrame->IsFrameOfType(nsIFrame::eReplacedContainsBlock)) {
    frameType = NS_FRAME_REPLACED_CONTAINS_BLOCK(frameType);
  } else if (mFrame->IsFrameOfType(nsIFrame::eReplaced)) {
    frameType = NS_FRAME_REPLACED(frameType);
  }

  mFrameType = frameType;
}

/* static */
void ReflowInput::ComputeRelativeOffsets(WritingMode aWM, nsIFrame* aFrame,
                                         const LogicalSize& aCBSize,
                                         nsMargin& aComputedOffsets) {
  LogicalMargin offsets(aWM);
  mozilla::Side inlineStart = aWM.PhysicalSide(eLogicalSideIStart);
  mozilla::Side inlineEnd = aWM.PhysicalSide(eLogicalSideIEnd);
  mozilla::Side blockStart = aWM.PhysicalSide(eLogicalSideBStart);
  mozilla::Side blockEnd = aWM.PhysicalSide(eLogicalSideBEnd);

  const nsStylePosition* position = aFrame->StylePosition();

  // Compute the 'inlineStart' and 'inlineEnd' values. 'inlineStart'
  // moves the boxes to the end of the line, and 'inlineEnd' moves the
  // boxes to the start of the line. The computed values are always:
  // inlineStart=-inlineEnd
  bool inlineStartIsAuto = position->mOffset.Get(inlineStart).IsAuto();
  bool inlineEndIsAuto = position->mOffset.Get(inlineEnd).IsAuto();

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
      offsets.IEnd(aWM) = nsLayoutUtils::ComputeCBDependentValue(
          aCBSize.ISize(aWM), position->mOffset.Get(inlineEnd));

      // Computed value for 'inlineStart' is minus the value of 'inlineEnd'
      offsets.IStart(aWM) = -offsets.IEnd(aWM);
    }

  } else {
    NS_ASSERTION(inlineEndIsAuto, "unexpected specified constraint");

    // 'InlineStart' isn't 'auto' so compute its value
    offsets.IStart(aWM) = nsLayoutUtils::ComputeCBDependentValue(
        aCBSize.ISize(aWM), position->mOffset.Get(inlineStart));

    // Computed value for 'inlineEnd' is minus the value of 'inlineStart'
    offsets.IEnd(aWM) = -offsets.IStart(aWM);
  }

  // Compute the 'blockStart' and 'blockEnd' values. The 'blockStart'
  // and 'blockEnd' properties move relatively positioned elements in
  // the block progression direction. They also must be each other's
  // negative
  bool blockStartIsAuto = position->mOffset.Get(blockStart).IsAuto();
  bool blockEndIsAuto = position->mOffset.Get(blockEnd).IsAuto();

  // Check for percentage based values and a containing block block-size
  // that depends on the content block-size. Treat them like 'auto'
  if (NS_UNCONSTRAINEDSIZE == aCBSize.BSize(aWM)) {
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
      offsets.BEnd(aWM) = nsLayoutUtils::ComputeBSizeDependentValue(
          aCBSize.BSize(aWM), position->mOffset.Get(blockEnd));

      // Computed value for 'blockStart' is minus the value of 'blockEnd'
      offsets.BStart(aWM) = -offsets.BEnd(aWM);
    }

  } else {
    NS_ASSERTION(blockEndIsAuto, "unexpected specified constraint");

    // 'blockStart' isn't 'auto' so compute its value
    offsets.BStart(aWM) = nsLayoutUtils::ComputeBSizeDependentValue(
        aCBSize.BSize(aWM), position->mOffset.Get(blockStart));

    // Computed value for 'blockEnd' is minus the value of 'blockStart'
    offsets.BEnd(aWM) = -offsets.BStart(aWM);
  }

  // Convert the offsets to physical coordinates and store them on the frame
  aComputedOffsets = offsets.GetPhysicalMargin(aWM);
  nsMargin* physicalOffsets =
      aFrame->GetProperty(nsIFrame::ComputedOffsetProperty());
  if (physicalOffsets) {
    *physicalOffsets = aComputedOffsets;
  } else {
    aFrame->AddProperty(nsIFrame::ComputedOffsetProperty(),
                        new nsMargin(aComputedOffsets));
  }
}

/* static */
void ReflowInput::ApplyRelativePositioning(nsIFrame* aFrame,
                                           const nsMargin& aComputedOffsets,
                                           nsPoint* aPosition) {
  if (!aFrame->IsRelativelyPositioned()) {
    NS_ASSERTION(!aFrame->GetProperty(nsIFrame::NormalPositionProperty()),
                 "We assume that changing the 'position' property causes "
                 "frame reconstruction.  If that ever changes, this code "
                 "should call "
                 "aFrame->DeleteProperty(nsIFrame::NormalPositionProperty())");
    return;
  }

  // Store the normal position
  nsPoint* normalPosition =
      aFrame->GetProperty(nsIFrame::NormalPositionProperty());
  if (normalPosition) {
    *normalPosition = *aPosition;
  } else {
    aFrame->AddProperty(nsIFrame::NormalPositionProperty(),
                        new nsPoint(*aPosition));
  }

  const nsStyleDisplay* display = aFrame->StyleDisplay();
  if (NS_STYLE_POSITION_RELATIVE == display->mPosition) {
    *aPosition += nsPoint(aComputedOffsets.left, aComputedOffsets.top);
  } else if (NS_STYLE_POSITION_STICKY == display->mPosition &&
             !aFrame->GetNextContinuation() && !aFrame->GetPrevContinuation() &&
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

// Returns true if aFrame is non-null, a XUL frame, and "XUL-collapsed" (which
// only becomes a valid question to ask if we know it's a XUL frame).
static bool IsXULCollapsedXULFrame(nsIFrame* aFrame) {
  return aFrame && aFrame->IsXULBoxFrame() && aFrame->IsXULCollapsed();
}

nsIFrame* ReflowInput::GetHypotheticalBoxContainer(nsIFrame* aFrame,
                                                   nscoord& aCBIStartEdge,
                                                   LogicalSize& aCBSize) const {
  aFrame = aFrame->GetContainingBlock();
  NS_ASSERTION(aFrame != mFrame, "How did that happen?");

  /* Now aFrame is the containing block we want */

  /* Check whether the containing block is currently being reflowed.
     If so, use the info from the reflow input. */
  const ReflowInput* reflowInput;
  if (aFrame->GetStateBits() & NS_FRAME_IN_REFLOW) {
    for (reflowInput = mParentReflowInput;
         reflowInput && reflowInput->mFrame != aFrame;
         reflowInput = reflowInput->mParentReflowInput) {
      /* do nothing */
    }
  } else {
    reflowInput = nullptr;
  }

  if (reflowInput) {
    WritingMode wm = reflowInput->GetWritingMode();
    NS_ASSERTION(wm == aFrame->GetWritingMode(), "unexpected writing mode");
    aCBIStartEdge = reflowInput->ComputedLogicalBorderPadding().IStart(wm);
    aCBSize = reflowInput->ComputedSize(wm);
  } else {
    /* Didn't find a reflow reflowInput for aFrame.  Just compute the
       information we want, on the assumption that aFrame already knows its
       size.  This really ought to be true by now. */
    NS_ASSERTION(!(aFrame->GetStateBits() & NS_FRAME_IN_REFLOW),
                 "aFrame shouldn't be in reflow; we'll lie if it is");
    WritingMode wm = aFrame->GetWritingMode();
    // Compute CB's offset & content-box size by subtracting borderpadding from
    // frame size.  Exception: if the CB is 0-sized, it *might* be a child of a
    // XUL-collapsed frame and might have nonzero borderpadding that was simply
    // discarded during its layout. (See the child-zero-sizing in
    // nsSprocketLayout::XULLayout()).  In that case, we ignore the
    // borderpadding here (just like we did when laying it out), or else we'd
    // produce a bogus negative content-box size.
    aCBIStartEdge = 0;
    aCBSize = aFrame->GetLogicalSize(wm);
    if (!aCBSize.IsAllZero() ||
        (!IsXULCollapsedXULFrame(aFrame->GetParent()))) {
      // aFrame is not XUL-collapsed (nor is it a child of a XUL-collapsed
      // frame), so we can go ahead and subtract out border padding.
      LogicalMargin borderPadding = aFrame->GetLogicalUsedBorderAndPadding(wm);
      aCBIStartEdge += borderPadding.IStart(wm);
      aCBSize -= borderPadding.Size(wm);
    }
  }

  return aFrame;
}

struct nsHypotheticalPosition {
  // offset from inline-start edge of containing block (which is a padding edge)
  nscoord mIStart;
  // offset from block-start edge of containing block (which is a padding edge)
  nscoord mBStart;
  WritingMode mWritingMode;
};

static bool GetIntrinsicSizeFor(nsIFrame* aFrame, nsSize& aIntrinsicSize,
                                LayoutFrameType aFrameType) {
  // See if it is an image frame
  bool success = false;

  // Currently the only type of replaced frame that we can get the intrinsic
  // size for is an image frame
  // XXX We should add back the GetReflowOutput() function and one of the
  // things should be the intrinsic size...
  if (aFrameType == LayoutFrameType::Image) {
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
void ReflowInput::CalculateBorderPaddingMargin(
    LogicalAxis aAxis, nscoord aContainingBlockSize, nscoord* aInsideBoxSizing,
    nscoord* aOutsideBoxSizing) const {
  WritingMode wm = GetWritingMode();
  mozilla::Side startSide =
      wm.PhysicalSide(MakeLogicalSide(aAxis, eLogicalEdgeStart));
  mozilla::Side endSide =
      wm.PhysicalSide(MakeLogicalSide(aAxis, eLogicalEdgeEnd));

  nsMargin styleBorder = mStyleBorder->GetComputedBorder();
  nscoord borderStartEnd =
      styleBorder.Side(startSide) + styleBorder.Side(endSide);

  nscoord paddingStartEnd, marginStartEnd;

  // See if the style system can provide us the padding directly
  nsMargin stylePadding;
  if (mStylePadding->GetPadding(stylePadding)) {
    paddingStartEnd = stylePadding.Side(startSide) + stylePadding.Side(endSide);
  } else {
    // We have to compute the start and end values
    nscoord start, end;
    start = nsLayoutUtils::ComputeCBDependentValue(
        aContainingBlockSize, mStylePadding->mPadding.Get(startSide));
    end = nsLayoutUtils::ComputeCBDependentValue(
        aContainingBlockSize, mStylePadding->mPadding.Get(endSide));
    paddingStartEnd = start + end;
  }

  // See if the style system can provide us the margin directly
  nsMargin styleMargin;
  if (mStyleMargin->GetMargin(styleMargin)) {
    marginStartEnd = styleMargin.Side(startSide) + styleMargin.Side(endSide);
  } else {
    nscoord start, end;
    // We have to compute the start and end values
    if (mStyleMargin->mMargin.Get(startSide).IsAuto()) {
      // We set this to 0 for now, and fix it up later in
      // InitAbsoluteConstraints (which is caller of this function, via
      // CalculateHypotheticalPosition).
      start = 0;
    } else {
      start = nsLayoutUtils::ComputeCBDependentValue(
          aContainingBlockSize, mStyleMargin->mMargin.Get(startSide));
    }
    if (mStyleMargin->mMargin.Get(endSide).IsAuto()) {
      // We set this to 0 for now, and fix it up later in
      // InitAbsoluteConstraints (which is caller of this function, via
      // CalculateHypotheticalPosition).
      end = 0;
    } else {
      end = nsLayoutUtils::ComputeCBDependentValue(
          aContainingBlockSize, mStyleMargin->mMargin.Get(endSide));
    }
    marginStartEnd = start + end;
  }

  nscoord outside = paddingStartEnd + borderStartEnd + marginStartEnd;
  nscoord inside = 0;
  if (mStylePosition->mBoxSizing == StyleBoxSizing::Border) {
    inside = borderStartEnd + paddingStartEnd;
  }
  outside -= inside;
  *aInsideBoxSizing = inside;
  *aOutsideBoxSizing = outside;
}

/**
 * Returns true iff a pre-order traversal of the normal child
 * frames rooted at aFrame finds no non-empty frame before aDescendant.
 */
static bool AreAllEarlierInFlowFramesEmpty(nsIFrame* aFrame,
                                           nsIFrame* aDescendant,
                                           bool* aFound) {
  if (aFrame == aDescendant) {
    *aFound = true;
    return true;
  }
  if (aFrame->IsPlaceholderFrame()) {
    auto ph = static_cast<nsPlaceholderFrame*>(aFrame);
    MOZ_ASSERT(ph->IsSelfEmpty() && ph->PrincipalChildList().IsEmpty());
    ph->SetLineIsEmptySoFar(true);
  } else {
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
// In the code below, |aCBReflowInput->frame| is the absolute containing block,
// while |containingBlock| is the nearest block container of the placeholder
// frame, which may be different from the absolute containing block.
void ReflowInput::CalculateHypotheticalPosition(
    nsPresContext* aPresContext, nsPlaceholderFrame* aPlaceholderFrame,
    const ReflowInput* aCBReflowInput, nsHypotheticalPosition& aHypotheticalPos,
    LayoutFrameType aFrameType) const {
  NS_ASSERTION(mStyleDisplay->mOriginalDisplay != StyleDisplay::None,
               "mOriginalDisplay has not been properly initialized");

  // Find the nearest containing block frame to the placeholder frame,
  // and its inline-start edge and width.
  nscoord blockIStartContentEdge;
  // Dummy writing mode for blockContentSize, will be changed as needed by
  // GetHypotheticalBoxContainer.
  WritingMode cbwm = aCBReflowInput->GetWritingMode();
  LogicalSize blockContentSize(cbwm);
  nsIFrame* containingBlock = GetHypotheticalBoxContainer(
      aPlaceholderFrame, blockIStartContentEdge, blockContentSize);
  // Now blockContentSize is in containingBlock's writing mode.

  // If it's a replaced element and it has a 'auto' value for
  //'inline size', see if we can get the intrinsic size. This will allow
  // us to exactly determine both the inline edges
  WritingMode wm = containingBlock->GetWritingMode();

  const auto& styleISize = mStylePosition->ISize(wm);
  bool isAutoISize = styleISize.IsAuto();
  nsSize intrinsicSize;
  bool knowIntrinsicSize = false;
  if (NS_FRAME_IS_REPLACED(mFrameType) && isAutoISize) {
    // See if we can get the intrinsic size of the element
    knowIntrinsicSize = GetIntrinsicSizeFor(mFrame, intrinsicSize, aFrameType);
  }

  // See if we can calculate what the box inline size would have been if
  // the element had been in the flow
  nscoord boxISize;
  bool knowBoxISize = false;
  if ((StyleDisplay::Inline == mStyleDisplay->mOriginalDisplay) &&
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
    CalculateBorderPaddingMargin(eLogicalAxisInline, blockContentSize.ISize(wm),
                                 &insideBoxSizing, &outsideBoxSizing);

    if (NS_FRAME_IS_REPLACED(mFrameType) && isAutoISize) {
      // It's a replaced element with an 'auto' inline size so the box
      // inline size is its intrinsic size plus any border/padding/margin
      if (knowIntrinsicSize) {
        boxISize = LogicalSize(wm, intrinsicSize).ISize(wm) + outsideBoxSizing +
                   insideBoxSizing;
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
      boxISize = ComputeISizeValue(blockContentSize.ISize(wm), insideBoxSizing,
                                   outsideBoxSizing, styleISize) +
                 insideBoxSizing + outsideBoxSizing;
      knowBoxISize = true;
    }
  }

  // Get the placeholder x-offset and y-offset in the coordinate
  // space of its containing block
  // XXXbz the placeholder is not fully reflowed yet if our containing block is
  // relatively positioned...
  nsSize containerSize =
      containingBlock->GetStateBits() & NS_FRAME_IN_REFLOW
          ? aCBReflowInput->ComputedSizeAsContainerIfConstrained()
          : containingBlock->GetSize();
  LogicalPoint placeholderOffset(
      wm, aPlaceholderFrame->GetOffsetToIgnoringScrolling(containingBlock),
      containerSize);

  // First, determine the hypothetical box's mBStart.  We want to check the
  // content insertion frame of containingBlock for block-ness, but make
  // sure to compute all coordinates in the coordinate system of
  // containingBlock.
  nsBlockFrame* blockFrame =
      do_QueryFrame(containingBlock->GetContentInsertionFrame());
  if (blockFrame) {
    // Use a null containerSize to convert a LogicalPoint functioning as a
    // vector into a physical nsPoint vector.
    const nsSize nullContainerSize;
    LogicalPoint blockOffset(
        wm, blockFrame->GetOffsetToIgnoringScrolling(containingBlock),
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
      nsBlockFrame::LineIterator lineBox = iter.GetLine();

      // How we determine the hypothetical box depends on whether the element
      // would have been inline-level or block-level
      LogicalRect lineBounds = lineBox->GetBounds().ConvertTo(
          wm, lineBox->mWritingMode, lineBox->mContainerSize);
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
          nsIFrame* firstFrame = lineBox->mFirstChild;
          bool allEmpty = false;
          if (firstFrame == aPlaceholderFrame) {
            aPlaceholderFrame->SetLineIsEmptySoFar(true);
            allEmpty = true;
          } else {
            auto prev = aPlaceholderFrame->GetPrevSibling();
            if (prev && prev->IsPlaceholderFrame()) {
              auto ph = static_cast<nsPlaceholderFrame*>(prev);
              if (ph->GetLineIsEmptySoFar(&allEmpty)) {
                aPlaceholderFrame->SetLineIsEmptySoFar(allEmpty);
              }
            }
          }
          if (!allEmpty) {
            bool found = false;
            while (firstFrame) {  // See bug 223064
              allEmpty = AreAllEarlierInFlowFramesEmpty(
                  firstFrame, aPlaceholderFrame, &found);
              if (found || !allEmpty) {
                break;
              }
              firstFrame = firstFrame->GetNextSibling();
            }
            aPlaceholderFrame->SetLineIsEmptySoFar(allEmpty);
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
            aHypotheticalPos.mBStart = lineBounds.BEnd(wm) + blockOffset.B(wm);
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
  if (mStyleDisplay->IsOriginalDisplayInlineOutsideStyle() ||
      mFlags.mIOffsetsNeedCSSAlign) {
    // The placeholder represents the IStart edge of the hypothetical box.
    // (Or if mFlags.mIOffsetsNeedCSSAlign is set, it represents the IStart
    // edge of the Alignment Container.)
    aHypotheticalPos.mIStart = placeholderOffset.I(wm);
  } else {
    aHypotheticalPos.mIStart = blockIStartContentEdge;
  }

  // The current coordinate space is that of the nearest block to the
  // placeholder. Convert to the coordinate space of the absolute containing
  // block.
  nsPoint cbOffset =
      containingBlock->GetOffsetToIgnoringScrolling(aCBReflowInput->mFrame);

  nsSize reflowSize = aCBReflowInput->ComputedSizeAsContainerIfConstrained();
  LogicalPoint logCBOffs(wm, cbOffset, reflowSize - containerSize);
  aHypotheticalPos.mIStart += logCBOffs.I(wm);
  aHypotheticalPos.mBStart += logCBOffs.B(wm);

  // The specified offsets are relative to the absolute containing block's
  // padding edge and our current values are relative to the border edge, so
  // translate.
  LogicalMargin border = aCBReflowInput->ComputedLogicalBorderPadding() -
                         aCBReflowInput->ComputedLogicalPadding();
  border = border.ConvertTo(wm, aCBReflowInput->GetWritingMode());
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
    CalculateBorderPaddingMargin(eLogicalAxisBlock, blockContentSize.BSize(wm),
                                 &insideBoxSizing, &outsideBoxSizing);

    nscoord boxBSize;
    const auto& styleBSize = mStylePosition->BSize(wm);
    if (styleBSize.BehavesLikeInitialValueOnBlockAxis()) {
      if (NS_FRAME_IS_REPLACED(mFrameType) && knowIntrinsicSize) {
        // It's a replaced element with an 'auto' block size so the box
        // block size is its intrinsic size plus any border/padding/margin
        boxBSize = LogicalSize(wm, intrinsicSize).BSize(wm) + outsideBoxSizing +
                   insideBoxSizing;
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
      boxBSize = nsLayoutUtils::ComputeBSizeValue(
                     blockContentSize.BSize(wm), insideBoxSizing,
                     styleBSize.AsLengthPercentage()) +
                 insideBoxSizing + outsideBoxSizing;
    }

    LogicalSize boxSize(wm, knowBoxISize ? boxISize : 0, boxBSize);

    LogicalPoint origin(wm, aHypotheticalPos.mIStart, aHypotheticalPos.mBStart);
    origin =
        origin.ConvertTo(cbwm, wm, reflowSize - boxSize.GetPhysicalSize(wm));

    aHypotheticalPos.mIStart = origin.I(cbwm);
    aHypotheticalPos.mBStart = origin.B(cbwm);
    aHypotheticalPos.mWritingMode = cbwm;
  } else {
    aHypotheticalPos.mWritingMode = wm;
  }
}

void ReflowInput::InitAbsoluteConstraints(nsPresContext* aPresContext,
                                          const ReflowInput* aCBReflowInput,
                                          const LogicalSize& aCBSize,
                                          LayoutFrameType aFrameType) {
  WritingMode wm = GetWritingMode();
  WritingMode cbwm = aCBReflowInput->GetWritingMode();
  NS_WARNING_ASSERTION(aCBSize.BSize(cbwm) != NS_UNCONSTRAINEDSIZE,
                       "containing block bsize must be constrained");

  NS_ASSERTION(aFrameType != LayoutFrameType::Table,
               "InitAbsoluteConstraints should not be called on table frames");
  NS_ASSERTION(mFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW,
               "Why are we here?");

  const auto& styleOffset = mStylePosition->mOffset;
  bool iStartIsAuto = styleOffset.GetIStart(cbwm).IsAuto();
  bool iEndIsAuto = styleOffset.GetIEnd(cbwm).IsAuto();
  bool bStartIsAuto = styleOffset.GetBStart(cbwm).IsAuto();
  bool bEndIsAuto = styleOffset.GetBEnd(cbwm).IsAuto();

  // If both 'left' and 'right' are 'auto' or both 'top' and 'bottom' are
  // 'auto', then compute the hypothetical box position where the element would
  // have been if it had been in the flow
  nsHypotheticalPosition hypotheticalPos;
  if ((iStartIsAuto && iEndIsAuto) || (bStartIsAuto && bEndIsAuto)) {
    nsPlaceholderFrame* placeholderFrame = mFrame->GetPlaceholderFrame();
    MOZ_ASSERT(placeholderFrame, "no placeholder frame");

    if (placeholderFrame->HasAnyStateBits(
            PLACEHOLDER_STATICPOS_NEEDS_CSSALIGN)) {
      DebugOnly<nsIFrame*> placeholderParent = placeholderFrame->GetParent();
      MOZ_ASSERT(placeholderParent, "shouldn't have unparented placeholders");
      MOZ_ASSERT(placeholderParent->IsFlexOrGridContainer(),
                 "This flag should only be set on grid/flex children");

      // If the (as-yet unknown) static position will determine the inline
      // and/or block offsets, set flags to note those offsets aren't valid
      // until we can do CSS Box Alignment on the OOF frame.
      mFlags.mIOffsetsNeedCSSAlign = (iStartIsAuto && iEndIsAuto);
      mFlags.mBOffsetsNeedCSSAlign = (bStartIsAuto && bEndIsAuto);
    }

    if (mFlags.mStaticPosIsCBOrigin) {
      hypotheticalPos.mWritingMode = cbwm;
      hypotheticalPos.mIStart = nscoord(0);
      hypotheticalPos.mBStart = nscoord(0);
    } else {
      // XXXmats all this is broken for orthogonal writing-modes: bug 1521988.
      CalculateHypotheticalPosition(aPresContext, placeholderFrame,
                                    aCBReflowInput, hypotheticalPos,
                                    aFrameType);
      if (aCBReflowInput->mFrame->IsGridContainerFrame()) {
        // 'hypotheticalPos' is relative to the padding rect of the CB *frame*.
        // In grid layout the CB is the grid area rectangle, so we translate
        // 'hypotheticalPos' to be relative that rectangle here.
        nsRect cb = nsGridContainerFrame::GridItemCB(mFrame);
        nscoord left(0);
        nscoord right(0);
        if (cbwm.IsBidiLTR()) {
          left = cb.X();
        } else {
          right = aCBReflowInput->ComputedWidth() +
                  aCBReflowInput->ComputedPhysicalPadding().LeftRight() -
                  cb.XMost();
        }
        LogicalMargin offsets(cbwm, nsMargin(cb.Y(), right, nscoord(0), left));
        hypotheticalPos.mIStart -= offsets.IStart(cbwm);
        hypotheticalPos.mBStart -= offsets.BStart(cbwm);
      }
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
    offsets.IStart(cbwm) = nsLayoutUtils::ComputeCBDependentValue(
        cbSize.ISize(cbwm), styleOffset.GetIStart(cbwm));
  }
  if (iEndIsAuto) {
    offsets.IEnd(cbwm) = 0;
  } else {
    offsets.IEnd(cbwm) = nsLayoutUtils::ComputeCBDependentValue(
        cbSize.ISize(cbwm), styleOffset.GetIEnd(cbwm));
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
    offsets.BStart(cbwm) = nsLayoutUtils::ComputeBSizeDependentValue(
        cbSize.BSize(cbwm), styleOffset.GetBStart(cbwm));
  }
  if (bEndIsAuto) {
    offsets.BEnd(cbwm) = 0;
  } else {
    offsets.BEnd(cbwm) = nsLayoutUtils::ComputeBSizeDependentValue(
        cbSize.BSize(cbwm), styleOffset.GetBEnd(cbwm));
  }

  if (bStartIsAuto && bEndIsAuto) {
    // Treat 'top' like 'static-position'
    offsets.BStart(cbwm) = hypotheticalPos.mBStart;
    bStartIsAuto = false;
  }

  SetComputedLogicalOffsets(offsets.ConvertTo(wm, cbwm));

  typedef nsIFrame::ComputeSizeFlags ComputeSizeFlags;
  ComputeSizeFlags computeSizeFlags = ComputeSizeFlags::eDefault;
  if (mFlags.mIClampMarginBoxMinSize) {
    computeSizeFlags = ComputeSizeFlags(
        computeSizeFlags | ComputeSizeFlags::eIClampMarginBoxMinSize);
  }
  if (mFlags.mBClampMarginBoxMinSize) {
    computeSizeFlags = ComputeSizeFlags(
        computeSizeFlags | ComputeSizeFlags::eBClampMarginBoxMinSize);
  }
  if (mFlags.mApplyAutoMinSize) {
    computeSizeFlags = ComputeSizeFlags(computeSizeFlags |
                                        ComputeSizeFlags::eIApplyAutoMinSize);
  }
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
    AutoMaybeDisableFontInflation an(mFrame);

    computedSize = mFrame->ComputeSize(
        mRenderingContext, wm, cbSize.ConvertTo(wm, cbwm),
        cbSize.ConvertTo(wm, cbwm).ISize(wm),  // XXX or AvailableISize()?
        ComputedLogicalMargin().Size(wm) + ComputedLogicalOffsets().Size(wm),
        ComputedLogicalBorderPadding().Size(wm) -
            ComputedLogicalPadding().Size(wm),
        ComputedLogicalPadding().Size(wm), computeSizeFlags);
    ComputedISize() = computedSize.ISize(wm);
    ComputedBSize() = computedSize.BSize(wm);
    NS_ASSERTION(ComputedISize() >= 0, "Bogus inline-size");
    NS_ASSERTION(
        ComputedBSize() == NS_UNCONSTRAINEDSIZE || ComputedBSize() >= 0,
        "Bogus block-size");
  }
  computedSize = computedSize.ConvertTo(cbwm, wm);

  // XXX Now that we have ComputeSize, can we condense many of the
  // branches off of widthIsAuto?

  LogicalMargin margin = ComputedLogicalMargin().ConvertTo(cbwm, wm);
  const LogicalMargin borderPadding =
      ComputedLogicalBorderPadding().ConvertTo(cbwm, wm);

  bool iSizeIsAuto = mStylePosition->ISize(cbwm).IsAuto();
  bool marginIStartIsAuto = false;
  bool marginIEndIsAuto = false;
  bool marginBStartIsAuto = false;
  bool marginBEndIsAuto = false;
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
      offsets.IStart(cbwm) = cbSize.ISize(cbwm) - offsets.IEnd(cbwm) -
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
      offsets.IEnd(cbwm) = cbSize.ISize(cbwm) - offsets.IStart(cbwm) -
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
                          borderPadding.IStartEnd(cbwm) -
                          offsets.IStartEnd(cbwm);
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
    marginIStartIsAuto = mStyleMargin->mMargin.GetIStart(cbwm).IsAuto();
    marginIEndIsAuto = mStyleMargin->mMargin.GetIEnd(cbwm).IsAuto();

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

  bool bSizeIsAuto =
      mStylePosition->BSize(cbwm).BehavesLikeInitialValueOnBlockAxis();
  if (bStartIsAuto) {
    // solve for block-start
    if (bSizeIsAuto) {
      offsets.BStart(cbwm) = NS_AUTOOFFSET;
    } else {
      offsets.BStart(cbwm) = cbSize.BSize(cbwm) - margin.BStartEnd(cbwm) -
                             borderPadding.BStartEnd(cbwm) -
                             computedSize.BSize(cbwm) - offsets.BEnd(cbwm);
    }
  } else if (bEndIsAuto) {
    // solve for block-end
    if (bSizeIsAuto) {
      offsets.BEnd(cbwm) = NS_AUTOOFFSET;
    } else {
      offsets.BEnd(cbwm) = cbSize.BSize(cbwm) - margin.BStartEnd(cbwm) -
                           borderPadding.BStartEnd(cbwm) -
                           computedSize.BSize(cbwm) - offsets.BStart(cbwm);
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
    marginBStartIsAuto = mStyleMargin->mMargin.GetBStart(cbwm).IsAuto();
    marginBEndIsAuto = mStyleMargin->mMargin.GetBEnd(cbwm).IsAuto();

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

  LogicalMargin marginInOurWM = margin.ConvertTo(wm, cbwm);
  SetComputedLogicalMargin(marginInOurWM);

  // If we have auto margins, update our UsedMarginProperty. The property
  // will have already been created by InitOffsets if it is needed.
  if (marginIStartIsAuto || marginIEndIsAuto || marginBStartIsAuto ||
      marginBEndIsAuto) {
    nsMargin* propValue = mFrame->GetProperty(nsIFrame::UsedMarginProperty());
    MOZ_ASSERT(propValue,
               "UsedMarginProperty should have been created "
               "by InitOffsets.");
    *propValue = marginInOurWM.GetPhysicalMargin(wm);
  }
}

// This will not be converted to abstract coordinates because it's only
// used in CalcQuirkContainingBlockHeight
static nscoord GetBlockMarginBorderPadding(const ReflowInput* aReflowInput) {
  nscoord result = 0;
  if (!aReflowInput) return result;

  // zero auto margins
  nsMargin margin = aReflowInput->ComputedPhysicalMargin();
  if (NS_AUTOMARGIN == margin.top) margin.top = 0;
  if (NS_AUTOMARGIN == margin.bottom) margin.bottom = 0;

  result += margin.top + margin.bottom;
  result += aReflowInput->ComputedPhysicalBorderPadding().top +
            aReflowInput->ComputedPhysicalBorderPadding().bottom;

  return result;
}

/* Get the height based on the viewport of the containing block specified
 * in aReflowInput when the containing block has mComputedHeight ==
 * NS_UNCONSTRAINEDSIZE This will walk up the chain of containing blocks looking
 * for a computed height until it finds the canvas frame, or it encounters a
 * frame that is not a block, area, or scroll frame. This handles compatibility
 * with IE (see bug 85016 and bug 219693)
 *
 * When we encounter scrolledContent block frames, we skip over them,
 * since they are guaranteed to not be useful for computing the containing
 * block.
 *
 * See also IsQuirkContainingBlockHeight.
 */
static nscoord CalcQuirkContainingBlockHeight(
    const ReflowInput* aCBReflowInput) {
  const ReflowInput* firstAncestorRI = nullptr;   // a candidate for html frame
  const ReflowInput* secondAncestorRI = nullptr;  // a candidate for body frame

  // initialize the default to NS_UNCONSTRAINEDSIZE as this is the containings
  // block computed height when this function is called. It is possible that we
  // don't alter this height especially if we are restricted to one level
  nscoord result = NS_UNCONSTRAINEDSIZE;

  const ReflowInput* ri = aCBReflowInput;
  for (; ri; ri = ri->mParentReflowInput) {
    LayoutFrameType frameType = ri->mFrame->Type();
    // if the ancestor is auto height then skip it and continue up if it
    // is the first block frame and possibly the body/html
    if (LayoutFrameType::Block == frameType ||
#ifdef MOZ_XUL
        LayoutFrameType::XULLabel == frameType ||
#endif
        LayoutFrameType::Scroll == frameType) {

      secondAncestorRI = firstAncestorRI;
      firstAncestorRI = ri;

      // If the current frame we're looking at is positioned, we don't want to
      // go any further (see bug 221784).  The behavior we want here is: 1) If
      // not auto-height, use this as the percentage base.  2) If auto-height,
      // keep looking, unless the frame is positioned.
      if (NS_UNCONSTRAINEDSIZE == ri->ComputedHeight()) {
        if (ri->mFrame->IsAbsolutelyPositioned(ri->mStyleDisplay)) {
          break;
        } else {
          continue;
        }
      }
    } else if (LayoutFrameType::Canvas == frameType) {
      // Always continue on to the height calculation
    } else if (LayoutFrameType::PageContent == frameType) {
      nsIFrame* prevInFlow = ri->mFrame->GetPrevInFlow();
      // only use the page content frame for a height basis if it is the first
      // in flow
      if (prevInFlow) break;
    } else {
      break;
    }

    // if the ancestor is the page content frame then the percent base is
    // the avail height, otherwise it is the computed height
    result = (LayoutFrameType::PageContent == frameType) ? ri->AvailableHeight()
                                                         : ri->ComputedHeight();
    // if unconstrained - don't sutract borders - would result in huge height
    if (NS_UNCONSTRAINEDSIZE == result) return result;

    // if we got to the canvas or page content frame, then subtract out
    // margin/border/padding for the BODY and HTML elements
    if ((LayoutFrameType::Canvas == frameType) ||
        (LayoutFrameType::PageContent == frameType)) {
      result -= GetBlockMarginBorderPadding(firstAncestorRI);
      result -= GetBlockMarginBorderPadding(secondAncestorRI);

#ifdef DEBUG
      // make sure the first ancestor is the HTML and the second is the BODY
      if (firstAncestorRI) {
        nsIContent* frameContent = firstAncestorRI->mFrame->GetContent();
        if (frameContent) {
          NS_ASSERTION(frameContent->IsHTMLElement(nsGkAtoms::html),
                       "First ancestor is not HTML");
        }
      }
      if (secondAncestorRI) {
        nsIContent* frameContent = secondAncestorRI->mFrame->GetContent();
        if (frameContent) {
          NS_ASSERTION(frameContent->IsHTMLElement(nsGkAtoms::body),
                       "Second ancestor is not BODY");
        }
      }
#endif

    }
    // if we got to the html frame (a block child of the canvas) ...
    else if (LayoutFrameType::Block == frameType && ri->mParentReflowInput &&
             ri->mParentReflowInput->mFrame->IsCanvasFrame()) {
      // ... then subtract out margin/border/padding for the BODY element
      result -= GetBlockMarginBorderPadding(secondAncestorRI);
    }
    break;
  }

  // Make sure not to return a negative height here!
  return std::max(result, 0);
}

// Called by InitConstraints() to compute the containing block rectangle for
// the element. Handles the special logic for absolutely positioned elements
LogicalSize ReflowInput::ComputeContainingBlockRectangle(
    nsPresContext* aPresContext, const ReflowInput* aContainingBlockRI) const {
  // Unless the element is absolutely positioned, the containing block is
  // formed by the content edge of the nearest block-level ancestor
  LogicalSize cbSize = aContainingBlockRI->ComputedSize();

  WritingMode wm = aContainingBlockRI->GetWritingMode();

  // mFrameType for abs-pos tables is NS_CSS_FRAME_TYPE_BLOCK, so we need to
  // special case them here.
  if (NS_FRAME_GET_TYPE(mFrameType) == NS_CSS_FRAME_TYPE_ABSOLUTE ||
      (mFrame->IsTableFrame() &&
       mFrame->IsAbsolutelyPositioned(mStyleDisplay) &&
       (mFrame->GetParent()->GetStateBits() & NS_FRAME_OUT_OF_FLOW))) {
    // See if the ancestor is block-level or inline-level
    if (NS_FRAME_GET_TYPE(aContainingBlockRI->mFrameType) ==
        NS_CSS_FRAME_TYPE_INLINE) {
      // Base our size on the actual size of the frame.  In cases when this is
      // completely bogus (eg initial reflow), this code shouldn't even be
      // called, since the code in nsInlineFrame::Reflow will pass in
      // the containing block dimensions to our constructor.
      // XXXbz we should be taking the in-flows into account too, but
      // that's very hard.

      LogicalMargin computedBorder =
          aContainingBlockRI->ComputedLogicalBorderPadding() -
          aContainingBlockRI->ComputedLogicalPadding();
      cbSize.ISize(wm) =
          aContainingBlockRI->mFrame->ISize(wm) - computedBorder.IStartEnd(wm);
      NS_ASSERTION(cbSize.ISize(wm) >= 0, "Negative containing block isize!");
      cbSize.BSize(wm) =
          aContainingBlockRI->mFrame->BSize(wm) - computedBorder.BStartEnd(wm);
      NS_ASSERTION(cbSize.BSize(wm) >= 0, "Negative containing block bsize!");
    } else {
      // If the ancestor is block-level, the containing block is formed by the
      // padding edge of the ancestor
      cbSize.ISize(wm) +=
          aContainingBlockRI->ComputedLogicalPadding().IStartEnd(wm);
      cbSize.BSize(wm) +=
          aContainingBlockRI->ComputedLogicalPadding().BStartEnd(wm);
    }
  } else {
    auto IsQuirky = [&](const StyleSize& aSize) -> bool {
      return aSize.ConvertsToPercentage() &&
             !aSize.AsLengthPercentage().was_calc;
    };
    // an element in quirks mode gets a containing block based on looking for a
    // parent with a non-auto height if the element has a percent height
    // Note: We don't emulate this quirk for percents in calc() or in
    // vertical writing modes.
    if (!wm.IsVertical() && NS_UNCONSTRAINEDSIZE == cbSize.BSize(wm)) {
      if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
          (IsQuirky(mStylePosition->mHeight) ||
           (mFrame->IsTableWrapperFrame() &&
            IsQuirky(mFrame->PrincipalChildList()
                         .FirstChild()
                         ->StylePosition()
                         ->mHeight)))) {
        cbSize.BSize(wm) = CalcQuirkContainingBlockHeight(aContainingBlockRI);
      }
    }
  }

  return cbSize.ConvertTo(GetWritingMode(), wm);
}

static eNormalLineHeightControl GetNormalLineHeightCalcControl(void) {
  if (sNormalLineHeightControl == eUninitialized) {
    // browser.display.normal_lineheight_calc_control is not user
    // changeable, so no need to register callback for it.
    int32_t val = Preferences::GetInt(
        "browser.display.normal_lineheight_calc_control", eNoExternalLeading);
    sNormalLineHeightControl = static_cast<eNormalLineHeightControl>(val);
  }
  return sNormalLineHeightControl;
}

static inline bool IsSideCaption(nsIFrame* aFrame,
                                 const nsStyleDisplay* aStyleDisplay,
                                 WritingMode aWM) {
  if (aStyleDisplay->mDisplay != StyleDisplay::TableCaption) {
    return false;
  }
  uint8_t captionSide = aFrame->StyleTableBorder()->mCaptionSide;
  return captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
         captionSide == NS_STYLE_CAPTION_SIDE_RIGHT;
}

// XXX refactor this code to have methods for each set of properties
// we are computing: width,height,line-height; margin; offsets

void ReflowInput::InitConstraints(
    nsPresContext* aPresContext, const Maybe<LogicalSize>& aContainingBlockSize,
    const nsMargin* aBorder, const nsMargin* aPadding,
    LayoutFrameType aFrameType) {
  WritingMode wm = GetWritingMode();
  LogicalSize cbSize = aContainingBlockSize.valueOr(
      LogicalSize(mWritingMode, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE));
  DISPLAY_INIT_CONSTRAINTS(mFrame, this, cbSize.ISize(wm), cbSize.BSize(wm),
                           aBorder, aPadding);

  // If this is a reflow root, then set the computed width and
  // height equal to the available space
  if (nullptr == mParentReflowInput || mFlags.mDummyParentReflowInput) {
    // XXXldb This doesn't mean what it used to!
    InitOffsets(wm, cbSize.ISize(wm), aFrameType, mFlags, aBorder, aPadding,
                mStyleDisplay);
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
    // Get the containing block reflow input
    const ReflowInput* cbri = mCBReflowInput;
    MOZ_ASSERT(cbri, "no containing block");
    MOZ_ASSERT(mFrame->GetParent());

    // If we weren't given a containing block size, then compute one.
    if (aContainingBlockSize.isNothing()) {
      cbSize = ComputeContainingBlockRectangle(aPresContext, cbri);
    }

    // See if the containing block height is based on the size of its
    // content
    if (NS_UNCONSTRAINEDSIZE == cbSize.BSize(wm)) {
      // See if the containing block is a cell frame which needs
      // to use the mComputedHeight of the cell instead of what the cell block
      // passed in.
      // XXX It seems like this could lead to bugs with min-height and friends
      if (cbri->mParentReflowInput) {
        if (cbri->mFrame->IsTableCellFrame()) {
          // use the cell's computed block size
          cbSize.BSize(wm) = cbri->ComputedSize(wm).BSize(wm);
        }
      }
    }

    // XXX Might need to also pass the CB height (not width) for page boxes,
    // too, if we implement them.

    // For calculating positioning offsets, margins, borders and
    // padding, we use the writing mode of the containing block
    WritingMode cbwm = cbri->GetWritingMode();
    InitOffsets(cbwm, cbSize.ConvertTo(cbwm, wm).ISize(cbwm), aFrameType,
                mFlags, aBorder, aPadding, mStyleDisplay);

    // For calculating the size of this box, we use its own writing mode
    const auto& blockSize = mStylePosition->BSize(wm);
    bool isAutoBSize = blockSize.BehavesLikeInitialValueOnBlockAxis();

    // Check for a percentage based block size and a containing block
    // block size that depends on the content block size
    if (blockSize.HasPercent()) {
      if (NS_UNCONSTRAINEDSIZE == cbSize.BSize(wm)) {
        // this if clause enables %-blockSize on replaced inline frames,
        // such as images.  See bug 54119.  The else clause "blockSizeUnit =
        // eStyleUnit_Auto;" used to be called exclusively.
        if (NS_FRAME_REPLACED(NS_CSS_FRAME_TYPE_INLINE) == mFrameType ||
            NS_FRAME_REPLACED_CONTAINS_BLOCK(NS_CSS_FRAME_TYPE_INLINE) ==
                mFrameType) {
          // Get the containing block reflow input
          NS_ASSERTION(nullptr != cbri, "no containing block");
          // in quirks mode, get the cb height using the special quirk method
          if (!wm.IsVertical() &&
              eCompatibility_NavQuirks == aPresContext->CompatibilityMode()) {
            if (!cbri->mFrame->IsTableCellFrame()) {
              cbSize.BSize(wm) = CalcQuirkContainingBlockHeight(cbri);
              if (cbSize.BSize(wm) == NS_UNCONSTRAINEDSIZE) {
                isAutoBSize = true;
              }
            } else {
              isAutoBSize = true;
            }
          }
          // in standard mode, use the cb block size.  if it's "auto",
          // as will be the case by default in BODY, use auto block size
          // as per CSS2 spec.
          else {
            nscoord computedBSize = cbri->ComputedSize(wm).BSize(wm);
            if (NS_UNCONSTRAINEDSIZE != computedBSize) {
              cbSize.BSize(wm) = computedBSize;
            } else {
              isAutoBSize = true;
            }
          }
        } else {
          // default to interpreting the blockSize like 'auto'
          isAutoBSize = true;
        }
      }
    }

    // Compute our offsets if the element is relatively positioned.  We
    // need the correct containing block inline-size and block-size
    // here, which is why we need to do it after all the quirks-n-such
    // above. (If the element is sticky positioned, we need to wait
    // until the scroll container knows its size, so we compute offsets
    // from StickyScrollContainer::UpdatePositions.)
    if (mStyleDisplay->IsRelativelyPositioned(mFrame) &&
        NS_STYLE_POSITION_RELATIVE == mStyleDisplay->mPosition) {
      ComputeRelativeOffsets(cbwm, mFrame, cbSize.ConvertTo(cbwm, wm),
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
      const auto& inlineSize = mStylePosition->ISize(wm);
      bool isAutoISize = inlineSize.IsAuto();
      if ((StyleDisplay::TableRow == mStyleDisplay->mDisplay) ||
          (StyleDisplay::TableRowGroup == mStyleDisplay->mDisplay)) {
        // 'inlineSize' property doesn't apply to table rows and row groups
        isAutoISize = true;
        rowOrRowGroup = true;
      }

      // calc() with both percentages and lengths act like auto on internal
      // table elements
      if (isAutoISize || inlineSize.HasLengthAndPercentage()) {
        ComputedISize() = AvailableISize();

        if ((ComputedISize() != NS_UNCONSTRAINEDSIZE) && !rowOrRowGroup) {
          // Internal table elements don't have margins. Only tables and
          // cells have border and padding
          ComputedISize() -= ComputedLogicalBorderPadding().IStartEnd(wm);
          if (ComputedISize() < 0) ComputedISize() = 0;
        }
        NS_ASSERTION(ComputedISize() >= 0, "Bogus computed isize");

      } else {
        ComputedISize() = ComputeISizeValue(
            cbSize.ISize(wm), mStylePosition->mBoxSizing, inlineSize);
      }

      // Calculate the computed block size
      if ((StyleDisplay::TableColumn == mStyleDisplay->mDisplay) ||
          (StyleDisplay::TableColumnGroup == mStyleDisplay->mDisplay)) {
        // 'blockSize' property doesn't apply to table columns and column groups
        isAutoBSize = true;
      }
      // calc() with both percentages and lengths acts like 'auto' on internal
      // table elements
      if (isAutoBSize || blockSize.HasLengthAndPercentage()) {
        ComputedBSize() = NS_UNCONSTRAINEDSIZE;
      } else {
        ComputedBSize() =
            ComputeBSizeValue(cbSize.BSize(wm), mStylePosition->mBoxSizing,
                              blockSize.AsLengthPercentage());
      }

      // Doesn't apply to table elements
      ComputedMinWidth() = ComputedMinHeight() = 0;
      ComputedMaxWidth() = ComputedMaxHeight() = NS_UNCONSTRAINEDSIZE;

    } else if (NS_FRAME_GET_TYPE(mFrameType) == NS_CSS_FRAME_TYPE_ABSOLUTE) {
      // XXX not sure if this belongs here or somewhere else - cwk
      InitAbsoluteConstraints(aPresContext, cbri,
                              cbSize.ConvertTo(cbri->GetWritingMode(), wm),
                              aFrameType);
    } else {
      AutoMaybeDisableFontInflation an(mFrame);

      bool isBlock = NS_CSS_FRAME_TYPE_BLOCK == NS_FRAME_GET_TYPE(mFrameType);
      typedef nsIFrame::ComputeSizeFlags ComputeSizeFlags;
      ComputeSizeFlags computeSizeFlags =
          isBlock ? ComputeSizeFlags::eDefault : ComputeSizeFlags::eShrinkWrap;
      if (mFlags.mIClampMarginBoxMinSize) {
        computeSizeFlags = ComputeSizeFlags(
            computeSizeFlags | ComputeSizeFlags::eIClampMarginBoxMinSize);
      }
      if (mFlags.mBClampMarginBoxMinSize) {
        computeSizeFlags = ComputeSizeFlags(
            computeSizeFlags | ComputeSizeFlags::eBClampMarginBoxMinSize);
      }
      if (mFlags.mApplyAutoMinSize) {
        computeSizeFlags = ComputeSizeFlags(
            computeSizeFlags | ComputeSizeFlags::eIApplyAutoMinSize);
      }
      if (mFlags.mShrinkWrap) {
        computeSizeFlags =
            ComputeSizeFlags(computeSizeFlags | ComputeSizeFlags::eShrinkWrap);
      }
      if (mFlags.mUseAutoBSize) {
        computeSizeFlags = ComputeSizeFlags(computeSizeFlags |
                                            ComputeSizeFlags::eUseAutoBSize);
      }

      nsIFrame* alignCB = mFrame->GetParent();
      if (alignCB->IsTableWrapperFrame() && alignCB->GetParent()) {
        // XXX grid-specific for now; maybe remove this check after we address
        // bug 799725
        if (alignCB->GetParent()->IsGridContainerFrame()) {
          alignCB = alignCB->GetParent();
        }
      }
      if (alignCB->IsGridContainerFrame()) {
        // Shrink-wrap grid items that will be aligned (rather than stretched)
        // in its inline axis.
        auto inlineAxisAlignment =
            wm.IsOrthogonalTo(cbwm)
                ? mStylePosition->UsedAlignSelf(alignCB->Style())
                : mStylePosition->UsedJustifySelf(alignCB->Style());
        if ((inlineAxisAlignment != NS_STYLE_ALIGN_STRETCH &&
             inlineAxisAlignment != NS_STYLE_ALIGN_NORMAL) ||
            mStyleMargin->mMargin.GetIStart(wm).IsAuto() ||
            mStyleMargin->mMargin.GetIEnd(wm).IsAuto()) {
          computeSizeFlags = ComputeSizeFlags(computeSizeFlags |
                                              ComputeSizeFlags::eShrinkWrap);
        }
      } else {
        // Make sure legend frames with display:block and width:auto still
        // shrink-wrap.
        // Also shrink-wrap blocks that are orthogonal to their container.
        if (isBlock &&
            ((aFrameType == LayoutFrameType::Legend &&
              mFrame->Style()->GetPseudoType() !=
                  PseudoStyleType::scrolledContent) ||
             (aFrameType == LayoutFrameType::Scroll &&
              mFrame->GetContentInsertionFrame()->IsLegendFrame()) ||
             (mCBReflowInput &&
              mCBReflowInput->GetWritingMode().IsOrthogonalTo(mWritingMode)))) {
          computeSizeFlags = ComputeSizeFlags(computeSizeFlags |
                                              ComputeSizeFlags::eShrinkWrap);
        }

        if (alignCB->IsFlexContainerFrame()) {
          computeSizeFlags = ComputeSizeFlags(computeSizeFlags |
                                              ComputeSizeFlags::eShrinkWrap);

          // If we're inside of a flex container that needs to measure our
          // auto BSize, pass that information along to ComputeSize().
          if (mFlags.mIsFlexContainerMeasuringBSize) {
            computeSizeFlags = ComputeSizeFlags(
                computeSizeFlags | ComputeSizeFlags::eUseAutoBSize);
          }
        } else {
          MOZ_ASSERT(!mFlags.mIsFlexContainerMeasuringBSize,
                     "We're not in a flex container, so the flag "
                     "'mIsFlexContainerMeasuringBSize' shouldn't be set");
        }
      }

      if (cbSize.ISize(wm) == NS_UNCONSTRAINEDSIZE) {
        // For orthogonal flows, where we found a parent orthogonal-limit
        // for AvailableISize() in Init(), we'll use the same here as well.
        cbSize.ISize(wm) = AvailableISize();
      }

      LogicalSize size = mFrame->ComputeSize(
          mRenderingContext, wm, cbSize, AvailableISize(),
          ComputedLogicalMargin().Size(wm),
          ComputedLogicalBorderPadding().Size(wm) -
              ComputedLogicalPadding().Size(wm),
          ComputedLogicalPadding().Size(wm), computeSizeFlags);

      ComputedISize() = size.ISize(wm);
      ComputedBSize() = size.BSize(wm);
      NS_ASSERTION(ComputedISize() >= 0, "Bogus inline-size");
      NS_ASSERTION(
          ComputedBSize() == NS_UNCONSTRAINEDSIZE || ComputedBSize() >= 0,
          "Bogus block-size");

      // Exclude inline tables, side captions, outside ::markers, flex and grid
      // items from block margin calculations.
      if (isBlock && !IsSideCaption(mFrame, mStyleDisplay, cbwm) &&
          mStyleDisplay->mDisplay != StyleDisplay::InlineTable &&
          !alignCB->IsFlexOrGridContainer() &&
          !(mFrame->Style()->GetPseudoType() == PseudoStyleType::marker &&
            mFrame->GetParent()->StyleList()->mListStylePosition ==
                NS_STYLE_LIST_STYLE_POSITION_OUTSIDE)) {
        CalculateBlockSideMargins(aFrameType);
      }
    }
  }

  // Save our containing block dimensions
  mContainingBlockSize = cbSize;
}

static void UpdateProp(nsIFrame* aFrame,
                       const FramePropertyDescriptor<nsMargin>* aProperty,
                       bool aNeeded, const nsMargin& aNewValue) {
  if (aNeeded) {
    nsMargin* propValue = aFrame->GetProperty(aProperty);
    if (propValue) {
      *propValue = aNewValue;
    } else {
      aFrame->AddProperty(aProperty, new nsMargin(aNewValue));
    }
  } else {
    aFrame->DeleteProperty(aProperty);
  }
}

void SizeComputationInput::InitOffsets(WritingMode aWM, nscoord aPercentBasis,
                                       LayoutFrameType aFrameType,
                                       ReflowInputFlags aFlags,
                                       const nsMargin* aBorder,
                                       const nsMargin* aPadding,
                                       const nsStyleDisplay* aDisplay) {
  DISPLAY_INIT_OFFSETS(mFrame, this, aPercentBasis, aWM, aBorder, aPadding);

  // Since we are in reflow, we don't need to store these properties anymore
  // unless they are dependent on width, in which case we store the new value.
  nsPresContext* presContext = mFrame->PresContext();
  mFrame->DeleteProperty(nsIFrame::UsedBorderProperty());

  // Compute margins from the specified margin style information. These
  // become the default computed values, and may be adjusted below
  // XXX fix to provide 0,0 for the top&bottom margins for
  // inline-non-replaced elements
  bool needMarginProp = ComputeMargin(aWM, aPercentBasis);
  // Note that ComputeMargin() simplistically resolves 'auto' margins to 0.
  // In formatting contexts where this isn't correct, some later code will
  // need to update the UsedMargin() property with the actual resolved value.
  // One example of this is ::CalculateBlockSideMargins().
  ::UpdateProp(mFrame, nsIFrame::UsedMarginProperty(), needMarginProp,
               ComputedPhysicalMargin());

  const nsStyleDisplay* disp = mFrame->StyleDisplayWithOptionalParam(aDisplay);
  bool isThemed = mFrame->IsThemed(disp);
  bool needPaddingProp;
  LayoutDeviceIntMargin widgetPadding;
  if (isThemed && presContext->GetTheme()->GetWidgetPadding(
                      presContext->DeviceContext(), mFrame, disp->mAppearance,
                      &widgetPadding)) {
    ComputedPhysicalPadding() = LayoutDevicePixel::ToAppUnits(
        widgetPadding, presContext->AppUnitsPerDevPixel());
    needPaddingProp = false;
  } else if (nsSVGUtils::IsInSVGTextSubtree(mFrame)) {
    ComputedPhysicalPadding().SizeTo(0, 0, 0, 0);
    needPaddingProp = false;
  } else if (aPadding) {  // padding is an input arg
    ComputedPhysicalPadding() = *aPadding;
    needPaddingProp = mFrame->StylePadding()->IsWidthDependent() ||
                      mFrame->HasAnyStateBits(NS_FRAME_REFLOW_ROOT |
                                              NS_FRAME_DYNAMIC_REFLOW_ROOT);
  } else {
    needPaddingProp = ComputePadding(aWM, aPercentBasis, aFrameType);
  }

  // Add [align|justify]-content:baseline padding contribution.
  typedef const FramePropertyDescriptor<SmallValueHolder<nscoord>>* Prop;
  auto ApplyBaselinePadding = [this, &needPaddingProp](LogicalAxis aAxis,
                                                       Prop aProp) {
    bool found;
    nscoord val = mFrame->GetProperty(aProp, &found);
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
    LayoutDeviceIntMargin border = presContext->GetTheme()->GetWidgetBorder(
        presContext->DeviceContext(), mFrame, disp->mAppearance);
    ComputedPhysicalBorderPadding() = LayoutDevicePixel::ToAppUnits(
        border, presContext->AppUnitsPerDevPixel());
  } else if (nsSVGUtils::IsInSVGTextSubtree(mFrame)) {
    ComputedPhysicalBorderPadding().SizeTo(0, 0, 0, 0);
  } else if (aBorder) {  // border is an input arg
    ComputedPhysicalBorderPadding() = *aBorder;
  } else {
    ComputedPhysicalBorderPadding() =
        mFrame->StyleBorder()->GetComputedBorder();
  }
  ComputedPhysicalBorderPadding() += ComputedPhysicalPadding();

  if (aFrameType == LayoutFrameType::Table) {
    nsTableFrame* tableFrame = static_cast<nsTableFrame*>(mFrame);

    if (tableFrame->IsBorderCollapse()) {
      // border-collapsed tables don't use any of their padding, and
      // only part of their border.  We need to do this here before we
      // try to do anything like handling 'auto' widths,
      // 'box-sizing', or 'auto' margins.
      ComputedPhysicalPadding().SizeTo(0, 0, 0, 0);
      SetComputedLogicalBorderPadding(
          tableFrame->GetIncludedOuterBCBorder(mWritingMode));
    }

    // The margin is inherited to the table wrapper frame via
    // the ::-moz-table-wrapper rule in ua.css.
    ComputedPhysicalMargin().SizeTo(0, 0, 0, 0);
  } else if (aFrameType == LayoutFrameType::Scrollbar) {
    // scrollbars may have had their width or height smashed to zero
    // by the associated scrollframe, in which case we must not report
    // any padding or border.
    nsSize size(mFrame->GetSize());
    if (size.width == 0 || size.height == 0) {
      ComputedPhysicalPadding().SizeTo(0, 0, 0, 0);
      ComputedPhysicalBorderPadding().SizeTo(0, 0, 0, 0);
    }
  }
  ::UpdateProp(mFrame, nsIFrame::UsedPaddingProperty(), needPaddingProp,
               ComputedPhysicalPadding());
}

// This code enforces section 10.3.3 of the CSS2 spec for this formula:
//
// 'margin-left' + 'border-left-width' + 'padding-left' + 'width' +
//   'padding-right' + 'border-right-width' + 'margin-right'
//   = width of containing block
//
// Note: the width unit is not auto when this is called
void ReflowInput::CalculateBlockSideMargins(LayoutFrameType aFrameType) {
  // Calculations here are done in the containing block's writing mode,
  // which is where margins will eventually be applied: we're calculating
  // margins that will be used by the container in its inline direction,
  // which in the case of an orthogonal contained block will correspond to
  // the block direction of this reflow input. So in the orthogonal-flow
  // case, "CalculateBlock*Side*Margins" will actually end up adjusting
  // the BStart/BEnd margins; those are the "sides" of the block from its
  // container's point of view.
  WritingMode cbWM =
      mCBReflowInput ? mCBReflowInput->GetWritingMode() : GetWritingMode();

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

  LogicalMargin margin = ComputedLogicalMargin().ConvertTo(cbWM, mWritingMode);
  LogicalMargin borderPadding =
      ComputedLogicalBorderPadding().ConvertTo(cbWM, mWritingMode);
  nscoord sum = margin.IStartEnd(cbWM) + borderPadding.IStartEnd(cbWM) +
                computedISizeCBWM;
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
  const auto& styleSides = mStyleMargin->mMargin;
  bool isAutoStartMargin = styleSides.GetIStart(cbWM).IsAuto();
  bool isAutoEndMargin = styleSides.GetIEnd(cbWM).IsAuto();
  if (!isAutoStartMargin && !isAutoEndMargin) {
    // Neither margin is 'auto' so we're over constrained. Use the
    // 'direction' property of the parent to tell which margin to
    // ignore
    // First check if there is an HTML alignment that we should honor
    const ReflowInput* pri = mParentReflowInput;
    if (aFrameType == LayoutFrameType::Table) {
      NS_ASSERTION(pri->mFrame->IsTableWrapperFrame(),
                   "table not inside table wrapper");
      // Center the table within the table wrapper based on the alignment
      // of the table wrapper's parent.
      pri = pri->mParentReflowInput;
    }
    if (pri && (pri->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_LEFT ||
                pri->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER ||
                pri->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT)) {
      if (pri->mWritingMode.IsBidiLTR()) {
        isAutoStartMargin =
            pri->mStyleText->mTextAlign != NS_STYLE_TEXT_ALIGN_MOZ_LEFT;
        isAutoEndMargin =
            pri->mStyleText->mTextAlign != NS_STYLE_TEXT_ALIGN_MOZ_RIGHT;
      } else {
        isAutoStartMargin =
            pri->mStyleText->mTextAlign != NS_STYLE_TEXT_ALIGN_MOZ_RIGHT;
        isAutoEndMargin =
            pri->mStyleText->mTextAlign != NS_STYLE_TEXT_ALIGN_MOZ_LEFT;
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
  LogicalMargin marginInOurWM = margin.ConvertTo(mWritingMode, cbWM);
  SetComputedLogicalMargin(marginInOurWM);

  if (isAutoStartMargin || isAutoEndMargin) {
    // Update the UsedMargin property if we were tracking it already.
    nsMargin* propValue = mFrame->GetProperty(nsIFrame::UsedMarginProperty());
    if (propValue) {
      *propValue = marginInOurWM.GetPhysicalMargin(mWritingMode);
    }
  }
}

#define NORMAL_LINE_HEIGHT_FACTOR 1.2f  // in term of emHeight
// For "normal" we use the font's normal line height (em height + leading).
// If both internal leading and  external leading specified by font itself
// are zeros, we should compensate this by creating extra (external) leading
// in eCompensateLeading mode. This is necessary because without this
// compensation, normal line height might looks too tight.

// For risk management, we use preference to control the behavior, and
// eNoExternalLeading is the old behavior.
static nscoord GetNormalLineHeight(nsFontMetrics* aFontMetrics) {
  MOZ_ASSERT(nullptr != aFontMetrics, "no font metrics");

  nscoord normalLineHeight;

  nscoord externalLeading = aFontMetrics->ExternalLeading();
  nscoord internalLeading = aFontMetrics->InternalLeading();
  nscoord emHeight = aFontMetrics->EmHeight();
  switch (GetNormalLineHeightCalcControl()) {
    case eIncludeExternalLeading:
      normalLineHeight = emHeight + internalLeading + externalLeading;
      break;
    case eCompensateLeading:
      if (!internalLeading && !externalLeading)
        normalLineHeight = NSToCoordRound(emHeight * NORMAL_LINE_HEIGHT_FACTOR);
      else
        normalLineHeight = emHeight + internalLeading + externalLeading;
      break;
    default:
      // case eNoExternalLeading:
      normalLineHeight = emHeight + internalLeading;
  }
  return normalLineHeight;
}

static inline nscoord ComputeLineHeight(ComputedStyle* aComputedStyle,
                                        nsPresContext* aPresContext,
                                        nscoord aBlockBSize,
                                        float aFontSizeInflation) {
  const StyleLineHeight& lineHeight = aComputedStyle->StyleText()->mLineHeight;
  if (lineHeight.IsLength()) {
    nscoord result = lineHeight.length._0.ToAppUnits();
    if (aFontSizeInflation != 1.0f) {
      result = NSToCoordRound(result * aFontSizeInflation);
    }
    return result;
  }

  if (lineHeight.IsNumber()) {
    // For factor units the computed value of the line-height property
    // is found by multiplying the factor by the font's computed size
    // (adjusted for min-size prefs and text zoom).
    return NSToCoordRound(lineHeight.number._0 * aFontSizeInflation *
                          aComputedStyle->StyleFont()->mFont.size);
  }

  MOZ_ASSERT(lineHeight.IsNormal() || lineHeight.IsMozBlockHeight());
  if (lineHeight.IsMozBlockHeight() && aBlockBSize != NS_UNCONSTRAINEDSIZE) {
    return aBlockBSize;
  }

  RefPtr<nsFontMetrics> fm = nsLayoutUtils::GetFontMetricsForComputedStyle(
      aComputedStyle, aPresContext, aFontSizeInflation);
  return GetNormalLineHeight(fm);
}

nscoord ReflowInput::CalcLineHeight() const {
  nscoord blockBSize = nsLayoutUtils::IsNonWrapperBlock(mFrame)
                           ? ComputedBSize()
                           : (mCBReflowInput ? mCBReflowInput->ComputedBSize()
                                             : NS_UNCONSTRAINEDSIZE);

  return CalcLineHeight(mFrame->GetContent(), mFrame->Style(),
                        mFrame->PresContext(), blockBSize,
                        nsLayoutUtils::FontSizeInflationFor(mFrame));
}

/* static */
nscoord ReflowInput::CalcLineHeight(nsIContent* aContent,
                                    ComputedStyle* aComputedStyle,
                                    nsPresContext* aPresContext,
                                    nscoord aBlockBSize,
                                    float aFontSizeInflation) {
  MOZ_ASSERT(aComputedStyle, "Must have a ComputedStyle");

  nscoord lineHeight = ComputeLineHeight(aComputedStyle, aPresContext,
                                         aBlockBSize, aFontSizeInflation);

  NS_ASSERTION(lineHeight >= 0, "ComputeLineHeight screwed up");

  HTMLInputElement* input = HTMLInputElement::FromNodeOrNull(aContent);
  if (input && input->IsSingleLineTextControl()) {
    // For Web-compatibility, single-line text input elements cannot
    // have a line-height smaller than one.
    nscoord lineHeightOne =
        aFontSizeInflation * aComputedStyle->StyleFont()->mFont.size;
    if (lineHeight < lineHeightOne) {
      lineHeight = lineHeightOne;
    }
  }

  return lineHeight;
}

bool SizeComputationInput::ComputeMargin(WritingMode aWM,
                                         nscoord aPercentBasis) {
  // SVG text frames have no margin.
  if (nsSVGUtils::IsInSVGTextSubtree(mFrame)) {
    return false;
  }

  // If style style can provide us the margin directly, then use it.
  const nsStyleMargin* styleMargin = mFrame->StyleMargin();

  bool isCBDependent = !styleMargin->GetMargin(ComputedPhysicalMargin());
  if (isCBDependent) {
    // We have to compute the value. Note that this calculation is
    // performed according to the writing mode of the containing block
    // (http://dev.w3.org/csswg/css-writing-modes-3/#orthogonal-flows)
    if (aPercentBasis == NS_UNCONSTRAINEDSIZE) {
      aPercentBasis = 0;
    }
    LogicalMargin m(aWM);
    m.IStart(aWM) = nsLayoutUtils::ComputeCBDependentValue(
        aPercentBasis, styleMargin->mMargin.GetIStart(aWM));
    m.IEnd(aWM) = nsLayoutUtils::ComputeCBDependentValue(
        aPercentBasis, styleMargin->mMargin.GetIEnd(aWM));

    m.BStart(aWM) = nsLayoutUtils::ComputeCBDependentValue(
        aPercentBasis, styleMargin->mMargin.GetBStart(aWM));
    m.BEnd(aWM) = nsLayoutUtils::ComputeCBDependentValue(
        aPercentBasis, styleMargin->mMargin.GetBEnd(aWM));

    SetComputedLogicalMargin(aWM, m);
  }

  // ... but font-size-inflation-based margin adjustment uses the
  // frame's writing mode
  nscoord marginAdjustment = FontSizeInflationListMarginAdjustment(mFrame);

  if (marginAdjustment > 0) {
    LogicalMargin m = ComputedLogicalMargin();
    m.IStart(mWritingMode) += marginAdjustment;
    SetComputedLogicalMargin(m);
  }

  return isCBDependent;
}

bool SizeComputationInput::ComputePadding(WritingMode aWM,
                                          nscoord aPercentBasis,
                                          LayoutFrameType aFrameType) {
  // If style can provide us the padding directly, then use it.
  const nsStylePadding* stylePadding = mFrame->StylePadding();
  bool isCBDependent = !stylePadding->GetPadding(ComputedPhysicalPadding());
  // a table row/col group, row/col doesn't have padding
  // XXXldb Neither do border-collapse tables.
  if (LayoutFrameType::TableRowGroup == aFrameType ||
      LayoutFrameType::TableColGroup == aFrameType ||
      LayoutFrameType::TableRow == aFrameType ||
      LayoutFrameType::TableCol == aFrameType) {
    ComputedPhysicalPadding().SizeTo(0, 0, 0, 0);
  } else if (isCBDependent) {
    // We have to compute the value. This calculation is performed
    // according to the writing mode of the containing block
    // (http://dev.w3.org/csswg/css-writing-modes-3/#orthogonal-flows)
    // clamp negative calc() results to 0
    if (aPercentBasis == NS_UNCONSTRAINEDSIZE) {
      aPercentBasis = 0;
    }
    LogicalMargin p(aWM);
    p.IStart(aWM) =
        std::max(0, nsLayoutUtils::ComputeCBDependentValue(
                        aPercentBasis, stylePadding->mPadding.GetIStart(aWM)));
    p.IEnd(aWM) =
        std::max(0, nsLayoutUtils::ComputeCBDependentValue(
                        aPercentBasis, stylePadding->mPadding.GetIEnd(aWM)));

    p.BStart(aWM) =
        std::max(0, nsLayoutUtils::ComputeCBDependentValue(
                        aPercentBasis, stylePadding->mPadding.GetBStart(aWM)));
    p.BEnd(aWM) =
        std::max(0, nsLayoutUtils::ComputeCBDependentValue(
                        aPercentBasis, stylePadding->mPadding.GetBEnd(aWM)));

    SetComputedLogicalPadding(aWM, p);
  }
  return isCBDependent;
}

void ReflowInput::ComputeMinMaxValues(const LogicalSize& aCBSize) {
  WritingMode wm = GetWritingMode();

  const auto& minISize = mStylePosition->MinISize(wm);
  const auto& maxISize = mStylePosition->MaxISize(wm);
  const auto& minBSize = mStylePosition->MinBSize(wm);
  const auto& maxBSize = mStylePosition->MaxBSize(wm);

  // NOTE: min-width:auto resolves to 0, except on a flex item. (But
  // even there, it's supposed to be ignored (i.e. treated as 0) until
  // the flex container explicitly resolves & considers it.)
  if (minISize.IsAuto()) {
    ComputedMinISize() = 0;
  } else {
    ComputedMinISize() = ComputeISizeValue(
        aCBSize.ISize(wm), mStylePosition->mBoxSizing, minISize);
  }

  if (maxISize.IsNone()) {
    // Specified value of 'none'
    ComputedMaxISize() = NS_UNCONSTRAINEDSIZE;  // no limit
  } else {
    ComputedMaxISize() = ComputeISizeValue(
        aCBSize.ISize(wm), mStylePosition->mBoxSizing, maxISize);
  }

  // If the computed value of 'min-width' is greater than the value of
  // 'max-width', 'max-width' is set to the value of 'min-width'
  if (ComputedMinISize() > ComputedMaxISize()) {
    ComputedMaxISize() = ComputedMinISize();
  }

  // Check for percentage based values and a containing block height that
  // depends on the content height. Treat them like the initial value.
  // Likewise, check for calc() with percentages on internal table elements;
  // that's treated as the initial value too.
  // Likewise, if we're a child of a flex container who's measuring our
  // intrinsic height, then we want to disregard our min-height/max-height.
  const nscoord& bPercentageBasis = aCBSize.BSize(wm);
  auto BSizeBehavesAsInitialValue = [&](const auto& aBSize) {
    if (nsLayoutUtils::IsAutoBSize(aBSize, bPercentageBasis)) {
      return true;
    }
    if (mFlags.mIsFlexContainerMeasuringBSize) {
      return true;
    }
    if (mFrameType == NS_CSS_FRAME_TYPE_INTERNAL_TABLE) {
      return aBSize.HasLengthAndPercentage();
    }
    return false;
  };

  // NOTE: min-height:auto resolves to 0, except on a flex item. (But
  // even there, it's supposed to be ignored (i.e. treated as 0) until
  // the flex container explicitly resolves & considers it.)
  if (BSizeBehavesAsInitialValue(minBSize)) {
    ComputedMinBSize() = 0;
  } else {
    ComputedMinBSize() =
        ComputeBSizeValue(bPercentageBasis, mStylePosition->mBoxSizing,
                          minBSize.AsLengthPercentage());
  }

  if (BSizeBehavesAsInitialValue(maxBSize)) {
    // Specified value of 'none'
    ComputedMaxBSize() = NS_UNCONSTRAINEDSIZE;  // no limit
  } else {
    ComputedMaxBSize() =
        ComputeBSizeValue(bPercentageBasis, mStylePosition->mBoxSizing,
                          maxBSize.AsLengthPercentage());
  }

  // If the computed value of 'min-height' is greater than the value of
  // 'max-height', 'max-height' is set to the value of 'min-height'
  if (ComputedMinBSize() > ComputedMaxBSize()) {
    ComputedMaxBSize() = ComputedMinBSize();
  }
}

bool ReflowInput::IsFloating() const {
  return mStyleDisplay->IsFloating(mFrame);
}

mozilla::StyleDisplay ReflowInput::GetDisplay() const {
  return mStyleDisplay->GetDisplay(mFrame);
}
