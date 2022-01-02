/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* struct containing the input to nsIFrame::Reflow */

#ifndef mozilla_ReflowInput_h
#define mozilla_ReflowInput_h

#include "nsMargin.h"
#include "nsStyleConsts.h"
#include "mozilla/Assertions.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Maybe.h"
#include "mozilla/WritingModes.h"
#include "LayoutConstants.h"
#include "ReflowOutput.h"
#include <algorithm>

class gfxContext;
class nsFloatManager;
struct nsHypotheticalPosition;
class nsIPercentBSizeObserver;
class nsLineLayout;
class nsPlaceholderFrame;
class nsPresContext;
class nsReflowStatus;

namespace mozilla {
enum class LayoutFrameType : uint8_t;

/**
 * A set of StyleSizes used as an input parameter to various functions that
 * compute sizes like nsIFrame::ComputeSize(). If any of the member fields has a
 * value, the function may use the value instead of retrieving it from the
 * frame's style.
 *
 * The logical sizes are assumed to be in the associated frame's writing-mode.
 */
struct StyleSizeOverrides {
  Maybe<StyleSize> mStyleISize;
  Maybe<StyleSize> mStyleBSize;
  Maybe<AspectRatio> mAspectRatio;

  bool HasAnyOverrides() const { return mStyleISize || mStyleBSize; }
  bool HasAnyLengthOverrides() const {
    return (mStyleISize && mStyleISize->ConvertsToLength()) ||
           (mStyleBSize && mStyleBSize->ConvertsToLength());
  }

  // By default, table wrapper frame considers the size overrides applied to
  // itself, so it creates any length size overrides for inner table frame by
  // subtracting the area occupied by the caption and border & padding according
  // to box-sizing.
  //
  // When this flag is true, table wrapper frame is required to apply the size
  // overrides to the inner table frame directly, without any modification,
  // which is useful for flex container to override the inner table frame's
  // preferred main size with 'flex-basis'.
  //
  // Note: if mStyleISize is a LengthPercentage, the inner table frame will
  // comply with the inline-size override without enforcing its min-content
  // inline-size in nsTableFrame::ComputeSize(). This is necessary so that small
  // flex-basis values like 'flex-basis:1%' can be resolved correctly; the
  // flexbox layout algorithm does still explicitly clamp to min-sizes *at a
  // later step*, after the flex-basis has been resolved -- so this flag won't
  // actually produce any user-visible tables whose final inline size is smaller
  // than their min-content inline size.
  bool mApplyOverridesVerbatim = false;
};
}  // namespace mozilla

/**
 * @return aValue clamped to [aMinValue, aMaxValue].
 *
 * @note This function needs to handle aMinValue > aMaxValue. In that case,
 *       aMinValue is returned.
 * @see http://www.w3.org/TR/CSS21/visudet.html#min-max-widths
 * @see http://www.w3.org/TR/CSS21/visudet.html#min-max-heights
 */
template <class NumericType>
NumericType NS_CSS_MINMAX(NumericType aValue, NumericType aMinValue,
                          NumericType aMaxValue) {
  NumericType result = aValue;
  if (aMaxValue < result) result = aMaxValue;
  if (aMinValue > result) result = aMinValue;
  return result;
}

namespace mozilla {

// A base class of ReflowInput that computes only the padding,
// border, and margin, since those values are needed more often.
struct SizeComputationInput {
 public:
  // The frame being reflowed.
  nsIFrame* mFrame;

  // Rendering context to use for measurement.
  gfxContext* mRenderingContext;

  nsMargin ComputedPhysicalMargin() const {
    return mComputedMargin.GetPhysicalMargin(mWritingMode);
  }
  nsMargin ComputedPhysicalBorderPadding() const {
    return mComputedBorderPadding.GetPhysicalMargin(mWritingMode);
  }
  nsMargin ComputedPhysicalPadding() const {
    return mComputedPadding.GetPhysicalMargin(mWritingMode);
  }

  mozilla::LogicalMargin ComputedLogicalMargin(mozilla::WritingMode aWM) const {
    return mComputedMargin.ConvertTo(aWM, mWritingMode);
  }
  mozilla::LogicalMargin ComputedLogicalBorderPadding(
      mozilla::WritingMode aWM) const {
    return mComputedBorderPadding.ConvertTo(aWM, mWritingMode);
  }
  mozilla::LogicalMargin ComputedLogicalPadding(
      mozilla::WritingMode aWM) const {
    return mComputedPadding.ConvertTo(aWM, mWritingMode);
  }
  mozilla::LogicalMargin ComputedLogicalBorder(mozilla::WritingMode aWM) const {
    return (mComputedBorderPadding - mComputedPadding)
        .ConvertTo(aWM, mWritingMode);
  }

  void SetComputedLogicalMargin(mozilla::WritingMode aWM,
                                const mozilla::LogicalMargin& aMargin) {
    mComputedMargin = aMargin.ConvertTo(mWritingMode, aWM);
  }
  void SetComputedLogicalBorderPadding(
      mozilla::WritingMode aWM, const mozilla::LogicalMargin& aBorderPadding) {
    mComputedBorderPadding = aBorderPadding.ConvertTo(mWritingMode, aWM);
  }
  void SetComputedLogicalPadding(mozilla::WritingMode aWM,
                                 const mozilla::LogicalMargin& aPadding) {
    mComputedPadding = aPadding.ConvertTo(mWritingMode, aWM);
  }

  mozilla::WritingMode GetWritingMode() const { return mWritingMode; }

 protected:
  // cached copy of the frame's writing-mode, for logical coordinates
  mozilla::WritingMode mWritingMode;

  // Computed margin values
  mozilla::LogicalMargin mComputedMargin;

  // Cached copy of the border + padding values
  mozilla::LogicalMargin mComputedBorderPadding;

  // Computed padding values
  mozilla::LogicalMargin mComputedPadding;

 public:
  // Callers using this constructor must call InitOffsets on their own.
  SizeComputationInput(nsIFrame* aFrame, gfxContext* aRenderingContext);

  SizeComputationInput(nsIFrame* aFrame, gfxContext* aRenderingContext,
                       mozilla::WritingMode aContainingBlockWritingMode,
                       nscoord aContainingBlockISize,
                       const mozilla::Maybe<mozilla::LogicalMargin>& aBorder =
                           mozilla::Nothing(),
                       const mozilla::Maybe<mozilla::LogicalMargin>& aPadding =
                           mozilla::Nothing());

#ifdef DEBUG
  // Reflow trace methods.  Defined in nsFrame.cpp so they have access
  // to the display-reflow infrastructure.
  static void* DisplayInitOffsetsEnter(nsIFrame* aFrame,
                                       SizeComputationInput* aState,
                                       nscoord aPercentBasis,
                                       mozilla::WritingMode aCBWritingMode,
                                       const nsMargin* aBorder,
                                       const nsMargin* aPadding);
  static void DisplayInitOffsetsExit(nsIFrame* aFrame,
                                     SizeComputationInput* aState,
                                     void* aValue);
#endif

 private:
  /**
   * Computes margin values from the specified margin style information, and
   * fills in the mComputedMargin member.
   *
   * @param aCBWM Writing mode of the containing block
   * @param aPercentBasis
   *    Inline size of the containing block (in its own writing mode), to use
   *    for resolving percentage margin values in the inline and block axes.
   * @return true if the margin is dependent on the containing block size.
   */
  bool ComputeMargin(mozilla::WritingMode aCBWM, nscoord aPercentBasis,
                     mozilla::LayoutFrameType aFrameType);

  /**
   * Computes padding values from the specified padding style information, and
   * fills in the mComputedPadding member.
   *
   * @param aCBWM Writing mode of the containing block
   * @param aPercentBasis
   *    Inline size of the containing block (in its own writing mode), to use
   *    for resolving percentage padding values in the inline and block axes.
   * @return true if the padding is dependent on the containing block size.
   */
  bool ComputePadding(mozilla::WritingMode aCBWM, nscoord aPercentBasis,
                      mozilla::LayoutFrameType aFrameType);

 protected:
  void InitOffsets(mozilla::WritingMode aCBWM, nscoord aPercentBasis,
                   mozilla::LayoutFrameType aFrameType,
                   mozilla::ComputeSizeFlags aFlags = {},
                   const mozilla::Maybe<mozilla::LogicalMargin>& aBorder =
                       mozilla::Nothing(),
                   const mozilla::Maybe<mozilla::LogicalMargin>& aPadding =
                       mozilla::Nothing(),
                   const nsStyleDisplay* aDisplay = nullptr);

  /*
   * Convert StyleSize or StyleMaxSize to nscoord when percentages depend on the
   * inline size of the containing block, and enumerated values are for inline
   * size, min-inline-size, or max-inline-size.  Does not handle auto inline
   * sizes.
   */
  template <typename SizeOrMaxSize>
  inline nscoord ComputeISizeValue(const WritingMode aWM,
                                   const LogicalSize& aContainingBlockSize,
                                   const LogicalSize& aContentEdgeToBoxSizing,
                                   nscoord aBoxSizingToMarginEdge,
                                   const SizeOrMaxSize&) const;
  // same as previous, but using mComputedBorderPadding, mComputedPadding,
  // and mComputedMargin
  template <typename SizeOrMaxSize>
  inline nscoord ComputeISizeValue(const LogicalSize& aContainingBlockSize,
                                   mozilla::StyleBoxSizing aBoxSizing,
                                   const SizeOrMaxSize&) const;

  nscoord ComputeBSizeValue(nscoord aContainingBlockBSize,
                            mozilla::StyleBoxSizing aBoxSizing,
                            const mozilla::LengthPercentage& aCoord) const;
};

/**
 * State passed to a frame during reflow or intrinsic size calculation.
 *
 * XXX Refactor so only a base class (nsSizingState?) is used for intrinsic
 * size calculation.
 *
 * @see nsIFrame#Reflow()
 */
struct ReflowInput : public SizeComputationInput {
  // the reflow inputs are linked together. this is the pointer to the
  // parent's reflow input
  const ReflowInput* mParentReflowInput = nullptr;

  // A non-owning pointer to the float manager associated with this area,
  // which points to the object owned by nsAutoFloatManager::mNew.
  nsFloatManager* mFloatManager = nullptr;

  // LineLayout object (only for inline reflow; set to nullptr otherwise)
  nsLineLayout* mLineLayout = nullptr;

  // The appropriate reflow input for the containing block (for
  // percentage widths, etc.) of this reflow input's frame. It will be setup
  // properly in InitCBReflowInput().
  const ReflowInput* mCBReflowInput = nullptr;

  // The amount the in-flow position of the block is moving vertically relative
  // to its previous in-flow position (i.e. the amount the line containing the
  // block is moving).
  // This should be zero for anything which is not a block outside, and it
  // should be zero for anything which has a non-block parent.
  // The intended use of this value is to allow the accurate determination
  // of the potential impact of a float
  // This takes on an arbitrary value the first time a block is reflowed
  nscoord mBlockDelta = 0;

  // If a ReflowInput finds itself initialized with an unconstrained
  // inline-size, it will look up its parentReflowInput chain for a reflow input
  // with an orthogonal writing mode and a non-NS_UNCONSTRAINEDSIZE value for
  // orthogonal limit; when it finds such a reflow input, it will use its
  // orthogonal-limit value to constrain inline-size.
  // This is initialized to NS_UNCONSTRAINEDSIZE (so it will be ignored),
  // but reset to a suitable value for the reflow root by PresShell.
  nscoord mOrthogonalLimit = NS_UNCONSTRAINEDSIZE;

  // Physical accessors for the private fields. They are needed for
  // compatibility with not-yet-updated code. New code should use the accessors
  // for logical coordinates, unless the code really works on physical
  // coordinates.
  nscoord AvailableWidth() const { return mAvailableSize.Width(mWritingMode); }
  nscoord AvailableHeight() const {
    return mAvailableSize.Height(mWritingMode);
  }
  nscoord ComputedWidth() const { return mComputedSize.Width(mWritingMode); }
  nscoord ComputedHeight() const { return mComputedSize.Height(mWritingMode); }
  nscoord ComputedMinWidth() const {
    return mComputedMinSize.Width(mWritingMode);
  }
  nscoord ComputedMaxWidth() const {
    return mComputedMaxSize.Width(mWritingMode);
  }
  nscoord ComputedMinHeight() const {
    return mComputedMinSize.Height(mWritingMode);
  }
  nscoord ComputedMaxHeight() const {
    return mComputedMaxSize.Height(mWritingMode);
  }

  // Logical accessors for private fields in mWritingMode.
  nscoord AvailableISize() const { return mAvailableSize.ISize(mWritingMode); }
  nscoord AvailableBSize() const { return mAvailableSize.BSize(mWritingMode); }
  nscoord ComputedISize() const { return mComputedSize.ISize(mWritingMode); }
  nscoord ComputedBSize() const { return mComputedSize.BSize(mWritingMode); }
  nscoord ComputedMinISize() const {
    return mComputedMinSize.ISize(mWritingMode);
  }
  nscoord ComputedMaxISize() const {
    return mComputedMaxSize.ISize(mWritingMode);
  }
  nscoord ComputedMinBSize() const {
    return mComputedMinSize.BSize(mWritingMode);
  }
  nscoord ComputedMaxBSize() const {
    return mComputedMaxSize.BSize(mWritingMode);
  }

  nscoord& AvailableISize() { return mAvailableSize.ISize(mWritingMode); }
  nscoord& AvailableBSize() { return mAvailableSize.BSize(mWritingMode); }
  nscoord& ComputedISize() { return mComputedSize.ISize(mWritingMode); }
  nscoord& ComputedBSize() { return mComputedSize.BSize(mWritingMode); }
  nscoord& ComputedMinISize() { return mComputedMinSize.ISize(mWritingMode); }
  nscoord& ComputedMaxISize() { return mComputedMaxSize.ISize(mWritingMode); }
  nscoord& ComputedMinBSize() { return mComputedMinSize.BSize(mWritingMode); }
  nscoord& ComputedMaxBSize() { return mComputedMaxSize.BSize(mWritingMode); }

  mozilla::LogicalSize AvailableSize() const { return mAvailableSize; }
  mozilla::LogicalSize ComputedSize() const { return mComputedSize; }
  mozilla::LogicalSize ComputedMinSize() const { return mComputedMinSize; }
  mozilla::LogicalSize ComputedMaxSize() const { return mComputedMaxSize; }

  mozilla::LogicalSize AvailableSize(mozilla::WritingMode aWM) const {
    return AvailableSize().ConvertTo(aWM, mWritingMode);
  }
  mozilla::LogicalSize ComputedSize(mozilla::WritingMode aWM) const {
    return ComputedSize().ConvertTo(aWM, mWritingMode);
  }
  mozilla::LogicalSize ComputedMinSize(mozilla::WritingMode aWM) const {
    return ComputedMinSize().ConvertTo(aWM, mWritingMode);
  }
  mozilla::LogicalSize ComputedMaxSize(mozilla::WritingMode aWM) const {
    return ComputedMaxSize().ConvertTo(aWM, mWritingMode);
  }

  mozilla::LogicalSize ComputedSizeWithPadding(mozilla::WritingMode aWM) const {
    return ComputedSize(aWM) + ComputedLogicalPadding(aWM).Size(aWM);
  }

  mozilla::LogicalSize ComputedSizeWithBorderPadding(
      mozilla::WritingMode aWM) const {
    return ComputedSize(aWM) + ComputedLogicalBorderPadding(aWM).Size(aWM);
  }

  mozilla::LogicalSize ComputedSizeWithMarginBorderPadding(
      mozilla::WritingMode aWM) const {
    return ComputedSizeWithBorderPadding(aWM) +
           ComputedLogicalMargin(aWM).Size(aWM);
  }

  nsSize ComputedPhysicalSize() const {
    return nsSize(ComputedWidth(), ComputedHeight());
  }

  nsMargin ComputedPhysicalOffsets() const {
    return mComputedOffsets.GetPhysicalMargin(mWritingMode);
  }

  LogicalMargin ComputedLogicalOffsets(mozilla::WritingMode aWM) const {
    return mComputedOffsets.ConvertTo(aWM, mWritingMode);
  }

  void SetComputedLogicalOffsets(mozilla::WritingMode aWM,
                                 const LogicalMargin& aOffsets) {
    mComputedOffsets = aOffsets.ConvertTo(mWritingMode, aWM);
  }

  // Return ReflowInput's computed size including border-padding, with
  // unconstrained dimensions replaced by zero.
  nsSize ComputedSizeAsContainerIfConstrained() const {
    const nscoord wd = ComputedWidth();
    const nscoord ht = ComputedHeight();
    return nsSize(wd == NS_UNCONSTRAINEDSIZE
                      ? 0
                      : wd + ComputedPhysicalBorderPadding().LeftRight(),
                  ht == NS_UNCONSTRAINEDSIZE
                      ? 0
                      : ht + ComputedPhysicalBorderPadding().TopBottom());
  }

  // Our saved containing block dimensions.
  LogicalSize mContainingBlockSize{mWritingMode};

  // Cached pointers to the various style structs used during initialization.
  const nsStyleDisplay* mStyleDisplay = nullptr;
  const nsStyleVisibility* mStyleVisibility = nullptr;
  const nsStylePosition* mStylePosition = nullptr;
  const nsStyleBorder* mStyleBorder = nullptr;
  const nsStyleMargin* mStyleMargin = nullptr;
  const nsStylePadding* mStylePadding = nullptr;
  const nsStyleText* mStyleText = nullptr;

  enum class BreakType : uint8_t {
    Auto,
    Column,
    Page,
  };
  BreakType mBreakType = BreakType::Auto;

  // a frame (e.g. nsTableCellFrame) which may need to generate a special
  // reflow for percent bsize calculations
  nsIPercentBSizeObserver* mPercentBSizeObserver = nullptr;

  // CSS margin collapsing sometimes requires us to reflow
  // optimistically assuming that margins collapse to see if clearance
  // is required. When we discover that clearance is required, we
  // store the frame in which clearance was discovered to the location
  // requested here.
  nsIFrame** mDiscoveredClearance = nullptr;

  struct Flags {
    Flags() { memset(this, 0, sizeof(*this)); }

    // cached mFrame->IsFrameOfType(nsIFrame::eReplaced) ||
    //        mFrame->IsFrameOfType(nsIFrame::eReplacedContainsBlock)
    bool mIsReplaced : 1;

    // used by tables to communicate special reflow (in process) to handle
    // percent bsize frames inside cells which may not have computed bsizes
    bool mSpecialBSizeReflow : 1;

    // nothing in the frame's next-in-flow (or its descendants) is changing
    bool mNextInFlowUntouched : 1;

    // Is the current context at the top of a page?  When true, we force
    // something that's too tall for a page/column to fit anyway to avoid
    // infinite loops.
    bool mIsTopOfPage : 1;

    // parent frame is an nsIScrollableFrame and it is assuming a horizontal
    // scrollbar
    bool mAssumingHScrollbar : 1;

    // parent frame is an nsIScrollableFrame and it is assuming a vertical
    // scrollbar
    bool mAssumingVScrollbar : 1;

    // Is frame a different inline-size than before?
    bool mIsIResize : 1;

    // Is frame (potentially) a different block-size than before?
    // This includes cases where the block-size is 'auto' and the
    // contents or width have changed.
    bool mIsBResize : 1;

    // Has this frame changed block-size in a way that affects
    // block-size percentages on frames for which it is the containing
    // block?  This includes a change between 'auto' and a length that
    // doesn't actually change the frame's block-size.  It does not
    // include cases where the block-size is 'auto' and the frame's
    // contents have changed.
    //
    // In the current code, this is only true when mIsBResize is also
    // true, although it doesn't necessarily need to be that way (e.g.,
    // in the case of a frame changing from 'auto' to a length that
    // produces the same height).
    bool mIsBResizeForPercentages : 1;

    // tables are splittable, this should happen only inside a page and never
    // insider a column frame
    bool mTableIsSplittable : 1;

    // Does frame height depend on an ancestor table-cell?
    bool mHeightDependsOnAncestorCell : 1;

    // nsColumnSetFrame is balancing columns
    bool mIsColumnBalancing : 1;

    // True if ColumnSetWrapperFrame has a constrained block-size, and is going
    // to consume all of its block-size in this fragment. This bit is passed to
    // nsColumnSetFrame to determine whether to give up balancing and create
    // overflow columns.
    bool mColumnSetWrapperHasNoBSizeLeft : 1;

    // If this flag is set, the BSize of this frame should be considered
    // indefinite for the purposes of percent resolution on child frames (we
    // should behave as if ComputedBSize() were NS_UNCONSTRAINEDSIZE when doing
    // percent resolution against this.ComputedBSize()).  For example: flex
    // items may have their ComputedBSize() resolved ahead-of-time by their
    // flex container, and yet their BSize might have to be considered
    // indefinite per https://drafts.csswg.org/css-flexbox/#definite-sizes
    bool mTreatBSizeAsIndefinite : 1;

    // a "fake" reflow input made in order to be the parent of a real one
    bool mDummyParentReflowInput : 1;

    // Should this frame reflow its place-holder children? If the available
    // height of this frame didn't change, but its in a paginated environment
    // (e.g. columns), it should always reflow its placeholder children.
    bool mMustReflowPlaceholders : 1;

    // the STATIC_POS_IS_CB_ORIGIN ctor flag
    bool mStaticPosIsCBOrigin : 1;

    // If set, the following two flags indicate that:
    // (1) this frame is absolutely-positioned (or fixed-positioned).
    // (2) this frame's static position depends on the CSS Box Alignment.
    // (3) we do need to compute the static position, because the frame's
    //     {Inline and/or Block} offsets actually depend on it.
    // When these bits are set, the offset values (IStart/IEnd, BStart/BEnd)
    // represent the "start" edge of the frame's CSS Box Alignment container
    // area, in that axis -- and these offsets need to be further-resolved
    // (with CSS Box Alignment) after we know the OOF frame's size.
    // NOTE: The "I" and "B" (for "Inline" and "Block") refer the axes of the
    // *containing block's writing-mode*, NOT mFrame's own writing-mode. This
    // is purely for convenience, since that's the writing-mode we're dealing
    // with when we set & react to these bits.
    bool mIOffsetsNeedCSSAlign : 1;
    bool mBOffsetsNeedCSSAlign : 1;

    // Are we somewhere inside an element with -webkit-line-clamp set?
    // This flag is inherited into descendant ReflowInputs, but we don't bother
    // resetting it to false when crossing over into a block descendant that
    // -webkit-line-clamp skips over (such as a BFC).
    bool mInsideLineClamp : 1;

    // Is this a flex item, and should we add or remove a -webkit-line-clamp
    // ellipsis on a descendant line?  It's possible for this flag to be true
    // when mInsideLineClamp is false if we previously had a numeric
    // -webkit-line-clamp value, but now have 'none' and we need to find the
    // line with the ellipsis flag and clear it.
    // This flag is not inherited into descendant ReflowInputs.
    bool mApplyLineClamp : 1;

    // Is this frame or one of its ancestors being reflowed in a different
    // continuation than the one in which it was previously reflowed?  In
    // other words, has it moved to a different column or page than it was in
    // the previous reflow?
    //
    // FIXME: For now, we only ensure that this is set correctly for blocks.
    // This is okay because the only thing that uses it only cares about
    // whether there's been a fragment change within the same block formatting
    // context.
    bool mMovedBlockFragments : 1;

    // Is the block-size computed by aspect-ratio and inline size (i.e. block
    // axis is the ratio-dependent axis)? We set this flag so that we can check
    // whether to apply automatic content-based minimum sizes once we know the
    // children's block-size (after reflowing them).
    // https://drafts.csswg.org/css-sizing-4/#aspect-ratio-minimum
    bool mIsBSizeSetByAspectRatio : 1;
  };
  Flags mFlags;

  mozilla::StyleSizeOverrides mStyleSizeOverrides;

  mozilla::ComputeSizeFlags mComputeSizeFlags;

  // This value keeps track of how deeply nested a given reflow input
  // is from the top of the frame tree.
  int16_t mReflowDepth = 0;

  // Logical and physical accessors for the resize flags.
  bool IsHResize() const {
    return mWritingMode.IsVertical() ? mFlags.mIsBResize : mFlags.mIsIResize;
  }
  bool IsVResize() const {
    return mWritingMode.IsVertical() ? mFlags.mIsIResize : mFlags.mIsBResize;
  }
  bool IsIResize() const { return mFlags.mIsIResize; }
  bool IsBResize() const { return mFlags.mIsBResize; }
  bool IsBResizeForWM(mozilla::WritingMode aWM) const {
    return aWM.IsOrthogonalTo(mWritingMode) ? mFlags.mIsIResize
                                            : mFlags.mIsBResize;
  }
  bool IsBResizeForPercentagesForWM(mozilla::WritingMode aWM) const {
    // This uses the relatively-accurate mIsBResizeForPercentages flag
    // when the writing modes are parallel, and is a bit more
    // pessimistic when orthogonal.
    return !aWM.IsOrthogonalTo(mWritingMode) ? mFlags.mIsBResizeForPercentages
                                             : IsIResize();
  }
  void SetHResize(bool aValue) {
    if (mWritingMode.IsVertical()) {
      mFlags.mIsBResize = aValue;
    } else {
      mFlags.mIsIResize = aValue;
    }
  }
  void SetVResize(bool aValue) {
    if (mWritingMode.IsVertical()) {
      mFlags.mIsIResize = aValue;
    } else {
      mFlags.mIsBResize = aValue;
    }
  }
  void SetIResize(bool aValue) { mFlags.mIsIResize = aValue; }
  void SetBResize(bool aValue) { mFlags.mIsBResize = aValue; }

  // Values for |aFlags| passed to constructor
  enum class InitFlag : uint8_t {
    // Indicates that the parent of this reflow input is "fake" (see
    // mDummyParentReflowInput in mFlags).
    DummyParentReflowInput,

    // Indicates that the calling function will initialize the reflow input, and
    // that the constructor should not call Init().
    CallerWillInit,

    // The caller wants the abs.pos. static-position resolved at the origin of
    // the containing block, i.e. at LogicalPoint(0, 0). (Note that this
    // doesn't necessarily mean that (0, 0) is the *correct* static position
    // for the frame in question.)
    // @note In a Grid container's masonry axis we'll always use
    // the placeholder's position in that axis regardless of this flag.
    StaticPosIsCBOrigin,
  };
  using InitFlags = mozilla::EnumSet<InitFlag>;

  // Note: The copy constructor is written by the compiler automatically. You
  // can use that and then override specific values if you want, or you can
  // call Init as desired...

  /**
   * Initialize a ROOT reflow input.
   *
   * @param aPresContext Must be equal to aFrame->PresContext().
   * @param aFrame The frame for whose reflow input is being constructed.
   * @param aRenderingContext The rendering context to be used for measurements.
   * @param aAvailableSpace The available space to reflow aFrame (in aFrame's
   *        writing-mode). See comments for mAvailableSize for more information.
   * @param aFlags A set of flags used for additional boolean parameters (see
   *        InitFlags above).
   */
  ReflowInput(nsPresContext* aPresContext, nsIFrame* aFrame,
              gfxContext* aRenderingContext,
              const mozilla::LogicalSize& aAvailableSpace,
              InitFlags aFlags = {});

  /**
   * Initialize a reflow input for a child frame's reflow. Some parts of the
   * state are copied from the parent's reflow input. The remainder is computed.
   *
   * @param aPresContext Must be equal to aFrame->PresContext().
   * @param aParentReflowInput A reference to an ReflowInput object that
   *        is to be the parent of this object.
   * @param aFrame The frame for whose reflow input is being constructed.
   * @param aAvailableSpace The available space to reflow aFrame (in aFrame's
   *        writing-mode). See comments for mAvailableSize for more information.
   * @param aContainingBlockSize An optional size (in aFrame's writing mode),
   *        specifying the containing block size to use instead of the default
   *        size computed by ComputeContainingBlockRectangle(). If
   *        InitFlag::CallerWillInit is used, this is ignored. Pass it via
   *        Init() instead.
   * @param aFlags A set of flags used for additional boolean parameters (see
   *        InitFlags above).
   * @param aStyleSizeOverrides The style data used to override mFrame's when we
   *        call nsIFrame::ComputeSize() internally.
   * @param aComputeSizeFlags A set of flags used when we call
   *        nsIFrame::ComputeSize() internally.
   */
  ReflowInput(nsPresContext* aPresContext,
              const ReflowInput& aParentReflowInput, nsIFrame* aFrame,
              const mozilla::LogicalSize& aAvailableSpace,
              const mozilla::Maybe<mozilla::LogicalSize>& aContainingBlockSize =
                  mozilla::Nothing(),
              InitFlags aFlags = {},
              const mozilla::StyleSizeOverrides& aSizeOverrides = {},
              mozilla::ComputeSizeFlags aComputeSizeFlags = {});

  /**
   * This method initializes various data members. It is automatically called by
   * the constructors if InitFlags::CallerWillInit is *not* used.
   *
   * @param aContainingBlockSize An optional size (in mFrame's writing mode),
   *        specifying the containing block size to use instead of the default
   *        size computed by ComputeContainingBlockRectangle().
   * @param aBorder An optional border (in mFrame's writing mode). If given, use
   *        it instead of the border computed from mFrame's StyleBorder.
   * @param aPadding An optional padding (in mFrame's writing mode). If given,
   *        use it instead of the padding computing from mFrame's StylePadding.
   */
  void Init(nsPresContext* aPresContext,
            const mozilla::Maybe<mozilla::LogicalSize>& aContainingBlockSize =
                mozilla::Nothing(),
            const mozilla::Maybe<mozilla::LogicalMargin>& aBorder =
                mozilla::Nothing(),
            const mozilla::Maybe<mozilla::LogicalMargin>& aPadding =
                mozilla::Nothing());

  /**
   * Get the used line-height property. The return value will be >= 0.
   */
  nscoord GetLineHeight() const;

  /**
   * Set the used line-height. aLineHeight must be >= 0.
   */
  void SetLineHeight(nscoord aLineHeight);

  /**
   * Calculate the used line-height property without a reflow input instance.
   * The return value will be >= 0.
   *
   * @param aBlockBSize The computed block size of the content rect of the block
   *                    that the line should fill. Only used with
   *                    line-height:-moz-block-height. NS_UNCONSTRAINEDSIZE
   *                    results in a normal line-height for
   *                    line-height:-moz-block-height.
   * @param aFontSizeInflation The result of the appropriate
   *                           nsLayoutUtils::FontSizeInflationFor call,
   *                           or 1.0 if during intrinsic size
   *                           calculation.
   */
  static nscoord CalcLineHeight(nsIContent* aContent,
                                ComputedStyle* aComputedStyle,
                                nsPresContext* aPresContext,
                                nscoord aBlockBSize, float aFontSizeInflation);

  mozilla::LogicalSize ComputeContainingBlockRectangle(
      nsPresContext* aPresContext, const ReflowInput* aContainingBlockRI) const;

  /**
   * Apply the mComputed(Min/Max)Width constraints to the content
   * size computed so far.
   */
  nscoord ApplyMinMaxWidth(nscoord aWidth) const {
    if (NS_UNCONSTRAINEDSIZE != ComputedMaxWidth()) {
      aWidth = std::min(aWidth, ComputedMaxWidth());
    }
    return std::max(aWidth, ComputedMinWidth());
  }

  /**
   * Apply the mComputed(Min/Max)ISize constraints to the content
   * size computed so far.
   */
  nscoord ApplyMinMaxISize(nscoord aISize) const {
    if (NS_UNCONSTRAINEDSIZE != ComputedMaxISize()) {
      aISize = std::min(aISize, ComputedMaxISize());
    }
    return std::max(aISize, ComputedMinISize());
  }

  /**
   * Apply the mComputed(Min/Max)Height constraints to the content
   * size computed so far.
   *
   * @param aHeight The height that we've computed an to which we want to apply
   *        min/max constraints.
   * @param aConsumed The amount of the computed height that was consumed by
   *        our prev-in-flows.
   */
  nscoord ApplyMinMaxHeight(nscoord aHeight, nscoord aConsumed = 0) const {
    aHeight += aConsumed;

    if (NS_UNCONSTRAINEDSIZE != ComputedMaxHeight()) {
      aHeight = std::min(aHeight, ComputedMaxHeight());
    }

    if (NS_UNCONSTRAINEDSIZE != ComputedMinHeight()) {
      aHeight = std::max(aHeight, ComputedMinHeight());
    }

    return aHeight - aConsumed;
  }

  /**
   * Apply the mComputed(Min/Max)BSize constraints to the content
   * size computed so far.
   *
   * @param aBSize The block-size that we've computed an to which we want to
   *        apply min/max constraints.
   * @param aConsumed The amount of the computed block-size that was consumed by
   *        our prev-in-flows.
   */
  nscoord ApplyMinMaxBSize(nscoord aBSize, nscoord aConsumed = 0) const {
    aBSize += aConsumed;

    if (NS_UNCONSTRAINEDSIZE != ComputedMaxBSize()) {
      aBSize = std::min(aBSize, ComputedMaxBSize());
    }

    if (NS_UNCONSTRAINEDSIZE != ComputedMinBSize()) {
      aBSize = std::max(aBSize, ComputedMinBSize());
    }

    return aBSize - aConsumed;
  }

  bool ShouldReflowAllKids() const;

  // This method doesn't apply min/max computed widths to the value passed in.
  void SetComputedWidth(nscoord aComputedWidth) {
    if (mWritingMode.IsVertical()) {
      SetComputedBSize(aComputedWidth);
    } else {
      SetComputedISize(aComputedWidth);
    }
  }

  // This method doesn't apply min/max computed heights to the value passed in.
  void SetComputedHeight(nscoord aComputedHeight) {
    if (mWritingMode.IsVertical()) {
      SetComputedISize(aComputedHeight);
    } else {
      SetComputedBSize(aComputedHeight);
    }
  }

  // This method doesn't apply min/max computed inline-sizes to the value passed
  // in.
  void SetComputedISize(nscoord aComputedISize);

  // These methods don't apply min/max computed block-sizes to the value passed
  // in.
  void SetComputedBSize(nscoord aComputedBSize);
  void SetComputedBSizeWithoutResettingResizeFlags(nscoord aComputedBSize) {
    // Viewport frames reset the computed block size on a copy of their reflow
    // input when reflowing fixed-pos kids.  In that case we actually don't
    // want to mess with the resize flags, because comparing the frame's rect
    // to the munged computed isize is pointless.
    MOZ_ASSERT(aComputedBSize >= 0, "Invalid computed block-size!");
    ComputedBSize() = aComputedBSize;
  }

  void SetTruncated(const ReflowOutput& aMetrics,
                    nsReflowStatus* aStatus) const;

  bool WillReflowAgainForClearance() const {
    return mDiscoveredClearance && *mDiscoveredClearance;
  }

  // Compute the offsets for a relative position element
  //
  // @param aWM the writing mode of aCBSize and the returned offsets.
  static mozilla::LogicalMargin ComputeRelativeOffsets(
      mozilla::WritingMode aWM, nsIFrame* aFrame,
      const mozilla::LogicalSize& aCBSize);

  // If a relatively positioned element, adjust the position appropriately.
  static void ApplyRelativePositioning(nsIFrame* aFrame,
                                       const nsMargin& aComputedOffsets,
                                       nsPoint* aPosition);

  void ApplyRelativePositioning(nsPoint* aPosition) const {
    ApplyRelativePositioning(mFrame, ComputedPhysicalOffsets(), aPosition);
  }

  static void ApplyRelativePositioning(
      nsIFrame* aFrame, mozilla::WritingMode aWritingMode,
      const mozilla::LogicalMargin& aComputedOffsets,
      mozilla::LogicalPoint* aPosition, const nsSize& aContainerSize);

  void ApplyRelativePositioning(mozilla::LogicalPoint* aPosition,
                                const nsSize& aContainerSize) const {
    ApplyRelativePositioning(mFrame, mWritingMode,
                             ComputedLogicalOffsets(mWritingMode), aPosition,
                             aContainerSize);
  }

  // Resolve any block-axis 'auto' margins (if any) for an absolutely positioned
  // frame. aMargin and aOffsets are both outparams (though we only touch
  // aOffsets if the position is overconstrained)
  static void ComputeAbsPosBlockAutoMargin(nscoord aAvailMarginSpace,
                                           WritingMode aContainingBlockWM,
                                           bool aIsMarginBStartAuto,
                                           bool aIsMarginBEndAuto,
                                           LogicalMargin& aMargin,
                                           LogicalMargin& aOffsets);

  // Resolve any inline-axis 'auto' margins (if any) for an absolutely
  // positioned frame. aMargin and aOffsets are both outparams (though we only
  // touch aOffsets if the position is overconstrained)
  static void ComputeAbsPosInlineAutoMargin(nscoord aAvailMarginSpace,
                                            WritingMode aContainingBlockWM,
                                            bool aIsMarginIStartAuto,
                                            bool aIsMarginIEndAuto,
                                            LogicalMargin& aMargin,
                                            LogicalMargin& aOffsets);

#ifdef DEBUG
  // Reflow trace methods.  Defined in nsFrame.cpp so they have access
  // to the display-reflow infrastructure.
  static void* DisplayInitConstraintsEnter(nsIFrame* aFrame,
                                           ReflowInput* aState,
                                           nscoord aCBISize, nscoord aCBBSize,
                                           const nsMargin* aBorder,
                                           const nsMargin* aPadding);
  static void DisplayInitConstraintsExit(nsIFrame* aFrame, ReflowInput* aState,
                                         void* aValue);
  static void* DisplayInitFrameTypeEnter(nsIFrame* aFrame, ReflowInput* aState);
  static void DisplayInitFrameTypeExit(nsIFrame* aFrame, ReflowInput* aState,
                                       void* aValue);
#endif

 protected:
  void InitCBReflowInput();
  void InitResizeFlags(nsPresContext* aPresContext,
                       mozilla::LayoutFrameType aFrameType);
  void InitDynamicReflowRoot();

  void InitConstraints(
      nsPresContext* aPresContext,
      const mozilla::Maybe<mozilla::LogicalSize>& aContainingBlockSize,
      const mozilla::Maybe<mozilla::LogicalMargin>& aBorder,
      const mozilla::Maybe<mozilla::LogicalMargin>& aPadding,
      mozilla::LayoutFrameType aFrameType);

  // Returns the nearest containing block or block frame (whether or not
  // it is a containing block) for the specified frame.  Also returns
  // the inline-start edge and logical size of the containing block's
  // content area.
  // These are returned in the coordinate space of the containing block.
  nsIFrame* GetHypotheticalBoxContainer(nsIFrame* aFrame,
                                        nscoord& aCBIStartEdge,
                                        mozilla::LogicalSize& aCBSize) const;

  // Calculate a "hypothetical box" position where the placeholder frame
  // (for a position:fixed/absolute element) would have been placed if it were
  // positioned statically. The hypothetical box position will have a writing
  // mode with the same block direction as the absolute containing block
  // (aCBReflowInput->frame), though it may differ in inline direction.
  void CalculateHypotheticalPosition(nsPresContext* aPresContext,
                                     nsPlaceholderFrame* aPlaceholderFrame,
                                     const ReflowInput* aCBReflowInput,
                                     nsHypotheticalPosition& aHypotheticalPos,
                                     mozilla::LayoutFrameType aFrameType) const;

  // Check if we can use the resolved auto block size (by insets) to compute
  // the inline size through aspect-ratio on absolute-positioned elements.
  // This is only needed for non-replaced elements.
  // https://drafts.csswg.org/css-position/#abspos-auto-size
  bool IsInlineSizeComputableByBlockSizeAndAspectRatio(
      nscoord aBlockSize) const;

  // This calculates the size by using the resolved auto block size (from
  // non-auto block insets), according to the writing mode of current block.
  LogicalSize CalculateAbsoluteSizeWithResolvedAutoBlockSize(
      nscoord aAutoBSize, const LogicalSize& aTentativeComputedSize);

  void InitAbsoluteConstraints(nsPresContext* aPresContext,
                               const ReflowInput* aCBReflowInput,
                               const mozilla::LogicalSize& aContainingBlockSize,
                               mozilla::LayoutFrameType aFrameType);

  // Calculates the computed values for the 'min-inline-size',
  // 'max-inline-size', 'min-block-size', and 'max-block-size' properties, and
  // stores them in the assorted data members
  void ComputeMinMaxValues(const mozilla::LogicalSize& aCBSize);

  // aInsideBoxSizing returns the part of the padding, border, and margin
  // in the aAxis dimension that goes inside the edge given by box-sizing;
  // aOutsideBoxSizing returns the rest.
  void CalculateBorderPaddingMargin(mozilla::LogicalAxis aAxis,
                                    nscoord aContainingBlockSize,
                                    nscoord* aInsideBoxSizing,
                                    nscoord* aOutsideBoxSizing) const;

  void CalculateBlockSideMargins();

  /**
   * @return true if mFrame is an internal table frame, i.e. an
   * ns[RowGroup|ColGroup|Row|Cell]Frame.  (We exclude nsTableColFrame
   * here since we never setup a ReflowInput for those.)
   */
  bool IsInternalTableFrame() const;

 private:
  // The available size in which to reflow the frame. The space represents the
  // amount of room for the frame's margin, border, padding, and content area.
  //
  // The available inline-size should be constrained. The frame's inline-size
  // you choose should fit within it.

  // In galley mode, the available block-size is always unconstrained, and only
  // page mode or multi-column layout involves a constrained available
  // block-size.
  //
  // An unconstrained available block-size means you can choose whatever size
  // you want. If the value is constrained, the frame's block-start border,
  // padding, and content, must fit. If a frame is fully-complete after reflow,
  // then its block-end border, padding, and margin (and similar for its
  // fully-complete ancestors) will need to fit within this available
  // block-size. However, if a frame is monolithic, it may choose a block-size
  // larger than the available block-size.
  mozilla::LogicalSize mAvailableSize{mWritingMode};

  // The computed size specifies the frame's content area, and it does not apply
  // to inline non-replaced elements.
  //
  // For block-level frames, the computed inline-size is based on the
  // inline-size of the containing block, the margin/border/padding areas, and
  // the min/max inline-size.
  //
  // For non-replaced block-level frames in the flow and floated, if the
  // computed block-size is NS_UNCONSTRAINEDSIZE, you should choose a block-size
  // to shrink wrap around the normal flow child frames. The block-size must be
  // within the limit of the min/max block-size if there is such a limit.
  mozilla::LogicalSize mComputedSize{mWritingMode};

  // Computed values for 'inset' properties. Only applies to 'positioned'
  // elements.
  mozilla::LogicalMargin mComputedOffsets{mWritingMode};

  // Computed value for 'min-inline-size'/'min-block-size'.
  mozilla::LogicalSize mComputedMinSize{mWritingMode};

  // Computed value for 'max-inline-size'/'max-block-size'.
  mozilla::LogicalSize mComputedMaxSize{mWritingMode, NS_UNCONSTRAINEDSIZE,
                                        NS_UNCONSTRAINEDSIZE};

  // Cache the used line-height property.
  mutable nscoord mLineHeight = NS_UNCONSTRAINEDSIZE;
};

}  // namespace mozilla

#endif  // mozilla_ReflowInput_h
