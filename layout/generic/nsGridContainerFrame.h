/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: grid | inline-grid" */

#ifndef nsGridContainerFrame_h___
#define nsGridContainerFrame_h___

#include "mozilla/TypeTraits.h"
#include "nsContainerFrame.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"

/**
 * Factory function.
 * @return a newly allocated nsGridContainerFrame (infallible)
 */
nsContainerFrame* NS_NewGridContainerFrame(nsIPresShell* aPresShell,
                                           nsStyleContext* aContext);

namespace mozilla {
/**
 * The number of implicit / explicit tracks and their sizes.
 */
struct ComputedGridTrackInfo
{
  ComputedGridTrackInfo(uint32_t aNumLeadingImplicitTracks,
                        uint32_t aNumExplicitTracks,
                        nsTArray<nscoord>&& aSizes)
    : mNumLeadingImplicitTracks(aNumLeadingImplicitTracks)
    , mNumExplicitTracks(aNumExplicitTracks)
    , mSizes(aSizes)
  {}
  uint32_t mNumLeadingImplicitTracks;
  uint32_t mNumExplicitTracks;
  nsTArray<nscoord> mSizes;
};
} // namespace mozilla

class nsGridContainerFrame final : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsGridContainerFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  void Reflow(nsPresContext*           aPresContext,
              nsHTMLReflowMetrics&     aDesiredSize,
              const nsHTMLReflowState& aReflowState,
              nsReflowStatus&          aStatus) override;
  nscoord GetMinISize(nsRenderingContext* aRenderingContext) override;
  nscoord GetPrefISize(nsRenderingContext* aRenderingContext) override;
  void MarkIntrinsicISizesDirty() override;
  virtual nsIAtom* GetType() const override;

  void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                        const nsRect&           aDirtyRect,
                        const nsDisplayListSet& aLists) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  /**
   * Return the containing block for aChild which MUST be an abs.pos. child
   * of a grid container.  This is just a helper method for
   * nsAbsoluteContainingBlock::Reflow - it's not meant to be used elsewhere.
   */
  static const nsRect& GridItemCB(nsIFrame* aChild);

  struct TrackSize {
    void Initialize(nscoord aPercentageBasis,
                    const nsStyleCoord& aMinCoord,
                    const nsStyleCoord& aMaxCoord);
    bool IsFrozen() const { return mState & eFrozen; }
#ifdef DEBUG
    void Dump() const;
#endif
    enum StateBits : uint16_t {
      eAutoMinSizing =           0x1,
      eMinContentMinSizing =     0x2,
      eMaxContentMinSizing =     0x4,
      eMinOrMaxContentMinSizing = eMinContentMinSizing | eMaxContentMinSizing,
      eIntrinsicMinSizing = eMinOrMaxContentMinSizing | eAutoMinSizing,
      eFlexMinSizing =           0x8,
      eAutoMaxSizing =          0x10,
      eMinContentMaxSizing =    0x20,
      eMaxContentMaxSizing =    0x40,
      eAutoOrMaxContentMaxSizing = eAutoMaxSizing | eMaxContentMaxSizing,
      eIntrinsicMaxSizing = eAutoOrMaxContentMaxSizing | eMinContentMaxSizing,
      eFlexMaxSizing =          0x80,
      eFrozen =                0x100,
      eSkipGrowUnlimited1 =    0x200,
      eSkipGrowUnlimited2 =    0x400,
      eSkipGrowUnlimited = eSkipGrowUnlimited1 | eSkipGrowUnlimited2,
    };

    nscoord mBase;
    nscoord mLimit;
    nscoord mPosition;  // zero until we apply 'align/justify-content'
    StateBits mState;
  };

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridItemContainingBlockRect, nsRect)

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridColTrackInfo, ComputedGridTrackInfo)
  const ComputedGridTrackInfo* GetComputedTemplateColumns()
  {
    return Properties().Get(GridColTrackInfo());
  }

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridRowTrackInfo, ComputedGridTrackInfo)
  const ComputedGridTrackInfo* GetComputedTemplateRows()
  {
    return Properties().Get(GridRowTrackInfo());
  }

protected:
  static const uint32_t kAutoLine;
  // The maximum line number, in the zero-based translated grid.
  static const uint32_t kTranslatedMaxLine;
  typedef mozilla::LogicalPoint LogicalPoint;
  typedef mozilla::LogicalRect LogicalRect;
  typedef mozilla::LogicalSize LogicalSize;
  typedef mozilla::WritingMode WritingMode;
  typedef mozilla::css::GridNamedArea GridNamedArea;
  typedef nsLayoutUtils::IntrinsicISizeType IntrinsicISizeType;
  class GridItemCSSOrderIterator;
  struct Grid;
  struct TrackSizingFunctions;
  struct Tracks;
  struct GridReflowState;
  class LineNameMap;
  friend nsContainerFrame* NS_NewGridContainerFrame(nsIPresShell* aPresShell,
                                                    nsStyleContext* aContext);
  explicit nsGridContainerFrame(nsStyleContext* aContext)
    : nsContainerFrame(aContext)
    , mCachedMinISize(NS_INTRINSIC_WIDTH_UNKNOWN)
    , mCachedPrefISize(NS_INTRINSIC_WIDTH_UNKNOWN)
  {}

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
  struct LineRange {
   LineRange(int32_t aStart, int32_t aEnd)
     : mUntranslatedStart(aStart), mUntranslatedEnd(aEnd)
    {
#ifdef DEBUG
      if (!IsAutoAuto()) {
        if (IsAuto()) {
          MOZ_ASSERT(aEnd >= nsStyleGridLine::kMinLine &&
                     aEnd <= nsStyleGridLine::kMaxLine, "invalid span");
        } else {
          MOZ_ASSERT(aStart >= nsStyleGridLine::kMinLine &&
                     aStart <= nsStyleGridLine::kMaxLine, "invalid start line");
          MOZ_ASSERT(aEnd == int32_t(kAutoLine) ||
                     (aEnd >= nsStyleGridLine::kMinLine &&
                      aEnd <= nsStyleGridLine::kMaxLine), "invalid end line");
        }
      }
#endif
    }
    bool IsAutoAuto() const { return mStart == kAutoLine && mEnd == kAutoLine; }
    bool IsAuto() const { return mStart == kAutoLine; }
    bool IsDefinite() const { return mStart != kAutoLine; }
    uint32_t Extent() const
    {
      MOZ_ASSERT(mEnd != kAutoLine, "Extent is undefined for abs.pos. 'auto'");
      if (IsAuto()) {
        MOZ_ASSERT(mEnd >= 1 && mEnd < uint32_t(nsStyleGridLine::kMaxLine),
                   "invalid span");
        return mEnd;
      }
      return mEnd - mStart;
    }
    /**
     * Resolve this auto range to start at aStart, making it definite.
     * Precondition: this range IsAuto()
     */
    void ResolveAutoPosition(uint32_t aStart, uint32_t aExplicitGridOffset)
    {
      MOZ_ASSERT(IsAuto(), "Why call me?");
      mStart = aStart;
      mEnd += aStart;
      // Clamping to where kMaxLine is in the explicit grid, per
      // http://dev.w3.org/csswg/css-grid/#overlarge-grids :
      uint32_t translatedMax = aExplicitGridOffset + nsStyleGridLine::kMaxLine;
      if (MOZ_UNLIKELY(mStart >= translatedMax)) {
        mEnd = translatedMax;
        mStart = mEnd - 1;
      } else if (MOZ_UNLIKELY(mEnd > translatedMax)) {
        mEnd = translatedMax;
      }
    }
    /**
     * Translate the lines to account for (empty) removed tracks.  This method
     * is only for grid items and should only be called after placement.
     * aNumRemovedTracks contains a count for each line in the grid how many
     * tracks were removed between the start of the grid and that line.
     */
    void AdjustForRemovedTracks(const nsTArray<uint32_t>& aNumRemovedTracks)
    {
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
    void AdjustAbsPosForRemovedTracks(const nsTArray<uint32_t>& aNumRemovedTracks)
    {
      if (mStart != nsGridContainerFrame::kAutoLine) {
        mStart -= aNumRemovedTracks[mStart];
      }
      if (mEnd != nsGridContainerFrame::kAutoLine) {
        MOZ_ASSERT(mStart == nsGridContainerFrame::kAutoLine ||
                   mEnd > mStart, "invalid line range");
        mEnd -= aNumRemovedTracks[mEnd];
      }
      if (mStart == mEnd) {
        mEnd = nsGridContainerFrame::kAutoLine;
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
     * Given a set of tracks and a grid origin coordinate, adjust the
     * abs.pos. containing block along an axis given by aPos and aLength.
     * aPos and aLength should already be initialized to the grid container
     * containing block for this axis before calling this method.
     */
    void ToPositionAndLengthForAbsPos(const Tracks& aTracks,
                                      nscoord aGridOrigin,
                                      nscoord* aPos, nscoord* aLength) const;

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
    LineRange() {}
  };

  /**
   * Helper class to construct a LineRange from translated lines.
   * The ctor only accepts translated definite line numbers.
   */
  struct TranslatedLineRange : public LineRange {
    TranslatedLineRange(uint32_t aStart, uint32_t aEnd)
    {
      MOZ_ASSERT(aStart < aEnd && aEnd <= kTranslatedMaxLine);
      mStart = aStart;
      mEnd = aEnd;
    }
  };

  /**
   * Return aLine if it's inside the aMin..aMax range (inclusive),
   * otherwise return kAutoLine.
   */
  static int32_t
  AutoIfOutside(int32_t aLine, int32_t aMin, int32_t aMax)
  {
    MOZ_ASSERT(aMin <= aMax);
    if (aLine < aMin || aLine > aMax) {
      return kAutoLine;
    }
    return aLine;
  }

  /**
   * A GridArea is the area in the grid for a grid item.
   * The area is represented by two LineRanges, both of which can be auto
   * (@see LineRange) in intermediate steps while the item is being placed.
   * @see PlaceGridItems
   */
  struct GridArea {
    GridArea(const LineRange& aCols, const LineRange& aRows)
      : mCols(aCols), mRows(aRows) {}
    bool IsDefinite() const { return mCols.IsDefinite() && mRows.IsDefinite(); }
    LineRange mCols;
    LineRange mRows;
  };

  struct GridItemInfo {
    explicit GridItemInfo(const GridArea& aArea)
      : mArea(aArea)
    {
      mIsFlexing[0] = false;
      mIsFlexing[1] = false;
    }

    GridArea mArea;
    bool mIsFlexing[2]; // does the item span a flex track? (LogicalAxis index)
    static_assert(mozilla::eLogicalAxisBlock == 0, "unexpected index value");
    static_assert(mozilla::eLogicalAxisInline == 1, "unexpected index value");
#ifdef DEBUG
    nsIFrame* mFrame;
#endif
  };

  /**
   * XXX temporary - move the ImplicitNamedAreas stuff to the style system.
   * The implicit area names that come from x-start .. x-end lines in
   * grid-template-columns / grid-template-rows are stored in this frame
   * property when needed, as a ImplicitNamedAreas* value.
   */
  typedef nsTHashtable<nsStringHashKey> ImplicitNamedAreas;
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(ImplicitNamedAreasProperty,
                                      ImplicitNamedAreas)
  void InitImplicitNamedAreas(const nsStylePosition* aStyle);
  void AddImplicitNamedAreas(const nsTArray<nsTArray<nsString>>& aLineNameLists);
  ImplicitNamedAreas* GetImplicitNamedAreas() const {
    return Properties().Get(ImplicitNamedAreasProperty());
  }

  /**
   * Reflow and place our children.
   */
  void ReflowChildren(GridReflowState&     aState,
                      const LogicalRect&   aContentArea,
                      nsHTMLReflowMetrics& aDesiredSize,
                      nsReflowStatus&      aStatus);

  /**
   * Helper for GetMinISize / GetPrefISize.
   */
  nscoord IntrinsicISize(nsRenderingContext* aRenderingContext,
                         IntrinsicISizeType  aConstraint);

#ifdef DEBUG
  void SanityCheckAnonymousGridItems() const;
#endif // DEBUG

private:
  /**
   * Cached values to optimize GetMinISize/GetPrefISize.
   */
  nscoord mCachedMinISize;
  nscoord mCachedPrefISize;

  /**
   * True iff the normal flow children are already in CSS 'order' in the
   * order they occur in the child frame list.
   */
  bool mIsNormalFlowInCSSOrder : 1;
};

namespace mozilla {
template <>
struct IsPod<nsGridContainerFrame::TrackSize> : TrueType {};
}

#endif /* nsGridContainerFrame_h___ */
