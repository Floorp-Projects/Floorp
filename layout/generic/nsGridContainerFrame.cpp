/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: grid | inline-grid" */

#include "nsGridContainerFrame.h"

#include <algorithm> // for std::stable_sort
#include <limits>
#include "mozilla/Maybe.h"
#include "mozilla/PodOperations.h" // for PodZero
#include "nsAbsoluteContainingBlock.h"
#include "nsAlgorithm.h" // for clamped()
#include "nsAutoPtr.h"
#include "nsCSSAnonBoxes.h"
#include "nsDataHashtable.h"
#include "nsDisplayList.h"
#include "nsHashKeys.h"
#include "nsIFrameInlines.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsReadableUtils.h"
#include "nsRuleNode.h"
#include "nsStyleContext.h"

using namespace mozilla;
typedef nsAbsoluteContainingBlock::AbsPosReflowFlags AbsPosReflowFlags;
typedef nsGridContainerFrame::TrackSize TrackSize;
const uint32_t nsGridContainerFrame::kTranslatedMaxLine =
  uint32_t(nsStyleGridLine::kMaxLine - nsStyleGridLine::kMinLine);
const uint32_t nsGridContainerFrame::kAutoLine = kTranslatedMaxLine + 3457U;

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(TrackSize::StateBits)

class nsGridContainerFrame::GridItemCSSOrderIterator
{
public:
  enum OrderState { eUnknownOrder, eKnownOrdered, eKnownUnordered };
  enum ChildFilter { eSkipPlaceholders, eIncludeAll };
  GridItemCSSOrderIterator(nsIFrame* aGridContainer,
                           nsIFrame::ChildListID aListID,
                           ChildFilter aFilter = eSkipPlaceholders,
                           OrderState aState = eUnknownOrder)
    : mChildren(aGridContainer->GetChildList(aListID))
    , mArrayIndex(0)
    , mGridItemIndex(0)
    , mSkipPlaceholders(aFilter == eSkipPlaceholders)
#ifdef DEBUG
    , mGridContainer(aGridContainer)
    , mListID(aListID)
#endif
  {
    size_t count = 0;
    bool isOrdered = aState != eKnownUnordered;
    if (aState == eUnknownOrder) {
      auto maxOrder = std::numeric_limits<int32_t>::min();
      for (nsFrameList::Enumerator e(mChildren); !e.AtEnd(); e.Next()) {
        ++count;
        int32_t order = e.get()->StylePosition()->mOrder;
        if (order < maxOrder) {
          isOrdered = false;
          break;
        }
        maxOrder = order;
      }
    }
    if (isOrdered) {
      mEnumerator.emplace(mChildren);
    } else {
      count *= 2; // XXX somewhat arbitrary estimate for now...
      mArray.emplace(count);
      for (nsFrameList::Enumerator e(mChildren); !e.AtEnd(); e.Next()) {
        mArray->AppendElement(e.get());
      }
      // XXX replace this with nsTArray::StableSort when bug 1147091 is fixed.
      std::stable_sort(mArray->begin(), mArray->end(), IsCSSOrderLessThan);
    }

    if (mSkipPlaceholders) {
      SkipPlaceholders();
    }
  }

  nsIFrame* operator*() const
  {
    MOZ_ASSERT(!AtEnd());
    if (mEnumerator) {
      return mEnumerator->get();
    }
    return (*mArray)[mArrayIndex];
  }

  /**
   * Return the child index of the current item, placeholders not counted.
   * It's forbidden to call this method when the current frame is placeholder.
   */
  size_t GridItemIndex() const
  {
    MOZ_ASSERT(!AtEnd());
    MOZ_ASSERT((**this)->GetType() != nsGkAtoms::placeholderFrame,
               "MUST not call this when at a placeholder");
    return mGridItemIndex;
  }

  /**
   * Skip over placeholder children.
   */
  void SkipPlaceholders()
  {
    if (mEnumerator) {
      for (; !mEnumerator->AtEnd(); mEnumerator->Next()) {
        nsIFrame* child = mEnumerator->get();
        if (child->GetType() != nsGkAtoms::placeholderFrame) {
          return;
        }
      }
    } else {
      for (; mArrayIndex < mArray->Length(); ++mArrayIndex) {
        nsIFrame* child = (*mArray)[mArrayIndex];
        if (child->GetType() != nsGkAtoms::placeholderFrame) {
          return;
        }
      }
    }
  }

  bool AtEnd() const
  {
    MOZ_ASSERT(mEnumerator || mArrayIndex <= mArray->Length());
    return mEnumerator ? mEnumerator->AtEnd() : mArrayIndex >= mArray->Length();
  }

  void Next()
  {
#ifdef DEBUG
    MOZ_ASSERT(!AtEnd());
    nsFrameList list = mGridContainer->GetChildList(mListID);
    MOZ_ASSERT(list.FirstChild() == mChildren.FirstChild() &&
               list.LastChild() == mChildren.LastChild(),
               "the list of child frames must not change while iterating!");
#endif
    if (mSkipPlaceholders ||
        (**this)->GetType() != nsGkAtoms::placeholderFrame) {
      ++mGridItemIndex;
    }
    if (mEnumerator) {
      mEnumerator->Next();
    } else {
      ++mArrayIndex;
    }
    if (mSkipPlaceholders) {
      SkipPlaceholders();
    }
  }

  void Reset(ChildFilter aFilter = eSkipPlaceholders)
  {
    if (mEnumerator) {
      mEnumerator.reset();
      mEnumerator.emplace(mChildren);
    } else {
      mArrayIndex = 0;
    }
    mGridItemIndex = 0;
    mSkipPlaceholders = aFilter == eSkipPlaceholders;
    if (mSkipPlaceholders) {
      SkipPlaceholders();
    }
  }

  bool ItemsAreAlreadyInOrder() const { return mEnumerator.isSome(); }

private:
  static bool IsCSSOrderLessThan(nsIFrame* const& a, nsIFrame* const& b)
    { return a->StylePosition()->mOrder < b->StylePosition()->mOrder; }

  nsFrameList mChildren;
  // Used if child list is already in ascending 'order'.
  Maybe<nsFrameList::Enumerator> mEnumerator;
  // Used if child list is *not* in ascending 'order'.
  Maybe<nsTArray<nsIFrame*>> mArray;
  size_t mArrayIndex;
  // The index of the current grid item (placeholders excluded).
  size_t mGridItemIndex;
  // Skip placeholder children in the iteration?
  bool mSkipPlaceholders;
#ifdef DEBUG
  nsIFrame* mGridContainer;
  nsIFrame::ChildListID mListID;
#endif
};

/**
 * Utility class to find line names.  It provides an interface to lookup line
 * names with a dynamic number of repeat(auto-fill/fit) tracks taken into
 * account.
 */
class MOZ_STACK_CLASS nsGridContainerFrame::LineNameMap
{
public:
  /**
   * Create a LineNameMap.
   * @param aGridTemplate is the grid-template-rows/columns data for this axis
   * @param aNumRepeatTracks the number of actual tracks associated with
   *   a repeat(auto-fill/fit) track (zero or more), or zero if there is no
   *   specified repeat(auto-fill/fit) track
   */
  LineNameMap(const nsStyleGridTemplate& aGridTemplate,
              uint32_t                   aNumRepeatTracks)
    : mLineNameLists(aGridTemplate.mLineNameLists)
    , mRepeatAutoLineNameListBefore(aGridTemplate.mRepeatAutoLineNameListBefore)
    , mRepeatAutoLineNameListAfter(aGridTemplate.mRepeatAutoLineNameListAfter)
    , mRepeatAutoStart(aGridTemplate.HasRepeatAuto() ?
                         aGridTemplate.mRepeatAutoIndex : 0)
    , mRepeatAutoEnd(mRepeatAutoStart + aNumRepeatTracks)
    , mRepeatEndDelta(aGridTemplate.HasRepeatAuto() ?
                        int32_t(aNumRepeatTracks) - 1 :
                        0)
    , mTemplateLinesEnd(mLineNameLists.Length() + mRepeatEndDelta)
    , mHasRepeatAuto(aGridTemplate.HasRepeatAuto())
  {
    MOZ_ASSERT(mHasRepeatAuto || aNumRepeatTracks == 0);
    MOZ_ASSERT(mRepeatAutoStart <= mLineNameLists.Length());
    MOZ_ASSERT(!mHasRepeatAuto || mLineNameLists.Length() >= 2);
  }

  /**
   * Find the aNth occurrence of aName, searching forward if aNth is positive,
   * and in reverse if aNth is negative (aNth == 0 is invalid), starting from
   * aFromIndex (not inclusive), and return a 1-based line number.
   * Also take into account there is an unconditional match at aImplicitLine
   * unless it's zero.
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
  uint32_t FindNamedLine(const nsString& aName, int32_t* aNth,
                         uint32_t aFromIndex, uint32_t aImplicitLine) const
  {
    MOZ_ASSERT(aNth && *aNth != 0);
    if (*aNth > 0) {
      return FindLine(aName, aNth, aFromIndex, aImplicitLine);
    }
    int32_t nth = -*aNth;
    int32_t line = RFindLine(aName, &nth, aFromIndex, aImplicitLine);
    *aNth = -nth;
    return line;
  }

private:
  /**
   * @see FindNamedLine, this function searches forward.
   */
  uint32_t FindLine(const nsString& aName, int32_t* aNth,
                    uint32_t aFromIndex, uint32_t aImplicitLine) const
  {
    MOZ_ASSERT(aNth && *aNth > 0);
    int32_t nth = *aNth;
    const uint32_t end = mTemplateLinesEnd;
    uint32_t line;
    uint32_t i = aFromIndex;
    for (; i < end; i = line) {
      line = i + 1;
      if (line == aImplicitLine || Contains(i, aName)) {
        if (--nth == 0) {
          return line;
        }
      }
    }
    if (aImplicitLine > i) {
      // aImplicitLine is after the lines we searched above so it's last.
      // (grid-template-areas has more tracks than grid-template-[rows|columns])
      if (--nth == 0) {
        return aImplicitLine;
      }
    }
    MOZ_ASSERT(nth > 0, "should have returned a valid line above already");
    *aNth = nth;
    return 0;
  }

  /**
   * @see FindNamedLine, this function searches in reverse.
   */
  uint32_t RFindLine(const nsString& aName, int32_t* aNth,
                     uint32_t aFromIndex, uint32_t aImplicitLine) const
  {
    MOZ_ASSERT(aNth && *aNth > 0);
    if (MOZ_UNLIKELY(aFromIndex == 0)) {
      return 0; // There are no named lines beyond the start of the explicit grid.
    }
    --aFromIndex; // (shift aFromIndex so we can treat it as inclusive)
    int32_t nth = *aNth;
    // The implicit line may be beyond the explicit grid so we match
    // this line first if it's within the mTemplateLinesEnd..aFromIndex range.
    const uint32_t end = mTemplateLinesEnd;
    if (aImplicitLine > end && aImplicitLine < aFromIndex) {
      if (--nth == 0) {
        return aImplicitLine;
      }
    }
    for (uint32_t i = std::min(aFromIndex, end); i; --i) {
      if (i == aImplicitLine || Contains(i - 1, aName)) {
        if (--nth == 0) {
          return i;
        }
      }
    }
    MOZ_ASSERT(nth > 0, "should have returned a valid line above already");
    *aNth = nth;
    return 0;
  }

  // Return true if aName exists at aIndex.
  const bool Contains(uint32_t aIndex, const nsString& aName) const
  {
    if (!mHasRepeatAuto) {
      return mLineNameLists[aIndex].Contains(aName);
    }
    if (aIndex < mRepeatAutoEnd && aIndex >= mRepeatAutoStart &&
        mRepeatAutoLineNameListBefore.Contains(aName)) {
      return true;
    }
    if (aIndex <= mRepeatAutoEnd && aIndex > mRepeatAutoStart &&
        mRepeatAutoLineNameListAfter.Contains(aName)) {
      return true;
    }
    if (aIndex <= mRepeatAutoStart) {
      return mLineNameLists[aIndex].Contains(aName) ||
             (aIndex == mRepeatAutoEnd &&
              mLineNameLists[aIndex + 1].Contains(aName));
    }
    return aIndex >= mRepeatAutoEnd &&
           mLineNameLists[aIndex - mRepeatEndDelta].Contains(aName);
  }

  // Some style data references, for easy access.
  const nsTArray<nsTArray<nsString>>& mLineNameLists;
  const nsTArray<nsString>& mRepeatAutoLineNameListBefore;
  const nsTArray<nsString>& mRepeatAutoLineNameListAfter;
  // The index of the repeat(auto-fill/fit) track, or zero if there is none.
  const uint32_t mRepeatAutoStart;
  // The (hypothetical) index of the last such repeat() track.
  const uint32_t mRepeatAutoEnd;
  // The difference between mTemplateLinesEnd and mLineNameLists.Length().
  const int32_t mRepeatEndDelta;
  // The end of the line name lists with repeat(auto-fill/fit) tracks accounted
  // for.  It is equal to mLineNameLists.Length() when a repeat() track
  // generates one track (making mRepeatEndDelta == 0).
  const uint32_t mTemplateLinesEnd;
  // True if there is a specified repeat(auto-fill/fit) track.
  const bool mHasRepeatAuto;
};

/**
 * Encapsulates CSS track-sizing functions.
 */
struct MOZ_STACK_CLASS nsGridContainerFrame::TrackSizingFunctions
{
  TrackSizingFunctions(const nsStyleGridTemplate& aGridTemplate,
                       const nsStyleCoord&        aAutoMinSizing,
                       const nsStyleCoord&        aAutoMaxSizing)
    : mMinSizingFunctions(aGridTemplate.mMinTrackSizingFunctions)
    , mMaxSizingFunctions(aGridTemplate.mMaxTrackSizingFunctions)
    , mAutoMinSizing(aAutoMinSizing)
    , mAutoMaxSizing(aAutoMaxSizing)
    , mExplicitGridOffset(0)
    , mRepeatAutoStart(aGridTemplate.HasRepeatAuto() ?
                         aGridTemplate.mRepeatAutoIndex : 0)
    , mRepeatAutoEnd(mRepeatAutoStart)
    , mRepeatEndDelta(0)
    , mHasRepeatAuto(aGridTemplate.HasRepeatAuto())
  {
    MOZ_ASSERT(mMinSizingFunctions.Length() == mMaxSizingFunctions.Length());
    MOZ_ASSERT(!mHasRepeatAuto ||
               (mMinSizingFunctions.Length() >= 1 &&
                mRepeatAutoStart < mMinSizingFunctions.Length()));
  }

  /**
   * Initialize the number of auto-fill/fit tracks to use and return that.
   * (zero if no auto-fill/fit track was specified)
   */
  uint32_t InitRepeatTracks(nscoord aGridGap, nscoord aMinSize, nscoord aSize,
                            nscoord aMaxSize)
  {
    uint32_t repeatTracks =
      CalculateRepeatFillCount(aGridGap, aMinSize, aSize, aMaxSize);
    SetNumRepeatTracks(repeatTracks);
    return repeatTracks;
  }

  uint32_t CalculateRepeatFillCount(nscoord aGridGap,
                                    nscoord aMinSize,
                                    nscoord aSize,
                                    nscoord aMaxSize) const
  {
    if (!mHasRepeatAuto) {
      return 0;
    }
    // Spec quotes are from https://drafts.csswg.org/css-grid/#repeat-notation
    const uint32_t numTracks = mMinSizingFunctions.Length();
    MOZ_ASSERT(numTracks >= 1, "expected at least the repeat() track");
    nscoord maxFill = aSize != NS_UNCONSTRAINEDSIZE ? aSize : aMaxSize;
    if (maxFill == NS_UNCONSTRAINEDSIZE && aMinSize == NS_UNCONSTRAINEDSIZE) {
      // "Otherwise, the specified track list repeats only once."
      return 1;
    }
    nscoord repeatTrackSize = 0;
    // Note that the repeat() track size is included in |sum| in this loop.
    nscoord sum = 0;
    for (uint32_t i = 0; i < numTracks; ++i) {
      // "The <auto-repeat> variant ... requires definite minimum track sizes"
      // "... treating each track as its max track sizing function if that is
      //  definite or as its minimum track sizing function otherwise"
      // https://drafts.csswg.org/css-grid/#valdef-repeat-auto-fill
      const auto& maxCoord = mMaxSizingFunctions[i];
      const auto* coord = &maxCoord;
      if (!coord->IsCoordPercentCalcUnit()) {
        coord = &mMinSizingFunctions[i];
        if (!coord->IsCoordPercentCalcUnit()) {
          return 1;
        }
      }
      nscoord trackSize = nsRuleNode::ComputeCoordPercentCalc(*coord, aSize);
      if (i == mRepeatAutoStart) {
        // Use a minimum 1px for the repeat() track-size.
        if (trackSize < AppUnitsPerCSSPixel()) {
          trackSize = AppUnitsPerCSSPixel();
        }
        repeatTrackSize = trackSize;
      }
      sum += trackSize;
    }
    if (numTracks > 1) {
      // Add grid-gaps for all the tracks including the repeat() track.
      sum += aGridGap * (numTracks - 1);
    }
    nscoord available = maxFill != NS_UNCONSTRAINEDSIZE ? maxFill : aMinSize;
    nscoord spaceToFill = available - sum;
    if (spaceToFill <= 0) {
      // "if any number of repetitions would overflow, then 1 repetition"
      return 1;
    }
    // Calculate the max number of tracks that fits without overflow.
    uint32_t numRepeatTracks = (spaceToFill / (repeatTrackSize + aGridGap)) + 1;
    if (maxFill == NS_UNCONSTRAINEDSIZE) {
      // "Otherwise, if the grid container has a definite min size in
      // the relevant axis, the number of repetitions is the largest possible
      // positive integer that fulfills that minimum requirement."
      ++numRepeatTracks; // one more to ensure the grid is at least min-size
    }
    // Clamp the number of repeat tracks so that the last line <= kMaxLine.
    // (note that |numTracks| already includes one repeat() track)
    const uint32_t maxRepeatTracks = nsStyleGridLine::kMaxLine - numTracks;
    return std::min(numRepeatTracks, maxRepeatTracks);
  }

  /**
   * Compute the explicit grid end line number (in a zero-based grid).
   * @param aGridTemplateAreasEnd 'grid-template-areas' end line in this axis
   */
  uint32_t ComputeExplicitGridEnd(uint32_t aGridTemplateAreasEnd)
  {
    uint32_t end = NumExplicitTracks() + 1;
    end = std::max(end, aGridTemplateAreasEnd);
    end = std::min(end, uint32_t(nsStyleGridLine::kMaxLine));
    return end;
  }

  const nsStyleCoord& MinSizingFor(uint32_t aTrackIndex) const
  {
    if (MOZ_UNLIKELY(aTrackIndex < mExplicitGridOffset)) {
      return mAutoMinSizing;
    }
    uint32_t index = aTrackIndex - mExplicitGridOffset;
    if (index >= mRepeatAutoStart) {
      if (index < mRepeatAutoEnd) {
        return mMinSizingFunctions[mRepeatAutoStart];
      }
      index -= mRepeatEndDelta;
    }
    return index < mMinSizingFunctions.Length() ?
      mMinSizingFunctions[index] : mAutoMinSizing;
  }
  const nsStyleCoord& MaxSizingFor(uint32_t aTrackIndex) const
  {
    if (MOZ_UNLIKELY(aTrackIndex < mExplicitGridOffset)) {
      return mAutoMaxSizing;
    }
    uint32_t index = aTrackIndex - mExplicitGridOffset;
    if (index >= mRepeatAutoStart) {
      if (index < mRepeatAutoEnd) {
        return mMaxSizingFunctions[mRepeatAutoStart];
      }
      index -= mRepeatEndDelta;
    }
    return index < mMaxSizingFunctions.Length() ?
      mMaxSizingFunctions[index] : mAutoMaxSizing;
  }
  uint32_t NumExplicitTracks() const
  {
    return mMinSizingFunctions.Length() + mRepeatEndDelta;
  }
  uint32_t NumRepeatTracks() const
  {
    return mRepeatAutoEnd - mRepeatAutoStart;
  }
  void SetNumRepeatTracks(uint32_t aNumRepeatTracks)
  {
    MOZ_ASSERT(mHasRepeatAuto || aNumRepeatTracks == 0);
    mRepeatAutoEnd = mRepeatAutoStart + aNumRepeatTracks;
    mRepeatEndDelta = mHasRepeatAuto ?
                        int32_t(aNumRepeatTracks) - 1 :
                        0;
  }

  // Some style data references, for easy access.
  const nsTArray<nsStyleCoord>& mMinSizingFunctions;
  const nsTArray<nsStyleCoord>& mMaxSizingFunctions;
  const nsStyleCoord& mAutoMinSizing;
  const nsStyleCoord& mAutoMaxSizing;
  // Offset from the start of the implicit grid to the first explicit track.
  uint32_t mExplicitGridOffset;
  // The index of the repeat(auto-fill/fit) track, or zero if there is none.
  const uint32_t mRepeatAutoStart;
  // The (hypothetical) index of the last such repeat() track.
  uint32_t mRepeatAutoEnd;
  // The difference between mExplicitGridEnd and mMinSizingFunctions.Length().
  int32_t mRepeatEndDelta;
  // True if there is a specified repeat(auto-fill/fit) track.
  const bool mHasRepeatAuto;
};

/**
 * State for the tracks in one dimension.
 */
struct MOZ_STACK_CLASS nsGridContainerFrame::Tracks
{
  explicit Tracks(LogicalAxis aAxis) : mAxis(aAxis) {}

  void Initialize(const TrackSizingFunctions& aFunctions,
                  nscoord                     aGridGap,
                  uint32_t                    aNumTracks,
                  nscoord                     aContentBoxSize);

  /**
   * Return true if aRange spans at least one track with an intrinsic sizing
   * function and does not span any tracks with a <flex> max-sizing function.
   * @param aRange the span of tracks to check
   * @param aConstraint if MIN_ISIZE, treat a <flex> min-sizing as 'min-content'
   * @param aState will be set to the union of the state bits of all the spanned
   *               tracks, unless a flex track is found - then it only contains
   *               the union of the tracks up to and including the flex track.
   */
  bool HasIntrinsicButNoFlexSizingInRange(const LineRange&      aRange,
                                          IntrinsicISizeType    aConstraint,
                                          TrackSize::StateBits* aState) const;

  /**
   * Resolve Intrinsic Track Sizes.
   * http://dev.w3.org/csswg/css-grid/#algo-content
   */
  void ResolveIntrinsicSize(GridReflowState&            aState,
                            nsTArray<GridItemInfo>&     aGridItems,
                            const TrackSizingFunctions& aFunctions,
                            LineRange GridArea::*       aRange,
                            nscoord                     aPercentageBasis,
                            IntrinsicISizeType          aConstraint);

  /**
   * Helper for ResolveIntrinsicSize.  It implements step 1 "size tracks to fit
   * non-spanning items" in the spec.  Return true if the track has a <flex>
   * max-sizing function, false otherwise.
   */
  bool ResolveIntrinsicSizeStep1(GridReflowState&            aState,
                                 const TrackSizingFunctions& aFunctions,
                                 nscoord                     aPercentageBasis,
                                 IntrinsicISizeType          aConstraint,
                                 const LineRange&            aRange,
                                 nsIFrame*                   aGridItem);
  /**
   * Collect the tracks which are growable (matching aSelector) into
   * aGrowableTracks, and return the amount of space that can be used
   * to grow those tracks.  Specifically, we return aAvailableSpace minus
   * the sum of mBase's in aPlan (clamped to 0) for the tracks in aRange,
   * or zero when there are no growable tracks.
   * @note aPlan[*].mBase represents a planned new base or limit.
   */
  static nscoord CollectGrowable(nscoord                    aAvailableSpace,
                                 const nsTArray<TrackSize>& aPlan,
                                 const LineRange&           aRange,
                                 TrackSize::StateBits       aSelector,
                                 nsTArray<uint32_t>&        aGrowableTracks)
  {
    MOZ_ASSERT(aAvailableSpace > 0, "why call me?");
    nscoord space = aAvailableSpace;
    const uint32_t start = aRange.mStart;
    const uint32_t end = aRange.mEnd;
    for (uint32_t i = start; i < end; ++i) {
      const TrackSize& sz = aPlan[i];
      space -= sz.mBase;
      if (space <= 0) {
        return 0;
      }
      if ((sz.mState & aSelector) && !sz.IsFrozen()) {
        aGrowableTracks.AppendElement(i);
      }
    }
    return aGrowableTracks.IsEmpty() ? 0 : space;
  }

  void SetupGrowthPlan(nsTArray<TrackSize>&      aPlan,
                       const nsTArray<uint32_t>& aTracks) const
  {
    for (uint32_t track : aTracks) {
      aPlan[track] = mSizes[track];
    }
  }

  void CopyPlanToBase(const nsTArray<TrackSize>& aPlan,
                      const nsTArray<uint32_t>&  aTracks)
  {
    for (uint32_t track : aTracks) {
      MOZ_ASSERT(mSizes[track].mBase <= aPlan[track].mBase);
      mSizes[track].mBase = aPlan[track].mBase;
    }
  }

  void CopyPlanToLimit(const nsTArray<TrackSize>& aPlan,
                       const nsTArray<uint32_t>&  aTracks)
  {
    for (uint32_t track : aTracks) {
      MOZ_ASSERT(mSizes[track].mLimit == NS_UNCONSTRAINEDSIZE ||
                 mSizes[track].mLimit <= aPlan[track].mBase);
      mSizes[track].mLimit = aPlan[track].mBase;
    }
  }

  /**
   * Grow the planned size for tracks in aGrowableTracks up to their limit
   * and then freeze them (all aGrowableTracks must be unfrozen on entry).
   * Subtract the space added from aAvailableSpace and return that.
   */
  nscoord GrowTracksToLimit(nscoord                   aAvailableSpace,
                            nsTArray<TrackSize>&      aPlan,
                            const nsTArray<uint32_t>& aGrowableTracks) const
  {
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
        if (newBase > sz.mLimit) {
          nscoord consumed = sz.mLimit - sz.mBase;
          if (consumed > 0) {
            space -= consumed;
            sz.mBase = sz.mLimit;
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
   uint32_t MarkExcludedTracks(nsTArray<TrackSize>&      aPlan,
                               uint32_t                  aNumGrowable,
                               const nsTArray<uint32_t>& aGrowableTracks,
                               TrackSize::StateBits      aMinSizingSelector,
                               TrackSize::StateBits      aMaxSizingSelector,
                               TrackSize::StateBits      aSkipFlag) const
  {
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
   * Increase the planned size for tracks in aGrowableTracks that match
   * aSelector (or all tracks if aSelector is zero) beyond their limit.
   * This implements the "Distribute space beyond growth limits" step in
   * https://drafts.csswg.org/css-grid/#distribute-extra-space
   */
  void GrowSelectedTracksUnlimited(nscoord                   aAvailableSpace,
                                   nsTArray<TrackSize>&      aPlan,
                                   const nsTArray<uint32_t>& aGrowableTracks,
                                   TrackSize::StateBits      aSelector) const
  {
    MOZ_ASSERT(aAvailableSpace > 0 && aGrowableTracks.Length() > 0);
    uint32_t numGrowable = aGrowableTracks.Length();
    if (aSelector) {
      DebugOnly<TrackSize::StateBits> withoutFlexMin =
        TrackSize::StateBits(aSelector & ~TrackSize::eFlexMinSizing);
      MOZ_ASSERT(withoutFlexMin == TrackSize::eIntrinsicMinSizing ||
                 withoutFlexMin == TrackSize::eMinOrMaxContentMinSizing ||
                 withoutFlexMin == TrackSize::eMaxContentMinSizing);
      // Note that eMaxContentMinSizing is always included. We do those first:
      numGrowable = MarkExcludedTracks(aPlan, numGrowable, aGrowableTracks,
                                       TrackSize::eMaxContentMinSizing,
                                       TrackSize::eMaxContentMaxSizing,
                                       TrackSize::eSkipGrowUnlimited1);
      // Now mark min-content/auto/<flex> min-sizing tracks if requested.
      auto minOrAutoSelector = aSelector & ~TrackSize::eMaxContentMinSizing;
      if (minOrAutoSelector) {
        numGrowable = MarkExcludedTracks(aPlan, numGrowable, aGrowableTracks,
                                         minOrAutoSelector,
                                         TrackSize::eIntrinsicMaxSizing,
                                         TrackSize::eSkipGrowUnlimited2);
      }
    }
    nscoord space = aAvailableSpace;
    while (true) {
      nscoord spacePerTrack = std::max<nscoord>(space / numGrowable, 1);
      for (uint32_t track : aGrowableTracks) {
        TrackSize& sz = aPlan[track];
        if (sz.mState & TrackSize::eSkipGrowUnlimited) {
          continue; // an excluded track
        }
        sz.mBase += spacePerTrack;
        space -= spacePerTrack;
        MOZ_ASSERT(space >= 0);
        if (space == 0) {
          return;
        }
      }
    }
    MOZ_ASSERT_UNREACHABLE("we don't exit the loop above except by return");
  }

  /**
   * Distribute aAvailableSpace to the planned base size for aGrowableTracks
   * up to their limits, then distribute the remaining space beyond the limits.
   */
  void DistributeToTrackBases(nscoord              aAvailableSpace,
                              nsTArray<TrackSize>& aPlan,
                              nsTArray<uint32_t>&  aGrowableTracks,
                              TrackSize::StateBits aSelector)
  {
    SetupGrowthPlan(aPlan, aGrowableTracks);
    nscoord space = GrowTracksToLimit(aAvailableSpace, aPlan, aGrowableTracks);
    if (space > 0) {
      GrowSelectedTracksUnlimited(space, aPlan, aGrowableTracks, aSelector);
    }
    CopyPlanToBase(aPlan, aGrowableTracks);
  }

  /**
   * Distribute aAvailableSpace to the planned limits for aGrowableTracks.
   */
  void DistributeToTrackLimits(nscoord              aAvailableSpace,
                               nsTArray<TrackSize>& aPlan,
                               nsTArray<uint32_t>&  aGrowableTracks)
  {
    nscoord space = GrowTracksToLimit(aAvailableSpace, aPlan, aGrowableTracks);
    if (space > 0) {
      GrowSelectedTracksUnlimited(aAvailableSpace, aPlan, aGrowableTracks,
                                  TrackSize::StateBits(0));
    }
    CopyPlanToLimit(aPlan, aGrowableTracks);
  }

  /**
   * Distribute aAvailableSize to the tracks.  This implements 12.6 at:
   * http://dev.w3.org/csswg/css-grid/#algo-grow-tracks
   */
  void DistributeFreeSpace(nscoord aAvailableSize)
  {
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
        nscoord spacePerTrack =
          std::max<nscoord>(space / numGrowable, 1);
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
  float FindFrUnitSize(const LineRange&            aRange,
                       const nsTArray<uint32_t>&   aFlexTracks,
                       const TrackSizingFunctions& aFunctions,
                       nscoord                     aSpaceToFill) const;

  /**
   * Implements the "find the used flex fraction" part of StretchFlexibleTracks.
   * (The returned value is a 'nscoord' divided by a factor - a floating type
   * is used to avoid intermediary rounding errors.)
   */
  float FindUsedFlexFraction(GridReflowState&            aState,
                             nsTArray<GridItemInfo>&     aGridItems,
                             const nsTArray<uint32_t>&   aFlexTracks,
                             const TrackSizingFunctions& aFunctions,
                             nscoord                     aAvailableSize) const;

  /**
   * Implements "12.7. Stretch Flexible Tracks"
   * http://dev.w3.org/csswg/css-grid/#algo-flex-tracks
   */
  void StretchFlexibleTracks(GridReflowState&            aState,
                             nsTArray<GridItemInfo>&     aGridItems,
                             const TrackSizingFunctions& aFunctions,
                             nscoord                     aAvailableSize);

  /**
   * Implements "12.3. Track Sizing Algorithm"
   * http://dev.w3.org/csswg/css-grid/#algo-track-sizing
   */
  void CalculateSizes(GridReflowState&            aState,
                      nsTArray<GridItemInfo>&     aGridItems,
                      const TrackSizingFunctions& aFunctions,
                      nscoord                     aContentSize,
                      LineRange GridArea::*       aRange,
                      IntrinsicISizeType          aConstraint);

  /**
   * Apply 'align/justify-content', whichever is relevant for this axis.
   * https://drafts.csswg.org/css-align-3/#propdef-align-content
   */
  void AlignJustifyContent(const nsHTMLReflowState& aReflowState,
                           const LogicalSize& aContainerSize);


  nscoord SumOfGridGaps() const {
    auto len = mSizes.Length();
    return MOZ_LIKELY(len > 1) ? (len - 1) * mGridGap : 0;
  }

#ifdef DEBUG
  void Dump() const
  {
    for (uint32_t i = 0, len = mSizes.Length(); i < len; ++i) {
      printf("  %d: ", i);
      mSizes[i].Dump();
      printf("\n");
    }
  }
#endif

  nsAutoTArray<TrackSize, 32> mSizes;
  nscoord mGridGap;
  LogicalAxis mAxis;
};

struct MOZ_STACK_CLASS nsGridContainerFrame::GridReflowState
{
  GridReflowState(nsGridContainerFrame*    aFrame,
                  const nsHTMLReflowState& aRS)
    : GridReflowState(aFrame, *aRS.rendContext, &aRS, aRS.mStylePosition,
                      aRS.GetWritingMode())
  {}
  GridReflowState(nsGridContainerFrame* aFrame,
                  nsRenderingContext&   aRC)
    : GridReflowState(aFrame, aRC, nullptr, aFrame->StylePosition(),
                      aFrame->GetWritingMode())
  {}

  GridItemCSSOrderIterator mIter;
  const nsStylePosition* const mGridStyle;
  Tracks mCols;
  Tracks mRows;
  TrackSizingFunctions mColFunctions;
  TrackSizingFunctions mRowFunctions;
  /**
   * @note mReflowState may be null when using the 2nd ctor above. In this case
   * we'll construct a dummy parent reflow state if we need it to calculate
   * min/max-content contributions when sizing tracks.
   */
  const nsHTMLReflowState* mReflowState;
  nsRenderingContext& mRenderingContext;
  const WritingMode mWM;

private:
  GridReflowState(nsGridContainerFrame*    aFrame,
                  nsRenderingContext&      aRenderingContext,
                  const nsHTMLReflowState* aReflowState,
                  const nsStylePosition*   aGridStyle,
                  const WritingMode&       aWM)
    : mIter(aFrame, kPrincipalList)
    , mGridStyle(aGridStyle)
    , mCols(eLogicalAxisInline)
    , mRows(eLogicalAxisBlock)
    , mColFunctions(mGridStyle->mGridTemplateColumns,
                    mGridStyle->mGridAutoColumnsMin,
                    mGridStyle->mGridAutoColumnsMax)
    , mRowFunctions(mGridStyle->mGridTemplateRows,
                    mGridStyle->mGridAutoRowsMin,
                    mGridStyle->mGridAutoRowsMax)
    , mReflowState(aReflowState)
    , mRenderingContext(aRenderingContext)
    , mWM(aWM)
  {}
};

static
bool IsMinContent(const nsStyleCoord& aCoord)
{
  return aCoord.GetUnit() == eStyleUnit_Enumerated &&
         aCoord.GetIntValue() == NS_STYLE_GRID_TRACK_BREADTH_MIN_CONTENT;
}

/**
 * A convenience method to lookup a name in 'grid-template-areas'.
 * @param aStyle the StylePosition() for the grid container
 * @return null if not found
 */
static const css::GridNamedArea*
FindNamedArea(const nsSubstring& aName, const nsStylePosition* aStyle)
{
  if (!aStyle->mGridTemplateAreas) {
    return nullptr;
  }
  const nsTArray<css::GridNamedArea>& areas =
    aStyle->mGridTemplateAreas->mNamedAreas;
  size_t len = areas.Length();
  for (size_t i = 0; i < len; ++i) {
    const css::GridNamedArea& area = areas[i];
    if (area.mName == aName) {
      return &area;
    }
  }
  return nullptr;
}

// Return true if aString ends in aSuffix and has at least one character before
// the suffix. Assign aIndex to where the suffix starts.
static bool
IsNameWithSuffix(const nsString& aString, const nsString& aSuffix,
                 uint32_t* aIndex)
{
  if (StringEndsWith(aString, aSuffix)) {
    *aIndex = aString.Length() - aSuffix.Length();
    return *aIndex != 0;
  }
  return false;
}

static bool
IsNameWithEndSuffix(const nsString& aString, uint32_t* aIndex)
{
  return IsNameWithSuffix(aString, NS_LITERAL_STRING("-end"), aIndex);
}

static bool
IsNameWithStartSuffix(const nsString& aString, uint32_t* aIndex)
{
  return IsNameWithSuffix(aString, NS_LITERAL_STRING("-start"), aIndex);
}

enum class GridLineSide {
  eBeforeGridGap,
  eAfterGridGap,
};

static nscoord
GridLineEdge(uint32_t aLine, const nsTArray<TrackSize>& aTrackSizes,
             GridLineSide aSide)
{
  if (MOZ_UNLIKELY(aTrackSizes.IsEmpty())) {
    // https://drafts.csswg.org/css-grid/#grid-definition
    // "... the explicit grid still contains one grid line in each axis."
    MOZ_ASSERT(aLine == 0, "We should only resolve line 1 in an empty grid");
    return nscoord(0);
  }
  MOZ_ASSERT(aLine <= aTrackSizes.Length(), "aTrackSizes is too small");
  if (aSide == GridLineSide::eBeforeGridGap) {
    if (aLine == 0) {
      return aTrackSizes[0].mPosition;
    }
    const TrackSize& sz = aTrackSizes[aLine - 1];
    return sz.mPosition + sz.mBase;
  }
  if (aLine == aTrackSizes.Length()) {
    const TrackSize& sz = aTrackSizes[aLine - 1];
    return sz.mPosition + sz.mBase;
  }
  return aTrackSizes[aLine].mPosition;
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
static uint32_t
GetDisplayFlagsForGridItem(nsIFrame* aFrame)
{
  const nsStylePosition* pos = aFrame->StylePosition();
  if (pos->mZIndex.GetUnit() == eStyleUnit_Integer) {
    return nsIFrame::DISPLAY_CHILD_FORCE_STACKING_CONTEXT;
  }
  return nsIFrame::DISPLAY_CHILD_FORCE_PSEUDO_STACKING_CONTEXT;
}

static nscoord
SpaceToFill(WritingMode aWM, const LogicalSize& aSize, nscoord aMargin,
            LogicalAxis aAxis, nscoord aCBSize)
{
  nscoord size = aAxis == eLogicalAxisBlock ? aSize.BSize(aWM)
                                            : aSize.ISize(aWM);
  return aCBSize - (size + aMargin);
}

// Align (or stretch) an item's margin box in its aAxis inside aCBSize.
static bool
AlignJustifySelf(uint8_t aAlignment, bool aOverflowSafe, LogicalAxis aAxis,
                 bool aSameSide, nscoord aCBSize, const nsHTMLReflowState& aRS,
                 const LogicalSize& aChildSize, LogicalSize* aContentSize,
                 LogicalPoint* aPos)
{
  MOZ_ASSERT(aAlignment != NS_STYLE_ALIGN_AUTO, "unexpected 'auto' "
             "computed value for normal flow grid item");
  MOZ_ASSERT(aAlignment != NS_STYLE_ALIGN_LEFT &&
             aAlignment != NS_STYLE_ALIGN_RIGHT,
             "caller should map that to the corresponding START/END");

  // Map some alignment values to 'start' / 'end'.
  switch (aAlignment) {
    case NS_STYLE_ALIGN_SELF_START: // align/justify-self: self-start
      aAlignment = MOZ_LIKELY(aSameSide) ? NS_STYLE_ALIGN_START
                                         : NS_STYLE_ALIGN_END;
      break;
    case NS_STYLE_ALIGN_SELF_END: // align/justify-self: self-end
      aAlignment = MOZ_LIKELY(aSameSide) ? NS_STYLE_ALIGN_END
                                         : NS_STYLE_ALIGN_START;
      break;
    case NS_STYLE_ALIGN_FLEX_START: // same as 'start' for Grid
      aAlignment = NS_STYLE_ALIGN_START;
      break;
    case NS_STYLE_ALIGN_FLEX_END: // same as 'end' for Grid
      aAlignment = NS_STYLE_ALIGN_END;
      break;
  }

  // XXX try to condense this code a bit by adding the necessary convenience
  // methods? (bug 1209710)

  // Get the item's margin corresponding to the container's start/end side.
  const LogicalMargin margin = aRS.ComputedLogicalMargin();
  WritingMode wm = aRS.GetWritingMode();
  nscoord marginStart, marginEnd;
  if (aAxis == eLogicalAxisBlock) {
    if (MOZ_LIKELY(aSameSide)) {
      marginStart = margin.BStart(wm);
      marginEnd = margin.BEnd(wm);
    } else {
      marginStart = margin.BEnd(wm);
      marginEnd = margin.BStart(wm);
    }
  } else {
    if (MOZ_LIKELY(aSameSide)) {
      marginStart = margin.IStart(wm);
      marginEnd = margin.IEnd(wm);
    } else {
      marginStart = margin.IEnd(wm);
      marginEnd = margin.IStart(wm);
    }
  }

  const auto& styleMargin = aRS.mStyleMargin->mMargin;
  bool hasAutoMarginStart;
  bool hasAutoMarginEnd;
  if (aAxis == eLogicalAxisBlock) {
    hasAutoMarginStart = styleMargin.GetBStartUnit(wm) == eStyleUnit_Auto;
    hasAutoMarginEnd = styleMargin.GetBEndUnit(wm) == eStyleUnit_Auto;
  } else {
    hasAutoMarginStart = styleMargin.GetIStartUnit(wm) == eStyleUnit_Auto;
    hasAutoMarginEnd = styleMargin.GetIEndUnit(wm) == eStyleUnit_Auto;
  }

  // https://drafts.csswg.org/css-align-3/#overflow-values
  // This implements <overflow-position> = 'safe'.
  // And auto-margins: https://drafts.csswg.org/css-grid/#auto-margins
  if ((MOZ_UNLIKELY(aOverflowSafe) && aAlignment != NS_STYLE_ALIGN_START) ||
      hasAutoMarginStart || hasAutoMarginEnd) {
    nscoord space = SpaceToFill(wm, aChildSize, marginStart + marginEnd,
                                aAxis, aCBSize);
    // XXX we might want to include == 0 here as an optimization -
    // I need to see what the baseline/last-baseline code looks like first.
    if (space < 0) {
      // "Overflowing elements ignore their auto margins and overflow
      // in the end directions"
      aAlignment = NS_STYLE_ALIGN_START;
    } else if (hasAutoMarginEnd) {
      aAlignment = hasAutoMarginStart ? NS_STYLE_ALIGN_CENTER
                                      : (aSameSide ? NS_STYLE_ALIGN_START
                                                   : NS_STYLE_ALIGN_END);
    } else if (hasAutoMarginStart) {
      aAlignment = aSameSide ? NS_STYLE_ALIGN_END : NS_STYLE_ALIGN_START;
    }
  }

  // Set the position and size (aPos/aContentSize) for the requested alignment.
  bool didResize = false;
  nscoord offset = 0; // NOTE: this is the resulting frame offset (border box).
  switch (aAlignment) {
    case NS_STYLE_ALIGN_BASELINE:
    case NS_STYLE_ALIGN_LAST_BASELINE:
      NS_WARNING("NYI: baseline/last-baseline for grid (bug 1151204)"); // XXX
    case NS_STYLE_ALIGN_START:
      offset = marginStart;
      break;
    case NS_STYLE_ALIGN_END: {
      nscoord size = aAxis == eLogicalAxisBlock ? aChildSize.BSize(wm)
                                                : aChildSize.ISize(wm);
      offset = aCBSize - (size + marginEnd);
      break;
    }
    case NS_STYLE_ALIGN_CENTER: {
      nscoord size = aAxis == eLogicalAxisBlock ? aChildSize.BSize(wm)
                                                : aChildSize.ISize(wm);
      offset = (aCBSize - size + marginStart - marginEnd) / 2;
      break;
    }
    case NS_STYLE_ALIGN_STRETCH: {
      MOZ_ASSERT(!hasAutoMarginStart && !hasAutoMarginEnd);
      offset = marginStart;
      if (aAxis == eLogicalAxisBlock
            ? (aRS.mStylePosition->BSize(wm).GetUnit() == eStyleUnit_Auto)
            : (aRS.mStylePosition->ISize(wm).GetUnit() == eStyleUnit_Auto)) {
        nscoord size = aAxis == eLogicalAxisBlock ? aChildSize.BSize(wm)
                                                  : aChildSize.ISize(wm);
        nscoord gap = aCBSize - (size + marginStart + marginEnd);
        if (gap > 0) {
          // Note: The ComputedMax* values are always content-box max values,
          // even for box-sizing:border-box.
          LogicalMargin bp = aRS.ComputedLogicalBorderPadding();
          // XXX ApplySkipSides is probably not very useful here since we
          // might not have created any next-in-flow yet.  Use the reflow status
          // instead?  Do all fragments stretch? (bug 1144096).
          bp.ApplySkipSides(aRS.frame->GetLogicalSkipSides());
          nscoord bpInAxis = aAxis == eLogicalAxisBlock ? bp.BStartEnd(wm)
                                                        : bp.IStartEnd(wm);
          nscoord contentSize = size - bpInAxis;
          const nscoord unstretchedContentSize = contentSize;
          contentSize += gap;
          nscoord max = aAxis == eLogicalAxisBlock ? aRS.ComputedMaxBSize()
                                                   : aRS.ComputedMaxISize();
          if (MOZ_UNLIKELY(contentSize > max)) {
            contentSize = max;
            gap = contentSize - unstretchedContentSize;
          }
          // |gap| is now how much the content size is actually allowed to grow.
          didResize = gap > 0;
          // (nscoord overflow can make |contentSize| negative, bug 1225118)
          if (didResize && MOZ_LIKELY(contentSize >= 0)) {
            (aAxis == eLogicalAxisBlock ? aContentSize->BSize(wm)
                                        : aContentSize->ISize(wm)) = contentSize;
            if (MOZ_UNLIKELY(!aSameSide)) {
              offset += gap;
            }
          }
        }
      }
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("unknown align-/justify-self value");
  }
  if (offset != 0) {
    nscoord& pos = aAxis == eLogicalAxisBlock ? aPos->B(wm) : aPos->I(wm);
    pos += MOZ_LIKELY(aSameSide) ? offset : -offset;
  }
  return didResize;
}

static bool
SameSide(WritingMode aContainerWM, LogicalSide aContainerSide,
         WritingMode aChildWM, LogicalSide aChildSide)
{
  MOZ_ASSERT(aContainerWM.PhysicalAxis(GetAxis(aContainerSide)) ==
                 aChildWM.PhysicalAxis(GetAxis(aChildSide)));
  return aContainerWM.PhysicalSide(aContainerSide) ==
             aChildWM.PhysicalSide(aChildSide);
}

static Maybe<LogicalAxis>
AlignSelf(uint8_t aAlignSelf, const LogicalRect& aCB, const WritingMode aCBWM,
          const nsHTMLReflowState& aRS, const LogicalSize& aSize,
          LogicalSize* aContentSize, LogicalPoint* aPos)
{
  Maybe<LogicalAxis> resizedAxis;
  auto alignSelf = aAlignSelf;
  bool overflowSafe = alignSelf & NS_STYLE_ALIGN_SAFE;
  alignSelf &= ~NS_STYLE_ALIGN_FLAG_BITS;
  // Grid's 'align-self' axis is never parallel to the container's inline axis.
  if (alignSelf == NS_STYLE_ALIGN_LEFT || alignSelf == NS_STYLE_ALIGN_RIGHT) {
    alignSelf = NS_STYLE_ALIGN_START;
  }
  if (MOZ_UNLIKELY(alignSelf == NS_STYLE_ALIGN_AUTO)) {
    // Happens in rare edge cases when 'position' was ignored by the frame
    // constructor (and the style system computed 'auto' based on 'position').
    alignSelf = NS_STYLE_ALIGN_STRETCH;
  }
  WritingMode childWM = aRS.GetWritingMode();
  bool isOrthogonal = aCBWM.IsOrthogonalTo(childWM);
  // |sameSide| is true if the container's start side in this axis is the same
  // as the child's start side, in the child's parallel axis.
  bool sameSide = SameSide(aCBWM, eLogicalSideBStart,
                           childWM, isOrthogonal ? eLogicalSideIStart
                                                 : eLogicalSideBStart);
  LogicalAxis axis = isOrthogonal ? eLogicalAxisInline : eLogicalAxisBlock;
  if (AlignJustifySelf(alignSelf, overflowSafe, axis, sameSide,
                       aCB.BSize(aCBWM), aRS, aSize, aContentSize, aPos)) {
    resizedAxis.emplace(axis);
  }
  return resizedAxis;
}

static Maybe<LogicalAxis>
JustifySelf(uint8_t aJustifySelf, const LogicalRect& aCB, const WritingMode aCBWM,
            const nsHTMLReflowState& aRS, const LogicalSize& aSize,
            LogicalSize* aContentSize, LogicalPoint* aPos)
{
  Maybe<LogicalAxis> resizedAxis;
  auto justifySelf = aJustifySelf;
  bool overflowSafe = justifySelf & NS_STYLE_JUSTIFY_SAFE;
  justifySelf &= ~NS_STYLE_JUSTIFY_FLAG_BITS;
  if (MOZ_UNLIKELY(justifySelf == NS_STYLE_ALIGN_AUTO)) {
    // Happens in rare edge cases when 'position' was ignored by the frame
    // constructor (and the style system computed 'auto' based on 'position').
    justifySelf = NS_STYLE_ALIGN_STRETCH;
  }
  WritingMode childWM = aRS.GetWritingMode();
  bool isOrthogonal = aCBWM.IsOrthogonalTo(childWM);
  // |sameSide| is true if the container's start side in this axis is the same
  // as the child's start side, in the child's parallel axis.
  bool sameSide = SameSide(aCBWM, eLogicalSideIStart,
                           childWM, isOrthogonal ? eLogicalSideBStart
                                                 : eLogicalSideIStart);
  // Grid's 'justify-self' axis is always parallel to the container's inline
  // axis, so justify-self:left|right always applies.
  switch (justifySelf) {
    case NS_STYLE_JUSTIFY_LEFT:
      justifySelf = aCBWM.IsBidiLTR() ? NS_STYLE_JUSTIFY_START
                                      : NS_STYLE_JUSTIFY_END;
    break;
    case NS_STYLE_JUSTIFY_RIGHT:
      justifySelf = aCBWM.IsBidiLTR() ? NS_STYLE_JUSTIFY_END
                                      : NS_STYLE_JUSTIFY_START;
    break;
  }

  LogicalAxis axis = isOrthogonal ? eLogicalAxisBlock : eLogicalAxisInline;
  if (AlignJustifySelf(justifySelf, overflowSafe, axis, sameSide,
                       aCB.ISize(aCBWM), aRS, aSize, aContentSize, aPos)) {
    resizedAxis.emplace(axis);
  }
  return resizedAxis;
}

static uint16_t
GetAlignJustifyValue(uint16_t aAlignment, const WritingMode aWM,
                     const bool aIsAlign, bool* aOverflowSafe)
{
  *aOverflowSafe = aAlignment & NS_STYLE_ALIGN_SAFE;
  aAlignment &= (NS_STYLE_ALIGN_ALL_BITS & ~NS_STYLE_ALIGN_FLAG_BITS);

  // Map some alignment values to 'start' / 'end'.
  switch (aAlignment) {
    case NS_STYLE_ALIGN_LEFT:
    case NS_STYLE_ALIGN_RIGHT: {
      if (aIsAlign) {
        // Grid's 'align-content' axis is never parallel to the inline axis.
        return NS_STYLE_ALIGN_START;
      }
      bool isStart = aWM.IsBidiLTR() == (aAlignment == NS_STYLE_ALIGN_LEFT);
      return isStart ? NS_STYLE_ALIGN_START : NS_STYLE_ALIGN_END;
    }
    case NS_STYLE_ALIGN_FLEX_START: // same as 'start' for Grid
      return NS_STYLE_ALIGN_START;
    case NS_STYLE_ALIGN_FLEX_END: // same as 'end' for Grid
      return NS_STYLE_ALIGN_END;
  }
  return aAlignment;
}

static uint16_t
GetAlignJustifyFallbackIfAny(uint16_t aAlignment, const WritingMode aWM,
                             const bool aIsAlign, bool* aOverflowSafe)
{
  uint16_t fallback = aAlignment >> NS_STYLE_ALIGN_ALL_SHIFT;
  if (fallback) {
    return GetAlignJustifyValue(fallback, aWM, aIsAlign, aOverflowSafe);
  }
  // https://drafts.csswg.org/css-align-3/#fallback-alignment
  switch (aAlignment) {
    case NS_STYLE_ALIGN_STRETCH:
    case NS_STYLE_ALIGN_SPACE_BETWEEN:
      return NS_STYLE_ALIGN_START;
    case NS_STYLE_ALIGN_SPACE_AROUND:
    case NS_STYLE_ALIGN_SPACE_EVENLY:
      return NS_STYLE_ALIGN_CENTER;
  }
  return 0;
}

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsGridContainerFrame)
  NS_QUERYFRAME_ENTRY(nsGridContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsGridContainerFrame)

nsContainerFrame*
NS_NewGridContainerFrame(nsIPresShell* aPresShell,
                         nsStyleContext* aContext)
{
  return new (aPresShell) nsGridContainerFrame(aContext);
}


//----------------------------------------------------------------------

// nsGridContainerFrame Method Implementations
// ===========================================

/*static*/ const nsRect&
nsGridContainerFrame::GridItemCB(nsIFrame* aChild)
{
  MOZ_ASSERT((aChild->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
             aChild->IsAbsolutelyPositioned());
  nsRect* cb = static_cast<nsRect*>(aChild->Properties().Get(
                 GridItemContainingBlockRect()));
  MOZ_ASSERT(cb, "this method must only be called on grid items, and the grid "
                 "container should've reflowed this item by now and set up cb");
  return *cb;
}

void
nsGridContainerFrame::AddImplicitNamedAreas(
  const nsTArray<nsTArray<nsString>>& aLineNameLists)
{
  // http://dev.w3.org/csswg/css-grid/#implicit-named-areas
  // Note: recording these names for fast lookup later is just an optimization.
  const uint32_t len =
    std::min(aLineNameLists.Length(), size_t(nsStyleGridLine::kMaxLine));
  nsTHashtable<nsStringHashKey> currentStarts;
  ImplicitNamedAreas* areas = GetImplicitNamedAreas();
  for (uint32_t i = 0; i < len; ++i) {
    for (const nsString& name : aLineNameLists[i]) {
      uint32_t index;
      if (::IsNameWithStartSuffix(name, &index) ||
          ::IsNameWithEndSuffix(name, &index)) {
        nsDependentSubstring area(name, 0, index);
        if (!areas) {
          areas = new ImplicitNamedAreas;
          Properties().Set(ImplicitNamedAreasProperty(), areas);
        }
        areas->PutEntry(area);
      }
    }
  }
}

void
nsGridContainerFrame::InitImplicitNamedAreas(const nsStylePosition* aStyle)
{
  ImplicitNamedAreas* areas = GetImplicitNamedAreas();
  if (areas) {
    // Clear it, but reuse the hashtable itself for now.  We'll remove it
    // below if it isn't needed anymore.
    areas->Clear();
  }
  AddImplicitNamedAreas(aStyle->mGridTemplateColumns.mLineNameLists);
  AddImplicitNamedAreas(aStyle->mGridTemplateRows.mLineNameLists);
  if (areas && areas->Count() == 0) {
    Properties().Delete(ImplicitNamedAreasProperty());
  }
}

int32_t
nsGridContainerFrame::ResolveLine(
  const nsStyleGridLine& aLine,
  int32_t aNth,
  uint32_t aFromIndex,
  const LineNameMap& aNameMap,
  uint32_t GridNamedArea::* aAreaStart,
  uint32_t GridNamedArea::* aAreaEnd,
  uint32_t aExplicitGridEnd,
  LineRangeSide aSide,
  const nsStylePosition* aStyle)
{
  MOZ_ASSERT(!aLine.IsAuto());
  int32_t line = 0;
  if (aLine.mLineName.IsEmpty()) {
    MOZ_ASSERT(aNth != 0, "css-grid 9.2: <integer> must not be zero.");
    line = int32_t(aFromIndex) + aNth;
  } else {
    if (aNth == 0) {
      // <integer> was omitted; treat it as 1.
      aNth = 1;
    }
    bool isNameOnly = !aLine.mHasSpan && aLine.mInteger == 0;
    if (isNameOnly) {
      const GridNamedArea* area = ::FindNamedArea(aLine.mLineName, aStyle);
      if (area || HasImplicitNamedArea(aLine.mLineName)) {
        // The given name is a named area - look for explicit lines named
        // <name>-start/-end depending on which side we're resolving.
        // http://dev.w3.org/csswg/css-grid/#grid-placement-slot
        uint32_t implicitLine = 0;
        nsAutoString lineName(aLine.mLineName);
        if (aSide == eLineRangeSideStart) {
          lineName.AppendLiteral("-start");
          implicitLine = area ? area->*aAreaStart : 0;
        } else {
          lineName.AppendLiteral("-end");
          implicitLine = area ? area->*aAreaEnd : 0;
        }
        line = aNameMap.FindNamedLine(lineName, &aNth, aFromIndex,
                                      implicitLine);
      }
    }

    if (line == 0) {
      // If mLineName ends in -start/-end, try the prefix as a named area.
      uint32_t implicitLine = 0;
      uint32_t index;
      auto GridNamedArea::* areaEdge = aAreaStart;
      bool found = ::IsNameWithStartSuffix(aLine.mLineName, &index);
      if (!found) {
        found = ::IsNameWithEndSuffix(aLine.mLineName, &index);
        areaEdge = aAreaEnd;
      }
      if (found) {
        const GridNamedArea* area =
          ::FindNamedArea(nsDependentSubstring(aLine.mLineName, 0, index),
                          aStyle);
        if (area) {
          implicitLine = area->*areaEdge;
        }
      }
      line = aNameMap.FindNamedLine(aLine.mLineName, &aNth, aFromIndex,
                                    implicitLine);
    }

    if (line == 0) {
      MOZ_ASSERT(aNth != 0, "we found all N named lines but 'line' is zero!");
      int32_t edgeLine;
      if (aLine.mHasSpan) {
        // http://dev.w3.org/csswg/css-grid/#grid-placement-span-int
        // 'span <custom-ident> N'
        edgeLine = aSide == eLineRangeSideStart ? 1 : aExplicitGridEnd;
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
  return clamped(line, nsStyleGridLine::kMinLine, nsStyleGridLine::kMaxLine);
}

nsGridContainerFrame::LinePair
nsGridContainerFrame::ResolveLineRangeHelper(
  const nsStyleGridLine& aStart,
  const nsStyleGridLine& aEnd,
  const LineNameMap& aNameMap,
  uint32_t GridNamedArea::* aAreaStart,
  uint32_t GridNamedArea::* aAreaEnd,
  uint32_t aExplicitGridEnd,
  const nsStylePosition* aStyle)
{
  MOZ_ASSERT(int32_t(nsGridContainerFrame::kAutoLine) > nsStyleGridLine::kMaxLine);

  if (aStart.mHasSpan) {
    if (aEnd.mHasSpan || aEnd.IsAuto()) {
      // http://dev.w3.org/csswg/css-grid/#grid-placement-errors
      if (aStart.mLineName.IsEmpty()) {
        // span <integer> / span *
        // span <integer> / auto
        return LinePair(kAutoLine, aStart.mInteger);
      }
      // span <custom-ident> / span *
      // span <custom-ident> / auto
      return LinePair(kAutoLine, 1); // XXX subgrid explicit size instead of 1?
    }

    uint32_t from = aEnd.mInteger < 0 ? aExplicitGridEnd + 1: 0;
    auto end = ResolveLine(aEnd, aEnd.mInteger, from, aNameMap, aAreaStart,
                           aAreaEnd, aExplicitGridEnd, eLineRangeSideEnd,
                           aStyle);
    int32_t span = aStart.mInteger == 0 ? 1 : aStart.mInteger;
    if (end <= 1) {
      // The end is at or before the first explicit line, thus all lines before
      // it match <custom-ident> since they're implicit.
      int32_t start = std::max(end - span, nsStyleGridLine::kMinLine);
      return LinePair(start, end);
    }
    auto start = ResolveLine(aStart, -span, end, aNameMap, aAreaStart,
                             aAreaEnd, aExplicitGridEnd, eLineRangeSideStart,
                             aStyle);
    return LinePair(start, end);
  }

  int32_t start = kAutoLine;
  if (aStart.IsAuto()) {
    if (aEnd.IsAuto()) {
      // auto / auto
      return LinePair(start, 1); // XXX subgrid explicit size instead of 1?
    }
    if (aEnd.mHasSpan) {
      if (aEnd.mLineName.IsEmpty()) {
        // auto / span <integer>
        MOZ_ASSERT(aEnd.mInteger != 0);
        return LinePair(start, aEnd.mInteger);
      }
      // http://dev.w3.org/csswg/css-grid/#grid-placement-errors
      // auto / span <custom-ident>
      return LinePair(start, 1); // XXX subgrid explicit size instead of 1?
    }
  } else {
    uint32_t from = aStart.mInteger < 0 ? aExplicitGridEnd + 1: 0;
    start = ResolveLine(aStart, aStart.mInteger, from, aNameMap,
                        aAreaStart, aAreaEnd, aExplicitGridEnd,
                        eLineRangeSideStart, aStyle);
    if (aEnd.IsAuto()) {
      // A "definite line / auto" should resolve the auto to 'span 1'.
      // The error handling in ResolveLineRange will make that happen and also
      // clamp the end line correctly if we return "start / start".
      return LinePair(start, start);
    }
  }

  uint32_t from;
  int32_t nth = aEnd.mInteger == 0 ? 1 : aEnd.mInteger;
  if (aEnd.mHasSpan) {
    if (MOZ_UNLIKELY(start < 0)) {
      if (aEnd.mLineName.IsEmpty()) {
        return LinePair(start, start + nth);
      }
      from = 0;
    } else {
      if (start >= int32_t(aExplicitGridEnd)) {
        // The start is at or after the last explicit line, thus all lines
        // after it match <custom-ident> since they're implicit.
        return LinePair(start, std::min(start + nth, nsStyleGridLine::kMaxLine));
      }
      from = start;
    }
  } else {
    from = aEnd.mInteger < 0 ? aExplicitGridEnd + 1: 0;
  }
  auto end = ResolveLine(aEnd, nth, from, aNameMap, aAreaStart,
                         aAreaEnd, aExplicitGridEnd, eLineRangeSideEnd, aStyle);
  if (start == int32_t(kAutoLine)) {
    // auto / definite line
    start = std::max(nsStyleGridLine::kMinLine, end - 1);
  }
  return LinePair(start, end);
}

nsGridContainerFrame::LineRange
nsGridContainerFrame::ResolveLineRange(
  const nsStyleGridLine& aStart,
  const nsStyleGridLine& aEnd,
  const LineNameMap& aNameMap,
  uint32_t GridNamedArea::* aAreaStart,
  uint32_t GridNamedArea::* aAreaEnd,
  uint32_t aExplicitGridEnd,
  const nsStylePosition* aStyle)
{
  LinePair r = ResolveLineRangeHelper(aStart, aEnd, aNameMap, aAreaStart,
                                      aAreaEnd, aExplicitGridEnd, aStyle);
  MOZ_ASSERT(r.second != int32_t(kAutoLine));

  if (r.first == int32_t(kAutoLine)) {
    // r.second is a span, clamp it to kMaxLine - 1 so that the returned
    // range has a HypotheticalEnd <= kMaxLine.
    // http://dev.w3.org/csswg/css-grid/#overlarge-grids
    r.second = std::min(r.second, nsStyleGridLine::kMaxLine - 1);
  } else {
    // http://dev.w3.org/csswg/css-grid/#grid-placement-errors
    if (r.first > r.second) {
      Swap(r.first, r.second);
    } else if (r.first == r.second) {
      if (MOZ_UNLIKELY(r.first == nsStyleGridLine::kMaxLine)) {
        r.first = nsStyleGridLine::kMaxLine - 1;
      }
      r.second = r.first + 1; // XXX subgrid explicit size instead of 1?
    }
  }
  return LineRange(r.first, r.second);
}

nsGridContainerFrame::GridArea
nsGridContainerFrame::PlaceDefinite(nsIFrame* aChild,
                                    const LineNameMap& aColLineNameMap,
                                    const LineNameMap& aRowLineNameMap,
                                    const nsStylePosition* aStyle)
{
  const nsStylePosition* itemStyle = aChild->StylePosition();
  return GridArea(
    ResolveLineRange(itemStyle->mGridColumnStart, itemStyle->mGridColumnEnd,
                     aColLineNameMap,
                     &GridNamedArea::mColumnStart, &GridNamedArea::mColumnEnd,
                     mExplicitGridColEnd, aStyle),
    ResolveLineRange(itemStyle->mGridRowStart, itemStyle->mGridRowEnd,
                     aRowLineNameMap,
                     &GridNamedArea::mRowStart, &GridNamedArea::mRowEnd,
                     mExplicitGridRowEnd, aStyle));
}

nsGridContainerFrame::LineRange
nsGridContainerFrame::ResolveAbsPosLineRange(
  const nsStyleGridLine& aStart,
  const nsStyleGridLine& aEnd,
  const LineNameMap& aNameMap,
  uint32_t GridNamedArea::* aAreaStart,
  uint32_t GridNamedArea::* aAreaEnd,
  uint32_t aExplicitGridEnd,
  int32_t aGridStart,
  int32_t aGridEnd,
  const nsStylePosition* aStyle)
{
  if (aStart.IsAuto()) {
    if (aEnd.IsAuto()) {
      return LineRange(kAutoLine, kAutoLine);
    }
    uint32_t from = aEnd.mInteger < 0 ? aExplicitGridEnd + 1: 0;
    int32_t end =
      ResolveLine(aEnd, aEnd.mInteger, from, aNameMap, aAreaStart,
                  aAreaEnd, aExplicitGridEnd, eLineRangeSideEnd, aStyle);
    if (aEnd.mHasSpan) {
      ++end;
    }
    // A line outside the existing grid is treated as 'auto' for abs.pos (10.1).
    end = AutoIfOutside(end, aGridStart, aGridEnd);
    return LineRange(kAutoLine, end);
  }

  if (aEnd.IsAuto()) {
    uint32_t from = aStart.mInteger < 0 ? aExplicitGridEnd + 1: 0;
    int32_t start =
      ResolveLine(aStart, aStart.mInteger, from, aNameMap, aAreaStart,
                  aAreaEnd, aExplicitGridEnd, eLineRangeSideStart, aStyle);
    if (aStart.mHasSpan) {
      start = std::max(aGridEnd - start, aGridStart);
    }
    start = AutoIfOutside(start, aGridStart, aGridEnd);
    return LineRange(start, kAutoLine);
  }

  LineRange r = ResolveLineRange(aStart, aEnd, aNameMap, aAreaStart,
                                 aAreaEnd, aExplicitGridEnd, aStyle);
  if (r.IsAuto()) {
    MOZ_ASSERT(aStart.mHasSpan && aEnd.mHasSpan, "span / span is the only case "
               "leading to IsAuto here -- we dealt with the other cases above");
    // The second span was ignored per 9.2.1.  For abs.pos., 10.1 says that this
    // case should result in "auto / auto" unlike normal flow grid items.
    return LineRange(kAutoLine, kAutoLine);
  }

  return LineRange(AutoIfOutside(r.mUntranslatedStart, aGridStart, aGridEnd),
                   AutoIfOutside(r.mUntranslatedEnd, aGridStart, aGridEnd));
}

uint32_t
nsGridContainerFrame::FindAutoCol(uint32_t aStartCol, uint32_t aLockedRow,
                                  const GridArea* aArea) const
{
  const uint32_t extent = aArea->mCols.Extent();
  const uint32_t iStart = aLockedRow;
  const uint32_t iEnd = iStart + aArea->mRows.Extent();
  uint32_t candidate = aStartCol;
  for (uint32_t i = iStart; i < iEnd; ) {
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

nsGridContainerFrame::GridArea
nsGridContainerFrame::PlaceAbsPos(nsIFrame* aChild,
                                  const LineNameMap& aColLineNameMap,
                                  const LineNameMap& aRowLineNameMap,
                                  const nsStylePosition* aStyle)
{
  const nsStylePosition* itemStyle = aChild->StylePosition();
  int32_t gridColStart = 1 - mExplicitGridOffsetCol;
  int32_t gridRowStart = 1 - mExplicitGridOffsetRow;
  return GridArea(
    ResolveAbsPosLineRange(itemStyle->mGridColumnStart,
                           itemStyle->mGridColumnEnd,
                           aColLineNameMap,
                           &GridNamedArea::mColumnStart,
                           &GridNamedArea::mColumnEnd,
                           mExplicitGridColEnd, gridColStart, mGridColEnd,
                           aStyle),
    ResolveAbsPosLineRange(itemStyle->mGridRowStart,
                           itemStyle->mGridRowEnd,
                           aRowLineNameMap,
                           &GridNamedArea::mRowStart,
                           &GridNamedArea::mRowEnd,
                           mExplicitGridRowEnd, gridRowStart, mGridRowEnd,
                           aStyle));
}

void
nsGridContainerFrame::PlaceAutoCol(uint32_t aStartCol, GridArea* aArea) const
{
  MOZ_ASSERT(aArea->mRows.IsDefinite() && aArea->mCols.IsAuto());
  uint32_t col = FindAutoCol(aStartCol, aArea->mRows.mStart, aArea);
  aArea->mCols.ResolveAutoPosition(col, mExplicitGridOffsetCol);
  MOZ_ASSERT(aArea->IsDefinite());
}

uint32_t
nsGridContainerFrame::FindAutoRow(uint32_t aLockedCol, uint32_t aStartRow,
                                  const GridArea* aArea) const
{
  const uint32_t extent = aArea->mRows.Extent();
  const uint32_t jStart = aLockedCol;
  const uint32_t jEnd = jStart + aArea->mCols.Extent();
  const uint32_t iEnd = mCellMap.mCells.Length();
  uint32_t candidate = aStartRow;
  // Find the first gap in the rows that's at least 'extent' tall.
  // ('gap' tracks how tall the current row gap is.)
  for (uint32_t i = candidate, gap = 0; i < iEnd && gap < extent; ++i) {
    ++gap; // tentative, but we may reset it below if a column is occupied
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

void
nsGridContainerFrame::PlaceAutoRow(uint32_t aStartRow, GridArea* aArea) const
{
  MOZ_ASSERT(aArea->mCols.IsDefinite() && aArea->mRows.IsAuto());
  uint32_t row = FindAutoRow(aArea->mCols.mStart, aStartRow, aArea);
  aArea->mRows.ResolveAutoPosition(row, mExplicitGridOffsetRow);
  MOZ_ASSERT(aArea->IsDefinite());
}

void
nsGridContainerFrame::PlaceAutoAutoInRowOrder(uint32_t aStartCol,
                                              uint32_t aStartRow,
                                              GridArea* aArea) const
{
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
  aArea->mCols.ResolveAutoPosition(col, mExplicitGridOffsetCol);
  aArea->mRows.ResolveAutoPosition(row, mExplicitGridOffsetRow);
  MOZ_ASSERT(aArea->IsDefinite());
}

void
nsGridContainerFrame::PlaceAutoAutoInColOrder(uint32_t aStartCol,
                                              uint32_t aStartRow,
                                              GridArea* aArea) const
{
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
  aArea->mCols.ResolveAutoPosition(col, mExplicitGridOffsetCol);
  aArea->mRows.ResolveAutoPosition(row, mExplicitGridOffsetRow);
  MOZ_ASSERT(aArea->IsDefinite());
}

void
nsGridContainerFrame::PlaceGridItems(GridReflowState& aState,
                                     const LogicalSize& aComputedMinSize,
                                     const LogicalSize& aComputedSize,
                                     const LogicalSize& aComputedMaxSize)
{
  const nsStylePosition* const gridStyle = aState.mGridStyle;
  mCellMap.ClearOccupied();

  // http://dev.w3.org/csswg/css-grid/#grid-definition
  // Initialize the end lines of the Explicit Grid (mExplicitGridCol[Row]End).
  // This is determined by the larger of the number of rows/columns defined
  // by 'grid-template-areas' and the 'grid-template-rows'/'-columns', plus one.
  // Also initialize the Implicit Grid (mGridCol[Row]End) to the same values.
  // Note that this is for a grid with a 1,1 origin.  We'll change that
  // to a 0,0 based grid after placing definite lines.
  auto areas = gridStyle->mGridTemplateAreas.get();
  uint32_t numRepeatCols = aState.mColFunctions.InitRepeatTracks(
                             gridStyle->mGridColumnGap,
                             aComputedMinSize.ISize(aState.mWM),
                             aComputedSize.ISize(aState.mWM),
                             aComputedMaxSize.ISize(aState.mWM));
  mGridColEnd = mExplicitGridColEnd =
    aState.mColFunctions.ComputeExplicitGridEnd(areas ? areas->mNColumns + 1 : 1);
  LineNameMap colLineNameMap(gridStyle->mGridTemplateColumns, numRepeatCols);

  uint32_t numRepeatRows = aState.mRowFunctions.InitRepeatTracks(
                             gridStyle->mGridRowGap,
                             aComputedMinSize.BSize(aState.mWM),
                             aComputedSize.BSize(aState.mWM),
                             aComputedMaxSize.BSize(aState.mWM));
  mGridRowEnd = mExplicitGridRowEnd =
    aState.mRowFunctions.ComputeExplicitGridEnd(areas ? areas->NRows() + 1 : 1);
  LineNameMap rowLineNameMap(gridStyle->mGridTemplateRows, numRepeatRows);

  // http://dev.w3.org/csswg/css-grid/#line-placement
  // Resolve definite positions per spec chap 9.2.
  int32_t minCol = 1;
  int32_t minRow = 1;
  mGridItems.ClearAndRetainStorage();
  for (; !aState.mIter.AtEnd(); aState.mIter.Next()) {
    nsIFrame* child = *aState.mIter;
    GridItemInfo* info =
      mGridItems.AppendElement(GridItemInfo(PlaceDefinite(child,
                                                          colLineNameMap,
                                                          rowLineNameMap,
                                                          gridStyle)));
#ifdef DEBUG
    MOZ_ASSERT(aState.mIter.GridItemIndex() == mGridItems.Length() - 1,
               "GridItemIndex() is broken");
    info->mFrame = child;
#endif
    GridArea& area = info->mArea;
    if (area.mCols.IsDefinite()) {
      minCol = std::min(minCol, area.mCols.mUntranslatedStart);
    }
    if (area.mRows.IsDefinite()) {
      minRow = std::min(minRow, area.mRows.mUntranslatedStart);
    }
  }

  // Translate the whole grid so that the top-/left-most area is at 0,0.
  mExplicitGridOffsetCol = 1 - minCol; // minCol/Row is always <= 1, see above
  mExplicitGridOffsetRow = 1 - minRow;
  aState.mColFunctions.mExplicitGridOffset = mExplicitGridOffsetCol;
  aState.mRowFunctions.mExplicitGridOffset = mExplicitGridOffsetRow;
  const int32_t offsetToColZero = int32_t(mExplicitGridOffsetCol) - 1;
  const int32_t offsetToRowZero = int32_t(mExplicitGridOffsetRow) - 1;
  mGridColEnd += offsetToColZero;
  mGridRowEnd += offsetToRowZero;
  aState.mIter.Reset();
  for (; !aState.mIter.AtEnd(); aState.mIter.Next()) {
    GridArea& area = mGridItems[aState.mIter.GridItemIndex()].mArea;
    if (area.mCols.IsDefinite()) {
      area.mCols.mStart = area.mCols.mUntranslatedStart + offsetToColZero;
      area.mCols.mEnd = area.mCols.mUntranslatedEnd + offsetToColZero;
    }
    if (area.mRows.IsDefinite()) {
      area.mRows.mStart = area.mRows.mUntranslatedStart + offsetToRowZero;
      area.mRows.mEnd = area.mRows.mUntranslatedEnd + offsetToRowZero;
    }
    if (area.IsDefinite()) {
      mCellMap.Fill(area);
      InflateGridFor(area);
    }
  }

  // http://dev.w3.org/csswg/css-grid/#auto-placement-algo
  // Step 1, place 'auto' items that have one definite position -
  // definite row (column) for grid-auto-flow:row (column).
  auto flowStyle = gridStyle->mGridAutoFlow;
  const bool isRowOrder = (flowStyle & NS_STYLE_GRID_AUTO_FLOW_ROW);
  const bool isSparse = !(flowStyle & NS_STYLE_GRID_AUTO_FLOW_DENSE);
  // We need 1 cursor per row (or column) if placement is sparse.
  {
    Maybe<nsDataHashtable<nsUint32HashKey, uint32_t>> cursors;
    if (isSparse) {
      cursors.emplace();
    }
    auto placeAutoMinorFunc = isRowOrder ? &nsGridContainerFrame::PlaceAutoCol
                                         : &nsGridContainerFrame::PlaceAutoRow;
    aState.mIter.Reset();
    for (; !aState.mIter.AtEnd(); aState.mIter.Next()) {
      GridArea& area = mGridItems[aState.mIter.GridItemIndex()].mArea;
      LineRange& major = isRowOrder ? area.mRows : area.mCols;
      LineRange& minor = isRowOrder ? area.mCols : area.mRows;
      if (major.IsDefinite() && minor.IsAuto()) {
        // Items with 'auto' in the minor dimension only.
        uint32_t cursor = 0;
        if (isSparse) {
          cursors->Get(major.mStart, &cursor);
        }
        (this->*placeAutoMinorFunc)(cursor, &area);
        mCellMap.Fill(area);
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
  uint32_t cursorMajor = 0; // for 'dense' these two cursors will stay at 0,0
  uint32_t cursorMinor = 0;
  auto placeAutoMajorFunc = isRowOrder ? &nsGridContainerFrame::PlaceAutoRow
                                       : &nsGridContainerFrame::PlaceAutoCol;
  aState.mIter.Reset();
  for (; !aState.mIter.AtEnd(); aState.mIter.Next()) {
    GridArea& area = mGridItems[aState.mIter.GridItemIndex()].mArea;
    MOZ_ASSERT(*aState.mIter == mGridItems[aState.mIter.GridItemIndex()].mFrame,
               "iterator out of sync with mGridItems");
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
        (this->*placeAutoMajorFunc)(cursorMajor, &area);
        if (isSparse) {
          cursorMajor = major.mStart;
        }
      } else {
        // Items with 'auto' in both dimensions.
        if (isRowOrder) {
          PlaceAutoAutoInRowOrder(cursorMinor, cursorMajor, &area);
        } else {
          PlaceAutoAutoInColOrder(cursorMajor, cursorMinor, &area);
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
      mCellMap.Fill(area);
      InflateGridFor(area);
    }
  }

  if (IsAbsoluteContainer()) {
    // 9.4 Absolutely-positioned Grid Items
    // http://dev.w3.org/csswg/css-grid/#abspos-items
    // We only resolve definite lines here; we'll align auto positions to the
    // grid container later during reflow.
    nsFrameList children(GetChildList(GetAbsoluteListID()));
    const int32_t offsetToColZero = int32_t(mExplicitGridOffsetCol) - 1;
    const int32_t offsetToRowZero = int32_t(mExplicitGridOffsetRow) - 1;
    // Untranslate the grid again temporarily while resolving abs.pos. lines.
    AutoRestore<uint32_t> save1(mGridColEnd);
    AutoRestore<uint32_t> save2(mGridRowEnd);
    mGridColEnd -= offsetToColZero;
    mGridRowEnd -= offsetToRowZero;
    mAbsPosItems.ClearAndRetainStorage();
    size_t i = 0;
    for (nsFrameList::Enumerator e(children); !e.AtEnd(); e.Next(), ++i) {
      nsIFrame* child = e.get();
      GridItemInfo* info =
        mAbsPosItems.AppendElement(GridItemInfo(PlaceAbsPos(child,
                                                            colLineNameMap,
                                                            rowLineNameMap,
                                                            gridStyle)));
#ifdef DEBUG
      info->mFrame = child;
#endif
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
    }
  }

  // Count empty 'auto-fit' tracks at the end of the repeat() range.
  uint32_t numEmptyCols = 0;
  if (aState.mColFunctions.mHasRepeatAuto &&
      !gridStyle->mGridTemplateColumns.mIsAutoFill &&
      aState.mColFunctions.NumRepeatTracks() > 0) {
    for (int32_t start = aState.mColFunctions.mRepeatAutoStart,
                   col = aState.mColFunctions.mRepeatAutoEnd - 1;
         col >= start && mCellMap.IsEmptyCol(col);
         --col) {
      ++numEmptyCols;
    }
  }
  uint32_t numEmptyRows = 0;
  if (aState.mRowFunctions.mHasRepeatAuto &&
      !gridStyle->mGridTemplateRows.mIsAutoFill &&
      aState.mRowFunctions.NumRepeatTracks() > 0) {
    for (int32_t start = aState.mRowFunctions.mRepeatAutoStart,
                   row = aState.mRowFunctions.mRepeatAutoEnd - 1;
         row >= start && mCellMap.IsEmptyRow(row);
         --row) {
      ++numEmptyRows;
    }
  }
  // Remove the empty 'auto-fit' tracks we found above, if any.
  if (numEmptyCols || numEmptyRows) {
    // Adjust the line numbers in the grid areas.
    const uint32_t firstRemovedCol =
      aState.mColFunctions.mRepeatAutoEnd - numEmptyCols;
    const uint32_t firstRemovedRow =
      aState.mRowFunctions.mRepeatAutoEnd - numEmptyRows;
    for (auto& item : mGridItems) {
      GridArea& area = item.mArea;
      area.mCols.AdjustForRemovedTracks(firstRemovedCol, numEmptyCols);
      area.mRows.AdjustForRemovedTracks(firstRemovedRow, numEmptyRows);
    }
    for (auto& item : mAbsPosItems) {
      GridArea& area = item.mArea;
      area.mCols.AdjustAbsPosForRemovedTracks(firstRemovedCol, numEmptyCols);
      area.mRows.AdjustAbsPosForRemovedTracks(firstRemovedRow, numEmptyRows);
    }
    // Adjust the grid size.
    mGridColEnd -= numEmptyCols;
    mExplicitGridColEnd -= numEmptyCols;
    mGridRowEnd -= numEmptyRows;
    mExplicitGridRowEnd -= numEmptyRows;
    // Adjust the track mapping to unmap the removed tracks.
    auto finalColRepeatCount = aState.mColFunctions.NumRepeatTracks() - numEmptyCols;
    aState.mColFunctions.SetNumRepeatTracks(finalColRepeatCount);
    auto finalRowRepeatCount = aState.mRowFunctions.NumRepeatTracks() - numEmptyRows;
    aState.mRowFunctions.SetNumRepeatTracks(finalRowRepeatCount);
  }
}

void
nsGridContainerFrame::TrackSize::Initialize(nscoord aPercentageBasis,
                                            const nsStyleCoord& aMinCoord,
                                            const nsStyleCoord& aMaxCoord)
{
  MOZ_ASSERT(mBase == 0 && mLimit == 0 && mState == 0,
             "track size data is expected to be initialized to zero");
  // http://dev.w3.org/csswg/css-grid/#algo-init
  switch (aMinCoord.GetUnit()) {
    case eStyleUnit_Auto:
      mState = eAutoMinSizing;
      break;
    case eStyleUnit_Enumerated:
      mState = IsMinContent(aMinCoord) ? eMinContentMinSizing
                                       : eMaxContentMinSizing;
      break;
    case eStyleUnit_FlexFraction:
      mState = eFlexMinSizing;
      break;
    default:
      mBase = nsRuleNode::ComputeCoordPercentCalc(aMinCoord, aPercentageBasis);
  }
  switch (aMaxCoord.GetUnit()) {
    case eStyleUnit_Auto:
      mState |= eAutoMaxSizing;
      mLimit = NS_UNCONSTRAINEDSIZE;
      break;
    case eStyleUnit_Enumerated:
      mState |= IsMinContent(aMaxCoord) ? eMinContentMaxSizing
                                        : eMaxContentMaxSizing;
      mLimit = NS_UNCONSTRAINEDSIZE;
      break;
    case eStyleUnit_FlexFraction:
      mState |= eFlexMaxSizing;
      mLimit = mBase;
      break;
    default:
      mLimit = nsRuleNode::ComputeCoordPercentCalc(aMaxCoord, aPercentageBasis);
      if (mLimit < mBase) {
        mLimit = mBase;
      }
  }
}

void
nsGridContainerFrame::Tracks::Initialize(
  const TrackSizingFunctions& aFunctions,
  nscoord                     aGridGap,
  uint32_t                    aNumTracks,
  nscoord                     aContentBoxSize)
{
  MOZ_ASSERT(aNumTracks >= aFunctions.mExplicitGridOffset +
                             aFunctions.NumExplicitTracks());
  mSizes.SetLength(aNumTracks);
  PodZero(mSizes.Elements(), mSizes.Length());
  nscoord percentageBasis = aContentBoxSize;
  if (percentageBasis == NS_UNCONSTRAINEDSIZE) {
    percentageBasis = 0;
  }
  for (uint32_t i = 0, len = mSizes.Length(); i < len; ++i) {
    mSizes[i].Initialize(percentageBasis,
                         aFunctions.MinSizingFor(i),
                         aFunctions.MaxSizingFor(i));
  }
  mGridGap = aGridGap;
  MOZ_ASSERT(mGridGap >= nscoord(0), "negative grid gap");
}

/**
 * Return the [min|max]-content contribution of aChild to its parent (i.e.
 * the child's margin-box) in aAxis.
 */
static nscoord
ContentContribution(nsIFrame*                         aChild,
                    const nsHTMLReflowState*          aReflowState,
                    nsRenderingContext*               aRC,
                    WritingMode                       aCBWM,
                    LogicalAxis                       aAxis,
                    nsLayoutUtils::IntrinsicISizeType aConstraint,
                    uint32_t                          aFlags = 0)
{
  PhysicalAxis axis(aCBWM.PhysicalAxis(aAxis));
  nscoord size = nsLayoutUtils::IntrinsicForAxis(axis, aRC, aChild, aConstraint,
                   aFlags | nsLayoutUtils::BAIL_IF_REFLOW_NEEDED);
  if (size == NS_INTRINSIC_WIDTH_UNKNOWN) {
    // We need to reflow the child to find its BSize contribution.
    WritingMode wm = aChild->GetWritingMode();
    nsContainerFrame* parent = aChild->GetParent();
    nsPresContext* pc = aChild->PresContext();
    Maybe<nsHTMLReflowState> dummyParentState;
    const nsHTMLReflowState* rs = aReflowState;
    if (!aReflowState) {
      MOZ_ASSERT(!parent->HasAnyStateBits(NS_FRAME_IN_REFLOW));
      dummyParentState.emplace(pc, parent, aRC,
                               LogicalSize(parent->GetWritingMode(), 0,
                                           NS_UNCONSTRAINEDSIZE),
                               nsHTMLReflowState::DUMMY_PARENT_REFLOW_STATE);
      rs = dummyParentState.ptr();
    }
#ifdef DEBUG
    // This will suppress various CRAZY_SIZE warnings for this reflow.
    parent->Properties().Set(nsContainerFrame::DebugReflowingWithInfiniteISize(),
                             parent /* anything non-null will do */);
#endif
    // XXX this will give mostly correct results for now (until bug 1174569).
    LogicalSize availableSize(wm, INFINITE_ISIZE_COORD, NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowState childRS(pc, *rs, aChild, availableSize, nullptr,
                              nsHTMLReflowState::COMPUTE_SIZE_SHRINK_WRAP);
    nsHTMLReflowMetrics childSize(childRS);
    nsReflowStatus childStatus;
    const uint32_t flags = NS_FRAME_NO_MOVE_FRAME | NS_FRAME_NO_SIZE_VIEW;
    parent->ReflowChild(aChild, pc, childSize, childRS, wm,
                        LogicalPoint(wm), nsSize(), flags, childStatus);
    parent->FinishReflowChild(aChild, pc, childSize, &childRS, wm,
                              LogicalPoint(wm), nsSize(), flags);
    size = childSize.BSize(wm);
    nsIFrame::IntrinsicISizeOffsetData offsets = aChild->IntrinsicBSizeOffsets();
    size += offsets.hMargin;
    size = nsLayoutUtils::AddPercents(aConstraint, size, offsets.hPctMargin);
#ifdef DEBUG
    parent->Properties().Delete(nsContainerFrame::DebugReflowingWithInfiniteISize());
#endif
  }
  return std::max(size, 0);
}

static nscoord
MinContentContribution(nsIFrame*                aChild,
                       const nsHTMLReflowState* aRS,
                       nsRenderingContext*      aRC,
                       WritingMode              aCBWM,
                       LogicalAxis              aAxis)
{
  return ContentContribution(aChild, aRS, aRC, aCBWM, aAxis,
                             nsLayoutUtils::MIN_ISIZE);
}

static nscoord
MaxContentContribution(nsIFrame*                aChild,
                       const nsHTMLReflowState* aRS,
                       nsRenderingContext*      aRC,
                       WritingMode              aCBWM,
                       LogicalAxis              aAxis)
{
  return ContentContribution(aChild, aRS, aRC, aCBWM, aAxis,
                             nsLayoutUtils::PREF_ISIZE);
}

static nscoord
MinSize(nsIFrame*                aChild,
        const nsHTMLReflowState* aRS,
        nsRenderingContext*      aRC,
        WritingMode              aCBWM,
        LogicalAxis              aAxis)
{
  PhysicalAxis axis(aCBWM.PhysicalAxis(aAxis));
  const nsStylePosition* stylePos = aChild->StylePosition();
  const nsStyleCoord& style = axis == eAxisHorizontal ? stylePos->mMinWidth
                                                      : stylePos->mMinHeight;
  // https://drafts.csswg.org/css-grid/#min-size-auto
  // This calculates the min-content contribution from either a definite
  // min-width (or min-height depending on aAxis), or the "specified /
  // transferred size" for min-width:auto if overflow == visible (as min-width:0
  // otherwise), or NS_UNCONSTRAINEDSIZE for other min-width intrinsic values
  // (which results in always taking the "content size" part below).
  nscoord sz =
    nsLayoutUtils::MinSizeContributionForAxis(axis, aRC, aChild,
                                              nsLayoutUtils::MIN_ISIZE);
  auto unit = style.GetUnit();
  if (unit == eStyleUnit_Enumerated ||
      (unit == eStyleUnit_Auto &&
       aChild->StyleDisplay()->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE)) {
    // Now calculate the "content size" part and return whichever is smaller.
    MOZ_ASSERT(unit != eStyleUnit_Enumerated || sz == NS_UNCONSTRAINEDSIZE);
    sz = std::min(sz, ContentContribution(aChild, aRS, aRC, aCBWM, aAxis,
                                          nsLayoutUtils::MIN_ISIZE,
                                          nsLayoutUtils::MIN_INTRINSIC_ISIZE));
  }
  return sz;
}

void
nsGridContainerFrame::Tracks::CalculateSizes(
  GridReflowState&            aState,
  nsTArray<GridItemInfo>&     aGridItems,
  const TrackSizingFunctions& aFunctions,
  nscoord                     aContentBoxSize,
  LineRange GridArea::*       aRange,
  IntrinsicISizeType          aConstraint)
{
  nscoord percentageBasis = aContentBoxSize;
  if (percentageBasis == NS_UNCONSTRAINEDSIZE) {
    percentageBasis = 0;
  }
  ResolveIntrinsicSize(aState, aGridItems, aFunctions, aRange, percentageBasis,
                       aConstraint);
  if (aConstraint != nsLayoutUtils::MIN_ISIZE) {
    nscoord freeSpace = aContentBoxSize;
    if (freeSpace != NS_UNCONSTRAINEDSIZE) {
      freeSpace -= SumOfGridGaps();
    }
    DistributeFreeSpace(freeSpace);
    StretchFlexibleTracks(aState, aGridItems, aFunctions, freeSpace);
  }
}

void
nsGridContainerFrame::CalculateTrackSizes(GridReflowState&   aState,
                                          const LogicalSize& aContentBox,
                                          IntrinsicISizeType aConstraint)
{
  const WritingMode& wm = aState.mWM;
  const nsStylePosition* stylePos = aState.mGridStyle;
  aState.mCols.Initialize(aState.mColFunctions, stylePos->mGridColumnGap,
                          mGridColEnd, aContentBox.ISize(wm));
  aState.mRows.Initialize(aState.mRowFunctions, stylePos->mGridRowGap,
                          mGridRowEnd, aContentBox.BSize(wm));

  aState.mCols.CalculateSizes(aState, mGridItems, aState.mColFunctions,
                              aContentBox.ISize(wm), &GridArea::mCols,
                              aConstraint);

  aState.mIter.Reset(); // XXX cleanup this Reset mess!
  aState.mRows.CalculateSizes(aState, mGridItems, aState.mRowFunctions,
                              aContentBox.BSize(wm), &GridArea::mRows,
                              aConstraint);
}

bool
nsGridContainerFrame::Tracks::HasIntrinsicButNoFlexSizingInRange(
  const LineRange&      aRange,
  IntrinsicISizeType    aConstraint,
  TrackSize::StateBits* aState) const
{
  MOZ_ASSERT(!aRange.IsAuto(), "must have a definite range");
  const uint32_t start = aRange.mStart;
  const uint32_t end = aRange.mEnd;
  const TrackSize::StateBits selector =
    TrackSize::eIntrinsicMinSizing |
    TrackSize::eIntrinsicMaxSizing |
    (aConstraint == nsLayoutUtils::MIN_ISIZE ? TrackSize::eFlexMinSizing
                                             : TrackSize::StateBits(0));
  bool foundIntrinsic = false;
  for (uint32_t i = start; i < end; ++i) {
    TrackSize::StateBits state = mSizes[i].mState;
    *aState |= state;
    if (state & TrackSize::eFlexMaxSizing) {
      return false;
    }
    if (state & selector) {
      foundIntrinsic = true;
    }
  }
  return foundIntrinsic;
}

bool
nsGridContainerFrame::Tracks::ResolveIntrinsicSizeStep1(
  GridReflowState&            aState,
  const TrackSizingFunctions& aFunctions,
  nscoord                     aPercentageBasis,
  IntrinsicISizeType          aConstraint,
  const LineRange&            aRange,
  nsIFrame*                   aGridItem)
{
  Maybe<nscoord> minContentContribution;
  Maybe<nscoord> maxContentContribution;
  // min sizing
  TrackSize& sz = mSizes[aRange.mStart];
  WritingMode wm = aState.mWM;
  const nsHTMLReflowState* rs = aState.mReflowState;
  nsRenderingContext* rc = &aState.mRenderingContext;
  if (sz.mState & TrackSize::eAutoMinSizing) {
    nscoord s = MinSize(aGridItem, rs, rc, wm, mAxis);
    sz.mBase = std::max(sz.mBase, s);
  } else if ((sz.mState & TrackSize::eMinContentMinSizing) ||
             (aConstraint == nsLayoutUtils::MIN_ISIZE &&
              (sz.mState & TrackSize::eFlexMinSizing))) {
    nscoord s = MinContentContribution(aGridItem, rs, rc, wm, mAxis);
    minContentContribution.emplace(s);
    sz.mBase = std::max(sz.mBase, minContentContribution.value());
  } else if (sz.mState & TrackSize::eMaxContentMinSizing) {
    nscoord s = MaxContentContribution(aGridItem, rs, rc, wm, mAxis);
    maxContentContribution.emplace(s);
    sz.mBase = std::max(sz.mBase, maxContentContribution.value());
  }
  // max sizing
  if (sz.mState & TrackSize::eMinContentMaxSizing) {
    if (minContentContribution.isNothing()) {
      nscoord s = MinContentContribution(aGridItem, rs, rc, wm, mAxis);
      minContentContribution.emplace(s);
    }
    if (sz.mLimit == NS_UNCONSTRAINEDSIZE) {
      sz.mLimit = minContentContribution.value();
    } else {
      sz.mLimit = std::max(sz.mLimit, minContentContribution.value());
    }
  } else if (sz.mState & (TrackSize::eAutoMaxSizing |
                          TrackSize::eMaxContentMaxSizing)) {
    if (maxContentContribution.isNothing()) {
      nscoord s = MaxContentContribution(aGridItem, rs, rc, wm, mAxis);
      maxContentContribution.emplace(s);
    }
    if (sz.mLimit == NS_UNCONSTRAINEDSIZE) {
      sz.mLimit = maxContentContribution.value();
    } else {
      sz.mLimit = std::max(sz.mLimit, maxContentContribution.value());
    }
  }
  if (sz.mLimit < sz.mBase) {
    sz.mLimit = sz.mBase;
  }
  return sz.mState & TrackSize::eFlexMaxSizing;
}

void
nsGridContainerFrame::Tracks::ResolveIntrinsicSize(
  GridReflowState&            aState,
  nsTArray<GridItemInfo>&     aGridItems,
  const TrackSizingFunctions& aFunctions,
  LineRange GridArea::*       aRange,
  nscoord                     aPercentageBasis,
  IntrinsicISizeType          aConstraint)
{
  // Some data we collect on each item for Step 2 of the algorithm below.
  struct Step2ItemData
  {
    uint32_t mSpan;
    TrackSize::StateBits mState;
    LineRange mLineRange;
    nscoord mMinSize;
    nscoord mMinContentContribution;
    nscoord mMaxContentContribution;
    nsIFrame* mFrame;
    static bool IsSpanLessThan(const Step2ItemData& a, const Step2ItemData& b)
    {
      return a.mSpan < b.mSpan;
    }
  };

  // Resolve Intrinsic Track Sizes
  // http://dev.w3.org/csswg/css-grid/#algo-content
  // We're also setting mIsFlexing on the item here to speed up
  // FindUsedFlexFraction later.
  nsAutoTArray<TrackSize::StateBits, 16> stateBitsPerSpan;
  nsTArray<Step2ItemData> step2Items;
  GridItemCSSOrderIterator& iter = aState.mIter;
  nsRenderingContext* rc = &aState.mRenderingContext;
  WritingMode wm = aState.mWM;
  uint32_t maxSpan = 0; // max span of the step2Items items
  const TrackSize::StateBits flexMin =
    aConstraint == nsLayoutUtils::MIN_ISIZE ? TrackSize::eFlexMinSizing
                                            : TrackSize::StateBits(0);
  for (; !iter.AtEnd(); iter.Next()) {
    nsIFrame* child = *iter;
    const GridArea& area = aGridItems[iter.GridItemIndex()].mArea;
    const LineRange& lineRange = area.*aRange;
    uint32_t span = lineRange.Extent();
    if (span == 1) {
      // Step 1. Size tracks to fit non-spanning items.
      aGridItems[iter.GridItemIndex()].mIsFlexing[mAxis] =
        ResolveIntrinsicSizeStep1(aState, aFunctions, aPercentageBasis,
                                  aConstraint, lineRange, child);
    } else {
      TrackSize::StateBits state = TrackSize::StateBits(0);
      if (HasIntrinsicButNoFlexSizingInRange(lineRange, aConstraint, &state)) {
        // Collect data for Step 2.
        maxSpan = std::max(maxSpan, span);
        if (span >= stateBitsPerSpan.Length()) {
          uint32_t len = 2 * span;
          stateBitsPerSpan.SetCapacity(len);
          for (uint32_t i = stateBitsPerSpan.Length(); i < len; ++i) {
            stateBitsPerSpan.AppendElement(TrackSize::StateBits(0));
          }
        }
        stateBitsPerSpan[span] |= state;
        nscoord minSize = 0;
        if (state & (flexMin | TrackSize::eIntrinsicMinSizing)) { // for 2.1
          minSize = MinSize(child, aState.mReflowState, rc, wm, mAxis);
        }
        nscoord minContent = 0;
        if (state & (flexMin | TrackSize::eMinOrMaxContentMinSizing | // for 2.2
                     TrackSize::eIntrinsicMaxSizing)) {               // for 2.5
          minContent = MinContentContribution(child, aState.mReflowState,
                                              rc, wm, mAxis);
        }
        nscoord maxContent = 0;
        if (state & (TrackSize::eMaxContentMinSizing |         // for 2.3
                     TrackSize::eAutoOrMaxContentMaxSizing)) { // for 2.6
          maxContent = MaxContentContribution(child, aState.mReflowState,
                                              rc, wm, mAxis);
        }
        step2Items.AppendElement(
          Step2ItemData({span, state, lineRange, minSize,
                         minContent, maxContent, child}));
      } else {
        aGridItems[iter.GridItemIndex()].mIsFlexing[mAxis] =
          !!(state & TrackSize::eFlexMaxSizing);
      }
    }
  }

  // Step 2.
  if (maxSpan) {
    // Sort the collected items on span length, shortest first.
    std::stable_sort(step2Items.begin(), step2Items.end(),
                     Step2ItemData::IsSpanLessThan);

    nsTArray<uint32_t> tracks(maxSpan);
    nsTArray<TrackSize> plan(mSizes.Length());
    plan.SetLength(mSizes.Length());
    for (uint32_t i = 0, len = step2Items.Length(); i < len; ) {
      // Start / end index for items of the same span length:
      const uint32_t spanGroupStartIndex = i;
      uint32_t spanGroupEndIndex = len;
      const uint32_t span = step2Items[i].mSpan;
      for (++i; i < len; ++i) {
        if (step2Items[i].mSpan != span) {
          spanGroupEndIndex = i;
          break;
        }
      }

      bool updatedBase = false; // Did we update any mBase in step 2.1 - 2.3?
      TrackSize::StateBits selector(flexMin | TrackSize::eIntrinsicMinSizing);
      if (stateBitsPerSpan[span] & selector) {
        // Step 2.1 MinSize to intrinsic min-sizing.
        for (i = spanGroupStartIndex; i < spanGroupEndIndex; ++i) {
          Step2ItemData& item = step2Items[i];
          if (!(item.mState & selector)) {
            continue;
          }
          nscoord space = item.mMinSize;
          if (space <= 0) {
            continue;
          }
          tracks.ClearAndRetainStorage();
          space = CollectGrowable(space, mSizes, item.mLineRange, selector,
                                  tracks);
          if (space > 0) {
            DistributeToTrackBases(space, plan, tracks, selector);
            updatedBase = true;
          }
        }
      }

      selector = flexMin | TrackSize::eMinOrMaxContentMinSizing;
      if (stateBitsPerSpan[span] & selector) {
        // Step 2.2 MinContentContribution to min-/max-content min-sizing.
        for (i = spanGroupStartIndex; i < spanGroupEndIndex; ++i) {
          Step2ItemData& item = step2Items[i];
          if (!(item.mState & selector)) {
            continue;
          }
          nscoord space = item.mMinContentContribution;
          if (space <= 0) {
            continue;
          }
          tracks.ClearAndRetainStorage();
          space = CollectGrowable(space, mSizes, item.mLineRange, selector,
                                  tracks);
          if (space > 0) {
            DistributeToTrackBases(space, plan, tracks, selector);
            updatedBase = true;
          }
        }
      }

      if (stateBitsPerSpan[span] & TrackSize::eMaxContentMinSizing) {
        // Step 2.3 MaxContentContribution to max-content min-sizing.
        for (i = spanGroupStartIndex; i < spanGroupEndIndex; ++i) {
          Step2ItemData& item = step2Items[i];
          if (!(item.mState & TrackSize::eMaxContentMinSizing)) {
            continue;
          }
          nscoord space = item.mMaxContentContribution;
          if (space <= 0) {
            continue;
          }
          tracks.ClearAndRetainStorage();
          space = CollectGrowable(space, mSizes, item.mLineRange,
                                  TrackSize::eMaxContentMinSizing,
                                  tracks);
          if (space > 0) {
            DistributeToTrackBases(space, plan, tracks,
                                   TrackSize::eMaxContentMinSizing);
            updatedBase = true;
          }
        }
      }

      if (updatedBase) {
        // Step 2.4
        for (TrackSize& sz : mSizes) {
          if (sz.mBase > sz.mLimit) {
            sz.mLimit = sz.mBase;
          }
        }
      }
      if (stateBitsPerSpan[span] & TrackSize::eIntrinsicMaxSizing) {
        plan = mSizes;
        for (TrackSize& sz : plan) {
          if (sz.mLimit == NS_UNCONSTRAINEDSIZE) {
            // use mBase as the planned limit
          } else {
            sz.mBase = sz.mLimit;
          }
        }

        // Step 2.5 MinContentContribution to intrinsic max-sizing.
        for (i = spanGroupStartIndex; i < spanGroupEndIndex; ++i) {
          Step2ItemData& item = step2Items[i];
          if (!(item.mState & TrackSize::eIntrinsicMaxSizing)) {
            continue;
          }
          nscoord space = item.mMinContentContribution;
          if (space <= 0) {
            continue;
          }
          tracks.ClearAndRetainStorage();
          space = CollectGrowable(space, plan, item.mLineRange,
                                  TrackSize::eIntrinsicMaxSizing,
                                  tracks);
          if (space > 0) {
            DistributeToTrackLimits(space, plan, tracks);
          }
        }
        for (size_t j = 0, len = mSizes.Length(); j < len; ++j) {
          TrackSize& sz = plan[j];
          sz.mState &= ~(TrackSize::eFrozen | TrackSize::eSkipGrowUnlimited);
          if (sz.mLimit != NS_UNCONSTRAINEDSIZE) {
            sz.mLimit = sz.mBase;  // collect the results from 2.5
          }
        }

        if (stateBitsPerSpan[span] & TrackSize::eAutoOrMaxContentMaxSizing) {
          // Step 2.6 MaxContentContribution to max-content max-sizing.
          for (i = spanGroupStartIndex; i < spanGroupEndIndex; ++i) {
            Step2ItemData& item = step2Items[i];
            if (!(item.mState & TrackSize::eAutoOrMaxContentMaxSizing)) {
              continue;
            }
            nscoord space = item.mMaxContentContribution;
            if (space <= 0) {
              continue;
            }
            tracks.ClearAndRetainStorage();
            space = CollectGrowable(space, plan, item.mLineRange,
                                    TrackSize::eAutoOrMaxContentMaxSizing,
                                    tracks);
            if (space > 0) {
              DistributeToTrackLimits(space, plan, tracks);
            }
          }
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

float
nsGridContainerFrame::Tracks::FindFrUnitSize(
  const LineRange&            aRange,
  const nsTArray<uint32_t>&   aFlexTracks,
  const TrackSizingFunctions& aFunctions,
  nscoord                     aSpaceToFill) const
{
  MOZ_ASSERT(aSpaceToFill > 0 && !aFlexTracks.IsEmpty());
  float flexFactorSum = 0.0f;
  nscoord leftOverSpace = aSpaceToFill;
  for (uint32_t i = aRange.mStart, end = aRange.mEnd; i < end; ++i) {
    const TrackSize& sz = mSizes[i];
    if (sz.mState & TrackSize::eFlexMaxSizing) {
      flexFactorSum += aFunctions.MaxSizingFor(i).GetFlexFractionValue();
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
        continue; // Track marked as inflexible in a prev. iter of this loop.
      }
      float flexFactor = aFunctions.MaxSizingFor(track).GetFlexFractionValue();
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

float
nsGridContainerFrame::Tracks::FindUsedFlexFraction(
  GridReflowState&            aState,
  nsTArray<GridItemInfo>&     aGridItems,
  const nsTArray<uint32_t>&   aFlexTracks,
  const TrackSizingFunctions& aFunctions,
  nscoord                     aAvailableSize) const
{
  if (aAvailableSize != NS_UNCONSTRAINEDSIZE) {
    // Use all of the grid tracks and a 'space to fill' of the available space.
    const TranslatedLineRange range(0, mSizes.Length());
    return FindFrUnitSize(range, aFlexTracks, aFunctions, aAvailableSize);
  }

  // The used flex fraction is the maximum of:
  // ... each flexible track's base size divided by its flex factor
  float fr = 0.0f;
  for (uint32_t track : aFlexTracks) {
    float flexFactor = aFunctions.MaxSizingFor(track).GetFlexFractionValue();
    if (flexFactor > 0.0f) {
      fr = std::max(fr, mSizes[track].mBase / flexFactor);
    }
  }
  WritingMode wm = aState.mWM;
  nsRenderingContext* rc = &aState.mRenderingContext;
  const nsHTMLReflowState* rs = aState.mReflowState;
  GridItemCSSOrderIterator& iter = aState.mIter;
  iter.Reset();
  // ... the result of 'finding the size of an fr' for each item that spans
  // a flex track with its max-content contribution as 'space to fill'
  for (; !iter.AtEnd(); iter.Next()) {
    const GridItemInfo& item = aGridItems[iter.GridItemIndex()];
    if (item.mIsFlexing[mAxis]) {
      nscoord spaceToFill = MaxContentContribution(*iter, rs, rc, wm, mAxis);
      if (spaceToFill <= 0) {
        continue;
      }
      // ... and all its spanned tracks as input.
      const LineRange& range =
        mAxis == eLogicalAxisInline ? item.mArea.mCols : item.mArea.mRows;
      nsTArray<uint32_t> itemFlexTracks;
      for (uint32_t i = range.mStart, end = range.mEnd; i < end; ++i) {
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

void
nsGridContainerFrame::Tracks::StretchFlexibleTracks(
  GridReflowState&            aState,
  nsTArray<GridItemInfo>&     aGridItems,
  const TrackSizingFunctions& aFunctions,
  nscoord                     aAvailableSize)
{
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
  float fr = FindUsedFlexFraction(aState, aGridItems, flexTracks,
                                  aFunctions, aAvailableSize);
  if (fr != 0.0f) {
    for (uint32_t i : flexTracks) {
      float flexFactor = aFunctions.MaxSizingFor(i).GetFlexFractionValue();
      nscoord flexLength = NSToCoordRound(flexFactor * fr);
      nscoord& base = mSizes[i].mBase;
      if (flexLength > base) {
        base = flexLength;
      }
    }
  }
}

void
nsGridContainerFrame::Tracks::AlignJustifyContent(
  const nsHTMLReflowState& aReflowState,
  const LogicalSize&     aContainerSize)
{
  if (mSizes.IsEmpty()) {
    return;
  }

  const bool isAlign = mAxis == eLogicalAxisBlock;
  auto stylePos = aReflowState.mStylePosition;
  const auto valueAndFallback = isAlign ?
    stylePos->ComputedAlignContent() :
    stylePos->ComputedJustifyContent(aReflowState.mStyleDisplay);
  WritingMode wm = aReflowState.GetWritingMode();
  bool overflowSafe;
  auto alignment = ::GetAlignJustifyValue(valueAndFallback, wm, isAlign,
                                          &overflowSafe);
  if (alignment == NS_STYLE_ALIGN_AUTO) {
    alignment = NS_STYLE_ALIGN_START;
  }

  // Compute the free space and count auto-sized tracks.
  size_t numAutoTracks = 0;
  nscoord space;
  if (alignment != NS_STYLE_ALIGN_START) {
    nscoord trackSizeSum = 0;
    for (const TrackSize& sz : mSizes) {
      trackSizeSum += sz.mBase;
      if (sz.mState & TrackSize::eAutoMaxSizing) {
        ++numAutoTracks;
      }
    }
    nscoord cbSize = isAlign ? aContainerSize.BSize(wm)
                             : aContainerSize.ISize(wm);
    space = cbSize - trackSizeSum - SumOfGridGaps();
    // Use the fallback value instead when applicable.
    if (space < 0 ||
        (alignment == NS_STYLE_ALIGN_SPACE_BETWEEN && mSizes.Length() == 1)) {
      auto fallback = ::GetAlignJustifyFallbackIfAny(valueAndFallback, wm,
                                                     isAlign, &overflowSafe);
      if (fallback) {
        alignment = fallback;
      }
    }
    if (space == 0 || (space < 0 && overflowSafe)) {
      // XXX check that this makes sense also for [last-]baseline (bug 1151204).
      alignment = NS_STYLE_ALIGN_START;
    }
  }

  // Optimize the cases where we just need to set each track's position.
  nscoord pos = 0;
  bool distribute = true;
  switch (alignment) {
    case NS_STYLE_ALIGN_BASELINE:
    case NS_STYLE_ALIGN_LAST_BASELINE:
      NS_WARNING("'NYI: baseline/last-baseline' (bug 1151204)"); // XXX
    case NS_STYLE_ALIGN_START:
      distribute = false;
      break;
    case NS_STYLE_ALIGN_END:
      pos = space;
      distribute = false;
      break;
    case NS_STYLE_ALIGN_CENTER:
      pos = space / 2;
      distribute = false;
      break;
    case NS_STYLE_ALIGN_STRETCH:
      distribute = numAutoTracks != 0;
      break;
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
  switch (alignment) {
    case NS_STYLE_ALIGN_STRETCH: {
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
    case NS_STYLE_ALIGN_SPACE_BETWEEN:
      MOZ_ASSERT(mSizes.Length() > 1, "should've used a fallback above");
      roundingError = NSCoordDivRem(space, mSizes.Length() - 1, &between);
      break;
    case NS_STYLE_ALIGN_SPACE_AROUND:
      roundingError = NSCoordDivRem(space, mSizes.Length(), &between);
      pos = between / 2;
      break;
    case NS_STYLE_ALIGN_SPACE_EVENLY:
      roundingError = NSCoordDivRem(space, mSizes.Length() + 1, &between);
      pos = between;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unknown align-/justify-content value");
      between = 0; // just to avoid a compiler warning
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

void
nsGridContainerFrame::LineRange::ToPositionAndLength(
  const nsTArray<TrackSize>& aTrackSizes, nscoord* aPos, nscoord* aLength) const
{
  MOZ_ASSERT(mStart != kAutoLine && mEnd != kAutoLine,
             "expected a definite LineRange");
  MOZ_ASSERT(mStart < mEnd);
  nscoord startPos = aTrackSizes[mStart].mPosition;
  const TrackSize& sz = aTrackSizes[mEnd - 1];
  *aPos = startPos;
  *aLength = (sz.mPosition + sz.mBase) - startPos;
}

nscoord
nsGridContainerFrame::LineRange::ToLength(
  const nsTArray<TrackSize>& aTrackSizes) const
{
  MOZ_ASSERT(mStart != kAutoLine && mEnd != kAutoLine,
             "expected a definite LineRange");
  MOZ_ASSERT(mStart < mEnd);
  nscoord startPos = aTrackSizes[mStart].mPosition;
  const TrackSize& sz = aTrackSizes[mEnd - 1];
  return (sz.mPosition + sz.mBase) - startPos;
}

void
nsGridContainerFrame::LineRange::ToPositionAndLengthForAbsPos(
  const nsTArray<TrackSize>& aTrackSizes, nscoord aGridOrigin,
  nscoord* aPos, nscoord* aLength) const
{
  // kAutoLine for abspos children contributes the corresponding edge
  // of the grid container's padding-box.
  if (mEnd == kAutoLine) {
    if (mStart == kAutoLine) {
      // done
    } else {
      const nscoord endPos = *aPos + *aLength;
      nscoord startPos =
        ::GridLineEdge(mStart, aTrackSizes, GridLineSide::eAfterGridGap);
      *aPos = aGridOrigin + startPos;
      *aLength = std::max(endPos - *aPos, 0);
    }
  } else {
    if (mStart == kAutoLine) {
      nscoord endPos =
        ::GridLineEdge(mEnd, aTrackSizes, GridLineSide::eBeforeGridGap);
      *aLength = std::max(aGridOrigin + endPos, 0);
    } else {
      nscoord pos;
      ToPositionAndLength(aTrackSizes, &pos, aLength);
      *aPos = aGridOrigin + pos;
    }
  }
}

LogicalRect
nsGridContainerFrame::ContainingBlockFor(const GridReflowState& aState,
                                         const GridArea&        aArea) const
{
  nscoord i, b, iSize, bSize;
  MOZ_ASSERT(aArea.mCols.Extent() > 0, "grid items cover at least one track");
  MOZ_ASSERT(aArea.mRows.Extent() > 0, "grid items cover at least one track");
  aArea.mCols.ToPositionAndLength(aState.mCols.mSizes, &i, &iSize);
  aArea.mRows.ToPositionAndLength(aState.mRows.mSizes, &b, &bSize);
  return LogicalRect(aState.mWM, i, b, iSize, bSize);
}

LogicalRect
nsGridContainerFrame::ContainingBlockForAbsPos(const GridReflowState& aState,
                                               const GridArea&        aArea,
                                               const LogicalPoint& aGridOrigin,
                                               const LogicalRect& aGridCB) const
{
  const WritingMode& wm = aState.mWM;
  nscoord i = aGridCB.IStart(wm);
  nscoord b = aGridCB.BStart(wm);
  nscoord iSize = aGridCB.ISize(wm);
  nscoord bSize = aGridCB.BSize(wm);
  aArea.mCols.ToPositionAndLengthForAbsPos(aState.mCols.mSizes,
                                           aGridOrigin.I(wm),
                                           &i, &iSize);
  aArea.mRows.ToPositionAndLengthForAbsPos(aState.mRows.mSizes,
                                           aGridOrigin.B(wm),
                                           &b, &bSize);
  return LogicalRect(wm, i, b, iSize, bSize);
}

void
nsGridContainerFrame::ReflowChildren(GridReflowState&     aState,
                                     const LogicalRect&   aContentArea,
                                     nsHTMLReflowMetrics& aDesiredSize,
                                     nsReflowStatus&      aStatus)
{
  MOZ_ASSERT(aState.mReflowState);

  WritingMode wm = aState.mReflowState->GetWritingMode();
  const LogicalPoint gridOrigin(aContentArea.Origin(wm));
  const nsSize containerSize =
    (aContentArea.Size(wm) +
     aState.mReflowState->ComputedLogicalBorderPadding().Size(wm)).GetPhysicalSize(wm);
  nsPresContext* pc = PresContext();
  nsStyleContext* containerSC = StyleContext();
  for (; !aState.mIter.AtEnd(); aState.mIter.Next()) {
    nsIFrame* child = *aState.mIter;
    const bool isGridItem = child->GetType() != nsGkAtoms::placeholderFrame;
    LogicalRect cb(wm);
    if (MOZ_LIKELY(isGridItem)) {
      MOZ_ASSERT(mGridItems[aState.mIter.GridItemIndex()].mFrame == child,
                 "iterator out of sync with mGridItems");
      GridArea& area = mGridItems[aState.mIter.GridItemIndex()].mArea;
      MOZ_ASSERT(area.IsDefinite());
      cb = ContainingBlockFor(aState, area);
      cb += gridOrigin;
    } else {
      cb = aContentArea;
    }
    WritingMode childWM = child->GetWritingMode();
    LogicalSize childCBSize = cb.Size(wm).ConvertTo(childWM, wm);
    LogicalSize percentBasis(childCBSize);
    // XXX temporary workaround to avoid being INCOMPLETE until we have
    // support for fragmentation (bug 1144096)
    childCBSize.BSize(childWM) = NS_UNCONSTRAINEDSIZE;

    Maybe<nsHTMLReflowState> childRS; // Maybe<> so we can reuse the space
    childRS.emplace(pc, *aState.mReflowState, child, childCBSize, &percentBasis);
    // We need the width of the child before we can correctly convert
    // the writing-mode of its origin, so we reflow at (0, 0) using a dummy
    // containerSize, and then pass the correct position to FinishReflowChild.
    Maybe<nsHTMLReflowMetrics> childSize; // Maybe<> so we can reuse the space
    childSize.emplace(*childRS);
    nsReflowStatus childStatus;
    const nsSize dummyContainerSize;
    ReflowChild(child, pc, *childSize, *childRS, childWM, LogicalPoint(childWM),
                dummyContainerSize, 0, childStatus);
    LogicalPoint childPos =
      cb.Origin(wm).ConvertTo(childWM, wm,
                              containerSize - childSize->PhysicalSize());
    // Apply align/justify-self and reflow again if that affects the size.
    if (isGridItem) {
      LogicalSize oldSize = childSize->Size(childWM); // from the ReflowChild()
      LogicalSize newContentSize(childWM);
      auto align = childRS->mStylePosition->ComputedAlignSelf(
                     childRS->mStyleDisplay, containerSC);
      Maybe<LogicalAxis> alignResize =
        AlignSelf(align, cb, wm, *childRS, oldSize, &newContentSize, &childPos);
      auto justify = childRS->mStylePosition->ComputedJustifySelf(
                       childRS->mStyleDisplay, containerSC);
      Maybe<LogicalAxis> justifyResize =
        JustifySelf(justify, cb, wm, *childRS, oldSize, &newContentSize, &childPos);
      if (alignResize || justifyResize) {
        FinishReflowChild(child, pc, *childSize, childRS.ptr(), childWM,
                          LogicalPoint(childWM), containerSize,
                          NS_FRAME_NO_MOVE_FRAME | NS_FRAME_NO_SIZE_VIEW);
        childSize.reset(); // In reverse declaration order since it runs
        childRS.reset();   // destructors.
        childRS.emplace(pc, *aState.mReflowState, child, childCBSize, &percentBasis);
        if ((alignResize && alignResize.value() == eLogicalAxisBlock) ||
            (justifyResize && justifyResize.value() == eLogicalAxisBlock)) {
          childRS->SetComputedBSize(newContentSize.BSize(childWM));
          childRS->SetBResize(true);
        }
        if ((alignResize && alignResize.value() == eLogicalAxisInline) ||
            (justifyResize && justifyResize.value() == eLogicalAxisInline)) {
          childRS->SetComputedISize(newContentSize.ISize(childWM));
          childRS->SetIResize(true);
        }
        childSize.emplace(*childRS);
        ReflowChild(child, pc, *childSize, *childRS, childWM,
                    LogicalPoint(childWM), dummyContainerSize, 0, childStatus);
      }
    }
    childRS->ApplyRelativePositioning(&childPos, containerSize);
    FinishReflowChild(child, pc, *childSize, childRS.ptr(), childWM, childPos,
                      containerSize, 0);
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, child);
    // XXX deal with 'childStatus' not being COMPLETE (bug 1144096)
  }

  if (IsAbsoluteContainer()) {
    nsFrameList children(GetChildList(GetAbsoluteListID()));
    if (!children.IsEmpty()) {
      LogicalMargin pad(aState.mReflowState->ComputedLogicalPadding());
      pad.ApplySkipSides(GetLogicalSkipSides(aState.mReflowState));
      // 'gridOrigin' is the origin of the grid (the start of the first track),
      // with respect to the grid container's padding-box (CB).
      const LogicalPoint gridOrigin(wm, pad.IStart(wm), pad.BStart(wm));
      const LogicalRect gridCB(wm, 0, 0,
                               aContentArea.ISize(wm) + pad.IStartEnd(wm),
                               aContentArea.BSize(wm) + pad.BStartEnd(wm));
      const nsSize gridCBPhysicalSize = gridCB.Size(wm).GetPhysicalSize(wm);
      size_t i = 0;
      for (nsFrameList::Enumerator e(children); !e.AtEnd(); e.Next(), ++i) {
        nsIFrame* child = e.get();
        MOZ_ASSERT(i < mAbsPosItems.Length());
        MOZ_ASSERT(mAbsPosItems[i].mFrame == child);
        GridArea& area = mAbsPosItems[i].mArea;
        LogicalRect itemCB =
          ContainingBlockForAbsPos(aState, area, gridOrigin, gridCB);
        // nsAbsoluteContainingBlock::Reflow uses physical coordinates.
        nsRect* cb = static_cast<nsRect*>(child->Properties().Get(
                       GridItemContainingBlockRect()));
        if (!cb) {
          cb = new nsRect;
          child->Properties().Set(GridItemContainingBlockRect(), cb);
        }
        *cb = itemCB.GetPhysicalRect(wm, gridCBPhysicalSize);
      }
      // We pass a dummy rect as CB because each child has its own CB rect.
      // The eIsGridContainerCB flag tells nsAbsoluteContainingBlock::Reflow to
      // use those instead.
      nsRect dummyRect;
      AbsPosReflowFlags flags =
        AbsPosReflowFlags::eCBWidthAndHeightChanged; // XXX could be optimized
      flags |= AbsPosReflowFlags::eConstrainHeight;
      flags |= AbsPosReflowFlags::eIsGridContainerCB;
      GetAbsoluteContainingBlock()->Reflow(this, pc, *aState.mReflowState,
                                           aStatus, dummyRect, flags,
                                           &aDesiredSize.mOverflowAreas);
    }
  }
}

void
nsGridContainerFrame::Reflow(nsPresContext*           aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsGridContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  if (IsFrameTreeTooDeep(aReflowState, aDesiredSize, aStatus)) {
    return;
  }

#ifdef DEBUG
  SanityCheckAnonymousGridItems();
#endif // DEBUG

  LogicalMargin bp = aReflowState.ComputedLogicalBorderPadding();
  bp.ApplySkipSides(GetLogicalSkipSides());
  const nsStylePosition* stylePos = aReflowState.mStylePosition;
  InitImplicitNamedAreas(stylePos);
  GridReflowState gridReflowState(this, aReflowState);
  mIsNormalFlowInCSSOrder = gridReflowState.mIter.ItemsAreAlreadyInOrder();
  const nscoord computedBSize = aReflowState.ComputedBSize();
  const nscoord computedISize = aReflowState.ComputedISize();
  const WritingMode& wm = gridReflowState.mWM;
  LogicalSize computedSize(wm, computedISize, computedBSize);

  // ComputedMinSize is zero rather than NS_UNCONSTRAINEDSIZE when indefinite
  // (unfortunately) so we have to check the style data and parent reflow state
  // to determine if it's indefinite.
  LogicalSize computedMinSize(aReflowState.ComputedMinSize());
  const nsHTMLReflowState* cbState = aReflowState.mCBReflowState;
  if (!stylePos->MinISize(wm).IsCoordPercentCalcUnit() ||
      (stylePos->MinISize(wm).HasPercent() && cbState &&
       cbState->ComputedSize(wm).ISize(wm) == NS_UNCONSTRAINEDSIZE)) {
    computedMinSize.ISize(wm) = NS_UNCONSTRAINEDSIZE;
  }
  if (!stylePos->MinBSize(wm).IsCoordPercentCalcUnit() ||
      (stylePos->MinBSize(wm).HasPercent() && cbState &&
       cbState->ComputedSize(wm).BSize(wm) == NS_UNCONSTRAINEDSIZE)) {
    computedMinSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
  }

  PlaceGridItems(gridReflowState, computedMinSize, computedSize,
                 aReflowState.ComputedMaxSize());

  gridReflowState.mIter.Reset();
  CalculateTrackSizes(gridReflowState, computedSize,
                      nsLayoutUtils::PREF_ISIZE);

  // FIXME bug 1229180: Instead of doing this on every reflow, we should only
  // set these properties if they are needed.
  nsTArray<nscoord> colTrackSizes(gridReflowState.mCols.mSizes.Length());
  for (const TrackSize& sz : gridReflowState.mCols.mSizes) {
    colTrackSizes.AppendElement(sz.mBase);
  }
  Properties().Set(GridColTrackSizes(),
                   new nsTArray<nscoord>(mozilla::Move(colTrackSizes)));
  nsTArray<nscoord> rowTrackSizes(gridReflowState.mRows.mSizes.Length());
  for (const TrackSize& sz : gridReflowState.mRows.mSizes) {
    rowTrackSizes.AppendElement(sz.mBase);
  }
  Properties().Set(GridRowTrackSizes(),
                   new nsTArray<nscoord>(mozilla::Move(rowTrackSizes)));
  
  nscoord bSize = 0;
  if (computedBSize == NS_AUTOHEIGHT) {
    for (uint32_t i = 0; i < mGridRowEnd; ++i) {
      bSize += gridReflowState.mRows.mSizes[i].mBase;
    }
    bSize += gridReflowState.mRows.SumOfGridGaps();
    bSize = NS_CSS_MINMAX(bSize,
                          aReflowState.ComputedMinBSize(),
                          aReflowState.ComputedMaxBSize());
  } else {
    bSize = computedBSize;
  }
  bSize = std::max(bSize - GetConsumedBSize(), 0);
  LogicalSize desiredSize(wm, computedISize + bp.IStartEnd(wm),
                          bSize + bp.BStartEnd(wm));
  aDesiredSize.SetSize(wm, desiredSize);
  aDesiredSize.SetOverflowAreasToDesiredBounds();

  LogicalRect contentArea(wm, bp.IStart(wm), bp.BStart(wm),
                          computedISize, bSize);

  // Apply 'align/justify-content' to the grid.
  gridReflowState.mCols.AlignJustifyContent(aReflowState, contentArea.Size(wm));
  gridReflowState.mRows.AlignJustifyContent(aReflowState, contentArea.Size(wm));

  gridReflowState.mIter.Reset(GridItemCSSOrderIterator::eIncludeAll);
  ReflowChildren(gridReflowState, contentArea, aDesiredSize, aStatus);

  FinishAndStoreOverflow(&aDesiredSize);
  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
}

nscoord
nsGridContainerFrame::IntrinsicISize(nsRenderingContext* aRenderingContext,
                                     IntrinsicISizeType  aConstraint)
{
  // Calculate the sum of column sizes under aConstraint.
  // http://dev.w3.org/csswg/css-grid/#intrinsic-sizes
  GridReflowState state(this, *aRenderingContext);
  InitImplicitNamedAreas(state.mGridStyle); // XXX optimize
  LogicalSize indefinite(state.mWM, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  PlaceGridItems(state, indefinite, indefinite, indefinite);  // XXX optimize
  if (mGridColEnd == 0) {
    return 0;
  }
  state.mCols.Initialize(state.mColFunctions, state.mGridStyle->mGridColumnGap,
                         mGridColEnd, NS_UNCONSTRAINEDSIZE);
  state.mIter.Reset();
  state.mCols.CalculateSizes(state, mGridItems, state.mColFunctions,
                             NS_UNCONSTRAINEDSIZE, &GridArea::mCols,
                             aConstraint);
  nscoord length = 0;
  for (const TrackSize& sz : state.mCols.mSizes) {
    length += sz.mBase;
  }
  return length + state.mCols.SumOfGridGaps();
}

nscoord
nsGridContainerFrame::GetMinISize(nsRenderingContext* aRC)
{
  DISPLAY_MIN_WIDTH(this, mCachedMinISize);
  if (mCachedMinISize == NS_INTRINSIC_WIDTH_UNKNOWN) {
    mCachedMinISize = IntrinsicISize(aRC, nsLayoutUtils::MIN_ISIZE);
  }
  return mCachedMinISize;
}

nscoord
nsGridContainerFrame::GetPrefISize(nsRenderingContext* aRC)
{
  DISPLAY_PREF_WIDTH(this, mCachedPrefISize);
  if (mCachedPrefISize == NS_INTRINSIC_WIDTH_UNKNOWN) {
    mCachedPrefISize = IntrinsicISize(aRC, nsLayoutUtils::PREF_ISIZE);
  }
  return mCachedPrefISize;
}

void
nsGridContainerFrame::MarkIntrinsicISizesDirty()
{
  mCachedMinISize = NS_INTRINSIC_WIDTH_UNKNOWN;
  mCachedPrefISize = NS_INTRINSIC_WIDTH_UNKNOWN;
  nsContainerFrame::MarkIntrinsicISizesDirty();
}

nsIAtom*
nsGridContainerFrame::GetType() const
{
  return nsGkAtoms::gridContainerFrame;
}

void
nsGridContainerFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                       const nsRect&           aDirtyRect,
                                       const nsDisplayListSet& aLists)
{
  DisplayBorderBackgroundOutline(aBuilder, aLists);

  // Our children are all grid-level boxes, which behave the same as
  // inline-blocks in painting, so their borders/backgrounds all go on
  // the BlockBorderBackgrounds list.
  // Also, we capture positioned descendants so we can sort them by
  // CSS 'order'.
  nsDisplayList positionedDescendants;
  nsDisplayListSet childLists(aLists.BlockBorderBackgrounds(),
                              aLists.BlockBorderBackgrounds(),
                              aLists.Floats(),
                              aLists.Content(),
                              &positionedDescendants,
                              aLists.Outlines());
  typedef GridItemCSSOrderIterator::OrderState OrderState;
  OrderState order = mIsNormalFlowInCSSOrder ? OrderState::eKnownOrdered
                                             : OrderState::eKnownUnordered;
  GridItemCSSOrderIterator iter(this, kPrincipalList,
                                GridItemCSSOrderIterator::eIncludeAll, order);
  for (; !iter.AtEnd(); iter.Next()) {
    nsIFrame* child = *iter;
    BuildDisplayListForChild(aBuilder, child, aDirtyRect, childLists,
                             ::GetDisplayFlagsForGridItem(child));
  }
  positionedDescendants.SortByCSSOrder(aBuilder);
  aLists.PositionedDescendants()->AppendToTop(&positionedDescendants);
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsGridContainerFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("GridContainer"), aResult);
}
#endif

void
nsGridContainerFrame::CellMap::Fill(const GridArea& aGridArea)
{
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

void
nsGridContainerFrame::CellMap::ClearOccupied()
{
  const size_t numRows = mCells.Length();
  for (size_t i = 0; i < numRows; ++i) {
    nsTArray<Cell>& cellsInRow = mCells[i];
    const size_t numCols = cellsInRow.Length();
    for (size_t j = 0; j < numCols; ++j) {
      cellsInRow[j].mIsOccupied = false;
    }
  }
}

#ifdef DEBUG
void
nsGridContainerFrame::CellMap::Dump() const
{
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

static bool
FrameWantsToBeInAnonymousGridItem(nsIFrame* aFrame)
{
  // Note: This needs to match the logic in
  // nsCSSFrameConstructor::FrameConstructionItem::NeedsAnonFlexOrGridItem()
  return aFrame->IsFrameOfType(nsIFrame::eLineParticipant);
}

// Debugging method, to let us assert that our anonymous grid items are
// set up correctly -- in particular, we assert:
//  (1) we don't have any inline non-replaced children
//  (2) we don't have any consecutive anonymous grid items
//  (3) we don't have any empty anonymous grid items
//  (4) all children are on the expected child lists
void
nsGridContainerFrame::SanityCheckAnonymousGridItems() const
{
  // XXX handle kOverflowContainersList / kExcessOverflowContainersList
  // when we implement fragmentation?
  ChildListIDs noCheckLists = kAbsoluteList | kFixedList;
  ChildListIDs checkLists = kPrincipalList | kOverflowList;
  for (nsIFrame::ChildListIterator childLists(this);
       !childLists.IsDone(); childLists.Next()) {
    if (!checkLists.Contains(childLists.CurrentID())) {
      MOZ_ASSERT(noCheckLists.Contains(childLists.CurrentID()),
                 "unexpected non-empty child list");
      continue;
    }

    bool prevChildWasAnonGridItem = false;
    nsFrameList children = childLists.CurrentList();
    for (nsFrameList::Enumerator e(children); !e.AtEnd(); e.Next()) {
      nsIFrame* child = e.get();
      MOZ_ASSERT(!FrameWantsToBeInAnonymousGridItem(child),
                 "frame wants to be inside an anonymous grid item, "
                 "but it isn't");
      if (child->StyleContext()->GetPseudo() ==
            nsCSSAnonBoxes::anonymousGridItem) {
/*
  XXX haven't decided yet whether to reorder children or not.
  XXX If we do, we want this assertion instead of the one below.
        MOZ_ASSERT(!prevChildWasAnonGridItem ||
                   HasAnyStateBits(NS_STATE_GRID_CHILDREN_REORDERED),
                   "two anon grid items in a row (shouldn't happen, unless our "
                   "children have been reordered with the 'order' property)");
*/
        MOZ_ASSERT(!prevChildWasAnonGridItem, "two anon grid items in a row");
        nsIFrame* firstWrappedChild = child->GetFirstPrincipalChild();
        MOZ_ASSERT(firstWrappedChild,
                   "anonymous grid item is empty (shouldn't happen)");
        prevChildWasAnonGridItem = true;
      } else {
        prevChildWasAnonGridItem = false;
      }
    }
  }
}

void
nsGridContainerFrame::TrackSize::Dump() const
{
  printf("mBase=%d mLimit=%d", mBase, mLimit);

  printf(" min:");
  if (mState & eAutoMinSizing) {
    printf("auto ");
  } else if (mState & eMinContentMinSizing) {
    printf("min-content ");
  } else if (mState & eMaxContentMinSizing) {
    printf("max-content ");
  } else if (mState & eFlexMinSizing) {
    printf("flex ");
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
}

#endif // DEBUG
