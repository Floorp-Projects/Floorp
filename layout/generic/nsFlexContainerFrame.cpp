/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: flex" */

#include "nsFlexContainerFrame.h"
#include "nsContentUtils.h"
#include "nsCSSAnonBoxes.h"
#include "nsDisplayList.h"
#include "nsIFrameInlines.h"
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "prlog.h"
#include <algorithm>
#include "mozilla/LinkedList.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::layout;

// Convenience typedefs for helper classes that we forward-declare in .h file
// (so that nsFlexContainerFrame methods can use them as parameters):
typedef nsFlexContainerFrame::FlexItem FlexItem;
typedef nsFlexContainerFrame::FlexLine FlexLine;
typedef nsFlexContainerFrame::FlexboxAxisTracker FlexboxAxisTracker;
typedef nsFlexContainerFrame::StrutInfo StrutInfo;

#ifdef PR_LOGGING
static PRLogModuleInfo*
GetFlexContainerLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("nsFlexContainerFrame");
  return sLog;
}
#endif /* PR_LOGGING */

// XXXdholbert Some of this helper-stuff should be separated out into a general
// "LogicalAxisUtils.h" helper.  Should that be a class, or a namespace (under
// what super-namespace?), or what?

// Helper enums
// ============

// Represents a physical orientation for an axis.
// The directional suffix indicates the direction in which the axis *grows*.
// So e.g. eAxis_LR means a horizontal left-to-right axis, whereas eAxis_BT
// means a vertical bottom-to-top axis.
// NOTE: The order here is important -- these values are used as indices into
// the static array 'kAxisOrientationToSidesMap', defined below.
enum AxisOrientationType {
  eAxis_LR,
  eAxis_RL,
  eAxis_TB,
  eAxis_BT,
  eNumAxisOrientationTypes // For sizing arrays that use these values as indices
};

// Represents one or the other extreme of an axis (e.g. for the main axis, the
// main-start vs. main-end edge.
// NOTE: The order here is important -- these values are used as indices into
// the sub-arrays in 'kAxisOrientationToSidesMap', defined below.
enum AxisEdgeType {
  eAxisEdge_Start,
  eAxisEdge_End,
  eNumAxisEdges // For sizing arrays that use these values as indices
};

// This array maps each axis orientation to a pair of corresponding
// [start, end] physical mozilla::css::Side values.
static const Side
kAxisOrientationToSidesMap[eNumAxisOrientationTypes][eNumAxisEdges] = {
  { eSideLeft,   eSideRight  },  // eAxis_LR
  { eSideRight,  eSideLeft   },  // eAxis_RL
  { eSideTop,    eSideBottom },  // eAxis_TB
  { eSideBottom, eSideTop }      // eAxis_BT
};

// Helper structs / classes / methods
// ==================================

// Indicates whether advancing along the given axis is equivalent to
// increasing our X or Y position (as opposed to decreasing it).
static inline bool
AxisGrowsInPositiveDirection(AxisOrientationType aAxis)
{
  return eAxis_LR == aAxis || eAxis_TB == aAxis;
}

// Indicates whether the given axis is horizontal.
static inline bool
IsAxisHorizontal(AxisOrientationType aAxis)
{
  return eAxis_LR == aAxis || eAxis_RL == aAxis;
}

// Given an AxisOrientationType, returns the "reverse" AxisOrientationType
// (in the same dimension, but the opposite direction)
static inline AxisOrientationType
GetReverseAxis(AxisOrientationType aAxis)
{
  AxisOrientationType reversedAxis;

  if (aAxis % 2 == 0) {
    // even enum value. Add 1 to reverse.
    reversedAxis = AxisOrientationType(aAxis + 1);
  } else {
    // odd enum value. Subtract 1 to reverse.
    reversedAxis = AxisOrientationType(aAxis - 1);
  }

  // Check that we're still in the enum's valid range
  MOZ_ASSERT(reversedAxis >= eAxis_LR &&
             reversedAxis <= eAxis_BT);

  return reversedAxis;
}

// Returns aFrame's computed value for 'height' or 'width' -- whichever is in
// the same dimension as aAxis.
static inline const nsStyleCoord&
GetSizePropertyForAxis(const nsIFrame* aFrame, AxisOrientationType aAxis)
{
  const nsStylePosition* stylePos = aFrame->StylePosition();

  return IsAxisHorizontal(aAxis) ?
    stylePos->mWidth :
    stylePos->mHeight;
}

/**
 * Converts a logical position in a given axis into a position in the
 * corresponding physical (x or y) axis. If the logical axis already maps
 * directly onto one of our physical axes (i.e. LTR or TTB), then the logical
 * and physical positions are equal; otherwise, we subtract the logical
 * position from the container-size in that axis, to flip the polarity.
 * (so e.g. a logical position of 2px in a RTL 20px-wide container
 * would correspond to a physical position of 18px.)
 */
static nscoord
PhysicalPosFromLogicalPos(nscoord aLogicalPosn,
                          nscoord aLogicalContainerSize,
                          AxisOrientationType aAxis) {
  if (AxisGrowsInPositiveDirection(aAxis)) {
    return aLogicalPosn;
  }
  return aLogicalContainerSize - aLogicalPosn;
}

static nscoord
MarginComponentForSide(const nsMargin& aMargin, Side aSide)
{
  switch (aSide) {
    case eSideLeft:
      return aMargin.left;
    case eSideRight:
      return aMargin.right;
    case eSideTop:
      return aMargin.top;
    case eSideBottom:
      return aMargin.bottom;
  }

  NS_NOTREACHED("unexpected Side enum");
  return aMargin.left; // have to return something
                       // (but something's busted if we got here)
}

static nscoord&
MarginComponentForSide(nsMargin& aMargin, Side aSide)
{
  switch (aSide) {
    case eSideLeft:
      return aMargin.left;
    case eSideRight:
      return aMargin.right;
    case eSideTop:
      return aMargin.top;
    case eSideBottom:
      return aMargin.bottom;
  }

  NS_NOTREACHED("unexpected Side enum");
  return aMargin.left; // have to return something
                       // (but something's busted if we got here)
}

// Helper-macro to let us pick one of two expressions to evaluate
// (a width expression vs. a height expression), to get a main-axis or
// cross-axis component.
// For code that has e.g. a nsSize object, FlexboxAxisTracker::GetMainComponent
// and GetCrossComponent are cleaner; but in cases where we simply have
// two separate expressions for width and height (which may be expensive to
// evaluate), these macros will ensure that only the expression for the correct
// axis gets evaluated.
#define GET_MAIN_COMPONENT(axisTracker_, width_, height_)  \
  IsAxisHorizontal((axisTracker_).GetMainAxis()) ? (width_) : (height_)

#define GET_CROSS_COMPONENT(axisTracker_, width_, height_)  \
  IsAxisHorizontal((axisTracker_).GetCrossAxis()) ? (width_) : (height_)

// Encapsulates our flex container's main & cross axes.
class MOZ_STACK_CLASS nsFlexContainerFrame::FlexboxAxisTracker {
public:
  FlexboxAxisTracker(nsFlexContainerFrame* aFlexContainerFrame);

  // Accessors:
  AxisOrientationType GetMainAxis() const  { return mMainAxis;  }
  AxisOrientationType GetCrossAxis() const { return mCrossAxis; }

  nscoord GetMainComponent(const nsSize& aSize) const {
    return GET_MAIN_COMPONENT(*this, aSize.width, aSize.height);
  }
  int32_t GetMainComponent(const nsIntSize& aIntSize) const {
    return GET_MAIN_COMPONENT(*this, aIntSize.width, aIntSize.height);
  }

  nscoord GetCrossComponent(const nsSize& aSize) const {
    return GET_CROSS_COMPONENT(*this, aSize.width, aSize.height);
  }
  int32_t GetCrossComponent(const nsIntSize& aIntSize) const {
    return GET_CROSS_COMPONENT(*this, aIntSize.width, aIntSize.height);
  }

  nscoord GetMarginSizeInMainAxis(const nsMargin& aMargin) const {
    return IsAxisHorizontal(mMainAxis) ?
      aMargin.LeftRight() :
      aMargin.TopBottom();
  }
  nscoord GetMarginSizeInCrossAxis(const nsMargin& aMargin) const {
    return IsAxisHorizontal(mCrossAxis) ?
      aMargin.LeftRight() :
      aMargin.TopBottom();
  }

  /**
   * Converts a logical point into a "physical" x,y point.
   *
   * In the simplest case where the main-axis is left-to-right and the
   * cross-axis is top-to-bottom, this just returns
   * nsPoint(aMainPosn, aCrossPosn).
   *
   *  @arg aMainPosn  The main-axis position -- i.e an offset from the
   *                  main-start edge of the container's content box.
   *  @arg aCrossPosn The cross-axis position -- i.e an offset from the
   *                  cross-start edge of the container's content box.
   *  @return A nsPoint representing the same position (in coordinates
   *          relative to the container's content box).
   */
  nsPoint PhysicalPointFromLogicalPoint(nscoord aMainPosn,
                                        nscoord aCrossPosn,
                                        nscoord aContainerMainSize,
                                        nscoord aContainerCrossSize) const {
    nscoord physicalPosnInMainAxis =
      PhysicalPosFromLogicalPos(aMainPosn, aContainerMainSize, mMainAxis);
    nscoord physicalPosnInCrossAxis =
      PhysicalPosFromLogicalPos(aCrossPosn, aContainerCrossSize, mCrossAxis);

    return IsAxisHorizontal(mMainAxis) ?
      nsPoint(physicalPosnInMainAxis, physicalPosnInCrossAxis) :
      nsPoint(physicalPosnInCrossAxis, physicalPosnInMainAxis);
  }
  nsSize PhysicalSizeFromLogicalSizes(nscoord aMainSize,
                                      nscoord aCrossSize) const {
    return IsAxisHorizontal(mMainAxis) ?
      nsSize(aMainSize, aCrossSize) :
      nsSize(aCrossSize, aMainSize);
  }

  // Are my axes reversed with respect to what the author asked for?
  // (We may reverse the axes in the FlexboxAxisTracker constructor and set
  // this flag, to avoid reflowing our children in bottom-to-top order.)
  bool AreAxesInternallyReversed() const
  {
    return mAreAxesInternallyReversed;
  }

private:
  AxisOrientationType mMainAxis;
  AxisOrientationType mCrossAxis;
  bool mAreAxesInternallyReversed;
};

/**
 * Represents a flex item.
 * Includes the various pieces of input that the Flexbox Layout Algorithm uses
 * to resolve a flexible width.
 */
class nsFlexContainerFrame::FlexItem : public LinkedListElement<FlexItem>
{
public:
  // Normal constructor:
  FlexItem(nsIFrame* aChildFrame,
           float aFlexGrow, float aFlexShrink, nscoord aMainBaseSize,
           nscoord aMainMinSize, nscoord aMainMaxSize,
           nscoord aCrossMinSize, nscoord aCrossMaxSize,
           nsMargin aMargin, nsMargin aBorderPadding,
           const FlexboxAxisTracker& aAxisTracker);

  // Simplified constructor, to be used only for generating "struts":
  FlexItem(nsIFrame* aChildFrame, nscoord aCrossSize);

  // Accessors
  nsIFrame* Frame() const          { return mFrame; }
  nscoord GetFlexBaseSize() const  { return mFlexBaseSize; }

  nscoord GetMainMinSize() const   { return mMainMinSize; }
  nscoord GetMainMaxSize() const   { return mMainMaxSize; }

  // Note: These return the main-axis position and size of our *content box*.
  nscoord GetMainSize() const      { return mMainSize; }
  nscoord GetMainPosition() const  { return mMainPosn; }

  nscoord GetCrossMinSize() const  { return mCrossMinSize; }
  nscoord GetCrossMaxSize() const  { return mCrossMaxSize; }

  // Note: These return the cross-axis position and size of our *content box*.
  nscoord GetCrossSize() const     { return mCrossSize;  }
  nscoord GetCrossPosition() const { return mCrossPosn; }

  // Convenience methods to compute the main & cross size of our *margin-box*.
  // The caller is responsible for telling us the right axis, so that we can
  // pull out the appropriate components of our margin/border/padding structs.
  nscoord GetOuterMainSize(AxisOrientationType aMainAxis) const
  {
    return mMainSize + GetMarginBorderPaddingSizeInAxis(aMainAxis);
  }

  nscoord GetOuterCrossSize(AxisOrientationType aCrossAxis) const
  {
    return mCrossSize + GetMarginBorderPaddingSizeInAxis(aCrossAxis);
  }

  // Returns the distance between this FlexItem's baseline and the cross-start
  // edge of its margin-box. Used in baseline alignment.
  // (This function needs to be told what cross axis is & which edge we're
  // measuring the baseline from, so that it can look up the appropriate
  // components from mMargin.)
  nscoord GetBaselineOffsetFromOuterCrossEdge(AxisOrientationType aCrossAxis,
                                              AxisEdgeType aEdge) const;

  float GetShareOfWeightSoFar() const { return mShareOfWeightSoFar; }

  bool IsFrozen() const            { return mIsFrozen; }

  bool HadMinViolation() const     { return mHadMinViolation; }
  bool HadMaxViolation() const     { return mHadMaxViolation; }

  // Indicates whether this item received a preliminary "measuring" reflow
  // before its actual reflow.
  bool HadMeasuringReflow() const  { return mHadMeasuringReflow; }

  // Indicates whether this item's cross-size has been stretched (from having
  // "align-self: stretch" with an auto cross-size and no auto margins in the
  // cross axis).
  bool IsStretched() const         { return mIsStretched; }

  // Indicates whether this item is a "strut" left behind by an element with
  // visibility:collapse.
  bool IsStrut() const             { return mIsStrut; }

  uint8_t GetAlignSelf() const     { return mAlignSelf; }

  // Returns the flex factor (flex-grow or flex-shrink), depending on
  // 'aIsUsingFlexGrow'.
  //
  // Asserts fatally if called on a frozen item (since frozen items are not
  // flexible).
  float GetFlexFactor(bool aIsUsingFlexGrow)
  {
    MOZ_ASSERT(!IsFrozen(), "shouldn't need flex factor after item is frozen");

    return aIsUsingFlexGrow ? mFlexGrow : mFlexShrink;
  }

  // Returns the weight that we should use in the "resolving flexible lengths"
  // algorithm.  If we're using the flex grow factor, we just return that;
  // otherwise, we return the "scaled flex shrink factor" (scaled by our flex
  // base size, so that when both large and small items are shrinking, the large
  // items shrink more).
  //
  // I'm calling this a "weight" instead of a "[scaled] flex-[grow|shrink]
  // factor", to more clearly distinguish it from the actual flex-grow &
  // flex-shrink factors.
  //
  // Asserts fatally if called on a frozen item (since frozen items are not
  // flexible).
  float GetWeight(bool aIsUsingFlexGrow)
  {
    MOZ_ASSERT(!IsFrozen(), "shouldn't need weight after item is frozen");

    if (aIsUsingFlexGrow) {
      return mFlexGrow;
    }

    // We're using flex-shrink --> return mFlexShrink * mFlexBaseSize
    if (mFlexBaseSize == 0) {
      // Special-case for mFlexBaseSize == 0 -- we have no room to shrink, so
      // regardless of mFlexShrink, we should just return 0.
      // (This is really a special-case for when mFlexShrink is infinity, to
      // avoid performing mFlexShrink * mFlexBaseSize = inf * 0 = undefined.)
      return 0.0f;
    }
    return mFlexShrink * mFlexBaseSize;
  }

  // Getters for margin:
  // ===================
  const nsMargin& GetMargin() const { return mMargin; }

  // Returns the margin component for a given mozilla::css::Side
  nscoord GetMarginComponentForSide(Side aSide) const
  { return MarginComponentForSide(mMargin, aSide); }

  // Returns the total space occupied by this item's margins in the given axis
  nscoord GetMarginSizeInAxis(AxisOrientationType aAxis) const
  {
    Side startSide = kAxisOrientationToSidesMap[aAxis][eAxisEdge_Start];
    Side endSide = kAxisOrientationToSidesMap[aAxis][eAxisEdge_End];
    return GetMarginComponentForSide(startSide) +
      GetMarginComponentForSide(endSide);
  }

  // Getters for border/padding
  // ==========================
  const nsMargin& GetBorderPadding() const { return mBorderPadding; }

  // Returns the border+padding component for a given mozilla::css::Side
  nscoord GetBorderPaddingComponentForSide(Side aSide) const
  { return MarginComponentForSide(mBorderPadding, aSide); }

  // Returns the total space occupied by this item's borders and padding in
  // the given axis
  nscoord GetBorderPaddingSizeInAxis(AxisOrientationType aAxis) const
  {
    Side startSide = kAxisOrientationToSidesMap[aAxis][eAxisEdge_Start];
    Side endSide = kAxisOrientationToSidesMap[aAxis][eAxisEdge_End];
    return GetBorderPaddingComponentForSide(startSide) +
      GetBorderPaddingComponentForSide(endSide);
  }

  // Getter for combined margin/border/padding
  // =========================================
  // Returns the total space occupied by this item's margins, borders and
  // padding in the given axis
  nscoord GetMarginBorderPaddingSizeInAxis(AxisOrientationType aAxis) const
  {
    return GetMarginSizeInAxis(aAxis) + GetBorderPaddingSizeInAxis(aAxis);
  }

  // Setters
  // =======

  // This sets our flex base size, and then sets our main size to the
  // resulting "hypothetical main size" (the base size clamped to our
  // main-axis [min,max] sizing constraints).
  void SetFlexBaseSizeAndMainSize(nscoord aNewFlexBaseSize)
  {
    MOZ_ASSERT(!mIsFrozen || mFlexBaseSize == NS_INTRINSICSIZE,
               "flex base size shouldn't change after we're frozen "
               "(unless we're just resolving an intrinsic size)");
    mFlexBaseSize = aNewFlexBaseSize;

    // Before we've resolved flexible lengths, we keep mMainSize set to
    // the 'hypothetical main size', which is the flex base size, clamped
    // to the [min,max] range:
    mMainSize = NS_CSS_MINMAX(mFlexBaseSize, mMainMinSize, mMainMaxSize);
  }

  // Setters used while we're resolving flexible lengths
  // ---------------------------------------------------

  // Sets the main-size of our flex item's content-box.
  void SetMainSize(nscoord aNewMainSize)
  {
    MOZ_ASSERT(!mIsFrozen, "main size shouldn't change after we're frozen");
    mMainSize = aNewMainSize;
  }

  void SetShareOfWeightSoFar(float aNewShare)
  {
    MOZ_ASSERT(!mIsFrozen || aNewShare == 0.0f,
               "shouldn't be giving this item any share of the weight "
               "after it's frozen");
    mShareOfWeightSoFar = aNewShare;
  }

  void Freeze() { mIsFrozen = true; }

  void SetHadMinViolation()
  {
    MOZ_ASSERT(!mIsFrozen,
               "shouldn't be changing main size & having violations "
               "after we're frozen");
    mHadMinViolation = true;
  }
  void SetHadMaxViolation()
  {
    MOZ_ASSERT(!mIsFrozen,
               "shouldn't be changing main size & having violations "
               "after we're frozen");
    mHadMaxViolation = true;
  }
  void ClearViolationFlags()
  { mHadMinViolation = mHadMaxViolation = false; }

  // Setters for values that are determined after we've resolved our main size
  // -------------------------------------------------------------------------

  // Sets the main-axis position of our flex item's content-box.
  // (This is the distance between the main-start edge of the flex container
  // and the main-start edge of the flex item's content-box.)
  void SetMainPosition(nscoord aPosn) {
    MOZ_ASSERT(mIsFrozen, "main size should be resolved before this");
    mMainPosn  = aPosn;
  }

  // Sets the cross-size of our flex item's content-box.
  void SetCrossSize(nscoord aCrossSize) {
    MOZ_ASSERT(!mIsStretched,
               "Cross size shouldn't be modified after it's been stretched");
    mCrossSize = aCrossSize;
  }

  // Sets the cross-axis position of our flex item's content-box.
  // (This is the distance between the cross-start edge of the flex container
  // and the cross-start edge of the flex item.)
  void SetCrossPosition(nscoord aPosn) {
    MOZ_ASSERT(mIsFrozen, "main size should be resolved before this");
    mCrossPosn = aPosn;
  }

  void SetAscent(nscoord aAscent) {
    mAscent = aAscent;
  }

  void SetHadMeasuringReflow() {
    mHadMeasuringReflow = true;
  }

  void SetIsStretched() {
    MOZ_ASSERT(mIsFrozen, "main size should be resolved before this");
    mIsStretched = true;
  }

  // Setter for margin components (for resolving "auto" margins)
  void SetMarginComponentForSide(Side aSide, nscoord aLength)
  {
    MOZ_ASSERT(mIsFrozen, "main size should be resolved before this");
    MarginComponentForSide(mMargin, aSide) = aLength;
  }

  void ResolveStretchedCrossSize(nscoord aLineCrossSize,
                                 const FlexboxAxisTracker& aAxisTracker);

  uint32_t GetNumAutoMarginsInAxis(AxisOrientationType aAxis) const;

protected:
  // Our frame:
  nsIFrame* const mFrame;

  // Values that we already know in constructor: (and are hence mostly 'const')
  const float mFlexGrow;
  const float mFlexShrink;

  const nsMargin mBorderPadding;
  nsMargin mMargin; // non-const because we need to resolve auto margins

  nscoord mFlexBaseSize;

  const nscoord mMainMinSize;
  const nscoord mMainMaxSize;
  const nscoord mCrossMinSize;
  const nscoord mCrossMaxSize;

  // Values that we compute after constructor:
  nscoord mMainSize;
  nscoord mMainPosn;
  nscoord mCrossSize;
  nscoord mCrossPosn;
  nscoord mAscent;

  // Temporary state, while we're resolving flexible widths (for our main size)
  // XXXdholbert To save space, we could use a union to make these variables
  // overlay the same memory as some other member vars that aren't touched
  // until after main-size has been resolved. In particular, these could share
  // memory with mMainPosn through mAscent, and mIsStretched.
  float mShareOfWeightSoFar;
  bool mIsFrozen;
  bool mHadMinViolation;
  bool mHadMaxViolation;

  // Misc:
  bool mHadMeasuringReflow; // Did this item get a preliminary reflow,
                            // to measure its desired height?
  bool mIsStretched; // See IsStretched() documentation
  bool mIsStrut;     // Is this item a "strut" left behind by an element
                     // with visibility:collapse?
  uint8_t mAlignSelf; // My "align-self" computed value (with "auto"
                      // swapped out for parent"s "align-items" value,
                      // in our constructor).
};

/**
 * Represents a single flex line in a flex container.
 * Manages a linked list of the FlexItems that are in the line.
 */
class nsFlexContainerFrame::FlexLine : public LinkedListElement<FlexLine>
{
public:
  FlexLine()
  : mNumItems(0),
    mNumFrozenItems(0),
    mTotalInnerHypotheticalMainSize(0),
    mTotalOuterHypotheticalMainSize(0),
    mLineCrossSize(0),
    mBaselineOffset(nscoord_MIN)
  {}

  // Returns the sum of our FlexItems' outer hypothetical main sizes.
  // ("outer" = margin-box, and "hypothetical" = before flexing)
  nscoord GetTotalOuterHypotheticalMainSize() const {
    return mTotalOuterHypotheticalMainSize;
  }

  // Accessors for our FlexItems & information about them:
  FlexItem* GetFirstItem()
  {
    MOZ_ASSERT(mItems.isEmpty() == (mNumItems == 0),
               "mNumItems bookkeeping is off");
    return mItems.getFirst();
  }

  const FlexItem* GetFirstItem() const
  {
    MOZ_ASSERT(mItems.isEmpty() == (mNumItems == 0),
               "mNumItems bookkeeping is off");
    return mItems.getFirst();
  }

  bool IsEmpty() const
  {
    MOZ_ASSERT(mItems.isEmpty() == (mNumItems == 0),
               "mNumItems bookkeeping is off");
    return mItems.isEmpty();
  }

  uint32_t NumItems() const
  {
    MOZ_ASSERT(mItems.isEmpty() == (mNumItems == 0),
               "mNumItems bookkeeping is off");
    return mNumItems;
  }

  // Adds the given FlexItem to our list of items (at the front or back
  // depending on aShouldInsertAtFront), and adds its hypothetical
  // outer & inner main sizes to our totals. Use this method instead of
  // directly modifying the item list, so that our bookkeeping remains correct.
  void AddItem(FlexItem* aItem,
               bool aShouldInsertAtFront,
               nscoord aItemInnerHypotheticalMainSize,
               nscoord aItemOuterHypotheticalMainSize)
  {
    if (aShouldInsertAtFront) {
      mItems.insertFront(aItem);
    } else {
      mItems.insertBack(aItem);
    }

    // Update our various bookkeeping member-vars:
    mNumItems++;
    if (aItem->IsFrozen()) {
      mNumFrozenItems++;
    }
    mTotalInnerHypotheticalMainSize += aItemInnerHypotheticalMainSize;
    mTotalOuterHypotheticalMainSize += aItemOuterHypotheticalMainSize;
  }

  // Computes the cross-size and baseline position of this FlexLine, based on
  // its FlexItems.
  void ComputeCrossSizeAndBaseline(const FlexboxAxisTracker& aAxisTracker);

  // Returns the cross-size of this line.
  nscoord GetLineCrossSize() const { return mLineCrossSize; }

  // Setter for line cross-size -- needed for cases where the flex container
  // imposes a cross-size on the line. (e.g. for single-line flexbox, or for
  // multi-line flexbox with 'align-content: stretch')
  void SetLineCrossSize(nscoord aLineCrossSize) {
    mLineCrossSize = aLineCrossSize;
  }

  /**
   * Returns the offset within this line where any baseline-aligned FlexItems
   * should place their baseline. Usually, this represents a distance from the
   * line's cross-start edge, but if we're internally reversing the axes (see
   * AreAxesInternallyReversed()), this instead represents the distance from
   * its cross-end edge.
   *
   * If there are no baseline-aligned FlexItems, returns nscoord_MIN.
   */
  nscoord GetBaselineOffset() const {
    return mBaselineOffset;
  }

  // Runs the "Resolving Flexible Lengths" algorithm from section 9.7 of the
  // CSS flexbox spec to distribute aFlexContainerMainSize among our flex items.
  void ResolveFlexibleLengths(nscoord aFlexContainerMainSize);

  void PositionItemsInMainAxis(uint8_t aJustifyContent,
                               nscoord aContentBoxMainSize,
                               const FlexboxAxisTracker& aAxisTracker);

  void PositionItemsInCrossAxis(nscoord aLineStartPosition,
                                const FlexboxAxisTracker& aAxisTracker);

  friend class AutoFlexLineListClearer; // (needs access to mItems)

private:
  // Helpers for ResolveFlexibleLengths():
  void FreezeItemsEarly(bool aIsUsingFlexGrow);

  void FreezeOrRestoreEachFlexibleSize(const nscoord aTotalViolation,
                                       bool aIsFinalIteration);

  LinkedList<FlexItem> mItems; // Linked list of this line's flex items.

  uint32_t mNumItems; // Number of FlexItems in this line (in |mItems|).
                      // (Shouldn't change after GenerateFlexLines finishes
                      // with this line -- at least, not until we add support
                      // for splitting lines across continuations. Then we can
                      // update this count carefully.)

  // Number of *frozen* FlexItems in this line, based on FlexItem::IsFrozen().
  // Mostly used for optimization purposes, e.g. to bail out early from loops
  // when we can tell they have nothing left to do.
  uint32_t mNumFrozenItems;

  nscoord mTotalInnerHypotheticalMainSize;
  nscoord mTotalOuterHypotheticalMainSize;
  nscoord mLineCrossSize;
  nscoord mBaselineOffset;
};

// Information about a strut left behind by a FlexItem that's been collapsed
// using "visibility:collapse".
struct nsFlexContainerFrame::StrutInfo {
  StrutInfo(uint32_t aItemIdx, nscoord aStrutCrossSize)
    : mItemIdx(aItemIdx),
      mStrutCrossSize(aStrutCrossSize)
  {
  }

  uint32_t mItemIdx;      // Index in the child list.
  nscoord mStrutCrossSize; // The cross-size of this strut.
};

static void
BuildStrutInfoFromCollapsedItems(const FlexLine* aFirstLine,
                                 nsTArray<StrutInfo>& aStruts)
{
  MOZ_ASSERT(aFirstLine, "null first line pointer");
  MOZ_ASSERT(aStruts.IsEmpty(),
             "We should only build up StrutInfo once per reflow, so "
             "aStruts should be empty when this is called");

  uint32_t itemIdxInContainer = 0;
  for (const FlexLine* line = aFirstLine; line; line = line->getNext()) {
    for (const FlexItem* item = line->GetFirstItem(); item;
         item = item->getNext()) {
      if (NS_STYLE_VISIBILITY_COLLAPSE ==
          item->Frame()->StyleVisibility()->mVisible) {
        // Note the cross size of the line as the item's strut size.
        aStruts.AppendElement(StrutInfo(itemIdxInContainer,
                                        line->GetLineCrossSize()));
      }
      itemIdxInContainer++;
    }
  }
}

// Helper-function to find the first non-anonymous-box descendent of aFrame.
static nsIFrame*
GetFirstNonAnonBoxDescendant(nsIFrame* aFrame)
{
  while (aFrame) {
    nsIAtom* pseudoTag = aFrame->StyleContext()->GetPseudo();

    // If aFrame isn't an anonymous container, then it'll do.
    if (!pseudoTag ||                                 // No pseudotag.
        !nsCSSAnonBoxes::IsAnonBox(pseudoTag) ||      // Pseudotag isn't anon.
        pseudoTag == nsCSSAnonBoxes::mozNonElement) { // Text, not a container.
      break;
    }

    // Otherwise, descend to its first child and repeat.

    // SPECIAL CASE: if we're dealing with an anonymous table, then it might
    // be wrapping something non-anonymous in its caption or col-group lists
    // (instead of its principal child list), so we have to look there.
    // (Note: For anonymous tables that have a non-anon cell *and* a non-anon
    // column, we'll always return the column. This is fine; we're really just
    // looking for a handle to *anything* with a meaningful content node inside
    // the table, for use in DOM comparisons to things outside of the table.)
    if (MOZ_UNLIKELY(aFrame->GetType() == nsGkAtoms::tableOuterFrame)) {
      nsIFrame* captionDescendant =
        GetFirstNonAnonBoxDescendant(aFrame->GetFirstChild(kCaptionList));
      if (captionDescendant) {
        return captionDescendant;
      }
    } else if (MOZ_UNLIKELY(aFrame->GetType() == nsGkAtoms::tableFrame)) {
      nsIFrame* colgroupDescendant =
        GetFirstNonAnonBoxDescendant(aFrame->GetFirstChild(kColGroupList));
      if (colgroupDescendant) {
        return colgroupDescendant;
      }
    }

    // USUAL CASE: Descend to the first child in principal list.
    aFrame = aFrame->GetFirstPrincipalChild();
  }
  return aFrame;
}

/**
 * Sorting helper-function that compares two frames' "order" property-values,
 * and if they're equal, compares the DOM positions of their corresponding
 * content nodes. Returns true if aFrame1 is "less than or equal to" aFrame2
 * according to this comparison.
 *
 * Note: This can't be a static function, because we need to pass it as a
 * template argument. (Only functions with external linkage can be passed as
 * template arguments.)
 *
 * @return true if the computed "order" property of aFrame1 is less than that
 *         of aFrame2, or if the computed "order" values are equal and aFrame1's
 *         corresponding DOM node is earlier than aFrame2's in the DOM tree.
 *         Otherwise, returns false.
 */
bool
IsOrderLEQWithDOMFallback(nsIFrame* aFrame1,
                          nsIFrame* aFrame2)
{
  MOZ_ASSERT(aFrame1->IsFlexItem() && aFrame2->IsFlexItem(),
             "this method only intended for comparing flex items");

  if (aFrame1 == aFrame2) {
    // Anything is trivially LEQ itself, so we return "true" here... but it's
    // probably bad if we end up actually needing this, so let's assert.
    NS_ERROR("Why are we checking if a frame is LEQ itself?");
    return true;
  }

  // If we've got a placeholder frame, use its out-of-flow frame's 'order' val.
  {
    nsIFrame* aRealFrame1 = nsPlaceholderFrame::GetRealFrameFor(aFrame1);
    nsIFrame* aRealFrame2 = nsPlaceholderFrame::GetRealFrameFor(aFrame2);

    int32_t order1 = aRealFrame1->StylePosition()->mOrder;
    int32_t order2 = aRealFrame2->StylePosition()->mOrder;

    if (order1 != order2) {
      return order1 < order2;
    }
  }

  // The "order" values are equal, so we need to fall back on DOM comparison.
  // For that, we need to dig through any anonymous box wrapper frames to find
  // the actual frame that corresponds to our child content.
  aFrame1 = GetFirstNonAnonBoxDescendant(aFrame1);
  aFrame2 = GetFirstNonAnonBoxDescendant(aFrame2);
  MOZ_ASSERT(aFrame1 && aFrame2,
             "why do we have an anonymous box without any "
             "non-anonymous descendants?");


  // Special case:
  // If either frame is for generated content from ::before or ::after, then
  // we can't use nsContentUtils::PositionIsBefore(), since that method won't
  // recognize generated content as being an actual sibling of other nodes.
  // We know where ::before and ::after nodes *effectively* insert in the DOM
  // tree, though (at the beginning & end), so we can just special-case them.
  nsIAtom* pseudo1 = aFrame1->StyleContext()->GetPseudo();
  nsIAtom* pseudo2 = aFrame2->StyleContext()->GetPseudo();
  if (pseudo1 == nsCSSPseudoElements::before ||
      pseudo2 == nsCSSPseudoElements::after) {
    // frame1 is ::before and/or frame2 is ::after => frame1 is LEQ frame2.
    return true;
  }
  if (pseudo1 == nsCSSPseudoElements::after ||
      pseudo2 == nsCSSPseudoElements::before) {
    // frame1 is ::after and/or frame2 is ::before => frame1 is not LEQ frame2.
    return false;
  }

  // Usual case: Compare DOM position.
  nsIContent* content1 = aFrame1->GetContent();
  nsIContent* content2 = aFrame2->GetContent();
  MOZ_ASSERT(content1 != content2,
             "Two different flex items are using the same nsIContent node for "
             "comparison, so we may be sorting them in an arbitrary order");

  return nsContentUtils::PositionIsBefore(content1, content2);
}

/**
 * Sorting helper-function that compares two frames' "order" property-values.
 * Returns true if aFrame1 is "less than or equal to" aFrame2 according to this
 * comparison.
 *
 * Note: This can't be a static function, because we need to pass it as a
 * template argument. (Only functions with external linkage can be passed as
 * template arguments.)
 *
 * @return true if the computed "order" property of aFrame1 is less than or
 *         equal to that of aFrame2.  Otherwise, returns false.
 */
bool
IsOrderLEQ(nsIFrame* aFrame1,
           nsIFrame* aFrame2)
{
  MOZ_ASSERT(aFrame1->IsFlexItem() && aFrame2->IsFlexItem(),
             "this method only intended for comparing flex items");

  // If we've got a placeholder frame, use its out-of-flow frame's 'order' val.
  nsIFrame* aRealFrame1 = nsPlaceholderFrame::GetRealFrameFor(aFrame1);
  nsIFrame* aRealFrame2 = nsPlaceholderFrame::GetRealFrameFor(aFrame2);

  int32_t order1 = aRealFrame1->StylePosition()->mOrder;
  int32_t order2 = aRealFrame2->StylePosition()->mOrder;

  return order1 <= order2;
}

bool
nsFlexContainerFrame::IsHorizontal()
{
  const FlexboxAxisTracker axisTracker(this);
  return IsAxisHorizontal(axisTracker.GetMainAxis());
}

FlexItem*
nsFlexContainerFrame::GenerateFlexItemForChild(
  nsPresContext* aPresContext,
  nsIFrame*      aChildFrame,
  const nsHTMLReflowState& aParentReflowState,
  const FlexboxAxisTracker& aAxisTracker)
{
  // Create temporary reflow state just for sizing -- to get hypothetical
  // main-size and the computed values of min / max main-size property.
  // (This reflow state will _not_ be used for reflow.)
  nsHTMLReflowState childRS(aPresContext, aParentReflowState, aChildFrame,
                            nsSize(aParentReflowState.ComputedWidth(),
                                   aParentReflowState.ComputedHeight()));

  // FLEX GROW & SHRINK WEIGHTS
  // --------------------------
  const nsStylePosition* stylePos = aChildFrame->StylePosition();
  float flexGrow   = stylePos->mFlexGrow;
  float flexShrink = stylePos->mFlexShrink;

  // MAIN SIZES (flex base size, min/max size)
  // -----------------------------------------
  nscoord flexBaseSize = GET_MAIN_COMPONENT(aAxisTracker,
                                            childRS.ComputedWidth(),
                                            childRS.ComputedHeight());
  nscoord mainMinSize = GET_MAIN_COMPONENT(aAxisTracker,
                                           childRS.ComputedMinWidth(),
                                           childRS.ComputedMinHeight());
  nscoord mainMaxSize = GET_MAIN_COMPONENT(aAxisTracker,
                                           childRS.ComputedMaxWidth(),
                                           childRS.ComputedMaxHeight());
  // This is enforced by the nsHTMLReflowState where these values come from:
  MOZ_ASSERT(mainMinSize <= mainMaxSize, "min size is larger than max size");

  // CROSS MIN/MAX SIZE
  // ------------------

  nscoord crossMinSize = GET_CROSS_COMPONENT(aAxisTracker,
                                             childRS.ComputedMinWidth(),
                                             childRS.ComputedMinHeight());
  nscoord crossMaxSize = GET_CROSS_COMPONENT(aAxisTracker,
                                             childRS.ComputedMaxWidth(),
                                             childRS.ComputedMaxHeight());

  // SPECIAL-CASE FOR WIDGET-IMPOSED SIZES
  // Check if we're a themed widget, in which case we might have a minimum
  // main & cross size imposed by our widget (which we can't go below), or
  // (more severe) our widget might have only a single valid size.
  bool isFixedSizeWidget = false;
  const nsStyleDisplay* disp = aChildFrame->StyleDisplay();
  if (aChildFrame->IsThemed(disp)) {
    nsIntSize widgetMinSize(0, 0);
    bool canOverride = true;
    aPresContext->GetTheme()->
      GetMinimumWidgetSize(childRS.rendContext, aChildFrame,
                           disp->mAppearance,
                           &widgetMinSize, &canOverride);

    nscoord widgetMainMinSize =
      aPresContext->DevPixelsToAppUnits(
        aAxisTracker.GetMainComponent(widgetMinSize));
    nscoord widgetCrossMinSize =
      aPresContext->DevPixelsToAppUnits(
        aAxisTracker.GetCrossComponent(widgetMinSize));

    // GMWS() returns border-box. We need content-box, so subtract
    // borderPadding (but don't let that push our min sizes below 0).
    nsMargin& bp = childRS.ComputedPhysicalBorderPadding();
    widgetMainMinSize = std::max(widgetMainMinSize -
                                 aAxisTracker.GetMarginSizeInMainAxis(bp), 0);
    widgetCrossMinSize = std::max(widgetCrossMinSize -
                                  aAxisTracker.GetMarginSizeInCrossAxis(bp), 0);

    if (!canOverride) {
      // Fixed-size widget: freeze our main-size at the widget's mandated size.
      // (Set min and max main-sizes to that size, too, to keep us from
      // clamping to any other size later on.)
      flexBaseSize = mainMinSize = mainMaxSize = widgetMainMinSize;
      crossMinSize = crossMaxSize = widgetCrossMinSize;
      isFixedSizeWidget = true;
    } else {
      // Variable-size widget: ensure our min/max sizes are at least as large
      // as the widget's mandated minimum size, so we don't flex below that.
      mainMinSize = std::max(mainMinSize, widgetMainMinSize);
      mainMaxSize = std::max(mainMaxSize, widgetMainMinSize);

      crossMinSize = std::max(crossMinSize, widgetCrossMinSize);
      crossMaxSize = std::max(crossMaxSize, widgetCrossMinSize);
    }
  }

  // Construct the flex item!
  FlexItem* item = new FlexItem(aChildFrame,
                                flexGrow, flexShrink, flexBaseSize,
                                mainMinSize, mainMaxSize,
                                crossMinSize, crossMaxSize,
                                childRS.ComputedPhysicalMargin(),
                                childRS.ComputedPhysicalBorderPadding(),
                                aAxisTracker);

  // If we're inflexible, we can just freeze to our hypothetical main-size
  // up-front. Similarly, if we're a fixed-size widget, we only have one
  // valid size, so we freeze to keep ourselves from flexing.
  if (isFixedSizeWidget || (flexGrow == 0.0f && flexShrink == 0.0f)) {
    item->Freeze();
  }

  return item;
}

void
nsFlexContainerFrame::
  ResolveFlexItemMaxContentSizing(nsPresContext* aPresContext,
                                  FlexItem& aFlexItem,
                                  const nsHTMLReflowState& aParentReflowState,
                                  const FlexboxAxisTracker& aAxisTracker)
{
  if (IsAxisHorizontal(aAxisTracker.GetMainAxis())) {
    // Nothing to do -- this function is only for measuring flex items
    // in a vertical flex container.
    return;
  }

  if (NS_AUTOHEIGHT != aFlexItem.GetFlexBaseSize()) {
    // Nothing to do; this function's only relevant for flex items
    // with a base size of "auto" (or equivalent).
    // XXXdholbert If & when we handle "min-height: min-content" for flex items,
    // we'll want to resolve that in this function, too.
    return;
  }

  // If we get here, we're vertical and our main size ended up being
  // unconstrained. We need to use our "max-content" height, which is what we
  // get from reflowing into our available width.
  // Note: This has to come *after* we construct the FlexItem, since we
  // invoke at least one convenience method (ResolveStretchedCrossSize) which
  // requires a FlexItem.

  // Give the item a special reflow with "mIsFlexContainerMeasuringHeight"
  // set.  This tells it to behave as if it had "height: auto", regardless
  // of what the "height" property is actually set to.
  nsHTMLReflowState
    childRSForMeasuringHeight(aPresContext, aParentReflowState,
                              aFlexItem.Frame(),
                              nsSize(aParentReflowState.ComputedWidth(),
                                     NS_UNCONSTRAINEDSIZE),
                              -1, -1, nsHTMLReflowState::CALLER_WILL_INIT);
  childRSForMeasuringHeight.mFlags.mIsFlexContainerMeasuringHeight = true;
  childRSForMeasuringHeight.Init(aPresContext);

  // For single-line vertical flexbox, we need to give our flex items an early
  // opportunity to stretch, since any stretching of their cross-size (width)
  // will likely impact the max-content main-size (height) that we're about to
  // measure for them. (We can't do this for multi-line, since we don't know
  // yet how many lines there will be & how much each line will stretch.)
  if (NS_STYLE_FLEX_WRAP_NOWRAP ==
      aParentReflowState.mStylePosition->mFlexWrap) {
    aFlexItem.ResolveStretchedCrossSize(aParentReflowState.ComputedWidth(),
                                        aAxisTracker);
  }

  if (aFlexItem.IsStretched()) {
    childRSForMeasuringHeight.SetComputedWidth(aFlexItem.GetCrossSize());
    childRSForMeasuringHeight.mFlags.mHResize = true;
  }

  // If this item is flexible (vertically), then we assume that the
  // computed-height we're reflowing with now could be different
  // from the one we'll use for this flex item's "actual" reflow later on.
  // In that case, we need to be sure the flex item treats this as a
  // vertical resize, even though none of its ancestors are necessarily
  // being vertically resized.
  // (Note: We don't have to do this for width, because InitResizeFlags
  // will always turn on mHResize on when it sees that the computed width
  // is different from current width, and that's all we need.)
  if (!aFlexItem.IsFrozen()) {  // Are we flexible?
    childRSForMeasuringHeight.mFlags.mVResize = true;
  }

  nsHTMLReflowMetrics childDesiredSize(childRSForMeasuringHeight);
  nsReflowStatus childReflowStatus;
  const uint32_t flags = NS_FRAME_NO_MOVE_FRAME;
  ReflowChild(aFlexItem.Frame(), aPresContext,
              childDesiredSize, childRSForMeasuringHeight,
              0, 0, flags, childReflowStatus);

  MOZ_ASSERT(NS_FRAME_IS_COMPLETE(childReflowStatus),
             "We gave flex item unconstrained available height, so it "
             "should be complete");

  FinishReflowChild(aFlexItem.Frame(), aPresContext,
                    childDesiredSize, &childRSForMeasuringHeight,
                    0, 0, flags);

  // Subtract border/padding in vertical axis, to get _just_
  // the effective computed value of the "height" property.
  nscoord childDesiredHeight = childDesiredSize.Height() -
    childRSForMeasuringHeight.ComputedPhysicalBorderPadding().TopBottom();
  childDesiredHeight = std::max(0, childDesiredHeight);

  aFlexItem.SetFlexBaseSizeAndMainSize(childDesiredHeight);
  aFlexItem.SetHadMeasuringReflow();
}

FlexItem::FlexItem(nsIFrame* aChildFrame,
                   float aFlexGrow, float aFlexShrink, nscoord aFlexBaseSize,
                   nscoord aMainMinSize,  nscoord aMainMaxSize,
                   nscoord aCrossMinSize, nscoord aCrossMaxSize,
                   nsMargin aMargin, nsMargin aBorderPadding,
                   const FlexboxAxisTracker& aAxisTracker)
  : mFrame(aChildFrame),
    mFlexGrow(aFlexGrow),
    mFlexShrink(aFlexShrink),
    mBorderPadding(aBorderPadding),
    mMargin(aMargin),
    mMainMinSize(aMainMinSize),
    mMainMaxSize(aMainMaxSize),
    mCrossMinSize(aCrossMinSize),
    mCrossMaxSize(aCrossMaxSize),
    mMainPosn(0),
    mCrossSize(0),
    mCrossPosn(0),
    mAscent(0),
    mShareOfWeightSoFar(0.0f),
    mIsFrozen(false),
    mHadMinViolation(false),
    mHadMaxViolation(false),
    mHadMeasuringReflow(false),
    mIsStretched(false),
    mIsStrut(false),
    mAlignSelf(aChildFrame->StylePosition()->mAlignSelf)
{
  MOZ_ASSERT(mFrame, "expecting a non-null child frame");
  MOZ_ASSERT(mFrame->GetType() != nsGkAtoms::placeholderFrame,
             "placeholder frames should not be treated as flex items");
  MOZ_ASSERT(!(mFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW),
             "out-of-flow frames should not be treated as flex items");

  SetFlexBaseSizeAndMainSize(aFlexBaseSize);

  // Assert that any "auto" margin components are set to 0.
  // (We'll resolve them later; until then, we want to treat them as 0-sized.)
#ifdef DEBUG
  {
    const nsStyleSides& styleMargin = mFrame->StyleMargin()->mMargin;
    NS_FOR_CSS_SIDES(side) {
      if (styleMargin.GetUnit(side) == eStyleUnit_Auto) {
        MOZ_ASSERT(GetMarginComponentForSide(side) == 0,
                   "Someone else tried to resolve our auto margin");
      }
    }
  }
#endif // DEBUG

  // Resolve "align-self: auto" to parent's "align-items" value.
  if (mAlignSelf == NS_STYLE_ALIGN_SELF_AUTO) {
    mAlignSelf =
      mFrame->StyleContext()->GetParent()->StylePosition()->mAlignItems;
  }

  // If the flex item's inline axis is the same as the cross axis, then
  // 'align-self:baseline' is identical to 'flex-start'. If that's the case, we
  // just directly convert our align-self value here, so that we don't have to
  // handle this with special cases elsewhere.
  // Moreover: for the time being (until we support writing-modes),
  // all inline axes are horizontal -- so we can just check if the cross axis
  // is horizontal.
  // FIXME: Once we support writing-mode (vertical text), this IsAxisHorizontal
  // check won't be sufficient anymore -- we'll actually need to compare our
  // inline axis vs. the cross axis.
  if (mAlignSelf == NS_STYLE_ALIGN_ITEMS_BASELINE &&
      IsAxisHorizontal(aAxisTracker.GetCrossAxis())) {
    mAlignSelf = NS_STYLE_ALIGN_ITEMS_FLEX_START;
  }
}

// Simplified constructor for creating a special "strut" FlexItem, for a child
// with visibility:collapse. The strut has 0 main-size, and it only exists to
// impose a minimum cross size on whichever FlexLine it ends up in.
FlexItem::FlexItem(nsIFrame* aChildFrame, nscoord aCrossSize)
  : mFrame(aChildFrame),
    mFlexGrow(0.0f),
    mFlexShrink(0.0f),
    // mBorderPadding uses default constructor,
    // mMargin uses default constructor,
    mFlexBaseSize(0),
    mMainMinSize(0),
    mMainMaxSize(0),
    mCrossMinSize(0),
    mCrossMaxSize(0),
    mMainSize(0),
    mMainPosn(0),
    mCrossSize(aCrossSize),
    mCrossPosn(0),
    mAscent(0),
    mShareOfWeightSoFar(0.0f),
    mIsFrozen(true),
    mHadMinViolation(false),
    mHadMaxViolation(false),
    mHadMeasuringReflow(false),
    mIsStretched(false),
    mIsStrut(true), // (this is the constructor for making struts, after all)
    mAlignSelf(NS_STYLE_ALIGN_ITEMS_FLEX_START)
{
  MOZ_ASSERT(mFrame, "expecting a non-null child frame");
  MOZ_ASSERT(NS_STYLE_VISIBILITY_COLLAPSE ==
             mFrame->StyleVisibility()->mVisible,
             "Should only make struts for children with 'visibility:collapse'");
  MOZ_ASSERT(mFrame->GetType() != nsGkAtoms::placeholderFrame,
             "placeholder frames should not be treated as flex items");
  MOZ_ASSERT(!(mFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW),
             "out-of-flow frames should not be treated as flex items");
}

nscoord
FlexItem::GetBaselineOffsetFromOuterCrossEdge(AxisOrientationType aCrossAxis,
                                              AxisEdgeType aEdge) const
{
  // NOTE: Currently, 'mAscent' (taken from reflow) is an inherently vertical
  // measurement -- it's the distance from the border-top edge of this FlexItem
  // to its baseline. So, we can really only do baseline alignment when the
  // cross axis is vertical. (The FlexItem constructor enforces this when
  // resolving the item's "mAlignSelf" value).
  MOZ_ASSERT(!IsAxisHorizontal(aCrossAxis),
             "Only expecting to be doing baseline computations when the "
             "cross axis is vertical");

  Side sideToMeasureFrom = kAxisOrientationToSidesMap[aCrossAxis][aEdge];

  nscoord marginTopToBaseline = mAscent + mMargin.top;

  if (sideToMeasureFrom == eSideTop) {
    // Measuring from top (normal case): the distance from the margin-box top
    // edge to the baseline is just ascent + margin-top.
    return marginTopToBaseline;
  }

  MOZ_ASSERT(sideToMeasureFrom == eSideBottom,
             "We already checked that we're dealing with a vertical axis, and "
             "we're not using the top side, so that only leaves the bottom...");

  // Measuring from bottom: The distance from the margin-box bottom edge to the
  // baseline is just the margin-box cross size (i.e. outer cross size), minus
  // the already-computed distance from margin-top to baseline.
  return GetOuterCrossSize(aCrossAxis) - marginTopToBaseline;
}

uint32_t
FlexItem::GetNumAutoMarginsInAxis(AxisOrientationType aAxis) const
{
  uint32_t numAutoMargins = 0;
  const nsStyleSides& styleMargin = mFrame->StyleMargin()->mMargin;
  for (uint32_t i = 0; i < eNumAxisEdges; i++) {
    Side side = kAxisOrientationToSidesMap[aAxis][i];
    if (styleMargin.GetUnit(side) == eStyleUnit_Auto) {
      numAutoMargins++;
    }
  }

  // Mostly for clarity:
  MOZ_ASSERT(numAutoMargins <= 2,
             "We're just looking at one item along one dimension, so we "
             "should only have examined 2 margins");

  return numAutoMargins;
}

// Keeps track of our position along a particular axis (where a '0' position
// corresponds to the 'start' edge of that axis).
// This class shouldn't be instantiated directly -- rather, it should only be
// instantiated via its subclasses defined below.
class MOZ_STACK_CLASS PositionTracker {
public:
  // Accessor for the current value of the position that we're tracking.
  inline nscoord GetPosition() const { return mPosition; }
  inline AxisOrientationType GetAxis() const { return mAxis; }

  // Advances our position across the start edge of the given margin, in the
  // axis we're tracking.
  void EnterMargin(const nsMargin& aMargin)
  {
    Side side = kAxisOrientationToSidesMap[mAxis][eAxisEdge_Start];
    mPosition += MarginComponentForSide(aMargin, side);
  }

  // Advances our position across the end edge of the given margin, in the axis
  // we're tracking.
  void ExitMargin(const nsMargin& aMargin)
  {
    Side side = kAxisOrientationToSidesMap[mAxis][eAxisEdge_End];
    mPosition += MarginComponentForSide(aMargin, side);
  }

  // Advances our current position from the start side of a child frame's
  // border-box to the frame's upper or left edge (depending on our axis).
  // (Note that this is a no-op if our axis grows in positive direction.)
  void EnterChildFrame(nscoord aChildFrameSize)
  {
    if (!AxisGrowsInPositiveDirection(mAxis))
      mPosition += aChildFrameSize;
  }

  // Advances our current position from a frame's upper or left border-box edge
  // (whichever is in the axis we're tracking) to the 'end' side of the frame
  // in the axis that we're tracking. (Note that this is a no-op if our axis
  // grows in the negative direction.)
  void ExitChildFrame(nscoord aChildFrameSize)
  {
    if (AxisGrowsInPositiveDirection(mAxis))
      mPosition += aChildFrameSize;
  }

protected:
  // Protected constructor, to be sure we're only instantiated via a subclass.
  PositionTracker(AxisOrientationType aAxis)
    : mPosition(0),
      mAxis(aAxis)
  {}

private:
  // Private copy-constructor, since we don't want any instances of our
  // subclasses to be accidentally copied.
  PositionTracker(const PositionTracker& aOther)
    : mPosition(aOther.mPosition),
      mAxis(aOther.mAxis)
  {}

protected:
  // Member data:
  nscoord mPosition;               // The position we're tracking
  const AxisOrientationType mAxis; // The axis along which we're moving
};

// Tracks our position in the main axis, when we're laying out flex items.
// The "0" position represents the main-start edge of the flex container's
// content-box.
class MOZ_STACK_CLASS MainAxisPositionTracker : public PositionTracker {
public:
  MainAxisPositionTracker(const FlexboxAxisTracker& aAxisTracker,
                          const FlexLine* aLine,
                          uint8_t aJustifyContent,
                          nscoord aContentBoxMainSize);

  ~MainAxisPositionTracker() {
    MOZ_ASSERT(mNumPackingSpacesRemaining == 0,
               "miscounted the number of packing spaces");
    MOZ_ASSERT(mNumAutoMarginsInMainAxis == 0,
               "miscounted the number of auto margins");
  }

  // Advances past the packing space (if any) between two flex items
  void TraversePackingSpace();

  // If aItem has any 'auto' margins in the main axis, this method updates the
  // corresponding values in its margin.
  void ResolveAutoMarginsInMainAxis(FlexItem& aItem);

private:
  nscoord  mPackingSpaceRemaining;
  uint32_t mNumAutoMarginsInMainAxis;
  uint32_t mNumPackingSpacesRemaining;
  uint8_t  mJustifyContent;
};

// Utility class for managing our position along the cross axis along
// the whole flex container (at a higher level than a single line).
// The "0" position represents the cross-start edge of the flex container's
// content-box.
class MOZ_STACK_CLASS CrossAxisPositionTracker : public PositionTracker {
public:
  CrossAxisPositionTracker(FlexLine* aFirstLine,
                           uint8_t aAlignContent,
                           nscoord aContentBoxCrossSize,
                           bool aIsCrossSizeDefinite,
                           const FlexboxAxisTracker& aAxisTracker);

  // Advances past the packing space (if any) between two flex lines
  void TraversePackingSpace();

  // Advances past the given FlexLine
  void TraverseLine(FlexLine& aLine) { mPosition += aLine.GetLineCrossSize(); }

private:
  // Redeclare the frame-related methods from PositionTracker as private with
  // MOZ_DELETE, to be sure (at compile time) that no client code can invoke
  // them. (Unlike the other PositionTracker derived classes, this class here
  // deals with FlexLines, not with individual FlexItems or frames.)
  void EnterMargin(const nsMargin& aMargin) MOZ_DELETE;
  void ExitMargin(const nsMargin& aMargin) MOZ_DELETE;
  void EnterChildFrame(nscoord aChildFrameSize) MOZ_DELETE;
  void ExitChildFrame(nscoord aChildFrameSize) MOZ_DELETE;

  nscoord  mPackingSpaceRemaining;
  uint32_t mNumPackingSpacesRemaining;
  uint8_t  mAlignContent;
};

// Utility class for managing our position along the cross axis, *within* a
// single flex line.
class MOZ_STACK_CLASS SingleLineCrossAxisPositionTracker : public PositionTracker {
public:
  SingleLineCrossAxisPositionTracker(const FlexboxAxisTracker& aAxisTracker);

  void ResolveAutoMarginsInCrossAxis(const FlexLine& aLine,
                                     FlexItem& aItem);

  void EnterAlignPackingSpace(const FlexLine& aLine,
                              const FlexItem& aItem,
                              const FlexboxAxisTracker& aAxisTracker);

  // Resets our position to the cross-start edge of this line.
  inline void ResetPosition() { mPosition = 0; }
};

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsFlexContainerFrame)
  NS_QUERYFRAME_ENTRY(nsFlexContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsFlexContainerFrameSuper)

NS_IMPL_FRAMEARENA_HELPERS(nsFlexContainerFrame)

nsContainerFrame*
NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                         nsStyleContext* aContext)
{
  return new (aPresShell) nsFlexContainerFrame(aContext);
}

//----------------------------------------------------------------------

// nsFlexContainerFrame Method Implementations
// ===========================================

/* virtual */
nsFlexContainerFrame::~nsFlexContainerFrame()
{
}

template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
/* static */ bool
nsFlexContainerFrame::SortChildrenIfNeeded()
{
  if (nsIFrame::IsFrameListSorted<IsLessThanOrEqual>(mFrames)) {
    return false;
  }

  nsIFrame::SortFrameList<IsLessThanOrEqual>(mFrames);
  return true;
}

/* virtual */
nsIAtom*
nsFlexContainerFrame::GetType() const
{
  return nsGkAtoms::flexContainerFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsFlexContainerFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FlexContainer"), aResult);
}
#endif

// Helper for BuildDisplayList, to implement this special-case for flex items
// from the spec:
//    Flex items paint exactly the same as block-level elements in the
//    normal flow, except that 'z-index' values other than 'auto' create
//    a stacking context even if 'position' is 'static'.
// http://www.w3.org/TR/2012/CR-css3-flexbox-20120918/#painting
uint32_t
GetDisplayFlagsForFlexItem(nsIFrame* aFrame)
{
  MOZ_ASSERT(aFrame->IsFlexItem(), "Should only be called on flex items");

  const nsStylePosition* pos = aFrame->StylePosition();
  if (pos->mZIndex.GetUnit() == eStyleUnit_Integer) {
    return nsIFrame::DISPLAY_CHILD_FORCE_STACKING_CONTEXT;
  }
  return nsIFrame::DISPLAY_CHILD_FORCE_PSEUDO_STACKING_CONTEXT;
}

void
nsFlexContainerFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                       const nsRect&           aDirtyRect,
                                       const nsDisplayListSet& aLists)
{
  NS_ASSERTION(
    nsIFrame::IsFrameListSorted<IsOrderLEQWithDOMFallback>(mFrames),
    "Child frames aren't sorted correctly");

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  // Our children are all block-level, so their borders/backgrounds all go on
  // the BlockBorderBackgrounds list.
  nsDisplayListSet childLists(aLists, aLists.BlockBorderBackgrounds());
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    BuildDisplayListForChild(aBuilder, e.get(), aDirtyRect, childLists,
                             GetDisplayFlagsForFlexItem(e.get()));
  }
}

#ifdef DEBUG
// helper for the debugging method below
bool
FrameWantsToBeInAnonymousFlexItem(nsIFrame* aFrame)
{
  // Note: This needs to match the logic in
  // nsCSSFrameConstructor::FrameConstructionItem::NeedsAnonFlexOrGridItem()
  return (aFrame->IsFrameOfType(nsIFrame::eLineParticipant) ||
          nsGkAtoms::placeholderFrame == aFrame->GetType());
}

// Debugging method, to let us assert that our anonymous flex items are
// set up correctly -- in particular, we assert:
//  (1) we don't have any inline non-replaced children
//  (2) we don't have any consecutive anonymous flex items
//  (3) we don't have any empty anonymous flex items
//
// XXXdholbert This matches what nsCSSFrameConstructor currently does, and what
// the spec used to say.  However, the spec has now changed regarding what
// types of content get wrapped in an anonymous flexbox item.  The patch that
// implements those changes (in nsCSSFrameConstructor) will need to change
// this method as well.
void
nsFlexContainerFrame::SanityCheckAnonymousFlexItems() const
{
  bool prevChildWasAnonFlexItem = false;
  for (nsIFrame* child = mFrames.FirstChild(); child;
       child = child->GetNextSibling()) {
    MOZ_ASSERT(!FrameWantsToBeInAnonymousFlexItem(child),
               "frame wants to be inside an anonymous flex item, "
               "but it isn't");
    if (child->StyleContext()->GetPseudo() ==
        nsCSSAnonBoxes::anonymousFlexItem) {
      MOZ_ASSERT(!prevChildWasAnonFlexItem ||
                 HasAnyStateBits(NS_STATE_FLEX_CHILDREN_REORDERED),
                 "two anon flex items in a row (shouldn't happen, unless our "
                 "children have been reordered with the 'order' property)");

      nsIFrame* firstWrappedChild = child->GetFirstPrincipalChild();
      MOZ_ASSERT(firstWrappedChild,
                 "anonymous flex item is empty (shouldn't happen)");
      prevChildWasAnonFlexItem = true;
    } else {
      prevChildWasAnonFlexItem = false;
    }
  }
}
#endif // DEBUG

void
FlexLine::FreezeItemsEarly(bool aIsUsingFlexGrow)
{
  // After we've established the type of flexing we're doing (growing vs.
  // shrinking), and before we try to flex any items, we freeze items that
  // obviously *can't* flex.
  //
  // Quoting the spec:
  //  # Freeze, setting its target main size to its hypothetical main size...
  //  #  - any item that has a flex factor of zero
  //  #  - if using the flex grow factor: any item that has a flex base size
  //  #    greater than its hypothetical main size
  //  #  - if using the flex shrink factor: any item that has a flex base size
  //  #    smaller than its hypothetical main size
  //  http://dev.w3.org/csswg/css-flexbox/#resolve-flexible-lengths-flex-factors
  //
  // (NOTE: At this point, item->GetMainSize() *is* the item's hypothetical
  // main size, since SetFlexBaseSizeAndMainSize() sets it up that way, and the
  // item hasn't had a chance to flex away from that yet.)

  // Since this loop only operates on unfrozen flex items, we can break as
  // soon as we have seen all of them.
  uint32_t numUnfrozenItemsToBeSeen = mNumItems - mNumFrozenItems;
  for (FlexItem* item = mItems.getFirst();
       numUnfrozenItemsToBeSeen > 0; item = item->getNext()) {
    MOZ_ASSERT(item, "numUnfrozenItemsToBeSeen says items remain to be seen");

    if (!item->IsFrozen()) {
      numUnfrozenItemsToBeSeen--;
      bool shouldFreeze = (0.0f == item->GetFlexFactor(aIsUsingFlexGrow));
      if (!shouldFreeze) {
        if (aIsUsingFlexGrow) {
          if (item->GetFlexBaseSize() > item->GetMainSize()) {
            shouldFreeze = true;
          }
        } else { // using flex-shrink
          if (item->GetFlexBaseSize() < item->GetMainSize()) {
            shouldFreeze = true;
          }
        }
      }
      if (shouldFreeze) {
        // Freeze item! (at its hypothetical main size)
        item->Freeze();
        mNumFrozenItems++;
      }
    }
  }
}

// Based on the sign of aTotalViolation, this function freezes a subset of our
// flexible sizes, and restores the remaining ones to their initial pref sizes.
void
FlexLine::FreezeOrRestoreEachFlexibleSize(const nscoord aTotalViolation,
                                          bool aIsFinalIteration)
{
  enum FreezeType {
    eFreezeEverything,
    eFreezeMinViolations,
    eFreezeMaxViolations
  };

  FreezeType freezeType;
  if (aTotalViolation == 0) {
    freezeType = eFreezeEverything;
  } else if (aTotalViolation > 0) {
    freezeType = eFreezeMinViolations;
  } else { // aTotalViolation < 0
    freezeType = eFreezeMaxViolations;
  }

  // Since this loop only operates on unfrozen flex items, we can break as
  // soon as we have seen all of them.
  uint32_t numUnfrozenItemsToBeSeen = mNumItems - mNumFrozenItems;
  for (FlexItem* item = mItems.getFirst();
       numUnfrozenItemsToBeSeen > 0; item = item->getNext()) {
    MOZ_ASSERT(item, "numUnfrozenItemsToBeSeen says items remain to be seen");
    if (!item->IsFrozen()) {
      numUnfrozenItemsToBeSeen--;

      MOZ_ASSERT(!item->HadMinViolation() || !item->HadMaxViolation(),
                 "Can have either min or max violation, but not both");

      if (eFreezeEverything == freezeType ||
          (eFreezeMinViolations == freezeType && item->HadMinViolation()) ||
          (eFreezeMaxViolations == freezeType && item->HadMaxViolation())) {

        MOZ_ASSERT(item->GetMainSize() >= item->GetMainMinSize(),
                   "Freezing item at a size below its minimum");
        MOZ_ASSERT(item->GetMainSize() <= item->GetMainMaxSize(),
                   "Freezing item at a size above its maximum");

        item->Freeze();
        mNumFrozenItems++;
      } else if (MOZ_UNLIKELY(aIsFinalIteration)) {
        // XXXdholbert If & when bug 765861 is fixed, we should upgrade this
        // assertion to be fatal except in documents with enormous lengths.
        NS_ERROR("Final iteration still has unfrozen items, this shouldn't"
                 " happen unless there was nscoord under/overflow.");
        item->Freeze();
        mNumFrozenItems++;
      } // else, we'll reset this item's main size to its flex base size on the
        // next iteration of this algorithm.

      // Clear this item's violation(s), now that we've dealt with them
      item->ClearViolationFlags();
    }
  }
}

void
FlexLine::ResolveFlexibleLengths(nscoord aFlexContainerMainSize)
{
  PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG, ("ResolveFlexibleLengths\n"));

  // Determine whether we're going to be growing or shrinking items.
  const bool isUsingFlexGrow =
    (mTotalOuterHypotheticalMainSize < aFlexContainerMainSize);

  // Do an "early freeze" for flex items that obviously can't flex in the
  // direction we've chosen:
  FreezeItemsEarly(isUsingFlexGrow);

  if (mNumFrozenItems == mNumItems) {
    // All our items are frozen, so we have no flexible lengths to resolve.
    return;
  }
  MOZ_ASSERT(!IsEmpty(), "empty lines should take the early-return above");

  // Subtract space occupied by our items' margins/borders/padding, so we can
  // just be dealing with the space available for our flex items' content
  // boxes.
  nscoord spaceReservedForMarginBorderPadding =
    mTotalOuterHypotheticalMainSize - mTotalInnerHypotheticalMainSize;

  nscoord spaceAvailableForFlexItemsContentBoxes =
    aFlexContainerMainSize - spaceReservedForMarginBorderPadding;

  nscoord origAvailableFreeSpace;
  bool isOrigAvailFreeSpaceInitialized = false;

  // NOTE: I claim that this chunk of the algorithm (the looping part) needs to
  // run the loop at MOST mNumItems times.  This claim should hold up
  // because we'll freeze at least one item on each loop iteration, and once
  // we've run out of items to freeze, there's nothing left to do.  However,
  // in most cases, we'll break out of this loop long before we hit that many
  // iterations.
  for (uint32_t iterationCounter = 0;
       iterationCounter < mNumItems; iterationCounter++) {
    // Set every not-yet-frozen item's used main size to its
    // flex base size, and subtract all the used main sizes from our
    // total amount of space to determine the 'available free space'
    // (positive or negative) to be distributed among our flexible items.
    nscoord availableFreeSpace = spaceAvailableForFlexItemsContentBoxes;
    for (FlexItem* item = mItems.getFirst(); item; item = item->getNext()) {
      if (!item->IsFrozen()) {
        item->SetMainSize(item->GetFlexBaseSize());
      }
      availableFreeSpace -= item->GetMainSize();
    }

    PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG,
           (" available free space = %d\n", availableFreeSpace));


    // The sign of our free space should agree with the type of flexing
    // (grow/shrink) that we're doing (except if we've had integer overflow;
    // then, all bets are off). Any disagreement should've made us use the
    // other type of flexing, or should've been resolved in FreezeItemsEarly.
    // XXXdholbert If & when bug 765861 is fixed, we should upgrade this
    // assertion to be fatal except in documents with enormous lengths.
    NS_ASSERTION((isUsingFlexGrow && availableFreeSpace >= 0) ||
                 (!isUsingFlexGrow && availableFreeSpace <= 0),
                 "availableFreeSpace's sign should match isUsingFlexGrow");

    // If we have any free space available, give each flexible item a portion
    // of availableFreeSpace.
    if (availableFreeSpace != 0) {
      // The first time we do this, we initialize origAvailableFreeSpace.
      if (!isOrigAvailFreeSpaceInitialized) {
        origAvailableFreeSpace = availableFreeSpace;
        isOrigAvailFreeSpaceInitialized = true;
      }

      // STRATEGY: On each item, we compute & store its "share" of the total
      // weight that we've seen so far:
      //   curWeight / weightSum
      //
      // Then, when we go to actually distribute the space (in the next loop),
      // we can simply walk backwards through the elements and give each item
      // its "share" multiplied by the remaining available space.
      //
      // SPECIAL CASE: If the sum of the weights is larger than the
      // maximum representable float (overflowing to infinity), then we can't
      // sensibly divide out proportional shares anymore. In that case, we
      // simply treat the flex item(s) with the largest weights as if
      // their weights were infinite (dwarfing all the others), and we
      // distribute all of the available space among them.
      float weightSum = 0.0f;
      float flexFactorSum = 0.0f;
      float largestWeight = 0.0f;
      uint32_t numItemsWithLargestWeight = 0;

      // Since this loop only operates on unfrozen flex items, we can break as
      // soon as we have seen all of them.
      uint32_t numUnfrozenItemsToBeSeen = mNumItems - mNumFrozenItems;
      for (FlexItem* item = mItems.getFirst();
           numUnfrozenItemsToBeSeen > 0; item = item->getNext()) {
        MOZ_ASSERT(item,
                   "numUnfrozenItemsToBeSeen says items remain to be seen");
        if (!item->IsFrozen()) {
          numUnfrozenItemsToBeSeen--;

          float curWeight = item->GetWeight(isUsingFlexGrow);
          float curFlexFactor = item->GetFlexFactor(isUsingFlexGrow);
          MOZ_ASSERT(curWeight >= 0.0f, "weights are non-negative");
          MOZ_ASSERT(curFlexFactor >= 0.0f, "flex factors are non-negative");

          weightSum += curWeight;
          flexFactorSum += curFlexFactor;

          if (NS_finite(weightSum)) {
            if (curWeight == 0.0f) {
              item->SetShareOfWeightSoFar(0.0f);
            } else {
              item->SetShareOfWeightSoFar(curWeight / weightSum);
            }
          } // else, the sum of weights overflows to infinity, in which
            // case we don't bother with "SetShareOfWeightSoFar" since
            // we know we won't use it. (instead, we'll just give every
            // item with the largest weight an equal share of space.)

          // Update our largest-weight tracking vars
          if (curWeight > largestWeight) {
            largestWeight = curWeight;
            numItemsWithLargestWeight = 1;
          } else if (curWeight == largestWeight) {
            numItemsWithLargestWeight++;
          }
        }
      }

      if (weightSum != 0.0f) {
        MOZ_ASSERT(flexFactorSum != 0.0f,
                   "flex factor sum can't be 0, if a weighted sum "
                   "of its components (weightSum) is nonzero");
        if (flexFactorSum < 1.0f) {
          // Our unfrozen flex items don't want all of the original free space!
          // (Their flex factors add up to something less than 1.)
          // Hence, make sure we don't distribute any more than the portion of
          // our original free space that these items actually want.
          nscoord totalDesiredPortionOfOrigFreeSpace =
            NSToCoordRound(origAvailableFreeSpace * flexFactorSum);

          // Clamp availableFreeSpace to be no larger than that ^^.
          // (using min or max, depending on sign).
          // This should not change the sign of availableFreeSpace (except
          // possibly by setting it to 0), as enforced by this assertion:
          MOZ_ASSERT(totalDesiredPortionOfOrigFreeSpace == 0 ||
                     ((totalDesiredPortionOfOrigFreeSpace > 0) ==
                      (availableFreeSpace > 0)),
                     "When we reduce available free space for flex factors < 1,"
                     "we shouldn't change the sign of the free space...");

          if (availableFreeSpace > 0) {
            availableFreeSpace = std::min(availableFreeSpace,
                                          totalDesiredPortionOfOrigFreeSpace);
          } else {
            availableFreeSpace = std::max(availableFreeSpace,
                                          totalDesiredPortionOfOrigFreeSpace);
          }
        }

        PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG,
               (" Distributing available space:"));
        // Since this loop only operates on unfrozen flex items, we can break as
        // soon as we have seen all of them.
        numUnfrozenItemsToBeSeen = mNumItems - mNumFrozenItems;

        // NOTE: It's important that we traverse our items in *reverse* order
        // here, for correct width distribution according to the items'
        // "ShareOfWeightSoFar" progressively-calculated values.
        for (FlexItem* item = mItems.getLast();
             numUnfrozenItemsToBeSeen > 0; item = item->getPrevious()) {
          MOZ_ASSERT(item,
                     "numUnfrozenItemsToBeSeen says items remain to be seen");
          if (!item->IsFrozen()) {
            numUnfrozenItemsToBeSeen--;

            // To avoid rounding issues, we compute the change in size for this
            // item, and then subtract it from the remaining available space.
            nscoord sizeDelta = 0;
            if (NS_finite(weightSum)) {
              float myShareOfRemainingSpace =
                item->GetShareOfWeightSoFar();

              MOZ_ASSERT(myShareOfRemainingSpace >= 0.0f &&
                         myShareOfRemainingSpace <= 1.0f,
                         "my share should be nonnegative fractional amount");

              if (myShareOfRemainingSpace == 1.0f) {
                // (We special-case 1.0f to avoid float error from converting
                // availableFreeSpace from integer*1.0f --> float --> integer)
                sizeDelta = availableFreeSpace;
              } else if (myShareOfRemainingSpace > 0.0f) {
                sizeDelta = NSToCoordRound(availableFreeSpace *
                                           myShareOfRemainingSpace);
              }
            } else if (item->GetWeight(isUsingFlexGrow) == largestWeight) {
              // Total flexibility is infinite, so we're just distributing
              // the available space equally among the items that are tied for
              // having the largest weight (and this is one of those items).
              sizeDelta =
                NSToCoordRound(availableFreeSpace /
                               float(numItemsWithLargestWeight));
              numItemsWithLargestWeight--;
            }

            availableFreeSpace -= sizeDelta;

            item->SetMainSize(item->GetMainSize() + sizeDelta);
            PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG,
                   ("  child %p receives %d, for a total of %d\n",
                    item, sizeDelta, item->GetMainSize()));
          }
        }
      }
    }

    // Fix min/max violations:
    nscoord totalViolation = 0; // keeps track of adjustments for min/max
    PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG,
           (" Checking for violations:"));

    // Since this loop only operates on unfrozen flex items, we can break as
    // soon as we have seen all of them.
    uint32_t numUnfrozenItemsToBeSeen = mNumItems - mNumFrozenItems;
    for (FlexItem* item = mItems.getFirst();
         numUnfrozenItemsToBeSeen > 0; item = item->getNext()) {
      MOZ_ASSERT(item, "numUnfrozenItemsToBeSeen says items remain to be seen");
      if (!item->IsFrozen()) {
        numUnfrozenItemsToBeSeen--;

        if (item->GetMainSize() < item->GetMainMinSize()) {
          // min violation
          totalViolation += item->GetMainMinSize() - item->GetMainSize();
          item->SetMainSize(item->GetMainMinSize());
          item->SetHadMinViolation();
        } else if (item->GetMainSize() > item->GetMainMaxSize()) {
          // max violation
          totalViolation += item->GetMainMaxSize() - item->GetMainSize();
          item->SetMainSize(item->GetMainMaxSize());
          item->SetHadMaxViolation();
        }
      }
    }

    FreezeOrRestoreEachFlexibleSize(totalViolation,
                                    iterationCounter + 1 == mNumItems);

    PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG,
           (" Total violation: %d\n", totalViolation));

    if (mNumFrozenItems == mNumItems) {
      break;
    }

    MOZ_ASSERT(totalViolation != 0,
               "Zero violation should've made us freeze all items & break");
  }

#ifdef DEBUG
  // Post-condition: all items should've been frozen.
  // Make sure the counts match:
  MOZ_ASSERT(mNumFrozenItems == mNumItems, "All items should be frozen");

  // For good measure, check each item directly, in case our counts are busted:
  for (const FlexItem* item = mItems.getFirst(); item; item = item->getNext()) {
    MOZ_ASSERT(item->IsFrozen(), "All items should be frozen");
  }
#endif // DEBUG
}

MainAxisPositionTracker::
  MainAxisPositionTracker(const FlexboxAxisTracker& aAxisTracker,
                          const FlexLine* aLine,
                          uint8_t aJustifyContent,
                          nscoord aContentBoxMainSize)
  : PositionTracker(aAxisTracker.GetMainAxis()),
    mPackingSpaceRemaining(aContentBoxMainSize), // we chip away at this below
    mNumAutoMarginsInMainAxis(0),
    mNumPackingSpacesRemaining(0),
    mJustifyContent(aJustifyContent)
{
  // mPackingSpaceRemaining is initialized to the container's main size.  Now
  // we'll subtract out the main sizes of our flex items, so that it ends up
  // with the *actual* amount of packing space.
  for (const FlexItem* item = aLine->GetFirstItem(); item;
       item = item->getNext()) {
    mPackingSpaceRemaining -= item->GetOuterMainSize(mAxis);
    mNumAutoMarginsInMainAxis += item->GetNumAutoMarginsInAxis(mAxis);
  }

  if (mPackingSpaceRemaining <= 0) {
    // No available packing space to use for resolving auto margins.
    mNumAutoMarginsInMainAxis = 0;
  }

  // If packing space is negative, 'space-between' behaves like 'flex-start',
  // and 'space-around' behaves like 'center'. In those cases, it's simplest to
  // just pretend we have a different 'justify-content' value and share code.
  if (mPackingSpaceRemaining < 0) {
    if (mJustifyContent == NS_STYLE_JUSTIFY_CONTENT_SPACE_BETWEEN) {
      mJustifyContent = NS_STYLE_JUSTIFY_CONTENT_FLEX_START;
    } else if (mJustifyContent == NS_STYLE_JUSTIFY_CONTENT_SPACE_AROUND) {
      mJustifyContent = NS_STYLE_JUSTIFY_CONTENT_CENTER;
    }
  }

  // If our main axis is (internally) reversed, swap the justify-content
  // "flex-start" and "flex-end" behaviors:
  if (aAxisTracker.AreAxesInternallyReversed()) {
    if (mJustifyContent == NS_STYLE_JUSTIFY_CONTENT_FLEX_START) {
      mJustifyContent = NS_STYLE_JUSTIFY_CONTENT_FLEX_END;
    } else if (mJustifyContent == NS_STYLE_JUSTIFY_CONTENT_FLEX_END) {
      mJustifyContent = NS_STYLE_JUSTIFY_CONTENT_FLEX_START;
    }
  }

  // Figure out how much space we'll set aside for auto margins or
  // packing spaces, and advance past any leading packing-space.
  if (mNumAutoMarginsInMainAxis == 0 &&
      mPackingSpaceRemaining != 0 &&
      !aLine->IsEmpty()) {
    switch (mJustifyContent) {
      case NS_STYLE_JUSTIFY_CONTENT_FLEX_START:
        // All packing space should go at the end --> nothing to do here.
        break;
      case NS_STYLE_JUSTIFY_CONTENT_FLEX_END:
        // All packing space goes at the beginning
        mPosition += mPackingSpaceRemaining;
        break;
      case NS_STYLE_JUSTIFY_CONTENT_CENTER:
        // Half the packing space goes at the beginning
        mPosition += mPackingSpaceRemaining / 2;
        break;
      case NS_STYLE_JUSTIFY_CONTENT_SPACE_BETWEEN:
        MOZ_ASSERT(mPackingSpaceRemaining >= 0,
                   "negative packing space should make us use 'flex-start' "
                   "instead of 'space-between'");
        // 1 packing space between each flex item, no packing space at ends.
        mNumPackingSpacesRemaining = aLine->NumItems() - 1;
        break;
      case NS_STYLE_JUSTIFY_CONTENT_SPACE_AROUND:
        MOZ_ASSERT(mPackingSpaceRemaining >= 0,
                   "negative packing space should make us use 'center' "
                   "instead of 'space-around'");
        // 1 packing space between each flex item, plus half a packing space
        // at beginning & end.  So our number of full packing-spaces is equal
        // to the number of flex items.
        mNumPackingSpacesRemaining = aLine->NumItems();
        if (mNumPackingSpacesRemaining > 0) {
          // The edges (start/end) share one full packing space
          nscoord totalEdgePackingSpace =
            mPackingSpaceRemaining / mNumPackingSpacesRemaining;

          // ...and we'll use half of that right now, at the start
          mPosition += totalEdgePackingSpace / 2;
          // ...but we need to subtract all of it right away, so that we won't
          // hand out any of it to intermediate packing spaces.
          mPackingSpaceRemaining -= totalEdgePackingSpace;
          mNumPackingSpacesRemaining--;
        }
        break;
      default:
        MOZ_CRASH("Unexpected justify-content value");
    }
  }

  MOZ_ASSERT(mNumPackingSpacesRemaining == 0 ||
             mNumAutoMarginsInMainAxis == 0,
             "extra space should either go to packing space or to "
             "auto margins, but not to both");
}

void
MainAxisPositionTracker::ResolveAutoMarginsInMainAxis(FlexItem& aItem)
{
  if (mNumAutoMarginsInMainAxis) {
    const nsStyleSides& styleMargin = aItem.Frame()->StyleMargin()->mMargin;
    for (uint32_t i = 0; i < eNumAxisEdges; i++) {
      Side side = kAxisOrientationToSidesMap[mAxis][i];
      if (styleMargin.GetUnit(side) == eStyleUnit_Auto) {
        // NOTE: This integer math will skew the distribution of remainder
        // app-units towards the end, which is fine.
        nscoord curAutoMarginSize =
          mPackingSpaceRemaining / mNumAutoMarginsInMainAxis;

        MOZ_ASSERT(aItem.GetMarginComponentForSide(side) == 0,
                   "Expecting auto margins to have value '0' before we "
                   "resolve them");
        aItem.SetMarginComponentForSide(side, curAutoMarginSize);

        mNumAutoMarginsInMainAxis--;
        mPackingSpaceRemaining -= curAutoMarginSize;
      }
    }
  }
}

void
MainAxisPositionTracker::TraversePackingSpace()
{
  if (mNumPackingSpacesRemaining) {
    MOZ_ASSERT(mJustifyContent == NS_STYLE_JUSTIFY_CONTENT_SPACE_BETWEEN ||
               mJustifyContent == NS_STYLE_JUSTIFY_CONTENT_SPACE_AROUND,
               "mNumPackingSpacesRemaining only applies for "
               "space-between/space-around");

    MOZ_ASSERT(mPackingSpaceRemaining >= 0,
               "ran out of packing space earlier than we expected");

    // NOTE: This integer math will skew the distribution of remainder
    // app-units towards the end, which is fine.
    nscoord curPackingSpace =
      mPackingSpaceRemaining / mNumPackingSpacesRemaining;

    mPosition += curPackingSpace;
    mNumPackingSpacesRemaining--;
    mPackingSpaceRemaining -= curPackingSpace;
  }
}

CrossAxisPositionTracker::
  CrossAxisPositionTracker(FlexLine* aFirstLine,
                           uint8_t aAlignContent,
                           nscoord aContentBoxCrossSize,
                           bool aIsCrossSizeDefinite,
                           const FlexboxAxisTracker& aAxisTracker)
  : PositionTracker(aAxisTracker.GetCrossAxis()),
    mPackingSpaceRemaining(0),
    mNumPackingSpacesRemaining(0),
    mAlignContent(aAlignContent)
{
  MOZ_ASSERT(aFirstLine, "null first line pointer");

  if (aIsCrossSizeDefinite && !aFirstLine->getNext()) {
    // "If the flex container has only a single line (even if it's a
    // multi-line flex container) and has a definite cross size, the cross
    // size of the flex line is the flex container's inner cross size."
    // SOURCE: http://dev.w3.org/csswg/css-flexbox/#algo-line-break
    // NOTE: This means (by definition) that there's no packing space, which
    // means we don't need to be concerned with "align-conent" at all and we
    // can return early. This is handy, because this is the usual case (for
    // single-line flexbox).
    aFirstLine->SetLineCrossSize(aContentBoxCrossSize);
    return;
  }

  // NOTE: The rest of this function should essentially match
  // MainAxisPositionTracker's constructor, though with FlexLines instead of
  // FlexItems, and with the additional value "stretch" (and of course with
  // cross sizes instead of main sizes.)

  // Figure out how much packing space we have (container's cross size minus
  // all the lines' cross sizes).  Also, share this loop to count how many
  // lines we have. (We need that count in some cases below.)
  mPackingSpaceRemaining = aContentBoxCrossSize;
  uint32_t numLines = 0;
  for (FlexLine* line = aFirstLine; line; line = line->getNext()) {
    mPackingSpaceRemaining -= line->GetLineCrossSize();
    numLines++;
  }

  // If packing space is negative, 'space-between' and 'stretch' behave like
  // 'flex-start', and 'space-around' behaves like 'center'. In those cases,
  // it's simplest to just pretend we have a different 'align-content' value
  // and share code.
  if (mPackingSpaceRemaining < 0) {
    if (mAlignContent == NS_STYLE_ALIGN_CONTENT_SPACE_BETWEEN ||
        mAlignContent == NS_STYLE_ALIGN_CONTENT_STRETCH) {
      mAlignContent = NS_STYLE_ALIGN_CONTENT_FLEX_START;
    } else if (mAlignContent == NS_STYLE_ALIGN_CONTENT_SPACE_AROUND) {
      mAlignContent = NS_STYLE_ALIGN_CONTENT_CENTER;
    }
  }

  // If our cross axis is (internally) reversed, swap the align-content
  // "flex-start" and "flex-end" behaviors:
  if (aAxisTracker.AreAxesInternallyReversed()) {
    if (mAlignContent == NS_STYLE_ALIGN_CONTENT_FLEX_START) {
      mAlignContent = NS_STYLE_ALIGN_CONTENT_FLEX_END;
    } else if (mAlignContent == NS_STYLE_ALIGN_CONTENT_FLEX_END) {
      mAlignContent = NS_STYLE_ALIGN_CONTENT_FLEX_START;
    }
  }

  // Figure out how much space we'll set aside for packing spaces, and advance
  // past any leading packing-space.
  if (mPackingSpaceRemaining != 0) {
    switch (mAlignContent) {
      case NS_STYLE_ALIGN_CONTENT_FLEX_START:
        // All packing space should go at the end --> nothing to do here.
        break;
      case NS_STYLE_ALIGN_CONTENT_FLEX_END:
        // All packing space goes at the beginning
        mPosition += mPackingSpaceRemaining;
        break;
      case NS_STYLE_ALIGN_CONTENT_CENTER:
        // Half the packing space goes at the beginning
        mPosition += mPackingSpaceRemaining / 2;
        break;
      case NS_STYLE_ALIGN_CONTENT_SPACE_BETWEEN:
        MOZ_ASSERT(mPackingSpaceRemaining >= 0,
                   "negative packing space should make us use 'flex-start' "
                   "instead of 'space-between'");
        // 1 packing space between each flex line, no packing space at ends.
        mNumPackingSpacesRemaining = numLines - 1;
        break;
      case NS_STYLE_ALIGN_CONTENT_SPACE_AROUND: {
        MOZ_ASSERT(mPackingSpaceRemaining >= 0,
                   "negative packing space should make us use 'center' "
                   "instead of 'space-around'");
        // 1 packing space between each flex line, plus half a packing space
        // at beginning & end.  So our number of full packing-spaces is equal
        // to the number of flex lines.
        mNumPackingSpacesRemaining = numLines;
        // The edges (start/end) share one full packing space
        nscoord totalEdgePackingSpace =
          mPackingSpaceRemaining / mNumPackingSpacesRemaining;

        // ...and we'll use half of that right now, at the start
        mPosition += totalEdgePackingSpace / 2;
        // ...but we need to subtract all of it right away, so that we won't
        // hand out any of it to intermediate packing spaces.
        mPackingSpaceRemaining -= totalEdgePackingSpace;
        mNumPackingSpacesRemaining--;
        break;
      }
      case NS_STYLE_ALIGN_CONTENT_STRETCH: {
        // Split space equally between the lines:
        MOZ_ASSERT(mPackingSpaceRemaining > 0,
                   "negative packing space should make us use 'flex-start' "
                   "instead of 'stretch' (and we shouldn't bother with this "
                   "code if we have 0 packing space)");

        uint32_t numLinesLeft = numLines;
        for (FlexLine* line = aFirstLine; line; line = line->getNext()) {
          // Our share is the amount of space remaining, divided by the number
          // of lines remainig.
          MOZ_ASSERT(numLinesLeft > 0, "miscalculated num lines");
          nscoord shareOfExtraSpace = mPackingSpaceRemaining / numLinesLeft;
          nscoord newSize = line->GetLineCrossSize() + shareOfExtraSpace;
          line->SetLineCrossSize(newSize);

          mPackingSpaceRemaining -= shareOfExtraSpace;
          numLinesLeft--;
        }
        MOZ_ASSERT(numLinesLeft == 0, "miscalculated num lines");
        break;
      }
      default:
        MOZ_CRASH("Unexpected align-content value");
    }
  }
}

void
CrossAxisPositionTracker::TraversePackingSpace()
{
  if (mNumPackingSpacesRemaining) {
    MOZ_ASSERT(mAlignContent == NS_STYLE_ALIGN_CONTENT_SPACE_BETWEEN ||
               mAlignContent == NS_STYLE_ALIGN_CONTENT_SPACE_AROUND,
               "mNumPackingSpacesRemaining only applies for "
               "space-between/space-around");

    MOZ_ASSERT(mPackingSpaceRemaining >= 0,
               "ran out of packing space earlier than we expected");

    // NOTE: This integer math will skew the distribution of remainder
    // app-units towards the end, which is fine.
    nscoord curPackingSpace =
      mPackingSpaceRemaining / mNumPackingSpacesRemaining;

    mPosition += curPackingSpace;
    mNumPackingSpacesRemaining--;
    mPackingSpaceRemaining -= curPackingSpace;
  }
}

SingleLineCrossAxisPositionTracker::
  SingleLineCrossAxisPositionTracker(const FlexboxAxisTracker& aAxisTracker)
  : PositionTracker(aAxisTracker.GetCrossAxis())
{
}

void
FlexLine::ComputeCrossSizeAndBaseline(const FlexboxAxisTracker& aAxisTracker)
{
  nscoord crossStartToFurthestBaseline = nscoord_MIN;
  nscoord crossEndToFurthestBaseline = nscoord_MIN;
  nscoord largestOuterCrossSize = 0;
  for (const FlexItem* item = mItems.getFirst(); item; item = item->getNext()) {
    nscoord curOuterCrossSize =
      item->GetOuterCrossSize(aAxisTracker.GetCrossAxis());

    if (item->GetAlignSelf() == NS_STYLE_ALIGN_ITEMS_BASELINE &&
        item->GetNumAutoMarginsInAxis(aAxisTracker.GetCrossAxis()) == 0) {
      // FIXME: Once we support "writing-mode", we'll have to do baseline
      // alignment in vertical flex containers here (w/ horizontal cross-axes).

      // Find distance from our item's cross-start and cross-end margin-box
      // edges to its baseline.
      //
      // Here's a diagram of a flex-item that we might be doing this on.
      // "mmm" is the margin-box, "bbb" is the border-box. The bottom of
      // the text "BASE" is the baseline.
      //
      // ---(cross-start)---
      //                ___              ___            ___
      //   mmmmmmmmmmmm  |                |margin-start  |
      //   m          m  |               _|_   ___       |
      //   m bbbbbbbb m  |curOuterCrossSize     |        |crossStartToBaseline
      //   m b      b m  |                      |ascent  |
      //   m b BASE b m  |                     _|_      _|_
      //   m b      b m  |                               |
      //   m bbbbbbbb m  |                               |crossEndToBaseline
      //   m          m  |                               |
      //   mmmmmmmmmmmm _|_                             _|_
      //
      // ---(cross-end)---
      //
      // We already have the curOuterCrossSize, margin-start, and the ascent.
      // * We can get crossStartToBaseline by adding margin-start + ascent.
      // * If we subtract that from the curOuterCrossSize, we get
      //   crossEndToBaseline.

      nscoord crossStartToBaseline =
        item->GetBaselineOffsetFromOuterCrossEdge(aAxisTracker.GetCrossAxis(),
                                                  eAxisEdge_Start);
      nscoord crossEndToBaseline = curOuterCrossSize - crossStartToBaseline;

      // Now, update our "largest" values for these (across all the flex items
      // in this flex line), so we can use them in computing the line's cross
      // size below:
      crossStartToFurthestBaseline = std::max(crossStartToFurthestBaseline,
                                              crossStartToBaseline);
      crossEndToFurthestBaseline = std::max(crossEndToFurthestBaseline,
                                            crossEndToBaseline);
    } else {
      largestOuterCrossSize = std::max(largestOuterCrossSize, curOuterCrossSize);
    }
  }

  // The line's baseline offset is the distance from the line's edge (start or
  // end, depending on whether we've flipped the axes) to the furthest
  // item-baseline. The item(s) with that baseline will be exactly aligned with
  // the line's edge.
  mBaselineOffset = aAxisTracker.AreAxesInternallyReversed() ?
    crossEndToFurthestBaseline : crossStartToFurthestBaseline;

  // The line's cross-size is the larger of:
  //  (a) [largest cross-start-to-baseline + largest baseline-to-cross-end] of
  //      all baseline-aligned items with no cross-axis auto margins...
  // and
  //  (b) largest cross-size of all other children.
  mLineCrossSize = std::max(crossStartToFurthestBaseline +
                            crossEndToFurthestBaseline,
                            largestOuterCrossSize);
}

void
FlexItem::ResolveStretchedCrossSize(nscoord aLineCrossSize,
                                    const FlexboxAxisTracker& aAxisTracker)
{
  AxisOrientationType crossAxis = aAxisTracker.GetCrossAxis();
  // We stretch IFF we are align-self:stretch, have no auto margins in
  // cross axis, and have cross-axis size property == "auto". If any of those
  // conditions don't hold up, we won't stretch.
  if (mAlignSelf != NS_STYLE_ALIGN_ITEMS_STRETCH ||
      GetNumAutoMarginsInAxis(crossAxis) != 0 ||
      eStyleUnit_Auto != GetSizePropertyForAxis(mFrame, crossAxis).GetUnit()) {
    return;
  }

  // If we've already been stretched, we can bail out early, too.
  // No need to redo the calculation.
  if (mIsStretched) {
    return;
  }

  // Reserve space for margins & border & padding, and then use whatever
  // remains as our item's cross-size (clamped to its min/max range).
  nscoord stretchedSize = aLineCrossSize -
    GetMarginBorderPaddingSizeInAxis(crossAxis);

  stretchedSize = NS_CSS_MINMAX(stretchedSize, mCrossMinSize, mCrossMaxSize);

  // Update the cross-size & make a note that it's stretched, so we know to
  // override the reflow state's computed cross-size in our final reflow.
  SetCrossSize(stretchedSize);
  mIsStretched = true;
}

void
SingleLineCrossAxisPositionTracker::
  ResolveAutoMarginsInCrossAxis(const FlexLine& aLine,
                                FlexItem& aItem)
{
  // Subtract the space that our item is already occupying, to see how much
  // space (if any) is available for its auto margins.
  nscoord spaceForAutoMargins = aLine.GetLineCrossSize() -
    aItem.GetOuterCrossSize(mAxis);

  if (spaceForAutoMargins <= 0) {
    return; // No available space  --> nothing to do
  }

  uint32_t numAutoMargins = aItem.GetNumAutoMarginsInAxis(mAxis);
  if (numAutoMargins == 0) {
    return; // No auto margins --> nothing to do.
  }

  // OK, we have at least one auto margin and we have some available space.
  // Give each auto margin a share of the space.
  const nsStyleSides& styleMargin = aItem.Frame()->StyleMargin()->mMargin;
  for (uint32_t i = 0; i < eNumAxisEdges; i++) {
    Side side = kAxisOrientationToSidesMap[mAxis][i];
    if (styleMargin.GetUnit(side) == eStyleUnit_Auto) {
      MOZ_ASSERT(aItem.GetMarginComponentForSide(side) == 0,
                 "Expecting auto margins to have value '0' before we "
                 "update them");

      // NOTE: integer divison is fine here; numAutoMargins is either 1 or 2.
      // If it's 2 & spaceForAutoMargins is odd, 1st margin gets smaller half.
      nscoord curAutoMarginSize = spaceForAutoMargins / numAutoMargins;
      aItem.SetMarginComponentForSide(side, curAutoMarginSize);
      numAutoMargins--;
      spaceForAutoMargins -= curAutoMarginSize;
    }
  }
}

void
SingleLineCrossAxisPositionTracker::
  EnterAlignPackingSpace(const FlexLine& aLine,
                         const FlexItem& aItem,
                         const FlexboxAxisTracker& aAxisTracker)
{
  // We don't do align-self alignment on items that have auto margins
  // in the cross axis.
  if (aItem.GetNumAutoMarginsInAxis(mAxis)) {
    return;
  }

  uint8_t alignSelf = aItem.GetAlignSelf();
  // NOTE: 'stretch' behaves like 'flex-start' once we've stretched any
  // auto-sized items (which we've already done).
  if (alignSelf == NS_STYLE_ALIGN_ITEMS_STRETCH) {
    alignSelf = NS_STYLE_ALIGN_ITEMS_FLEX_START;
  }

  // If our cross axis is (internally) reversed, swap the align-self
  // "flex-start" and "flex-end" behaviors:
  if (aAxisTracker.AreAxesInternallyReversed()) {
    if (alignSelf == NS_STYLE_ALIGN_ITEMS_FLEX_START) {
      alignSelf = NS_STYLE_ALIGN_ITEMS_FLEX_END;
    } else if (alignSelf == NS_STYLE_ALIGN_ITEMS_FLEX_END) {
      alignSelf = NS_STYLE_ALIGN_ITEMS_FLEX_START;
    }
  }

  switch (alignSelf) {
    case NS_STYLE_ALIGN_ITEMS_FLEX_START:
      // No space to skip over -- we're done.
      break;
    case NS_STYLE_ALIGN_ITEMS_FLEX_END:
      mPosition += aLine.GetLineCrossSize() - aItem.GetOuterCrossSize(mAxis);
      break;
    case NS_STYLE_ALIGN_ITEMS_CENTER:
      // Note: If cross-size is odd, the "after" space will get the extra unit.
      mPosition +=
        (aLine.GetLineCrossSize() - aItem.GetOuterCrossSize(mAxis)) / 2;
      break;
    case NS_STYLE_ALIGN_ITEMS_BASELINE: {
      // Normally, baseline-aligned items are collectively aligned with the
      // line's cross-start edge; however, if our cross axis is (internally)
      // reversed, we instead align them with the cross-end edge.
      nscoord itemBaselineOffset =
        aItem.GetBaselineOffsetFromOuterCrossEdge(mAxis,
          aAxisTracker.AreAxesInternallyReversed() ?
          eAxisEdge_End : eAxisEdge_Start);

      nscoord lineBaselineOffset = aLine.GetBaselineOffset();

      NS_ASSERTION(lineBaselineOffset >= itemBaselineOffset,
                   "failed at finding largest baseline offset");

      // How much do we need to adjust our position (from the line edge),
      // to get the item's baseline to hit the line's baseline offset:
      nscoord baselineDiff = lineBaselineOffset - itemBaselineOffset;

      if (aAxisTracker.AreAxesInternallyReversed()) {
        // Advance to align item w/ line's flex-end edge (as in FLEX_END case):
        mPosition += aLine.GetLineCrossSize() - aItem.GetOuterCrossSize(mAxis);
        // ...and step *back* by the baseline adjustment:
        mPosition -= baselineDiff;
      } else {
        // mPosition is already at line's flex-start edge.
        // From there, we step *forward* by the baseline adjustment:
        mPosition += baselineDiff;
      }
      break;
    }
    default:
      NS_NOTREACHED("Unexpected align-self value");
      break;
  }
}

FlexboxAxisTracker::FlexboxAxisTracker(
  nsFlexContainerFrame* aFlexContainerFrame)
  : mAreAxesInternallyReversed(false)
{
  const nsStylePosition* pos = aFlexContainerFrame->StylePosition();
  uint32_t flexDirection = pos->mFlexDirection;
  uint32_t cssDirection =
    aFlexContainerFrame->StyleVisibility()->mDirection;

  MOZ_ASSERT(cssDirection == NS_STYLE_DIRECTION_LTR ||
             cssDirection == NS_STYLE_DIRECTION_RTL,
             "Unexpected computed value for 'direction' property");
  // (Not asserting for flexDirection here; it's checked by the switch below.)

  // These are defined according to writing-modes' definitions of
  // start/end (for the inline dimension) and before/after (for the block
  // dimension), here:
  //   http://www.w3.org/TR/css3-writing-modes/#logical-directions
  // (NOTE: I'm intentionally not calling this "inlineAxis"/"blockAxis", since
  // those terms have explicit definition in the writing-modes spec, which are
  // the opposite of how I'd be using them here.)
  // XXXdholbert Once we support the 'writing-mode' property, use its value
  // here to further customize inlineDimension & blockDimension.

  // Inline dimension ("start-to-end"):
  AxisOrientationType inlineDimension =
    cssDirection == NS_STYLE_DIRECTION_RTL ? eAxis_RL : eAxis_LR;

  // Block dimension ("before-to-after"):
  AxisOrientationType blockDimension = eAxis_TB;

  // Determine main axis:
  switch (flexDirection) {
    case NS_STYLE_FLEX_DIRECTION_ROW:
      mMainAxis = inlineDimension;
      break;
    case NS_STYLE_FLEX_DIRECTION_ROW_REVERSE:
      mMainAxis = GetReverseAxis(inlineDimension);
      break;
    case NS_STYLE_FLEX_DIRECTION_COLUMN:
      mMainAxis = blockDimension;
      break;
    case NS_STYLE_FLEX_DIRECTION_COLUMN_REVERSE:
      mMainAxis = GetReverseAxis(blockDimension);
      break;
    default:
      MOZ_CRASH("Unexpected computed value for 'flex-flow' property");
  }

  // Determine cross axis:
  // (This is set up so that a bogus |flexDirection| value will
  // give us blockDimension.
  if (flexDirection == NS_STYLE_FLEX_DIRECTION_COLUMN ||
      flexDirection == NS_STYLE_FLEX_DIRECTION_COLUMN_REVERSE) {
    mCrossAxis = inlineDimension;
  } else {
    mCrossAxis = blockDimension;
  }

  // "flex-wrap: wrap-reverse" reverses our cross axis.
  if (pos->mFlexWrap == NS_STYLE_FLEX_WRAP_WRAP_REVERSE) {
    mCrossAxis = GetReverseAxis(mCrossAxis);
  }

  // Master switch to enable/disable bug 983427's code for reversing our axes
  // and reversing some logic, to avoid reflowing children in bottom-to-top
  // order. (This switch can be removed eventually, but for now, it allows
  // this special-case code path to be compared against the normal code path.)
  static bool sPreventBottomToTopChildOrdering = true;

  if (sPreventBottomToTopChildOrdering) {
    // If either axis is bottom-to-top, we flip both axes (and set a flag
    // so that we can flip some logic to make the reversal transparent).
    if (eAxis_BT == mMainAxis || eAxis_BT == mCrossAxis) {
      mMainAxis = GetReverseAxis(mMainAxis);
      mCrossAxis = GetReverseAxis(mCrossAxis);
      mAreAxesInternallyReversed = true;
    }
  }

  MOZ_ASSERT(IsAxisHorizontal(mMainAxis) != IsAxisHorizontal(mCrossAxis),
             "main & cross axes should be in different dimensions");
}

// Allocates a new FlexLine, adds it to the given LinkedList (at the front or
// back depending on aShouldInsertAtFront), and returns a pointer to it.
static FlexLine*
AddNewFlexLineToList(LinkedList<FlexLine>& aLines,
                     bool aShouldInsertAtFront)
{
  FlexLine* newLine = new FlexLine();
  if (aShouldInsertAtFront) {
    aLines.insertFront(newLine);
  } else {
    aLines.insertBack(newLine);
  }
  return newLine;
}

void
nsFlexContainerFrame::GenerateFlexLines(
  nsPresContext* aPresContext,
  const nsHTMLReflowState& aReflowState,
  nscoord aContentBoxMainSize,
  nscoord aAvailableHeightForContent,
  const nsTArray<StrutInfo>& aStruts,
  const FlexboxAxisTracker& aAxisTracker,
  LinkedList<FlexLine>& aLines)
{
  MOZ_ASSERT(aLines.isEmpty(), "Expecting outparam to start out empty");

  const bool isSingleLine =
    NS_STYLE_FLEX_WRAP_NOWRAP == aReflowState.mStylePosition->mFlexWrap;

  // If we're transparently reversing axes, then we'll need to link up our
  // FlexItems and FlexLines in the reverse order, so that the rest of flex
  // layout (with flipped axes) will still produce the correct result.
  // Here, we declare a convenience bool that we'll pass when adding a new
  // FlexLine or FlexItem, to make us insert it at the beginning of its list
  // (so the list ends up reversed).
  const bool shouldInsertAtFront = aAxisTracker.AreAxesInternallyReversed();

  // We have at least one FlexLine. Even an empty flex container has a single
  // (empty) flex line.
  FlexLine* curLine = AddNewFlexLineToList(aLines, shouldInsertAtFront);

  nscoord wrapThreshold;
  if (isSingleLine) {
    // Not wrapping. Set threshold to sentinel value that tells us not to wrap.
    wrapThreshold = NS_UNCONSTRAINEDSIZE;
  } else {
    // Wrapping! Set wrap threshold to flex container's content-box main-size.
    wrapThreshold = aContentBoxMainSize;

    // If the flex container doesn't have a definite content-box main-size
    // (e.g. if we're 'height:auto'), make sure we at least wrap when we hit
    // its max main-size.
    if (wrapThreshold == NS_UNCONSTRAINEDSIZE) {
      const nscoord flexContainerMaxMainSize =
        GET_MAIN_COMPONENT(aAxisTracker,
                           aReflowState.ComputedMaxWidth(),
                           aReflowState.ComputedMaxHeight());

      wrapThreshold = flexContainerMaxMainSize;
    }

    // Also: if we're vertical and paginating, we may need to wrap sooner
    // (before we run off the end of the page)
    if (!IsAxisHorizontal(aAxisTracker.GetMainAxis()) &&
        aAvailableHeightForContent != NS_UNCONSTRAINEDSIZE) {
      wrapThreshold = std::min(wrapThreshold, aAvailableHeightForContent);
    }
  }

  // Tracks the index of the next strut, in aStruts (and when this hits
  // aStruts.Length(), that means there are no more struts):
  uint32_t nextStrutIdx = 0;

  // Overall index of the current flex item in the flex container. (This gets
  // checked against entries in aStruts.)
  uint32_t itemIdxInContainer = 0;

  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    nsIFrame* childFrame = e.get();

    // Honor "page-break-before", if we're multi-line and this line isn't empty:
    if (!isSingleLine && !curLine->IsEmpty() &&
        childFrame->StyleDisplay()->mBreakBefore) {
      curLine = AddNewFlexLineToList(aLines, shouldInsertAtFront);
    }

    nsAutoPtr<FlexItem> item;
    if (nextStrutIdx < aStruts.Length() &&
        aStruts[nextStrutIdx].mItemIdx == itemIdxInContainer) {

      // Use the simplified "strut" FlexItem constructor:
      item = new FlexItem(childFrame, aStruts[nextStrutIdx].mStrutCrossSize);
      nextStrutIdx++;
    } else {
      item = GenerateFlexItemForChild(aPresContext, childFrame,
                                      aReflowState, aAxisTracker);

      ResolveFlexItemMaxContentSizing(aPresContext, *item,
                                      aReflowState, aAxisTracker);
    }

    nscoord itemInnerHypotheticalMainSize = item->GetMainSize();
    nscoord itemOuterHypotheticalMainSize =
      item->GetOuterMainSize(aAxisTracker.GetMainAxis());

    // Check if we need to wrap |item| to a new line
    // (i.e. check if its outer hypothetical main size pushes our line over
    // the threshold)
    if (wrapThreshold != NS_UNCONSTRAINEDSIZE &&
        !curLine->IsEmpty() && // No need to wrap at start of a line.
        wrapThreshold < (curLine->GetTotalOuterHypotheticalMainSize() +
                         itemOuterHypotheticalMainSize)) {
      curLine = AddNewFlexLineToList(aLines, shouldInsertAtFront);
    }

    // Add item to current flex line (and update the line's bookkeeping about
    // how large its items collectively are).
    curLine->AddItem(item.forget(), shouldInsertAtFront,
                     itemInnerHypotheticalMainSize,
                     itemOuterHypotheticalMainSize);

    // Honor "page-break-after", if we're multi-line and have more children:
    if (!isSingleLine && childFrame->GetNextSibling() &&
        childFrame->StyleDisplay()->mBreakAfter) {
      curLine = AddNewFlexLineToList(aLines, shouldInsertAtFront);
    }
    itemIdxInContainer++;
  }
}

// Retrieves the content-box main-size of our flex container from the
// reflow state (specifically, the main-size of *this continuation* of the
// flex container).
nscoord
nsFlexContainerFrame::GetMainSizeFromReflowState(
  const nsHTMLReflowState& aReflowState,
  const FlexboxAxisTracker& aAxisTracker)
{
  if (IsAxisHorizontal(aAxisTracker.GetMainAxis())) {
    // Horizontal case is easy -- our main size is our computed width
    // (which is already resolved).
    return aReflowState.ComputedISize();
  }

  return GetEffectiveComputedBSize(aReflowState);
}

// Returns the largest outer hypothetical main-size of any line in |aLines|.
// (i.e. the hypothetical main-size of the largest line)
static nscoord
GetLargestLineMainSize(const FlexLine* aFirstLine)
{
  nscoord largestLineOuterSize = 0;
  for (const FlexLine* line = aFirstLine; line; line = line->getNext()) {
    largestLineOuterSize = std::max(largestLineOuterSize,
                                    line->GetTotalOuterHypotheticalMainSize());
  }
  return largestLineOuterSize;
}

// Returns the content-box main-size of our flex container, based on the
// available height (if appropriate) and the main-sizes of the flex items.
static nscoord
ClampFlexContainerMainSize(const nsHTMLReflowState& aReflowState,
                           const FlexboxAxisTracker& aAxisTracker,
                           nscoord aUnclampedMainSize,
                           nscoord aAvailableHeightForContent,
                           const FlexLine* aFirstLine,
                           nsReflowStatus& aStatus)
{
  MOZ_ASSERT(aFirstLine, "null first line pointer");

  if (IsAxisHorizontal(aAxisTracker.GetMainAxis())) {
    // Horizontal case is easy -- our main size should already be resolved
    // before we get a call to Reflow. We don't have to worry about doing
    // page-breaking or shrinkwrapping in the horizontal axis.
    return aUnclampedMainSize;
  }

  if (aUnclampedMainSize != NS_INTRINSICSIZE) {
    // Vertical case, with fixed height:
    if (aAvailableHeightForContent == NS_UNCONSTRAINEDSIZE ||
        aUnclampedMainSize < aAvailableHeightForContent) {
      // Not in a fragmenting context, OR no need to fragment because we have
      // more available height than we need. Either way, just use our fixed
      // height.  (Note that the reflow state has already done the appropriate
      // min/max-height clamping.)
      return aUnclampedMainSize;
    }

    // Fragmenting *and* our fixed height is too tall for available height:
    // Mark incomplete so we get a next-in-flow, and take up all of the
    // available height (or the amount of height required by our children, if
    // that's larger; but of course not more than our own computed height).
    // XXXdholbert For now, we don't support pushing children to our next
    // continuation or splitting children, so "amount of height required by
    // our children" is just the main-size (height) of our longest flex line.
    NS_FRAME_SET_INCOMPLETE(aStatus);
    nscoord largestLineOuterSize = GetLargestLineMainSize(aFirstLine);

    if (largestLineOuterSize <= aAvailableHeightForContent) {
      return aAvailableHeightForContent;
    }
    return std::min(aUnclampedMainSize, largestLineOuterSize);
  }

  // Vertical case, with auto-height:
  // Resolve auto-height to the largest FlexLine-length, clamped to our
  // computed min/max main-size properties (min-height & max-height).
  // XXXdholbert Handle constrained-aAvailableHeightForContent case here.
  nscoord largestLineOuterSize = GetLargestLineMainSize(aFirstLine);
  return NS_CSS_MINMAX(largestLineOuterSize,
                       aReflowState.ComputedMinHeight(),
                       aReflowState.ComputedMaxHeight());
}

nscoord
nsFlexContainerFrame::ComputeCrossSize(const nsHTMLReflowState& aReflowState,
                                       const FlexboxAxisTracker& aAxisTracker,
                                       nscoord aSumLineCrossSizes,
                                       nscoord aAvailableHeightForContent,
                                       bool* aIsDefinite,
                                       nsReflowStatus& aStatus)
{
  MOZ_ASSERT(aIsDefinite, "outparam pointer must be non-null");

  if (IsAxisHorizontal(aAxisTracker.GetCrossAxis())) {
    // Cross axis is horizontal: our cross size is our computed width
    // (which is already resolved).
    *aIsDefinite = true;
    return aReflowState.ComputedISize();
  }

  nscoord effectiveComputedBSize = GetEffectiveComputedBSize(aReflowState);
  if (effectiveComputedBSize != NS_INTRINSICSIZE) {
    // Cross-axis is vertical, and we have a fixed height:
    *aIsDefinite = true;
    if (aAvailableHeightForContent == NS_UNCONSTRAINEDSIZE ||
        effectiveComputedBSize < aAvailableHeightForContent) {
      // Not in a fragmenting context, OR no need to fragment because we have
      // more available height than we need. Either way, just use our fixed
      // height.  (Note that the reflow state has already done the appropriate
      // min/max-height clamping.)
      return effectiveComputedBSize;
    }

    // Fragmenting *and* our fixed height is too tall for available height:
    // Mark incomplete so we get a next-in-flow, and take up all of the
    // available height (or the amount of height required by our children, if
    // that's larger; but of course not more than our own computed height).
    // XXXdholbert For now, we don't support pushing children to our next
    // continuation or splitting children, so "amount of height required by
    // our children" is just our line-height.
    NS_FRAME_SET_INCOMPLETE(aStatus);
    if (aSumLineCrossSizes <= aAvailableHeightForContent) {
      return aAvailableHeightForContent;
    }
    return std::min(effectiveComputedBSize, aSumLineCrossSizes);
  }

  // Cross axis is vertical and we have auto-height: shrink-wrap our line(s),
  // subject to our min-size / max-size constraints in that (vertical) axis.
  // XXXdholbert Handle constrained-aAvailableHeightForContent case here.
  *aIsDefinite = false;
  return NS_CSS_MINMAX(aSumLineCrossSizes,
                       aReflowState.ComputedMinHeight(),
                       aReflowState.ComputedMaxHeight());
}

void
FlexLine::PositionItemsInMainAxis(uint8_t aJustifyContent,
                                  nscoord aContentBoxMainSize,
                                  const FlexboxAxisTracker& aAxisTracker)
{
  MainAxisPositionTracker mainAxisPosnTracker(aAxisTracker, this,
                                              aJustifyContent,
                                              aContentBoxMainSize);
  for (FlexItem* item = mItems.getFirst(); item; item = item->getNext()) {
    nscoord itemMainBorderBoxSize =
      item->GetMainSize() +
      item->GetBorderPaddingSizeInAxis(mainAxisPosnTracker.GetAxis());

    // Resolve any main-axis 'auto' margins on aChild to an actual value.
    mainAxisPosnTracker.ResolveAutoMarginsInMainAxis(*item);

    // Advance our position tracker to child's upper-left content-box corner,
    // and use that as its position in the main axis.
    mainAxisPosnTracker.EnterMargin(item->GetMargin());
    mainAxisPosnTracker.EnterChildFrame(itemMainBorderBoxSize);

    item->SetMainPosition(mainAxisPosnTracker.GetPosition());

    mainAxisPosnTracker.ExitChildFrame(itemMainBorderBoxSize);
    mainAxisPosnTracker.ExitMargin(item->GetMargin());
    mainAxisPosnTracker.TraversePackingSpace();
  }
}

// Helper method to take care of children who ASK_FOR_BASELINE, when
// we need their baseline.
static void
ResolveReflowedChildAscent(nsIFrame* aFrame,
                           nsHTMLReflowMetrics& aChildDesiredSize)
{
  WritingMode wm = aChildDesiredSize.GetWritingMode();
  if (aChildDesiredSize.BlockStartAscent() ==
      nsHTMLReflowMetrics::ASK_FOR_BASELINE) {
    // Use GetFirstLineBaseline(), or just GetBaseline() if that fails.
    nscoord ascent;
    if (nsLayoutUtils::GetFirstLineBaseline(wm, aFrame, &ascent)) {
      aChildDesiredSize.SetBlockStartAscent(ascent);
    } else {
      aChildDesiredSize.SetBlockStartAscent(aFrame->GetLogicalBaseline(wm));
    }
  }
}

/**
 * Given the flex container's "logical ascent" (i.e. distance from the
 * flex container's content-box cross-start edge to its baseline), returns
 * its actual physical ascent value (the distance from the *border-box* top
 * edge to its baseline).
 */
static nscoord
ComputePhysicalAscentFromLogicalAscent(nscoord aLogicalAscent,
                                       nscoord aContentBoxCrossSize,
                                       const nsHTMLReflowState& aReflowState,
                                       const FlexboxAxisTracker& aAxisTracker)
{
  return aReflowState.ComputedPhysicalBorderPadding().top +
    PhysicalPosFromLogicalPos(aLogicalAscent, aContentBoxCrossSize,
                              aAxisTracker.GetCrossAxis());
}

void
nsFlexContainerFrame::SizeItemInCrossAxis(
  nsPresContext* aPresContext,
  const FlexboxAxisTracker& aAxisTracker,
  nsHTMLReflowState& aChildReflowState,
  FlexItem& aItem)
{
  // In vertical flexbox (with horizontal cross-axis), we can just trust the
  // reflow state's computed-width as our cross-size. We also don't need to
  // record the baseline because we'll have converted any "align-self:baseline"
  // items to be "align-self:flex-start" in the FlexItem constructor.
  // FIXME: Once we support writing-mode (vertical text), we will be able to
  // have baseline-aligned items in a vertical flexbox, and we'll need to
  // record baseline information here.
  if (IsAxisHorizontal(aAxisTracker.GetCrossAxis())) {
    MOZ_ASSERT(aItem.GetAlignSelf() != NS_STYLE_ALIGN_ITEMS_BASELINE,
               "In vert flex container, we depend on FlexItem constructor to "
               "convert 'align-self: baseline' to 'align-self: flex-start'");
    aItem.SetCrossSize(aChildReflowState.ComputedWidth());
    return;
  }

  MOZ_ASSERT(!aItem.HadMeasuringReflow(),
             "We shouldn't need more than one measuring reflow");

  if (aItem.GetAlignSelf() == NS_STYLE_ALIGN_ITEMS_STRETCH) {
    // This item's got "align-self: stretch", so we probably imposed a
    // stretched computed height on it during its previous reflow. We're
    // not imposing that height for *this* measuring reflow, so we need to
    // tell it to treat this reflow as a vertical resize (regardless of
    // whether any of its ancestors are being resized).
    aChildReflowState.mFlags.mVResize = true;
  }
  nsHTMLReflowMetrics childDesiredSize(aChildReflowState);
  nsReflowStatus childReflowStatus;
  const uint32_t flags = NS_FRAME_NO_MOVE_FRAME;
  ReflowChild(aItem.Frame(), aPresContext,
              childDesiredSize, aChildReflowState,
              0, 0, flags, childReflowStatus);
  aItem.SetHadMeasuringReflow();

  // XXXdholbert Once we do pagination / splitting, we'll need to actually
  // handle incomplete childReflowStatuses. But for now, we give our kids
  // unconstrained available height, which means they should always complete.
  MOZ_ASSERT(NS_FRAME_IS_COMPLETE(childReflowStatus),
             "We gave flex item unconstrained available height, so it "
             "should be complete");

  // Tell the child we're done with its initial reflow.
  // (Necessary for e.g. GetBaseline() to work below w/out asserting)
  FinishReflowChild(aItem.Frame(), aPresContext,
                    childDesiredSize, &aChildReflowState, 0, 0, flags);

  // Save the sizing info that we learned from this reflow
  // -----------------------------------------------------

  // Tentatively store the child's desired content-box cross-size.
  // Note that childDesiredSize is the border-box size, so we have to
  // subtract border & padding to get the content-box size.
  // (Note that at this point in the code, we know our cross axis is vertical,
  // so we don't bother with making aAxisTracker pick the cross-axis component
  // for us.)
  nscoord crossAxisBorderPadding = aItem.GetBorderPadding().TopBottom();
  if (childDesiredSize.Height() < crossAxisBorderPadding) {
    // Child's requested size isn't large enough for its border/padding!
    // This is OK for the trivial nsFrame::Reflow() impl, but other frame
    // classes should know better. So, if we get here, the child had better be
    // an instance of nsFrame (i.e. it should return null from GetType()).
    // XXXdholbert Once we've fixed bug 765861, we should upgrade this to an
    // assertion that trivially passes if bug 765861's flag has been flipped.
    NS_WARN_IF_FALSE(!aItem.Frame()->GetType(),
                     "Child should at least request space for border/padding");
    aItem.SetCrossSize(0);
  } else {
    // (normal case)
    aItem.SetCrossSize(childDesiredSize.Height() - crossAxisBorderPadding);
  }

  // If we need to do baseline-alignment, store the child's ascent.
  if (aItem.GetAlignSelf() == NS_STYLE_ALIGN_ITEMS_BASELINE) {
    ResolveReflowedChildAscent(aItem.Frame(), childDesiredSize);
    aItem.SetAscent(childDesiredSize.BlockStartAscent());
  }
}

void
FlexLine::PositionItemsInCrossAxis(nscoord aLineStartPosition,
                                   const FlexboxAxisTracker& aAxisTracker)
{
  SingleLineCrossAxisPositionTracker lineCrossAxisPosnTracker(aAxisTracker);

  for (FlexItem* item = mItems.getFirst(); item; item = item->getNext()) {
    // First, stretch the item's cross size (if appropriate), and resolve any
    // auto margins in this axis.
    item->ResolveStretchedCrossSize(mLineCrossSize, aAxisTracker);
    lineCrossAxisPosnTracker.ResolveAutoMarginsInCrossAxis(*this, *item);

    // Compute the cross-axis position of this item
    nscoord itemCrossBorderBoxSize =
      item->GetCrossSize() +
      item->GetBorderPaddingSizeInAxis(aAxisTracker.GetCrossAxis());
    lineCrossAxisPosnTracker.EnterAlignPackingSpace(*this, *item, aAxisTracker);
    lineCrossAxisPosnTracker.EnterMargin(item->GetMargin());
    lineCrossAxisPosnTracker.EnterChildFrame(itemCrossBorderBoxSize);

    item->SetCrossPosition(aLineStartPosition +
                           lineCrossAxisPosnTracker.GetPosition());

    // Back out to cross-axis edge of the line.
    lineCrossAxisPosnTracker.ResetPosition();
  }
}

void
nsFlexContainerFrame::Reflow(nsPresContext*           aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFlexContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG,
         ("Reflow() for nsFlexContainerFrame %p\n", this));

  if (IsFrameTreeTooDeep(aReflowState, aDesiredSize, aStatus)) {
    return;
  }

  // We (and our children) can only depend on our ancestor's height if we have
  // a percent-height, or if we're positioned and we have "top" and "bottom"
  // set and have height:auto.  (There are actually other cases, too -- e.g. if
  // our parent is itself a vertical flex container and we're flexible -- but
  // we'll let our ancestors handle those sorts of cases.)
  const nsStylePosition* stylePos = StylePosition();
  if (stylePos->mHeight.HasPercent() ||
      (StyleDisplay()->IsAbsolutelyPositionedStyle() &&
       eStyleUnit_Auto == stylePos->mHeight.GetUnit() &&
       eStyleUnit_Auto != stylePos->mOffset.GetTopUnit() &&
       eStyleUnit_Auto != stylePos->mOffset.GetBottomUnit())) {
    AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
  }

#ifdef DEBUG
  SanityCheckAnonymousFlexItems();
#endif // DEBUG

  // If we've never reordered our children, then we can trust that they're
  // already in DOM-order, and we only need to consider their "order" property
  // when checking them for sortedness & sorting them.
  //
  // After we actually sort them, though, we can't trust that they're in DOM
  // order anymore.  So, from that point on, our sort & sorted-order-checking
  // operations need to use a fancier LEQ function that also takes DOM order
  // into account, so that we can honor the spec's requirement that frames w/
  // equal "order" values are laid out in DOM order.

  if (!HasAnyStateBits(NS_STATE_FLEX_CHILDREN_REORDERED)) {
    if (SortChildrenIfNeeded<IsOrderLEQ>()) {
      AddStateBits(NS_STATE_FLEX_CHILDREN_REORDERED);
    }
  } else {
    SortChildrenIfNeeded<IsOrderLEQWithDOMFallback>();
  }

  const FlexboxAxisTracker axisTracker(this);

  // If we're being fragmented into a constrained height, subtract off
  // borderpadding-top from it, to get the available height for our
  // content box. (Don't subtract if we're skipping top border/padding,
  // though.)
  nscoord availableHeightForContent = aReflowState.AvailableHeight();
  if (availableHeightForContent != NS_UNCONSTRAINEDSIZE &&
      !(GetSkipSides() & (1 << NS_SIDE_TOP))) {
    availableHeightForContent -= aReflowState.ComputedPhysicalBorderPadding().top;
    // (Don't let that push availableHeightForContent below zero, though):
    availableHeightForContent = std::max(availableHeightForContent, 0);
  }

  nscoord contentBoxMainSize = GetMainSizeFromReflowState(aReflowState,
                                                          axisTracker);

  nsAutoTArray<StrutInfo, 1> struts;
  DoFlexLayout(aPresContext, aDesiredSize, aReflowState, aStatus,
               contentBoxMainSize, availableHeightForContent,
               struts, axisTracker);

  if (!struts.IsEmpty()) {
    // We're restarting flex layout, with new knowledge of collapsed items.
    DoFlexLayout(aPresContext, aDesiredSize, aReflowState, aStatus,
                 contentBoxMainSize, availableHeightForContent,
                 struts, axisTracker);
  }
}

// RAII class to clean up a list of FlexLines.
// Specifically, this removes each line from the list, deletes all the
// FlexItems in its list, and deletes the FlexLine.
class MOZ_STACK_CLASS AutoFlexLineListClearer
{
public:
  AutoFlexLineListClearer(LinkedList<FlexLine>& aLines
                          MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
  : mLines(aLines)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  ~AutoFlexLineListClearer()
  {
    while (FlexLine* line = mLines.popFirst()) {
      while (FlexItem* item = line->mItems.popFirst()) {
        delete item;
      }
      delete line;
    }
  }

private:
  LinkedList<FlexLine>& mLines;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

void
nsFlexContainerFrame::DoFlexLayout(nsPresContext*           aPresContext,
                                   nsHTMLReflowMetrics&     aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   nscoord aContentBoxMainSize,
                                   nscoord aAvailableHeightForContent,
                                   nsTArray<StrutInfo>& aStruts,
                                   const FlexboxAxisTracker& aAxisTracker)
{
  aStatus = NS_FRAME_COMPLETE;

  LinkedList<FlexLine> lines;
  AutoFlexLineListClearer cleanupLines(lines);

  GenerateFlexLines(aPresContext, aReflowState,
                    aContentBoxMainSize,
                    aAvailableHeightForContent,
                    aStruts, aAxisTracker, lines);

  aContentBoxMainSize =
    ClampFlexContainerMainSize(aReflowState, aAxisTracker,
                               aContentBoxMainSize, aAvailableHeightForContent,
                               lines.getFirst(), aStatus);

  for (FlexLine* line = lines.getFirst(); line; line = line->getNext()) {
    line->ResolveFlexibleLengths(aContentBoxMainSize);
  }

  // Cross Size Determination - Flexbox spec section 9.4
  // ===================================================
  // Calculate the hypothetical cross size of each item:
  nscoord sumLineCrossSizes = 0;
  for (FlexLine* line = lines.getFirst(); line; line = line->getNext()) {
    for (FlexItem* item = line->GetFirstItem(); item; item = item->getNext()) {
      // (If the item's already been stretched, or it's a strut, then it
      // already knows its cross size.  Don't bother trying to recalculate it.)
      if (!item->IsStretched() && !item->IsStrut()) {
        nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                           item->Frame(),
                                           nsSize(aReflowState.ComputedWidth(),
                                                  NS_UNCONSTRAINEDSIZE));
        // Override computed main-size
        if (IsAxisHorizontal(aAxisTracker.GetMainAxis())) {
          childReflowState.SetComputedWidth(item->GetMainSize());
        } else {
          childReflowState.SetComputedHeight(item->GetMainSize());
        }
        
        SizeItemInCrossAxis(aPresContext, aAxisTracker,
                            childReflowState, *item);
      }
    }
    // Now that we've finished with this line's items, size the line itself:
    line->ComputeCrossSizeAndBaseline(aAxisTracker);
    sumLineCrossSizes += line->GetLineCrossSize();
  }

  bool isCrossSizeDefinite;
  const nscoord contentBoxCrossSize =
    ComputeCrossSize(aReflowState, aAxisTracker, sumLineCrossSizes,
                     aAvailableHeightForContent, &isCrossSizeDefinite, aStatus);

  // Set up state for cross-axis alignment, at a high level (outside the
  // scope of a particular flex line)
  CrossAxisPositionTracker
    crossAxisPosnTracker(lines.getFirst(),
                         aReflowState.mStylePosition->mAlignContent,
                         contentBoxCrossSize, isCrossSizeDefinite,
                         aAxisTracker);

  // Now that we know the cross size of each line (including
  // "align-content:stretch" adjustments, from the CrossAxisPositionTracker
  // constructor), we can create struts for any flex items with
  // "visibility: collapse" (and restart flex layout).
  if (aStruts.IsEmpty()) { // (Don't make struts if we already did)
    BuildStrutInfoFromCollapsedItems(lines.getFirst(), aStruts);
    if (!aStruts.IsEmpty()) {
      // Restart flex layout, using our struts.
      return;
    }
  }

  // If the container should derive its baseline from the first FlexLine,
  // do that here (while crossAxisPosnTracker is conveniently pointing
  // at the cross-start edge of that line, which the line's baseline offset is
  // measured from):
  nscoord flexContainerAscent;
  if (!aAxisTracker.AreAxesInternallyReversed()) {
    nscoord firstLineBaselineOffset = lines.getFirst()->GetBaselineOffset();
    if (firstLineBaselineOffset == nscoord_MIN) {
      // No baseline-aligned items in line. Use sentinel value to prompt us to
      // get baseline from the first FlexItem after we've reflowed it.
      flexContainerAscent = nscoord_MIN;
    } else  {
      flexContainerAscent =
        ComputePhysicalAscentFromLogicalAscent(
          crossAxisPosnTracker.GetPosition() + firstLineBaselineOffset,
          contentBoxCrossSize, aReflowState, aAxisTracker);
    }
  }

  for (FlexLine* line = lines.getFirst(); line; line = line->getNext()) {

    // Main-Axis Alignment - Flexbox spec section 9.5
    // ==============================================
    line->PositionItemsInMainAxis(aReflowState.mStylePosition->mJustifyContent,
                                  aContentBoxMainSize,
                                  aAxisTracker);

    // Cross-Axis Alignment - Flexbox spec section 9.6
    // ===============================================
    line->PositionItemsInCrossAxis(crossAxisPosnTracker.GetPosition(),
                                   aAxisTracker);
    crossAxisPosnTracker.TraverseLine(*line);
    crossAxisPosnTracker.TraversePackingSpace();
  }

  // If the container should derive its baseline from the last FlexLine,
  // do that here (while crossAxisPosnTracker is conveniently pointing
  // at the cross-end edge of that line, which the line's baseline offset is
  // measured from):
  if (aAxisTracker.AreAxesInternallyReversed()) {
    nscoord lastLineBaselineOffset = lines.getLast()->GetBaselineOffset();
    if (lastLineBaselineOffset == nscoord_MIN) {
      // No baseline-aligned items in line. Use sentinel value to prompt us to
      // get baseline from the last FlexItem after we've reflowed it.
      flexContainerAscent = nscoord_MIN;
    } else {
      flexContainerAscent =
        ComputePhysicalAscentFromLogicalAscent(
          crossAxisPosnTracker.GetPosition() - lastLineBaselineOffset,
          contentBoxCrossSize, aReflowState, aAxisTracker);
    }
  }

  // Before giving each child a final reflow, calculate the origin of the
  // flex container's content box (with respect to its border-box), so that
  // we can compute our flex item's final positions.
  nsMargin containerBorderPadding(aReflowState.ComputedPhysicalBorderPadding());
  containerBorderPadding.ApplySkipSides(GetSkipSides(&aReflowState));
  const nsPoint containerContentBoxOrigin(containerBorderPadding.left,
                                          containerBorderPadding.top);

  // FINAL REFLOW: Give each child frame another chance to reflow, now that
  // we know its final size and position.
  for (const FlexLine* line = lines.getFirst(); line; line = line->getNext()) {
    for (const FlexItem* item = line->GetFirstItem(); item;
         item = item->getNext()) {
      nsPoint physicalPosn = aAxisTracker.PhysicalPointFromLogicalPoint(
                               item->GetMainPosition(),
                               item->GetCrossPosition(),
                               aContentBoxMainSize,
                               contentBoxCrossSize);
      // Adjust physicalPosn to be relative to the container's border-box
      // (i.e. its frame rect), instead of the container's content-box:
      physicalPosn += containerContentBoxOrigin;

      nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                         item->Frame(),
                                         nsSize(aReflowState.ComputedWidth(),
                                                NS_UNCONSTRAINEDSIZE));

      // Keep track of whether we've overriden the child's computed height
      // and/or width, so we can set its resize flags accordingly.
      bool didOverrideComputedWidth = false;
      bool didOverrideComputedHeight = false;

      // Override computed main-size
      if (IsAxisHorizontal(aAxisTracker.GetMainAxis())) {
        childReflowState.SetComputedWidth(item->GetMainSize());
        didOverrideComputedWidth = true;
      } else {
        childReflowState.SetComputedHeight(item->GetMainSize());
        didOverrideComputedHeight = true;
      }

      // Override reflow state's computed cross-size, for stretched items.
      if (item->IsStretched()) {
        MOZ_ASSERT(item->GetAlignSelf() == NS_STYLE_ALIGN_ITEMS_STRETCH,
                   "stretched item w/o 'align-self: stretch'?");
        if (IsAxisHorizontal(aAxisTracker.GetCrossAxis())) {
          childReflowState.SetComputedWidth(item->GetCrossSize());
          didOverrideComputedWidth = true;
        } else {
          // If this item's height is stretched, it's a relative height.
          item->Frame()->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
          childReflowState.SetComputedHeight(item->GetCrossSize());
          didOverrideComputedHeight = true;
        }
      }

      // XXXdholbert Might need to actually set the correct margins in the
      // reflow state at some point, so that they can be saved on the frame for
      // UsedMarginProperty().  Maybe doesn't matter though...?

      // If we're overriding the computed width or height, *and* we had an
      // earlier "measuring" reflow, then this upcoming reflow needs to be
      // treated as a resize.
      if (item->HadMeasuringReflow()) {
        if (didOverrideComputedWidth) {
          // (This is somewhat redundant, since the reflow state already
          // sets mHResize whenever our computed width has changed since the
          // previous reflow. Still, it's nice for symmetry, and it may become
          // necessary once we support orthogonal flows.)
          childReflowState.mFlags.mHResize = true;
        }
        if (didOverrideComputedHeight) {
          childReflowState.mFlags.mVResize = true;
        }
      }
      // NOTE: Be very careful about doing anything else with childReflowState
      // after this point, because some of its methods (e.g. SetComputedWidth)
      // internally call InitResizeFlags and stomp on mVResize & mHResize.

      nsHTMLReflowMetrics childDesiredSize(childReflowState);
      nsReflowStatus childReflowStatus;
      ReflowChild(item->Frame(), aPresContext,
                  childDesiredSize, childReflowState,
                  physicalPosn.x, physicalPosn.y,
                  0, childReflowStatus);

      // XXXdholbert Once we do pagination / splitting, we'll need to actually
      // handle incomplete childReflowStatuses. But for now, we give our kids
      // unconstrained available height, which means they should always
      // complete.
      MOZ_ASSERT(NS_FRAME_IS_COMPLETE(childReflowStatus),
                 "We gave flex item unconstrained available height, so it "
                 "should be complete");

      childReflowState.ApplyRelativePositioning(&physicalPosn);

      FinishReflowChild(item->Frame(), aPresContext,
                        childDesiredSize, &childReflowState,
                        physicalPosn.x, physicalPosn.y, 0);

      // If this is our first child and we haven't established a baseline for
      // the container yet (i.e. if we don't have 'align-self: baseline' on any
      // children), then use this child's baseline as the container's baseline.
      if (item->Frame() == mFrames.FirstChild() &&
          flexContainerAscent == nscoord_MIN) {
        ResolveReflowedChildAscent(item->Frame(), childDesiredSize);

        // (We use GetNormalPosition() instead of physicalPosn because we don't
        // want relative positioning on the child to affect the baseline that we
        // read from it).
        WritingMode wm = aReflowState.GetWritingMode();
        flexContainerAscent =
          item->Frame()->GetLogicalNormalPosition(wm,
                                                  childDesiredSize.Width()).B(wm) +
          childDesiredSize.BlockStartAscent();
      }
    }
  }

  nsSize desiredContentBoxSize =
    aAxisTracker.PhysicalSizeFromLogicalSizes(aContentBoxMainSize,
                                              contentBoxCrossSize);

  aDesiredSize.Width() = desiredContentBoxSize.width +
    containerBorderPadding.LeftRight();
  // Does *NOT* include bottom border/padding yet (we add that a bit lower down)
  aDesiredSize.Height() = desiredContentBoxSize.height +
    containerBorderPadding.top;

  if (flexContainerAscent == nscoord_MIN) {
    // Still don't have our baseline set -- this happens if we have no
    // children (or if our children are huge enough that they have nscoord_MIN
    // as their baseline... in which case, we'll use the wrong baseline, but no
    // big deal)
    NS_WARN_IF_FALSE(lines.getFirst()->IsEmpty(),
                     "Have flex items but didn't get an ascent - that's odd "
                     "(or there are just gigantic sizes involved)");
    // Per spec, just use the bottom of content-box.
    flexContainerAscent = aDesiredSize.Height();
  }
  aDesiredSize.SetBlockStartAscent(flexContainerAscent);

  // Now: If we're complete, add bottom border/padding to desired height
  // (unless that pushes us over available height, in which case we become
  // incomplete (unless we already weren't asking for any height, in which case
  // we stay complete to avoid looping forever)).
  // NOTE: If we're auto-height, we allow our bottom border/padding to push us
  // over the available height without requesting a continuation, for
  // consistency with the behavior of "display:block" elements.
  if (NS_FRAME_IS_COMPLETE(aStatus)) {
    // NOTE: We can't use containerBorderPadding.bottom for this, because if
    // we're auto-height, ApplySkipSides will have zeroed it (because it
    // assumed we might get a continuation). We have the correct value in
    // aReflowState.ComputedPhyiscalBorderPadding().bottom, though, so we use that.
    nscoord desiredHeightWithBottomBP =
      aDesiredSize.Height() + aReflowState.ComputedPhysicalBorderPadding().bottom;

    if (aReflowState.AvailableHeight() == NS_UNCONSTRAINEDSIZE ||
        aDesiredSize.Height() == 0 ||
        desiredHeightWithBottomBP <= aReflowState.AvailableHeight() ||
        aReflowState.ComputedHeight() == NS_INTRINSICSIZE) {
      // Update desired height to include bottom border/padding
      aDesiredSize.Height() = desiredHeightWithBottomBP;
    } else {
      // We couldn't fit bottom border/padding, so we'll need a continuation.
      NS_FRAME_SET_INCOMPLETE(aStatus);
    }
  }

  // Overflow area = union(my overflow area, kids' overflow areas)
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, e.get());
  }

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize,
                                 aReflowState, aStatus);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize)
}

/* virtual */ nscoord
nsFlexContainerFrame::GetMinWidth(nsRenderingContext* aRenderingContext)
{
  nscoord minWidth = 0;
  DISPLAY_MIN_WIDTH(this, minWidth);

  FlexboxAxisTracker axisTracker(this);

  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    nscoord childMinWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, e.get(),
                                           nsLayoutUtils::MIN_WIDTH);
    // For a horizontal single-line flex container, the intrinsic min width is
    // the sum of its items' min widths.
    // For a vertical flex container, or for a multi-line horizontal flex
    // container, the intrinsic min width is the max of its items' min widths.
    if (IsAxisHorizontal(axisTracker.GetMainAxis()) &&
        NS_STYLE_FLEX_WRAP_NOWRAP == StylePosition()->mFlexWrap) {
      minWidth += childMinWidth;
    } else {
      minWidth = std::max(minWidth, childMinWidth);
    }
  }
  return minWidth;
}

/* virtual */ nscoord
nsFlexContainerFrame::GetPrefWidth(nsRenderingContext* aRenderingContext)
{
  nscoord prefWidth = 0;
  DISPLAY_PREF_WIDTH(this, prefWidth);

  // XXXdholbert Optimization: We could cache our intrinsic widths like
  // nsBlockFrame does (and return it early from this function if it's set).
  // Whenever anything happens that might change it, set it to
  // NS_INTRINSIC_WIDTH_UNKNOWN (like nsBlockFrame::MarkIntrinsicWidthsDirty
  // does)
  FlexboxAxisTracker axisTracker(this);

  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    nscoord childPrefWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, e.get(),
                                           nsLayoutUtils::PREF_WIDTH);
    if (IsAxisHorizontal(axisTracker.GetMainAxis())) {
      prefWidth += childPrefWidth;
    } else {
      prefWidth = std::max(prefWidth, childPrefWidth);
    }
  }
  return prefWidth;
}
