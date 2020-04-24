/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: grid | inline-grid" */

#include "nsGridContainerFrame.h"

#include <functional>
#include <limits>
#include <stdlib.h>  // for div()
#include <type_traits>
#include "gfxContext.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/CSSAlignUtils.h"
#include "mozilla/CSSOrderAwareFrameIterator.h"
#include "mozilla/dom/GridBinding.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/Maybe.h"
#include "mozilla/PodOperations.h"  // for PodZero
#include "mozilla/Poison.h"
#include "mozilla/PresShell.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsAlgorithm.h"  // for clamped()
#include "nsBoxLayoutState.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSFrameConstructor.h"
#include "nsDataHashtable.h"
#include "nsDisplayList.h"
#include "nsHashKeys.h"
#include "nsFieldSetFrame.h"
#include "nsIFrameInlines.h"
#include "nsPresContext.h"
#include "nsReadableUtils.h"
#include "nsTableWrapperFrame.h"

using namespace mozilla;

typedef nsAbsoluteContainingBlock::AbsPosReflowFlags AbsPosReflowFlags;
typedef nsGridContainerFrame::TrackSize TrackSize;
typedef mozilla::CSSAlignUtils::AlignJustifyFlags AlignJustifyFlags;
typedef nsLayoutUtils::IntrinsicISizeType IntrinsicISizeType;

using GridTemplate = StyleGridTemplateComponent;
using TrackListValue =
    StyleGenericTrackListValue<LengthPercentage, StyleInteger>;
using TrackRepeat = StyleGenericTrackRepeat<LengthPercentage, StyleInteger>;
using NameList = StyleOwnedSlice<StyleCustomIdent>;

static const int32_t kMaxLine = StyleMAX_GRID_LINE;
static const int32_t kMinLine = StyleMIN_GRID_LINE;
// The maximum line number, in the zero-based translated grid.
static const uint32_t kTranslatedMaxLine = uint32_t(kMaxLine - kMinLine);
static const uint32_t kAutoLine = kTranslatedMaxLine + 3457U;

static const nsFrameState kIsSubgridBits =
    (NS_STATE_GRID_IS_COL_SUBGRID | NS_STATE_GRID_IS_ROW_SUBGRID);

// https://drafts.csswg.org/css-sizing/#constraints
enum class SizingConstraint {
  MinContent,   // sizing under min-content constraint
  MaxContent,   // sizing under max-content constraint
  NoConstraint  // no constraint, used during Reflow
};

namespace mozilla {

template <>
inline Span<const StyleOwnedSlice<StyleCustomIdent>>
GridTemplate::LineNameLists(bool aIsSubgrid) const {
  if (IsTrackList()) {
    return AsTrackList()->line_names.AsSpan();
  }
  if (IsSubgrid() && aIsSubgrid) {
    return AsSubgrid()->names.AsSpan();
  }
  MOZ_ASSERT(IsNone() || (IsSubgrid() && !aIsSubgrid));
  return {};
}

template <>
inline const StyleTrackBreadth& StyleTrackSize::GetMax() const {
  if (IsBreadth()) {
    return AsBreadth();
  }
  if (IsMinmax()) {
    return AsMinmax()._1;
  }
  MOZ_ASSERT(IsFitContent());
  return AsFitContent();
}

template <>
inline const StyleTrackBreadth& StyleTrackSize::GetMin() const {
  static const StyleTrackBreadth kAuto = StyleTrackBreadth::Auto();
  if (IsBreadth()) {
    // <flex> behaves like minmax(auto, <flex>)
    return AsBreadth().IsFr() ? kAuto : AsBreadth();
  }
  if (IsMinmax()) {
    return AsMinmax()._0;
  }
  MOZ_ASSERT(IsFitContent());
  return kAuto;
}

}  // namespace mozilla

static nscoord ClampToCSSMaxBSize(nscoord aSize,
                                  const ReflowInput* aReflowInput) {
  auto maxSize = aReflowInput->ComputedMaxBSize();
  if (MOZ_UNLIKELY(maxSize != NS_UNCONSTRAINEDSIZE)) {
    MOZ_ASSERT(aReflowInput->ComputedMinBSize() <= maxSize);
    aSize = std::min(aSize, maxSize);
  }
  return aSize;
}

// Same as above and set aStatus INCOMPLETE if aSize wasn't clamped.
// (If we clamp aSize it means our size is less than the break point,
// i.e. we're effectively breaking in our overflow, so we should leave
// aStatus as is (it will likely be set to OVERFLOW_INCOMPLETE later)).
static nscoord ClampToCSSMaxBSize(nscoord aSize,
                                  const ReflowInput* aReflowInput,
                                  nsReflowStatus* aStatus) {
  auto maxSize = aReflowInput->ComputedMaxBSize();
  if (MOZ_UNLIKELY(maxSize != NS_UNCONSTRAINEDSIZE)) {
    MOZ_ASSERT(aReflowInput->ComputedMinBSize() <= maxSize);
    if (aSize < maxSize) {
      aStatus->SetIncomplete();
    } else {
      aSize = maxSize;
    }
  } else {
    aStatus->SetIncomplete();
  }
  return aSize;
}

template <typename Size>
static bool IsPercentOfIndefiniteSize(const Size& aCoord,
                                      nscoord aPercentBasis) {
  return aPercentBasis == NS_UNCONSTRAINEDSIZE && aCoord.HasPercent();
}

static nscoord ResolveToDefiniteSize(const StyleTrackBreadth& aBreadth,
                                     nscoord aPercentBasis) {
  MOZ_ASSERT(aBreadth.IsBreadth());
  if (::IsPercentOfIndefiniteSize(aBreadth.AsBreadth(), aPercentBasis)) {
    return nscoord(0);
  }
  return std::max(nscoord(0), aBreadth.AsBreadth().Resolve(aPercentBasis));
}

// Synthesize a baseline from a border box.  For an alphabetical baseline
// this is the end edge of the border box.  For a central baseline it's
// the center of the border box.
// https://drafts.csswg.org/css-align-3/#synthesize-baselines
// For a 'first baseline' the measure is from the border-box start edge and
// for a 'last baseline' the measure is from the border-box end edge.
static nscoord SynthesizeBaselineFromBorderBox(BaselineSharingGroup aGroup,
                                               WritingMode aWM,
                                               nscoord aBorderBoxSize) {
  if (aGroup == BaselineSharingGroup::First) {
    return aWM.IsAlphabeticalBaseline() ? aBorderBoxSize : aBorderBoxSize / 2;
  }
  MOZ_ASSERT(aGroup == BaselineSharingGroup::Last);
  // Round up for central baseline offset, to be consistent with eFirst.
  return aWM.IsAlphabeticalBaseline()
             ? 0
             : (aBorderBoxSize / 2) + (aBorderBoxSize % 2);
}

// The input sizes for calculating the number of repeat(auto-fill/fit) tracks.
// https://drafts.csswg.org/css-grid/#auto-repeat
struct RepeatTrackSizingInput {
  explicit RepeatTrackSizingInput(WritingMode aWM)
      : mMin(aWM, 0, 0),
        mSize(aWM, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE),
        mMax(aWM, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE) {}
  RepeatTrackSizingInput(const LogicalSize& aMin, const LogicalSize& aSize,
                         const LogicalSize& aMax)
      : mMin(aMin), mSize(aSize), mMax(aMax) {}

  // This should be used in intrinsic sizing (i.e. when we can't initialize
  // the sizes directly from ReflowInput values).
  void InitFromStyle(LogicalAxis aAxis, WritingMode aWM,
                     const ComputedStyle* aStyle) {
    const auto& pos = aStyle->StylePosition();
    const bool borderBoxSizing = pos->mBoxSizing == StyleBoxSizing::Border;
    nscoord bp = NS_UNCONSTRAINEDSIZE;  // a sentinel to calculate it only once
    auto adjustForBoxSizing = [borderBoxSizing, aWM, aAxis, aStyle,
                               &bp](nscoord aSize) {
      if (!borderBoxSizing) {
        return aSize;
      }
      if (bp == NS_UNCONSTRAINEDSIZE) {
        const auto& padding = aStyle->StylePadding()->mPadding;
        LogicalMargin border(aWM, aStyle->StyleBorder()->GetComputedBorder());
        // We can use zero percentage basis since this is only called from
        // intrinsic sizing code.
        const nscoord percentageBasis = 0;
        if (aAxis == eLogicalAxisInline) {
          bp = std::max(padding.GetIStart(aWM).Resolve(percentageBasis), 0) +
               std::max(padding.GetIEnd(aWM).Resolve(percentageBasis), 0) +
               border.IStartEnd(aWM);
        } else {
          bp = std::max(padding.GetBStart(aWM).Resolve(percentageBasis), 0) +
               std::max(padding.GetBEnd(aWM).Resolve(percentageBasis), 0) +
               border.BStartEnd(aWM);
        }
      }
      return std::max(aSize - bp, 0);
    };
    nscoord& min = mMin.Size(aAxis, aWM);
    nscoord& size = mSize.Size(aAxis, aWM);
    nscoord& max = mMax.Size(aAxis, aWM);
    const auto& minCoord =
        aAxis == eLogicalAxisInline ? pos->MinISize(aWM) : pos->MinBSize(aWM);
    if (minCoord.ConvertsToLength()) {
      min = adjustForBoxSizing(minCoord.ToLength());
    }
    const auto& maxCoord =
        aAxis == eLogicalAxisInline ? pos->MaxISize(aWM) : pos->MaxBSize(aWM);
    if (maxCoord.ConvertsToLength()) {
      max = std::max(min, adjustForBoxSizing(maxCoord.ToLength()));
    }
    const auto& sizeCoord =
        aAxis == eLogicalAxisInline ? pos->ISize(aWM) : pos->BSize(aWM);
    if (sizeCoord.ConvertsToLength()) {
      size = Clamp(adjustForBoxSizing(sizeCoord.ToLength()), min, max);
    }
  }

  LogicalSize mMin;
  LogicalSize mSize;
  LogicalSize mMax;
};

enum class GridLineSide {
  BeforeGridGap,
  AfterGridGap,
};

struct nsGridContainerFrame::TrackSize {
  enum StateBits : uint16_t {
    // clang-format off
    eAutoMinSizing =              0x1,
    eMinContentMinSizing =        0x2,
    eMaxContentMinSizing =        0x4,
    eMinOrMaxContentMinSizing = eMinContentMinSizing | eMaxContentMinSizing,
    eIntrinsicMinSizing = eMinOrMaxContentMinSizing | eAutoMinSizing,
    eModified =                   0x8,
    eAutoMaxSizing =             0x10,
    eMinContentMaxSizing =       0x20,
    eMaxContentMaxSizing =       0x40,
    eAutoOrMaxContentMaxSizing = eAutoMaxSizing | eMaxContentMaxSizing,
    eIntrinsicMaxSizing = eAutoOrMaxContentMaxSizing | eMinContentMaxSizing,
    eFlexMaxSizing =             0x80,
    eFrozen =                   0x100,
    eSkipGrowUnlimited1 =       0x200,
    eSkipGrowUnlimited2 =       0x400,
    eSkipGrowUnlimited = eSkipGrowUnlimited1 | eSkipGrowUnlimited2,
    eBreakBefore =              0x800,
    eFitContent =              0x1000,
    eInfinitelyGrowable =      0x2000,
    // clang-format on
  };

  StateBits Initialize(nscoord aPercentageBasis, const StyleTrackSize&);
  bool IsFrozen() const { return mState & eFrozen; }
#ifdef DEBUG
  void Dump() const;
#endif

  static bool IsDefiniteMaxSizing(StateBits aStateBits) {
    return (aStateBits & (eIntrinsicMaxSizing | eFlexMaxSizing)) == 0;
  }

  nscoord mBase;
  nscoord mLimit;
  nscoord mPosition;  // zero until we apply 'align/justify-content'
  // mBaselineSubtreeSize is the size of a baseline-aligned subtree within
  // this track.  One subtree per baseline-sharing group (per track).
  PerBaseline<nscoord> mBaselineSubtreeSize;
  StateBits mState;
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(TrackSize::StateBits)

namespace mozilla {
template <>
struct IsPod<nsGridContainerFrame::TrackSize> : std::true_type {};
}  // namespace mozilla

TrackSize::StateBits nsGridContainerFrame::TrackSize::Initialize(
    nscoord aPercentageBasis, const StyleTrackSize& aSize) {
  using Tag = StyleTrackBreadth::Tag;

  MOZ_ASSERT(mBase == 0 && mLimit == 0 && mState == 0,
             "track size data is expected to be initialized to zero");
  mBaselineSubtreeSize[BaselineSharingGroup::First] = nscoord(0);
  mBaselineSubtreeSize[BaselineSharingGroup::Last] = nscoord(0);

  auto& min = aSize.GetMin();
  auto& max = aSize.GetMax();

  Tag minSizeTag = min.tag;
  Tag maxSizeTag = max.tag;
  if (aSize.IsFitContent()) {
    // In layout, fit-content(size) behaves as minmax(auto, max-content), with
    // 'size' as an additional upper-bound.
    mState = eFitContent;
    minSizeTag = Tag::Auto;
    maxSizeTag = Tag::MaxContent;
  }
  if (::IsPercentOfIndefiniteSize(min, aPercentageBasis)) {
    // https://drafts.csswg.org/css-grid/#valdef-grid-template-columns-percentage
    // "If the inline or block size of the grid container is indefinite,
    //  <percentage> values relative to that size are treated as 'auto'."
    minSizeTag = Tag::Auto;
  }
  if (::IsPercentOfIndefiniteSize(max, aPercentageBasis)) {
    maxSizeTag = Tag::Auto;
  }

  // http://dev.w3.org/csswg/css-grid/#algo-init
  switch (minSizeTag) {
    case Tag::Auto:
      mState |= eAutoMinSizing;
      break;
    case Tag::MinContent:
      mState |= eMinContentMinSizing;
      break;
    case Tag::MaxContent:
      mState |= eMaxContentMinSizing;
      break;
    default:
      MOZ_ASSERT(!min.IsFr(), "<flex> min-sizing is invalid as a track size");
      mBase = ::ResolveToDefiniteSize(min, aPercentageBasis);
  }
  switch (maxSizeTag) {
    case Tag::Auto:
      mState |= eAutoMaxSizing;
      mLimit = NS_UNCONSTRAINEDSIZE;
      break;
    case Tag::MinContent:
    case Tag::MaxContent:
      mState |= maxSizeTag == Tag::MinContent ? eMinContentMaxSizing
                                              : eMaxContentMaxSizing;
      mLimit = NS_UNCONSTRAINEDSIZE;
      break;
    case Tag::Fr:
      mState |= eFlexMaxSizing;
      mLimit = mBase;
      break;
    default:
      mLimit = ::ResolveToDefiniteSize(max, aPercentageBasis);
      if (mLimit < mBase) {
        mLimit = mBase;
      }
  }
  return mState;
}

/**
 * A LineRange can be definite or auto - when it's definite it represents
 * a consecutive set of tracks between a starting line and an ending line.
 * Before it's definite it can also represent an auto position with a span,
 * where mStart == kAutoLine and mEnd is the (non-zero positive) span.
 * For normal-flow items, the invariant mStart < mEnd holds when both
 * lines are definite.
 *
 * For abs.pos. grid items, mStart and mEnd may both be kAutoLine, meaning
 * "attach this side to the grid container containing block edge".
 * Additionally, mStart <= mEnd holds when both are definite (non-kAutoLine),
 * i.e. the invariant is slightly relaxed compared to normal flow items.
 */
struct nsGridContainerFrame::LineRange {
  LineRange(int32_t aStart, int32_t aEnd)
      : mUntranslatedStart(aStart), mUntranslatedEnd(aEnd) {
#ifdef DEBUG
    if (!IsAutoAuto()) {
      if (IsAuto()) {
        MOZ_ASSERT(aEnd >= kMinLine && aEnd <= kMaxLine, "invalid span");
      } else {
        MOZ_ASSERT(aStart >= kMinLine && aStart <= kMaxLine,
                   "invalid start line");
        MOZ_ASSERT(aEnd == int32_t(kAutoLine) ||
                       (aEnd >= kMinLine && aEnd <= kMaxLine),
                   "invalid end line");
      }
    }
#endif
  }
  bool IsAutoAuto() const { return mStart == kAutoLine && mEnd == kAutoLine; }
  bool IsAuto() const { return mStart == kAutoLine; }
  bool IsDefinite() const { return mStart != kAutoLine; }
  uint32_t Extent() const {
    MOZ_ASSERT(mEnd != kAutoLine, "Extent is undefined for abs.pos. 'auto'");
    if (IsAuto()) {
      MOZ_ASSERT(mEnd >= 1 && mEnd < uint32_t(kMaxLine), "invalid span");
      return mEnd;
    }
    return mEnd - mStart;
  }

  /**
   * Return an object suitable for iterating this range.
   */
  auto Range() const { return IntegerRange<uint32_t>(mStart, mEnd); }

  /**
   * Resolve this auto range to start at aStart, making it definite.
   * @param aClampMaxLine the maximum allowed line number (zero-based)
   * Precondition: this range IsAuto()
   */
  void ResolveAutoPosition(uint32_t aStart, uint32_t aClampMaxLine) {
    MOZ_ASSERT(IsAuto(), "Why call me?");
    mStart = aStart;
    mEnd += aStart;
    // Clamp to aClampMaxLine, which is where kMaxLine is in the explicit
    // grid in a non-subgrid axis; this implements clamping per
    // http://dev.w3.org/csswg/css-grid/#overlarge-grids
    // In a subgrid axis it's the end of the grid in that axis.
    if (MOZ_UNLIKELY(mStart >= aClampMaxLine)) {
      mEnd = aClampMaxLine;
      mStart = mEnd - 1;
    } else if (MOZ_UNLIKELY(mEnd > aClampMaxLine)) {
      mEnd = aClampMaxLine;
    }
  }
  /**
   * Translate the lines to account for (empty) removed tracks.  This method
   * is only for grid items and should only be called after placement.
   * aNumRemovedTracks contains a count for each line in the grid how many
   * tracks were removed between the start of the grid and that line.
   */
  void AdjustForRemovedTracks(const nsTArray<uint32_t>& aNumRemovedTracks) {
    MOZ_ASSERT(mStart != kAutoLine, "invalid resolved line for a grid item");
    MOZ_ASSERT(mEnd != kAutoLine, "invalid resolved line for a grid item");
    uint32_t numRemovedTracks = aNumRemovedTracks[mStart];
    MOZ_ASSERT(numRemovedTracks == aNumRemovedTracks[mEnd],
               "tracks that a grid item spans can't be removed");
    mStart -= numRemovedTracks;
    mEnd -= numRemovedTracks;
  }
  /**
   * Translate the lines to account for (empty) removed tracks.  This method
   * is only for abs.pos. children and should only be called after placement.
   * Same as for in-flow items, but we don't touch 'auto' lines here and we
   * also need to adjust areas that span into the removed tracks.
   */
  void AdjustAbsPosForRemovedTracks(
      const nsTArray<uint32_t>& aNumRemovedTracks) {
    if (mStart != kAutoLine) {
      mStart -= aNumRemovedTracks[mStart];
    }
    if (mEnd != kAutoLine) {
      MOZ_ASSERT(mStart == kAutoLine || mEnd > mStart, "invalid line range");
      mEnd -= aNumRemovedTracks[mEnd];
    }
  }
  /**
   * Return the contribution of this line range for step 2 in
   * http://dev.w3.org/csswg/css-grid/#auto-placement-algo
   */
  uint32_t HypotheticalEnd() const { return mEnd; }
  /**
   * Given an array of track sizes, return the starting position and length
   * of the tracks in this line range.
   */
  void ToPositionAndLength(const nsTArray<TrackSize>& aTrackSizes,
                           nscoord* aPos, nscoord* aLength) const;
  /**
   * Given an array of track sizes, return the length of the tracks in this
   * line range.
   */
  nscoord ToLength(const nsTArray<TrackSize>& aTrackSizes) const;
  /**
   * Given an array of track sizes and a grid origin coordinate, adjust the
   * abs.pos. containing block along an axis given by aPos and aLength.
   * aPos and aLength should already be initialized to the grid container
   * containing block for this axis before calling this method.
   */
  void ToPositionAndLengthForAbsPos(const Tracks& aTracks, nscoord aGridOrigin,
                                    nscoord* aPos, nscoord* aLength) const;

  void Translate(int32_t aOffset) {
    MOZ_ASSERT(IsDefinite());
    mStart += aOffset;
    mEnd += aOffset;
  }

  /** Swap the start/end sides of this range. */
  void ReverseDirection(uint32_t aGridEnd) {
    MOZ_ASSERT(IsDefinite());
    MOZ_ASSERT(aGridEnd >= mEnd);
    uint32_t newStart = aGridEnd - mEnd;
    mEnd = aGridEnd - mStart;
    mStart = newStart;
  }

  /**
   * @note We'll use the signed member while resolving definite positions
   * to line numbers (1-based), which may become negative for implicit lines
   * to the top/left of the explicit grid.  PlaceGridItems() then translates
   * the whole grid to a 0,0 origin and we'll use the unsigned member from
   * there on.
   */
  union {
    uint32_t mStart;
    int32_t mUntranslatedStart;
  };
  union {
    uint32_t mEnd;
    int32_t mUntranslatedEnd;
  };

 protected:
  LineRange() : mStart(0), mEnd(0) {}
};

/**
 * Helper class to construct a LineRange from translated lines.
 * The ctor only accepts translated definite line numbers.
 */
struct nsGridContainerFrame::TranslatedLineRange : public LineRange {
  TranslatedLineRange(uint32_t aStart, uint32_t aEnd) {
    MOZ_ASSERT(aStart < aEnd && aEnd <= kTranslatedMaxLine);
    mStart = aStart;
    mEnd = aEnd;
  }
};

/**
 * A GridArea is the area in the grid for a grid item.
 * The area is represented by two LineRanges, both of which can be auto
 * (@see LineRange) in intermediate steps while the item is being placed.
 * @see PlaceGridItems
 */
struct nsGridContainerFrame::GridArea {
  GridArea(const LineRange& aCols, const LineRange& aRows)
      : mCols(aCols), mRows(aRows) {}
  bool IsDefinite() const { return mCols.IsDefinite() && mRows.IsDefinite(); }
  LineRange& LineRangeForAxis(LogicalAxis aAxis) {
    return aAxis == eLogicalAxisInline ? mCols : mRows;
  }
  const LineRange& LineRangeForAxis(LogicalAxis aAxis) const {
    return aAxis == eLogicalAxisInline ? mCols : mRows;
  }
  LineRange mCols;
  LineRange mRows;
};

struct nsGridContainerFrame::GridItemInfo {
  /**
   * Item state per axis.
   */
  enum StateBits : uint16_t {
    // clang-format off
    eIsFlexing =              0x1, // does the item span a flex track?
    eFirstBaseline =          0x2, // participate in 'first baseline' alignment?
    // ditto 'last baseline', mutually exclusive w. eFirstBaseline
    eLastBaseline =           0x4,
    eIsBaselineAligned = eFirstBaseline | eLastBaseline,
    // One of e[Self|Content]Baseline is set when eIsBaselineAligned is true
    eSelfBaseline =           0x8, // is it *-self:[last ]baseline alignment?
    // Ditto *-content:[last ]baseline. Mutually exclusive w. eSelfBaseline.
    eContentBaseline =       0x10,
    eAllBaselineBits = eIsBaselineAligned | eSelfBaseline | eContentBaseline,
    // Should apply Automatic Minimum Size per:
    // https://drafts.csswg.org/css-grid/#min-size-auto
    eApplyAutoMinSize =      0x20,
    // Clamp per https://drafts.csswg.org/css-grid/#min-size-auto
    eClampMarginBoxMinSize = 0x40,
    eIsSubgrid =             0x80,
    // set on subgrids and items in subgrids if they are adjacent to the grid
    // start/end edge (excluding grid-aligned abs.pos. frames)
    eStartEdge =            0x100,
    eEndEdge =              0x200,
    eEdgeBits = eStartEdge | eEndEdge,
    // clang-format on
  };

  explicit GridItemInfo(nsIFrame* aFrame, const GridArea& aArea)
      : mFrame(aFrame), mArea(aArea) {
    mState[eLogicalAxisBlock] = StateBits(0);
    mState[eLogicalAxisInline] = StateBits(0);
    nsGridContainerFrame* gridFrame = GetGridContainerFrame(mFrame);
    if (gridFrame) {
      auto parentWM = aFrame->GetParent()->GetWritingMode();
      bool isOrthogonal = parentWM.IsOrthogonalTo(gridFrame->GetWritingMode());
      if (gridFrame->IsColSubgrid()) {
        mState[isOrthogonal ? eLogicalAxisBlock : eLogicalAxisInline] =
            StateBits::eIsSubgrid;
      }
      if (gridFrame->IsRowSubgrid()) {
        mState[isOrthogonal ? eLogicalAxisInline : eLogicalAxisBlock] =
            StateBits::eIsSubgrid;
      }
    }
    mBaselineOffset[eLogicalAxisBlock] = nscoord(0);
    mBaselineOffset[eLogicalAxisInline] = nscoord(0);
  }

  /**
   * Return a copy of this item with its row/column data swapped.
   */
  GridItemInfo Transpose() const {
    GridItemInfo info(mFrame, GridArea(mArea.mRows, mArea.mCols));
    info.mState[0] = mState[1];
    info.mState[1] = mState[0];
    info.mBaselineOffset[0] = mBaselineOffset[1];
    info.mBaselineOffset[1] = mBaselineOffset[0];
    return info;
  }

  /** Swap the start/end sides in aAxis. */
  inline void ReverseDirection(LogicalAxis aAxis, uint32_t aGridEnd);

  // Is this item a subgrid in the given container axis?
  bool IsSubgrid(LogicalAxis aAxis) const {
    return mState[aAxis] & StateBits::eIsSubgrid;
  }

  // Is this item a subgrid in either axis?
  bool IsSubgrid() const {
    return IsSubgrid(eLogicalAxisInline) || IsSubgrid(eLogicalAxisBlock);
  }

  // Return the (inner) grid container frame associated with this subgrid item.
  nsGridContainerFrame* SubgridFrame() const {
    MOZ_ASSERT(IsSubgrid());
    nsGridContainerFrame* gridFrame = GetGridContainerFrame(mFrame);
    MOZ_ASSERT(gridFrame && gridFrame->IsSubgrid());
    return gridFrame;
  }

  /**
   * If the item is [align|justify]-self:[last ]baseline aligned in the given
   * axis then set aBaselineOffset to the baseline offset and return aAlign.
   * Otherwise, return a fallback alignment.
   */
  StyleAlignFlags GetSelfBaseline(StyleAlignFlags aAlign, LogicalAxis aAxis,
                                  nscoord* aBaselineOffset) const {
    MOZ_ASSERT(aAlign == StyleAlignFlags::BASELINE ||
               aAlign == StyleAlignFlags::LAST_BASELINE);
    if (!(mState[aAxis] & eSelfBaseline)) {
      return aAlign == StyleAlignFlags::BASELINE ? StyleAlignFlags::SELF_START
                                                 : StyleAlignFlags::SELF_END;
    }
    *aBaselineOffset = mBaselineOffset[aAxis];
    return aAlign;
  }

  // Return true if we should apply Automatic Minimum Size to this item.
  // https://drafts.csswg.org/css-grid/#min-size-auto
  // @note the caller should also check that the item spans at least one track
  // that has a min track sizing function that is 'auto' before applying it.
  bool ShouldApplyAutoMinSize(WritingMode aContainerWM,
                              LogicalAxis aContainerAxis,
                              nscoord aPercentageBasis) const {
    const bool isInlineAxis = aContainerAxis == eLogicalAxisInline;
    const auto* pos =
        mFrame->IsTableWrapperFrame()
            ? mFrame->PrincipalChildList().FirstChild()->StylePosition()
            : mFrame->StylePosition();
    const auto& size =
        isInlineAxis ? pos->ISize(aContainerWM) : pos->BSize(aContainerWM);
    // max-content and min-content should behave as initial value in block axis.
    // FIXME: Bug 567039: moz-fit-content and -moz-available are not supported
    // for block size dimension on sizing properties (e.g. height), so we
    // treat it as `auto`.
    bool isAuto = size.IsAuto() ||
                  (isInlineAxis ==
                       aContainerWM.IsOrthogonalTo(mFrame->GetWritingMode()) &&
                   size.IsExtremumLength());
    // NOTE: if we have a definite size then our automatic minimum size
    // can't affect our size.  Excluding these simplifies applying
    // the clamping in the right cases later.
    if (!isAuto && !::IsPercentOfIndefiniteSize(size, aPercentageBasis)) {
      return false;
    }
    const auto& minSize = isInlineAxis ? pos->MinISize(aContainerWM)
                                       : pos->MinBSize(aContainerWM);
    // max-content and min-content should behave as initial value in block axis.
    // FIXME: Bug 567039: moz-fit-content and -moz-available are not supported
    // for block size dimension on sizing properties (e.g. height), so we
    // treat it as `auto`.
    isAuto = minSize.IsAuto() ||
             (isInlineAxis ==
                  aContainerWM.IsOrthogonalTo(mFrame->GetWritingMode()) &&
              minSize.IsExtremumLength());
    return isAuto &&
           mFrame->StyleDisplay()->mOverflowX == StyleOverflow::Visible;
  }

#ifdef DEBUG
  void Dump() const;
#endif

  static bool IsStartRowLessThan(const GridItemInfo* a, const GridItemInfo* b) {
    return a->mArea.mRows.mStart < b->mArea.mRows.mStart;
  }

  nsIFrame* const mFrame;
  GridArea mArea;
  // Offset from the margin edge to the baseline (LogicalAxis index).  It's from
  // the start edge when eFirstBaseline is set, end edge otherwise. It's mutable
  // since we update the value fairly late (just before reflowing the item).
  mutable nscoord mBaselineOffset[2];
  mutable StateBits mState[2];  // state bits per axis (LogicalAxis index)
  static_assert(mozilla::eLogicalAxisBlock == 0, "unexpected index value");
  static_assert(mozilla::eLogicalAxisInline == 1, "unexpected index value");
};

using GridItemInfo = nsGridContainerFrame::GridItemInfo;
using ItemState = GridItemInfo::StateBits;
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ItemState)

void GridItemInfo::ReverseDirection(LogicalAxis aAxis, uint32_t aGridEnd) {
  mArea.LineRangeForAxis(aAxis).ReverseDirection(aGridEnd);
  ItemState& state = mState[aAxis];
  ItemState newState = state & ~ItemState::eEdgeBits;
  if (state & ItemState::eStartEdge) {
    newState |= ItemState::eEndEdge;
  }
  if (state & ItemState::eEndEdge) {
    newState |= ItemState::eStartEdge;
  }
  state = newState;
}

// Each subgrid stores this data about its items etc on a frame property.
struct nsGridContainerFrame::Subgrid {
  Subgrid(const GridArea& aArea, bool aIsOrthogonal, WritingMode aCBWM)
      : mArea(aArea),
        mGridColEnd(0),
        mGridRowEnd(0),
        mMarginBorderPadding(aCBWM),
        mIsOrthogonal(aIsOrthogonal) {}

  // Return the relevant line range for the subgrid column axis.
  const LineRange& SubgridCols() const {
    return mIsOrthogonal ? mArea.mRows : mArea.mCols;
  }
  // Return the relevant line range for the subgrid row axis.
  const LineRange& SubgridRows() const {
    return mIsOrthogonal ? mArea.mCols : mArea.mRows;
  }

  // The subgrid's items.
  nsTArray<GridItemInfo> mGridItems;
  // The subgrid's abs.pos. items.
  nsTArray<GridItemInfo> mAbsPosItems;
  // The subgrid's area as a grid item, i.e. in its parent's grid space.
  GridArea mArea;
  // The (inner) grid size for the subgrid, zero-based.
  uint32_t mGridColEnd;
  uint32_t mGridRowEnd;
  // The margin+border+padding for the subgrid box in its parent grid's WM.
  // (This also includes the size of any scrollbars.)
  LogicalMargin mMarginBorderPadding;
  // Does the subgrid frame have orthogonal writing-mode to its parent grid
  // container?
  bool mIsOrthogonal;

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(Prop, Subgrid)
};
using Subgrid = nsGridContainerFrame::Subgrid;

/**
 * Track size data for use by subgrids (which don't do sizing of their own
 * in a subgridded axis).  A non-subgrid container stores its resolved sizes,
 * but only if it has any subgrid children.  A subgrid always stores one.
 * In a subgridded axis, we copy the parent's sizes (see CopyUsedTrackSizes).
 *
 * This struct us stored on a frame property, which may be null before the track
 * sizing step for the given container.  A null property is semantically
 * equivalent to mCanResolveLineRangeSize being false in both axes.
 * @note the axis used to access this data is in the grid container's own
 * writing-mode, same as in other track-sizing functions.
 */
struct nsGridContainerFrame::UsedTrackSizes {
  UsedTrackSizes() : mCanResolveLineRangeSize{false, false} {}

  /**
   * Setup mSizes by copying track sizes from aFrame's grid container
   * parent when aAxis is subgridded (and recurse if the parent is a subgrid
   * that doesn't have sizes yet), or by running the Track Sizing Algo when
   * the axis is not subgridded (for a subgrid).
   * Set mCanResolveLineRangeSize[aAxis] to true once we have obtained
   * sizes for an axis (if it's already true then this method is a NOP).
   */
  void ResolveTrackSizesForAxis(nsGridContainerFrame* aFrame, LogicalAxis aAxis,
                                gfxContext& aRC);

  /** Helper function for the above method */
  void ResolveSubgridTrackSizesForAxis(nsGridContainerFrame* aFrame,
                                       LogicalAxis aAxis, Subgrid* aSubgrid,
                                       gfxContext& aRC,
                                       nscoord aContentBoxSize);

  // This only has valid sizes when mCanResolveLineRangeSize is true in
  // the same axis.  It may have zero tracks (a grid with only abs.pos.
  // subgrids/items may have zero tracks).
  PerLogicalAxis<nsTArray<TrackSize>> mSizes;
  // True if mSizes can be used to resolve line range sizes in an axis.
  PerLogicalAxis<bool> mCanResolveLineRangeSize;

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(Prop, UsedTrackSizes)
};
using UsedTrackSizes = nsGridContainerFrame::UsedTrackSizes;

#ifdef DEBUG
void nsGridContainerFrame::GridItemInfo::Dump() const {
  auto Dump1 = [this](const char* aMsg, LogicalAxis aAxis) {
    auto state = mState[aAxis];
    if (!state) {
      return;
    }
    printf("%s", aMsg);
    if (state & ItemState::eEdgeBits) {
      printf("subgrid-adjacent-edges(");
      if (state & ItemState::eStartEdge) {
        printf("start ");
      }
      if (state & ItemState::eEndEdge) {
        printf("end");
      }
      printf(") ");
    }
    if (state & ItemState::eIsSubgrid) {
      printf("subgrid ");
    }
    if (state & ItemState::eIsFlexing) {
      printf("flexing ");
    }
    if (state & ItemState::eApplyAutoMinSize) {
      printf("auto-min-size ");
    }
    if (state & ItemState::eClampMarginBoxMinSize) {
      printf("clamp ");
    }
    if (state & ItemState::eFirstBaseline) {
      printf("first baseline %s-alignment ",
             (state & ItemState::eSelfBaseline) ? "self" : "content");
    }
    if (state & ItemState::eLastBaseline) {
      printf("last baseline %s-alignment ",
             (state & ItemState::eSelfBaseline) ? "self" : "content");
    }
    if (state & ItemState::eIsBaselineAligned) {
      printf("%.2fpx", NSAppUnitsToFloatPixels(mBaselineOffset[aAxis],
                                               AppUnitsPerCSSPixel()));
    }
    printf("\n");
  };
  printf("grid-row: %d %d\n", mArea.mRows.mStart, mArea.mRows.mEnd);
  Dump1("  grid block-axis: ", eLogicalAxisBlock);
  printf("grid-column: %d %d\n", mArea.mCols.mStart, mArea.mCols.mEnd);
  Dump1("  grid inline-axis: ", eLogicalAxisInline);
}
#endif

/**
 * Encapsulates CSS track-sizing functions.
 */
struct nsGridContainerFrame::TrackSizingFunctions {
 private:
  TrackSizingFunctions(const GridTemplate& aTemplate,
                       const StyleImplicitGridTracks& aAutoSizing,
                       const Maybe<size_t>& aRepeatAutoIndex, bool aIsSubgrid)
      : mTemplate(aTemplate),
        mTrackListValues(aTemplate.TrackListValues()),
        mAutoSizing(aAutoSizing),
        mExplicitGridOffset(0),
        mRepeatAutoStart(aRepeatAutoIndex.valueOr(0)),
        mRepeatAutoEnd(mRepeatAutoStart),
        mHasRepeatAuto(aRepeatAutoIndex.isSome()) {
    MOZ_ASSERT(!mHasRepeatAuto || !aIsSubgrid,
               "a track-list for a subgrid can't have an <auto-repeat> track");
    if (!aIsSubgrid) {
      ExpandNonRepeatAutoTracks();
    }
    MOZ_ASSERT(!mHasRepeatAuto ||
               (mExpandedTracks.Length() >= 1 &&
                mRepeatAutoStart < mExpandedTracks.Length()));
  }

 public:
  TrackSizingFunctions(const GridTemplate& aGridTemplate,
                       const StyleImplicitGridTracks& aAutoSizing,
                       bool aIsSubgrid)
      : TrackSizingFunctions(aGridTemplate, aAutoSizing,
                             aGridTemplate.RepeatAutoIndex(), aIsSubgrid) {}

 private:
  enum { ForSubgridFallbackTag };
  TrackSizingFunctions(const GridTemplate& aGridTemplate,
                       const StyleImplicitGridTracks& aAutoSizing,
                       decltype(ForSubgridFallbackTag))
      : TrackSizingFunctions(aGridTemplate, aAutoSizing, Nothing(),
                             /* aIsSubgrid */ true) {}

 public:
  /**
   * This is used in a subgridded axis to resolve sizes before its parent's
   * sizes are known for intrinsic sizing purposes.  It copies the slice of
   * the nearest non-subgridded axis' track sizing functions spanned by
   * the subgrid.
   *
   * FIXME: this was written before there was a spec... the spec now says:
   * "If calculating the layout of a grid item in this step depends on
   *  the available space in the block axis, assume the available space
   *  that it would have if any row with a definite max track sizing
   *  function had that size and all other rows were infinite."
   * https://drafts.csswg.org/css-grid-2/#subgrid-sizing
   */
  static TrackSizingFunctions ForSubgridFallback(
      nsGridContainerFrame* aSubgridFrame, const Subgrid* aSubgrid,
      nsGridContainerFrame* aParentGridContainer, LogicalAxis aParentAxis) {
    MOZ_ASSERT(aSubgrid);
    MOZ_ASSERT(aSubgridFrame->IsSubgrid(aSubgrid->mIsOrthogonal
                                            ? GetOrthogonalAxis(aParentAxis)
                                            : aParentAxis));
    nsGridContainerFrame* parent = aParentGridContainer;
    auto parentAxis = aParentAxis;
    LineRange range = aSubgrid->mArea.LineRangeForAxis(parentAxis);
    // Find our nearest non-subgridded axis and use its track sizing functions.
    while (parent->IsSubgrid(parentAxis)) {
      const auto* parentSubgrid = parent->GetProperty(Subgrid::Prop());
      auto* grandParent = parent->ParentGridContainerForSubgrid();
      auto grandParentWM = grandParent->GetWritingMode();
      bool isSameDirInAxis =
          parent->GetWritingMode().ParallelAxisStartsOnSameSide(parentAxis,
                                                                grandParentWM);
      if (MOZ_UNLIKELY(!isSameDirInAxis)) {
        auto end = parentAxis == eLogicalAxisBlock ? parentSubgrid->mGridRowEnd
                                                   : parentSubgrid->mGridColEnd;
        range.ReverseDirection(end);
        // range is now in the same direction as the grand-parent's axis
      }
      auto grandParentAxis = parentSubgrid->mIsOrthogonal
                                 ? GetOrthogonalAxis(parentAxis)
                                 : parentAxis;
      const auto& parentRange =
          parentSubgrid->mArea.LineRangeForAxis(grandParentAxis);
      range.Translate(parentRange.mStart);
      // range is now in the grand-parent's coordinates
      parentAxis = grandParentAxis;
      parent = grandParent;
    }
    const auto* pos = parent->StylePosition();
    const auto isInlineAxis = parentAxis == eLogicalAxisInline;
    const auto& szf =
        isInlineAxis ? pos->mGridTemplateRows : pos->mGridTemplateColumns;
    const auto& autoSizing =
        isInlineAxis ? pos->mGridAutoColumns : pos->mGridAutoRows;
    return TrackSizingFunctions(szf, autoSizing, ForSubgridFallbackTag);
  }

  /**
   * Initialize the number of auto-fill/fit tracks to use and return that.
   * (zero if no auto-fill/fit track was specified)
   */
  uint32_t InitRepeatTracks(const NonNegativeLengthPercentageOrNormal& aGridGap,
                            nscoord aMinSize, nscoord aSize, nscoord aMaxSize) {
    const uint32_t repeatTracks =
        CalculateRepeatFillCount(aGridGap, aMinSize, aSize, aMaxSize) *
        NumRepeatTracks();
    SetNumRepeatTracks(repeatTracks);
    // Blank out the removed flags for each of these tracks.
    mRemovedRepeatTracks.SetLength(repeatTracks);
    for (auto& track : mRemovedRepeatTracks) {
      track = false;
    }
    return repeatTracks;
  }

  uint32_t CalculateRepeatFillCount(
      const NonNegativeLengthPercentageOrNormal& aGridGap, nscoord aMinSize,
      nscoord aSize, nscoord aMaxSize) const {
    if (!mHasRepeatAuto) {
      return 0;
    }
    // Note that this uses NumRepeatTracks and mRepeatAutoStart/End, although
    // the result of this method is used to change those values to a fully
    // expanded value. Spec quotes are from
    // https://drafts.csswg.org/css-grid/#repeat-notation
    const uint32_t repeatDelta = mHasRepeatAuto ? NumRepeatTracks() - 1 : 0;
    const uint32_t numTracks = mExpandedTracks.Length() + repeatDelta;
    MOZ_ASSERT(numTracks >= 1, "expected at least the repeat() track");
    nscoord maxFill = aSize != NS_UNCONSTRAINEDSIZE ? aSize : aMaxSize;
    if (maxFill == NS_UNCONSTRAINEDSIZE && aMinSize == 0) {
      // "Otherwise, the specified track list repeats only once."
      return 1;
    }
    nscoord repeatTrackSum = 0;
    // Note that one repeat() track size is included in |sum| in this loop.
    nscoord sum = 0;
    const nscoord percentBasis = aSize;
    for (uint32_t i = 0; i < numTracks; ++i) {
      // "treating each track as its max track sizing function if that is
      // definite or as its minimum track sizing function otherwise"
      // https://drafts.csswg.org/css-grid/#valdef-repeat-auto-fill
      const auto& sizingFunction = SizingFor(i);
      const auto& maxCoord = sizingFunction.GetMax();
      const auto* coord = &maxCoord;
      if (!coord->IsBreadth()) {
        coord = &sizingFunction.GetMin();
        if (!coord->IsBreadth()) {
          return 1;
        }
      }
      nscoord trackSize = ::ResolveToDefiniteSize(*coord, percentBasis);
      if (i >= mRepeatAutoStart && i < mRepeatAutoEnd) {
        // Use a minimum 1px for the repeat() track-size.
        if (trackSize < AppUnitsPerCSSPixel()) {
          trackSize = AppUnitsPerCSSPixel();
        }
        repeatTrackSum += trackSize;
      }
      sum += trackSize;
    }
    nscoord gridGap = nsLayoutUtils::ResolveGapToLength(aGridGap, aSize);
    if (numTracks > 1) {
      // Add grid-gaps for all the tracks including the repeat() track.
      sum += gridGap * (numTracks - 1);
    }
    // Calculate the max number of tracks that fits without overflow.
    nscoord available = maxFill != NS_UNCONSTRAINEDSIZE ? maxFill : aMinSize;
    nscoord spaceToFill = available - sum;
    if (spaceToFill <= 0) {
      // "if any number of repetitions would overflow, then 1 repetition"
      return 1;
    }
    // Calculate the max number of tracks that fits without overflow.
    // Since we already have one repetition in sum, we can simply add one grid
    // gap for each element in the repeat.
    div_t q = div(spaceToFill, repeatTrackSum + gridGap * NumRepeatTracks());
    // The +1 here is for the one repeat track we already accounted for above.
    uint32_t numRepeatTracks = q.quot + 1;
    if (q.rem != 0 && maxFill == NS_UNCONSTRAINEDSIZE) {
      // "Otherwise, if the grid container has a definite min size in
      // the relevant axis, the number of repetitions is the largest possible
      // positive integer that fulfills that minimum requirement."
      ++numRepeatTracks;  // one more to ensure the grid is at least min-size
    }
    // Clamp the number of repeat tracks so that the last line <= kMaxLine.
    // (note that |numTracks| already includes one repeat() track)
    MOZ_ASSERT(numTracks >= NumRepeatTracks());
    MOZ_ASSERT(kMaxLine > numTracks);
    const uint32_t maxRepeatTrackCount = kMaxLine - numTracks;
    const uint32_t maxRepetitions = maxRepeatTrackCount / NumRepeatTracks();
    return std::min(numRepeatTracks, maxRepetitions);
  }

  /**
   * Compute the explicit grid end line number (in a zero-based grid).
   * @param aGridTemplateAreasEnd 'grid-template-areas' end line in this axis
   */
  uint32_t ComputeExplicitGridEnd(uint32_t aGridTemplateAreasEnd) {
    uint32_t end = NumExplicitTracks() + 1;
    end = std::max(end, aGridTemplateAreasEnd);
    end = std::min(end, uint32_t(kMaxLine));
    return end;
  }
  const StyleTrackSize& SizingFor(uint32_t aTrackIndex) const {
    static const StyleTrackSize kAutoTrackSize =
        StyleTrackSize::Breadth(StyleTrackBreadth::Auto());
    // |aIndex| is the relative index to mAutoSizing. A negative value means it
    // is the last Nth element.
    auto getImplicitSize = [this](int32_t aIndex) -> const StyleTrackSize& {
      MOZ_ASSERT(!(mAutoSizing.Length() == 1 &&
                   mAutoSizing.AsSpan()[0] == kAutoTrackSize),
                 "It's impossible to have one track with auto value because we "
                 "filter out this case during parsing");

      if (mAutoSizing.IsEmpty()) {
        return kAutoTrackSize;
      }

      // If multiple track sizes are given, the pattern is repeated as necessary
      // to find the size of the implicit tracks.
      int32_t i = aIndex % int32_t(mAutoSizing.Length());
      if (i < 0) {
        i += mAutoSizing.Length();
      }
      return mAutoSizing.AsSpan()[i];
    };

    if (MOZ_UNLIKELY(aTrackIndex < mExplicitGridOffset)) {
      // The last implicit grid track before the explicit grid receives the
      // last specified size, and so on backwards. Therefore we pass the
      // negative relative index to imply that we should get the implicit size
      // from the last Nth specified grid auto size.
      return getImplicitSize(int32_t(aTrackIndex) -
                             int32_t(mExplicitGridOffset));
    }
    uint32_t index = aTrackIndex - mExplicitGridOffset;
    MOZ_ASSERT(mRepeatAutoStart <= mRepeatAutoEnd);

    if (index >= mRepeatAutoStart) {
      if (index < mRepeatAutoEnd) {
        // Expand the repeat tracks.
        const auto& indices = mExpandedTracks[mRepeatAutoStart];
        const TrackListValue& value = mTrackListValues[indices.first];

        // We expect the default to be used for all track repeats.
        MOZ_ASSERT(indices.second == 0);

        const auto& repeatTracks = value.AsTrackRepeat().track_sizes.AsSpan();

        // Find the repeat track to use, skipping over any collapsed tracks.
        const uint32_t finalRepeatIndex = (index - mRepeatAutoStart);
        uint32_t repeatWithCollapsed = 0;
        // NOTE: We need SizingFor before the final collapsed tracks are known.
        // We know that it's invalid to have empty mRemovedRepeatTracks when
        // there are any repeat tracks, so we can detect that situation here.
        if (mRemovedRepeatTracks.IsEmpty()) {
          repeatWithCollapsed = finalRepeatIndex;
        } else {
          // Count up through the repeat tracks, until we have seen
          // finalRepeatIndex number of non-collapsed tracks.
          for (uint32_t repeatNoCollapsed = 0;
               repeatNoCollapsed < finalRepeatIndex; repeatWithCollapsed++) {
            if (!mRemovedRepeatTracks[repeatWithCollapsed]) {
              repeatNoCollapsed++;
            }
          }
          // If we stopped iterating on a collapsed track, continue to the next
          // non-collapsed track.
          while (mRemovedRepeatTracks[repeatWithCollapsed]) {
            repeatWithCollapsed++;
          }
        }
        return repeatTracks[repeatWithCollapsed % repeatTracks.Length()];
      } else {
        // The index is after the repeat auto range, adjust it to skip over the
        // repeat value. This will have no effect if there is no auto repeat,
        // since then RepeatEndDelta will return zero.
        index -= RepeatEndDelta();
      }
    }
    if (index >= mExpandedTracks.Length()) {
      return getImplicitSize(index - mExpandedTracks.Length());
    }
    auto& indices = mExpandedTracks[index];
    const TrackListValue& value = mTrackListValues[indices.first];
    if (value.IsTrackSize()) {
      MOZ_ASSERT(indices.second == 0);
      return value.AsTrackSize();
    }
    return value.AsTrackRepeat().track_sizes.AsSpan()[indices.second];
  }
  const StyleTrackBreadth& MaxSizingFor(uint32_t aTrackIndex) const {
    return SizingFor(aTrackIndex).GetMax();
  }
  const StyleTrackBreadth& MinSizingFor(uint32_t aTrackIndex) const {
    return SizingFor(aTrackIndex).GetMin();
  }
  uint32_t NumExplicitTracks() const {
    return mExpandedTracks.Length() + RepeatEndDelta();
  }
  uint32_t NumRepeatTracks() const { return mRepeatAutoEnd - mRepeatAutoStart; }
  // The difference between mExplicitGridEnd and mSizingFunctions.Length().
  int32_t RepeatEndDelta() const {
    return mHasRepeatAuto ? int32_t(NumRepeatTracks()) - 1 : 0;
  }
  void SetNumRepeatTracks(uint32_t aNumRepeatTracks) {
    MOZ_ASSERT(mHasRepeatAuto || aNumRepeatTracks == 0);
    mRepeatAutoEnd = mRepeatAutoStart + aNumRepeatTracks;
  }

  // Store mTrackListValues into mExpandedTracks with `repeat(INTEGER, ...)`
  // tracks expanded.
  void ExpandNonRepeatAutoTracks() {
    for (size_t i = 0; i < mTrackListValues.Length(); ++i) {
      auto& value = mTrackListValues[i];
      if (value.IsTrackSize()) {
        mExpandedTracks.EmplaceBack(i, 0);
        continue;
      }
      auto& repeat = value.AsTrackRepeat();
      if (!repeat.count.IsNumber()) {
        MOZ_ASSERT(i == mRepeatAutoStart);
        mRepeatAutoStart = mExpandedTracks.Length();
        mRepeatAutoEnd = mRepeatAutoStart + repeat.track_sizes.Length();
        mExpandedTracks.EmplaceBack(i, 0);
        continue;
      }
      for (auto j : IntegerRange(repeat.count.AsNumber())) {
        Unused << j;
        size_t trackSizesCount = repeat.track_sizes.Length();
        for (auto k : IntegerRange(trackSizesCount)) {
          mExpandedTracks.EmplaceBack(i, k);
        }
      }
    }
    if (MOZ_UNLIKELY(mExpandedTracks.Length() > kMaxLine - 1)) {
      mExpandedTracks.TruncateLength(kMaxLine - 1);
      if (mHasRepeatAuto && mRepeatAutoStart > kMaxLine - 1) {
        // The `repeat(auto-fill/fit)` track is outside the clamped grid.
        mHasRepeatAuto = false;
      }
    }
  }

  // Some style data references, for easy access.
  const GridTemplate& mTemplate;
  const Span<const TrackListValue> mTrackListValues;
  const StyleImplicitGridTracks& mAutoSizing;
  // An array from expanded track sizes (without expanding auto-repeat, which is
  // included just once at `mRepeatAutoStart`).
  //
  // Each entry contains two indices, the first into mTrackListValues, and a
  // second one inside mTrackListValues' repeat value, if any, or zero
  // otherwise.
  nsTArray<std::pair<size_t, size_t>> mExpandedTracks;
  // Offset from the start of the implicit grid to the first explicit track.
  uint32_t mExplicitGridOffset;
  // The index of the repeat(auto-fill/fit) track, or zero if there is none.
  // Relative to mExplicitGridOffset (repeat tracks are explicit by definition).
  uint32_t mRepeatAutoStart;
  // The (hypothetical) index of the last such repeat() track.
  uint32_t mRepeatAutoEnd;
  // True if there is a specified repeat(auto-fill/fit) track.
  bool mHasRepeatAuto;
  // True if this track (relative to mRepeatAutoStart) is a removed auto-fit.
  // Indexed relative to mExplicitGridOffset + mRepeatAutoStart.
  nsTArray<bool> mRemovedRepeatTracks;
};

/**
 * Utility class to find line names.  It provides an interface to lookup line
 * names with a dynamic number of repeat(auto-fill/fit) tracks taken into
 * account.
 */
class MOZ_STACK_CLASS nsGridContainerFrame::LineNameMap {
 public:
  /**
   * Create a LineNameMap.
   * @param aStylePosition the style for the grid container
   * @param aImplicitNamedAreas the implicit areas for the grid container
   * @param aGridTemplate is the grid-template-rows/columns data for this axis
   * @param aParentLineNameMap the parent grid's map parallel to this map, or
   *                           null if this map isn't for a subgrid
   * @param aRange the subgrid's range in the parent grid, or null
   * @param aIsSameDirection true if our axis progresses in the same direction
   *                              in the subgrid and parent
   */
  LineNameMap(const nsStylePosition* aStylePosition,
              const ImplicitNamedAreas* aImplicitNamedAreas,
              const TrackSizingFunctions& aTracks,
              const LineNameMap* aParentLineNameMap, const LineRange* aRange,
              bool aIsSameDirection)
      : mStylePosition(aStylePosition),
        mAreas(aImplicitNamedAreas),
        mRepeatAutoStart(aTracks.mRepeatAutoStart),
        mRepeatAutoEnd(aTracks.mRepeatAutoEnd),
        mRepeatEndDelta(aTracks.RepeatEndDelta()),
        mParentLineNameMap(aParentLineNameMap),
        mRange(aRange),
        mIsSameDirection(aIsSameDirection),
        mHasRepeatAuto(aTracks.mHasRepeatAuto) {
    if (MOZ_UNLIKELY(aRange)) {  // subgrid case
      mClampMinLine = 1;
      mClampMaxLine = 1 + aRange->Extent();
      mRepeatAutoEnd = mRepeatAutoStart;
      const auto& styleSubgrid = aTracks.mTemplate.AsSubgrid();
      const auto fillStart = styleSubgrid->fill_start;
      // Use decltype so we do not rely on the exact type that was exposed from
      // rust code.
      mHasRepeatAuto =
          fillStart != std::numeric_limits<decltype(fillStart)>::max();
      if (mHasRepeatAuto) {
        const auto& lineNameLists = styleSubgrid->names;
        int32_t extraAutoFillLineCount = mClampMaxLine - lineNameLists.Length();
        // Maximum possible number of repeat name lists. This must be reduced
        // to a whole number of repetitions of the fill length.
        const uint32_t possibleRepeatLength = std::max<int32_t>(
            0, extraAutoFillLineCount + styleSubgrid->fill_len);
        MOZ_ASSERT(styleSubgrid->fill_len > 0);
        const uint32_t repeatRemainder =
            possibleRepeatLength % styleSubgrid->fill_len;
        mRepeatAutoStart = fillStart;
        mRepeatAutoEnd =
            mRepeatAutoStart + possibleRepeatLength - repeatRemainder;
      }
    } else {
      mClampMinLine = kMinLine;
      mClampMaxLine = kMaxLine;
      if (mHasRepeatAuto) {
        mTrackAutoRepeatLineNames =
            aTracks.mTemplate.GetRepeatAutoValue()->line_names.AsSpan();
      }
    }
    ExpandRepeatLineNames(!!aRange, aTracks);
    mTemplateLinesEnd = mExpandedLineNames.Length() + mRepeatEndDelta;
    MOZ_ASSERT(mHasRepeatAuto || mRepeatEndDelta <= 0);
    MOZ_ASSERT(!mHasRepeatAuto || aRange ||
               (mExpandedLineNames.Length() >= 2 &&
                mRepeatAutoStart <= mExpandedLineNames.Length()));
  }

  // Store line names into mExpandedLineNames with `repeat(INTEGER, ...)`
  // expanded (for non-subgrid), and all `repeat(...)` expanded (for subgrid).
  void ExpandRepeatLineNames(bool aIsSubgrid,
                             const TrackSizingFunctions& aTracks) {
    auto lineNameLists = aTracks.mTemplate.LineNameLists(aIsSubgrid);

    const auto& trackListValues = aTracks.mTrackListValues;
    const NameList* nameListToMerge = nullptr;
    // NOTE(emilio): We rely on std::move clearing out the array.
    SmallPointerArray<const NameList> names;
    // This adjusts for outputting the repeat auto names in subgrid. In that
    // case, all of the repeat values are handled in a single iteration.
    const uint32_t subgridRepeatDelta =
        (aIsSubgrid && mHasRepeatAuto)
            ? (aTracks.mTemplate.AsSubgrid()->fill_len - 1)
            : 0;
    const uint32_t end = std::min<uint32_t>(
        lineNameLists.Length() - subgridRepeatDelta, mClampMaxLine + 1);
    for (uint32_t i = 0; i < end; ++i) {
      if (aIsSubgrid) {
        if (MOZ_UNLIKELY(mHasRepeatAuto && i == mRepeatAutoStart)) {
          // XXX expand 'auto-fill' names for subgrid for now since HasNameAt()
          // only deals with auto-repeat **tracks** currently.
          const auto& styleSubgrid = aTracks.mTemplate.AsSubgrid();
          MOZ_ASSERT(styleSubgrid->fill_len > 0);
          for (auto j = i; j < mRepeatAutoEnd; ++j) {
            const auto repeatIndex = (j - i) % styleSubgrid->fill_len;
            names.AppendElement(
                &lineNameLists[styleSubgrid->fill_start + repeatIndex]);
            mExpandedLineNames.AppendElement(std::move(names));
          }
        } else if (mHasRepeatAuto && i > mRepeatAutoStart) {
          const auto& styleSubgrid = aTracks.mTemplate.AsSubgrid();
          names.AppendElement(&lineNameLists[i + styleSubgrid->fill_len - 1]);
          mExpandedLineNames.AppendElement(std::move(names));
        } else {
          names.AppendElement(&lineNameLists[i]);
          mExpandedLineNames.AppendElement(std::move(names));
        }
        // XXX expand repeat(<integer>, ...) line names here (bug 1583429)
        continue;
      }

      if (nameListToMerge) {
        names.AppendElement(nameListToMerge);
        nameListToMerge = nullptr;
      }
      names.AppendElement(&lineNameLists[i]);
      if (i >= trackListValues.Length()) {
        mExpandedLineNames.AppendElement(std::move(names));
        continue;
      }
      const auto& value = trackListValues[i];
      if (value.IsTrackSize()) {
        mExpandedLineNames.AppendElement(std::move(names));
        continue;
      }
      const auto& repeat = value.AsTrackRepeat();
      if (!repeat.count.IsNumber()) {
        const auto repeatNames = repeat.line_names.AsSpan();
        MOZ_ASSERT(mRepeatAutoStart == mExpandedLineNames.Length());
        MOZ_ASSERT(repeatNames.Length() >= 2);
        for (const auto j : IntegerRange(repeatNames.Length() - 1)) {
          names.AppendElement(&repeatNames[j]);
          mExpandedLineNames.AppendElement(std::move(names));
        }
        nameListToMerge = &repeatNames[repeatNames.Length() - 1];
        continue;
      }
      for (auto j : IntegerRange(repeat.count.AsNumber())) {
        Unused << j;
        if (nameListToMerge) {
          names.AppendElement(nameListToMerge);
          nameListToMerge = nullptr;
        }
        size_t trackSizesCount = repeat.track_sizes.Length();
        auto repeatLineNames = repeat.line_names.AsSpan();
        MOZ_ASSERT(repeatLineNames.Length() == trackSizesCount ||
                   repeatLineNames.Length() == trackSizesCount + 1);
        for (auto k : IntegerRange(trackSizesCount)) {
          names.AppendElement(&repeatLineNames[k]);
          mExpandedLineNames.AppendElement(std::move(names));
        }
        if (repeatLineNames.Length() == trackSizesCount + 1) {
          nameListToMerge = &repeatLineNames[trackSizesCount];
        }
      }
    }

    if (MOZ_UNLIKELY(mExpandedLineNames.Length() > uint32_t(mClampMaxLine))) {
      mExpandedLineNames.TruncateLength(mClampMaxLine);
    }
    if (MOZ_UNLIKELY(mHasRepeatAuto && aIsSubgrid)) {
      mHasRepeatAuto = false;  // we've expanded all subgrid auto-fill lines
    }
  }

  /**
   * Find the aNth occurrence of aName, searching forward if aNth is positive,
   * and in reverse if aNth is negative (aNth == 0 is invalid), starting from
   * aFromIndex (not inclusive), and return a 1-based line number.
   * Also take into account there is an unconditional match at the lines in
   * aImplicitLines.
   * Return zero if aNth occurrences can't be found.  In that case, aNth has
   * been decremented with the number of occurrences that were found (if any).
   *
   * E.g. to search for "A 2" forward from the start of the grid: aName is "A"
   * aNth is 2 and aFromIndex is zero.  To search for "A -2", aNth is -2 and
   * aFromIndex is ExplicitGridEnd + 1 (which is the line "before" the last
   * line when we're searching in reverse).  For "span A 2", aNth is 2 when
   * used on a grid-[row|column]-end property and -2 for a *-start property,
   * and aFromIndex is the line (which we should skip) on the opposite property.
   */
  uint32_t FindNamedLine(nsAtom* aName, int32_t* aNth, uint32_t aFromIndex,
                         const nsTArray<uint32_t>& aImplicitLines) const {
    MOZ_ASSERT(aName);
    MOZ_ASSERT(!aName->IsEmpty());
    MOZ_ASSERT(aNth && *aNth != 0);
    if (*aNth > 0) {
      return FindLine(aName, aNth, aFromIndex, aImplicitLines);
    }
    int32_t nth = -*aNth;
    int32_t line = RFindLine(aName, &nth, aFromIndex, aImplicitLines);
    *aNth = -nth;
    return line;
  }

  /**
   * Return a set of lines in aImplicitLines which matches the area name aName
   * on aSide.  For example, for aName "a" and aSide being an end side, it
   * returns the line numbers which would match "a-end" in the relevant axis.
   * For subgrids it includes searching the relevant axis in all ancestor
   * grids too (within this subgrid's spanned area).  If an ancestor has
   * opposite direction, we switch aSide to the opposite logical side so we
   * match on the same physical side as the original subgrid we're resolving
   * the name for.
   */
  void FindNamedAreas(nsAtom* aName, LogicalSide aSide,
                      nsTArray<uint32_t>& aImplicitLines) const {
    // True if we're currently in a map that has the same direction as 'this'.
    bool sameDirectionAsThis = true;
    uint32_t min = !mParentLineNameMap ? 1 : mClampMinLine;
    uint32_t max = mClampMaxLine;
    for (auto* map = this; true;) {
      uint32_t line = map->FindNamedArea(aName, aSide, min, max);
      if (line > 0) {
        if (MOZ_LIKELY(sameDirectionAsThis)) {
          line -= min - 1;
        } else {
          line = max - line + 1;
        }
        aImplicitLines.AppendElement(line);
      }
      auto* parent = map->mParentLineNameMap;
      if (!parent) {
        if (MOZ_UNLIKELY(aImplicitLines.Length() > 1)) {
          // Remove duplicates and sort in ascending order.
          aImplicitLines.Sort();
          for (size_t i = 0; i < aImplicitLines.Length(); ++i) {
            uint32_t prev = aImplicitLines[i];
            auto j = i + 1;
            const auto start = j;
            while (j < aImplicitLines.Length() && aImplicitLines[j] == prev) {
              ++j;
            }
            if (j != start) {
              aImplicitLines.RemoveElementsAt(start, j - start);
            }
          }
        }
        return;
      }
      if (MOZ_UNLIKELY(!map->mIsSameDirection)) {
        aSide = GetOppositeSide(aSide);
        sameDirectionAsThis = !sameDirectionAsThis;
      }
      min = map->TranslateToParentMap(min);
      max = map->TranslateToParentMap(max);
      if (min > max) {
        MOZ_ASSERT(!map->mIsSameDirection);
        std::swap(min, max);
      }
      map = parent;
    }
  }

  /**
   * Return true if any implicit named areas match aName, in this map or
   * in any of our ancestor maps.
   */
  bool HasImplicitNamedArea(nsAtom* aName) const {
    const auto* map = this;
    do {
      if (map->mAreas && map->mAreas->has(aName)) {
        return true;
      }
      map = map->mParentLineNameMap;
    } while (map);
    return false;
  }

  // For generating line name data for devtools.
  nsTArray<nsTArray<StyleCustomIdent>>
  GetResolvedLineNamesForComputedGridTrackInfo() const {
    nsTArray<nsTArray<StyleCustomIdent>> result;
    for (auto& expandedLine : mExpandedLineNames) {
      nsTArray<StyleCustomIdent> line;
      for (auto* chunk : expandedLine) {
        for (auto& name : chunk->AsSpan()) {
          line.AppendElement(name);
        }
      }
      result.AppendElement(std::move(line));
    }
    return result;
  }

  nsTArray<RefPtr<nsAtom>> GetExplicitLineNamesAtIndex(uint32_t aIndex) const {
    nsTArray<RefPtr<nsAtom>> lineNames;
    if (aIndex < mTemplateLinesEnd) {
      const auto nameLists = GetLineNamesAt(aIndex);
      for (const NameList* nameList : nameLists) {
        for (const auto& name : nameList->AsSpan()) {
          lineNames.AppendElement(name.AsAtom());
        }
      }
    }
    return lineNames;
  }

  const nsTArray<SmallPointerArray<const NameList>>& ExpandedLineNames() const {
    return mExpandedLineNames;
  }
  const Span<const StyleOwnedSlice<StyleCustomIdent>>&
  TrackAutoRepeatLineNames() const {
    return mTrackAutoRepeatLineNames;
  }
  bool HasRepeatAuto() const { return mHasRepeatAuto; }
  uint32_t NumRepeatTracks() const { return mRepeatAutoEnd - mRepeatAutoStart; }
  uint32_t RepeatAutoStart() const { return mRepeatAutoStart; }

  // The min/max line number (1-based) for clamping.
  int32_t mClampMinLine;
  int32_t mClampMaxLine;

 private:
  // Return true if this map represents a subgridded axis.
  bool IsSubgridded() const { return mParentLineNameMap != nullptr; }

  /**
   * @see FindNamedLine, this function searches forward.
   */
  uint32_t FindLine(nsAtom* aName, int32_t* aNth, uint32_t aFromIndex,
                    const nsTArray<uint32_t>& aImplicitLines) const {
    MOZ_ASSERT(aNth && *aNth > 0);
    int32_t nth = *aNth;
    // For a subgrid we need to search to the end of the grid rather than
    // the end of the local name list, since ancestors might match.
    const uint32_t end = IsSubgridded() ? mClampMaxLine : mTemplateLinesEnd;
    uint32_t line;
    uint32_t i = aFromIndex;
    for (; i < end; i = line) {
      line = i + 1;
      if (Contains(i, aName) || aImplicitLines.Contains(line)) {
        if (--nth == 0) {
          return line;
        }
      }
    }
    for (auto implicitLine : aImplicitLines) {
      if (implicitLine > i) {
        // implicitLine is after the lines we searched above so it's last.
        // (grid-template-areas has more tracks than
        // grid-template-[rows|columns])
        if (--nth == 0) {
          return implicitLine;
        }
      }
    }
    MOZ_ASSERT(nth > 0, "should have returned a valid line above already");
    *aNth = nth;
    return 0;
  }

  /**
   * @see FindNamedLine, this function searches in reverse.
   */
  uint32_t RFindLine(nsAtom* aName, int32_t* aNth, uint32_t aFromIndex,
                     const nsTArray<uint32_t>& aImplicitLines) const {
    MOZ_ASSERT(aNth && *aNth > 0);
    if (MOZ_UNLIKELY(aFromIndex == 0)) {
      return 0;  // There are no named lines beyond the start of the explicit
                 // grid.
    }
    --aFromIndex;  // (shift aFromIndex so we can treat it as inclusive)
    int32_t nth = *aNth;
    // Implicit lines may be beyond the explicit grid so we match those
    // first if it's within the mTemplateLinesEnd..aFromIndex range.
    // aImplicitLines is presumed sorted.
    // For a subgrid we need to search to the end of the grid rather than
    // the end of the local name list, since ancestors might match.
    const uint32_t end = IsSubgridded() ? mClampMaxLine : mTemplateLinesEnd;
    for (auto implicitLine : Reversed(aImplicitLines)) {
      if (implicitLine <= end) {
        break;
      }
      if (implicitLine < aFromIndex) {
        if (--nth == 0) {
          return implicitLine;
        }
      }
    }
    for (uint32_t i = std::min(aFromIndex, end); i; --i) {
      if (Contains(i - 1, aName) || aImplicitLines.Contains(i)) {
        if (--nth == 0) {
          return i;
        }
      }
    }
    MOZ_ASSERT(nth > 0, "should have returned a valid line above already");
    *aNth = nth;
    return 0;
  }

  // Return true if aName exists at aIndex in this map or any parent map.
  bool Contains(uint32_t aIndex, nsAtom* aName) const {
    const auto* map = this;
    while (true) {
      if (aIndex < map->mTemplateLinesEnd && map->HasNameAt(aIndex, aName)) {
        return true;
      }
      auto* parent = map->mParentLineNameMap;
      if (!parent) {
        return false;
      }
      uint32_t line = map->TranslateToParentMap(aIndex + 1);
      MOZ_ASSERT(line >= 1, "expected a 1-based line number");
      aIndex = line - 1;
      map = parent;
    }
    MOZ_ASSERT_UNREACHABLE("we always return from inside the loop above");
  }

  static bool Contains(Span<const StyleCustomIdent> aNames, nsAtom* aName) {
    for (auto& name : aNames) {
      if (name.AsAtom() == aName) {
        return true;
      }
    }
    return false;
  }

  // Return true if aName exists at aIndex in this map.
  bool HasNameAt(const uint32_t aIndex, nsAtom* const aName) const {
    const auto nameLists = GetLineNamesAt(aIndex);
    for (const NameList* nameList : nameLists) {
      if (Contains(nameList->AsSpan(), aName)) {
        return true;
      }
    }
    return false;
  }

  // Get the line names at an index.
  // This accounts for auto repeat. The results may be spread over multiple name
  // lists returned in the array, which is done to avoid unneccessarily copying
  // the arrays to concatenate them.
  SmallPointerArray<const NameList> GetLineNamesAt(
      const uint32_t aIndex) const {
    SmallPointerArray<const NameList> names;
    // The index into mExpandedLineNames to use, if aIndex doesn't point to a
    // name inside of a auto repeat.
    uint32_t repeatAdjustedIndex = aIndex;
    if (mHasRepeatAuto) {
      // If the index is inside of the auto repeat, use the repeat line
      // names. Otherwise, if the index is past the end of the repeat it must
      // be adjusted to acount for the repeat tracks.
      // mExpandedLineNames has the first and last line name lists from the
      // repeat in it already, so we can just ignore aIndex == mRepeatAutoStart
      // and treat when aIndex == mRepeatAutoEnd the same as any line after the
      // the repeat.
      const uint32_t maxRepeatLine = mTrackAutoRepeatLineNames.Length() - 1;
      if (aIndex > mRepeatAutoStart && aIndex < mRepeatAutoEnd) {
        // The index is inside the auto repeat. Calculate the lines to use,
        // including the previous repetitions final names when we roll over
        // from one repetition to the next.
        const uint32_t repeatIndex =
            (aIndex - mRepeatAutoStart) % maxRepeatLine;
        if (repeatIndex == 0) {
          // The index is at the start of a new repetition. The start of the
          // first repetition is intentionally ignored above, so this will
          // consider both the end of the previous repetition and the start
          // the one that contains aIndex.
          names.AppendElement(&mTrackAutoRepeatLineNames[maxRepeatLine]);
        }
        names.AppendElement(&mTrackAutoRepeatLineNames[repeatIndex]);
        return names;
      }
      if (aIndex != mRepeatAutoStart && aIndex >= mRepeatAutoEnd) {
        // Adjust the index to account for the line names of the repeat.
        repeatAdjustedIndex -= mRepeatEndDelta;
        repeatAdjustedIndex += mTrackAutoRepeatLineNames.Length() - 2;
      }
    }
    MOZ_ASSERT(names.IsEmpty());
    // The index is not inside the repeat tracks, or no repeat tracks exist.
    const auto& nameLists = mExpandedLineNames[repeatAdjustedIndex];
    for (const NameList* nameList : nameLists) {
      names.AppendElement(nameList);
    }
    return names;
  }

  // Translate a subgrid line (1-based) to a parent line (1-based).
  uint32_t TranslateToParentMap(uint32_t aLine) const {
    if (MOZ_LIKELY(mIsSameDirection)) {
      return aLine + mRange->mStart;
    }
    MOZ_ASSERT(mRange->mEnd + 1 >= aLine);
    return mRange->mEnd - (aLine - 1) + 1;
  }

  /**
   * Return the 1-based line that match aName in 'grid-template-areas'
   * on the side aSide.  Clamp the result to aMin..aMax but require
   * that some part of the area is inside for it to match.
   * Return zero if there is no match.
   */
  uint32_t FindNamedArea(nsAtom* aName, LogicalSide aSide, int32_t aMin,
                         int32_t aMax) const {
    if (const NamedArea* area = FindNamedArea(aName)) {
      int32_t start = IsBlock(aSide) ? area->rows.start : area->columns.start;
      int32_t end = IsBlock(aSide) ? area->rows.end : area->columns.end;
      if (IsStart(aSide)) {
        if (start >= aMin) {
          if (start <= aMax) {
            return start;
          }
        } else if (end >= aMin) {
          return aMin;
        }
      } else {
        if (end <= aMax) {
          if (end >= aMin) {
            return end;
          }
        } else if (start <= aMax) {
          return aMax;
        }
      }
    }
    return 0;  // no match
  }

  /**
   * A convenience method to lookup a name in 'grid-template-areas'.
   * @return null if not found
   */
  const NamedArea* FindNamedArea(nsAtom* aName) const {
    if (mStylePosition->mGridTemplateAreas.IsNone()) {
      return nullptr;
    }
    const auto areas = mStylePosition->mGridTemplateAreas.AsAreas();
    for (const NamedArea& area : areas->areas.AsSpan()) {
      if (area.name.AsAtom() == aName) {
        return &area;
      }
    }
    return nullptr;
  }

  // Some style data references, for easy access.
  const nsStylePosition* mStylePosition;
  const ImplicitNamedAreas* mAreas;
  // The expanded list of line-names. Each entry is usually a single NameList,
  // but can be multiple in the case where repeat() expands to something that
  // has a line name list at the end.
  nsTArray<SmallPointerArray<const NameList>> mExpandedLineNames;
  // The repeat(auto-fill/fit) track value, if any. (always empty for subgrid)
  Span<const StyleOwnedSlice<StyleCustomIdent>> mTrackAutoRepeatLineNames;
  // The index of the repeat(auto-fill/fit) track, or zero if there is none.
  uint32_t mRepeatAutoStart;
  // The (hypothetical) index of the last such repeat() track.
  uint32_t mRepeatAutoEnd;
  // The difference between mTemplateLinesEnd and mExpandedLineNames.Length().
  int32_t mRepeatEndDelta;
  // The end of the line name lists with repeat(auto-fill/fit) tracks accounted
  // for.  It is equal to mExpandedLineNames.Length() when a repeat()
  // track generates one track (making mRepeatEndDelta == 0).
  uint32_t mTemplateLinesEnd;

  // The parent line map, or null if this map isn't for a subgrid.
  const LineNameMap* mParentLineNameMap;
  // The subgrid's range, or null if this map isn't for a subgrid.
  const LineRange* mRange;
  // True if the subgrid/parent axes progresses in the same direction.
  const bool mIsSameDirection;

  // True if there is a specified repeat(auto-fill/fit) track.
  bool mHasRepeatAuto;
};

/**
 * State for the tracks in one dimension.
 */
struct nsGridContainerFrame::Tracks {
  explicit Tracks(LogicalAxis aAxis)
      : mContentBoxSize(NS_UNCONSTRAINEDSIZE),
        mGridGap(NS_UNCONSTRAINEDSIZE),
        mStateUnion(TrackSize::StateBits(0)),
        mAxis(aAxis),
        mCanResolveLineRangeSize(false) {
    mBaselineSubtreeAlign[BaselineSharingGroup::First] = StyleAlignFlags::AUTO;
    mBaselineSubtreeAlign[BaselineSharingGroup::Last] = StyleAlignFlags::AUTO;
    mBaseline[BaselineSharingGroup::First] = NS_INTRINSIC_ISIZE_UNKNOWN;
    mBaseline[BaselineSharingGroup::Last] = NS_INTRINSIC_ISIZE_UNKNOWN;
  }

  void Initialize(const TrackSizingFunctions& aFunctions,
                  const NonNegativeLengthPercentageOrNormal& aGridGap,
                  uint32_t aNumTracks, nscoord aContentBoxSize);

  /**
   * Return the union of the state bits for the tracks in aRange.
   */
  TrackSize::StateBits StateBitsForRange(const LineRange& aRange) const;

  // Some data we collect for aligning baseline-aligned items.
  struct ItemBaselineData {
    uint32_t mBaselineTrack;
    nscoord mBaseline;
    nscoord mSize;
    GridItemInfo* mGridItem;
    static bool IsBaselineTrackLessThan(const ItemBaselineData& a,
                                        const ItemBaselineData& b) {
      return a.mBaselineTrack < b.mBaselineTrack;
    }
  };

  /**
   * Calculate baseline offsets for the given set of items.
   * Helper for InitialzeItemBaselines.
   */
  void CalculateItemBaselines(nsTArray<ItemBaselineData>& aBaselineItems,
                              BaselineSharingGroup aBaselineGroup);

  /**
   * Initialize grid item baseline state and offsets.
   */
  void InitializeItemBaselines(GridReflowInput& aState,
                               nsTArray<GridItemInfo>& aGridItems);

  /**
   * Apply the additional alignment needed to align the baseline-aligned subtree
   * the item belongs to within its baseline track.
   */
  void AlignBaselineSubtree(const GridItemInfo& aGridItem) const;

  enum class TrackSizingPhase {
    IntrinsicMinimums,
    ContentBasedMinimums,
    MaxContentMinimums,
    IntrinsicMaximums,
    MaxContentMaximums,
  };

  // Some data we collect on each item for Step 2 of the Track Sizing Algorithm
  // in ResolveIntrinsicSize below.
  struct Step2ItemData final {
    uint32_t mSpan;
    TrackSize::StateBits mState;
    LineRange mLineRange;
    nscoord mMinSize;
    nscoord mMinContentContribution;
    nscoord mMaxContentContribution;
    nsIFrame* mFrame;
    static bool IsSpanLessThan(const Step2ItemData& a, const Step2ItemData& b) {
      return a.mSpan < b.mSpan;
    }

    template <TrackSizingPhase phase>
    nscoord SizeContributionForPhase() const {
      switch (phase) {
        case TrackSizingPhase::IntrinsicMinimums:
        case TrackSizingPhase::IntrinsicMaximums:
          return mMinSize;
        case TrackSizingPhase::ContentBasedMinimums:
          return mMinContentContribution;
        case TrackSizingPhase::MaxContentMinimums:
        case TrackSizingPhase::MaxContentMaximums:
          return mMaxContentContribution;
      }
      MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unexpected phase");
    }
  };

  using FitContentClamper =
      std::function<bool(uint32_t aTrack, nscoord aMinSize, nscoord* aSize)>;

  // Helper method for ResolveIntrinsicSize.
  template <TrackSizingPhase phase>
  bool GrowSizeForSpanningItems(nsTArray<Step2ItemData>::iterator aIter,
                                const nsTArray<Step2ItemData>::iterator aEnd,
                                nsTArray<uint32_t>& aTracks,
                                nsTArray<TrackSize>& aPlan,
                                nsTArray<TrackSize>& aItemPlan,
                                TrackSize::StateBits aSelector,
                                const FitContentClamper& aClamper = nullptr,
                                bool aNeedInfinitelyGrowableFlag = false);
  /**
   * Resolve Intrinsic Track Sizes.
   * http://dev.w3.org/csswg/css-grid/#algo-content
   */
  void ResolveIntrinsicSize(GridReflowInput& aState,
                            nsTArray<GridItemInfo>& aGridItems,
                            const TrackSizingFunctions& aFunctions,
                            LineRange GridArea::*aRange,
                            nscoord aPercentageBasis,
                            SizingConstraint aConstraint);

  /**
   * Helper for ResolveIntrinsicSize.  It implements step 1 "size tracks to fit
   * non-spanning items" in the spec.  Return true if the track has a <flex>
   * max-sizing function, false otherwise.
   */
  bool ResolveIntrinsicSizeStep1(GridReflowInput& aState,
                                 const TrackSizingFunctions& aFunctions,
                                 nscoord aPercentageBasis,
                                 SizingConstraint aConstraint,
                                 const LineRange& aRange,
                                 const GridItemInfo& aGridItem);

  // Helper method that returns the track size to use in §11.5.1.2
  // https://drafts.csswg.org/css-grid/#extra-space
  template <TrackSizingPhase phase>
  static nscoord StartSizeInDistribution(const TrackSize& aSize) {
    switch (phase) {
      case TrackSizingPhase::IntrinsicMinimums:
      case TrackSizingPhase::ContentBasedMinimums:
      case TrackSizingPhase::MaxContentMinimums:
        return aSize.mBase;
      case TrackSizingPhase::IntrinsicMaximums:
      case TrackSizingPhase::MaxContentMaximums:
        if (aSize.mLimit == NS_UNCONSTRAINEDSIZE) {
          return aSize.mBase;
        }
        return aSize.mLimit;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unexpected phase");
  }

  /**
   * Collect the tracks which are growable (matching aSelector) into
   * aGrowableTracks, and return the amount of space that can be used
   * to grow those tracks.  This method implements CSS Grid §11.5.1.2.
   * https://drafts.csswg.org/css-grid/#extra-space
   */
  template <TrackSizingPhase phase>
  nscoord CollectGrowable(nscoord aAvailableSpace, const LineRange& aRange,
                          TrackSize::StateBits aSelector,
                          nsTArray<uint32_t>& aGrowableTracks) const {
    MOZ_ASSERT(aAvailableSpace > 0, "why call me?");
    nscoord space = aAvailableSpace - mGridGap * (aRange.Extent() - 1);
    for (auto i : aRange.Range()) {
      const TrackSize& sz = mSizes[i];
      space -= StartSizeInDistribution<phase>(sz);
      if (space <= 0) {
        return 0;
      }
      if (sz.mState & aSelector) {
        aGrowableTracks.AppendElement(i);
      }
    }
    return aGrowableTracks.IsEmpty() ? 0 : space;
  }

  template <TrackSizingPhase phase>
  void InitializeItemPlan(nsTArray<TrackSize>& aItemPlan,
                          const nsTArray<uint32_t>& aTracks) const {
    for (uint32_t track : aTracks) {
      auto& plan = aItemPlan[track];
      const TrackSize& sz = mSizes[track];
      plan.mBase = StartSizeInDistribution<phase>(sz);
      bool unlimited = sz.mState & TrackSize::eInfinitelyGrowable;
      plan.mLimit = unlimited ? NS_UNCONSTRAINEDSIZE : sz.mLimit;
      plan.mState = sz.mState;
    }
  }

  template <TrackSizingPhase phase>
  void InitializePlan(nsTArray<TrackSize>& aPlan) const {
    for (size_t i = 0, len = aPlan.Length(); i < len; ++i) {
      auto& plan = aPlan[i];
      const auto& sz = mSizes[i];
      plan.mBase = StartSizeInDistribution<phase>(sz);
      MOZ_ASSERT(phase == TrackSizingPhase::MaxContentMaximums ||
                     !(sz.mState & TrackSize::eInfinitelyGrowable),
                 "forgot to reset the eInfinitelyGrowable bit?");
      plan.mState = sz.mState;
    }
  }

  template <TrackSizingPhase phase>
  void CopyPlanToSize(const nsTArray<TrackSize>& aPlan,
                      bool aNeedInfinitelyGrowableFlag = false) {
    for (size_t i = 0, len = mSizes.Length(); i < len; ++i) {
      const auto& plan = aPlan[i];
      MOZ_ASSERT(plan.mBase >= 0);
      auto& sz = mSizes[i];
      switch (phase) {
        case TrackSizingPhase::IntrinsicMinimums:
        case TrackSizingPhase::ContentBasedMinimums:
        case TrackSizingPhase::MaxContentMinimums:
          sz.mBase = plan.mBase;
          break;
        case TrackSizingPhase::IntrinsicMaximums:
          if (plan.mState & TrackSize::eModified) {
            if (sz.mLimit == NS_UNCONSTRAINEDSIZE &&
                aNeedInfinitelyGrowableFlag) {
              sz.mState |= TrackSize::eInfinitelyGrowable;
            }
            sz.mLimit = plan.mBase;
          }
          break;
        case TrackSizingPhase::MaxContentMaximums:
          if (plan.mState & TrackSize::eModified) {
            sz.mLimit = plan.mBase;
          }
          sz.mState &= ~TrackSize::eInfinitelyGrowable;
          break;
      }
    }
  }

  /**
   * Grow the planned size for tracks in aGrowableTracks up to their limit
   * and then freeze them (all aGrowableTracks must be unfrozen on entry).
   * Subtract the space added from aAvailableSpace and return that.
   */
  nscoord GrowTracksToLimit(nscoord aAvailableSpace, nsTArray<TrackSize>& aPlan,
                            const nsTArray<uint32_t>& aGrowableTracks,
                            const FitContentClamper& aFitContentClamper) const {
    MOZ_ASSERT(aAvailableSpace > 0 && aGrowableTracks.Length() > 0);
    nscoord space = aAvailableSpace;
    uint32_t numGrowable = aGrowableTracks.Length();
    while (true) {
      nscoord spacePerTrack = std::max<nscoord>(space / numGrowable, 1);
      for (uint32_t track : aGrowableTracks) {
        TrackSize& sz = aPlan[track];
        if (sz.IsFrozen()) {
          continue;
        }
        nscoord newBase = sz.mBase + spacePerTrack;
        nscoord limit = sz.mLimit;
        if (MOZ_UNLIKELY((sz.mState & TrackSize::eFitContent) &&
                         aFitContentClamper)) {
          // Clamp the limit to the fit-content() size, for §12.5.2 step 5/6.
          aFitContentClamper(track, sz.mBase, &limit);
        }
        if (newBase > limit) {
          nscoord consumed = limit - sz.mBase;
          if (consumed > 0) {
            space -= consumed;
            sz.mBase = limit;
          }
          sz.mState |= TrackSize::eFrozen;
          if (--numGrowable == 0) {
            return space;
          }
        } else {
          sz.mBase = newBase;
          space -= spacePerTrack;
        }
        MOZ_ASSERT(space >= 0);
        if (space == 0) {
          return 0;
        }
      }
    }
    MOZ_ASSERT_UNREACHABLE("we don't exit the loop above except by return");
    return 0;
  }

  /**
   * Helper for GrowSelectedTracksUnlimited.  For the set of tracks (S) that
   * match aMinSizingSelector: if a track in S doesn't match aMaxSizingSelector
   * then mark it with aSkipFlag.  If all tracks in S were marked then unmark
   * them.  Return aNumGrowable minus the number of tracks marked.  It is
   * assumed that aPlan have no aSkipFlag set for tracks in aGrowableTracks
   * on entry to this method.
   */
  static uint32_t MarkExcludedTracks(nsTArray<TrackSize>& aPlan,
                                     uint32_t aNumGrowable,
                                     const nsTArray<uint32_t>& aGrowableTracks,
                                     TrackSize::StateBits aMinSizingSelector,
                                     TrackSize::StateBits aMaxSizingSelector,
                                     TrackSize::StateBits aSkipFlag) {
    bool foundOneSelected = false;
    bool foundOneGrowable = false;
    uint32_t numGrowable = aNumGrowable;
    for (uint32_t track : aGrowableTracks) {
      TrackSize& sz = aPlan[track];
      const auto state = sz.mState;
      if (state & aMinSizingSelector) {
        foundOneSelected = true;
        if (state & aMaxSizingSelector) {
          foundOneGrowable = true;
          continue;
        }
        sz.mState |= aSkipFlag;
        MOZ_ASSERT(numGrowable != 0);
        --numGrowable;
      }
    }
    // 12.5 "if there are no such tracks, then all affected tracks"
    if (foundOneSelected && !foundOneGrowable) {
      for (uint32_t track : aGrowableTracks) {
        aPlan[track].mState &= ~aSkipFlag;
      }
      numGrowable = aNumGrowable;
    }
    return numGrowable;
  }

  /**
   * Mark all tracks in aGrowableTracks with an eSkipGrowUnlimited bit if
   * they *shouldn't* grow unlimited in §11.5.1.2.3 "Distribute space beyond
   * growth limits" https://drafts.csswg.org/css-grid/#extra-space
   * Return the number of tracks that are still growable.
   */
  template <TrackSizingPhase phase>
  static uint32_t MarkExcludedTracks(nsTArray<TrackSize>& aPlan,
                                     const nsTArray<uint32_t>& aGrowableTracks,
                                     TrackSize::StateBits aSelector) {
    uint32_t numGrowable = aGrowableTracks.Length();
    if (phase == TrackSizingPhase::IntrinsicMaximums ||
        phase == TrackSizingPhase::MaxContentMaximums) {
      // "when handling any intrinsic growth limit: all affected tracks"
      return numGrowable;
    }
    MOZ_ASSERT(aSelector == (aSelector & TrackSize::eIntrinsicMinSizing) &&
                   (aSelector & TrackSize::eMaxContentMinSizing),
               "Should only get here for track sizing steps 2.1 to 2.3");
    // Note that eMaxContentMinSizing is always included. We do those first:
    numGrowable = MarkExcludedTracks(
        aPlan, numGrowable, aGrowableTracks, TrackSize::eMaxContentMinSizing,
        TrackSize::eMaxContentMaxSizing, TrackSize::eSkipGrowUnlimited1);
    // Now mark min-content/auto min-sizing tracks if requested.
    auto minOrAutoSelector = aSelector & ~TrackSize::eMaxContentMinSizing;
    if (minOrAutoSelector) {
      numGrowable = MarkExcludedTracks(
          aPlan, numGrowable, aGrowableTracks, minOrAutoSelector,
          TrackSize::eIntrinsicMaxSizing, TrackSize::eSkipGrowUnlimited2);
    }
    return numGrowable;
  }

  /**
   * Increase the planned size for tracks in aGrowableTracks that aren't
   * marked with a eSkipGrowUnlimited flag beyond their limit.
   * This implements the "Distribute space beyond growth limits" step in
   * https://drafts.csswg.org/css-grid/#distribute-extra-space
   */
  void GrowSelectedTracksUnlimited(
      nscoord aAvailableSpace, nsTArray<TrackSize>& aPlan,
      const nsTArray<uint32_t>& aGrowableTracks, uint32_t aNumGrowable,
      const FitContentClamper& aFitContentClamper) const {
    MOZ_ASSERT(aAvailableSpace > 0 && aGrowableTracks.Length() > 0 &&
               aNumGrowable <= aGrowableTracks.Length());
    nscoord space = aAvailableSpace;
    DebugOnly<bool> didClamp = false;
    while (aNumGrowable) {
      nscoord spacePerTrack = std::max<nscoord>(space / aNumGrowable, 1);
      for (uint32_t track : aGrowableTracks) {
        TrackSize& sz = aPlan[track];
        if (sz.mState & TrackSize::eSkipGrowUnlimited) {
          continue;  // an excluded track
        }
        nscoord delta = spacePerTrack;
        nscoord newBase = sz.mBase + delta;
        if (MOZ_UNLIKELY((sz.mState & TrackSize::eFitContent) &&
                         aFitContentClamper)) {
          // Clamp newBase to the fit-content() size, for §12.5.2 step 5/6.
          if (aFitContentClamper(track, sz.mBase, &newBase)) {
            didClamp = true;
            delta = newBase - sz.mBase;
            MOZ_ASSERT(delta >= 0, "track size shouldn't shrink");
            sz.mState |= TrackSize::eSkipGrowUnlimited1;
            --aNumGrowable;
          }
        }
        sz.mBase = newBase;
        space -= delta;
        MOZ_ASSERT(space >= 0);
        if (space == 0) {
          return;
        }
      }
    }
    MOZ_ASSERT(didClamp,
               "we don't exit the loop above except by return, "
               "unless we clamped some track's size");
  }

  /**
   * Distribute aAvailableSpace to the planned base size for aGrowableTracks
   * up to their limits, then distribute the remaining space beyond the limits.
   */
  template <TrackSizingPhase phase>
  void DistributeToTrackSizes(nscoord aAvailableSpace,
                              nsTArray<TrackSize>& aPlan,
                              nsTArray<TrackSize>& aItemPlan,
                              nsTArray<uint32_t>& aGrowableTracks,
                              TrackSize::StateBits aSelector,
                              const FitContentClamper& aFitContentClamper) {
    InitializeItemPlan<phase>(aItemPlan, aGrowableTracks);
    nscoord space = GrowTracksToLimit(aAvailableSpace, aItemPlan,
                                      aGrowableTracks, aFitContentClamper);
    if (space > 0) {
      uint32_t numGrowable =
          MarkExcludedTracks<phase>(aItemPlan, aGrowableTracks, aSelector);
      GrowSelectedTracksUnlimited(space, aItemPlan, aGrowableTracks,
                                  numGrowable, aFitContentClamper);
    }
    for (uint32_t track : aGrowableTracks) {
      nscoord& plannedSize = aPlan[track].mBase;
      nscoord itemIncurredSize = aItemPlan[track].mBase;
      if (plannedSize < itemIncurredSize) {
        plannedSize = itemIncurredSize;
      }
    }
  }

  /**
   * Distribute aAvailableSize to the tracks.  This implements 12.6 at:
   * http://dev.w3.org/csswg/css-grid/#algo-grow-tracks
   */
  void DistributeFreeSpace(nscoord aAvailableSize) {
    const uint32_t numTracks = mSizes.Length();
    if (MOZ_UNLIKELY(numTracks == 0 || aAvailableSize <= 0)) {
      return;
    }
    if (aAvailableSize == NS_UNCONSTRAINEDSIZE) {
      for (TrackSize& sz : mSizes) {
        sz.mBase = sz.mLimit;
      }
    } else {
      // Compute free space and count growable tracks.
      nscoord space = aAvailableSize;
      uint32_t numGrowable = numTracks;
      for (const TrackSize& sz : mSizes) {
        space -= sz.mBase;
        MOZ_ASSERT(sz.mBase <= sz.mLimit);
        if (sz.mBase == sz.mLimit) {
          --numGrowable;
        }
      }
      // Distribute the free space evenly to the growable tracks. If not exactly
      // divisable the remainder is added to the leading tracks.
      while (space > 0 && numGrowable) {
        nscoord spacePerTrack = std::max<nscoord>(space / numGrowable, 1);
        for (uint32_t i = 0; i < numTracks && space > 0; ++i) {
          TrackSize& sz = mSizes[i];
          if (sz.mBase == sz.mLimit) {
            continue;
          }
          nscoord newBase = sz.mBase + spacePerTrack;
          if (newBase >= sz.mLimit) {
            space -= sz.mLimit - sz.mBase;
            sz.mBase = sz.mLimit;
            --numGrowable;
          } else {
            space -= spacePerTrack;
            sz.mBase = newBase;
          }
        }
      }
    }
  }

  /**
   * Implements "12.7.1. Find the Size of an 'fr'".
   * http://dev.w3.org/csswg/css-grid/#algo-find-fr-size
   * (The returned value is a 'nscoord' divided by a factor - a floating type
   * is used to avoid intermediary rounding errors.)
   */
  float FindFrUnitSize(const LineRange& aRange,
                       const nsTArray<uint32_t>& aFlexTracks,
                       const TrackSizingFunctions& aFunctions,
                       nscoord aSpaceToFill) const;

  /**
   * Implements the "find the used flex fraction" part of StretchFlexibleTracks.
   * (The returned value is a 'nscoord' divided by a factor - a floating type
   * is used to avoid intermediary rounding errors.)
   */
  float FindUsedFlexFraction(GridReflowInput& aState,
                             nsTArray<GridItemInfo>& aGridItems,
                             const nsTArray<uint32_t>& aFlexTracks,
                             const TrackSizingFunctions& aFunctions,
                             nscoord aAvailableSize) const;

  /**
   * Implements "12.7. Stretch Flexible Tracks"
   * http://dev.w3.org/csswg/css-grid/#algo-flex-tracks
   */
  void StretchFlexibleTracks(GridReflowInput& aState,
                             nsTArray<GridItemInfo>& aGridItems,
                             const TrackSizingFunctions& aFunctions,
                             nscoord aAvailableSize);

  /**
   * Implements "12.3. Track Sizing Algorithm"
   * http://dev.w3.org/csswg/css-grid/#algo-track-sizing
   */
  void CalculateSizes(GridReflowInput& aState,
                      nsTArray<GridItemInfo>& aGridItems,
                      const TrackSizingFunctions& aFunctions,
                      nscoord aContentBoxSize, LineRange GridArea::*aRange,
                      SizingConstraint aConstraint);

  /**
   * Apply 'align/justify-content', whichever is relevant for this axis.
   * https://drafts.csswg.org/css-align-3/#propdef-align-content
   */
  void AlignJustifyContent(const nsStylePosition* aStyle, WritingMode aWM,
                           nscoord aContentBoxSize, bool aIsSubgridded);

  nscoord GridLineEdge(uint32_t aLine, GridLineSide aSide) const {
    if (MOZ_UNLIKELY(mSizes.IsEmpty())) {
      // https://drafts.csswg.org/css-grid/#grid-definition
      // "... the explicit grid still contains one grid line in each axis."
      MOZ_ASSERT(aLine == 0, "We should only resolve line 1 in an empty grid");
      return nscoord(0);
    }
    MOZ_ASSERT(aLine <= mSizes.Length(), "mSizes is too small");
    if (aSide == GridLineSide::BeforeGridGap) {
      if (aLine == 0) {
        return nscoord(0);
      }
      const TrackSize& sz = mSizes[aLine - 1];
      return sz.mPosition + sz.mBase;
    }
    if (aLine == mSizes.Length()) {
      return mContentBoxSize;
    }
    return mSizes[aLine].mPosition;
  }

  nscoord SumOfGridGaps() const {
    auto len = mSizes.Length();
    return MOZ_LIKELY(len > 1) ? (len - 1) * mGridGap : 0;
  }

  /**
   * Break before aRow, i.e. set the eBreakBefore flag on aRow and set the grid
   * gap before aRow to zero (and shift all rows after it by the removed gap).
   */
  void BreakBeforeRow(uint32_t aRow) {
    MOZ_ASSERT(mAxis == eLogicalAxisBlock,
               "Should only be fragmenting in the block axis (between rows)");
    nscoord prevRowEndPos = 0;
    if (aRow != 0) {
      auto& prevSz = mSizes[aRow - 1];
      prevRowEndPos = prevSz.mPosition + prevSz.mBase;
    }
    auto& sz = mSizes[aRow];
    const nscoord gap = sz.mPosition - prevRowEndPos;
    sz.mState |= TrackSize::eBreakBefore;
    if (gap != 0) {
      for (uint32_t i = aRow, len = mSizes.Length(); i < len; ++i) {
        mSizes[i].mPosition -= gap;
      }
    }
  }

  /**
   * Set the size of aRow to aSize and adjust the position of all rows after it.
   */
  void ResizeRow(uint32_t aRow, nscoord aNewSize) {
    MOZ_ASSERT(mAxis == eLogicalAxisBlock,
               "Should only be fragmenting in the block axis (between rows)");
    MOZ_ASSERT(aNewSize >= 0);
    auto& sz = mSizes[aRow];
    nscoord delta = aNewSize - sz.mBase;
    NS_WARNING_ASSERTION(delta != nscoord(0), "Useless call to ResizeRow");
    sz.mBase = aNewSize;
    const uint32_t numRows = mSizes.Length();
    for (uint32_t r = aRow + 1; r < numRows; ++r) {
      mSizes[r].mPosition += delta;
    }
  }

  nscoord ResolveSize(const LineRange& aRange) const {
    MOZ_ASSERT(mCanResolveLineRangeSize);
    MOZ_ASSERT(aRange.Extent() > 0, "grid items cover at least one track");
    nscoord pos, size;
    aRange.ToPositionAndLength(mSizes, &pos, &size);
    return size;
  }

#ifdef DEBUG
  void Dump() const {
    for (uint32_t i = 0, len = mSizes.Length(); i < len; ++i) {
      printf("  %d: ", i);
      mSizes[i].Dump();
      printf("\n");
    }
  }
#endif

  AutoTArray<TrackSize, 32> mSizes;
  nscoord mContentBoxSize;
  nscoord mGridGap;
  // The first(last)-baseline for the first(last) track in this axis.
  PerBaseline<nscoord> mBaseline;
  // The union of the track min/max-sizing state bits in this axis.
  TrackSize::StateBits mStateUnion;
  LogicalAxis mAxis;
  // Used for aligning a baseline-aligned subtree of items.  The only possible
  // values are StyleAlignFlags::{START,END,CENTER,AUTO}.  AUTO means there are
  // no baseline-aligned items in any track in that axis.
  // There is one alignment value for each BaselineSharingGroup.
  PerBaseline<StyleAlignFlags> mBaselineSubtreeAlign;
  // True if track positions and sizes are final in this axis.
  bool mCanResolveLineRangeSize;
};

/**
 * Grid data shared by all continuations, owned by the first-in-flow.
 * The data is initialized from the first-in-flow's GridReflowInput at
 * the end of its reflow.  Fragmentation will modify mRows.mSizes -
 * the mPosition to remove the row gap at the break boundary, the mState
 * by setting the eBreakBefore flag, and mBase is modified when we decide
 * to grow a row.  mOriginalRowData is setup by the first-in-flow and
 * not modified after that.  It's used for undoing the changes to mRows.
 * mCols, mGridItems, mAbsPosItems are used for initializing the grid
 * reflow input for continuations, see GridReflowInput::Initialize below.
 */
struct nsGridContainerFrame::SharedGridData {
  SharedGridData()
      : mCols(eLogicalAxisInline),
        mRows(eLogicalAxisBlock),
        mGenerateComputedGridInfo(false) {}
  Tracks mCols;
  Tracks mRows;
  struct RowData {
    nscoord mBase;  // the original track size
    nscoord mGap;   // the original gap before a track
  };
  nsTArray<RowData> mOriginalRowData;
  nsTArray<GridItemInfo> mGridItems;
  nsTArray<GridItemInfo> mAbsPosItems;
  bool mGenerateComputedGridInfo;

  /**
   * Only set on the first-in-flow.  Continuations will Initialize() their
   * GridReflowInput from it.
   */
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(Prop, SharedGridData)
};

struct MOZ_STACK_CLASS nsGridContainerFrame::GridReflowInput {
  GridReflowInput(nsGridContainerFrame* aFrame, const ReflowInput& aRI)
      : GridReflowInput(aFrame, *aRI.mRenderingContext, &aRI,
                        aRI.mStylePosition, aRI.GetWritingMode()) {}
  GridReflowInput(nsGridContainerFrame* aFrame, gfxContext& aRC)
      : GridReflowInput(aFrame, aRC, nullptr, aFrame->StylePosition(),
                        aFrame->GetWritingMode()) {}

  /**
   * Initialize our track sizes and grid item info using the shared
   * state from aGridContainerFrame first-in-flow.
   */
  void InitializeForContinuation(nsGridContainerFrame* aGridContainerFrame,
                                 nscoord aConsumedBSize) {
    MOZ_ASSERT(aGridContainerFrame->GetPrevInFlow(),
               "don't call this on the first-in-flow");
    MOZ_ASSERT(mGridItems.IsEmpty() && mAbsPosItems.IsEmpty(),
               "shouldn't have any item data yet");

    // Get the SharedGridData from the first-in-flow. Also calculate the number
    // of fragments before this so that we can figure out our start row below.
    uint32_t fragment = 0;
    nsIFrame* firstInFlow = aGridContainerFrame;
    for (auto pif = aGridContainerFrame->GetPrevInFlow(); pif;
         pif = pif->GetPrevInFlow()) {
      ++fragment;
      firstInFlow = pif;
    }
    mSharedGridData = firstInFlow->GetProperty(SharedGridData::Prop());
    MOZ_ASSERT(mSharedGridData, "first-in-flow must have SharedGridData");

    // Find the start row for this fragment and undo breaks after that row
    // since the breaks might be different from the last reflow.
    auto& rowSizes = mSharedGridData->mRows.mSizes;
    const uint32_t numRows = rowSizes.Length();
    mStartRow = numRows;
    for (uint32_t row = 0, breakCount = 0; row < numRows; ++row) {
      if (rowSizes[row].mState & TrackSize::eBreakBefore) {
        if (fragment == ++breakCount) {
          mStartRow = row;
          mFragBStart = rowSizes[row].mPosition;
          // Restore the original size for |row| and grid gaps / state after it.
          const auto& origRowData = mSharedGridData->mOriginalRowData;
          rowSizes[row].mBase = origRowData[row].mBase;
          nscoord prevEndPos = rowSizes[row].mPosition + rowSizes[row].mBase;
          while (++row < numRows) {
            auto& sz = rowSizes[row];
            const auto& orig = origRowData[row];
            sz.mPosition = prevEndPos + orig.mGap;
            sz.mBase = orig.mBase;
            sz.mState &= ~TrackSize::eBreakBefore;
            prevEndPos = sz.mPosition + sz.mBase;
          }
          break;
        }
      }
    }
    if (mStartRow == numRows) {
      // All of the grid's rows fit inside of previous grid-container fragments.
      mFragBStart = aConsumedBSize;
    }

    // Copy the shared track state.
    // XXX consider temporarily swapping the array elements instead and swapping
    // XXX them back after we're done reflowing, for better performance.
    // XXX (bug 1252002)
    mCols = mSharedGridData->mCols;
    mRows = mSharedGridData->mRows;

    if (firstInFlow->GetProperty(UsedTrackSizes::Prop())) {
      auto* prop = aGridContainerFrame->GetProperty(UsedTrackSizes::Prop());
      if (!prop) {
        prop = new UsedTrackSizes();
        aGridContainerFrame->SetProperty(UsedTrackSizes::Prop(), prop);
      }
      prop->mCanResolveLineRangeSize = {true, true};
      prop->mSizes[eLogicalAxisInline] = mCols.mSizes;
      prop->mSizes[eLogicalAxisBlock] = mRows.mSizes;
    }

    // Copy item data from each child's first-in-flow data in mSharedGridData.
    // XXX NOTE: This is O(n^2) in the number of items. (bug 1252186)
    mIter.Reset();
    for (; !mIter.AtEnd(); mIter.Next()) {
      nsIFrame* child = *mIter;
      nsIFrame* childFirstInFlow = child->FirstInFlow();
      DebugOnly<size_t> len = mGridItems.Length();
      for (auto& itemInfo : mSharedGridData->mGridItems) {
        if (itemInfo.mFrame == childFirstInFlow) {
          auto item =
              mGridItems.AppendElement(GridItemInfo(child, itemInfo.mArea));
          // Copy the item's baseline data so that the item's last fragment can
          // do 'last baseline' alignment if necessary.
          item->mState[0] |= itemInfo.mState[0] & ItemState::eAllBaselineBits;
          item->mState[1] |= itemInfo.mState[1] & ItemState::eAllBaselineBits;
          item->mBaselineOffset[0] = itemInfo.mBaselineOffset[0];
          item->mBaselineOffset[1] = itemInfo.mBaselineOffset[1];
          break;
        }
      }
      MOZ_ASSERT(mGridItems.Length() == len + 1, "can't find GridItemInfo");
    }

    // XXX NOTE: This is O(n^2) in the number of abs.pos. items. (bug 1252186)
    nsFrameList absPosChildren(aGridContainerFrame->GetChildList(
        aGridContainerFrame->GetAbsoluteListID()));
    for (auto f : absPosChildren) {
      nsIFrame* childFirstInFlow = f->FirstInFlow();
      DebugOnly<size_t> len = mAbsPosItems.Length();
      for (auto& itemInfo : mSharedGridData->mAbsPosItems) {
        if (itemInfo.mFrame == childFirstInFlow) {
          mAbsPosItems.AppendElement(GridItemInfo(f, itemInfo.mArea));
          break;
        }
      }
      MOZ_ASSERT(mAbsPosItems.Length() == len + 1, "can't find GridItemInfo");
    }

    // Copy in the computed grid info state bit
    if (mSharedGridData->mGenerateComputedGridInfo) {
      aGridContainerFrame->AddStateBits(NS_STATE_GRID_GENERATE_COMPUTED_VALUES);
    }
  }

  /**
   * Calculate our track sizes in the given axis.
   */
  void CalculateTrackSizesForAxis(LogicalAxis aAxis, const Grid& aGrid,
                                  nscoord aCBSize,
                                  SizingConstraint aConstraint);

  /**
   * Calculate our track sizes.
   */
  void CalculateTrackSizes(const Grid& aGrid, const LogicalSize& aContentBox,
                           SizingConstraint aConstraint);

  /**
   * Return the percentage basis for a grid item in its writing-mode.
   * If aAxis is eLogicalAxisInline then we return NS_UNCONSTRAINEDSIZE in
   * both axes since we know all track sizes are indefinite at this point
   * (we calculate column sizes before row sizes).  Otherwise, assert that
   * column sizes are known and calculate the size for aGridItem.mArea.mCols
   * and use NS_UNCONSTRAINEDSIZE in the other axis.
   * @param aAxis the axis we're currently calculating track sizes for
   */
  LogicalSize PercentageBasisFor(LogicalAxis aAxis,
                                 const GridItemInfo& aGridItem) const;

  /**
   * Return the containing block for a grid item occupying aArea.
   */
  LogicalRect ContainingBlockFor(const GridArea& aArea) const;

  /**
   * Return the containing block for an abs.pos. grid item occupying aArea.
   * Any 'auto' lines in the grid area will be aligned with grid container
   * containing block on that side.
   * @param aGridOrigin the origin of the grid
   * @param aGridCB the grid container containing block (its padding area)
   */
  LogicalRect ContainingBlockForAbsPos(const GridArea& aArea,
                                       const LogicalPoint& aGridOrigin,
                                       const LogicalRect& aGridCB) const;

  // Helper for CollectSubgridItemsForAxis.
  static void CollectSubgridForAxis(LogicalAxis aAxis, WritingMode aContainerWM,
                                    const LineRange& aRangeInAxis,
                                    const LineRange& aRangeInOppositeAxis,
                                    const GridItemInfo& aItem,
                                    const nsTArray<GridItemInfo>& aItems,
                                    nsTArray<GridItemInfo>& aResult) {
    const auto oppositeAxis = GetOrthogonalAxis(aAxis);
    bool itemIsSubgridInOppositeAxis = aItem.IsSubgrid(oppositeAxis);
    auto subgridWM = aItem.mFrame->GetWritingMode();
    bool isOrthogonal = subgridWM.IsOrthogonalTo(aContainerWM);
    bool isSameDirInAxis =
        subgridWM.ParallelAxisStartsOnSameSide(aAxis, aContainerWM);
    bool isSameDirInOppositeAxis =
        subgridWM.ParallelAxisStartsOnSameSide(oppositeAxis, aContainerWM);
    if (isOrthogonal) {
      // We'll Transpose the area below so these needs to be transposed as well.
      std::swap(isSameDirInAxis, isSameDirInOppositeAxis);
    }
    uint32_t offsetInAxis = aRangeInAxis.mStart;
    uint32_t gridEndInAxis = aRangeInAxis.Extent();
    uint32_t offsetInOppositeAxis = aRangeInOppositeAxis.mStart;
    uint32_t gridEndInOppositeAxis = aRangeInOppositeAxis.Extent();
    for (const auto& subgridItem : aItems) {
      auto newItem = aResult.AppendElement(
          isOrthogonal ? subgridItem.Transpose() : subgridItem);
      if (MOZ_UNLIKELY(!isSameDirInAxis)) {
        newItem->ReverseDirection(aAxis, gridEndInAxis);
      }
      newItem->mArea.LineRangeForAxis(aAxis).Translate(offsetInAxis);
      if (itemIsSubgridInOppositeAxis) {
        if (MOZ_UNLIKELY(!isSameDirInOppositeAxis)) {
          newItem->ReverseDirection(oppositeAxis, gridEndInOppositeAxis);
        }
        LineRange& range = newItem->mArea.LineRangeForAxis(oppositeAxis);
        range.Translate(offsetInOppositeAxis);
      }
      if (newItem->IsSubgrid(aAxis)) {
        auto* subgrid =
            subgridItem.SubgridFrame()->GetProperty(Subgrid::Prop());
        CollectSubgridForAxis(aAxis, aContainerWM,
                              newItem->mArea.LineRangeForAxis(aAxis),
                              newItem->mArea.LineRangeForAxis(oppositeAxis),
                              *newItem, subgrid->mGridItems, aResult);
      }
    }
  }

  // Copy all descendant items from all our subgrid children that are subgridded
  // in aAxis recursively into aResult.  All item grid area's and state are
  // translated to our coordinates.
  void CollectSubgridItemsForAxis(LogicalAxis aAxis,
                                  nsTArray<GridItemInfo>& aResult) const {
    for (const auto& item : mGridItems) {
      if (item.IsSubgrid(aAxis)) {
        const auto oppositeAxis = GetOrthogonalAxis(aAxis);
        auto* subgrid = item.SubgridFrame()->GetProperty(Subgrid::Prop());
        CollectSubgridForAxis(aAxis, mWM, item.mArea.LineRangeForAxis(aAxis),
                              item.mArea.LineRangeForAxis(oppositeAxis), item,
                              subgrid->mGridItems, aResult);
      }
    }
  }

  CSSOrderAwareFrameIterator mIter;
  const nsStylePosition* const mGridStyle;
  Tracks mCols;
  Tracks mRows;
  TrackSizingFunctions mColFunctions;
  TrackSizingFunctions mRowFunctions;
  /**
   * Info about each (normal flow) grid item.
   */
  nsTArray<GridItemInfo> mGridItems;
  /**
   * Info about each grid-aligned abs.pos. child.
   */
  nsTArray<GridItemInfo> mAbsPosItems;

  /**
   * @note mReflowInput may be null when using the 2nd ctor above. In this case
   * we'll construct a dummy parent reflow input if we need it to calculate
   * min/max-content contributions when sizing tracks.
   */
  const ReflowInput* const mReflowInput;
  gfxContext& mRenderingContext;
  nsGridContainerFrame* const mFrame;
  SharedGridData* mSharedGridData;  // [weak] owned by mFrame's first-in-flow.
  /** Computed border+padding with mSkipSides applied. */
  LogicalMargin mBorderPadding;
  /**
   * BStart of this fragment in "grid space" (i.e. the concatenation of content
   * areas of all fragments).  Equal to mRows.mSizes[mStartRow].mPosition,
   * or, if this fragment starts after the last row, the ConsumedBSize().
   */
  nscoord mFragBStart;
  /** The start row for this fragment. */
  uint32_t mStartRow;
  /**
   * The start row for the next fragment, if any.  If mNextFragmentStartRow ==
   * mStartRow then there are no rows in this fragment.
   */
  uint32_t mNextFragmentStartRow;
  /** Our tentative ApplySkipSides bits. */
  LogicalSides mSkipSides;
  const WritingMode mWM;
  /** Initialized lazily, when we find the fragmentainer. */
  bool mInFragmentainer;

 private:
  GridReflowInput(nsGridContainerFrame* aFrame, gfxContext& aRenderingContext,
                  const ReflowInput* aReflowInput,
                  const nsStylePosition* aGridStyle, const WritingMode& aWM)
      : mIter(aFrame, kPrincipalList),
        mGridStyle(aGridStyle),
        mCols(eLogicalAxisInline),
        mRows(eLogicalAxisBlock),
        mColFunctions(mGridStyle->mGridTemplateColumns,
                      mGridStyle->mGridAutoColumns,
                      aFrame->IsSubgrid(eLogicalAxisInline)),
        mRowFunctions(mGridStyle->mGridTemplateRows, mGridStyle->mGridAutoRows,
                      aFrame->IsSubgrid(eLogicalAxisBlock)),
        mReflowInput(aReflowInput),
        mRenderingContext(aRenderingContext),
        mFrame(aFrame),
        mSharedGridData(nullptr),
        mBorderPadding(aWM),
        mFragBStart(0),
        mStartRow(0),
        mNextFragmentStartRow(0),
        mSkipSides(aFrame->GetWritingMode()),
        mWM(aWM),
        mInFragmentainer(false) {
    MOZ_ASSERT(!aReflowInput || aReflowInput->mFrame == mFrame);
    if (aReflowInput) {
      mBorderPadding = aReflowInput->ComputedLogicalBorderPadding();
      mSkipSides = aFrame->PreReflowBlockLevelLogicalSkipSides();
      mBorderPadding.ApplySkipSides(mSkipSides);
    }
  }
};

using GridReflowInput = nsGridContainerFrame::GridReflowInput;

/**
 * The Grid implements grid item placement and the state of the grid -
 * the size of the explicit/implicit grid, which cells are occupied etc.
 */
struct MOZ_STACK_CLASS nsGridContainerFrame::Grid {
  explicit Grid(const Grid* aParentGrid = nullptr) : mParentGrid(aParentGrid) {}

  /**
   * Place all child frames into the grid and expand the (implicit) grid as
   * needed.  The allocated GridAreas are stored in the GridAreaProperty
   * frame property on the child frame.
   * @param aRepeatSizing the container's [min-|max-]*size - used to determine
   *   the number of repeat(auto-fill/fit) tracks.
   */
  void PlaceGridItems(GridReflowInput& aState,
                      const RepeatTrackSizingInput& aRepeatSizing);

  void SubgridPlaceGridItems(GridReflowInput& aParentState, Grid* aParentGrid,
                             const GridItemInfo& aGridItem);

  /**
   * As above but for an abs.pos. child.  Any 'auto' lines will be represented
   * by kAutoLine in the LineRange result.
   * @param aGridStart the first line in the final, but untranslated grid
   * @param aGridEnd the last line in the final, but untranslated grid
   */
  LineRange ResolveAbsPosLineRange(const StyleGridLine& aStart,
                                   const StyleGridLine& aEnd,
                                   const LineNameMap& aNameMap,
                                   LogicalAxis aAxis, uint32_t aExplicitGridEnd,
                                   int32_t aGridStart, int32_t aGridEnd,
                                   const nsStylePosition* aStyle);

  /**
   * Return a GridArea for abs.pos. item with non-auto lines placed at
   * a definite line (1-based) with placement errors resolved.  One or both
   * positions may still be 'auto'.
   * @param aChild the abs.pos. grid item to place
   * @param aStyle the StylePosition() for the grid container
   */
  GridArea PlaceAbsPos(nsIFrame* aChild, const LineNameMap& aColLineNameMap,
                       const LineNameMap& aRowLineNameMap,
                       const nsStylePosition* aStyle);

  /**
   * Find the first column in row aLockedRow starting at aStartCol where aArea
   * could be placed without overlapping other items.  The returned column may
   * cause aArea to overflow the current implicit grid bounds if placed there.
   */
  uint32_t FindAutoCol(uint32_t aStartCol, uint32_t aLockedRow,
                       const GridArea* aArea) const;

  /**
   * Place aArea in the first column (in row aArea->mRows.mStart) starting at
   * aStartCol without overlapping other items.  The resulting aArea may
   * overflow the current implicit grid bounds.
   * @param aClampMaxColLine the maximum allowed column line number (zero-based)
   * Pre-condition: aArea->mRows.IsDefinite() is true.
   * Post-condition: aArea->IsDefinite() is true.
   */
  void PlaceAutoCol(uint32_t aStartCol, GridArea* aArea,
                    uint32_t aClampMaxColLine) const;

  /**
   * Find the first row in column aLockedCol starting at aStartRow where aArea
   * could be placed without overlapping other items.  The returned row may
   * cause aArea to overflow the current implicit grid bounds if placed there.
   */
  uint32_t FindAutoRow(uint32_t aLockedCol, uint32_t aStartRow,
                       const GridArea* aArea) const;

  /**
   * Place aArea in the first row (in column aArea->mCols.mStart) starting at
   * aStartRow without overlapping other items. The resulting aArea may
   * overflow the current implicit grid bounds.
   * @param aClampMaxRowLine the maximum allowed row line number (zero-based)
   * Pre-condition: aArea->mCols.IsDefinite() is true.
   * Post-condition: aArea->IsDefinite() is true.
   */
  void PlaceAutoRow(uint32_t aStartRow, GridArea* aArea,
                    uint32_t aClampMaxRowLine) const;

  /**
   * Place aArea in the first column starting at aStartCol,aStartRow without
   * causing it to overlap other items or overflow mGridColEnd.
   * If there's no such column in aStartRow, continue in position 1,aStartRow+1.
   * @param aClampMaxColLine the maximum allowed column line number (zero-based)
   * @param aClampMaxRowLine the maximum allowed row line number (zero-based)
   * Pre-condition: aArea->mCols.IsAuto() && aArea->mRows.IsAuto() is true.
   * Post-condition: aArea->IsDefinite() is true.
   */
  void PlaceAutoAutoInRowOrder(uint32_t aStartCol, uint32_t aStartRow,
                               GridArea* aArea, uint32_t aClampMaxColLine,
                               uint32_t aClampMaxRowLine) const;

  /**
   * Place aArea in the first row starting at aStartCol,aStartRow without
   * causing it to overlap other items or overflow mGridRowEnd.
   * If there's no such row in aStartCol, continue in position aStartCol+1,1.
   * @param aClampMaxColLine the maximum allowed column line number (zero-based)
   * @param aClampMaxRowLine the maximum allowed row line number (zero-based)
   * Pre-condition: aArea->mCols.IsAuto() && aArea->mRows.IsAuto() is true.
   * Post-condition: aArea->IsDefinite() is true.
   */
  void PlaceAutoAutoInColOrder(uint32_t aStartCol, uint32_t aStartRow,
                               GridArea* aArea, uint32_t aClampMaxColLine,
                               uint32_t aClampMaxRowLine) const;

  /**
   * Return aLine if it's inside the aMin..aMax range (inclusive),
   * otherwise return kAutoLine.
   */
  static int32_t AutoIfOutside(int32_t aLine, int32_t aMin, int32_t aMax) {
    MOZ_ASSERT(aMin <= aMax);
    if (aLine < aMin || aLine > aMax) {
      return kAutoLine;
    }
    return aLine;
  }

  /**
   * Inflate the implicit grid to include aArea.
   * @param aArea may be definite or auto
   */
  void InflateGridFor(const GridArea& aArea) {
    mGridColEnd = std::max(mGridColEnd, aArea.mCols.HypotheticalEnd());
    mGridRowEnd = std::max(mGridRowEnd, aArea.mRows.HypotheticalEnd());
    MOZ_ASSERT(mGridColEnd <= kTranslatedMaxLine &&
               mGridRowEnd <= kTranslatedMaxLine);
  }

  /**
   * Calculates the empty tracks in a repeat(auto-fit).
   * @param aOutNumEmptyLines Outputs the number of tracks which are empty.
   * @param aSizingFunctions Sizing functions for the relevant axis.
   * @param aNumGridLines Number of grid lines for the relevant axis.
   * @param aIsEmptyFunc Functor to check if a cell is empty. This should be
   * mCellMap.IsColEmpty or mCellMap.IsRowEmpty, depending on the axis.
   */
  template <typename IsEmptyFuncT>
  static Maybe<nsTArray<uint32_t>> CalculateAdjustForAutoFitElements(
      uint32_t* aOutNumEmptyTracks, TrackSizingFunctions& aSizingFunctions,
      uint32_t aNumGridLines, IsEmptyFuncT aIsEmptyFunc);

  /**
   * Return a line number for (non-auto) aLine, per:
   * http://dev.w3.org/csswg/css-grid/#line-placement
   * @param aLine style data for the line (must be non-auto)
   * @param aNth a number of lines to find from aFromIndex, negative if the
   *             search should be in reverse order.  In the case aLine has
   *             a specified line name, it's permitted to pass in zero which
   *             will be treated as one.
   * @param aFromIndex the zero-based index to start counting from
   * @param aLineNameList the explicit named lines
   * @param aSide the axis+edge we're resolving names for (e.g. if we're
                  resolving a grid-row-start line, pass eLogicalSideBStart)
   * @param aExplicitGridEnd the last line in the explicit grid
   * @param aStyle the StylePosition() for the grid container
   * @return a definite line (1-based), clamped to
   *   the mClampMinLine..mClampMaxLine range
   */
  int32_t ResolveLine(const StyleGridLine& aLine, int32_t aNth,
                      uint32_t aFromIndex, const LineNameMap& aNameMap,
                      LogicalSide aSide, uint32_t aExplicitGridEnd,
                      const nsStylePosition* aStyle);

  /**
   * Helper method for ResolveLineRange.
   * @see ResolveLineRange
   * @return a pair (start,end) of lines
   */
  typedef std::pair<int32_t, int32_t> LinePair;
  LinePair ResolveLineRangeHelper(const StyleGridLine& aStart,
                                  const StyleGridLine& aEnd,
                                  const LineNameMap& aNameMap,
                                  LogicalAxis aAxis, uint32_t aExplicitGridEnd,
                                  const nsStylePosition* aStyle);

  /**
   * Return a LineRange based on the given style data. Non-auto lines
   * are resolved to a definite line number (1-based) per:
   * http://dev.w3.org/csswg/css-grid/#line-placement
   * with placement errors corrected per:
   * http://dev.w3.org/csswg/css-grid/#grid-placement-errors
   * @param aStyle the StylePosition() for the grid container
   * @param aStart style data for the start line
   * @param aEnd style data for the end line
   * @param aLineNameList the explicit named lines
   * @param aAxis the axis we're resolving names in
   * @param aExplicitGridEnd the last line in the explicit grid
   * @param aStyle the StylePosition() for the grid container
   */
  LineRange ResolveLineRange(const StyleGridLine& aStart,
                             const StyleGridLine& aEnd,
                             const LineNameMap& aNameMap, LogicalAxis aAxis,
                             uint32_t aExplicitGridEnd,
                             const nsStylePosition* aStyle);

  /**
   * Return a GridArea with non-auto lines placed at a definite line (1-based)
   * with placement errors resolved.  One or both positions may still
   * be 'auto'.
   * @param aChild the grid item
   * @param aStyle the StylePosition() for the grid container
   */
  GridArea PlaceDefinite(nsIFrame* aChild, const LineNameMap& aColLineNameMap,
                         const LineNameMap& aRowLineNameMap,
                         const nsStylePosition* aStyle);

  bool HasImplicitNamedArea(nsAtom* aName) const {
    return mAreas && mAreas->has(aName);
  }

  // Return true if aString ends in aSuffix and has at least one character
  // before the suffix. Assign aIndex to where the suffix starts.
  static bool IsNameWithSuffix(nsAtom* aString, const nsString& aSuffix,
                               uint32_t* aIndex) {
    if (StringEndsWith(nsDependentAtomString(aString), aSuffix)) {
      *aIndex = aString->GetLength() - aSuffix.Length();
      return *aIndex != 0;
    }
    return false;
  }

  static bool IsNameWithEndSuffix(nsAtom* aString, uint32_t* aIndex) {
    return IsNameWithSuffix(aString, NS_LITERAL_STRING("-end"), aIndex);
  }

  static bool IsNameWithStartSuffix(nsAtom* aString, uint32_t* aIndex) {
    return IsNameWithSuffix(aString, NS_LITERAL_STRING("-start"), aIndex);
  }

  // Return the relevant parent LineNameMap for the given subgrid axis aAxis.
  const LineNameMap* ParentLineMapForAxis(bool aIsOrthogonal,
                                          LogicalAxis aAxis) const {
    if (!mParentGrid) {
      return nullptr;
    }
    bool isRows = aIsOrthogonal == (aAxis == eLogicalAxisInline);
    return isRows ? mParentGrid->mRowNameMap : mParentGrid->mColNameMap;
  }

  void SetLineMaps(const LineNameMap* aColNameMap,
                   const LineNameMap* aRowNameMap) {
    mColNameMap = aColNameMap;
    mRowNameMap = aRowNameMap;
  }

  /**
   * A CellMap holds state for each cell in the grid.
   * It's row major.  It's sparse in the sense that it only has enough rows to
   * cover the last row that has a grid item.  Each row only has enough entries
   * to cover columns that are occupied *on that row*, i.e. it's not a full
   * matrix covering the entire implicit grid.  An absent Cell means that it's
   * unoccupied by any grid item.
   */
  struct CellMap {
    struct Cell {
      constexpr Cell() : mIsOccupied(false) {}
      bool mIsOccupied : 1;
    };

    void Fill(const GridArea& aGridArea) {
      MOZ_ASSERT(aGridArea.IsDefinite());
      MOZ_ASSERT(aGridArea.mRows.mStart < aGridArea.mRows.mEnd);
      MOZ_ASSERT(aGridArea.mCols.mStart < aGridArea.mCols.mEnd);
      const auto numRows = aGridArea.mRows.mEnd;
      const auto numCols = aGridArea.mCols.mEnd;
      mCells.EnsureLengthAtLeast(numRows);
      for (auto i = aGridArea.mRows.mStart; i < numRows; ++i) {
        nsTArray<Cell>& cellsInRow = mCells[i];
        cellsInRow.EnsureLengthAtLeast(numCols);
        for (auto j = aGridArea.mCols.mStart; j < numCols; ++j) {
          cellsInRow[j].mIsOccupied = true;
        }
      }
    }

    uint32_t IsEmptyCol(uint32_t aCol) const {
      for (auto& row : mCells) {
        if (aCol < row.Length() && row[aCol].mIsOccupied) {
          return false;
        }
      }
      return true;
    }
    uint32_t IsEmptyRow(uint32_t aRow) const {
      if (aRow >= mCells.Length()) {
        return true;
      }
      for (const Cell& cell : mCells[aRow]) {
        if (cell.mIsOccupied) {
          return false;
        }
      }
      return true;
    }
#ifdef DEBUG
    void Dump() const {
      const size_t numRows = mCells.Length();
      for (size_t i = 0; i < numRows; ++i) {
        const nsTArray<Cell>& cellsInRow = mCells[i];
        const size_t numCols = cellsInRow.Length();
        printf("%lu:\t", (unsigned long)i + 1);
        for (size_t j = 0; j < numCols; ++j) {
          printf(cellsInRow[j].mIsOccupied ? "X " : ". ");
        }
        printf("\n");
      }
    }
#endif

    nsTArray<nsTArray<Cell>> mCells;
  };

  /**
   * State for each cell in the grid.
   */
  CellMap mCellMap;
  /**
   * @see HasImplicitNamedArea.
   */
  ImplicitNamedAreas* mAreas;
  /**
   * The last column grid line (1-based) in the explicit grid.
   * (i.e. the number of explicit columns + 1)
   */
  uint32_t mExplicitGridColEnd;
  /**
   * The last row grid line (1-based) in the explicit grid.
   * (i.e. the number of explicit rows + 1)
   */
  uint32_t mExplicitGridRowEnd;
  // Same for the implicit grid, except these become zero-based after
  // resolving definite lines.
  uint32_t mGridColEnd;
  uint32_t mGridRowEnd;

  /**
   * Offsets from the start of the implicit grid to the start of the translated
   * explicit grid.  They are zero if there are no implicit lines before 1,1.
   * e.g. "grid-column: span 3 / 1" makes mExplicitGridOffsetCol = 3 and the
   * corresponding GridArea::mCols will be 0 / 3 in the zero-based translated
   * grid.
   */
  uint32_t mExplicitGridOffsetCol;
  uint32_t mExplicitGridOffsetRow;

  /**
   * Our parent grid if any.
   */
  const Grid* mParentGrid;

  /**
   * Our LineNameMaps.
   */
  const LineNameMap* mColNameMap;
  const LineNameMap* mRowNameMap;
};

/**
 * Compute margin+border+padding for aGridItem.mFrame (a subgrid) and store it
 * on its Subgrid property (and return that property).
 * aPercentageBasis is in the grid item's writing-mode.
 */
static Subgrid* SubgridComputeMarginBorderPadding(
    const GridItemInfo& aGridItem, const LogicalSize& aPercentageBasis) {
  auto* subgridFrame = aGridItem.SubgridFrame();
  auto cbWM = aGridItem.mFrame->GetParent()->GetWritingMode();
  nsMargin physicalMBP;
  {
    auto wm = subgridFrame->GetWritingMode();
    auto pmPercentageBasis = cbWM.IsOrthogonalTo(wm)
                                 ? aPercentageBasis.BSize(wm)
                                 : aPercentageBasis.ISize(wm);
    SizeComputationInput sz(subgridFrame, nullptr, cbWM, pmPercentageBasis);
    physicalMBP =
        sz.ComputedPhysicalMargin() + sz.ComputedPhysicalBorderPadding();
  }
  auto* subgrid = subgridFrame->GetProperty(Subgrid::Prop());
  subgrid->mMarginBorderPadding = LogicalMargin(cbWM, physicalMBP);
  if (aGridItem.mFrame != subgridFrame) {
    nsIScrollableFrame* scrollFrame = aGridItem.mFrame->GetScrollTargetFrame();
    if (scrollFrame) {
      nsMargin ssz = scrollFrame->GetActualScrollbarSizes();
      subgrid->mMarginBorderPadding += LogicalMargin(cbWM, ssz);
    }

    if (aGridItem.mFrame->IsFieldSetFrame()) {
      const auto* f = static_cast<nsFieldSetFrame*>(aGridItem.mFrame);
      const auto* inner = f->GetInner();
      auto wm = inner->GetWritingMode();
      LogicalPoint pos = inner->GetLogicalPosition(aGridItem.mFrame->GetSize());
      // The legend is always on the BStart side and it inflates the fieldset's
      // "border area" size.  The inner frame's b-start pos equals that size.
      LogicalMargin offsets(wm, pos.B(wm), 0, 0, 0);
      subgrid->mMarginBorderPadding += offsets.ConvertTo(cbWM, wm);
    }
  }
  return subgrid;
}

static void CopyUsedTrackSizes(nsTArray<TrackSize>& aResult,
                               const nsGridContainerFrame* aUsedTrackSizesFrame,
                               const UsedTrackSizes* aUsedTrackSizes,
                               const nsGridContainerFrame* aSubgridFrame,
                               const Subgrid* aSubgrid,
                               LogicalAxis aSubgridAxis) {
  MOZ_ASSERT(aSubgridFrame->ParentGridContainerForSubgrid() ==
             aUsedTrackSizesFrame);
  aResult.SetLength(aSubgridAxis == eLogicalAxisInline ? aSubgrid->mGridColEnd
                                                       : aSubgrid->mGridRowEnd);
  auto parentAxis =
      aSubgrid->mIsOrthogonal ? GetOrthogonalAxis(aSubgridAxis) : aSubgridAxis;
  const auto& parentSizes = aUsedTrackSizes->mSizes[parentAxis];
  MOZ_ASSERT(aUsedTrackSizes->mCanResolveLineRangeSize[parentAxis]);
  if (parentSizes.IsEmpty()) {
    return;
  }
  const auto& range = aSubgrid->mArea.LineRangeForAxis(parentAxis);
  const auto cbwm = aUsedTrackSizesFrame->GetWritingMode();
  const auto wm = aSubgridFrame->GetWritingMode();
  // Recompute the MBP to resolve percentages against the resolved track sizes.
  if (parentAxis == eLogicalAxisInline) {
    // Find the subgrid's grid item frame in its parent grid container.  This
    // is usually the same as aSubgridFrame but it may also have a ScrollFrame,
    // FieldSetFrame etc.  We just loop until we see the first ancestor
    // GridContainerFrame and pick the last frame we saw before that.
    // Note that all subgrids are inside a parent (sub)grid container.
    const nsIFrame* outerGridItemFrame = aSubgridFrame;
    for (nsIFrame* parent = aSubgridFrame->GetParent();
         parent != aUsedTrackSizesFrame; parent = parent->GetParent()) {
      MOZ_ASSERT(!parent->IsGridContainerFrame());
      outerGridItemFrame = parent;
    }
    auto sizeInAxis = range.ToLength(aUsedTrackSizes->mSizes[parentAxis]);
    LogicalSize pmPercentageBasis =
        aSubgrid->mIsOrthogonal ? LogicalSize(wm, nscoord(0), sizeInAxis)
                                : LogicalSize(wm, sizeInAxis, nscoord(0));
    GridItemInfo info(const_cast<nsIFrame*>(outerGridItemFrame),
                      aSubgrid->mArea);
    SubgridComputeMarginBorderPadding(info, pmPercentageBasis);
  }
  const LogicalMargin& mbp = aSubgrid->mMarginBorderPadding;
  nscoord startMBP;
  nscoord endMBP;
  if (MOZ_LIKELY(cbwm.ParallelAxisStartsOnSameSide(parentAxis, wm))) {
    startMBP = mbp.Start(parentAxis, cbwm);
    endMBP = mbp.End(parentAxis, cbwm);
    uint32_t i = range.mStart;
    nscoord startPos = parentSizes[i].mPosition + startMBP;
    for (auto& sz : aResult) {
      sz = parentSizes[i++];
      sz.mPosition -= startPos;
    }
  } else {
    startMBP = mbp.End(parentAxis, cbwm);
    endMBP = mbp.Start(parentAxis, cbwm);
    uint32_t i = range.mEnd - 1;
    const auto& parentEnd = parentSizes[i];
    nscoord parentEndPos = parentEnd.mPosition + parentEnd.mBase - startMBP;
    for (auto& sz : aResult) {
      sz = parentSizes[i--];
      sz.mPosition = parentEndPos - (sz.mPosition + sz.mBase);
    }
  }
  auto& startTrack = aResult[0];
  startTrack.mPosition = 0;
  startTrack.mBase -= startMBP;
  if (MOZ_UNLIKELY(startTrack.mBase < nscoord(0))) {
    // Our MBP doesn't fit in the start track.  Adjust the track position
    // to maintain track alignment with our parent.
    startTrack.mPosition = startTrack.mBase;
    startTrack.mBase = nscoord(0);
  }
  auto& endTrack = aResult.LastElement();
  endTrack.mBase -= endMBP;
  if (MOZ_UNLIKELY(endTrack.mBase < nscoord(0))) {
    endTrack.mBase = nscoord(0);
  }
}

void nsGridContainerFrame::UsedTrackSizes::ResolveTrackSizesForAxis(
    nsGridContainerFrame* aFrame, LogicalAxis aAxis, gfxContext& aRC) {
  if (mCanResolveLineRangeSize[aAxis]) {
    return;
  }
  if (!aFrame->IsSubgrid()) {
    // We can't resolve sizes in this axis at this point. aFrame is the top grid
    // container, which will store its final track sizes later once they're
    // resolved in this axis (in GridReflowInput::CalculateTrackSizesForAxis).
    // The single caller of this method only needs track sizes for
    // calculating a CB size and it will treat it as indefinite when
    // this happens.
    return;
  }
  auto* parent = aFrame->ParentGridContainerForSubgrid();
  auto* parentSizes = parent->GetUsedTrackSizes();
  if (!parentSizes) {
    parentSizes = new UsedTrackSizes();
    parent->SetProperty(UsedTrackSizes::Prop(), parentSizes);
  }
  auto* subgrid = aFrame->GetProperty(Subgrid::Prop());
  const auto parentAxis =
      subgrid->mIsOrthogonal ? GetOrthogonalAxis(aAxis) : aAxis;
  parentSizes->ResolveTrackSizesForAxis(parent, parentAxis, aRC);
  if (!parentSizes->mCanResolveLineRangeSize[parentAxis]) {
    if (aFrame->IsSubgrid(aAxis)) {
      ResolveSubgridTrackSizesForAxis(aFrame, aAxis, subgrid, aRC,
                                      NS_UNCONSTRAINEDSIZE);
    }
    return;
  }
  if (aFrame->IsSubgrid(aAxis)) {
    CopyUsedTrackSizes(mSizes[aAxis], parent, parentSizes, aFrame, subgrid,
                       aAxis);
    mCanResolveLineRangeSize[aAxis] = true;
  } else {
    const auto& range = subgrid->mArea.LineRangeForAxis(parentAxis);
    nscoord contentBoxSize = range.ToLength(parentSizes->mSizes[parentAxis]);
    auto parentWM = aFrame->GetParent()->GetWritingMode();
    contentBoxSize -=
        subgrid->mMarginBorderPadding.StartEnd(parentAxis, parentWM);
    contentBoxSize = std::max(nscoord(0), contentBoxSize);
    ResolveSubgridTrackSizesForAxis(aFrame, aAxis, subgrid, aRC,
                                    contentBoxSize);
  }
}

void nsGridContainerFrame::UsedTrackSizes::ResolveSubgridTrackSizesForAxis(
    nsGridContainerFrame* aFrame, LogicalAxis aAxis, Subgrid* aSubgrid,
    gfxContext& aRC, nscoord aContentBoxSize) {
  GridReflowInput state(aFrame, aRC);
  state.mGridItems = aSubgrid->mGridItems;
  Grid grid;
  grid.mGridColEnd = aSubgrid->mGridColEnd;
  grid.mGridRowEnd = aSubgrid->mGridRowEnd;
  state.CalculateTrackSizesForAxis(aAxis, grid, aContentBoxSize,
                                   SizingConstraint::NoConstraint);
  const auto& tracks = aAxis == eLogicalAxisInline ? state.mCols : state.mRows;
  mSizes[aAxis] = tracks.mSizes;
  mCanResolveLineRangeSize[aAxis] = tracks.mCanResolveLineRangeSize;
  MOZ_ASSERT(mCanResolveLineRangeSize[aAxis]);
}

void nsGridContainerFrame::GridReflowInput::CalculateTrackSizesForAxis(
    LogicalAxis aAxis, const Grid& aGrid, nscoord aContentBoxSize,
    SizingConstraint aConstraint) {
  auto& tracks = aAxis == eLogicalAxisInline ? mCols : mRows;
  const auto& sizingFunctions =
      aAxis == eLogicalAxisInline ? mColFunctions : mRowFunctions;
  const auto& gapStyle = aAxis == eLogicalAxisInline ? mGridStyle->mColumnGap
                                                     : mGridStyle->mRowGap;
  uint32_t gridEnd =
      aAxis == eLogicalAxisInline ? aGrid.mGridColEnd : aGrid.mGridRowEnd;
  Maybe<TrackSizingFunctions> fallbackTrackSizing;

  bool useParentGaps = false;
  const bool isSubgriddedAxis = mFrame->IsSubgrid(aAxis);
  if (MOZ_LIKELY(!isSubgriddedAxis)) {
    tracks.Initialize(sizingFunctions, gapStyle, gridEnd, aContentBoxSize);
  } else {
    tracks.mGridGap =
        nsLayoutUtils::ResolveGapToLength(gapStyle, aContentBoxSize);
    tracks.mContentBoxSize = aContentBoxSize;
    const auto* subgrid = mFrame->GetProperty(Subgrid::Prop());
    tracks.mSizes.SetLength(gridEnd);
    auto* parent = mFrame->ParentGridContainerForSubgrid();
    auto parentAxis = subgrid->mIsOrthogonal ? GetOrthogonalAxis(aAxis) : aAxis;
    const auto* parentSizes = parent->GetUsedTrackSizes();
    if (parentSizes && parentSizes->mCanResolveLineRangeSize[parentAxis]) {
      CopyUsedTrackSizes(tracks.mSizes, parent, parentSizes, mFrame, subgrid,
                         aAxis);
      useParentGaps = gapStyle.IsNormal();
    } else {
      fallbackTrackSizing.emplace(TrackSizingFunctions::ForSubgridFallback(
          mFrame, subgrid, parent, parentAxis));
      tracks.Initialize(*fallbackTrackSizing, gapStyle, gridEnd,
                        aContentBoxSize);
    }
  }

  // We run the Track Sizing Algorithm in non-subgridded axes, and in some
  // cases in a subgridded axis when our parent track sizes aren't resolved yet.
  if (MOZ_LIKELY(!isSubgriddedAxis) || fallbackTrackSizing.isSome()) {
    const size_t origGridItemCount = mGridItems.Length();
    if (mFrame->HasSubgridItems(aAxis)) {
      CollectSubgridItemsForAxis(aAxis, mGridItems);
    }
    tracks.CalculateSizes(
        *this, mGridItems,
        fallbackTrackSizing ? *fallbackTrackSizing : sizingFunctions,
        aContentBoxSize,
        aAxis == eLogicalAxisInline ? &GridArea::mCols : &GridArea::mRows,
        aConstraint);
    // XXXmats we're losing the baseline state of subgrid descendants that
    // CollectSubgridItemsForAxis added here.  We need to propagate that
    // state into the subgrid's Reflow somehow...
    mGridItems.TruncateLength(origGridItemCount);
  }

  if (aContentBoxSize != NS_UNCONSTRAINEDSIZE) {
    tracks.AlignJustifyContent(mGridStyle, mWM, aContentBoxSize,
                               isSubgriddedAxis);
  } else if (!useParentGaps) {
    const nscoord gridGap = tracks.mGridGap;
    nscoord pos = 0;
    for (TrackSize& sz : tracks.mSizes) {
      sz.mPosition = pos;
      pos += sz.mBase + gridGap;
    }
  }

  if (aConstraint == SizingConstraint::NoConstraint &&
      (mFrame->HasSubgridItems() || mFrame->IsSubgrid())) {
    mFrame->StoreUsedTrackSizes(aAxis, tracks.mSizes);
  }

  // positions and sizes are now final
  tracks.mCanResolveLineRangeSize = true;
}

void nsGridContainerFrame::GridReflowInput::CalculateTrackSizes(
    const Grid& aGrid, const LogicalSize& aContentBox,
    SizingConstraint aConstraint) {
  CalculateTrackSizesForAxis(eLogicalAxisInline, aGrid, aContentBox.ISize(mWM),
                             aConstraint);
  CalculateTrackSizesForAxis(eLogicalAxisBlock, aGrid, aContentBox.BSize(mWM),
                             aConstraint);
}

/**
 * (XXX share this utility function with nsFlexContainerFrame at some point)
 *
 * Helper for BuildDisplayList, to implement this special-case for grid
 * items from the spec:
 *   The painting order of grid items is exactly the same as inline blocks,
 *   except that [...] 'z-index' values other than 'auto' create a stacking
 *   context even if 'position' is 'static'.
 * http://dev.w3.org/csswg/css-grid/#z-order
 */
static uint32_t GetDisplayFlagsForGridItem(nsIFrame* aFrame) {
  const nsStylePosition* pos = aFrame->StylePosition();
  if (pos->mZIndex.IsInteger()) {
    return nsIFrame::DISPLAY_CHILD_FORCE_STACKING_CONTEXT;
  }
  return nsIFrame::DISPLAY_CHILD_FORCE_PSEUDO_STACKING_CONTEXT;
}

// Align an item's margin box in its aAxis inside aCBSize.
static void AlignJustifySelf(StyleAlignFlags aAlignment, LogicalAxis aAxis,
                             AlignJustifyFlags aFlags, nscoord aBaselineAdjust,
                             nscoord aCBSize, const ReflowInput& aRI,
                             const LogicalSize& aChildSize,
                             LogicalPoint* aPos) {
  MOZ_ASSERT(aAlignment != StyleAlignFlags::AUTO,
             "unexpected 'auto' "
             "computed value for normal flow grid item");

  // NOTE: this is the resulting frame offset (border box).
  nscoord offset = CSSAlignUtils::AlignJustifySelf(
      aAlignment, aAxis, aFlags, aBaselineAdjust, aCBSize, aRI, aChildSize);

  // Set the position (aPos) for the requested alignment.
  if (offset != 0) {
    WritingMode wm = aRI.GetWritingMode();
    nscoord& pos = aAxis == eLogicalAxisBlock ? aPos->B(wm) : aPos->I(wm);
    pos += MOZ_LIKELY(aFlags & AlignJustifyFlags::SameSide) ? offset : -offset;
  }
}

static void AlignSelf(const nsGridContainerFrame::GridItemInfo& aGridItem,
                      StyleAlignSelf aAlignSelf, nscoord aCBSize,
                      const WritingMode aCBWM, const ReflowInput& aRI,
                      const LogicalSize& aSize, LogicalPoint* aPos) {
  auto alignSelf = aAlignSelf._0;

  AlignJustifyFlags flags = AlignJustifyFlags::NoFlags;
  if (alignSelf & StyleAlignFlags::SAFE) {
    flags |= AlignJustifyFlags::OverflowSafe;
  }
  alignSelf &= ~StyleAlignFlags::FLAG_BITS;

  WritingMode childWM = aRI.GetWritingMode();
  if (aCBWM.ParallelAxisStartsOnSameSide(eLogicalAxisBlock, childWM)) {
    flags |= AlignJustifyFlags::SameSide;
  }

  // Grid's 'align-self' axis is never parallel to the container's inline axis.
  if (alignSelf == StyleAlignFlags::LEFT ||
      alignSelf == StyleAlignFlags::RIGHT) {
    alignSelf = StyleAlignFlags::START;
  }
  if (MOZ_LIKELY(alignSelf == StyleAlignFlags::NORMAL)) {
    alignSelf = StyleAlignFlags::STRETCH;
  }

  nscoord baselineAdjust = 0;
  if (alignSelf == StyleAlignFlags::BASELINE ||
      alignSelf == StyleAlignFlags::LAST_BASELINE) {
    alignSelf = aGridItem.GetSelfBaseline(alignSelf, eLogicalAxisBlock,
                                          &baselineAdjust);
  }

  bool isOrthogonal = aCBWM.IsOrthogonalTo(childWM);
  LogicalAxis axis = isOrthogonal ? eLogicalAxisInline : eLogicalAxisBlock;
  AlignJustifySelf(alignSelf, axis, flags, baselineAdjust, aCBSize, aRI, aSize,
                   aPos);
}

static void JustifySelf(const nsGridContainerFrame::GridItemInfo& aGridItem,
                        StyleJustifySelf aJustifySelf, nscoord aCBSize,
                        const WritingMode aCBWM, const ReflowInput& aRI,
                        const LogicalSize& aSize, LogicalPoint* aPos) {
  auto justifySelf = aJustifySelf._0;

  AlignJustifyFlags flags = AlignJustifyFlags::NoFlags;
  if (justifySelf & StyleAlignFlags::SAFE) {
    flags |= AlignJustifyFlags::OverflowSafe;
  }
  justifySelf &= ~StyleAlignFlags::FLAG_BITS;

  WritingMode childWM = aRI.GetWritingMode();
  if (aCBWM.ParallelAxisStartsOnSameSide(eLogicalAxisInline, childWM)) {
    flags |= AlignJustifyFlags::SameSide;
  }

  if (MOZ_LIKELY(justifySelf == StyleAlignFlags::NORMAL)) {
    justifySelf = StyleAlignFlags::STRETCH;
  }

  nscoord baselineAdjust = 0;
  // Grid's 'justify-self' axis is always parallel to the container's inline
  // axis, so justify-self:left|right always applies.
  if (justifySelf == StyleAlignFlags::LEFT) {
    justifySelf =
        aCBWM.IsBidiLTR() ? StyleAlignFlags::START : StyleAlignFlags::END;
  } else if (justifySelf == StyleAlignFlags::RIGHT) {
    justifySelf =
        aCBWM.IsBidiLTR() ? StyleAlignFlags::END : StyleAlignFlags::START;
  } else if (justifySelf == StyleAlignFlags::BASELINE ||
             justifySelf == StyleAlignFlags::LAST_BASELINE) {
    justifySelf = aGridItem.GetSelfBaseline(justifySelf, eLogicalAxisInline,
                                            &baselineAdjust);
  }

  bool isOrthogonal = aCBWM.IsOrthogonalTo(childWM);
  LogicalAxis axis = isOrthogonal ? eLogicalAxisBlock : eLogicalAxisInline;
  AlignJustifySelf(justifySelf, axis, flags, baselineAdjust, aCBSize, aRI,
                   aSize, aPos);
}

static StyleAlignFlags GetAlignJustifyValue(StyleAlignFlags aAlignment,
                                            const WritingMode aWM,
                                            const bool aIsAlign,
                                            bool* aOverflowSafe) {
  *aOverflowSafe = bool(aAlignment & StyleAlignFlags::SAFE);
  aAlignment &= ~StyleAlignFlags::FLAG_BITS;

  // Map some alignment values to 'start' / 'end'.
  if (aAlignment == StyleAlignFlags::LEFT ||
      aAlignment == StyleAlignFlags::RIGHT) {
    if (aIsAlign) {
      // Grid's 'align-content' axis is never parallel to the inline axis.
      return StyleAlignFlags::START;
    }
    bool isStart = aWM.IsBidiLTR() == (aAlignment == StyleAlignFlags::LEFT);
    return isStart ? StyleAlignFlags::START : StyleAlignFlags::END;
  }
  if (aAlignment == StyleAlignFlags::FLEX_START) {
    return StyleAlignFlags::START;  // same as 'start' for Grid
  }
  if (aAlignment == StyleAlignFlags::FLEX_END) {
    return StyleAlignFlags::END;  // same as 'end' for Grid
  }
  return aAlignment;
}

static Maybe<StyleAlignFlags> GetAlignJustifyFallbackIfAny(
    const StyleContentDistribution& aDistribution, const WritingMode aWM,
    const bool aIsAlign, bool* aOverflowSafe) {
  // TODO: Eventually this should look at aDistribution's fallback alignment,
  // see https://github.com/w3c/csswg-drafts/issues/1002.
  if (aDistribution.primary == StyleAlignFlags::STRETCH ||
      aDistribution.primary == StyleAlignFlags::SPACE_BETWEEN) {
    return Some(StyleAlignFlags::START);
  }
  if (aDistribution.primary == StyleAlignFlags::SPACE_AROUND ||
      aDistribution.primary == StyleAlignFlags::SPACE_EVENLY) {
    return Some(StyleAlignFlags::CENTER);
  }
  return Nothing();
}

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsGridContainerFrame)
  NS_QUERYFRAME_ENTRY(nsGridContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsGridContainerFrame)

nsContainerFrame* NS_NewGridContainerFrame(PresShell* aPresShell,
                                           ComputedStyle* aStyle) {
  return new (aPresShell)
      nsGridContainerFrame(aStyle, aPresShell->GetPresContext());
}

//----------------------------------------------------------------------

// nsGridContainerFrame Method Implementations
// ===========================================

/*static*/ const nsRect& nsGridContainerFrame::GridItemCB(nsIFrame* aChild) {
  MOZ_ASSERT((aChild->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
             aChild->IsAbsolutelyPositioned());
  nsRect* cb = aChild->GetProperty(GridItemContainingBlockRect());
  MOZ_ASSERT(cb,
             "this method must only be called on grid items, and the grid "
             "container should've reflowed this item by now and set up cb");
  return *cb;
}

void nsGridContainerFrame::AddImplicitNamedAreas(
    Span<LineNameList> aLineNameLists) {
  // http://dev.w3.org/csswg/css-grid/#implicit-named-areas
  // Note: recording these names for fast lookup later is just an optimization.
  const uint32_t len = std::min(aLineNameLists.Length(), size_t(kMaxLine));
  nsTHashtable<nsStringHashKey> currentStarts;
  ImplicitNamedAreas* areas = GetImplicitNamedAreas();
  for (uint32_t i = 0; i < len; ++i) {
    for (const auto& nameIdent : aLineNameLists[i].AsSpan()) {
      nsAtom* name = nameIdent.AsAtom();
      uint32_t indexOfSuffix;
      if (Grid::IsNameWithStartSuffix(name, &indexOfSuffix) ||
          Grid::IsNameWithEndSuffix(name, &indexOfSuffix)) {
        // Extract the name that was found earlier.
        nsDependentSubstring areaName(nsDependentAtomString(name), 0,
                                      indexOfSuffix);

        // Lazily create the ImplicitNamedAreas.
        if (!areas) {
          areas = new ImplicitNamedAreas;
          SetProperty(ImplicitNamedAreasProperty(), areas);
        }

        RefPtr<nsAtom> name = NS_Atomize(areaName);
        auto addPtr = areas->lookupForAdd(name);
        if (!addPtr) {
          if (!areas->add(
                  addPtr, name,
                  NamedArea{StyleAtom(do_AddRef(name)), {0, 0}, {0, 0}})) {
            MOZ_CRASH("OOM while adding grid name lists");
          }
        }
      }
    }
  }
}

void nsGridContainerFrame::InitImplicitNamedAreas(
    const nsStylePosition* aStyle) {
  ImplicitNamedAreas* areas = GetImplicitNamedAreas();
  if (areas) {
    // Clear it, but reuse the hashtable itself for now.  We'll remove it
    // below if it isn't needed anymore.
    areas->clear();
  }
  auto Add = [&](const GridTemplate& aTemplate, bool aIsSubgrid) {
    AddImplicitNamedAreas(aTemplate.LineNameLists(aIsSubgrid));
    for (auto& value : aTemplate.TrackListValues()) {
      if (value.IsTrackRepeat()) {
        AddImplicitNamedAreas(value.AsTrackRepeat().line_names.AsSpan());
      }
    }
  };
  Add(aStyle->mGridTemplateColumns, IsSubgrid(eLogicalAxisInline));
  Add(aStyle->mGridTemplateRows, IsSubgrid(eLogicalAxisBlock));
  if (areas && areas->count() == 0) {
    RemoveProperty(ImplicitNamedAreasProperty());
  }
}

int32_t nsGridContainerFrame::Grid::ResolveLine(
    const StyleGridLine& aLine, int32_t aNth, uint32_t aFromIndex,
    const LineNameMap& aNameMap, LogicalSide aSide, uint32_t aExplicitGridEnd,
    const nsStylePosition* aStyle) {
  MOZ_ASSERT(!aLine.IsAuto());
  int32_t line = 0;
  if (aLine.LineName()->IsEmpty()) {
    MOZ_ASSERT(aNth != 0, "css-grid 9.2: <integer> must not be zero.");
    line = int32_t(aFromIndex) + aNth;
  } else {
    if (aNth == 0) {
      // <integer> was omitted; treat it as 1.
      aNth = 1;
    }
    bool isNameOnly = !aLine.is_span && aLine.line_num == 0;
    if (isNameOnly) {
      AutoTArray<uint32_t, 16> implicitLines;
      aNameMap.FindNamedAreas(aLine.ident.AsAtom(), aSide, implicitLines);
      if (!implicitLines.IsEmpty() ||
          aNameMap.HasImplicitNamedArea(aLine.LineName())) {
        // aName is a named area - look for explicit lines named
        // <name>-start/-end depending on which side we're resolving.
        // http://dev.w3.org/csswg/css-grid/#grid-placement-slot
        nsAutoString lineName(nsDependentAtomString(aLine.LineName()));
        if (IsStart(aSide)) {
          lineName.AppendLiteral("-start");
        } else {
          lineName.AppendLiteral("-end");
        }
        RefPtr<nsAtom> name = NS_Atomize(lineName);
        line = aNameMap.FindNamedLine(name, &aNth, aFromIndex, implicitLines);
      }
    }

    if (line == 0) {
      // If LineName() ends in -start/-end, try the prefix as a named area.
      AutoTArray<uint32_t, 16> implicitLines;
      uint32_t index;
      bool useStart = IsNameWithStartSuffix(aLine.LineName(), &index);
      if (useStart || IsNameWithEndSuffix(aLine.LineName(), &index)) {
        auto side = MakeLogicalSide(
            GetAxis(aSide), useStart ? eLogicalEdgeStart : eLogicalEdgeEnd);
        RefPtr<nsAtom> name = NS_Atomize(nsDependentSubstring(
            nsDependentAtomString(aLine.LineName()), 0, index));
        aNameMap.FindNamedAreas(name, side, implicitLines);
      }
      line = aNameMap.FindNamedLine(aLine.LineName(), &aNth, aFromIndex,
                                    implicitLines);
    }

    if (line == 0) {
      MOZ_ASSERT(aNth != 0, "we found all N named lines but 'line' is zero!");
      int32_t edgeLine;
      if (aLine.is_span) {
        // http://dev.w3.org/csswg/css-grid/#grid-placement-span-int
        // 'span <custom-ident> N'
        edgeLine = IsStart(aSide) ? 1 : aExplicitGridEnd;
      } else {
        // http://dev.w3.org/csswg/css-grid/#grid-placement-int
        // '<custom-ident> N'
        edgeLine = aNth < 0 ? 1 : aExplicitGridEnd;
      }
      // "If not enough lines with that name exist, all lines in the implicit
      // grid are assumed to have that name..."
      line = edgeLine + aNth;
    }
  }
  return clamped(line, aNameMap.mClampMinLine, aNameMap.mClampMaxLine);
}

nsGridContainerFrame::Grid::LinePair
nsGridContainerFrame::Grid::ResolveLineRangeHelper(
    const StyleGridLine& aStart, const StyleGridLine& aEnd,
    const LineNameMap& aNameMap, LogicalAxis aAxis, uint32_t aExplicitGridEnd,
    const nsStylePosition* aStyle) {
  MOZ_ASSERT(int32_t(kAutoLine) > kMaxLine);

  if (aStart.is_span) {
    if (aEnd.is_span || aEnd.IsAuto()) {
      // http://dev.w3.org/csswg/css-grid/#grid-placement-errors
      if (aStart.LineName()->IsEmpty()) {
        // span <integer> / span *
        // span <integer> / auto
        return LinePair(kAutoLine, aStart.line_num);
      }
      // span <custom-ident> / span *
      // span <custom-ident> / auto
      return LinePair(kAutoLine, 1);  // XXX subgrid explicit size instead of 1?
    }

    uint32_t from = aEnd.line_num < 0 ? aExplicitGridEnd + 1 : 0;
    auto end = ResolveLine(aEnd, aEnd.line_num, from, aNameMap,
                           MakeLogicalSide(aAxis, eLogicalEdgeEnd),
                           aExplicitGridEnd, aStyle);
    int32_t span = aStart.line_num == 0 ? 1 : aStart.line_num;
    if (end <= 1) {
      // The end is at or before the first explicit line, thus all lines before
      // it match <custom-ident> since they're implicit.
      int32_t start = std::max(end - span, aNameMap.mClampMinLine);
      return LinePair(start, end);
    }
    auto start = ResolveLine(aStart, -span, end, aNameMap,
                             MakeLogicalSide(aAxis, eLogicalEdgeStart),
                             aExplicitGridEnd, aStyle);
    return LinePair(start, end);
  }

  int32_t start = kAutoLine;
  if (aStart.IsAuto()) {
    if (aEnd.IsAuto()) {
      // auto / auto
      return LinePair(start, 1);  // XXX subgrid explicit size instead of 1?
    }
    if (aEnd.is_span) {
      if (aEnd.LineName()->IsEmpty()) {
        // auto / span <integer>
        MOZ_ASSERT(aEnd.line_num != 0);
        return LinePair(start, aEnd.line_num);
      }
      // http://dev.w3.org/csswg/css-grid/#grid-placement-errors
      // auto / span <custom-ident>
      return LinePair(start, 1);  // XXX subgrid explicit size instead of 1?
    }
  } else {
    uint32_t from = aStart.line_num < 0 ? aExplicitGridEnd + 1 : 0;
    start = ResolveLine(aStart, aStart.line_num, from, aNameMap,
                        MakeLogicalSide(aAxis, eLogicalEdgeStart),
                        aExplicitGridEnd, aStyle);
    if (aEnd.IsAuto()) {
      // A "definite line / auto" should resolve the auto to 'span 1'.
      // The error handling in ResolveLineRange will make that happen and also
      // clamp the end line correctly if we return "start / start".
      return LinePair(start, start);
    }
  }

  uint32_t from;
  int32_t nth = aEnd.line_num == 0 ? 1 : aEnd.line_num;
  if (aEnd.is_span) {
    if (MOZ_UNLIKELY(start < 0)) {
      if (aEnd.LineName()->IsEmpty()) {
        return LinePair(start, start + nth);
      }
      from = 0;
    } else {
      if (start >= int32_t(aExplicitGridEnd)) {
        // The start is at or after the last explicit line, thus all lines
        // after it match <custom-ident> since they're implicit.
        return LinePair(start, std::min(start + nth, aNameMap.mClampMaxLine));
      }
      from = start;
    }
  } else {
    from = aEnd.line_num < 0 ? aExplicitGridEnd + 1 : 0;
  }
  auto end = ResolveLine(aEnd, nth, from, aNameMap,
                         MakeLogicalSide(aAxis, eLogicalEdgeEnd),
                         aExplicitGridEnd, aStyle);
  if (start == int32_t(kAutoLine)) {
    // auto / definite line
    start = std::max(aNameMap.mClampMinLine, end - 1);
  }
  return LinePair(start, end);
}

nsGridContainerFrame::LineRange nsGridContainerFrame::Grid::ResolveLineRange(
    const StyleGridLine& aStart, const StyleGridLine& aEnd,
    const LineNameMap& aNameMap, LogicalAxis aAxis, uint32_t aExplicitGridEnd,
    const nsStylePosition* aStyle) {
  LinePair r = ResolveLineRangeHelper(aStart, aEnd, aNameMap, aAxis,
                                      aExplicitGridEnd, aStyle);
  MOZ_ASSERT(r.second != int32_t(kAutoLine));

  if (r.first == int32_t(kAutoLine)) {
    // r.second is a span, clamp it to aNameMap.mClampMaxLine - 1 so that
    // the returned range has a HypotheticalEnd <= aNameMap.mClampMaxLine.
    // http://dev.w3.org/csswg/css-grid/#overlarge-grids
    r.second = std::min(r.second, aNameMap.mClampMaxLine - 1);
  } else {
    // http://dev.w3.org/csswg/css-grid/#grid-placement-errors
    if (r.first > r.second) {
      std::swap(r.first, r.second);
    } else if (r.first == r.second) {
      if (MOZ_UNLIKELY(r.first == aNameMap.mClampMaxLine)) {
        r.first = aNameMap.mClampMaxLine - 1;
      }
      r.second = r.first + 1;  // XXX subgrid explicit size instead of 1?
    }
  }
  return LineRange(r.first, r.second);
}

nsGridContainerFrame::GridArea nsGridContainerFrame::Grid::PlaceDefinite(
    nsIFrame* aChild, const LineNameMap& aColLineNameMap,
    const LineNameMap& aRowLineNameMap, const nsStylePosition* aStyle) {
  const nsStylePosition* itemStyle = aChild->StylePosition();
  return GridArea(
      ResolveLineRange(itemStyle->mGridColumnStart, itemStyle->mGridColumnEnd,
                       aColLineNameMap, eLogicalAxisInline, mExplicitGridColEnd,
                       aStyle),
      ResolveLineRange(itemStyle->mGridRowStart, itemStyle->mGridRowEnd,
                       aRowLineNameMap, eLogicalAxisBlock, mExplicitGridRowEnd,
                       aStyle));
}

nsGridContainerFrame::LineRange
nsGridContainerFrame::Grid::ResolveAbsPosLineRange(
    const StyleGridLine& aStart, const StyleGridLine& aEnd,
    const LineNameMap& aNameMap, LogicalAxis aAxis, uint32_t aExplicitGridEnd,
    int32_t aGridStart, int32_t aGridEnd, const nsStylePosition* aStyle) {
  if (aStart.IsAuto()) {
    if (aEnd.IsAuto()) {
      return LineRange(kAutoLine, kAutoLine);
    }
    uint32_t from = aEnd.line_num < 0 ? aExplicitGridEnd + 1 : 0;
    int32_t end = ResolveLine(aEnd, aEnd.line_num, from, aNameMap,
                              MakeLogicalSide(aAxis, eLogicalEdgeEnd),
                              aExplicitGridEnd, aStyle);
    if (aEnd.is_span) {
      ++end;
    }
    // A line outside the existing grid is treated as 'auto' for abs.pos (10.1).
    end = AutoIfOutside(end, aGridStart, aGridEnd);
    return LineRange(kAutoLine, end);
  }

  if (aEnd.IsAuto()) {
    uint32_t from = aStart.line_num < 0 ? aExplicitGridEnd + 1 : 0;
    int32_t start = ResolveLine(aStart, aStart.line_num, from, aNameMap,
                                MakeLogicalSide(aAxis, eLogicalEdgeStart),
                                aExplicitGridEnd, aStyle);
    if (aStart.is_span) {
      start = std::max(aGridEnd - start, aGridStart);
    }
    start = AutoIfOutside(start, aGridStart, aGridEnd);
    return LineRange(start, kAutoLine);
  }

  LineRange r =
      ResolveLineRange(aStart, aEnd, aNameMap, aAxis, aExplicitGridEnd, aStyle);
  if (r.IsAuto()) {
    MOZ_ASSERT(aStart.is_span && aEnd.is_span,
               "span / span is the only case "
               "leading to IsAuto here -- we dealt with the other cases above");
    // The second span was ignored per 9.2.1.  For abs.pos., 10.1 says that this
    // case should result in "auto / auto" unlike normal flow grid items.
    return LineRange(kAutoLine, kAutoLine);
  }

  return LineRange(AutoIfOutside(r.mUntranslatedStart, aGridStart, aGridEnd),
                   AutoIfOutside(r.mUntranslatedEnd, aGridStart, aGridEnd));
}

nsGridContainerFrame::GridArea nsGridContainerFrame::Grid::PlaceAbsPos(
    nsIFrame* aChild, const LineNameMap& aColLineNameMap,
    const LineNameMap& aRowLineNameMap, const nsStylePosition* aStyle) {
  const nsStylePosition* itemStyle = aChild->StylePosition();
  int32_t gridColStart = 1 - mExplicitGridOffsetCol;
  int32_t gridRowStart = 1 - mExplicitGridOffsetRow;
  return GridArea(ResolveAbsPosLineRange(
                      itemStyle->mGridColumnStart, itemStyle->mGridColumnEnd,
                      aColLineNameMap, eLogicalAxisInline, mExplicitGridColEnd,
                      gridColStart, mGridColEnd, aStyle),
                  ResolveAbsPosLineRange(
                      itemStyle->mGridRowStart, itemStyle->mGridRowEnd,
                      aRowLineNameMap, eLogicalAxisBlock, mExplicitGridRowEnd,
                      gridRowStart, mGridRowEnd, aStyle));
}

uint32_t nsGridContainerFrame::Grid::FindAutoCol(uint32_t aStartCol,
                                                 uint32_t aLockedRow,
                                                 const GridArea* aArea) const {
  const uint32_t extent = aArea->mCols.Extent();
  const uint32_t iStart = aLockedRow;
  const uint32_t iEnd = iStart + aArea->mRows.Extent();
  uint32_t candidate = aStartCol;
  for (uint32_t i = iStart; i < iEnd;) {
    if (i >= mCellMap.mCells.Length()) {
      break;
    }
    const nsTArray<CellMap::Cell>& cellsInRow = mCellMap.mCells[i];
    const uint32_t len = cellsInRow.Length();
    const uint32_t lastCandidate = candidate;
    // Find the first gap in the current row that's at least 'extent' wide.
    // ('gap' tracks how wide the current column gap is.)
    for (uint32_t j = candidate, gap = 0; j < len && gap < extent; ++j) {
      if (!cellsInRow[j].mIsOccupied) {
        ++gap;
        continue;
      }
      candidate = j + 1;
      gap = 0;
    }
    if (lastCandidate < candidate && i != iStart) {
      // Couldn't fit 'extent' tracks at 'lastCandidate' here so we must
      // restart from the beginning with the new 'candidate'.
      i = iStart;
    } else {
      ++i;
    }
  }
  return candidate;
}

void nsGridContainerFrame::Grid::PlaceAutoCol(uint32_t aStartCol,
                                              GridArea* aArea,
                                              uint32_t aClampMaxColLine) const {
  MOZ_ASSERT(aArea->mRows.IsDefinite() && aArea->mCols.IsAuto());
  uint32_t col = FindAutoCol(aStartCol, aArea->mRows.mStart, aArea);
  aArea->mCols.ResolveAutoPosition(col, aClampMaxColLine);
  MOZ_ASSERT(aArea->IsDefinite());
}

uint32_t nsGridContainerFrame::Grid::FindAutoRow(uint32_t aLockedCol,
                                                 uint32_t aStartRow,
                                                 const GridArea* aArea) const {
  const uint32_t extent = aArea->mRows.Extent();
  const uint32_t jStart = aLockedCol;
  const uint32_t jEnd = jStart + aArea->mCols.Extent();
  const uint32_t iEnd = mCellMap.mCells.Length();
  uint32_t candidate = aStartRow;
  // Find the first gap in the rows that's at least 'extent' tall.
  // ('gap' tracks how tall the current row gap is.)
  for (uint32_t i = candidate, gap = 0; i < iEnd && gap < extent; ++i) {
    ++gap;  // tentative, but we may reset it below if a column is occupied
    const nsTArray<CellMap::Cell>& cellsInRow = mCellMap.mCells[i];
    const uint32_t clampedJEnd = std::min<uint32_t>(jEnd, cellsInRow.Length());
    // Check if the current row is unoccupied from jStart to jEnd.
    for (uint32_t j = jStart; j < clampedJEnd; ++j) {
      if (cellsInRow[j].mIsOccupied) {
        // Couldn't fit 'extent' rows at 'candidate' here; we hit something
        // at row 'i'.  So, try the row after 'i' as our next candidate.
        candidate = i + 1;
        gap = 0;
        break;
      }
    }
  }
  return candidate;
}

void nsGridContainerFrame::Grid::PlaceAutoRow(uint32_t aStartRow,
                                              GridArea* aArea,
                                              uint32_t aClampMaxRowLine) const {
  MOZ_ASSERT(aArea->mCols.IsDefinite() && aArea->mRows.IsAuto());
  uint32_t row = FindAutoRow(aArea->mCols.mStart, aStartRow, aArea);
  aArea->mRows.ResolveAutoPosition(row, aClampMaxRowLine);
  MOZ_ASSERT(aArea->IsDefinite());
}

void nsGridContainerFrame::Grid::PlaceAutoAutoInRowOrder(
    uint32_t aStartCol, uint32_t aStartRow, GridArea* aArea,
    uint32_t aClampMaxColLine, uint32_t aClampMaxRowLine) const {
  MOZ_ASSERT(aArea->mCols.IsAuto() && aArea->mRows.IsAuto());
  const uint32_t colExtent = aArea->mCols.Extent();
  const uint32_t gridRowEnd = mGridRowEnd;
  const uint32_t gridColEnd = mGridColEnd;
  uint32_t col = aStartCol;
  uint32_t row = aStartRow;
  for (; row < gridRowEnd; ++row) {
    col = FindAutoCol(col, row, aArea);
    if (col + colExtent <= gridColEnd) {
      break;
    }
    col = 0;
  }
  MOZ_ASSERT(row < gridRowEnd || col == 0,
             "expected column 0 for placing in a new row");
  aArea->mCols.ResolveAutoPosition(col, aClampMaxColLine);
  aArea->mRows.ResolveAutoPosition(row, aClampMaxRowLine);
  MOZ_ASSERT(aArea->IsDefinite());
}

void nsGridContainerFrame::Grid::PlaceAutoAutoInColOrder(
    uint32_t aStartCol, uint32_t aStartRow, GridArea* aArea,
    uint32_t aClampMaxColLine, uint32_t aClampMaxRowLine) const {
  MOZ_ASSERT(aArea->mCols.IsAuto() && aArea->mRows.IsAuto());
  const uint32_t rowExtent = aArea->mRows.Extent();
  const uint32_t gridRowEnd = mGridRowEnd;
  const uint32_t gridColEnd = mGridColEnd;
  uint32_t col = aStartCol;
  uint32_t row = aStartRow;
  for (; col < gridColEnd; ++col) {
    row = FindAutoRow(col, row, aArea);
    if (row + rowExtent <= gridRowEnd) {
      break;
    }
    row = 0;
  }
  MOZ_ASSERT(col < gridColEnd || row == 0,
             "expected row 0 for placing in a new column");
  aArea->mCols.ResolveAutoPosition(col, aClampMaxColLine);
  aArea->mRows.ResolveAutoPosition(row, aClampMaxRowLine);
  MOZ_ASSERT(aArea->IsDefinite());
}

template <typename IsEmptyFuncT>
Maybe<nsTArray<uint32_t>>
nsGridContainerFrame::Grid::CalculateAdjustForAutoFitElements(
    uint32_t* const aOutNumEmptyLines, TrackSizingFunctions& aSizingFunctions,
    uint32_t aNumGridLines, IsEmptyFuncT aIsEmptyFunc) {
  Maybe<nsTArray<uint32_t>> trackAdjust;
  uint32_t& numEmptyLines = *aOutNumEmptyLines;
  numEmptyLines = 0;
  if (aSizingFunctions.NumRepeatTracks() > 0) {
    MOZ_ASSERT(aSizingFunctions.mHasRepeatAuto);
    // Since this loop is concerned with just the repeat tracks, we
    // iterate from 0..NumRepeatTracks() which is the natural range of
    // mRemoveRepeatTracks. This means we have to add
    // (mExplicitGridOffset + mRepeatAutoStart) to get a zero-based
    // index for arrays like mCellMap/aIsEmptyFunc and trackAdjust. We'll then
    // fill out the trackAdjust array for all the remaining lines.
    const uint32_t repeatStart = (aSizingFunctions.mExplicitGridOffset +
                                  aSizingFunctions.mRepeatAutoStart);
    const uint32_t numRepeats = aSizingFunctions.NumRepeatTracks();
    for (uint32_t i = 0; i < numRepeats; ++i) {
      if (numEmptyLines) {
        MOZ_ASSERT(trackAdjust.isSome());
        (*trackAdjust)[repeatStart + i] = numEmptyLines;
      }
      if (aIsEmptyFunc(repeatStart + i)) {
        ++numEmptyLines;
        if (trackAdjust.isNothing()) {
          trackAdjust.emplace(aNumGridLines);
          trackAdjust->SetLength(aNumGridLines);
          PodZero(trackAdjust->Elements(), trackAdjust->Length());
        }

        aSizingFunctions.mRemovedRepeatTracks[i] = true;
      }
    }
    // Fill out the trackAdjust array for all the tracks after the repeats.
    if (numEmptyLines) {
      for (uint32_t line = repeatStart + numRepeats; line < aNumGridLines;
           ++line) {
        (*trackAdjust)[line] = numEmptyLines;
      }
    }
  }

  return trackAdjust;
}

void nsGridContainerFrame::Grid::SubgridPlaceGridItems(
    GridReflowInput& aParentState, Grid* aParentGrid,
    const GridItemInfo& aGridItem) {
  MOZ_ASSERT(aGridItem.mArea.IsDefinite() ||
                 aGridItem.mFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW),
             "the subgrid's lines should be resolved by now");
  if (aGridItem.IsSubgrid(eLogicalAxisInline)) {
    aParentState.mFrame->AddStateBits(NS_STATE_GRID_HAS_COL_SUBGRID_ITEM);
  }
  if (aGridItem.IsSubgrid(eLogicalAxisBlock)) {
    aParentState.mFrame->AddStateBits(NS_STATE_GRID_HAS_ROW_SUBGRID_ITEM);
  }
  auto* childGrid = aGridItem.SubgridFrame();
  const auto* pos = childGrid->StylePosition();
  childGrid->NormalizeChildLists();
  GridReflowInput state(childGrid, aParentState.mRenderingContext);
  childGrid->InitImplicitNamedAreas(pos);

  const bool isOrthogonal = aParentState.mWM.IsOrthogonalTo(state.mWM);
  // Record the subgrid's GridArea in a frame property.
  auto* subgrid = childGrid->GetProperty(Subgrid::Prop());
  if (!subgrid) {
    subgrid = new Subgrid(aGridItem.mArea, isOrthogonal, aParentState.mWM);
    childGrid->SetProperty(Subgrid::Prop(), subgrid);
  } else {
    subgrid->mArea = aGridItem.mArea;
    subgrid->mIsOrthogonal = isOrthogonal;
    subgrid->mGridItems.Clear();
    subgrid->mAbsPosItems.Clear();
  }

  // Abs.pos. subgrids may have kAutoLine in their area.  Map those to the edge
  // line in the parent's explicit grid (zero-based line numbers).  This is ok
  // because it's only used for translating lines and such, not for layout.
  if (MOZ_UNLIKELY(subgrid->mArea.mCols.mStart == kAutoLine)) {
    subgrid->mArea.mCols.mStart = 0;
  }
  if (MOZ_UNLIKELY(subgrid->mArea.mCols.mEnd == kAutoLine)) {
    subgrid->mArea.mCols.mEnd = aParentGrid->mExplicitGridOffsetCol +
                                aParentGrid->mExplicitGridColEnd - 1;
  }
  if (MOZ_UNLIKELY(subgrid->mArea.mRows.mStart == kAutoLine)) {
    subgrid->mArea.mRows.mStart = 0;
  }
  if (MOZ_UNLIKELY(subgrid->mArea.mRows.mEnd == kAutoLine)) {
    subgrid->mArea.mRows.mEnd = aParentGrid->mExplicitGridOffsetRow +
                                aParentGrid->mExplicitGridRowEnd - 1;
  }

  // The min/sz/max sizes are the input to the "repeat-to-fill" algorithm:
  // https://drafts.csswg.org/css-grid/#auto-repeat
  // They're only used for auto-repeat in a non-subgridded axis so we skip
  // computing them otherwise.
  RepeatTrackSizingInput repeatSizing(state.mWM);
  if (!childGrid->IsColSubgrid() && state.mColFunctions.mHasRepeatAuto) {
    repeatSizing.InitFromStyle(eLogicalAxisInline, state.mWM,
                               state.mFrame->Style());
  }
  if (!childGrid->IsRowSubgrid() && state.mRowFunctions.mHasRepeatAuto) {
    repeatSizing.InitFromStyle(eLogicalAxisBlock, state.mWM,
                               state.mFrame->Style());
  }

  PlaceGridItems(state, repeatSizing);

  subgrid->mGridItems = std::move(state.mGridItems);
  subgrid->mAbsPosItems = std::move(state.mAbsPosItems);
  subgrid->mGridColEnd = mGridColEnd;
  subgrid->mGridRowEnd = mGridRowEnd;
}

void nsGridContainerFrame::Grid::PlaceGridItems(
    GridReflowInput& aState, const RepeatTrackSizingInput& aSizes) {
  MOZ_ASSERT(mCellMap.mCells.IsEmpty(), "unexpected entries in cell map");

  mAreas = aState.mFrame->GetImplicitNamedAreas();

  if (aState.mFrame->HasSubgridItems() || aState.mFrame->IsSubgrid()) {
    if (auto* uts = aState.mFrame->GetUsedTrackSizes()) {
      uts->mCanResolveLineRangeSize = {false, false};
      uts->mSizes[eLogicalAxisInline].ClearAndRetainStorage();
      uts->mSizes[eLogicalAxisBlock].ClearAndRetainStorage();
    }
  }

  // SubgridPlaceGridItems will set these if we find any subgrid items.
  aState.mFrame->RemoveStateBits(NS_STATE_GRID_HAS_COL_SUBGRID_ITEM |
                                 NS_STATE_GRID_HAS_ROW_SUBGRID_ITEM);

  // http://dev.w3.org/csswg/css-grid/#grid-definition
  // Initialize the end lines of the Explicit Grid (mExplicitGridCol[Row]End).
  // This is determined by the larger of the number of rows/columns defined
  // by 'grid-template-areas' and the 'grid-template-rows'/'-columns', plus one.
  // Also initialize the Implicit Grid (mGridCol[Row]End) to the same values.
  // Note that this is for a grid with a 1,1 origin.  We'll change that
  // to a 0,0 based grid after placing definite lines.
  const nsStylePosition* const gridStyle = aState.mGridStyle;
  const auto* areas = gridStyle->mGridTemplateAreas.IsNone()
                          ? nullptr
                          : &*gridStyle->mGridTemplateAreas.AsAreas();
  const LineNameMap* parentLineNameMap = nullptr;
  const LineRange* subgridRange = nullptr;
  bool subgridAxisIsSameDirection = true;
  if (!aState.mFrame->IsColSubgrid()) {
    aState.mColFunctions.InitRepeatTracks(
        gridStyle->mColumnGap, aSizes.mMin.ISize(aState.mWM),
        aSizes.mSize.ISize(aState.mWM), aSizes.mMax.ISize(aState.mWM));
    uint32_t areaCols = areas ? areas->width + 1 : 1;
    mExplicitGridColEnd = aState.mColFunctions.ComputeExplicitGridEnd(areaCols);
  } else {
    const auto* subgrid = aState.mFrame->GetProperty(Subgrid::Prop());
    subgridRange = &subgrid->SubgridCols();
    uint32_t extent = subgridRange->Extent();
    mExplicitGridColEnd = extent + 1;  // the grid is 1-based at this point
    parentLineNameMap =
        ParentLineMapForAxis(subgrid->mIsOrthogonal, eLogicalAxisInline);
    auto parentWM =
        aState.mFrame->ParentGridContainerForSubgrid()->GetWritingMode();
    subgridAxisIsSameDirection =
        aState.mWM.ParallelAxisStartsOnSameSide(eLogicalAxisInline, parentWM);
  }
  mGridColEnd = mExplicitGridColEnd;
  LineNameMap colLineNameMap(gridStyle, mAreas, aState.mColFunctions,
                             parentLineNameMap, subgridRange,
                             subgridAxisIsSameDirection);

  if (!aState.mFrame->IsRowSubgrid()) {
    aState.mRowFunctions.InitRepeatTracks(
        gridStyle->mRowGap, aSizes.mMin.BSize(aState.mWM),
        aSizes.mSize.BSize(aState.mWM), aSizes.mMax.BSize(aState.mWM));
    uint32_t areaRows = areas ? areas->strings.Length() + 1 : 1;
    mExplicitGridRowEnd = aState.mRowFunctions.ComputeExplicitGridEnd(areaRows);
    parentLineNameMap = nullptr;
    subgridRange = nullptr;
  } else {
    const auto* subgrid = aState.mFrame->GetProperty(Subgrid::Prop());
    subgridRange = &subgrid->SubgridRows();
    uint32_t extent = subgridRange->Extent();
    mExplicitGridRowEnd = extent + 1;  // the grid is 1-based at this point
    parentLineNameMap =
        ParentLineMapForAxis(subgrid->mIsOrthogonal, eLogicalAxisBlock);
    auto parentWM =
        aState.mFrame->ParentGridContainerForSubgrid()->GetWritingMode();
    subgridAxisIsSameDirection =
        aState.mWM.ParallelAxisStartsOnSameSide(eLogicalAxisBlock, parentWM);
  }
  mGridRowEnd = mExplicitGridRowEnd;
  LineNameMap rowLineNameMap(gridStyle, mAreas, aState.mRowFunctions,
                             parentLineNameMap, subgridRange,
                             subgridAxisIsSameDirection);

  const bool isSubgridOrItemInSubgrid =
      aState.mFrame->IsSubgrid() || !!mParentGrid;
  auto SetSubgridChildEdgeBits =
      [this, isSubgridOrItemInSubgrid](GridItemInfo& aItem) -> void {
    if (isSubgridOrItemInSubgrid) {
      const auto& area = aItem.mArea;
      if (area.mCols.mStart == 0) {
        aItem.mState[eLogicalAxisInline] |= ItemState::eStartEdge;
      }
      if (area.mCols.mEnd == mGridColEnd) {
        aItem.mState[eLogicalAxisInline] |= ItemState::eEndEdge;
      }
      if (area.mRows.mStart == 0) {
        aItem.mState[eLogicalAxisBlock] |= ItemState::eStartEdge;
      }
      if (area.mRows.mEnd == mGridRowEnd) {
        aItem.mState[eLogicalAxisBlock] |= ItemState::eEndEdge;
      }
    }
  };

  SetLineMaps(&colLineNameMap, &rowLineNameMap);

  // http://dev.w3.org/csswg/css-grid/#line-placement
  // Resolve definite positions per spec chap 9.2.
  int32_t minCol = 1;
  int32_t minRow = 1;
  aState.mGridItems.ClearAndRetainStorage();
  aState.mIter.Reset();
  for (; !aState.mIter.AtEnd(); aState.mIter.Next()) {
    nsIFrame* child = *aState.mIter;
    GridItemInfo* info = aState.mGridItems.AppendElement(GridItemInfo(
        child,
        PlaceDefinite(child, colLineNameMap, rowLineNameMap, gridStyle)));
    MOZ_ASSERT(aState.mIter.ItemIndex() == aState.mGridItems.Length() - 1,
               "ItemIndex() is broken");
    GridArea& area = info->mArea;
    if (area.mCols.IsDefinite()) {
      minCol = std::min(minCol, area.mCols.mUntranslatedStart);
    }
    if (area.mRows.IsDefinite()) {
      minRow = std::min(minRow, area.mRows.mUntranslatedStart);
    }
  }

  // Translate the whole grid so that the top-/left-most area is at 0,0.
  mExplicitGridOffsetCol = 1 - minCol;  // minCol/Row is always <= 1, see above
  mExplicitGridOffsetRow = 1 - minRow;
  aState.mColFunctions.mExplicitGridOffset = mExplicitGridOffsetCol;
  aState.mRowFunctions.mExplicitGridOffset = mExplicitGridOffsetRow;
  const int32_t offsetToColZero = int32_t(mExplicitGridOffsetCol) - 1;
  const int32_t offsetToRowZero = int32_t(mExplicitGridOffsetRow) - 1;
  mGridColEnd += offsetToColZero;
  mGridRowEnd += offsetToRowZero;
  aState.mIter.Reset();
  for (; !aState.mIter.AtEnd(); aState.mIter.Next()) {
    auto& item = aState.mGridItems[aState.mIter.ItemIndex()];
    GridArea& area = item.mArea;
    if (area.mCols.IsDefinite()) {
      area.mCols.mStart = area.mCols.mUntranslatedStart + offsetToColZero;
      area.mCols.mEnd = area.mCols.mUntranslatedEnd + offsetToColZero;
    }
    if (area.mRows.IsDefinite()) {
      area.mRows.mStart = area.mRows.mUntranslatedStart + offsetToRowZero;
      area.mRows.mEnd = area.mRows.mUntranslatedEnd + offsetToRowZero;
    }
    if (area.IsDefinite()) {
      if (item.IsSubgrid()) {
        Grid grid(this);
        grid.SubgridPlaceGridItems(aState, this, item);
      }
      mCellMap.Fill(area);
      InflateGridFor(area);
      SetSubgridChildEdgeBits(item);
    }
  }

  // http://dev.w3.org/csswg/css-grid/#auto-placement-algo
  // Step 1, place 'auto' items that have one definite position -
  // definite row (column) for grid-auto-flow:row (column).
  auto flowStyle = gridStyle->mGridAutoFlow;
  const bool isRowOrder = bool(flowStyle & StyleGridAutoFlow::ROW);
  const bool isSparse = !(flowStyle & StyleGridAutoFlow::DENSE);
  uint32_t clampMaxColLine = colLineNameMap.mClampMaxLine + offsetToColZero;
  uint32_t clampMaxRowLine = rowLineNameMap.mClampMaxLine + offsetToRowZero;
  // We need 1 cursor per row (or column) if placement is sparse.
  {
    Maybe<nsDataHashtable<nsUint32HashKey, uint32_t>> cursors;
    if (isSparse) {
      cursors.emplace();
    }
    auto placeAutoMinorFunc =
        isRowOrder ? &Grid::PlaceAutoCol : &Grid::PlaceAutoRow;
    uint32_t clampMaxLine = isRowOrder ? clampMaxColLine : clampMaxRowLine;
    aState.mIter.Reset();
    for (; !aState.mIter.AtEnd(); aState.mIter.Next()) {
      auto& item = aState.mGridItems[aState.mIter.ItemIndex()];
      GridArea& area = item.mArea;
      LineRange& major = isRowOrder ? area.mRows : area.mCols;
      LineRange& minor = isRowOrder ? area.mCols : area.mRows;
      if (major.IsDefinite() && minor.IsAuto()) {
        // Items with 'auto' in the minor dimension only.
        uint32_t cursor = 0;
        if (isSparse) {
          cursors->Get(major.mStart, &cursor);
        }
        (this->*placeAutoMinorFunc)(cursor, &area, clampMaxLine);
        if (item.IsSubgrid()) {
          Grid grid(this);
          grid.SubgridPlaceGridItems(aState, this, item);
        }
        mCellMap.Fill(area);
        SetSubgridChildEdgeBits(item);
        if (isSparse) {
          cursors->Put(major.mStart, minor.mEnd);
        }
      }
      InflateGridFor(area);  // Step 2, inflating for auto items too
    }
  }

  // XXX NOTE possible spec issue.
  // XXX It's unclear if the remaining major-dimension auto and
  // XXX auto in both dimensions should use the same cursor or not,
  // XXX https://www.w3.org/Bugs/Public/show_bug.cgi?id=16044
  // XXX seems to indicate it shouldn't.
  // XXX http://dev.w3.org/csswg/css-grid/#auto-placement-cursor
  // XXX now says it should (but didn't in earlier versions)

  // Step 3, place the remaining grid items
  uint32_t cursorMajor = 0;  // for 'dense' these two cursors will stay at 0,0
  uint32_t cursorMinor = 0;
  auto placeAutoMajorFunc =
      isRowOrder ? &Grid::PlaceAutoRow : &Grid::PlaceAutoCol;
  uint32_t clampMaxMajorLine = isRowOrder ? clampMaxRowLine : clampMaxColLine;
  aState.mIter.Reset();
  for (; !aState.mIter.AtEnd(); aState.mIter.Next()) {
    auto& item = aState.mGridItems[aState.mIter.ItemIndex()];
    GridArea& area = item.mArea;
    MOZ_ASSERT(*aState.mIter == item.mFrame,
               "iterator out of sync with aState.mGridItems");
    LineRange& major = isRowOrder ? area.mRows : area.mCols;
    LineRange& minor = isRowOrder ? area.mCols : area.mRows;
    if (major.IsAuto()) {
      if (minor.IsDefinite()) {
        // Items with 'auto' in the major dimension only.
        if (isSparse) {
          if (minor.mStart < cursorMinor) {
            ++cursorMajor;
          }
          cursorMinor = minor.mStart;
        }
        (this->*placeAutoMajorFunc)(cursorMajor, &area, clampMaxMajorLine);
        if (isSparse) {
          cursorMajor = major.mStart;
        }
      } else {
        // Items with 'auto' in both dimensions.
        if (isRowOrder) {
          PlaceAutoAutoInRowOrder(cursorMinor, cursorMajor, &area,
                                  clampMaxColLine, clampMaxRowLine);
        } else {
          PlaceAutoAutoInColOrder(cursorMajor, cursorMinor, &area,
                                  clampMaxColLine, clampMaxRowLine);
        }
        if (isSparse) {
          cursorMajor = major.mStart;
          cursorMinor = minor.mEnd;
#ifdef DEBUG
          uint32_t gridMajorEnd = isRowOrder ? mGridRowEnd : mGridColEnd;
          uint32_t gridMinorEnd = isRowOrder ? mGridColEnd : mGridRowEnd;
          MOZ_ASSERT(cursorMajor <= gridMajorEnd,
                     "we shouldn't need to place items further than 1 track "
                     "past the current end of the grid, in major dimension");
          MOZ_ASSERT(cursorMinor <= gridMinorEnd,
                     "we shouldn't add implicit minor tracks for auto/auto");
#endif
        }
      }
      if (item.IsSubgrid()) {
        Grid grid(this);
        grid.SubgridPlaceGridItems(aState, this, item);
      }
      mCellMap.Fill(area);
      InflateGridFor(area);
      SetSubgridChildEdgeBits(item);
    }
  }

  if (aState.mFrame->IsAbsoluteContainer()) {
    // 9.4 Absolutely-positioned Grid Items
    // http://dev.w3.org/csswg/css-grid/#abspos-items
    // We only resolve definite lines here; we'll align auto positions to the
    // grid container later during reflow.
    nsFrameList children(
        aState.mFrame->GetChildList(aState.mFrame->GetAbsoluteListID()));
    const int32_t offsetToColZero = int32_t(mExplicitGridOffsetCol) - 1;
    const int32_t offsetToRowZero = int32_t(mExplicitGridOffsetRow) - 1;
    // Untranslate the grid again temporarily while resolving abs.pos. lines.
    AutoRestore<uint32_t> save1(mGridColEnd);
    AutoRestore<uint32_t> save2(mGridRowEnd);
    mGridColEnd -= offsetToColZero;
    mGridRowEnd -= offsetToRowZero;
    aState.mAbsPosItems.ClearAndRetainStorage();
    size_t i = 0;
    for (nsFrameList::Enumerator e(children); !e.AtEnd(); e.Next(), ++i) {
      nsIFrame* child = e.get();
      GridItemInfo* info = aState.mAbsPosItems.AppendElement(GridItemInfo(
          child,
          PlaceAbsPos(child, colLineNameMap, rowLineNameMap, gridStyle)));
      GridArea& area = info->mArea;
      if (area.mCols.mUntranslatedStart != int32_t(kAutoLine)) {
        area.mCols.mStart = area.mCols.mUntranslatedStart + offsetToColZero;
      }
      if (area.mCols.mUntranslatedEnd != int32_t(kAutoLine)) {
        area.mCols.mEnd = area.mCols.mUntranslatedEnd + offsetToColZero;
      }
      if (area.mRows.mUntranslatedStart != int32_t(kAutoLine)) {
        area.mRows.mStart = area.mRows.mUntranslatedStart + offsetToRowZero;
      }
      if (area.mRows.mUntranslatedEnd != int32_t(kAutoLine)) {
        area.mRows.mEnd = area.mRows.mUntranslatedEnd + offsetToRowZero;
      }
      if (info->IsSubgrid()) {
        Grid grid(this);
        grid.SubgridPlaceGridItems(aState, this, *info);
      }
    }
  }

  // Count empty 'auto-fit' tracks in the repeat() range.
  // |colAdjust| will have a count for each line in the grid of how many
  // tracks were empty between the start of the grid and that line.

  Maybe<nsTArray<uint32_t>> colAdjust;
  uint32_t numEmptyCols = 0;
  if (aState.mColFunctions.mHasRepeatAuto &&
      gridStyle->mGridTemplateColumns.GetRepeatAutoValue()->count.IsAutoFit()) {
    const auto& cellMap = mCellMap;
    colAdjust = CalculateAdjustForAutoFitElements(
        &numEmptyCols, aState.mColFunctions, mGridColEnd + 1,
        [cellMap](uint32_t i) -> bool { return cellMap.IsEmptyCol(i); });
  }

  // Do similar work for the row tracks, with the same logic.
  Maybe<nsTArray<uint32_t>> rowAdjust;
  uint32_t numEmptyRows = 0;
  if (aState.mRowFunctions.mHasRepeatAuto &&
      gridStyle->mGridTemplateRows.GetRepeatAutoValue()->count.IsAutoFit()) {
    const auto& cellMap = mCellMap;
    rowAdjust = CalculateAdjustForAutoFitElements(
        &numEmptyRows, aState.mRowFunctions, mGridRowEnd + 1,
        [cellMap](uint32_t i) -> bool { return cellMap.IsEmptyRow(i); });
  }
  MOZ_ASSERT((numEmptyCols > 0) == colAdjust.isSome());
  MOZ_ASSERT((numEmptyRows > 0) == rowAdjust.isSome());
  // Remove the empty 'auto-fit' tracks we found above, if any.
  if (numEmptyCols || numEmptyRows) {
    // Adjust the line numbers in the grid areas.
    for (auto& item : aState.mGridItems) {
      GridArea& area = item.mArea;
      if (numEmptyCols) {
        area.mCols.AdjustForRemovedTracks(*colAdjust);
      }
      if (numEmptyRows) {
        area.mRows.AdjustForRemovedTracks(*rowAdjust);
      }
    }
    for (auto& item : aState.mAbsPosItems) {
      GridArea& area = item.mArea;
      if (numEmptyCols) {
        area.mCols.AdjustAbsPosForRemovedTracks(*colAdjust);
      }
      if (numEmptyRows) {
        area.mRows.AdjustAbsPosForRemovedTracks(*rowAdjust);
      }
    }
    // Adjust the grid size.
    mGridColEnd -= numEmptyCols;
    mExplicitGridColEnd -= numEmptyCols;
    mGridRowEnd -= numEmptyRows;
    mExplicitGridRowEnd -= numEmptyRows;
    // Adjust the track mapping to unmap the removed tracks.
    auto colRepeatCount = aState.mColFunctions.NumRepeatTracks();
    aState.mColFunctions.SetNumRepeatTracks(colRepeatCount - numEmptyCols);
    auto rowRepeatCount = aState.mRowFunctions.NumRepeatTracks();
    aState.mRowFunctions.SetNumRepeatTracks(rowRepeatCount - numEmptyRows);
  }

  // Update the line boundaries of the implicit grid areas, if needed.
  if (mAreas &&
      aState.mFrame->HasAnyStateBits(NS_STATE_GRID_GENERATE_COMPUTED_VALUES)) {
    for (auto iter = mAreas->iter(); !iter.done(); iter.next()) {
      auto& areaInfo = iter.get().value();

      // Resolve the lines for the area. We use the name of the area as the
      // name of the lines, knowing that the line placement algorithm will
      // add the -start and -end suffixes as appropriate for layout.
      StyleGridLine lineStartAndEnd;
      lineStartAndEnd.ident = areaInfo.name;

      LineRange columnLines =
          ResolveLineRange(lineStartAndEnd, lineStartAndEnd, colLineNameMap,
                           eLogicalAxisInline, mExplicitGridColEnd, gridStyle);

      LineRange rowLines =
          ResolveLineRange(lineStartAndEnd, lineStartAndEnd, rowLineNameMap,
                           eLogicalAxisBlock, mExplicitGridRowEnd, gridStyle);

      // Put the resolved line indices back into the area structure.
      areaInfo.columns.start = columnLines.mStart + mExplicitGridOffsetCol;
      areaInfo.columns.end = columnLines.mEnd + mExplicitGridOffsetCol;
      areaInfo.rows.start = rowLines.mStart + mExplicitGridOffsetRow;
      areaInfo.rows.end = rowLines.mEnd + mExplicitGridOffsetRow;
    }
  }
}

void nsGridContainerFrame::Tracks::Initialize(
    const TrackSizingFunctions& aFunctions,
    const NonNegativeLengthPercentageOrNormal& aGridGap, uint32_t aNumTracks,
    nscoord aContentBoxSize) {
  mSizes.SetLength(aNumTracks);
  PodZero(mSizes.Elements(), mSizes.Length());
  for (uint32_t i = 0, len = mSizes.Length(); i < len; ++i) {
    mStateUnion |=
        mSizes[i].Initialize(aContentBoxSize, aFunctions.SizingFor(i));
  }
  mGridGap = nsLayoutUtils::ResolveGapToLength(aGridGap, aContentBoxSize);
  mContentBoxSize = aContentBoxSize;
}

/**
 * Reflow aChild in the given aAvailableSize.
 */
static nscoord MeasuringReflow(nsIFrame* aChild,
                               const ReflowInput* aReflowInput, gfxContext* aRC,
                               const LogicalSize& aAvailableSize,
                               const LogicalSize& aCBSize,
                               nscoord aIMinSizeClamp = NS_MAXSIZE,
                               nscoord aBMinSizeClamp = NS_MAXSIZE) {
  nsContainerFrame* parent = aChild->GetParent();
  nsPresContext* pc = aChild->PresContext();
  Maybe<ReflowInput> dummyParentState;
  const ReflowInput* rs = aReflowInput;
  if (!aReflowInput) {
    MOZ_ASSERT(!parent->HasAnyStateBits(NS_FRAME_IN_REFLOW));
    dummyParentState.emplace(
        pc, parent, aRC,
        LogicalSize(parent->GetWritingMode(), 0, NS_UNCONSTRAINEDSIZE),
        ReflowInput::DUMMY_PARENT_REFLOW_INPUT);
    rs = dummyParentState.ptr();
  }
#ifdef DEBUG
  // This will suppress various CRAZY_SIZE warnings for this reflow.
  parent->SetProperty(nsContainerFrame::DebugReflowingWithInfiniteISize(),
                      true);
#endif
  auto wm = aChild->GetWritingMode();
  uint32_t riFlags = ReflowInput::COMPUTE_SIZE_USE_AUTO_BSIZE;
  if (aAvailableSize.ISize(wm) == INFINITE_ISIZE_COORD) {
    riFlags |= ReflowInput::COMPUTE_SIZE_SHRINK_WRAP;
  }
  if (aIMinSizeClamp != NS_MAXSIZE) {
    riFlags |= ReflowInput::I_CLAMP_MARGIN_BOX_MIN_SIZE;
  }
  if (aBMinSizeClamp != NS_MAXSIZE) {
    riFlags |= ReflowInput::B_CLAMP_MARGIN_BOX_MIN_SIZE;
    aChild->SetProperty(nsIFrame::BClampMarginBoxMinSizeProperty(),
                        aBMinSizeClamp);
  } else {
    aChild->RemoveProperty(nsIFrame::BClampMarginBoxMinSizeProperty());
  }
  ReflowInput childRI(pc, *rs, aChild, aAvailableSize, Some(aCBSize), riFlags);

  // Because we pass ReflowInput::COMPUTE_SIZE_USE_AUTO_BSIZE, and the
  // previous reflow of the child might not have, set the child's
  // block-resize flag to true.
  // FIXME (perf): It would be faster to do this only if the previous
  // reflow of the child was not a measuring reflow, and only if the
  // child does some of the things that are affected by
  // ReflowInput::COMPUTE_SIZE_USE_AUTO_BSIZE.
  childRI.SetBResize(true);
  // Not 100% sure this is needed, but be conservative for now:
  childRI.mFlags.mIsBResizeForPercentages = true;

  ReflowOutput childSize(childRI);
  nsReflowStatus childStatus;
  const nsIFrame::ReflowChildFlags flags =
      nsIFrame::ReflowChildFlags::NoMoveFrame |
      nsIFrame::ReflowChildFlags::NoSizeView |
      nsIFrame::ReflowChildFlags::NoDeleteNextInFlowChild;
  parent->ReflowChild(aChild, pc, childSize, childRI, wm, LogicalPoint(wm),
                      nsSize(), flags, childStatus);
  nsContainerFrame::FinishReflowChild(aChild, pc, childSize, &childRI, wm,
                                      LogicalPoint(wm), nsSize(), flags);
#ifdef DEBUG
  parent->RemoveProperty(nsContainerFrame::DebugReflowingWithInfiniteISize());
#endif
  return childSize.BSize(wm);
}

/**
 * Return the accumulated margin+border+padding in aAxis for aFrame (a subgrid)
 * and its ancestor subgrids.
 */
static LogicalMargin SubgridAccumulatedMarginBorderPadding(
    nsIFrame* aFrame, const Subgrid* aSubgrid, WritingMode aResultWM,
    LogicalAxis aAxis) {
  MOZ_ASSERT(aFrame->IsGridContainerFrame());
  auto* subgridFrame = static_cast<nsGridContainerFrame*>(aFrame);
  LogicalMargin result(aSubgrid->mMarginBorderPadding);
  auto* parent = subgridFrame->ParentGridContainerForSubgrid();
  auto subgridCBWM = parent->GetWritingMode();
  auto childRange = aSubgrid->mArea.LineRangeForAxis(aAxis);
  bool skipStartSide = false;
  bool skipEndSide = false;
  auto axis = aSubgrid->mIsOrthogonal ? GetOrthogonalAxis(aAxis) : aAxis;
  // If aFrame's parent is also a subgrid, then add its MBP on the edges that
  // are adjacent (i.e. start or end in the same track), recursively.
  // ("parent" refers to the grid-frame we're currently adding MBP for,
  // and "grandParent" its parent, as we walk up the chain.)
  while (parent->IsSubgrid(axis)) {
    auto* parentSubgrid = parent->GetProperty(Subgrid::Prop());
    auto* grandParent = parent->ParentGridContainerForSubgrid();
    auto parentCBWM = grandParent->GetWritingMode();
    if (parentCBWM.IsOrthogonalTo(subgridCBWM)) {
      axis = GetOrthogonalAxis(axis);
    }
    const auto& parentRange = parentSubgrid->mArea.LineRangeForAxis(axis);
    bool sameDir = parentCBWM.ParallelAxisStartsOnSameSide(axis, subgridCBWM);
    if (sameDir) {
      skipStartSide |= childRange.mStart != 0;
      skipEndSide |= childRange.mEnd != parentRange.Extent();
    } else {
      skipEndSide |= childRange.mStart != 0;
      skipStartSide |= childRange.mEnd != parentRange.Extent();
    }
    if (skipStartSide && skipEndSide) {
      break;
    }
    auto mbp =
        parentSubgrid->mMarginBorderPadding.ConvertTo(subgridCBWM, parentCBWM);
    if (skipStartSide) {
      mbp.Start(aAxis, subgridCBWM) = nscoord(0);
    }
    if (skipEndSide) {
      mbp.End(aAxis, subgridCBWM) = nscoord(0);
    }
    result += mbp;
    parent = grandParent;
    childRange = parentRange;
  }
  return result.ConvertTo(aResultWM, subgridCBWM);
}

/**
 * Return the [min|max]-content contribution of aChild to its parent (i.e.
 * the child's margin-box) in aAxis.
 */
static nscoord ContentContribution(
    const GridItemInfo& aGridItem, const GridReflowInput& aState,
    gfxContext* aRC, WritingMode aCBWM, LogicalAxis aAxis,
    const Maybe<LogicalSize>& aPercentageBasis, IntrinsicISizeType aConstraint,
    nscoord aMinSizeClamp = NS_MAXSIZE, uint32_t aFlags = 0) {
  nsIFrame* child = aGridItem.mFrame;

  nscoord extraMargin = 0;
  nsGridContainerFrame::Subgrid* subgrid = nullptr;
  if (child->GetParent() != aState.mFrame) {
    // |child| is a subgrid descendant, so it contributes its subgrids'
    // margin+border+padding for any edge tracks that it spans.
    auto* subgridFrame = child->GetParent();
    subgrid = subgridFrame->GetProperty(Subgrid::Prop());
    const auto itemEdgeBits = aGridItem.mState[aAxis] & ItemState::eEdgeBits;
    if (itemEdgeBits) {
      LogicalMargin mbp = SubgridAccumulatedMarginBorderPadding(
          subgridFrame, subgrid, aCBWM, aAxis);
      if (itemEdgeBits & ItemState::eStartEdge) {
        extraMargin += mbp.Start(aAxis, aCBWM);
      }
      if (itemEdgeBits & ItemState::eEndEdge) {
        extraMargin += mbp.End(aAxis, aCBWM);
      }
    }
    // It also contributes (half of) the subgrid's gap on its edges (if any)
    // subtracted by the non-subgrid ancestor grid container's gap.
    // Note that this can also be negative since it's considered a margin.
    if (itemEdgeBits != ItemState::eEdgeBits) {
      auto subgridAxis = aCBWM.IsOrthogonalTo(subgridFrame->GetWritingMode())
                             ? GetOrthogonalAxis(aAxis)
                             : aAxis;
      auto& gapStyle = subgridAxis == eLogicalAxisBlock
                           ? subgridFrame->StylePosition()->mRowGap
                           : subgridFrame->StylePosition()->mColumnGap;
      if (!gapStyle.IsNormal()) {
        auto subgridExtent = subgridAxis == eLogicalAxisBlock
                                 ? subgrid->mGridRowEnd
                                 : subgrid->mGridColEnd;
        if (subgridExtent > 1) {
          nscoord subgridGap =
              nsLayoutUtils::ResolveGapToLength(gapStyle, NS_UNCONSTRAINEDSIZE);
          auto& tracks =
              aAxis == eLogicalAxisBlock ? aState.mRows : aState.mCols;
          auto gapDelta = subgridGap - tracks.mGridGap;
          if (!itemEdgeBits) {
            extraMargin += gapDelta;
          } else {
            extraMargin += gapDelta / 2;
          }
        }
      }
    }
  }

  PhysicalAxis axis(aCBWM.PhysicalAxis(aAxis));
  nscoord size = nsLayoutUtils::IntrinsicForAxis(
      axis, aRC, child, aConstraint, aPercentageBasis,
      aFlags | nsLayoutUtils::BAIL_IF_REFLOW_NEEDED, aMinSizeClamp);
  auto childWM = child->GetWritingMode();
  const bool isOrthogonal = childWM.IsOrthogonalTo(aCBWM);
  auto childAxis = isOrthogonal ? GetOrthogonalAxis(aAxis) : aAxis;
  if (size == NS_INTRINSIC_ISIZE_UNKNOWN && childAxis == eLogicalAxisBlock) {
    // We need to reflow the child to find its BSize contribution.
    // XXX this will give mostly correct results for now (until bug 1174569).
    nscoord availISize = INFINITE_ISIZE_COORD;
    nscoord availBSize = NS_UNCONSTRAINEDSIZE;
    // The next two variables are MinSizeClamp values in the child's axes.
    nscoord iMinSizeClamp = NS_MAXSIZE;
    nscoord bMinSizeClamp = NS_MAXSIZE;
    LogicalSize cbSize(childWM, 0, NS_UNCONSTRAINEDSIZE);
    // Below, we try to resolve the child's grid-area size in its inline-axis
    // to use as the CB/Available size in the MeasuringReflow that follows.
    if (child->GetParent() != aState.mFrame) {
      // This item is a child of a subgrid descendant.
      auto* subgridFrame =
          static_cast<nsGridContainerFrame*>(child->GetParent());
      MOZ_ASSERT(subgridFrame->IsGridContainerFrame());
      auto* uts = subgridFrame->GetProperty(UsedTrackSizes::Prop());
      if (!uts) {
        uts = new UsedTrackSizes();
        subgridFrame->SetProperty(UsedTrackSizes::Prop(), uts);
      }
      // The grid-item's inline-axis as expressed in the subgrid's WM.
      auto subgridAxis = childWM.IsOrthogonalTo(subgridFrame->GetWritingMode())
                             ? eLogicalAxisBlock
                             : eLogicalAxisInline;
      uts->ResolveTrackSizesForAxis(subgridFrame, subgridAxis, *aRC);
      if (uts->mCanResolveLineRangeSize[subgridAxis]) {
        auto* subgrid =
            subgridFrame->GetProperty(nsGridContainerFrame::Subgrid::Prop());
        const GridItemInfo* originalItem = nullptr;
        for (const auto& item : subgrid->mGridItems) {
          if (item.mFrame == child) {
            originalItem = &item;
            break;
          }
        }
        MOZ_ASSERT(originalItem, "huh?");
        const auto& range = originalItem->mArea.LineRangeForAxis(subgridAxis);
        nscoord pos, sz;
        range.ToPositionAndLength(uts->mSizes[subgridAxis], &pos, &sz);
        if (childWM.IsOrthogonalTo(subgridFrame->GetWritingMode())) {
          availBSize = sz;
          cbSize.BSize(childWM) = sz;
          if (aGridItem.mState[aAxis] & ItemState::eClampMarginBoxMinSize) {
            bMinSizeClamp = sz;
          }
        } else {
          availISize = sz;
          cbSize.ISize(childWM) = sz;
          if (aGridItem.mState[aAxis] & ItemState::eClampMarginBoxMinSize) {
            iMinSizeClamp = sz;
          }
        }
      }
    } else if (aState.mCols.mCanResolveLineRangeSize) {
      nscoord sz = aState.mCols.ResolveSize(aGridItem.mArea.mCols);
      if (isOrthogonal) {
        availBSize = sz;
        cbSize.BSize(childWM) = sz;
        if (aGridItem.mState[aAxis] & ItemState::eClampMarginBoxMinSize) {
          bMinSizeClamp = sz;
        }
      } else {
        availISize = sz;
        cbSize.ISize(childWM) = sz;
        if (aGridItem.mState[aAxis] & ItemState::eClampMarginBoxMinSize) {
          iMinSizeClamp = sz;
        }
      }
    }
    if (isOrthogonal == (aAxis == eLogicalAxisInline)) {
      bMinSizeClamp = aMinSizeClamp;
    } else {
      iMinSizeClamp = aMinSizeClamp;
    }
    LogicalSize availableSize(childWM, availISize, availBSize);
    if (MOZ_UNLIKELY(child->IsXULBoxFrame())) {
      auto* pc = child->PresContext();
      // For XUL-in-CSS-Grid (e.g. in our frontend code), we defer to XUL's
      // GetPrefSize() function (which reports an answer in both axes), instead
      // of actually reflowing.  It's important to avoid the "measuring + final"
      // two-pass reflow for XUL, because some XUL layout code may incorrectly
      // optimize away the second reflow in cases where it's really needed.
      // XXXdholbert We'll remove this special case in bug 1600542.
      ReflowInput childRI(pc, *aState.mReflowInput, child, availableSize,
                          Some(cbSize));

      nsBoxLayoutState state(pc, &aState.mRenderingContext, &childRI,
                             childRI.mReflowDepth);
      nsSize physicalPrefSize = child->GetXULPrefSize(state);
      auto prefSize = LogicalSize(childWM, physicalPrefSize);
      size = prefSize.BSize(childWM);

      // XXXdholbert This won't have percentage margins resolved.
      // Hopefully we can just avoid those for XUL-content-in-css-grid?
      size += childRI.ComputedLogicalMargin().BStartEnd(childWM);
    } else {
      size = ::MeasuringReflow(child, aState.mReflowInput, aRC, availableSize,
                               cbSize, iMinSizeClamp, bMinSizeClamp);
      size += child->GetLogicalUsedMargin(childWM).BStartEnd(childWM);
    }
    nscoord overflow = size - aMinSizeClamp;
    if (MOZ_UNLIKELY(overflow > 0)) {
      nscoord contentSize = child->ContentSize(childWM).BSize(childWM);
      nscoord newContentSize = std::max(nscoord(0), contentSize - overflow);
      // XXXmats deal with percentages better, see bug 1300369 comment 27.
      size -= contentSize - newContentSize;
    }
  }
  MOZ_ASSERT(aGridItem.mBaselineOffset[aAxis] >= 0,
             "baseline offset should be non-negative at this point");
  MOZ_ASSERT((aGridItem.mState[aAxis] & ItemState::eIsBaselineAligned) ||
                 aGridItem.mBaselineOffset[aAxis] == nscoord(0),
             "baseline offset should be zero when not baseline-aligned");
  size += aGridItem.mBaselineOffset[aAxis];
  size += extraMargin;
  return std::max(size, 0);
}

struct CachedIntrinsicSizes {
  Maybe<nscoord> mMinSize;
  Maybe<nscoord> mMinContentContribution;
  Maybe<nscoord> mMaxContentContribution;

  // The item's percentage basis for intrinsic sizing purposes.
  Maybe<LogicalSize> mPercentageBasis;

  // "if the grid item spans only grid tracks that have a fixed max track
  // sizing function, its automatic minimum size in that dimension is
  // further clamped to less than or equal to the size necessary to fit its
  // margin box within the resulting grid area (flooring at zero)"
  // https://drafts.csswg.org/css-grid/#min-size-auto
  // This is the clamp value to use for that:
  nscoord mMinSizeClamp = NS_MAXSIZE;
};

static nscoord MinContentContribution(const GridItemInfo& aGridItem,
                                      const GridReflowInput& aState,
                                      gfxContext* aRC, WritingMode aCBWM,
                                      LogicalAxis aAxis,
                                      CachedIntrinsicSizes* aCache) {
  if (aCache->mMinContentContribution.isSome()) {
    return aCache->mMinContentContribution.value();
  }
  if (aCache->mPercentageBasis.isNothing()) {
    aCache->mPercentageBasis.emplace(
        aState.PercentageBasisFor(aAxis, aGridItem));
  }
  nscoord s = ContentContribution(
      aGridItem, aState, aRC, aCBWM, aAxis, aCache->mPercentageBasis,
      nsLayoutUtils::MIN_ISIZE, aCache->mMinSizeClamp);
  aCache->mMinContentContribution.emplace(s);
  return s;
}

static nscoord MaxContentContribution(const GridItemInfo& aGridItem,
                                      const GridReflowInput& aState,
                                      gfxContext* aRC, WritingMode aCBWM,
                                      LogicalAxis aAxis,
                                      CachedIntrinsicSizes* aCache) {
  if (aCache->mMaxContentContribution.isSome()) {
    return aCache->mMaxContentContribution.value();
  }
  if (aCache->mPercentageBasis.isNothing()) {
    aCache->mPercentageBasis.emplace(
        aState.PercentageBasisFor(aAxis, aGridItem));
  }
  nscoord s = ContentContribution(
      aGridItem, aState, aRC, aCBWM, aAxis, aCache->mPercentageBasis,
      nsLayoutUtils::PREF_ISIZE, aCache->mMinSizeClamp);
  aCache->mMaxContentContribution.emplace(s);
  return s;
}

// Computes the min-size contribution for a grid item, as defined at
// https://drafts.csswg.org/css-grid/#min-size-contribution
static nscoord MinSize(const GridItemInfo& aGridItem,
                       const GridReflowInput& aState, gfxContext* aRC,
                       WritingMode aCBWM, LogicalAxis aAxis,
                       CachedIntrinsicSizes* aCache) {
  if (aCache->mMinSize.isSome()) {
    return aCache->mMinSize.value();
  }
  nsIFrame* child = aGridItem.mFrame;
  PhysicalAxis axis(aCBWM.PhysicalAxis(aAxis));
  const nsStylePosition* stylePos = child->StylePosition();
  StyleSize sizeStyle =
      axis == eAxisHorizontal ? stylePos->mWidth : stylePos->mHeight;

  auto ourInlineAxis = child->GetWritingMode().PhysicalAxis(eLogicalAxisInline);
  // max-content and min-content should behave as initial value in block axis.
  // FIXME: Bug 567039: moz-fit-content and -moz-available are not supported
  // for block size dimension on sizing properties (e.g. height), so we
  // treat it as `auto`.
  if (axis != ourInlineAxis && sizeStyle.IsExtremumLength()) {
    sizeStyle = StyleSize::Auto();
  }

  if (!sizeStyle.IsAuto() && !sizeStyle.HasPercent()) {
    nscoord s =
        MinContentContribution(aGridItem, aState, aRC, aCBWM, aAxis, aCache);
    aCache->mMinSize.emplace(s);
    return s;
  }

  if (aCache->mPercentageBasis.isNothing()) {
    aCache->mPercentageBasis.emplace(
        aState.PercentageBasisFor(aAxis, aGridItem));
  }

  // https://drafts.csswg.org/css-grid/#min-size-auto
  // This calculates the min-content contribution from either a definite
  // min-width (or min-height depending on aAxis), or the "specified /
  // transferred size" for min-width:auto if overflow == visible (as min-width:0
  // otherwise), or NS_UNCONSTRAINEDSIZE for other min-width intrinsic values
  // (which results in always taking the "content size" part below).
  MOZ_ASSERT(aGridItem.mBaselineOffset[aAxis] >= 0,
             "baseline offset should be non-negative at this point");
  MOZ_ASSERT((aGridItem.mState[aAxis] & ItemState::eIsBaselineAligned) ||
                 aGridItem.mBaselineOffset[aAxis] == nscoord(0),
             "baseline offset should be zero when not baseline-aligned");
  nscoord sz = aGridItem.mBaselineOffset[aAxis] +
               nsLayoutUtils::MinSizeContributionForAxis(
                   axis, aRC, child, nsLayoutUtils::MIN_ISIZE,
                   *aCache->mPercentageBasis);
  const StyleSize& style =
      axis == eAxisHorizontal ? stylePos->mMinWidth : stylePos->mMinHeight;
  // max-content and min-content should behave as initial value in block axis.
  // FIXME: Bug 567039: moz-fit-content and -moz-available are not supported
  // for block size dimension on sizing properties (e.g. height), so we
  // treat it as `auto`.
  const bool inInlineAxis = axis == ourInlineAxis;
  const bool isAuto =
      style.IsAuto() || (!inInlineAxis && style.IsExtremumLength());
  if ((inInlineAxis && style.IsExtremumLength()) ||
      (isAuto && child->StyleDisplay()->mOverflowX == StyleOverflow::Visible)) {
    // Now calculate the "content size" part and return whichever is smaller.
    MOZ_ASSERT(isAuto || sz == NS_UNCONSTRAINEDSIZE);
    sz = std::min(
        sz, ContentContribution(aGridItem, aState, aRC, aCBWM, aAxis,
                                aCache->mPercentageBasis,
                                nsLayoutUtils::MIN_ISIZE, aCache->mMinSizeClamp,
                                nsLayoutUtils::MIN_INTRINSIC_ISIZE));
  }
  aCache->mMinSize.emplace(sz);
  return sz;
}

void nsGridContainerFrame::Tracks::CalculateSizes(
    GridReflowInput& aState, nsTArray<GridItemInfo>& aGridItems,
    const TrackSizingFunctions& aFunctions, nscoord aContentBoxSize,
    LineRange GridArea::*aRange, SizingConstraint aConstraint) {
  nscoord percentageBasis = aContentBoxSize;
  if (percentageBasis == NS_UNCONSTRAINEDSIZE) {
    percentageBasis = 0;
  }
  InitializeItemBaselines(aState, aGridItems);
  ResolveIntrinsicSize(aState, aGridItems, aFunctions, aRange, percentageBasis,
                       aConstraint);
  if (aConstraint != SizingConstraint::MinContent) {
    nscoord freeSpace = aContentBoxSize;
    if (freeSpace != NS_UNCONSTRAINEDSIZE) {
      freeSpace -= SumOfGridGaps();
    }
    DistributeFreeSpace(freeSpace);
    StretchFlexibleTracks(aState, aGridItems, aFunctions, freeSpace);
  }
}

TrackSize::StateBits nsGridContainerFrame::Tracks::StateBitsForRange(
    const LineRange& aRange) const {
  MOZ_ASSERT(!aRange.IsAuto(), "must have a definite range");
  TrackSize::StateBits state = TrackSize::StateBits(0);
  for (auto i : aRange.Range()) {
    state |= mSizes[i].mState;
  }
  return state;
}

static void AddSubgridContribution(TrackSize& aSize,
                                   nscoord aMarginBorderPadding) {
  if (aSize.mState & TrackSize::eIntrinsicMinSizing) {
    aSize.mBase = std::max(aSize.mBase, aMarginBorderPadding);
    aSize.mLimit = std::max(aSize.mLimit, aSize.mBase);
  }
  // XXX maybe eFlexMaxSizing too?
  // (once we implement https://github.com/w3c/csswg-drafts/issues/2177)
  if (aSize.mState &
      (TrackSize::eIntrinsicMaxSizing | TrackSize::eFitContent)) {
    aSize.mLimit = std::max(aSize.mLimit, aMarginBorderPadding);
  }
}

bool nsGridContainerFrame::Tracks::ResolveIntrinsicSizeStep1(
    GridReflowInput& aState, const TrackSizingFunctions& aFunctions,
    nscoord aPercentageBasis, SizingConstraint aConstraint,
    const LineRange& aRange, const GridItemInfo& aGridItem) {
  CachedIntrinsicSizes cache;
  TrackSize& sz = mSizes[aRange.mStart];
  WritingMode wm = aState.mWM;

  // min sizing
  gfxContext* rc = &aState.mRenderingContext;
  if (sz.mState & TrackSize::eAutoMinSizing) {
    nscoord s;
    // Check if we need to apply "Automatic Minimum Size" and cache it.
    if (aGridItem.ShouldApplyAutoMinSize(wm, mAxis, aPercentageBasis)) {
      aGridItem.mState[mAxis] |= ItemState::eApplyAutoMinSize;
      // Clamp it if it's spanning a definite track max-sizing function.
      if (TrackSize::IsDefiniteMaxSizing(sz.mState)) {
        cache.mMinSizeClamp = aFunctions.MaxSizingFor(aRange.mStart)
                                  .AsBreadth()
                                  .Resolve(aPercentageBasis);
        aGridItem.mState[mAxis] |= ItemState::eClampMarginBoxMinSize;
      }
      if (aConstraint != SizingConstraint::MaxContent) {
        s = MinContentContribution(aGridItem, aState, rc, wm, mAxis, &cache);
      } else {
        s = MaxContentContribution(aGridItem, aState, rc, wm, mAxis, &cache);
      }
    } else {
      s = MinSize(aGridItem, aState, rc, wm, mAxis, &cache);
    }
    sz.mBase = std::max(sz.mBase, s);
  } else if (sz.mState & TrackSize::eMinContentMinSizing) {
    auto s = MinContentContribution(aGridItem, aState, rc, wm, mAxis, &cache);
    sz.mBase = std::max(sz.mBase, s);
  } else if (sz.mState & TrackSize::eMaxContentMinSizing) {
    auto s = MaxContentContribution(aGridItem, aState, rc, wm, mAxis, &cache);
    sz.mBase = std::max(sz.mBase, s);
  }
  // max sizing
  if (sz.mState & TrackSize::eMinContentMaxSizing) {
    auto s = MinContentContribution(aGridItem, aState, rc, wm, mAxis, &cache);
    if (sz.mLimit == NS_UNCONSTRAINEDSIZE) {
      sz.mLimit = s;
    } else {
      sz.mLimit = std::max(sz.mLimit, s);
    }
  } else if (sz.mState &
             (TrackSize::eAutoMaxSizing | TrackSize::eMaxContentMaxSizing)) {
    auto s = MaxContentContribution(aGridItem, aState, rc, wm, mAxis, &cache);
    if (sz.mLimit == NS_UNCONSTRAINEDSIZE) {
      sz.mLimit = s;
    } else {
      sz.mLimit = std::max(sz.mLimit, s);
    }
    if (MOZ_UNLIKELY(sz.mState & TrackSize::eFitContent)) {
      // Clamp mLimit to the fit-content() size, for §12.5.1.
      nscoord fitContentClamp = aFunctions.SizingFor(aRange.mStart)
                                    .AsFitContent()
                                    .AsBreadth()
                                    .Resolve(aPercentageBasis);
      sz.mLimit = std::min(sz.mLimit, fitContentClamp);
    }
  }
  if (sz.mLimit < sz.mBase) {
    sz.mLimit = sz.mBase;
  }
  return sz.mState & TrackSize::eFlexMaxSizing;
}

void nsGridContainerFrame::Tracks::CalculateItemBaselines(
    nsTArray<ItemBaselineData>& aBaselineItems,
    BaselineSharingGroup aBaselineGroup) {
  if (aBaselineItems.IsEmpty()) {
    return;
  }

  // Sort the collected items on their baseline track.
  std::sort(aBaselineItems.begin(), aBaselineItems.end(),
            ItemBaselineData::IsBaselineTrackLessThan);

  MOZ_ASSERT(mSizes.Length() > 0, "having an item implies at least one track");
  const uint32_t lastTrack = mSizes.Length() - 1;
  nscoord maxBaseline = 0;
  nscoord maxDescent = 0;
  uint32_t currentTrack = kAutoLine;  // guaranteed to not match any item
  uint32_t trackStartIndex = 0;
  for (uint32_t i = 0, len = aBaselineItems.Length(); true; ++i) {
    // Find the maximum baseline and descent in the current track.
    if (i != len) {
      const ItemBaselineData& item = aBaselineItems[i];
      if (currentTrack == item.mBaselineTrack) {
        maxBaseline = std::max(maxBaseline, item.mBaseline);
        maxDescent = std::max(maxDescent, item.mSize - item.mBaseline);
        continue;
      }
    }
    // Iterate the current track again and update the baseline offsets making
    // all items baseline-aligned within this group in this track.
    for (uint32_t j = trackStartIndex; j < i; ++j) {
      const ItemBaselineData& item = aBaselineItems[j];
      item.mGridItem->mBaselineOffset[mAxis] = maxBaseline - item.mBaseline;
      MOZ_ASSERT(item.mGridItem->mBaselineOffset[mAxis] >= 0);
    }
    if (i != 0) {
      // Store the size of this baseline-aligned subtree.
      mSizes[currentTrack].mBaselineSubtreeSize[aBaselineGroup] =
          maxBaseline + maxDescent;
      // Record the first(last) baseline for the first(last) track.
      if (currentTrack == 0 && aBaselineGroup == BaselineSharingGroup::First) {
        mBaseline[aBaselineGroup] = maxBaseline;
      }
      if (currentTrack == lastTrack &&
          aBaselineGroup == BaselineSharingGroup::Last) {
        mBaseline[aBaselineGroup] = maxBaseline;
      }
    }
    if (i == len) {
      break;
    }
    // Initialize data for the next track with baseline-aligned items.
    const ItemBaselineData& item = aBaselineItems[i];
    currentTrack = item.mBaselineTrack;
    trackStartIndex = i;
    maxBaseline = item.mBaseline;
    maxDescent = item.mSize - item.mBaseline;
  }
}

void nsGridContainerFrame::Tracks::InitializeItemBaselines(
    GridReflowInput& aState, nsTArray<GridItemInfo>& aGridItems) {
  if (aState.mFrame->IsSubgrid(mAxis)) {
    // A grid container's subgridded axis doesn't have a baseline.
    return;
  }
  nsTArray<ItemBaselineData> firstBaselineItems;
  nsTArray<ItemBaselineData> lastBaselineItems;
  WritingMode wm = aState.mWM;
  ComputedStyle* containerSC = aState.mFrame->Style();
  for (GridItemInfo& gridItem : aGridItems) {
    if (gridItem.IsSubgrid(mAxis)) {
      // A subgrid itself is never baseline-aligned.
      continue;
    }
    nsIFrame* child = gridItem.mFrame;
    uint32_t baselineTrack = kAutoLine;
    auto state = ItemState(0);
    auto childWM = child->GetWritingMode();
    const bool isOrthogonal = wm.IsOrthogonalTo(childWM);
    const bool isInlineAxis = mAxis == eLogicalAxisInline;  // i.e. columns
    // XXX update the line below to include orthogonal grid/table boxes
    // XXX since they have baselines in both dimensions. And flexbox with
    // XXX reversed main/cross axis?
    const bool itemHasBaselineParallelToTrack = isInlineAxis == isOrthogonal;
    if (itemHasBaselineParallelToTrack) {
      // [align|justify]-self:[last ]baseline.
      auto selfAlignment =
          isOrthogonal ? child->StylePosition()->UsedJustifySelf(containerSC)._0
                       : child->StylePosition()->UsedAlignSelf(containerSC)._0;
      selfAlignment &= ~StyleAlignFlags::FLAG_BITS;
      if (selfAlignment == StyleAlignFlags::BASELINE) {
        state |= ItemState::eFirstBaseline | ItemState::eSelfBaseline;
        const GridArea& area = gridItem.mArea;
        baselineTrack = isInlineAxis ? area.mCols.mStart : area.mRows.mStart;
      } else if (selfAlignment == StyleAlignFlags::LAST_BASELINE) {
        state |= ItemState::eLastBaseline | ItemState::eSelfBaseline;
        const GridArea& area = gridItem.mArea;
        baselineTrack = (isInlineAxis ? area.mCols.mEnd : area.mRows.mEnd) - 1;
      }

      // [align|justify]-content:[last ]baseline.
      // https://drafts.csswg.org/css-align-3/#baseline-align-content
      // "[...] and its computed 'align-self' or 'justify-self' (whichever
      // affects its block axis) is 'stretch' or 'self-start' ('self-end').
      // For this purpose, the 'start', 'end', 'flex-start', and 'flex-end'
      // values of 'align-self' are treated as either 'self-start' or
      // 'self-end', whichever they end up equivalent to.
      auto alignContent = child->StylePosition()->mAlignContent.primary;
      alignContent &= ~StyleAlignFlags::FLAG_BITS;
      if (alignContent == StyleAlignFlags::BASELINE ||
          alignContent == StyleAlignFlags::LAST_BASELINE) {
        const auto selfAlignEdge = alignContent == StyleAlignFlags::BASELINE
                                       ? StyleAlignFlags::SELF_START
                                       : StyleAlignFlags::SELF_END;
        bool validCombo = selfAlignment == StyleAlignFlags::NORMAL ||
                          selfAlignment == StyleAlignFlags::STRETCH ||
                          selfAlignment == selfAlignEdge;
        if (!validCombo) {
          // We're doing alignment in the axis that's orthogonal to mAxis here.
          LogicalAxis alignAxis = GetOrthogonalAxis(mAxis);
          // |sameSide| is true if the container's start side in this axis is
          // the same as the child's start side, in the child's parallel axis.
          bool sameSide = wm.ParallelAxisStartsOnSameSide(alignAxis, childWM);
          if (selfAlignment == StyleAlignFlags::LEFT) {
            selfAlignment = !isInlineAxis || wm.IsBidiLTR()
                                ? StyleAlignFlags::START
                                : StyleAlignFlags::END;
          } else if (selfAlignment == StyleAlignFlags::RIGHT) {
            selfAlignment = isInlineAxis && wm.IsBidiLTR()
                                ? StyleAlignFlags::END
                                : StyleAlignFlags::START;
          }

          if (selfAlignment == StyleAlignFlags::START ||
              selfAlignment == StyleAlignFlags::FLEX_START) {
            validCombo =
                sameSide == (alignContent == StyleAlignFlags::BASELINE);
          } else if (selfAlignment == StyleAlignFlags::END ||
                     selfAlignment == StyleAlignFlags::FLEX_END) {
            validCombo =
                sameSide == (alignContent == StyleAlignFlags::LAST_BASELINE);
          }
        }
        if (validCombo) {
          const GridArea& area = gridItem.mArea;
          if (alignContent == StyleAlignFlags::BASELINE) {
            state |= ItemState::eFirstBaseline | ItemState::eContentBaseline;
            baselineTrack =
                isInlineAxis ? area.mCols.mStart : area.mRows.mStart;
          } else if (alignContent == StyleAlignFlags::LAST_BASELINE) {
            state |= ItemState::eLastBaseline | ItemState::eContentBaseline;
            baselineTrack =
                (isInlineAxis ? area.mCols.mEnd : area.mRows.mEnd) - 1;
          }
        }
      }
    }

    if (state & ItemState::eIsBaselineAligned) {
      // XXXmats if |child| is a descendant of a subgrid then the metrics
      // below needs to account for the accumulated MPB somehow...

      // XXX available size issue
      LogicalSize avail(childWM, INFINITE_ISIZE_COORD, NS_UNCONSTRAINEDSIZE);
      auto* rc = &aState.mRenderingContext;
      // XXX figure out if we can avoid/merge this reflow with the main reflow.
      // XXX (after bug 1174569 is sorted out)
      //
      // XXX How should we handle percentage padding here? (bug 1330866)
      // XXX (see ::ContentContribution and how it deals with percentages)
      // XXX What if the true baseline after line-breaking differs from this
      // XXX hypothetical baseline based on an infinite inline size?
      // XXX Maybe we should just call ::ContentContribution here instead?
      // XXX For now we just pass a zero-sized CB:
      LogicalSize cbSize(childWM, 0, 0);
      ::MeasuringReflow(child, aState.mReflowInput, rc, avail, cbSize);
      nscoord baseline;
      nsGridContainerFrame* grid = do_QueryFrame(child);
      if (state & ItemState::eFirstBaseline) {
        if (grid) {
          if (isOrthogonal == isInlineAxis) {
            grid->GetBBaseline(BaselineSharingGroup::First, &baseline);
          } else {
            grid->GetIBaseline(BaselineSharingGroup::First, &baseline);
          }
        }
        if (grid || nsLayoutUtils::GetFirstLineBaseline(wm, child, &baseline)) {
          NS_ASSERTION(baseline != NS_INTRINSIC_ISIZE_UNKNOWN,
                       "about to use an unknown baseline");
          auto frameSize = isInlineAxis ? child->ISize(wm) : child->BSize(wm);
          auto m = child->GetLogicalUsedMargin(wm);
          baseline += isInlineAxis ? m.IStart(wm) : m.BStart(wm);
          auto alignSize =
              frameSize + (isInlineAxis ? m.IStartEnd(wm) : m.BStartEnd(wm));
          firstBaselineItems.AppendElement(ItemBaselineData(
              {baselineTrack, baseline, alignSize, &gridItem}));
        } else {
          state &= ~ItemState::eAllBaselineBits;
        }
      } else {
        if (grid) {
          if (isOrthogonal == isInlineAxis) {
            grid->GetBBaseline(BaselineSharingGroup::Last, &baseline);
          } else {
            grid->GetIBaseline(BaselineSharingGroup::Last, &baseline);
          }
        }
        if (grid || nsLayoutUtils::GetLastLineBaseline(wm, child, &baseline)) {
          NS_ASSERTION(baseline != NS_INTRINSIC_ISIZE_UNKNOWN,
                       "about to use an unknown baseline");
          auto frameSize = isInlineAxis ? child->ISize(wm) : child->BSize(wm);
          auto m = child->GetLogicalUsedMargin(wm);
          if (!grid) {
            // Convert to distance from border-box end.
            baseline = frameSize - baseline;
          }
          auto descent = baseline + (isInlineAxis ? m.IEnd(wm) : m.BEnd(wm));
          auto alignSize =
              frameSize + (isInlineAxis ? m.IStartEnd(wm) : m.BStartEnd(wm));
          lastBaselineItems.AppendElement(
              ItemBaselineData({baselineTrack, descent, alignSize, &gridItem}));
        } else {
          state &= ~ItemState::eAllBaselineBits;
        }
      }
    }
    MOZ_ASSERT(
        (state & (ItemState::eFirstBaseline | ItemState::eLastBaseline)) !=
            (ItemState::eFirstBaseline | ItemState::eLastBaseline),
        "first/last baseline bits are mutually exclusive");
    MOZ_ASSERT(
        (state & (ItemState::eSelfBaseline | ItemState::eContentBaseline)) !=
            (ItemState::eSelfBaseline | ItemState::eContentBaseline),
        "*-self and *-content baseline bits are mutually exclusive");
    MOZ_ASSERT(
        !(state & (ItemState::eFirstBaseline | ItemState::eLastBaseline)) ==
            !(state & (ItemState::eSelfBaseline | ItemState::eContentBaseline)),
        "first/last bit requires self/content bit and vice versa");
    gridItem.mState[mAxis] |= state;
    gridItem.mBaselineOffset[mAxis] = nscoord(0);
  }

  if (firstBaselineItems.IsEmpty() && lastBaselineItems.IsEmpty()) {
    return;
  }

  // TODO: CSS Align spec issue - how to align a baseline subtree in a track?
  // https://lists.w3.org/Archives/Public/www-style/2016May/0141.html
  mBaselineSubtreeAlign[BaselineSharingGroup::First] = StyleAlignFlags::START;
  mBaselineSubtreeAlign[BaselineSharingGroup::Last] = StyleAlignFlags::END;

  CalculateItemBaselines(firstBaselineItems, BaselineSharingGroup::First);
  CalculateItemBaselines(lastBaselineItems, BaselineSharingGroup::Last);
}

void nsGridContainerFrame::Tracks::AlignBaselineSubtree(
    const GridItemInfo& aGridItem) const {
  auto state = aGridItem.mState[mAxis];
  if (!(state & ItemState::eIsBaselineAligned)) {
    return;
  }
  const GridArea& area = aGridItem.mArea;
  int32_t baselineTrack;
  const bool isFirstBaseline = state & ItemState::eFirstBaseline;
  if (isFirstBaseline) {
    baselineTrack =
        mAxis == eLogicalAxisBlock ? area.mRows.mStart : area.mCols.mStart;
  } else {
    baselineTrack =
        (mAxis == eLogicalAxisBlock ? area.mRows.mEnd : area.mCols.mEnd) - 1;
  }
  const TrackSize& sz = mSizes[baselineTrack];
  auto baselineGroup = isFirstBaseline ? BaselineSharingGroup::First
                                       : BaselineSharingGroup::Last;
  nscoord delta = sz.mBase - sz.mBaselineSubtreeSize[baselineGroup];
  const auto subtreeAlign = mBaselineSubtreeAlign[baselineGroup];
  if (subtreeAlign == StyleAlignFlags::START) {
    if (state & ItemState::eLastBaseline) {
      aGridItem.mBaselineOffset[mAxis] += delta;
    }
  } else if (subtreeAlign == StyleAlignFlags::END) {
    if (isFirstBaseline) {
      aGridItem.mBaselineOffset[mAxis] += delta;
    }
  } else if (subtreeAlign == StyleAlignFlags::CENTER) {
    aGridItem.mBaselineOffset[mAxis] += delta / 2;
  } else {
    MOZ_ASSERT_UNREACHABLE("unexpected baseline subtree alignment");
  }
}

template <nsGridContainerFrame::Tracks::TrackSizingPhase phase>
bool nsGridContainerFrame::Tracks::GrowSizeForSpanningItems(
    nsTArray<Step2ItemData>::iterator aIter,
    const nsTArray<Step2ItemData>::iterator aIterEnd,
    nsTArray<uint32_t>& aTracks, nsTArray<TrackSize>& aPlan,
    nsTArray<TrackSize>& aItemPlan, TrackSize::StateBits aSelector,
    const FitContentClamper& aFitContentClamper,
    bool aNeedInfinitelyGrowableFlag) {
  constexpr bool isMaxSizingPhase =
      phase == TrackSizingPhase::IntrinsicMaximums ||
      phase == TrackSizingPhase::MaxContentMaximums;
  bool needToUpdateSizes = false;
  InitializePlan<phase>(aPlan);
  for (; aIter != aIterEnd; ++aIter) {
    const Step2ItemData& item = *aIter;
    if (!(item.mState & aSelector)) {
      continue;
    }
    if (isMaxSizingPhase) {
      for (auto i : item.mLineRange.Range()) {
        aPlan[i].mState |= TrackSize::eModified;
      }
    }
    nscoord space = item.SizeContributionForPhase<phase>();
    if (space <= 0) {
      continue;
    }
    aTracks.ClearAndRetainStorage();
    space = CollectGrowable<phase>(space, item.mLineRange, aSelector, aTracks);
    if (space > 0) {
      DistributeToTrackSizes<phase>(space, aPlan, aItemPlan, aTracks, aSelector,
                                    aFitContentClamper);
      needToUpdateSizes = true;
    }
  }
  if (isMaxSizingPhase) {
    needToUpdateSizes = true;
  }
  if (needToUpdateSizes) {
    CopyPlanToSize<phase>(aPlan, aNeedInfinitelyGrowableFlag);
  }
  return needToUpdateSizes;
}

void nsGridContainerFrame::Tracks::ResolveIntrinsicSize(
    GridReflowInput& aState, nsTArray<GridItemInfo>& aGridItems,
    const TrackSizingFunctions& aFunctions, LineRange GridArea::*aRange,
    nscoord aPercentageBasis, SizingConstraint aConstraint) {
  // Resolve Intrinsic Track Sizes
  // http://dev.w3.org/csswg/css-grid/#algo-content
  // We're also setting eIsFlexing on the item state here to speed up
  // FindUsedFlexFraction later.
  struct PerSpanData {
    PerSpanData()
        : mItemCountWithSameSpan(0), mStateBits(TrackSize::StateBits(0)) {}
    uint32_t mItemCountWithSameSpan;
    TrackSize::StateBits mStateBits;
  };
  AutoTArray<PerSpanData, 16> perSpanData;
  nsTArray<Step2ItemData> step2Items;
  gfxContext* rc = &aState.mRenderingContext;
  WritingMode wm = aState.mWM;
  uint32_t maxSpan = 0;  // max span of the step2Items items
  // Setup track selector for step 2.2:
  const auto contentBasedMinSelector =
      aConstraint == SizingConstraint::MinContent
          ? TrackSize::eIntrinsicMinSizing
          : TrackSize::eMinOrMaxContentMinSizing;
  // Setup track selector for step 2.3:
  const auto maxContentMinSelector =
      aConstraint == SizingConstraint::MaxContent
          ? (TrackSize::eMaxContentMinSizing | TrackSize::eAutoMinSizing)
          : TrackSize::eMaxContentMinSizing;
  for (auto& gridItem : aGridItems) {
    MOZ_ASSERT(!(gridItem.mState[mAxis] &
                 (ItemState::eApplyAutoMinSize | ItemState::eIsFlexing |
                  ItemState::eClampMarginBoxMinSize)),
               "Why are any of these bits set already?");
    const GridArea& area = gridItem.mArea;
    const LineRange& lineRange = area.*aRange;
    uint32_t span = lineRange.Extent();
    if (MOZ_UNLIKELY(gridItem.mState[mAxis] & ItemState::eIsSubgrid)) {
      auto itemWM = gridItem.mFrame->GetWritingMode();
      auto percentageBasis = aState.PercentageBasisFor(mAxis, gridItem);
      if (percentageBasis.ISize(itemWM) == NS_UNCONSTRAINEDSIZE) {
        percentageBasis.ISize(itemWM) = nscoord(0);
      }
      if (percentageBasis.BSize(itemWM) == NS_UNCONSTRAINEDSIZE) {
        percentageBasis.BSize(itemWM) = nscoord(0);
      }
      auto* subgrid =
          SubgridComputeMarginBorderPadding(gridItem, percentageBasis);
      LogicalMargin mbp = SubgridAccumulatedMarginBorderPadding(
          gridItem.SubgridFrame(), subgrid, wm, mAxis);
      if (span == 1) {
        AddSubgridContribution(mSizes[lineRange.mStart],
                               mbp.StartEnd(mAxis, wm));
      } else {
        AddSubgridContribution(mSizes[lineRange.mStart], mbp.Start(mAxis, wm));
        AddSubgridContribution(mSizes[lineRange.mEnd - 1], mbp.End(mAxis, wm));
      }
      continue;
    }

    if (span == 1) {
      // Step 1. Size tracks to fit non-spanning items.
      if (ResolveIntrinsicSizeStep1(aState, aFunctions, aPercentageBasis,
                                    aConstraint, lineRange, gridItem)) {
        gridItem.mState[mAxis] |= ItemState::eIsFlexing;
      }
    } else {
      TrackSize::StateBits state = StateBitsForRange(lineRange);

      // Check if we need to apply "Automatic Minimum Size" and cache it.
      if ((state & TrackSize::eAutoMinSizing) &&
          gridItem.ShouldApplyAutoMinSize(wm, mAxis, aPercentageBasis)) {
        gridItem.mState[mAxis] |= ItemState::eApplyAutoMinSize;
      }

      if (state & TrackSize::eFlexMaxSizing) {
        gridItem.mState[mAxis] |= ItemState::eIsFlexing;
      } else if (state & (TrackSize::eIntrinsicMinSizing |
                          TrackSize::eIntrinsicMaxSizing)) {
        // Collect data for Step 2.
        maxSpan = std::max(maxSpan, span);
        if (span >= perSpanData.Length()) {
          perSpanData.SetLength(2 * span);
        }
        perSpanData[span].mItemCountWithSameSpan++;
        perSpanData[span].mStateBits |= state;
        CachedIntrinsicSizes cache;
        // Calculate data for "Automatic Minimum Size" clamping, if needed.
        if (TrackSize::IsDefiniteMaxSizing(state) &&
            (gridItem.mState[mAxis] & ItemState::eApplyAutoMinSize)) {
          nscoord minSizeClamp = 0;
          for (auto i : lineRange.Range()) {
            minSizeClamp += aFunctions.MaxSizingFor(i).AsBreadth().Resolve(
                aPercentageBasis);
          }
          minSizeClamp += mGridGap * (span - 1);
          cache.mMinSizeClamp = minSizeClamp;
          gridItem.mState[mAxis] |= ItemState::eClampMarginBoxMinSize;
        }
        // Collect the various grid item size contributions we need.
        nscoord minSize = 0;
        if (state & (TrackSize::eIntrinsicMinSizing |    // for 2.1
                     TrackSize::eIntrinsicMaxSizing)) {  // for 2.5
          minSize = MinSize(gridItem, aState, rc, wm, mAxis, &cache);
        }
        nscoord minContent = 0;
        if (state & contentBasedMinSelector) {  // for 2.2
          minContent =
              MinContentContribution(gridItem, aState, rc, wm, mAxis, &cache);
        }
        nscoord maxContent = 0;
        if (state & (maxContentMinSelector |                    // for 2.3
                     TrackSize::eAutoOrMaxContentMaxSizing)) {  // for 2.6
          maxContent =
              MaxContentContribution(gridItem, aState, rc, wm, mAxis, &cache);
        }
        step2Items.AppendElement(
            Step2ItemData({span, state, lineRange, minSize, minContent,
                           maxContent, gridItem.mFrame}));
      }
    }
    MOZ_ASSERT(!(gridItem.mState[mAxis] & ItemState::eClampMarginBoxMinSize) ||
                   (gridItem.mState[mAxis] & ItemState::eApplyAutoMinSize),
               "clamping only applies to Automatic Minimum Size");
  }

  // Step 2.
  if (maxSpan) {
    auto fitContentClamper = [&aFunctions, aPercentageBasis](uint32_t aTrack,
                                                             nscoord aMinSize,
                                                             nscoord* aSize) {
      nscoord fitContentLimit = ::ResolveToDefiniteSize(
          aFunctions.MaxSizingFor(aTrack), aPercentageBasis);
      if (*aSize > fitContentLimit) {
        *aSize = std::max(aMinSize, fitContentLimit);
        return true;
      }
      return false;
    };

    // Sort the collected items on span length, shortest first.  There's no need
    // for a stable sort here since the sizing isn't order dependent within
    // a group of items with the same span length.
    std::sort(step2Items.begin(), step2Items.end(),
              Step2ItemData::IsSpanLessThan);

    nsTArray<uint32_t> tracks(maxSpan);
    nsTArray<TrackSize> plan(mSizes.Length());
    plan.SetLength(mSizes.Length());
    nsTArray<TrackSize> itemPlan(mSizes.Length());
    itemPlan.SetLength(mSizes.Length());
    // Start / end iterator for items of the same span length:
    auto spanGroupStart = step2Items.begin();
    auto spanGroupEnd = spanGroupStart;
    const auto end = step2Items.end();
    for (; spanGroupStart != end; spanGroupStart = spanGroupEnd) {
      const uint32_t span = spanGroupStart->mSpan;
      spanGroupEnd = spanGroupStart + perSpanData[span].mItemCountWithSameSpan;
      TrackSize::StateBits stateBitsForSpan = perSpanData[span].mStateBits;
      bool updatedBase = false;  // Did we update any mBase in step 2.1 - 2.3?
      TrackSize::StateBits selector(TrackSize::eIntrinsicMinSizing);
      if (stateBitsForSpan & selector) {
        // Step 2.1 MinSize to intrinsic min-sizing.
        updatedBase =
            GrowSizeForSpanningItems<TrackSizingPhase::IntrinsicMinimums>(
                spanGroupStart, spanGroupEnd, tracks, plan, itemPlan, selector);
      }

      selector = contentBasedMinSelector;
      if (stateBitsForSpan & selector) {
        // Step 2.2 MinContentContribution to min-/max-content (and 'auto' when
        // sizing under a min-content constraint) min-sizing.
        updatedBase |=
            GrowSizeForSpanningItems<TrackSizingPhase::ContentBasedMinimums>(
                spanGroupStart, spanGroupEnd, tracks, plan, itemPlan, selector);
      }

      selector = maxContentMinSelector;
      if (stateBitsForSpan & selector) {
        // Step 2.3 MaxContentContribution to max-content (and 'auto' when
        // sizing under a max-content constraint) min-sizing.
        updatedBase |=
            GrowSizeForSpanningItems<TrackSizingPhase::MaxContentMinimums>(
                spanGroupStart, spanGroupEnd, tracks, plan, itemPlan, selector);
      }

      if (updatedBase) {
        // Step 2.4
        for (TrackSize& sz : mSizes) {
          if (sz.mBase > sz.mLimit) {
            sz.mLimit = sz.mBase;
          }
        }
      }

      selector = TrackSize::eIntrinsicMaxSizing;
      if (stateBitsForSpan & selector) {
        const bool willRunStep2_6 =
            stateBitsForSpan & TrackSize::eAutoOrMaxContentMaxSizing;
        // Step 2.5 MinSize to intrinsic max-sizing.
        GrowSizeForSpanningItems<TrackSizingPhase::IntrinsicMaximums>(
            spanGroupStart, spanGroupEnd, tracks, plan, itemPlan, selector,
            fitContentClamper, willRunStep2_6);

        if (willRunStep2_6) {
          // Step 2.6 MaxContentContribution to max-content max-sizing.
          selector = TrackSize::eAutoOrMaxContentMaxSizing;
          GrowSizeForSpanningItems<TrackSizingPhase::MaxContentMaximums>(
              spanGroupStart, spanGroupEnd, tracks, plan, itemPlan, selector,
              fitContentClamper);
        }
      }
    }
  }

  // Step 3.
  for (TrackSize& sz : mSizes) {
    if (sz.mLimit == NS_UNCONSTRAINEDSIZE) {
      sz.mLimit = sz.mBase;
    }
  }
}

float nsGridContainerFrame::Tracks::FindFrUnitSize(
    const LineRange& aRange, const nsTArray<uint32_t>& aFlexTracks,
    const TrackSizingFunctions& aFunctions, nscoord aSpaceToFill) const {
  MOZ_ASSERT(aSpaceToFill > 0 && !aFlexTracks.IsEmpty());
  float flexFactorSum = 0.0f;
  nscoord leftOverSpace = aSpaceToFill;
  for (auto i : aRange.Range()) {
    const TrackSize& sz = mSizes[i];
    if (sz.mState & TrackSize::eFlexMaxSizing) {
      flexFactorSum += aFunctions.MaxSizingFor(i).AsFr();
    } else {
      leftOverSpace -= sz.mBase;
      if (leftOverSpace <= 0) {
        return 0.0f;
      }
    }
  }
  bool restart;
  float hypotheticalFrSize;
  nsTArray<uint32_t> flexTracks(aFlexTracks);
  uint32_t numFlexTracks = flexTracks.Length();
  do {
    restart = false;
    hypotheticalFrSize = leftOverSpace / std::max(flexFactorSum, 1.0f);
    for (uint32_t i = 0, len = flexTracks.Length(); i < len; ++i) {
      uint32_t track = flexTracks[i];
      if (track == kAutoLine) {
        continue;  // Track marked as inflexible in a prev. iter of this loop.
      }
      float flexFactor = aFunctions.MaxSizingFor(track).AsFr();
      const nscoord base = mSizes[track].mBase;
      if (flexFactor * hypotheticalFrSize < base) {
        // 12.7.1.4: Treat this track as inflexible.
        flexTracks[i] = kAutoLine;
        flexFactorSum -= flexFactor;
        leftOverSpace -= base;
        --numFlexTracks;
        if (numFlexTracks == 0 || leftOverSpace <= 0) {
          return 0.0f;
        }
        restart = true;
        // break; XXX (bug 1176621 comment 16) measure which is more common
      }
    }
  } while (restart);
  return hypotheticalFrSize;
}

float nsGridContainerFrame::Tracks::FindUsedFlexFraction(
    GridReflowInput& aState, nsTArray<GridItemInfo>& aGridItems,
    const nsTArray<uint32_t>& aFlexTracks,
    const TrackSizingFunctions& aFunctions, nscoord aAvailableSize) const {
  if (aAvailableSize != NS_UNCONSTRAINEDSIZE) {
    // Use all of the grid tracks and a 'space to fill' of the available space.
    const TranslatedLineRange range(0, mSizes.Length());
    return FindFrUnitSize(range, aFlexTracks, aFunctions, aAvailableSize);
  }

  // The used flex fraction is the maximum of:
  // ... each flexible track's base size divided by its flex factor (which is
  // floored at 1).
  float fr = 0.0f;
  for (uint32_t track : aFlexTracks) {
    float flexFactor = aFunctions.MaxSizingFor(track).AsFr();
    float possiblyDividedBaseSize = (flexFactor > 1.0f)
                                        ? mSizes[track].mBase / flexFactor
                                        : mSizes[track].mBase;
    fr = std::max(fr, possiblyDividedBaseSize);
  }
  WritingMode wm = aState.mWM;
  gfxContext* rc = &aState.mRenderingContext;
  // ... the result of 'finding the size of an fr' for each item that spans
  // a flex track with its max-content contribution as 'space to fill'
  for (const GridItemInfo& item : aGridItems) {
    if (item.mState[mAxis] & ItemState::eIsFlexing) {
      // XXX optimize: bug 1194446
      auto pb = Some(aState.PercentageBasisFor(mAxis, item));
      nscoord spaceToFill = ContentContribution(item, aState, rc, wm, mAxis, pb,
                                                nsLayoutUtils::PREF_ISIZE);
      const LineRange& range =
          mAxis == eLogicalAxisInline ? item.mArea.mCols : item.mArea.mRows;
      MOZ_ASSERT(range.Extent() >= 1);
      const auto spannedGaps = range.Extent() - 1;
      if (spannedGaps > 0) {
        spaceToFill -= mGridGap * spannedGaps;
      }
      if (spaceToFill <= 0) {
        continue;
      }
      // ... and all its spanned tracks as input.
      nsTArray<uint32_t> itemFlexTracks;
      for (auto i : range.Range()) {
        if (mSizes[i].mState & TrackSize::eFlexMaxSizing) {
          itemFlexTracks.AppendElement(i);
        }
      }
      float itemFr =
          FindFrUnitSize(range, itemFlexTracks, aFunctions, spaceToFill);
      fr = std::max(fr, itemFr);
    }
  }
  return fr;
}

void nsGridContainerFrame::Tracks::StretchFlexibleTracks(
    GridReflowInput& aState, nsTArray<GridItemInfo>& aGridItems,
    const TrackSizingFunctions& aFunctions, nscoord aAvailableSize) {
  if (aAvailableSize <= 0) {
    return;
  }
  nsTArray<uint32_t> flexTracks(mSizes.Length());
  for (uint32_t i = 0, len = mSizes.Length(); i < len; ++i) {
    if (mSizes[i].mState & TrackSize::eFlexMaxSizing) {
      flexTracks.AppendElement(i);
    }
  }
  if (flexTracks.IsEmpty()) {
    return;
  }
  nscoord minSize = 0;
  nscoord maxSize = NS_UNCONSTRAINEDSIZE;
  if (aState.mReflowInput) {
    auto* ri = aState.mReflowInput;
    minSize = mAxis == eLogicalAxisBlock ? ri->ComputedMinBSize()
                                         : ri->ComputedMinISize();
    maxSize = mAxis == eLogicalAxisBlock ? ri->ComputedMaxBSize()
                                         : ri->ComputedMaxISize();
  }
  Maybe<nsTArray<TrackSize>> origSizes;
  bool applyMinMax = (minSize != 0 || maxSize != NS_UNCONSTRAINEDSIZE) &&
                     aAvailableSize == NS_UNCONSTRAINEDSIZE;
  // We iterate twice at most.  The 2nd time if the grid size changed after
  // applying a min/max-size (can only occur if aAvailableSize is indefinite).
  while (true) {
    float fr = FindUsedFlexFraction(aState, aGridItems, flexTracks, aFunctions,
                                    aAvailableSize);
    if (fr != 0.0f) {
      for (uint32_t i : flexTracks) {
        float flexFactor = aFunctions.MaxSizingFor(i).AsFr();
        nscoord flexLength = NSToCoordRound(flexFactor * fr);
        nscoord& base = mSizes[i].mBase;
        if (flexLength > base) {
          if (applyMinMax && origSizes.isNothing()) {
            origSizes.emplace(mSizes);
          }
          base = flexLength;
        }
      }
    }
    if (applyMinMax) {
      applyMinMax = false;
      // https://drafts.csswg.org/css-grid/#algo-flex-tracks
      // "If using this flex fraction would cause the grid to be smaller than
      // the grid container’s min-width/height (or larger than the grid
      // container’s max-width/height), then redo this step, treating the free
      // space as definite [...]"
      nscoord newSize = 0;
      for (auto& sz : mSizes) {
        newSize += sz.mBase;
      }
      const auto sumOfGridGaps = SumOfGridGaps();
      newSize += sumOfGridGaps;
      if (newSize > maxSize) {
        aAvailableSize = maxSize;
      } else if (newSize < minSize) {
        aAvailableSize = minSize;
      }
      if (aAvailableSize != NS_UNCONSTRAINEDSIZE) {
        aAvailableSize = std::max(0, aAvailableSize - sumOfGridGaps);
        // Restart with the original track sizes and definite aAvailableSize.
        if (origSizes.isSome()) {
          mSizes = std::move(*origSizes);
          origSizes.reset();
        }  // else, no mSizes[].mBase were changed above so it's still correct
        if (aAvailableSize == 0) {
          break;  // zero available size wouldn't change any sizes though...
        }
        continue;
      }
    }
    break;
  }
}

void nsGridContainerFrame::Tracks::AlignJustifyContent(
    const nsStylePosition* aStyle, WritingMode aWM, nscoord aContentBoxSize,
    bool aIsSubgriddedAxis) {
  const bool isAlign = mAxis == eLogicalAxisBlock;
  // Align-/justify-content doesn't apply in a subgridded axis.
  // Gap properties do apply though so we need to stretch/position the tracks
  // to center-align the gaps with the parent's gaps.
  if (MOZ_UNLIKELY(aIsSubgriddedAxis)) {
    auto& gap = isAlign ? aStyle->mRowGap : aStyle->mColumnGap;
    if (gap.IsNormal()) {
      return;
    }
    auto len = mSizes.Length();
    if (len <= 1) {
      return;
    }
    // This stores the gap deltas between the subgrid gap and the gaps in
    // the used track sizes (as encoded in its tracks' mPosition):
    nsTArray<nscoord> gapDeltas;
    const size_t numGaps = len - 1;
    gapDeltas.SetLength(numGaps);
    for (size_t i = 0; i < numGaps; ++i) {
      TrackSize& sz1 = mSizes[i];
      TrackSize& sz2 = mSizes[i + 1];
      nscoord currentGap = sz2.mPosition - (sz1.mPosition + sz1.mBase);
      gapDeltas[i] = mGridGap - currentGap;
    }
    // Recompute the tracks' size/position so that they end up with
    // a subgrid-gap centered on the original track gap.
    nscoord currentPos = mSizes[0].mPosition;
    nscoord lastHalfDelta(0);
    for (size_t i = 0; i < numGaps; ++i) {
      TrackSize& sz = mSizes[i];
      nscoord delta = gapDeltas[i];
      nscoord halfDelta;
      nscoord roundingError = NSCoordDivRem(delta, 2, &halfDelta);
      auto newSize = sz.mBase - (halfDelta + roundingError) - lastHalfDelta;
      lastHalfDelta = halfDelta;
      if (newSize >= 0) {
        sz.mBase = newSize;
        sz.mPosition = currentPos;
        currentPos += newSize + mGridGap;
      } else {
        sz.mBase = nscoord(0);
        sz.mPosition = currentPos + newSize;
        currentPos = sz.mPosition + mGridGap;
      }
    }
    auto& lastTrack = mSizes.LastElement();
    auto newSize = lastTrack.mBase - lastHalfDelta;
    if (newSize >= 0) {
      lastTrack.mBase = newSize;
      lastTrack.mPosition = currentPos;
    } else {
      lastTrack.mBase = nscoord(0);
      lastTrack.mPosition = currentPos + newSize;
    }
    return;
  }

  if (mSizes.IsEmpty()) {
    return;
  }

  auto valueAndFallback =
      isAlign ? aStyle->mAlignContent : aStyle->mJustifyContent;
  bool overflowSafe;
  auto alignment = ::GetAlignJustifyValue(valueAndFallback.primary, aWM,
                                          isAlign, &overflowSafe);
  if (alignment == StyleAlignFlags::NORMAL) {
    alignment = StyleAlignFlags::STRETCH;
    // we may need a fallback for 'stretch' below
    valueAndFallback = {alignment};
  }

  // Compute the free space and count auto-sized tracks.
  size_t numAutoTracks = 0;
  nscoord space;
  if (alignment != StyleAlignFlags::START) {
    nscoord trackSizeSum = 0;
    if (aIsSubgriddedAxis) {
      numAutoTracks = mSizes.Length();
    } else {
      for (const TrackSize& sz : mSizes) {
        trackSizeSum += sz.mBase;
        if (sz.mState & TrackSize::eAutoMaxSizing) {
          ++numAutoTracks;
        }
      }
    }
    space = aContentBoxSize - trackSizeSum - SumOfGridGaps();
    // Use the fallback value instead when applicable.
    if (space < 0 ||
        (alignment == StyleAlignFlags::SPACE_BETWEEN && mSizes.Length() == 1)) {
      auto fallback = ::GetAlignJustifyFallbackIfAny(valueAndFallback, aWM,
                                                     isAlign, &overflowSafe);
      if (fallback) {
        alignment = *fallback;
      }
    }
    if (space == 0 || (space < 0 && overflowSafe)) {
      // XXX check that this makes sense also for [last ]baseline (bug 1151204).
      alignment = StyleAlignFlags::START;
    }
  }

  // Optimize the cases where we just need to set each track's position.
  nscoord pos = 0;
  bool distribute = true;
  if (alignment == StyleAlignFlags::BASELINE ||
      alignment == StyleAlignFlags::LAST_BASELINE) {
    NS_WARNING("NYI: 'first/last baseline' (bug 1151204)");  // XXX
    alignment = StyleAlignFlags::START;
  }
  if (alignment == StyleAlignFlags::START) {
    distribute = false;
  } else if (alignment == StyleAlignFlags::END) {
    pos = space;
    distribute = false;
  } else if (alignment == StyleAlignFlags::CENTER) {
    pos = space / 2;
    distribute = false;
  } else if (alignment == StyleAlignFlags::STRETCH) {
    distribute = numAutoTracks != 0;
  }
  if (!distribute) {
    for (TrackSize& sz : mSizes) {
      sz.mPosition = pos;
      pos += sz.mBase + mGridGap;
    }
    return;
  }

  // Distribute free space to/between tracks and set their position.
  MOZ_ASSERT(space > 0, "should've handled that on the fallback path above");
  nscoord between, roundingError;
  if (alignment == StyleAlignFlags::STRETCH) {
    MOZ_ASSERT(numAutoTracks > 0, "we handled numAutoTracks == 0 above");
    nscoord spacePerTrack;
    roundingError = NSCoordDivRem(space, numAutoTracks, &spacePerTrack);
    for (TrackSize& sz : mSizes) {
      sz.mPosition = pos;
      if (!(sz.mState & TrackSize::eAutoMaxSizing)) {
        pos += sz.mBase + mGridGap;
        continue;
      }
      nscoord stretch = spacePerTrack;
      if (roundingError) {
        roundingError -= 1;
        stretch += 1;
      }
      nscoord newBase = sz.mBase + stretch;
      sz.mBase = newBase;
      pos += newBase + mGridGap;
    }
    MOZ_ASSERT(!roundingError, "we didn't distribute all rounding error?");
    return;
  }
  if (alignment == StyleAlignFlags::SPACE_BETWEEN) {
    MOZ_ASSERT(mSizes.Length() > 1, "should've used a fallback above");
    roundingError = NSCoordDivRem(space, mSizes.Length() - 1, &between);
  } else if (alignment == StyleAlignFlags::SPACE_AROUND) {
    roundingError = NSCoordDivRem(space, mSizes.Length(), &between);
    pos = between / 2;
  } else if (alignment == StyleAlignFlags::SPACE_EVENLY) {
    roundingError = NSCoordDivRem(space, mSizes.Length() + 1, &between);
    pos = between;
  } else {
    MOZ_ASSERT_UNREACHABLE("unknown align-/justify-content value");
    between = 0;        // just to avoid a compiler warning
    roundingError = 0;  // just to avoid a compiler warning
  }
  between += mGridGap;
  for (TrackSize& sz : mSizes) {
    sz.mPosition = pos;
    nscoord spacing = between;
    if (roundingError) {
      roundingError -= 1;
      spacing += 1;
    }
    pos += sz.mBase + spacing;
  }
  MOZ_ASSERT(!roundingError, "we didn't distribute all rounding error?");
}

void nsGridContainerFrame::LineRange::ToPositionAndLength(
    const nsTArray<TrackSize>& aTrackSizes, nscoord* aPos,
    nscoord* aLength) const {
  MOZ_ASSERT(mStart != kAutoLine && mEnd != kAutoLine,
             "expected a definite LineRange");
  MOZ_ASSERT(mStart < mEnd);
  nscoord startPos = aTrackSizes[mStart].mPosition;
  const TrackSize& sz = aTrackSizes[mEnd - 1];
  *aPos = startPos;
  *aLength = (sz.mPosition + sz.mBase) - startPos;
}

nscoord nsGridContainerFrame::LineRange::ToLength(
    const nsTArray<TrackSize>& aTrackSizes) const {
  MOZ_ASSERT(mStart != kAutoLine && mEnd != kAutoLine,
             "expected a definite LineRange");
  MOZ_ASSERT(mStart < mEnd);
  nscoord startPos = aTrackSizes[mStart].mPosition;
  const TrackSize& sz = aTrackSizes[mEnd - 1];
  return (sz.mPosition + sz.mBase) - startPos;
}

void nsGridContainerFrame::LineRange::ToPositionAndLengthForAbsPos(
    const Tracks& aTracks, nscoord aGridOrigin, nscoord* aPos,
    nscoord* aLength) const {
  // kAutoLine for abspos children contributes the corresponding edge
  // of the grid container's padding-box.
  if (mEnd == kAutoLine) {
    if (mStart == kAutoLine) {
      // done
    } else {
      const nscoord endPos = *aPos + *aLength;
      auto side = mStart == aTracks.mSizes.Length()
                      ? GridLineSide::BeforeGridGap
                      : GridLineSide::AfterGridGap;
      nscoord startPos = aTracks.GridLineEdge(mStart, side);
      *aPos = aGridOrigin + startPos;
      *aLength = std::max(endPos - *aPos, 0);
    }
  } else {
    if (mStart == kAutoLine) {
      auto side =
          mEnd == 0 ? GridLineSide::AfterGridGap : GridLineSide::BeforeGridGap;
      nscoord endPos = aTracks.GridLineEdge(mEnd, side);
      *aLength = std::max(aGridOrigin + endPos, 0);
    } else if (MOZ_LIKELY(mStart != mEnd)) {
      nscoord pos;
      ToPositionAndLength(aTracks.mSizes, &pos, aLength);
      *aPos = aGridOrigin + pos;
    } else {
      // The grid area only covers removed 'auto-fit' tracks.
      nscoord pos = aTracks.GridLineEdge(mStart, GridLineSide::BeforeGridGap);
      *aPos = aGridOrigin + pos;
      *aLength = nscoord(0);
    }
  }
}

LogicalSize nsGridContainerFrame::GridReflowInput::PercentageBasisFor(
    LogicalAxis aAxis, const GridItemInfo& aGridItem) const {
  auto wm = aGridItem.mFrame->GetWritingMode();
  const auto* itemParent = aGridItem.mFrame->GetParent();
  if (MOZ_UNLIKELY(itemParent != mFrame)) {
    // The item comes from a descendant subgrid.  Use the subgrid's
    // used track sizes to resolve the grid area size, if present.
    MOZ_ASSERT(itemParent->IsGridContainerFrame());
    auto* subgridFrame = static_cast<const nsGridContainerFrame*>(itemParent);
    MOZ_ASSERT(subgridFrame->IsSubgrid());
    if (auto* uts = subgridFrame->GetUsedTrackSizes()) {
      auto subgridWM = subgridFrame->GetWritingMode();
      LogicalSize cbSize(subgridWM, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
      if (!subgridFrame->IsSubgrid(eLogicalAxisInline) &&
          uts->mCanResolveLineRangeSize[eLogicalAxisInline]) {
        // NOTE: At this point aGridItem.mArea is in this->mFrame coordinates
        // and thus may have been transposed.  The range values in a non-
        // subgridded axis still has its original values in subgridFrame's
        // coordinates though.
        auto rangeAxis = subgridWM.IsOrthogonalTo(mWM) ? eLogicalAxisBlock
                                                       : eLogicalAxisInline;
        const auto& range = aGridItem.mArea.LineRangeForAxis(rangeAxis);
        cbSize.ISize(subgridWM) =
            range.ToLength(uts->mSizes[eLogicalAxisInline]);
      }
      if (!subgridFrame->IsSubgrid(eLogicalAxisBlock) &&
          uts->mCanResolveLineRangeSize[eLogicalAxisBlock]) {
        auto rangeAxis = subgridWM.IsOrthogonalTo(mWM) ? eLogicalAxisInline
                                                       : eLogicalAxisBlock;
        const auto& range = aGridItem.mArea.LineRangeForAxis(rangeAxis);
        cbSize.BSize(subgridWM) =
            range.ToLength(uts->mSizes[eLogicalAxisBlock]);
      }
      return cbSize.ConvertTo(wm, subgridWM);
    }

    return LogicalSize(wm, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }

  if (aAxis == eLogicalAxisInline || !mCols.mCanResolveLineRangeSize) {
    return LogicalSize(wm, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }
  // Note: for now, we only resolve transferred percentages to row sizing.
  // We may need to adjust these assertions once we implement bug 1300366.
  MOZ_ASSERT(!mRows.mCanResolveLineRangeSize);
  nscoord colSize = aGridItem.mArea.mCols.ToLength(mCols.mSizes);
  nscoord rowSize = NS_UNCONSTRAINEDSIZE;
  return !wm.IsOrthogonalTo(mWM) ? LogicalSize(wm, colSize, rowSize)
                                 : LogicalSize(wm, rowSize, colSize);
}

LogicalRect nsGridContainerFrame::GridReflowInput::ContainingBlockFor(
    const GridArea& aArea) const {
  nscoord i, b, iSize, bSize;
  MOZ_ASSERT(aArea.mCols.Extent() > 0, "grid items cover at least one track");
  MOZ_ASSERT(aArea.mRows.Extent() > 0, "grid items cover at least one track");
  aArea.mCols.ToPositionAndLength(mCols.mSizes, &i, &iSize);
  aArea.mRows.ToPositionAndLength(mRows.mSizes, &b, &bSize);
  return LogicalRect(mWM, i, b, iSize, bSize);
}

LogicalRect nsGridContainerFrame::GridReflowInput::ContainingBlockForAbsPos(
    const GridArea& aArea, const LogicalPoint& aGridOrigin,
    const LogicalRect& aGridCB) const {
  nscoord i = aGridCB.IStart(mWM);
  nscoord b = aGridCB.BStart(mWM);
  nscoord iSize = aGridCB.ISize(mWM);
  nscoord bSize = aGridCB.BSize(mWM);
  aArea.mCols.ToPositionAndLengthForAbsPos(mCols, aGridOrigin.I(mWM), &i,
                                           &iSize);
  aArea.mRows.ToPositionAndLengthForAbsPos(mRows, aGridOrigin.B(mWM), &b,
                                           &bSize);
  return LogicalRect(mWM, i, b, iSize, bSize);
}

/**
 * Return a Fragmentainer object if we have a fragmentainer frame in our
 * ancestor chain of containing block (CB) reflow inputs.  We'll only
 * continue traversing the ancestor chain as long as the CBs have
 * the same writing-mode and have overflow:visible.
 */
Maybe<nsGridContainerFrame::Fragmentainer>
nsGridContainerFrame::GetNearestFragmentainer(
    const GridReflowInput& aState) const {
  Maybe<nsGridContainerFrame::Fragmentainer> data;
  const ReflowInput* gridRI = aState.mReflowInput;
  if (gridRI->AvailableBSize() == NS_UNCONSTRAINEDSIZE) {
    return data;
  }
  WritingMode wm = aState.mWM;
  const ReflowInput* cbRI = gridRI->mCBReflowInput;
  for (; cbRI; cbRI = cbRI->mCBReflowInput) {
    nsIScrollableFrame* sf = do_QueryFrame(cbRI->mFrame);
    if (sf) {
      break;
    }
    if (wm.IsOrthogonalTo(cbRI->GetWritingMode())) {
      break;
    }
    LayoutFrameType frameType = cbRI->mFrame->Type();
    if ((frameType == LayoutFrameType::Canvas &&
         PresContext()->IsPaginated()) ||
        frameType == LayoutFrameType::ColumnSet) {
      data.emplace();
      data->mIsTopOfPage = gridRI->mFlags.mIsTopOfPage;
      data->mToFragmentainerEnd = aState.mFragBStart +
                                  gridRI->AvailableBSize() -
                                  aState.mBorderPadding.BStart(wm);
      const auto numRows = aState.mRows.mSizes.Length();
      data->mCanBreakAtStart =
          numRows > 0 && aState.mRows.mSizes[0].mPosition > 0;
      nscoord bSize = gridRI->ComputedBSize();
      data->mIsAutoBSize = bSize == NS_UNCONSTRAINEDSIZE;
      if (data->mIsAutoBSize) {
        bSize = gridRI->ComputedMinBSize();
      } else {
        bSize = NS_CSS_MINMAX(bSize, gridRI->ComputedMinBSize(),
                              gridRI->ComputedMaxBSize());
      }
      nscoord gridEnd =
          aState.mRows.GridLineEdge(numRows, GridLineSide::BeforeGridGap);
      data->mCanBreakAtEnd = bSize > gridEnd && bSize > aState.mFragBStart;
      break;
    }
  }
  return data;
}

void nsGridContainerFrame::ReflowInFlowChild(
    nsIFrame* aChild, const GridItemInfo* aGridItemInfo, nsSize aContainerSize,
    const Maybe<nscoord>& aStretchBSize, const Fragmentainer* aFragmentainer,
    const GridReflowInput& aState, const LogicalRect& aContentArea,
    ReflowOutput& aDesiredSize, nsReflowStatus& aStatus) {
  nsPresContext* pc = PresContext();
  ComputedStyle* containerSC = Style();
  WritingMode wm = aState.mReflowInput->GetWritingMode();
  const bool isGridItem = !!aGridItemInfo;
  MOZ_ASSERT(isGridItem == !aChild->IsPlaceholderFrame());
  LogicalRect cb(wm);
  WritingMode childWM = aChild->GetWritingMode();
  bool isConstrainedBSize = false;
  nscoord toFragmentainerEnd;
  // The part of the child's grid area that's in previous container fragments.
  nscoord consumedGridAreaBSize = 0;
  const bool isOrthogonal = wm.IsOrthogonalTo(childWM);
  if (MOZ_LIKELY(isGridItem)) {
    MOZ_ASSERT(aGridItemInfo->mFrame == aChild);
    const GridArea& area = aGridItemInfo->mArea;
    MOZ_ASSERT(area.IsDefinite());
    cb = aState.ContainingBlockFor(area);
    isConstrainedBSize = aFragmentainer && !wm.IsOrthogonalTo(childWM);
    if (isConstrainedBSize) {
      // |gridAreaBOffset| is the offset of the child's grid area in this
      // container fragment (if negative, that distance is the child CB size
      // consumed in previous container fragments).  Note that cb.BStart
      // (initially) and aState.mFragBStart are in "global" grid coordinates
      // (like all track positions).
      nscoord gridAreaBOffset = cb.BStart(wm) - aState.mFragBStart;
      consumedGridAreaBSize = std::max(0, -gridAreaBOffset);
      cb.BStart(wm) = std::max(0, gridAreaBOffset);
      toFragmentainerEnd = aFragmentainer->mToFragmentainerEnd -
                           aState.mFragBStart - cb.BStart(wm);
      toFragmentainerEnd = std::max(toFragmentainerEnd, 0);
    }
    cb += aContentArea.Origin(wm);
    aState.mRows.AlignBaselineSubtree(*aGridItemInfo);
    aState.mCols.AlignBaselineSubtree(*aGridItemInfo);
    // Setup [align|justify]-content:[last ]baseline related frame properties.
    // These are added to the padding in SizeComputationInput::InitOffsets.
    // (a negative value signals the value is for 'last baseline' and should be
    //  added to the (logical) end padding)
    typedef const FramePropertyDescriptor<SmallValueHolder<nscoord>>* Prop;
    auto SetProp = [aGridItemInfo, aChild](LogicalAxis aGridAxis, Prop aProp) {
      auto state = aGridItemInfo->mState[aGridAxis];
      auto baselineAdjust = (state & ItemState::eContentBaseline)
                                ? aGridItemInfo->mBaselineOffset[aGridAxis]
                                : nscoord(0);
      if (baselineAdjust < nscoord(0)) {
        // This happens when the subtree overflows its track.
        // XXX spec issue? it's unclear how to handle this.
        baselineAdjust = nscoord(0);
      } else if (baselineAdjust > nscoord(0) &&
                 (state & ItemState::eLastBaseline)) {
        baselineAdjust = -baselineAdjust;
      }
      if (baselineAdjust != nscoord(0)) {
        aChild->SetProperty(aProp, baselineAdjust);
      } else {
        aChild->RemoveProperty(aProp);
      }
    };
    SetProp(eLogicalAxisBlock,
            isOrthogonal ? IBaselinePadProperty() : BBaselinePadProperty());
    SetProp(eLogicalAxisInline,
            isOrthogonal ? BBaselinePadProperty() : IBaselinePadProperty());
  } else {
    // By convention, for frames that perform CSS Box Alignment, we position
    // placeholder children at the start corner of their alignment container,
    // and in this case that's usually the grid's content-box.
    // ("Usually" - the exception is when the grid *also* forms the
    // abs.pos. containing block. In that case, the alignment container isn't
    // the content-box -- it's some grid area instead.  But that case doesn't
    // require any special handling here, because we handle it later using a
    // special flag (STATIC_POS_IS_CB_ORIGIN) which will make us ignore the
    // placeholder's position entirely.)
    cb = aContentArea;
    aChild->AddStateBits(PLACEHOLDER_STATICPOS_NEEDS_CSSALIGN);
  }

  LogicalSize reflowSize(cb.Size(wm));
  if (isConstrainedBSize) {
    reflowSize.BSize(wm) = toFragmentainerEnd;
  }
  LogicalSize childCBSize = reflowSize.ConvertTo(childWM, wm);

  // Setup the ClampMarginBoxMinSize reflow flags and property, if needed.
  uint32_t flags = 0;
  if (aGridItemInfo) {
    // Clamp during reflow if we're stretching in that axis.
    auto* pos = aChild->StylePosition();
    auto j = pos->UsedJustifySelf(Style())._0;
    auto a = pos->UsedAlignSelf(Style())._0;
    bool stretch[2];
    stretch[eLogicalAxisInline] =
        j == StyleAlignFlags::NORMAL || j == StyleAlignFlags::STRETCH;
    stretch[eLogicalAxisBlock] =
        a == StyleAlignFlags::NORMAL || a == StyleAlignFlags::STRETCH;
    auto childIAxis = isOrthogonal ? eLogicalAxisBlock : eLogicalAxisInline;
    if (stretch[childIAxis] &&
        aGridItemInfo->mState[childIAxis] & ItemState::eClampMarginBoxMinSize) {
      flags |= ReflowInput::I_CLAMP_MARGIN_BOX_MIN_SIZE;
    }

    auto childBAxis = GetOrthogonalAxis(childIAxis);
    if (stretch[childBAxis] &&
        aGridItemInfo->mState[childBAxis] & ItemState::eClampMarginBoxMinSize) {
      flags |= ReflowInput::B_CLAMP_MARGIN_BOX_MIN_SIZE;
      aChild->SetProperty(BClampMarginBoxMinSizeProperty(),
                          childCBSize.BSize(childWM));
    } else {
      aChild->RemoveProperty(BClampMarginBoxMinSizeProperty());
    }

    if ((aGridItemInfo->mState[childIAxis] & ItemState::eApplyAutoMinSize)) {
      flags |= ReflowInput::I_APPLY_AUTO_MIN_SIZE;
    }
  }

  if (!isConstrainedBSize) {
    childCBSize.BSize(childWM) = NS_UNCONSTRAINEDSIZE;
  }
  LogicalSize percentBasis(cb.Size(wm).ConvertTo(childWM, wm));
  ReflowInput childRI(pc, *aState.mReflowInput, aChild, childCBSize,
                      Some(percentBasis), flags);
  childRI.mFlags.mIsTopOfPage =
      aFragmentainer ? aFragmentainer->mIsTopOfPage : false;

  // Because we pass ReflowInput::COMPUTE_SIZE_USE_AUTO_BSIZE, and the
  // previous reflow of the child might not have, set the child's
  // block-resize flag to true.
  // FIXME (perf): It would be faster to do this only if the previous
  // reflow of the child was a measuring reflow, and only if the child
  // does some of the things that are affected by
  // ReflowInput::COMPUTE_SIZE_USE_AUTO_BSIZE.
  childRI.SetBResize(true);
  childRI.mFlags.mIsBResizeForPercentages = true;

  // A table-wrapper needs to propagate the CB size we give it to its
  // inner table frame later.  @see nsTableWrapperFrame::InitChildReflowInput.
  if (aChild->IsTableWrapperFrame()) {
    LogicalSize* cb =
        aChild->GetProperty(nsTableWrapperFrame::GridItemCBSizeProperty());
    if (!cb) {
      cb = new LogicalSize(childWM);
      aChild->SetProperty(nsTableWrapperFrame::GridItemCBSizeProperty(), cb);
    }
    *cb = percentBasis;
  }

  // If the child is stretching in its block axis, and we might be fragmenting
  // it in that axis, then setup a frame property to tell
  // nsBlockFrame::ComputeFinalSize the size.
  if (isConstrainedBSize && !wm.IsOrthogonalTo(childWM)) {
    bool stretch = false;
    if (!childRI.mStyleMargin->HasBlockAxisAuto(childWM) &&
        childRI.mStylePosition->BSize(childWM).IsAuto()) {
      auto blockAxisAlignment = childRI.mStylePosition->UsedAlignSelf(Style());
      if (blockAxisAlignment._0 == StyleAlignFlags::NORMAL ||
          blockAxisAlignment._0 == StyleAlignFlags::STRETCH) {
        stretch = true;
      }
    }
    if (stretch) {
      aChild->SetProperty(FragStretchBSizeProperty(), *aStretchBSize);
    } else {
      aChild->RemoveProperty(FragStretchBSizeProperty());
    }
  }

  // We need the width of the child before we can correctly convert
  // the writing-mode of its origin, so we reflow at (0, 0) using a dummy
  // aContainerSize, and then pass the correct position to FinishReflowChild.
  ReflowOutput childSize(childRI);
  const nsSize dummyContainerSize;

  // XXXdholbert The childPos that we use for ReflowChild shouldn't matter,
  // since we finalize it in FinishReflowChild. However, it does matter if the
  // child happens to be XUL (which sizes menu popup frames based on the
  // position within the viewport, during this ReflowChild call). So we make an
  // educated guess that the child will be at the origin of its containing
  // block, and then use align/justify to correct that as-needed further
  // down. (If the child has a different writing mode than its parent, though,
  // then we can't express the CB origin until we've reflowed the child and
  // determined its size. In that case, we throw up our hands and don't bother
  // trying to guess the position up-front after all.)
  // XXXdholbert We'll remove this special case in bug 1600542, and then we can
  // go back to just setting childPos in a single call after ReflowChild.
  LogicalPoint childPos(childWM);
  if (MOZ_LIKELY(childWM == wm)) {
    // Initially, assume the child will be at the containing block origin.
    // (This may get corrected during alignment/justification below.)
    childPos = cb.Origin(wm);
  }
  ReflowChild(aChild, pc, childSize, childRI, childWM, childPos,
              dummyContainerSize, ReflowChildFlags::Default, aStatus);
  if (MOZ_UNLIKELY(childWM != wm)) {
    // As above: assume the child will be at the containing block origin.
    // (which we can now compute in terms of the childWM, now that we know the
    // child's size).
    childPos = cb.Origin(wm).ConvertTo(
        childWM, wm, aContainerSize - childSize.PhysicalSize());
  }
  // Apply align/justify-self and reflow again if that affects the size.
  if (MOZ_LIKELY(isGridItem)) {
    LogicalSize size = childSize.Size(childWM);  // from the ReflowChild()
    if (aStatus.IsComplete()) {
      auto align = childRI.mStylePosition->UsedAlignSelf(containerSC);
      auto state = aGridItemInfo->mState[eLogicalAxisBlock];
      if (state & ItemState::eContentBaseline) {
        align = {(state & ItemState::eFirstBaseline)
                     ? StyleAlignFlags::SELF_START
                     : StyleAlignFlags::SELF_END};
      }
      nscoord cbsz = cb.BSize(wm) - consumedGridAreaBSize;
      AlignSelf(*aGridItemInfo, align, cbsz, wm, childRI, size, &childPos);
    }
    auto justify = childRI.mStylePosition->UsedJustifySelf(containerSC);
    auto state = aGridItemInfo->mState[eLogicalAxisInline];
    if (state & ItemState::eContentBaseline) {
      justify = {(state & ItemState::eFirstBaseline)
                     ? StyleAlignFlags::SELF_START
                     : StyleAlignFlags::SELF_END};
    }
    nscoord cbsz = cb.ISize(wm);
    JustifySelf(*aGridItemInfo, justify, cbsz, wm, childRI, size, &childPos);
  }  // else, nsAbsoluteContainingBlock.cpp will handle align/justify-self.

  FinishReflowChild(aChild, pc, childSize, &childRI, childWM, childPos,
                    aContainerSize, ReflowChildFlags::ApplyRelativePositioning);
  ConsiderChildOverflow(aDesiredSize.mOverflowAreas, aChild);
}

nscoord nsGridContainerFrame::ReflowInFragmentainer(
    GridReflowInput& aState, const LogicalRect& aContentArea,
    ReflowOutput& aDesiredSize, nsReflowStatus& aStatus,
    Fragmentainer& aFragmentainer, const nsSize& aContainerSize) {
  MOZ_ASSERT(aStatus.IsEmpty());
  MOZ_ASSERT(aState.mReflowInput);

  // Collect our grid items and sort them in row order.  Collect placeholders
  // and put them in a separate array.
  nsTArray<const GridItemInfo*> sortedItems(aState.mGridItems.Length());
  nsTArray<nsIFrame*> placeholders(aState.mAbsPosItems.Length());
  aState.mIter.Reset(CSSOrderAwareFrameIterator::eIncludeAll);
  for (; !aState.mIter.AtEnd(); aState.mIter.Next()) {
    nsIFrame* child = *aState.mIter;
    if (!child->IsPlaceholderFrame()) {
      const GridItemInfo* info = &aState.mGridItems[aState.mIter.ItemIndex()];
      sortedItems.AppendElement(info);
    } else {
      placeholders.AppendElement(child);
    }
  }
  // NOTE: no need to use stable_sort here, there are no dependencies on
  // having content order between items on the same row in the code below.
  std::sort(sortedItems.begin(), sortedItems.end(),
            GridItemInfo::IsStartRowLessThan);

  // Reflow our placeholder children; they must all be complete.
  for (auto child : placeholders) {
    nsReflowStatus childStatus;
    ReflowInFlowChild(child, nullptr, aContainerSize, Nothing(),
                      &aFragmentainer, aState, aContentArea, aDesiredSize,
                      childStatus);
    MOZ_ASSERT(childStatus.IsComplete(),
               "nsPlaceholderFrame should never need to be fragmented");
  }

  // The available size for children - we'll set this to the edge of the last
  // row in most cases below, but for now use the full size.
  nscoord childAvailableSize = aFragmentainer.mToFragmentainerEnd;
  const uint32_t startRow = aState.mStartRow;
  const uint32_t numRows = aState.mRows.mSizes.Length();
  bool isBDBClone = aState.mReflowInput->mStyleBorder->mBoxDecorationBreak ==
                    StyleBoxDecorationBreak::Clone;
  nscoord bpBEnd = aState.mBorderPadding.BEnd(aState.mWM);

  // Set |endRow| to the first row that doesn't fit.
  uint32_t endRow = numRows;
  for (uint32_t row = startRow; row < numRows; ++row) {
    auto& sz = aState.mRows.mSizes[row];
    const nscoord bEnd = sz.mPosition + sz.mBase;
    nscoord remainingAvailableSize = childAvailableSize - bEnd;
    if (remainingAvailableSize < 0 ||
        (isBDBClone && remainingAvailableSize < bpBEnd)) {
      endRow = row;
      break;
    }
  }

  // Check for forced breaks on the items.
  const bool isTopOfPage = aFragmentainer.mIsTopOfPage;
  bool isForcedBreak = false;
  const bool avoidBreakInside = ShouldAvoidBreakInside(*aState.mReflowInput);
  for (const GridItemInfo* info : sortedItems) {
    uint32_t itemStartRow = info->mArea.mRows.mStart;
    if (itemStartRow == endRow) {
      break;
    }
    auto disp = info->mFrame->StyleDisplay();
    if (disp->BreakBefore()) {
      // Propagate break-before on the first row to the container unless we're
      // already at top-of-page.
      if ((itemStartRow == 0 && !isTopOfPage) || avoidBreakInside) {
        aStatus.SetInlineLineBreakBeforeAndReset();
        return aState.mFragBStart;
      }
      if ((itemStartRow > startRow ||
           (itemStartRow == startRow && !isTopOfPage)) &&
          itemStartRow < endRow) {
        endRow = itemStartRow;
        isForcedBreak = true;
        // reset any BREAK_AFTER we found on an earlier item
        aStatus.Reset();
        break;  // we're done since the items are sorted in row order
      }
    }
    uint32_t itemEndRow = info->mArea.mRows.mEnd;
    if (disp->BreakAfter()) {
      if (itemEndRow != numRows) {
        if (itemEndRow > startRow && itemEndRow < endRow) {
          endRow = itemEndRow;
          isForcedBreak = true;
          // No "break;" here since later items with break-after may have
          // a shorter span.
        }
      } else {
        // Propagate break-after on the last row to the container, we may still
        // find a break-before on this row though (and reset aStatus).
        aStatus.SetInlineLineBreakAfter();  // tentative
      }
    }
  }

  // Consume at least one row in each fragment until we have consumed them all.
  // Except for the first row if there's a break opportunity before it.
  if (startRow == endRow && startRow != numRows &&
      (startRow != 0 || !aFragmentainer.mCanBreakAtStart)) {
    ++endRow;
  }

  // Honor break-inside:avoid if we can't fit all rows.
  if (avoidBreakInside && endRow < numRows) {
    aStatus.SetInlineLineBreakBeforeAndReset();
    return aState.mFragBStart;
  }

  // Calculate the block-size including this fragment.
  nscoord bEndRow =
      aState.mRows.GridLineEdge(endRow, GridLineSide::BeforeGridGap);
  nscoord bSize;
  if (aFragmentainer.mIsAutoBSize) {
    // We only apply min-bsize once all rows are complete (when bsize is auto).
    if (endRow < numRows) {
      bSize = bEndRow;
      auto clampedBSize = ClampToCSSMaxBSize(bSize, aState.mReflowInput);
      if (MOZ_UNLIKELY(clampedBSize != bSize)) {
        // We apply max-bsize in all fragments though.
        bSize = clampedBSize;
      } else if (!isBDBClone) {
        // The max-bsize won't make this fragment COMPLETE, so the block-end
        // border will be in a later fragment.
        bpBEnd = 0;
      }
    } else {
      bSize = NS_CSS_MINMAX(bEndRow, aState.mReflowInput->ComputedMinBSize(),
                            aState.mReflowInput->ComputedMaxBSize());
    }
  } else {
    bSize = NS_CSS_MINMAX(aState.mReflowInput->ComputedBSize(),
                          aState.mReflowInput->ComputedMinBSize(),
                          aState.mReflowInput->ComputedMaxBSize());
  }

  // Check for overflow and set aStatus INCOMPLETE if so.
  bool overflow = bSize + bpBEnd > childAvailableSize;
  if (overflow) {
    if (avoidBreakInside) {
      aStatus.SetInlineLineBreakBeforeAndReset();
      return aState.mFragBStart;
    }
    bool breakAfterLastRow = endRow == numRows && aFragmentainer.mCanBreakAtEnd;
    if (breakAfterLastRow) {
      MOZ_ASSERT(bEndRow < bSize, "bogus aFragmentainer.mCanBreakAtEnd");
      nscoord availableSize = childAvailableSize;
      if (isBDBClone) {
        availableSize -= bpBEnd;
      }
      // Pretend we have at least 1px available size, otherwise we'll never make
      // progress in consuming our bSize.
      availableSize =
          std::max(availableSize, aState.mFragBStart + AppUnitsPerCSSPixel());
      // Fill the fragmentainer, but not more than our desired block-size and
      // at least to the size of the last row (even if that overflows).
      nscoord newBSize = std::min(bSize, availableSize);
      newBSize = std::max(newBSize, bEndRow);
      // If it's just the border+padding that is overflowing and we have
      // box-decoration-break:clone then we are technically COMPLETE.  There's
      // no point in creating another zero-bsize fragment in this case.
      if (newBSize < bSize || !isBDBClone) {
        aStatus.SetIncomplete();
      }
      bSize = newBSize;
    } else if (bSize <= bEndRow && startRow + 1 < endRow) {
      if (endRow == numRows) {
        // We have more than one row in this fragment, so we can break before
        // the last row instead.
        --endRow;
        bEndRow =
            aState.mRows.GridLineEdge(endRow, GridLineSide::BeforeGridGap);
        bSize = bEndRow;
        if (aFragmentainer.mIsAutoBSize) {
          bSize = ClampToCSSMaxBSize(bSize, aState.mReflowInput);
        }
      }
      aStatus.SetIncomplete();
    } else if (endRow < numRows) {
      bSize = ClampToCSSMaxBSize(bEndRow, aState.mReflowInput, &aStatus);
    }  // else - no break opportunities.
  } else {
    // Even though our block-size fits we need to honor forced breaks, or if
    // a row doesn't fit in an auto-sized container (unless it's constrained
    // by a max-bsize which make us overflow-incomplete).
    if (endRow < numRows &&
        (isForcedBreak || (aFragmentainer.mIsAutoBSize && bEndRow == bSize))) {
      bSize = ClampToCSSMaxBSize(bEndRow, aState.mReflowInput, &aStatus);
    }
  }

  // If we can't fit all rows then we're at least overflow-incomplete.
  if (endRow < numRows) {
    childAvailableSize = bEndRow;
    if (aStatus.IsComplete()) {
      aStatus.SetOverflowIncomplete();
      aStatus.SetNextInFlowNeedsReflow();
    }
  } else {
    // Children always have the full size of the rows in this fragment.
    childAvailableSize = std::max(childAvailableSize, bEndRow);
  }

  return ReflowRowsInFragmentainer(aState, aContentArea, aDesiredSize, aStatus,
                                   aFragmentainer, aContainerSize, sortedItems,
                                   startRow, endRow, bSize, childAvailableSize);
}

nscoord nsGridContainerFrame::ReflowRowsInFragmentainer(
    GridReflowInput& aState, const LogicalRect& aContentArea,
    ReflowOutput& aDesiredSize, nsReflowStatus& aStatus,
    Fragmentainer& aFragmentainer, const nsSize& aContainerSize,
    const nsTArray<const GridItemInfo*>& aSortedItems, uint32_t aStartRow,
    uint32_t aEndRow, nscoord aBSize, nscoord aAvailableSize) {
  FrameHashtable pushedItems;
  FrameHashtable incompleteItems;
  FrameHashtable overflowIncompleteItems;
  bool isBDBClone = aState.mReflowInput->mStyleBorder->mBoxDecorationBreak ==
                    StyleBoxDecorationBreak::Clone;
  bool didGrowRow = false;
  // As we walk across rows, we track whether the current row is at the top
  // of its grid-fragment, to help decide whether we can break before it. When
  // this function starts, our row is at the top of the current fragment if:
  //  - we're starting with a nonzero row (i.e. we're a continuation)
  // OR:
  //  - we're starting with the first row, & we're not allowed to break before
  //    it (which makes it effectively at the top of its grid-fragment).
  bool isRowTopOfPage = aStartRow != 0 || !aFragmentainer.mCanBreakAtStart;
  const bool isStartRowTopOfPage = isRowTopOfPage;
  // Save our full available size for later.
  const nscoord gridAvailableSize = aFragmentainer.mToFragmentainerEnd;
  // Propagate the constrained size to our children.
  aFragmentainer.mToFragmentainerEnd = aAvailableSize;
  // Reflow the items in row order up to |aEndRow| and push items after that.
  uint32_t row = 0;
  // |i| is intentionally signed, so we can set it to -1 to restart the loop.
  for (int32_t i = 0, len = aSortedItems.Length(); i < len; ++i) {
    const GridItemInfo* const info = aSortedItems[i];
    nsIFrame* child = info->mFrame;
    row = info->mArea.mRows.mStart;
    MOZ_ASSERT(child->GetPrevInFlow() ? row < aStartRow : row >= aStartRow,
               "unexpected child start row");
    if (row >= aEndRow) {
      pushedItems.PutEntry(child);
      continue;
    }

    bool rowCanGrow = false;
    nscoord maxRowSize = 0;
    if (row >= aStartRow) {
      if (row > aStartRow) {
        isRowTopOfPage = false;
      }
      // Can we grow this row?  Only consider span=1 items per spec...
      rowCanGrow = !didGrowRow && info->mArea.mRows.Extent() == 1;
      if (rowCanGrow) {
        auto& sz = aState.mRows.mSizes[row];
        // and only min-/max-content rows or flex rows in an auto-sized
        // container
        rowCanGrow = (sz.mState & TrackSize::eMinOrMaxContentMinSizing) ||
                     ((sz.mState & TrackSize::eFlexMaxSizing) &&
                      aFragmentainer.mIsAutoBSize);
        if (rowCanGrow) {
          if (isBDBClone) {
            maxRowSize =
                gridAvailableSize - aState.mBorderPadding.BEnd(aState.mWM);
          } else {
            maxRowSize = gridAvailableSize;
          }
          maxRowSize -= sz.mPosition;
          // ...and only if there is space for it to grow.
          rowCanGrow = maxRowSize > sz.mBase;
        }
      }
    }

    // aFragmentainer.mIsTopOfPage is propagated to the child reflow input.
    // When it's false the child may request InlineBreak::Before.  We set it
    // to false when the row is growable (as determined in the CSS Grid
    // Fragmentation spec) and there is a non-zero space between it and the
    // fragmentainer end (that can be used to grow it).  If the child reports
    // a forced break in this case, we grow this row to fill the fragment and
    // restart the loop.  We also restart the loop with |aEndRow = row|
    // (but without growing any row) for a InlineBreak::Before child if it spans
    // beyond the last row in this fragment.  This is to avoid fragmenting it.
    // We only restart the loop once.
    aFragmentainer.mIsTopOfPage = isRowTopOfPage && !rowCanGrow;
    nsReflowStatus childStatus;
    // Pass along how much to stretch this fragment, in case it's needed.
    nscoord bSize =
        aState.mRows.GridLineEdge(std::min(aEndRow, info->mArea.mRows.mEnd),
                                  GridLineSide::BeforeGridGap) -
        aState.mRows.GridLineEdge(std::max(aStartRow, row),
                                  GridLineSide::AfterGridGap);
    ReflowInFlowChild(child, info, aContainerSize, Some(bSize), &aFragmentainer,
                      aState, aContentArea, aDesiredSize, childStatus);
    MOZ_ASSERT(childStatus.IsInlineBreakBefore() ||
                   !childStatus.IsFullyComplete() || !child->GetNextInFlow(),
               "fully-complete reflow should destroy any NIFs");

    if (childStatus.IsInlineBreakBefore()) {
      MOZ_ASSERT(
          !child->GetPrevInFlow(),
          "continuations should never report InlineBreak::Before status");
      MOZ_ASSERT(!aFragmentainer.mIsTopOfPage,
                 "got IsInlineBreakBefore() at top of page");
      if (!didGrowRow) {
        if (rowCanGrow) {
          // Grow this row and restart with the next row as |aEndRow|.
          aState.mRows.ResizeRow(row, maxRowSize);
          if (aState.mSharedGridData) {
            aState.mSharedGridData->mRows.ResizeRow(row, maxRowSize);
          }
          didGrowRow = true;
          aEndRow = row + 1;  // growing this row makes the next one not fit
          i = -1;             // i == 0 after the next loop increment
          isRowTopOfPage = isStartRowTopOfPage;
          overflowIncompleteItems.Clear();
          incompleteItems.Clear();
          nscoord bEndRow =
              aState.mRows.GridLineEdge(aEndRow, GridLineSide::BeforeGridGap);
          aFragmentainer.mToFragmentainerEnd = bEndRow;
          if (aFragmentainer.mIsAutoBSize) {
            aBSize = ClampToCSSMaxBSize(bEndRow, aState.mReflowInput, &aStatus);
          } else if (aStatus.IsIncomplete()) {
            aBSize = NS_CSS_MINMAX(aState.mReflowInput->ComputedBSize(),
                                   aState.mReflowInput->ComputedMinBSize(),
                                   aState.mReflowInput->ComputedMaxBSize());
            aBSize = std::min(bEndRow, aBSize);
          }
          continue;
        }

        if (!isRowTopOfPage) {
          // We can break before this row - restart with it as the new end row.
          aEndRow = row;
          aBSize =
              aState.mRows.GridLineEdge(aEndRow, GridLineSide::BeforeGridGap);
          i = -1;  // i == 0 after the next loop increment
          isRowTopOfPage = isStartRowTopOfPage;
          overflowIncompleteItems.Clear();
          incompleteItems.Clear();
          aStatus.SetIncomplete();
          continue;
        }
        NS_ERROR("got InlineBreak::Before at top-of-page");
        childStatus.Reset();
      } else {
        // We got InlineBreak::Before again after growing the row - this can
        // happen if the child isn't splittable, e.g. some form controls.
        childStatus.Reset();
        if (child->GetNextInFlow()) {
          // The child already has a fragment, so we know it's splittable.
          childStatus.SetIncomplete();
        }  // else, report that it's complete
      }
    } else if (childStatus.IsInlineBreakAfter()) {
      MOZ_ASSERT_UNREACHABLE("unexpected child reflow status");
    }

    MOZ_ASSERT(!childStatus.IsInlineBreakBefore(),
               "should've handled InlineBreak::Before above");
    if (childStatus.IsIncomplete()) {
      incompleteItems.PutEntry(child);
    } else if (!childStatus.IsFullyComplete()) {
      overflowIncompleteItems.PutEntry(child);
    }
  }

  // Record a break before |aEndRow|.
  aState.mNextFragmentStartRow = aEndRow;
  if (aEndRow < aState.mRows.mSizes.Length()) {
    aState.mRows.BreakBeforeRow(aEndRow);
    if (aState.mSharedGridData) {
      aState.mSharedGridData->mRows.BreakBeforeRow(aEndRow);
    }
  }

  const bool childrenMoved = PushIncompleteChildren(
      pushedItems, incompleteItems, overflowIncompleteItems);
  if (childrenMoved && aStatus.IsComplete()) {
    aStatus.SetOverflowIncomplete();
    aStatus.SetNextInFlowNeedsReflow();
  }
  if (!pushedItems.IsEmpty()) {
    AddStateBits(NS_STATE_GRID_DID_PUSH_ITEMS);
    // NOTE since we messed with our child list here, we intentionally
    // make aState.mIter invalid to avoid any use of it after this point.
    aState.mIter.Invalidate();
  }
  if (!incompleteItems.IsEmpty()) {
    // NOTE since we messed with our child list here, we intentionally
    // make aState.mIter invalid to avoid any use of it after this point.
    aState.mIter.Invalidate();
  }

  return aBSize;
}

nsGridContainerFrame* nsGridContainerFrame::ParentGridContainerForSubgrid()
    const {
  MOZ_ASSERT(IsSubgrid());
  nsIFrame* p = GetParent();
  while (p->GetContent() == GetContent()) {
    p = p->GetParent();
  }
  MOZ_ASSERT(p->IsGridContainerFrame());
  auto* parent = static_cast<nsGridContainerFrame*>(p);
  MOZ_ASSERT(parent->HasSubgridItems());
  return parent;
}

nscoord nsGridContainerFrame::ReflowChildren(GridReflowInput& aState,
                                             const LogicalRect& aContentArea,
                                             ReflowOutput& aDesiredSize,
                                             nsReflowStatus& aStatus) {
  MOZ_ASSERT(aState.mReflowInput);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  nsOverflowAreas ocBounds;
  nsReflowStatus ocStatus;
  if (GetPrevInFlow()) {
    ReflowOverflowContainerChildren(PresContext(), *aState.mReflowInput,
                                    ocBounds, ReflowChildFlags::Default,
                                    ocStatus, MergeSortedFrameListsFor);
  }

  WritingMode wm = aState.mReflowInput->GetWritingMode();
  const nsSize containerSize =
      (aContentArea.Size(wm) + aState.mBorderPadding.Size(wm))
          .GetPhysicalSize(wm);

  nscoord bSize = aContentArea.BSize(wm);
  Maybe<Fragmentainer> fragmentainer = GetNearestFragmentainer(aState);
  if (MOZ_UNLIKELY(fragmentainer.isSome())) {
    aState.mInFragmentainer = true;
    bSize = ReflowInFragmentainer(aState, aContentArea, aDesiredSize, aStatus,
                                  *fragmentainer, containerSize);
  } else {
    aState.mIter.Reset(CSSOrderAwareFrameIterator::eIncludeAll);
    for (; !aState.mIter.AtEnd(); aState.mIter.Next()) {
      nsIFrame* child = *aState.mIter;
      const GridItemInfo* info = nullptr;
      if (!child->IsPlaceholderFrame()) {
        info = &aState.mGridItems[aState.mIter.ItemIndex()];
      }
      ReflowInFlowChild(*aState.mIter, info, containerSize, Nothing(), nullptr,
                        aState, aContentArea, aDesiredSize, aStatus);
      MOZ_ASSERT(aStatus.IsComplete(),
                 "child should be complete "
                 "in unconstrained reflow");
    }
  }

  // Merge overflow container bounds and status.
  aDesiredSize.mOverflowAreas.UnionWith(ocBounds);
  aStatus.MergeCompletionStatusFrom(ocStatus);

  if (IsAbsoluteContainer()) {
    nsFrameList children(GetChildList(GetAbsoluteListID()));
    if (!children.IsEmpty()) {
      // 'gridOrigin' is the origin of the grid (the start of the first track),
      // with respect to the grid container's padding-box (CB).
      LogicalMargin pad(aState.mReflowInput->ComputedLogicalPadding());
      const LogicalPoint gridOrigin(wm, pad.IStart(wm), pad.BStart(wm));
      const LogicalRect gridCB(wm, 0, 0,
                               aContentArea.ISize(wm) + pad.IStartEnd(wm),
                               bSize + pad.BStartEnd(wm));
      const nsSize gridCBPhysicalSize = gridCB.Size(wm).GetPhysicalSize(wm);
      size_t i = 0;
      for (nsFrameList::Enumerator e(children); !e.AtEnd(); e.Next(), ++i) {
        nsIFrame* child = e.get();
        MOZ_ASSERT(i < aState.mAbsPosItems.Length());
        MOZ_ASSERT(aState.mAbsPosItems[i].mFrame == child);
        GridArea& area = aState.mAbsPosItems[i].mArea;
        LogicalRect itemCB =
            aState.ContainingBlockForAbsPos(area, gridOrigin, gridCB);
        // nsAbsoluteContainingBlock::Reflow uses physical coordinates.
        nsRect* cb = child->GetProperty(GridItemContainingBlockRect());
        if (!cb) {
          cb = new nsRect;
          child->SetProperty(GridItemContainingBlockRect(), cb);
        }
        *cb = itemCB.GetPhysicalRect(wm, gridCBPhysicalSize);
      }
      // We pass a dummy rect as CB because each child has its own CB rect.
      // The eIsGridContainerCB flag tells nsAbsoluteContainingBlock::Reflow to
      // use those instead.
      nsRect dummyRect;
      AbsPosReflowFlags flags =
          AbsPosReflowFlags::CBWidthAndHeightChanged;  // XXX could be optimized
      flags |= AbsPosReflowFlags::ConstrainHeight;
      flags |= AbsPosReflowFlags::IsGridContainerCB;
      GetAbsoluteContainingBlock()->Reflow(
          this, PresContext(), *aState.mReflowInput, aStatus, dummyRect, flags,
          &aDesiredSize.mOverflowAreas);
    }
  }
  return bSize;
}

void nsGridContainerFrame::NormalizeChildLists() {
  // First we gather child frames we should include in our reflow/placement,
  // i.e. overflowed children from our prev-in-flow, and pushed first-in-flow
  // children (that might now fit). It's important to note that these children
  // can be in arbitrary order vis-a-vis the current children in our lists.
  // E.g. grid items in the document order: A, B, C may be placed in the rows
  // 3, 2, 1.  Assume each row goes in a separate grid container fragment,
  // and we reflow the second fragment.  Now if C (in fragment 1) overflows,
  // we can't just prepend it to our mFrames like we usually do because that
  // would violate the document order invariant that other code depends on.
  // Similarly if we pull up child A (from fragment 3) we can't just append
  // that for the same reason.  Instead, we must sort these children into
  // our child lists.  (The sorting is trivial given that both lists are
  // already fully sorted individually - it's just a merge.)
  //
  // The invariants that we maintain are that each grid container child list
  // is sorted in the normal document order at all times, but that children
  // in different grid container continuations may be in arbitrary order.

  auto prevInFlow = static_cast<nsGridContainerFrame*>(GetPrevInFlow());
  // Merge overflow frames from our prev-in-flow into our principal child list.
  if (prevInFlow) {
    AutoFrameListPtr overflow(PresContext(), prevInFlow->StealOverflowFrames());
    if (overflow) {
      ReparentFrames(*overflow, prevInFlow, this);
      MergeSortedFrameLists(mFrames, *overflow, GetContent());

      // Move trailing next-in-flows into our overflow list.
      nsFrameList continuations;
      for (nsIFrame* f = mFrames.FirstChild(); f;) {
        nsIFrame* next = f->GetNextSibling();
        nsIFrame* pif = f->GetPrevInFlow();
        if (pif && pif->GetParent() == this) {
          mFrames.RemoveFrame(f);
          continuations.AppendFrame(nullptr, f);
        }
        f = next;
      }
      MergeSortedOverflow(continuations);

      // Move trailing OC next-in-flows into our excess overflow containers
      // list.
      nsFrameList* overflowContainers =
          GetPropTableFrames(OverflowContainersProperty());
      if (overflowContainers) {
        nsFrameList moveToEOC;
        for (nsIFrame* f = overflowContainers->FirstChild(); f;) {
          nsIFrame* next = f->GetNextSibling();
          nsIFrame* pif = f->GetPrevInFlow();
          if (pif && pif->GetParent() == this) {
            overflowContainers->RemoveFrame(f);
            moveToEOC.AppendFrame(nullptr, f);
          }
          f = next;
        }
        if (overflowContainers->IsEmpty()) {
          RemoveProperty(OverflowContainersProperty());
        }
        MergeSortedExcessOverflowContainers(moveToEOC);
      }
    }
  }

  // Merge our own overflow frames into our principal child list,
  // except those that are a next-in-flow for one of our items.
  DebugOnly<bool> foundOwnPushedChild = false;
  {
    nsFrameList* ourOverflow = GetOverflowFrames();
    if (ourOverflow) {
      nsFrameList items;
      for (nsIFrame* f = ourOverflow->FirstChild(); f;) {
        nsIFrame* next = f->GetNextSibling();
        nsIFrame* pif = f->GetPrevInFlow();
        if (!pif || pif->GetParent() != this) {
          MOZ_ASSERT(f->GetParent() == this);
          ourOverflow->RemoveFrame(f);
          items.AppendFrame(nullptr, f);
          if (!pif) {
            foundOwnPushedChild = true;
          }
        }
        f = next;
      }
      MergeSortedFrameLists(mFrames, items, GetContent());
      if (ourOverflow->IsEmpty()) {
        DestroyOverflowList();
      }
    }
  }

  // Push any child next-in-flows in our principal list to OverflowList.
  if (HasAnyStateBits(NS_STATE_GRID_HAS_CHILD_NIFS)) {
    nsFrameList framesToPush;
    nsIFrame* firstChild = mFrames.FirstChild();
    // Note that we potentially modify our mFrames list as we go.
    for (auto child = firstChild; child; child = child->GetNextSibling()) {
      if (auto* childNIF = child->GetNextInFlow()) {
        if (childNIF->GetParent() == this) {
          for (auto c = child->GetNextSibling(); c; c = c->GetNextSibling()) {
            if (c == childNIF) {
              // child's next-in-flow is in our principal child list, push it.
              mFrames.RemoveFrame(childNIF);
              framesToPush.AppendFrame(nullptr, childNIF);
              break;
            }
          }
        }
      }
    }
    if (!framesToPush.IsEmpty()) {
      MergeSortedOverflow(framesToPush);
    }
    RemoveStateBits(NS_STATE_GRID_HAS_CHILD_NIFS);
  }

  // Pull up any first-in-flow children we might have pushed.
  if (HasAnyStateBits(NS_STATE_GRID_DID_PUSH_ITEMS)) {
    RemoveStateBits(NS_STATE_GRID_DID_PUSH_ITEMS);
    nsFrameList items;
    auto nif = static_cast<nsGridContainerFrame*>(GetNextInFlow());
    auto firstNIF = nif;
    DebugOnly<bool> nifNeedPushedItem = false;
    while (nif) {
      nsFrameList nifItems;
      for (nsIFrame* nifChild = nif->GetChildList(kPrincipalList).FirstChild();
           nifChild;) {
        nsIFrame* next = nifChild->GetNextSibling();
        if (!nifChild->GetPrevInFlow()) {
          nif->StealFrame(nifChild);
          ReparentFrame(nifChild, nif, this);
          nifItems.AppendFrame(nullptr, nifChild);
          nifNeedPushedItem = false;
        }
        nifChild = next;
      }
      MergeSortedFrameLists(items, nifItems, GetContent());

      if (!nif->HasAnyStateBits(NS_STATE_GRID_DID_PUSH_ITEMS)) {
        MOZ_ASSERT(!nifNeedPushedItem || mDidPushItemsBitMayLie,
                   "NS_STATE_GRID_DID_PUSH_ITEMS lied");
        break;
      }
      nifNeedPushedItem = true;

      for (nsIFrame* nifChild = nif->GetChildList(kOverflowList).FirstChild();
           nifChild;) {
        nsIFrame* next = nifChild->GetNextSibling();
        if (!nifChild->GetPrevInFlow()) {
          nif->StealFrame(nifChild);
          ReparentFrame(nifChild, nif, this);
          nifItems.AppendFrame(nullptr, nifChild);
          nifNeedPushedItem = false;
        }
        nifChild = next;
      }
      MergeSortedFrameLists(items, nifItems, GetContent());

      nif->RemoveStateBits(NS_STATE_GRID_DID_PUSH_ITEMS);
      nif = static_cast<nsGridContainerFrame*>(nif->GetNextInFlow());
      MOZ_ASSERT(nif || !nifNeedPushedItem || mDidPushItemsBitMayLie,
                 "NS_STATE_GRID_DID_PUSH_ITEMS lied");
    }

    if (!items.IsEmpty()) {
      // Pull up the first next-in-flow of the pulled up items too, unless its
      // parent is our nif (to avoid leaving a hole there).
      nsFrameList childNIFs;
      nsFrameList childOCNIFs;
      for (auto child : items) {
        auto childNIF = child->GetNextInFlow();
        if (childNIF && childNIF->GetParent() != firstNIF) {
          auto parent = childNIF->GetParent();
          parent->StealFrame(childNIF);
          ReparentFrame(childNIF, parent, firstNIF);
          if ((childNIF->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
            childOCNIFs.AppendFrame(nullptr, childNIF);
          } else {
            childNIFs.AppendFrame(nullptr, childNIF);
          }
        }
      }
      // Merge items' NIFs into our NIF's respective overflow child lists.
      firstNIF->MergeSortedOverflow(childNIFs);
      firstNIF->MergeSortedExcessOverflowContainers(childOCNIFs);
    }

    MOZ_ASSERT(
        foundOwnPushedChild || !items.IsEmpty() || mDidPushItemsBitMayLie,
        "NS_STATE_GRID_DID_PUSH_ITEMS lied");
    MergeSortedFrameLists(mFrames, items, GetContent());
  }
}

void nsGridContainerFrame::Reflow(nsPresContext* aPresContext,
                                  ReflowOutput& aDesiredSize,
                                  const ReflowInput& aReflowInput,
                                  nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsGridContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  if (IsFrameTreeTooDeep(aReflowInput, aDesiredSize, aStatus)) {
    return;
  }

  NormalizeChildLists();

#ifdef DEBUG
  mDidPushItemsBitMayLie = false;
  SanityCheckGridItemsBeforeReflow();
#endif  // DEBUG

  for (auto& perAxisBaseline : mBaseline) {
    for (auto& baseline : perAxisBaseline) {
      baseline = NS_INTRINSIC_ISIZE_UNKNOWN;
    }
  }

  const nsStylePosition* stylePos = aReflowInput.mStylePosition;
  auto prevInFlow = static_cast<nsGridContainerFrame*>(GetPrevInFlow());
  if (MOZ_LIKELY(!prevInFlow)) {
    InitImplicitNamedAreas(stylePos);
  } else {
    MOZ_ASSERT((prevInFlow->GetStateBits() & kIsSubgridBits) ==
                   (GetStateBits() & kIsSubgridBits),
               "continuations should have same kIsSubgridBits");
  }
  GridReflowInput gridReflowInput(this, aReflowInput);
  if (gridReflowInput.mIter.ItemsAreAlreadyInOrder()) {
    AddStateBits(NS_STATE_GRID_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER);
  } else {
    RemoveStateBits(NS_STATE_GRID_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER);
  }
  if (gridReflowInput.mIter.AtEnd() ||
      aReflowInput.mStyleDisplay->IsContainLayout()) {
    // We have no grid items, or we're layout-contained. So, we have no
    // baseline, and our parent should synthesize a baseline if needed.
    AddStateBits(NS_STATE_GRID_SYNTHESIZE_BASELINE);
  } else {
    RemoveStateBits(NS_STATE_GRID_SYNTHESIZE_BASELINE);
  }
  const nscoord computedBSize = aReflowInput.ComputedBSize();
  const nscoord computedISize = aReflowInput.ComputedISize();
  const WritingMode& wm = gridReflowInput.mWM;
  const LogicalSize computedSize(wm, computedISize, computedBSize);

  nscoord consumedBSize = 0;
  nscoord bSize = 0;
  if (MOZ_LIKELY(!prevInFlow)) {
    Grid grid;
    if (MOZ_LIKELY(!IsSubgrid())) {
      RepeatTrackSizingInput repeatSizing(aReflowInput.ComputedMinSize(),
                                          computedSize,
                                          aReflowInput.ComputedMaxSize());
      grid.PlaceGridItems(gridReflowInput, repeatSizing);
    } else {
      auto* subgrid = GetProperty(Subgrid::Prop());
      MOZ_ASSERT(subgrid, "an ancestor forgot to call PlaceGridItems?");
      gridReflowInput.mGridItems = subgrid->mGridItems;
      gridReflowInput.mAbsPosItems = subgrid->mAbsPosItems;
      grid.mGridColEnd = subgrid->mGridColEnd;
      grid.mGridRowEnd = subgrid->mGridRowEnd;
    }
    gridReflowInput.CalculateTrackSizes(grid, computedSize,
                                        SizingConstraint::NoConstraint);
    // XXX Technically incorrect: We're ignoring our row sizes, when really
    // we should use them but *they* should be computed as if we had no
    // children. To be fixed in bug 1488878.
    if (!aReflowInput.mStyleDisplay->IsContainSize()) {
      const auto& rowSizes = gridReflowInput.mRows.mSizes;
      if (MOZ_LIKELY(!IsSubgrid(eLogicalAxisBlock))) {
        // Note: we can't use GridLineEdge here since we haven't calculated
        // the rows' mPosition yet (happens in AlignJustifyContent below).
        for (const auto& sz : rowSizes) {
          bSize += sz.mBase;
        }
        bSize += gridReflowInput.mRows.SumOfGridGaps();
      } else if (computedBSize == NS_UNCONSTRAINEDSIZE) {
        bSize = gridReflowInput.mRows.GridLineEdge(rowSizes.Length(),
                                                   GridLineSide::BeforeGridGap);
      }
    }
  } else {
    consumedBSize = ConsumedBSize(wm);
    gridReflowInput.InitializeForContinuation(this, consumedBSize);
    // XXX Technically incorrect: We're ignoring our row sizes, when really
    // we should use them but *they* should be computed as if we had no
    // children. To be fixed in bug 1488878.
    if (!aReflowInput.mStyleDisplay->IsContainSize()) {
      const uint32_t numRows = gridReflowInput.mRows.mSizes.Length();
      bSize = gridReflowInput.mRows.GridLineEdge(numRows,
                                                 GridLineSide::AfterGridGap);
    }
  }
  if (computedBSize == NS_UNCONSTRAINEDSIZE) {
    bSize = NS_CSS_MINMAX(bSize, aReflowInput.ComputedMinBSize(),
                          aReflowInput.ComputedMaxBSize());
  } else {
    bSize = computedBSize;
  }
  bSize = std::max(bSize - consumedBSize, 0);
  auto& bp = gridReflowInput.mBorderPadding;
  LogicalRect contentArea(wm, bp.IStart(wm), bp.BStart(wm), computedISize,
                          bSize);

  if (!prevInFlow) {
    const auto& rowSizes = gridReflowInput.mRows.mSizes;
    if (!IsRowSubgrid()) {
      // Apply 'align-content' to the grid.
      if (computedBSize == NS_UNCONSTRAINEDSIZE &&
          stylePos->mRowGap.IsLengthPercentage() &&
          stylePos->mRowGap.AsLengthPercentage().HasPercent()) {
        // Re-resolve the row-gap now that we know our intrinsic block-size.
        gridReflowInput.mRows.mGridGap =
            nsLayoutUtils::ResolveGapToLength(stylePos->mRowGap, bSize);
      }
      gridReflowInput.mRows.AlignJustifyContent(stylePos, wm, bSize, false);
    } else {
      if (computedBSize == NS_UNCONSTRAINEDSIZE) {
        bSize = gridReflowInput.mRows.GridLineEdge(rowSizes.Length(),
                                                   GridLineSide::BeforeGridGap);
        contentArea.BSize(wm) = std::max(bSize, nscoord(0));
      }
    }
    // Save the final row sizes for use by subgrids, if needed.
    if (HasSubgridItems() || IsSubgrid()) {
      StoreUsedTrackSizes(eLogicalAxisBlock, rowSizes);
    }
  }

  bSize = ReflowChildren(gridReflowInput, contentArea, aDesiredSize, aStatus);
  bSize = std::max(bSize - consumedBSize, 0);

  // Skip our block-end border if we're INCOMPLETE.
  if (!aStatus.IsComplete() && !gridReflowInput.mSkipSides.BEnd() &&
      StyleBorder()->mBoxDecorationBreak != StyleBoxDecorationBreak::Clone) {
    bp.BEnd(wm) = nscoord(0);
  }

  LogicalSize desiredSize(wm, computedISize + bp.IStartEnd(wm),
                          bSize + bp.BStartEnd(wm));
  aDesiredSize.SetSize(wm, desiredSize);
  nsRect frameRect(0, 0, aDesiredSize.Width(), aDesiredSize.Height());
  aDesiredSize.mOverflowAreas.UnionAllWith(frameRect);

  // Convert INCOMPLETE -> OVERFLOW_INCOMPLETE and zero bsize if we're an OC.
  if (HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER)) {
    if (!aStatus.IsComplete()) {
      aStatus.SetOverflowIncomplete();
      aStatus.SetNextInFlowNeedsReflow();
    }
    bSize = 0;
    desiredSize.BSize(wm) = bSize + bp.BStartEnd(wm);
    aDesiredSize.SetSize(wm, desiredSize);
  }

  if (!gridReflowInput.mInFragmentainer) {
    MOZ_ASSERT(gridReflowInput.mIter.IsValid());
    auto sz = frameRect.Size();
    CalculateBaselines(BaselineSet::eBoth, &gridReflowInput.mIter,
                       &gridReflowInput.mGridItems, gridReflowInput.mCols, 0,
                       gridReflowInput.mCols.mSizes.Length(), wm, sz,
                       bp.IStart(wm), bp.IEnd(wm), desiredSize.ISize(wm));
    CalculateBaselines(BaselineSet::eBoth, &gridReflowInput.mIter,
                       &gridReflowInput.mGridItems, gridReflowInput.mRows, 0,
                       gridReflowInput.mRows.mSizes.Length(), wm, sz,
                       bp.BStart(wm), bp.BEnd(wm), desiredSize.BSize(wm));
  } else {
    // Only compute 'first baseline' if this fragment contains the first track.
    // XXXmats maybe remove this condition? bug 1306499
    BaselineSet baselines = BaselineSet::eNone;
    if (gridReflowInput.mStartRow == 0 &&
        gridReflowInput.mStartRow != gridReflowInput.mNextFragmentStartRow) {
      baselines = BaselineSet::eFirst;
    }
    // Only compute 'last baseline' if this fragment contains the last track.
    // XXXmats maybe remove this condition? bug 1306499
    uint32_t len = gridReflowInput.mRows.mSizes.Length();
    if (gridReflowInput.mStartRow != len &&
        gridReflowInput.mNextFragmentStartRow == len) {
      baselines = BaselineSet(baselines | BaselineSet::eLast);
    }
    Maybe<CSSOrderAwareFrameIterator> iter;
    Maybe<nsTArray<GridItemInfo>> gridItems;
    if (baselines != BaselineSet::eNone) {
      // We need to create a new iterator and GridItemInfo array because we
      // might have pushed some children at this point.
      // Even if the gridReflowInput iterator is invalid we can reuse its
      // state about order to optimize initialization of the new iterator.
      // An ordered child list can't become unordered by pushing frames.
      // An unordered list can become ordered in a number of cases, but we
      // ignore that here and guess that the child list is still unordered.
      // XXX this is O(n^2) in the number of items in this fragment: bug 1306705
      using Filter = CSSOrderAwareFrameIterator::ChildFilter;
      using Order = CSSOrderAwareFrameIterator::OrderState;
      bool ordered = gridReflowInput.mIter.ItemsAreAlreadyInOrder();
      auto orderState = ordered ? Order::eKnownOrdered : Order::eKnownUnordered;
      iter.emplace(this, kPrincipalList, Filter::eSkipPlaceholders, orderState);
      gridItems.emplace();
      for (; !iter->AtEnd(); iter->Next()) {
        auto child = **iter;
        for (const auto& info : gridReflowInput.mGridItems) {
          if (info.mFrame == child) {
            gridItems->AppendElement(info);
          }
        }
      }
    }
    auto sz = frameRect.Size();
    CalculateBaselines(baselines, iter.ptrOr(nullptr), gridItems.ptrOr(nullptr),
                       gridReflowInput.mCols, 0,
                       gridReflowInput.mCols.mSizes.Length(), wm, sz,
                       bp.IStart(wm), bp.IEnd(wm), desiredSize.ISize(wm));
    CalculateBaselines(baselines, iter.ptrOr(nullptr), gridItems.ptrOr(nullptr),
                       gridReflowInput.mRows, gridReflowInput.mStartRow,
                       gridReflowInput.mNextFragmentStartRow, wm, sz,
                       bp.BStart(wm), bp.BEnd(wm), desiredSize.BSize(wm));
  }

  if (HasAnyStateBits(NS_STATE_GRID_GENERATE_COMPUTED_VALUES)) {
    // This state bit will never be cleared, since reflow can be called
    // multiple times in fragmented grids, and it's challenging to scope
    // the bit to only that sequence of calls. This is relatively harmless
    // since this bit is only set by accessing a ChromeOnly property, and
    // therefore can't unduly slow down normal web browsing.

    // Now that we know column and row sizes and positions, set
    // the ComputedGridTrackInfo and related properties

    const auto* subgrid = GetProperty(Subgrid::Prop());
    const auto* subgridColRange = subgrid && IsSubgrid(eLogicalAxisInline)
                                      ? &subgrid->SubgridCols()
                                      : nullptr;

    LineNameMap colLineNameMap(
        gridReflowInput.mGridStyle, GetImplicitNamedAreas(),
        gridReflowInput.mColFunctions, nullptr, subgridColRange, true);
    uint32_t colTrackCount = gridReflowInput.mCols.mSizes.Length();
    nsTArray<nscoord> colTrackPositions(colTrackCount);
    nsTArray<nscoord> colTrackSizes(colTrackCount);
    nsTArray<uint32_t> colTrackStates(colTrackCount);
    nsTArray<bool> colRemovedRepeatTracks(
        gridReflowInput.mColFunctions.mRemovedRepeatTracks);
    uint32_t col = 0;
    for (const TrackSize& sz : gridReflowInput.mCols.mSizes) {
      colTrackPositions.AppendElement(sz.mPosition);
      colTrackSizes.AppendElement(sz.mBase);
      bool isRepeat =
          ((col >= gridReflowInput.mColFunctions.mRepeatAutoStart) &&
           (col < gridReflowInput.mColFunctions.mRepeatAutoEnd));
      colTrackStates.AppendElement(
          isRepeat ? (uint32_t)mozilla::dom::GridTrackState::Repeat
                   : (uint32_t)mozilla::dom::GridTrackState::Static);

      col++;
    }
    // Get the number of explicit tracks first. The order of argument evaluation
    // is implementation-defined. We should be OK here because colTrackSizes is
    // taken by rvalue, but computing the size first prevents any changes in the
    // argument types of the constructor from breaking this.
    const uint32_t numColExplicitTracks =
        IsSubgrid(eLogicalAxisInline)
            ? colTrackSizes.Length()
            : gridReflowInput.mColFunctions.NumExplicitTracks();
    ComputedGridTrackInfo* colInfo = new ComputedGridTrackInfo(
        gridReflowInput.mColFunctions.mExplicitGridOffset, numColExplicitTracks,
        0, col, std::move(colTrackPositions), std::move(colTrackSizes),
        std::move(colTrackStates), std::move(colRemovedRepeatTracks),
        gridReflowInput.mColFunctions.mRepeatAutoStart,
        colLineNameMap.GetResolvedLineNamesForComputedGridTrackInfo(),
        IsSubgrid(eLogicalAxisInline));
    SetProperty(GridColTrackInfo(), colInfo);

    const auto* subgridRowRange = subgrid && IsSubgrid(eLogicalAxisBlock)
                                      ? &subgrid->SubgridRows()
                                      : nullptr;
    LineNameMap rowLineNameMap(
        gridReflowInput.mGridStyle, GetImplicitNamedAreas(),
        gridReflowInput.mRowFunctions, nullptr, subgridRowRange, true);
    uint32_t rowTrackCount = gridReflowInput.mRows.mSizes.Length();
    nsTArray<nscoord> rowTrackPositions(rowTrackCount);
    nsTArray<nscoord> rowTrackSizes(rowTrackCount);
    nsTArray<uint32_t> rowTrackStates(rowTrackCount);
    nsTArray<bool> rowRemovedRepeatTracks(
        gridReflowInput.mRowFunctions.mRemovedRepeatTracks);
    uint32_t row = 0;
    for (const TrackSize& sz : gridReflowInput.mRows.mSizes) {
      rowTrackPositions.AppendElement(sz.mPosition);
      rowTrackSizes.AppendElement(sz.mBase);
      bool isRepeat =
          ((row >= gridReflowInput.mRowFunctions.mRepeatAutoStart) &&
           (row < gridReflowInput.mRowFunctions.mRepeatAutoEnd));
      rowTrackStates.AppendElement(
          isRepeat ? (uint32_t)mozilla::dom::GridTrackState::Repeat
                   : (uint32_t)mozilla::dom::GridTrackState::Static);

      row++;
    }
    // Get the number of explicit tracks first. The order of argument evaluation
    // is implementation-defined. We should be OK here because colTrackSizes is
    // taken by rvalue, but computing the size first prevents any changes in the
    // argument types of the constructor from breaking this.
    const uint32_t numRowExplicitTracks =
        IsSubgrid(eLogicalAxisBlock)
            ? rowTrackSizes.Length()
            : gridReflowInput.mRowFunctions.NumExplicitTracks();
    // Row info has to accommodate fragmentation of the grid, which may happen
    // in later calls to Reflow. For now, presume that no more fragmentation
    // will occur.
    ComputedGridTrackInfo* rowInfo = new ComputedGridTrackInfo(
        gridReflowInput.mRowFunctions.mExplicitGridOffset, numRowExplicitTracks,
        gridReflowInput.mStartRow, row, std::move(rowTrackPositions),
        std::move(rowTrackSizes), std::move(rowTrackStates),
        std::move(rowRemovedRepeatTracks),
        gridReflowInput.mRowFunctions.mRepeatAutoStart,
        rowLineNameMap.GetResolvedLineNamesForComputedGridTrackInfo(),
        IsSubgrid(eLogicalAxisBlock));
    SetProperty(GridRowTrackInfo(), rowInfo);

    if (prevInFlow) {
      // This frame is fragmenting rows from a previous frame, so patch up
      // the prior GridRowTrackInfo with a new end row.

      // FIXME: This can be streamlined and/or removed when bug 1151204 lands.

      ComputedGridTrackInfo* priorRowInfo =
          prevInFlow->GetProperty(GridRowTrackInfo());

      // Adjust track positions based on the first track in this fragment.
      if (priorRowInfo->mPositions.Length() >
          priorRowInfo->mStartFragmentTrack) {
        nscoord delta =
            priorRowInfo->mPositions[priorRowInfo->mStartFragmentTrack];
        for (nscoord& pos : priorRowInfo->mPositions) {
          pos -= delta;
        }
      }

      ComputedGridTrackInfo* revisedPriorRowInfo = new ComputedGridTrackInfo(
          priorRowInfo->mNumLeadingImplicitTracks,
          priorRowInfo->mNumExplicitTracks, priorRowInfo->mStartFragmentTrack,
          gridReflowInput.mStartRow, std::move(priorRowInfo->mPositions),
          std::move(priorRowInfo->mSizes), std::move(priorRowInfo->mStates),
          std::move(priorRowInfo->mRemovedRepeatTracks),
          priorRowInfo->mRepeatFirstTrack,
          std::move(priorRowInfo->mResolvedLineNames),
          priorRowInfo->mIsSubgrid);
      prevInFlow->SetProperty(GridRowTrackInfo(), revisedPriorRowInfo);
    }

    // Generate the line info properties. We need to provide the number of
    // repeat tracks produced in the reflow. Only explicit names are assigned
    // to lines here; the mozilla::dom::GridLines class will later extract
    // implicit names from grid areas and assign them to the appropriate lines.

    auto& colFunctions = gridReflowInput.mColFunctions;

    // Generate column lines first.
    uint32_t capacity = gridReflowInput.mCols.mSizes.Length();
    nsTArray<nsTArray<RefPtr<nsAtom>>> columnLineNames(capacity);
    for (col = 0; col <= gridReflowInput.mCols.mSizes.Length(); col++) {
      // Offset col by the explicit grid offset, to get the original names.
      nsTArray<RefPtr<nsAtom>> explicitNames =
          colLineNameMap.GetExplicitLineNamesAtIndex(
              col - colFunctions.mExplicitGridOffset);

      columnLineNames.AppendElement(explicitNames);
    }
    // Get the explicit names that follow a repeat auto declaration.
    nsTArray<RefPtr<nsAtom>> colNamesFollowingRepeat;
    nsTArray<RefPtr<nsAtom>> colBeforeRepeatAuto;
    nsTArray<RefPtr<nsAtom>> colAfterRepeatAuto;
    // Note: the following is only used for a non-subgridded axis.
    if (colLineNameMap.HasRepeatAuto()) {
      MOZ_ASSERT(!colFunctions.mTemplate.IsSubgrid());
      // The line name list after the repeatAutoIndex holds the line names
      // for the first explicit line after the repeat auto declaration.
      uint32_t repeatAutoEnd = colLineNameMap.RepeatAutoStart() + 1;
      for (auto* list : colLineNameMap.ExpandedLineNames()[repeatAutoEnd]) {
        for (auto& name : list->AsSpan()) {
          colNamesFollowingRepeat.AppendElement(name.AsAtom());
        }
      }
      auto names = colLineNameMap.TrackAutoRepeatLineNames();
      for (auto& name : names[0].AsSpan()) {
        colBeforeRepeatAuto.AppendElement(name.AsAtom());
      }
      for (auto& name : names[1].AsSpan()) {
        colAfterRepeatAuto.AppendElement(name.AsAtom());
      }
    }

    ComputedGridLineInfo* columnLineInfo = new ComputedGridLineInfo(
        std::move(columnLineNames), std::move(colBeforeRepeatAuto),
        std::move(colAfterRepeatAuto), std::move(colNamesFollowingRepeat));
    SetProperty(GridColumnLineInfo(), columnLineInfo);

    // Generate row lines next.
    auto& rowFunctions = gridReflowInput.mRowFunctions;
    capacity = gridReflowInput.mRows.mSizes.Length();
    nsTArray<nsTArray<RefPtr<nsAtom>>> rowLineNames(capacity);
    for (row = 0; row <= gridReflowInput.mRows.mSizes.Length(); row++) {
      // Offset row by the explicit grid offset, to get the original names.
      nsTArray<RefPtr<nsAtom>> explicitNames =
          rowLineNameMap.GetExplicitLineNamesAtIndex(
              row - rowFunctions.mExplicitGridOffset);
      rowLineNames.AppendElement(explicitNames);
    }
    // Get the explicit names that follow a repeat auto declaration.
    nsTArray<RefPtr<nsAtom>> rowNamesFollowingRepeat;
    nsTArray<RefPtr<nsAtom>> rowBeforeRepeatAuto;
    nsTArray<RefPtr<nsAtom>> rowAfterRepeatAuto;
    // Note: the following is only used for a non-subgridded axis.
    if (rowLineNameMap.HasRepeatAuto()) {
      MOZ_ASSERT(!rowFunctions.mTemplate.IsSubgrid());
      // The line name list after the repeatAutoIndex holds the line names
      // for the first explicit line after the repeat auto declaration.
      uint32_t repeatAutoEnd = rowLineNameMap.RepeatAutoStart() + 1;
      for (auto* list : rowLineNameMap.ExpandedLineNames()[repeatAutoEnd]) {
        for (auto& name : list->AsSpan()) {
          rowNamesFollowingRepeat.AppendElement(name.AsAtom());
        }
      }
      auto names = rowLineNameMap.TrackAutoRepeatLineNames();
      for (auto& name : names[0].AsSpan()) {
        rowBeforeRepeatAuto.AppendElement(name.AsAtom());
      }
      for (auto& name : names[1].AsSpan()) {
        rowAfterRepeatAuto.AppendElement(name.AsAtom());
      }
    }

    ComputedGridLineInfo* rowLineInfo = new ComputedGridLineInfo(
        std::move(rowLineNames), std::move(rowBeforeRepeatAuto),
        std::move(rowAfterRepeatAuto), std::move(rowNamesFollowingRepeat));
    SetProperty(GridRowLineInfo(), rowLineInfo);

    // Generate area info for explicit areas. Implicit areas are handled
    // elsewhere.
    if (!gridReflowInput.mGridStyle->mGridTemplateAreas.IsNone()) {
      auto* areas = new StyleOwnedSlice<NamedArea>(
          gridReflowInput.mGridStyle->mGridTemplateAreas.AsAreas()->areas);
      SetProperty(ExplicitNamedAreasProperty(), areas);
    } else {
      RemoveProperty(ExplicitNamedAreasProperty());
    }
  }

  if (!prevInFlow) {
    SharedGridData* sharedGridData = GetProperty(SharedGridData::Prop());
    if (!aStatus.IsFullyComplete()) {
      if (!sharedGridData) {
        sharedGridData = new SharedGridData;
        SetProperty(SharedGridData::Prop(), sharedGridData);
      }
      sharedGridData->mCols.mSizes.Clear();
      sharedGridData->mCols.mSizes.SwapElements(gridReflowInput.mCols.mSizes);
      sharedGridData->mCols.mContentBoxSize =
          gridReflowInput.mCols.mContentBoxSize;
      sharedGridData->mCols.mBaselineSubtreeAlign =
          gridReflowInput.mCols.mBaselineSubtreeAlign;
      sharedGridData->mRows.mSizes.Clear();
      sharedGridData->mRows.mSizes.SwapElements(gridReflowInput.mRows.mSizes);
      // Save the original row grid sizes and gaps so we can restore them later
      // in GridReflowInput::Initialize for the continuations.
      auto& origRowData = sharedGridData->mOriginalRowData;
      origRowData.ClearAndRetainStorage();
      origRowData.SetCapacity(sharedGridData->mRows.mSizes.Length());
      nscoord prevTrackEnd = 0;
      for (auto& sz : sharedGridData->mRows.mSizes) {
        SharedGridData::RowData data = {sz.mBase, sz.mPosition - prevTrackEnd};
        origRowData.AppendElement(data);
        prevTrackEnd = sz.mPosition + sz.mBase;
      }
      sharedGridData->mRows.mContentBoxSize =
          gridReflowInput.mRows.mContentBoxSize;
      sharedGridData->mRows.mBaselineSubtreeAlign =
          gridReflowInput.mRows.mBaselineSubtreeAlign;
      sharedGridData->mGridItems.Clear();
      sharedGridData->mGridItems.SwapElements(gridReflowInput.mGridItems);
      sharedGridData->mAbsPosItems.Clear();
      sharedGridData->mAbsPosItems.SwapElements(gridReflowInput.mAbsPosItems);

      sharedGridData->mGenerateComputedGridInfo =
          HasAnyStateBits(NS_STATE_GRID_GENERATE_COMPUTED_VALUES);
    } else if (sharedGridData && !GetNextInFlow()) {
      RemoveProperty(SharedGridData::Prop());
    }
  }

  FinishAndStoreOverflow(&aDesiredSize);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

void nsGridContainerFrame::UpdateSubgridFrameState() {
  nsFrameState oldBits = GetStateBits() & kIsSubgridBits;
  nsFrameState newBits = ComputeSelfSubgridBits();
  if (newBits != oldBits) {
    RemoveStateBits(kIsSubgridBits);
    if (!newBits) {
      RemoveProperty(Subgrid::Prop());
    } else {
      AddStateBits(newBits);
    }
  }
}

nsFrameState nsGridContainerFrame::ComputeSelfSubgridBits() const {
  // 'contain:layout/paint' makes us an "independent formatting context",
  // which prevents us from being a subgrid in this case (but not always).
  // We will also need to check our containing scroll frame for this property.
  // https://drafts.csswg.org/css-display-3/#establish-an-independent-formatting-context
  {
    const auto* display = StyleDisplay();
    if (display->IsContainLayout() || display->IsContainPaint()) {
      return nsFrameState(0);
    }
  }

  // Skip our scroll frame and such if we have it.
  // This will store the outermost frame that shares our content node:
  const nsIFrame* outerFrame = this;
  // ...and this will store that frame's parent:
  auto* parent = GetParent();
  while (parent && parent->GetContent() == GetContent()) {
    // If we find our containing frame has 'contain:layout/paint' we can't be
    // subgrid, for the same reasons as above. This can happen when this frame
    // is itself a grid item.
    const auto* parentDisplay = parent->StyleDisplay();
    if (parentDisplay->IsContainLayout() || parentDisplay->IsContainPaint()) {
      return nsFrameState(0);
    }
    outerFrame = parent;
    parent = parent->GetParent();
  }
  nsFrameState bits = nsFrameState(0);
  const nsGridContainerFrame* gridParent = do_QueryFrame(parent);
  if (gridParent) {
    // NOTE: our NS_FRAME_OUT_OF_FLOW isn't set yet so we check our style.
    bool isOutOfFlow =
        outerFrame->StyleDisplay()->IsAbsolutelyPositionedStyle();
    const auto* pos = StylePosition();
    bool isColSubgrid = pos->mGridTemplateColumns.IsSubgrid();
    // OOF subgrids don't create tracks in the parent, so we need to check that
    // it has one anyway. Otherwise we refuse to subgrid that axis since we
    // can't place grid items inside a subgrid without at least one track.
    if (isColSubgrid && isOutOfFlow) {
      bool isOrthogonal =
          GetWritingMode().IsOrthogonalTo(parent->GetWritingMode());
      auto parentAxis = isOrthogonal ? eLogicalAxisBlock : eLogicalAxisInline;
      if (!gridParent->WillHaveAtLeastOneTrackInAxis(parentAxis)) {
        isColSubgrid = false;
      }
    }
    if (isColSubgrid) {
      bits |= NS_STATE_GRID_IS_COL_SUBGRID;
    }

    bool isRowSubgrid = pos->mGridTemplateRows.IsSubgrid();
    if (isRowSubgrid && isOutOfFlow) {
      bool isOrthogonal =
          GetWritingMode().IsOrthogonalTo(parent->GetWritingMode());
      auto parentAxis = isOrthogonal ? eLogicalAxisInline : eLogicalAxisBlock;
      if (!gridParent->WillHaveAtLeastOneTrackInAxis(parentAxis)) {
        isRowSubgrid = false;
      }
    }
    if (isRowSubgrid) {
      bits |= NS_STATE_GRID_IS_ROW_SUBGRID;
    }
  }
  return bits;
}

bool nsGridContainerFrame::WillHaveAtLeastOneTrackInAxis(
    LogicalAxis aAxis) const {
  if (IsSubgrid(aAxis)) {
    // This is enforced by refusing to be a subgrid unless our parent has
    // at least one track in aAxis by ComputeSelfSubgridBits above.
    return true;
  }
  const auto* pos = StylePosition();
  const auto& gridTemplate = aAxis == eLogicalAxisBlock
                                 ? pos->mGridTemplateRows
                                 : pos->mGridTemplateColumns;
  if (gridTemplate.IsTrackList()) {
    return true;
  }
  for (nsIFrame* child : PrincipalChildList()) {
    if (!child->IsPlaceholderFrame()) {
      // A grid item triggers at least one implicit track in each axis.
      return true;
    }
  }
  if (!pos->mGridTemplateAreas.IsNone()) {
    return true;
  }
  return false;
}

void nsGridContainerFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                                nsIFrame* aPrevInFlow) {
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  if (GetStateBits() & NS_FRAME_FONT_INFLATION_CONTAINER) {
    AddStateBits(NS_FRAME_FONT_INFLATION_FLOW_ROOT);
  }

  nsFrameState bits = nsFrameState(0);
  if (MOZ_LIKELY(!aPrevInFlow)) {
    bits = ComputeSelfSubgridBits();
  } else {
    bits = aPrevInFlow->GetStateBits() &
           (kIsSubgridBits | NS_STATE_GRID_HAS_COL_SUBGRID_ITEM |
            NS_STATE_GRID_HAS_ROW_SUBGRID_ITEM);
  }
  AddStateBits(bits);
}

void nsGridContainerFrame::DidSetComputedStyle(ComputedStyle* aOldStyle) {
  nsContainerFrame::DidSetComputedStyle(aOldStyle);

  if (!aOldStyle) {
    return;  // Init() already initialized the bits.
  }
  UpdateSubgridFrameState();
}

nscoord nsGridContainerFrame::IntrinsicISize(gfxContext* aRenderingContext,
                                             IntrinsicISizeType aType) {
  // Calculate the sum of column sizes under intrinsic sizing.
  // http://dev.w3.org/csswg/css-grid/#intrinsic-sizes
  NormalizeChildLists();
  GridReflowInput state(this, *aRenderingContext);
  InitImplicitNamedAreas(state.mGridStyle);  // XXX optimize

  // The min/sz/max sizes are the input to the "repeat-to-fill" algorithm:
  // https://drafts.csswg.org/css-grid/#auto-repeat
  // They're only used for auto-repeat so we skip computing them otherwise.
  RepeatTrackSizingInput repeatSizing(state.mWM);
  if (!IsColSubgrid() && state.mColFunctions.mHasRepeatAuto) {
    repeatSizing.InitFromStyle(eLogicalAxisInline, state.mWM,
                               state.mFrame->Style());
  }
  if (!IsRowSubgrid() && state.mRowFunctions.mHasRepeatAuto &&
      !(state.mGridStyle->mGridAutoFlow & StyleGridAutoFlow::ROW)) {
    // Only 'grid-auto-flow:column' can create new implicit columns, so that's
    // the only case where our block-size can affect the number of columns.
    repeatSizing.InitFromStyle(eLogicalAxisBlock, state.mWM,
                               state.mFrame->Style());
  }

  Grid grid;
  if (MOZ_LIKELY(!IsSubgrid())) {
    grid.PlaceGridItems(state, repeatSizing);  // XXX optimize
  } else {
    auto* subgrid = GetProperty(Subgrid::Prop());
    state.mGridItems = subgrid->mGridItems;
    state.mAbsPosItems = subgrid->mAbsPosItems;
    grid.mGridColEnd = subgrid->mGridColEnd;
    grid.mGridRowEnd = subgrid->mGridRowEnd;
  }
  if (grid.mGridColEnd == 0) {
    return nscoord(0);
  }

  auto constraint = aType == nsLayoutUtils::MIN_ISIZE
                        ? SizingConstraint::MinContent
                        : SizingConstraint::MaxContent;
  state.CalculateTrackSizesForAxis(eLogicalAxisInline, grid,
                                   NS_UNCONSTRAINEDSIZE, constraint);

  if (MOZ_LIKELY(!IsSubgrid())) {
    nscoord length = 0;
    for (const TrackSize& sz : state.mCols.mSizes) {
      length += sz.mBase;
    }
    return length + state.mCols.SumOfGridGaps();
  }
  const auto& last = state.mCols.mSizes.LastElement();
  return last.mPosition + last.mBase;
}

nscoord nsGridContainerFrame::GetMinISize(gfxContext* aRC) {
  auto* f = static_cast<nsGridContainerFrame*>(FirstContinuation());
  if (f != this) {
    return f->GetMinISize(aRC);
  }

  DISPLAY_MIN_INLINE_SIZE(this, mCachedMinISize);
  if (mCachedMinISize == NS_INTRINSIC_ISIZE_UNKNOWN) {
    mCachedMinISize = StyleDisplay()->IsContainSize()
                          ? 0
                          : IntrinsicISize(aRC, nsLayoutUtils::MIN_ISIZE);
  }
  return mCachedMinISize;
}

nscoord nsGridContainerFrame::GetPrefISize(gfxContext* aRC) {
  auto* f = static_cast<nsGridContainerFrame*>(FirstContinuation());
  if (f != this) {
    return f->GetPrefISize(aRC);
  }

  DISPLAY_PREF_INLINE_SIZE(this, mCachedPrefISize);
  if (mCachedPrefISize == NS_INTRINSIC_ISIZE_UNKNOWN) {
    mCachedPrefISize = StyleDisplay()->IsContainSize()
                           ? 0
                           : IntrinsicISize(aRC, nsLayoutUtils::PREF_ISIZE);
  }
  return mCachedPrefISize;
}

void nsGridContainerFrame::MarkIntrinsicISizesDirty() {
  mCachedMinISize = NS_INTRINSIC_ISIZE_UNKNOWN;
  mCachedPrefISize = NS_INTRINSIC_ISIZE_UNKNOWN;
  for (auto& perAxisBaseline : mBaseline) {
    for (auto& baseline : perAxisBaseline) {
      baseline = NS_INTRINSIC_ISIZE_UNKNOWN;
    }
  }
  nsContainerFrame::MarkIntrinsicISizesDirty();
}

void nsGridContainerFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                            const nsDisplayListSet& aLists) {
  DisplayBorderBackgroundOutline(aBuilder, aLists);
  if (GetPrevInFlow()) {
    DisplayOverflowContainers(aBuilder, aLists);
  }

  // Our children are all grid-level boxes, which behave the same as
  // inline-blocks in painting, so their borders/backgrounds all go on
  // the BlockBorderBackgrounds list.
  typedef CSSOrderAwareFrameIterator::OrderState OrderState;
  OrderState order =
      HasAnyStateBits(NS_STATE_GRID_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER)
          ? OrderState::eKnownOrdered
          : OrderState::eKnownUnordered;
  CSSOrderAwareFrameIterator iter(
      this, kPrincipalList, CSSOrderAwareFrameIterator::eIncludeAll, order);
  for (; !iter.AtEnd(); iter.Next()) {
    nsIFrame* child = *iter;
    BuildDisplayListForChild(aBuilder, child, aLists,
                             ::GetDisplayFlagsForGridItem(child));
  }
}

bool nsGridContainerFrame::DrainSelfOverflowList() {
  // Unlike nsContainerFrame::DrainSelfOverflowList we need to merge these lists
  // so that the resulting mFrames is in document content order.
  // NOTE: nsContainerFrame::AppendFrames/InsertFrames calls this method and
  // there are also direct calls from the fctor (FindAppendPrevSibling).
  AutoFrameListPtr overflowFrames(PresContext(), StealOverflowFrames());
  if (overflowFrames) {
    MergeSortedFrameLists(mFrames, *overflowFrames, GetContent());
    // We set a frame bit to push them again in Reflow() to avoid creating
    // multiple grid items per grid container fragment for the same content.
    AddStateBits(NS_STATE_GRID_HAS_CHILD_NIFS);
    return true;
  }
  return false;
}

void nsGridContainerFrame::AppendFrames(ChildListID aListID,
                                        nsFrameList& aFrameList) {
  NoteNewChildren(aListID, aFrameList);
  nsContainerFrame::AppendFrames(aListID, aFrameList);
}

void nsGridContainerFrame::InsertFrames(
    ChildListID aListID, nsIFrame* aPrevFrame,
    const nsLineList::iterator* aPrevFrameLine, nsFrameList& aFrameList) {
  NoteNewChildren(aListID, aFrameList);
  nsContainerFrame::InsertFrames(aListID, aPrevFrame, aPrevFrameLine,
                                 aFrameList);
}

void nsGridContainerFrame::RemoveFrame(ChildListID aListID,
                                       nsIFrame* aOldFrame) {
#ifdef DEBUG
  ChildListIDs supportedLists = {kAbsoluteList, kFixedList, kPrincipalList,
                                 kNoReflowPrincipalList};
  // We don't handle the kBackdropList frames in any way, but it only contains
  // a placeholder for ::backdrop which is OK to not reflow (for now anyway).
  supportedLists += kBackdropList;
  MOZ_ASSERT(supportedLists.contains(aListID), "unexpected child list");

  // Note that kPrincipalList doesn't mean aOldFrame must be on that list.
  // It can also be on kOverflowList, in which case it might be a pushed
  // item, and if it's the only pushed item our DID_PUSH_ITEMS bit will lie.
  if (aListID == kPrincipalList && !aOldFrame->GetPrevInFlow()) {
    // Since the bit may lie, set the mDidPushItemsBitMayLie value to true for
    // ourself and for all our contiguous previous-in-flow
    // nsGridContainerFrames.
    nsGridContainerFrame* frameThatMayLie = this;
    do {
      frameThatMayLie->mDidPushItemsBitMayLie = true;
      frameThatMayLie =
          static_cast<nsGridContainerFrame*>(frameThatMayLie->GetPrevInFlow());
    } while (frameThatMayLie);
  }
#endif

  nsContainerFrame::RemoveFrame(aListID, aOldFrame);
}

StyleAlignFlags nsGridContainerFrame::CSSAlignmentForAbsPosChild(
    const ReflowInput& aChildRI, LogicalAxis aLogicalAxis) const {
  MOZ_ASSERT(aChildRI.mFrame->IsAbsolutelyPositioned(),
             "This method should only be called for abspos children");

  StyleAlignFlags alignment =
      (aLogicalAxis == eLogicalAxisInline)
          ? aChildRI.mStylePosition->UsedJustifySelf(Style())._0
          : aChildRI.mStylePosition->UsedAlignSelf(Style())._0;

  // Extract and strip the flag bits
  StyleAlignFlags alignmentFlags = alignment & StyleAlignFlags::FLAG_BITS;
  alignment &= ~StyleAlignFlags::FLAG_BITS;

  if (alignment == StyleAlignFlags::NORMAL) {
    // "the 'normal' keyword behaves as 'start' on replaced
    // absolutely-positioned boxes, and behaves as 'stretch' on all other
    // absolutely-positioned boxes."
    // https://drafts.csswg.org/css-align/#align-abspos
    // https://drafts.csswg.org/css-align/#justify-abspos
    alignment = aChildRI.mFrame->IsFrameOfType(nsIFrame::eReplaced)
                    ? StyleAlignFlags::START
                    : StyleAlignFlags::STRETCH;
  } else if (alignment == StyleAlignFlags::FLEX_START) {
    alignment = StyleAlignFlags::START;
  } else if (alignment == StyleAlignFlags::FLEX_END) {
    alignment = StyleAlignFlags::END;
  } else if (alignment == StyleAlignFlags::LEFT ||
             alignment == StyleAlignFlags::RIGHT) {
    if (aLogicalAxis == eLogicalAxisInline) {
      const bool isLeft = (alignment == StyleAlignFlags::LEFT);
      WritingMode wm = GetWritingMode();
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

nscoord nsGridContainerFrame::SynthesizeBaseline(
    const FindItemInGridOrderResult& aGridOrderItem, LogicalAxis aAxis,
    BaselineSharingGroup aGroup, const nsSize& aCBPhysicalSize, nscoord aCBSize,
    WritingMode aCBWM) {
  if (MOZ_UNLIKELY(!aGridOrderItem.mItem)) {
    // No item in this fragment - synthesize a baseline from our border-box.
    return ::SynthesizeBaselineFromBorderBox(aGroup, aCBWM, aCBSize);
  }
  auto GetBBaseline = [](BaselineSharingGroup aGroup, WritingMode aWM,
                         const nsIFrame* aFrame, nscoord* aBaseline) {
    return aGroup == BaselineSharingGroup::First
               ? nsLayoutUtils::GetFirstLineBaseline(aWM, aFrame, aBaseline)
               : nsLayoutUtils::GetLastLineBaseline(aWM, aFrame, aBaseline);
  };
  nsIFrame* child = aGridOrderItem.mItem->mFrame;
  nsGridContainerFrame* grid = do_QueryFrame(child);
  auto childWM = child->GetWritingMode();
  bool isOrthogonal = aCBWM.IsOrthogonalTo(childWM);
  nscoord baseline;
  nscoord start;
  nscoord size;
  if (aAxis == eLogicalAxisBlock) {
    start = child->GetLogicalNormalPosition(aCBWM, aCBPhysicalSize).B(aCBWM);
    size = child->BSize(aCBWM);
    if (grid && aGridOrderItem.mIsInEdgeTrack) {
      isOrthogonal ? grid->GetIBaseline(aGroup, &baseline)
                   : grid->GetBBaseline(aGroup, &baseline);
    } else if (!isOrthogonal && aGridOrderItem.mIsInEdgeTrack) {
      baseline =
          child->BaselineBOffset(childWM, aGroup, AlignmentContext::Grid);
    } else {
      baseline = ::SynthesizeBaselineFromBorderBox(aGroup, childWM, size);
    }
  } else {
    start = child->GetLogicalNormalPosition(aCBWM, aCBPhysicalSize).I(aCBWM);
    size = child->ISize(aCBWM);
    if (grid && aGridOrderItem.mIsInEdgeTrack) {
      isOrthogonal ? grid->GetBBaseline(aGroup, &baseline)
                   : grid->GetIBaseline(aGroup, &baseline);
    } else if (isOrthogonal && aGridOrderItem.mIsInEdgeTrack &&
               GetBBaseline(aGroup, childWM, child, &baseline)) {
      if (aGroup == BaselineSharingGroup::Last) {
        baseline = size - baseline;  // convert to distance from border-box end
      }
    } else {
      baseline = ::SynthesizeBaselineFromBorderBox(aGroup, childWM, size);
    }
  }
  return aGroup == BaselineSharingGroup::First
             ? start + baseline
             : aCBSize - start - size + baseline;
}

void nsGridContainerFrame::CalculateBaselines(
    BaselineSet aBaselineSet, CSSOrderAwareFrameIterator* aIter,
    const nsTArray<GridItemInfo>* aGridItems, const Tracks& aTracks,
    uint32_t aFragmentStartTrack, uint32_t aFirstExcludedTrack, WritingMode aWM,
    const nsSize& aCBPhysicalSize, nscoord aCBBorderPaddingStart,
    nscoord aCBBorderPaddingEnd, nscoord aCBSize) {
  const auto axis = aTracks.mAxis;
  auto firstBaseline = aTracks.mBaseline[BaselineSharingGroup::First];
  if (!(aBaselineSet & BaselineSet::eFirst)) {
    mBaseline[axis][BaselineSharingGroup::First] =
        ::SynthesizeBaselineFromBorderBox(BaselineSharingGroup::First, aWM,
                                          aCBSize);
  } else if (firstBaseline == NS_INTRINSIC_ISIZE_UNKNOWN) {
    FindItemInGridOrderResult gridOrderFirstItem = FindFirstItemInGridOrder(
        *aIter, *aGridItems,
        axis == eLogicalAxisBlock ? &GridArea::mRows : &GridArea::mCols,
        axis == eLogicalAxisBlock ? &GridArea::mCols : &GridArea::mRows,
        aFragmentStartTrack);
    mBaseline[axis][BaselineSharingGroup::First] = SynthesizeBaseline(
        gridOrderFirstItem, axis, BaselineSharingGroup::First, aCBPhysicalSize,
        aCBSize, aWM);
  } else {
    // We have a 'first baseline' group in the start track in this fragment.
    // Convert it from track to grid container border-box coordinates.
    MOZ_ASSERT(!aGridItems->IsEmpty());
    nscoord gapBeforeStartTrack =
        aFragmentStartTrack == 0
            ? aTracks.GridLineEdge(aFragmentStartTrack,
                                   GridLineSide::AfterGridGap)
            : nscoord(0);  // no content gap at start of fragment
    mBaseline[axis][BaselineSharingGroup::First] =
        aCBBorderPaddingStart + gapBeforeStartTrack + firstBaseline;
  }

  auto lastBaseline = aTracks.mBaseline[BaselineSharingGroup::Last];
  if (!(aBaselineSet & BaselineSet::eLast)) {
    mBaseline[axis][BaselineSharingGroup::Last] =
        ::SynthesizeBaselineFromBorderBox(BaselineSharingGroup::Last, aWM,
                                          aCBSize);
  } else if (lastBaseline == NS_INTRINSIC_ISIZE_UNKNOWN) {
    // For finding items for the 'last baseline' we need to create a reverse
    // iterator ('aIter' is the forward iterator from the GridReflowInput).
    using Iter = ReverseCSSOrderAwareFrameIterator;
    auto orderState = aIter->ItemsAreAlreadyInOrder()
                          ? Iter::OrderState::eKnownOrdered
                          : Iter::OrderState::eKnownUnordered;
    Iter iter(this, kPrincipalList, Iter::ChildFilter::eSkipPlaceholders,
              orderState);
    iter.SetItemCount(aGridItems->Length());
    FindItemInGridOrderResult gridOrderLastItem = FindLastItemInGridOrder(
        iter, *aGridItems,
        axis == eLogicalAxisBlock ? &GridArea::mRows : &GridArea::mCols,
        axis == eLogicalAxisBlock ? &GridArea::mCols : &GridArea::mRows,
        aFragmentStartTrack, aFirstExcludedTrack);
    mBaseline[axis][BaselineSharingGroup::Last] =
        SynthesizeBaseline(gridOrderLastItem, axis, BaselineSharingGroup::Last,
                           aCBPhysicalSize, aCBSize, aWM);
  } else {
    // We have a 'last baseline' group in the end track in this fragment.
    // Convert it from track to grid container border-box coordinates.
    MOZ_ASSERT(!aGridItems->IsEmpty());
    auto borderBoxStartToEndOfEndTrack =
        aCBBorderPaddingStart +
        aTracks.GridLineEdge(aFirstExcludedTrack, GridLineSide::BeforeGridGap) -
        aTracks.GridLineEdge(aFragmentStartTrack, GridLineSide::BeforeGridGap);
    mBaseline[axis][BaselineSharingGroup::Last] =
        (aCBSize - borderBoxStartToEndOfEndTrack) + lastBaseline;
  }
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsGridContainerFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(NS_LITERAL_STRING("GridContainer"), aResult);
}

void nsGridContainerFrame::ExtraContainerFrameInfo(nsACString& aTo) const {
  if (const void* const subgrid = GetProperty(Subgrid::Prop())) {
    aTo += nsPrintfCString(" [subgrid=%p]", subgrid);
  }
}

#endif

void nsGridContainerFrame::NoteNewChildren(ChildListID aListID,
                                           const nsFrameList& aFrameList) {
#ifdef DEBUG
  ChildListIDs supportedLists = {kAbsoluteList, kFixedList, kPrincipalList,
                                 kNoReflowPrincipalList};
  // We don't handle the kBackdropList frames in any way, but it only contains
  // a placeholder for ::backdrop which is OK to not reflow (for now anyway).
  supportedLists += kBackdropList;
  MOZ_ASSERT(supportedLists.contains(aListID), "unexpected child list");
#endif

  mozilla::PresShell* presShell = PresShell();
  for (auto pif = GetPrevInFlow(); pif; pif = pif->GetPrevInFlow()) {
    if (aListID == kPrincipalList) {
      pif->AddStateBits(NS_STATE_GRID_DID_PUSH_ITEMS);
    }
    presShell->FrameNeedsReflow(pif, IntrinsicDirty::TreeChange,
                                NS_FRAME_IS_DIRTY);
  }
}

/* static */ nsGridContainerFrame::FindItemInGridOrderResult
nsGridContainerFrame::FindFirstItemInGridOrder(
    CSSOrderAwareFrameIterator& aIter, const nsTArray<GridItemInfo>& aGridItems,
    LineRange GridArea::*aMajor, LineRange GridArea::*aMinor,
    uint32_t aFragmentStartTrack) {
  FindItemInGridOrderResult result = {nullptr, false};
  uint32_t minMajor = kTranslatedMaxLine + 1;
  uint32_t minMinor = kTranslatedMaxLine + 1;
  aIter.Reset();
  for (; !aIter.AtEnd(); aIter.Next()) {
    const GridItemInfo& item = aGridItems[aIter.ItemIndex()];
    if ((item.mArea.*aMajor).mEnd <= aFragmentStartTrack) {
      continue;  // item doesn't span any track in this fragment
    }
    uint32_t major = (item.mArea.*aMajor).mStart;
    uint32_t minor = (item.mArea.*aMinor).mStart;
    if (major < minMajor || (major == minMajor && minor < minMinor)) {
      minMajor = major;
      minMinor = minor;
      result.mItem = &item;
      result.mIsInEdgeTrack = major == 0U;
    }
  }
  return result;
}

/* static */ nsGridContainerFrame::FindItemInGridOrderResult
nsGridContainerFrame::FindLastItemInGridOrder(
    ReverseCSSOrderAwareFrameIterator& aIter,
    const nsTArray<GridItemInfo>& aGridItems, LineRange GridArea::*aMajor,
    LineRange GridArea::*aMinor, uint32_t aFragmentStartTrack,
    uint32_t aFirstExcludedTrack) {
  FindItemInGridOrderResult result = {nullptr, false};
  int32_t maxMajor = -1;
  int32_t maxMinor = -1;
  aIter.Reset();
  int32_t lastMajorTrack = int32_t(aFirstExcludedTrack) - 1;
  for (; !aIter.AtEnd(); aIter.Next()) {
    const GridItemInfo& item = aGridItems[aIter.ItemIndex()];
    // Subtract 1 from the end line to get the item's last track index.
    int32_t major = (item.mArea.*aMajor).mEnd - 1;
    // Currently, this method is only called with aFirstExcludedTrack ==
    // the first track in the next fragment, so we take the opportunity
    // to assert this item really belongs to this fragment.
    MOZ_ASSERT((item.mArea.*aMajor).mStart < aFirstExcludedTrack,
               "found an item that belongs to some later fragment");
    if (major < int32_t(aFragmentStartTrack)) {
      continue;  // item doesn't span any track in this fragment
    }
    int32_t minor = (item.mArea.*aMinor).mEnd - 1;
    MOZ_ASSERT(minor >= 0 && major >= 0, "grid item must have span >= 1");
    if (major > maxMajor || (major == maxMajor && minor > maxMinor)) {
      maxMajor = major;
      maxMinor = minor;
      result.mItem = &item;
      result.mIsInEdgeTrack = major == lastMajorTrack;
    }
  }
  return result;
}

nsGridContainerFrame::UsedTrackSizes* nsGridContainerFrame::GetUsedTrackSizes()
    const {
  return GetProperty(UsedTrackSizes::Prop());
}

void nsGridContainerFrame::StoreUsedTrackSizes(
    LogicalAxis aAxis, const nsTArray<TrackSize>& aSizes) {
  auto* uts = GetUsedTrackSizes();
  if (!uts) {
    uts = new UsedTrackSizes();
    SetProperty(UsedTrackSizes::Prop(), uts);
  }
  uts->mSizes[aAxis] = aSizes;
  uts->mCanResolveLineRangeSize[aAxis] = true;
  // XXX is resetting these bits necessary?
  for (auto& sz : uts->mSizes[aAxis]) {
    sz.mState &= ~(TrackSize::eFrozen | TrackSize::eSkipGrowUnlimited |
                   TrackSize::eInfinitelyGrowable);
  }
}

#ifdef DEBUG
void nsGridContainerFrame::SetInitialChildList(ChildListID aListID,
                                               nsFrameList& aChildList) {
  ChildListIDs supportedLists = {kAbsoluteList, kFixedList, kPrincipalList};
  // We don't handle the kBackdropList frames in any way, but it only contains
  // a placeholder for ::backdrop which is OK to not reflow (for now anyway).
  supportedLists += kBackdropList;
  MOZ_ASSERT(supportedLists.contains(aListID), "unexpected child list");

  return nsContainerFrame::SetInitialChildList(aListID, aChildList);
}

void nsGridContainerFrame::SanityCheckGridItemsBeforeReflow() const {
  ChildListIDs absLists = {kAbsoluteList, kFixedList, kOverflowContainersList,
                           kExcessOverflowContainersList};
  ChildListIDs itemLists = {kPrincipalList, kOverflowList};
  for (const nsIFrame* f = this; f; f = f->GetNextInFlow()) {
    MOZ_ASSERT(!f->HasAnyStateBits(NS_STATE_GRID_DID_PUSH_ITEMS),
               "At start of reflow, we should've pulled items back from all "
               "NIFs and cleared NS_STATE_GRID_DID_PUSH_ITEMS in the process");
    for (nsIFrame::ChildListIterator childLists(f); !childLists.IsDone();
         childLists.Next()) {
      if (!itemLists.contains(childLists.CurrentID())) {
        MOZ_ASSERT(absLists.contains(childLists.CurrentID()) ||
                       childLists.CurrentID() == kBackdropList,
                   "unexpected non-empty child list");
        continue;
      }
      for (auto child : childLists.CurrentList()) {
        MOZ_ASSERT(f == this || child->GetPrevInFlow(),
                   "all pushed items must be pulled up before reflow");
      }
    }
  }
  // If we have a prev-in-flow, each of its children's next-in-flow
  // should be one of our children or be null.
  const auto pif = static_cast<nsGridContainerFrame*>(GetPrevInFlow());
  if (pif) {
    const nsFrameList* oc = GetPropTableFrames(OverflowContainersProperty());
    const nsFrameList* eoc =
        GetPropTableFrames(ExcessOverflowContainersProperty());
    const nsFrameList* pifEOC =
        pif->GetPropTableFrames(ExcessOverflowContainersProperty());
    for (const nsIFrame* child : pif->GetChildList(kPrincipalList)) {
      const nsIFrame* childNIF = child->GetNextInFlow();
      MOZ_ASSERT(!childNIF || mFrames.ContainsFrame(childNIF) ||
                 (pifEOC && pifEOC->ContainsFrame(childNIF)) ||
                 (oc && oc->ContainsFrame(childNIF)) ||
                 (eoc && eoc->ContainsFrame(childNIF)));
    }
  }
}

void nsGridContainerFrame::TrackSize::Dump() const {
  printf("mPosition=%d mBase=%d mLimit=%d", mPosition, mBase, mLimit);

  printf(" min:");
  if (mState & eAutoMinSizing) {
    printf("auto ");
  } else if (mState & eMinContentMinSizing) {
    printf("min-content ");
  } else if (mState & eMaxContentMinSizing) {
    printf("max-content ");
  }

  printf(" max:");
  if (mState & eAutoMaxSizing) {
    printf("auto ");
  } else if (mState & eMinContentMaxSizing) {
    printf("min-content ");
  } else if (mState & eMaxContentMaxSizing) {
    printf("max-content ");
  } else if (mState & eFlexMaxSizing) {
    printf("flex ");
  }

  if (mState & eFrozen) {
    printf("frozen ");
  }
  if (mState & eModified) {
    printf("modified ");
  }
  if (mState & eBreakBefore) {
    printf("break-before ");
  }
}

#endif  // DEBUG

nsGridContainerFrame* nsGridContainerFrame::GetGridContainerFrame(
    nsIFrame* aFrame) {
  nsGridContainerFrame* gridFrame = nullptr;

  if (aFrame) {
    nsIFrame* inner = aFrame;
    if (MOZ_UNLIKELY(aFrame->IsFieldSetFrame())) {
      inner = static_cast<nsFieldSetFrame*>(aFrame)->GetInner();
    }
    inner = inner->GetContentInsertionFrame();
    nsIFrame* possibleGridFrame = inner ? inner : aFrame;
    gridFrame = possibleGridFrame->IsGridContainerFrame()
                    ? static_cast<nsGridContainerFrame*>(possibleGridFrame)
                    : nullptr;
  }
  return gridFrame;
}

nsGridContainerFrame* nsGridContainerFrame::GetGridFrameWithComputedInfo(
    nsIFrame* aFrame) {
  nsGridContainerFrame* gridFrame = GetGridContainerFrame(aFrame);
  if (!gridFrame) {
    return nullptr;
  }

  auto HasComputedInfo = [](const nsGridContainerFrame& aFrame) -> bool {
    return aFrame.HasProperty(GridColTrackInfo()) &&
           aFrame.HasProperty(GridRowTrackInfo()) &&
           aFrame.HasProperty(GridColumnLineInfo()) &&
           aFrame.HasProperty(GridRowLineInfo());
  };

  if (HasComputedInfo(*gridFrame)) {
    return gridFrame;
  }

  // Trigger a reflow that generates additional grid property data.
  // Hold onto aFrame while we do this, in case reflow destroys it.
  AutoWeakFrame weakFrameRef(gridFrame);

  RefPtr<mozilla::PresShell> presShell = gridFrame->PresShell();
  gridFrame->AddStateBits(NS_STATE_GRID_GENERATE_COMPUTED_VALUES);
  presShell->FrameNeedsReflow(gridFrame, IntrinsicDirty::Resize,
                              NS_FRAME_IS_DIRTY);
  presShell->FlushPendingNotifications(FlushType::Layout);

  // If the weakFrameRef is no longer valid, then we must bail out.
  if (!weakFrameRef.IsAlive()) {
    return nullptr;
  }

  // This can happen if for some reason we ended up not reflowing, like in print
  // preview under some circumstances.
  if (MOZ_UNLIKELY(!HasComputedInfo(*gridFrame))) {
    return nullptr;
  }

  return gridFrame;
}
