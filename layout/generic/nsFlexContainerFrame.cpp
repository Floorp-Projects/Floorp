/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: flex" */

#include "nsAutoPtr.h"
#include "nsFlexContainerFrame.h"
#include "nsContentUtils.h"
#include "nsCSSAnonBoxes.h"
#include "nsDisplayList.h"
#include "nsIFrameInlines.h"
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsStyleContext.h"
#include "mozilla/Logging.h"
#include <algorithm>
#include "mozilla/LinkedList.h"
#include "mozilla/FloatingPoint.h"
#include "WritingModes.h"

using namespace mozilla;
using namespace mozilla::layout;

// Convenience typedefs for helper classes that we forward-declare in .h file
// (so that nsFlexContainerFrame methods can use them as parameters):
typedef nsFlexContainerFrame::FlexItem FlexItem;
typedef nsFlexContainerFrame::FlexLine FlexLine;
typedef nsFlexContainerFrame::FlexboxAxisTracker FlexboxAxisTracker;
typedef nsFlexContainerFrame::StrutInfo StrutInfo;

static mozilla::LazyLogModule gFlexContainerLog("nsFlexContainerFrame");

// XXXdholbert Some of this helper-stuff should be separated out into a general
// "main/cross-axis utils" header, shared by grid & flexbox?
// (Particularly when grid gets support for align-*/justify-* properties.)

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
// [start, end] physical mozilla::Side values.
static const mozilla::Side
kAxisOrientationToSidesMap[eNumAxisOrientationTypes][eNumAxisEdges] = {
  { eSideLeft,   eSideRight  },  // eAxis_LR
  { eSideRight,  eSideLeft   },  // eAxis_RL
  { eSideTop,    eSideBottom },  // eAxis_TB
  { eSideBottom, eSideTop }      // eAxis_BT
};

// Helper structs / classes / methods
// ==================================
// Returns true iff the given nsStyleDisplay has display:-webkit-{inline-}-box.
static inline bool
IsDisplayValueLegacyBox(const nsStyleDisplay* aStyleDisp)
{
  return aStyleDisp->mDisplay == NS_STYLE_DISPLAY_WEBKIT_BOX ||
    aStyleDisp->mDisplay == NS_STYLE_DISPLAY_WEBKIT_INLINE_BOX;
}

// Helper to check whether our nsFlexContainerFrame is emulating a legacy
// -webkit-{inline-}box, in which case we should use legacy CSS properties
// instead of the modern ones. The params are are the nsStyleDisplay and the
// nsStyleContext associated with the nsFlexContainerFrame itself.
static inline bool
IsLegacyBox(const nsStyleDisplay* aStyleDisp,
            nsStyleContext* aStyleContext)
{
  // Trivial case: just check "display" directly.
  if (IsDisplayValueLegacyBox(aStyleDisp)) {
    return true;
  }

  // If this frame is for a scrollable element, then it will actually have
  // "display:block", and its *parent* will have the real flex-flavored display
  // value. So in that case, check the parent to find out if we're legacy.
  if (aStyleDisp->mDisplay == NS_STYLE_DISPLAY_BLOCK) {
    nsStyleContext* parentStyleContext = aStyleContext->GetParent();
    NS_ASSERTION(parentStyleContext &&
                 aStyleContext->GetPseudo() == nsCSSAnonBoxes::scrolledContent,
                 "The only way a nsFlexContainerFrame can have 'display:block' "
                 "should be if it's the inner part of a scrollable element");
    if (IsDisplayValueLegacyBox(parentStyleContext->StyleDisplay())) {
      return true;
    }
  }

  return false;
}

// Returns the "align-items" value that's equivalent to the legacy "box-align"
// value in the given style struct.
static uint8_t
ConvertLegacyStyleToAlignItems(const nsStyleXUL* aStyleXUL)
{
  // -[moz|webkit]-box-align corresponds to modern "align-items"
  switch (aStyleXUL->mBoxAlign) {
    case NS_STYLE_BOX_ALIGN_STRETCH:
      return NS_STYLE_ALIGN_STRETCH;
    case NS_STYLE_BOX_ALIGN_START:
      return NS_STYLE_ALIGN_FLEX_START;
    case NS_STYLE_BOX_ALIGN_CENTER:
      return NS_STYLE_ALIGN_CENTER;
    case NS_STYLE_BOX_ALIGN_BASELINE:
      return NS_STYLE_ALIGN_BASELINE;
    case NS_STYLE_BOX_ALIGN_END:
      return NS_STYLE_ALIGN_FLEX_END;
  }

  MOZ_ASSERT_UNREACHABLE("Unrecognized mBoxAlign enum value");
  // Fall back to default value of "align-items" property:
  return NS_STYLE_ALIGN_STRETCH;
}

// Returns the "justify-content" value that's equivalent to the legacy
// "box-pack" value in the given style struct.
static uint8_t
ConvertLegacyStyleToJustifyContent(const nsStyleXUL* aStyleXUL)
{
  // -[moz|webkit]-box-pack corresponds to modern "justify-content"
  switch (aStyleXUL->mBoxPack) {
    case NS_STYLE_BOX_PACK_START:
      return NS_STYLE_ALIGN_FLEX_START;
    case NS_STYLE_BOX_PACK_CENTER:
      return NS_STYLE_ALIGN_CENTER;
    case NS_STYLE_BOX_PACK_END:
      return NS_STYLE_ALIGN_FLEX_END;
    case NS_STYLE_BOX_PACK_JUSTIFY:
      return NS_STYLE_ALIGN_SPACE_BETWEEN;
  }

  MOZ_ASSERT_UNREACHABLE("Unrecognized mBoxPack enum value");
  // Fall back to default value of "justify-content" property:
  return NS_STYLE_ALIGN_FLEX_START;
}

// Indicates whether advancing along the given axis is equivalent to
// increasing our X or Y position (as opposed to decreasing it).
static inline bool
AxisGrowsInPositiveDirection(AxisOrientationType aAxis)
{
  return eAxis_LR == aAxis || eAxis_TB == aAxis;
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

/**
 * Converts a "flex-relative" coordinate in a single axis (a main- or cross-axis
 * coordinate) into a coordinate in the corresponding physical (x or y) axis. If
 * the flex-relative axis in question already maps *directly* to a physical
 * axis (i.e. if it's LTR or TTB), then the physical coordinate has the same
 * numeric value as the provided flex-relative coordinate. Otherwise, we have to
 * subtract the flex-relative coordinate from the flex container's size in that
 * axis, to flip the polarity. (So e.g. a main-axis position of 2px in a RTL
 * 20px-wide container would correspond to a physical coordinate (x-value) of
 * 18px.)
 */
static nscoord
PhysicalCoordFromFlexRelativeCoord(nscoord aFlexRelativeCoord,
                                   nscoord aContainerSize,
                                   AxisOrientationType aAxis) {
  if (AxisGrowsInPositiveDirection(aAxis)) {
    return aFlexRelativeCoord;
  }
  return aContainerSize - aFlexRelativeCoord;
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
  (axisTracker_).IsMainAxisHorizontal() ? (width_) : (height_)

#define GET_CROSS_COMPONENT(axisTracker_, width_, height_)  \
  (axisTracker_).IsCrossAxisHorizontal() ? (width_) : (height_)

// Logical versions of helper-macros above:
#define GET_MAIN_COMPONENT_LOGICAL(axisTracker_, wm_, isize_, bsize_)  \
  wm_.IsOrthogonalTo(axisTracker_.GetWritingMode()) != \
    (axisTracker_).IsRowOriented() ? (isize_) : (bsize_)

#define GET_CROSS_COMPONENT_LOGICAL(axisTracker_, wm_, isize_, bsize_)  \
  wm_.IsOrthogonalTo(axisTracker_.GetWritingMode()) != \
    (axisTracker_).IsRowOriented() ? (bsize_) : (isize_)

// Encapsulates our flex container's main & cross axes.
class MOZ_STACK_CLASS nsFlexContainerFrame::FlexboxAxisTracker {
public:
  FlexboxAxisTracker(const nsFlexContainerFrame* aFlexContainer,
                     const WritingMode& aWM);

  // Accessors:
  // XXXdholbert [BEGIN DEPRECATED]
  AxisOrientationType GetMainAxis() const  { return mMainAxis;  }
  AxisOrientationType GetCrossAxis() const { return mCrossAxis; }

  bool IsMainAxisHorizontal() const {
    // If we're row-oriented, and our writing mode is NOT vertical,
    // or we're column-oriented and our writing mode IS vertical,
    // then our main axis is horizontal. This handles all cases:
    return mIsRowOriented != mWM.IsVertical();
  }
  bool IsCrossAxisHorizontal() const {
    return !IsMainAxisHorizontal();
  }
  // XXXdholbert [END DEPRECATED]

  // Returns the flex container's writing mode.
  WritingMode GetWritingMode() const { return mWM; }

  // Returns true if our main axis is in the reverse direction of our
  // writing mode's corresponding axis. (From 'flex-direction: *-reverse')
  bool IsMainAxisReversed() const {
    return mIsMainAxisReversed;
  }
  // Returns true if our cross axis is in the reverse direction of our
  // writing mode's corresponding axis. (From 'flex-wrap: *-reverse')
  bool IsCrossAxisReversed() const {
    return mIsCrossAxisReversed;
  }

  bool IsRowOriented() const { return mIsRowOriented; }
  bool IsColumnOriented() const { return !mIsRowOriented; }

  nscoord GetMainComponent(const nsSize& aSize) const {
    return GET_MAIN_COMPONENT(*this, aSize.width, aSize.height);
  }
  int32_t GetMainComponent(const LayoutDeviceIntSize& aIntSize) const {
    return GET_MAIN_COMPONENT(*this, aIntSize.width, aIntSize.height);
  }

  nscoord GetCrossComponent(const nsSize& aSize) const {
    return GET_CROSS_COMPONENT(*this, aSize.width, aSize.height);
  }
  int32_t GetCrossComponent(const LayoutDeviceIntSize& aIntSize) const {
    return GET_CROSS_COMPONENT(*this, aIntSize.width, aIntSize.height);
  }

  nscoord GetMarginSizeInMainAxis(const nsMargin& aMargin) const {
    return IsMainAxisHorizontal() ?
      aMargin.LeftRight() :
      aMargin.TopBottom();
  }
  nscoord GetMarginSizeInCrossAxis(const nsMargin& aMargin) const {
    return IsCrossAxisHorizontal() ?
      aMargin.LeftRight() :
      aMargin.TopBottom();
  }

  // Returns aFrame's computed value for 'height' or 'width' -- whichever is in
  // the cross-axis. (NOTE: This is cross-axis-specific for now. If we need a
  // main-axis version as well, we could generalize or clone this function.)
  const nsStyleCoord& ComputedCrossSize(const nsIFrame* aFrame) const {
    const nsStylePosition* stylePos = aFrame->StylePosition();

    return IsCrossAxisHorizontal() ?
      stylePos->mWidth :
      stylePos->mHeight;
  }

  /**
   * Converts a "flex-relative" point (a main-axis & cross-axis coordinate)
   * into a LogicalPoint, using the flex container's writing mode.
   *
   *  @arg aMainCoord  The main-axis coordinate -- i.e an offset from the
   *                   main-start edge of the flex container's content box.
   *  @arg aCrossCoord The cross-axis coordinate -- i.e an offset from the
   *                   cross-start edge of the flex container's content box.
   *  @arg aContainerMainSize  The main size of flex container's content box.
   *  @arg aContainerCrossSize The cross size of flex container's content box.
   *  @return A LogicalPoint, with the flex container's writing mode, that
   *          represents the same position. The logical coordinates are
   *          relative to the flex container's content box.
   */
  LogicalPoint
  LogicalPointFromFlexRelativePoint(nscoord aMainCoord,
                                    nscoord aCrossCoord,
                                    nscoord aContainerMainSize,
                                    nscoord aContainerCrossSize) const {
    nscoord logicalCoordInMainAxis = mIsMainAxisReversed ?
      aContainerMainSize - aMainCoord : aMainCoord;
    nscoord logicalCoordInCrossAxis = mIsCrossAxisReversed ?
      aContainerCrossSize - aCrossCoord : aCrossCoord;

    return mIsRowOriented ?
      LogicalPoint(mWM, logicalCoordInMainAxis, logicalCoordInCrossAxis) :
      LogicalPoint(mWM, logicalCoordInCrossAxis, logicalCoordInMainAxis);
  }

  /**
   * Converts a "flex-relative" size (a main-axis & cross-axis size)
   * into a LogicalSize, using the flex container's writing mode.
   *
   *  @arg aMainSize  The main-axis size.
   *  @arg aCrossSize The cross-axis size.
   *  @return A LogicalSize, with the flex container's writing mode, that
   *          represents the same size.
   */
  LogicalSize LogicalSizeFromFlexRelativeSizes(nscoord aMainSize,
                                               nscoord aCrossSize) const {
    return mIsRowOriented ?
      LogicalSize(mWM, aMainSize, aCrossSize) :
      LogicalSize(mWM, aCrossSize, aMainSize);
  }

  // Are my axes reversed with respect to what the author asked for?
  // (We may reverse the axes in the FlexboxAxisTracker constructor and set
  // this flag, to avoid reflowing our children in bottom-to-top order.)
  bool AreAxesInternallyReversed() const
  {
    return mAreAxesInternallyReversed;
  }

private:
  // Delete copy-constructor & reassignment operator, to prevent accidental
  // (unnecessary) copying.
  FlexboxAxisTracker(const FlexboxAxisTracker&) = delete;
  FlexboxAxisTracker& operator=(const FlexboxAxisTracker&) = delete;

  // Helpers for constructor which determine the orientation of our axes, based
  // on legacy box properties (-webkit-box-orient, -webkit-box-direction) or
  // modern flexbox properties (flex-direction, flex-wrap) depending on whether
  // the flex container is a "legacy box" (as determined by IsLegacyBox).
  void InitAxesFromLegacyProps(const nsFlexContainerFrame* aFlexContainer,
                               const WritingMode& aWM);
  void InitAxesFromModernProps(const nsFlexContainerFrame* aFlexContainer,
                               const WritingMode& aWM);

  // XXXdholbert [BEGIN DEPRECATED]
  AxisOrientationType mMainAxis;
  AxisOrientationType mCrossAxis;
  // XXXdholbert [END DEPRECATED]

  const WritingMode mWM; // The flex container's writing mode.

  bool mIsRowOriented; // Is our main axis the inline axis?
                       // (Are we 'flex-direction:row[-reverse]'?)

  bool mIsMainAxisReversed; // Is our main axis in the opposite direction
                            // as mWM's corresponding axis? (e.g. RTL vs LTR)
  bool mIsCrossAxisReversed; // Is our cross axis in the opposite direction
                             // as mWM's corresponding axis? (e.g. BTT vs TTB)

  // Implementation detail -- this indicates whether we've decided to
  // transparently reverse our axes & our child ordering, to avoid having
  // frames flow from bottom to top in either axis (& to make pagination saner).
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
  FlexItem(nsHTMLReflowState& aFlexItemReflowState,
           float aFlexGrow, float aFlexShrink, nscoord aMainBaseSize,
           nscoord aMainMinSize, nscoord aMainMaxSize,
           nscoord aTentativeCrossSize,
           nscoord aCrossMinSize, nscoord aCrossMaxSize,
           const FlexboxAxisTracker& aAxisTracker);

  // Simplified constructor, to be used only for generating "struts":
  // (NOTE: This "strut" constructor uses the *container's* writing mode, which
  // we'll use on this FlexItem instead of the child frame's real writing mode.
  // This is fine - it doesn't matter what writing mode we use for a
  // strut, since it won't render any content and we already know its size.)
  FlexItem(nsIFrame* aChildFrame, nscoord aCrossSize, WritingMode aContainerWM);

  // Accessors
  nsIFrame* Frame() const          { return mFrame; }
  nscoord GetFlexBaseSize() const  { return mFlexBaseSize; }

  nscoord GetMainMinSize() const   {
    MOZ_ASSERT(!mNeedsMinSizeAutoResolution,
               "Someone's using an unresolved 'auto' main min-size");
    return mMainMinSize;
  }
  nscoord GetMainMaxSize() const   { return mMainMaxSize; }

  // Note: These return the main-axis position and size of our *content box*.
  nscoord GetMainSize() const      { return mMainSize; }
  nscoord GetMainPosition() const  { return mMainPosn; }

  nscoord GetCrossMinSize() const  { return mCrossMinSize; }
  nscoord GetCrossMaxSize() const  { return mCrossMaxSize; }

  // Note: These return the cross-axis position and size of our *content box*.
  nscoord GetCrossSize() const     { return mCrossSize;  }
  nscoord GetCrossPosition() const { return mCrossPosn; }

  nscoord ResolvedAscent() const {
    if (mAscent == nsHTMLReflowMetrics::ASK_FOR_BASELINE) {
      // XXXdholbert We should probably be using the *container's* writing-mode
      // here, instead of the item's -- though it doesn't much matter right
      // now, because all of the baseline-handling code here essentially
      // assumes that the container & items have the same writing-mode. This
      // will matter more (& can be expanded/tested) once we officially support
      // logical directions & vertical writing-modes in flexbox, in bug 1079155
      // or a dependency.
      // Use GetFirstLineBaseline(), or just GetBaseline() if that fails.
      if (!nsLayoutUtils::GetFirstLineBaseline(mWM, mFrame, &mAscent)) {
        mAscent = mFrame->GetLogicalBaseline(mWM);
      }
    }
    return mAscent;
  }

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
  // (This function needs to be told which edge we're measuring the baseline
  // from, so that it can look up the appropriate components from mMargin.)
  nscoord GetBaselineOffsetFromOuterCrossEdge(
    AxisEdgeType aEdge,
    const FlexboxAxisTracker& aAxisTracker) const;

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

  // Indicates whether we need to resolve an 'auto' value for the main-axis
  // min-[width|height] property.
  bool NeedsMinSizeAutoResolution() const
    { return mNeedsMinSizeAutoResolution; }

  // Indicates whether this item is a "strut" left behind by an element with
  // visibility:collapse.
  bool IsStrut() const             { return mIsStrut; }

  WritingMode GetWritingMode() const { return mWM; }
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

  const nsSize& IntrinsicRatio() const { return mIntrinsicRatio; }
  bool HasIntrinsicRatio() const { return mIntrinsicRatio != nsSize(); }

  // Getters for margin:
  // ===================
  const nsMargin& GetMargin() const { return mMargin; }

  // Returns the margin component for a given mozilla::Side
  nscoord GetMarginComponentForSide(mozilla::Side aSide) const
  { return mMargin.Side(aSide); }

  // Returns the total space occupied by this item's margins in the given axis
  nscoord GetMarginSizeInAxis(AxisOrientationType aAxis) const
  {
    mozilla::Side startSide = kAxisOrientationToSidesMap[aAxis][eAxisEdge_Start];
    mozilla::Side endSide = kAxisOrientationToSidesMap[aAxis][eAxisEdge_End];
    return GetMarginComponentForSide(startSide) +
      GetMarginComponentForSide(endSide);
  }

  // Getters for border/padding
  // ==========================
  const nsMargin& GetBorderPadding() const { return mBorderPadding; }

  // Returns the border+padding component for a given mozilla::Side
  nscoord GetBorderPaddingComponentForSide(mozilla::Side aSide) const
  { return mBorderPadding.Side(aSide); }

  // Returns the total space occupied by this item's borders and padding in
  // the given axis
  nscoord GetBorderPaddingSizeInAxis(AxisOrientationType aAxis) const
  {
    mozilla::Side startSide = kAxisOrientationToSidesMap[aAxis][eAxisEdge_Start];
    mozilla::Side endSide = kAxisOrientationToSidesMap[aAxis][eAxisEdge_End];
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
  // Helper to set the resolved value of min-[width|height]:auto for the main
  // axis. (Should only be used if NeedsMinSizeAutoResolution() returns true.)
  void UpdateMainMinSize(nscoord aNewMinSize)
  {
    NS_ASSERTION(aNewMinSize >= 0,
                 "How did we end up with a negative min-size?");
    MOZ_ASSERT(mMainMaxSize >= aNewMinSize,
               "Should only use this function for resolving min-size:auto, "
               "and main max-size should be an upper-bound for resolved val");
    MOZ_ASSERT(mNeedsMinSizeAutoResolution &&
               (mMainMinSize == 0 || mFrame->IsThemed(mFrame->StyleDisplay())),
               "Should only use this function for resolving min-size:auto, "
               "so we shouldn't already have a nonzero min-size established "
               "(unless it's a themed-widget-imposed minimum size)");

    if (aNewMinSize > mMainMinSize) {
      mMainMinSize = aNewMinSize;
      // Also clamp main-size to be >= new min-size:
      mMainSize = std::max(mMainSize, aNewMinSize);
    }
    mNeedsMinSizeAutoResolution = false;
  }

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

  // After a FlexItem has had a reflow, this method can be used to cache its
  // (possibly-unresolved) ascent, in case it's needed later for
  // baseline-alignment or to establish the container's baseline.
  // (NOTE: This can be marked 'const' even though it's modifying mAscent,
  // because mAscent is mutable. It's nice for this to be 'const', because it
  // means our final reflow can iterate over const FlexItem pointers, and we
  // can be sure it's not modifying those FlexItems, except via this method.)
  void SetAscent(nscoord aAscent) const {
    mAscent = aAscent; // NOTE: this may be ASK_FOR_BASELINE
  }

  void SetHadMeasuringReflow() {
    mHadMeasuringReflow = true;
  }

  void SetIsStretched() {
    MOZ_ASSERT(mIsFrozen, "main size should be resolved before this");
    mIsStretched = true;
  }

  // Setter for margin components (for resolving "auto" margins)
  void SetMarginComponentForSide(mozilla::Side aSide, nscoord aLength)
  {
    MOZ_ASSERT(mIsFrozen, "main size should be resolved before this");
    mMargin.Side(aSide) = aLength;
  }

  void ResolveStretchedCrossSize(nscoord aLineCrossSize,
                                 const FlexboxAxisTracker& aAxisTracker);

  uint32_t GetNumAutoMarginsInAxis(AxisOrientationType aAxis) const;

  // Once the main size has been resolved, should we bother doing layout to
  // establish the cross size?
  bool CanMainSizeInfluenceCrossSize(const FlexboxAxisTracker& aAxisTracker) const;

protected:
  // Helper called by the constructor, to set mNeedsMinSizeAutoResolution:
  void CheckForMinSizeAuto(const nsHTMLReflowState& aFlexItemReflowState,
                           const FlexboxAxisTracker& aAxisTracker);

  // Our frame:
  nsIFrame* const mFrame;

  // Values that we already know in constructor: (and are hence mostly 'const')
  const float mFlexGrow;
  const float mFlexShrink;

  const nsSize mIntrinsicRatio;

  const nsMargin mBorderPadding;
  nsMargin mMargin; // non-const because we need to resolve auto margins

  // These are non-const so that we can lazily update them with the item's
  // intrinsic size (obtained via a "measuring" reflow), when necessary.
  // (e.g. for "flex-basis:auto;height:auto" & "min-height:auto")
  nscoord mFlexBaseSize;
  nscoord mMainMinSize;
  nscoord mMainMaxSize;

  const nscoord mCrossMinSize;
  const nscoord mCrossMaxSize;

  // Values that we compute after constructor:
  nscoord mMainSize;
  nscoord mMainPosn;
  nscoord mCrossSize;
  nscoord mCrossPosn;
  mutable nscoord mAscent; // Mutable b/c it's set & resolved lazily, sometimes
                           // via const pointer. See comment above SetAscent().

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

  // Does this item need to resolve a min-[width|height]:auto (in main-axis).
  bool mNeedsMinSizeAutoResolution;

  const WritingMode mWM; // The flex item's writing mode.
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

// Convenience function to get either the "order" or the "box-ordinal-group"
// property-value for a flex item (depending on whether the container is a
// modern flex container or a legacy box).
static int32_t
GetOrderOrBoxOrdinalGroup(nsIFrame* aFlexItem, bool aIsLegacyBox)
{
  if (aIsLegacyBox) {
    // We'll be using mBoxOrdinal, which has type uint32_t. However, the modern
    // 'order' property (whose functionality we're co-opting) has type int32_t.
    // So: if we happen to have a uint32_t value that's greater than INT32_MAX,
    // we clamp it rather than letting it overflow. Chances are, this is just
    // an author using BIG_VALUE anyway, so the clamped value should be fine.
    // (particularly since sufficiently-huge values are busted in Chrome/WebKit
    // per https://bugs.chromium.org/p/chromium/issues/detail?id=599645 )
    uint32_t clampedBoxOrdinal = std::min(aFlexItem->StyleXUL()->mBoxOrdinal,
                                          static_cast<uint32_t>(INT32_MAX));
    return static_cast<int32_t>(clampedBoxOrdinal);
  }

  // Normal case: just use modern 'order' property.
  return aFlexItem->StylePosition()->mOrder;
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
        nsCSSAnonBoxes::IsNonElement(pseudoTag)) {    // Text, not a container.
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
        GetFirstNonAnonBoxDescendant(aFrame->GetChildList(kCaptionList).FirstChild());
      if (captionDescendant) {
        return captionDescendant;
      }
    } else if (MOZ_UNLIKELY(aFrame->GetType() == nsGkAtoms::tableFrame)) {
      nsIFrame* colgroupDescendant =
        GetFirstNonAnonBoxDescendant(aFrame->GetChildList(kColGroupList).FirstChild());
      if (colgroupDescendant) {
        return colgroupDescendant;
      }
    }

    // USUAL CASE: Descend to the first child in principal list.
    aFrame = aFrame->PrincipalChildList().FirstChild();
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
  MOZ_ASSERT(aFrame1->GetParent() == aFrame2->GetParent(),
             "this method only intended for comparing siblings");
  nsStyleContext* parentFrameSC = aFrame1->GetParent()->StyleContext();
  bool isInLegacyBox = IsLegacyBox(parentFrameSC->StyleDisplay(),
                                   parentFrameSC);

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

    int32_t order1 = GetOrderOrBoxOrdinalGroup(aRealFrame1, isInLegacyBox);
    int32_t order2 = GetOrderOrBoxOrdinalGroup(aRealFrame2, isInLegacyBox);

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
  nsIAtom* pseudo1 =
    nsPlaceholderFrame::GetRealFrameFor(aFrame1)->StyleContext()->GetPseudo();
  nsIAtom* pseudo2 =
    nsPlaceholderFrame::GetRealFrameFor(aFrame2)->StyleContext()->GetPseudo();

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
  MOZ_ASSERT(aFrame1->GetParent() == aFrame2->GetParent(),
             "this method only intended for comparing siblings");
  nsStyleContext* parentFrameSC = aFrame1->GetParent()->StyleContext();
  bool isInLegacyBox = IsLegacyBox(parentFrameSC->StyleDisplay(),
                                   parentFrameSC);

  // If we've got a placeholder frame, use its out-of-flow frame's 'order' val.
  nsIFrame* aRealFrame1 = nsPlaceholderFrame::GetRealFrameFor(aFrame1);
  nsIFrame* aRealFrame2 = nsPlaceholderFrame::GetRealFrameFor(aFrame2);

  int32_t order1 = GetOrderOrBoxOrdinalGroup(aRealFrame1, isInLegacyBox);
  int32_t order2 = GetOrderOrBoxOrdinalGroup(aRealFrame2, isInLegacyBox);

  return order1 <= order2;
}

bool
nsFlexContainerFrame::IsHorizontal()
{
  const FlexboxAxisTracker axisTracker(this, GetWritingMode());
  return axisTracker.IsMainAxisHorizontal();
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
  nsHTMLReflowState
    childRS(aPresContext, aParentReflowState, aChildFrame,
            aParentReflowState.ComputedSize(aChildFrame->GetWritingMode()));

  // FLEX GROW & SHRINK WEIGHTS
  // --------------------------
  float flexGrow, flexShrink;
  if (IsLegacyBox(aParentReflowState.mStyleDisplay, mStyleContext)) {
    flexGrow = flexShrink = aChildFrame->StyleXUL()->mBoxFlex;
  } else {
    const nsStylePosition* stylePos = aChildFrame->StylePosition();
    flexGrow   = stylePos->mFlexGrow;
    flexShrink = stylePos->mFlexShrink;
  }

  WritingMode childWM = childRS.GetWritingMode();

  // MAIN SIZES (flex base size, min/max size)
  // -----------------------------------------
  nscoord flexBaseSize = GET_MAIN_COMPONENT_LOGICAL(aAxisTracker, childWM,
                                                    childRS.ComputedISize(),
                                                    childRS.ComputedBSize());
  nscoord mainMinSize = GET_MAIN_COMPONENT_LOGICAL(aAxisTracker, childWM,
                                                   childRS.ComputedMinISize(),
                                                   childRS.ComputedMinBSize());
  nscoord mainMaxSize = GET_MAIN_COMPONENT_LOGICAL(aAxisTracker, childWM,
                                                   childRS.ComputedMaxISize(),
                                                   childRS.ComputedMaxBSize());
  // This is enforced by the nsHTMLReflowState where these values come from:
  MOZ_ASSERT(mainMinSize <= mainMaxSize, "min size is larger than max size");

  // CROSS SIZES (tentative cross size, min/max cross size)
  // ------------------------------------------------------
  // Grab the cross size from the reflow state. This might be the right value,
  // or we might resolve it to something else in SizeItemInCrossAxis(); hence,
  // it's tentative. See comment under "Cross Size Determination" for more.
  nscoord tentativeCrossSize =
    GET_CROSS_COMPONENT_LOGICAL(aAxisTracker, childWM,
                                childRS.ComputedISize(),
                                childRS.ComputedBSize());
  nscoord crossMinSize =
    GET_CROSS_COMPONENT_LOGICAL(aAxisTracker, childWM,
                                childRS.ComputedMinISize(),
                                childRS.ComputedMinBSize());
  nscoord crossMaxSize =
    GET_CROSS_COMPONENT_LOGICAL(aAxisTracker, childWM,
                                childRS.ComputedMaxISize(),
                                childRS.ComputedMaxBSize());

  // SPECIAL-CASE FOR WIDGET-IMPOSED SIZES
  // Check if we're a themed widget, in which case we might have a minimum
  // main & cross size imposed by our widget (which we can't go below), or
  // (more severe) our widget might have only a single valid size.
  bool isFixedSizeWidget = false;
  const nsStyleDisplay* disp = aChildFrame->StyleDisplay();
  if (aChildFrame->IsThemed(disp)) {
    LayoutDeviceIntSize widgetMinSize;
    bool canOverride = true;
    aPresContext->GetTheme()->
      GetMinimumWidgetSize(aPresContext, aChildFrame,
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
      tentativeCrossSize = crossMinSize = crossMaxSize = widgetCrossMinSize;
      isFixedSizeWidget = true;
    } else {
      // Variable-size widget: ensure our min/max sizes are at least as large
      // as the widget's mandated minimum size, so we don't flex below that.
      mainMinSize = std::max(mainMinSize, widgetMainMinSize);
      mainMaxSize = std::max(mainMaxSize, widgetMainMinSize);

      if (tentativeCrossSize != NS_INTRINSICSIZE) {
        tentativeCrossSize = std::max(tentativeCrossSize, widgetCrossMinSize);
      }
      crossMinSize = std::max(crossMinSize, widgetCrossMinSize);
      crossMaxSize = std::max(crossMaxSize, widgetCrossMinSize);
    }
  }

  // Construct the flex item!
  FlexItem* item = new FlexItem(childRS,
                                flexGrow, flexShrink, flexBaseSize,
                                mainMinSize, mainMaxSize,
                                tentativeCrossSize,
                                crossMinSize, crossMaxSize,
                                aAxisTracker);

  // If we're inflexible, we can just freeze to our hypothetical main-size
  // up-front. Similarly, if we're a fixed-size widget, we only have one
  // valid size, so we freeze to keep ourselves from flexing.
  if (isFixedSizeWidget || (flexGrow == 0.0f && flexShrink == 0.0f)) {
    item->Freeze();
  }

  // Resolve "flex-basis:auto" and/or "min-[width|height]:auto" (which might
  // require us to reflow the item to measure content height)
  ResolveAutoFlexBasisAndMinSize(aPresContext, *item,
                                 childRS, aAxisTracker);
  return item;
}

// Static helper-functions for ResolveAutoFlexBasisAndMinSize():
// -------------------------------------------------------------
// Indicates whether the cross-size property is set to something definite.
// The logic here should be similar to the logic for isAutoWidth/isAutoHeight
// in nsLayoutUtils::ComputeSizeWithIntrinsicDimensions().
static bool
IsCrossSizeDefinite(const nsHTMLReflowState& aItemReflowState,
                    const FlexboxAxisTracker& aAxisTracker)
{
  const nsStylePosition* pos = aItemReflowState.mStylePosition;
  if (aAxisTracker.IsCrossAxisHorizontal()) {
    return pos->mWidth.GetUnit() != eStyleUnit_Auto;
  }
  // else, vertical. (We need to use IsAutoHeight() to catch e.g. %-height
  // applied to indefinite-height containing block, which counts as auto.)
  nscoord cbHeight = aItemReflowState.mCBReflowState->ComputedHeight();
  return !nsLayoutUtils::IsAutoHeight(pos->mHeight, cbHeight);
}

// If aFlexItem has a definite cross size, this function returns it, for usage
// (in combination with an intrinsic ratio) for resolving the item's main size
// or main min-size.
//
// The parameter "aMinSizeFallback" indicates whether we should fall back to
// returning the cross min-size, when the cross size is indefinite. (This param
// should be set IFF the caller intends to resolve the main min-size.) If this
// param is true, then this function is guaranteed to return a definite value
// (i.e. not NS_AUTOHEIGHT, excluding cases where huge sizes are involved).
//
// XXXdholbert the min-size behavior here is based on my understanding in
//   http://lists.w3.org/Archives/Public/www-style/2014Jul/0053.html
// If my understanding there ends up being wrong, we'll need to update this.
static nscoord
CrossSizeToUseWithRatio(const FlexItem& aFlexItem,
                        const nsHTMLReflowState& aItemReflowState,
                        bool aMinSizeFallback,
                        const FlexboxAxisTracker& aAxisTracker)
{
  if (aFlexItem.IsStretched()) {
    // Definite cross-size, imposed via 'align-self:stretch' & flex container.
    return aFlexItem.GetCrossSize();
  }

  if (IsCrossSizeDefinite(aItemReflowState, aAxisTracker)) {
    // Definite cross size.
    return GET_CROSS_COMPONENT_LOGICAL(aAxisTracker, aFlexItem.GetWritingMode(),
                                       aItemReflowState.ComputedISize(),
                                       aItemReflowState.ComputedBSize());
  }

  if (aMinSizeFallback) {
    // Indefinite cross-size, and we're resolving main min-size, so we'll fall
    // back to ussing the cross min-size (which should be definite).
    return GET_CROSS_COMPONENT_LOGICAL(aAxisTracker, aFlexItem.GetWritingMode(),
                                       aItemReflowState.ComputedMinISize(),
                                       aItemReflowState.ComputedMinBSize());
  }

  // Indefinite cross-size.
  return NS_AUTOHEIGHT;
}

// Convenience function; returns a main-size, given a cross-size and an
// intrinsic ratio. The intrinsic ratio must not have 0 in its cross-axis
// component (or else we'll divide by 0).
static nscoord
MainSizeFromAspectRatio(nscoord aCrossSize,
                        const nsSize& aIntrinsicRatio,
                        const FlexboxAxisTracker& aAxisTracker)
{
  MOZ_ASSERT(aAxisTracker.GetCrossComponent(aIntrinsicRatio) != 0,
             "Invalid ratio; will divide by 0! Caller should've checked...");

  if (aAxisTracker.IsCrossAxisHorizontal()) {
    // cross axis horiz --> aCrossSize is a width. Converting to height.
    return NSCoordMulDiv(aCrossSize, aIntrinsicRatio.height, aIntrinsicRatio.width);
  }
  // cross axis vert --> aCrossSize is a height. Converting to width.
  return NSCoordMulDiv(aCrossSize, aIntrinsicRatio.width, aIntrinsicRatio.height);
}

// Partially resolves "min-[width|height]:auto" and returns the resulting value.
// By "partially", I mean we don't consider the min-content size (but we do
// consider flex-basis, main max-size, and the intrinsic aspect ratio).
// The caller is responsible for computing & considering the min-content size
// in combination with the partially-resolved value that this function returns.
//
// Spec reference: http://dev.w3.org/csswg/css-flexbox/#min-size-auto
static nscoord
PartiallyResolveAutoMinSize(const FlexItem& aFlexItem,
                            const nsHTMLReflowState& aItemReflowState,
                            const FlexboxAxisTracker& aAxisTracker)
{
  MOZ_ASSERT(aFlexItem.NeedsMinSizeAutoResolution(),
             "only call for FlexItems that need min-size auto resolution");

  nscoord minMainSize = nscoord_MAX; // Intentionally huge; we'll shrink it
                                     // from here, w/ std::min().

  // We need the smallest of:
  // * the used flex-basis, if the computed flex-basis was 'auto':
  // XXXdholbert ('auto' might be renamed to 'main-size'; see bug 1032922)
  if (eStyleUnit_Auto ==
      aItemReflowState.mStylePosition->mFlexBasis.GetUnit() &&
      aFlexItem.GetFlexBaseSize() != NS_AUTOHEIGHT) {
    // NOTE: We skip this if the flex base size depends on content & isn't yet
    // resolved. This is OK, because the caller is responsible for computing
    // the min-content height and min()'ing it with the value we return, which
    // is equivalent to what would happen if we min()'d that at this point.
    minMainSize = std::min(minMainSize, aFlexItem.GetFlexBaseSize());
  }

  // * the computed max-width (max-height), if that value is definite:
  nscoord maxSize =
    GET_MAIN_COMPONENT_LOGICAL(aAxisTracker, aFlexItem.GetWritingMode(),
                               aItemReflowState.ComputedMaxISize(),
                               aItemReflowState.ComputedMaxBSize());
  if (maxSize != NS_UNCONSTRAINEDSIZE) {
    minMainSize = std::min(minMainSize, maxSize);
  }

  // * if the item has no intrinsic aspect ratio, its min-content size:
  //  --- SKIPPING THIS IN THIS FUNCTION --- caller's responsibility.

  // * if the item has an intrinsic aspect ratio, the width (height) calculated
  //   from the aspect ratio and any definite size constraints in the opposite
  //   dimension.
  if (aAxisTracker.GetCrossComponent(aFlexItem.IntrinsicRatio()) != 0) {
    // We have a usable aspect ratio. (not going to divide by 0)
    const bool useMinSizeIfCrossSizeIsIndefinite = true;
    nscoord crossSizeToUseWithRatio =
      CrossSizeToUseWithRatio(aFlexItem, aItemReflowState,
                              useMinSizeIfCrossSizeIsIndefinite,
                              aAxisTracker);
    nscoord minMainSizeFromRatio =
      MainSizeFromAspectRatio(crossSizeToUseWithRatio,
                              aFlexItem.IntrinsicRatio(), aAxisTracker);
    minMainSize = std::min(minMainSize, minMainSizeFromRatio);
  }

  return minMainSize;
}

// Resolves flex-basis:auto, using the given intrinsic ratio and the flex
// item's cross size.  On success, updates the flex item with its resolved
// flex-basis and returns true. On failure (e.g. if the ratio is invalid or
// the cross-size is indefinite), returns false.
static bool
ResolveAutoFlexBasisFromRatio(FlexItem& aFlexItem,
                              const nsHTMLReflowState& aItemReflowState,
                              const FlexboxAxisTracker& aAxisTracker)
{
  MOZ_ASSERT(NS_AUTOHEIGHT == aFlexItem.GetFlexBaseSize(),
             "Should only be called to resolve an 'auto' flex-basis");
  // If the flex item has ...
  //  - an intrinsic aspect ratio,
  //  - a [used] flex-basis of 'main-size' [auto?] [We have this, if we're here.]
  //  - a definite cross size
  // then the flex base size is calculated from its inner cross size and the
  // flex item’s intrinsic aspect ratio.
  if (aAxisTracker.GetCrossComponent(aFlexItem.IntrinsicRatio()) != 0) {
    // We have a usable aspect ratio. (not going to divide by 0)
    const bool useMinSizeIfCrossSizeIsIndefinite = false;
    nscoord crossSizeToUseWithRatio =
      CrossSizeToUseWithRatio(aFlexItem, aItemReflowState,
                              useMinSizeIfCrossSizeIsIndefinite,
                              aAxisTracker);
    if (crossSizeToUseWithRatio != NS_AUTOHEIGHT) {
      // We have a definite cross-size
      nscoord mainSizeFromRatio =
        MainSizeFromAspectRatio(crossSizeToUseWithRatio,
                                aFlexItem.IntrinsicRatio(), aAxisTracker);
      aFlexItem.SetFlexBaseSizeAndMainSize(mainSizeFromRatio);
      return true;
    }
  }
  return false;
}

// Note: If & when we handle "min-height: min-content" for flex items,
// we may want to resolve that in this function, too.
void
nsFlexContainerFrame::
  ResolveAutoFlexBasisAndMinSize(nsPresContext* aPresContext,
                                 FlexItem& aFlexItem,
                                 const nsHTMLReflowState& aItemReflowState,
                                 const FlexboxAxisTracker& aAxisTracker)
{
  // (Note: We should never have a used flex-basis of "auto" if our main axis
  // is horizontal; width values should always be resolvable without reflow.)
  const bool isMainSizeAuto = (!aAxisTracker.IsMainAxisHorizontal() &&
                               NS_AUTOHEIGHT == aFlexItem.GetFlexBaseSize());

  const bool isMainMinSizeAuto = aFlexItem.NeedsMinSizeAutoResolution();

  if (!isMainSizeAuto && !isMainMinSizeAuto) {
    // Nothing to do; this function is only needed for flex items
    // with a used flex-basis of "auto" or a min-main-size of "auto".
    return;
  }

  // We may be about to do computations based on our item's cross-size
  // (e.g. using it as a contstraint when measuring our content in the
  // main axis, or using it with the intrinsic ratio to obtain a main size).
  // BEFORE WE DO THAT, we need let the item "pre-stretch" its cross size (if
  // it's got 'align-self:stretch'), for a certain case where the spec says
  // the stretched cross size is considered "definite". That case is if we
  // have a single-line (nowrap) flex container which itself has a definite
  // cross-size.  Otherwise, we'll wait to do stretching, since (in other
  // cases) we don't know how much the item should stretch yet.
  const nsHTMLReflowState* flexContainerRS = aItemReflowState.mParentReflowState;
  MOZ_ASSERT(flexContainerRS,
             "flex item's reflow state should have ptr to container's state");
  if (NS_STYLE_FLEX_WRAP_NOWRAP == flexContainerRS->mStylePosition->mFlexWrap) {
    // XXXdholbert Maybe this should share logic with ComputeCrossSize()...
    // Alternately, maybe tentative container cross size should be passed down.
    nscoord containerCrossSize =
      GET_CROSS_COMPONENT_LOGICAL(aAxisTracker, aAxisTracker.GetWritingMode(),
                                  flexContainerRS->ComputedISize(),
                                  flexContainerRS->ComputedBSize());
    // Is container's cross size "definite"?
    // (Container's cross size is definite if cross-axis is horizontal, or if
    // cross-axis is vertical and the cross-size is not NS_AUTOHEIGHT.)
    if (aAxisTracker.IsCrossAxisHorizontal() ||
        containerCrossSize != NS_AUTOHEIGHT) {
      aFlexItem.ResolveStretchedCrossSize(containerCrossSize, aAxisTracker);
    }
  }

  nscoord resolvedMinSize; // (only set/used if isMainMinSizeAuto==true)
  bool minSizeNeedsToMeasureContent = false; // assume the best
  if (isMainMinSizeAuto) {
    // Resolve the min-size, except for considering the min-content size.
    // (We'll consider that later, if we need to.)
    resolvedMinSize = PartiallyResolveAutoMinSize(aFlexItem, aItemReflowState,
                                                  aAxisTracker);
    if (resolvedMinSize > 0 &&
        aAxisTracker.GetCrossComponent(aFlexItem.IntrinsicRatio()) == 0) {
      // We don't have a usable aspect ratio, so we need to consider our
      // min-content size as another candidate min-size, which we'll have to
      // min() with the current resolvedMinSize.
      // (If resolvedMinSize were already at 0, we could skip this measurement
      // because it can't go any lower. But it's not 0, so we need it.)
      minSizeNeedsToMeasureContent = true;
    }
  }

  bool flexBasisNeedsToMeasureContent = false; // assume the best
  if (isMainSizeAuto) {
    if (!ResolveAutoFlexBasisFromRatio(aFlexItem, aItemReflowState,
                                       aAxisTracker)) {
      flexBasisNeedsToMeasureContent = true;
    }
  }

  // Measure content, if needed (w/ intrinsic-width method or a reflow)
  if (minSizeNeedsToMeasureContent || flexBasisNeedsToMeasureContent) {
    if (aAxisTracker.IsMainAxisHorizontal()) {
      if (minSizeNeedsToMeasureContent) {
        nscoord frameMinISize =
          aFlexItem.Frame()->GetMinISize(aItemReflowState.rendContext);
        resolvedMinSize = std::min(resolvedMinSize, frameMinISize);
      }
      NS_ASSERTION(!flexBasisNeedsToMeasureContent,
                   "flex-basis:auto should have been resolved in the "
                   "reflow state, for horizontal flexbox. It shouldn't need "
                   "special handling here");
    } else {
      // If this item is flexible (vertically), or if we're measuring the
      // 'auto' min-height and our main-size is something else, then we assume
      // that the computed-height we're reflowing with now could be different
      // from the one we'll use for this flex item's "actual" reflow later on.
      // In that case, we need to be sure the flex item treats this as a
      // vertical resize, even though none of its ancestors are necessarily
      // being vertically resized.
      // (Note: We don't have to do this for width, because InitResizeFlags
      // will always turn on mHResize on when it sees that the computed width
      // is different from current width, and that's all we need.)
      bool forceVerticalResizeForMeasuringReflow =
        !aFlexItem.IsFrozen() ||         // Is the item flexible?
        !flexBasisNeedsToMeasureContent; // Are we *only* measuring it for
                                         // 'min-height:auto'?

      nscoord contentHeight =
        MeasureFlexItemContentHeight(aPresContext, aFlexItem,
                                     forceVerticalResizeForMeasuringReflow,
                                     *flexContainerRS);
      if (minSizeNeedsToMeasureContent) {
        resolvedMinSize = std::min(resolvedMinSize, contentHeight);
      }
      if (flexBasisNeedsToMeasureContent) {
        aFlexItem.SetFlexBaseSizeAndMainSize(contentHeight);
      }
    }
  }

  if (isMainMinSizeAuto) {
    aFlexItem.UpdateMainMinSize(resolvedMinSize);
  }
}

nscoord
nsFlexContainerFrame::
  MeasureFlexItemContentHeight(nsPresContext* aPresContext,
                               FlexItem& aFlexItem,
                               bool aForceVerticalResizeForMeasuringReflow,
                               const nsHTMLReflowState& aParentReflowState)
{
  // Set up a reflow state for measuring the flex item's auto-height:
  WritingMode wm = aFlexItem.Frame()->GetWritingMode();
  LogicalSize availSize = aParentReflowState.ComputedSize(wm);
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
  nsHTMLReflowState
    childRSForMeasuringHeight(aPresContext, aParentReflowState,
                              aFlexItem.Frame(), availSize,
                              nullptr, nsHTMLReflowState::CALLER_WILL_INIT);
  childRSForMeasuringHeight.mFlags.mIsFlexContainerMeasuringHeight = true;
  childRSForMeasuringHeight.Init(aPresContext);

  if (aFlexItem.IsStretched()) {
    childRSForMeasuringHeight.SetComputedWidth(aFlexItem.GetCrossSize());
    childRSForMeasuringHeight.SetHResize(true);
  }

  if (aForceVerticalResizeForMeasuringReflow) {
    childRSForMeasuringHeight.SetVResize(true);
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

  aFlexItem.SetHadMeasuringReflow();

  // If this is the first child, save its ascent, since it may be what
  // establishes the container's baseline. Also save the ascent if this child
  // needs to be baseline-aligned. (Else, we don't care about ascent/baseline.)
  if (aFlexItem.Frame() == mFrames.FirstChild() ||
      aFlexItem.GetAlignSelf() == NS_STYLE_ALIGN_BASELINE) {
    aFlexItem.SetAscent(childDesiredSize.BlockStartAscent());
  }

  // Subtract border/padding in vertical axis, to get _just_
  // the effective computed value of the "height" property.
  nscoord childDesiredHeight = childDesiredSize.Height() -
    childRSForMeasuringHeight.ComputedPhysicalBorderPadding().TopBottom();

  return std::max(0, childDesiredHeight);
}

FlexItem::FlexItem(nsHTMLReflowState& aFlexItemReflowState,
                   float aFlexGrow, float aFlexShrink, nscoord aFlexBaseSize,
                   nscoord aMainMinSize,  nscoord aMainMaxSize,
                   nscoord aTentativeCrossSize,
                   nscoord aCrossMinSize, nscoord aCrossMaxSize,
                   const FlexboxAxisTracker& aAxisTracker)
  : mFrame(aFlexItemReflowState.frame),
    mFlexGrow(aFlexGrow),
    mFlexShrink(aFlexShrink),
    mIntrinsicRatio(mFrame->GetIntrinsicRatio()),
    mBorderPadding(aFlexItemReflowState.ComputedPhysicalBorderPadding()),
    mMargin(aFlexItemReflowState.ComputedPhysicalMargin()),
    mMainMinSize(aMainMinSize),
    mMainMaxSize(aMainMaxSize),
    mCrossMinSize(aCrossMinSize),
    mCrossMaxSize(aCrossMaxSize),
    mMainPosn(0),
    mCrossSize(aTentativeCrossSize),
    mCrossPosn(0),
    mAscent(0),
    mShareOfWeightSoFar(0.0f),
    mIsFrozen(false),
    mHadMinViolation(false),
    mHadMaxViolation(false),
    mHadMeasuringReflow(false),
    mIsStretched(false),
    mIsStrut(false),
    // mNeedsMinSizeAutoResolution is initialized in CheckForMinSizeAuto()
    mWM(aFlexItemReflowState.GetWritingMode())
    // mAlignSelf, see below
{
  MOZ_ASSERT(mFrame, "expecting a non-null child frame");
  MOZ_ASSERT(mFrame->GetType() != nsGkAtoms::placeholderFrame,
             "placeholder frames should not be treated as flex items");
  MOZ_ASSERT(!(mFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW),
             "out-of-flow frames should not be treated as flex items");

  const nsHTMLReflowState* containerRS = aFlexItemReflowState.mParentReflowState;
  if (IsLegacyBox(containerRS->mStyleDisplay,
                  containerRS->frame->StyleContext())) {
    // For -webkit-box/-webkit-inline-box, we need to:
    // (1) Use "-webkit-box-align" instead of "align-items" to determine the
    //     container's cross-axis alignment behavior.
    // (2) Suppress the ability for flex items to override that with their own
    //     cross-axis alignment. (The legacy box model doesn't support this.)
    // So, each FlexItem simply copies the container's converted "align-items"
    // value and disregards their own "align-self" property.
    const nsStyleXUL* containerStyleXUL = containerRS->frame->StyleXUL();
    mAlignSelf = ConvertLegacyStyleToAlignItems(containerStyleXUL);
  } else {
    mAlignSelf = aFlexItemReflowState.mStylePosition->ComputedAlignSelf(
                   mFrame->StyleContext()->GetParent());
    if (MOZ_LIKELY(mAlignSelf == NS_STYLE_ALIGN_NORMAL)) {
      mAlignSelf = NS_STYLE_ALIGN_STRETCH;
    }

    // XXX strip off the <overflow-position> bit until we implement that
    mAlignSelf &= ~NS_STYLE_ALIGN_FLAG_BITS;
  }

  SetFlexBaseSizeAndMainSize(aFlexBaseSize);
  CheckForMinSizeAuto(aFlexItemReflowState, aAxisTracker);

  // Assert that any "auto" margin components are set to 0.
  // (We'll resolve them later; until then, we want to treat them as 0-sized.)
#ifdef DEBUG
  {
    const nsStyleSides& styleMargin =
      aFlexItemReflowState.mStyleMargin->mMargin;
    NS_FOR_CSS_SIDES(side) {
      if (styleMargin.GetUnit(side) == eStyleUnit_Auto) {
        MOZ_ASSERT(GetMarginComponentForSide(side) == 0,
                   "Someone else tried to resolve our auto margin");
      }
    }
  }
#endif // DEBUG

  // If the flex item's inline axis is the same as the cross axis, then
  // 'align-self:baseline' is identical to 'flex-start'. If that's the case, we
  // just directly convert our align-self value here, so that we don't have to
  // handle this with special cases elsewhere.
  // Moreover: for the time being (until we support writing-modes),
  // all inline axes are horizontal -- so we can just check if the cross axis
  // is horizontal.
  // FIXME: Once we support writing-mode (vertical text), this
  // IsCrossAxisHorizontal check won't be sufficient anymore -- we'll actually
  // need to compare our inline axis vs. the cross axis.
  if (mAlignSelf == NS_STYLE_ALIGN_BASELINE &&
      aAxisTracker.IsCrossAxisHorizontal()) {
    mAlignSelf = NS_STYLE_ALIGN_FLEX_START;
  }
}

// Simplified constructor for creating a special "strut" FlexItem, for a child
// with visibility:collapse. The strut has 0 main-size, and it only exists to
// impose a minimum cross size on whichever FlexLine it ends up in.
FlexItem::FlexItem(nsIFrame* aChildFrame, nscoord aCrossSize,
                   WritingMode aContainerWM)
  : mFrame(aChildFrame),
    mFlexGrow(0.0f),
    mFlexShrink(0.0f),
    mIntrinsicRatio(),
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
    mNeedsMinSizeAutoResolution(false),
    mWM(aContainerWM),
    mAlignSelf(NS_STYLE_ALIGN_FLEX_START)
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

void
FlexItem::CheckForMinSizeAuto(const nsHTMLReflowState& aFlexItemReflowState,
                              const FlexboxAxisTracker& aAxisTracker)
{
  const nsStylePosition* pos = aFlexItemReflowState.mStylePosition;
  const nsStyleDisplay* disp = aFlexItemReflowState.mStyleDisplay;

  // We'll need special behavior for "min-[width|height]:auto" (whichever is in
  // the main axis) iff:
  // (a) its computed value is "auto"
  // (b) the "overflow" sub-property in the same axis (the main axis) has a
  //     computed value of "visible"
  const nsStyleCoord& minSize = GET_MAIN_COMPONENT(aAxisTracker,
                                                   pos->mMinWidth,
                                                   pos->mMinHeight);

  const uint8_t overflowVal = GET_MAIN_COMPONENT(aAxisTracker,
                                                 disp->mOverflowX,
                                                 disp->mOverflowY);

  mNeedsMinSizeAutoResolution = (minSize.GetUnit() == eStyleUnit_Auto &&
                                 overflowVal == NS_STYLE_OVERFLOW_VISIBLE);
}

nscoord
FlexItem::GetBaselineOffsetFromOuterCrossEdge(
  AxisEdgeType aEdge,
  const FlexboxAxisTracker& aAxisTracker) const
{
  // NOTE: Currently, 'mAscent' (taken from reflow) is an inherently vertical
  // measurement -- it's the distance from the border-top edge of this FlexItem
  // to its baseline. So, we can really only do baseline alignment when the
  // cross axis is vertical. (The FlexItem constructor enforces this when
  // resolving the item's "mAlignSelf" value).
  MOZ_ASSERT(!aAxisTracker.IsCrossAxisHorizontal(),
             "Only expecting to be doing baseline computations when the "
             "cross axis is vertical");

  AxisOrientationType crossAxis = aAxisTracker.GetCrossAxis();
  mozilla::Side sideToMeasureFrom = kAxisOrientationToSidesMap[crossAxis][aEdge];

  nscoord marginTopToBaseline = ResolvedAscent() + mMargin.top;

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
  return GetOuterCrossSize(crossAxis) - marginTopToBaseline;
}

uint32_t
FlexItem::GetNumAutoMarginsInAxis(AxisOrientationType aAxis) const
{
  uint32_t numAutoMargins = 0;
  const nsStyleSides& styleMargin = mFrame->StyleMargin()->mMargin;
  for (uint32_t i = 0; i < eNumAxisEdges; i++) {
    mozilla::Side side = kAxisOrientationToSidesMap[aAxis][i];
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

bool
FlexItem::CanMainSizeInfluenceCrossSize(
  const FlexboxAxisTracker& aAxisTracker) const
{
  if (mIsStretched) {
    // We've already had our cross-size stretched for "align-self:stretch").
    // The container is imposing its cross size on us.
    return false;
  }

  if (mIsStrut) {
    // Struts (for visibility:collapse items) have a predetermined size;
    // no need to measure anything.
    return false;
  }

  if (HasIntrinsicRatio()) {
    // For flex items that have an intrinsic ratio (and maintain it, i.e. are
    // not stretched, which we already checked above): changes to main-size
    // *do* influence the cross size.
    return true;
  }

  if (aAxisTracker.IsCrossAxisHorizontal()) {
    // If the cross axis is horizontal, then changes to the item's main size
    // (height) can't influence its cross size (width), if the item is a block
    // with a horizontal writing-mode.
    // XXXdholbert This doesn't account for vertical writing-modes, items with
    // aspect ratios, items that are multicol elements, & items that are
    // multi-line vertical flex containers. In all of those cases, a change to
    // the height could influence the width.
    return false;
  }

  // Default assumption, if we haven't proven otherwise: the resolved main size
  // *can* change the cross size.
  return true;
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
    mozilla::Side side = kAxisOrientationToSidesMap[mAxis][eAxisEdge_Start];
    mPosition += aMargin.Side(side);
  }

  // Advances our position across the end edge of the given margin, in the axis
  // we're tracking.
  void ExitMargin(const nsMargin& aMargin)
  {
    mozilla::Side side = kAxisOrientationToSidesMap[mAxis][eAxisEdge_End];
    mPosition += aMargin.Side(side);
  }

  // Advances our current position from the start side of a child frame's
  // border-box to the frame's upper or left edge (depending on our axis).
  // (Note that this is a no-op if our axis grows in the same direction as
  // the corresponding logical axis.)
  void EnterChildFrame(nscoord aChildFrameSize)
  {
    if (mIsAxisReversed) {
      mPosition += aChildFrameSize;
    }
  }

  // Advances our current position from a frame's upper or left border-box edge
  // (whichever is in the axis we're tracking) to the 'end' side of the frame
  // in the axis that we're tracking. (Note that this is a no-op if our axis
  // is reversed with respect to the corresponding logical axis.)
  void ExitChildFrame(nscoord aChildFrameSize)
  {
    if (!mIsAxisReversed) {
      mPosition += aChildFrameSize;
    }
  }

protected:
  // Protected constructor, to be sure we're only instantiated via a subclass.
  PositionTracker(AxisOrientationType aAxis, bool aIsAxisReversed)
    : mPosition(0),
      mAxis(aAxis),
      mIsAxisReversed(aIsAxisReversed)
  {}

  // Delete copy-constructor & reassignment operator, to prevent accidental
  // (unnecessary) copying.
  PositionTracker(const PositionTracker&) = delete;
  PositionTracker& operator=(const PositionTracker&) = delete;

  // Member data:
  nscoord mPosition;               // The position we're tracking
  // XXXdholbert [BEGIN DEPRECATED]
  const AxisOrientationType mAxis; // The axis along which we're moving.
  // XXXdholbert [END DEPRECATED]
  const bool mIsAxisReversed; // Is the axis along which we're moving reversed
                              // (e.g. LTR vs RTL) with respect to the
                              // corresponding axis on the flex container's WM?
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
  // XXX this should be uint16_t when we add explicit fallback handling
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
  // = delete, to be sure (at compile time) that no client code can invoke
  // them. (Unlike the other PositionTracker derived classes, this class here
  // deals with FlexLines, not with individual FlexItems or frames.)
  void EnterMargin(const nsMargin& aMargin) = delete;
  void ExitMargin(const nsMargin& aMargin) = delete;
  void EnterChildFrame(nscoord aChildFrameSize) = delete;
  void ExitChildFrame(nscoord aChildFrameSize) = delete;

  nscoord  mPackingSpaceRemaining;
  uint32_t mNumPackingSpacesRemaining;
  // XXX this should be uint16_t when we add explicit fallback handling
  uint8_t  mAlignContent;
};

// Utility class for managing our position along the cross axis, *within* a
// single flex line.
class MOZ_STACK_CLASS SingleLineCrossAxisPositionTracker : public PositionTracker {
public:
  explicit SingleLineCrossAxisPositionTracker(const FlexboxAxisTracker& aAxisTracker);

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
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

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
  // XXXdholbert hacky temporary band-aid for bug 1059138: Trivially pass this
  // assertion (skip it, basically) if the first child is part of a shadow DOM.
  // (IsOrderLEQWithDOMFallback doesn't know how to compare tree-position of a
  // shadow-DOM element vs. a non-shadow-DOM element.)
  NS_ASSERTION(
    (!mFrames.IsEmpty() &&
     mFrames.FirstChild()->GetContent()->GetContainingShadow()) ||
    nsIFrame::IsFrameListSorted<IsOrderLEQWithDOMFallback>(mFrames),
    "Child frames aren't sorted correctly");

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  // Our children are all block-level, so their borders/backgrounds all go on
  // the BlockBorderBackgrounds list.
  nsDisplayListSet childLists(aLists, aLists.BlockBorderBackgrounds());
  for (nsIFrame* childFrame : mFrames) {
    BuildDisplayListForChild(aBuilder, childFrame, aDirtyRect, childLists,
                             GetDisplayFlagsForFlexItem(childFrame));
  }
}

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
  MOZ_LOG(gFlexContainerLog, LogLevel::Debug, ("ResolveFlexibleLengths\n"));

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

    MOZ_LOG(gFlexContainerLog, LogLevel::Debug,
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

          if (IsFinite(weightSum)) {
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

        MOZ_LOG(gFlexContainerLog, LogLevel::Debug,
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
            if (IsFinite(weightSum)) {
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
            MOZ_LOG(gFlexContainerLog, LogLevel::Debug,
                   ("  child %p receives %d, for a total of %d\n",
                    item, sizeDelta, item->GetMainSize()));
          }
        }
      }
    }

    // Fix min/max violations:
    nscoord totalViolation = 0; // keeps track of adjustments for min/max
    MOZ_LOG(gFlexContainerLog, LogLevel::Debug,
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

    MOZ_LOG(gFlexContainerLog, LogLevel::Debug,
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
  : PositionTracker(aAxisTracker.GetMainAxis(),
                    aAxisTracker.IsMainAxisReversed()),
    mPackingSpaceRemaining(aContentBoxMainSize), // we chip away at this below
    mNumAutoMarginsInMainAxis(0),
    mNumPackingSpacesRemaining(0),
    mJustifyContent(aJustifyContent)
{
  // 'normal' behaves as 'stretch', and 'stretch' behaves as 'flex-start',
  // in the main axis
  // https://drafts.csswg.org/css-align-3/#propdef-justify-content
  if (mJustifyContent == NS_STYLE_JUSTIFY_NORMAL ||
      mJustifyContent == NS_STYLE_JUSTIFY_STRETCH) {
    mJustifyContent = NS_STYLE_JUSTIFY_FLEX_START;
  }

  // XXX strip off the <overflow-position> bit until we implement that
  mJustifyContent &= ~NS_STYLE_JUSTIFY_FLAG_BITS;

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
    if (mJustifyContent == NS_STYLE_JUSTIFY_SPACE_BETWEEN) {
      mJustifyContent = NS_STYLE_JUSTIFY_FLEX_START;
    } else if (mJustifyContent == NS_STYLE_JUSTIFY_SPACE_AROUND) {
      mJustifyContent = NS_STYLE_JUSTIFY_CENTER;
    }
  }

  // Map 'start'/'end' to 'flex-start'/'flex-end'.
  if (mJustifyContent == NS_STYLE_JUSTIFY_START) {
    mJustifyContent = NS_STYLE_JUSTIFY_FLEX_START;
  } else if (mJustifyContent == NS_STYLE_JUSTIFY_END) {
    mJustifyContent = NS_STYLE_JUSTIFY_FLEX_END;
  }

  // If our main axis is (internally) reversed, swap the justify-content
  // "flex-start" and "flex-end" behaviors:
  if (aAxisTracker.AreAxesInternallyReversed()) {
    if (mJustifyContent == NS_STYLE_JUSTIFY_FLEX_START) {
      mJustifyContent = NS_STYLE_JUSTIFY_FLEX_END;
    } else if (mJustifyContent == NS_STYLE_JUSTIFY_FLEX_END) {
      mJustifyContent = NS_STYLE_JUSTIFY_FLEX_START;
    }
  }

  // Figure out how much space we'll set aside for auto margins or
  // packing spaces, and advance past any leading packing-space.
  if (mNumAutoMarginsInMainAxis == 0 &&
      mPackingSpaceRemaining != 0 &&
      !aLine->IsEmpty()) {
    switch (mJustifyContent) {
      case NS_STYLE_JUSTIFY_LEFT:
      case NS_STYLE_JUSTIFY_RIGHT:
      case NS_STYLE_JUSTIFY_BASELINE:
      case NS_STYLE_JUSTIFY_LAST_BASELINE:
      case NS_STYLE_JUSTIFY_SPACE_EVENLY:
        NS_WARNING("NYI: justify-content:left/right/baseline/last-baseline/space-evenly");
        MOZ_FALLTHROUGH;
      case NS_STYLE_JUSTIFY_FLEX_START:
        // All packing space should go at the end --> nothing to do here.
        break;
      case NS_STYLE_JUSTIFY_FLEX_END:
        // All packing space goes at the beginning
        mPosition += mPackingSpaceRemaining;
        break;
      case NS_STYLE_JUSTIFY_CENTER:
        // Half the packing space goes at the beginning
        mPosition += mPackingSpaceRemaining / 2;
        break;
      case NS_STYLE_JUSTIFY_SPACE_BETWEEN:
        MOZ_ASSERT(mPackingSpaceRemaining >= 0,
                   "negative packing space should make us use 'flex-start' "
                   "instead of 'space-between'");
        // 1 packing space between each flex item, no packing space at ends.
        mNumPackingSpacesRemaining = aLine->NumItems() - 1;
        break;
      case NS_STYLE_JUSTIFY_SPACE_AROUND:
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
        MOZ_ASSERT_UNREACHABLE("Unexpected justify-content value");
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
      mozilla::Side side = kAxisOrientationToSidesMap[mAxis][i];
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
    MOZ_ASSERT(mJustifyContent == NS_STYLE_JUSTIFY_SPACE_BETWEEN ||
               mJustifyContent == NS_STYLE_JUSTIFY_SPACE_AROUND,
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
  : PositionTracker(aAxisTracker.GetCrossAxis(),
                    aAxisTracker.IsCrossAxisReversed()),
    mPackingSpaceRemaining(0),
    mNumPackingSpacesRemaining(0),
    mAlignContent(aAlignContent)
{
  MOZ_ASSERT(aFirstLine, "null first line pointer");

  // 'normal' behaves as 'stretch'
  if (mAlignContent == NS_STYLE_ALIGN_NORMAL) {
    mAlignContent = NS_STYLE_ALIGN_STRETCH;
  }

  // XXX strip of the <overflow-position> bit until we implement that
  mAlignContent &= ~NS_STYLE_ALIGN_FLAG_BITS;

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
    if (mAlignContent == NS_STYLE_ALIGN_SPACE_BETWEEN ||
        mAlignContent == NS_STYLE_ALIGN_STRETCH) {
      mAlignContent = NS_STYLE_ALIGN_FLEX_START;
    } else if (mAlignContent == NS_STYLE_ALIGN_SPACE_AROUND) {
      mAlignContent = NS_STYLE_ALIGN_CENTER;
    }
  }

  // Map 'start'/'end' to 'flex-start'/'flex-end'.
  if (mAlignContent == NS_STYLE_ALIGN_START) {
    mAlignContent = NS_STYLE_ALIGN_FLEX_START;
  } else if (mAlignContent == NS_STYLE_ALIGN_END) {
    mAlignContent = NS_STYLE_ALIGN_FLEX_END;
  }

  // If our cross axis is (internally) reversed, swap the align-content
  // "flex-start" and "flex-end" behaviors:
  if (aAxisTracker.AreAxesInternallyReversed()) {
    if (mAlignContent == NS_STYLE_ALIGN_FLEX_START) {
      mAlignContent = NS_STYLE_ALIGN_FLEX_END;
    } else if (mAlignContent == NS_STYLE_ALIGN_FLEX_END) {
      mAlignContent = NS_STYLE_ALIGN_FLEX_START;
    }
  }

  // Figure out how much space we'll set aside for packing spaces, and advance
  // past any leading packing-space.
  if (mPackingSpaceRemaining != 0) {
    switch (mAlignContent) {
      case NS_STYLE_JUSTIFY_LEFT:
      case NS_STYLE_JUSTIFY_RIGHT:
      case NS_STYLE_ALIGN_SELF_START:
      case NS_STYLE_ALIGN_SELF_END:
      case NS_STYLE_ALIGN_SPACE_EVENLY:
      case NS_STYLE_ALIGN_BASELINE:
      case NS_STYLE_ALIGN_LAST_BASELINE:
        NS_WARNING("NYI: align-self:left/right/self-start/self-end/space-evenly/baseline/last-baseline");
        MOZ_FALLTHROUGH;
      case NS_STYLE_ALIGN_FLEX_START:
        // All packing space should go at the end --> nothing to do here.
        break;
      case NS_STYLE_ALIGN_FLEX_END:
        // All packing space goes at the beginning
        mPosition += mPackingSpaceRemaining;
        break;
      case NS_STYLE_ALIGN_CENTER:
        // Half the packing space goes at the beginning
        mPosition += mPackingSpaceRemaining / 2;
        break;
      case NS_STYLE_ALIGN_SPACE_BETWEEN:
        MOZ_ASSERT(mPackingSpaceRemaining >= 0,
                   "negative packing space should make us use 'flex-start' "
                   "instead of 'space-between'");
        // 1 packing space between each flex line, no packing space at ends.
        mNumPackingSpacesRemaining = numLines - 1;
        break;
      case NS_STYLE_ALIGN_SPACE_AROUND: {
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
      case NS_STYLE_ALIGN_STRETCH: {
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
        MOZ_ASSERT_UNREACHABLE("Unexpected align-content value");
    }
  }
}

void
CrossAxisPositionTracker::TraversePackingSpace()
{
  if (mNumPackingSpacesRemaining) {
    MOZ_ASSERT(mAlignContent == NS_STYLE_ALIGN_SPACE_BETWEEN ||
               mAlignContent == NS_STYLE_ALIGN_SPACE_AROUND,
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
  : PositionTracker(aAxisTracker.GetCrossAxis(),
                    aAxisTracker.IsCrossAxisReversed())
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

    if (item->GetAlignSelf() == NS_STYLE_ALIGN_BASELINE &&
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
        item->GetBaselineOffsetFromOuterCrossEdge(eAxisEdge_Start,
                                                  aAxisTracker);
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
  if (mAlignSelf != NS_STYLE_ALIGN_STRETCH ||
      GetNumAutoMarginsInAxis(crossAxis) != 0 ||
      eStyleUnit_Auto != aAxisTracker.ComputedCrossSize(mFrame).GetUnit()) {
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
    mozilla::Side side = kAxisOrientationToSidesMap[mAxis][i];
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
  if (alignSelf == NS_STYLE_ALIGN_STRETCH) {
    alignSelf = NS_STYLE_ALIGN_FLEX_START;
  }

  // Map 'start'/'end' to 'flex-start'/'flex-end'.
  if (alignSelf == NS_STYLE_ALIGN_START) {
    alignSelf = NS_STYLE_ALIGN_FLEX_START;
  } else if (alignSelf == NS_STYLE_ALIGN_END) {
    alignSelf = NS_STYLE_ALIGN_FLEX_END;
  }

  // If our cross axis is (internally) reversed, swap the align-self
  // "flex-start" and "flex-end" behaviors:
  if (aAxisTracker.AreAxesInternallyReversed()) {
    if (alignSelf == NS_STYLE_ALIGN_FLEX_START) {
      alignSelf = NS_STYLE_ALIGN_FLEX_END;
    } else if (alignSelf == NS_STYLE_ALIGN_FLEX_END) {
      alignSelf = NS_STYLE_ALIGN_FLEX_START;
    }
  }

  switch (alignSelf) {
    case NS_STYLE_JUSTIFY_LEFT:
    case NS_STYLE_JUSTIFY_RIGHT:
    case NS_STYLE_ALIGN_SELF_START:
    case NS_STYLE_ALIGN_SELF_END:
    case NS_STYLE_ALIGN_LAST_BASELINE:
      NS_WARNING("NYI: align-self:left/right/self-start/self-end/last-baseline");
      MOZ_FALLTHROUGH;
    case NS_STYLE_ALIGN_FLEX_START:
      // No space to skip over -- we're done.
      break;
    case NS_STYLE_ALIGN_FLEX_END:
      mPosition += aLine.GetLineCrossSize() - aItem.GetOuterCrossSize(mAxis);
      break;
    case NS_STYLE_ALIGN_CENTER:
      // Note: If cross-size is odd, the "after" space will get the extra unit.
      mPosition +=
        (aLine.GetLineCrossSize() - aItem.GetOuterCrossSize(mAxis)) / 2;
      break;
    case NS_STYLE_ALIGN_BASELINE: {
      // Normally, baseline-aligned items are collectively aligned with the
      // line's cross-start edge; however, if our cross axis is (internally)
      // reversed, we instead align them with the cross-end edge.
      AxisEdgeType baselineAlignEdge =
        aAxisTracker.AreAxesInternallyReversed() ?
        eAxisEdge_End : eAxisEdge_Start;

      nscoord itemBaselineOffset =
        aItem.GetBaselineOffsetFromOuterCrossEdge(baselineAlignEdge,
                                                  aAxisTracker);

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
      MOZ_ASSERT_UNREACHABLE("Unexpected align-self value");
      break;
  }
}

// Utility function to convert an InlineDir to an AxisOrientationType
static inline AxisOrientationType
InlineDirToAxisOrientation(WritingMode::InlineDir aInlineDir)
{
  switch (aInlineDir) {
    case WritingMode::eInlineLTR:
      return eAxis_LR;
    case WritingMode::eInlineRTL:
      return eAxis_RL;
    case WritingMode::eInlineTTB:
      return eAxis_TB;
    case WritingMode::eInlineBTT:
      return eAxis_BT;
  }

  MOZ_ASSERT_UNREACHABLE("Unhandled InlineDir");
  return eAxis_LR; // in case of unforseen error, assume English LTR text flow.
}

// Utility function to convert a BlockDir to an AxisOrientationType
static inline AxisOrientationType
BlockDirToAxisOrientation(WritingMode::BlockDir aBlockDir)
{
  switch (aBlockDir) {
    case WritingMode::eBlockLR:
      return eAxis_LR;
    case WritingMode::eBlockRL:
      return eAxis_RL;
    case WritingMode::eBlockTB:
      return eAxis_TB;
    // NOTE: WritingMode::eBlockBT (bottom-to-top) does not exist.
  }

  MOZ_ASSERT_UNREACHABLE("Unhandled BlockDir");
  return eAxis_TB; // in case of unforseen error, assume English TTB block-flow
}

FlexboxAxisTracker::FlexboxAxisTracker(
  const nsFlexContainerFrame* aFlexContainer,
  const WritingMode& aWM)
  : mWM(aWM),
    mAreAxesInternallyReversed(false)
{
  if (IsLegacyBox(aFlexContainer->StyleDisplay(),
                  aFlexContainer->StyleContext())) {
    InitAxesFromLegacyProps(aFlexContainer, aWM);
  } else {
    InitAxesFromModernProps(aFlexContainer, aWM);
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
      mIsMainAxisReversed = !mIsMainAxisReversed;
      mIsCrossAxisReversed = !mIsCrossAxisReversed;
    }
  }
}

void
FlexboxAxisTracker::InitAxesFromLegacyProps(
  const nsFlexContainerFrame* aFlexContainer,
  const WritingMode& aWM)
{
  const nsStyleXUL* styleXUL = aFlexContainer->StyleXUL();

  const bool boxOrientIsVertical = (styleXUL->mBoxOrient ==
                                    NS_STYLE_BOX_ORIENT_VERTICAL);
  const bool wmIsVertical = aWM.IsVertical();

  // If box-orient agrees with our writing-mode, then we're "row-oriented"
  // (i.e. the flexbox main axis is the same as our writing mode's inline
  // direction).  Otherwise, we're column-oriented (i.e. the flexbox's main
  // axis is perpendicular to the writing-mode's inline direction).
  mIsRowOriented = (boxOrientIsVertical == wmIsVertical);

  // XXXdholbert BEGIN CODE TO SET DEPRECATED MEMBER-VARS
  if (boxOrientIsVertical) {
    mMainAxis = eAxis_TB;
    mCrossAxis = eAxis_LR;
  } else {
    mMainAxis = eAxis_LR;
    mCrossAxis = eAxis_TB;
  }
  // "direction: rtl" reverses the writing-mode's inline axis.
  // So, we need to reverse the corresponding flex axis to match.
  // (Note this we don't toggle "mIsMainAxisReversed" for this condition,
  // because the main axis will still match aWM's inline direction.)
  if (!aWM.IsBidiLTR()) {
    AxisOrientationType& axisToFlip = mIsRowOriented ? mMainAxis : mCrossAxis;
    axisToFlip = GetReverseAxis(axisToFlip);
  }
  // XXXdholbert END CODE TO SET DEPRECATED MEMBER-VARS

  // Legacy flexbox can use "-webkit-box-direction: reverse" to reverse the
  // main axis (so it runs in the reverse direction of the inline axis):
  if (styleXUL->mBoxDirection == NS_STYLE_BOX_DIRECTION_REVERSE) {
    mMainAxis = GetReverseAxis(mMainAxis);
    mIsMainAxisReversed = true;
  } else {
    mIsMainAxisReversed = false;
  }

  // Legacy flexbox does not support reversing the cross axis -- it has no
  // equivalent of modern flexbox's "flex-wrap: wrap-reverse".
  mIsCrossAxisReversed = false;
}

void
FlexboxAxisTracker::InitAxesFromModernProps(
  const nsFlexContainerFrame* aFlexContainer,
  const WritingMode& aWM)
{
  const nsStylePosition* stylePos = aFlexContainer->StylePosition();
  uint32_t flexDirection = stylePos->mFlexDirection;

  // Inline dimension ("start-to-end"):
  // (NOTE: I'm intentionally not calling these "inlineAxis"/"blockAxis", since
  // those terms have explicit definition in the writing-modes spec, which are
  // the opposite of how I'd be using them here.)
  AxisOrientationType inlineDimension =
    InlineDirToAxisOrientation(mWM.GetInlineDir());
  AxisOrientationType blockDimension =
    BlockDirToAxisOrientation(mWM.GetBlockDir());

  // Determine main axis:
  switch (flexDirection) {
    case NS_STYLE_FLEX_DIRECTION_ROW:
      mMainAxis = inlineDimension;
      mIsRowOriented = true;
      mIsMainAxisReversed = false;
      break;
    case NS_STYLE_FLEX_DIRECTION_ROW_REVERSE:
      mMainAxis = GetReverseAxis(inlineDimension);
      mIsRowOriented = true;
      mIsMainAxisReversed = true;
      break;
    case NS_STYLE_FLEX_DIRECTION_COLUMN:
      mMainAxis = blockDimension;
      mIsRowOriented = false;
      mIsMainAxisReversed = false;
      break;
    case NS_STYLE_FLEX_DIRECTION_COLUMN_REVERSE:
      mMainAxis = GetReverseAxis(blockDimension);
      mIsRowOriented = false;
      mIsMainAxisReversed = true;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected flex-direction value");
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
  if (stylePos->mFlexWrap == NS_STYLE_FLEX_WRAP_WRAP_REVERSE) {
    mCrossAxis = GetReverseAxis(mCrossAxis);
    mIsCrossAxisReversed = true;
  } else {
    mIsCrossAxisReversed = false;
  }
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
  nscoord aAvailableBSizeForContent,
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
    // (e.g. if main axis is vertical & 'height' is 'auto'), make sure we at
    // least wrap when we hit its max main-size.
    if (wrapThreshold == NS_UNCONSTRAINEDSIZE) {
      const nscoord flexContainerMaxMainSize =
        GET_MAIN_COMPONENT_LOGICAL(aAxisTracker, aAxisTracker.GetWritingMode(),
                                   aReflowState.ComputedMaxISize(),
                                   aReflowState.ComputedMaxBSize());

      wrapThreshold = flexContainerMaxMainSize;
    }

    // Also: if we're column-oriented and paginating in the block dimension,
    // we may need to wrap to a new flex line sooner (before we grow past the
    // available BSize, potentially running off the end of the page).
    if (aAxisTracker.IsColumnOriented() &&
        aAvailableBSizeForContent != NS_UNCONSTRAINEDSIZE) {
      wrapThreshold = std::min(wrapThreshold, aAvailableBSizeForContent);
    }
  }

  // Tracks the index of the next strut, in aStruts (and when this hits
  // aStruts.Length(), that means there are no more struts):
  uint32_t nextStrutIdx = 0;

  // Overall index of the current flex item in the flex container. (This gets
  // checked against entries in aStruts.)
  uint32_t itemIdxInContainer = 0;

  for (nsIFrame* childFrame : mFrames) {
    // Honor "page-break-before", if we're multi-line and this line isn't empty:
    if (!isSingleLine && !curLine->IsEmpty() &&
        childFrame->StyleDisplay()->mBreakBefore) {
      curLine = AddNewFlexLineToList(aLines, shouldInsertAtFront);
    }

    nsAutoPtr<FlexItem> item;
    if (nextStrutIdx < aStruts.Length() &&
        aStruts[nextStrutIdx].mItemIdx == itemIdxInContainer) {

      // Use the simplified "strut" FlexItem constructor:
      item = new FlexItem(childFrame, aStruts[nextStrutIdx].mStrutCrossSize,
                          aReflowState.GetWritingMode());
      nextStrutIdx++;
    } else {
      item = GenerateFlexItemForChild(aPresContext, childFrame,
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
  if (aAxisTracker.IsRowOriented()) {
    // Row-oriented --> our main axis is the inline axis, so our main size
    // is our inline size (which should already be resolved).
    // XXXdholbert ISize may be (wrongly) unconstrained right now: bug 1163238
    // Uncomment when that's fixed:
    /*
    NS_WARN_IF_FALSE(aReflowState.ComputedISize() != NS_UNCONSTRAINEDSIZE,
                     "Unconstrained inline size; this should only result from "
                     "huge sizes (not intrinsic sizing w/ orthogonal flows)");
    */
    return aReflowState.ComputedISize();
  }

  // Note: This may be unconstrained, if our block size is "auto":
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

/* Resolves the content-box main-size of a flex container frame,
 * primarily based on:
 * - the "tentative" main size, taken from the reflow state ("tentative"
 *    because it may be unconstrained or may run off the page).
 * - the available BSize (needed if the main axis is the block axis).
 * - the sizes of our lines of flex items.
 *
 * Guaranteed to return a definite length, i.e. not NS_UNCONSTRAINEDSIZE,
 * aside from cases with huge lengths which happen to compute to that value.
 * XXXdholbert (this^ isn't quite true, if we're row-oriented and in an
 * orthogonal flow, per mentions of bug 1163238 in GetMainSizeFromReflowState.)
 *
 * (Note: This function should be structurally similar to 'ComputeCrossSize()',
 * except that here, the caller has already grabbed the tentative size from the
 * reflow state.)
 */
static nscoord
ResolveFlexContainerMainSize(const nsHTMLReflowState& aReflowState,
                             const FlexboxAxisTracker& aAxisTracker,
                             nscoord aTentativeMainSize,
                             nscoord aAvailableBSizeForContent,
                             const FlexLine* aFirstLine,
                             nsReflowStatus& aStatus)
{
  MOZ_ASSERT(aFirstLine, "null first line pointer");

  if (aAxisTracker.IsRowOriented()) {
    // Row-oriented --> our main axis is the inline axis, so our main size
    // is our inline size (which should already be resolved).
    return aTentativeMainSize;
  }

  if (aTentativeMainSize != NS_INTRINSICSIZE) {
    // Column-oriented case, with fixed BSize:
    if (aAvailableBSizeForContent == NS_UNCONSTRAINEDSIZE ||
        aTentativeMainSize < aAvailableBSizeForContent) {
      // Not in a fragmenting context, OR no need to fragment because we have
      // more available BSize than we need. Either way, we don't need to clamp.
      // (Note that the reflow state has already done the appropriate
      // min/max-BSize clamping.)
      return aTentativeMainSize;
    }

    // Fragmenting *and* our fixed BSize is larger than available BSize:
    // Mark incomplete so we get a next-in-flow, and take up all of the
    // available BSize (or the amount of BSize required by our children, if
    // that's larger; but of course not more than our own computed BSize).
    // XXXdholbert For now, we don't support pushing children to our next
    // continuation or splitting children, so "amount of BSize required by
    // our children" is just the main-size (BSize) of our longest flex line.
    NS_FRAME_SET_INCOMPLETE(aStatus);
    nscoord largestLineOuterSize = GetLargestLineMainSize(aFirstLine);

    if (largestLineOuterSize <= aAvailableBSizeForContent) {
      return aAvailableBSizeForContent;
    }
    return std::min(aTentativeMainSize, largestLineOuterSize);
  }

  // Column-oriented case, with auto BSize:
  // Resolve auto BSize to the largest FlexLine length, clamped to our
  // computed min/max main-size properties.
  // XXXdholbert Handle constrained-aAvailableBSizeForContent case here.
  nscoord largestLineOuterSize = GetLargestLineMainSize(aFirstLine);
  return NS_CSS_MINMAX(largestLineOuterSize,
                       aReflowState.ComputedMinBSize(),
                       aReflowState.ComputedMaxBSize());
}

nscoord
nsFlexContainerFrame::ComputeCrossSize(const nsHTMLReflowState& aReflowState,
                                       const FlexboxAxisTracker& aAxisTracker,
                                       nscoord aSumLineCrossSizes,
                                       nscoord aAvailableBSizeForContent,
                                       bool* aIsDefinite,
                                       nsReflowStatus& aStatus)
{
  MOZ_ASSERT(aIsDefinite, "outparam pointer must be non-null");

  if (aAxisTracker.IsColumnOriented()) {
    // Column-oriented --> our cross axis is the inline axis, so our cross size
    // is our inline size (which should already be resolved).
    // XXXdholbert ISize may be (wrongly) unconstrained right now: bug 1163238.
    // Uncomment when that's fixed:
    /*
    NS_WARN_IF_FALSE(aReflowState.ComputedISize() != NS_UNCONSTRAINEDSIZE,
                     "Unconstrained inline size; this should only result from "
                     "huge sizes (not intrinsic sizing w/ orthogonal flows)");
    */
    *aIsDefinite = true;
    return aReflowState.ComputedISize();
  }

  nscoord effectiveComputedBSize = GetEffectiveComputedBSize(aReflowState);
  if (effectiveComputedBSize != NS_INTRINSICSIZE) {
    // Row-oriented case (cross axis is block-axis), with fixed BSize:
    *aIsDefinite = true;
    if (aAvailableBSizeForContent == NS_UNCONSTRAINEDSIZE ||
        effectiveComputedBSize < aAvailableBSizeForContent) {
      // Not in a fragmenting context, OR no need to fragment because we have
      // more available BSize than we need. Either way, just use our fixed
      // BSize.  (Note that the reflow state has already done the appropriate
      // min/max-BSize clamping.)
      return effectiveComputedBSize;
    }

    // Fragmenting *and* our fixed BSize is too tall for available BSize:
    // Mark incomplete so we get a next-in-flow, and take up all of the
    // available BSize (or the amount of BSize required by our children, if
    // that's larger; but of course not more than our own computed BSize).
    // XXXdholbert For now, we don't support pushing children to our next
    // continuation or splitting children, so "amount of BSize required by
    // our children" is just the sum of our FlexLines' BSizes (cross sizes).
    NS_FRAME_SET_INCOMPLETE(aStatus);
    if (aSumLineCrossSizes <= aAvailableBSizeForContent) {
      return aAvailableBSizeForContent;
    }
    return std::min(effectiveComputedBSize, aSumLineCrossSizes);
  }

  // Row-oriented case (cross axis is block axis), with auto BSize:
  // Shrink-wrap our line(s), subject to our min-size / max-size
  // constraints in that (block) axis.
  // XXXdholbert Handle constrained-aAvailableBSizeForContent case here.
  *aIsDefinite = false;
  return NS_CSS_MINMAX(aSumLineCrossSizes,
                       aReflowState.ComputedMinBSize(),
                       aReflowState.ComputedMaxBSize());
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

/**
 * Given the flex container's "flex-relative ascent" (i.e. distance from the
 * flex container's content-box cross-start edge to its baseline), returns
 * its actual physical ascent value (the distance from the *border-box* top
 * edge to its baseline).
 */
static nscoord
ComputePhysicalAscentFromFlexRelativeAscent(
  nscoord aFlexRelativeAscent,
  nscoord aContentBoxCrossSize,
  const nsHTMLReflowState& aReflowState,
  const FlexboxAxisTracker& aAxisTracker)
{
  return aReflowState.ComputedPhysicalBorderPadding().top +
    PhysicalCoordFromFlexRelativeCoord(aFlexRelativeAscent,
                                       aContentBoxCrossSize,
                                       aAxisTracker.GetCrossAxis());
}

void
nsFlexContainerFrame::SizeItemInCrossAxis(
  nsPresContext* aPresContext,
  const FlexboxAxisTracker& aAxisTracker,
  nsHTMLReflowState& aChildReflowState,
  FlexItem& aItem)
{
  if (aAxisTracker.IsCrossAxisHorizontal()) {
    MOZ_ASSERT(aItem.HasIntrinsicRatio(),
               "For now, caller's CanMainSizeInfluenceCrossSize check should "
               "only allow us to get here for items with intrinsic ratio");
    // XXXdholbert When we finish support for vertical writing-modes,
    // (in bug 1079155 or a dependency), we'll relax the horizontal check in
    // CanMainSizeInfluenceCrossSize, and this function will need to be able
    // to measure the baseline & width (given our resolved height)
    // of vertical-writing-mode flex items here.
    // For now, we only expect to get here for items with an intrinsic aspect
    // ratio; and for those items, we can just read the size off of the reflow
    // state, without performing reflow.
    aItem.SetCrossSize(aChildReflowState.ComputedWidth());
    return;
  }

  MOZ_ASSERT(!aItem.HadMeasuringReflow(),
             "We shouldn't need more than one measuring reflow");

  if (aItem.GetAlignSelf() == NS_STYLE_ALIGN_STRETCH) {
    // This item's got "align-self: stretch", so we probably imposed a
    // stretched computed height on it during its previous reflow. We're
    // not imposing that height for *this* measuring reflow, so we need to
    // tell it to treat this reflow as a vertical resize (regardless of
    // whether any of its ancestors are being resized).
    aChildReflowState.SetVResize(true);
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

  // If this is the first child, save its ascent, since it may be what
  // establishes the container's baseline. Also save the ascent if this child
  // needs to be baseline-aligned. (Else, we don't care about baseline/ascent.)
  if (aItem.Frame() == mFrames.FirstChild() ||
      aItem.GetAlignSelf() == NS_STYLE_ALIGN_BASELINE) {
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
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsFlexContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  MOZ_LOG(gFlexContainerLog, LogLevel::Debug,
         ("Reflow() for nsFlexContainerFrame %p\n", this));

  if (IsFrameTreeTooDeep(aReflowState, aDesiredSize, aStatus)) {
    return;
  }

  // We (and our children) can only depend on our ancestor's bsize if we have
  // a percent-bsize, or if we're positioned and we have "block-start" and "block-end"
  // set and have block-size:auto.  (There are actually other cases, too -- e.g. if
  // our parent is itself a block-dir flex container and we're flexible -- but
  // we'll let our ancestors handle those sorts of cases.)
  WritingMode wm = aReflowState.GetWritingMode();
  const nsStylePosition* stylePos = StylePosition();
  const nsStyleCoord& bsize = stylePos->BSize(wm);
  if (bsize.HasPercent() ||
      (StyleDisplay()->IsAbsolutelyPositionedStyle() &&
       eStyleUnit_Auto == bsize.GetUnit() &&
       eStyleUnit_Auto != stylePos->mOffset.GetBStartUnit(wm) &&
       eStyleUnit_Auto != stylePos->mOffset.GetBEndUnit(wm))) {
    AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
  }

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

  const FlexboxAxisTracker axisTracker(this, aReflowState.GetWritingMode());

  // If we're being fragmented into a constrained BSize, then subtract off
  // borderpadding BStart from that constrained BSize, to get the available
  // BSize for our content box. (No need to subtract the borderpadding BStart
  // if we're already skipping it via GetLogicalSkipSides, though.)
  nscoord availableBSizeForContent = aReflowState.AvailableBSize();
  if (availableBSizeForContent != NS_UNCONSTRAINEDSIZE &&
      !(GetLogicalSkipSides(&aReflowState).BStart())) {
    availableBSizeForContent -=
      aReflowState.ComputedLogicalBorderPadding().BStart(wm);
    // (Don't let that push availableBSizeForContent below zero, though):
    availableBSizeForContent = std::max(availableBSizeForContent, 0);
  }

  nscoord contentBoxMainSize = GetMainSizeFromReflowState(aReflowState,
                                                          axisTracker);

  AutoTArray<StrutInfo, 1> struts;
  DoFlexLayout(aPresContext, aDesiredSize, aReflowState, aStatus,
               contentBoxMainSize, availableBSizeForContent,
               struts, axisTracker);

  if (!struts.IsEmpty()) {
    // We're restarting flex layout, with new knowledge of collapsed items.
    DoFlexLayout(aPresContext, aDesiredSize, aReflowState, aStatus,
                 contentBoxMainSize, availableBSizeForContent,
                 struts, axisTracker);
  }
}

// RAII class to clean up a list of FlexLines.
// Specifically, this removes each line from the list, deletes all the
// FlexItems in its list, and deletes the FlexLine.
class MOZ_RAII AutoFlexLineListClearer
{
public:
  explicit AutoFlexLineListClearer(LinkedList<FlexLine>& aLines
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

// Class to let us temporarily provide an override value for the the main-size
// CSS property ('width' or 'height') on a flex item, for use in
// nsLayoutUtils::ComputeSizeWithIntrinsicDimensions.
// (We could use this overridden size more broadly, too, but it's probably
// better to avoid property-table accesses.  So, where possible, we communicate
// the resolved main-size to the child via modifying its reflow state directly,
// instead of using this class.)
class MOZ_RAII AutoFlexItemMainSizeOverride final
{
public:
  explicit AutoFlexItemMainSizeOverride(FlexItem& aItem
                                        MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mItemProps(aItem.Frame()->Properties())
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    MOZ_ASSERT(!mItemProps.Has(nsIFrame::FlexItemMainSizeOverride()),
               "FlexItemMainSizeOverride prop shouldn't be set already; "
               "it should only be set temporarily (& not recursively)");
    NS_ASSERTION(aItem.HasIntrinsicRatio(),
                 "This should only be needed for items with an aspect ratio");

    mItemProps.Set(nsIFrame::FlexItemMainSizeOverride(), aItem.GetMainSize());
  }

  ~AutoFlexItemMainSizeOverride() {
    mItemProps.Remove(nsIFrame::FlexItemMainSizeOverride());
  }

private:
  const FrameProperties mItemProps;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

void
nsFlexContainerFrame::DoFlexLayout(nsPresContext*           aPresContext,
                                   nsHTMLReflowMetrics&     aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   nscoord aContentBoxMainSize,
                                   nscoord aAvailableBSizeForContent,
                                   nsTArray<StrutInfo>& aStruts,
                                   const FlexboxAxisTracker& aAxisTracker)
{
  aStatus = NS_FRAME_COMPLETE;

  LinkedList<FlexLine> lines;
  AutoFlexLineListClearer cleanupLines(lines);

  GenerateFlexLines(aPresContext, aReflowState,
                    aContentBoxMainSize,
                    aAvailableBSizeForContent,
                    aStruts, aAxisTracker, lines);

  aContentBoxMainSize =
    ResolveFlexContainerMainSize(aReflowState, aAxisTracker,
                                 aContentBoxMainSize, aAvailableBSizeForContent,
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
      // The item may already have the correct cross-size; only recalculate
      // if the item's main size resolution (flexing) could have influenced it:
      if (item->CanMainSizeInfluenceCrossSize(aAxisTracker)) {
        Maybe<AutoFlexItemMainSizeOverride> sizeOverride;
        if (item->HasIntrinsicRatio()) {
          // For flex items with an aspect ratio, we have to impose an override
          // for the main-size property *before* we even instantiate the reflow
          // state, in order for aspect ratio calculations to produce the right
          // cross size in the reflow state. (For other flex items, it's OK
          // (and cheaper) to impose our main size *after* the reflow state has
          // been constructed, since the main size shouldn't influence anything
          // about cross-size measurement until we actually reflow the child.)
          sizeOverride.emplace(*item);
        }

        WritingMode wm = item->Frame()->GetWritingMode();
        LogicalSize availSize = aReflowState.ComputedSize(wm);
        availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
        nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                           item->Frame(), availSize);
        if (!sizeOverride) {
          // Directly override the computed main-size, by tweaking reflow state:
          if (aAxisTracker.IsMainAxisHorizontal()) {
            childReflowState.SetComputedWidth(item->GetMainSize());
          } else {
            childReflowState.SetComputedHeight(item->GetMainSize());
          }
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
                     aAvailableBSizeForContent, &isCrossSizeDefinite, aStatus);

  // Set up state for cross-axis alignment, at a high level (outside the
  // scope of a particular flex line)
  CrossAxisPositionTracker
    crossAxisPosnTracker(lines.getFirst(),
                         aReflowState.mStylePosition->ComputedAlignContent(),
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
        ComputePhysicalAscentFromFlexRelativeAscent(
          crossAxisPosnTracker.GetPosition() + firstLineBaselineOffset,
          contentBoxCrossSize, aReflowState, aAxisTracker);
    }
  }

  const auto justifyContent = IsLegacyBox(aReflowState.mStyleDisplay,
                                          mStyleContext) ?
    ConvertLegacyStyleToJustifyContent(StyleXUL()) :
    aReflowState.mStylePosition->ComputedJustifyContent();

  for (FlexLine* line = lines.getFirst(); line; line = line->getNext()) {
    // Main-Axis Alignment - Flexbox spec section 9.5
    // ==============================================
    line->PositionItemsInMainAxis(justifyContent,
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
        ComputePhysicalAscentFromFlexRelativeAscent(
          crossAxisPosnTracker.GetPosition() - lastLineBaselineOffset,
          contentBoxCrossSize, aReflowState, aAxisTracker);
    }
  }

  // Before giving each child a final reflow, calculate the origin of the
  // flex container's content box (with respect to its border-box), so that
  // we can compute our flex item's final positions.
  WritingMode flexWM = aReflowState.GetWritingMode();
  LogicalMargin containerBP = aReflowState.ComputedLogicalBorderPadding();

  // Unconditionally skip block-end border & padding for now, regardless of
  // writing-mode/GetLogicalSkipSides.  We add it lower down, after we've
  // established baseline and decided whether bottom border-padding fits (if
  // we're fragmented).
  const nscoord blockEndContainerBP = containerBP.BEnd(flexWM);
  const LogicalSides skipSides =
    GetLogicalSkipSides(&aReflowState) | LogicalSides(eLogicalSideBitsBEnd);
  containerBP.ApplySkipSides(skipSides);

  const LogicalPoint containerContentBoxOrigin(flexWM,
                                               containerBP.IStart(flexWM),
                                               containerBP.BStart(flexWM));

  // Determine flex container's border-box size (used in positioning children):
  LogicalSize logSize =
    aAxisTracker.LogicalSizeFromFlexRelativeSizes(aContentBoxMainSize,
                                                  contentBoxCrossSize);
  logSize += aReflowState.ComputedLogicalBorderPadding().Size(flexWM);
  nsSize containerSize = logSize.GetPhysicalSize(flexWM);

  // FINAL REFLOW: Give each child frame another chance to reflow, now that
  // we know its final size and position.
  for (const FlexLine* line = lines.getFirst(); line; line = line->getNext()) {
    for (const FlexItem* item = line->GetFirstItem(); item;
         item = item->getNext()) {
      LogicalPoint framePos = aAxisTracker.LogicalPointFromFlexRelativePoint(
                               item->GetMainPosition(),
                               item->GetCrossPosition(),
                               aContentBoxMainSize,
                               contentBoxCrossSize);
      // Adjust framePos to be relative to the container's border-box
      // (i.e. its frame rect), instead of the container's content-box:
      framePos += containerContentBoxOrigin;

      // (Intentionally snapshotting this before ApplyRelativePositioning, to
      // maybe use for setting the flex container's baseline.)
      const nscoord itemNormalBPos = framePos.B(flexWM);

      // Check if we actually need to reflow the item -- if we already reflowed
      // it with the right size, we can just reposition it as-needed.
      bool itemNeedsReflow = true; // (Start out assuming the worst.)
      if (item->HadMeasuringReflow()) {
        LogicalSize finalFlexItemCBSize =
          aAxisTracker.LogicalSizeFromFlexRelativeSizes(item->GetMainSize(),
                                                        item->GetCrossSize());
        // We've already reflowed the child once. Was the size we gave it in
        // that reflow the same as its final (post-flexing/stretching) size?
        if (finalFlexItemCBSize ==
            LogicalSize(flexWM,
                        item->Frame()->GetContentRectRelativeToSelf().Size())) {
          // Even if our size hasn't changed, some of our descendants might
          // care that our bsize is now considered "definite" (whereas it
          // wasn't in our previous "measuring" reflow), if they have a
          // relative bsize.
          if (!(item->Frame()->GetStateBits() &
                NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
            // Item has the correct size (and its children don't care that
            // it's now "definite"). Let's just make sure it's at the right
            // position.
            itemNeedsReflow = false;
            MoveFlexItemToFinalPosition(aReflowState, *item, framePos,
                                        containerSize);
          }
        }
      }
      if (itemNeedsReflow) {
        ReflowFlexItem(aPresContext, aAxisTracker, aReflowState,
                       *item, framePos, containerSize);
      }

      // If this is our first child and we haven't established a baseline for
      // the container yet (i.e. if we don't have 'align-self: baseline' on any
      // children), then use this child's baseline as the container's baseline.
      if (item->Frame() == mFrames.FirstChild() &&
          flexContainerAscent == nscoord_MIN) {
        flexContainerAscent = itemNormalBPos + item->ResolvedAscent();
      }
    }
  }

  // Compute flex container's desired size (in its own writing-mode),
  // starting w/ content-box size & growing from there:
  LogicalSize desiredSizeInFlexWM =
    aAxisTracker.LogicalSizeFromFlexRelativeSizes(aContentBoxMainSize,
                                                  contentBoxCrossSize);
  // Add border/padding (w/ skipSides already applied):
  desiredSizeInFlexWM.ISize(flexWM) += containerBP.IStartEnd(flexWM);
  desiredSizeInFlexWM.BSize(flexWM) += containerBP.BStartEnd(flexWM);

  if (flexContainerAscent == nscoord_MIN) {
    // Still don't have our baseline set -- this happens if we have no
    // children (or if our children are huge enough that they have nscoord_MIN
    // as their baseline... in which case, we'll use the wrong baseline, but no
    // big deal)
    NS_WARN_IF_FALSE(lines.getFirst()->IsEmpty(),
                     "Have flex items but didn't get an ascent - that's odd "
                     "(or there are just gigantic sizes involved)");
    // Per spec, synthesize baseline from the flex container's content box
    // (i.e. use block-end side of content-box)
    // XXXdholbert This only makes sense if parent's writing mode is
    // horizontal (& even then, really we should be using the BSize in terms
    // of the parent's writing mode, not ours). Clean up in bug 1155322.
    flexContainerAscent = desiredSizeInFlexWM.BSize(flexWM);
  }

  // XXXdholbert flexContainerAscent needs to be in terms of
  // our parent's writing-mode here. See bug 1155322.
  aDesiredSize.SetBlockStartAscent(flexContainerAscent);

  // Now: If we're complete, add bottom border/padding to desired height (which
  // we skipped via skipSides) -- unless that pushes us over available height,
  // in which case we become incomplete (unless we already weren't asking for
  // any height, in which case we stay complete to avoid looping forever).
  // NOTE: If we're auto-height, we allow our bottom border/padding to push us
  // over the available height without requesting a continuation, for
  // consistency with the behavior of "display:block" elements.
  if (NS_FRAME_IS_COMPLETE(aStatus)) {
    nscoord desiredBSizeWithBEndBP =
      desiredSizeInFlexWM.BSize(flexWM) + blockEndContainerBP;

    if (aReflowState.AvailableBSize() == NS_UNCONSTRAINEDSIZE ||
        desiredSizeInFlexWM.BSize(flexWM) == 0 ||
        desiredBSizeWithBEndBP <= aReflowState.AvailableBSize() ||
        aReflowState.ComputedBSize() == NS_INTRINSICSIZE) {
      // Update desired height to include block-end border/padding
      desiredSizeInFlexWM.BSize(flexWM) = desiredBSizeWithBEndBP;
    } else {
      // We couldn't fit bottom border/padding, so we'll need a continuation.
      NS_FRAME_SET_INCOMPLETE(aStatus);
    }
  }

  // Convert flex container's final desired size to parent's WM, for outparam.
  aDesiredSize.SetSize(flexWM, desiredSizeInFlexWM);

  // Overflow area = union(my overflow area, kids' overflow areas)
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  for (nsIFrame* childFrame : mFrames) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, childFrame);
  }

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize,
                                 aReflowState, aStatus);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize)
}

void
nsFlexContainerFrame::MoveFlexItemToFinalPosition(
  const nsHTMLReflowState& aReflowState,
  const FlexItem& aItem,
  LogicalPoint& aFramePos,
  const nsSize& aContainerSize)
{
  WritingMode outerWM = aReflowState.GetWritingMode();

  // If item is relpos, look up its offsets (cached from prev reflow)
  LogicalMargin logicalOffsets(outerWM);
  if (NS_STYLE_POSITION_RELATIVE == aItem.Frame()->StyleDisplay()->mPosition) {
    FrameProperties props = aItem.Frame()->Properties();
    nsMargin* cachedOffsets =
      static_cast<nsMargin*>(props.Get(nsIFrame::ComputedOffsetProperty()));
    MOZ_ASSERT(cachedOffsets,
               "relpos previously-reflowed frame should've cached its offsets");
    logicalOffsets = LogicalMargin(outerWM, *cachedOffsets);
  }
  nsHTMLReflowState::ApplyRelativePositioning(aItem.Frame(), outerWM,
                                              logicalOffsets, &aFramePos,
                                              aContainerSize);
  aItem.Frame()->SetPosition(outerWM, aFramePos, aContainerSize);
  PositionFrameView(aItem.Frame());
  PositionChildViews(aItem.Frame());
}

void
nsFlexContainerFrame::ReflowFlexItem(nsPresContext* aPresContext,
                                     const FlexboxAxisTracker& aAxisTracker,
                                     const nsHTMLReflowState& aReflowState,
                                     const FlexItem& aItem,
                                     LogicalPoint& aFramePos,
                                     const nsSize& aContainerSize)
{
  WritingMode outerWM = aReflowState.GetWritingMode();
  WritingMode wm = aItem.Frame()->GetWritingMode();
  LogicalSize availSize = aReflowState.ComputedSize(wm);
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
  nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                     aItem.Frame(), availSize);

  // Keep track of whether we've overriden the child's computed height
  // and/or width, so we can set its resize flags accordingly.
  bool didOverrideComputedWidth = false;
  bool didOverrideComputedHeight = false;

  // Override computed main-size
  if (aAxisTracker.IsMainAxisHorizontal()) {
    childReflowState.SetComputedWidth(aItem.GetMainSize());
    didOverrideComputedWidth = true;
  } else {
    childReflowState.SetComputedHeight(aItem.GetMainSize());
    didOverrideComputedHeight = true;
  }

  // Override reflow state's computed cross-size if either:
  // - the item was stretched (in which case we're imposing a cross size)
  // ...or...
  // - the item it has an aspect ratio (in which case the cross-size that's
  // currently in the reflow state is based on arithmetic involving a stale
  // main-size value that we just stomped on above). (Note that we could handle
  // this case using an AutoFlexItemMainSizeOverride, as we do elsewhere; but
  // given that we *already know* the correct cross size to use here, it's
  // cheaper to just directly set it instead of setting a frame property.)
  if (aItem.IsStretched() ||
      aItem.HasIntrinsicRatio()) {
    if (aAxisTracker.IsCrossAxisHorizontal()) {
      childReflowState.SetComputedWidth(aItem.GetCrossSize());
      didOverrideComputedWidth = true;
    } else {
      childReflowState.SetComputedHeight(aItem.GetCrossSize());
      didOverrideComputedHeight = true;
    }
  }
  if (aItem.IsStretched() && !aAxisTracker.IsCrossAxisHorizontal()) {
    // If this item's height is stretched, it's a relative height.
    aItem.Frame()->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
  }

  // XXXdholbert Might need to actually set the correct margins in the
  // reflow state at some point, so that they can be saved on the frame for
  // UsedMarginProperty().  Maybe doesn't matter though...?

  // If we're overriding the computed width or height, *and* we had an
  // earlier "measuring" reflow, then this upcoming reflow needs to be
  // treated as a resize.
  if (aItem.HadMeasuringReflow()) {
    if (didOverrideComputedWidth) {
      // (This is somewhat redundant, since the reflow state already
      // sets mHResize whenever our computed width has changed since the
      // previous reflow. Still, it's nice for symmetry, and it may become
      // necessary once we support orthogonal flows.)
      childReflowState.SetHResize(true);
    }
    if (didOverrideComputedHeight) {
      childReflowState.SetVResize(true);
    }
  }
  // NOTE: Be very careful about doing anything else with childReflowState
  // after this point, because some of its methods (e.g. SetComputedWidth)
  // internally call InitResizeFlags and stomp on mVResize & mHResize.

  nsHTMLReflowMetrics childDesiredSize(childReflowState);
  nsReflowStatus childReflowStatus;
  ReflowChild(aItem.Frame(), aPresContext,
              childDesiredSize, childReflowState,
              outerWM, aFramePos, aContainerSize,
              0, childReflowStatus);

  // XXXdholbert Once we do pagination / splitting, we'll need to actually
  // handle incomplete childReflowStatuses. But for now, we give our kids
  // unconstrained available height, which means they should always
  // complete.
  MOZ_ASSERT(NS_FRAME_IS_COMPLETE(childReflowStatus),
             "We gave flex item unconstrained available height, so it "
             "should be complete");

  LogicalMargin offsets =
    childReflowState.ComputedLogicalOffsets().ConvertTo(outerWM, wm);
  nsHTMLReflowState::ApplyRelativePositioning(aItem.Frame(), outerWM,
                                              offsets, &aFramePos,
                                              aContainerSize);

  FinishReflowChild(aItem.Frame(), aPresContext,
                    childDesiredSize, &childReflowState,
                    outerWM, aFramePos, aContainerSize, 0);

  // Save the first child's ascent; it may establish container's baseline.
  if (aItem.Frame() == mFrames.FirstChild()) {
    aItem.SetAscent(childDesiredSize.BlockStartAscent());
  }
}

/* virtual */ nscoord
nsFlexContainerFrame::GetMinISize(nsRenderingContext* aRenderingContext)
{
  nscoord minWidth = 0;
  DISPLAY_MIN_WIDTH(this, minWidth);

  const nsStylePosition* stylePos = StylePosition();
  const FlexboxAxisTracker axisTracker(this, GetWritingMode());

  for (nsIFrame* childFrame : mFrames) {
    nscoord childMinWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, childFrame,
                                           nsLayoutUtils::MIN_ISIZE);
    // For a horizontal single-line flex container, the intrinsic min width is
    // the sum of its items' min widths.
    // For a vertical flex container, or for a multi-line horizontal flex
    // container, the intrinsic min width is the max of its items' min widths.
    if (axisTracker.IsMainAxisHorizontal() &&
        NS_STYLE_FLEX_WRAP_NOWRAP == stylePos->mFlexWrap) {
      minWidth += childMinWidth;
    } else {
      minWidth = std::max(minWidth, childMinWidth);
    }
  }
  return minWidth;
}

/* virtual */ nscoord
nsFlexContainerFrame::GetPrefISize(nsRenderingContext* aRenderingContext)
{
  nscoord prefWidth = 0;
  DISPLAY_PREF_WIDTH(this, prefWidth);

  // XXXdholbert Optimization: We could cache our intrinsic widths like
  // nsBlockFrame does (and return it early from this function if it's set).
  // Whenever anything happens that might change it, set it to
  // NS_INTRINSIC_WIDTH_UNKNOWN (like nsBlockFrame::MarkIntrinsicISizesDirty
  // does)
  const FlexboxAxisTracker axisTracker(this, GetWritingMode());

  for (nsIFrame* childFrame : mFrames) {
    nscoord childPrefWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, childFrame,
                                           nsLayoutUtils::PREF_ISIZE);
    if (axisTracker.IsMainAxisHorizontal()) {
      prefWidth += childPrefWidth;
    } else {
      prefWidth = std::max(prefWidth, childPrefWidth);
    }
  }
  return prefWidth;
}
