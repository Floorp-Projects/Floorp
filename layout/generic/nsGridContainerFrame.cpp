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
typedef nsGridContainerFrame::TrackSize TrackSize;
const uint32_t nsGridContainerFrame::kTranslatedMaxLine =
  uint32_t(nsStyleGridLine::kMaxLine - nsStyleGridLine::kMinLine - 1);
const uint32_t nsGridContainerFrame::kAutoLine = kTranslatedMaxLine + 3457U;

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
 * Encapsulates CSS track-sizing functions.
 */
struct MOZ_STACK_CLASS nsGridContainerFrame::TrackSizingFunctions
{
  const nsStyleCoord& MinSizingFor(uint32_t aTrackIndex) const
  {
    if (MOZ_UNLIKELY(aTrackIndex < mExplicitGridOffset)) {
      return mAutoMinSizing;
    }
    uint32_t index = aTrackIndex - mExplicitGridOffset;
    return index < mMinSizingFunctions.Length() ?
      mMinSizingFunctions[index] : mAutoMinSizing;
  }
  const nsStyleCoord& MaxSizingFor(uint32_t aTrackIndex) const
  {
    if (MOZ_UNLIKELY(aTrackIndex < mExplicitGridOffset)) {
      return mAutoMaxSizing;
    }
    uint32_t index = aTrackIndex - mExplicitGridOffset;
    return index < mMaxSizingFunctions.Length() ?
      mMaxSizingFunctions[index] : mAutoMaxSizing;
  }

  const nsTArray<nsStyleCoord>& mMinSizingFunctions;
  const nsTArray<nsStyleCoord>& mMaxSizingFunctions;
  const nsStyleCoord& mAutoMinSizing;
  const nsStyleCoord& mAutoMaxSizing;
  uint32_t mExplicitGridOffset;
};

/**
 * State for the tracks in one dimension.
 */
struct MOZ_STACK_CLASS nsGridContainerFrame::Tracks
{
  explicit Tracks(Dimension aDimension) : mDimension(aDimension) {}

  void Initialize(const TrackSizingFunctions& aFunctions,
                  nscoord                     aPercentageBasis);

  nsAutoTArray<TrackSize, 32> mSizes;
  Dimension mDimension;
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
    , mCols(eColDimension)
    , mRows(eRowDimension)
    , mColFunctions({
        mGridStyle->mGridTemplateColumns.mMinTrackSizingFunctions,
        mGridStyle->mGridTemplateColumns.mMaxTrackSizingFunctions,
        mGridStyle->mGridAutoColumnsMin,
        mGridStyle->mGridAutoColumnsMax,
      })
    , mRowFunctions({
        mGridStyle->mGridTemplateRows.mMinTrackSizingFunctions,
        mGridStyle->mGridTemplateRows.mMaxTrackSizingFunctions,
        mGridStyle->mGridAutoRowsMin,
        mGridStyle->mGridAutoRowsMax,
      })
    , mReflowState(aReflowState)
    , mRenderingContext(aRenderingContext)
    , mWM(aWM)
  {}
};

/**
 * Search for the aNth occurrence of aName in aNameList (forward), starting at
 * the zero-based aFromIndex, and return the 1-based index (line number).
 * Also take into account there is an unconditional match at aImplicitLine
 * unless it's zero.
 * Return zero if aNth occurrences can't be found.  In that case, aNth has
 * been decremented with the number of occurrences that were found (if any).
 */
static uint32_t
FindLine(const nsString& aName, int32_t* aNth,
         uint32_t aFromIndex, uint32_t aImplicitLine,
         const nsTArray<nsTArray<nsString>>& aNameList)
{
  MOZ_ASSERT(aNth && *aNth > 0);
  int32_t nth = *aNth;
  const uint32_t len = aNameList.Length();
  uint32_t line;
  uint32_t i = aFromIndex;
  for (; i < len; i = line) {
    line = i + 1;
    if (line == aImplicitLine || aNameList[i].Contains(aName)) {
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
 * @see FindLine, this function does the same but searches in reverse.
 */
static uint32_t
RFindLine(const nsString& aName, int32_t* aNth,
          uint32_t aFromIndex, uint32_t aImplicitLine,
          const nsTArray<nsTArray<nsString>>& aNameList)
{
  MOZ_ASSERT(aNth && *aNth > 0);
  int32_t nth = *aNth;
  const uint32_t len = aNameList.Length();
  // The implicit line may be beyond the length of aNameList so we match this
  // line first if it's within the len..aFromIndex range.
  if (aImplicitLine > len && aImplicitLine < aFromIndex) {
    if (--nth == 0) {
      return aImplicitLine;
    }
  }
  uint32_t i = aFromIndex == 0 ? len : std::min(aFromIndex, len);
  for (; i; --i) {
    if (i == aImplicitLine || aNameList[i - 1].Contains(aName)) {
      if (--nth == 0) {
        return i;
      }
    }
  }
  MOZ_ASSERT(nth > 0, "should have returned a valid line above already");
  *aNth = nth;
  return 0;
}

static uint32_t
FindNamedLine(const nsString& aName, int32_t* aNth,
              uint32_t aFromIndex, uint32_t aImplicitLine,
              const nsTArray<nsTArray<nsString>>& aNameList)
{
  MOZ_ASSERT(aNth && *aNth != 0);
  if (*aNth > 0) {
    return ::FindLine(aName, aNth, aFromIndex, aImplicitLine, aNameList);
  }
  int32_t nth = -*aNth;
  int32_t line = ::RFindLine(aName, &nth, aFromIndex, aImplicitLine, aNameList);
  *aNth = -nth;
  return line;
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

static nscoord
GridLinePosition(uint32_t aLine, const nsTArray<TrackSize>& aTrackSizes)
{
  const uint32_t endIndex = aLine;
  MOZ_ASSERT(endIndex <= aTrackSizes.Length(), "aTrackSizes is too small");
  nscoord pos = 0;
  for (uint32_t i = 0; i < endIndex; ++i) {
    pos += aTrackSizes[i].mBase;
  }
  return pos;
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
  // XXX this just checks x-start .. x-end in one dimension and there's
  // no other error checking.  A few wrong cases (maybe):
  // (x-start x-end)
  // (x-start) 0 (x-start) 0 (x-end)
  // (x-end) 0 (x-start) 0 (x-end)
  // (x-start) 0 (x-end) 0 (x-start) 0 (x-end)
  const uint32_t len =
    std::min(aLineNameLists.Length(), size_t(nsStyleGridLine::kMaxLine));
  nsTHashtable<nsStringHashKey> currentStarts;
  ImplicitNamedAreas* areas = GetImplicitNamedAreas();
  for (uint32_t i = 0; i < len; ++i) {
    const nsTArray<nsString>& names(aLineNameLists[i]);
    const uint32_t jLen = names.Length();
    for (uint32_t j = 0; j < jLen; ++j) {
      const nsString& name = names[j];
      uint32_t index;
      if (::IsNameWithStartSuffix(name, &index)) {
        currentStarts.PutEntry(nsDependentSubstring(name, 0, index));
      } else if (::IsNameWithEndSuffix(name, &index)) {
        nsDependentSubstring area(name, 0, index);
        if (currentStarts.Contains(area)) {
          if (!areas) {
            areas = new ImplicitNamedAreas;
            Properties().Set(ImplicitNamedAreasProperty(), areas);
          }
          areas->PutEntry(area);
        }
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
  const nsTArray<nsTArray<nsString>>& aLineNameList,
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
        // XXX must Implicit Named Areas have all four lines?
        // http://dev.w3.org/csswg/css-grid/#implicit-named-areas
        line = ::FindNamedLine(lineName, &aNth, aFromIndex, implicitLine,
                               aLineNameList);
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
      line = ::FindNamedLine(aLine.mLineName, &aNth, aFromIndex, implicitLine,
                             aLineNameList);
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
  const nsTArray<nsTArray<nsString>>& aLineNameList,
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

    auto end = ResolveLine(aEnd, aEnd.mInteger, 0, aLineNameList, aAreaStart,
                           aAreaEnd, aExplicitGridEnd, eLineRangeSideEnd,
                           aStyle);
    int32_t span = aStart.mInteger == 0 ? 1 : aStart.mInteger;
    if (end <= 1) {
      // The end is at or before the first explicit line, thus all lines before
      // it match <custom-ident> since they're implicit.
      int32_t start = std::max(end - span, nsStyleGridLine::kMinLine);
      return LinePair(start, end);
    }
    auto start = ResolveLine(aStart, -span, end, aLineNameList, aAreaStart,
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
    start = ResolveLine(aStart, aStart.mInteger, 0, aLineNameList, aAreaStart,
                        aAreaEnd, aExplicitGridEnd, eLineRangeSideStart,
                        aStyle);
    if (aEnd.IsAuto()) {
      // A "definite line / auto" should resolve the auto to 'span 1'.
      // The error handling in ResolveLineRange will make that happen and also
      // clamp the end line correctly if we return "start / start".
      return LinePair(start, start);
    }
  }

  uint32_t from = 0;
  int32_t nth = aEnd.mInteger == 0 ? 1 : aEnd.mInteger;
  if (aEnd.mHasSpan) {
    if (MOZ_UNLIKELY(start < 0)) {
      if (aEnd.mLineName.IsEmpty()) {
        return LinePair(start, start + nth);
      }
      // Fall through and start searching from the start of the grid (from=0).
    } else {
      if (start >= int32_t(aExplicitGridEnd)) {
        // The start is at or after the last explicit line, thus all lines
        // after it match <custom-ident> since they're implicit.
        return LinePair(start, std::min(start + nth, nsStyleGridLine::kMaxLine));
      }
      from = start;
    }
  }
  auto end = ResolveLine(aEnd, nth, from, aLineNameList, aAreaStart,
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
  const nsTArray<nsTArray<nsString>>& aLineNameList,
  uint32_t GridNamedArea::* aAreaStart,
  uint32_t GridNamedArea::* aAreaEnd,
  uint32_t aExplicitGridEnd,
  const nsStylePosition* aStyle)
{
  LinePair r = ResolveLineRangeHelper(aStart, aEnd, aLineNameList, aAreaStart,
                                      aAreaEnd, aExplicitGridEnd, aStyle);
  MOZ_ASSERT(r.second != int32_t(kAutoLine));

  if (r.first == int32_t(kAutoLine)) {
    // r.second is a span, clamp it to kMaxLine - 1 so that the returned
    // range has a HypotheticalEnd <= kMaxLine.
    // http://dev.w3.org/csswg/css-grid/#overlarge-grids
    r.second = std::min(r.second, nsStyleGridLine::kMaxLine - 1);
  } else if (r.second <= r.first) {
    // http://dev.w3.org/csswg/css-grid/#grid-placement-errors
    if (MOZ_UNLIKELY(r.first == nsStyleGridLine::kMaxLine)) {
      r.first = nsStyleGridLine::kMaxLine - 1;
    }
    r.second = r.first + 1; // XXX subgrid explicit size instead of 1?
  }
  return LineRange(r.first, r.second);
}

nsGridContainerFrame::GridArea
nsGridContainerFrame::PlaceDefinite(nsIFrame* aChild,
                                    const nsStylePosition* aStyle)
{
  const nsStylePosition* itemStyle = aChild->StylePosition();
  return GridArea(
    ResolveLineRange(itemStyle->mGridColumnStart, itemStyle->mGridColumnEnd,
                     aStyle->mGridTemplateColumns.mLineNameLists,
                     &GridNamedArea::mColumnStart, &GridNamedArea::mColumnEnd,
                     mExplicitGridColEnd, aStyle),
    ResolveLineRange(itemStyle->mGridRowStart, itemStyle->mGridRowEnd,
                     aStyle->mGridTemplateRows.mLineNameLists,
                     &GridNamedArea::mRowStart, &GridNamedArea::mRowEnd,
                     mExplicitGridRowEnd, aStyle));
}

nsGridContainerFrame::LineRange
nsGridContainerFrame::ResolveAbsPosLineRange(
  const nsStyleGridLine& aStart,
  const nsStyleGridLine& aEnd,
  const nsTArray<nsTArray<nsString>>& aLineNameList,
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
    int32_t end = ResolveLine(aEnd, aEnd.mInteger, 0, aLineNameList, aAreaStart,
                              aAreaEnd, aExplicitGridEnd, eLineRangeSideEnd,
                              aStyle);
    if (aEnd.mHasSpan) {
      ++end;
    }
    // A line outside the existing grid is treated as 'auto' for abs.pos (10.1).
    end = AutoIfOutside(end, aGridStart, aGridEnd);
    return LineRange(kAutoLine, end);
  }

  if (aEnd.IsAuto()) {
    int32_t start =
      ResolveLine(aStart, aStart.mInteger, 0, aLineNameList, aAreaStart,
                  aAreaEnd, aExplicitGridEnd, eLineRangeSideStart, aStyle);
    if (aStart.mHasSpan) {
      start = std::max(aGridEnd - start, aGridStart);
    }
    start = AutoIfOutside(start, aGridStart, aGridEnd);
    return LineRange(start, kAutoLine);
  }

  LineRange r = ResolveLineRange(aStart, aEnd, aLineNameList, aAreaStart,
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
      ++gap; // tentative, but we may reset it below if a column is occupied
      if (cellsInRow[j].mIsOccupied) {
        // Optimization: skip as many occupied cells as we can.
        do {
          ++j;
        } while (j < len && cellsInRow[j].mIsOccupied);
        candidate = j;
        gap = 0;
      }
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
                                  const nsStylePosition* aStyle)
{
  const nsStylePosition* itemStyle = aChild->StylePosition();
  int32_t gridColStart = 1 - mExplicitGridOffsetCol;
  int32_t gridRowStart = 1 - mExplicitGridOffsetRow;
  return GridArea(
    ResolveAbsPosLineRange(itemStyle->mGridColumnStart,
                           itemStyle->mGridColumnEnd,
                           aStyle->mGridTemplateColumns.mLineNameLists,
                           &GridNamedArea::mColumnStart,
                           &GridNamedArea::mColumnEnd,
                           mExplicitGridColEnd, gridColStart, mGridColEnd,
                           aStyle),
    ResolveAbsPosLineRange(itemStyle->mGridRowStart,
                           itemStyle->mGridRowEnd,
                           aStyle->mGridTemplateRows.mLineNameLists,
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
  aArea->mCols.ResolveAutoPosition(col);
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
  aArea->mRows.ResolveAutoPosition(row);
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
  aArea->mCols.ResolveAutoPosition(col);
  aArea->mRows.ResolveAutoPosition(row);
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
  aArea->mCols.ResolveAutoPosition(col);
  aArea->mRows.ResolveAutoPosition(row);
  MOZ_ASSERT(aArea->IsDefinite());
}

void
nsGridContainerFrame::InitializeGridBounds(const nsStylePosition* aStyle)
{
  // http://dev.w3.org/csswg/css-grid/#grid-definition
  // Note that this is for a grid with a 1,1 origin.  We'll change that
  // to a 0,0 based grid after placing definite lines.
  uint32_t colEnd = aStyle->mGridTemplateColumns.mLineNameLists.Length();
  uint32_t rowEnd = aStyle->mGridTemplateRows.mLineNameLists.Length();
  auto areas = aStyle->mGridTemplateAreas.get();
  mExplicitGridColEnd = std::max(colEnd, areas ? areas->mNColumns + 1 : 1);
  mExplicitGridRowEnd = std::max(rowEnd, areas ? areas->NRows() + 1 : 1);
  mExplicitGridColEnd =
    std::min(mExplicitGridColEnd, uint32_t(nsStyleGridLine::kMaxLine));
  mExplicitGridRowEnd =
    std::min(mExplicitGridRowEnd, uint32_t(nsStyleGridLine::kMaxLine));
  mGridColEnd = mExplicitGridColEnd;
  mGridRowEnd = mExplicitGridRowEnd;
}

void
nsGridContainerFrame::PlaceGridItems(GridReflowState& aState)
{
  const nsStylePosition* const gridStyle = aState.mGridStyle;

  mCellMap.ClearOccupied();
  InitializeGridBounds(gridStyle);

  // http://dev.w3.org/csswg/css-grid/#line-placement
  // Resolve definite positions per spec chap 9.2.
  int32_t minCol = 1;
  int32_t minRow = 1;
  mGridItems.ClearAndRetainStorage();
  for (; !aState.mIter.AtEnd(); aState.mIter.Next()) {
    nsIFrame* child = *aState.mIter;
    GridItemInfo* info =
      mGridItems.AppendElement(GridItemInfo(PlaceDefinite(child, gridStyle)));
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
        mAbsPosItems.AppendElement(GridItemInfo(PlaceAbsPos(child, gridStyle)));
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
}

static void
InitializeTrackSize(nscoord aPercentageBasis,
                    const nsStyleCoord& aMinCoord,
                    const nsStyleCoord& aMaxCoord,
                    TrackSize* aTrackSize)
{
  // http://dev.w3.org/csswg/css-grid/#algo-init
  nscoord& base = aTrackSize->mBase;
  switch (aMinCoord.GetUnit()) {
    case eStyleUnit_Auto:
    case eStyleUnit_Enumerated:
    case eStyleUnit_FlexFraction:
      base = 0;
      break;
    default:
      base = nsRuleNode::ComputeCoordPercentCalc(aMinCoord, aPercentageBasis);
  }
  nscoord& limit = aTrackSize->mLimit;
  switch (aMaxCoord.GetUnit()) {
    case eStyleUnit_Auto:
    case eStyleUnit_Enumerated:
      limit = NS_UNCONSTRAINEDSIZE;
      break;
    case eStyleUnit_FlexFraction:
      limit = base;
      break;
    default:
      limit = nsRuleNode::ComputeCoordPercentCalc(aMaxCoord, aPercentageBasis);
      if (limit < base) {
        limit = base;
      }
  }
}

void
nsGridContainerFrame::Tracks::Initialize(const TrackSizingFunctions& aFunctions,
                                         nscoord            aPercentageBasis)
{
  const uint32_t explicitGridOffset = aFunctions.mExplicitGridOffset;
  MOZ_ASSERT(mSizes.Length() >=
               explicitGridOffset + aFunctions.mMinSizingFunctions.Length());
  MOZ_ASSERT(aFunctions.mMinSizingFunctions.Length() ==
               aFunctions.mMaxSizingFunctions.Length());
  uint32_t i = 0;
  for (; i < explicitGridOffset; ++i) {
    InitializeTrackSize(aPercentageBasis,
                        aFunctions.mAutoMinSizing,
                        aFunctions.mAutoMaxSizing,
                        &mSizes[i]);
  }
  uint32_t j = 0;
  for (uint32_t len = aFunctions.mMinSizingFunctions.Length(); j < len; ++j) {
    InitializeTrackSize(aPercentageBasis,
                        aFunctions.mMinSizingFunctions[j],
                        aFunctions.mMaxSizingFunctions[j],
                        &mSizes[i + j]);
  }
  i += j;
  for (; i < mSizes.Length(); ++i) {
    InitializeTrackSize(aPercentageBasis,
                        aFunctions.mAutoMinSizing,
                        aFunctions.mAutoMaxSizing,
                        &mSizes[i]);
  }
}

#if 0
static nscoord
MinSize(nsIFrame* aChild, nsRenderingContext* aRC, WritingMode aCBWM,
        nsGridContainerFrame::Dimension aDimension,
        nsLayoutUtils::IntrinsicISizeType aConstraint)
{
  PhysicalAxis axis(((aDimension == nsGridContainerFrame::eColDimension) ==
                     aCBWM.IsVertical()) ? eAxisVertical : eAxisHorizontal);
  return nsLayoutUtils::MinSizeContributionForAxis(axis, aRC, aChild,
                                                   aConstraint);
}
#endif

void
nsGridContainerFrame::CalculateTrackSizes(GridReflowState&   aState,
                                          const LogicalSize& aContentBox)
{
  aState.mCols.mSizes.SetLength(mGridColEnd);
  aState.mRows.mSizes.SetLength(mGridRowEnd);
  const WritingMode& wm = aState.mWM;
  aState.mCols.Initialize(aState.mColFunctions, aContentBox.ISize(wm));
  nscoord rowPercentageBasis = aContentBox.BSize(wm);
  if (rowPercentageBasis == NS_AUTOHEIGHT) {
    rowPercentageBasis = 0;
  }
  aState.mRows.Initialize(aState.mRowFunctions, rowPercentageBasis);
}

void
nsGridContainerFrame::LineRange::ToPositionAndLength(
  const nsTArray<TrackSize>& aTrackSizes, nscoord* aPos, nscoord* aLength) const
{
  MOZ_ASSERT(mStart != kAutoLine && mEnd != kAutoLine,
             "expected a definite LineRange");
  nscoord pos = 0;
  const uint32_t start = mStart;
  uint32_t i = 0;
  for (; i < start; ++i) {
    pos += aTrackSizes[i].mBase;
  }
  *aPos = pos;

  nscoord length = 0;
  const uint32_t end = mEnd;
  MOZ_ASSERT(end <= aTrackSizes.Length(), "aTrackSizes isn't large enough");
  for (; i < end; ++i) {
    length += aTrackSizes[i].mBase;
  }
  *aLength = length;
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
      nscoord startPos = ::GridLinePosition(mStart, aTrackSizes);
      *aPos = aGridOrigin + startPos;
      *aLength = std::max(endPos - *aPos, 0);
    }
  } else {
    if (mStart == kAutoLine) {
      nscoord endPos = ::GridLinePosition(mEnd, aTrackSizes);
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
  return LogicalRect(aWM, i, b, iSize, bSize);
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
    nsHTMLReflowState childRS(pc, *aState.mReflowState, child, childCBSize);
    const LogicalMargin margin = childRS.ComputedLogicalMargin();
    if (childRS.ComputedBSize() == NS_AUTOHEIGHT && MOZ_LIKELY(isGridItem)) {
      // XXX the start of an align-self:stretch impl.  Needs min-/max-bsize
      // clamping though, and check the prop value is actually 'stretch'!
      LogicalMargin bp = childRS.ComputedLogicalBorderPadding();
      bp.ApplySkipSides(child->GetLogicalSkipSides());
      nscoord bSize = childCBSize.BSize(childWM) - bp.BStartEnd(childWM) -
                        margin.BStartEnd(childWM);
      childRS.SetComputedBSize(std::max(bSize, 0));
    }
    // We need the width of the child before we can correctly convert
    // the writing-mode of its origin, so we reflow at (0, 0) using a dummy
    // containerSize, and then pass the correct position to FinishReflowChild.
    nsHTMLReflowMetrics childSize(childRS);
    nsReflowStatus childStatus;
    const nsSize dummyContainerSize;
    ReflowChild(child, pc, childSize, childRS, childWM, LogicalPoint(childWM),
                dummyContainerSize, 0, childStatus);
    LogicalPoint childPos =
      cb.Origin(wm).ConvertTo(childWM, wm,
                              containerSize - childSize.PhysicalSize() -
                              margin.Size(childWM).GetPhysicalSize(childWM));
    childPos.I(childWM) += margin.IStart(childWM);
    childPos.B(childWM) += margin.BStart(childWM);
    childRS.ApplyRelativePositioning(&childPos, containerSize);
    FinishReflowChild(child, pc, childSize, &childRS, childWM, childPos,
                      containerSize, 0);
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, child);
    // XXX deal with 'childStatus' not being COMPLETE
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
        *cb = itemCB.GetPhysicalRect(wm, containerSize);
      }
      // This rect isn't used at all for layout so we use it to optimize
      // away the virtual GetType() call in the callee in most cases.
      // @see nsAbsoluteContainingBlock::Reflow
      nsRect dummyRect(0, 0, VERY_LIKELY_A_GRID_CONTAINER, 0);
      GetAbsoluteContainingBlock()->Reflow(this, pc, *aState.mReflowState,
                                           aStatus, dummyRect, true,
                                           true, true, // XXX could be optimized
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
  PlaceGridItems(gridReflowState);

  const nscoord computedBSize = aReflowState.ComputedBSize();
  const nscoord computedISize = aReflowState.ComputedISize();
  const WritingMode& wm = gridReflowState.mWM;
  CalculateTrackSizes(gridReflowState,
                      LogicalSize(wm, computedISize, computedBSize));

  nscoord bSize = 0;
  if (computedBSize == NS_AUTOHEIGHT) {
    for (uint32_t i = 0; i < mGridRowEnd; ++i) {
      bSize += gridReflowState.mRows.mSizes[i].mBase;
    }
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
  gridReflowState.mIter.Reset(GridItemCSSOrderIterator::eIncludeAll);
  ReflowChildren(gridReflowState, contentArea, aDesiredSize, aStatus);

  FinishAndStoreOverflow(&aDesiredSize);
  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
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
#endif // DEBUG
