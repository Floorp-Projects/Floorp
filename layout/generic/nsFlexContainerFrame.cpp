/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: flex" */

#include "nsFlexContainerFrame.h"

#include <algorithm>

#include "gfxContext.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/CSSOrderAwareFrameIterator.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Logging.h"
#include "mozilla/PresShell.h"
#include "mozilla/WritingModes.h"
#include "nsBlockFrame.h"
#include "nsContentUtils.h"
#include "nsCSSAnonBoxes.h"
#include "nsDisplayList.h"
#include "nsFieldSetFrame.h"
#include "nsIFrameInlines.h"
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsPresContext.h"

using namespace mozilla;
using namespace mozilla::layout;

// Convenience typedefs for helper classes that we forward-declare in .h file
// (so that nsFlexContainerFrame methods can use them as parameters):
using FlexItem = nsFlexContainerFrame::FlexItem;
using FlexLine = nsFlexContainerFrame::FlexLine;
using FlexboxAxisTracker = nsFlexContainerFrame::FlexboxAxisTracker;
using StrutInfo = nsFlexContainerFrame::StrutInfo;
using CachedBAxisMeasurement = nsFlexContainerFrame::CachedBAxisMeasurement;

static mozilla::LazyLogModule gFlexContainerLog("FlexContainer");
#define FLEX_LOG(...) \
  MOZ_LOG(gFlexContainerLog, LogLevel::Debug, (__VA_ARGS__));
#define FLEX_LOGV(...) \
  MOZ_LOG(gFlexContainerLog, LogLevel::Verbose, (__VA_ARGS__));

// Returns true iff the given nsStyleDisplay has display:-webkit-{inline-}box
// or display:-moz-{inline-}box.
static inline bool IsDisplayValueLegacyBox(const nsStyleDisplay* aStyleDisp) {
  return aStyleDisp->mDisplay == mozilla::StyleDisplay::WebkitBox ||
         aStyleDisp->mDisplay == mozilla::StyleDisplay::WebkitInlineBox ||
         aStyleDisp->mDisplay == mozilla::StyleDisplay::MozBox ||
         aStyleDisp->mDisplay == mozilla::StyleDisplay::MozInlineBox;
}

// Returns true if aFlexContainer is a frame for some element that has
// display:-webkit-{inline-}box (or -moz-{inline-}box). aFlexContainer is
// expected to be an instance of nsFlexContainerFrame (enforced with an assert);
// otherwise, this function's state-bit-check here is bogus.
static bool IsLegacyBox(const nsIFrame* aFlexContainer) {
  MOZ_ASSERT(aFlexContainer->IsFlexContainerFrame(),
             "only flex containers may be passed to this function");
  return aFlexContainer->HasAnyStateBits(NS_STATE_FLEX_IS_EMULATING_LEGACY_BOX);
}

// Returns the OrderState enum we should pass to CSSOrderAwareFrameIterator
// (depending on whether aFlexContainer has
// NS_STATE_FLEX_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER state bit).
static CSSOrderAwareFrameIterator::OrderState OrderStateForIter(
    const nsFlexContainerFrame* aFlexContainer) {
  return aFlexContainer->HasAnyStateBits(
             NS_STATE_FLEX_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER)
             ? CSSOrderAwareFrameIterator::OrderState::Ordered
             : CSSOrderAwareFrameIterator::OrderState::Unordered;
}

// Returns the OrderingProperty enum that we should pass to
// CSSOrderAwareFrameIterator (depending on whether it's a legacy box).
static CSSOrderAwareFrameIterator::OrderingProperty OrderingPropertyForIter(
    const nsFlexContainerFrame* aFlexContainer) {
  return IsLegacyBox(aFlexContainer)
             ? CSSOrderAwareFrameIterator::OrderingProperty::BoxOrdinalGroup
             : CSSOrderAwareFrameIterator::OrderingProperty::Order;
}

// Returns the "align-items" value that's equivalent to the legacy "box-align"
// value in the given style struct.
static StyleAlignFlags ConvertLegacyStyleToAlignItems(
    const nsStyleXUL* aStyleXUL) {
  // -[moz|webkit]-box-align corresponds to modern "align-items"
  switch (aStyleXUL->mBoxAlign) {
    case StyleBoxAlign::Stretch:
      return StyleAlignFlags::STRETCH;
    case StyleBoxAlign::Start:
      return StyleAlignFlags::FLEX_START;
    case StyleBoxAlign::Center:
      return StyleAlignFlags::CENTER;
    case StyleBoxAlign::Baseline:
      return StyleAlignFlags::BASELINE;
    case StyleBoxAlign::End:
      return StyleAlignFlags::FLEX_END;
  }

  MOZ_ASSERT_UNREACHABLE("Unrecognized mBoxAlign enum value");
  // Fall back to default value of "align-items" property:
  return StyleAlignFlags::STRETCH;
}

// Returns the "justify-content" value that's equivalent to the legacy
// "box-pack" value in the given style struct.
static StyleContentDistribution ConvertLegacyStyleToJustifyContent(
    const nsStyleXUL* aStyleXUL) {
  // -[moz|webkit]-box-pack corresponds to modern "justify-content"
  switch (aStyleXUL->mBoxPack) {
    case StyleBoxPack::Start:
      return {StyleAlignFlags::FLEX_START};
    case StyleBoxPack::Center:
      return {StyleAlignFlags::CENTER};
    case StyleBoxPack::End:
      return {StyleAlignFlags::FLEX_END};
    case StyleBoxPack::Justify:
      return {StyleAlignFlags::SPACE_BETWEEN};
  }

  MOZ_ASSERT_UNREACHABLE("Unrecognized mBoxPack enum value");
  // Fall back to default value of "justify-content" property:
  return {StyleAlignFlags::FLEX_START};
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
static nscoord PhysicalCoordFromFlexRelativeCoord(nscoord aFlexRelativeCoord,
                                                  nscoord aContainerSize,
                                                  mozilla::Side aStartSide) {
  if (aStartSide == eSideLeft || aStartSide == eSideTop) {
    return aFlexRelativeCoord;
  }
  return aContainerSize - aFlexRelativeCoord;
}

// Check if the size is auto or it is a keyword in the block axis.
// |aIsInline| should represent whether aSize is in the inline axis, from the
// perspective of the writing mode of the flex item that the size comes from.
//
// max-content and min-content should behave as property's initial value.
// Bug 567039: We treat -moz-fit-content and -moz-available as property's
// initial value for now.
static inline bool IsAutoOrEnumOnBSize(const StyleSize& aSize, bool aIsInline) {
  return aSize.IsAuto() || (!aIsInline && !aSize.IsLengthPercentage());
}

// Helper-macros to let us pick one of two expressions to evaluate
// (an inline-axis expression vs. a block-axis expression), to get a
// main-axis or cross-axis component.
// For code that has e.g. a LogicalSize object, the methods
// FlexboxAxisTracker::MainComponent and CrossComponent are cleaner
// than these macros. But in cases where we simply have two separate
// expressions for ISize and BSize (which may be expensive to evaluate),
// these macros can be used to ensure that only the needed expression is
// evaluated.
#define GET_MAIN_COMPONENT_LOGICAL(axisTracker_, wm_, isize_, bsize_) \
  (axisTracker_).IsInlineAxisMainAxis((wm_)) ? (isize_) : (bsize_)

#define GET_CROSS_COMPONENT_LOGICAL(axisTracker_, wm_, isize_, bsize_) \
  (axisTracker_).IsInlineAxisMainAxis((wm_)) ? (bsize_) : (isize_)

// Encapsulates our flex container's main & cross axes. This class is backed by
// a FlexboxAxisInfo helper member variable, and it adds some convenience APIs
// on top of what that struct offers.
class MOZ_STACK_CLASS nsFlexContainerFrame::FlexboxAxisTracker {
 public:
  explicit FlexboxAxisTracker(const nsFlexContainerFrame* aFlexContainer);

  // Accessors:
  LogicalAxis MainAxis() const {
    return IsRowOriented() ? eLogicalAxisInline : eLogicalAxisBlock;
  }
  LogicalAxis CrossAxis() const {
    return IsRowOriented() ? eLogicalAxisBlock : eLogicalAxisInline;
  }

  LogicalSide MainAxisStartSide() const;
  LogicalSide MainAxisEndSide() const {
    return GetOppositeSide(MainAxisStartSide());
  }

  LogicalSide CrossAxisStartSide() const;
  LogicalSide CrossAxisEndSide() const {
    return GetOppositeSide(CrossAxisStartSide());
  }

  mozilla::Side MainAxisPhysicalStartSide() const {
    return mWM.PhysicalSide(MainAxisStartSide());
  }
  mozilla::Side MainAxisPhysicalEndSide() const {
    return mWM.PhysicalSide(MainAxisEndSide());
  }

  mozilla::Side CrossAxisPhysicalStartSide() const {
    return mWM.PhysicalSide(CrossAxisStartSide());
  }
  mozilla::Side CrossAxisPhysicalEndSide() const {
    return mWM.PhysicalSide(CrossAxisEndSide());
  }

  // Returns the flex container's writing mode.
  WritingMode GetWritingMode() const { return mWM; }

  // Returns true if our main axis is in the reverse direction of our
  // writing mode's corresponding axis. (From 'flex-direction: *-reverse')
  bool IsMainAxisReversed() const { return mAxisInfo.mIsMainAxisReversed; }
  // Returns true if our cross axis is in the reverse direction of our
  // writing mode's corresponding axis. (From 'flex-wrap: *-reverse')
  bool IsCrossAxisReversed() const { return mAxisInfo.mIsCrossAxisReversed; }

  bool IsRowOriented() const { return mAxisInfo.mIsRowOriented; }
  bool IsColumnOriented() const { return !IsRowOriented(); }

  // aSize is expected to match the flex container's WritingMode.
  nscoord MainComponent(const LogicalSize& aSize) const {
    return IsRowOriented() ? aSize.ISize(mWM) : aSize.BSize(mWM);
  }
  int32_t MainComponent(const LayoutDeviceIntSize& aIntSize) const {
    return IsMainAxisHorizontal() ? aIntSize.width : aIntSize.height;
  }

  // aSize is expected to match the flex container's WritingMode.
  nscoord CrossComponent(const LogicalSize& aSize) const {
    return IsRowOriented() ? aSize.BSize(mWM) : aSize.ISize(mWM);
  }
  int32_t CrossComponent(const LayoutDeviceIntSize& aIntSize) const {
    return IsMainAxisHorizontal() ? aIntSize.height : aIntSize.width;
  }

  // NOTE: aMargin is expected to use the flex container's WritingMode.
  nscoord MarginSizeInMainAxis(const LogicalMargin& aMargin) const {
    // If we're row-oriented, our main axis is the inline axis.
    return IsRowOriented() ? aMargin.IStartEnd(mWM) : aMargin.BStartEnd(mWM);
  }
  nscoord MarginSizeInCrossAxis(const LogicalMargin& aMargin) const {
    // If we're row-oriented, our cross axis is the block axis.
    return IsRowOriented() ? aMargin.BStartEnd(mWM) : aMargin.IStartEnd(mWM);
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
  LogicalPoint LogicalPointFromFlexRelativePoint(
      nscoord aMainCoord, nscoord aCrossCoord, nscoord aContainerMainSize,
      nscoord aContainerCrossSize) const {
    nscoord logicalCoordInMainAxis =
        IsMainAxisReversed() ? aContainerMainSize - aMainCoord : aMainCoord;
    nscoord logicalCoordInCrossAxis =
        IsCrossAxisReversed() ? aContainerCrossSize - aCrossCoord : aCrossCoord;

    return IsRowOriented() ? LogicalPoint(mWM, logicalCoordInMainAxis,
                                          logicalCoordInCrossAxis)
                           : LogicalPoint(mWM, logicalCoordInCrossAxis,
                                          logicalCoordInMainAxis);
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
    return IsRowOriented() ? LogicalSize(mWM, aMainSize, aCrossSize)
                           : LogicalSize(mWM, aCrossSize, aMainSize);
  }

  bool IsMainAxisHorizontal() const {
    // If we're row-oriented, and our writing mode is NOT vertical,
    // or we're column-oriented and our writing mode IS vertical,
    // then our main axis is horizontal. This handles all cases:
    return IsRowOriented() != mWM.IsVertical();
  }

  // Returns true if this flex item's inline axis in aItemWM is parallel (or
  // antiparallel) to the container's main axis. Returns false, otherwise.
  //
  // Note: this is a helper for implementing macros and can also be used before
  // constructing FlexItem. Inside of flex reflow code,
  // FlexItem::IsInlineAxisMainAxis() is equivalent & more optimal.
  bool IsInlineAxisMainAxis(WritingMode aItemWM) const {
    return IsRowOriented() != GetWritingMode().IsOrthogonalTo(aItemWM);
  }

  // Maps justify-*: 'left' or 'right' to 'start' or 'end'.
  StyleAlignFlags ResolveJustifyLeftRight(const StyleAlignFlags& aFlags) const {
    MOZ_ASSERT(
        aFlags == StyleAlignFlags::LEFT || aFlags == StyleAlignFlags::RIGHT,
        "This helper accepts only 'LEFT' or 'RIGHT' flags!");

    const auto wm = GetWritingMode();
    const bool isJustifyLeft = aFlags == StyleAlignFlags::LEFT;
    if (IsColumnOriented()) {
      if (!wm.IsVertical()) {
        // Container's alignment axis (main axis) is *not* parallel to the
        // line-left <-> line-right axis or the physical left <-> physical right
        // axis, so we map both 'left' and 'right' to 'start'.
        return StyleAlignFlags::START;
      }

      MOZ_ASSERT(wm.PhysicalAxis(MainAxis()) == eAxisHorizontal,
                 "Vertical column-oriented flex container's main axis should "
                 "be parallel to physical left <-> right axis!");
      // Map 'left' or 'right' to 'start' or 'end', depending on its block flow
      // direction.
      return isJustifyLeft == wm.IsVerticalLR() ? StyleAlignFlags::START
                                                : StyleAlignFlags::END;
    }

    MOZ_ASSERT(MainAxis() == eLogicalAxisInline,
               "Row-oriented flex container's main axis should be parallel to "
               "line-left <-> line-right axis!");

    // If we get here, we're operating on the flex container's inline axis,
    // so we map 'left' to whichever of 'start' or 'end' corresponds to the
    // *line-relative* left side; and similar for 'right'.
    return isJustifyLeft == wm.IsBidiLTR() ? StyleAlignFlags::START
                                           : StyleAlignFlags::END;
  }

  // Delete copy-constructor & reassignment operator, to prevent accidental
  // (unnecessary) copying.
  FlexboxAxisTracker(const FlexboxAxisTracker&) = delete;
  FlexboxAxisTracker& operator=(const FlexboxAxisTracker&) = delete;

 private:
  const WritingMode mWM;  // The flex container's writing mode.
  const FlexboxAxisInfo mAxisInfo;
};

/**
 * Represents a flex item.
 * Includes the various pieces of input that the Flexbox Layout Algorithm uses
 * to resolve a flexible width.
 */
class nsFlexContainerFrame::FlexItem final {
 public:
  // Normal constructor:
  FlexItem(ReflowInput& aFlexItemReflowInput, float aFlexGrow,
           float aFlexShrink, nscoord aFlexBaseSize, nscoord aMainMinSize,
           nscoord aMainMaxSize, nscoord aTentativeCrossSize,
           nscoord aCrossMinSize, nscoord aCrossMaxSize,
           const FlexboxAxisTracker& aAxisTracker);

  // Simplified constructor, to be used only for generating "struts":
  // (NOTE: This "strut" constructor uses the *container's* writing mode, which
  // we'll use on this FlexItem instead of the child frame's real writing mode.
  // This is fine - it doesn't matter what writing mode we use for a
  // strut, since it won't render any content and we already know its size.)
  FlexItem(nsIFrame* aChildFrame, nscoord aCrossSize, WritingMode aContainerWM,
           const FlexboxAxisTracker& aAxisTracker);

  // Clone existing FlexItem for its underlying frame's continuation.
  // @param aContinuation a continuation in our next-in-flow chain.
  FlexItem CloneFor(nsIFrame* const aContinuation) const {
    MOZ_ASSERT(Frame() == aContinuation->FirstInFlow(),
               "aContinuation should be in aItem's continuation chain!");
    FlexItem item(*this);
    item.mFrame = aContinuation;
    item.mHadMeasuringReflow = false;
    return item;
  }

  // Accessors
  nsIFrame* Frame() const { return mFrame; }
  nscoord FlexBaseSize() const { return mFlexBaseSize; }

  nscoord MainMinSize() const {
    MOZ_ASSERT(!mNeedsMinSizeAutoResolution,
               "Someone's using an unresolved 'auto' main min-size");
    return mMainMinSize;
  }
  nscoord MainMaxSize() const { return mMainMaxSize; }

  // Note: These return the main-axis position and size of our *content box*.
  nscoord MainSize() const { return mMainSize; }
  nscoord MainPosition() const { return mMainPosn; }

  nscoord CrossMinSize() const { return mCrossMinSize; }
  nscoord CrossMaxSize() const { return mCrossMaxSize; }

  // Note: These return the cross-axis position and size of our *content box*.
  nscoord CrossSize() const { return mCrossSize; }
  nscoord CrossPosition() const { return mCrossPosn; }

  // Lazy getter for mAscent.
  nscoord ResolvedAscent(bool aUseFirstBaseline) const {
    // XXXdholbert Two concerns to follow up on here:
    // (1) We probably should be checking and reacting to aUseFirstBaseline
    // for all of the cases here (e.g. this first one). Maybe we need to store
    // two versions of mAscent and choose the appropriate one based on
    // aUseFirstBaseline? This is roughly bug 1480850, I think.
    // (2) We should be using the *container's* writing-mode (mCBWM) here,
    // instead of the item's (mWM). This is essentially bug 1155322.
    if (mAscent != ReflowOutput::ASK_FOR_BASELINE) {
      return mAscent;
    }

    // Use GetFirstLineBaseline() or GetLastLineBaseline() as appropriate:
    bool found =
        aUseFirstBaseline
            ? nsLayoutUtils::GetFirstLineBaseline(mWM, mFrame, &mAscent)
            : nsLayoutUtils::GetLastLineBaseline(mWM, mFrame, &mAscent);
    if (found) {
      return mAscent;
    }

    // If the nsLayoutUtils getter fails, then ask the frame directly:
    auto baselineGroup = aUseFirstBaseline ? BaselineSharingGroup::First
                                           : BaselineSharingGroup::Last;
    if (mFrame->GetNaturalBaselineBOffset(mWM, baselineGroup, &mAscent)) {
      return mAscent;
    }

    // We couldn't determine a baseline, so we synthesize one from border box:
    mAscent = mFrame->SynthesizeBaselineBOffsetFromBorderBox(
        mWM, BaselineSharingGroup::First);
    return mAscent;
  }

  // Convenience methods to compute the main & cross size of our *margin-box*.
  nscoord OuterMainSize() const {
    return mMainSize + MarginBorderPaddingSizeInMainAxis();
  }

  nscoord OuterCrossSize() const {
    return mCrossSize + MarginBorderPaddingSizeInCrossAxis();
  }

  // Convenience methods to synthesize a style main size or a style cross size
  // with box-size considered, to provide the size overrides when constructing
  // ReflowInput for flex items.
  StyleSize StyleMainSize() const {
    nscoord mainSize = MainSize();
    if (Frame()->StylePosition()->mBoxSizing == StyleBoxSizing::Border) {
      mainSize += BorderPaddingSizeInMainAxis();
    }
    return StyleSize::LengthPercentage(
        LengthPercentage::FromAppUnits(mainSize));
  }

  StyleSize StyleCrossSize() const {
    nscoord crossSize = CrossSize();
    if (Frame()->StylePosition()->mBoxSizing == StyleBoxSizing::Border) {
      crossSize += BorderPaddingSizeInCrossAxis();
    }
    return StyleSize::LengthPercentage(
        LengthPercentage::FromAppUnits(crossSize));
  }

  // Returns the distance between this FlexItem's baseline and the cross-start
  // edge of its margin-box. Used in baseline alignment.
  //
  // (This function needs to be told which physical start side we're measuring
  // the baseline from, so that it can look up the appropriate components from
  // margin.)
  nscoord BaselineOffsetFromOuterCrossEdge(mozilla::Side aStartSide,
                                           bool aUseFirstLineBaseline) const;

  double ShareOfWeightSoFar() const { return mShareOfWeightSoFar; }

  bool IsFrozen() const { return mIsFrozen; }

  bool HadMinViolation() const {
    MOZ_ASSERT(!mIsFrozen, "min violation has no meaning for frozen items.");
    return mHadMinViolation;
  }

  bool HadMaxViolation() const {
    MOZ_ASSERT(!mIsFrozen, "max violation has no meaning for frozen items.");
    return mHadMaxViolation;
  }

  bool WasMinClamped() const {
    MOZ_ASSERT(mIsFrozen, "min clamping has no meaning for unfrozen items.");
    return mHadMinViolation;
  }

  bool WasMaxClamped() const {
    MOZ_ASSERT(mIsFrozen, "max clamping has no meaning for unfrozen items.");
    return mHadMaxViolation;
  }

  // Indicates whether this item received a preliminary "measuring" reflow
  // before its actual reflow.
  bool HadMeasuringReflow() const { return mHadMeasuringReflow; }

  // Indicates whether this item's computed cross-size property is 'auto'.
  bool IsCrossSizeAuto() const;

  // Indicates whether the cross-size property is set to something definite,
  // for the purpose of preferred aspect ratio calculations.
  bool IsCrossSizeDefinite(const ReflowInput& aItemReflowInput) const;

  // Indicates whether this item's cross-size has been stretched (from having
  // "align-self: stretch" with an auto cross-size and no auto margins in the
  // cross axis).
  bool IsStretched() const { return mIsStretched; }

  // Indicates whether we need to resolve an 'auto' value for the main-axis
  // min-[width|height] property.
  bool NeedsMinSizeAutoResolution() const {
    return mNeedsMinSizeAutoResolution;
  }

  bool HasAnyAutoMargin() const { return mHasAnyAutoMargin; }

  // Indicates whether this item is a "strut" left behind by an element with
  // visibility:collapse.
  bool IsStrut() const { return mIsStrut; }

  // The main axis and cross axis are relative to mCBWM.
  LogicalAxis MainAxis() const { return mMainAxis; }
  LogicalAxis CrossAxis() const { return GetOrthogonalAxis(mMainAxis); }

  // IsInlineAxisMainAxis() returns true if this item's inline axis is parallel
  // (or antiparallel) to the container's main axis. Otherwise (i.e. if this
  // item's inline axis is orthogonal to the container's main axis), this
  // function returns false. The next 3 methods are all other ways of asking
  // the same question, and only exist for readability at callsites (depending
  // on which axes those callsites are reasoning about).
  bool IsInlineAxisMainAxis() const { return mIsInlineAxisMainAxis; }
  bool IsInlineAxisCrossAxis() const { return !mIsInlineAxisMainAxis; }
  bool IsBlockAxisMainAxis() const { return !mIsInlineAxisMainAxis; }
  bool IsBlockAxisCrossAxis() const { return mIsInlineAxisMainAxis; }

  WritingMode GetWritingMode() const { return mWM; }
  WritingMode ContainingBlockWM() const { return mCBWM; }
  StyleAlignSelf AlignSelf() const { return mAlignSelf; }
  StyleAlignFlags AlignSelfFlags() const { return mAlignSelfFlags; }

  // Returns the flex factor (flex-grow or flex-shrink), depending on
  // 'aIsUsingFlexGrow'.
  //
  // Asserts fatally if called on a frozen item (since frozen items are not
  // flexible).
  float GetFlexFactor(bool aIsUsingFlexGrow) {
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
  float GetWeight(bool aIsUsingFlexGrow) {
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

  bool TreatBSizeAsIndefinite() const { return mTreatBSizeAsIndefinite; }

  const AspectRatio& GetAspectRatio() const { return mAspectRatio; }
  bool HasAspectRatio() const { return !!mAspectRatio; }

  // Getters for margin:
  // ===================
  LogicalMargin Margin() const { return mMargin; }
  nsMargin PhysicalMargin() const { return mMargin.GetPhysicalMargin(mCBWM); }

  // Returns the margin component for a given LogicalSide in flex container's
  // writing-mode.
  nscoord GetMarginComponentForSide(LogicalSide aSide) const {
    return mMargin.Side(aSide, mCBWM);
  }

  // Returns the total space occupied by this item's margins in the given axis
  nscoord MarginSizeInMainAxis() const {
    return mMargin.StartEnd(MainAxis(), mCBWM);
  }
  nscoord MarginSizeInCrossAxis() const {
    return mMargin.StartEnd(CrossAxis(), mCBWM);
  }

  // Getters for border/padding
  // ==========================
  // Returns the total space occupied by this item's borders and padding in
  // the given axis
  LogicalMargin BorderPadding() const { return mBorderPadding; }
  nscoord BorderPaddingSizeInMainAxis() const {
    return mBorderPadding.StartEnd(MainAxis(), mCBWM);
  }
  nscoord BorderPaddingSizeInCrossAxis() const {
    return mBorderPadding.StartEnd(CrossAxis(), mCBWM);
  }

  // Getter for combined margin/border/padding
  // =========================================
  // Returns the total space occupied by this item's margins, borders and
  // padding in the given axis
  nscoord MarginBorderPaddingSizeInMainAxis() const {
    return MarginSizeInMainAxis() + BorderPaddingSizeInMainAxis();
  }
  nscoord MarginBorderPaddingSizeInCrossAxis() const {
    return MarginSizeInCrossAxis() + BorderPaddingSizeInCrossAxis();
  }

  // Setters
  // =======
  // Helper to set the resolved value of min-[width|height]:auto for the main
  // axis. (Should only be used if NeedsMinSizeAutoResolution() returns true.)
  void UpdateMainMinSize(nscoord aNewMinSize) {
    NS_ASSERTION(aNewMinSize >= 0,
                 "How did we end up with a negative min-size?");
    MOZ_ASSERT(
        mMainMaxSize == NS_UNCONSTRAINEDSIZE || mMainMaxSize >= aNewMinSize,
        "Should only use this function for resolving min-size:auto, "
        "and main max-size should be an upper-bound for resolved val");
    MOZ_ASSERT(
        mNeedsMinSizeAutoResolution &&
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
  void SetFlexBaseSizeAndMainSize(nscoord aNewFlexBaseSize) {
    MOZ_ASSERT(!mIsFrozen || mFlexBaseSize == NS_UNCONSTRAINEDSIZE,
               "flex base size shouldn't change after we're frozen "
               "(unless we're just resolving an intrinsic size)");
    mFlexBaseSize = aNewFlexBaseSize;

    // Before we've resolved flexible lengths, we keep mMainSize set to
    // the 'hypothetical main size', which is the flex base size, clamped
    // to the [min,max] range:
    mMainSize = NS_CSS_MINMAX(mFlexBaseSize, mMainMinSize, mMainMaxSize);

    FLEX_LOGV(
        "Set flex base size: %d, hypothetical main size: %d for flex item %p",
        mFlexBaseSize, mMainSize, mFrame);
  }

  // Setters used while we're resolving flexible lengths
  // ---------------------------------------------------

  // Sets the main-size of our flex item's content-box.
  void SetMainSize(nscoord aNewMainSize) {
    MOZ_ASSERT(!mIsFrozen, "main size shouldn't change after we're frozen");
    mMainSize = aNewMainSize;
  }

  void SetShareOfWeightSoFar(double aNewShare) {
    MOZ_ASSERT(!mIsFrozen || aNewShare == 0.0,
               "shouldn't be giving this item any share of the weight "
               "after it's frozen");
    mShareOfWeightSoFar = aNewShare;
  }

  void Freeze() {
    mIsFrozen = true;
    // Now that we are frozen, the meaning of mHadMinViolation and
    // mHadMaxViolation changes to indicate min and max clamping. Clear
    // both of the member variables so that they are ready to be set
    // as clamping state later, if necessary.
    mHadMinViolation = false;
    mHadMaxViolation = false;
  }

  void SetHadMinViolation() {
    MOZ_ASSERT(!mIsFrozen,
               "shouldn't be changing main size & having violations "
               "after we're frozen");
    mHadMinViolation = true;
  }
  void SetHadMaxViolation() {
    MOZ_ASSERT(!mIsFrozen,
               "shouldn't be changing main size & having violations "
               "after we're frozen");
    mHadMaxViolation = true;
  }
  void ClearViolationFlags() {
    MOZ_ASSERT(!mIsFrozen,
               "shouldn't be altering violation flags after we're "
               "frozen");
    mHadMinViolation = mHadMaxViolation = false;
  }

  void SetWasMinClamped() {
    MOZ_ASSERT(!mHadMinViolation && !mHadMaxViolation, "only clamp once");
    // This reuses the mHadMinViolation member variable to track clamping
    // events. This is allowable because mHadMinViolation only reflects
    // a violation up until the item is frozen.
    MOZ_ASSERT(mIsFrozen, "shouldn't set clamping state when we are unfrozen");
    mHadMinViolation = true;
  }
  void SetWasMaxClamped() {
    MOZ_ASSERT(!mHadMinViolation && !mHadMaxViolation, "only clamp once");
    // This reuses the mHadMaxViolation member variable to track clamping
    // events. This is allowable because mHadMaxViolation only reflects
    // a violation up until the item is frozen.
    MOZ_ASSERT(mIsFrozen, "shouldn't set clamping state when we are unfrozen");
    mHadMaxViolation = true;
  }

  // Setters for values that are determined after we've resolved our main size
  // -------------------------------------------------------------------------

  // Sets the main-axis position of our flex item's content-box.
  // (This is the distance between the main-start edge of the flex container
  // and the main-start edge of the flex item's content-box.)
  void SetMainPosition(nscoord aPosn) {
    MOZ_ASSERT(mIsFrozen, "main size should be resolved before this");
    mMainPosn = aPosn;
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
    mAscent = aAscent;  // NOTE: this may be ASK_FOR_BASELINE
  }

  void SetHadMeasuringReflow() { mHadMeasuringReflow = true; }

  void SetIsStretched() {
    MOZ_ASSERT(mIsFrozen, "main size should be resolved before this");
    mIsStretched = true;
  }

  // Setter for margin components (for resolving "auto" margins)
  void SetMarginComponentForSide(LogicalSide aSide, nscoord aLength) {
    MOZ_ASSERT(mIsFrozen, "main size should be resolved before this");
    mMargin.Side(aSide, mCBWM) = aLength;
  }

  void ResolveStretchedCrossSize(nscoord aLineCrossSize);

  // Resolves flex base size if flex-basis' used value is 'content', using this
  // item's preferred aspect ratio and cross size.
  void ResolveFlexBaseSizeFromAspectRatio(const ReflowInput& aItemReflowInput);

  uint32_t NumAutoMarginsInMainAxis() const {
    return NumAutoMarginsInAxis(MainAxis());
  };

  uint32_t NumAutoMarginsInCrossAxis() const {
    return NumAutoMarginsInAxis(CrossAxis());
  };

  // Once the main size has been resolved, should we bother doing layout to
  // establish the cross size?
  bool CanMainSizeInfluenceCrossSize() const;

  // Returns a main size, clamped by any definite min and max cross size
  // converted through the preferred aspect ratio. The caller is responsible for
  // ensuring that the flex item's preferred aspect ratio is not zero.
  nscoord ClampMainSizeViaCrossAxisConstraints(
      nscoord aMainSize, const ReflowInput& aItemReflowInput) const;

  // Indicates whether we think this flex item needs a "final" reflow
  // (after its final flexed size & final position have been determined).
  //
  // @param aAvailableBSizeForItem the available block-size for this item (in
  //                               flex container's writing-mode)
  // @return true if such a reflow is needed, or false if we believe it can
  // simply be moved to its final position and skip the reflow.
  bool NeedsFinalReflow(const nscoord aAvailableBSizeForItem) const;

  // Gets the block frame that contains the flex item's content.  This is
  // Frame() itself or one of its descendants.
  nsBlockFrame* BlockFrame() const;

 protected:
  // Helper called by the constructor, to set mNeedsMinSizeAutoResolution:
  void CheckForMinSizeAuto(const ReflowInput& aFlexItemReflowInput,
                           const FlexboxAxisTracker& aAxisTracker);

  uint32_t NumAutoMarginsInAxis(LogicalAxis aAxis) const;

  // Values that we already know in constructor, and remain unchanged:
  // The flex item's frame.
  nsIFrame* mFrame = nullptr;
  float mFlexGrow = 0.0f;
  float mFlexShrink = 0.0f;
  AspectRatio mAspectRatio;

  // The flex item's writing mode.
  WritingMode mWM;

  // The flex container's writing mode.
  WritingMode mCBWM;

  // The flex container's main axis in flex container's writing mode.
  LogicalAxis mMainAxis;

  // Stored in flex container's writing mode.
  LogicalMargin mBorderPadding;

  // Stored in flex container's writing mode. Its value can change when we
  // resolve "auto" marigns.
  LogicalMargin mMargin;

  // These are non-const so that we can lazily update them with the item's
  // intrinsic size (obtained via a "measuring" reflow), when necessary.
  // (e.g. for "flex-basis:auto;height:auto" & "min-height:auto")
  nscoord mFlexBaseSize = 0;
  nscoord mMainMinSize = 0;
  nscoord mMainMaxSize = 0;

  // mCrossMinSize and mCrossMaxSize are not changed after constructor.
  nscoord mCrossMinSize = 0;
  nscoord mCrossMaxSize = 0;

  // Values that we compute after constructor:
  nscoord mMainSize = 0;
  nscoord mMainPosn = 0;
  nscoord mCrossSize = 0;
  nscoord mCrossPosn = 0;

  // Mutable b/c it's set & resolved lazily, sometimes via const pointer. See
  // comment above SetAscent().
  // We initialize this to ASK_FOR_BASELINE, and opportunistically fill it in
  // with a real value if we end up reflowing this flex item. (But if we don't
  // reflow this flex item, then this sentinel tells us that we don't know it
  // yet & anyone who cares will need to explicitly request it.)
  mutable nscoord mAscent = ReflowOutput::ASK_FOR_BASELINE;

  // Temporary state, while we're resolving flexible widths (for our main size)
  // XXXdholbert To save space, we could use a union to make these variables
  // overlay the same memory as some other member vars that aren't touched
  // until after main-size has been resolved. In particular, these could share
  // memory with mMainPosn through mAscent, and mIsStretched.
  double mShareOfWeightSoFar = 0.0;

  bool mIsFrozen = false;
  bool mHadMinViolation = false;
  bool mHadMaxViolation = false;

  // Did this item get a preliminary reflow, to measure its desired height?
  bool mHadMeasuringReflow = false;

  // See IsStretched() documentation.
  bool mIsStretched = false;

  // Is this item a "strut" left behind by an element with visibility:collapse?
  bool mIsStrut = false;

  // See IsInlineAxisMainAxis() documentation. This is not changed after
  // constructor.
  bool mIsInlineAxisMainAxis = true;

  // Does this item need to resolve a min-[width|height]:auto (in main-axis).
  bool mNeedsMinSizeAutoResolution = false;

  // Should we take care to treat this item's resolved BSize as indefinite?
  bool mTreatBSizeAsIndefinite = false;

  // Does this item have an auto margin in either main or cross axis?
  bool mHasAnyAutoMargin = false;

  // My "align-self" computed value (with "auto" swapped out for parent"s
  // "align-items" value, in our constructor).
  StyleAlignSelf mAlignSelf{StyleAlignFlags::AUTO};

  // Flags for 'align-self' (safe/unsafe/legacy).
  StyleAlignFlags mAlignSelfFlags{0};
};

/**
 * Represents a single flex line in a flex container.
 * Manages an array of the FlexItems that are in the line.
 */
class nsFlexContainerFrame::FlexLine final {
 public:
  explicit FlexLine(nscoord aMainGapSize) : mMainGapSize(aMainGapSize) {}

  nscoord SumOfGaps() const {
    return NumItems() > 0 ? (NumItems() - 1) * mMainGapSize : 0;
  }

  // Returns the sum of our FlexItems' outer hypothetical main sizes plus the
  // sum of main axis {row,column}-gaps between items.
  // ("outer" = margin-box, and "hypothetical" = before flexing)
  AuCoord64 TotalOuterHypotheticalMainSize() const {
    return mTotalOuterHypotheticalMainSize;
  }

  // Accessors for our FlexItems & information about them:
  //
  // Note: Using IsEmpty() to ensure that the FlexLine is non-empty before
  // calling FirstItem() or LastItem().
  FlexItem& FirstItem() { return mItems[0]; }
  const FlexItem& FirstItem() const { return mItems[0]; }

  FlexItem& LastItem() { return mItems.LastElement(); }
  const FlexItem& LastItem() const { return mItems.LastElement(); }

  bool IsEmpty() const { return mItems.IsEmpty(); }

  uint32_t NumItems() const { return mItems.Length(); }

  nsTArray<FlexItem>& Items() { return mItems; }
  const nsTArray<FlexItem>& Items() const { return mItems; }

  // Adds the last flex item's hypothetical outer main-size and
  // margin/border/padding to our totals. This should be called exactly once for
  // each flex item, after we've determined that this line is the correct home
  // for that item.
  void AddLastItemToMainSizeTotals() {
    const FlexItem& lastItem = Items().LastElement();

    // Update our various bookkeeping member-vars:
    if (lastItem.IsFrozen()) {
      mNumFrozenItems++;
    }

    mTotalItemMBP += lastItem.MarginBorderPaddingSizeInMainAxis();
    mTotalOuterHypotheticalMainSize += lastItem.OuterMainSize();

    // If the item added was not the first item in the line, we add in any gap
    // space as needed.
    if (NumItems() >= 2) {
      mTotalOuterHypotheticalMainSize += mMainGapSize;
    }
  }

  // Computes the cross-size and baseline position of this FlexLine, based on
  // its FlexItems.
  void ComputeCrossSizeAndBaseline(const FlexboxAxisTracker& aAxisTracker);

  // Returns the cross-size of this line.
  nscoord LineCrossSize() const { return mLineCrossSize; }

  // Setter for line cross-size -- needed for cases where the flex container
  // imposes a cross-size on the line. (e.g. for single-line flexbox, or for
  // multi-line flexbox with 'align-content: stretch')
  void SetLineCrossSize(nscoord aLineCrossSize) {
    mLineCrossSize = aLineCrossSize;
  }

  /**
   * Returns the offset within this line where any baseline-aligned FlexItems
   * should place their baseline. The return value represents a distance from
   * the line's cross-start edge.
   *
   * If there are no baseline-aligned FlexItems, returns nscoord_MIN.
   */
  nscoord FirstBaselineOffset() const { return mFirstBaselineOffset; }

  /**
   * Returns the offset within this line where any last baseline-aligned
   * FlexItems should place their baseline. Opposite the case of the first
   * baseline offset, this represents a distance from the line's cross-end
   * edge (since last baseline-aligned items are flush to the cross-end edge).
   * If we're internally reversing the axes, this instead represents the
   * distance from the line's cross-start edge.
   *
   * If there are no last baseline-aligned FlexItems, returns nscoord_MIN.
   */
  nscoord LastBaselineOffset() const { return mLastBaselineOffset; }

  /**
   * Returns the gap size in the main axis for this line. Used for gap
   * calculations.
   */
  nscoord MainGapSize() const { return mMainGapSize; }

  // Runs the "Resolving Flexible Lengths" algorithm from section 9.7 of the
  // CSS flexbox spec to distribute aFlexContainerMainSize among our flex items.
  // https://drafts.csswg.org/css-flexbox-1/#resolve-flexible-lengths
  void ResolveFlexibleLengths(nscoord aFlexContainerMainSize,
                              ComputedFlexLineInfo* aLineInfo);

  void PositionItemsInMainAxis(const StyleContentDistribution& aJustifyContent,
                               nscoord aContentBoxMainSize,
                               const FlexboxAxisTracker& aAxisTracker);

  void PositionItemsInCrossAxis(nscoord aLineStartPosition,
                                const FlexboxAxisTracker& aAxisTracker);

 private:
  // Helpers for ResolveFlexibleLengths():
  void FreezeItemsEarly(bool aIsUsingFlexGrow, ComputedFlexLineInfo* aLineInfo);

  void FreezeOrRestoreEachFlexibleSize(const nscoord aTotalViolation,
                                       bool aIsFinalIteration);

  // Stores this line's flex items.
  nsTArray<FlexItem> mItems;

  // Number of *frozen* FlexItems in this line, based on FlexItem::IsFrozen().
  // Mostly used for optimization purposes, e.g. to bail out early from loops
  // when we can tell they have nothing left to do.
  uint32_t mNumFrozenItems = 0;

  // Sum of margin/border/padding for the FlexItems in this FlexLine.
  nscoord mTotalItemMBP = 0;

  // Sum of FlexItems' outer hypothetical main sizes and all main-axis
  // {row,columnm}-gaps between items.
  // (i.e. their flex base sizes, clamped via their min/max-size properties,
  // plus their main-axis margin/border/padding, plus the sum of the gaps.)
  //
  // This variable uses a 64-bit coord type to avoid integer overflow in case
  // several of the individual items have huge hypothetical main sizes, which
  // can happen with percent-width table-layout:fixed descendants. We have to
  // avoid integer overflow in order to shrink items properly in that scenario.
  AuCoord64 mTotalOuterHypotheticalMainSize = 0;

  nscoord mLineCrossSize = 0;
  nscoord mFirstBaselineOffset = nscoord_MIN;
  nscoord mLastBaselineOffset = nscoord_MIN;

  // Maintain size of each {row,column}-gap in the main axis
  const nscoord mMainGapSize;
};

// Information about a strut left behind by a FlexItem that's been collapsed
// using "visibility:collapse".
struct nsFlexContainerFrame::StrutInfo {
  StrutInfo(uint32_t aItemIdx, nscoord aStrutCrossSize)
      : mItemIdx(aItemIdx), mStrutCrossSize(aStrutCrossSize) {}

  uint32_t mItemIdx;        // Index in the child list.
  nscoord mStrutCrossSize;  // The cross-size of this strut.
};

// Flex data shared by the flex container frames in a continuation chain, owned
// by the first-in-flow. The data is initialized at the end of the
// first-in-flow's Reflow().
struct nsFlexContainerFrame::SharedFlexData {
  nsTArray<FlexLine> mLines;

  // The final content main/cross size computed by DoFlexLayout.
  nscoord mContentBoxMainSize = NS_UNCONSTRAINEDSIZE;
  nscoord mContentBoxCrossSize = NS_UNCONSTRAINEDSIZE;

  // The frame property under which this struct is stored. Set only on the
  // first-in-flow.
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(Prop, SharedFlexData)
};

// Forward iterate all the FlexItems in aLines.
class nsFlexContainerFrame::FlexItemIterator final {
 public:
  explicit FlexItemIterator(const nsTArray<FlexLine>& aLines)
      : mLineIter(aLines.begin()),
        mLineIterEnd(aLines.end()),
        mItemIter(mLineIter->Items().begin()),
        mItemIterEnd(mLineIter->Items().end()) {
    MOZ_ASSERT(mLineIter != mLineIterEnd,
               "Flex container should have at least one FlexLine!");

    if (mItemIter == mItemIterEnd) {
      // The flex container is empty, so advance to mLineIterEnd.
      ++mLineIter;
      MOZ_ASSERT(AtEnd());
    }
  }

  void Next() {
    MOZ_ASSERT(!AtEnd());
    ++mItemIter;

    if (mItemIter == mItemIterEnd) {
      // We are pointing to the end of the flex items, so advance to the next
      // line.
      ++mLineIter;

      if (mLineIter != mLineIterEnd) {
        mItemIter = mLineIter->Items().begin();
        mItemIterEnd = mLineIter->Items().end();
        MOZ_ASSERT(mItemIter != mItemIterEnd,
                   "Why do we have a FlexLine with no FlexItem?");
      }
    }
  }

  bool AtEnd() const {
    MOZ_ASSERT(
        (mLineIter == mLineIterEnd && mItemIter == mItemIterEnd) ||
            (mLineIter != mLineIterEnd && mItemIter != mItemIterEnd),
        "Line & item iterators should agree on whether we're at the end!");
    return mLineIter == mLineIterEnd;
  }

  const FlexItem& operator*() const {
    MOZ_ASSERT(!AtEnd());
    return mItemIter.operator*();
  }

  const FlexItem* operator->() const {
    MOZ_ASSERT(!AtEnd());
    return mItemIter.operator->();
  }

 private:
  nsTArray<FlexLine>::const_iterator mLineIter;
  nsTArray<FlexLine>::const_iterator mLineIterEnd;
  nsTArray<FlexItem>::const_iterator mItemIter;
  nsTArray<FlexItem>::const_iterator mItemIterEnd;
};

static void BuildStrutInfoFromCollapsedItems(const nsTArray<FlexLine>& aLines,
                                             nsTArray<StrutInfo>& aStruts) {
  MOZ_ASSERT(aStruts.IsEmpty(),
             "We should only build up StrutInfo once per reflow, so "
             "aStruts should be empty when this is called");

  uint32_t itemIdxInContainer = 0;
  for (const FlexLine& line : aLines) {
    for (const FlexItem& item : line.Items()) {
      if (StyleVisibility::Collapse ==
          item.Frame()->StyleVisibility()->mVisible) {
        // Note the cross size of the line as the item's strut size.
        aStruts.AppendElement(
            StrutInfo(itemIdxInContainer, line.LineCrossSize()));
      }
      itemIdxInContainer++;
    }
  }
}

static mozilla::StyleAlignFlags SimplifyAlignOrJustifyContentForOneItem(
    const StyleContentDistribution& aAlignmentVal, bool aIsAlign) {
  // Mask away any explicit fallback, to get the main (non-fallback) part of
  // the specified value:
  StyleAlignFlags specified = aAlignmentVal.primary;

  // XXX strip off <overflow-position> bits until we implement it (bug 1311892)
  specified &= ~StyleAlignFlags::FLAG_BITS;

  // FIRST: handle a special-case for "justify-content:stretch" (or equivalent),
  // which requires that we ignore any author-provided explicit fallback value.
  if (specified == StyleAlignFlags::NORMAL) {
    // In a flex container, *-content: "'normal' behaves as 'stretch'".
    // Do that conversion early, so it benefits from our 'stretch' special-case.
    // https://drafts.csswg.org/css-align-3/#distribution-flex
    specified = StyleAlignFlags::STRETCH;
  }
  if (!aIsAlign && specified == StyleAlignFlags::STRETCH) {
    // In a flex container, in "justify-content Axis: [...] 'stretch' behaves
    // as 'flex-start' (ignoring the specified fallback alignment, if any)."
    // https://drafts.csswg.org/css-align-3/#distribution-flex
    // So, we just directly return 'flex-start', & ignore explicit fallback..
    return StyleAlignFlags::FLEX_START;
  }

  // TODO: Check for an explicit fallback value (and if it's present, use it)
  // here once we parse it, see https://github.com/w3c/csswg-drafts/issues/1002.

  // If there's no explicit fallback, use the implied fallback values for
  // space-{between,around,evenly} (since those values only make sense with
  // multiple alignment subjects), and otherwise just use the specified value:
  if (specified == StyleAlignFlags::SPACE_BETWEEN) {
    return StyleAlignFlags::FLEX_START;
  }
  if (specified == StyleAlignFlags::SPACE_AROUND ||
      specified == StyleAlignFlags::SPACE_EVENLY) {
    return StyleAlignFlags::CENTER;
  }
  return specified;
}

bool nsFlexContainerFrame::DrainSelfOverflowList() {
  return DrainAndMergeSelfOverflowList();
}

void nsFlexContainerFrame::AppendFrames(ChildListID aListID,
                                        nsFrameList& aFrameList) {
  NoteNewChildren(aListID, aFrameList);
  nsContainerFrame::AppendFrames(aListID, aFrameList);
}

void nsFlexContainerFrame::InsertFrames(
    ChildListID aListID, nsIFrame* aPrevFrame,
    const nsLineList::iterator* aPrevFrameLine, nsFrameList& aFrameList) {
  NoteNewChildren(aListID, aFrameList);
  nsContainerFrame::InsertFrames(aListID, aPrevFrame, aPrevFrameLine,
                                 aFrameList);
}

void nsFlexContainerFrame::RemoveFrame(ChildListID aListID,
                                       nsIFrame* aOldFrame) {
  MOZ_ASSERT(aListID == kPrincipalList, "unexpected child list");

#ifdef DEBUG
  SetDidPushItemsBitIfNeeded(aListID, aOldFrame);
#endif

  nsContainerFrame::RemoveFrame(aListID, aOldFrame);
}

StyleAlignFlags nsFlexContainerFrame::CSSAlignmentForAbsPosChild(
    const ReflowInput& aChildRI, LogicalAxis aLogicalAxis) const {
  const FlexboxAxisTracker axisTracker(this);

  // If we're row-oriented and the caller is asking about our inline axis (or
  // alternately, if we're column-oriented and the caller is asking about our
  // block axis), then the caller is really asking about our *main* axis.
  // Otherwise, the caller is asking about our cross axis.
  const bool isMainAxis =
      (axisTracker.IsRowOriented() == (aLogicalAxis == eLogicalAxisInline));
  const nsStylePosition* containerStylePos = StylePosition();
  const bool isAxisReversed = isMainAxis ? axisTracker.IsMainAxisReversed()
                                         : axisTracker.IsCrossAxisReversed();

  StyleAlignFlags alignment{0};
  StyleAlignFlags alignmentFlags{0};
  if (isMainAxis) {
    alignment = SimplifyAlignOrJustifyContentForOneItem(
        containerStylePos->mJustifyContent,
        /*aIsAlign = */ false);
  } else {
    const StyleAlignFlags alignContent =
        SimplifyAlignOrJustifyContentForOneItem(
            containerStylePos->mAlignContent,
            /*aIsAlign = */ true);
    if (StyleFlexWrap::Nowrap != containerStylePos->mFlexWrap &&
        alignContent != StyleAlignFlags::STRETCH) {
      // Multi-line, align-content isn't stretch --> align-content determines
      // this child's alignment in the cross axis.
      alignment = alignContent;
    } else {
      // Single-line, or multi-line but the (one) line stretches to fill
      // container. Respect align-self.
      alignment = aChildRI.mStylePosition->UsedAlignSelf(Style())._0;
      // Extract and strip align flag bits
      alignmentFlags = alignment & StyleAlignFlags::FLAG_BITS;
      alignment &= ~StyleAlignFlags::FLAG_BITS;

      if (alignment == StyleAlignFlags::NORMAL) {
        // "the 'normal' keyword behaves as 'start' on replaced
        // absolutely-positioned boxes, and behaves as 'stretch' on all other
        // absolutely-positioned boxes."
        // https://drafts.csswg.org/css-align/#align-abspos
        alignment = aChildRI.mFrame->IsFrameOfType(nsIFrame::eReplaced)
                        ? StyleAlignFlags::START
                        : StyleAlignFlags::STRETCH;
      }
    }
  }

  if (alignment == StyleAlignFlags::STRETCH) {
    // The default fallback alignment for 'stretch' is 'flex-start'.
    alignment = StyleAlignFlags::FLEX_START;
  }

  // Resolve flex-start, flex-end, auto, left, right, baseline, last baseline;
  if (alignment == StyleAlignFlags::FLEX_START) {
    alignment = isAxisReversed ? StyleAlignFlags::END : StyleAlignFlags::START;
  } else if (alignment == StyleAlignFlags::FLEX_END) {
    alignment = isAxisReversed ? StyleAlignFlags::START : StyleAlignFlags::END;
  } else if (alignment == StyleAlignFlags::LEFT ||
             alignment == StyleAlignFlags::RIGHT) {
    MOZ_ASSERT(isMainAxis, "Only justify-* can have 'left' and 'right'!");
    alignment = axisTracker.ResolveJustifyLeftRight(alignment);
  } else if (alignment == StyleAlignFlags::BASELINE) {
    alignment = StyleAlignFlags::START;
  } else if (alignment == StyleAlignFlags::LAST_BASELINE) {
    alignment = StyleAlignFlags::END;
  }

  MOZ_ASSERT(alignment != StyleAlignFlags::STRETCH,
             "We should've converted 'stretch' to the fallback alignment!");
  MOZ_ASSERT(alignment != StyleAlignFlags::FLEX_START &&
                 alignment != StyleAlignFlags::FLEX_END,
             "nsAbsoluteContainingBlock doesn't know how to handle "
             "flex-relative axis for flex containers!");

  return (alignment | alignmentFlags);
}

FlexItem* nsFlexContainerFrame::GenerateFlexItemForChild(
    FlexLine& aLine, nsIFrame* aChildFrame,
    const ReflowInput& aParentReflowInput,
    const FlexboxAxisTracker& aAxisTracker,
    const nscoord aTentativeContentBoxCrossSize, bool aHasLineClampEllipsis) {
  const auto flexWM = aAxisTracker.GetWritingMode();
  const auto childWM = aChildFrame->GetWritingMode();

  // Note: we use GetStyleFrame() to access the sizing & flex properties here.
  // This lets us correctly handle table wrapper frames as flex items since
  // their inline-size and block-size properties are always 'auto'. In order for
  // 'flex-basis:auto' to actually resolve to the author's specified inline-size
  // or block-size, we need to dig through to the inner table.
  const auto* stylePos =
      nsLayoutUtils::GetStyleFrame(aChildFrame)->StylePosition();

  // Construct a StyleSizeOverrides for this flex item so that its ReflowInput
  // below will use and resolve its flex base size rather than its corresponding
  // preferred main size property (only for modern CSS flexbox).
  StyleSizeOverrides sizeOverrides;
  if (!IsLegacyBox(this)) {
    Maybe<StyleSize> styleFlexBaseSize;

    // When resolving flex base size, flex items use their 'flex-basis' property
    // in place of their preferred main size (e.g. 'width') for sizing purposes,
    // *unless* they have 'flex-basis:auto' in which case they use their
    // preferred main size after all.
    const auto& flexBasis = stylePos->mFlexBasis;
    const auto& styleMainSize = stylePos->Size(aAxisTracker.MainAxis(), flexWM);
    if (IsUsedFlexBasisContent(flexBasis, styleMainSize)) {
      // If we get here, we're resolving the flex base size for a flex item, and
      // we fall into the flexbox spec section 9.2 step 3, substep C (if we have
      // a definite cross size) or E (if not).
      if (aChildFrame->GetAspectRatio()) {
        // FIXME: This is a workaround. Once bug 1670151 is fixed, aspect-ratio
        // will be considered when resolving flex item's flex base size with the
        // value 'max-content'.
        styleFlexBaseSize.emplace(StyleSize::Auto());
      } else {
        styleFlexBaseSize.emplace(StyleSize::MaxContent());
      }
    } else if (flexBasis.IsSize() && !flexBasis.IsAuto()) {
      // For all other non-'auto' flex-basis values, we just swap in the
      // flex-basis itself for the preferred main-size property.
      styleFlexBaseSize.emplace(flexBasis.AsSize());
    } else {
      // else: flex-basis is 'auto', which is deferring to some explicit value
      // in the preferred main size.
      MOZ_ASSERT(flexBasis.IsAuto());
      styleFlexBaseSize.emplace(styleMainSize);
    }

    MOZ_ASSERT(styleFlexBaseSize, "We should've emplace styleFlexBaseSize!");

    // Provide the size override for the preferred main size property.
    if (aAxisTracker.IsInlineAxisMainAxis(childWM)) {
      sizeOverrides.mStyleISize = std::move(styleFlexBaseSize);
    } else {
      sizeOverrides.mStyleBSize = std::move(styleFlexBaseSize);
    }

    // 'flex-basis' should works on the inner table frame for a table flex item,
    // just like how 'height' works on a table element.
    sizeOverrides.mApplyOverridesVerbatim = true;
  }

  // Create temporary reflow input just for sizing -- to get hypothetical
  // main-size and the computed values of min / max main-size property.
  // (This reflow input will _not_ be used for reflow.)
  ReflowInput childRI(PresContext(), aParentReflowInput, aChildFrame,
                      aParentReflowInput.ComputedSize(childWM), Nothing(), {},
                      sizeOverrides);
  childRI.mFlags.mInsideLineClamp = GetLineClampValue() != 0;

  // FLEX GROW & SHRINK WEIGHTS
  // --------------------------
  float flexGrow, flexShrink;
  if (IsLegacyBox(this)) {
    if (GetLineClampValue() != 0) {
      // Items affected by -webkit-line-clamp are always inflexible.
      flexGrow = flexShrink = 0;
    } else {
      flexGrow = flexShrink = aChildFrame->StyleXUL()->mBoxFlex;
    }
  } else {
    flexGrow = stylePos->mFlexGrow;
    flexShrink = stylePos->mFlexShrink;
  }

  // MAIN SIZES (flex base size, min/max size)
  // -----------------------------------------
  nscoord flexBaseSize = GET_MAIN_COMPONENT_LOGICAL(
      aAxisTracker, childWM, childRI.ComputedISize(), childRI.ComputedBSize());
  nscoord mainMinSize = GET_MAIN_COMPONENT_LOGICAL(aAxisTracker, childWM,
                                                   childRI.ComputedMinISize(),
                                                   childRI.ComputedMinBSize());
  nscoord mainMaxSize = GET_MAIN_COMPONENT_LOGICAL(aAxisTracker, childWM,
                                                   childRI.ComputedMaxISize(),
                                                   childRI.ComputedMaxBSize());
  // This is enforced by the ReflowInput where these values come from:
  MOZ_ASSERT(mainMinSize <= mainMaxSize, "min size is larger than max size");

  // CROSS SIZES (tentative cross size, min/max cross size)
  // ------------------------------------------------------
  // Grab the cross size from the reflow input. This might be the right value,
  // or we might resolve it to something else in SizeItemInCrossAxis(); hence,
  // it's tentative. See comment under "Cross Size Determination" for more.
  nscoord tentativeCrossSize = GET_CROSS_COMPONENT_LOGICAL(
      aAxisTracker, childWM, childRI.ComputedISize(), childRI.ComputedBSize());
  nscoord crossMinSize = GET_CROSS_COMPONENT_LOGICAL(
      aAxisTracker, childWM, childRI.ComputedMinISize(),
      childRI.ComputedMinBSize());
  nscoord crossMaxSize = GET_CROSS_COMPONENT_LOGICAL(
      aAxisTracker, childWM, childRI.ComputedMaxISize(),
      childRI.ComputedMaxBSize());

  // SPECIAL-CASE FOR WIDGET-IMPOSED SIZES
  // Check if we're a themed widget, in which case we might have a minimum
  // main & cross size imposed by our widget (which we can't go below), or
  // (more severe) our widget might have only a single valid size.
  bool isFixedSizeWidget = false;
  const nsStyleDisplay* disp = aChildFrame->StyleDisplay();
  if (aChildFrame->IsThemed(disp)) {
    LayoutDeviceIntSize widgetMinSize;
    bool canOverride = true;
    PresContext()->Theme()->GetMinimumWidgetSize(PresContext(), aChildFrame,
                                                 disp->EffectiveAppearance(),
                                                 &widgetMinSize, &canOverride);

    nscoord widgetMainMinSize = PresContext()->DevPixelsToAppUnits(
        aAxisTracker.MainComponent(widgetMinSize));
    nscoord widgetCrossMinSize = PresContext()->DevPixelsToAppUnits(
        aAxisTracker.CrossComponent(widgetMinSize));

    // GetMinimumWidgetSize() returns border-box. We need content-box, so
    // subtract borderPadding.
    const LogicalMargin bpInFlexWM =
        childRI.ComputedLogicalBorderPadding(flexWM);
    widgetMainMinSize -= aAxisTracker.MarginSizeInMainAxis(bpInFlexWM);
    widgetCrossMinSize -= aAxisTracker.MarginSizeInCrossAxis(bpInFlexWM);
    // ... (but don't let that push these min sizes below 0).
    widgetMainMinSize = std::max(0, widgetMainMinSize);
    widgetCrossMinSize = std::max(0, widgetCrossMinSize);

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

      if (tentativeCrossSize != NS_UNCONSTRAINEDSIZE) {
        tentativeCrossSize = std::max(tentativeCrossSize, widgetCrossMinSize);
      }
      crossMinSize = std::max(crossMinSize, widgetCrossMinSize);
      crossMaxSize = std::max(crossMaxSize, widgetCrossMinSize);
    }
  }

  // Construct the flex item!
  FlexItem* item = aLine.Items().EmplaceBack(
      childRI, flexGrow, flexShrink, flexBaseSize, mainMinSize, mainMaxSize,
      tentativeCrossSize, crossMinSize, crossMaxSize, aAxisTracker);

  // We may be about to do computations based on our item's cross-size
  // (e.g. using it as a constraint when measuring our content in the
  // main axis, or using it with the preferred aspect ratio to obtain a main
  // size). BEFORE WE DO THAT, we need let the item "pre-stretch" its cross size
  // (if it's got 'align-self:stretch'), for a certain case where the spec says
  // the stretched cross size is considered "definite". That case is if we
  // have a single-line (nowrap) flex container which itself has a definite
  // cross-size.  Otherwise, we'll wait to do stretching, since (in other
  // cases) we don't know how much the item should stretch yet.
  const bool isSingleLine =
      StyleFlexWrap::Nowrap == aParentReflowInput.mStylePosition->mFlexWrap;
  if (isSingleLine) {
    // Is container's cross size "definite"?
    // - If it's column-oriented, then "yes", because its cross size is its
    // inline-size which is always definite from its descendants' perspective.
    // - Otherwise (if it's row-oriented), then we check the actual size
    // and call it definite if it's not NS_UNCONSTRAINEDSIZE.
    if (aAxisTracker.IsColumnOriented() ||
        aTentativeContentBoxCrossSize != NS_UNCONSTRAINEDSIZE) {
      // Container's cross size is "definite", so we can resolve the item's
      // stretched cross size using that.
      item->ResolveStretchedCrossSize(aTentativeContentBoxCrossSize);
    }
  }

  // Before thinking about freezing the item at its base size, we need to give
  // it a chance to recalculate the base size from its cross size and aspect
  // ratio (since its cross size might've *just* now become definite due to
  // 'stretch' above)
  item->ResolveFlexBaseSizeFromAspectRatio(childRI);

  // If we're inflexible, we can just freeze to our hypothetical main-size
  // up-front. Similarly, if we're a fixed-size widget, we only have one
  // valid size, so we freeze to keep ourselves from flexing.
  if (isFixedSizeWidget || (flexGrow == 0.0f && flexShrink == 0.0f)) {
    item->Freeze();
    if (flexBaseSize < mainMinSize) {
      item->SetWasMinClamped();
    } else if (flexBaseSize > mainMaxSize) {
      item->SetWasMaxClamped();
    }
  }

  // Resolve "flex-basis:auto" and/or "min-[width|height]:auto" (which might
  // require us to reflow the item to measure content height)
  ResolveAutoFlexBasisAndMinSize(*item, childRI, aAxisTracker,
                                 aHasLineClampEllipsis);
  return item;
}

// Static helper-functions for ResolveAutoFlexBasisAndMinSize():
// -------------------------------------------------------------
// Partially resolves "min-[width|height]:auto" and returns the resulting value.
// By "partially", I mean we don't consider the min-content size (but we do
// consider the main-size and main max-size properties, and the preferred aspect
// ratio). The caller is responsible for computing & considering the min-content
// size in combination with the partially-resolved value that this function
// returns.
//
// Basically, this function gets the specified size suggestion; if not, the
// transferred size suggestion; if both sizes do not exist, return nscoord_MAX.
//
// Spec reference: https://drafts.csswg.org/css-flexbox-1/#min-size-auto
static nscoord PartiallyResolveAutoMinSize(
    const FlexItem& aFlexItem, const ReflowInput& aItemReflowInput,
    const FlexboxAxisTracker& aAxisTracker) {
  MOZ_ASSERT(aFlexItem.NeedsMinSizeAutoResolution(),
             "only call for FlexItems that need min-size auto resolution");

  const auto itemWM = aFlexItem.GetWritingMode();
  const auto cbWM = aAxisTracker.GetWritingMode();
  const auto& mainStyleSize =
      aItemReflowInput.mStylePosition->Size(aAxisTracker.MainAxis(), cbWM);
  const auto& maxMainStyleSize =
      aItemReflowInput.mStylePosition->MaxSize(aAxisTracker.MainAxis(), cbWM);
  const auto boxSizingAdjust =
      aItemReflowInput.mStylePosition->mBoxSizing == StyleBoxSizing::Border
          ? aFlexItem.BorderPadding().Size(cbWM)
          : LogicalSize(cbWM);

  // If this flex item is a compressible replaced element list in CSS Sizing 3
  // §5.2.2, CSS Sizing 3 §5.2.1c requires us to resolve the percentage part of
  // the preferred main size property against zero, yielding a definite
  // specified size suggestion. Here we can use a zero percentage basis to
  // fulfill this requirement.
  const auto percentBasis =
      aFlexItem.Frame()->IsPercentageResolvedAgainstZero(mainStyleSize,
                                                         maxMainStyleSize)
          ? LogicalSize(cbWM, 0, 0)
          : aItemReflowInput.mContainingBlockSize.ConvertTo(cbWM, itemWM);

  // Compute the specified size suggestion, which is the main-size property if
  // it's definite.
  nscoord specifiedSizeSuggestion = nscoord_MAX;

  if (aAxisTracker.IsRowOriented()) {
    if (mainStyleSize.IsLengthPercentage()) {
      // NOTE: We ignore extremum inline-size. This is OK because the caller is
      // responsible for computing the min-content inline-size and min()'ing it
      // with the value we return.
      specifiedSizeSuggestion = aFlexItem.Frame()->ComputeISizeValue(
          cbWM, percentBasis, boxSizingAdjust,
          mainStyleSize.AsLengthPercentage());
    }
  } else {
    if (!nsLayoutUtils::IsAutoBSize(mainStyleSize, percentBasis.BSize(cbWM))) {
      // NOTE: We ignore auto and extremum block-size. This is OK because the
      // caller is responsible for computing the min-content block-size and
      // min()'ing it with the value we return.
      specifiedSizeSuggestion = nsLayoutUtils::ComputeBSizeValue(
          percentBasis.BSize(cbWM), boxSizingAdjust.BSize(cbWM),
          mainStyleSize.AsLengthPercentage());
    }
  }

  if (specifiedSizeSuggestion != nscoord_MAX) {
    // We have the specified size suggestion. Return it now since we don't need
    // to consider transferred size suggestion.
    FLEX_LOGV(" Specified size suggestion: %d", specifiedSizeSuggestion);
    return specifiedSizeSuggestion;
  }

  // Compute the transferred size suggestion, which is the cross size converted
  // through the aspect ratio (if the item is replaced, and it has an aspect
  // ratio and a definite cross size).
  if (const auto& aspectRatio = aFlexItem.GetAspectRatio();
      aFlexItem.Frame()->IsFrameOfType(nsIFrame::eReplaced) && aspectRatio &&
      aFlexItem.IsCrossSizeDefinite(aItemReflowInput)) {
    // We have a usable aspect ratio. (not going to divide by 0)
    nscoord transferredSizeSuggestion = aspectRatio.ComputeRatioDependentSize(
        aFlexItem.MainAxis(), cbWM, aFlexItem.CrossSize(), boxSizingAdjust);

    // Clamp the transferred size suggestion by any definite min and max
    // cross size converted through the aspect ratio.
    transferredSizeSuggestion = aFlexItem.ClampMainSizeViaCrossAxisConstraints(
        transferredSizeSuggestion, aItemReflowInput);

    FLEX_LOGV(" Transferred size suggestion: %d", transferredSizeSuggestion);
    return transferredSizeSuggestion;
  }

  return nscoord_MAX;
}

// Note: If & when we handle "min-height: min-content" for flex items,
// we may want to resolve that in this function, too.
void nsFlexContainerFrame::ResolveAutoFlexBasisAndMinSize(
    FlexItem& aFlexItem, const ReflowInput& aItemReflowInput,
    const FlexboxAxisTracker& aAxisTracker, bool aHasLineClampEllipsis) {
  // (Note: We can guarantee that the flex-basis will have already been
  // resolved if the main axis is the same as the item's inline
  // axis. Inline-axis values should always be resolvable without reflow.)
  const bool isMainSizeAuto =
      (!aFlexItem.IsInlineAxisMainAxis() &&
       NS_UNCONSTRAINEDSIZE == aFlexItem.FlexBaseSize());

  const bool isMainMinSizeAuto = aFlexItem.NeedsMinSizeAutoResolution();

  if (!isMainSizeAuto && !isMainMinSizeAuto) {
    // Nothing to do; this function is only needed for flex items
    // with a used flex-basis of "auto" or a min-main-size of "auto".
    return;
  }

  FLEX_LOGV("Resolving auto main size or auto min main size for flex item %p",
            aFlexItem.Frame());

  nscoord resolvedMinSize;  // (only set/used if isMainMinSizeAuto==true)
  bool minSizeNeedsToMeasureContent = false;  // assume the best
  if (isMainMinSizeAuto) {
    // Resolve the min-size, except for considering the min-content size.
    // (We'll consider that later, if we need to.)
    resolvedMinSize =
        PartiallyResolveAutoMinSize(aFlexItem, aItemReflowInput, aAxisTracker);
    if (resolvedMinSize > 0) {
      // If resolvedMinSize were already at 0, we could skip calculating content
      // size suggestion because it can't go any lower.
      minSizeNeedsToMeasureContent = true;
    }
  }

  const bool flexBasisNeedsToMeasureContent = isMainSizeAuto;

  // Measure content, if needed (w/ intrinsic-width method or a reflow)
  if (minSizeNeedsToMeasureContent || flexBasisNeedsToMeasureContent) {
    // Compute the content size suggestion, which is the min-content size in the
    // main axis.
    nscoord contentSizeSuggestion = nscoord_MAX;

    if (aFlexItem.IsInlineAxisMainAxis()) {
      if (minSizeNeedsToMeasureContent) {
        // Compute the flex item's content size suggestion, which is the
        // 'min-content' size on the main axis.
        // https://drafts.csswg.org/css-flexbox-1/#content-size-suggestion
        const auto cbWM = aAxisTracker.GetWritingMode();
        const auto itemWM = aFlexItem.GetWritingMode();
        const nscoord availISize = 0;  // for min-content size
        StyleSizeOverrides sizeOverrides;
        sizeOverrides.mStyleISize.emplace(StyleSize::Auto());
        const auto sizeInItemWM = aFlexItem.Frame()->ComputeSize(
            aItemReflowInput.mRenderingContext, itemWM,
            aItemReflowInput.mContainingBlockSize, availISize,
            aItemReflowInput.ComputedLogicalMargin(itemWM).Size(itemWM),
            aItemReflowInput.ComputedLogicalBorderPadding(itemWM).Size(itemWM),
            sizeOverrides, {ComputeSizeFlag::ShrinkWrap});

        contentSizeSuggestion = aAxisTracker.MainComponent(
            sizeInItemWM.mLogicalSize.ConvertTo(cbWM, itemWM));
      }
      NS_ASSERTION(!flexBasisNeedsToMeasureContent,
                   "flex-basis:auto should have been resolved in the "
                   "reflow input, for horizontal flexbox. It shouldn't need "
                   "special handling here");
    } else {
      // If this item is flexible (in its block axis)...
      // OR if we're measuring its 'auto' min-BSize, with its main-size (in its
      // block axis) being something non-"auto"...
      // THEN: we assume that the computed BSize that we're reflowing with now
      // could be different from the one we'll use for this flex item's
      // "actual" reflow later on.  In that case, we need to be sure the flex
      // item treats this as a block-axis resize (regardless of whether there
      // are actually any ancestors being resized in that axis).
      // (Note: We don't have to do this for the inline axis, because
      // InitResizeFlags will always turn on mIsIResize on when it sees that
      // the computed ISize is different from current ISize, and that's all we
      // need.)
      bool forceBResizeForMeasuringReflow =
          !aFlexItem.IsFrozen() ||          // Is the item flexible?
          !flexBasisNeedsToMeasureContent;  // Are we *only* measuring it for
                                            // 'min-block-size:auto'?

      const ReflowInput& flexContainerRI = *aItemReflowInput.mParentReflowInput;
      nscoord contentBSize =
          MeasureFlexItemContentBSize(aFlexItem, forceBResizeForMeasuringReflow,
                                      aHasLineClampEllipsis, flexContainerRI);
      if (minSizeNeedsToMeasureContent) {
        contentSizeSuggestion = contentBSize;
      }
      if (flexBasisNeedsToMeasureContent) {
        aFlexItem.SetFlexBaseSizeAndMainSize(contentBSize);
      }
    }

    if (minSizeNeedsToMeasureContent) {
      // Clamp the content size suggestion by any definite min and max cross
      // size converted through the aspect ratio.
      if (aFlexItem.HasAspectRatio()) {
        contentSizeSuggestion = aFlexItem.ClampMainSizeViaCrossAxisConstraints(
            contentSizeSuggestion, aItemReflowInput);
      }

      FLEX_LOGV(" Content size suggestion: %d", contentSizeSuggestion);
      resolvedMinSize = std::min(resolvedMinSize, contentSizeSuggestion);

      // Clamp the resolved min main size by the max main size if it's definite.
      if (aFlexItem.MainMaxSize() != NS_UNCONSTRAINEDSIZE) {
        resolvedMinSize = std::min(resolvedMinSize, aFlexItem.MainMaxSize());
      } else if (MOZ_UNLIKELY(resolvedMinSize > nscoord_MAX)) {
        NS_WARNING("Bogus resolved auto min main size!");
        // Our resolved min-size is bogus, probably due to some huge sizes in
        // the content. Clamp it to the valid nscoord range, so that we can at
        // least depend on it being <= the max-size (which is also the
        // nscoord_MAX sentinel value if we reach this point).
        resolvedMinSize = nscoord_MAX;
      }
      FLEX_LOGV(" Resolved auto min main size: %d", resolvedMinSize);
    }
  }

  if (isMainMinSizeAuto) {
    aFlexItem.UpdateMainMinSize(resolvedMinSize);
  }
}

/**
 * A cached result for a flex item's block-axis measuring reflow. This cache
 * prevents us from doing exponential reflows in cases of deeply nested flex
 * and scroll frames.
 *
 * We store the cached value in the flex item's frame property table, for
 * simplicity.
 *
 * Right now, we cache the following as a "key", from the item's ReflowInput:
 *   - its ComputedSize
 *   - its min/max block size (in case its ComputedBSize is unconstrained)
 *   - its AvailableBSize
 * ...and we cache the following as the "value", from the item's ReflowOutput:
 *   - its final content-box BSize
 *
 * The assumption here is that a given flex item measurement from our "value"
 * won't change unless one of the pieces of the "key" change, or the flex
 * item's intrinsic size is marked as dirty (due to a style or DOM change).
 * (The latter will cause the cached value to be discarded, in
 * nsIFrame::MarkIntrinsicISizesDirty.)
 *
 * Note that the components of "Key" (mComputed{MinB,MaxB,}Size and
 * mAvailableBSize) are sufficient to catch any changes to the flex container's
 * size that the item may care about for its measuring reflow. Specifically:
 *  - If the item cares about the container's size (e.g. if it has a percent
 *    height and the container's height changes, in a horizontal-WM container)
 *    then that'll be detectable via the item's ReflowInput's "ComputedSize()"
 *    differing from the value in our Key.  And the same applies for the
 *    inline axis.
 *  - If the item is fragmentable (pending bug 939897) and its measured BSize
 *    depends on where it gets fragmented, then that sort of change can be
 *    detected due to the item's ReflowInput's "AvailableBSize()" differing
 *    from the value in our Key.
 *
 * One particular case to consider (& need to be sure not to break when
 * changing this class): the flex item's computed BSize may change between
 * measuring reflows due to how the mIsFlexContainerMeasuringBSize flag affects
 * size computation (see bug 1336708). This is one reason we need to use the
 * computed BSize as part of the key.
 */
class nsFlexContainerFrame::CachedBAxisMeasurement {
  struct Key {
    const LogicalSize mComputedSize;
    const nscoord mComputedMinBSize;
    const nscoord mComputedMaxBSize;
    const nscoord mAvailableBSize;

    explicit Key(const ReflowInput& aRI)
        : mComputedSize(aRI.ComputedSize()),
          mComputedMinBSize(aRI.ComputedMinBSize()),
          mComputedMaxBSize(aRI.ComputedMaxBSize()),
          mAvailableBSize(aRI.AvailableBSize()) {}

    bool operator==(const Key& aOther) const {
      return mComputedSize == aOther.mComputedSize &&
             mComputedMinBSize == aOther.mComputedMinBSize &&
             mComputedMaxBSize == aOther.mComputedMaxBSize &&
             mAvailableBSize == aOther.mAvailableBSize;
    }
  };

  const Key mKey;

  // This could/should be const, but it's non-const for now just because it's
  // assigned via a series of steps in the constructor body:
  nscoord mBSize;

 public:
  CachedBAxisMeasurement(const ReflowInput& aReflowInput,
                         const ReflowOutput& aReflowOutput)
      : mKey(aReflowInput) {
    // To get content-box bsize, we have to subtract off border & padding
    // (and floor at 0 in case the border/padding are too large):
    WritingMode itemWM = aReflowInput.GetWritingMode();
    nscoord borderBoxBSize = aReflowOutput.BSize(itemWM);
    mBSize =
        borderBoxBSize -
        aReflowInput.ComputedLogicalBorderPadding(itemWM).BStartEnd(itemWM);
    mBSize = std::max(0, mBSize);
  }

  /**
   * Returns true if this cached flex item measurement is valid for (i.e. can
   * be expected to match the output of) a measuring reflow whose input
   * parameters are given via aReflowInput.
   */
  bool IsValidFor(const ReflowInput& aReflowInput) const {
    return mKey == Key(aReflowInput);
  }

  nscoord BSize() const { return mBSize; }
};

/**
 * A cached copy of various metrics from a flex item's most recent final reflow.
 * It can be used to determine whether we can optimize away the flex item's
 * final reflow, when we perform an incremental reflow of its flex container.
 */
class CachedFinalReflowMetrics final {
 public:
  CachedFinalReflowMetrics(const ReflowInput& aReflowInput,
                           const ReflowOutput& aReflowOutput)
      : CachedFinalReflowMetrics(aReflowInput.GetWritingMode(), aReflowInput,
                                 aReflowOutput) {}

  CachedFinalReflowMetrics(const FlexItem& aItem, const LogicalSize& aSize)
      : mBorderPadding(aItem.BorderPadding().ConvertTo(
            aItem.GetWritingMode(), aItem.ContainingBlockWM())),
        mSize(aSize),
        mTreatBSizeAsIndefinite(aItem.TreatBSizeAsIndefinite()) {}

  const LogicalSize& Size() const { return mSize; }
  const LogicalMargin& BorderPadding() const { return mBorderPadding; }
  bool TreatBSizeAsIndefinite() const { return mTreatBSizeAsIndefinite; }

 private:
  // A convenience constructor with a WritingMode argument.
  CachedFinalReflowMetrics(WritingMode aWM, const ReflowInput& aReflowInput,
                           const ReflowOutput& aReflowOutput)
      : mBorderPadding(aReflowInput.ComputedLogicalBorderPadding(aWM)),
        mSize(aReflowOutput.Size(aWM) - mBorderPadding.Size(aWM)),
        mTreatBSizeAsIndefinite(aReflowInput.mFlags.mTreatBSizeAsIndefinite) {}

  // The flex item's border and padding, in its own writing-mode, that it used
  // used during its most recent "final reflow".
  LogicalMargin mBorderPadding;

  // The flex item's content-box size, in its own writing-mode, that it used
  // during its most recent "final reflow".
  LogicalSize mSize;

  // True if the flex item's BSize was considered "indefinite" in its most
  // recent "final reflow". (For a flex item "final reflow", this is fully
  // determined by the mTreatBSizeAsIndefinite flag in ReflowInput. See the
  // flag's documentation for more information.)
  bool mTreatBSizeAsIndefinite;
};

/**
 * When we instantiate/update a CachedFlexItemData, this enum must be used to
 * indicate the sort of reflow whose results we're capturing. This impacts
 * what we cache & how we use the cached information.
 */
enum class FlexItemReflowType {
  // A reflow to measure the block-axis size of a flex item (as an input to the
  // flex layout algorithm).
  Measuring,

  // A reflow with the flex item's "final" size at the end of the flex layout
  // algorithm.
  Final,
};

/**
 * This class stores information about the conditions and results for the most
 * recent ReflowChild call that we made on a given flex item.  This information
 * helps us reason about whether we can assume that a subsequent ReflowChild()
 * invocation is unnecessary & skippable.
 */
class nsFlexContainerFrame::CachedFlexItemData {
 public:
  CachedFlexItemData(const ReflowInput& aReflowInput,
                     const ReflowOutput& aReflowOutput,
                     FlexItemReflowType aType) {
    Update(aReflowInput, aReflowOutput, aType);
  }

  // This method is intended to be called after we perform either a "measuring
  // reflow" or a "final reflow" for a given flex item.
  void Update(const ReflowInput& aReflowInput,
              const ReflowOutput& aReflowOutput, FlexItemReflowType aType) {
    if (aType == FlexItemReflowType::Measuring) {
      mBAxisMeasurement.reset();
      mBAxisMeasurement.emplace(aReflowInput, aReflowOutput);
      // Clear any cached "last final reflow metrics", too, because now the most
      // recent reflow was *not* a "final reflow".
      mFinalReflowMetrics.reset();
      return;
    }

    MOZ_ASSERT(aType == FlexItemReflowType::Final);
    mFinalReflowMetrics.reset();
    mFinalReflowMetrics.emplace(aReflowInput, aReflowOutput);
  }

  // This method is intended to be called for situations where we decide to
  // skip a final reflow because we've just done a measuring reflow which left
  // us (and our descendants) with the correct sizes. In this scenario, we
  // still want to cache the size as if we did a final reflow (because we've
  // determined that the recent measuring reflow was sufficient).  That way,
  // our flex container can still skip a final reflow for this item in the
  // future as long as conditions are right.
  void Update(const FlexItem& aItem, const LogicalSize& aSize) {
    MOZ_ASSERT(!mFinalReflowMetrics,
               "This version of the method is only intended to be called when "
               "the most recent reflow was a 'measuring reflow'; and that "
               "should have cleared out mFinalReflowMetrics");

    mFinalReflowMetrics.reset();  // Just in case this assert^ fails.
    mFinalReflowMetrics.emplace(aItem, aSize);
  }

  // If the flex container needs a measuring reflow for the flex item, then the
  // resulting block-axis measurements can be cached here.  If no measurement
  // has been needed so far, then this member will be Nothing().
  Maybe<CachedBAxisMeasurement> mBAxisMeasurement;

  // The metrics that the corresponding flex item used in its most recent
  // "final reflow". (Note: the assumption here is that this reflow was this
  // item's most recent reflow of any type.  If the item ends up undergoing a
  // subsequent measuring reflow, then this value needs to be cleared, because
  // at that point it's no longer an accurate way of reasoning about the
  // current state of the frame tree.)
  Maybe<CachedFinalReflowMetrics> mFinalReflowMetrics;

  // Instances of this class are stored under this frame property, on
  // frames that are flex items:
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(Prop, CachedFlexItemData)
};

void nsFlexContainerFrame::MarkCachedFlexMeasurementsDirty(
    nsIFrame* aItemFrame) {
  MOZ_ASSERT(aItemFrame->IsFlexItem());
  if (auto* cache = aItemFrame->GetProperty(CachedFlexItemData::Prop())) {
    cache->mBAxisMeasurement.reset();
    cache->mFinalReflowMetrics.reset();
  }
}

const CachedBAxisMeasurement& nsFlexContainerFrame::MeasureBSizeForFlexItem(
    FlexItem& aItem, ReflowInput& aChildReflowInput) {
  auto* cachedData = aItem.Frame()->GetProperty(CachedFlexItemData::Prop());

  if (cachedData && cachedData->mBAxisMeasurement) {
    if (!aItem.Frame()->IsSubtreeDirty() &&
        cachedData->mBAxisMeasurement->IsValidFor(aChildReflowInput)) {
      FLEX_LOG("[perf] MeasureBSizeForFlexItem accepted cached value");
      return *(cachedData->mBAxisMeasurement);
    }
    FLEX_LOG("[perf] MeasureBSizeForFlexItem rejected cached value");
  } else {
    FLEX_LOG("[perf] MeasureBSizeForFlexItem didn't have a cached value");
  }

  // CachedFlexItemData is stored in item's writing mode, so we pass
  // aChildReflowInput into ReflowOutput's constructor.
  ReflowOutput childReflowOutput(aChildReflowInput);
  nsReflowStatus childReflowStatus;

  const ReflowChildFlags flags = ReflowChildFlags::NoMoveFrame;
  const WritingMode outerWM = GetWritingMode();
  const LogicalPoint dummyPosition(outerWM);
  const nsSize dummyContainerSize;

  // We use NoMoveFrame, so the position and container size used here are
  // unimportant.
  ReflowChild(aItem.Frame(), PresContext(), childReflowOutput,
              aChildReflowInput, outerWM, dummyPosition, dummyContainerSize,
              flags, childReflowStatus);
  aItem.SetHadMeasuringReflow();

  // We always use unconstrained available block-size to measure flex items,
  // which means they should always complete.
  MOZ_ASSERT(childReflowStatus.IsComplete(),
             "We gave flex item unconstrained available block-size, so it "
             "should be complete");

  // Tell the child we're done with its initial reflow.
  // (Necessary for e.g. GetBaseline() to work below w/out asserting)
  FinishReflowChild(aItem.Frame(), PresContext(), childReflowOutput,
                    &aChildReflowInput, outerWM, dummyPosition,
                    dummyContainerSize, flags);

  aItem.SetAscent(childReflowOutput.BlockStartAscent());

  // Update (or add) our cached measurement, so that we can hopefully skip this
  // measuring reflow the next time around:
  if (cachedData) {
    cachedData->Update(aChildReflowInput, childReflowOutput,
                       FlexItemReflowType::Measuring);
  } else {
    cachedData = new CachedFlexItemData(aChildReflowInput, childReflowOutput,
                                        FlexItemReflowType::Measuring);
    aItem.Frame()->SetProperty(CachedFlexItemData::Prop(), cachedData);
  }
  return *(cachedData->mBAxisMeasurement);
}

/* virtual */
void nsFlexContainerFrame::MarkIntrinsicISizesDirty() {
  mCachedMinISize = NS_INTRINSIC_ISIZE_UNKNOWN;
  mCachedPrefISize = NS_INTRINSIC_ISIZE_UNKNOWN;

  nsContainerFrame::MarkIntrinsicISizesDirty();
}

nscoord nsFlexContainerFrame::MeasureFlexItemContentBSize(
    FlexItem& aFlexItem, bool aForceBResizeForMeasuringReflow,
    bool aHasLineClampEllipsis, const ReflowInput& aParentReflowInput) {
  FLEX_LOG("Measuring flex item's content block-size");

  // Set up a reflow input for measuring the flex item's content block-size:
  WritingMode wm = aFlexItem.Frame()->GetWritingMode();
  LogicalSize availSize = aParentReflowInput.ComputedSize(wm);
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;

  StyleSizeOverrides sizeOverrides;
  if (aFlexItem.IsStretched()) {
    sizeOverrides.mStyleISize.emplace(aFlexItem.StyleCrossSize());
    // Suppress any AspectRatio that we might have to prevent ComputeSize() from
    // transferring our inline-size override through the aspect-ratio to set the
    // block-size, because that would prevent us from measuring the content
    // block-size.
    sizeOverrides.mAspectRatio.emplace(AspectRatio());
    FLEX_LOGV(" Cross size override: %d", aFlexItem.CrossSize());
  }
  sizeOverrides.mStyleBSize.emplace(StyleSize::Auto());

  ReflowInput childRIForMeasuringBSize(
      PresContext(), aParentReflowInput, aFlexItem.Frame(), availSize,
      Nothing(), ReflowInput::InitFlag::CallerWillInit, sizeOverrides);
  childRIForMeasuringBSize.mFlags.mInsideLineClamp = GetLineClampValue() != 0;
  childRIForMeasuringBSize.mFlags.mApplyLineClamp =
      childRIForMeasuringBSize.mFlags.mInsideLineClamp || aHasLineClampEllipsis;
  childRIForMeasuringBSize.Init(PresContext());

  // When measuring flex item's content block-size, disregard the item's
  // min-block-size and max-block-size by resetting both to to their
  // unconstraining (extreme) values. The flexbox layout algorithm does still
  // explicitly clamp both sizes when resolving the target main size.
  childRIForMeasuringBSize.ComputedMinBSize() = 0;
  childRIForMeasuringBSize.ComputedMaxBSize() = NS_UNCONSTRAINEDSIZE;

  if (aForceBResizeForMeasuringReflow) {
    childRIForMeasuringBSize.SetBResize(true);
    // Not 100% sure this is needed, but be conservative for now:
    childRIForMeasuringBSize.mFlags.mIsBResizeForPercentages = true;
  }

  const CachedBAxisMeasurement& measurement =
      MeasureBSizeForFlexItem(aFlexItem, childRIForMeasuringBSize);

  return measurement.BSize();
}

FlexItem::FlexItem(ReflowInput& aFlexItemReflowInput, float aFlexGrow,
                   float aFlexShrink, nscoord aFlexBaseSize,
                   nscoord aMainMinSize, nscoord aMainMaxSize,
                   nscoord aTentativeCrossSize, nscoord aCrossMinSize,
                   nscoord aCrossMaxSize,
                   const FlexboxAxisTracker& aAxisTracker)
    : mFrame(aFlexItemReflowInput.mFrame),
      mFlexGrow(aFlexGrow),
      mFlexShrink(aFlexShrink),
      mAspectRatio(mFrame->GetAspectRatio()),
      mWM(aFlexItemReflowInput.GetWritingMode()),
      mCBWM(aAxisTracker.GetWritingMode()),
      mMainAxis(aAxisTracker.MainAxis()),
      mBorderPadding(aFlexItemReflowInput.ComputedLogicalBorderPadding(mCBWM)),
      mMargin(aFlexItemReflowInput.ComputedLogicalMargin(mCBWM)),
      mMainMinSize(aMainMinSize),
      mMainMaxSize(aMainMaxSize),
      mCrossMinSize(aCrossMinSize),
      mCrossMaxSize(aCrossMaxSize),
      mCrossSize(aTentativeCrossSize),
      mIsInlineAxisMainAxis(aAxisTracker.IsInlineAxisMainAxis(mWM))
// mNeedsMinSizeAutoResolution is initialized in CheckForMinSizeAuto()
// mAlignSelf, mHasAnyAutoMargin see below
{
  MOZ_ASSERT(mFrame, "expecting a non-null child frame");
  MOZ_ASSERT(!mFrame->IsPlaceholderFrame(),
             "placeholder frames should not be treated as flex items");
  MOZ_ASSERT(!mFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW),
             "out-of-flow frames should not be treated as flex items");
  MOZ_ASSERT(mIsInlineAxisMainAxis ==
                 nsFlexContainerFrame::IsItemInlineAxisMainAxis(mFrame),
             "public API should be consistent with internal state (about "
             "whether flex item's inline axis is flex container's main axis)");

  const ReflowInput* containerRS = aFlexItemReflowInput.mParentReflowInput;
  if (IsLegacyBox(containerRS->mFrame)) {
    // For -webkit-{inline-}box and -moz-{inline-}box, we need to:
    // (1) Use prefixed "box-align" instead of "align-items" to determine the
    //     container's cross-axis alignment behavior.
    // (2) Suppress the ability for flex items to override that with their own
    //     cross-axis alignment. (The legacy box model doesn't support this.)
    // So, each FlexItem simply copies the container's converted "align-items"
    // value and disregards their own "align-self" property.
    const nsStyleXUL* containerStyleXUL = containerRS->mFrame->StyleXUL();
    mAlignSelf = {ConvertLegacyStyleToAlignItems(containerStyleXUL)};
    mAlignSelfFlags = {0};
  } else {
    mAlignSelf = aFlexItemReflowInput.mStylePosition->UsedAlignSelf(
        containerRS->mFrame->Style());
    if (MOZ_LIKELY(mAlignSelf._0 == StyleAlignFlags::NORMAL)) {
      mAlignSelf = {StyleAlignFlags::STRETCH};
    }

    // Store and strip off the <overflow-position> bits
    mAlignSelfFlags = mAlignSelf._0 & StyleAlignFlags::FLAG_BITS;
    mAlignSelf._0 &= ~StyleAlignFlags::FLAG_BITS;
  }

  // Our main-size is considered definite if any of these are true:
  // (a) main axis is the item's inline axis.
  // (b) flex container has definite main size.
  // (c) flex item has a definite flex basis.
  //
  // Hence, we need to take care to treat the final main-size as *indefinite*
  // if none of these conditions are satisfied.
  if (mIsInlineAxisMainAxis) {
    // The item's block-axis is the flex container's cross axis. We don't need
    // any special handling to treat cross sizes as indefinite, because the
    // cases where we stomp on the cross size with a definite value are all...
    // - situations where the spec requires us to treat the cross size as
    // definite; specifically, `align-self:stretch` whose cross size is
    // definite.
    // - situations where definiteness doesn't matter (e.g. for an element with
    // an aspect ratio, which for now are all leaf nodes and hence
    // can't have any percent-height descendants that would care about the
    // definiteness of its size. (Once bug 1528375 is fixed, we might need to
    // be more careful about definite vs. indefinite sizing on flex items with
    // aspect ratios.)
    mTreatBSizeAsIndefinite = false;
  } else {
    // The item's block-axis is the flex container's main axis. So, the flex
    // item's main size is its BSize, and is considered definite under certain
    // conditions laid out for definite flex-item main-sizes in the spec.
    if (aAxisTracker.IsRowOriented() ||
        (containerRS->ComputedBSize() != NS_UNCONSTRAINEDSIZE &&
         !containerRS->mFlags.mTreatBSizeAsIndefinite)) {
      // The flex *container* has a definite main-size (either by being
      // row-oriented [and using its own inline size which is by definition
      // definite, or by being column-oriented and having a definite
      // block-size).  The spec says this means all of the flex items'
      // post-flexing main sizes should *also* be treated as definite.
      mTreatBSizeAsIndefinite = false;
    } else if (aFlexBaseSize != NS_UNCONSTRAINEDSIZE) {
      // The flex item has a definite flex basis, which we'll treat as making
      // its main-size definite.
      mTreatBSizeAsIndefinite = false;
    } else {
      // Otherwise, we have to treat the item's BSize as indefinite.
      mTreatBSizeAsIndefinite = true;
    }
  }

  SetFlexBaseSizeAndMainSize(aFlexBaseSize);
  CheckForMinSizeAuto(aFlexItemReflowInput, aAxisTracker);

  const nsStyleMargin* styleMargin = aFlexItemReflowInput.mStyleMargin;
  mHasAnyAutoMargin = styleMargin->HasInlineAxisAuto(mCBWM) ||
                      styleMargin->HasBlockAxisAuto(mCBWM);

  // Assert that any "auto" margin components are set to 0.
  // (We'll resolve them later; until then, we want to treat them as 0-sized.)
#ifdef DEBUG
  {
    for (const auto side : AllLogicalSides()) {
      if (styleMargin->mMargin.Get(mCBWM, side).IsAuto()) {
        MOZ_ASSERT(GetMarginComponentForSide(side) == 0,
                   "Someone else tried to resolve our auto margin");
      }
    }
  }
#endif  // DEBUG

  // Map align-self 'baseline' value to 'start' when baseline alignment
  // is not possible because the FlexItem's block axis is orthogonal to
  // the cross axis of the container. If that's the case, we just directly
  // convert our align-self value here, so that we don't have to handle this
  // with special cases elsewhere.
  // We are treating this case as one where it is appropriate to use the
  // fallback values defined at https://www.w3.org/TR/css-align/#baseline-values
  if (!IsBlockAxisCrossAxis()) {
    if (mAlignSelf._0 == StyleAlignFlags::BASELINE) {
      mAlignSelf = {StyleAlignFlags::FLEX_START};
    } else if (mAlignSelf._0 == StyleAlignFlags::LAST_BASELINE) {
      mAlignSelf = {StyleAlignFlags::FLEX_END};
    }
  }
}

// Simplified constructor for creating a special "strut" FlexItem, for a child
// with visibility:collapse. The strut has 0 main-size, and it only exists to
// impose a minimum cross size on whichever FlexLine it ends up in.
FlexItem::FlexItem(nsIFrame* aChildFrame, nscoord aCrossSize,
                   WritingMode aContainerWM,
                   const FlexboxAxisTracker& aAxisTracker)
    : mFrame(aChildFrame),
      mWM(aContainerWM),
      mCBWM(aContainerWM),
      mMainAxis(aAxisTracker.MainAxis()),
      mBorderPadding(mCBWM),
      mMargin(mCBWM),
      mCrossSize(aCrossSize),
      // Struts don't do layout, so its WM doesn't matter at this point. So, we
      // just share container's WM for simplicity:
      mIsFrozen(true),
      mIsStrut(true),  // (this is the constructor for making struts, after all)
      mAlignSelf({StyleAlignFlags::FLEX_START}) {
  MOZ_ASSERT(mFrame, "expecting a non-null child frame");
  MOZ_ASSERT(StyleVisibility::Collapse == mFrame->StyleVisibility()->mVisible,
             "Should only make struts for children with 'visibility:collapse'");
  MOZ_ASSERT(!mFrame->IsPlaceholderFrame(),
             "placeholder frames should not be treated as flex items");
  MOZ_ASSERT(!mFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW),
             "out-of-flow frames should not be treated as flex items");
}

void FlexItem::CheckForMinSizeAuto(const ReflowInput& aFlexItemReflowInput,
                                   const FlexboxAxisTracker& aAxisTracker) {
  const nsStylePosition* pos = aFlexItemReflowInput.mStylePosition;
  const nsStyleDisplay* disp = aFlexItemReflowInput.mStyleDisplay;

  // We'll need special behavior for "min-[width|height]:auto" (whichever is in
  // the flex container's main axis) iff:
  // (a) its computed value is "auto"
  // (b) the "overflow" sub-property in the same axis (the main axis) has a
  //     computed value of "visible" and the item does not create a scroll
  //     container.
  const auto& mainMinSize = aAxisTracker.IsRowOriented()
                                ? pos->MinISize(aAxisTracker.GetWritingMode())
                                : pos->MinBSize(aAxisTracker.GetWritingMode());

  // If the scrollable overflow makes us create a scroll container, then we
  // don't need to do any extra resolution for our `min-size:auto` value.
  // We don't need to check for scrollable overflow in a particular axis
  // because this will be true for both or neither axis.
  mNeedsMinSizeAutoResolution =
      IsAutoOrEnumOnBSize(mainMinSize, IsInlineAxisMainAxis()) &&
      !disp->IsScrollableOverflow();
}

nscoord FlexItem::BaselineOffsetFromOuterCrossEdge(
    mozilla::Side aStartSide, bool aUseFirstLineBaseline) const {
  // NOTE:
  //  * We only use baselines for aligning in the flex container's cross axis.
  //  * Baselines are a measurement in the item's block axis.
  // ...so we only expect to get here if the item's block axis is parallel (or
  // antiparallel) to the container's cross axis.  (Otherwise, the FlexItem
  // constructor should've resolved mAlignSelf with a fallback value, which
  // would prevent this function from being called.)
  MOZ_ASSERT(IsBlockAxisCrossAxis(),
             "Only expecting to be doing baseline computations when the "
             "cross axis is the block axis");

  mozilla::Side itemBlockStartSide = mWM.PhysicalSide(eLogicalSideBStart);

  nscoord marginBStartToBaseline = ResolvedAscent(aUseFirstLineBaseline) +
                                   PhysicalMargin().Side(itemBlockStartSide);

  return (aStartSide == itemBlockStartSide)
             ? marginBStartToBaseline
             : OuterCrossSize() - marginBStartToBaseline;
}

bool FlexItem::IsCrossSizeAuto() const {
  const nsStylePosition* stylePos =
      nsLayoutUtils::GetStyleFrame(mFrame)->StylePosition();
  // Check whichever component is in the flex container's cross axis.
  // (IsInlineAxisCrossAxis() tells us whether that's our ISize or BSize, in
  // terms of our own WritingMode, mWM.)
  return IsInlineAxisCrossAxis() ? stylePos->ISize(mWM).IsAuto()
                                 : stylePos->BSize(mWM).IsAuto();
}

bool FlexItem::IsCrossSizeDefinite(const ReflowInput& aItemReflowInput) const {
  if (IsStretched()) {
    // Definite cross-size, imposed via 'align-self:stretch' & flex container.
    return true;
  }

  const nsStylePosition* pos = aItemReflowInput.mStylePosition;
  const auto itemWM = GetWritingMode();

  // The logic here should be similar to the logic for isAutoISize/isAutoBSize
  // in nsContainerFrame::ComputeSizeWithIntrinsicDimensions().
  if (IsInlineAxisCrossAxis()) {
    return !pos->ISize(itemWM).IsAuto();
  }

  nscoord cbBSize = aItemReflowInput.mContainingBlockSize.BSize(itemWM);
  return !nsLayoutUtils::IsAutoBSize(pos->BSize(itemWM), cbBSize);
}

void FlexItem::ResolveFlexBaseSizeFromAspectRatio(
    const ReflowInput& aItemReflowInput) {
  // This implements the Flex Layout Algorithm Step 3B:
  // https://drafts.csswg.org/css-flexbox-1/#algo-main-item
  // If the flex item has ...
  //  - an aspect ratio,
  //  - a [used] flex-basis of 'content', and
  //  - a definite cross size
  // then the flex base size is calculated from its inner cross size and the
  // flex item's preferred aspect ratio.
  if (HasAspectRatio() &&
      nsFlexContainerFrame::IsUsedFlexBasisContent(
          aItemReflowInput.mStylePosition->mFlexBasis,
          aItemReflowInput.mStylePosition->Size(MainAxis(), mCBWM)) &&
      IsCrossSizeDefinite(aItemReflowInput)) {
    const LogicalSize contentBoxSizeToBoxSizingAdjust =
        aItemReflowInput.mStylePosition->mBoxSizing == StyleBoxSizing::Border
            ? BorderPadding().Size(mCBWM)
            : LogicalSize(mCBWM);
    const nscoord mainSizeFromRatio = mAspectRatio.ComputeRatioDependentSize(
        MainAxis(), mCBWM, CrossSize(), contentBoxSizeToBoxSizingAdjust);
    SetFlexBaseSizeAndMainSize(mainSizeFromRatio);
  }
}

uint32_t FlexItem::NumAutoMarginsInAxis(LogicalAxis aAxis) const {
  uint32_t numAutoMargins = 0;
  const auto& styleMargin = mFrame->StyleMargin()->mMargin;
  for (const auto edge : {eLogicalEdgeStart, eLogicalEdgeEnd}) {
    const auto side = MakeLogicalSide(aAxis, edge);
    if (styleMargin.Get(mCBWM, side).IsAuto()) {
      numAutoMargins++;
    }
  }

  // Mostly for clarity:
  MOZ_ASSERT(numAutoMargins <= 2,
             "We're just looking at one item along one dimension, so we "
             "should only have examined 2 margins");

  return numAutoMargins;
}

bool FlexItem::CanMainSizeInfluenceCrossSize() const {
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

  if (HasAspectRatio()) {
    // For flex items that have an aspect ratio (and maintain it, i.e. are
    // not stretched, which we already checked above): changes to main-size
    // *do* influence the cross size.
    return true;
  }

  if (IsInlineAxisCrossAxis()) {
    // If we get here, this function is really asking: "can changes to this
    // item's block size have an influence on its inline size"?  For blocks and
    // tables, the answer is "no".
    if (mFrame->IsBlockFrame() || mFrame->IsTableWrapperFrame()) {
      // XXXdholbert (Maybe use an IsFrameOfType query or something more
      // general to test this across all frame types? For now, I'm just
      // optimizing for block and table, since those are common containers that
      // can contain arbitrarily-large subtrees (and that reliably have ISize
      // being unaffected by BSize, per CSS2).  So optimizing away needless
      // relayout is possible & especially valuable for these containers.)
      return false;
    }
    // Other opt-outs can go here, as they're identified as being useful
    // (particularly for containers where an extra reflow is expensive). But in
    // general, we have to assume that a flexed BSize *could* influence the
    // ISize. Some examples where this can definitely happen:
    // * Intrinsically-sized multicol with fixed-ISize columns, which adds
    // columns (i.e. grows in inline axis) depending on its block size.
    // * Intrinsically-sized multi-line column-oriented flex container, which
    // adds flex lines (i.e. grows in inline axis) depending on its block size.
  }

  // Default assumption, if we haven't proven otherwise: the resolved main size
  // *can* change the cross size.
  return true;
}

nscoord FlexItem::ClampMainSizeViaCrossAxisConstraints(
    nscoord aMainSize, const ReflowInput& aItemReflowInput) const {
  MOZ_ASSERT(HasAspectRatio(), "Caller should've checked the ratio is valid!");

  const LogicalSize contentBoxSizeToBoxSizingAdjust =
      aItemReflowInput.mStylePosition->mBoxSizing == StyleBoxSizing::Border
          ? BorderPadding().Size(mCBWM)
          : LogicalSize(mCBWM);

  const nscoord mainMinSizeFromRatio = mAspectRatio.ComputeRatioDependentSize(
      MainAxis(), mCBWM, CrossMinSize(), contentBoxSizeToBoxSizingAdjust);
  nscoord clampedMainSize = std::max(aMainSize, mainMinSizeFromRatio);

  if (CrossMaxSize() != NS_UNCONSTRAINEDSIZE) {
    const nscoord mainMaxSizeFromRatio = mAspectRatio.ComputeRatioDependentSize(
        MainAxis(), mCBWM, CrossMaxSize(), contentBoxSizeToBoxSizingAdjust);
    clampedMainSize = std::min(clampedMainSize, mainMaxSizeFromRatio);
  }

  return clampedMainSize;
}

/**
 * Returns true if aFrame or any of its children have the
 * NS_FRAME_CONTAINS_RELATIVE_BSIZE flag set -- i.e. if any of these frames (or
 * their descendants) might have a relative-BSize dependency on aFrame (or its
 * ancestors).
 */
static bool FrameHasRelativeBSizeDependency(nsIFrame* aFrame) {
  if (aFrame->HasAnyStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
    return true;
  }
  for (const auto& childList : aFrame->ChildLists()) {
    for (nsIFrame* childFrame : childList.mList) {
      if (childFrame->HasAnyStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
        return true;
      }
    }
  }
  return false;
}

bool FlexItem::NeedsFinalReflow(const nscoord aAvailableBSizeForItem) const {
  MOZ_ASSERT(
      aAvailableBSizeForItem == NS_UNCONSTRAINEDSIZE ||
          aAvailableBSizeForItem > 0,
      "We can only handle unconstrained or positive available block-size.");

  if (!StaticPrefs::layout_flexbox_item_final_reflow_optimization_enabled()) {
    FLEX_LOG(
        "[perf] Flex item %p needed a final reflow due to optimization being "
        "disabled via the preference",
        mFrame);
    return true;
  }

  // NOTE: even if aAvailableBSizeForItem == NS_UNCONSTRAINEDSIZE we can still
  // have continuations from an earlier constrained reflow.
  if (mFrame->GetPrevInFlow() || mFrame->GetNextInFlow()) {
    // This is an item has continuation(s). Reflow it.
    FLEX_LOG("[frag] Flex item %p needed a final reflow due to continuation(s)",
             mFrame);
    return true;
  }

  // Bug 1637091: We can do better and skip this flex item's final reflow if
  // both this flex item's block-size and overflow areas can fit the
  // aAvailableBSizeForItem.
  if (aAvailableBSizeForItem != NS_UNCONSTRAINEDSIZE) {
    FLEX_LOG(
        "[frag] Flex item %p needed both a measuring reflow and a final "
        "reflow due to constrained available block-size",
        mFrame);
    return true;
  }

  // Flex item's final content-box size (in terms of its own writing-mode):
  const LogicalSize finalSize = mIsInlineAxisMainAxis
                                    ? LogicalSize(mWM, mMainSize, mCrossSize)
                                    : LogicalSize(mWM, mCrossSize, mMainSize);

  if (HadMeasuringReflow()) {
    // We've already reflowed this flex item once, to measure it. In that
    // reflow, did its frame happen to end up with the correct final size
    // that the flex container would like it to have?
    if (finalSize != mFrame->ContentSize(mWM)) {
      // The measuring reflow left the item with a different size than its
      // final flexed size. So, we need to reflow to give it the correct size.
      FLEX_LOG(
          "[perf] Flex item %p needed both a measuring reflow and a final "
          "reflow due to measured size disagreeing with final size",
          mFrame);
      return true;
    }

    if (FrameHasRelativeBSizeDependency(mFrame)) {
      // This item has descendants with relative BSizes who may care that its
      // size may now be considered "definite" in the final reflow (whereas it
      // was indefinite during the measuring reflow).
      FLEX_LOG(
          "[perf] Flex item %p needed both a measuring reflow and a final "
          "reflow due to BSize potentially becoming definite",
          mFrame);
      return true;
    }

    // If we get here, then this flex item had a measuring reflow, it left us
    // with the correct size, none of its descendants care that its BSize may
    // now be considered definite, and it can fit into the available block-size.
    // So it doesn't need a final reflow.
    //
    // We now cache this size as if we had done a final reflow (because we've
    // determined that the measuring reflow was effectively equivalent).  This
    // way, in our next time through flex layout, we may be able to skip both
    // the measuring reflow *and* the final reflow (if conditions are the same
    // as they are now).
    if (auto* cache = mFrame->GetProperty(CachedFlexItemData::Prop())) {
      cache->Update(*this, finalSize);
    }

    return false;
  }

  // This item didn't receive a measuring reflow (at least, not during this
  // reflow of our flex container).  We may still be able to skip reflowing it
  // (i.e. return false from this function), if its subtree is clean & its most
  // recent "final reflow" had it at the correct content-box size &
  // definiteness.
  // Let's check for each condition that would still require us to reflow:
  if (mFrame->IsSubtreeDirty()) {
    FLEX_LOG(
        "[perf] Flex item %p needed a final reflow due to its subtree "
        "being dirty",
        mFrame);
    return true;
  }

  // Cool; this item & its subtree haven't experienced any style/content
  // changes that would automatically require a reflow.

  // Did we cache the metrics from its most recent "final reflow"?
  auto* cache = mFrame->GetProperty(CachedFlexItemData::Prop());
  if (!cache || !cache->mFinalReflowMetrics) {
    FLEX_LOG(
        "[perf] Flex item %p needed a final reflow due to lacking a "
        "cached mFinalReflowMetrics (maybe cache was cleared)",
        mFrame);
    return true;
  }

  // Does the cached size match our current size?
  if (cache->mFinalReflowMetrics->Size() != finalSize) {
    FLEX_LOG(
        "[perf] Flex item %p needed a final reflow due to having a "
        "different content box size vs. its most recent final reflow",
        mFrame);
    return true;
  }

  // Does the cached border and padding match our current ones?
  //
  // Note: this is just to detect cases where we have a percent padding whose
  // basis has changed. Any other sort of change to BorderPadding() (e.g. a new
  // specified value) should result in the frame being marked dirty via proper
  // change hint (see nsStylePadding::CalcDifference()), which will force it to
  // reflow.
  if (cache->mFinalReflowMetrics->BorderPadding() !=
      BorderPadding().ConvertTo(mWM, mCBWM)) {
    FLEX_LOG(
        "[perf] Flex item %p needed a final reflow due to having a "
        "different border and padding vs. its most recent final reflow",
        mFrame);
    return true;
  }

  // The flex container is giving this flex item the same size that the item
  // had on its most recent "final reflow". But if its definiteness changed and
  // one of the descendants cares, then it would still need a reflow.
  if (cache->mFinalReflowMetrics->TreatBSizeAsIndefinite() !=
          mTreatBSizeAsIndefinite &&
      FrameHasRelativeBSizeDependency(mFrame)) {
    FLEX_LOG(
        "[perf] Flex item %p needed a final reflow due to having "
        "its BSize change definiteness & having a rel-BSize child",
        mFrame);
    return true;
  }

  // If we get here, we can skip the final reflow! (The item's subtree isn't
  // dirty, and our current conditions are sufficiently similar to the most
  // recent "final reflow" that it should have left our subtree in the correct
  // state.)
  FLEX_LOG("[perf] Flex item %p didn't need a final reflow", mFrame);
  return false;
}

// Keeps track of our position along a particular axis (where a '0' position
// corresponds to the 'start' edge of that axis).
// This class shouldn't be instantiated directly -- rather, it should only be
// instantiated via its subclasses defined below.
class MOZ_STACK_CLASS PositionTracker {
 public:
  // Accessor for the current value of the position that we're tracking.
  inline nscoord Position() const { return mPosition; }
  inline LogicalAxis Axis() const { return mAxis; }

  inline LogicalSide StartSide() {
    return MakeLogicalSide(
        mAxis, mIsAxisReversed ? eLogicalEdgeEnd : eLogicalEdgeStart);
  }

  inline LogicalSide EndSide() {
    return MakeLogicalSide(
        mAxis, mIsAxisReversed ? eLogicalEdgeStart : eLogicalEdgeEnd);
  }

  // Advances our position across the start edge of the given margin, in the
  // axis we're tracking.
  void EnterMargin(const LogicalMargin& aMargin) {
    mPosition += aMargin.Side(StartSide(), mWM);
  }

  // Advances our position across the end edge of the given margin, in the axis
  // we're tracking.
  void ExitMargin(const LogicalMargin& aMargin) {
    mPosition += aMargin.Side(EndSide(), mWM);
  }

  // Advances our current position from the start side of a child frame's
  // border-box to the frame's upper or left edge (depending on our axis).
  // (Note that this is a no-op if our axis grows in the same direction as
  // the corresponding logical axis.)
  void EnterChildFrame(nscoord aChildFrameSize) {
    if (mIsAxisReversed) {
      mPosition += aChildFrameSize;
    }
  }

  // Advances our current position from a frame's upper or left border-box edge
  // (whichever is in the axis we're tracking) to the 'end' side of the frame
  // in the axis that we're tracking. (Note that this is a no-op if our axis
  // is reversed with respect to the corresponding logical axis.)
  void ExitChildFrame(nscoord aChildFrameSize) {
    if (!mIsAxisReversed) {
      mPosition += aChildFrameSize;
    }
  }

  // Delete copy-constructor & reassignment operator, to prevent accidental
  // (unnecessary) copying.
  PositionTracker(const PositionTracker&) = delete;
  PositionTracker& operator=(const PositionTracker&) = delete;

 protected:
  // Protected constructor, to be sure we're only instantiated via a subclass.
  PositionTracker(WritingMode aWM, LogicalAxis aAxis, bool aIsAxisReversed)
      : mWM(aWM), mAxis(aAxis), mIsAxisReversed(aIsAxisReversed) {}

  // Member data:
  // The position we're tracking.
  nscoord mPosition = 0;

  // The flex container's writing mode.
  const WritingMode mWM;

  // The axis along which we're moving.
  const LogicalAxis mAxis = eLogicalAxisInline;

  // Is the axis along which we're moving reversed (e.g. LTR vs RTL) with
  // respect to the corresponding axis on the flex container's WM?
  const bool mIsAxisReversed = false;
};

// Tracks our position in the main axis, when we're laying out flex items.
// The "0" position represents the main-start edge of the flex container's
// content-box.
class MOZ_STACK_CLASS MainAxisPositionTracker : public PositionTracker {
 public:
  MainAxisPositionTracker(const FlexboxAxisTracker& aAxisTracker,
                          const FlexLine* aLine,
                          const StyleContentDistribution& aJustifyContent,
                          nscoord aContentBoxMainSize);

  ~MainAxisPositionTracker() {
    MOZ_ASSERT(mNumPackingSpacesRemaining == 0,
               "miscounted the number of packing spaces");
    MOZ_ASSERT(mNumAutoMarginsInMainAxis == 0,
               "miscounted the number of auto margins");
  }

  // Advances past the gap space (if any) between two flex items
  void TraverseGap(nscoord aGapSize) { mPosition += aGapSize; }

  // Advances past the packing space (if any) between two flex items
  void TraversePackingSpace();

  // If aItem has any 'auto' margins in the main axis, this method updates the
  // corresponding values in its margin.
  void ResolveAutoMarginsInMainAxis(FlexItem& aItem);

 private:
  nscoord mPackingSpaceRemaining = 0;
  uint32_t mNumAutoMarginsInMainAxis = 0;
  uint32_t mNumPackingSpacesRemaining = 0;
  StyleContentDistribution mJustifyContent = {StyleAlignFlags::AUTO};
};

// Utility class for managing our position along the cross axis along
// the whole flex container (at a higher level than a single line).
// The "0" position represents the cross-start edge of the flex container's
// content-box.
class MOZ_STACK_CLASS CrossAxisPositionTracker : public PositionTracker {
 public:
  CrossAxisPositionTracker(nsTArray<FlexLine>& aLines,
                           const ReflowInput& aReflowInput,
                           nscoord aContentBoxCrossSize,
                           bool aIsCrossSizeDefinite,
                           const FlexboxAxisTracker& aAxisTracker,
                           const nscoord aCrossGapSize);

  // Advances past the gap (if any) between two flex lines
  void TraverseGap() { mPosition += mCrossGapSize; }

  // Advances past the packing space (if any) between two flex lines
  void TraversePackingSpace();

  // Advances past the given FlexLine
  void TraverseLine(FlexLine& aLine) { mPosition += aLine.LineCrossSize(); }

  // Redeclare the frame-related methods from PositionTracker with
  // = delete, to be sure (at compile time) that no client code can invoke
  // them. (Unlike the other PositionTracker derived classes, this class here
  // deals with FlexLines, not with individual FlexItems or frames.)
  void EnterMargin(const LogicalMargin& aMargin) = delete;
  void ExitMargin(const LogicalMargin& aMargin) = delete;
  void EnterChildFrame(nscoord aChildFrameSize) = delete;
  void ExitChildFrame(nscoord aChildFrameSize) = delete;

 private:
  nscoord mPackingSpaceRemaining = 0;
  uint32_t mNumPackingSpacesRemaining = 0;
  StyleContentDistribution mAlignContent = {StyleAlignFlags::AUTO};

  const nscoord mCrossGapSize;
};

// Utility class for managing our position along the cross axis, *within* a
// single flex line.
class MOZ_STACK_CLASS SingleLineCrossAxisPositionTracker
    : public PositionTracker {
 public:
  explicit SingleLineCrossAxisPositionTracker(
      const FlexboxAxisTracker& aAxisTracker);

  void ResolveAutoMarginsInCrossAxis(const FlexLine& aLine, FlexItem& aItem);

  void EnterAlignPackingSpace(const FlexLine& aLine, const FlexItem& aItem,
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

// Suppose D is the distance from a flex container fragment's content-box
// block-start edge to whichever is larger of either (a) the block-end edge of
// its children, or (b) the available space's block-end edge. D is conceptually
// the sum of the block-size of the children, the packing space before & in
// between them, and part of the packing space after them if (b) happens.
//
// SumOfBlockEndEdgeOfChildrenProperty stores the sum of the D of the current
// flex container fragment and the D's of its prev-in-flows from the last
// reflow. It's intended to prevent quadratic operations resulting from each
// fragment having to walk its full prev-in-flow chain, and also serves as an
// argument to the flex fragmentainer next-in-flow's ReflowChildren(), to
// compute the position offset for each flex item.
NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(SumOfChildrenBlockSizeProperty, nscoord)

nsContainerFrame* NS_NewFlexContainerFrame(PresShell* aPresShell,
                                           ComputedStyle* aStyle) {
  return new (aPresShell)
      nsFlexContainerFrame(aStyle, aPresShell->GetPresContext());
}

//----------------------------------------------------------------------

// nsFlexContainerFrame Method Implementations
// ===========================================

/* virtual */
nsFlexContainerFrame::~nsFlexContainerFrame() = default;

/* virtual */
void nsFlexContainerFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                                nsIFrame* aPrevInFlow) {
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  if (HasAnyStateBits(NS_FRAME_FONT_INFLATION_CONTAINER)) {
    AddStateBits(NS_FRAME_FONT_INFLATION_FLOW_ROOT);
  }

  const nsStyleDisplay* styleDisp = Style()->StyleDisplay();

  // Figure out if we should set a frame state bit to indicate that this frame
  // represents a legacy -webkit-{inline-}box or -moz-{inline-}box container.
  // First, the trivial case: just check "display" directly.
  bool isLegacyBox = IsDisplayValueLegacyBox(styleDisp);

  // If this frame is for a scrollable element, then it will actually have
  // "display:block", and its *parent frame* will have the real
  // flex-flavored display value. So in that case, check the parent frame to
  // find out if we're legacy.
  if (!isLegacyBox && styleDisp->mDisplay == mozilla::StyleDisplay::Block) {
    ComputedStyle* parentComputedStyle = GetParent()->Style();
    NS_ASSERTION(
        Style()->GetPseudoType() == PseudoStyleType::buttonContent ||
            Style()->GetPseudoType() == PseudoStyleType::scrolledContent,
        "The only way a nsFlexContainerFrame can have 'display:block' "
        "should be if it's the inner part of a scrollable or button "
        "element");
    isLegacyBox = IsDisplayValueLegacyBox(parentComputedStyle->StyleDisplay());
  }

  if (isLegacyBox) {
    AddStateBits(NS_STATE_FLEX_IS_EMULATING_LEGACY_BOX);
  }
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsFlexContainerFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"FlexContainer"_ns, aResult);
}
#endif

nscoord nsFlexContainerFrame::GetLogicalBaseline(
    mozilla::WritingMode aWM) const {
  NS_ASSERTION(mBaselineFromLastReflow != NS_INTRINSIC_ISIZE_UNKNOWN,
               "baseline has not been set");

  if (HasAnyStateBits(NS_STATE_FLEX_SYNTHESIZE_BASELINE)) {
    // Return a baseline synthesized from our margin-box.
    return nsContainerFrame::GetLogicalBaseline(aWM);
  }
  return mBaselineFromLastReflow;
}

void nsFlexContainerFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                            const nsDisplayListSet& aLists) {
  nsDisplayListCollection tempLists(aBuilder);

  DisplayBorderBackgroundOutline(aBuilder, tempLists);
  if (GetPrevInFlow()) {
    DisplayOverflowContainers(aBuilder, tempLists);
  }

  // Our children are all block-level, so their borders/backgrounds all go on
  // the BlockBorderBackgrounds list.
  nsDisplayListSet childLists(tempLists, tempLists.BlockBorderBackgrounds());

  CSSOrderAwareFrameIterator iter(
      this, kPrincipalList, CSSOrderAwareFrameIterator::ChildFilter::IncludeAll,
      OrderStateForIter(this), OrderingPropertyForIter(this));

  for (; !iter.AtEnd(); iter.Next()) {
    nsIFrame* childFrame = *iter;
    BuildDisplayListForChild(aBuilder, childFrame, childLists,
                             childFrame->DisplayFlagForFlexOrGridItem());
  }

  tempLists.MoveTo(aLists);
}

void FlexLine::FreezeItemsEarly(bool aIsUsingFlexGrow,
                                ComputedFlexLineInfo* aLineInfo) {
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
  //  https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths
  //
  // (NOTE: At this point, item->MainSize() *is* the item's hypothetical
  // main size, since SetFlexBaseSizeAndMainSize() sets it up that way, and the
  // item hasn't had a chance to flex away from that yet.)

  // Since this loop only operates on unfrozen flex items, we can break as
  // soon as we have seen all of them.
  uint32_t numUnfrozenItemsToBeSeen = NumItems() - mNumFrozenItems;
  for (FlexItem& item : Items()) {
    if (numUnfrozenItemsToBeSeen == 0) {
      break;
    }

    if (!item.IsFrozen()) {
      numUnfrozenItemsToBeSeen--;
      bool shouldFreeze = (0.0f == item.GetFlexFactor(aIsUsingFlexGrow));
      if (!shouldFreeze) {
        if (aIsUsingFlexGrow) {
          if (item.FlexBaseSize() > item.MainSize()) {
            shouldFreeze = true;
          }
        } else {  // using flex-shrink
          if (item.FlexBaseSize() < item.MainSize()) {
            shouldFreeze = true;
          }
        }
      }
      if (shouldFreeze) {
        // Freeze item! (at its hypothetical main size)
        item.Freeze();
        if (item.FlexBaseSize() < item.MainSize()) {
          item.SetWasMinClamped();
        } else if (item.FlexBaseSize() > item.MainSize()) {
          item.SetWasMaxClamped();
        }
        mNumFrozenItems++;
      }
    }
  }

  MOZ_ASSERT(numUnfrozenItemsToBeSeen == 0, "miscounted frozen items?");
}

// Based on the sign of aTotalViolation, this function freezes a subset of our
// flexible sizes, and restores the remaining ones to their initial pref sizes.
void FlexLine::FreezeOrRestoreEachFlexibleSize(const nscoord aTotalViolation,
                                               bool aIsFinalIteration) {
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
  } else {  // aTotalViolation < 0
    freezeType = eFreezeMaxViolations;
  }

  // Since this loop only operates on unfrozen flex items, we can break as
  // soon as we have seen all of them.
  uint32_t numUnfrozenItemsToBeSeen = NumItems() - mNumFrozenItems;
  for (FlexItem& item : Items()) {
    if (numUnfrozenItemsToBeSeen == 0) {
      break;
    }

    if (!item.IsFrozen()) {
      numUnfrozenItemsToBeSeen--;

      MOZ_ASSERT(!item.HadMinViolation() || !item.HadMaxViolation(),
                 "Can have either min or max violation, but not both");

      bool hadMinViolation = item.HadMinViolation();
      bool hadMaxViolation = item.HadMaxViolation();
      if (eFreezeEverything == freezeType ||
          (eFreezeMinViolations == freezeType && hadMinViolation) ||
          (eFreezeMaxViolations == freezeType && hadMaxViolation)) {
        MOZ_ASSERT(item.MainSize() >= item.MainMinSize(),
                   "Freezing item at a size below its minimum");
        MOZ_ASSERT(item.MainSize() <= item.MainMaxSize(),
                   "Freezing item at a size above its maximum");

        item.Freeze();
        if (hadMinViolation) {
          item.SetWasMinClamped();
        } else if (hadMaxViolation) {
          item.SetWasMaxClamped();
        }
        mNumFrozenItems++;
      } else if (MOZ_UNLIKELY(aIsFinalIteration)) {
        // XXXdholbert If & when bug 765861 is fixed, we should upgrade this
        // assertion to be fatal except in documents with enormous lengths.
        NS_ERROR(
            "Final iteration still has unfrozen items, this shouldn't"
            " happen unless there was nscoord under/overflow.");
        item.Freeze();
        mNumFrozenItems++;
      }  // else, we'll reset this item's main size to its flex base size on the
         // next iteration of this algorithm.

      if (!item.IsFrozen()) {
        // Clear this item's violation(s), now that we've dealt with them
        item.ClearViolationFlags();
      }
    }
  }

  MOZ_ASSERT(numUnfrozenItemsToBeSeen == 0, "miscounted frozen items?");
}

void FlexLine::ResolveFlexibleLengths(nscoord aFlexContainerMainSize,
                                      ComputedFlexLineInfo* aLineInfo) {
  // In this function, we use 64-bit coord type to avoid integer overflow in
  // case several of the individual items have huge hypothetical main sizes,
  // which can happen with percent-width table-layout:fixed descendants. Here we
  // promote the container's main size to 64-bit to make the arithmetic
  // convenient.
  AuCoord64 flexContainerMainSize(aFlexContainerMainSize);

  // Before we start resolving sizes: if we have an aLineInfo structure to fill
  // out, we inform it of each item's base size, and we initialize the "delta"
  // for each item to 0. (And if the flex algorithm wants to grow or shrink the
  // item, we'll update this delta further down.)
  if (aLineInfo) {
    uint32_t itemIndex = 0;
    for (FlexItem& item : Items()) {
      aLineInfo->mItems[itemIndex].mMainBaseSize = item.FlexBaseSize();
      aLineInfo->mItems[itemIndex].mMainDeltaSize = 0;
      ++itemIndex;
    }
  }

  // Determine whether we're going to be growing or shrinking items.
  const bool isUsingFlexGrow =
      (mTotalOuterHypotheticalMainSize < flexContainerMainSize);

  if (aLineInfo) {
    aLineInfo->mGrowthState =
        isUsingFlexGrow ? mozilla::dom::FlexLineGrowthState::Growing
                        : mozilla::dom::FlexLineGrowthState::Shrinking;
  }

  // Do an "early freeze" for flex items that obviously can't flex in the
  // direction we've chosen:
  FreezeItemsEarly(isUsingFlexGrow, aLineInfo);

  if ((mNumFrozenItems == NumItems()) && !aLineInfo) {
    // All our items are frozen, so we have no flexible lengths to resolve,
    // and we aren't being asked to generate computed line info.
    FLEX_LOG("No flexible length to resolve");
    return;
  }
  MOZ_ASSERT(!IsEmpty() || aLineInfo,
             "empty lines should take the early-return above");

  FLEX_LOG("Resolving flexible lengths for items");

  // Subtract space occupied by our items' margins/borders/padding/gaps, so
  // we can just be dealing with the space available for our flex items' content
  // boxes.
  const AuCoord64 totalItemMBPAndGaps = mTotalItemMBP + SumOfGaps();
  const AuCoord64 spaceAvailableForFlexItemsContentBoxes =
      flexContainerMainSize - totalItemMBPAndGaps;

  Maybe<AuCoord64> origAvailableFreeSpace;

  // NOTE: I claim that this chunk of the algorithm (the looping part) needs to
  // run the loop at MOST NumItems() times.  This claim should hold up
  // because we'll freeze at least one item on each loop iteration, and once
  // we've run out of items to freeze, there's nothing left to do.  However,
  // in most cases, we'll break out of this loop long before we hit that many
  // iterations.
  for (uint32_t iterationCounter = 0; iterationCounter < NumItems();
       iterationCounter++) {
    // Set every not-yet-frozen item's used main size to its
    // flex base size, and subtract all the used main sizes from our
    // total amount of space to determine the 'available free space'
    // (positive or negative) to be distributed among our flexible items.
    AuCoord64 availableFreeSpace = spaceAvailableForFlexItemsContentBoxes;
    for (FlexItem& item : Items()) {
      if (!item.IsFrozen()) {
        item.SetMainSize(item.FlexBaseSize());
      }
      availableFreeSpace -= item.MainSize();
    }

    FLEX_LOG(" available free space: %" PRId64 "; flex items should \"%s\"",
             availableFreeSpace.value, isUsingFlexGrow ? "grow" : "shrink");

    // The sign of our free space should agree with the type of flexing
    // (grow/shrink) that we're doing. Any disagreement should've made us use
    // the other type of flexing, or should've been resolved in
    // FreezeItemsEarly.
    //
    // Note: it's possible that an individual flex item has huge
    // margin/border/padding that makes either its
    // MarginBorderPaddingSizeInMainAxis() or OuterMainSize() negative due to
    // integer overflow. If that happens, the accumulated
    // mTotalOuterHypotheticalMainSize or mTotalItemMBP could be negative due to
    // that one item's negative (overflowed) size. Likewise, a huge main gap
    // size between flex items can also make our accumulated SumOfGaps()
    // negative. In these case, we throw up our hands and don't require
    // isUsingFlexGrow to agree with availableFreeSpace. Luckily, we won't get
    // stuck in the algorithm below, and just distribute the wrong
    // availableFreeSpace with the wrong grow/shrink factors.
    MOZ_ASSERT(!(mTotalOuterHypotheticalMainSize >= 0 && mTotalItemMBP >= 0 &&
                 totalItemMBPAndGaps >= 0) ||
                   (isUsingFlexGrow && availableFreeSpace >= 0) ||
                   (!isUsingFlexGrow && availableFreeSpace <= 0),
               "availableFreeSpace's sign should match isUsingFlexGrow");

    // If we have any free space available, give each flexible item a portion
    // of availableFreeSpace.
    if (availableFreeSpace != AuCoord64(0)) {
      // The first time we do this, we initialize origAvailableFreeSpace.
      if (!origAvailableFreeSpace) {
        origAvailableFreeSpace.emplace(availableFreeSpace);
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
      // maximum representable double (overflowing to infinity), then we can't
      // sensibly divide out proportional shares anymore. In that case, we
      // simply treat the flex item(s) with the largest weights as if
      // their weights were infinite (dwarfing all the others), and we
      // distribute all of the available space among them.
      double weightSum = 0.0;
      double flexFactorSum = 0.0;
      double largestWeight = 0.0;
      uint32_t numItemsWithLargestWeight = 0;

      // Since this loop only operates on unfrozen flex items, we can break as
      // soon as we have seen all of them.
      uint32_t numUnfrozenItemsToBeSeen = NumItems() - mNumFrozenItems;
      for (FlexItem& item : Items()) {
        if (numUnfrozenItemsToBeSeen == 0) {
          break;
        }

        if (!item.IsFrozen()) {
          numUnfrozenItemsToBeSeen--;

          const double curWeight = item.GetWeight(isUsingFlexGrow);
          const double curFlexFactor = item.GetFlexFactor(isUsingFlexGrow);
          MOZ_ASSERT(curWeight >= 0.0, "weights are non-negative");
          MOZ_ASSERT(curFlexFactor >= 0.0, "flex factors are non-negative");

          weightSum += curWeight;
          flexFactorSum += curFlexFactor;

          if (IsFinite(weightSum)) {
            if (curWeight == 0.0) {
              item.SetShareOfWeightSoFar(0.0);
            } else {
              item.SetShareOfWeightSoFar(curWeight / weightSum);
            }
          }  // else, the sum of weights overflows to infinity, in which
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

      MOZ_ASSERT(numUnfrozenItemsToBeSeen == 0, "miscounted frozen items?");

      if (weightSum != 0.0) {
        MOZ_ASSERT(flexFactorSum != 0.0,
                   "flex factor sum can't be 0, if a weighted sum "
                   "of its components (weightSum) is nonzero");
        if (flexFactorSum < 1.0) {
          // Our unfrozen flex items don't want all of the original free space!
          // (Their flex factors add up to something less than 1.)
          // Hence, make sure we don't distribute any more than the portion of
          // our original free space that these items actually want.
          auto totalDesiredPortionOfOrigFreeSpace =
              AuCoord64::FromRound(*origAvailableFreeSpace * flexFactorSum);

          // Clamp availableFreeSpace to be no larger than that ^^.
          // (using min or max, depending on sign).
          // This should not change the sign of availableFreeSpace (except
          // possibly by setting it to 0), as enforced by this assertion:
          NS_ASSERTION(totalDesiredPortionOfOrigFreeSpace == AuCoord64(0) ||
                           ((totalDesiredPortionOfOrigFreeSpace > 0) ==
                            (availableFreeSpace > 0)),
                       "When we reduce available free space for flex "
                       "factors < 1, we shouldn't change the sign of the "
                       "free space...");

          if (availableFreeSpace > 0) {
            availableFreeSpace = std::min(availableFreeSpace,
                                          totalDesiredPortionOfOrigFreeSpace);
          } else {
            availableFreeSpace = std::max(availableFreeSpace,
                                          totalDesiredPortionOfOrigFreeSpace);
          }
        }

        FLEX_LOG(" Distributing available space:");
        // Since this loop only operates on unfrozen flex items, we can break as
        // soon as we have seen all of them.
        numUnfrozenItemsToBeSeen = NumItems() - mNumFrozenItems;

        // NOTE: It's important that we traverse our items in *reverse* order
        // here, for correct width distribution according to the items'
        // "ShareOfWeightSoFar" progressively-calculated values.
        for (FlexItem& item : Reversed(Items())) {
          if (numUnfrozenItemsToBeSeen == 0) {
            break;
          }

          if (!item.IsFrozen()) {
            numUnfrozenItemsToBeSeen--;

            // To avoid rounding issues, we compute the change in size for this
            // item, and then subtract it from the remaining available space.
            AuCoord64 sizeDelta = 0;
            if (IsFinite(weightSum)) {
              double myShareOfRemainingSpace = item.ShareOfWeightSoFar();

              MOZ_ASSERT(myShareOfRemainingSpace >= 0.0 &&
                             myShareOfRemainingSpace <= 1.0,
                         "my share should be nonnegative fractional amount");

              if (myShareOfRemainingSpace == 1.0) {
                // (We special-case 1.0 to avoid float error from converting
                // availableFreeSpace from integer*1.0 --> double --> integer)
                sizeDelta = availableFreeSpace;
              } else if (myShareOfRemainingSpace > 0.0) {
                sizeDelta = AuCoord64::FromRound(availableFreeSpace *
                                                 myShareOfRemainingSpace);
              }
            } else if (item.GetWeight(isUsingFlexGrow) == largestWeight) {
              // Total flexibility is infinite, so we're just distributing
              // the available space equally among the items that are tied for
              // having the largest weight (and this is one of those items).
              sizeDelta = AuCoord64::FromRound(
                  availableFreeSpace / double(numItemsWithLargestWeight));
              numItemsWithLargestWeight--;
            }

            availableFreeSpace -= sizeDelta;

            item.SetMainSize(item.MainSize() +
                             nscoord(sizeDelta.ToMinMaxClamped()));
            FLEX_LOG("  flex item %p receives %" PRId64 ", for a total of %d",
                     item.Frame(), sizeDelta.value, item.MainSize());
          }
        }

        MOZ_ASSERT(numUnfrozenItemsToBeSeen == 0, "miscounted frozen items?");

        // If we have an aLineInfo structure to fill out, capture any
        // size changes that may have occurred in the previous loop.
        // We don't do this inside the previous loop, because we don't
        // want to burden layout when aLineInfo is null.
        if (aLineInfo) {
          uint32_t itemIndex = 0;
          for (FlexItem& item : Items()) {
            if (!item.IsFrozen()) {
              // Calculate a deltaSize that represents how much the flex sizing
              // algorithm "wants" to stretch or shrink this item during this
              // pass through the algorithm. Later passes through the algorithm
              // may overwrite this, until this item is frozen. Note that this
              // value may not reflect how much the size of the item is
              // actually changed, since the size of the item will be clamped
              // to min and max values later in this pass. That's intentional,
              // since we want to report the value that the sizing algorithm
              // tried to stretch or shrink the item.
              nscoord deltaSize =
                  item.MainSize() - aLineInfo->mItems[itemIndex].mMainBaseSize;

              aLineInfo->mItems[itemIndex].mMainDeltaSize = deltaSize;
            }
            ++itemIndex;
          }
        }
      }
    }

    // Fix min/max violations:
    nscoord totalViolation = 0;  // keeps track of adjustments for min/max
    FLEX_LOG(" Checking for violations:");

    // Since this loop only operates on unfrozen flex items, we can break as
    // soon as we have seen all of them.
    uint32_t numUnfrozenItemsToBeSeen = NumItems() - mNumFrozenItems;
    for (FlexItem& item : Items()) {
      if (numUnfrozenItemsToBeSeen == 0) {
        break;
      }

      if (!item.IsFrozen()) {
        numUnfrozenItemsToBeSeen--;

        if (item.MainSize() < item.MainMinSize()) {
          // min violation
          totalViolation += item.MainMinSize() - item.MainSize();
          item.SetMainSize(item.MainMinSize());
          item.SetHadMinViolation();
        } else if (item.MainSize() > item.MainMaxSize()) {
          // max violation
          totalViolation += item.MainMaxSize() - item.MainSize();
          item.SetMainSize(item.MainMaxSize());
          item.SetHadMaxViolation();
        }
      }
    }

    MOZ_ASSERT(numUnfrozenItemsToBeSeen == 0, "miscounted frozen items?");

    FreezeOrRestoreEachFlexibleSize(totalViolation,
                                    iterationCounter + 1 == NumItems());

    FLEX_LOG(" Total violation: %d", totalViolation);

    if (mNumFrozenItems == NumItems()) {
      break;
    }

    MOZ_ASSERT(totalViolation != 0,
               "Zero violation should've made us freeze all items & break");
  }

#ifdef DEBUG
  // Post-condition: all items should've been frozen.
  // Make sure the counts match:
  MOZ_ASSERT(mNumFrozenItems == NumItems(), "All items should be frozen");

  // For good measure, check each item directly, in case our counts are busted:
  for (const FlexItem& item : Items()) {
    MOZ_ASSERT(item.IsFrozen(), "All items should be frozen");
  }
#endif  // DEBUG
}

MainAxisPositionTracker::MainAxisPositionTracker(
    const FlexboxAxisTracker& aAxisTracker, const FlexLine* aLine,
    const StyleContentDistribution& aJustifyContent,
    nscoord aContentBoxMainSize)
    : PositionTracker(aAxisTracker.GetWritingMode(), aAxisTracker.MainAxis(),
                      aAxisTracker.IsMainAxisReversed()),
      // we chip away at this below
      mPackingSpaceRemaining(aContentBoxMainSize),
      mJustifyContent(aJustifyContent) {
  // Extract the flag portion of mJustifyContent and strip off the flag bits
  // NOTE: This must happen before any assignment to mJustifyContent to
  // avoid overwriting the flag bits.
  StyleAlignFlags justifyContentFlags =
      mJustifyContent.primary & StyleAlignFlags::FLAG_BITS;
  mJustifyContent.primary &= ~StyleAlignFlags::FLAG_BITS;

  // 'normal' behaves as 'stretch', and 'stretch' behaves as 'flex-start',
  // in the main axis
  // https://drafts.csswg.org/css-align-3/#propdef-justify-content
  if (mJustifyContent.primary == StyleAlignFlags::NORMAL ||
      mJustifyContent.primary == StyleAlignFlags::STRETCH) {
    mJustifyContent.primary = StyleAlignFlags::FLEX_START;
  }

  // mPackingSpaceRemaining is initialized to the container's main size.  Now
  // we'll subtract out the main sizes of our flex items, so that it ends up
  // with the *actual* amount of packing space.
  for (const FlexItem& item : aLine->Items()) {
    mPackingSpaceRemaining -= item.OuterMainSize();
    mNumAutoMarginsInMainAxis += item.NumAutoMarginsInMainAxis();
  }

  // Subtract space required for row/col gap from the remaining packing space
  mPackingSpaceRemaining -= aLine->SumOfGaps();

  if (mPackingSpaceRemaining <= 0) {
    // No available packing space to use for resolving auto margins.
    mNumAutoMarginsInMainAxis = 0;
    // If packing space is negative and <overflow-position> is set to 'safe'
    // all justify options fall back to 'start'
    if (justifyContentFlags & StyleAlignFlags::SAFE) {
      mJustifyContent.primary = StyleAlignFlags::START;
    }
  }

  // If packing space is negative or we only have one item, 'space-between'
  // falls back to 'flex-start', and 'space-around' & 'space-evenly' fall back
  // to 'center'. In those cases, it's simplest to just pretend we have a
  // different 'justify-content' value and share code.
  if (mPackingSpaceRemaining < 0 || aLine->NumItems() == 1) {
    if (mJustifyContent.primary == StyleAlignFlags::SPACE_BETWEEN) {
      mJustifyContent.primary = StyleAlignFlags::FLEX_START;
    } else if (mJustifyContent.primary == StyleAlignFlags::SPACE_AROUND ||
               mJustifyContent.primary == StyleAlignFlags::SPACE_EVENLY) {
      mJustifyContent.primary = StyleAlignFlags::CENTER;
    }
  }

  // Map 'left'/'right' to 'start'/'end'
  if (mJustifyContent.primary == StyleAlignFlags::LEFT ||
      mJustifyContent.primary == StyleAlignFlags::RIGHT) {
    mJustifyContent.primary =
        aAxisTracker.ResolveJustifyLeftRight(mJustifyContent.primary);
  }

  // Map 'start'/'end' to 'flex-start'/'flex-end'.
  if (mJustifyContent.primary == StyleAlignFlags::START) {
    mJustifyContent.primary = aAxisTracker.IsMainAxisReversed()
                                  ? StyleAlignFlags::FLEX_END
                                  : StyleAlignFlags::FLEX_START;
  } else if (mJustifyContent.primary == StyleAlignFlags::END) {
    mJustifyContent.primary = aAxisTracker.IsMainAxisReversed()
                                  ? StyleAlignFlags::FLEX_START
                                  : StyleAlignFlags::FLEX_END;
  }

  // Figure out how much space we'll set aside for auto margins or
  // packing spaces, and advance past any leading packing-space.
  if (mNumAutoMarginsInMainAxis == 0 && mPackingSpaceRemaining != 0 &&
      !aLine->IsEmpty()) {
    if (mJustifyContent.primary == StyleAlignFlags::FLEX_START) {
      // All packing space should go at the end --> nothing to do here.
    } else if (mJustifyContent.primary == StyleAlignFlags::FLEX_END) {
      // All packing space goes at the beginning
      mPosition += mPackingSpaceRemaining;
    } else if (mJustifyContent.primary == StyleAlignFlags::CENTER) {
      // Half the packing space goes at the beginning
      mPosition += mPackingSpaceRemaining / 2;
    } else if (mJustifyContent.primary == StyleAlignFlags::SPACE_BETWEEN ||
               mJustifyContent.primary == StyleAlignFlags::SPACE_AROUND ||
               mJustifyContent.primary == StyleAlignFlags::SPACE_EVENLY) {
      nsFlexContainerFrame::CalculatePackingSpace(
          aLine->NumItems(), mJustifyContent, &mPosition,
          &mNumPackingSpacesRemaining, &mPackingSpaceRemaining);
    } else {
      MOZ_ASSERT_UNREACHABLE("Unexpected justify-content value");
    }
  }

  MOZ_ASSERT(mNumPackingSpacesRemaining == 0 || mNumAutoMarginsInMainAxis == 0,
             "extra space should either go to packing space or to "
             "auto margins, but not to both");
}

void MainAxisPositionTracker::ResolveAutoMarginsInMainAxis(FlexItem& aItem) {
  if (mNumAutoMarginsInMainAxis) {
    const auto& styleMargin = aItem.Frame()->StyleMargin()->mMargin;
    for (const auto side : {StartSide(), EndSide()}) {
      if (styleMargin.Get(mWM, side).IsAuto()) {
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

void MainAxisPositionTracker::TraversePackingSpace() {
  if (mNumPackingSpacesRemaining) {
    MOZ_ASSERT(mJustifyContent.primary == StyleAlignFlags::SPACE_BETWEEN ||
                   mJustifyContent.primary == StyleAlignFlags::SPACE_AROUND ||
                   mJustifyContent.primary == StyleAlignFlags::SPACE_EVENLY,
               "mNumPackingSpacesRemaining only applies for "
               "space-between/space-around/space-evenly");

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

CrossAxisPositionTracker::CrossAxisPositionTracker(
    nsTArray<FlexLine>& aLines, const ReflowInput& aReflowInput,
    nscoord aContentBoxCrossSize, bool aIsCrossSizeDefinite,
    const FlexboxAxisTracker& aAxisTracker, const nscoord aCrossGapSize)
    : PositionTracker(aAxisTracker.GetWritingMode(), aAxisTracker.CrossAxis(),
                      aAxisTracker.IsCrossAxisReversed()),
      mAlignContent(aReflowInput.mStylePosition->mAlignContent),
      mCrossGapSize(aCrossGapSize) {
  // Extract and strip the flag bits from alignContent
  StyleAlignFlags alignContentFlags =
      mAlignContent.primary & StyleAlignFlags::FLAG_BITS;
  mAlignContent.primary &= ~StyleAlignFlags::FLAG_BITS;

  // 'normal' behaves as 'stretch'
  if (mAlignContent.primary == StyleAlignFlags::NORMAL) {
    mAlignContent.primary = StyleAlignFlags::STRETCH;
  }

  const bool isSingleLine =
      StyleFlexWrap::Nowrap == aReflowInput.mStylePosition->mFlexWrap;
  if (isSingleLine) {
    MOZ_ASSERT(aLines.Length() == 1,
               "If we're styled as single-line, we should only have 1 line");
    // "If the flex container is single-line and has a definite cross size, the
    // cross size of the flex line is the flex container's inner cross size."
    //
    // SOURCE: https://drafts.csswg.org/css-flexbox/#algo-cross-line
    // NOTE: This means (by definition) that there's no packing space, which
    // means we don't need to be concerned with "align-content" at all and we
    // can return early. This is handy, because this is the usual case (for
    // single-line flexbox).
    if (aIsCrossSizeDefinite) {
      aLines[0].SetLineCrossSize(aContentBoxCrossSize);
      return;
    }

    // "If the flex container is single-line, then clamp the line's
    // cross-size to be within the container's computed min and max cross-size
    // properties."
    aLines[0].SetLineCrossSize(NS_CSS_MINMAX(aLines[0].LineCrossSize(),
                                             aReflowInput.ComputedMinBSize(),
                                             aReflowInput.ComputedMaxBSize()));
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
  for (FlexLine& line : aLines) {
    mPackingSpaceRemaining -= line.LineCrossSize();
    numLines++;
  }

  // Subtract space required for row/col gap from the remaining packing space
  MOZ_ASSERT(numLines >= 1,
             "GenerateFlexLines should've produced at least 1 line");
  mPackingSpaceRemaining -= aCrossGapSize * (numLines - 1);

  // If <overflow-position> is 'safe' and packing space is negative
  // all align options fall back to 'start'
  if ((alignContentFlags & StyleAlignFlags::SAFE) &&
      mPackingSpaceRemaining < 0) {
    mAlignContent.primary = StyleAlignFlags::START;
  }

  // If packing space is negative, 'space-between' and 'stretch' behave like
  // 'flex-start', and 'space-around' and 'space-evenly' behave like 'center'.
  // In those cases, it's simplest to just pretend we have a different
  // 'align-content' value and share code. (If we only have one line, all of
  // the 'space-*' keywords fall back as well, but 'stretch' doesn't because
  // even a single line can still stretch.)
  if (mPackingSpaceRemaining < 0 &&
      mAlignContent.primary == StyleAlignFlags::STRETCH) {
    mAlignContent.primary = StyleAlignFlags::FLEX_START;
  } else if (mPackingSpaceRemaining < 0 || numLines == 1) {
    if (mAlignContent.primary == StyleAlignFlags::SPACE_BETWEEN) {
      mAlignContent.primary = StyleAlignFlags::FLEX_START;
    } else if (mAlignContent.primary == StyleAlignFlags::SPACE_AROUND ||
               mAlignContent.primary == StyleAlignFlags::SPACE_EVENLY) {
      mAlignContent.primary = StyleAlignFlags::CENTER;
    }
  }

  // Map 'start'/'end' to 'flex-start'/'flex-end'.
  if (mAlignContent.primary == StyleAlignFlags::START) {
    mAlignContent.primary = aAxisTracker.IsCrossAxisReversed()
                                ? StyleAlignFlags::FLEX_END
                                : StyleAlignFlags::FLEX_START;
  } else if (mAlignContent.primary == StyleAlignFlags::END) {
    mAlignContent.primary = aAxisTracker.IsCrossAxisReversed()
                                ? StyleAlignFlags::FLEX_START
                                : StyleAlignFlags::FLEX_END;
  }

  // Figure out how much space we'll set aside for packing spaces, and advance
  // past any leading packing-space.
  if (mPackingSpaceRemaining != 0) {
    if (mAlignContent.primary == StyleAlignFlags::BASELINE ||
        mAlignContent.primary == StyleAlignFlags::LAST_BASELINE) {
      NS_WARNING(
          "NYI: "
          "align-items/align-self:left/right/self-start/self-end/baseline/"
          "last baseline");
    } else if (mAlignContent.primary == StyleAlignFlags::FLEX_START) {
      // All packing space should go at the end --> nothing to do here.
    } else if (mAlignContent.primary == StyleAlignFlags::FLEX_END) {
      // All packing space goes at the beginning
      mPosition += mPackingSpaceRemaining;
    } else if (mAlignContent.primary == StyleAlignFlags::CENTER) {
      // Half the packing space goes at the beginning
      mPosition += mPackingSpaceRemaining / 2;
    } else if (mAlignContent.primary == StyleAlignFlags::SPACE_BETWEEN ||
               mAlignContent.primary == StyleAlignFlags::SPACE_AROUND ||
               mAlignContent.primary == StyleAlignFlags::SPACE_EVENLY) {
      nsFlexContainerFrame::CalculatePackingSpace(
          numLines, mAlignContent, &mPosition, &mNumPackingSpacesRemaining,
          &mPackingSpaceRemaining);
    } else if (mAlignContent.primary == StyleAlignFlags::STRETCH) {
      // Split space equally between the lines:
      MOZ_ASSERT(mPackingSpaceRemaining > 0,
                 "negative packing space should make us use 'flex-start' "
                 "instead of 'stretch' (and we shouldn't bother with this "
                 "code if we have 0 packing space)");

      uint32_t numLinesLeft = numLines;
      for (FlexLine& line : aLines) {
        // Our share is the amount of space remaining, divided by the number
        // of lines remainig.
        MOZ_ASSERT(numLinesLeft > 0, "miscalculated num lines");
        nscoord shareOfExtraSpace = mPackingSpaceRemaining / numLinesLeft;
        nscoord newSize = line.LineCrossSize() + shareOfExtraSpace;
        line.SetLineCrossSize(newSize);

        mPackingSpaceRemaining -= shareOfExtraSpace;
        numLinesLeft--;
      }
      MOZ_ASSERT(numLinesLeft == 0, "miscalculated num lines");
    } else {
      MOZ_ASSERT_UNREACHABLE("Unexpected align-content value");
    }
  }
}

void CrossAxisPositionTracker::TraversePackingSpace() {
  if (mNumPackingSpacesRemaining) {
    MOZ_ASSERT(mAlignContent.primary == StyleAlignFlags::SPACE_BETWEEN ||
                   mAlignContent.primary == StyleAlignFlags::SPACE_AROUND ||
                   mAlignContent.primary == StyleAlignFlags::SPACE_EVENLY,
               "mNumPackingSpacesRemaining only applies for "
               "space-between/space-around/space-evenly");

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

SingleLineCrossAxisPositionTracker::SingleLineCrossAxisPositionTracker(
    const FlexboxAxisTracker& aAxisTracker)
    : PositionTracker(aAxisTracker.GetWritingMode(), aAxisTracker.CrossAxis(),
                      aAxisTracker.IsCrossAxisReversed()) {}

void FlexLine::ComputeCrossSizeAndBaseline(
    const FlexboxAxisTracker& aAxisTracker) {
  nscoord crossStartToFurthestFirstBaseline = nscoord_MIN;
  nscoord crossEndToFurthestFirstBaseline = nscoord_MIN;
  nscoord crossStartToFurthestLastBaseline = nscoord_MIN;
  nscoord crossEndToFurthestLastBaseline = nscoord_MIN;
  nscoord largestOuterCrossSize = 0;
  for (const FlexItem& item : Items()) {
    nscoord curOuterCrossSize = item.OuterCrossSize();

    if ((item.AlignSelf()._0 == StyleAlignFlags::BASELINE ||
         item.AlignSelf()._0 == StyleAlignFlags::LAST_BASELINE) &&
        item.NumAutoMarginsInCrossAxis() == 0) {
      const bool useFirst = (item.AlignSelf()._0 == StyleAlignFlags::BASELINE);
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

      nscoord crossStartToBaseline = item.BaselineOffsetFromOuterCrossEdge(
          aAxisTracker.CrossAxisPhysicalStartSide(), useFirst);
      nscoord crossEndToBaseline = curOuterCrossSize - crossStartToBaseline;

      // Now, update our "largest" values for these (across all the flex items
      // in this flex line), so we can use them in computing the line's cross
      // size below:
      if (useFirst) {
        crossStartToFurthestFirstBaseline =
            std::max(crossStartToFurthestFirstBaseline, crossStartToBaseline);
        crossEndToFurthestFirstBaseline =
            std::max(crossEndToFurthestFirstBaseline, crossEndToBaseline);
      } else {
        crossStartToFurthestLastBaseline =
            std::max(crossStartToFurthestLastBaseline, crossStartToBaseline);
        crossEndToFurthestLastBaseline =
            std::max(crossEndToFurthestLastBaseline, crossEndToBaseline);
      }
    } else {
      largestOuterCrossSize =
          std::max(largestOuterCrossSize, curOuterCrossSize);
    }
  }

  // The line's baseline offset is the distance from the line's edge to the
  // furthest item-baseline. The item(s) with that baseline will be exactly
  // aligned with the line's edge.
  mFirstBaselineOffset = crossStartToFurthestFirstBaseline;
  mLastBaselineOffset = crossEndToFurthestLastBaseline;

  // The line's cross-size is the larger of:
  //  (a) [largest cross-start-to-baseline + largest baseline-to-cross-end] of
  //      all baseline-aligned items with no cross-axis auto margins...
  // and
  //  (b) [largest cross-start-to-baseline + largest baseline-to-cross-end] of
  //      all last baseline-aligned items with no cross-axis auto margins...
  // and
  //  (c) largest cross-size of all other children.
  mLineCrossSize = std::max(
      std::max(
          crossStartToFurthestFirstBaseline + crossEndToFurthestFirstBaseline,
          crossStartToFurthestLastBaseline + crossEndToFurthestLastBaseline),
      largestOuterCrossSize);
}

void FlexItem::ResolveStretchedCrossSize(nscoord aLineCrossSize) {
  // We stretch IFF we are align-self:stretch, have no auto margins in
  // cross axis, and have cross-axis size property == "auto". If any of those
  // conditions don't hold up, we won't stretch.
  if (mAlignSelf._0 != StyleAlignFlags::STRETCH ||
      NumAutoMarginsInCrossAxis() != 0 || !IsCrossSizeAuto()) {
    return;
  }

  // If we've already been stretched, we can bail out early, too.
  // No need to redo the calculation.
  if (mIsStretched) {
    return;
  }

  // Reserve space for margins & border & padding, and then use whatever
  // remains as our item's cross-size (clamped to its min/max range).
  nscoord stretchedSize = aLineCrossSize - MarginBorderPaddingSizeInCrossAxis();

  stretchedSize = NS_CSS_MINMAX(stretchedSize, mCrossMinSize, mCrossMaxSize);

  // Update the cross-size & make a note that it's stretched, so we know to
  // override the reflow input's computed cross-size in our final reflow.
  SetCrossSize(stretchedSize);
  mIsStretched = true;
}

static nsBlockFrame* FindFlexItemBlockFrame(nsIFrame* aFrame) {
  if (nsBlockFrame* block = do_QueryFrame(aFrame)) {
    return block;
  }
  for (nsIFrame* f : aFrame->PrincipalChildList()) {
    if (nsBlockFrame* block = FindFlexItemBlockFrame(f)) {
      return block;
    }
  }
  return nullptr;
}

nsBlockFrame* FlexItem::BlockFrame() const {
  return FindFlexItemBlockFrame(Frame());
}

void SingleLineCrossAxisPositionTracker::ResolveAutoMarginsInCrossAxis(
    const FlexLine& aLine, FlexItem& aItem) {
  // Subtract the space that our item is already occupying, to see how much
  // space (if any) is available for its auto margins.
  nscoord spaceForAutoMargins = aLine.LineCrossSize() - aItem.OuterCrossSize();

  if (spaceForAutoMargins <= 0) {
    return;  // No available space  --> nothing to do
  }

  uint32_t numAutoMargins = aItem.NumAutoMarginsInCrossAxis();
  if (numAutoMargins == 0) {
    return;  // No auto margins --> nothing to do.
  }

  // OK, we have at least one auto margin and we have some available space.
  // Give each auto margin a share of the space.
  const auto& styleMargin = aItem.Frame()->StyleMargin()->mMargin;
  for (const auto side : {StartSide(), EndSide()}) {
    if (styleMargin.Get(mWM, side).IsAuto()) {
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

void SingleLineCrossAxisPositionTracker::EnterAlignPackingSpace(
    const FlexLine& aLine, const FlexItem& aItem,
    const FlexboxAxisTracker& aAxisTracker) {
  // We don't do align-self alignment on items that have auto margins
  // in the cross axis.
  if (aItem.NumAutoMarginsInCrossAxis()) {
    return;
  }

  StyleAlignFlags alignSelf = aItem.AlignSelf()._0;
  // NOTE: 'stretch' behaves like 'flex-start' once we've stretched any
  // auto-sized items (which we've already done).
  if (alignSelf == StyleAlignFlags::STRETCH) {
    alignSelf = StyleAlignFlags::FLEX_START;
  }

  // Map 'self-start'/'self-end' to 'start'/'end'
  if (alignSelf == StyleAlignFlags::SELF_START ||
      alignSelf == StyleAlignFlags::SELF_END) {
    const LogicalAxis logCrossAxis =
        aAxisTracker.IsRowOriented() ? eLogicalAxisBlock : eLogicalAxisInline;
    const WritingMode cWM = aAxisTracker.GetWritingMode();
    const bool sameStart =
        cWM.ParallelAxisStartsOnSameSide(logCrossAxis, aItem.GetWritingMode());
    alignSelf = sameStart == (alignSelf == StyleAlignFlags::SELF_START)
                    ? StyleAlignFlags::START
                    : StyleAlignFlags::END;
  }

  // Map 'start'/'end' to 'flex-start'/'flex-end'.
  if (alignSelf == StyleAlignFlags::START) {
    alignSelf = aAxisTracker.IsCrossAxisReversed()
                    ? StyleAlignFlags::FLEX_END
                    : StyleAlignFlags::FLEX_START;
  } else if (alignSelf == StyleAlignFlags::END) {
    alignSelf = aAxisTracker.IsCrossAxisReversed() ? StyleAlignFlags::FLEX_START
                                                   : StyleAlignFlags::FLEX_END;
  }

  // 'align-self' falls back to 'flex-start' if it is 'center'/'flex-end' and we
  // have cross axis overflow
  // XXX we should really be falling back to 'start' as of bug 1472843
  if (aLine.LineCrossSize() < aItem.OuterCrossSize() &&
      (aItem.AlignSelfFlags() & StyleAlignFlags::SAFE)) {
    alignSelf = StyleAlignFlags::FLEX_START;
  }

  if (alignSelf == StyleAlignFlags::FLEX_START) {
    // No space to skip over -- we're done.
  } else if (alignSelf == StyleAlignFlags::FLEX_END) {
    mPosition += aLine.LineCrossSize() - aItem.OuterCrossSize();
  } else if (alignSelf == StyleAlignFlags::CENTER) {
    // Note: If cross-size is odd, the "after" space will get the extra unit.
    mPosition += (aLine.LineCrossSize() - aItem.OuterCrossSize()) / 2;
  } else if (alignSelf == StyleAlignFlags::BASELINE ||
             alignSelf == StyleAlignFlags::LAST_BASELINE) {
    const bool useFirst = (alignSelf == StyleAlignFlags::BASELINE);

    // Baseline-aligned items are collectively aligned with the line's physical
    // cross-start or cross-end side, depending on whether we're doing
    // first-baseline or last-baseline alignment.
    const mozilla::Side baselineAlignStartSide =
        useFirst ? aAxisTracker.CrossAxisPhysicalStartSide()
                 : aAxisTracker.CrossAxisPhysicalEndSide();

    nscoord itemBaselineOffset = aItem.BaselineOffsetFromOuterCrossEdge(
        baselineAlignStartSide, useFirst);

    nscoord lineBaselineOffset =
        useFirst ? aLine.FirstBaselineOffset() : aLine.LastBaselineOffset();

    NS_ASSERTION(lineBaselineOffset >= itemBaselineOffset,
                 "failed at finding largest baseline offset");

    // How much do we need to adjust our position (from the line edge),
    // to get the item's baseline to hit the line's baseline offset:
    nscoord baselineDiff = lineBaselineOffset - itemBaselineOffset;

    if (useFirst) {
      // mPosition is already at line's flex-start edge.
      // From there, we step *forward* by the baseline adjustment:
      mPosition += baselineDiff;
    } else {
      // Advance to align item w/ line's flex-end edge (as in FLEX_END case):
      mPosition += aLine.LineCrossSize() - aItem.OuterCrossSize();
      // ...and step *back* by the baseline adjustment:
      mPosition -= baselineDiff;
    }
  } else {
    MOZ_ASSERT_UNREACHABLE("Unexpected align-self value");
  }
}

FlexboxAxisInfo::FlexboxAxisInfo(const nsIFrame* aFlexContainer) {
  MOZ_ASSERT(aFlexContainer && aFlexContainer->IsFlexContainerFrame(),
             "Only flex containers may be passed to this constructor!");
  if (IsLegacyBox(aFlexContainer)) {
    InitAxesFromLegacyProps(aFlexContainer);
  } else {
    InitAxesFromModernProps(aFlexContainer);
  }
}

void FlexboxAxisInfo::InitAxesFromLegacyProps(const nsIFrame* aFlexContainer) {
  const nsStyleXUL* styleXUL = aFlexContainer->StyleXUL();

  const bool boxOrientIsVertical =
      (styleXUL->mBoxOrient == StyleBoxOrient::Vertical);
  const bool wmIsVertical = aFlexContainer->GetWritingMode().IsVertical();

  // If box-orient agrees with our writing-mode, then we're "row-oriented"
  // (i.e. the flexbox main axis is the same as our writing mode's inline
  // direction).  Otherwise, we're column-oriented (i.e. the flexbox's main
  // axis is perpendicular to the writing-mode's inline direction).
  mIsRowOriented = (boxOrientIsVertical == wmIsVertical);

  // Legacy flexbox can use "-webkit-box-direction: reverse" to reverse the
  // main axis (so it runs in the reverse direction of the inline axis):
  mIsMainAxisReversed = styleXUL->mBoxDirection == StyleBoxDirection::Reverse;

  // Legacy flexbox does not support reversing the cross axis -- it has no
  // equivalent of modern flexbox's "flex-wrap: wrap-reverse".
  mIsCrossAxisReversed = false;
}

void FlexboxAxisInfo::InitAxesFromModernProps(const nsIFrame* aFlexContainer) {
  const nsStylePosition* stylePos = aFlexContainer->StylePosition();
  StyleFlexDirection flexDirection = stylePos->mFlexDirection;

  // Determine main axis:
  switch (flexDirection) {
    case StyleFlexDirection::Row:
      mIsRowOriented = true;
      mIsMainAxisReversed = false;
      break;
    case StyleFlexDirection::RowReverse:
      mIsRowOriented = true;
      mIsMainAxisReversed = true;
      break;
    case StyleFlexDirection::Column:
      mIsRowOriented = false;
      mIsMainAxisReversed = false;
      break;
    case StyleFlexDirection::ColumnReverse:
      mIsRowOriented = false;
      mIsMainAxisReversed = true;
      break;
  }

  // "flex-wrap: wrap-reverse" reverses our cross axis.
  mIsCrossAxisReversed = stylePos->mFlexWrap == StyleFlexWrap::WrapReverse;
}

FlexboxAxisTracker::FlexboxAxisTracker(
    const nsFlexContainerFrame* aFlexContainer)
    : mWM(aFlexContainer->GetWritingMode()), mAxisInfo(aFlexContainer) {}

LogicalSide FlexboxAxisTracker::MainAxisStartSide() const {
  return MakeLogicalSide(
      MainAxis(), IsMainAxisReversed() ? eLogicalEdgeEnd : eLogicalEdgeStart);
}

LogicalSide FlexboxAxisTracker::CrossAxisStartSide() const {
  return MakeLogicalSide(
      CrossAxis(), IsCrossAxisReversed() ? eLogicalEdgeEnd : eLogicalEdgeStart);
}

bool nsFlexContainerFrame::ShouldUseMozBoxCollapseBehavior(
    const nsStyleDisplay* aFlexStyleDisp) {
  MOZ_ASSERT(StyleDisplay() == aFlexStyleDisp, "wrong StyleDisplay passed in");

  // Quick filter to screen out *actual* (not-coopted-for-emulation)
  // flex containers, using state bit:
  if (!IsLegacyBox(this)) {
    return false;
  }

  // Check our own display value:
  if (aFlexStyleDisp->mDisplay == mozilla::StyleDisplay::MozBox ||
      aFlexStyleDisp->mDisplay == mozilla::StyleDisplay::MozInlineBox) {
    return true;
  }

  // Check our parent's display value, if we're an anonymous box (with a
  // potentially-untrustworthy display value):
  auto pseudoType = Style()->GetPseudoType();
  if (pseudoType == PseudoStyleType::scrolledContent ||
      pseudoType == PseudoStyleType::buttonContent) {
    const nsStyleDisplay* disp = GetParent()->StyleDisplay();
    if (disp->mDisplay == mozilla::StyleDisplay::MozBox ||
        disp->mDisplay == mozilla::StyleDisplay::MozInlineBox) {
      return true;
    }
  }

  return false;
}

void nsFlexContainerFrame::GenerateFlexLines(
    const ReflowInput& aReflowInput, const nscoord aTentativeContentBoxMainSize,
    const nscoord aTentativeContentBoxCrossSize,
    const nsTArray<StrutInfo>& aStruts, const FlexboxAxisTracker& aAxisTracker,
    nscoord aMainGapSize, bool aHasLineClampEllipsis,
    nsTArray<nsIFrame*>& aPlaceholders, /* out */
    nsTArray<FlexLine>& aLines /* out */) {
  MOZ_ASSERT(aLines.IsEmpty(), "Expecting outparam to start out empty");

  auto ConstructNewFlexLine = [&aLines, aMainGapSize]() {
    return aLines.EmplaceBack(aMainGapSize);
  };

  const bool isSingleLine =
      StyleFlexWrap::Nowrap == aReflowInput.mStylePosition->mFlexWrap;

  // We have at least one FlexLine. Even an empty flex container has a single
  // (empty) flex line.
  FlexLine* curLine = ConstructNewFlexLine();

  nscoord wrapThreshold;
  if (isSingleLine) {
    // Not wrapping. Set threshold to sentinel value that tells us not to wrap.
    wrapThreshold = NS_UNCONSTRAINEDSIZE;
  } else {
    // Wrapping! Set wrap threshold to flex container's content-box main-size.
    wrapThreshold = aTentativeContentBoxMainSize;

    // If the flex container doesn't have a definite content-box main-size
    // (e.g. if main axis is vertical & 'height' is 'auto'), make sure we at
    // least wrap when we hit its max main-size.
    if (wrapThreshold == NS_UNCONSTRAINEDSIZE) {
      const nscoord flexContainerMaxMainSize =
          aAxisTracker.MainComponent(aReflowInput.ComputedMaxSize());
      wrapThreshold = flexContainerMaxMainSize;
    }
  }

  // Tracks the index of the next strut, in aStruts (and when this hits
  // aStruts.Length(), that means there are no more struts):
  uint32_t nextStrutIdx = 0;

  // Overall index of the current flex item in the flex container. (This gets
  // checked against entries in aStruts.)
  uint32_t itemIdxInContainer = 0;

  CSSOrderAwareFrameIterator iter(
      this, kPrincipalList, CSSOrderAwareFrameIterator::ChildFilter::IncludeAll,
      CSSOrderAwareFrameIterator::OrderState::Unknown,
      OrderingPropertyForIter(this));

  AddOrRemoveStateBits(NS_STATE_FLEX_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER,
                       iter.ItemsAreAlreadyInOrder());

  bool prevItemRequestedBreakAfter = false;
  const bool useMozBoxCollapseBehavior =
      ShouldUseMozBoxCollapseBehavior(aReflowInput.mStyleDisplay);

  for (; !iter.AtEnd(); iter.Next()) {
    nsIFrame* childFrame = *iter;
    // Don't create flex items / lines for placeholder frames:
    if (childFrame->IsPlaceholderFrame()) {
      aPlaceholders.AppendElement(childFrame);
      continue;
    }

    // If we're multi-line and current line isn't empty, create a new flex line
    // to satisfy the previous flex item's request or to honor "break-before".
    if (!isSingleLine && !curLine->IsEmpty() &&
        (prevItemRequestedBreakAfter ||
         childFrame->StyleDisplay()->BreakBefore())) {
      curLine = ConstructNewFlexLine();
      prevItemRequestedBreakAfter = false;
    }

    if (useMozBoxCollapseBehavior &&
        (StyleVisibility::Collapse ==
         childFrame->StyleVisibility()->mVisible)) {
      // Legacy visibility:collapse behavior: make a 0-sized strut. (No need to
      // bother with aStruts and remembering cross size.)
      curLine->Items().EmplaceBack(childFrame, 0, aReflowInput.GetWritingMode(),
                                   aAxisTracker);
    } else if (nextStrutIdx < aStruts.Length() &&
               aStruts[nextStrutIdx].mItemIdx == itemIdxInContainer) {
      // Use the simplified "strut" FlexItem constructor:
      curLine->Items().EmplaceBack(childFrame,
                                   aStruts[nextStrutIdx].mStrutCrossSize,
                                   aReflowInput.GetWritingMode(), aAxisTracker);
      nextStrutIdx++;
    } else {
      GenerateFlexItemForChild(*curLine, childFrame, aReflowInput, aAxisTracker,
                               aTentativeContentBoxCrossSize,
                               aHasLineClampEllipsis);
    }

    // Check if we need to wrap the newly appended item to a new line, i.e. if
    // its outer hypothetical main size pushes our line over the threshold.
    // But we don't wrap if the line-length is unconstrained, nor do we wrap if
    // this was the first item on the line.
    if (wrapThreshold != NS_UNCONSTRAINEDSIZE &&
        curLine->Items().Length() > 1) {
      // If the line will be longer than wrapThreshold or at least as long as
      // nscoord_MAX because of the newly appended item, then wrap and move the
      // item to a new line.
      auto newOuterSize = curLine->TotalOuterHypotheticalMainSize();
      newOuterSize += curLine->Items().LastElement().OuterMainSize();

      // Account for gap between this line's previous item and this item.
      newOuterSize += aMainGapSize;

      if (newOuterSize >= nscoord_MAX || newOuterSize > wrapThreshold) {
        curLine = ConstructNewFlexLine();

        // Get the previous line after adding a new line because the address can
        // change if nsTArray needs to reallocate a new space for the new line.
        FlexLine& prevLine = aLines[aLines.Length() - 2];

        // Move the item from the end of prevLine to the end of curLine.
        curLine->Items().AppendElement(prevLine.Items().PopLastElement());
      }
    }

    // Update the line's bookkeeping about how large its items collectively are.
    curLine->AddLastItemToMainSizeTotals();

    // Honor "break-after" if we're multi-line. If we have more children, we
    // will create a new flex line in the next iteration.
    if (!isSingleLine && childFrame->StyleDisplay()->BreakAfter()) {
      prevItemRequestedBreakAfter = true;
    }
    itemIdxInContainer++;
  }
}

nsFlexContainerFrame::FlexLayoutResult
nsFlexContainerFrame::GenerateFlexLayoutResult() {
  MOZ_ASSERT(GetPrevInFlow(), "This should be called by non-first-in-flows!");

  auto* data = FirstInFlow()->GetProperty(SharedFlexData::Prop());
  MOZ_ASSERT(data, "SharedFlexData should be set by our first-in-flow!");

  FlexLayoutResult flr;

  // Pretend we have only one line and zero main gap size.
  flr.mLines.AppendElement(FlexLine(0));

  // The order state of the children is consistent across entire continuation
  // chain due to calling nsContainerFrame::NormalizeChildLists() at the
  // beginning of Reflow(), so we can align our state bit with our
  // prev-in-flow's state. Setup here before calling OrderStateForIter() below.
  AddOrRemoveStateBits(NS_STATE_FLEX_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER,
                       GetPrevInFlow()->HasAnyStateBits(
                           NS_STATE_FLEX_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER));

  // Construct flex items for this flex container fragment from existing flex
  // items in SharedFlexData.
  CSSOrderAwareFrameIterator iter(
      this, kPrincipalList,
      CSSOrderAwareFrameIterator::ChildFilter::SkipPlaceholders,
      OrderStateForIter(this), OrderingPropertyForIter(this));

  FlexItemIterator itemIter(data->mLines);

  for (; !iter.AtEnd(); iter.Next()) {
    nsIFrame* const child = *iter;
    nsIFrame* const childFirstInFlow = child->FirstInFlow();

    MOZ_ASSERT(!itemIter.AtEnd(),
               "Why can't we find FlexItem for our child frame?");

    for (; !itemIter.AtEnd(); itemIter.Next()) {
      if (itemIter->Frame() == childFirstInFlow) {
        flr.mLines[0].Items().AppendElement(itemIter->CloneFor(child));
        itemIter.Next();
        break;
      }
    }
  }

  flr.mContentBoxMainSize = data->mContentBoxMainSize;
  flr.mContentBoxCrossSize = data->mContentBoxCrossSize;

  return flr;
}

// Returns the largest outer hypothetical main-size of any line in |aLines|.
// (i.e. the hypothetical main-size of the largest line)
static AuCoord64 GetLargestLineMainSize(nsTArray<FlexLine>& aLines) {
  AuCoord64 largestLineOuterSize = 0;
  for (const FlexLine& line : aLines) {
    largestLineOuterSize =
        std::max(largestLineOuterSize, line.TotalOuterHypotheticalMainSize());
  }
  return largestLineOuterSize;
}

nscoord nsFlexContainerFrame::ComputeMainSize(
    const ReflowInput& aReflowInput, const FlexboxAxisTracker& aAxisTracker,
    const nscoord aTentativeContentBoxMainSize,
    nsTArray<FlexLine>& aLines) const {
  if (aAxisTracker.IsRowOriented()) {
    // Row-oriented --> our main axis is the inline axis, so our main size
    // is our inline size (which should already be resolved).
    return aTentativeContentBoxMainSize;
  }

  const bool shouldApplyAutomaticMinimumOnBlockAxis =
      aReflowInput.ShouldApplyAutomaticMinimumOnBlockAxis();
  if (aTentativeContentBoxMainSize != NS_UNCONSTRAINEDSIZE &&
      !shouldApplyAutomaticMinimumOnBlockAxis) {
    // Column-oriented case, with fixed BSize:
    // Just use our fixed block-size because we always assume the available
    // block-size is unconstrained, and the reflow input has already done the
    // appropriate min/max-BSize clamping.
    return aTentativeContentBoxMainSize;
  }

  // Column-oriented case, with size-containment in block axis:
  // Behave as if we had no content and just use our MinBSize.
  if (aReflowInput.mStyleDisplay->GetContainSizeAxes().mBContained) {
    return aReflowInput.ComputedMinBSize();
  }

  const AuCoord64 largestLineMainSize = GetLargestLineMainSize(aLines);
  const nscoord contentBSize = NS_CSS_MINMAX(
      nscoord(largestLineMainSize.ToMinMaxClamped()),
      aReflowInput.ComputedMinBSize(), aReflowInput.ComputedMaxBSize());
  // If the clamped largest FlexLine length is larger than the tentative main
  // size (which is resolved by aspect-ratio), we extend it to contain the
  // entire FlexLine.
  // https://drafts.csswg.org/css-sizing-4/#aspect-ratio-minimum
  if (shouldApplyAutomaticMinimumOnBlockAxis) {
    // Column-oriented case, with auto BSize which is resolved by
    // aspect-ratio.
    return std::max(contentBSize, aTentativeContentBoxMainSize);
  }

  // Column-oriented case, with auto BSize:
  // Resolve auto BSize to the largest FlexLine length, clamped to our
  // computed min/max main-size properties.
  return contentBSize;
}

nscoord nsFlexContainerFrame::ComputeCrossSize(
    const ReflowInput& aReflowInput, const FlexboxAxisTracker& aAxisTracker,
    const nscoord aTentativeContentBoxCrossSize, nscoord aSumLineCrossSizes,
    bool* aIsDefinite) const {
  MOZ_ASSERT(aIsDefinite, "outparam pointer must be non-null");

  if (aAxisTracker.IsColumnOriented()) {
    // Column-oriented --> our cross axis is the inline axis, so our cross size
    // is our inline size (which should already be resolved).
    *aIsDefinite = true;
    // FIXME: Bug 1661847 - there are cases where aTentativeContentBoxCrossSize
    // (i.e. aReflowInput.ComputedISize()) might not be the right thing to
    // return here. Specifically: if our cross size is an intrinsic size, and we
    // have flex items that are flexible and have aspect ratios, then we may
    // need to take their post-flexing main sizes into account (multiplied
    // through their aspect ratios to get their cross sizes), in order to
    // determine their flex line's size & the flex container's cross size (e.g.
    // as `aSumLineCrossSizes`).
    return aTentativeContentBoxCrossSize;
  }

  const bool shouldApplyAutomaticMinimumOnBlockAxis =
      aReflowInput.ShouldApplyAutomaticMinimumOnBlockAxis();
  const nscoord computedBSize = aReflowInput.ComputedBSize();
  if (computedBSize != NS_UNCONSTRAINEDSIZE &&
      !shouldApplyAutomaticMinimumOnBlockAxis) {
    // Row-oriented case (cross axis is block-axis), with fixed BSize:
    *aIsDefinite = true;

    // Just use our fixed block-size because we always assume the available
    // block-size is unconstrained, and the reflow input has already done the
    // appropriate min/max-BSize clamping.
    return computedBSize;
  }

  // Row-oriented case, with size-containment in block axis:
  // Behave as if we had no content and just use our MinBSize.
  if (aReflowInput.mStyleDisplay->GetContainSizeAxes().mBContained) {
    *aIsDefinite = true;
    return aReflowInput.ComputedMinBSize();
  }

  // The cross size must not be definite in the following cases.
  *aIsDefinite = false;

  const nscoord contentBSize =
      NS_CSS_MINMAX(aSumLineCrossSizes, aReflowInput.ComputedMinBSize(),
                    aReflowInput.ComputedMaxBSize());
  // If the content block-size is larger than the effective computed
  // block-size, we extend the block-size to contain all the content.
  // https://drafts.csswg.org/css-sizing-4/#aspect-ratio-minimum
  if (shouldApplyAutomaticMinimumOnBlockAxis) {
    // Row-oriented case (cross axis is block-axis), with auto BSize which is
    // resolved by aspect-ratio or content size.
    return std::max(contentBSize, computedBSize);
  }

  // Row-oriented case (cross axis is block axis), with auto BSize:
  // Shrink-wrap our line(s), subject to our min-size / max-size
  // constraints in that (block) axis.
  return contentBSize;
}

LogicalSize nsFlexContainerFrame::ComputeAvailableSizeForItems(
    const ReflowInput& aReflowInput,
    const mozilla::LogicalMargin& aBorderPadding) const {
  const WritingMode wm = GetWritingMode();
  nscoord availableBSize = aReflowInput.AvailableBSize();

  if (availableBSize != NS_UNCONSTRAINEDSIZE) {
    // Available block-size is constrained. Subtract block-start border and
    // padding from it.
    availableBSize -= aBorderPadding.BStart(wm);

    if (aReflowInput.mStyleBorder->mBoxDecorationBreak ==
        StyleBoxDecorationBreak::Clone) {
      // We have box-decoration-break:clone. Subtract block-end border and
      // padding from the available block-size as well.
      availableBSize -= aBorderPadding.BEnd(wm);
    }

    // Available block-size can became negative after subtracting block-axis
    // border and padding. Per spec, to guarantee progress, fragmentainers are
    // assumed to have a minimum block size of 1px regardless of their used
    // size. https://drafts.csswg.org/css-break/#breaking-rules
    availableBSize =
        std::max(nsPresContext::CSSPixelsToAppUnits(1), availableBSize);
  }

  return LogicalSize(wm, aReflowInput.ComputedISize(), availableBSize);
}

void FlexLine::PositionItemsInMainAxis(
    const StyleContentDistribution& aJustifyContent,
    nscoord aContentBoxMainSize, const FlexboxAxisTracker& aAxisTracker) {
  MainAxisPositionTracker mainAxisPosnTracker(
      aAxisTracker, this, aJustifyContent, aContentBoxMainSize);
  for (FlexItem& item : Items()) {
    nscoord itemMainBorderBoxSize =
        item.MainSize() + item.BorderPaddingSizeInMainAxis();

    // Resolve any main-axis 'auto' margins on aChild to an actual value.
    mainAxisPosnTracker.ResolveAutoMarginsInMainAxis(item);

    // Advance our position tracker to child's upper-left content-box corner,
    // and use that as its position in the main axis.
    mainAxisPosnTracker.EnterMargin(item.Margin());
    mainAxisPosnTracker.EnterChildFrame(itemMainBorderBoxSize);

    item.SetMainPosition(mainAxisPosnTracker.Position());

    mainAxisPosnTracker.ExitChildFrame(itemMainBorderBoxSize);
    mainAxisPosnTracker.ExitMargin(item.Margin());
    mainAxisPosnTracker.TraversePackingSpace();
    if (&item != &Items().LastElement()) {
      mainAxisPosnTracker.TraverseGap(mMainGapSize);
    }
  }
}

/**
 * Given the flex container's "flex-relative ascent" (i.e. distance from the
 * flex container's content-box cross-start edge to its baseline), returns
 * its actual physical ascent value (the distance from the *border-box* top
 * edge to its baseline).
 */
static nscoord ComputePhysicalAscentFromFlexRelativeAscent(
    nscoord aFlexRelativeAscent, nscoord aContentBoxCrossSize,
    const ReflowInput& aReflowInput, const FlexboxAxisTracker& aAxisTracker) {
  return aReflowInput.ComputedPhysicalBorderPadding().top +
         PhysicalCoordFromFlexRelativeCoord(
             aFlexRelativeAscent, aContentBoxCrossSize,
             aAxisTracker.CrossAxisPhysicalStartSide());
}

void nsFlexContainerFrame::SizeItemInCrossAxis(ReflowInput& aChildReflowInput,
                                               FlexItem& aItem) {
  // If cross axis is the item's inline axis, just use ISize from reflow input,
  // and don't bother with a full reflow.
  if (aItem.IsInlineAxisCrossAxis()) {
    aItem.SetCrossSize(aChildReflowInput.ComputedISize());
    return;
  }

  MOZ_ASSERT(!aItem.HadMeasuringReflow(),
             "We shouldn't need more than one measuring reflow");

  if (aItem.AlignSelf()._0 == StyleAlignFlags::STRETCH) {
    // This item's got "align-self: stretch", so we probably imposed a
    // stretched computed cross-size on it during its previous
    // reflow. We're not imposing that BSize for *this* "measuring" reflow, so
    // we need to tell it to treat this reflow as a resize in its block axis
    // (regardless of whether any of its ancestors are actually being resized).
    // (Note: we know that the cross axis is the item's *block* axis -- if it
    // weren't, then we would've taken the early-return above.)
    aChildReflowInput.SetBResize(true);
    // Not 100% sure this is needed, but be conservative for now:
    aChildReflowInput.mFlags.mIsBResizeForPercentages = true;
  }

  // Potentially reflow the item, and get the sizing info.
  const CachedBAxisMeasurement& measurement =
      MeasureBSizeForFlexItem(aItem, aChildReflowInput);

  // Save the sizing info that we learned from this reflow
  // -----------------------------------------------------

  // Tentatively store the child's desired content-box cross-size.
  aItem.SetCrossSize(measurement.BSize());
}

void FlexLine::PositionItemsInCrossAxis(
    nscoord aLineStartPosition, const FlexboxAxisTracker& aAxisTracker) {
  SingleLineCrossAxisPositionTracker lineCrossAxisPosnTracker(aAxisTracker);

  for (FlexItem& item : Items()) {
    // First, stretch the item's cross size (if appropriate), and resolve any
    // auto margins in this axis.
    item.ResolveStretchedCrossSize(mLineCrossSize);
    lineCrossAxisPosnTracker.ResolveAutoMarginsInCrossAxis(*this, item);

    // Compute the cross-axis position of this item
    nscoord itemCrossBorderBoxSize =
        item.CrossSize() + item.BorderPaddingSizeInCrossAxis();
    lineCrossAxisPosnTracker.EnterAlignPackingSpace(*this, item, aAxisTracker);
    lineCrossAxisPosnTracker.EnterMargin(item.Margin());
    lineCrossAxisPosnTracker.EnterChildFrame(itemCrossBorderBoxSize);

    item.SetCrossPosition(aLineStartPosition +
                          lineCrossAxisPosnTracker.Position());

    // Back out to cross-axis edge of the line.
    lineCrossAxisPosnTracker.ResetPosition();
  }
}

void nsFlexContainerFrame::Reflow(nsPresContext* aPresContext,
                                  ReflowOutput& aReflowOutput,
                                  const ReflowInput& aReflowInput,
                                  nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsFlexContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aReflowOutput, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  MOZ_ASSERT(aPresContext == PresContext());
  NS_WARNING_ASSERTION(
      aReflowInput.ComputedISize() != NS_UNCONSTRAINEDSIZE,
      "Unconstrained inline size; this should only result from huge sizes "
      "(not intrinsic sizing w/ orthogonal flows)");

  FLEX_LOG("Reflow() for nsFlexContainerFrame %p", this);

  if (IsFrameTreeTooDeep(aReflowInput, aReflowOutput, aStatus)) {
    return;
  }

  NormalizeChildLists();

#ifdef DEBUG
  mDidPushItemsBitMayLie = false;
  SanityCheckChildListsBeforeReflow();
#endif  // DEBUG

  // We (and our children) can only depend on our ancestor's bsize if we have
  // a percent-bsize, or if we're positioned and we have "block-start" and
  // "block-end" set and have block-size:auto.  (There are actually other cases,
  // too -- e.g. if our parent is itself a block-dir flex container and we're
  // flexible -- but we'll let our ancestors handle those sorts of cases.)
  //
  // TODO(emilio): the !bsize.IsLengthPercentage() preserves behavior, but it's
  // too conservative. min/max-content don't really depend on the container.
  WritingMode wm = aReflowInput.GetWritingMode();
  const nsStylePosition* stylePos = StylePosition();
  const auto& bsize = stylePos->BSize(wm);
  if (bsize.HasPercent() || (StyleDisplay()->IsAbsolutelyPositionedStyle() &&
                             (bsize.IsAuto() || !bsize.IsLengthPercentage()) &&
                             !stylePos->mOffset.GetBStart(wm).IsAuto() &&
                             !stylePos->mOffset.GetBEnd(wm).IsAuto())) {
    AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
  }

  // Check if there is a -webkit-line-clamp ellipsis somewhere inside at least
  // one of the flex items, so we can clear the flag before the block frame
  // re-sets it on the appropriate line during its bsize measuring reflow.
  bool hasLineClampEllipsis =
      HasAnyStateBits(NS_STATE_FLEX_HAS_LINE_CLAMP_ELLIPSIS);
  RemoveStateBits(NS_STATE_FLEX_HAS_LINE_CLAMP_ELLIPSIS);

  const FlexboxAxisTracker axisTracker(this);

  // Check to see if we need to create a computed info structure, to
  // be filled out for use by devtools.
  ComputedFlexContainerInfo* containerInfo = CreateOrClearFlexContainerInfo();

  FlexLayoutResult flr;
  if (!GetPrevInFlow()) {
    const LogicalSize tentativeContentBoxSize = aReflowInput.ComputedSize();
    const nscoord tentativeContentBoxMainSize =
        axisTracker.MainComponent(tentativeContentBoxSize);
    const nscoord tentativeContentBoxCrossSize =
        axisTracker.CrossComponent(tentativeContentBoxSize);

    // Calculate gap sizes for main and cross axis. We only need them in
    // DoFlexLayout in the first-in-flow, so no need to worry about consumed
    // block-size.
    const auto& mainGapStyle =
        axisTracker.IsRowOriented() ? stylePos->mColumnGap : stylePos->mRowGap;
    const auto& crossGapStyle =
        axisTracker.IsRowOriented() ? stylePos->mRowGap : stylePos->mColumnGap;
    const nscoord mainGapSize = nsLayoutUtils::ResolveGapToLength(
        mainGapStyle, tentativeContentBoxMainSize);
    const nscoord crossGapSize = nsLayoutUtils::ResolveGapToLength(
        crossGapStyle, tentativeContentBoxCrossSize);

    // When fragmenting a flex container, we run the flex algorithm without
    // regards to pagination in order to compute the flex container's desired
    // content-box size. https://drafts.csswg.org/css-flexbox-1/#pagination-algo
    //
    // Note: For a multi-line column-oriented flex container, the sample
    // algorithm suggests we wrap the flex line at the block-end edge of a
    // column/page, but we do not implement it intentionally. This brings the
    // layout result closer to the one as if there's no fragmentation.
    AutoTArray<StrutInfo, 1> struts;
    flr =
        DoFlexLayout(aReflowInput, tentativeContentBoxMainSize,
                     tentativeContentBoxCrossSize, axisTracker, mainGapSize,
                     crossGapSize, hasLineClampEllipsis, struts, containerInfo);

    if (!struts.IsEmpty()) {
      // We're restarting flex layout, with new knowledge of collapsed items.
      flr.mLines.Clear();
      flr.mPlaceholders.Clear();
      flr = DoFlexLayout(aReflowInput, tentativeContentBoxMainSize,
                         tentativeContentBoxCrossSize, axisTracker, mainGapSize,
                         crossGapSize, hasLineClampEllipsis, struts,
                         containerInfo);
    }
  } else {
    flr = GenerateFlexLayoutResult();
  }

  const LogicalSize contentBoxSize =
      axisTracker.LogicalSizeFromFlexRelativeSizes(flr.mContentBoxMainSize,
                                                   flr.mContentBoxCrossSize);
  const nscoord consumedBSize = CalcAndCacheConsumedBSize();
  const nscoord effectiveContentBSize =
      contentBoxSize.BSize(wm) - consumedBSize;
  LogicalMargin borderPadding = aReflowInput.ComputedLogicalBorderPadding(wm);
  bool mayNeedNextInFlow = false;
  if (MOZ_UNLIKELY(aReflowInput.AvailableBSize() != NS_UNCONSTRAINEDSIZE)) {
    // We assume we are the last fragment by using
    // PreReflowBlockLevelLogicalSkipSides(), and skip block-end border and
    // padding if needed.
    borderPadding.ApplySkipSides(PreReflowBlockLevelLogicalSkipSides());
    // Check if we may need a next-in-flow. If so, we'll need to skip block-end
    // border and padding.
    const LogicalSize availableSizeForItems =
        ComputeAvailableSizeForItems(aReflowInput, borderPadding);
    mayNeedNextInFlow = effectiveContentBSize > availableSizeForItems.BSize(wm);
    if (mayNeedNextInFlow && aReflowInput.mStyleBorder->mBoxDecorationBreak ==
                                 StyleBoxDecorationBreak::Slice) {
      borderPadding.BEnd(wm) = 0;
    }
  }

  // Determine this frame's tentative border-box size. This is used for logical
  // to physical coordinate conversion when positioning children.
  //
  // Note that vertical-rl writing-mode is the only case where the block flow
  // direction progresses in a negative physical direction, and therefore block
  // direction coordinate conversion depends on knowing the width of the
  // coordinate space in order to translate between the logical and physical
  // origins. As a result, if our final border-box block-size is different from
  // this tentative one, and we are in vertical-rl writing mode, we need to
  // adjust our children's position after reflowing them.
  const LogicalSize tentativeBorderBoxSize(
      wm, contentBoxSize.ISize(wm) + borderPadding.IStartEnd(wm),
      std::min(effectiveContentBSize + borderPadding.BStartEnd(wm),
               aReflowInput.AvailableBSize()));
  const nsSize containerSize = tentativeBorderBoxSize.GetPhysicalSize(wm);

  const auto* prevInFlow = static_cast<nsFlexContainerFrame*>(GetPrevInFlow());
  OverflowAreas ocBounds;
  nsReflowStatus ocStatus;
  nscoord sumOfChildrenBlockSize;
  if (prevInFlow) {
    ReflowOverflowContainerChildren(
        aPresContext, aReflowInput, ocBounds, ReflowChildFlags::Default,
        ocStatus, MergeSortedFrameListsFor, Some(containerSize));
    sumOfChildrenBlockSize =
        prevInFlow->GetProperty(SumOfChildrenBlockSizeProperty());
  } else {
    sumOfChildrenBlockSize = 0;
  }

  const LogicalSize availableSizeForItems =
      ComputeAvailableSizeForItems(aReflowInput, borderPadding);
  const auto [maxBlockEndEdgeOfChildren, areChildrenComplete] = ReflowChildren(
      aReflowInput, containerSize, availableSizeForItems, borderPadding,
      sumOfChildrenBlockSize, axisTracker, hasLineClampEllipsis, flr);

  // maxBlockEndEdgeOfChildren is relative to border-box, so we need to subtract
  // block-start border and padding to make it relative to our content-box. Note
  // that if there is a packing space in between the last flex item's block-end
  // edge and the available space's block-end edge, we want to record the
  // available size of item to consume part of the packing space.
  sumOfChildrenBlockSize +=
      std::max(maxBlockEndEdgeOfChildren - borderPadding.BStart(wm),
               availableSizeForItems.BSize(wm));

  PopulateReflowOutput(aReflowOutput, aReflowInput, aStatus, contentBoxSize,
                       borderPadding, consumedBSize, mayNeedNextInFlow,
                       maxBlockEndEdgeOfChildren, areChildrenComplete,
                       flr.mAscent, flr.mLines, axisTracker);

  if (wm.IsVerticalRL()) {
    // If the final border-box block-size is different from the tentative one,
    // adjust our children's position.
    const nscoord deltaBCoord =
        tentativeBorderBoxSize.BSize(wm) - aReflowOutput.Size(wm).BSize(wm);
    if (deltaBCoord != 0) {
      const LogicalPoint delta(wm, 0, deltaBCoord);
      for (const FlexLine& line : flr.mLines) {
        for (const FlexItem& item : line.Items()) {
          item.Frame()->MovePositionBy(wm, delta);
        }
      }
    }
  }

  // Overflow area = union(my overflow area, children's overflow areas)
  aReflowOutput.SetOverflowAreasToDesiredBounds();
  for (nsIFrame* childFrame : mFrames) {
    ConsiderChildOverflow(aReflowOutput.mOverflowAreas, childFrame);
  }

  MOZ_ASSERT(!flr.mLines.IsEmpty(),
             "Flex container should have at least one FlexLine!");
  if (Style()->GetPseudoType() == PseudoStyleType::scrolledContent &&
      !flr.mLines.IsEmpty() && !flr.mLines[0].IsEmpty()) {
    MOZ_ASSERT(aReflowInput.ComputedLogicalBorderPadding(wm) ==
                   aReflowInput.ComputedLogicalPadding(wm),
               "A scrolled inner frame shouldn't have any border!");
    const LogicalMargin& padding = borderPadding;

    // The CSS Overflow spec [1] requires that a scrollable container's
    // scrollable overflow should include the following areas.
    //
    // a) "the box's own content and padding areas": we treat the *content* as
    // the scrolled inner frame's theoretical content-box that's intrinsically
    // sized to the union of all the flex items' margin boxes, _without_
    // relative positioning applied. The *padding areas* is just inflation on
    // top of the theoretical content-box by the flex container's padding.
    //
    // b) "the margin areas of grid item and flex item boxes for which the box
    // establishes a containing block": a) already includes the flex items'
    // normal-positioned margin boxes into the scrollable overflow, but their
    // relative-positioned margin boxes should also be included because relpos
    // children are still flex items.
    //
    // [1] https://drafts.csswg.org/css-overflow-3/#scrollable.

    // Union of normal-positioned margin boxes for all the items.
    nsRect itemMarginBoxes;
    // Union of relative-positioned margin boxes for the relpos items only.
    nsRect relPosItemMarginBoxes;

    for (const FlexLine& line : flr.mLines) {
      for (const FlexItem& item : line.Items()) {
        const nsIFrame* f = item.Frame();
        if (MOZ_UNLIKELY(f->IsRelativelyOrStickyPositioned())) {
          const nsRect marginRect = f->GetMarginRectRelativeToSelf();
          itemMarginBoxes =
              itemMarginBoxes.Union(marginRect + f->GetNormalPosition());
          relPosItemMarginBoxes =
              relPosItemMarginBoxes.Union(marginRect + f->GetPosition());
        } else {
          itemMarginBoxes = itemMarginBoxes.Union(f->GetMarginRect());
        }
      }
    }

    itemMarginBoxes.Inflate(padding.GetPhysicalMargin(wm));
    aReflowOutput.mOverflowAreas.UnionAllWith(itemMarginBoxes);
    aReflowOutput.mOverflowAreas.UnionAllWith(relPosItemMarginBoxes);
  }

  // Merge overflow container bounds and status.
  aReflowOutput.mOverflowAreas.UnionWith(ocBounds);
  aStatus.MergeCompletionStatusFrom(ocStatus);

  FinishReflowWithAbsoluteFrames(PresContext(), aReflowOutput, aReflowInput,
                                 aStatus);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aReflowOutput)

  // Finally update our line and item measurements in our containerInfo.
  if (MOZ_UNLIKELY(containerInfo)) {
    UpdateFlexLineAndItemInfo(*containerInfo, flr.mLines);
  }

  // If we are the first-in-flow, we want to store data for our next-in-flows,
  // or clear the existing data if it is not needed.
  if (!GetPrevInFlow()) {
    SharedFlexData* data = GetProperty(SharedFlexData::Prop());
    if (!aStatus.IsFullyComplete()) {
      if (!data) {
        data = new SharedFlexData;
        SetProperty(SharedFlexData::Prop(), data);
      }
      data->mLines = std::move(flr.mLines);
      data->mContentBoxMainSize = flr.mContentBoxMainSize;
      data->mContentBoxCrossSize = flr.mContentBoxCrossSize;

      SetProperty(SumOfChildrenBlockSizeProperty(), sumOfChildrenBlockSize);
    } else if (data) {
      // We are fully-complete, so no next-in-flow is needed. Delete the
      // existing data.
      RemoveProperty(SharedFlexData::Prop());
      RemoveProperty(SumOfChildrenBlockSizeProperty());
    }
  } else {
    SetProperty(SumOfChildrenBlockSizeProperty(), sumOfChildrenBlockSize);
  }
}

void nsFlexContainerFrame::CalculatePackingSpace(
    uint32_t aNumThingsToPack, const StyleContentDistribution& aAlignVal,
    nscoord* aFirstSubjectOffset, uint32_t* aNumPackingSpacesRemaining,
    nscoord* aPackingSpaceRemaining) {
  StyleAlignFlags val = aAlignVal.primary;
  MOZ_ASSERT(val == StyleAlignFlags::SPACE_BETWEEN ||
                 val == StyleAlignFlags::SPACE_AROUND ||
                 val == StyleAlignFlags::SPACE_EVENLY,
             "Unexpected alignment value");

  MOZ_ASSERT(*aPackingSpaceRemaining >= 0,
             "Should not be called with negative packing space");

  // Note: In the aNumThingsToPack==1 case, the fallback behavior for
  // 'space-between' depends on precise information about the axes that we
  // don't have here. So, for that case, we just depend on the caller to
  // explicitly convert 'space-{between,around,evenly}' keywords to the
  // appropriate fallback alignment and skip this function.
  MOZ_ASSERT(aNumThingsToPack > 1,
             "Should not be called unless there's more than 1 thing to pack");

  // Packing spaces between items:
  *aNumPackingSpacesRemaining = aNumThingsToPack - 1;

  if (val == StyleAlignFlags::SPACE_BETWEEN) {
    // No need to reserve space at beginning/end, so we're done.
    return;
  }

  // We need to add 1 or 2 packing spaces, split between beginning/end, for
  // space-around / space-evenly:
  size_t numPackingSpacesForEdges =
      val == StyleAlignFlags::SPACE_AROUND ? 1 : 2;

  // How big will each "full" packing space be:
  nscoord packingSpaceSize =
      *aPackingSpaceRemaining /
      (*aNumPackingSpacesRemaining + numPackingSpacesForEdges);
  // How much packing-space are we allocating to the edges:
  nscoord totalEdgePackingSpace = numPackingSpacesForEdges * packingSpaceSize;

  // Use half of that edge packing space right now:
  *aFirstSubjectOffset += totalEdgePackingSpace / 2;
  // ...but we need to subtract all of it right away, so that we won't
  // hand out any of it to intermediate packing spaces.
  *aPackingSpaceRemaining -= totalEdgePackingSpace;
}

ComputedFlexContainerInfo*
nsFlexContainerFrame::CreateOrClearFlexContainerInfo() {
  if (!ShouldGenerateComputedInfo()) {
    return nullptr;
  }

  // The flag that sets ShouldGenerateComputedInfo() will never be cleared.
  // That's acceptable because it's only set in a Chrome API invoked by
  // devtools, and won't impact normal browsing.

  // Re-use the ComputedFlexContainerInfo, if it exists.
  ComputedFlexContainerInfo* info = GetProperty(FlexContainerInfo());
  if (info) {
    // We can reuse, as long as we clear out old data.
    info->mLines.Clear();
  } else {
    info = new ComputedFlexContainerInfo();
    SetProperty(FlexContainerInfo(), info);
  }

  return info;
}

void nsFlexContainerFrame::CreateFlexLineAndFlexItemInfo(
    ComputedFlexContainerInfo& aContainerInfo,
    const nsTArray<FlexLine>& aLines) {
  for (const FlexLine& line : aLines) {
    ComputedFlexLineInfo* lineInfo = aContainerInfo.mLines.AppendElement();
    // Most of the remaining lineInfo properties will be filled out in
    // UpdateFlexLineAndItemInfo (some will be provided by other functions),
    // when we have real values. But we still add all the items here, so
    // we can capture computed data for each item as we proceed.
    for (const FlexItem& item : line.Items()) {
      nsIFrame* frame = item.Frame();

      // The frame may be for an element, or it may be for an
      // anonymous flex item, e.g. wrapping one or more text nodes.
      // DevTools wants the content node for the actual child in
      // the DOM tree, so we descend through anonymous boxes.
      nsIFrame* targetFrame = GetFirstNonAnonBoxInSubtree(frame);
      nsIContent* content = targetFrame->GetContent();

      // Skip over content that is only whitespace, which might
      // have been broken off from a text node which is our real
      // target.
      while (content && content->TextIsOnlyWhitespace()) {
        // If content is only whitespace, try the frame sibling.
        targetFrame = targetFrame->GetNextSibling();
        if (targetFrame) {
          content = targetFrame->GetContent();
        } else {
          content = nullptr;
        }
      }

      ComputedFlexItemInfo* itemInfo = lineInfo->mItems.AppendElement();

      itemInfo->mNode = content;

      // itemInfo->mMainBaseSize and mMainDeltaSize will be filled out
      // in ResolveFlexibleLengths(). Other measurements will be captured in
      // UpdateFlexLineAndItemInfo.
    }
  }
}

void nsFlexContainerFrame::ComputeFlexDirections(
    ComputedFlexContainerInfo& aContainerInfo,
    const FlexboxAxisTracker& aAxisTracker) {
  auto ConvertPhysicalStartSideToFlexPhysicalDirection =
      [](mozilla::Side aStartSide) {
        switch (aStartSide) {
          case eSideLeft:
            return dom::FlexPhysicalDirection::Horizontal_lr;
          case eSideRight:
            return dom::FlexPhysicalDirection::Horizontal_rl;
          case eSideTop:
            return dom::FlexPhysicalDirection::Vertical_tb;
          case eSideBottom:
            return dom::FlexPhysicalDirection::Vertical_bt;
        }

        MOZ_ASSERT_UNREACHABLE("We should handle all sides!");
        return dom::FlexPhysicalDirection::Horizontal_lr;
      };

  aContainerInfo.mMainAxisDirection =
      ConvertPhysicalStartSideToFlexPhysicalDirection(
          aAxisTracker.MainAxisPhysicalStartSide());
  aContainerInfo.mCrossAxisDirection =
      ConvertPhysicalStartSideToFlexPhysicalDirection(
          aAxisTracker.CrossAxisPhysicalStartSide());
}

void nsFlexContainerFrame::UpdateFlexLineAndItemInfo(
    ComputedFlexContainerInfo& aContainerInfo,
    const nsTArray<FlexLine>& aLines) {
  uint32_t lineIndex = 0;
  for (const FlexLine& line : aLines) {
    ComputedFlexLineInfo& lineInfo = aContainerInfo.mLines[lineIndex];

    lineInfo.mCrossSize = line.LineCrossSize();
    lineInfo.mFirstBaselineOffset = line.FirstBaselineOffset();
    lineInfo.mLastBaselineOffset = line.LastBaselineOffset();

    uint32_t itemIndex = 0;
    for (const FlexItem& item : line.Items()) {
      ComputedFlexItemInfo& itemInfo = lineInfo.mItems[itemIndex];
      itemInfo.mFrameRect = item.Frame()->GetRect();
      itemInfo.mMainMinSize = item.MainMinSize();
      itemInfo.mMainMaxSize = item.MainMaxSize();
      itemInfo.mCrossMinSize = item.CrossMinSize();
      itemInfo.mCrossMaxSize = item.CrossMaxSize();
      itemInfo.mClampState =
          item.WasMinClamped()
              ? mozilla::dom::FlexItemClampState::Clamped_to_min
              : (item.WasMaxClamped()
                     ? mozilla::dom::FlexItemClampState::Clamped_to_max
                     : mozilla::dom::FlexItemClampState::Unclamped);
      ++itemIndex;
    }
    ++lineIndex;
  }
}

nsFlexContainerFrame* nsFlexContainerFrame::GetFlexFrameWithComputedInfo(
    nsIFrame* aFrame) {
  // Prepare a lambda function that we may need to call multiple times.
  auto GetFlexContainerFrame = [](nsIFrame* aFrame) {
    // Return the aFrame's content insertion frame, iff it is
    // a flex container frame.
    nsFlexContainerFrame* flexFrame = nullptr;

    if (aFrame) {
      nsIFrame* inner = aFrame;
      if (MOZ_UNLIKELY(aFrame->IsFieldSetFrame())) {
        inner = static_cast<nsFieldSetFrame*>(aFrame)->GetInner();
      }
      // Since "Get" methods like GetInner and GetContentInsertionFrame can
      // return null, we check the return values before dereferencing. Our
      // calling pattern makes this unlikely, but we're being careful.
      nsIFrame* insertionFrame =
          inner ? inner->GetContentInsertionFrame() : nullptr;
      nsIFrame* possibleFlexFrame = insertionFrame ? insertionFrame : aFrame;
      flexFrame = possibleFlexFrame->IsFlexContainerFrame()
                      ? static_cast<nsFlexContainerFrame*>(possibleFlexFrame)
                      : nullptr;
    }
    return flexFrame;
  };

  nsFlexContainerFrame* flexFrame = GetFlexContainerFrame(aFrame);
  if (flexFrame) {
    // Generate the FlexContainerInfo data, if it's not already there.
    bool reflowNeeded = !flexFrame->HasProperty(FlexContainerInfo());

    if (reflowNeeded) {
      // Trigger a reflow that generates additional flex property data.
      // Hold onto aFrame while we do this, in case reflow destroys it.
      AutoWeakFrame weakFrameRef(aFrame);

      RefPtr<mozilla::PresShell> presShell = flexFrame->PresShell();
      flexFrame->SetShouldGenerateComputedInfo(true);
      presShell->FrameNeedsReflow(flexFrame, IntrinsicDirty::Resize,
                                  NS_FRAME_IS_DIRTY);
      presShell->FlushPendingNotifications(FlushType::Layout);

      // Since the reflow may have side effects, get the flex frame
      // again. But if the weakFrameRef is no longer valid, then we
      // must bail out.
      if (!weakFrameRef.IsAlive()) {
        return nullptr;
      }

      flexFrame = GetFlexContainerFrame(weakFrameRef.GetFrame());

      NS_WARNING_ASSERTION(
          !flexFrame || flexFrame->HasProperty(FlexContainerInfo()),
          "The state bit should've made our forced-reflow "
          "generate a FlexContainerInfo object");
    }
  }
  return flexFrame;
}

/* static */
bool nsFlexContainerFrame::IsItemInlineAxisMainAxis(nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame && aFrame->IsFlexItem(), "expecting arg to be a flex item");
  const WritingMode flexItemWM = aFrame->GetWritingMode();
  const nsIFrame* flexContainer = aFrame->GetParent();

  if (IsLegacyBox(flexContainer)) {
    // For legacy boxes, the main axis is determined by "box-orient", and we can
    // just directly check if that's vertical, and compare that to whether the
    // item's WM is also vertical:
    bool boxOrientIsVertical =
        (flexContainer->StyleXUL()->mBoxOrient == StyleBoxOrient::Vertical);
    return flexItemWM.IsVertical() == boxOrientIsVertical;
  }

  // For modern CSS flexbox, we get our return value by asking two questions
  // and comparing their answers.
  // Question 1: does aFrame have the same inline axis as its flex container?
  bool itemInlineAxisIsParallelToParent =
      !flexItemWM.IsOrthogonalTo(flexContainer->GetWritingMode());

  // Question 2: is aFrame's flex container row-oriented? (This tells us
  // whether the flex container's main axis is its inline axis.)
  auto flexDirection = flexContainer->StylePosition()->mFlexDirection;
  bool flexContainerIsRowOriented =
      flexDirection == StyleFlexDirection::Row ||
      flexDirection == StyleFlexDirection::RowReverse;

  // aFrame's inline axis is its flex container's main axis IFF the above
  // questions have the same answer.
  return flexContainerIsRowOriented == itemInlineAxisIsParallelToParent;
}

/* static */
bool nsFlexContainerFrame::IsUsedFlexBasisContent(
    const StyleFlexBasis& aFlexBasis, const StyleSize& aMainSize) {
  // We have a used flex-basis of 'content' if flex-basis explicitly has that
  // value, OR if flex-basis is 'auto' (deferring to the main-size property)
  // and the main-size property is also 'auto'.
  // See https://drafts.csswg.org/css-flexbox-1/#valdef-flex-basis-auto
  if (aFlexBasis.IsContent()) {
    return true;
  }
  return aFlexBasis.IsAuto() && aMainSize.IsAuto();
}

nsFlexContainerFrame::FlexLayoutResult nsFlexContainerFrame::DoFlexLayout(
    const ReflowInput& aReflowInput, const nscoord aTentativeContentBoxMainSize,
    const nscoord aTentativeContentBoxCrossSize,
    const FlexboxAxisTracker& aAxisTracker, nscoord aMainGapSize,
    nscoord aCrossGapSize, bool aHasLineClampEllipsis,
    nsTArray<StrutInfo>& aStruts,
    ComputedFlexContainerInfo* const aContainerInfo) {
  FlexLayoutResult flr;

  GenerateFlexLines(aReflowInput, aTentativeContentBoxMainSize,
                    aTentativeContentBoxCrossSize, aStruts, aAxisTracker,
                    aMainGapSize, aHasLineClampEllipsis, flr.mPlaceholders,
                    flr.mLines);

  if ((flr.mLines.Length() == 1 && flr.mLines[0].IsEmpty()) ||
      aReflowInput.mStyleDisplay->IsContainLayout()) {
    // We have no flex items, or we're layout-contained. So, we have no
    // baseline, and our parent should synthesize a baseline if needed.
    AddStateBits(NS_STATE_FLEX_SYNTHESIZE_BASELINE);
  } else {
    RemoveStateBits(NS_STATE_FLEX_SYNTHESIZE_BASELINE);
  }

  // Construct our computed info if we've been asked to do so. This is
  // necessary to do now so we can capture some computed values for
  // FlexItems during layout that would not otherwise be saved (like
  // size adjustments). We'll later fix up the line properties,
  // because the correct values aren't available yet.
  if (aContainerInfo) {
    MOZ_ASSERT(ShouldGenerateComputedInfo(),
               "We should only have the info struct if "
               "ShouldGenerateComputedInfo() is true!");

    if (!aStruts.IsEmpty()) {
      // We restarted DoFlexLayout, and may have stale mLines to clear:
      aContainerInfo->mLines.Clear();
    } else {
      MOZ_ASSERT(aContainerInfo->mLines.IsEmpty(), "Shouldn't have lines yet.");
    }

    CreateFlexLineAndFlexItemInfo(*aContainerInfo, flr.mLines);
    ComputeFlexDirections(*aContainerInfo, aAxisTracker);
  }

  flr.mContentBoxMainSize = ComputeMainSize(
      aReflowInput, aAxisTracker, aTentativeContentBoxMainSize, flr.mLines);

  uint32_t lineIndex = 0;
  for (FlexLine& line : flr.mLines) {
    ComputedFlexLineInfo* lineInfo =
        aContainerInfo ? &aContainerInfo->mLines[lineIndex] : nullptr;
    line.ResolveFlexibleLengths(flr.mContentBoxMainSize, lineInfo);
    ++lineIndex;
  }

  // Cross Size Determination - Flexbox spec section 9.4
  // https://drafts.csswg.org/css-flexbox-1/#cross-sizing
  // ===================================================
  // Calculate the hypothetical cross size of each item:

  // 'sumLineCrossSizes' includes the size of all gaps between lines. We
  // initialize it with the sum of all the gaps, and add each line's cross size
  // at the end of the following for-loop.
  nscoord sumLineCrossSizes = aCrossGapSize * (flr.mLines.Length() - 1);
  for (FlexLine& line : flr.mLines) {
    for (FlexItem& item : line.Items()) {
      // The item may already have the correct cross-size; only recalculate
      // if the item's main size resolution (flexing) could have influenced it:
      if (item.CanMainSizeInfluenceCrossSize()) {
        StyleSizeOverrides sizeOverrides;
        if (item.IsInlineAxisMainAxis()) {
          sizeOverrides.mStyleISize.emplace(item.StyleMainSize());
        } else {
          sizeOverrides.mStyleBSize.emplace(item.StyleMainSize());
        }
        FLEX_LOG("Sizing flex item %p in cross axis", item.Frame());
        FLEX_LOGV(" Main size override: %d", item.MainSize());

        const WritingMode wm = item.GetWritingMode();
        LogicalSize availSize = aReflowInput.ComputedSize(wm);
        availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
        ReflowInput childReflowInput(PresContext(), aReflowInput, item.Frame(),
                                     availSize, Nothing(), {}, sizeOverrides);
        childReflowInput.mFlags.mInsideLineClamp = GetLineClampValue() != 0;
        if (item.IsBlockAxisMainAxis() && item.TreatBSizeAsIndefinite()) {
          childReflowInput.mFlags.mTreatBSizeAsIndefinite = true;
        }

        SizeItemInCrossAxis(childReflowInput, item);
      }
    }
    // Now that we've finished with this line's items, size the line itself:
    line.ComputeCrossSizeAndBaseline(aAxisTracker);
    sumLineCrossSizes += line.LineCrossSize();
  }

  bool isCrossSizeDefinite;
  flr.mContentBoxCrossSize = ComputeCrossSize(
      aReflowInput, aAxisTracker, aTentativeContentBoxCrossSize,
      sumLineCrossSizes, &isCrossSizeDefinite);

  // Set up state for cross-axis alignment, at a high level (outside the
  // scope of a particular flex line)
  CrossAxisPositionTracker crossAxisPosnTracker(
      flr.mLines, aReflowInput, flr.mContentBoxCrossSize, isCrossSizeDefinite,
      aAxisTracker, aCrossGapSize);

  // Now that we know the cross size of each line (including
  // "align-content:stretch" adjustments, from the CrossAxisPositionTracker
  // constructor), we can create struts for any flex items with
  // "visibility: collapse" (and restart flex layout).
  if (aStruts.IsEmpty() &&  // (Don't make struts if we already did)
      !ShouldUseMozBoxCollapseBehavior(aReflowInput.mStyleDisplay)) {
    BuildStrutInfoFromCollapsedItems(flr.mLines, aStruts);
    if (!aStruts.IsEmpty()) {
      // Restart flex layout, using our struts.
      return flr;
    }
  }

  // If the container should derive its baseline from the first FlexLine,
  // do that here (while crossAxisPosnTracker is conveniently pointing
  // at the cross-start edge of that line, which the line's baseline offset is
  // measured from):
  if (nscoord firstLineBaselineOffset = flr.mLines[0].FirstBaselineOffset();
      firstLineBaselineOffset == nscoord_MIN) {
    // No baseline-aligned items in line. Use sentinel value to prompt us to
    // get baseline from the first FlexItem after we've reflowed it.
    flr.mAscent = nscoord_MIN;
  } else {
    flr.mAscent = ComputePhysicalAscentFromFlexRelativeAscent(
        crossAxisPosnTracker.Position() + firstLineBaselineOffset,
        flr.mContentBoxCrossSize, aReflowInput, aAxisTracker);
  }

  const auto justifyContent =
      IsLegacyBox(aReflowInput.mFrame)
          ? ConvertLegacyStyleToJustifyContent(StyleXUL())
          : aReflowInput.mStylePosition->mJustifyContent;

  lineIndex = 0;
  for (FlexLine& line : flr.mLines) {
    // Main-Axis Alignment - Flexbox spec section 9.5
    // https://drafts.csswg.org/css-flexbox-1/#main-alignment
    // ==============================================
    line.PositionItemsInMainAxis(justifyContent, flr.mContentBoxMainSize,
                                 aAxisTracker);

    // See if we need to extract some computed info for this line.
    if (MOZ_UNLIKELY(aContainerInfo)) {
      ComputedFlexLineInfo& lineInfo = aContainerInfo->mLines[lineIndex];
      lineInfo.mCrossStart = crossAxisPosnTracker.Position();
    }

    // Cross-Axis Alignment - Flexbox spec section 9.6
    // https://drafts.csswg.org/css-flexbox-1/#cross-alignment
    // ===============================================
    line.PositionItemsInCrossAxis(crossAxisPosnTracker.Position(),
                                  aAxisTracker);
    crossAxisPosnTracker.TraverseLine(line);
    crossAxisPosnTracker.TraversePackingSpace();

    if (&line != &flr.mLines.LastElement()) {
      crossAxisPosnTracker.TraverseGap();
    }
    ++lineIndex;
  }

  return flr;
}

std::tuple<nscoord, bool> nsFlexContainerFrame::ReflowChildren(
    const ReflowInput& aReflowInput, const nsSize& aContainerSize,
    const LogicalSize& aAvailableSizeForItems,
    const LogicalMargin& aBorderPadding,
    const nscoord aSumOfPrevInFlowsChildrenBlockSize,
    const FlexboxAxisTracker& aAxisTracker, bool aHasLineClampEllipsis,
    FlexLayoutResult& aFlr) {
  // Before giving each child a final reflow, calculate the origin of the
  // flex container's content box (with respect to its border-box), so that
  // we can compute our flex item's final positions.
  WritingMode flexWM = aReflowInput.GetWritingMode();
  const LogicalPoint containerContentBoxOrigin(
      flexWM, aBorderPadding.IStart(flexWM), aBorderPadding.BStart(flexWM));

  // If the flex container has no baseline-aligned items, it will use the first
  // item to determine its baseline:
  const FlexItem* firstItem =
      aFlr.mLines[0].IsEmpty() ? nullptr : &aFlr.mLines[0].FirstItem();

  // The block-end of children is relative to the flex container's border-box.
  nscoord maxBlockEndEdgeOfChildren = containerContentBoxOrigin.B(flexWM);

  FrameHashtable pushedItems;
  FrameHashtable incompleteItems;
  FrameHashtable overflowIncompleteItems;

  // FINAL REFLOW: Give each child frame another chance to reflow, now that
  // we know its final size and position.
  for (const FlexLine& line : aFlr.mLines) {
    for (const FlexItem& item : line.Items()) {
      LogicalPoint framePos = aAxisTracker.LogicalPointFromFlexRelativePoint(
          item.MainPosition(), item.CrossPosition(), aFlr.mContentBoxMainSize,
          aFlr.mContentBoxCrossSize);

      if (item.Frame()->GetPrevInFlow()) {
        // The item is a continuation. Lay it out at the beginning of the
        // available space.
        framePos.B(flexWM) = 0;
      } else {
        // We haven't laid the item out. Subtract its block-direction position
        // by the sum of our prev-in-flows' content block-end.
        framePos.B(flexWM) -= aSumOfPrevInFlowsChildrenBlockSize;
      }

      // Adjust available block-size for the item. (We compute it here because
      // framePos is still relative to the container's content-box.)
      //
      // Note: The available block-size can become negative if item's
      // block-direction position is below available space's block-end.
      const nscoord availableBSizeForItem =
          aAvailableSizeForItems.BSize(flexWM) == NS_UNCONSTRAINEDSIZE
              ? NS_UNCONSTRAINEDSIZE
              : aAvailableSizeForItems.BSize(flexWM) - framePos.B(flexWM);

      // Adjust framePos to be relative to the container's border-box
      // (i.e. its frame rect), instead of the container's content-box:
      framePos += containerContentBoxOrigin;

      // (Intentionally snapshotting this before ApplyRelativePositioning, to
      // maybe use for setting the flex container's baseline.)
      const nscoord itemNormalBPos = framePos.B(flexWM);

      // Check if we actually need to reflow the item -- if the item's position
      // is below the available space's block-end, push it to our next-in-flow;
      // if it does need a reflow, and we already reflowed it with the right
      // content-box size, and there is no need to do a reflow to clear out a
      // -webkit-line-clamp ellipsis, we can just reposition it as-needed.
      const bool childBPosExceedAvailableSpaceBEnd =
          availableBSizeForItem != NS_UNCONSTRAINEDSIZE &&
          availableBSizeForItem <= 0;
      if (childBPosExceedAvailableSpaceBEnd) {
        // Note: Even if all of our items are beyond the available space & get
        // pushed here, we'll be guaranteed to place at least one of them (and
        // make progress) in one of the flex container's *next* fragment. It's
        // because ComputeAvailableSizeForItems() always reserves at least 1px
        // available block-size for its children, and we consume all available
        // block-size and add it to SumOfChildrenBlockSizeProperty even if we
        // are not laying out any child.
        FLEX_LOG(
            "[frag] Flex item %p needed to be pushed to container's "
            "next-in-flow due to position below available space's block-end",
            item.Frame());
        pushedItems.Insert(item.Frame());
      } else if (item.NeedsFinalReflow(availableBSizeForItem)) {
        // The available size must be in item's writing-mode.
        const WritingMode itemWM = item.GetWritingMode();
        const auto availableSize =
            LogicalSize(flexWM, aAvailableSizeForItems.ISize(flexWM),
                        availableBSizeForItem)
                .ConvertTo(itemWM, flexWM);

        const nsReflowStatus childReflowStatus = ReflowFlexItem(
            aAxisTracker, aReflowInput, item, framePos, availableSize,
            aContainerSize, aHasLineClampEllipsis);

        if (childReflowStatus.IsIncomplete()) {
          incompleteItems.Insert(item.Frame());
        } else if (childReflowStatus.IsOverflowIncomplete()) {
          overflowIncompleteItems.Insert(item.Frame());
        }
      } else {
        MoveFlexItemToFinalPosition(item, framePos, aContainerSize);
        // We didn't perform a final reflow of the item. If we still have a
        // -webkit-line-clamp ellipsis hanging around, but we shouldn't have
        // one any more, we need to clear that now.  Technically, we only need
        // to do this if we *didn't* do a bsize measuring reflow of the item
        // earlier (since that is normally when we deal with -webkit-line-clamp
        // ellipses) but not all flex items need such a reflow.
        // XXXdholbert This comment implies that we could skip this if
        // HadMeasuringReflow() is true.  Maybe we should try doing that?
        if (aHasLineClampEllipsis && GetLineClampValue() == 0) {
          item.BlockFrame()->ClearLineClampEllipsis();
        }
      }

      if (!childBPosExceedAvailableSpaceBEnd) {
        // The item (or a fragment thereof) was placed in this flex container
        // fragment. Update the max block-end edge with the item's block-end
        // edge.
        maxBlockEndEdgeOfChildren =
            std::max(maxBlockEndEdgeOfChildren,
                     itemNormalBPos + item.Frame()->BSize(flexWM));
      }

      // If the item has auto margins, and we were tracking the UsedMargin
      // property, set the property to the computed margin values.
      if (item.HasAnyAutoMargin()) {
        nsMargin* propValue =
            item.Frame()->GetProperty(nsIFrame::UsedMarginProperty());
        if (propValue) {
          *propValue = item.PhysicalMargin();
        }
      }

      // If this is our first item and we haven't established a baseline for
      // the container yet (i.e. if we don't have 'align-self: baseline' on any
      // children), then use this child's first baseline as the container's
      // baseline.
      if (&item == firstItem && aFlr.mAscent == nscoord_MIN) {
        aFlr.mAscent = itemNormalBPos + item.ResolvedAscent(true);
      }
    }
  }

  if (!aFlr.mPlaceholders.IsEmpty()) {
    ReflowPlaceholders(aReflowInput, aFlr.mPlaceholders,
                       containerContentBoxOrigin, aContainerSize);
  }

  const bool anyChildIncomplete = PushIncompleteChildren(
      pushedItems, incompleteItems, overflowIncompleteItems);

  if (!pushedItems.IsEmpty()) {
    AddStateBits(NS_STATE_FLEX_DID_PUSH_ITEMS);
  }

  return {maxBlockEndEdgeOfChildren, anyChildIncomplete};
}

void nsFlexContainerFrame::PopulateReflowOutput(
    ReflowOutput& aReflowOutput, const ReflowInput& aReflowInput,
    nsReflowStatus& aStatus, const LogicalSize& aContentBoxSize,
    const LogicalMargin& aBorderPadding, const nscoord aConsumedBSize,
    const bool aMayNeedNextInFlow, const nscoord aMaxBlockEndEdgeOfChildren,
    const bool aAnyChildIncomplete, nscoord aFlexContainerAscent,
    nsTArray<FlexLine>& aLines, const FlexboxAxisTracker& aAxisTracker) {
  const WritingMode flexWM = aReflowInput.GetWritingMode();

  // Compute flex container's desired size (in its own writing-mode).
  LogicalSize desiredSizeInFlexWM(flexWM);
  desiredSizeInFlexWM.ISize(flexWM) =
      aContentBoxSize.ISize(flexWM) + aBorderPadding.IStartEnd(flexWM);

  // Unconditionally skip adding block-end border and padding for now. We add it
  // lower down, after we've established baseline and decided whether bottom
  // border-padding fits (if we're fragmented).
  const nscoord effectiveContentBSizeWithBStartBP =
      aContentBoxSize.BSize(flexWM) - aConsumedBSize +
      aBorderPadding.BStart(flexWM);
  nscoord blockEndContainerBP = aBorderPadding.BEnd(flexWM);

  if (aMayNeedNextInFlow) {
    // We assume our status should be reported as incomplete because we may need
    // a next-in-flow.
    bool isStatusIncomplete = true;

    const nscoord availableBSizeMinusBEndBP =
        aReflowInput.AvailableBSize() - aBorderPadding.BEnd(flexWM);

    if (aMaxBlockEndEdgeOfChildren <= availableBSizeMinusBEndBP) {
      // Consume all the available block-size.
      desiredSizeInFlexWM.BSize(flexWM) = availableBSizeMinusBEndBP;
    } else {
      // This case happens if we have some tall unbreakable children exceeding
      // the available block-size.
      desiredSizeInFlexWM.BSize(flexWM) = std::min(
          effectiveContentBSizeWithBStartBP, aMaxBlockEndEdgeOfChildren);

      if (aMaxBlockEndEdgeOfChildren >= effectiveContentBSizeWithBStartBP) {
        // Some unbreakable children force us to consume all of our content
        // block-size, and make us complete.
        isStatusIncomplete = false;

        // We also potentially need to get the unskipped block-end border and
        // padding (if we assumed it'd be skipped as part of our tentative
        // assumption that we'd be complete).
        if (aReflowInput.mStyleBorder->mBoxDecorationBreak ==
            StyleBoxDecorationBreak::Slice) {
          blockEndContainerBP =
              aReflowInput.ComputedLogicalBorderPadding(flexWM).BEnd(flexWM);
        }
      }
    }

    if (isStatusIncomplete) {
      aStatus.SetIncomplete();
    }
  } else {
    // Our own effective content-box block-size can fit within the available
    // block-size.
    desiredSizeInFlexWM.BSize(flexWM) = effectiveContentBSizeWithBStartBP;
  }

  if (aFlexContainerAscent == nscoord_MIN) {
    // Still don't have our baseline set -- this happens if we have no
    // children (or if our children are huge enough that they have nscoord_MIN
    // as their baseline... in which case, we'll use the wrong baseline, but no
    // big deal)
    NS_WARNING_ASSERTION(
        aLines[0].IsEmpty(),
        "Have flex items but didn't get an ascent - that's odd (or there are "
        "just gigantic sizes involved)");
    // Per spec, synthesize baseline from the flex container's content box
    // (i.e. use block-end side of content-box)
    // XXXdholbert This only makes sense if parent's writing mode is
    // horizontal (& even then, really we should be using the BSize in terms
    // of the parent's writing mode, not ours). Clean up in bug 1155322.
    aFlexContainerAscent = desiredSizeInFlexWM.BSize(flexWM);
  }

  if (HasAnyStateBits(NS_STATE_FLEX_SYNTHESIZE_BASELINE)) {
    // This will force our parent to call GetLogicalBaseline, which will
    // synthesize a margin-box baseline.
    aReflowOutput.SetBlockStartAscent(ReflowOutput::ASK_FOR_BASELINE);
  } else {
    // XXXdholbert aFlexContainerAscent needs to be in terms of
    // our parent's writing-mode here. See bug 1155322.
    aReflowOutput.SetBlockStartAscent(aFlexContainerAscent);
  }

  // Now, we account for how the block-end border and padding (if any) impacts
  // our desired size. If adding it pushes us over the available block-size,
  // then we become incomplete (unless we already weren't asking for any
  // block-size, in which case we stay complete to avoid looping forever).
  //
  // NOTE: If we have auto block-size, we allow our block-end border and padding
  // to push us over the available block-size without requesting a continuation,
  // for consistency with the behavior of "display:block" elements.
  const nscoord effectiveContentBSizeWithBStartEndBP =
      desiredSizeInFlexWM.BSize(flexWM) + blockEndContainerBP;

  if (aReflowInput.AvailableBSize() != NS_UNCONSTRAINEDSIZE &&
      effectiveContentBSizeWithBStartEndBP > aReflowInput.AvailableBSize() &&
      desiredSizeInFlexWM.BSize(flexWM) != 0 &&
      aReflowInput.ComputedBSize() != NS_UNCONSTRAINEDSIZE) {
    // We couldn't fit with the block-end border and padding included, so we'll
    // need a continuation.
    aStatus.SetIncomplete();

    if (aReflowInput.mStyleBorder->mBoxDecorationBreak ==
        StyleBoxDecorationBreak::Slice) {
      blockEndContainerBP = 0;
    }
  }

  // The variable "blockEndContainerBP" now accurately reflects how much (if
  // any) block-end border and padding we want for this frame, so we can proceed
  // to add it in.
  desiredSizeInFlexWM.BSize(flexWM) += blockEndContainerBP;

  if (aStatus.IsComplete() && aAnyChildIncomplete) {
    aStatus.SetOverflowIncomplete();
    aStatus.SetNextInFlowNeedsReflow();
  }

  // If we are the first-in-flow and not fully complete (either our block-size
  // or any of our flex items cannot fit in the available block-size), and the
  // style requires us to avoid breaking inside, set the status to prompt our
  // parent to push us to the next page/column.
  if (!GetPrevInFlow() && !aStatus.IsFullyComplete() &&
      ShouldAvoidBreakInside(aReflowInput)) {
    aStatus.SetInlineLineBreakBeforeAndReset();
    return;
  }

  // Calculate the container baselines so that our parent can baseline-align us.
  mBaselineFromLastReflow = aFlexContainerAscent;
  mLastBaselineFromLastReflow = aLines.LastElement().LastBaselineOffset();
  if (mLastBaselineFromLastReflow == nscoord_MIN) {
    // XXX we fall back to a mirrored first baseline here for now, but this
    // should probably use the last baseline of the last item or something.
    mLastBaselineFromLastReflow =
        desiredSizeInFlexWM.BSize(flexWM) - aFlexContainerAscent;
  }

  // Convert flex container's final desired size to parent's WM, for outparam.
  aReflowOutput.SetSize(flexWM, desiredSizeInFlexWM);
}

void nsFlexContainerFrame::MoveFlexItemToFinalPosition(
    const FlexItem& aItem, LogicalPoint& aFramePos,
    const nsSize& aContainerSize) {
  const WritingMode outerWM = aItem.ContainingBlockWM();
  const nsStyleDisplay* display = aItem.Frame()->StyleDisplay();
  if (display->IsRelativelyOrStickyPositionedStyle()) {
    // If the item is relatively positioned, look up its offsets (cached from
    // previous reflow). A sticky positioned item can pass a dummy
    // logicalOffsets into ApplyRelativePositioning().
    LogicalMargin logicalOffsets(outerWM);
    if (display->IsRelativelyPositionedStyle()) {
      nsMargin* cachedOffsets =
          aItem.Frame()->GetProperty(nsIFrame::ComputedOffsetProperty());
      MOZ_ASSERT(
          cachedOffsets,
          "relpos previously-reflowed frame should've cached its offsets");
      logicalOffsets = LogicalMargin(outerWM, *cachedOffsets);
    }
    ReflowInput::ApplyRelativePositioning(
        aItem.Frame(), outerWM, logicalOffsets, &aFramePos, aContainerSize);
  }

  FLEX_LOG("Moving flex item %p to its desired position %s", aItem.Frame(),
           ToString(aFramePos).c_str());
  aItem.Frame()->SetPosition(outerWM, aFramePos, aContainerSize);
  PositionFrameView(aItem.Frame());
  PositionChildViews(aItem.Frame());
}

nsReflowStatus nsFlexContainerFrame::ReflowFlexItem(
    const FlexboxAxisTracker& aAxisTracker, const ReflowInput& aReflowInput,
    const FlexItem& aItem, LogicalPoint& aFramePos,
    const LogicalSize& aAvailableSize, const nsSize& aContainerSize,
    bool aHasLineClampEllipsis) {
  FLEX_LOG("Doing final reflow for flex item %p", aItem.Frame());

  WritingMode outerWM = aReflowInput.GetWritingMode();

  StyleSizeOverrides sizeOverrides;
  // Override flex item's main size.
  if (aItem.IsInlineAxisMainAxis()) {
    sizeOverrides.mStyleISize.emplace(aItem.StyleMainSize());
  } else {
    sizeOverrides.mStyleBSize.emplace(aItem.StyleMainSize());
  }
  FLEX_LOGV(" Main size override: %d", aItem.MainSize());

  // Override flex item's cross size if it was stretched in the cross axis (in
  // which case we're imposing a cross size).
  if (aItem.IsStretched()) {
    if (aItem.IsInlineAxisCrossAxis()) {
      sizeOverrides.mStyleISize.emplace(aItem.StyleCrossSize());
    } else {
      sizeOverrides.mStyleBSize.emplace(aItem.StyleCrossSize());
    }
    FLEX_LOGV(" Cross size override: %d", aItem.CrossSize());
  }
  if (sizeOverrides.mStyleBSize) {
    // We are overriding the block-size. For robustness, we always assume that
    // this represents a block-axis resize for the frame. This may be
    // conservative, but we do capture all the conditions in the block-axis
    // (checked in NeedsFinalReflow()) that make this item require a final
    // reflow. This sets relevant flags in ReflowInput::InitResizeFlags().
    aItem.Frame()->SetHasBSizeChange(true);
  }

  ReflowInput childReflowInput(PresContext(), aReflowInput, aItem.Frame(),
                               aAvailableSize, Nothing(), {}, sizeOverrides);
  childReflowInput.mFlags.mInsideLineClamp = GetLineClampValue() != 0;
  // This is the final reflow of this flex item; if we previously had a
  // -webkit-line-clamp, and we missed our chance to clear the ellipsis
  // because we didn't need to call MeasureFlexItemContentBSize, we set
  // mApplyLineClamp to cause it to get cleared here.
  childReflowInput.mFlags.mApplyLineClamp =
      !childReflowInput.mFlags.mInsideLineClamp && aHasLineClampEllipsis;

  if (aItem.TreatBSizeAsIndefinite() && aItem.IsBlockAxisMainAxis()) {
    childReflowInput.mFlags.mTreatBSizeAsIndefinite = true;
  }

  if (aItem.IsStretched() && aItem.IsBlockAxisCrossAxis()) {
    // This item is stretched (in the cross axis), and that axis is its block
    // axis.  That stretching effectively gives it a relative BSize.
    // XXXdholbert This flag only makes a difference if we use the flex items'
    // frame-state when deciding whether to reflow them -- and we don't, as of
    // the changes in bug 851607. So this has no effect right now, but it might
    // make a difference if we optimize to use dirty bits in the
    // future. (Reftests flexbox-resizeviewport-1.xhtml and -2.xhtml are
    // intended to catch any regressions here, if we end up relying on this bit
    // & neglecting to set it.)
    aItem.Frame()->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
  }

  // NOTE: Be very careful about doing anything else with childReflowInput
  // after this point, because some of its methods (e.g. SetComputedWidth)
  // internally call InitResizeFlags and stomp on mVResize & mHResize.

  // CachedFlexItemData is stored in item's writing mode, so we pass
  // aChildReflowInput into ReflowOutput's constructor.
  ReflowOutput childReflowOutput(childReflowInput);
  nsReflowStatus childReflowStatus;
  ReflowChild(aItem.Frame(), PresContext(), childReflowOutput, childReflowInput,
              outerWM, aFramePos, aContainerSize, ReflowChildFlags::Default,
              childReflowStatus);

  // XXXdholbert Perhaps we should call CheckForInterrupt here; see bug 1495532.

  FinishReflowChild(aItem.Frame(), PresContext(), childReflowOutput,
                    &childReflowInput, outerWM, aFramePos, aContainerSize,
                    ReflowChildFlags::ApplyRelativePositioning);

  aItem.SetAscent(childReflowOutput.BlockStartAscent());

  // Update our cached flex item info:
  if (auto* cached = aItem.Frame()->GetProperty(CachedFlexItemData::Prop())) {
    cached->Update(childReflowInput, childReflowOutput,
                   FlexItemReflowType::Final);
  } else {
    cached = new CachedFlexItemData(childReflowInput, childReflowOutput,
                                    FlexItemReflowType::Final);
    aItem.Frame()->SetProperty(CachedFlexItemData::Prop(), cached);
  }

  return childReflowStatus;
}

void nsFlexContainerFrame::ReflowPlaceholders(
    const ReflowInput& aReflowInput, nsTArray<nsIFrame*>& aPlaceholders,
    const LogicalPoint& aContentBoxOrigin, const nsSize& aContainerSize) {
  WritingMode outerWM = aReflowInput.GetWritingMode();

  // As noted in this method's documentation, we'll reflow every entry in
  // |aPlaceholders| at the container's content-box origin.
  for (nsIFrame* placeholder : aPlaceholders) {
    MOZ_ASSERT(placeholder->IsPlaceholderFrame(),
               "placeholders array should only contain placeholder frames");
    WritingMode wm = placeholder->GetWritingMode();
    LogicalSize availSize = aReflowInput.ComputedSize(wm);
    ReflowInput childReflowInput(PresContext(), aReflowInput, placeholder,
                                 availSize);
    // No need to set the -webkit-line-clamp related flags when reflowing
    // a placeholder.
    ReflowOutput childReflowOutput(outerWM);
    nsReflowStatus childReflowStatus;
    ReflowChild(placeholder, PresContext(), childReflowOutput, childReflowInput,
                outerWM, aContentBoxOrigin, aContainerSize,
                ReflowChildFlags::Default, childReflowStatus);

    FinishReflowChild(placeholder, PresContext(), childReflowOutput,
                      &childReflowInput, outerWM, aContentBoxOrigin,
                      aContainerSize, ReflowChildFlags::Default);

    // Mark the placeholder frame to indicate that it's not actually at the
    // element's static position, because we need to apply CSS Alignment after
    // we determine the OOF's size:
    placeholder->AddStateBits(PLACEHOLDER_STATICPOS_NEEDS_CSSALIGN);
  }
}

nscoord nsFlexContainerFrame::IntrinsicISize(gfxContext* aRenderingContext,
                                             IntrinsicISizeType aType) {
  nscoord containerISize = 0;
  const nsStylePosition* stylePos = StylePosition();
  const FlexboxAxisTracker axisTracker(this);

  nscoord mainGapSize;
  if (axisTracker.IsRowOriented()) {
    mainGapSize = nsLayoutUtils::ResolveGapToLength(stylePos->mColumnGap,
                                                    NS_UNCONSTRAINEDSIZE);
  } else {
    mainGapSize = nsLayoutUtils::ResolveGapToLength(stylePos->mRowGap,
                                                    NS_UNCONSTRAINEDSIZE);
  }

  const bool useMozBoxCollapseBehavior =
      ShouldUseMozBoxCollapseBehavior(StyleDisplay());

  // The loop below sets aside space for a gap before each item besides the
  // first. This bool helps us handle that special-case.
  bool onFirstChild = true;

  for (nsIFrame* childFrame : mFrames) {
    // Skip out-of-flow children because they don't participate in flex layout.
    if (childFrame->IsPlaceholderFrame()) {
      continue;
    }

    // If we're using legacy "visibility:collapse" behavior, then we don't
    // care about the sizes of any collapsed children.
    if (!useMozBoxCollapseBehavior ||
        (StyleVisibility::Collapse !=
         childFrame->StyleVisibility()->mVisible)) {
      nscoord childISize = nsLayoutUtils::IntrinsicForContainer(
          aRenderingContext, childFrame, aType);
      // * For a row-oriented single-line flex container, the intrinsic
      // {min/pref}-isize is the sum of its items' {min/pref}-isizes and
      // (n-1) column gaps.
      // * For a column-oriented flex container, the intrinsic min isize
      // is the max of its items' min isizes.
      // * For a row-oriented multi-line flex container, the intrinsic
      // pref isize is former (sum), and its min isize is the latter (max).
      bool isSingleLine = (StyleFlexWrap::Nowrap == stylePos->mFlexWrap);
      if (axisTracker.IsRowOriented() &&
          (isSingleLine || aType == IntrinsicISizeType::PrefISize)) {
        containerISize += childISize;
        if (!onFirstChild) {
          containerISize += mainGapSize;
        }
        onFirstChild = false;
      } else {  // (col-oriented, or MinISize for multi-line row flex container)
        containerISize = std::max(containerISize, childISize);
      }
    }
  }

  return containerISize;
}

/* virtual */
nscoord nsFlexContainerFrame::GetMinISize(gfxContext* aRenderingContext) {
  DISPLAY_MIN_INLINE_SIZE(this, mCachedMinISize);
  if (mCachedMinISize == NS_INTRINSIC_ISIZE_UNKNOWN) {
    mCachedMinISize =
        StyleDisplay()->GetContainSizeAxes().mIContained
            ? 0
            : IntrinsicISize(aRenderingContext, IntrinsicISizeType::MinISize);
  }

  return mCachedMinISize;
}

/* virtual */
nscoord nsFlexContainerFrame::GetPrefISize(gfxContext* aRenderingContext) {
  DISPLAY_PREF_INLINE_SIZE(this, mCachedPrefISize);
  if (mCachedPrefISize == NS_INTRINSIC_ISIZE_UNKNOWN) {
    mCachedPrefISize =
        StyleDisplay()->GetContainSizeAxes().mIContained
            ? 0
            : IntrinsicISize(aRenderingContext, IntrinsicISizeType::PrefISize);
  }

  return mCachedPrefISize;
}

uint32_t nsFlexContainerFrame::GetLineClampValue() const {
  // -webkit-line-clamp should only work on items in flex containers that are
  // display:-webkit-(inline-)box and -webkit-box-orient:vertical.
  //
  // This check makes -webkit-line-clamp work on display:-moz-box too, but
  // that shouldn't be a big deal.
  if (!HasAnyStateBits(NS_STATE_FLEX_IS_EMULATING_LEGACY_BOX) ||
      StyleXUL()->mBoxOrient != StyleBoxOrient::Vertical) {
    return 0;
  }

  return StyleDisplay()->mLineClamp;
}
