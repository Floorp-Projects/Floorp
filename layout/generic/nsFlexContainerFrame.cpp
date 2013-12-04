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
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "prlog.h"
#include <algorithm>

using namespace mozilla::css;
using namespace mozilla::layout;

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
class MOZ_STACK_CLASS FlexboxAxisTracker {
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
   * Converts a logical position into a "physical" x,y position.
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
  nsPoint PhysicalPositionFromLogicalPosition(nscoord aMainPosn,
                                              nscoord aCrossPosn,
                                              nscoord aContainerMainSize,
                                              nscoord aContainerCrossSize) const {
    // If either of our logical axes are backwards with respect to our x,y
    // coordinate system (e.g. right-to-left or bottom-to-top), then subtract
    // that axis's logical coord them from the container size in that dimension,
    // to flip the polarity around.
    if (!AxisGrowsInPositiveDirection(mMainAxis)) {
      aMainPosn = aContainerMainSize - aMainPosn;
    }
    if (!AxisGrowsInPositiveDirection(mCrossAxis)) {
      aCrossPosn = aContainerCrossSize - aCrossPosn;
    }

    return IsAxisHorizontal(mMainAxis) ?
      nsPoint(aMainPosn, aCrossPosn) :
      nsPoint(aCrossPosn, aMainPosn);
  }
  nsSize PhysicalSizeFromLogicalSizes(nscoord aMainSize,
                                      nscoord aCrossSize) const {
    return IsAxisHorizontal(mMainAxis) ?
      nsSize(aMainSize, aCrossSize) :
      nsSize(aCrossSize, aMainSize);
  }

private:
  AxisOrientationType mMainAxis;
  AxisOrientationType mCrossAxis;
};

// Represents a flex item.
// Includes the various pieces of input that the Flexbox Layout Algorithm uses
// to resolve a flexible width.
class FlexItem {
public:
  FlexItem(nsIFrame* aChildFrame,
           float aFlexGrow, float aFlexShrink, nscoord aMainBaseSize,
           nscoord aMainMinSize, nscoord aMainMaxSize,
           nscoord aCrossMinSize, nscoord aCrossMaxSize,
           nsMargin aMargin, nsMargin aBorderPadding,
           const FlexboxAxisTracker& aAxisTracker);

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

  // Returns the distance between this FlexItem's baseline and the cross-start
  // edge of its margin-box. Used in baseline alignment.
  // (This function needs to be told what the cross axis is so that it can
  // look up the appropriate component from mMargin.)
  nscoord GetBaselineOffsetFromOuterCrossStart(
            AxisOrientationType aCrossAxis) const;

  float GetShareOfFlexWeightSoFar() const { return mShareOfFlexWeightSoFar; }

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

  uint8_t GetAlignSelf() const     { return mAlignSelf; }

  // Returns the flex weight that we should use in the "resolving flexible
  // lengths" algorithm.  If we're using flex grow, we just return that;
  // otherwise, we use the "scaled flex shrink weight" (scaled by our flex
  // base size, so that when both large and small items are shrinking,
  // the large items shrink more).
  float GetFlexWeightToUse(bool aIsUsingFlexGrow)
  {
    if (IsFrozen()) {
      return 0.0f;
    }

    return aIsUsingFlexGrow ?
      mFlexGrow :
      mFlexShrink * mFlexBaseSize;
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

  // This sets our flex base size, and then updates the main size to the
  // base size clamped to our main-axis [min,max] constraints.
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

  void SetShareOfFlexWeightSoFar(float aNewShare)
  {
    MOZ_ASSERT(!mIsFrozen || aNewShare == 0.0f,
               "shouldn't be giving this item any share of the weight "
               "after it's frozen");
    mShareOfFlexWeightSoFar = aNewShare;
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
  float mShareOfFlexWeightSoFar;
  bool mIsFrozen;
  bool mHadMinViolation;
  bool mHadMaxViolation;

  // Misc:
  bool mHadMeasuringReflow; // Did this item get a preliminary reflow,
                            // to measure its desired height?
  bool mIsStretched; // See IsStretched() documentation
  uint8_t mAlignSelf; // My "align-self" computed value (with "auto"
                      // swapped out for parent"s "align-items" value,
                      // in our constructor).
};

// Represents a single flex line in a flex container.
// Manages an array of the FlexItems that are in the line.
class FlexLine {
public:
  FlexLine()
  : mTotalInnerHypotheticalMainSize(0),
    mTotalOuterHypotheticalMainSize(0),
    mLineCrossSize(0),
    mBaselineOffsetFromCrossStart(nscoord_MIN)
  {}

  // Returns the sum of our FlexItems' outer hypothetical main sizes.
  // ("outer" = margin-box, and "hypothetical" = before flexing)
  nscoord GetTotalOuterHypotheticalMainSize() const {
    return mTotalOuterHypotheticalMainSize;
  }

  // Adds a new FlexItem's hypothetical main sizes to our totals.
  // (Should only be called when a FlexItem is being appended to this line.)
  void AddToMainSizeTotals(nscoord aItemInnerHypotheticalMainSize,
                           nscoord aItemOuterHypotheticalMainSize) {
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

  // Returns the distance from the cross-start edge of this FlexLine
  // to its baseline (derived from its baseline-aligned FlexItems).
  // If there are no baseline-aligned FlexItems, returns nscoord_MIN.
  nscoord GetBaselineOffsetFromCrossStart() const {
    return mBaselineOffsetFromCrossStart;
  }

  // Runs the "resolve the flexible lengths" algorithm, distributing
  // |aFlexContainerMainSize| among the |aItems| and freezing them.
  void ResolveFlexibleLengths(nscoord aFlexContainerMainSize);

  void PositionItemsInMainAxis(uint8_t aJustifyContent,
                               nscoord aContentBoxMainSize,
                               const FlexboxAxisTracker& aAxisTracker);

  void PositionItemsInCrossAxis(nscoord aLineStartPosition,
                                const FlexboxAxisTracker& aAxisTracker);

  nsTArray<FlexItem> mItems; // Array of the flex items in this flex line.

private:
  nscoord mTotalInnerHypotheticalMainSize;
  nscoord mTotalOuterHypotheticalMainSize;
  nscoord mLineCrossSize;
  nscoord mBaselineOffsetFromCrossStart;
};

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

FlexItem
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
                                           childRS.mComputedMinWidth,
                                           childRS.mComputedMinHeight);
  nscoord mainMaxSize = GET_MAIN_COMPONENT(aAxisTracker,
                                           childRS.mComputedMaxWidth,
                                           childRS.mComputedMaxHeight);
  // This is enforced by the nsHTMLReflowState where these values come from:
  MOZ_ASSERT(mainMinSize <= mainMaxSize, "min size is larger than max size");

  // CROSS MIN/MAX SIZE
  // ------------------

  nscoord crossMinSize = GET_CROSS_COMPONENT(aAxisTracker,
                                             childRS.mComputedMinWidth,
                                             childRS.mComputedMinHeight);
  nscoord crossMaxSize = GET_CROSS_COMPONENT(aAxisTracker,
                                             childRS.mComputedMaxWidth,
                                             childRS.mComputedMaxHeight);

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

    // GMWS() returns border-box; we need content-box
    widgetMainMinSize -=
      aAxisTracker.GetMarginSizeInMainAxis(childRS.mComputedBorderPadding);
    widgetCrossMinSize -=
      aAxisTracker.GetMarginSizeInCrossAxis(childRS.mComputedBorderPadding);

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
  FlexItem item(aChildFrame,
                flexGrow, flexShrink, flexBaseSize,
                mainMinSize, mainMaxSize,
                crossMinSize, crossMaxSize,
                childRS.mComputedMargin,
                childRS.mComputedBorderPadding,
                aAxisTracker);

  // If we're inflexible, we can just freeze to our hypothetical main-size
  // up-front. Similarly, if we're a fixed-size widget, we only have one
  // valid size, so we freeze to keep ourselves from flexing.
  if (isFixedSizeWidget || (flexGrow == 0.0f && flexShrink == 0.0f)) {
    item.Freeze();
  }

  return item;
}

nsresult
nsFlexContainerFrame::
  ResolveFlexItemMaxContentSizing(nsPresContext* aPresContext,
                                  FlexItem& aFlexItem,
                                  const nsHTMLReflowState& aParentReflowState,
                                  const FlexboxAxisTracker& aAxisTracker)
{
  if (IsAxisHorizontal(aAxisTracker.GetMainAxis())) {
    // Nothing to do -- this function is only for measuring flex items
    // in a vertical flex container.
    return NS_OK;
  }

  if (NS_AUTOHEIGHT != aFlexItem.GetFlexBaseSize()) {
    // Nothing to do; this function's only relevant for flex items
    // with a base size of "auto" (or equivalent).
    // XXXdholbert If & when we handle "min-height: min-content" for flex items,
    // we'll want to resolve that in this function, too.
    return NS_OK;
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

  aFlexItem.ResolveStretchedCrossSize(aParentReflowState.ComputedWidth(),
                                      aAxisTracker);
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

  nsHTMLReflowMetrics childDesiredSize;
  nsReflowStatus childReflowStatus;
  nsresult rv = ReflowChild(aFlexItem.Frame(), aPresContext,
                            childDesiredSize, childRSForMeasuringHeight,
                            0, 0, NS_FRAME_NO_MOVE_FRAME,
                            childReflowStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(NS_FRAME_IS_COMPLETE(childReflowStatus),
             "We gave flex item unconstrained available height, so it "
             "should be complete");

  rv = FinishReflowChild(aFlexItem.Frame(), aPresContext,
                         &childRSForMeasuringHeight, childDesiredSize,
                         0, 0, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  // Subtract border/padding in vertical axis, to get _just_
  // the effective computed value of the "height" property.
  nscoord childDesiredHeight = childDesiredSize.height -
    childRSForMeasuringHeight.mComputedBorderPadding.TopBottom();
  childDesiredHeight = std::max(0, childDesiredHeight);

  aFlexItem.SetFlexBaseSizeAndMainSize(childDesiredHeight);
  aFlexItem.SetHadMeasuringReflow();

  return NS_OK;
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
    mShareOfFlexWeightSoFar(0.0f),
    mIsFrozen(false),
    mHadMinViolation(false),
    mHadMaxViolation(false),
    mHadMeasuringReflow(false),
    mIsStretched(false),
    mAlignSelf(aChildFrame->StylePosition()->mAlignSelf)
{
  MOZ_ASSERT(aChildFrame, "expecting a non-null child frame");

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

nscoord
FlexItem::GetBaselineOffsetFromOuterCrossStart(
  AxisOrientationType aCrossAxis) const
{
  // NOTE: Currently, 'mAscent' (taken from reflow) is an inherently vertical
  // measurement -- it's the distance from the border-top edge of this FlexItem
  // to its baseline. So, we can really only do baseline alignment when the
  // cross axis is vertical. (The FlexItem constructor enforces this when
  // resolving the item's "mAlignSelf" value).
  MOZ_ASSERT(!IsAxisHorizontal(aCrossAxis),
             "Only expecting to be doing baseline computations when the "
             "cross axis is vertical");
 
  nscoord marginTopToBaseline = mAscent + mMargin.top;

  if (aCrossAxis == eAxis_TB) {
    // Top-to-bottom (normal case): the distance from the cross-start margin-box
    // edge (i.e. the margin-top edge) to the baseline is ascent + margin-top.
    return marginTopToBaseline;
  }

  // Bottom-to-top: The distance from the cross-start margin-box edge (i.e. the
  // margin-bottom edge) to the baseline is just the margin-box cross size
  // (i.e. outer cross size), minus the distance from margin-top to baseline
  // (already computed above).
  nscoord outerCrossSize = mCrossSize +
    GetMarginBorderPaddingSizeInAxis(aCrossAxis);

  return outerCrossSize - marginTopToBaseline;
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
                          const nsTArray<FlexItem>& aItems,
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
  CrossAxisPositionTracker(nsFlexContainerFrame* aFlexContainerFrame,
                           const FlexboxAxisTracker& aAxisTracker,
                           const nsHTMLReflowState& aReflowState)
    : PositionTracker(aAxisTracker.GetCrossAxis()) {}

  // XXXdholbert This probably needs a ResolveStretchedLines() method,
  // (which takes an array of SingleLineCrossAxisPositionTracker objects
  // and distributes an equal amount of space to each one).
  // For now, we just have Reflow directly call
  // SingleLineCrossAxisPositionTracker::SetLineCrossSize().
};

// Utility class for managing our position along the cross axis, *within* a
// single flex line.
class MOZ_STACK_CLASS SingleLineCrossAxisPositionTracker : public PositionTracker {
public:
  SingleLineCrossAxisPositionTracker(const FlexboxAxisTracker& aAxisTracker);

  void ResolveAutoMarginsInCrossAxis(const FlexLine& aLine,
                                     FlexItem& aItem);

  void EnterAlignPackingSpace(const FlexLine& aLine,
                              const FlexItem& aItem);

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

nsIFrame*
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

#ifdef DEBUG
NS_IMETHODIMP
nsFlexContainerFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FlexContainer"), aResult);
}
#endif // DEBUG

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
  // nsCSSFrameConstructor::FrameConstructionItem::NeedsAnonFlexItem()
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
      MOZ_ASSERT(!prevChildWasAnonFlexItem,
                 "two anon flex items in a row (shouldn't happen)");

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

// Based on the sign of aTotalViolation, this function freezes a subset of our
// flexible sizes, and restores the remaining ones to their initial pref sizes.
static void
FreezeOrRestoreEachFlexibleSize(
  const nscoord aTotalViolation,
  nsTArray<FlexItem>& aItems)
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

  for (uint32_t i = 0; i < aItems.Length(); i++) {
    FlexItem& item = aItems[i];
    MOZ_ASSERT(!item.HadMinViolation() || !item.HadMaxViolation(),
               "Can have either min or max violation, but not both");

    if (!item.IsFrozen()) {
      if (eFreezeEverything == freezeType ||
          (eFreezeMinViolations == freezeType && item.HadMinViolation()) ||
          (eFreezeMaxViolations == freezeType && item.HadMaxViolation())) {

        MOZ_ASSERT(item.GetMainSize() >= item.GetMainMinSize(),
                   "Freezing item at a size below its minimum");
        MOZ_ASSERT(item.GetMainSize() <= item.GetMainMaxSize(),
                   "Freezing item at a size above its maximum");

        item.Freeze();
      } // else, we'll reset this item's main size to its flex base size on the
        // next iteration of this algorithm.

      // Clear this item's violation(s), now that we've dealt with them
      item.ClearViolationFlags();
    }
  }
}

// Implementation of flexbox spec's "resolve the flexible lengths" algorithm.
// NOTE: aTotalFreeSpace should already have the flex items' margin, border,
// & padding values subtracted out, so that all we need to do is distribute the
// remaining free space among content-box sizes.  (The spec deals with
// margin-box sizes, but we can have fewer values in play & a simpler algorithm
// if we subtract margin/border/padding up front.)
void
FlexLine::ResolveFlexibleLengths(nscoord aFlexContainerMainSize)
{
  PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG, ("ResolveFlexibleLengths\n"));
  if (mItems.IsEmpty()) {
    return;
  }

  // Subtract space occupied by our items' margins/borders/padding, so we can
  // just be dealing with the space available for our flex items' content
  // boxes.
  nscoord spaceReservedForMarginBorderPadding =
    mTotalOuterHypotheticalMainSize - mTotalInnerHypotheticalMainSize;

  nscoord spaceAvailableForFlexItemsContentBoxes =
    aFlexContainerMainSize - spaceReservedForMarginBorderPadding;

  // Determine whether we're going to be growing or shrinking items.
  const bool isUsingFlexGrow =
    (mTotalOuterHypotheticalMainSize < aFlexContainerMainSize);

  // NOTE: I claim that this chunk of the algorithm (the looping part) needs to
  // run the loop at MOST aItems.Length() times.  This claim should hold up
  // because we'll freeze at least one item on each loop iteration, and once
  // we've run out of items to freeze, there's nothing left to do.  However,
  // in most cases, we'll break out of this loop long before we hit that many
  // iterations.
  for (uint32_t iterationCounter = 0;
       iterationCounter < mItems.Length(); iterationCounter++) {
    // Set every not-yet-frozen item's used main size to its
    // flex base size, and subtract all the used main sizes from our
    // total amount of space to determine the 'available free space'
    // (positive or negative) to be distributed among our flexible items.
    nscoord availableFreeSpace = spaceAvailableForFlexItemsContentBoxes;
    for (uint32_t i = 0; i < mItems.Length(); i++) {
      FlexItem& item = mItems[i];
      if (!item.IsFrozen()) {
        item.SetMainSize(item.GetFlexBaseSize());
      }
      availableFreeSpace -= item.GetMainSize();
    }

    PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG,
           (" available free space = %d\n", availableFreeSpace));

    // If sign of free space matches the type of flexing that we're doing, give
    // each flexible item a portion of availableFreeSpace.
    if ((availableFreeSpace > 0 && isUsingFlexGrow) ||
        (availableFreeSpace < 0 && !isUsingFlexGrow)) {

      // STRATEGY: On each item, we compute & store its "share" of the total
      // flex weight that we've seen so far:
      //   curFlexWeight / runningFlexWeightSum
      //
      // Then, when we go to actually distribute the space (in the next loop),
      // we can simply walk backwards through the elements and give each item
      // its "share" multiplied by the remaining available space.
      //
      // SPECIAL CASE: If the sum of the flex weights is larger than the
      // maximum representable float (overflowing to infinity), then we can't
      // sensibly divide out proportional shares anymore. In that case, we
      // simply treat the flex item(s) with the largest flex weights as if
      // their weights were infinite (dwarfing all the others), and we
      // distribute all of the available space among them.
      float runningFlexWeightSum = 0.0f;
      float largestFlexWeight = 0.0f;
      uint32_t numItemsWithLargestFlexWeight = 0;
      for (uint32_t i = 0; i < mItems.Length(); i++) {
        FlexItem& item = mItems[i];
        float curFlexWeight = item.GetFlexWeightToUse(isUsingFlexGrow);
        MOZ_ASSERT(curFlexWeight >= 0.0f, "weights are non-negative");

        runningFlexWeightSum += curFlexWeight;
        if (NS_finite(runningFlexWeightSum)) {
          if (curFlexWeight == 0.0f) {
            item.SetShareOfFlexWeightSoFar(0.0f);
          } else {
            item.SetShareOfFlexWeightSoFar(curFlexWeight /
                                           runningFlexWeightSum);
          }
        } // else, the sum of weights overflows to infinity, in which
          // case we don't bother with "SetShareOfFlexWeightSoFar" since
          // we know we won't use it. (instead, we'll just give every
          // item with the largest flex weight an equal share of space.)

        // Update our largest-flex-weight tracking vars
        if (curFlexWeight > largestFlexWeight) {
          largestFlexWeight = curFlexWeight;
          numItemsWithLargestFlexWeight = 1;
        } else if (curFlexWeight == largestFlexWeight) {
          numItemsWithLargestFlexWeight++;
        }
      }

      if (runningFlexWeightSum != 0.0f) { // no distribution if no flexibility
        PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG,
               (" Distributing available space:"));
        for (uint32_t i = mItems.Length() - 1; i < mItems.Length(); --i) {
          FlexItem& item = mItems[i];

          if (!item.IsFrozen()) {
            // To avoid rounding issues, we compute the change in size for this
            // item, and then subtract it from the remaining available space.
            nscoord sizeDelta = 0;
            if (NS_finite(runningFlexWeightSum)) {
              float myShareOfRemainingSpace =
                item.GetShareOfFlexWeightSoFar();

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
            } else if (item.GetFlexWeightToUse(isUsingFlexGrow) ==
                       largestFlexWeight) {
              // Total flexibility is infinite, so we're just distributing
              // the available space equally among the items that are tied for
              // having the largest weight (and this is one of those items).
              sizeDelta =
                NSToCoordRound(availableFreeSpace /
                               float(numItemsWithLargestFlexWeight));
              numItemsWithLargestFlexWeight--;
            }

            availableFreeSpace -= sizeDelta;

            item.SetMainSize(item.GetMainSize() + sizeDelta);
            PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG,
                   ("  child %d receives %d, for a total of %d\n",
                    i, sizeDelta, item.GetMainSize()));
          }
        }
      }
    }

    // Fix min/max violations:
    nscoord totalViolation = 0; // keeps track of adjustments for min/max
    PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG,
           (" Checking for violations:"));

    for (uint32_t i = 0; i < mItems.Length(); i++) {
      FlexItem& item = mItems[i];
      if (!item.IsFrozen()) {
        if (item.GetMainSize() < item.GetMainMinSize()) {
          // min violation
          totalViolation += item.GetMainMinSize() - item.GetMainSize();
          item.SetMainSize(item.GetMainMinSize());
          item.SetHadMinViolation();
        } else if (item.GetMainSize() > item.GetMainMaxSize()) {
          // max violation
          totalViolation += item.GetMainMaxSize() - item.GetMainSize();
          item.SetMainSize(item.GetMainMaxSize());
          item.SetHadMaxViolation();
        }
      }
    }

    FreezeOrRestoreEachFlexibleSize(totalViolation, mItems);

    PR_LOG(GetFlexContainerLog(), PR_LOG_DEBUG,
           (" Total violation: %d\n", totalViolation));

    if (totalViolation == 0) {
      break;
    }
  }

  // Post-condition: all lengths should've been frozen.
#ifdef DEBUG
  for (uint32_t i = 0; i < mItems.Length(); ++i) {
    MOZ_ASSERT(mItems[i].IsFrozen(),
               "All flexible lengths should've been resolved");
  }
#endif // DEBUG
}

MainAxisPositionTracker::
  MainAxisPositionTracker(const FlexboxAxisTracker& aAxisTracker,
                          const nsTArray<FlexItem>& aItems,
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
  for (uint32_t i = 0; i < aItems.Length(); i++) {
    const FlexItem& curItem = aItems[i];
    nscoord itemMarginBoxMainSize =
      curItem.GetMainSize() +
      curItem.GetMarginBorderPaddingSizeInAxis(aAxisTracker.GetMainAxis());
    mPackingSpaceRemaining -= itemMarginBoxMainSize;
    mNumAutoMarginsInMainAxis += curItem.GetNumAutoMarginsInAxis(mAxis);
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

  // Figure out how much space we'll set aside for auto margins or
  // packing spaces, and advance past any leading packing-space.
  if (mNumAutoMarginsInMainAxis == 0 &&
      mPackingSpaceRemaining != 0 &&
      !aItems.IsEmpty()) {
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
        mNumPackingSpacesRemaining = aItems.Length() - 1;
        break;
      case NS_STYLE_JUSTIFY_CONTENT_SPACE_AROUND:
        MOZ_ASSERT(mPackingSpaceRemaining >= 0,
                   "negative packing space should make us use 'center' "
                   "instead of 'space-around'");
        // 1 packing space between each flex item, plus half a packing space
        // at beginning & end.  So our number of full packing-spaces is equal
        // to the number of flex items.
        mNumPackingSpacesRemaining = aItems.Length();
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

SingleLineCrossAxisPositionTracker::
  SingleLineCrossAxisPositionTracker(const FlexboxAxisTracker& aAxisTracker)
  : PositionTracker(aAxisTracker.GetCrossAxis())
{
}

void
FlexLine::ComputeCrossSizeAndBaseline(const FlexboxAxisTracker& aAxisTracker)
{
  nscoord crossStartToFurthestBaseline= nscoord_MIN;
  nscoord crossEndToFurthestBaseline = nscoord_MIN;
  nscoord largestOuterCrossSize = 0;
  for (uint32_t i = 0; i < mItems.Length(); ++i) {
    const FlexItem& curItem = mItems[i];
    nscoord curOuterCrossSize = curItem.GetCrossSize() +
      curItem.GetMarginBorderPaddingSizeInAxis(aAxisTracker.GetCrossAxis());

    if (curItem.GetAlignSelf() == NS_STYLE_ALIGN_ITEMS_BASELINE &&
        curItem.GetNumAutoMarginsInAxis(aAxisTracker.GetCrossAxis()) == 0) {
      // FIXME: Once we support multi-line flexbox with "wrap-reverse", that'll
      // give us bottom-to-top cross axes. (But for now, we assume eAxis_TB.)
      // FIXME: Once we support "writing-mode", we'll have to do baseline
      // alignment in vertical flex containers here (w/ horizontal cross-axes).
      MOZ_ASSERT(aAxisTracker.GetCrossAxis() == eAxis_TB,
                 "Only expecting to do baseline-alignment in horizontal "
                 "flex containers, with top-to-bottom cross axis");

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
        curItem.GetBaselineOffsetFromOuterCrossStart(aAxisTracker.GetCrossAxis());
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

  // The line's baseline is the distance from the cross-start edge to the
  // furthest baseline. (The item(s) with that baseline will be exactly
  // aligned with the line's cross-start edge.)
  mBaselineOffsetFromCrossStart = crossStartToFurthestBaseline;

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
    (aItem.GetCrossSize() + aItem.GetMarginBorderPaddingSizeInAxis(mAxis));

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
                         const FlexItem& aItem)
{
  // We don't do align-self alignment on items that have auto margins
  // in the cross axis.
  if (aItem.GetNumAutoMarginsInAxis(mAxis)) {
    return;
  }

  switch (aItem.GetAlignSelf()) {
    case NS_STYLE_ALIGN_ITEMS_FLEX_START:
    case NS_STYLE_ALIGN_ITEMS_STRETCH:
      // No space to skip over -- we're done.
      // NOTE: 'stretch' behaves like 'flex-start' once we've stretched any
      // auto-sized items (which we've already done).
      break;
    case NS_STYLE_ALIGN_ITEMS_FLEX_END:
      mPosition +=
        aLine.GetLineCrossSize() -
        (aItem.GetCrossSize() +
         aItem.GetMarginBorderPaddingSizeInAxis(mAxis));
      break;
    case NS_STYLE_ALIGN_ITEMS_CENTER:
      // Note: If cross-size is odd, the "after" space will get the extra unit.
      mPosition +=
        (aLine.GetLineCrossSize() -
         (aItem.GetCrossSize() +
          aItem.GetMarginBorderPaddingSizeInAxis(mAxis))) / 2;
      break;
    case NS_STYLE_ALIGN_ITEMS_BASELINE: {
      nscoord lineBaselineOffset =
        aLine.GetBaselineOffsetFromCrossStart();
      nscoord itemBaselineOffset =
        aItem.GetBaselineOffsetFromOuterCrossStart(mAxis);
      MOZ_ASSERT(lineBaselineOffset >= itemBaselineOffset,
                 "failed at finding largest baseline offset");

      // Advance so that aItem's baseline is aligned with the line's baseline.
      mPosition += (lineBaselineOffset - itemBaselineOffset);
      break;
    }
    default:
      NS_NOTREACHED("Unexpected align-self value");
      break;
  }
}

FlexboxAxisTracker::FlexboxAxisTracker(nsFlexContainerFrame* aFlexContainerFrame)
{
  uint32_t flexDirection =
    aFlexContainerFrame->StylePosition()->mFlexDirection;
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

  // FIXME: Once we support "flex-wrap", check if it's "wrap-reverse"
  // here to determine whether we should reverse mCrossAxis.
  MOZ_ASSERT(IsAxisHorizontal(mMainAxis) != IsAxisHorizontal(mCrossAxis),
             "main & cross axes should be in different dimensions");


  // NOTE: Right now, cross axis is never bottom-to-top.
  // The only way for it to be different would be if we used a vertical
  // "writing-mode" or if we had "flex-wrap: wrap-reverse" -- but we don't
  // support either of those yet, so that can't happen right now.
  // (When we add support for either of those properties, this assert will
  // no longer hold.)
  MOZ_ASSERT(mCrossAxis != eAxis_BT, "Not expecting bottom-to-top cross axis");
}

nsresult
nsFlexContainerFrame::GenerateFlexItems(
  nsPresContext* aPresContext,
  const nsHTMLReflowState& aReflowState,
  const FlexboxAxisTracker& aAxisTracker,
  FlexLine& aFlexLine)
{
  MOZ_ASSERT(aFlexLine.mItems.IsEmpty(),
             "Expecting outparam to start out empty");

  aFlexLine.mItems.SetCapacity(mFrames.GetLength());
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    FlexItem* item = aFlexLine.mItems.AppendElement(
                       GenerateFlexItemForChild(aPresContext, e.get(),
                                                aReflowState, aAxisTracker));

    nsresult rv = ResolveFlexItemMaxContentSizing(aPresContext, *item,
                                                  aReflowState, aAxisTracker);
    NS_ENSURE_SUCCESS(rv,rv);
    nscoord itemInnerHypotheticalMainSize = item->GetMainSize();
    nscoord itemOuterHypotheticalMainSize = item->GetMainSize() +
      item->GetMarginBorderPaddingSizeInAxis(aAxisTracker.GetMainAxis());

    // XXXdholbert When we support multi-line, we'll check here if this item's
    // outerHypotheticalMainSize takes us past the end of our line's available
    // space. If so, we'll create a new FlexLine and shift the item there.

    aFlexLine.AddToMainSizeTotals(itemInnerHypotheticalMainSize,
                                  itemOuterHypotheticalMainSize);
  }

  return NS_OK;
}

// Computes the content-box main-size of our flex container.
nscoord
nsFlexContainerFrame::ComputeFlexContainerMainSize(
  const nsHTMLReflowState& aReflowState,
  const FlexboxAxisTracker& aAxisTracker,
  const FlexLine& aLine,
  nscoord aAvailableHeightForContent,
  nsReflowStatus& aStatus)
{
  if (IsAxisHorizontal(aAxisTracker.GetMainAxis())) {
    // Horizontal case is easy -- our main size is our computed width
    // (which is already resolved).
    return aReflowState.ComputedWidth();
  }

  nscoord effectiveComputedHeight = GetEffectiveComputedHeight(aReflowState);
  if (effectiveComputedHeight != NS_INTRINSICSIZE) {
    // Vertical case, with fixed height:
    if (aAvailableHeightForContent == NS_UNCONSTRAINEDSIZE ||
        effectiveComputedHeight < aAvailableHeightForContent) {
      // Not in a fragmenting context, OR no need to fragment because we have
      // more available height than we need. Either way, just use our fixed
      // height.  (Note that the reflow state has already done the appropriate
      // min/max-height clamping.)
      return effectiveComputedHeight;
    }

    // Fragmenting *and* our fixed height is too tall for available height:
    // Mark incomplete so we get a next-in-flow, and take up all of the
    // available height (or the amount of height required by our children, if
    // that's larger; but of course not more than our own computed height).
    // XXXdholbert For now, we don't support pushing children to our next
    // continuation or splitting children, so "amount of height required by
    // our children" is just the sum of our children's heights.
    NS_FRAME_SET_INCOMPLETE(aStatus);
    nscoord sumOfChildHeights = aLine.GetTotalOuterHypotheticalMainSize();
    if (sumOfChildHeights <= aAvailableHeightForContent) {
      return aAvailableHeightForContent;
    }
    return std::min(effectiveComputedHeight, sumOfChildHeights);
  }

  // Vertical case, with auto-height:
  // Resolve auto-height to the sum of our items' hypothetical outer main
  // sizes (their outer heights), clamped to our computed min/max main-size
  // properties (min-height & max-height).
  // XXXdholbert Handle constrained-aAvailableHeightForContent case here.
  nscoord sumOfChildHeights = aLine.GetTotalOuterHypotheticalMainSize();
  return NS_CSS_MINMAX(sumOfChildHeights,
                       aReflowState.mComputedMinHeight,
                       aReflowState.mComputedMaxHeight);
}

nscoord
nsFlexContainerFrame::ComputeFlexContainerCrossSize(
  const nsHTMLReflowState& aReflowState,
  const FlexboxAxisTracker& aAxisTracker,
  nscoord aLineCrossSize,
  nscoord aAvailableHeightForContent,
  bool* aIsDefinite,
  nsReflowStatus& aStatus)
{
  MOZ_ASSERT(aIsDefinite, "outparam pointer must be non-null"); 

  if (IsAxisHorizontal(aAxisTracker.GetCrossAxis())) {
    // Cross axis is horizontal: our cross size is our computed width
    // (which is already resolved).
    *aIsDefinite = true;
    return aReflowState.ComputedWidth();
  }

  nscoord effectiveComputedHeight = GetEffectiveComputedHeight(aReflowState);
  if (effectiveComputedHeight != NS_INTRINSICSIZE) {
    // Cross-axis is vertical, and we have a fixed height:
    *aIsDefinite = true;
    if (aAvailableHeightForContent == NS_UNCONSTRAINEDSIZE ||
        effectiveComputedHeight < aAvailableHeightForContent) {
      // Not in a fragmenting context, OR no need to fragment because we have
      // more available height than we need. Either way, just use our fixed
      // height.  (Note that the reflow state has already done the appropriate
      // min/max-height clamping.)
      return effectiveComputedHeight;
    }

    // Fragmenting *and* our fixed height is too tall for available height:
    // Mark incomplete so we get a next-in-flow, and take up all of the
    // available height (or the amount of height required by our children, if
    // that's larger; but of course not more than our own computed height).
    // XXXdholbert For now, we don't support pushing children to our next
    // continuation or splitting children, so "amount of height required by
    // our children" is just our line-height.
    NS_FRAME_SET_INCOMPLETE(aStatus);
    if (aLineCrossSize <= aAvailableHeightForContent) {
      return aAvailableHeightForContent;
    }
    return std::min(effectiveComputedHeight, aLineCrossSize);
  }

  // Cross axis is vertical and we have auto-height: shrink-wrap our line(s),
  // subject to our min-size / max-size constraints in that (vertical) axis.
  // XXXdholbert Handle constrained-aAvailableHeightForContent case here.
  *aIsDefinite = false;
  return NS_CSS_MINMAX(aLineCrossSize,
                       aReflowState.mComputedMinHeight,
                       aReflowState.mComputedMaxHeight);
}

void
FlexLine::PositionItemsInMainAxis(uint8_t aJustifyContent,
                                  nscoord aContentBoxMainSize,
                                  const FlexboxAxisTracker& aAxisTracker)
{
  MainAxisPositionTracker mainAxisPosnTracker(aAxisTracker, mItems,
                                              aJustifyContent,
                                              aContentBoxMainSize);
  for (uint32_t i = 0; i < mItems.Length(); ++i) {
    FlexItem& item = mItems[i];

    nscoord itemMainBorderBoxSize =
      item.GetMainSize() +
      item.GetBorderPaddingSizeInAxis(mainAxisPosnTracker.GetAxis());

    // Resolve any main-axis 'auto' margins on aChild to an actual value.
    mainAxisPosnTracker.ResolveAutoMarginsInMainAxis(item);

    // Advance our position tracker to child's upper-left content-box corner,
    // and use that as its position in the main axis.
    mainAxisPosnTracker.EnterMargin(item.GetMargin());
    mainAxisPosnTracker.EnterChildFrame(itemMainBorderBoxSize);

    item.SetMainPosition(mainAxisPosnTracker.GetPosition());

    mainAxisPosnTracker.ExitChildFrame(itemMainBorderBoxSize);
    mainAxisPosnTracker.ExitMargin(item.GetMargin());
    mainAxisPosnTracker.TraversePackingSpace();
  }
}

// Helper method to take care of children who ASK_FOR_BASELINE, when
// we need their baseline.
static void
ResolveReflowedChildAscent(nsIFrame* aFrame,
                           nsHTMLReflowMetrics& aChildDesiredSize)
{
  if (aChildDesiredSize.ascent == nsHTMLReflowMetrics::ASK_FOR_BASELINE) {
    // Use GetFirstLineBaseline(), or just GetBaseline() if that fails.
    if (!nsLayoutUtils::GetFirstLineBaseline(aFrame,
                                             &aChildDesiredSize.ascent)) {
      aChildDesiredSize.ascent = aFrame->GetBaseline();
    }
  }
}

nsresult
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
    return NS_OK;
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
  nsHTMLReflowMetrics childDesiredSize;
  nsReflowStatus childReflowStatus;
  nsresult rv = ReflowChild(aItem.Frame(), aPresContext,
                            childDesiredSize, aChildReflowState,
                            0, 0, NS_FRAME_NO_MOVE_FRAME,
                            childReflowStatus);
  aItem.SetHadMeasuringReflow();
  NS_ENSURE_SUCCESS(rv, rv);

  // XXXdholbert Once we do pagination / splitting, we'll need to actually
  // handle incomplete childReflowStatuses. But for now, we give our kids
  // unconstrained available height, which means they should always complete.
  MOZ_ASSERT(NS_FRAME_IS_COMPLETE(childReflowStatus),
             "We gave flex item unconstrained available height, so it "
             "should be complete");

  // Tell the child we're done with its initial reflow.
  // (Necessary for e.g. GetBaseline() to work below w/out asserting)
  rv = FinishReflowChild(aItem.Frame(), aPresContext,
                         &aChildReflowState, childDesiredSize, 0, 0, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  // Save the sizing info that we learned from this reflow
  // -----------------------------------------------------

  // Tentatively store the child's desired content-box cross-size.
  // Note that childDesiredSize is the border-box size, so we have to
  // subtract border & padding to get the content-box size.
  // (Note that at this point in the code, we know our cross axis is vertical,
  // so we don't bother with making aAxisTracker pick the cross-axis component
  // for us.)
  nscoord crossAxisBorderPadding = aItem.GetBorderPadding().TopBottom();
  if (childDesiredSize.height < crossAxisBorderPadding) {
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
    aItem.SetCrossSize(childDesiredSize.height - crossAxisBorderPadding);
  }

  // If we need to do baseline-alignment, store the child's ascent.
  if (aItem.GetAlignSelf() == NS_STYLE_ALIGN_ITEMS_BASELINE) {
    ResolveReflowedChildAscent(aItem.Frame(), childDesiredSize);
    aItem.SetAscent(childDesiredSize.ascent);
  }

  return NS_OK;
}

void
FlexLine::PositionItemsInCrossAxis(nscoord aLineStartPosition,
                                   const FlexboxAxisTracker& aAxisTracker)
{
  SingleLineCrossAxisPositionTracker lineCrossAxisPosnTracker(aAxisTracker);

  for (uint32_t i = 0; i < mItems.Length(); ++i) {
    FlexItem& item = mItems[i];
    // First, stretch the item's cross size (if appropriate), and resolve any
    // auto margins in this axis.
    item.ResolveStretchedCrossSize(mLineCrossSize, aAxisTracker);
    lineCrossAxisPosnTracker.ResolveAutoMarginsInCrossAxis(*this, item);

    // Compute the cross-axis position of this item
    nscoord itemCrossBorderBoxSize =
      item.GetCrossSize() +
      item.GetBorderPaddingSizeInAxis(aAxisTracker.GetCrossAxis());
    lineCrossAxisPosnTracker.EnterAlignPackingSpace(*this, item);
    lineCrossAxisPosnTracker.EnterMargin(item.GetMargin());
    lineCrossAxisPosnTracker.EnterChildFrame(itemCrossBorderBoxSize);

    item.SetCrossPosition(aLineStartPosition +
                          lineCrossAxisPosnTracker.GetPosition());

    // Back out to cross-axis edge of the line.
    lineCrossAxisPosnTracker.ResetPosition();
  }
}

NS_IMETHODIMP
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
    return NS_OK;
  }

  aStatus = NS_FRAME_COMPLETE;

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
  if (!mChildrenHaveBeenReordered) {
    mChildrenHaveBeenReordered =
      SortChildrenIfNeeded<IsOrderLEQ>();
  } else {
    SortChildrenIfNeeded<IsOrderLEQWithDOMFallback>();
  }

  const FlexboxAxisTracker axisTracker(this);

  // Generate an array of our flex items (already sorted), in a FlexLine.
  FlexLine line;
  nsresult rv = GenerateFlexItems(aPresContext, aReflowState,
                                  axisTracker, line);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we're being fragmented into a constrained height, subtract off
  // borderpadding-top from it, to get the available height for our
  // content box. (Don't subtract if we're skipping top border/padding,
  // though.)
  nscoord availableHeightForContent = aReflowState.availableHeight;
  if (availableHeightForContent != NS_UNCONSTRAINEDSIZE &&
      !(GetSkipSides() & (1 << NS_SIDE_TOP))) {
    availableHeightForContent -= aReflowState.mComputedBorderPadding.top;
    // (Don't let that push availableHeightForContent below zero, though):
    availableHeightForContent = std::max(availableHeightForContent, 0);
  }

  const nscoord contentBoxMainSize =
    ComputeFlexContainerMainSize(aReflowState, axisTracker, line,
                                 availableHeightForContent, aStatus);

  line.ResolveFlexibleLengths(contentBoxMainSize);

  // Cross Size Determination - Flexbox spec section 9.4
  // ===================================================
  // Calculate the hypothetical cross size of each item:
  for (uint32_t i = 0; i < line.mItems.Length(); ++i) {
    FlexItem& curItem = line.mItems[i];

    // (If the item's already been stretched, then it already knows its
    // cross size.  Don't bother trying to recalculate it.)
    if (!curItem.IsStretched()) {
      nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                         curItem.Frame(),
                                         nsSize(aReflowState.ComputedWidth(),
                                                NS_UNCONSTRAINEDSIZE));
      // Override computed main-size
      if (IsAxisHorizontal(axisTracker.GetMainAxis())) {
        childReflowState.SetComputedWidth(curItem.GetMainSize());
      } else {
        childReflowState.SetComputedHeight(curItem.GetMainSize());
      }

      nsresult rv = SizeItemInCrossAxis(aPresContext, axisTracker,
                                        childReflowState, curItem);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Calculate the cross size and (if necessary) baseline-alignment position
  // for our (single) flex line:
  line.ComputeCrossSizeAndBaseline(axisTracker);

  // Set up state for cross-axis alignment, at a high level (outside the
  // scope of a particular flex line)
  CrossAxisPositionTracker
    crossAxisPosnTracker(this, axisTracker, aReflowState);

  // XXXdholbert Once we've got multi-line flexbox support: here, after we've
  // computed the cross size of all lines, we need to check if if
  // 'align-content' is 'stretch' -- if it is, we need to give each line an
  // additional share of our flex container's desired cross-size. (if it's
  // not NS_AUTOHEIGHT and there's any cross-size left over to distribute)
  bool isCrossSizeDefinite;
  const nscoord contentBoxCrossSize =
    ComputeFlexContainerCrossSize(aReflowState, axisTracker,
                                  line.GetLineCrossSize(), // XXXdholbert this should take |lines|
                                  availableHeightForContent,
                                  &isCrossSizeDefinite, aStatus);

  if (isCrossSizeDefinite) {
    // XXXdholbert When we support multi-line flex containers, we should
    // distribute any extra space among or between our lines here according
    // to 'align-content'. For now, we do the single-line special behavior:
    // "If the flex container has only a single line (even if it's a multi-line
    // flex container) and has a definite cross size, the cross size of the
    // flex line is the flex container's inner cross size."
    line.SetLineCrossSize(contentBoxCrossSize);
  }

  // Set the flex container's baseline, from its baseline-aligned items.
  // (This might give us nscoord_MIN if we don't have any baseline-aligned
  // flex items.  That's OK, we'll update it below.)
  nscoord flexContainerAscent =
    line.GetBaselineOffsetFromCrossStart();
  if (flexContainerAscent != nscoord_MIN) {
    // Add top borderpadding, so our ascent is w.r.t. border-box
    flexContainerAscent += aReflowState.mComputedBorderPadding.top;
  }

  // Main-Axis Alignment - Flexbox spec section 9.5
  // ==============================================
  line.PositionItemsInMainAxis(aReflowState.mStylePosition->mJustifyContent,
                               contentBoxMainSize,
                               axisTracker);

  // Cross-Axis Alignment - Flexbox spec section 9.6
  // ===============================================
  line.PositionItemsInCrossAxis(crossAxisPosnTracker.GetPosition(),
                                axisTracker);

  // Before giving each child a final reflow, calculate the origin of the
  // flex container's content box (with respect to its border-box), so that
  // we can compute our flex item's final positions.
  nsMargin containerBorderPadding(aReflowState.mComputedBorderPadding);
  ApplySkipSides(containerBorderPadding, &aReflowState);
  const nsPoint containerContentBoxOrigin(containerBorderPadding.left,
                                          containerBorderPadding.top);

  // FINAL REFLOW: Give each child frame another chance to reflow, now that
  // we know its final size and position.
  for (uint32_t i = 0; i < line.mItems.Length(); ++i) {
    FlexItem& curItem = line.mItems[i];

    nsPoint physicalPosn = axisTracker.PhysicalPositionFromLogicalPosition(
                             curItem.GetMainPosition(),
                             curItem.GetCrossPosition(),
                             contentBoxMainSize,
                             contentBoxCrossSize);
    // Adjust physicalPosn to be relative to the container's border-box
    // (i.e. its frame rect), instead of the container's content-box:
    physicalPosn += containerContentBoxOrigin;

    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       curItem.Frame(),
                                       nsSize(aReflowState.ComputedWidth(),
                                              NS_UNCONSTRAINEDSIZE));

    // Keep track of whether we've overriden the child's computed height
    // and/or width, so we can set its resize flags accordingly.
    bool didOverrideComputedWidth = false;
    bool didOverrideComputedHeight = false;

    // Override computed main-size
    if (IsAxisHorizontal(axisTracker.GetMainAxis())) {
      childReflowState.SetComputedWidth(curItem.GetMainSize());
      didOverrideComputedWidth = true;
    } else {
      childReflowState.SetComputedHeight(curItem.GetMainSize());
      didOverrideComputedHeight = true;
    }

    // Override reflow state's computed cross-size, for stretched items.
    if (curItem.IsStretched()) {
      MOZ_ASSERT(curItem.GetAlignSelf() == NS_STYLE_ALIGN_ITEMS_STRETCH,
                 "stretched item w/o 'align-self: stretch'?");
      if (IsAxisHorizontal(axisTracker.GetCrossAxis())) {
        childReflowState.SetComputedWidth(curItem.GetCrossSize());
        didOverrideComputedWidth = true;
      } else {
        // If this item's height is stretched, it's a relative height.
        curItem.Frame()->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
        childReflowState.SetComputedHeight(curItem.GetCrossSize());
        didOverrideComputedHeight = true;
      }
    }

    // XXXdholbert Might need to actually set the correct margins in the
    // reflow state at some point, so that they can be saved on the frame for
    // UsedMarginProperty().  Maybe doesn't matter though...?

    // If we're overriding the computed width or height, *and* we had an
    // earlier "measuring" reflow, then this upcoming reflow needs to be
    // treated as a resize.
    if (curItem.HadMeasuringReflow()) {
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

    nsHTMLReflowMetrics childDesiredSize;
    nsReflowStatus childReflowStatus;
    nsresult rv = ReflowChild(curItem.Frame(), aPresContext,
                              childDesiredSize, childReflowState,
                              physicalPosn.x, physicalPosn.y,
                              0, childReflowStatus);
    NS_ENSURE_SUCCESS(rv, rv);

    // XXXdholbert Once we do pagination / splitting, we'll need to actually
    // handle incomplete childReflowStatuses. But for now, we give our kids
    // unconstrained available height, which means they should always
    // complete.
    MOZ_ASSERT(NS_FRAME_IS_COMPLETE(childReflowStatus),
               "We gave flex item unconstrained available height, so it "
               "should be complete");

    childReflowState.ApplyRelativePositioning(&physicalPosn);

    rv = FinishReflowChild(curItem.Frame(), aPresContext,
                           &childReflowState, childDesiredSize,
                           physicalPosn.x, physicalPosn.y, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    // If this is our first child and we haven't established a baseline for
    // the container yet (i.e. if we don't have 'align-self: baseline' on any
    // children), then use this child's baseline as the container's baseline.
    if (i == 0 && flexContainerAscent == nscoord_MIN) {
      ResolveReflowedChildAscent(curItem.Frame(), childDesiredSize);

      // (We use GetNormalPosition() instead of physicalPosn because we don't
      // want relative positioning on the child to affect the baseline that we
      // read from it).
      flexContainerAscent = curItem.Frame()->GetNormalPosition().y +
        childDesiredSize.ascent;
    }
  }

  nsSize desiredContentBoxSize =
    axisTracker.PhysicalSizeFromLogicalSizes(contentBoxMainSize,
                                             contentBoxCrossSize);

  aDesiredSize.width = desiredContentBoxSize.width +
    containerBorderPadding.LeftRight();
  // Does *NOT* include bottom border/padding yet (we add that a bit lower down)
  aDesiredSize.height = desiredContentBoxSize.height +
    containerBorderPadding.top;

  if (flexContainerAscent == nscoord_MIN) {
    // Still don't have our baseline set -- this happens if we have no
    // children (or if our children are huge enough that they have nscoord_MIN
    // as their baseline... in which case, we'll use the wrong baseline, but no
    // big deal)
    NS_WARN_IF_FALSE(line.mItems.IsEmpty(),
                     "Have flex items but didn't get an ascent - that's odd "
                     "(or there are just gigantic sizes involved)");
    // Per spec, just use the bottom of content-box.
    flexContainerAscent = aDesiredSize.height;
  }
  aDesiredSize.ascent = flexContainerAscent;

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
    // aReflowState.mComputedBorderPadding.bottom, though, so we use that.
    nscoord desiredHeightWithBottomBP =
      aDesiredSize.height + aReflowState.mComputedBorderPadding.bottom;

    if (aReflowState.availableHeight == NS_UNCONSTRAINEDSIZE ||
        aDesiredSize.height == 0 ||
        desiredHeightWithBottomBP <= aReflowState.availableHeight ||
        aReflowState.ComputedHeight() == NS_INTRINSICSIZE) {
      // Update desired height to include bottom border/padding
      aDesiredSize.height = desiredHeightWithBottomBP;
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
  return NS_OK;
}

/* virtual */ nscoord
nsFlexContainerFrame::GetMinWidth(nsRenderingContext* aRenderingContext)
{
  FlexboxAxisTracker axisTracker(this);

  nscoord minWidth = 0;
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    nscoord childMinWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, e.get(),
                                           nsLayoutUtils::MIN_WIDTH);
    if (IsAxisHorizontal(axisTracker.GetMainAxis())) {
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
  // XXXdholbert Optimization: We could cache our intrinsic widths like
  // nsBlockFrame does (and return it early from this function if it's set).
  // Whenever anything happens that might change it, set it to
  // NS_INTRINSIC_WIDTH_UNKNOWN (like nsBlockFrame::MarkIntrinsicWidthsDirty
  // does)
  FlexboxAxisTracker axisTracker(this);

  nscoord prefWidth = 0;
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
