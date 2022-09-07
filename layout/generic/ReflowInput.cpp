/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* struct containing the input to nsIFrame::Reflow */

#include "mozilla/ReflowInput.h"

#include <algorithm>

#include "CounterStyleManager.h"
#include "LayoutLogging.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/WritingModes.h"
#include "nsBlockFrame.h"
#include "nsCSSAnonBoxes.h"
#include "nsFlexContainerFrame.h"
#include "nsFontInflationData.h"
#include "nsFontMetrics.h"
#include "nsGkAtoms.h"
#include "nsGridContainerFrame.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsImageFrame.h"
#include "nsIPercentBSizeObserver.h"
#include "nsLayoutUtils.h"
#include "nsLineBox.h"
#include "nsPresContext.h"
#include "nsStyleConsts.h"
#include "nsTableCellFrame.h"
#include "nsTableFrame.h"
#include "StickyScrollContainer.h"

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

  nsAtom* type = list->mCounterStyle.AsAtom();
  if (type != nsGkAtoms::none && type != nsGkAtoms::disc &&
      type != nsGkAtoms::circle && type != nsGkAtoms::square &&
      type != nsGkAtoms::disclosure_closed &&
      type != nsGkAtoms::disclosure_open) {
    return margin;
  }

  return 0;
}

SizeComputationInput::SizeComputationInput(nsIFrame* aFrame,
                                           gfxContext* aRenderingContext)
    : mFrame(aFrame),
      mRenderingContext(aRenderingContext),
      mWritingMode(aFrame->GetWritingMode()),
      mComputedMargin(mWritingMode),
      mComputedBorderPadding(mWritingMode),
      mComputedPadding(mWritingMode) {
  MOZ_ASSERT(mFrame);
}

SizeComputationInput::SizeComputationInput(
    nsIFrame* aFrame, gfxContext* aRenderingContext,
    WritingMode aContainingBlockWritingMode, nscoord aContainingBlockISize,
    const Maybe<LogicalMargin>& aBorder, const Maybe<LogicalMargin>& aPadding)
    : SizeComputationInput(aFrame, aRenderingContext) {
  MOZ_ASSERT(!mFrame->IsTableColFrame());
  InitOffsets(aContainingBlockWritingMode, aContainingBlockISize,
              mFrame->Type(), {}, aBorder, aPadding);
}

// Initialize a <b>root</b> reflow input with a rendering context to
// use for measuring things.
ReflowInput::ReflowInput(nsPresContext* aPresContext, nsIFrame* aFrame,
                         gfxContext* aRenderingContext,
                         const LogicalSize& aAvailableSpace, InitFlags aFlags)
    : SizeComputationInput(aFrame, aRenderingContext),
      mAvailableSize(aAvailableSpace) {
  MOZ_ASSERT(aRenderingContext, "no rendering context");
  MOZ_ASSERT(aPresContext, "no pres context");
  MOZ_ASSERT(aFrame, "no frame");
  MOZ_ASSERT(aPresContext == aFrame->PresContext(), "wrong pres context");

  if (aFlags.contains(InitFlag::DummyParentReflowInput)) {
    mFlags.mDummyParentReflowInput = true;
  }
  if (aFlags.contains(InitFlag::StaticPosIsCBOrigin)) {
    mFlags.mStaticPosIsCBOrigin = true;
  }

  if (!aFlags.contains(InitFlag::CallerWillInit)) {
    Init(aPresContext);
  }
}

// Initialize a reflow input for a child frame's reflow. Some state
// is copied from the parent reflow input; the remaining state is
// computed.
ReflowInput::ReflowInput(nsPresContext* aPresContext,
                         const ReflowInput& aParentReflowInput,
                         nsIFrame* aFrame, const LogicalSize& aAvailableSpace,
                         const Maybe<LogicalSize>& aContainingBlockSize,
                         InitFlags aFlags,
                         const StyleSizeOverrides& aSizeOverrides,
                         ComputeSizeFlags aComputeSizeFlags)
    : SizeComputationInput(aFrame, aParentReflowInput.mRenderingContext),
      mParentReflowInput(&aParentReflowInput),
      mFloatManager(aParentReflowInput.mFloatManager),
      mLineLayout(mFrame->IsFrameOfType(nsIFrame::eLineParticipant)
                      ? aParentReflowInput.mLineLayout
                      : nullptr),
      mBreakType(aParentReflowInput.mBreakType),
      mPercentBSizeObserver(
          (aParentReflowInput.mPercentBSizeObserver &&
           aParentReflowInput.mPercentBSizeObserver->NeedsToObserve(*this))
              ? aParentReflowInput.mPercentBSizeObserver
              : nullptr),
      mFlags(aParentReflowInput.mFlags),
      mStyleSizeOverrides(aSizeOverrides),
      mComputeSizeFlags(aComputeSizeFlags),
      mReflowDepth(aParentReflowInput.mReflowDepth + 1),
      mAvailableSize(aAvailableSpace) {
  MOZ_ASSERT(aPresContext, "no pres context");
  MOZ_ASSERT(aFrame, "no frame");
  MOZ_ASSERT(aPresContext == aFrame->PresContext(), "wrong pres context");
  MOZ_ASSERT(!mFlags.mSpecialBSizeReflow || !aFrame->IsSubtreeDirty(),
             "frame should be clean when getting special bsize reflow");

  if (mWritingMode.IsOrthogonalTo(aParentReflowInput.GetWritingMode())) {
    // If we're setting up for an orthogonal flow, and the parent reflow input
    // had a constrained ComputedBSize, we can use that as our AvailableISize
    // in preference to leaving it unconstrained.
    if (AvailableISize() == NS_UNCONSTRAINEDSIZE &&
        aParentReflowInput.ComputedBSize() != NS_UNCONSTRAINEDSIZE) {
      SetAvailableISize(aParentReflowInput.ComputedBSize());
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
  mFlags.mTreatBSizeAsIndefinite = false;
  mFlags.mDummyParentReflowInput = false;
  mFlags.mStaticPosIsCBOrigin = aFlags.contains(InitFlag::StaticPosIsCBOrigin);
  mFlags.mIOffsetsNeedCSSAlign = mFlags.mBOffsetsNeedCSSAlign = false;

  if (aFlags.contains(InitFlag::DummyParentReflowInput) ||
      (mParentReflowInput->mFlags.mDummyParentReflowInput &&
       mFrame->IsTableFrame())) {
    mFlags.mDummyParentReflowInput = true;
  }

  if (!aFlags.contains(InitFlag::CallerWillInit)) {
    Init(aPresContext, aContainingBlockSize);
  }
}

template <typename SizeOrMaxSize>
inline nscoord SizeComputationInput::ComputeISizeValue(
    const WritingMode aWM, const LogicalSize& aContainingBlockSize,
    const LogicalSize& aContentEdgeToBoxSizing, nscoord aBoxSizingToMarginEdge,
    const SizeOrMaxSize& aSize) const {
  return mFrame
      ->ComputeISizeValue(mRenderingContext, aWM, aContainingBlockSize,
                          aContentEdgeToBoxSizing, aBoxSizingToMarginEdge,
                          aSize)
      .mISize;
}

template <typename SizeOrMaxSize>
nscoord SizeComputationInput::ComputeISizeValue(
    const LogicalSize& aContainingBlockSize, StyleBoxSizing aBoxSizing,
    const SizeOrMaxSize& aSize) const {
  WritingMode wm = GetWritingMode();
  const auto borderPadding = ComputedLogicalBorderPadding(wm);
  LogicalSize inside = aBoxSizing == StyleBoxSizing::Border
                           ? borderPadding.Size(wm)
                           : LogicalSize(wm);
  nscoord outside =
      borderPadding.IStartEnd(wm) + ComputedLogicalMargin(wm).IStartEnd(wm);
  outside -= inside.ISize(wm);

  return ComputeISizeValue(wm, aContainingBlockSize, inside, outside, aSize);
}

nscoord SizeComputationInput::ComputeBSizeValue(
    nscoord aContainingBlockBSize, StyleBoxSizing aBoxSizing,
    const LengthPercentage& aSize) const {
  WritingMode wm = GetWritingMode();
  nscoord inside = 0;
  if (aBoxSizing == StyleBoxSizing::Border) {
    inside = ComputedLogicalBorderPadding(wm).BStartEnd(wm);
  }
  return nsLayoutUtils::ComputeBSizeValue(aContainingBlockBSize, inside, aSize);
}

bool ReflowInput::ShouldReflowAllKids() const {
  // Note that we could make a stronger optimization for IsBResize if
  // we use it in a ShouldReflowChild test that replaces the current
  // checks of NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN, if it
  // were tested there along with NS_FRAME_CONTAINS_RELATIVE_BSIZE.
  // This would need to be combined with a slight change in which
  // frames NS_FRAME_CONTAINS_RELATIVE_BSIZE is marked on.
  return mFrame->HasAnyStateBits(NS_FRAME_IS_DIRTY) || IsIResize() ||
         (IsBResize() &&
          mFrame->HasAnyStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE));
}

void ReflowInput::SetComputedISize(nscoord aComputedISize) {
  // It'd be nice to assert that |frame| is not in reflow, but this fails for
  // two reasons:
  //
  // 1) Viewport frames reset the computed isize on a copy of their reflow
  //    input when reflowing fixed-pos kids.  In that case we actually don't
  //    want to mess with the resize flags, because comparing the frame's rect
  //    to the munged computed width is pointless.
  // 2) nsIFrame::BoxReflow creates a reflow input for its parent.  This reflow
  //    input is not used to reflow the parent, but just as a parent for the
  //    frame's own reflow input.  So given a nsBoxFrame inside some non-XUL
  //    (like a text control, for example), we'll end up creating a reflow
  //    input for the parent while the parent is reflowing.

  NS_WARNING_ASSERTION(aComputedISize >= 0, "Invalid computed inline-size!");
  if (ComputedISize() != aComputedISize) {
    ComputedISize() = std::max(0, aComputedISize);
    const LayoutFrameType frameType = mFrame->Type();
    if (frameType != LayoutFrameType::Viewport) {
      InitResizeFlags(mFrame->PresContext(), frameType);
    }
  }
}

void ReflowInput::SetComputedBSize(nscoord aComputedBSize) {
  // It'd be nice to assert that |frame| is not in reflow, but this fails
  // because:
  //
  //    nsIFrame::BoxReflow creates a reflow input for its parent.  This reflow
  //    input is not used to reflow the parent, but just as a parent for the
  //    frame's own reflow input.  So given a nsBoxFrame inside some non-XUL
  //    (like a text control, for example), we'll end up creating a reflow
  //    input for the parent while the parent is reflowing.

  if (ComputedBSize() != aComputedBSize) {
    SetComputedBSizeWithoutResettingResizeFlags(aComputedBSize);
    InitResizeFlags(mFrame->PresContext(), mFrame->Type());
  }
}

void ReflowInput::SetComputedBSizeWithoutResettingResizeFlags(
    nscoord aComputedBSize) {
  // Viewport frames reset the computed block size on a copy of their reflow
  // input when reflowing fixed-pos kids.  In that case we actually don't
  // want to mess with the resize flags, because comparing the frame's rect
  // to the munged computed isize is pointless.
  NS_WARNING_ASSERTION(aComputedBSize >= 0, "Invalid computed block-size!");
  ComputedBSize() = std::max(0, aComputedBSize);
}

void ReflowInput::Init(nsPresContext* aPresContext,
                       const Maybe<LogicalSize>& aContainingBlockSize,
                       const Maybe<LogicalMargin>& aBorder,
                       const Maybe<LogicalMargin>& aPadding) {
  if (AvailableISize() == NS_UNCONSTRAINEDSIZE) {
    // Look up the parent chain for an orthogonal inline limit,
    // and reset AvailableISize() if found.
    for (const ReflowInput* parent = mParentReflowInput; parent != nullptr;
         parent = parent->mParentReflowInput) {
      if (parent->GetWritingMode().IsOrthogonalTo(mWritingMode) &&
          parent->mOrthogonalLimit != NS_UNCONSTRAINEDSIZE) {
        SetAvailableISize(parent->mOrthogonalLimit);
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
  mStyleBorder = mFrame->StyleBorder();
  mStyleMargin = mFrame->StyleMargin();

  InitCBReflowInput();

  LayoutFrameType type = mFrame->Type();
  if (type == mozilla::LayoutFrameType::Placeholder) {
    // Placeholders have a no-op Reflow method that doesn't need the rest of
    // this initialization, so we bail out early.
    ComputedBSize() = ComputedISize() = 0;
    return;
  }

  mFlags.mIsReplaced = mFrame->IsFrameOfType(nsIFrame::eReplaced) ||
                       mFrame->IsFrameOfType(nsIFrame::eReplacedContainsBlock);
  InitConstraints(aPresContext, aContainingBlockSize, aBorder, aPadding, type);

  InitResizeFlags(aPresContext, type);
  InitDynamicReflowRoot();

  nsIFrame* parent = mFrame->GetParent();
  if (parent && parent->HasAnyStateBits(NS_FRAME_IN_CONSTRAINED_BSIZE) &&
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
      SetAvailableBSize(NS_UNCONSTRAINEDSIZE);
    }
  }

  if (mStyleDisplay->GetContainSizeAxes().mBContained) {
    // In the case that a box is size contained in block axis, we want to ensure
    // that it is also monolithic. We do this by setting AvailableBSize() to an
    // unconstrained size to avoid fragmentation.
    SetAvailableBSize(NS_UNCONSTRAINEDSIZE);
  }

  LAYOUT_WARN_IF_FALSE((mStyleDisplay->IsInlineOutsideStyle() &&
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
  // border-box size change, continue to do that. It's possible for
  // the content-box size to change without a border-box size change or
  // a style change given (1) a fixed width (possibly fixed by max-width
  // or min-width), box-sizing:border-box, and percentage padding;
  // (2) box-sizing:content-box, M% width, and calc(Npx - M%) padding.
  //
  // However, we don't actually have the information at this point to tell
  // whether the content-box size has changed, since both style data and the
  // UsedPaddingProperty() have already been updated in
  // SizeComputationInput::InitOffsets(). So, we check the HasPaddingChange()
  // bit for the cases where it's possible for the content-box size to have
  // changed without either (a) a change in the border-box size or (b) an
  // nsChangeHint_NeedDirtyReflow change hint due to change in border or
  // padding.
  //
  // We don't clear the HasPaddingChange() bit here, since sometimes we
  // construct reflow input (e.g. in nsBlockFrame::ReflowBlockFrame to compute
  // margin collapsing) without reflowing the frame. Instead, we clear it in
  // nsIFrame::DidReflow().
  bool isIResize =
      // is the border-box resizing?
      mFrame->ISize(wm) !=
          ComputedISize() + ComputedLogicalBorderPadding(wm).IStartEnd(wm) ||
      // or is the content-box resizing?  (see comment above)
      mFrame->HasPaddingChange();

  if (mFrame->HasAnyStateBits(NS_FRAME_FONT_INFLATION_FLOW_ROOT) &&
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
        for (const auto& childList : f->ChildLists()) {
          for (nsIFrame* kid : childList.mList) {
            kid->MarkIntrinsicISizesDirty();
            stack.AppendElement(kid);
          }
        }
      } while (stack.Length() != 0);
    }
  }

  SetIResize(!mFrame->HasAnyStateBits(NS_FRAME_IS_DIRTY) && isIResize);

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
    // construct a ReflowInput (e.g. in nsBlockFrame::ReflowBlockFrame to
    // compute margin collapsing) without reflowing the frame. Instead, we
    // clear it in nsIFrame::DidReflow.
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
              mFrame->FirstInFlow()->HasAnyStateBits(
                  NS_TABLE_CELL_HAD_SPECIAL_REFLOW)) &&
             mFrame->HasAnyStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
    // Need to set the bit on the cell so that
    // mCBReflowInput->IsBResize() is set correctly below when
    // reflowing descendant.
    SetBResize(true);
    mFlags.mIsBResizeForPercentages = true;
  } else if (mCBReflowInput && mFrame->IsBlockWrapper()) {
    // XXX Is this problematic for relatively positioned inlines acting
    // as containing block for absolutely positioned elements?
    // Possibly; in that case we should at least be checking
    // IsSubtreeDirty(), I'd think.
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
    SetBResize(IsBResize() || mFrame->IsSubtreeDirty());
  } else {
    // We have a non-'auto' block-size, i.e., a length.  Set the BResize
    // flag to whether the size is actually different.
    SetBResize(mFrame->BSize(wm) !=
               ComputedBSize() +
                   ComputedLogicalBorderPadding(wm).BStartEnd(wm));
  }

  bool dependsOnCBBSize = (mStylePosition->BSizeDependsOnContainer(wm) &&
                           // FIXME: condition this on not-abspos?
                           !mStylePosition->BSize(wm).IsAuto()) ||
                          mStylePosition->MinBSizeDependsOnContainer(wm) ||
                          mStylePosition->MaxBSizeDependsOnContainer(wm) ||
                          mStylePosition->mOffset.GetBStart(wm).HasPercent() ||
                          !mStylePosition->mOffset.GetBEnd(wm).IsAuto() ||
                          mFrame->IsXULBoxFrame();

  // If mFrame is a flex item, and mFrame's block axis is the flex container's
  // main axis (e.g. in a column-oriented flex container with same
  // writing-mode), then its block-size depends on its CB size, if its
  // flex-basis has a percentage.
  if (mFrame->IsFlexItem() &&
      !nsFlexContainerFrame::IsItemInlineAxisMainAxis(mFrame)) {
    const auto& flexBasis = mStylePosition->mFlexBasis;
    dependsOnCBBSize |= (flexBasis.IsSize() && flexBasis.AsSize().HasPercent());
  }

  if (mFrame->StyleText()->mLineHeight.IsMozBlockHeight()) {
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

      if (rs->mFrame->HasAnyStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
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
  if (mFrame->HasAnyStateBits(NS_FRAME_IS_DIRTY)) {
    // If we're reflowing everything, then we'll find out if we need
    // to re-set this.
    mFrame->RemoveStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
  }
}

void ReflowInput::InitDynamicReflowRoot() {
  if (mFrame->CanBeDynamicReflowRoot()) {
    mFrame->AddStateBits(NS_FRAME_DYNAMIC_REFLOW_ROOT);
  } else {
    mFrame->RemoveStateBits(NS_FRAME_DYNAMIC_REFLOW_ROOT);
  }
}

bool ReflowInput::ShouldApplyAutomaticMinimumOnBlockAxis() const {
  MOZ_ASSERT(!mFrame->IsFrameOfType(nsIFrame::eReplacedSizing));
  return mFlags.mIsBSizeSetByAspectRatio &&
         !mStyleDisplay->IsScrollableOverflow() &&
         mStylePosition->MinBSize(GetWritingMode()).IsAuto();
}

/* static */
LogicalMargin ReflowInput::ComputeRelativeOffsets(WritingMode aWM,
                                                  nsIFrame* aFrame,
                                                  const LogicalSize& aCBSize) {
  LogicalMargin offsets(aWM);
  const nsStylePosition* position = aFrame->StylePosition();

  // Compute the 'inlineStart' and 'inlineEnd' values. 'inlineStart'
  // moves the boxes to the end of the line, and 'inlineEnd' moves the
  // boxes to the start of the line. The computed values are always:
  // inlineStart=-inlineEnd
  const auto& inlineStart = position->mOffset.GetIStart(aWM);
  const auto& inlineEnd = position->mOffset.GetIEnd(aWM);
  bool inlineStartIsAuto = inlineStart.IsAuto();
  bool inlineEndIsAuto = inlineEnd.IsAuto();

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
      offsets.IEnd(aWM) =
          nsLayoutUtils::ComputeCBDependentValue(aCBSize.ISize(aWM), inlineEnd);

      // Computed value for 'inlineStart' is minus the value of 'inlineEnd'
      offsets.IStart(aWM) = -offsets.IEnd(aWM);
    }

  } else {
    NS_ASSERTION(inlineEndIsAuto, "unexpected specified constraint");

    // 'InlineStart' isn't 'auto' so compute its value
    offsets.IStart(aWM) =
        nsLayoutUtils::ComputeCBDependentValue(aCBSize.ISize(aWM), inlineStart);

    // Computed value for 'inlineEnd' is minus the value of 'inlineStart'
    offsets.IEnd(aWM) = -offsets.IStart(aWM);
  }

  // Compute the 'blockStart' and 'blockEnd' values. The 'blockStart'
  // and 'blockEnd' properties move relatively positioned elements in
  // the block progression direction. They also must be each other's
  // negative
  const auto& blockStart = position->mOffset.GetBStart(aWM);
  const auto& blockEnd = position->mOffset.GetBEnd(aWM);
  bool blockStartIsAuto = blockStart.IsAuto();
  bool blockEndIsAuto = blockEnd.IsAuto();

  // Check for percentage based values and a containing block block-size
  // that depends on the content block-size. Treat them like 'auto'
  if (NS_UNCONSTRAINEDSIZE == aCBSize.BSize(aWM)) {
    if (blockStart.HasPercent()) {
      blockStartIsAuto = true;
    }
    if (blockEnd.HasPercent()) {
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
          aCBSize.BSize(aWM), blockEnd);

      // Computed value for 'blockStart' is minus the value of 'blockEnd'
      offsets.BStart(aWM) = -offsets.BEnd(aWM);
    }

  } else {
    NS_ASSERTION(blockEndIsAuto, "unexpected specified constraint");

    // 'blockStart' isn't 'auto' so compute its value
    offsets.BStart(aWM) = nsLayoutUtils::ComputeBSizeDependentValue(
        aCBSize.BSize(aWM), blockStart);

    // Computed value for 'blockEnd' is minus the value of 'blockStart'
    offsets.BEnd(aWM) = -offsets.BStart(aWM);
  }

  // Convert the offsets to physical coordinates and store them on the frame
  const nsMargin physicalOffsets = offsets.GetPhysicalMargin(aWM);
  if (nsMargin* prop =
          aFrame->GetProperty(nsIFrame::ComputedOffsetProperty())) {
    *prop = physicalOffsets;
  } else {
    aFrame->AddProperty(nsIFrame::ComputedOffsetProperty(),
                        new nsMargin(physicalOffsets));
  }

  NS_ASSERTION(offsets.IStart(aWM) == -offsets.IEnd(aWM) &&
                   offsets.BStart(aWM) == -offsets.BEnd(aWM),
               "ComputeRelativeOffsets should return valid results!");

  return offsets;
}

/* static */
void ReflowInput::ApplyRelativePositioning(nsIFrame* aFrame,
                                           const nsMargin& aComputedOffsets,
                                           nsPoint* aPosition) {
  if (!aFrame->IsRelativelyOrStickyPositioned()) {
    NS_ASSERTION(!aFrame->HasProperty(nsIFrame::NormalPositionProperty()),
                 "We assume that changing the 'position' property causes "
                 "frame reconstruction.  If that ever changes, this code "
                 "should call "
                 "aFrame->RemoveProperty(nsIFrame::NormalPositionProperty())");
    return;
  }

  // Store the normal position
  aFrame->SetProperty(nsIFrame::NormalPositionProperty(), *aPosition);

  const nsStyleDisplay* display = aFrame->StyleDisplay();
  if (StylePositionProperty::Relative == display->mPosition) {
    *aPosition += nsPoint(aComputedOffsets.left, aComputedOffsets.top);
  } else if (StylePositionProperty::Sticky == display->mPosition &&
             !aFrame->GetNextContinuation() && !aFrame->GetPrevContinuation() &&
             !aFrame->HasAnyStateBits(NS_FRAME_PART_OF_IBSPLIT)) {
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

// static
void ReflowInput::ComputeAbsPosInlineAutoMargin(nscoord aAvailMarginSpace,
                                                WritingMode aContainingBlockWM,
                                                bool aIsMarginIStartAuto,
                                                bool aIsMarginIEndAuto,
                                                LogicalMargin& aMargin,
                                                LogicalMargin& aOffsets) {
  if (aIsMarginIStartAuto) {
    if (aIsMarginIEndAuto) {
      if (aAvailMarginSpace < 0) {
        // Note that this case is different from the neither-'auto'
        // case below, where the spec says to ignore 'left'/'right'.
        // Ignore the specified value for 'margin-right'.
        aMargin.IEnd(aContainingBlockWM) = aAvailMarginSpace;
      } else {
        // Both 'margin-left' and 'margin-right' are 'auto', so they get
        // equal values
        aMargin.IStart(aContainingBlockWM) = aAvailMarginSpace / 2;
        aMargin.IEnd(aContainingBlockWM) =
            aAvailMarginSpace - aMargin.IStart(aContainingBlockWM);
      }
    } else {
      // Just 'margin-left' is 'auto'
      aMargin.IStart(aContainingBlockWM) = aAvailMarginSpace;
    }
  } else {
    if (aIsMarginIEndAuto) {
      // Just 'margin-right' is 'auto'
      aMargin.IEnd(aContainingBlockWM) = aAvailMarginSpace;
    } else {
      // We're over-constrained so use the direction of the containing
      // block to dictate which value to ignore.  (And note that the
      // spec says to ignore 'left' or 'right' rather than
      // 'margin-left' or 'margin-right'.)
      // Note that this case is different from the both-'auto' case
      // above, where the spec says to ignore
      // 'margin-left'/'margin-right'.
      // Ignore the specified value for 'right'.
      aOffsets.IEnd(aContainingBlockWM) += aAvailMarginSpace;
    }
  }
}

// static
void ReflowInput::ComputeAbsPosBlockAutoMargin(nscoord aAvailMarginSpace,
                                               WritingMode aContainingBlockWM,
                                               bool aIsMarginBStartAuto,
                                               bool aIsMarginBEndAuto,
                                               LogicalMargin& aMargin,
                                               LogicalMargin& aOffsets) {
  if (aIsMarginBStartAuto) {
    if (aIsMarginBEndAuto) {
      // Both 'margin-top' and 'margin-bottom' are 'auto', so they get
      // equal values
      aMargin.BStart(aContainingBlockWM) = aAvailMarginSpace / 2;
      aMargin.BEnd(aContainingBlockWM) =
          aAvailMarginSpace - aMargin.BStart(aContainingBlockWM);
    } else {
      // Just margin-block-start is 'auto'
      aMargin.BStart(aContainingBlockWM) = aAvailMarginSpace;
    }
  } else {
    if (aIsMarginBEndAuto) {
      // Just margin-block-end is 'auto'
      aMargin.BEnd(aContainingBlockWM) = aAvailMarginSpace;
    } else {
      // We're over-constrained so ignore the specified value for
      // block-end.  (And note that the spec says to ignore 'bottom'
      // rather than 'margin-bottom'.)
      aOffsets.BEnd(aContainingBlockWM) += aAvailMarginSpace;
    }
  }
}

void ReflowInput::ApplyRelativePositioning(
    nsIFrame* aFrame, mozilla::WritingMode aWritingMode,
    const mozilla::LogicalMargin& aComputedOffsets,
    mozilla::LogicalPoint* aPosition, const nsSize& aContainerSize) {
  // Subtract the size of the frame from the container size that we
  // use for converting between the logical and physical origins of
  // the frame. This accounts for the fact that logical origins in RTL
  // coordinate systems are at the top right of the frame instead of
  // the top left.
  nsSize frameSize = aFrame->GetSize();
  nsPoint pos =
      aPosition->GetPhysicalPoint(aWritingMode, aContainerSize - frameSize);
  ApplyRelativePositioning(
      aFrame, aComputedOffsets.GetPhysicalMargin(aWritingMode), &pos);
  *aPosition =
      mozilla::LogicalPoint(aWritingMode, pos, aContainerSize - frameSize);
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
  if (aFrame->HasAnyStateBits(NS_FRAME_IN_REFLOW)) {
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
    aCBIStartEdge = reflowInput->ComputedLogicalBorderPadding(wm).IStart(wm);
    aCBSize = reflowInput->ComputedSize(wm);
  } else {
    /* Didn't find a reflow reflowInput for aFrame.  Just compute the
       information we want, on the assumption that aFrame already knows its
       size.  This really ought to be true by now. */
    NS_ASSERTION(!aFrame->HasAnyStateBits(NS_FRAME_IN_REFLOW),
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
  const auto* stylePadding = mFrame->StylePadding();
  if (nsMargin padding; stylePadding->GetPadding(padding)) {
    paddingStartEnd = padding.Side(startSide) + padding.Side(endSide);
  } else {
    // We have to compute the start and end values
    nscoord start, end;
    start = nsLayoutUtils::ComputeCBDependentValue(
        aContainingBlockSize, stylePadding->mPadding.Get(startSide));
    end = nsLayoutUtils::ComputeCBDependentValue(
        aContainingBlockSize, stylePadding->mPadding.Get(endSide));
    paddingStartEnd = start + end;
  }

  // See if the style system can provide us the margin directly
  if (nsMargin margin; mStyleMargin->GetMargin(margin)) {
    marginStartEnd = margin.Side(startSide) + margin.Side(endSide);
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

static bool AxisPolarityFlipped(LogicalAxis aThisAxis, WritingMode aThisWm,
                                WritingMode aOtherWm) {
  if (MOZ_LIKELY(aThisWm == aOtherWm)) {
    // Dedicated short circuit for the common case.
    return false;
  }
  LogicalAxis otherAxis = aThisWm.IsOrthogonalTo(aOtherWm)
                              ? GetOrthogonalAxis(aThisAxis)
                              : aThisAxis;
  NS_ASSERTION(
      aThisWm.PhysicalAxis(aThisAxis) == aOtherWm.PhysicalAxis(otherAxis),
      "Physical axes must match!");
  Side thisStartSide =
      aThisWm.PhysicalSide(MakeLogicalSide(aThisAxis, eLogicalEdgeStart));
  Side otherStartSide =
      aOtherWm.PhysicalSide(MakeLogicalSide(otherAxis, eLogicalEdgeStart));
  return thisStartSide != otherStartSide;
}

static bool InlinePolarityFlipped(WritingMode aThisWm, WritingMode aOtherWm) {
  return AxisPolarityFlipped(eLogicalAxisInline, aThisWm, aOtherWm);
}

static bool BlockPolarityFlipped(WritingMode aThisWm, WritingMode aOtherWm) {
  return AxisPolarityFlipped(eLogicalAxisBlock, aThisWm, aOtherWm);
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
  Maybe<nsSize> intrinsicSize;
  if (mFlags.mIsReplaced && isAutoISize) {
    // See if we can get the intrinsic size of the element
    intrinsicSize = mFrame->GetIntrinsicSize().ToSize();
  }

  // See if we can calculate what the box inline size would have been if
  // the element had been in the flow
  Maybe<nscoord> boxISize;
  if (mStyleDisplay->IsOriginalDisplayInlineOutside() && !mFlags.mIsReplaced) {
    // For non-replaced inline-level elements the 'inline size' property
    // doesn't apply, so we don't know what the inline size would have
    // been without reflowing it

  } else {
    // It's either a replaced inline-level element or a block-level element

    // Determine the total amount of inline direction
    // border/padding/margin that the element would have had if it had
    // been in the flow. Note that we ignore any 'auto' and 'inherit'
    // values
    nscoord insideBoxISizing, outsideBoxISizing;
    CalculateBorderPaddingMargin(eLogicalAxisInline, blockContentSize.ISize(wm),
                                 &insideBoxISizing, &outsideBoxISizing);

    if (mFlags.mIsReplaced && isAutoISize) {
      // It's a replaced element with an 'auto' inline size so the box
      // inline size is its intrinsic size plus any border/padding/margin
      if (intrinsicSize) {
        boxISize.emplace(LogicalSize(wm, *intrinsicSize).ISize(wm) +
                         outsideBoxISizing + insideBoxISizing);
      }

    } else if (isAutoISize) {
      // The box inline size is the containing block inline size
      boxISize.emplace(blockContentSize.ISize(wm));
    } else {
      // We need to compute it. It's important we do this, because if it's
      // percentage based this computed value may be different from the computed
      // value calculated using the absolute containing block width
      nscoord insideBoxBSizing, dummy;
      CalculateBorderPaddingMargin(eLogicalAxisBlock,
                                   blockContentSize.BSize(wm),
                                   &insideBoxBSizing, &dummy);
      boxISize.emplace(
          ComputeISizeValue(wm, blockContentSize,
                            LogicalSize(wm, insideBoxISizing, insideBoxBSizing),
                            outsideBoxISizing, styleISize) +
          insideBoxISizing + outsideBoxISizing);
    }
  }

  // Get the placeholder x-offset and y-offset in the coordinate
  // space of its containing block
  // XXXbz the placeholder is not fully reflowed yet if our containing block is
  // relatively positioned...
  nsSize containerSize =
      containingBlock->HasAnyStateBits(NS_FRAME_IN_REFLOW)
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
      if (mStyleDisplay->IsOriginalDisplayInlineOutside()) {
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
  if (mStyleDisplay->IsOriginalDisplayInlineOutside() ||
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

  // If block direction doesn't match (whether orthogonal or antiparallel),
  // we'll have to convert aHypotheticalPos to be in terms of cbwm.
  // This upcoming conversion must be taken into account for border offsets.
  const bool hypotheticalPosWillUseCbwm =
      cbwm.GetBlockDir() != wm.GetBlockDir();
  // The specified offsets are relative to the absolute containing block's
  // padding edge and our current values are relative to the border edge, so
  // translate.
  const LogicalMargin border = aCBReflowInput->ComputedLogicalBorder(wm);
  if (hypotheticalPosWillUseCbwm && InlinePolarityFlipped(wm, cbwm)) {
    aHypotheticalPos.mIStart += border.IEnd(wm);
  } else {
    aHypotheticalPos.mIStart -= border.IStart(wm);
  }

  if (hypotheticalPosWillUseCbwm && BlockPolarityFlipped(wm, cbwm)) {
    aHypotheticalPos.mBStart += border.BEnd(wm);
  } else {
    aHypotheticalPos.mBStart -= border.BStart(wm);
  }
  // At this point, we have computed aHypotheticalPos using the writing mode
  // of the placeholder's containing block.

  if (hypotheticalPosWillUseCbwm) {
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
      if (mFlags.mIsReplaced && intrinsicSize) {
        // It's a replaced element with an 'auto' block size so the box
        // block size is its intrinsic size plus any border/padding/margin
        boxBSize = LogicalSize(wm, *intrinsicSize).BSize(wm) +
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
      boxBSize = nsLayoutUtils::ComputeBSizeValue(
                     blockContentSize.BSize(wm), insideBoxSizing,
                     styleBSize.AsLengthPercentage()) +
                 insideBoxSizing + outsideBoxSizing;
    }

    LogicalSize boxSize(wm, boxISize.valueOr(0), boxBSize);

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

bool ReflowInput::IsInlineSizeComputableByBlockSizeAndAspectRatio(
    nscoord aBlockSize) const {
  WritingMode wm = GetWritingMode();
  MOZ_ASSERT(!mStylePosition->mOffset.GetBStart(wm).IsAuto() &&
                 !mStylePosition->mOffset.GetBEnd(wm).IsAuto(),
             "If any of the block-start and block-end are auto, aBlockSize "
             "doesn't make sense");
  NS_WARNING_ASSERTION(
      aBlockSize >= 0 && aBlockSize != NS_UNCONSTRAINEDSIZE,
      "The caller shouldn't give us an unresolved or invalid block size");

  if (!mStylePosition->mAspectRatio.HasFiniteRatio()) {
    return false;
  }

  // We don't have to compute the inline size by aspect-ratio and the resolved
  // block size (from insets) for replaced elements.
  if (mFrame->IsFrameOfType(nsIFrame::eReplaced)) {
    return false;
  }

  // If inline size is specified, we should have it by mFrame->ComputeSize()
  // already.
  if (mStylePosition->ISize(wm).IsLengthPercentage()) {
    return false;
  }

  // If both inline insets are non-auto, mFrame->ComputeSize() should get a
  // possible inline size by those insets, so we don't rely on aspect-ratio.
  if (!mStylePosition->mOffset.GetIStart(wm).IsAuto() &&
      !mStylePosition->mOffset.GetIEnd(wm).IsAuto()) {
    return false;
  }

  // Just an error handling. If |aBlockSize| is NS_UNCONSTRAINEDSIZE, there must
  // be something wrong, and we don't want to continue the calculation for
  // aspect-ratio. So we return false if this happens.
  return aBlockSize != NS_UNCONSTRAINEDSIZE;
}

// FIXME: Move this into nsIFrame::ComputeSize() if possible, so most of the
// if-checks can be simplier.
LogicalSize ReflowInput::CalculateAbsoluteSizeWithResolvedAutoBlockSize(
    nscoord aAutoBSize, const LogicalSize& aTentativeComputedSize) {
  LogicalSize resultSize = aTentativeComputedSize;
  WritingMode wm = GetWritingMode();

  // Two cases we don't want to early return:
  // 1. If the block size behaves as initial value and we haven't resolved it in
  //    ComputeSize() yet, we need to apply |aAutoBSize|.
  //    Also, we check both computed style and |resultSize.BSize(wm)| to avoid
  //    applying |aAutoBSize| when the resolved block size is saturated at
  //    nscoord_MAX, and wrongly treated as NS_UNCONSTRAINEDSIZE because of a
  //    giant specified block-size.
  // 2. If the block size needs to be computed via aspect-ratio and
  //    |aAutoBSize|, we need to apply |aAutoBSize|. In this case,
  //    |resultSize.BSize(wm)| may not be NS_UNCONSTRAINEDSIZE because we apply
  //    aspect-ratio in ComputeSize() for block axis by default, so we have to
  //    check its computed style.
  const bool bSizeBehavesAsInitial =
      mStylePosition->BSize(wm).BehavesLikeInitialValueOnBlockAxis();
  const bool bSizeIsStillUnconstrained =
      bSizeBehavesAsInitial && resultSize.BSize(wm) == NS_UNCONSTRAINEDSIZE;
  const bool needsComputeInlineSizeByAspectRatio =
      bSizeBehavesAsInitial &&
      IsInlineSizeComputableByBlockSizeAndAspectRatio(aAutoBSize);
  if (!bSizeIsStillUnconstrained && !needsComputeInlineSizeByAspectRatio) {
    return resultSize;
  }

  // For non-replaced elements with block-size auto, the block-size
  // fills the remaining space, and we clamp it by min/max size constraints.
  resultSize.BSize(wm) = ApplyMinMaxBSize(aAutoBSize);

  if (!needsComputeInlineSizeByAspectRatio) {
    return resultSize;
  }

  // Calculate transferred inline size through aspect-ratio.
  // For non-replaced elements, we always take box-sizing into account.
  const auto boxSizingAdjust =
      mStylePosition->mBoxSizing == StyleBoxSizing::Border
          ? ComputedLogicalBorderPadding(wm).Size(wm)
          : LogicalSize(wm);
  auto transferredISize =
      mStylePosition->mAspectRatio.ToLayoutRatio().ComputeRatioDependentSize(
          LogicalAxis::eLogicalAxisInline, wm, aAutoBSize, boxSizingAdjust);
  resultSize.ISize(wm) = ApplyMinMaxISize(transferredISize);

  MOZ_ASSERT(mFlags.mIsBSizeSetByAspectRatio,
             "This flag should have been set because nsIFrame::ComputeSize() "
             "returns AspectRatioUsage::ToComputeBSize unconditionally for "
             "auto block-size");
  mFlags.mIsBSizeSetByAspectRatio = false;

  return resultSize;
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
  NS_ASSERTION(mFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW),
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
    nsIFrame* placeholderParent = placeholderFrame->GetParent();
    MOZ_ASSERT(placeholderParent, "shouldn't have unparented placeholders");

    if (placeholderFrame->HasAnyStateBits(
            PLACEHOLDER_STATICPOS_NEEDS_CSSALIGN)) {
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
      if (placeholderParent->IsGridContainerFrame() &&
          placeholderParent->HasAnyStateBits(NS_STATE_GRID_IS_COL_MASONRY |
                                             NS_STATE_GRID_IS_ROW_MASONRY)) {
        // Disable CSS alignment in Masonry layout since we don't have real grid
        // areas in that axis.  We'll use the placeholder position instead as it
        // was calculated by nsGridContainerFrame::MasonryLayout.
        auto cbsz = aCBSize.GetPhysicalSize(cbwm);
        LogicalPoint pos = placeholderFrame->GetLogicalPosition(cbwm, cbsz);
        if (placeholderParent->HasAnyStateBits(NS_STATE_GRID_IS_COL_MASONRY)) {
          mFlags.mIOffsetsNeedCSSAlign = false;
          hypotheticalPos.mIStart = pos.I(cbwm);
        } else {
          mFlags.mBOffsetsNeedCSSAlign = false;
          hypotheticalPos.mBStart = pos.B(cbwm);
        }
      }
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
  LogicalMargin offsets = ComputedLogicalOffsets(cbwm);

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

  SetComputedLogicalOffsets(cbwm, offsets);

  if (wm.IsOrthogonalTo(cbwm)) {
    if (bStartIsAuto || bEndIsAuto) {
      mComputeSizeFlags += ComputeSizeFlag::ShrinkWrap;
    }
  } else {
    if (iStartIsAuto || iEndIsAuto) {
      mComputeSizeFlags += ComputeSizeFlag::ShrinkWrap;
    }
  }

  nsIFrame::SizeComputationResult sizeResult = {
      LogicalSize(wm), nsIFrame::AspectRatioUsage::None};
  {
    AutoMaybeDisableFontInflation an(mFrame);

    sizeResult = mFrame->ComputeSize(
        mRenderingContext, wm, cbSize.ConvertTo(wm, cbwm),
        cbSize.ConvertTo(wm, cbwm).ISize(wm),  // XXX or AvailableISize()?
        ComputedLogicalMargin(wm).Size(wm) +
            ComputedLogicalOffsets(wm).Size(wm),
        ComputedLogicalBorderPadding(wm).Size(wm), {}, mComputeSizeFlags);
    ComputedISize() = sizeResult.mLogicalSize.ISize(wm);
    ComputedBSize() = sizeResult.mLogicalSize.BSize(wm);
    NS_ASSERTION(ComputedISize() >= 0, "Bogus inline-size");
    NS_ASSERTION(
        ComputedBSize() == NS_UNCONSTRAINEDSIZE || ComputedBSize() >= 0,
        "Bogus block-size");
  }

  LogicalSize& computedSize = sizeResult.mLogicalSize;
  computedSize = computedSize.ConvertTo(cbwm, wm);

  mFlags.mIsBSizeSetByAspectRatio = sizeResult.mAspectRatioUsage ==
                                    nsIFrame::AspectRatioUsage::ToComputeBSize;

  // XXX Now that we have ComputeSize, can we condense many of the
  // branches off of widthIsAuto?

  LogicalMargin margin = ComputedLogicalMargin(cbwm);
  const LogicalMargin borderPadding = ComputedLogicalBorderPadding(cbwm);

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
  } else if (!mFrame->HasIntrinsicKeywordForBSize() ||
             !wm.IsOrthogonalTo(cbwm)) {
    // Neither 'inline-start' nor 'inline-end' is 'auto'.
    if (wm.IsOrthogonalTo(cbwm)) {
      // For orthogonal blocks, we need to handle the case where the block had
      // unconstrained block-size, which mapped to unconstrained inline-size
      // in the containing block's writing mode.
      nscoord autoISize = cbSize.ISize(cbwm) - margin.IStartEnd(cbwm) -
                          borderPadding.IStartEnd(cbwm) -
                          offsets.IStartEnd(cbwm);
      autoISize = std::max(autoISize, 0);
      // FIXME: Bug 1602669: if |autoISize| happens to be numerically equal to
      // NS_UNCONSTRAINEDSIZE, we may get some unexpected behavior. We need a
      // better way to distinguish between unconstrained size and resolved
      // size.
      NS_WARNING_ASSERTION(autoISize != NS_UNCONSTRAINEDSIZE,
                           "Unexpected size from inline-start and inline-end");

      nscoord autoBSizeInWM = autoISize;
      LogicalSize computedSizeInWM =
          CalculateAbsoluteSizeWithResolvedAutoBlockSize(
              autoBSizeInWM, computedSize.ConvertTo(wm, cbwm));
      computedSize = computedSizeInWM.ConvertTo(cbwm, wm);
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
    ComputeAbsPosInlineAutoMargin(availMarginSpace, cbwm, marginIStartIsAuto,
                                  marginIEndIsAuto, margin, offsets);
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
  } else if (!mFrame->HasIntrinsicKeywordForBSize() ||
             wm.IsOrthogonalTo(cbwm)) {
    // Neither block-start nor -end is 'auto'.
    nscoord autoBSize = cbSize.BSize(cbwm) - margin.BStartEnd(cbwm) -
                        borderPadding.BStartEnd(cbwm) - offsets.BStartEnd(cbwm);
    autoBSize = std::max(autoBSize, 0);
    // FIXME: Bug 1602669: if |autoBSize| happens to be numerically equal to
    // NS_UNCONSTRAINEDSIZE, we may get some unexpected behavior. We need a
    // better way to distinguish between unconstrained size and resolved size.
    NS_WARNING_ASSERTION(autoBSize != NS_UNCONSTRAINEDSIZE,
                         "Unexpected size from block-start and block-end");

    // For orthogonal case, the inline size in |wm| should have been handled by
    // ComputeSize(). In other words, we only have to apply |autoBSize| to
    // the computed size if this value can represent the block size in |wm|.
    if (!wm.IsOrthogonalTo(cbwm)) {
      // We handle the unconstrained block-size in current block's writing
      // mode 'wm'.
      LogicalSize computedSizeInWM =
          CalculateAbsoluteSizeWithResolvedAutoBlockSize(
              autoBSize, computedSize.ConvertTo(wm, cbwm));
      computedSize = computedSizeInWM.ConvertTo(cbwm, wm);
    }

    // The block-size might still not fill all the available space in case:
    //  * bsize was specified
    //  * we're dealing with a replaced element
    //  * bsize was constrained by min- or max-bsize.
    nscoord availMarginSpace = autoBSize - computedSize.BSize(cbwm);
    marginBStartIsAuto = mStyleMargin->mMargin.GetBStart(cbwm).IsAuto();
    marginBEndIsAuto = mStyleMargin->mMargin.GetBEnd(cbwm).IsAuto();

    ComputeAbsPosBlockAutoMargin(availMarginSpace, cbwm, marginBStartIsAuto,
                                 marginBEndIsAuto, margin, offsets);
  }
  ComputedBSize() = computedSize.ConvertTo(wm, cbwm).BSize(wm);
  ComputedISize() = computedSize.ConvertTo(wm, cbwm).ISize(wm);

  SetComputedLogicalOffsets(cbwm, offsets);
  SetComputedLogicalMargin(cbwm, margin);

  // If we have auto margins, update our UsedMarginProperty. The property
  // will have already been created by InitOffsets if it is needed.
  if (marginIStartIsAuto || marginIEndIsAuto || marginBStartIsAuto ||
      marginBEndIsAuto) {
    nsMargin* propValue = mFrame->GetProperty(nsIFrame::UsedMarginProperty());
    MOZ_ASSERT(propValue,
               "UsedMarginProperty should have been created "
               "by InitOffsets.");
    *propValue = margin.GetPhysicalMargin(cbwm);
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

  if (aContainingBlockRI->mFlags.mTreatBSizeAsIndefinite) {
    cbSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
  }

  if (((mFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) &&
        // XXXfr hack for making frames behave properly when in overflow
        // container lists, see bug 154892; need to revisit later
        !mFrame->GetPrevInFlow()) ||
       (mFrame->IsTableFrame() &&
        mFrame->GetParent()->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW))) &&
      mStyleDisplay->IsAbsolutelyPositioned(mFrame)) {
    // See if the ancestor is block-level or inline-level
    const auto computedPadding = aContainingBlockRI->ComputedLogicalPadding(wm);
    if (aContainingBlockRI->mStyleDisplay->IsInlineOutsideStyle()) {
      // Base our size on the actual size of the frame.  In cases when this is
      // completely bogus (eg initial reflow), this code shouldn't even be
      // called, since the code in nsInlineFrame::Reflow will pass in
      // the containing block dimensions to our constructor.
      // XXXbz we should be taking the in-flows into account too, but
      // that's very hard.

      LogicalMargin computedBorder =
          aContainingBlockRI->ComputedLogicalBorderPadding(wm) -
          computedPadding;
      cbSize.ISize(wm) =
          aContainingBlockRI->mFrame->ISize(wm) - computedBorder.IStartEnd(wm);
      NS_ASSERTION(cbSize.ISize(wm) >= 0, "Negative containing block isize!");
      cbSize.BSize(wm) =
          aContainingBlockRI->mFrame->BSize(wm) - computedBorder.BStartEnd(wm);
      NS_ASSERTION(cbSize.BSize(wm) >= 0, "Negative containing block bsize!");
    } else {
      // If the ancestor is block-level, the containing block is formed by the
      // padding edge of the ancestor
      cbSize += computedPadding.Size(wm);
    }
  } else {
    auto IsQuirky = [](const StyleSize& aSize) -> bool {
      return aSize.ConvertsToPercentage();
    };
    // an element in quirks mode gets a containing block based on looking for a
    // parent with a non-auto height if the element has a percent height.
    // Note: We don't emulate this quirk for percents in calc(), or in vertical
    // writing modes, or if the containing block is a flex or grid item.
    if (!wm.IsVertical() && NS_UNCONSTRAINEDSIZE == cbSize.BSize(wm)) {
      if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
          !aContainingBlockRI->mFrame->IsFlexOrGridItem() &&
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
  auto captionSide = aFrame->StyleTableBorder()->mCaptionSide;
  return captionSide == StyleCaptionSide::Left ||
         captionSide == StyleCaptionSide::Right;
}

// XXX refactor this code to have methods for each set of properties
// we are computing: width,height,line-height; margin; offsets

void ReflowInput::InitConstraints(
    nsPresContext* aPresContext, const Maybe<LogicalSize>& aContainingBlockSize,
    const Maybe<LogicalMargin>& aBorder, const Maybe<LogicalMargin>& aPadding,
    LayoutFrameType aFrameType) {
  MOZ_ASSERT(!mStyleDisplay->IsFloating(mFrame) ||
                 (mStyleDisplay->mDisplay != StyleDisplay::MozBox &&
                  mStyleDisplay->mDisplay != StyleDisplay::MozInlineBox),
             "Please don't try to float a -moz-box or a -moz-inline-box");

  WritingMode wm = GetWritingMode();
  LogicalSize cbSize = aContainingBlockSize.valueOr(
      LogicalSize(mWritingMode, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE));
  DISPLAY_INIT_CONSTRAINTS(mFrame, this, cbSize.ISize(wm), cbSize.BSize(wm),
                           aBorder, aPadding);

  // If this is a reflow root, then set the computed width and
  // height equal to the available space
  if (nullptr == mParentReflowInput || mFlags.mDummyParentReflowInput) {
    // XXXldb This doesn't mean what it used to!
    InitOffsets(wm, cbSize.ISize(wm), aFrameType, mComputeSizeFlags, aBorder,
                aPadding, mStyleDisplay);
    // Override mComputedMargin since reflow roots start from the
    // frame's boundary, which is inside the margin.
    SetComputedLogicalMargin(wm, LogicalMargin(wm));
    SetComputedLogicalOffsets(wm, LogicalMargin(wm));

    const auto borderPadding = ComputedLogicalBorderPadding(wm);
    ComputedISize() = AvailableISize() - borderPadding.IStartEnd(wm);
    if (ComputedISize() < 0) {
      ComputedISize() = 0;
    }
    if (AvailableBSize() != NS_UNCONSTRAINEDSIZE) {
      ComputedBSize() = AvailableBSize() - borderPadding.BStartEnd(wm);
      if (ComputedBSize() < 0) {
        ComputedBSize() = 0;
      }
    } else {
      ComputedBSize() = NS_UNCONSTRAINEDSIZE;
    }

    ComputedMinISize() = ComputedMinBSize() = 0;
    ComputedMaxBSize() = ComputedMaxBSize() = NS_UNCONSTRAINEDSIZE;
  } else {
    // Get the containing block's reflow input
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
                mComputeSizeFlags, aBorder, aPadding, mStyleDisplay);

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
        if (mFlags.mIsReplaced && mStyleDisplay->IsInlineOutsideStyle()) {
          // Get the containing block's reflow input
          NS_ASSERTION(nullptr != cbri, "no containing block");
          // in quirks mode, get the cb height using the special quirk method
          if (!wm.IsVertical() &&
              eCompatibility_NavQuirks == aPresContext->CompatibilityMode()) {
            if (!cbri->mFrame->IsTableCellFrame() &&
                !cbri->mFrame->IsFlexOrGridItem()) {
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
    if (mStyleDisplay->IsRelativelyPositioned(mFrame)) {
      const LogicalMargin offsets =
          ComputeRelativeOffsets(cbwm, mFrame, cbSize.ConvertTo(cbwm, wm));
      SetComputedLogicalOffsets(cbwm, offsets);
    } else {
      // Initialize offsets to 0
      SetComputedLogicalOffsets(wm, LogicalMargin(wm));
    }

    // Calculate the computed values for min and max properties.  Note that
    // this MUST come after we've computed our border and padding.
    ComputeMinMaxValues(cbSize);

    // Calculate the computed inlineSize and blockSize.
    // This varies by frame type.

    if (IsInternalTableFrame()) {
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
          ComputedISize() -= ComputedLogicalBorderPadding(wm).IStartEnd(wm);
          if (ComputedISize() < 0) ComputedISize() = 0;
        }
        NS_ASSERTION(ComputedISize() >= 0, "Bogus computed isize");

      } else {
        ComputedISize() =
            ComputeISizeValue(cbSize, mStylePosition->mBoxSizing, inlineSize);
      }

      // Calculate the computed block size
      if (StyleDisplay::TableColumn == mStyleDisplay->mDisplay ||
          StyleDisplay::TableColumnGroup == mStyleDisplay->mDisplay) {
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

      // Doesn't apply to internal table elements
      ComputedMinISize() = ComputedMinBSize() = 0;
      ComputedMaxISize() = ComputedMaxBSize() = NS_UNCONSTRAINEDSIZE;

    } else if (mFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) &&
               mStyleDisplay->IsAbsolutelyPositionedStyle() &&
               // XXXfr hack for making frames behave properly when in overflow
               // container lists, see bug 154892; need to revisit later
               !mFrame->GetPrevInFlow()) {
      InitAbsoluteConstraints(aPresContext, cbri,
                              cbSize.ConvertTo(cbri->GetWritingMode(), wm),
                              aFrameType);
    } else {
      AutoMaybeDisableFontInflation an(mFrame);

      const bool isBlockLevel =
          ((!mStyleDisplay->IsInlineOutsideStyle() &&
            // internal table values on replaced elements behaves as inline
            // https://drafts.csswg.org/css-tables-3/#table-structure
            // "... it is handled instead as though the author had declared
            //  either 'block' (for 'table' display) or 'inline' (for all
            //  other values)"
            !(mFlags.mIsReplaced && (mStyleDisplay->IsInnerTableStyle() ||
                                     mStyleDisplay->DisplayOutside() ==
                                         StyleDisplayOutside::TableCaption))) ||
           // The inner table frame always fills its outer wrapper table frame,
           // even for 'inline-table'.
           mFrame->IsTableFrame()) &&
          // XXX abs.pos. continuations treated like blocks, see comment in
          // the else-if condition above.
          (!mFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) ||
           mStyleDisplay->IsAbsolutelyPositionedStyle());

      if (!isBlockLevel) {
        mComputeSizeFlags += ComputeSizeFlag::ShrinkWrap;
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
                ? mStylePosition->UsedAlignSelf(alignCB->Style())._0
                : mStylePosition->UsedJustifySelf(alignCB->Style())._0;
        if ((inlineAxisAlignment != StyleAlignFlags::STRETCH &&
             inlineAxisAlignment != StyleAlignFlags::NORMAL) ||
            mStyleMargin->mMargin.GetIStart(wm).IsAuto() ||
            mStyleMargin->mMargin.GetIEnd(wm).IsAuto()) {
          mComputeSizeFlags += ComputeSizeFlag::ShrinkWrap;
        }
      } else {
        // Shrink-wrap blocks that are orthogonal to their container.
        if (isBlockLevel && mCBReflowInput &&
            mCBReflowInput->GetWritingMode().IsOrthogonalTo(mWritingMode)) {
          mComputeSizeFlags += ComputeSizeFlag::ShrinkWrap;
        }

        if (alignCB->IsFlexContainerFrame()) {
          mComputeSizeFlags += ComputeSizeFlag::ShrinkWrap;
        }
      }

      if (cbSize.ISize(wm) == NS_UNCONSTRAINEDSIZE) {
        // For orthogonal flows, where we found a parent orthogonal-limit
        // for AvailableISize() in Init(), we'll use the same here as well.
        cbSize.ISize(wm) = AvailableISize();
      }

      auto size =
          mFrame->ComputeSize(mRenderingContext, wm, cbSize, AvailableISize(),
                              ComputedLogicalMargin(wm).Size(wm),
                              ComputedLogicalBorderPadding(wm).Size(wm),
                              mStyleSizeOverrides, mComputeSizeFlags);

      mComputedSize = size.mLogicalSize;
      NS_ASSERTION(ComputedISize() >= 0, "Bogus inline-size");
      NS_ASSERTION(
          ComputedBSize() == NS_UNCONSTRAINEDSIZE || ComputedBSize() >= 0,
          "Bogus block-size");

      mFlags.mIsBSizeSetByAspectRatio =
          size.mAspectRatioUsage == nsIFrame::AspectRatioUsage::ToComputeBSize;

      // Exclude inline tables, side captions, outside ::markers, flex and grid
      // items from block margin calculations.
      if (isBlockLevel && !IsSideCaption(mFrame, mStyleDisplay, cbwm) &&
          mStyleDisplay->mDisplay != StyleDisplay::InlineTable &&
          !mFrame->IsTableFrame() && !alignCB->IsFlexOrGridContainer() &&
          !(mFrame->Style()->GetPseudoType() == PseudoStyleType::marker &&
            mFrame->GetParent()->StyleList()->mListStylePosition ==
                NS_STYLE_LIST_STYLE_POSITION_OUTSIDE)) {
        CalculateBlockSideMargins();
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
    aFrame->RemoveProperty(aProperty);
  }
}

void SizeComputationInput::InitOffsets(WritingMode aCBWM, nscoord aPercentBasis,
                                       LayoutFrameType aFrameType,
                                       ComputeSizeFlags aFlags,
                                       const Maybe<LogicalMargin>& aBorder,
                                       const Maybe<LogicalMargin>& aPadding,
                                       const nsStyleDisplay* aDisplay) {
  DISPLAY_INIT_OFFSETS(mFrame, this, aPercentBasis, aCBWM, aBorder, aPadding);

  // Since we are in reflow, we don't need to store these properties anymore
  // unless they are dependent on width, in which case we store the new value.
  nsPresContext* presContext = mFrame->PresContext();
  mFrame->RemoveProperty(nsIFrame::UsedBorderProperty());

  // Compute margins from the specified margin style information. These
  // become the default computed values, and may be adjusted below
  // XXX fix to provide 0,0 for the top&bottom margins for
  // inline-non-replaced elements
  bool needMarginProp = ComputeMargin(aCBWM, aPercentBasis, aFrameType);
  // Note that ComputeMargin() simplistically resolves 'auto' margins to 0.
  // In formatting contexts where this isn't correct, some later code will
  // need to update the UsedMargin() property with the actual resolved value.
  // One example of this is ::CalculateBlockSideMargins().
  ::UpdateProp(mFrame, nsIFrame::UsedMarginProperty(), needMarginProp,
               ComputedPhysicalMargin());

  const WritingMode wm = GetWritingMode();
  const nsStyleDisplay* disp = mFrame->StyleDisplayWithOptionalParam(aDisplay);
  bool isThemed = mFrame->IsThemed(disp);
  bool needPaddingProp;
  LayoutDeviceIntMargin widgetPadding;
  if (isThemed && presContext->Theme()->GetWidgetPadding(
                      presContext->DeviceContext(), mFrame,
                      disp->EffectiveAppearance(), &widgetPadding)) {
    const nsMargin padding = LayoutDevicePixel::ToAppUnits(
        widgetPadding, presContext->AppUnitsPerDevPixel());
    SetComputedLogicalPadding(wm, LogicalMargin(wm, padding));
    needPaddingProp = false;
  } else if (SVGUtils::IsInSVGTextSubtree(mFrame)) {
    SetComputedLogicalPadding(wm, LogicalMargin(wm));
    needPaddingProp = false;
  } else if (aPadding) {  // padding is an input arg
    SetComputedLogicalPadding(wm, *aPadding);
    nsMargin stylePadding;
    // If the caller passes a padding that doesn't match our style (like
    // nsTextControlFrame might due due to theming), then we also need a
    // padding prop.
    needPaddingProp = !mFrame->StylePadding()->GetPadding(stylePadding) ||
                      aPadding->GetPhysicalMargin(wm) != stylePadding;
  } else {
    needPaddingProp = ComputePadding(aCBWM, aPercentBasis, aFrameType);
  }

  // Add [align|justify]-content:baseline padding contribution.
  typedef const FramePropertyDescriptor<SmallValueHolder<nscoord>>* Prop;
  auto ApplyBaselinePadding = [this, wm, &needPaddingProp](LogicalAxis aAxis,
                                                           Prop aProp) {
    bool found;
    nscoord val = mFrame->GetProperty(aProp, &found);
    if (found) {
      NS_ASSERTION(val != nscoord(0), "zero in this property is useless");
      LogicalSide side;
      if (val > 0) {
        side = MakeLogicalSide(aAxis, eLogicalEdgeStart);
      } else {
        side = MakeLogicalSide(aAxis, eLogicalEdgeEnd);
        val = -val;
      }
      mComputedPadding.Side(side, wm) += val;
      needPaddingProp = true;
      if (aAxis == eLogicalAxisBlock && val > 0) {
        // We have a baseline-adjusted block-axis start padding, so
        // we need this to mark lines dirty when mIsBResize is true:
        this->mFrame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
      }
    }
  };
  if (!aFlags.contains(ComputeSizeFlag::IsGridMeasuringReflow)) {
    ApplyBaselinePadding(eLogicalAxisBlock, nsIFrame::BBaselinePadProperty());
  }
  if (!aFlags.contains(ComputeSizeFlag::ShrinkWrap)) {
    ApplyBaselinePadding(eLogicalAxisInline, nsIFrame::IBaselinePadProperty());
  }

  LogicalMargin border(wm);
  if (isThemed) {
    const LayoutDeviceIntMargin widgetBorder =
        presContext->Theme()->GetWidgetBorder(
            presContext->DeviceContext(), mFrame, disp->EffectiveAppearance());
    border = LogicalMargin(
        wm, LayoutDevicePixel::ToAppUnits(widgetBorder,
                                          presContext->AppUnitsPerDevPixel()));
  } else if (SVGUtils::IsInSVGTextSubtree(mFrame)) {
    // Do nothing since the border local variable is initialized all zero.
  } else if (aBorder) {  // border is an input arg
    border = *aBorder;
  } else {
    border = LogicalMargin(wm, mFrame->StyleBorder()->GetComputedBorder());
  }
  SetComputedLogicalBorderPadding(wm, border + ComputedLogicalPadding(wm));

  if (aFrameType == LayoutFrameType::Scrollbar) {
    // scrollbars may have had their width or height smashed to zero
    // by the associated scrollframe, in which case we must not report
    // any padding or border.
    nsSize size(mFrame->GetSize());
    if (size.width == 0 || size.height == 0) {
      SetComputedLogicalPadding(wm, LogicalMargin(wm));
      SetComputedLogicalBorderPadding(wm, LogicalMargin(wm));
    }
  }

  bool hasPaddingChange;
  if (nsMargin* oldPadding =
          mFrame->GetProperty(nsIFrame::UsedPaddingProperty())) {
    // Note: If a padding change is already detectable without resolving the
    // percentage, e.g. a padding is changing from 50px to 50%,
    // nsIFrame::DidSetComputedStyle() will cache the old padding in
    // UsedPaddingProperty().
    hasPaddingChange = *oldPadding != ComputedPhysicalPadding();
  } else {
    // Our padding may have changed, but we can't tell at this point.
    hasPaddingChange = needPaddingProp;
  }
  // Keep mHasPaddingChange bit set until we've done reflow. We'll clear it in
  // nsIFrame::DidReflow()
  mFrame->SetHasPaddingChange(mFrame->HasPaddingChange() || hasPaddingChange);

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
void ReflowInput::CalculateBlockSideMargins() {
  MOZ_ASSERT(!mFrame->IsTableFrame(),
             "Inner table frame cannot have computed margins!");

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
    // for AvailableISize() in Init(), we don't have meaningful sizes to
    // adjust.  Act like the sum is already correct (below).
    return;
  }

  LAYOUT_WARN_IF_FALSE(NS_UNCONSTRAINEDSIZE != computedISizeCBWM &&
                           NS_UNCONSTRAINEDSIZE != availISizeCBWM,
                       "have unconstrained inline-size; this should only "
                       "result from very large sizes, not attempts at "
                       "intrinsic inline-size calculation");

  LogicalMargin margin = ComputedLogicalMargin(cbWM);
  LogicalMargin borderPadding = ComputedLogicalBorderPadding(cbWM);
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
    SetComputedLogicalMargin(cbWM, margin);
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
    const StyleTextAlign* textAlign =
        mParentReflowInput
            ? &mParentReflowInput->mFrame->StyleText()->mTextAlign
            : nullptr;
    if (textAlign && (*textAlign == StyleTextAlign::MozLeft ||
                      *textAlign == StyleTextAlign::MozCenter ||
                      *textAlign == StyleTextAlign::MozRight)) {
      if (mParentReflowInput->mWritingMode.IsBidiLTR()) {
        isAutoStartMargin = *textAlign != StyleTextAlign::MozLeft;
        isAutoEndMargin = *textAlign != StyleTextAlign::MozRight;
      } else {
        isAutoStartMargin = *textAlign != StyleTextAlign::MozRight;
        isAutoEndMargin = *textAlign != StyleTextAlign::MozLeft;
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
  SetComputedLogicalMargin(cbWM, margin);

  if (isAutoStartMargin || isAutoEndMargin) {
    // Update the UsedMargin property if we were tracking it already.
    nsMargin* propValue = mFrame->GetProperty(nsIFrame::UsedMarginProperty());
    if (propValue) {
      *propValue = margin.GetPhysicalMargin(cbWM);
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

static inline nscoord ComputeLineHeight(const ComputedStyle* aComputedStyle,
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
    return aComputedStyle->StyleFont()
        ->mFont.size.ScaledBy(lineHeight.AsNumber() * aFontSizeInflation)
        .ToAppUnits();
  }

  MOZ_ASSERT(lineHeight.IsNormal() || lineHeight.IsMozBlockHeight());
  if (lineHeight.IsMozBlockHeight() && aBlockBSize != NS_UNCONSTRAINEDSIZE) {
    return aBlockBSize;
  }

  RefPtr<nsFontMetrics> fm = nsLayoutUtils::GetFontMetricsForComputedStyle(
      aComputedStyle, aPresContext, aFontSizeInflation);
  return GetNormalLineHeight(fm);
}

nscoord ReflowInput::GetLineHeight() const {
  if (mLineHeight != NS_UNCONSTRAINEDSIZE) {
    return mLineHeight;
  }

  nscoord blockBSize = nsLayoutUtils::IsNonWrapperBlock(mFrame)
                           ? ComputedBSize()
                           : (mCBReflowInput ? mCBReflowInput->ComputedBSize()
                                             : NS_UNCONSTRAINEDSIZE);
  mLineHeight = CalcLineHeight(mFrame->GetContent(), mFrame->Style(),
                               mFrame->PresContext(), blockBSize,
                               nsLayoutUtils::FontSizeInflationFor(mFrame));
  return mLineHeight;
}

void ReflowInput::SetLineHeight(nscoord aLineHeight) {
  MOZ_ASSERT(aLineHeight >= 0, "aLineHeight must be >= 0!");

  if (mLineHeight != aLineHeight) {
    mLineHeight = aLineHeight;
    // Setting used line height can change a frame's block-size if mFrame's
    // block-size behaves as auto.
    InitResizeFlags(mFrame->PresContext(), mFrame->Type());
  }
}

/* static */
nscoord ReflowInput::CalcLineHeight(nsIContent* aContent,
                                    const ComputedStyle* aComputedStyle,
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
    // have a line-height smaller than 'normal'.
    const StyleLineHeight& lh = aComputedStyle->StyleText()->mLineHeight;
    if (!lh.IsNormal()) {
      RefPtr<nsFontMetrics> fm = nsLayoutUtils::GetFontMetricsForComputedStyle(
          aComputedStyle, aPresContext, aFontSizeInflation);
      nscoord normal = GetNormalLineHeight(fm);
      if (lineHeight < normal) {
        lineHeight = normal;
      }
    }
  }

  return lineHeight;
}

bool SizeComputationInput::ComputeMargin(WritingMode aCBWM,
                                         nscoord aPercentBasis,
                                         LayoutFrameType aFrameType) {
  // SVG text frames have no margin.
  if (SVGUtils::IsInSVGTextSubtree(mFrame)) {
    return false;
  }

  if (aFrameType == LayoutFrameType::Table) {
    // Table frame's margin is inherited to the table wrapper frame via the
    // ::-moz-table-wrapper rule in ua.css, so don't set any margins for it.
    SetComputedLogicalMargin(mWritingMode, LogicalMargin(mWritingMode));
    return false;
  }

  // If style style can provide us the margin directly, then use it.
  const nsStyleMargin* styleMargin = mFrame->StyleMargin();

  nsMargin margin;
  const bool isCBDependent = !styleMargin->GetMargin(margin);
  if (isCBDependent) {
    // We have to compute the value. Note that this calculation is
    // performed according to the writing mode of the containing block
    // (http://dev.w3.org/csswg/css-writing-modes-3/#orthogonal-flows)
    if (aPercentBasis == NS_UNCONSTRAINEDSIZE) {
      aPercentBasis = 0;
    }
    LogicalMargin m(aCBWM);
    m.IStart(aCBWM) = nsLayoutUtils::ComputeCBDependentValue(
        aPercentBasis, styleMargin->mMargin.GetIStart(aCBWM));
    m.IEnd(aCBWM) = nsLayoutUtils::ComputeCBDependentValue(
        aPercentBasis, styleMargin->mMargin.GetIEnd(aCBWM));

    m.BStart(aCBWM) = nsLayoutUtils::ComputeCBDependentValue(
        aPercentBasis, styleMargin->mMargin.GetBStart(aCBWM));
    m.BEnd(aCBWM) = nsLayoutUtils::ComputeCBDependentValue(
        aPercentBasis, styleMargin->mMargin.GetBEnd(aCBWM));

    SetComputedLogicalMargin(aCBWM, m);
  } else {
    SetComputedLogicalMargin(mWritingMode, LogicalMargin(mWritingMode, margin));
  }

  // ... but font-size-inflation-based margin adjustment uses the
  // frame's writing mode
  nscoord marginAdjustment = FontSizeInflationListMarginAdjustment(mFrame);

  if (marginAdjustment > 0) {
    LogicalMargin m = ComputedLogicalMargin(mWritingMode);
    m.IStart(mWritingMode) += marginAdjustment;
    SetComputedLogicalMargin(mWritingMode, m);
  }

  return isCBDependent;
}

bool SizeComputationInput::ComputePadding(WritingMode aCBWM,
                                          nscoord aPercentBasis,
                                          LayoutFrameType aFrameType) {
  // If style can provide us the padding directly, then use it.
  const nsStylePadding* stylePadding = mFrame->StylePadding();
  nsMargin padding;
  bool isCBDependent = !stylePadding->GetPadding(padding);
  // a table row/col group, row/col doesn't have padding
  // XXXldb Neither do border-collapse tables.
  if (LayoutFrameType::TableRowGroup == aFrameType ||
      LayoutFrameType::TableColGroup == aFrameType ||
      LayoutFrameType::TableRow == aFrameType ||
      LayoutFrameType::TableCol == aFrameType) {
    SetComputedLogicalPadding(mWritingMode, LogicalMargin(mWritingMode));
  } else if (isCBDependent) {
    // We have to compute the value. This calculation is performed
    // according to the writing mode of the containing block
    // (http://dev.w3.org/csswg/css-writing-modes-3/#orthogonal-flows)
    // clamp negative calc() results to 0
    if (aPercentBasis == NS_UNCONSTRAINEDSIZE) {
      aPercentBasis = 0;
    }
    LogicalMargin p(aCBWM);
    p.IStart(aCBWM) = std::max(
        0, nsLayoutUtils::ComputeCBDependentValue(
               aPercentBasis, stylePadding->mPadding.GetIStart(aCBWM)));
    p.IEnd(aCBWM) =
        std::max(0, nsLayoutUtils::ComputeCBDependentValue(
                        aPercentBasis, stylePadding->mPadding.GetIEnd(aCBWM)));

    p.BStart(aCBWM) = std::max(
        0, nsLayoutUtils::ComputeCBDependentValue(
               aPercentBasis, stylePadding->mPadding.GetBStart(aCBWM)));
    p.BEnd(aCBWM) =
        std::max(0, nsLayoutUtils::ComputeCBDependentValue(
                        aPercentBasis, stylePadding->mPadding.GetBEnd(aCBWM)));

    SetComputedLogicalPadding(aCBWM, p);
  } else {
    SetComputedLogicalPadding(mWritingMode,
                              LogicalMargin(mWritingMode, padding));
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
    ComputedMinISize() =
        ComputeISizeValue(aCBSize, mStylePosition->mBoxSizing, minISize);
  }

  if (maxISize.IsNone()) {
    // Specified value of 'none'
    ComputedMaxISize() = NS_UNCONSTRAINEDSIZE;  // no limit
  } else {
    ComputedMaxISize() =
        ComputeISizeValue(aCBSize, mStylePosition->mBoxSizing, maxISize);
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
  const bool isInternalTableFrame = IsInternalTableFrame();
  const nscoord& bPercentageBasis = aCBSize.BSize(wm);
  auto BSizeBehavesAsInitialValue = [&](const auto& aBSize) {
    if (nsLayoutUtils::IsAutoBSize(aBSize, bPercentageBasis)) {
      return true;
    }
    if (isInternalTableFrame) {
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

bool ReflowInput::IsInternalTableFrame() const {
  return mFrame->IsTableRowGroupFrame() || mFrame->IsTableColGroupFrame() ||
         mFrame->IsTableRowFrame() || mFrame->IsTableCellFrame();
}
