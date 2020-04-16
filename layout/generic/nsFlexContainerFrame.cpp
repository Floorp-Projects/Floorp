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

// XXXdholbert Some of this helper-stuff should be separated out into a general
// "main/cross-axis utils" header, shared by grid & flexbox?
// (Particularly when grid gets support for align-*/justify-* properties.)

// Helper structs / classes / methods
// ==================================
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

// Returns the OrderingProperty enum that we should pass to
// CSSOrderAwareFrameIterator (depending on whether it's a legacy box).
static CSSOrderAwareFrameIterator::OrderingProperty OrderingPropertyForIter(
    const nsFlexContainerFrame* aFlexContainer) {
  return IsLegacyBox(aFlexContainer)
             ? CSSOrderAwareFrameIterator::OrderingProperty::eUseBoxOrdinalGroup
             : CSSOrderAwareFrameIterator::OrderingProperty::eUseOrder;
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

// Helper-function to find the first non-anonymous-box descendent of aFrame.
static nsIFrame* GetFirstNonAnonBoxDescendant(nsIFrame* aFrame) {
  while (aFrame) {
    // If aFrame isn't an anonymous container, or it's text or such, then it'll
    // do.
    if (!aFrame->Style()->IsAnonBox() ||
        nsCSSAnonBoxes::IsNonElement(aFrame->Style()->GetPseudoType())) {
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
    if (MOZ_UNLIKELY(aFrame->IsTableWrapperFrame())) {
      nsIFrame* captionDescendant = GetFirstNonAnonBoxDescendant(
          aFrame->GetChildList(kCaptionList).FirstChild());
      if (captionDescendant) {
        return captionDescendant;
      }
    } else if (MOZ_UNLIKELY(aFrame->IsTableFrame())) {
      nsIFrame* colgroupDescendant = GetFirstNonAnonBoxDescendant(
          aFrame->GetChildList(kColGroupList).FirstChild());
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

// Add two nscoord values, using CheckedInt to handle integer overflow.
// This function returns the sum of its two args -- but if we trigger integer
// overflow while adding them, then this function returns nscoord_MAX instead.
static nscoord AddChecked(nscoord aFirst, nscoord aSecond) {
  CheckedInt<nscoord> checkedResult = CheckedInt<nscoord>(aFirst) + aSecond;
  return checkedResult.isValid() ? checkedResult.value() : nscoord_MAX;
}

// Check if the size is auto or it is a keyword in the block axis.
// |aIsInline| should represent whether aSize is in the inline axis, from the
// perspective of the writing mode of the flex item that the size comes from.
//
// max-content and min-content should behave as property's initial value.
// Bug 567039: We treat -moz-fit-content and -moz-available as property's
// initial value for now.
static inline bool IsAutoOrEnumOnBSize(const StyleSize& aSize, bool aIsInline) {
  return aSize.IsAuto() || (!aIsInline && aSize.IsExtremumLength());
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
  wm_.IsOrthogonalTo((axisTracker_).GetWritingMode()) !=              \
          (axisTracker_).IsRowOriented()                              \
      ? (isize_)                                                      \
      : (bsize_)

#define GET_CROSS_COMPONENT_LOGICAL(axisTracker_, wm_, isize_, bsize_) \
  wm_.IsOrthogonalTo((axisTracker_).GetWritingMode()) !=               \
          (axisTracker_).IsRowOriented()                               \
      ? (bsize_)                                                       \
      : (isize_)

// Flags to customize behavior of the FlexboxAxisTracker constructor:
enum class AxisTrackerFlags {
  eNoFlags = 0x0,

  // Normally, FlexboxAxisTracker may attempt to reverse axes & iteration order
  // to avoid bottom-to-top child ordering, for saner pagination. This flag
  // suppresses that behavior (so that we allow bottom-to-top child ordering).
  // (This may be helpful e.g. when we're only dealing with a single child.)
  eAllowBottomToTopChildOrdering = 0x1
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(AxisTrackerFlags)

// Encapsulates our flex container's main & cross axes.
class MOZ_STACK_CLASS nsFlexContainerFrame::FlexboxAxisTracker {
 public:
  FlexboxAxisTracker(const nsFlexContainerFrame* aFlexContainer,
                     const WritingMode& aWM,
                     AxisTrackerFlags aFlags = AxisTrackerFlags::eNoFlags);

  // Accessors:
  LogicalAxis MainAxis() const { return mMainAxis; }
  LogicalAxis CrossAxis() const { return GetOrthogonalAxis(mMainAxis); }

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
  bool IsMainAxisReversed() const { return mIsMainAxisReversed; }
  // Returns true if our cross axis is in the reverse direction of our
  // writing mode's corresponding axis. (From 'flex-wrap: *-reverse')
  bool IsCrossAxisReversed() const { return mIsCrossAxisReversed; }

  bool IsRowOriented() const { return mIsRowOriented; }
  bool IsColumnOriented() const { return !mIsRowOriented; }

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
        mIsMainAxisReversed ? aContainerMainSize - aMainCoord : aMainCoord;
    nscoord logicalCoordInCrossAxis =
        mIsCrossAxisReversed ? aContainerCrossSize - aCrossCoord : aCrossCoord;

    return mIsRowOriented ? LogicalPoint(mWM, logicalCoordInMainAxis,
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
    return mIsRowOriented ? LogicalSize(mWM, aMainSize, aCrossSize)
                          : LogicalSize(mWM, aCrossSize, aMainSize);
  }

  // Are my axes reversed with respect to what the author asked for?
  // (We may reverse the axes in the FlexboxAxisTracker constructor and set
  // this flag, to avoid reflowing our children in bottom-to-top order.)
  bool AreAxesInternallyReversed() const { return mAreAxesInternallyReversed; }

  bool IsMainAxisHorizontal() const {
    // If we're row-oriented, and our writing mode is NOT vertical,
    // or we're column-oriented and our writing mode IS vertical,
    // then our main axis is horizontal. This handles all cases:
    return mIsRowOriented != mWM.IsVertical();
  }

  // Delete copy-constructor & reassignment operator, to prevent accidental
  // (unnecessary) copying.
  FlexboxAxisTracker(const FlexboxAxisTracker&) = delete;
  FlexboxAxisTracker& operator=(const FlexboxAxisTracker&) = delete;

 private:
  // Helpers for constructor which determine the orientation of our axes, based
  // on legacy box properties (-webkit-box-orient, -webkit-box-direction) or
  // modern flexbox properties (flex-direction, flex-wrap) depending on whether
  // the flex container is a "legacy box" (as determined by IsLegacyBox).
  void InitAxesFromLegacyProps(const nsFlexContainerFrame* aFlexContainer);
  void InitAxesFromModernProps(const nsFlexContainerFrame* aFlexContainer);

  LogicalAxis mMainAxis = eLogicalAxisInline;

  const WritingMode mWM;  // The flex container's writing mode.

  // Is our main axis the inline axis? (Are we 'flex-direction:row[-reverse]'?)
  bool mIsRowOriented = true;

  // Is our main axis in the opposite direction as mWM's corresponding axis?
  // (e.g. RTL vs LTR)
  bool mIsMainAxisReversed = false;

  // Is our cross axis in the opposite direction as mWM's corresponding axis?
  // (e.g. BTT vs TTB)
  bool mIsCrossAxisReversed = false;

  // Implementation detail -- this indicates whether we've decided to
  // transparently reverse our axes & our child ordering, to avoid having
  // frames flow from bottom to top in either axis (& to make pagination saner).
  bool mAreAxesInternallyReversed = false;
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

  nscoord ResolvedAscent(bool aUseFirstBaseline) const {
    if (mAscent == ReflowOutput::ASK_FOR_BASELINE) {
      // XXXdholbert We should probably be using the *container's* writing-mode
      // here, instead of the item's -- though it doesn't much matter right
      // now, because all of the baseline-handling code here essentially
      // assumes that the container & items have the same writing-mode. This
      // will matter more (& can be expanded/tested) once we officially support
      // logical directions & vertical writing-modes in flexbox, in bug 1079155
      // or a dependency.
      // Use GetFirstLineBaseline() or GetLastLineBaseline() as appropriate,
      // or just GetLogicalBaseline() if that fails.
      bool found =
          aUseFirstBaseline
              ? nsLayoutUtils::GetFirstLineBaseline(mWM, mFrame, &mAscent)
              : nsLayoutUtils::GetLastLineBaseline(mWM, mFrame, &mAscent);

      if (!found) {
        mAscent = mFrame->SynthesizeBaselineBOffsetFromBorderBox(
            mWM, BaselineSharingGroup::First);
      }
    }
    return mAscent;
  }

  // Convenience methods to compute the main & cross size of our *margin-box*.
  nscoord OuterMainSize() const {
    return mMainSize + MarginBorderPaddingSizeInMainAxis();
  }

  nscoord OuterCrossSize() const {
    return mCrossSize + MarginBorderPaddingSizeInCrossAxis();
  }

  // Returns the distance between this FlexItem's baseline and the cross-start
  // edge of its margin-box. Used in baseline alignment.
  //
  // (This function needs to be told which physical start side we're measuring
  // the baseline from, so that it can look up the appropriate components from
  // margin.)
  nscoord BaselineOffsetFromOuterCrossEdge(mozilla::Side aStartSide,
                                           bool aUseFirstLineBaseline) const;

  float ShareOfWeightSoFar() const { return mShareOfWeightSoFar; }

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

  const AspectRatio& IntrinsicRatio() const { return mIntrinsicRatio; }
  bool HasIntrinsicRatio() const { return !!mIntrinsicRatio; }

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
    MOZ_ASSERT(mMainMaxSize >= aNewMinSize,
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
  }

  // Setters used while we're resolving flexible lengths
  // ---------------------------------------------------

  // Sets the main-size of our flex item's content-box.
  void SetMainSize(nscoord aNewMainSize) {
    MOZ_ASSERT(!mIsFrozen, "main size shouldn't change after we're frozen");
    mMainSize = aNewMainSize;
  }

  void SetShareOfWeightSoFar(float aNewShare) {
    MOZ_ASSERT(!mIsFrozen || aNewShare == 0.0f,
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

  uint32_t NumAutoMarginsInMainAxis() const {
    return NumAutoMarginsInAxis(MainAxis());
  };

  uint32_t NumAutoMarginsInCrossAxis() const {
    return NumAutoMarginsInAxis(CrossAxis());
  };

  // Once the main size has been resolved, should we bother doing layout to
  // establish the cross size?
  bool CanMainSizeInfluenceCrossSize() const;

  // Indicates whether we think this flex item needs a "final" reflow
  // (after its final flexed size & final position have been determined).
  // Retuns true if such a reflow is needed, or false if we believe it can
  // simply be moved to its final position and skip the reflow.
  bool NeedsFinalReflow() const;

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
  AspectRatio mIntrinsicRatio;

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
  mutable nscoord mAscent = 0;

  // Temporary state, while we're resolving flexible widths (for our main size)
  // XXXdholbert To save space, we could use a union to make these variables
  // overlay the same memory as some other member vars that aren't touched
  // until after main-size has been resolved. In particular, these could share
  // memory with mMainPosn through mAscent, and mIsStretched.
  float mShareOfWeightSoFar = 0.0f;

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
  nscoord TotalOuterHypotheticalMainSize() const {
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

    // Note: If our flex item is (or contains) a table with
    // "table-layout:fixed", it may have a value near nscoord_MAX as its
    // hypothetical main size. This means we can run into absurdly large sizes
    // here, even when the author didn't explicitly specify anything huge.
    // We'd really rather not allow that to cause integer overflow (e.g. we
    // don't want that to make mTotalOuterHypotheticalMainSize overflow to a
    // negative value), because that'd make us incorrectly think that we should
    // grow our flex items rather than shrink them when it comes time to
    // resolve flexible items. Hence, we sum up the hypothetical sizes using a
    // helper function AddChecked() to avoid overflow.
    mTotalItemMBP =
        AddChecked(mTotalItemMBP, lastItem.MarginBorderPaddingSizeInMainAxis());

    mTotalOuterHypotheticalMainSize =
        AddChecked(mTotalOuterHypotheticalMainSize, lastItem.OuterMainSize());

    // If the item added was not the first item in the line, we add in any gap
    // space as needed.
    if (NumItems() >= 2) {
      mTotalOuterHypotheticalMainSize =
          AddChecked(mTotalOuterHypotheticalMainSize, mMainGapSize);
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
   * should place their baseline. Usually, this represents a distance from the
   * line's cross-start edge, but if we're internally reversing the axes (see
   * AreAxesInternallyReversed()), this instead represents the distance from
   * its cross-end edge.
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

  inline void SetMainGapSize(nscoord aNewSize) { mMainGapSize = aNewSize; }

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
  nscoord mTotalOuterHypotheticalMainSize = 0;

  nscoord mLineCrossSize = 0;
  nscoord mFirstBaselineOffset = nscoord_MIN;
  nscoord mLastBaselineOffset = nscoord_MIN;

  // Maintain size of each {row,column}-gap in the main axis
  nscoord mMainGapSize = 0;
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
    return StyleAlignFlags::START;
  }
  if (specified == StyleAlignFlags::SPACE_AROUND ||
      specified == StyleAlignFlags::SPACE_EVENLY) {
    return StyleAlignFlags::CENTER;
  }
  return specified;
}

StyleAlignFlags nsFlexContainerFrame::CSSAlignmentForAbsPosChild(
    const ReflowInput& aChildRI, LogicalAxis aLogicalAxis) const {
  WritingMode wm = GetWritingMode();
  const FlexboxAxisTracker axisTracker(
      this, wm, AxisTrackerFlags::eAllowBottomToTopChildOrdering);

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

  // Resolve flex-start, flex-end, auto, left, right, baseline, last baseline;
  if (alignment == StyleAlignFlags::FLEX_START) {
    alignment = isAxisReversed ? StyleAlignFlags::END : StyleAlignFlags::START;
  } else if (alignment == StyleAlignFlags::FLEX_END) {
    alignment = isAxisReversed ? StyleAlignFlags::START : StyleAlignFlags::END;
  } else if (alignment == StyleAlignFlags::LEFT ||
             alignment == StyleAlignFlags::RIGHT) {
    if (aLogicalAxis == eLogicalAxisInline) {
      const bool isLeft = (alignment == StyleAlignFlags::LEFT);
      alignment = (isLeft == wm.IsBidiLTR()) ? StyleAlignFlags::START
                                             : StyleAlignFlags::END;
    } else {
      alignment = StyleAlignFlags::START;
    }
  } else if (alignment == StyleAlignFlags::BASELINE) {
    alignment = StyleAlignFlags::START;
  } else if (alignment == StyleAlignFlags::LAST_BASELINE) {
    alignment = StyleAlignFlags::END;
  }

  return (alignment | alignmentFlags);
}

FlexItem* nsFlexContainerFrame::GenerateFlexItemForChild(
    FlexLine& aLine, nsIFrame* aChildFrame,
    const ReflowInput& aParentReflowInput,
    const FlexboxAxisTracker& aAxisTracker, bool aHasLineClampEllipsis) {
  // Create temporary reflow input just for sizing -- to get hypothetical
  // main-size and the computed values of min / max main-size property.
  // (This reflow input will _not_ be used for reflow.)
  ReflowInput childRI(
      PresContext(), aParentReflowInput, aChildFrame,
      aParentReflowInput.ComputedSize(aChildFrame->GetWritingMode()));
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
    const nsStylePosition* stylePos = aChildFrame->StylePosition();
    flexGrow = stylePos->mFlexGrow;
    flexShrink = stylePos->mFlexShrink;
  }

  WritingMode childWM = childRI.GetWritingMode();

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
                                                 disp->mAppearance,
                                                 &widgetMinSize, &canOverride);

    nscoord widgetMainMinSize = PresContext()->DevPixelsToAppUnits(
        aAxisTracker.MainComponent(widgetMinSize));
    nscoord widgetCrossMinSize = PresContext()->DevPixelsToAppUnits(
        aAxisTracker.CrossComponent(widgetMinSize));

    // GetMinimumWidgetSize() returns border-box. We need content-box, so
    // subtract borderPadding.
    const LogicalMargin bpInChildWM = childRI.ComputedLogicalBorderPadding();
    const LogicalMargin bpInFlexWM =
        bpInChildWM.ConvertTo(aAxisTracker.GetWritingMode(), childWM);
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
// Indicates whether the cross-size property is set to something definite,
// for the purpose of intrinsic ratio calculations.
// The logic here should be similar to the logic for isAutoISize/isAutoBSize
// in nsFrame::ComputeSizeWithIntrinsicDimensions().
static bool IsCrossSizeDefinite(const ReflowInput& aItemReflowInput,
                                const FlexboxAxisTracker& aAxisTracker) {
  const nsStylePosition* pos = aItemReflowInput.mStylePosition;
  const WritingMode containerWM = aAxisTracker.GetWritingMode();

  if (aAxisTracker.IsColumnOriented()) {
    // Column-oriented means cross axis is container's inline axis.
    return !pos->ISize(containerWM).IsAuto();
  }
  // Else, we're row-oriented, which means cross axis is container's block
  // axis. We need to use IsAutoBSize() to catch e.g. %-BSize applied to
  // indefinite container BSize, which counts as auto.
  nscoord cbBSize = aItemReflowInput.mCBReflowInput->ComputedBSize();
  return !nsLayoutUtils::IsAutoBSize(pos->BSize(containerWM), cbBSize);
}

// If aFlexItem has a definite cross size, this function returns it, for usage
// (in combination with an intrinsic ratio) for resolving the item's main size
// or main min-size.
//
// The parameter "aMinSizeFallback" indicates whether we should fall back to
// returning the cross min-size, when the cross size is indefinite. (This param
// should be set IFF the caller intends to resolve the main min-size.) If this
// param is true, then this function is guaranteed to return a definite value
// (i.e. not NS_UNCONSTRAINEDSIZE, excluding cases where huge sizes are
// involved).
//
// XXXdholbert the min-size behavior here is based on my understanding in
//   http://lists.w3.org/Archives/Public/www-style/2014Jul/0053.html
// If my understanding there ends up being wrong, we'll need to update this.
static nscoord CrossSizeToUseWithRatio(const FlexItem& aFlexItem,
                                       const ReflowInput& aItemReflowInput,
                                       bool aMinSizeFallback,
                                       const FlexboxAxisTracker& aAxisTracker) {
  if (aFlexItem.IsStretched()) {
    // Definite cross-size, imposed via 'align-self:stretch' & flex container.
    return aFlexItem.CrossSize();
  }

  if (IsCrossSizeDefinite(aItemReflowInput, aAxisTracker)) {
    // Definite cross size.
    return GET_CROSS_COMPONENT_LOGICAL(aAxisTracker, aFlexItem.GetWritingMode(),
                                       aItemReflowInput.ComputedISize(),
                                       aItemReflowInput.ComputedBSize());
  }

  if (aMinSizeFallback) {
    // Indefinite cross-size, and we're resolving main min-size, so we'll fall
    // back to ussing the cross min-size (which should be definite).
    return GET_CROSS_COMPONENT_LOGICAL(aAxisTracker, aFlexItem.GetWritingMode(),
                                       aItemReflowInput.ComputedMinISize(),
                                       aItemReflowInput.ComputedMinBSize());
  }

  // Indefinite cross-size.
  return NS_UNCONSTRAINEDSIZE;
}

// Convenience function; returns a main-size, given a cross-size and an
// intrinsic ratio. The caller is responsible for ensuring that the passed-in
// intrinsic ratio is not zero.
static nscoord MainSizeFromAspectRatio(nscoord aCrossSize,
                                       const AspectRatio& aIntrinsicRatio,
                                       const FlexboxAxisTracker& aAxisTracker) {
  MOZ_ASSERT(aIntrinsicRatio,
             "Invalid ratio; will divide by 0! Caller should've checked...");
  AspectRatio ratio = aAxisTracker.IsMainAxisHorizontal()
                          ? aIntrinsicRatio
                          : aIntrinsicRatio.Inverted();

  return ratio.ApplyTo(aCrossSize);
}

// Partially resolves "min-[width|height]:auto" and returns the resulting value.
// By "partially", I mean we don't consider the min-content size (but we do
// consider flex-basis, main max-size, and the intrinsic aspect ratio).
// The caller is responsible for computing & considering the min-content size
// in combination with the partially-resolved value that this function returns.
//
// Spec reference: http://dev.w3.org/csswg/css-flexbox/#min-size-auto
static nscoord PartiallyResolveAutoMinSize(
    const FlexItem& aFlexItem, const ReflowInput& aItemReflowInput,
    const FlexboxAxisTracker& aAxisTracker) {
  MOZ_ASSERT(aFlexItem.NeedsMinSizeAutoResolution(),
             "only call for FlexItems that need min-size auto resolution");

  nscoord minMainSize = nscoord_MAX;  // Intentionally huge; we'll shrink it
                                      // from here, w/ std::min().

  // We need the smallest of:
  // * the used flex-basis, if the computed flex-basis was 'auto':
  if (aItemReflowInput.mStylePosition->mFlexBasis.IsAuto() &&
      aFlexItem.FlexBaseSize() != NS_UNCONSTRAINEDSIZE) {
    // NOTE: We skip this if the flex base size depends on content & isn't yet
    // resolved. This is OK, because the caller is responsible for computing
    // the min-content height and min()'ing it with the value we return, which
    // is equivalent to what would happen if we min()'d that at this point.
    minMainSize = std::min(minMainSize, aFlexItem.FlexBaseSize());
  }

  // * the computed max-width (max-height), if that value is definite:
  nscoord maxSize = GET_MAIN_COMPONENT_LOGICAL(
      aAxisTracker, aFlexItem.GetWritingMode(),
      aItemReflowInput.ComputedMaxISize(), aItemReflowInput.ComputedMaxBSize());
  if (maxSize != NS_UNCONSTRAINEDSIZE) {
    minMainSize = std::min(minMainSize, maxSize);
  }

  // * if the item has no intrinsic aspect ratio, its min-content size:
  //  --- SKIPPING THIS IN THIS FUNCTION --- caller's responsibility.

  // * if the item has an intrinsic aspect ratio, the width (height) calculated
  //   from the aspect ratio and any definite size constraints in the opposite
  //   dimension.
  if (aFlexItem.IntrinsicRatio()) {
    // We have a usable aspect ratio. (not going to divide by 0)
    const bool useMinSizeIfCrossSizeIsIndefinite = true;
    nscoord crossSizeToUseWithRatio = CrossSizeToUseWithRatio(
        aFlexItem, aItemReflowInput, useMinSizeIfCrossSizeIsIndefinite,
        aAxisTracker);
    nscoord minMainSizeFromRatio = MainSizeFromAspectRatio(
        crossSizeToUseWithRatio, aFlexItem.IntrinsicRatio(), aAxisTracker);
    minMainSize = std::min(minMainSize, minMainSizeFromRatio);
  }

  return minMainSize;
}

// Resolves flex-basis:auto, using the given intrinsic ratio and the flex
// item's cross size.  On success, updates the flex item with its resolved
// flex-basis and returns true. On failure (e.g. if the ratio is invalid or
// the cross-size is indefinite), returns false.
static bool ResolveAutoFlexBasisFromRatio(
    FlexItem& aFlexItem, const ReflowInput& aItemReflowInput,
    const FlexboxAxisTracker& aAxisTracker) {
  MOZ_ASSERT(NS_UNCONSTRAINEDSIZE == aFlexItem.FlexBaseSize(),
             "Should only be called to resolve an 'auto' flex-basis");
  // If the flex item has ...
  //  - an intrinsic aspect ratio,
  //  - a [used] flex-basis of 'main-size' [auto?]
  //    [We have this, if we're here.]
  //  - a definite cross size
  // then the flex base size is calculated from its inner cross size and the
  // flex item’s intrinsic aspect ratio.
  if (aFlexItem.IntrinsicRatio()) {
    // We have a usable aspect ratio. (not going to divide by 0)
    const bool useMinSizeIfCrossSizeIsIndefinite = false;
    nscoord crossSizeToUseWithRatio = CrossSizeToUseWithRatio(
        aFlexItem, aItemReflowInput, useMinSizeIfCrossSizeIsIndefinite,
        aAxisTracker);
    if (crossSizeToUseWithRatio != NS_UNCONSTRAINEDSIZE) {
      // We have a definite cross-size
      nscoord mainSizeFromRatio = MainSizeFromAspectRatio(
          crossSizeToUseWithRatio, aFlexItem.IntrinsicRatio(), aAxisTracker);
      aFlexItem.SetFlexBaseSizeAndMainSize(mainSizeFromRatio);
      return true;
    }
  }
  return false;
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

  // We may be about to do computations based on our item's cross-size
  // (e.g. using it as a contstraint when measuring our content in the
  // main axis, or using it with the intrinsic ratio to obtain a main size).
  // BEFORE WE DO THAT, we need let the item "pre-stretch" its cross size (if
  // it's got 'align-self:stretch'), for a certain case where the spec says
  // the stretched cross size is considered "definite". That case is if we
  // have a single-line (nowrap) flex container which itself has a definite
  // cross-size.  Otherwise, we'll wait to do stretching, since (in other
  // cases) we don't know how much the item should stretch yet.
  const ReflowInput* flexContainerRI = aItemReflowInput.mParentReflowInput;
  MOZ_ASSERT(flexContainerRI,
             "flex item's reflow input should have ptr to container's state");
  if (StyleFlexWrap::Nowrap == flexContainerRI->mStylePosition->mFlexWrap) {
    // XXXdholbert Maybe this should share logic with ComputeCrossSize()...
    // Alternately, maybe tentative container cross size should be passed down.
    nscoord containerCrossSize = GET_CROSS_COMPONENT_LOGICAL(
        aAxisTracker, aAxisTracker.GetWritingMode(),
        flexContainerRI->ComputedISize(), flexContainerRI->ComputedBSize());
    // Is container's cross size "definite"?
    // - If it's column-oriented, then "yes", because its cross size is its
    // inline-size which is always definite from its descendants' perspective.
    // - Otherwise (if it's row-oriented), then we check the actual size
    // and call it definite if it's not NS_UNCONSTRAINEDSIZE.
    if (aAxisTracker.IsColumnOriented() ||
        containerCrossSize != NS_UNCONSTRAINEDSIZE) {
      // Container's cross size is "definite", so we can resolve the item's
      // stretched cross size using that.
      aFlexItem.ResolveStretchedCrossSize(containerCrossSize);
    }
  }

  nscoord resolvedMinSize;  // (only set/used if isMainMinSizeAuto==true)
  bool minSizeNeedsToMeasureContent = false;  // assume the best
  if (isMainMinSizeAuto) {
    // Resolve the min-size, except for considering the min-content size.
    // (We'll consider that later, if we need to.)
    resolvedMinSize =
        PartiallyResolveAutoMinSize(aFlexItem, aItemReflowInput, aAxisTracker);
    if (resolvedMinSize > 0 && !aFlexItem.IntrinsicRatio()) {
      // We don't have a usable aspect ratio, so we need to consider our
      // min-content size as another candidate min-size, which we'll have to
      // min() with the current resolvedMinSize.
      // (If resolvedMinSize were already at 0, we could skip this measurement
      // because it can't go any lower. But it's not 0, so we need it.)
      minSizeNeedsToMeasureContent = true;
    }
  }

  bool flexBasisNeedsToMeasureContent = false;  // assume the best
  if (isMainSizeAuto) {
    if (!ResolveAutoFlexBasisFromRatio(aFlexItem, aItemReflowInput,
                                       aAxisTracker)) {
      flexBasisNeedsToMeasureContent = true;
    }
  }

  // Measure content, if needed (w/ intrinsic-width method or a reflow)
  if (minSizeNeedsToMeasureContent || flexBasisNeedsToMeasureContent) {
    if (aFlexItem.IsInlineAxisMainAxis()) {
      if (minSizeNeedsToMeasureContent) {
        nscoord frameMinISize =
            aFlexItem.Frame()->GetMinISize(aItemReflowInput.mRenderingContext);
        resolvedMinSize = std::min(resolvedMinSize, frameMinISize);
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

      nscoord contentBSize =
          MeasureFlexItemContentBSize(aFlexItem, forceBResizeForMeasuringReflow,
                                      aHasLineClampEllipsis, *flexContainerRI);
      if (minSizeNeedsToMeasureContent) {
        resolvedMinSize = std::min(resolvedMinSize, contentBSize);
      }
      if (flexBasisNeedsToMeasureContent) {
        aFlexItem.SetFlexBaseSizeAndMainSize(contentBSize);
      }
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
 *   - its ascent
 *
 * The assumption here is that a given flex item measurement from our "value"
 * won't change unless one of the pieces of the "key" change, or the flex
 * item's intrinsic size is marked as dirty (due to a style or DOM change).
 * (The latter will cause the cached value to be discarded, in
 * nsFrame::MarkIntrinsicISizesDirty.)
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
  const nscoord mAscent;

 public:
  CachedBAxisMeasurement(const ReflowInput& aReflowInput,
                         const ReflowOutput& aReflowOutput)
      : mKey(aReflowInput), mAscent(aReflowOutput.BlockStartAscent()) {
    // To get content-box bsize, we have to subtract off border & padding
    // (and floor at 0 in case the border/padding are too large):
    WritingMode itemWM = aReflowInput.GetWritingMode();
    nscoord borderBoxBSize = aReflowOutput.BSize(itemWM);
    mBSize = borderBoxBSize -
             aReflowInput.ComputedLogicalBorderPadding().BStartEnd(itemWM);
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

  nscoord Ascent() const { return mAscent; }
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
  // XXXdholbert Unused for now, but will be used in a later patch.
  Final,
};

/**
 * This class stores information about the previous reflow for a given flex
 * item.  This should hopefully help us avoid redundant reflows of that
 * flex item.
 */
class nsFlexContainerFrame::CachedFlexItemData {
 public:
  CachedFlexItemData(const ReflowInput& aReflowInput,
                     const ReflowOutput& aReflowOutput,
                     FlexItemReflowType aType) {
    if (aType == FlexItemReflowType::Measuring) {
      mBAxisMeasurement.emplace(aReflowInput, aReflowOutput);
    }
  }

  // If the flex container needs a measuring reflow for the flex item, then the
  // resulting block-axis measurements can be cached here.  If no measurement
  // has been needed so far, then this member will be Nothing().
  Maybe<CachedBAxisMeasurement> mBAxisMeasurement;

  // Instances of this class are stored under this frame property, on
  // frames that are flex items:
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(Prop, CachedFlexItemData)
};

void nsFlexContainerFrame::MarkCachedFlexMeasurementsDirty(
    nsIFrame* aItemFrame) {
  if (auto* cache = aItemFrame->GetProperty(CachedFlexItemData::Prop())) {
    cache->mBAxisMeasurement.reset();
  }
}

const CachedBAxisMeasurement&
nsFlexContainerFrame::MeasureAscentAndBSizeForFlexItem(
    FlexItem& aItem, ReflowInput& aChildReflowInput) {
  auto* cachedData = aItem.Frame()->GetProperty(CachedFlexItemData::Prop());

  if (cachedData && cachedData->mBAxisMeasurement) {
    if (cachedData->mBAxisMeasurement->IsValidFor(aChildReflowInput)) {
      return *(cachedData->mBAxisMeasurement);
    }
    FLEX_LOG("[perf] MeasureAscentAndBSizeForFlexItem rejected cached value");
  } else {
    FLEX_LOG(
        "[perf] MeasureAscentAndBSizeForFlexItem didn't have a cached value");
  }

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

  // XXXdholbert Once we do pagination / splitting, we'll need to actually
  // handle incomplete childReflowStatuses. But for now, we give our kids
  // unconstrained available height, which means they should always complete.
  MOZ_ASSERT(childReflowStatus.IsComplete(),
             "We gave flex item unconstrained available height, so it "
             "should be complete");

  // Tell the child we're done with its initial reflow.
  // (Necessary for e.g. GetBaseline() to work below w/out asserting)
  FinishReflowChild(aItem.Frame(), PresContext(), childReflowOutput,
                    &aChildReflowInput, outerWM, dummyPosition,
                    dummyContainerSize, flags);

  // Update (or add) our cached measurement, so that we can hopefully skip this
  // measuring reflow the next time around:
  if (cachedData) {
    cachedData->mBAxisMeasurement.reset();
    cachedData->mBAxisMeasurement.emplace(aChildReflowInput, childReflowOutput);
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
  // Set up a reflow input for measuring the flex item's auto-height:
  WritingMode wm = aFlexItem.Frame()->GetWritingMode();
  LogicalSize availSize = aParentReflowInput.ComputedSize(wm);
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
  ReflowInput childRIForMeasuringBSize(PresContext(), aParentReflowInput,
                                       aFlexItem.Frame(), availSize, Nothing(),
                                       ReflowInput::CALLER_WILL_INIT);
  childRIForMeasuringBSize.mFlags.mIsFlexContainerMeasuringBSize = true;
  childRIForMeasuringBSize.mFlags.mInsideLineClamp = GetLineClampValue() != 0;
  childRIForMeasuringBSize.mFlags.mApplyLineClamp =
      childRIForMeasuringBSize.mFlags.mInsideLineClamp || aHasLineClampEllipsis;
  childRIForMeasuringBSize.Init(PresContext());

  if (aFlexItem.IsStretched()) {
    childRIForMeasuringBSize.SetComputedISize(aFlexItem.CrossSize());
    childRIForMeasuringBSize.SetIResize(true);
  }

  if (aForceBResizeForMeasuringReflow) {
    childRIForMeasuringBSize.SetBResize(true);
    // Not 100% sure this is needed, but be conservative for now:
    childRIForMeasuringBSize.mFlags.mIsBResizeForPercentages = true;
  }

  const CachedBAxisMeasurement& measurement =
      MeasureAscentAndBSizeForFlexItem(aFlexItem, childRIForMeasuringBSize);

  aFlexItem.SetAscent(measurement.Ascent());
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
      mIntrinsicRatio(mFrame->GetIntrinsicRatio()),
      mWM(aFlexItemReflowInput.GetWritingMode()),
      mCBWM(aAxisTracker.GetWritingMode()),
      mMainAxis(aAxisTracker.MainAxis()),
      mBorderPadding(
          aFlexItemReflowInput.ComputedLogicalBorderPadding().ConvertTo(mCBWM,
                                                                        mWM)),
      mMargin(
          aFlexItemReflowInput.ComputedLogicalMargin().ConvertTo(mCBWM, mWM)),
      mMainMinSize(aMainMinSize),
      mMainMaxSize(aMainMaxSize),
      mCrossMinSize(aCrossMinSize),
      mCrossMaxSize(aCrossMaxSize),
      mCrossSize(aTentativeCrossSize),
      mIsInlineAxisMainAxis(aAxisTracker.IsRowOriented() !=
                            aAxisTracker.GetWritingMode().IsOrthogonalTo(mWM))
// mNeedsMinSizeAutoResolution is initialized in CheckForMinSizeAuto()
// mAlignSelf, mHasAnyAutoMargin see below
{
  MOZ_ASSERT(mFrame, "expecting a non-null child frame");
  MOZ_ASSERT(!mFrame->IsPlaceholderFrame(),
             "placeholder frames should not be treated as flex items");
  MOZ_ASSERT(!(mFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW),
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
  // (c) flex item has a definite flex basis and is fully inflexible
  //     (NOTE: We don't actually check "fully inflexible" because webcompat
  //     may not agree with that restriction...)
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
    // an intrinsic aspect ratio, which for now are all leaf nodes and hence
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
      // XXXdholbert Technically the spec requires the flex item to *also* be
      // fully inflexible in order to have its size treated as definite in this
      // scenario, but no browser implements that additional restriction, so
      // it's not clear that this additional requirement would be
      // web-compatible...
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
  MOZ_ASSERT(!(mFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW),
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
  const nsStylePosition* stylePos = mFrame->StylePosition();
  // Check whichever component is in the flex container's cross axis.
  // (IsInlineAxisCrossAxis() tells us whether that's our ISize or BSize, in
  // terms of our own WritingMode, mWM.)
  return IsInlineAxisCrossAxis() ? stylePos->ISize(mWM).IsAuto()
                                 : stylePos->BSize(mWM).IsAuto();
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

  if (HasIntrinsicRatio()) {
    // For flex items that have an intrinsic ratio (and maintain it, i.e. are
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
  for (nsIFrame::ChildListIterator childLists(aFrame); !childLists.IsDone();
       childLists.Next()) {
    for (nsIFrame* childFrame : childLists.CurrentList()) {
      if (childFrame->HasAnyStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
        return true;
      }
    }
  }
  return false;
}

bool FlexItem::NeedsFinalReflow() const {
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
          "[perf] Flex item needed both a measuring reflow and a final "
          "reflow due to measured size disagreeing with final size");
      return true;
    }

    if (FrameHasRelativeBSizeDependency(mFrame)) {
      // This item has descendants with relative BSizes who may care that its
      // size may now be considered "definite" in the final reflow (whereas it
      // was indefinite during the measuring reflow).
      FLEX_LOG(
          "[perf] Flex item needed both a measuring reflow and a final "
          "reflow due to BSize potentially becoming definite");
      return true;
    }
    // If we get here, then this flex item had a measuring reflow, and it left
    // us with the correct size, and none of our descendants care that our
    // BSize may now be considered definite. So we don't need a final reflow.
    return false;
  }

  // This item didn't receive a measuring reflow. Does it need to be reflowed
  // at all?

  // XXXdholbert in a later patch, we'll add some special cases here, making
  // use of "finalSize" (which is why it's declared at this outer scope).  For
  // now, we assume that we unconditionally must reflow the item.
  return true;
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

  inline void SetCrossGapSize(nscoord aNewSize) { mCrossGapSize = aNewSize; }

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

  nscoord mCrossGapSize = 0;
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

  if (GetStateBits() & NS_FRAME_FONT_INFLATION_CONTAINER) {
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
  return MakeFrameName(NS_LITERAL_STRING("FlexContainer"), aResult);
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

// Helper for BuildDisplayList, to implement this special-case for flex items
// from the spec:
//    Flex items paint exactly the same as block-level elements in the
//    normal flow, except that 'z-index' values other than 'auto' create
//    a stacking context even if 'position' is 'static'.
// http://www.w3.org/TR/2012/CR-css3-flexbox-20120918/#painting
static uint32_t GetDisplayFlagsForFlexItem(nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame->IsFlexItem(), "Should only be called on flex items");
  const nsStylePosition* pos = aFrame->StylePosition();
  if (pos->mZIndex.IsInteger()) {
    return nsIFrame::DISPLAY_CHILD_FORCE_STACKING_CONTEXT;
  }
  return nsIFrame::DISPLAY_CHILD_FORCE_PSEUDO_STACKING_CONTEXT;
}

void nsFlexContainerFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                            const nsDisplayListSet& aLists) {
  nsDisplayListCollection tempLists(aBuilder);

  DisplayBorderBackgroundOutline(aBuilder, tempLists);

  // Our children are all block-level, so their borders/backgrounds all go on
  // the BlockBorderBackgrounds list.
  nsDisplayListSet childLists(tempLists, tempLists.BlockBorderBackgrounds());

  typedef CSSOrderAwareFrameIterator::OrderState OrderState;
  OrderState orderState =
      HasAnyStateBits(NS_STATE_FLEX_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER)
          ? OrderState::eKnownOrdered
          : OrderState::eKnownUnordered;

  CSSOrderAwareFrameIterator iter(this, kPrincipalList,
                                  CSSOrderAwareFrameIterator::eIncludeAll,
                                  orderState, OrderingPropertyForIter(this));
  for (; !iter.AtEnd(); iter.Next()) {
    nsIFrame* childFrame = *iter;
    BuildDisplayListForChild(aBuilder, childFrame, childLists,
                             GetDisplayFlagsForFlexItem(childFrame));
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
  FLEX_LOG("ResolveFlexibleLengths");

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
      (mTotalOuterHypotheticalMainSize < aFlexContainerMainSize);

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
    return;
  }
  MOZ_ASSERT(!IsEmpty() || aLineInfo,
             "empty lines should take the early-return above");

  // Subtract space occupied by our items' margins/borders/padding/gaps, so
  // we can just be dealing with the space available for our flex items' content
  // boxes.
  nscoord spaceAvailableForFlexItemsContentBoxes =
      aFlexContainerMainSize - (mTotalItemMBP + SumOfGaps());

  nscoord origAvailableFreeSpace;
  bool isOrigAvailFreeSpaceInitialized = false;

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
    nscoord availableFreeSpace = spaceAvailableForFlexItemsContentBoxes;
    for (FlexItem& item : Items()) {
      if (!item.IsFrozen()) {
        item.SetMainSize(item.FlexBaseSize());
      }
      availableFreeSpace -= item.MainSize();
    }

    FLEX_LOG(" available free space = %d", availableFreeSpace);

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
      uint32_t numUnfrozenItemsToBeSeen = NumItems() - mNumFrozenItems;
      for (FlexItem& item : Items()) {
        if (numUnfrozenItemsToBeSeen == 0) {
          break;
        }

        if (!item.IsFrozen()) {
          numUnfrozenItemsToBeSeen--;

          float curWeight = item.GetWeight(isUsingFlexGrow);
          float curFlexFactor = item.GetFlexFactor(isUsingFlexGrow);
          MOZ_ASSERT(curWeight >= 0.0f, "weights are non-negative");
          MOZ_ASSERT(curFlexFactor >= 0.0f, "flex factors are non-negative");

          weightSum += curWeight;
          flexFactorSum += curFlexFactor;

          if (IsFinite(weightSum)) {
            if (curWeight == 0.0f) {
              item.SetShareOfWeightSoFar(0.0f);
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
          NS_ASSERTION(totalDesiredPortionOfOrigFreeSpace == 0 ||
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
            nscoord sizeDelta = 0;
            if (IsFinite(weightSum)) {
              float myShareOfRemainingSpace = item.ShareOfWeightSoFar();

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
            } else if (item.GetWeight(isUsingFlexGrow) == largestWeight) {
              // Total flexibility is infinite, so we're just distributing
              // the available space equally among the items that are tied for
              // having the largest weight (and this is one of those items).
              sizeDelta = NSToCoordRound(availableFreeSpace /
                                         float(numItemsWithLargestWeight));
              numItemsWithLargestWeight--;
            }

            availableFreeSpace -= sizeDelta;

            item.SetMainSize(item.MainSize() + sizeDelta);
            FLEX_LOG("  child %p receives %d, for a total of %d", &item,
                     sizeDelta, item.MainSize());
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

  // If our main axis is (internally) reversed, swap the justify-content
  // "flex-start" and "flex-end" behaviors:
  // NOTE: This must happen ...
  //  - *after* value-simplification for values that are dependent on our
  //    flex-axis reversedness; e.g. for "space-between" which specifically
  //    behaves like "flex-start" in some cases (per spec), and hence depends on
  //    the reversedness of flex axes.
  //  - *before* value simplification for values that don't care about
  //    flex-relative axis direction; e.g. for "start" which purely depends on
  //    writing-mode and isn't affected by reversedness of flex axes.
  if (aAxisTracker.AreAxesInternallyReversed()) {
    if (mJustifyContent.primary == StyleAlignFlags::FLEX_START) {
      mJustifyContent.primary = StyleAlignFlags::FLEX_END;
    } else if (mJustifyContent.primary == StyleAlignFlags::FLEX_END) {
      mJustifyContent.primary = StyleAlignFlags::FLEX_START;
    }
  }

  // Map 'left'/'right' to 'start'/'end'
  if (mJustifyContent.primary == StyleAlignFlags::LEFT ||
      mJustifyContent.primary == StyleAlignFlags::RIGHT) {
    if (aAxisTracker.IsColumnOriented()) {
      // Container's alignment axis is not parallel to the inline axis,
      // so we map both 'left' and 'right' to 'start'.
      mJustifyContent.primary = StyleAlignFlags::START;
    } else {
      // Row-oriented, so we map 'left' and 'right' to 'start' or 'end',
      // depending on left-to-right writing mode.
      const bool isLTR = aAxisTracker.GetWritingMode().IsBidiLTR();
      const bool isJustifyLeft =
          (mJustifyContent.primary == StyleAlignFlags::LEFT);
      mJustifyContent.primary = (isJustifyLeft == isLTR)
                                    ? StyleAlignFlags::START
                                    : StyleAlignFlags::END;
    }
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

  // If our cross axis is (internally) reversed, swap the align-content
  // "flex-start" and "flex-end" behaviors:
  // NOTE: It matters precisely when we do this; see comment alongside
  // MainAxisPositionTracker's AreAxesInternallyReversed check.
  if (aAxisTracker.AreAxesInternallyReversed()) {
    if (mAlignContent.primary == StyleAlignFlags::FLEX_START) {
      mAlignContent.primary = StyleAlignFlags::FLEX_END;
    } else if (mAlignContent.primary == StyleAlignFlags::FLEX_END) {
      mAlignContent.primary = StyleAlignFlags::FLEX_START;
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

  // The line's baseline offset is the distance from the line's edge (start or
  // end, depending on whether we've flipped the axes) to the furthest
  // item-baseline. The item(s) with that baseline will be exactly aligned with
  // the line's edge.
  mFirstBaselineOffset = aAxisTracker.AreAxesInternallyReversed()
                             ? crossEndToFurthestFirstBaseline
                             : crossStartToFurthestFirstBaseline;

  mLastBaselineOffset = aAxisTracker.AreAxesInternallyReversed()
                            ? crossStartToFurthestLastBaseline
                            : crossEndToFurthestLastBaseline;

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

  // If our cross axis is (internally) reversed, swap the align-self
  // "flex-start" and "flex-end" behaviors:
  if (aAxisTracker.AreAxesInternallyReversed()) {
    if (alignSelf == StyleAlignFlags::FLEX_START) {
      alignSelf = StyleAlignFlags::FLEX_END;
    } else if (alignSelf == StyleAlignFlags::FLEX_END) {
      alignSelf = StyleAlignFlags::FLEX_START;
    }
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

    // Normally, baseline-aligned items are collectively aligned with the
    // line's physical cross-start side; however, if our cross axis is
    // (internally) reversed, we instead align them with the physical
    // cross-end side. A similar logic holds for last baseline-aligned items,
    // but in reverse.
    const mozilla::Side baselineAlignStartSide =
        aAxisTracker.AreAxesInternallyReversed() == useFirst
            ? aAxisTracker.CrossAxisPhysicalEndSide()
            : aAxisTracker.CrossAxisPhysicalStartSide();

    nscoord itemBaselineOffset = aItem.BaselineOffsetFromOuterCrossEdge(
        baselineAlignStartSide, useFirst);

    nscoord lineBaselineOffset =
        useFirst ? aLine.FirstBaselineOffset() : aLine.LastBaselineOffset();

    NS_ASSERTION(lineBaselineOffset >= itemBaselineOffset,
                 "failed at finding largest baseline offset");

    // How much do we need to adjust our position (from the line edge),
    // to get the item's baseline to hit the line's baseline offset:
    nscoord baselineDiff = lineBaselineOffset - itemBaselineOffset;

    if (aAxisTracker.AreAxesInternallyReversed() == useFirst) {
      // Advance to align item w/ line's flex-end edge (as in FLEX_END case):
      mPosition += aLine.LineCrossSize() - aItem.OuterCrossSize();
      // ...and step *back* by the baseline adjustment:
      mPosition -= baselineDiff;
    } else {
      // mPosition is already at line's flex-start edge.
      // From there, we step *forward* by the baseline adjustment:
      mPosition += baselineDiff;
    }
  } else {
    MOZ_ASSERT_UNREACHABLE("Unexpected align-self value");
  }
}

FlexboxAxisTracker::FlexboxAxisTracker(
    const nsFlexContainerFrame* aFlexContainer, const WritingMode& aWM,
    AxisTrackerFlags aFlags)
    : mWM(aWM) {
  if (IsLegacyBox(aFlexContainer)) {
    InitAxesFromLegacyProps(aFlexContainer);
  } else {
    InitAxesFromModernProps(aFlexContainer);
  }

  // Master switch to enable/disable bug 983427's code for reversing our axes
  // and reversing some logic, to avoid reflowing children in bottom-to-top
  // order. (This switch can be removed eventually, but for now, it allows
  // this special-case code path to be compared against the normal code path.)
  static bool sPreventBottomToTopChildOrdering = true;

  // Note: if the eAllowBottomToTopChildOrdering flag is set, that overrides
  // the static boolean and makes us skip this special case.
  if (!(aFlags & AxisTrackerFlags::eAllowBottomToTopChildOrdering) &&
      sPreventBottomToTopChildOrdering) {
    // If either axis is bottom-to-top, we flip both axes (and set a flag
    // so that we can flip some logic to make the reversal transparent).
    if (MainAxisPhysicalStartSide() == eSideBottom ||
        CrossAxisPhysicalStartSide() == eSideBottom) {
      mAreAxesInternallyReversed = true;
      mIsMainAxisReversed = !mIsMainAxisReversed;
      mIsCrossAxisReversed = !mIsCrossAxisReversed;
    }
  }
}

void FlexboxAxisTracker::InitAxesFromLegacyProps(
    const nsFlexContainerFrame* aFlexContainer) {
  const nsStyleXUL* styleXUL = aFlexContainer->StyleXUL();

  const bool boxOrientIsVertical =
      (styleXUL->mBoxOrient == StyleBoxOrient::Vertical);
  const bool wmIsVertical = mWM.IsVertical();

  // If box-orient agrees with our writing-mode, then we're "row-oriented"
  // (i.e. the flexbox main axis is the same as our writing mode's inline
  // direction).  Otherwise, we're column-oriented (i.e. the flexbox's main
  // axis is perpendicular to the writing-mode's inline direction).
  mIsRowOriented = (boxOrientIsVertical == wmIsVertical);
  mMainAxis = mIsRowOriented ? eLogicalAxisInline : eLogicalAxisBlock;

  // Legacy flexbox can use "-webkit-box-direction: reverse" to reverse the
  // main axis (so it runs in the reverse direction of the inline axis):
  mIsMainAxisReversed = styleXUL->mBoxDirection == StyleBoxDirection::Reverse;

  // Legacy flexbox does not support reversing the cross axis -- it has no
  // equivalent of modern flexbox's "flex-wrap: wrap-reverse".
  mIsCrossAxisReversed = false;
}

void FlexboxAxisTracker::InitAxesFromModernProps(
    const nsFlexContainerFrame* aFlexContainer) {
  const nsStylePosition* stylePos = aFlexContainer->StylePosition();
  StyleFlexDirection flexDirection = stylePos->mFlexDirection;

  // Determine main axis:
  switch (flexDirection) {
    case StyleFlexDirection::Row:
      mMainAxis = eLogicalAxisInline;
      mIsRowOriented = true;
      mIsMainAxisReversed = false;
      break;
    case StyleFlexDirection::RowReverse:
      mMainAxis = eLogicalAxisInline;
      mIsRowOriented = true;
      mIsMainAxisReversed = true;
      break;
    case StyleFlexDirection::Column:
      mMainAxis = eLogicalAxisBlock;
      mIsRowOriented = false;
      mIsMainAxisReversed = false;
      break;
    case StyleFlexDirection::ColumnReverse:
      mMainAxis = eLogicalAxisBlock;
      mIsRowOriented = false;
      mIsMainAxisReversed = true;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected flex-direction value");
  }

  // "flex-wrap: wrap-reverse" reverses our cross axis.
  mIsCrossAxisReversed = stylePos->mFlexWrap == StyleFlexWrap::WrapReverse;
}

LogicalSide FlexboxAxisTracker::MainAxisStartSide() const {
  return MakeLogicalSide(
      MainAxis(), mIsMainAxisReversed ? eLogicalEdgeEnd : eLogicalEdgeStart);
}

LogicalSide FlexboxAxisTracker::CrossAxisStartSide() const {
  return MakeLogicalSide(
      CrossAxis(), mIsCrossAxisReversed ? eLogicalEdgeEnd : eLogicalEdgeStart);
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
    const ReflowInput& aReflowInput, nscoord aContentBoxMainSize,
    nscoord aColumnWrapThreshold, const nsTArray<StrutInfo>& aStruts,
    const FlexboxAxisTracker& aAxisTracker, nscoord aMainGapSize,
    bool aHasLineClampEllipsis, nsTArray<nsIFrame*>& aPlaceholders, /* out */
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
    wrapThreshold = aContentBoxMainSize;

    // If the flex container doesn't have a definite content-box main-size
    // (e.g. if main axis is vertical & 'height' is 'auto'), make sure we at
    // least wrap when we hit its max main-size.
    if (wrapThreshold == NS_UNCONSTRAINEDSIZE) {
      const nscoord flexContainerMaxMainSize = GET_MAIN_COMPONENT_LOGICAL(
          aAxisTracker, aAxisTracker.GetWritingMode(),
          aReflowInput.ComputedMaxISize(), aReflowInput.ComputedMaxBSize());

      wrapThreshold = flexContainerMaxMainSize;
    }

    // Also: if we're column-oriented and paginating in the block dimension,
    // we may need to wrap to a new flex line sooner (before we grow past the
    // available BSize, potentially running off the end of the page).
    if (aAxisTracker.IsColumnOriented() &&
        aColumnWrapThreshold != NS_UNCONSTRAINEDSIZE) {
      wrapThreshold = std::min(wrapThreshold, aColumnWrapThreshold);
    }
  }

  // Tracks the index of the next strut, in aStruts (and when this hits
  // aStruts.Length(), that means there are no more struts):
  uint32_t nextStrutIdx = 0;

  // Overall index of the current flex item in the flex container. (This gets
  // checked against entries in aStruts.)
  uint32_t itemIdxInContainer = 0;

  CSSOrderAwareFrameIterator iter(
      this, kPrincipalList, CSSOrderAwareFrameIterator::eIncludeAll,
      CSSOrderAwareFrameIterator::eUnknownOrder, OrderingPropertyForIter(this));

  if (iter.ItemsAreAlreadyInOrder()) {
    AddStateBits(NS_STATE_FLEX_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER);
  } else {
    RemoveStateBits(NS_STATE_FLEX_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER);
  }

  const bool useMozBoxCollapseBehavior =
      ShouldUseMozBoxCollapseBehavior(aReflowInput.mStyleDisplay);

  for (; !iter.AtEnd(); iter.Next()) {
    nsIFrame* childFrame = *iter;
    // Don't create flex items / lines for placeholder frames:
    if (childFrame->IsPlaceholderFrame()) {
      aPlaceholders.AppendElement(childFrame);
      continue;
    }

    // Honor "page-break-before", if we're multi-line and this line isn't empty:
    if (!isSingleLine && !curLine->IsEmpty() &&
        childFrame->StyleDisplay()->BreakBefore()) {
      curLine = ConstructNewFlexLine();
    }

    FlexItem* item;
    if (useMozBoxCollapseBehavior &&
        (StyleVisibility::Collapse ==
         childFrame->StyleVisibility()->mVisible)) {
      // Legacy visibility:collapse behavior: make a 0-sized strut. (No need to
      // bother with aStruts and remembering cross size.)
      item = curLine->Items().EmplaceBack(
          childFrame, 0, aReflowInput.GetWritingMode(), aAxisTracker);
    } else if (nextStrutIdx < aStruts.Length() &&
               aStruts[nextStrutIdx].mItemIdx == itemIdxInContainer) {
      // Use the simplified "strut" FlexItem constructor:
      item = curLine->Items().EmplaceBack(
          childFrame, aStruts[nextStrutIdx].mStrutCrossSize,
          aReflowInput.GetWritingMode(), aAxisTracker);
      nextStrutIdx++;
    } else {
      item = GenerateFlexItemForChild(*curLine, childFrame, aReflowInput,
                                      aAxisTracker, aHasLineClampEllipsis);
    }

    // Check if we need to wrap |item| to a new line, i.e. check if the newly
    // appended outer hypothetical main size pushes our line over the threshold.
    // But we don't wrap if the line-length is unconstrained, nor do we wrap if
    // this was the first item on the line.
    if (wrapThreshold != NS_UNCONSTRAINEDSIZE &&
        curLine->Items().Length() > 1) {
      // If the line will be longer than wrapThreshold because of the newly
      // appended item, then wrap and move the item to a new line.
      //
      // NOTE: We have to account for the fact that the item's outer
      // hypothetical main-size might be huge, if our item is (or contains) a
      // table with "table-layout:fixed". So we use AddChecked() rather than
      // (possibly-overflowing) normal addition, to be sure we don't make the
      // wrong judgement about whether the item fits on this line.
      nscoord newOuterSize = AddChecked(
          curLine->TotalOuterHypotheticalMainSize(), item->OuterMainSize());
      // Account for gap between this line's previous item and this item
      newOuterSize = AddChecked(newOuterSize, aMainGapSize);
      if (newOuterSize == nscoord_MAX || newOuterSize > wrapThreshold) {
        curLine = ConstructNewFlexLine();

        // Get the previous line after adding a new line because the address can
        // change if nsTArray needs to reallocate a new space for the new line.
        FlexLine& prevLine = aLines[aLines.Length() - 2];

        // Move the item from the end of prevLine to the end of curLine.
        item =
            curLine->Items().AppendElement(prevLine.Items().PopLastElement());
      }
    }

    // Update the line's bookkeeping about how large its items collectively are.
    curLine->AddLastItemToMainSizeTotals();

    // Honor "page-break-after", if we're multi-line and have more children:
    if (!isSingleLine && childFrame->GetNextSibling() &&
        childFrame->StyleDisplay()->BreakAfter()) {
      curLine = ConstructNewFlexLine();
    }
    itemIdxInContainer++;
  }

  // If we're transparently reversing axes, then we'll need to reverse the order
  // of our FlexItems and FlexLines, so that the rest of flex layout (with
  // flipped axes) will still produce the correct result.
  if (aAxisTracker.AreAxesInternallyReversed()) {
    for (FlexLine& line : aLines) {
      line.Items().Reverse();
    }
    aLines.Reverse();
  }
}

// Retrieves the content-box main-size of our flex container from the
// reflow input (specifically, the main-size of *this continuation* of the
// flex container).
nscoord nsFlexContainerFrame::GetMainSizeFromReflowInput(
    const ReflowInput& aReflowInput, const FlexboxAxisTracker& aAxisTracker) {
  if (aAxisTracker.IsRowOriented()) {
    // Row-oriented --> our main axis is the inline axis, so our main size
    // is our inline size (which should already be resolved).
    NS_WARNING_ASSERTION(
        aReflowInput.ComputedISize() != NS_UNCONSTRAINEDSIZE,
        "Unconstrained inline size; this should only result from huge sizes "
        "(not intrinsic sizing w/ orthogonal flows)");
    return aReflowInput.ComputedISize();
  }

  // Note: This may be unconstrained, if our block size is "auto":
  return GetEffectiveComputedBSize(aReflowInput);
}

// Returns the largest outer hypothetical main-size of any line in |aLines|.
// (i.e. the hypothetical main-size of the largest line)
static nscoord GetLargestLineMainSize(nsTArray<FlexLine>& aLines) {
  nscoord largestLineOuterSize = 0;
  for (const FlexLine& line : aLines) {
    largestLineOuterSize =
        std::max(largestLineOuterSize, line.TotalOuterHypotheticalMainSize());
  }
  return largestLineOuterSize;
}

nscoord nsFlexContainerFrame::ComputeMainSize(
    const ReflowInput& aReflowInput, const FlexboxAxisTracker& aAxisTracker,
    nscoord aTentativeMainSize, nscoord aAvailableBSizeForContent,
    nsTArray<FlexLine>& aLines, nsReflowStatus& aStatus) const {
  if (aAxisTracker.IsRowOriented()) {
    // Row-oriented --> our main axis is the inline axis, so our main size
    // is our inline size (which should already be resolved).
    return aTentativeMainSize;
  }

  if (aTentativeMainSize != NS_UNCONSTRAINEDSIZE) {
    // Column-oriented case, with fixed BSize:
    if (aAvailableBSizeForContent == NS_UNCONSTRAINEDSIZE ||
        aTentativeMainSize < aAvailableBSizeForContent) {
      // Not in a fragmenting context, OR no need to fragment because we have
      // more available BSize than we need. Either way, we don't need to clamp.
      // (Note that the reflow input has already done the appropriate
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
    aStatus.SetIncomplete();
    nscoord largestLineOuterSize = GetLargestLineMainSize(aLines);

    if (largestLineOuterSize <= aAvailableBSizeForContent) {
      return aAvailableBSizeForContent;
    }
    return std::min(aTentativeMainSize, largestLineOuterSize);
  }

  // Column-oriented case, with size-containment:
  // Behave as if we had no content and just use our MinBSize.
  if (aReflowInput.mStyleDisplay->IsContainSize()) {
    return aReflowInput.ComputedMinBSize();
  }

  // Column-oriented case, with auto BSize:
  // Resolve auto BSize to the largest FlexLine length, clamped to our
  // computed min/max main-size properties.
  // XXXdholbert Handle constrained-aAvailableBSizeForContent case here.
  nscoord largestLineOuterSize = GetLargestLineMainSize(aLines);
  return NS_CSS_MINMAX(largestLineOuterSize, aReflowInput.ComputedMinBSize(),
                       aReflowInput.ComputedMaxBSize());
}

nscoord nsFlexContainerFrame::ComputeCrossSize(
    const ReflowInput& aReflowInput, const FlexboxAxisTracker& aAxisTracker,
    nscoord aSumLineCrossSizes, nscoord aAvailableBSizeForContent,
    bool* aIsDefinite, nsReflowStatus& aStatus) const {
  MOZ_ASSERT(aIsDefinite, "outparam pointer must be non-null");

  if (aAxisTracker.IsColumnOriented()) {
    // Column-oriented --> our cross axis is the inline axis, so our cross size
    // is our inline size (which should already be resolved).
    NS_WARNING_ASSERTION(
        aReflowInput.ComputedISize() != NS_UNCONSTRAINEDSIZE,
        "Unconstrained inline size; this should only result from huge sizes "
        "(not intrinsic sizing w/ orthogonal flows)");
    *aIsDefinite = true;
    return aReflowInput.ComputedISize();
  }

  nscoord effectiveComputedBSize = GetEffectiveComputedBSize(aReflowInput);
  if (effectiveComputedBSize != NS_UNCONSTRAINEDSIZE) {
    // Row-oriented case (cross axis is block-axis), with fixed BSize:
    *aIsDefinite = true;
    if (aAvailableBSizeForContent == NS_UNCONSTRAINEDSIZE ||
        effectiveComputedBSize < aAvailableBSizeForContent) {
      // Not in a fragmenting context, OR no need to fragment because we have
      // more available BSize than we need. Either way, just use our fixed
      // BSize.  (Note that the reflow input has already done the appropriate
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
    aStatus.SetIncomplete();
    if (aSumLineCrossSizes <= aAvailableBSizeForContent) {
      return aAvailableBSizeForContent;
    }
    return std::min(effectiveComputedBSize, aSumLineCrossSizes);
  }

  // Row-oriented case, with size-containment:
  // Behave as if we had no content and just use our MinBSize.
  if (aReflowInput.mStyleDisplay->IsContainSize()) {
    *aIsDefinite = true;
    return aReflowInput.ComputedMinBSize();
  }

  // Row-oriented case (cross axis is block axis), with auto BSize:
  // Shrink-wrap our line(s), subject to our min-size / max-size
  // constraints in that (block) axis.
  // XXXdholbert Handle constrained-aAvailableBSizeForContent case here.
  *aIsDefinite = false;
  return NS_CSS_MINMAX(aSumLineCrossSizes, aReflowInput.ComputedMinBSize(),
                       aReflowInput.ComputedMaxBSize());
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
      MeasureAscentAndBSizeForFlexItem(aItem, aChildReflowInput);

  // Save the sizing info that we learned from this reflow
  // -----------------------------------------------------

  // Tentatively store the child's desired content-box cross-size.
  aItem.SetCrossSize(measurement.BSize());
  aItem.SetAscent(measurement.Ascent());
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

  FLEX_LOG("Reflow() for nsFlexContainerFrame %p", this);

  if (IsFrameTreeTooDeep(aReflowInput, aReflowOutput, aStatus)) {
    return;
  }

  // We (and our children) can only depend on our ancestor's bsize if we have
  // a percent-bsize, or if we're positioned and we have "block-start" and
  // "block-end" set and have block-size:auto.  (There are actually other cases,
  // too -- e.g. if our parent is itself a block-dir flex container and we're
  // flexible -- but we'll let our ancestors handle those sorts of cases.)
  WritingMode wm = aReflowInput.GetWritingMode();
  const nsStylePosition* stylePos = StylePosition();
  const auto& bsize = stylePos->BSize(wm);
  if (bsize.HasPercent() || (StyleDisplay()->IsAbsolutelyPositionedStyle() &&
                             (bsize.IsAuto() || bsize.IsExtremumLength()) &&
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

  const FlexboxAxisTracker axisTracker(this, aReflowInput.GetWritingMode());

  // Check to see if we need to create a computed info structure, to
  // be filled out for use by devtools.
  ComputedFlexContainerInfo* containerInfo = CreateOrClearFlexContainerInfo();

  // We assume we are the last fragment by using
  // PreReflowBlockLevelLogicalSkipSides(). We will skip block-end
  // border/padding when we know our content-box size after DoFlexLayout.
  LogicalMargin borderPadding =
      aReflowInput.ComputedLogicalBorderPadding().ApplySkipSides(
          PreReflowBlockLevelLogicalSkipSides());

  const LogicalSize availableSizeForItems =
      ComputeAvailableSizeForItems(aReflowInput, borderPadding);
  const nscoord availableBSizeForContent = availableSizeForItems.BSize(wm);
  const nscoord columnWrapThreshold = availableSizeForItems.BSize(wm);

  nscoord contentBoxMainSize =
      GetMainSizeFromReflowInput(aReflowInput, axisTracker);
  nscoord contentBoxCrossSize;
  nscoord flexContainerAscent;

  // Calculate gap size for main and cross axis
  nscoord mainGapSize;
  nscoord crossGapSize;
  if (axisTracker.IsRowOriented()) {
    mainGapSize = nsLayoutUtils::ResolveGapToLength(stylePos->mColumnGap,
                                                    contentBoxMainSize);
    crossGapSize = nsLayoutUtils::ResolveGapToLength(
        stylePos->mRowGap, GetEffectiveComputedBSize(aReflowInput));
  } else {
    mainGapSize = nsLayoutUtils::ResolveGapToLength(stylePos->mRowGap,
                                                    contentBoxMainSize);
    NS_WARNING_ASSERTION(aReflowInput.ComputedISize() != NS_UNCONSTRAINEDSIZE,
                         "Unconstrained inline size; this should only result "
                         "from huge sizes (not intrinsic sizing w/ orthogonal "
                         "flows)");
    crossGapSize = nsLayoutUtils::ResolveGapToLength(
        stylePos->mColumnGap, aReflowInput.ComputedISize());
  }

  AutoTArray<FlexLine, 1> lines;
  AutoTArray<StrutInfo, 1> struts;
  AutoTArray<nsIFrame*, 1> placeholders;

  if (!GetPrevInFlow()) {
    DoFlexLayout(aReflowInput, aStatus, contentBoxMainSize, contentBoxCrossSize,
                 flexContainerAscent, availableBSizeForContent,
                 columnWrapThreshold, lines, struts, placeholders, axisTracker,
                 mainGapSize, crossGapSize, hasLineClampEllipsis,
                 containerInfo);

    if (!struts.IsEmpty()) {
      // We're restarting flex layout, with new knowledge of collapsed items.
      aStatus.Reset();
      lines.Clear();
      placeholders.Clear();
      DoFlexLayout(aReflowInput, aStatus, contentBoxMainSize,
                   contentBoxCrossSize, flexContainerAscent,
                   availableBSizeForContent, columnWrapThreshold, lines, struts,
                   placeholders, axisTracker, mainGapSize, crossGapSize,
                   hasLineClampEllipsis, containerInfo);
    }
  } else {
    auto* data = FirstInFlow()->GetProperty(SharedFlexData::Prop());

    // XXX: Pretend we have only one line, with no flex items, and zero main gap
    // size.
    lines.AppendElement(FlexLine(0));

    // After we support flex item fragmentation, we'll create flex items here by
    // using the precomputed SharedFlexData.

    contentBoxMainSize = data->mContentBoxMainSize;
    contentBoxCrossSize = data->mContentBoxCrossSize;
  }

  const LogicalSize contentBoxSize =
      axisTracker.LogicalSizeFromFlexRelativeSizes(contentBoxMainSize,
                                                   contentBoxCrossSize);
  const nscoord consumedBSize = ConsumedBSize(wm);
  const nscoord effectiveContentBSize =
      contentBoxSize.BSize(wm) - consumedBSize;

  // Check if we may need a next-in-flow. If so, we'll need to skip block-end
  // border and padding.
  const bool mayNeedNextInFlow =
      effectiveContentBSize > availableSizeForItems.BSize(wm);
  if (mayNeedNextInFlow) {
    if (aReflowInput.mStyleBorder->mBoxDecorationBreak ==
        StyleBoxDecorationBreak::Slice) {
      borderPadding.BEnd(wm) = 0;
    }
  }

  ReflowChildren(aReflowInput, contentBoxMainSize, contentBoxCrossSize,
                 availableSizeForItems, borderPadding, consumedBSize,
                 flexContainerAscent, lines, placeholders, axisTracker,
                 hasLineClampEllipsis);

  ComputeFinalSize(aReflowOutput, aReflowInput, aStatus, contentBoxMainSize,
                   contentBoxCrossSize, flexContainerAscent, lines,
                   axisTracker);

  // Finally update our line and item measurements in our containerInfo.
  if (MOZ_UNLIKELY(containerInfo)) {
    UpdateFlexLineAndItemInfo(*containerInfo, lines);
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
      data->mLines = std::move(lines);
      data->mContentBoxMainSize = contentBoxMainSize;
      data->mContentBoxCrossSize = contentBoxCrossSize;
    } else if (data) {
      // We are fully-complete, so no next-in-flow is needed. Delete the
      // existing SharedFlexData.
      RemoveProperty(SharedFlexData::Prop());
    }
  }
}

// Class to let us temporarily provide an override value for the the main-size
// CSS property ('width' or 'height') on a flex item, for use in
// nsFrame::ComputeSizeWithIntrinsicDimensions.
// (We could use this overridden size more broadly, too, but it's probably
// better to avoid property-table accesses.  So, where possible, we communicate
// the resolved main-size to the child via modifying its reflow input directly,
// instead of using this class.)
class MOZ_RAII AutoFlexItemMainSizeOverride final {
 public:
  explicit AutoFlexItemMainSizeOverride(
      FlexItem& aItem MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mItemFrame(aItem.Frame()) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    MOZ_ASSERT(!mItemFrame->HasProperty(nsIFrame::FlexItemMainSizeOverride()),
               "FlexItemMainSizeOverride prop shouldn't be set already; "
               "it should only be set temporarily (& not recursively)");
    NS_ASSERTION(aItem.HasIntrinsicRatio(),
                 "This should only be needed for items with an aspect ratio");

    nscoord mainSizeOverrideVal = aItem.MainSize();
    // Note: aItem.MainSize() is the item's *content-box* main-size.  If we
    // have 'box-sizing: border-box', then we have to add our main-axis border
    // and padding in order to produce an appopriate "override" value that
    // gets us the content-box size that we expect.
    if (aItem.Frame()->StylePosition()->mBoxSizing == StyleBoxSizing::Border) {
      mainSizeOverrideVal += aItem.BorderPaddingSizeInMainAxis();
    }

    mItemFrame->SetProperty(nsIFrame::FlexItemMainSizeOverride(),
                            mainSizeOverrideVal);
  }

  ~AutoFlexItemMainSizeOverride() {
    mItemFrame->RemoveProperty(nsIFrame::FlexItemMainSizeOverride());
  }

 private:
  nsIFrame* mItemFrame;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

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
  if (!HasAnyStateBits(NS_STATE_FLEX_GENERATE_COMPUTED_VALUES)) {
    return nullptr;
  }

  // NS_STATE_FLEX_GENERATE_COMPUTED_VALUES will never be cleared. That's
  // acceptable because it's only set in a Chrome API invoked by devtools, and
  // won't impact normal browsing.

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
      nsIFrame* targetFrame = GetFirstNonAnonBoxDescendant(frame);
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
          aAxisTracker.AreAxesInternallyReversed()
              ? aAxisTracker.MainAxisPhysicalEndSide()
              : aAxisTracker.MainAxisPhysicalStartSide());
  aContainerInfo.mCrossAxisDirection =
      ConvertPhysicalStartSideToFlexPhysicalDirection(
          aAxisTracker.AreAxesInternallyReversed()
              ? aAxisTracker.CrossAxisPhysicalEndSide()
              : aAxisTracker.CrossAxisPhysicalStartSide());
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
      nsIFrame* contentFrame = aFrame->GetContentInsertionFrame();
      if (contentFrame && (contentFrame->IsFlexContainerFrame())) {
        flexFrame = static_cast<nsFlexContainerFrame*>(contentFrame);
      }
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
      flexFrame->AddStateBits(NS_STATE_FLEX_GENERATE_COMPUTED_VALUES);
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

void nsFlexContainerFrame::DoFlexLayout(
    const ReflowInput& aReflowInput, nsReflowStatus& aStatus,
    nscoord& aContentBoxMainSize, nscoord& aContentBoxCrossSize,
    nscoord& aFlexContainerAscent, nscoord aAvailableBSizeForContent,
    nscoord aColumnWrapThreshold, nsTArray<FlexLine>& aLines,
    nsTArray<StrutInfo>& aStruts, nsTArray<nsIFrame*>& aPlaceholders,
    const FlexboxAxisTracker& aAxisTracker, nscoord aMainGapSize,
    nscoord aCrossGapSize, bool aHasLineClampEllipsis,
    ComputedFlexContainerInfo* const aContainerInfo) {
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  MOZ_ASSERT(aLines.IsEmpty(), "Caller should pass an empty array for lines!");
  MOZ_ASSERT(aPlaceholders.IsEmpty(),
             "Caller should pass an empty array for placeholders!");

  GenerateFlexLines(aReflowInput, aContentBoxMainSize, aColumnWrapThreshold,
                    aStruts, aAxisTracker, aMainGapSize, aHasLineClampEllipsis,
                    aPlaceholders, aLines);

  if ((aLines.Length() == 1 && aLines[0].IsEmpty()) ||
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
    MOZ_ASSERT(HasAnyStateBits(NS_STATE_FLEX_GENERATE_COMPUTED_VALUES),
               "We should only have the info struct if "
               "NS_STATE_FLEX_GENERATE_COMPUTED_VALUES state bit is set!");

    if (!aStruts.IsEmpty()) {
      // We restarted DoFlexLayout, and may have stale mLines to clear:
      aContainerInfo->mLines.Clear();
    } else {
      MOZ_ASSERT(aContainerInfo->mLines.IsEmpty(), "Shouldn't have lines yet.");
    }

    CreateFlexLineAndFlexItemInfo(*aContainerInfo, aLines);
    ComputeFlexDirections(*aContainerInfo, aAxisTracker);
  }

  aContentBoxMainSize =
      ComputeMainSize(aReflowInput, aAxisTracker, aContentBoxMainSize,
                      aAvailableBSizeForContent, aLines, aStatus);

  uint32_t lineIndex = 0;
  for (FlexLine& line : aLines) {
    ComputedFlexLineInfo* lineInfo =
        aContainerInfo ? &aContainerInfo->mLines[lineIndex] : nullptr;
    line.ResolveFlexibleLengths(aContentBoxMainSize, lineInfo);
    ++lineIndex;
  }

  // Cross Size Determination - Flexbox spec section 9.4
  // https://drafts.csswg.org/css-flexbox-1/#cross-sizing
  // ===================================================
  // Calculate the hypothetical cross size of each item:

  // 'sumLineCrossSizes' includes the size of all gaps between lines. We
  // initialize it with the sum of all the gaps, and add each line's cross size
  // at the end of the following for-loop.
  nscoord sumLineCrossSizes = aCrossGapSize * (aLines.Length() - 1);
  for (FlexLine& line : aLines) {
    for (FlexItem& item : line.Items()) {
      // The item may already have the correct cross-size; only recalculate
      // if the item's main size resolution (flexing) could have influenced it:
      if (item.CanMainSizeInfluenceCrossSize()) {
        Maybe<AutoFlexItemMainSizeOverride> sizeOverride;
        if (item.HasIntrinsicRatio()) {
          // For flex items with an aspect ratio, we have to impose an override
          // for the main-size property *before* we even instantiate the reflow
          // input, in order for aspect ratio calculations to produce the right
          // cross size in the reflow input. (For other flex items, it's OK
          // (and cheaper) to impose our main size *after* the reflow input has
          // been constructed, since the main size shouldn't influence anything
          // about cross-size measurement until we actually reflow the child.)
          sizeOverride.emplace(item);
        }

        WritingMode wm = item.Frame()->GetWritingMode();
        LogicalSize availSize = aReflowInput.ComputedSize(wm);
        availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
        ReflowInput childReflowInput(PresContext(), aReflowInput, item.Frame(),
                                     availSize);
        childReflowInput.mFlags.mInsideLineClamp = GetLineClampValue() != 0;
        if (!sizeOverride) {
          // Directly override the computed main-size, by tweaking reflow input:
          if (item.IsInlineAxisMainAxis()) {
            childReflowInput.SetComputedISize(item.MainSize());
          } else {
            childReflowInput.SetComputedBSize(item.MainSize());
            if (item.TreatBSizeAsIndefinite()) {
              childReflowInput.mFlags.mTreatBSizeAsIndefinite = true;
            }
          }
        }

        SizeItemInCrossAxis(childReflowInput, item);
      }
    }
    // Now that we've finished with this line's items, size the line itself:
    line.ComputeCrossSizeAndBaseline(aAxisTracker);
    sumLineCrossSizes += line.LineCrossSize();
  }

  bool isCrossSizeDefinite;
  aContentBoxCrossSize = ComputeCrossSize(
      aReflowInput, aAxisTracker, sumLineCrossSizes, aAvailableBSizeForContent,
      &isCrossSizeDefinite, aStatus);

  // Set up state for cross-axis alignment, at a high level (outside the
  // scope of a particular flex line)
  CrossAxisPositionTracker crossAxisPosnTracker(
      aLines, aReflowInput, aContentBoxCrossSize, isCrossSizeDefinite,
      aAxisTracker, aCrossGapSize);

  // Now that we know the cross size of each line (including
  // "align-content:stretch" adjustments, from the CrossAxisPositionTracker
  // constructor), we can create struts for any flex items with
  // "visibility: collapse" (and restart flex layout).
  if (aStruts.IsEmpty() &&  // (Don't make struts if we already did)
      !ShouldUseMozBoxCollapseBehavior(aReflowInput.mStyleDisplay)) {
    BuildStrutInfoFromCollapsedItems(aLines, aStruts);
    if (!aStruts.IsEmpty()) {
      // Restart flex layout, using our struts.
      return;
    }
  }

  // If the container should derive its baseline from the first FlexLine,
  // do that here (while crossAxisPosnTracker is conveniently pointing
  // at the cross-start edge of that line, which the line's baseline offset is
  // measured from):
  if (!aAxisTracker.AreAxesInternallyReversed()) {
    nscoord firstLineBaselineOffset = aLines[0].FirstBaselineOffset();
    if (firstLineBaselineOffset == nscoord_MIN) {
      // No baseline-aligned items in line. Use sentinel value to prompt us to
      // get baseline from the first FlexItem after we've reflowed it.
      aFlexContainerAscent = nscoord_MIN;
    } else {
      aFlexContainerAscent = ComputePhysicalAscentFromFlexRelativeAscent(
          crossAxisPosnTracker.Position() + firstLineBaselineOffset,
          aContentBoxCrossSize, aReflowInput, aAxisTracker);
    }
  }

  const auto justifyContent =
      IsLegacyBox(aReflowInput.mFrame)
          ? ConvertLegacyStyleToJustifyContent(StyleXUL())
          : aReflowInput.mStylePosition->mJustifyContent;

  // Recalculate the gap sizes if necessary now that the container size has
  // been determined.
  if (aReflowInput.ComputedBSize() == NS_UNCONSTRAINEDSIZE &&
      aReflowInput.mStylePosition->mRowGap.IsLengthPercentage() &&
      aReflowInput.mStylePosition->mRowGap.AsLengthPercentage().HasPercent()) {
    bool rowIsCross = aAxisTracker.IsRowOriented();
    nscoord newBlockGapSize = nsLayoutUtils::ResolveGapToLength(
        aReflowInput.mStylePosition->mRowGap,
        rowIsCross ? aContentBoxCrossSize : aContentBoxMainSize);
    if (rowIsCross) {
      crossAxisPosnTracker.SetCrossGapSize(newBlockGapSize);
    } else {
      for (FlexLine& line : aLines) {
        line.SetMainGapSize(newBlockGapSize);
      }
    }
  }

  lineIndex = 0;
  for (FlexLine& line : aLines) {
    // Main-Axis Alignment - Flexbox spec section 9.5
    // https://drafts.csswg.org/css-flexbox-1/#main-alignment
    // ==============================================
    line.PositionItemsInMainAxis(justifyContent, aContentBoxMainSize,
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

    if (&line != &aLines.LastElement()) {
      crossAxisPosnTracker.TraverseGap();
    }
    ++lineIndex;
  }

  // If the container should derive its baseline from the last FlexLine,
  // do that here (while crossAxisPosnTracker is conveniently pointing
  // at the cross-end edge of that line, which the line's baseline offset is
  // measured from):
  if (aAxisTracker.AreAxesInternallyReversed()) {
    nscoord lastLineBaselineOffset = aLines.LastElement().FirstBaselineOffset();
    if (lastLineBaselineOffset == nscoord_MIN) {
      // No baseline-aligned items in line. Use sentinel value to prompt us to
      // get baseline from the last FlexItem after we've reflowed it.
      aFlexContainerAscent = nscoord_MIN;
    } else {
      aFlexContainerAscent = ComputePhysicalAscentFromFlexRelativeAscent(
          crossAxisPosnTracker.Position() - lastLineBaselineOffset,
          aContentBoxCrossSize, aReflowInput, aAxisTracker);
    }
  }
}

void nsFlexContainerFrame::ReflowChildren(
    const ReflowInput& aReflowInput, const nscoord aContentBoxMainSize,
    const nscoord aContentBoxCrossSize,
    const LogicalSize& aAvailableSizeForItems,
    const LogicalMargin& aBorderPadding, const nscoord aConsumedBSize,
    nscoord& aFlexContainerAscent, nsTArray<FlexLine>& aLines,
    nsTArray<nsIFrame*>& aPlaceholders, const FlexboxAxisTracker& aAxisTracker,
    bool aHasLineClampEllipsis) {
  // Before giving each child a final reflow, calculate the origin of the
  // flex container's content box (with respect to its border-box), so that
  // we can compute our flex item's final positions.
  WritingMode flexWM = aReflowInput.GetWritingMode();
  const LogicalPoint containerContentBoxOrigin(
      flexWM, aBorderPadding.IStart(flexWM), aBorderPadding.BStart(flexWM));

  // Determine flex container's border-box size (used in positioning children):
  LogicalSize logSize = aAxisTracker.LogicalSizeFromFlexRelativeSizes(
      aContentBoxMainSize, aContentBoxCrossSize);
  logSize += aBorderPadding.Size(flexWM);
  nsSize containerSize = logSize.GetPhysicalSize(flexWM);

  // If the flex container has no baseline-aligned items, it will use this item
  // (the first item, discounting any under-the-hood reversing that we've done)
  // to determine its baseline:
  const FlexItem* firstItem =
      aAxisTracker.AreAxesInternallyReversed()
          ? (aLines.LastElement().IsEmpty() ? nullptr
                                            : &aLines.LastElement().LastItem())
          : (aLines[0].IsEmpty() ? nullptr : &aLines[0].FirstItem());

  // FINAL REFLOW: Give each child frame another chance to reflow, now that
  // we know its final size and position.
  for (const FlexLine& line : aLines) {
    for (const FlexItem& item : line.Items()) {
      LogicalPoint framePos = aAxisTracker.LogicalPointFromFlexRelativePoint(
          item.MainPosition(), item.CrossPosition(), aContentBoxMainSize,
          aContentBoxCrossSize);
      // Adjust framePos to be relative to the container's border-box
      // (i.e. its frame rect), instead of the container's content-box:
      framePos += containerContentBoxOrigin;

      // XXX: We need to subtract aConsumedBSize from framePos.B(flewm) after we
      // support flex item fragmentation.

      // (Intentionally snapshotting this before ApplyRelativePositioning, to
      // maybe use for setting the flex container's baseline.)
      const nscoord itemNormalBPos = framePos.B(flexWM);

      // Check if we actually need to reflow the item -- if we already reflowed
      // it with the right content-box size, and there is no need to do a reflow
      // to clear out a -webkit-line-clamp ellipsis, we can just reposition it
      // as-needed.
      if (item.NeedsFinalReflow()) {
        // The available size must be in item's writing-mode.
        // XXX: The correct available block-size is from the position where the
        // flex item is placed to the end of the available block-size.
        const WritingMode itemWM = item.GetWritingMode();
        LogicalSize availableSize =
            aAvailableSizeForItems.ConvertTo(itemWM, flexWM);

        // XXX: Unconditionally give our children unconstrained block-size until
        // we support flex item fragmentation.
        availableSize.BSize(itemWM) = NS_UNCONSTRAINEDSIZE;

        const nsReflowStatus childReflowStatus =
            ReflowFlexItem(aAxisTracker, aReflowInput, item, framePos,
                           availableSize, containerSize, aHasLineClampEllipsis);

        // XXX: Silence the unused childReflowStatus warning in opt build for
        // now.
        Unused << childReflowStatus;

        // XXXdholbert Once we do pagination / splitting, we'll need to actually
        // handle incomplete childReflowStatuses. But for now, we give our kids
        // unconstrained available height, which means they should always
        // complete.
        MOZ_ASSERT(childReflowStatus.IsComplete(),
                   "We gave flex item unconstrained available height, so it "
                   "should be complete");
      } else {
        MoveFlexItemToFinalPosition(aReflowInput, item, framePos,
                                    containerSize);
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
      if (&item == firstItem && aFlexContainerAscent == nscoord_MIN) {
        aFlexContainerAscent = itemNormalBPos + item.ResolvedAscent(true);
      }
    }
  }

  if (!aPlaceholders.IsEmpty()) {
    ReflowPlaceholders(aReflowInput, aPlaceholders, containerContentBoxOrigin,
                       containerSize);
  }
}

void nsFlexContainerFrame::ComputeFinalSize(
    ReflowOutput& aReflowOutput, const ReflowInput& aReflowInput,
    nsReflowStatus& aStatus, const nscoord aContentBoxMainSize,
    const nscoord aContentBoxCrossSize, nscoord aFlexContainerAscent,
    nsTArray<FlexLine>& aLines, const FlexboxAxisTracker& aAxisTracker) {
  // XXXTYLin: Can we prepare side-skipped border and padding in Reflow() and
  // pass it down here?
  const WritingMode flexWM = aReflowInput.GetWritingMode();
  LogicalMargin containerBP = aReflowInput.ComputedLogicalBorderPadding();

  // Unconditionally skip block-end border & padding for now, regardless of
  // writing-mode/GetLogicalSkipSides.  We add it lower down, after we've
  // established baseline and decided whether bottom border-padding fits (if
  // we're fragmented).
  const nscoord blockEndContainerBP = containerBP.BEnd(flexWM);
  const LogicalSides skipSides =
      GetLogicalSkipSides(&aReflowInput) | LogicalSides(eLogicalSideBitsBEnd);
  containerBP.ApplySkipSides(skipSides);

  // Compute flex container's reflow-output measurements (in its own
  // writing-mode), starting w/ content-box size & growing from there:
  LogicalSize reflowOutputInFlexWM =
      aAxisTracker.LogicalSizeFromFlexRelativeSizes(aContentBoxMainSize,
                                                    aContentBoxCrossSize);
  // Add border/padding (w/ skipSides already applied):
  reflowOutputInFlexWM.ISize(flexWM) += containerBP.IStartEnd(flexWM);
  reflowOutputInFlexWM.BSize(flexWM) += containerBP.BStartEnd(flexWM);

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
    aFlexContainerAscent = reflowOutputInFlexWM.BSize(flexWM);
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

  // Now: If we're complete, add bottom border/padding to desired height (which
  // we skipped via skipSides) -- unless that pushes us over available height,
  // in which case we become incomplete (unless we already weren't asking for
  // any height, in which case we stay complete to avoid looping forever).
  // NOTE: If we're auto-height, we allow our bottom border/padding to push us
  // over the available height without requesting a continuation, for
  // consistency with the behavior of "display:block" elements.
  if (aStatus.IsComplete()) {
    nscoord desiredBSizeWithBEndBP =
        reflowOutputInFlexWM.BSize(flexWM) + blockEndContainerBP;

    if (aReflowInput.AvailableBSize() == NS_UNCONSTRAINEDSIZE ||
        reflowOutputInFlexWM.BSize(flexWM) == 0 ||
        desiredBSizeWithBEndBP <= aReflowInput.AvailableBSize() ||
        aReflowInput.ComputedBSize() == NS_UNCONSTRAINEDSIZE) {
      // Update desired height to include block-end border/padding
      reflowOutputInFlexWM.BSize(flexWM) = desiredBSizeWithBEndBP;
    } else {
      // We couldn't fit bottom border/padding, so we'll need a continuation.
      aStatus.SetIncomplete();
    }
  }

  // Calculate the container baselines so that our parent can baseline-align us.
  mBaselineFromLastReflow = aFlexContainerAscent;
  mLastBaselineFromLastReflow = aLines.LastElement().LastBaselineOffset();
  if (mLastBaselineFromLastReflow == nscoord_MIN) {
    // XXX we fall back to a mirrored first baseline here for now, but this
    // should probably use the last baseline of the last item or something.
    mLastBaselineFromLastReflow =
        reflowOutputInFlexWM.BSize(flexWM) - aFlexContainerAscent;
  }

  // Convert flex container's final reflow-output measurements to parent's WM,
  // for outparam.
  aReflowOutput.SetSize(flexWM, reflowOutputInFlexWM);

  // Overflow area = union(my overflow area, kids' overflow areas)
  aReflowOutput.SetOverflowAreasToDesiredBounds();
  for (nsIFrame* childFrame : mFrames) {
    ConsiderChildOverflow(aReflowOutput.mOverflowAreas, childFrame);
  }

  FinishReflowWithAbsoluteFrames(PresContext(), aReflowOutput, aReflowInput,
                                 aStatus);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aReflowOutput)
}

void nsFlexContainerFrame::MoveFlexItemToFinalPosition(
    const ReflowInput& aReflowInput, const FlexItem& aItem,
    LogicalPoint& aFramePos, const nsSize& aContainerSize) {
  WritingMode outerWM = aReflowInput.GetWritingMode();

  // If item is relpos, look up its offsets (cached from prev reflow)
  LogicalMargin logicalOffsets(outerWM);
  if (StylePositionProperty::Relative ==
      aItem.Frame()->StyleDisplay()->mPosition) {
    nsMargin* cachedOffsets =
        aItem.Frame()->GetProperty(nsIFrame::ComputedOffsetProperty());
    MOZ_ASSERT(cachedOffsets,
               "relpos previously-reflowed frame should've cached its offsets");
    logicalOffsets = LogicalMargin(outerWM, *cachedOffsets);
  }
  ReflowInput::ApplyRelativePositioning(aItem.Frame(), outerWM, logicalOffsets,
                                        &aFramePos, aContainerSize);
  aItem.Frame()->SetPosition(outerWM, aFramePos, aContainerSize);
  PositionFrameView(aItem.Frame());
  PositionChildViews(aItem.Frame());
}

nsReflowStatus nsFlexContainerFrame::ReflowFlexItem(
    const FlexboxAxisTracker& aAxisTracker, const ReflowInput& aReflowInput,
    const FlexItem& aItem, LogicalPoint& aFramePos,
    const LogicalSize& aAvailableSize, const nsSize& aContainerSize,
    bool aHasLineClampEllipsis) {
  WritingMode outerWM = aReflowInput.GetWritingMode();
  ReflowInput childReflowInput(PresContext(), aReflowInput, aItem.Frame(),
                               aAvailableSize);
  childReflowInput.mFlags.mInsideLineClamp = GetLineClampValue() != 0;
  // This is the final reflow of this flex item; if we previously had a
  // -webkit-line-clamp, and we missed our chance to clear the ellipsis
  // because we didn't need to call MeasureFlexItemContentBSize, we set
  // mApplyLineClamp to cause it to get cleared here.
  childReflowInput.mFlags.mApplyLineClamp =
      !childReflowInput.mFlags.mInsideLineClamp && aHasLineClampEllipsis;

  // Keep track of whether we've overriden the child's computed ISize
  // and/or BSize, so we can set its resize flags accordingly.
  bool didOverrideComputedISize = false;
  bool didOverrideComputedBSize = false;

  // Override computed main-size
  if (aItem.IsInlineAxisMainAxis()) {
    childReflowInput.SetComputedISize(aItem.MainSize());
    didOverrideComputedISize = true;
  } else {
    childReflowInput.SetComputedBSize(aItem.MainSize());
    didOverrideComputedBSize = true;
    if (aItem.TreatBSizeAsIndefinite()) {
      childReflowInput.mFlags.mTreatBSizeAsIndefinite = true;
    }
  }

  // Override reflow input's computed cross-size if either:
  // - the item was stretched (in which case we're imposing a cross size)
  // ...or...
  // - the item it has an aspect ratio (in which case the cross-size that's
  // currently in the reflow input is based on arithmetic involving a stale
  // main-size value that we just stomped on above). (Note that we could handle
  // this case using an AutoFlexItemMainSizeOverride, as we do elsewhere; but
  // given that we *already know* the correct cross size to use here, it's
  // cheaper to just directly set it instead of setting a frame property.)
  if (aItem.IsStretched() || aItem.HasIntrinsicRatio()) {
    if (aItem.IsInlineAxisCrossAxis()) {
      childReflowInput.SetComputedISize(aItem.CrossSize());
      didOverrideComputedISize = true;
    } else {
      // Note that in the above cases we don't need to worry about the BSize
      // needing to be treated as indefinite, because this is for cases where
      // the block size would always be considered definite (or where its
      // definiteness would be irrelevant).
      childReflowInput.SetComputedBSize(aItem.CrossSize());
      didOverrideComputedBSize = true;
    }
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

  // If we're overriding the computed width or height, *and* we had an
  // earlier "measuring" reflow, then this upcoming reflow needs to be
  // treated as a resize.
  if (aItem.HadMeasuringReflow()) {
    if (didOverrideComputedISize) {
      // (This is somewhat redundant, since ReflowInput::InitResizeFlags()
      // already calls SetIResize() whenever our computed ISize has changed
      // since the previous reflow. Still, it's nice for symmetry, and it might
      // be necessary for some edge cases.)
      childReflowInput.SetIResize(true);
    }
    if (didOverrideComputedBSize) {
      childReflowInput.SetBResize(true);
      childReflowInput.mFlags.mIsBResizeForPercentages = true;
    }
  }
  // NOTE: Be very careful about doing anything else with childReflowInput
  // after this point, because some of its methods (e.g. SetComputedWidth)
  // internally call InitResizeFlags and stomp on mVResize & mHResize.

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
    ReflowOutput childReflowOutput(childReflowInput);
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

nscoord nsFlexContainerFrame::IntrinsicISize(
    gfxContext* aRenderingContext, nsLayoutUtils::IntrinsicISizeType aType) {
  nscoord containerISize = 0;
  const nsStylePosition* stylePos = StylePosition();
  const FlexboxAxisTracker axisTracker(this, GetWritingMode());

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
          (isSingleLine || aType == nsLayoutUtils::PREF_ISIZE)) {
        containerISize += childISize;
        if (!onFirstChild) {
          containerISize += mainGapSize;
        }
        onFirstChild = false;
      } else {  // (col-oriented, or MIN_ISIZE for multi-line row flex
                // container)
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
        StyleDisplay()->IsContainSize()
            ? 0
            : IntrinsicISize(aRenderingContext, nsLayoutUtils::MIN_ISIZE);
  }

  return mCachedMinISize;
}

/* virtual */
nscoord nsFlexContainerFrame::GetPrefISize(gfxContext* aRenderingContext) {
  DISPLAY_PREF_INLINE_SIZE(this, mCachedPrefISize);
  if (mCachedPrefISize == NS_INTRINSIC_ISIZE_UNKNOWN) {
    mCachedPrefISize =
        StyleDisplay()->IsContainSize()
            ? 0
            : IntrinsicISize(aRenderingContext, nsLayoutUtils::PREF_ISIZE);
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
