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
#include "nsIFrame.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include <algorithm>

class gfxContext;
class nsFloatManager;
struct nsHypotheticalPosition;
class nsIPercentBSizeObserver;
class nsLineLayout;
class nsPlaceholderFrame;
class nsPresContext;

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

/**
 * CSS Frame type. Included as part of the reflow input.
 */
typedef uint32_t nsCSSFrameType;

#define NS_CSS_FRAME_TYPE_UNKNOWN 0
#define NS_CSS_FRAME_TYPE_INLINE 1
#define NS_CSS_FRAME_TYPE_BLOCK 2 /* block-level in normal flow */
#define NS_CSS_FRAME_TYPE_FLOATING 3
#define NS_CSS_FRAME_TYPE_ABSOLUTE 4
#define NS_CSS_FRAME_TYPE_INTERNAL_TABLE \
  5 /* row group frame, row frame, cell frame, ... */

/**
 * Bit-flag that indicates whether the element is replaced. Applies to inline,
 * block-level, floating, and absolutely positioned elements
 */
#define NS_CSS_FRAME_TYPE_REPLACED 0x08000

/**
 * Bit-flag that indicates that the element is replaced and contains a block
 * (eg some form controls).  Applies to inline, block-level, floating, and
 * absolutely positioned elements.  Mutually exclusive with
 * NS_CSS_FRAME_TYPE_REPLACED.
 */
#define NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK 0x10000

/**
 * Helper macros for telling whether items are replaced
 */
#define NS_FRAME_IS_REPLACED_NOBLOCK(_ft) \
  (NS_CSS_FRAME_TYPE_REPLACED == ((_ft)&NS_CSS_FRAME_TYPE_REPLACED))

#define NS_FRAME_IS_REPLACED(_ft)       \
  (NS_FRAME_IS_REPLACED_NOBLOCK(_ft) || \
   NS_FRAME_IS_REPLACED_CONTAINS_BLOCK(_ft))

#define NS_FRAME_REPLACED(_ft) (NS_CSS_FRAME_TYPE_REPLACED | (_ft))

#define NS_FRAME_IS_REPLACED_CONTAINS_BLOCK(_ft) \
  (NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK ==  \
   ((_ft)&NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK))

#define NS_FRAME_REPLACED_CONTAINS_BLOCK(_ft) \
  (NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK | (_ft))

/**
 * A macro to extract the type. Masks off the 'replaced' bit-flag
 */
#define NS_FRAME_GET_TYPE(_ft) \
  ((_ft) &                     \
   ~(NS_CSS_FRAME_TYPE_REPLACED | NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK))

namespace mozilla {

// A base class of ReflowInput that computes only the padding,
// border, and margin, since those values are needed more often.
struct SizeComputationInput {
 public:
  typedef mozilla::WritingMode WritingMode;
  typedef mozilla::LogicalMargin LogicalMargin;

  // The frame being reflowed.
  nsIFrame* mFrame;

  // Rendering context to use for measurement.
  gfxContext* mRenderingContext;

  const nsMargin& ComputedPhysicalMargin() const { return mComputedMargin; }
  const nsMargin& ComputedPhysicalBorderPadding() const {
    return mComputedBorderPadding;
  }
  const nsMargin& ComputedPhysicalPadding() const { return mComputedPadding; }

  // We may need to eliminate the (few) users of these writable-reference
  // accessors as part of migrating to logical coordinates.
  nsMargin& ComputedPhysicalMargin() { return mComputedMargin; }
  nsMargin& ComputedPhysicalBorderPadding() { return mComputedBorderPadding; }
  nsMargin& ComputedPhysicalPadding() { return mComputedPadding; }

  LogicalMargin ComputedLogicalMargin() const {
    return LogicalMargin(mWritingMode, mComputedMargin);
  }
  LogicalMargin ComputedLogicalBorderPadding() const {
    return LogicalMargin(mWritingMode, mComputedBorderPadding);
  }
  LogicalMargin ComputedLogicalPadding() const {
    return LogicalMargin(mWritingMode, mComputedPadding);
  }

  void SetComputedLogicalMargin(mozilla::WritingMode aWM,
                                const LogicalMargin& aMargin) {
    mComputedMargin = aMargin.GetPhysicalMargin(aWM);
  }
  void SetComputedLogicalMargin(const LogicalMargin& aMargin) {
    SetComputedLogicalMargin(mWritingMode, aMargin);
  }

  void SetComputedLogicalBorderPadding(mozilla::WritingMode aWM,
                                       const LogicalMargin& aMargin) {
    mComputedBorderPadding = aMargin.GetPhysicalMargin(aWM);
  }
  void SetComputedLogicalBorderPadding(const LogicalMargin& aMargin) {
    SetComputedLogicalBorderPadding(mWritingMode, aMargin);
  }

  void SetComputedLogicalPadding(mozilla::WritingMode aWM,
                                 const LogicalMargin& aMargin) {
    mComputedPadding = aMargin.GetPhysicalMargin(aWM);
  }
  void SetComputedLogicalPadding(const LogicalMargin& aMargin) {
    SetComputedLogicalPadding(mWritingMode, aMargin);
  }

  WritingMode GetWritingMode() const { return mWritingMode; }

 protected:
  // cached copy of the frame's writing-mode, for logical coordinates
  WritingMode mWritingMode;

  // These are PHYSICAL coordinates (for now).
  // Will probably become logical in due course.

  // Computed margin values
  nsMargin mComputedMargin;

  // Cached copy of the border + padding values
  nsMargin mComputedBorderPadding;

  // Computed padding values
  nsMargin mComputedPadding;

 public:
  // Callers using this constructor must call InitOffsets on their own.
  SizeComputationInput(nsIFrame* aFrame, gfxContext* aRenderingContext)
      : mFrame(aFrame),
        mRenderingContext(aRenderingContext),
        mWritingMode(aFrame->GetWritingMode()) {}

  SizeComputationInput(nsIFrame* aFrame, gfxContext* aRenderingContext,
                       mozilla::WritingMode aContainingBlockWritingMode,
                       nscoord aContainingBlockISize);

  struct ReflowInputFlags {
    ReflowInputFlags() { memset(this, 0, sizeof(*this)); }

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

    // nsFlexContainerFrame is reflowing this child to measure its intrinsic
    // BSize.
    bool mIsFlexContainerMeasuringBSize : 1;

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

    // stores the COMPUTE_SIZE_SHRINK_WRAP ctor flag
    bool mShrinkWrap : 1;

    // stores the COMPUTE_SIZE_USE_AUTO_BSIZE ctor flag
    bool mUseAutoBSize : 1;

    // the STATIC_POS_IS_CB_ORIGIN ctor flag
    bool mStaticPosIsCBOrigin : 1;

    // the I_CLAMP_MARGIN_BOX_MIN_SIZE ctor flag
    bool mIClampMarginBoxMinSize : 1;

    // the B_CLAMP_MARGIN_BOX_MIN_SIZE ctor flag
    bool mBClampMarginBoxMinSize : 1;

    // the I_APPLY_AUTO_MIN_SIZE ctor flag
    bool mApplyAutoMinSize : 1;

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
  };

#ifdef DEBUG
  // Reflow trace methods.  Defined in nsFrame.cpp so they have access
  // to the display-reflow infrastructure.
  static void* DisplayInitOffsetsEnter(nsIFrame* aFrame,
                                       SizeComputationInput* aState,
                                       nscoord aPercentBasis,
                                       WritingMode aCBWritingMode,
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
   * @param aWM Writing mode of the containing block
   * @param aPercentBasis
   *    Inline size of the containing block (in its own writing mode), to use
   *    for resolving percentage margin values in the inline and block axes.
   * @return true if the margin is dependent on the containing block size.
   */
  bool ComputeMargin(mozilla::WritingMode aWM, nscoord aPercentBasis);

  /**
   * Computes padding values from the specified padding style information, and
   * fills in the mComputedPadding member.
   *
   * @param aWM Writing mode of the containing block
   * @param aPercentBasis
   *    Inline size of the containing block (in its own writing mode), to use
   *    for resolving percentage padding values in the inline and block axes.
   * @return true if the padding is dependent on the containing block size.
   */
  bool ComputePadding(mozilla::WritingMode aWM, nscoord aPercentBasis,
                      mozilla::LayoutFrameType aFrameType);

 protected:
  void InitOffsets(mozilla::WritingMode aWM, nscoord aPercentBasis,
                   mozilla::LayoutFrameType aFrameType, ReflowInputFlags aFlags,
                   const nsMargin* aBorder = nullptr,
                   const nsMargin* aPadding = nullptr,
                   const nsStyleDisplay* aDisplay = nullptr);

  /*
   * Convert StyleSize or StyleMaxSize to nscoord when percentages depend on the
   * inline size of the containing block, and enumerated values are for inline
   * size, min-inline-size, or max-inline-size.  Does not handle auto inline
   * sizes.
   */
  template <typename SizeOrMaxSize>
  inline nscoord ComputeISizeValue(nscoord aContainingBlockISize,
                                   nscoord aContentEdgeToBoxSizing,
                                   nscoord aBoxSizingToMarginEdge,
                                   const SizeOrMaxSize&) const;
  // same as previous, but using mComputedBorderPadding, mComputedPadding,
  // and mComputedMargin
  template <typename SizeOrMaxSize>
  inline nscoord ComputeISizeValue(nscoord aContainingBlockISize,
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

  // The type of frame, from css's perspective. This value is
  // initialized by the Init method below.
  nsCSSFrameType mFrameType = NS_CSS_FRAME_TYPE_UNKNOWN;

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

  // Accessors for the private fields below. Forcing all callers to use these
  // will allow us to introduce logical-coordinate versions and gradually
  // change clients from physical to logical as needed; and potentially switch
  // the internal fields from physical to logical coordinates in due course,
  // while maintaining compatibility with not-yet-updated code.
  nscoord AvailableWidth() const { return mAvailableWidth; }
  nscoord AvailableHeight() const { return mAvailableHeight; }
  nscoord ComputedWidth() const { return mComputedWidth; }
  nscoord ComputedHeight() const { return mComputedHeight; }
  nscoord ComputedMinWidth() const { return mComputedMinWidth; }
  nscoord ComputedMaxWidth() const { return mComputedMaxWidth; }
  nscoord ComputedMinHeight() const { return mComputedMinHeight; }
  nscoord ComputedMaxHeight() const { return mComputedMaxHeight; }

  nscoord& AvailableWidth() { return mAvailableWidth; }
  nscoord& AvailableHeight() { return mAvailableHeight; }
  nscoord& ComputedWidth() { return mComputedWidth; }
  nscoord& ComputedHeight() { return mComputedHeight; }
  nscoord& ComputedMinWidth() { return mComputedMinWidth; }
  nscoord& ComputedMaxWidth() { return mComputedMaxWidth; }
  nscoord& ComputedMinHeight() { return mComputedMinHeight; }
  nscoord& ComputedMaxHeight() { return mComputedMaxHeight; }

  // ISize and BSize are logical-coordinate dimensions:
  // ISize is the size in the writing mode's inline direction (which equates to
  // width in horizontal writing modes, height in vertical ones), and BSize is
  // the size in the block-progression direction.
  nscoord AvailableISize() const {
    return mWritingMode.IsVertical() ? mAvailableHeight : mAvailableWidth;
  }
  nscoord AvailableBSize() const {
    return mWritingMode.IsVertical() ? mAvailableWidth : mAvailableHeight;
  }
  nscoord ComputedISize() const {
    return mWritingMode.IsVertical() ? mComputedHeight : mComputedWidth;
  }
  nscoord ComputedBSize() const {
    return mWritingMode.IsVertical() ? mComputedWidth : mComputedHeight;
  }
  nscoord ComputedMinISize() const {
    return mWritingMode.IsVertical() ? mComputedMinHeight : mComputedMinWidth;
  }
  nscoord ComputedMaxISize() const {
    return mWritingMode.IsVertical() ? mComputedMaxHeight : mComputedMaxWidth;
  }
  nscoord ComputedMinBSize() const {
    return mWritingMode.IsVertical() ? mComputedMinWidth : mComputedMinHeight;
  }
  nscoord ComputedMaxBSize() const {
    return mWritingMode.IsVertical() ? mComputedMaxWidth : mComputedMaxHeight;
  }

  nscoord& AvailableISize() {
    return mWritingMode.IsVertical() ? mAvailableHeight : mAvailableWidth;
  }
  nscoord& AvailableBSize() {
    return mWritingMode.IsVertical() ? mAvailableWidth : mAvailableHeight;
  }
  nscoord& ComputedISize() {
    return mWritingMode.IsVertical() ? mComputedHeight : mComputedWidth;
  }
  nscoord& ComputedBSize() {
    return mWritingMode.IsVertical() ? mComputedWidth : mComputedHeight;
  }
  nscoord& ComputedMinISize() {
    return mWritingMode.IsVertical() ? mComputedMinHeight : mComputedMinWidth;
  }
  nscoord& ComputedMaxISize() {
    return mWritingMode.IsVertical() ? mComputedMaxHeight : mComputedMaxWidth;
  }
  nscoord& ComputedMinBSize() {
    return mWritingMode.IsVertical() ? mComputedMinWidth : mComputedMinHeight;
  }
  nscoord& ComputedMaxBSize() {
    return mWritingMode.IsVertical() ? mComputedMaxWidth : mComputedMaxHeight;
  }

  mozilla::LogicalSize AvailableSize() const {
    return mozilla::LogicalSize(mWritingMode, AvailableISize(),
                                AvailableBSize());
  }
  mozilla::LogicalSize ComputedSize() const {
    return mozilla::LogicalSize(mWritingMode, ComputedISize(), ComputedBSize());
  }
  mozilla::LogicalSize ComputedMinSize() const {
    return mozilla::LogicalSize(mWritingMode, ComputedMinISize(),
                                ComputedMinBSize());
  }
  mozilla::LogicalSize ComputedMaxSize() const {
    return mozilla::LogicalSize(mWritingMode, ComputedMaxISize(),
                                ComputedMaxBSize());
  }

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

  mozilla::LogicalSize ComputedSizeWithPadding() const {
    mozilla::WritingMode wm = GetWritingMode();
    return mozilla::LogicalSize(
        wm, ComputedISize() + ComputedLogicalPadding().IStartEnd(wm),
        ComputedBSize() + ComputedLogicalPadding().BStartEnd(wm));
  }

  mozilla::LogicalSize ComputedSizeWithPadding(mozilla::WritingMode aWM) const {
    return ComputedSizeWithPadding().ConvertTo(aWM, GetWritingMode());
  }

  mozilla::LogicalSize ComputedSizeWithBorderPadding() const {
    mozilla::WritingMode wm = GetWritingMode();
    return mozilla::LogicalSize(
        wm, ComputedISize() + ComputedLogicalBorderPadding().IStartEnd(wm),
        ComputedBSize() + ComputedLogicalBorderPadding().BStartEnd(wm));
  }

  mozilla::LogicalSize ComputedSizeWithBorderPadding(
      mozilla::WritingMode aWM) const {
    return ComputedSizeWithBorderPadding().ConvertTo(aWM, GetWritingMode());
  }

  mozilla::LogicalSize ComputedSizeWithMarginBorderPadding() const {
    mozilla::WritingMode wm = GetWritingMode();
    return mozilla::LogicalSize(
        wm,
        ComputedISize() + ComputedLogicalMargin().IStartEnd(wm) +
            ComputedLogicalBorderPadding().IStartEnd(wm),
        ComputedBSize() + ComputedLogicalMargin().BStartEnd(wm) +
            ComputedLogicalBorderPadding().BStartEnd(wm));
  }

  mozilla::LogicalSize ComputedSizeWithMarginBorderPadding(
      mozilla::WritingMode aWM) const {
    return ComputedSizeWithMarginBorderPadding().ConvertTo(aWM,
                                                           GetWritingMode());
  }

  nsSize ComputedPhysicalSize() const {
    return nsSize(ComputedWidth(), ComputedHeight());
  }

  // XXX this will need to change when we make mComputedOffsets logical;
  // we won't be able to return a reference for the physical offsets
  const nsMargin& ComputedPhysicalOffsets() const { return mComputedOffsets; }
  nsMargin& ComputedPhysicalOffsets() { return mComputedOffsets; }

  const LogicalMargin ComputedLogicalOffsets() const {
    return LogicalMargin(mWritingMode, mComputedOffsets);
  }

  void SetComputedLogicalOffsets(const LogicalMargin& aOffsets) {
    mComputedOffsets = aOffsets.GetPhysicalMargin(mWritingMode);
  }

  // Return the state's computed size including border-padding, with
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

 private:
  // the available width in which to reflow the frame. The space
  // represents the amount of room for the frame's margin, border,
  // padding, and content area. The frame size you choose should fit
  // within the available width.
  nscoord mAvailableWidth = 0;

  // A value of NS_UNCONSTRAINEDSIZE for the available height means
  // you can choose whatever size you want. In galley mode the
  // available height is always NS_UNCONSTRAINEDSIZE, and only page
  // mode or multi-column layout involves a constrained height. The
  // element's the top border and padding, and content, must fit. If the
  // element is complete after reflow then its bottom border, padding
  // and margin (and similar for its complete ancestors) will need to
  // fit in this height.
  nscoord mAvailableHeight = 0;

  // The computed width specifies the frame's content area width, and it does
  // not apply to inline non-replaced elements
  //
  // For replaced inline frames, a value of NS_UNCONSTRAINEDSIZE means you
  // should use your intrinsic width as the computed width
  //
  // For block-level frames, the computed width is based on the width of the
  // containing block, the margin/border/padding areas, and the min/max width.
  MOZ_INIT_OUTSIDE_CTOR
  nscoord mComputedWidth;

  // The computed height specifies the frame's content height, and it does
  // not apply to inline non-replaced elements
  //
  // For replaced inline frames, a value of NS_UNCONSTRAINEDSIZE means you
  // should use your intrinsic height as the computed height
  //
  // For non-replaced block-level frames in the flow and floated, a value of
  // NS_UNCONSTRAINEDSIZE means you choose a height to shrink wrap around the
  // normal flow child frames. The height must be within the limit of the
  // min/max height if there is such a limit
  //
  // For replaced block-level frames, a value of NS_UNCONSTRAINEDSIZE
  // means you use your intrinsic height as the computed height
  MOZ_INIT_OUTSIDE_CTOR
  nscoord mComputedHeight;

  // Computed values for 'left/top/right/bottom' offsets. Only applies to
  // 'positioned' elements. These are PHYSICAL coordinates (for now).
  nsMargin mComputedOffsets;

  // Computed values for 'min-width/max-width' and 'min-height/max-height'
  // XXXldb The width ones here should go; they should be needed only
  // internally, except for nsComboboxDisplayFrame, which still wants to honor
  // min-inline-size even though it wants to trump inline-size.
  MOZ_INIT_OUTSIDE_CTOR
  nscoord mComputedMinWidth, mComputedMaxWidth;
  MOZ_INIT_OUTSIDE_CTOR
  nscoord mComputedMinHeight, mComputedMaxHeight;

 public:
  // Our saved containing block dimensions.
  LogicalSize mContainingBlockSize = LogicalSize(mWritingMode);

  // Cached pointers to the various style structs used during initialization.
  const nsStyleDisplay* mStyleDisplay = nullptr;
  const nsStyleVisibility* mStyleVisibility = nullptr;
  const nsStylePosition* mStylePosition = nullptr;
  const nsStyleBorder* mStyleBorder = nullptr;
  const nsStyleMargin* mStyleMargin = nullptr;
  const nsStylePadding* mStylePadding = nullptr;
  const nsStyleText* mStyleText = nullptr;

  bool IsFloating() const;

  mozilla::StyleDisplay GetDisplay() const;

  // a frame (e.g. nsTableCellFrame) which may need to generate a special
  // reflow for percent bsize calculations
  nsIPercentBSizeObserver* mPercentBSizeObserver = nullptr;

  // CSS margin collapsing sometimes requires us to reflow
  // optimistically assuming that margins collapse to see if clearance
  // is required. When we discover that clearance is required, we
  // store the frame in which clearance was discovered to the location
  // requested here.
  nsIFrame** mDiscoveredClearance = nullptr;

  ReflowInputFlags mFlags;

  // This value keeps track of how deeply nested a given reflow input
  // is from the top of the frame tree.
  int16_t mReflowDepth = 0;

  // Logical and physical accessors for the resize flags. All users should go
  // via these accessors, so that in due course we can change the storage from
  // physical to logical.
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

  // Note: The copy constructor is written by the compiler automatically. You
  // can use that and then override specific values if you want, or you can
  // call Init as desired...

  /**
   * Initialize a ROOT reflow input.
   *
   * @param aPresContext Must be equal to aFrame->PresContext().
   * @param aFrame The frame for whose reflow input is being constructed.
   * @param aRenderingContext The rendering context to be used for measurements.
   * @param aAvailableSpace See comments for availableHeight and availableWidth
   *        members.
   * @param aFlags A set of flags used for additional boolean parameters (see
   *        below).
   */
  ReflowInput(nsPresContext* aPresContext, nsIFrame* aFrame,
              gfxContext* aRenderingContext,
              const mozilla::LogicalSize& aAvailableSpace, uint32_t aFlags = 0);

  /**
   * Initialize a reflow input for a child frame's reflow. Some parts of the
   * state are copied from the parent's reflow input. The remainder is computed.
   *
   * @param aPresContext Must be equal to aFrame->PresContext().
   * @param aParentReflowInput A reference to an ReflowInput object that
   *        is to be the parent of this object.
   * @param aFrame The frame for whose reflow input is being constructed.
   * @param aAvailableSpace See comments for availableHeight and availableWidth
   *        members.
   * @param aContainingBlockSize An optional size, in app units, specifying
   *        the containing block size to use instead of the default which is
   *        computed by ComputeContainingBlockRectangle().
   * @param aFlags A set of flags used for additional boolean parameters (see
   *        below).
   */
  ReflowInput(nsPresContext* aPresContext,
              const ReflowInput& aParentReflowInput, nsIFrame* aFrame,
              const mozilla::LogicalSize& aAvailableSpace,
              const mozilla::Maybe<mozilla::LogicalSize>& aContainingBlockSize =
                  mozilla::Nothing(),
              uint32_t aFlags = 0);

  // Values for |aFlags| passed to constructor
  enum {
    // Indicates that the parent of this reflow input is "fake" (see
    // mDummyParentReflowInput in mFlags).
    DUMMY_PARENT_REFLOW_INPUT = (1 << 0),

    // Indicates that the calling function will initialize the reflow input, and
    // that the constructor should not call Init().
    CALLER_WILL_INIT = (1 << 1),

    // The caller wants shrink-wrap behavior (i.e. ComputeSizeFlags::eShrinkWrap
    // will be passed to ComputeSize()).
    COMPUTE_SIZE_SHRINK_WRAP = (1 << 2),

    // The caller wants 'auto' bsize behavior (ComputeSizeFlags::eUseAutoBSize
    // will be be passed to ComputeSize()).
    COMPUTE_SIZE_USE_AUTO_BSIZE = (1 << 3),

    // The caller wants the abs.pos. static-position resolved at the origin of
    // the containing block, i.e. at LogicalPoint(0, 0). (Note that this
    // doesn't necessarily mean that (0, 0) is the *correct* static position
    // for the frame in question.)
    STATIC_POS_IS_CB_ORIGIN = (1 << 4),

    // Pass ComputeSizeFlags::eIClampMarginBoxMinSize to ComputeSize().
    I_CLAMP_MARGIN_BOX_MIN_SIZE = (1 << 5),

    // Pass ComputeSizeFlags::eBClampMarginBoxMinSize to ComputeSize().
    B_CLAMP_MARGIN_BOX_MIN_SIZE = (1 << 6),

    // Pass ComputeSizeFlags::eIApplyAutoMinSize to ComputeSize().
    I_APPLY_AUTO_MIN_SIZE = (1 << 7),
  };

  // This method initializes various data members. It is automatically
  // called by the various constructors
  void Init(nsPresContext* aPresContext,
            const mozilla::Maybe<mozilla::LogicalSize>& aContainingBlockSize =
                mozilla::Nothing(),
            const nsMargin* aBorder = nullptr,
            const nsMargin* aPadding = nullptr);

  /**
   * Find the content isize of our containing block for the given writing mode,
   * which need not be the same as the reflow input's mode.
   */
  nscoord GetContainingBlockContentISize(
      mozilla::WritingMode aWritingMode) const;

  /**
   * Calculate the used line-height property. The return value will be >= 0.
   */
  nscoord CalcLineHeight() const;

  /**
   * Same as CalcLineHeight() above, but doesn't need a reflow input.
   *
   * @param aBlockBSize The computed block size of the content rect of the block
   *                     that the line should fill.
   *                     Only used with line-height:-moz-block-height.
   *                     NS_UNCONSTRAINEDSIZE results in a normal line-height
   * for line-height:-moz-block-height.
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

  bool ShouldReflowAllKids() const {
    // Note that we could make a stronger optimization for IsBResize if
    // we use it in a ShouldReflowChild test that replaces the current
    // checks of NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN, if it
    // were tested there along with NS_FRAME_CONTAINS_RELATIVE_BSIZE.
    // This would need to be combined with a slight change in which
    // frames NS_FRAME_CONTAINS_RELATIVE_BSIZE is marked on.
    return (mFrame->GetStateBits() & NS_FRAME_IS_DIRTY) || IsIResize() ||
           (IsBResize() &&
            (mFrame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_BSIZE));
  }

  // This method doesn't apply min/max computed widths to the value passed in.
  void SetComputedWidth(nscoord aComputedWidth);

  // This method doesn't apply min/max computed heights to the value passed in.
  void SetComputedHeight(nscoord aComputedHeight);

  void SetComputedISize(nscoord aComputedISize) {
    if (mWritingMode.IsVertical()) {
      SetComputedHeight(aComputedISize);
    } else {
      SetComputedWidth(aComputedISize);
    }
  }

  void SetComputedBSize(nscoord aComputedBSize) {
    if (mWritingMode.IsVertical()) {
      SetComputedWidth(aComputedBSize);
    } else {
      SetComputedHeight(aComputedBSize);
    }
  }

  void SetComputedBSizeWithoutResettingResizeFlags(nscoord aComputedBSize) {
    // Viewport frames reset the computed block size on a copy of their reflow
    // input when reflowing fixed-pos kids.  In that case we actually don't
    // want to mess with the resize flags, because comparing the frame's rect
    // to the munged computed isize is pointless.
    ComputedBSize() = aComputedBSize;
  }

  void SetTruncated(const ReflowOutput& aMetrics,
                    nsReflowStatus* aStatus) const;

  bool WillReflowAgainForClearance() const {
    return mDiscoveredClearance && *mDiscoveredClearance;
  }

  // Compute the offsets for a relative position element
  static void ComputeRelativeOffsets(mozilla::WritingMode aWM, nsIFrame* aFrame,
                                     const mozilla::LogicalSize& aCBSize,
                                     nsMargin& aComputedOffsets);

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

  void ApplyRelativePositioning(mozilla::LogicalPoint* aPosition,
                                const nsSize& aContainerSize) const {
    ApplyRelativePositioning(mFrame, mWritingMode, ComputedLogicalOffsets(),
                             aPosition, aContainerSize);
  }

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
  void InitFrameType(LayoutFrameType aFrameType);
  void InitCBReflowInput();
  void InitResizeFlags(nsPresContext* aPresContext,
                       mozilla::LayoutFrameType aFrameType);
  void InitDynamicReflowRoot();

  void InitConstraints(
      nsPresContext* aPresContext,
      const mozilla::Maybe<mozilla::LogicalSize>& aContainingBlockSize,
      const nsMargin* aBorder, const nsMargin* aPadding,
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

  void InitAbsoluteConstraints(nsPresContext* aPresContext,
                               const ReflowInput* aCBReflowInput,
                               const mozilla::LogicalSize& aContainingBlockSize,
                               mozilla::LayoutFrameType aFrameType);

  // Calculates the computed values for the 'min-Width', 'max-Width',
  // 'min-Height', and 'max-Height' properties, and stores them in the assorted
  // data members
  void ComputeMinMaxValues(const mozilla::LogicalSize& aContainingBlockSize);

  // aInsideBoxSizing returns the part of the padding, border, and margin
  // in the aAxis dimension that goes inside the edge given by box-sizing;
  // aOutsideBoxSizing returns the rest.
  void CalculateBorderPaddingMargin(mozilla::LogicalAxis aAxis,
                                    nscoord aContainingBlockSize,
                                    nscoord* aInsideBoxSizing,
                                    nscoord* aOutsideBoxSizing) const;

  void CalculateBlockSideMargins(LayoutFrameType aFrameType);
};

}  // namespace mozilla

#endif  // mozilla_ReflowInput_h
