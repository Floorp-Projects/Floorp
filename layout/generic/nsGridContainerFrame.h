/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: grid | inline-grid" */

#ifndef nsGridContainerFrame_h___
#define nsGridContainerFrame_h___

#include "nsContainerFrame.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"

/**
 * Factory function.
 * @return a newly allocated nsGridContainerFrame (infallible)
 */
nsContainerFrame* NS_NewGridContainerFrame(nsIPresShell* aPresShell,
                                           nsStyleContext* aContext);

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
    nscoord mBase;
    nscoord mLimit;
  };

  // @see nsAbsoluteContainingBlock::Reflow about this magic number
  static const nscoord VERY_LIKELY_A_GRID_CONTAINER = -123456789;

  NS_DECLARE_FRAME_PROPERTY(GridItemContainingBlockRect, DeleteValue<nsRect>)

protected:
  static const uint32_t kAutoLine;
  // The maximum line number, in the zero-based translated grid.
  static const uint32_t kTranslatedMaxLine;
  typedef mozilla::LogicalPoint LogicalPoint;
  typedef mozilla::LogicalRect LogicalRect;
  typedef mozilla::WritingMode WritingMode;
  typedef mozilla::css::GridNamedArea GridNamedArea;
  class GridItemCSSOrderIterator;
  struct TrackSizingFunctions;
  struct Tracks;
  struct GridReflowState;
  friend nsContainerFrame* NS_NewGridContainerFrame(nsIPresShell* aPresShell,
                                                    nsStyleContext* aContext);
  explicit nsGridContainerFrame(nsStyleContext* aContext) : nsContainerFrame(aContext) {}

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
    void ResolveAutoPosition(uint32_t aStart)
    {
      MOZ_ASSERT(IsAuto(), "Why call me?");
      mStart = aStart;
      mEnd += aStart;
      // Clamping per http://dev.w3.org/csswg/css-grid/#overlarge-grids :
      if (MOZ_UNLIKELY(mStart >= kTranslatedMaxLine)) {
        mEnd = kTranslatedMaxLine;
        mStart = mEnd - 1;
      } else if (MOZ_UNLIKELY(mEnd > kTranslatedMaxLine)) {
        mEnd = kTranslatedMaxLine;
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
     * Given an array of track sizes and a grid origin coordinate, adjust the
     * abs.pos. containing block along an axis given by aPos and aLength.
     * aPos and aLength should already be initialized to the grid container
     * containing block for this axis before calling this method.
     */
    void ToPositionAndLengthForAbsPos(const nsTArray<TrackSize>& aTrackSizes,
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
      Cell() : mIsOccupied(false) {}
      bool mIsOccupied : 1;
    };
    void Fill(const GridArea& aGridArea);
    void ClearOccupied();
#if DEBUG
    void Dump() const;
#endif
    nsTArray<nsTArray<Cell>> mCells;
  };

  struct GridItemInfo {
    explicit GridItemInfo(const GridArea& aArea) : mArea(aArea) {}
    GridArea mArea;
#ifdef DEBUG
    nsIFrame* mFrame;
#endif
  };

  enum LineRangeSide {
    eLineRangeSideStart, eLineRangeSideEnd
  };
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
   * @param aAreaStart a pointer to GridNamedArea::mColumnStart/mRowStart
   * @param aAreaEnd a pointer to GridNamedArea::mColumnEnd/mRowEnd
   * @param aExplicitGridEnd the last line in the explicit grid
   * @param aEdge indicates whether we are resolving a start or end line
   * @param aStyle the StylePosition() for the grid container
   * @return a definite line (1-based), clamped to the kMinLine..kMaxLine range
   */
  int32_t ResolveLine(const nsStyleGridLine& aLine,
                      int32_t aNth,
                      uint32_t aFromIndex,
                      const nsTArray<nsTArray<nsString>>& aLineNameList,
                      uint32_t GridNamedArea::* aAreaStart,
                      uint32_t GridNamedArea::* aAreaEnd,
                      uint32_t aExplicitGridEnd,
                      LineRangeSide aEdge,
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
   * @param aAreaStart a pointer to GridNamedArea::mColumnStart/mRowStart
   * @param aAreaEnd a pointer to GridNamedArea::mColumnEnd/mRowEnd
   * @param aExplicitGridEnd the last line in the explicit grid
   * @param aStyle the StylePosition() for the grid container
   */
  LineRange ResolveLineRange(const nsStyleGridLine& aStart,
                             const nsStyleGridLine& aEnd,
                             const nsTArray<nsTArray<nsString>>& aLineNameList,
                             uint32_t GridNamedArea::* aAreaStart,
                             uint32_t GridNamedArea::* aAreaEnd,
                             uint32_t aExplicitGridEnd,
                             const nsStylePosition* aStyle);

  /**
   * As above but for an abs.pos. child.  Any 'auto' lines will be represented
   * by kAutoLine in the LineRange result.
   * @param aGridStart the first line in the final, but untranslated grid
   * @param aGridEnd the last line in the final, but untranslated grid
   */
  LineRange
  ResolveAbsPosLineRange(const nsStyleGridLine& aStart,
                         const nsStyleGridLine& aEnd,
                         const nsTArray<nsTArray<nsString>>& aLineNameList,
                         uint32_t GridNamedArea::* aAreaStart,
                         uint32_t GridNamedArea::* aAreaEnd,
                         uint32_t aExplicitGridEnd,
                         int32_t aGridStart,
                         int32_t aGridEnd,
                         const nsStylePosition* aStyle);

  /**
   * Return a GridArea with non-auto lines placed at a definite line (1-based)
   * with placement errors resolved.  One or both positions may still
   * be 'auto'.
   * @param aChild the grid item
   * @param aStyle the StylePosition() for the grid container
   */
  GridArea PlaceDefinite(nsIFrame* aChild, const nsStylePosition* aStyle);

  /**
   * Place aArea in the first column (in row aArea->mRows.mStart) starting at
   * aStartCol without overlapping other items.  The resulting aArea may
   * overflow the current implicit grid bounds.
   * Pre-condition: aArea->mRows.IsDefinite() is true.
   * Post-condition: aArea->IsDefinite() is true.
   */
  void PlaceAutoCol(uint32_t aStartCol, GridArea* aArea) const;

  /**
   * Find the first column in row aLockedRow starting at aStartCol where aArea
   * could be placed without overlapping other items.  The returned column may
   * cause aArea to overflow the current implicit grid bounds if placed there.
   */
  uint32_t FindAutoCol(uint32_t aStartCol, uint32_t aLockedRow,
                       const GridArea* aArea) const;

  /**
   * Place aArea in the first row (in column aArea->mCols.mStart) starting at
   * aStartRow without overlapping other items. The resulting aArea may
   * overflow the current implicit grid bounds.
   * Pre-condition: aArea->mCols.IsDefinite() is true.
   * Post-condition: aArea->IsDefinite() is true.
   */
  void PlaceAutoRow(uint32_t aStartRow, GridArea* aArea) const;

  /**
   * Find the first row in column aLockedCol starting at aStartRow where aArea
   * could be placed without overlapping other items.  The returned row may
   * cause aArea to overflow the current implicit grid bounds if placed there.
   */
  uint32_t FindAutoRow(uint32_t aLockedCol, uint32_t aStartRow,
                       const GridArea* aArea) const;

  /**
   * Place aArea in the first column starting at aStartCol,aStartRow without
   * causing it to overlap other items or overflow mGridColEnd.
   * If there's no such column in aStartRow, continue in position 1,aStartRow+1.
   * Pre-condition: aArea->mCols.IsAuto() && aArea->mRows.IsAuto() is true.
   * Post-condition: aArea->IsDefinite() is true.
   */
  void PlaceAutoAutoInRowOrder(uint32_t aStartCol, uint32_t aStartRow,
                               GridArea* aArea) const;

  /**
   * Place aArea in the first row starting at aStartCol,aStartRow without
   * causing it to overlap other items or overflow mGridRowEnd.
   * If there's no such row in aStartCol, continue in position aStartCol+1,1.
   * Pre-condition: aArea->mCols.IsAuto() && aArea->mRows.IsAuto() is true.
   * Post-condition: aArea->IsDefinite() is true.
   */
  void PlaceAutoAutoInColOrder(uint32_t aStartCol, uint32_t aStartRow,
                               GridArea* aArea) const;

  /**
   * Return a GridArea for abs.pos. item with non-auto lines placed at
   * a definite line (1-based) with placement errors resolved.  One or both
   * positions may still be 'auto'.
   * @param aChild the abs.pos. grid item to place
   * @param aStyle the StylePosition() for the grid container
   */
  GridArea PlaceAbsPos(nsIFrame* aChild, const nsStylePosition* aStyle);

  /**
   * Place all child frames into the grid and expand the (implicit) grid as
   * needed.  The allocated GridAreas are stored in the GridAreaProperty
   * frame property on the child frame.
   */
  void PlaceGridItems(GridReflowState& aState);

  /**
   * Initialize the end lines of the Explicit Grid (mExplicitGridCol[Row]End).
   * This is determined by the larger of the number of rows/columns defined
   * by 'grid-template-areas' and the 'grid-template-rows'/'-columns', plus one.
   * Also initialize the Implicit Grid (mGridCol[Row]End) to the same values.
   * @param aStyle the StylePosition() for the grid container
   */
  void InitializeGridBounds(const nsStylePosition* aStyle);

  /**
   * Inflate the implicit grid to include aArea.
   * @param aArea may be definite or auto
   */
  void InflateGridFor(const GridArea& aArea)
  {
    mGridColEnd = std::max(mGridColEnd, aArea.mCols.HypotheticalEnd());
    mGridRowEnd = std::max(mGridRowEnd, aArea.mRows.HypotheticalEnd());
    MOZ_ASSERT(mGridColEnd <= kTranslatedMaxLine &&
               mGridRowEnd <= kTranslatedMaxLine);
  }

  /**
   * Calculate track sizes.
   */
  void CalculateTrackSizes(GridReflowState&            aState,
                           const mozilla::LogicalSize& aContentBox);

  /**
   * Helper method for ResolveLineRange.
   * @see ResolveLineRange
   * @return a pair (start,end) of lines
   */
  typedef std::pair<int32_t, int32_t> LinePair;
  LinePair ResolveLineRangeHelper(const nsStyleGridLine& aStart,
                                  const nsStyleGridLine& aEnd,
                                  const nsTArray<nsTArray<nsString>>& aLineNameList,
                                  uint32_t GridNamedArea::* aAreaStart,
                                  uint32_t GridNamedArea::* aAreaEnd,
                                  uint32_t aExplicitGridEnd,
                                  const nsStylePosition* aStyle);

  /**
   * XXX temporary - move the ImplicitNamedAreas stuff to the style system.
   * The implicit area names that come from x-start .. x-end lines in
   * grid-template-columns / grid-template-rows are stored in this frame
   * property when needed, as a ImplicitNamedAreas* value.
   */
  NS_DECLARE_FRAME_PROPERTY(ImplicitNamedAreasProperty,
                            DeleteValue<ImplicitNamedAreas>)
  void InitImplicitNamedAreas(const nsStylePosition* aStyle);
  void AddImplicitNamedAreas(const nsTArray<nsTArray<nsString>>& aLineNameLists);
  typedef nsTHashtable<nsStringHashKey> ImplicitNamedAreas;
  ImplicitNamedAreas* GetImplicitNamedAreas() const {
    return static_cast<ImplicitNamedAreas*>(Properties().Get(ImplicitNamedAreasProperty()));
  }
  bool HasImplicitNamedArea(const nsString& aName) const {
    ImplicitNamedAreas* areas = GetImplicitNamedAreas();
    return areas && areas->Contains(aName);
  }

  /**
   * Return the containing block for a grid item occupying aArea.
   */
  LogicalRect ContainingBlockFor(const GridReflowState& aState,
                                 const GridArea&        aArea) const;

  /**
   * Return the containing block for an abs.pos. grid item occupying aArea.
   * Any 'auto' lines in the grid area will be aligned with grid container
   * containing block on that side.
   * @param aGridOrigin the origin of the grid
   * @param aGridCB the grid container containing block (its padding area)
   */
  LogicalRect ContainingBlockForAbsPos(const GridReflowState& aState,
                                       const GridArea&        aArea,
                                       const LogicalPoint&    aGridOrigin,
                                       const LogicalRect&     aGridCB) const;

  /**
   * Reflow and place our children.
   */
  void ReflowChildren(GridReflowState&     aState,
                      const LogicalRect&   aContentArea,
                      nsHTMLReflowMetrics& aDesiredSize,
                      nsReflowStatus&      aStatus);

#ifdef DEBUG
  void SanityCheckAnonymousGridItems() const;
#endif // DEBUG

private:
  /**
   * Info about each (normal flow) grid item.
   */
  nsTArray<GridItemInfo> mGridItems;

  /**
   * Info about each grid-aligned abs.pos. child.
   */
  nsTArray<GridItemInfo> mAbsPosItems;

  /**
   * State for each cell in the grid.
   */
  CellMap mCellMap;

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
   * True iff the normal flow children are already in CSS 'order' in the
   * order they occur in the child frame list.
   */
  bool mIsNormalFlowInCSSOrder : 1;
};

#endif /* nsGridContainerFrame_h___ */
