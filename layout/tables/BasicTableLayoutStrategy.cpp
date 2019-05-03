/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Web-compatible algorithms that determine column and table widths,
 * used for CSS2's 'table-layout: auto'.
 */

#include "BasicTableLayoutStrategy.h"

#include <algorithm>

#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"
#include "nsLayoutUtils.h"
#include "nsGkAtoms.h"
#include "SpanningCellSorter.h"
#include "nsIContent.h"

using namespace mozilla;
using namespace mozilla::layout;

namespace css = mozilla::css;

#undef DEBUG_TABLE_STRATEGY

BasicTableLayoutStrategy::BasicTableLayoutStrategy(nsTableFrame* aTableFrame)
    : nsITableLayoutStrategy(nsITableLayoutStrategy::Auto),
      mTableFrame(aTableFrame) {
  MarkIntrinsicISizesDirty();
}

/* virtual */
BasicTableLayoutStrategy::~BasicTableLayoutStrategy() {}

/* virtual */
nscoord BasicTableLayoutStrategy::GetMinISize(gfxContext* aRenderingContext) {
  DISPLAY_MIN_INLINE_SIZE(mTableFrame, mMinISize);
  if (mMinISize == NS_INTRINSIC_ISIZE_UNKNOWN) {
    ComputeIntrinsicISizes(aRenderingContext);
  }
  return mMinISize;
}

/* virtual */
nscoord BasicTableLayoutStrategy::GetPrefISize(gfxContext* aRenderingContext,
                                               bool aComputingSize) {
  DISPLAY_PREF_INLINE_SIZE(mTableFrame, mPrefISize);
  NS_ASSERTION((mPrefISize == NS_INTRINSIC_ISIZE_UNKNOWN) ==
                   (mPrefISizePctExpand == NS_INTRINSIC_ISIZE_UNKNOWN),
               "dirtyness out of sync");
  if (mPrefISize == NS_INTRINSIC_ISIZE_UNKNOWN) {
    ComputeIntrinsicISizes(aRenderingContext);
  }
  return aComputingSize ? mPrefISizePctExpand : mPrefISize;
}

struct CellISizeInfo {
  CellISizeInfo(nscoord aMinCoord, nscoord aPrefCoord, float aPrefPercent,
                bool aHasSpecifiedISize)
      : hasSpecifiedISize(aHasSpecifiedISize),
        minCoord(aMinCoord),
        prefCoord(aPrefCoord),
        prefPercent(aPrefPercent) {}

  bool hasSpecifiedISize;
  nscoord minCoord;
  nscoord prefCoord;
  float prefPercent;
};

// Used for both column and cell calculations.  The parts needed only
// for cells are skipped when aIsCell is false.
static CellISizeInfo GetISizeInfo(gfxContext* aRenderingContext,
                                  nsIFrame* aFrame, WritingMode aWM,
                                  bool aIsCell) {
  nscoord minCoord, prefCoord;
  const nsStylePosition* stylePos = aFrame->StylePosition();
  bool isQuirks =
      aFrame->PresContext()->CompatibilityMode() == eCompatibility_NavQuirks;
  nscoord boxSizingToBorderEdge = 0;
  if (aIsCell) {
    // If aFrame is a container for font size inflation, then shrink
    // wrapping inside of it should not apply font size inflation.
    AutoMaybeDisableFontInflation an(aFrame);

    minCoord = aFrame->GetMinISize(aRenderingContext);
    prefCoord = aFrame->GetPrefISize(aRenderingContext);
    // Until almost the end of this function, minCoord and prefCoord
    // represent the box-sizing based isize values (which mean they
    // should include inline padding and border width when
    // box-sizing is set to border-box).
    // Note that this function returns border-box isize, we add the
    // outer edges near the end of this function.

    // XXX Should we ignore percentage padding?
    nsIFrame::IntrinsicISizeOffsetData offsets =
        aFrame->IntrinsicISizeOffsets();

    // In quirks mode, table cell isize should be content-box,
    // but bsize should be border box.
    // Because of this historic anomaly, we do not use quirk.css.
    // (We can't specify one value of box-sizing for isize and another
    // for bsize).
    // For this reason, we also do not use box-sizing for just one of
    // them, as this may be confusing.
    if (isQuirks || stylePos->mBoxSizing == StyleBoxSizing::Content) {
      boxSizingToBorderEdge = offsets.hPadding + offsets.hBorder;
    } else {
      // StyleBoxSizing::Border and standards-mode
      minCoord += offsets.hPadding + offsets.hBorder;
      prefCoord += offsets.hPadding + offsets.hBorder;
    }
  } else {
    minCoord = 0;
    prefCoord = 0;
  }
  float prefPercent = 0.0f;
  bool hasSpecifiedISize = false;

  const auto& iSize = stylePos->ISize(aWM);
  // NOTE: We're ignoring calc() units with both lengths and percentages here,
  // for lack of a sensible idea for what to do with them.  This means calc()
  // with percentages is basically handled like 'auto' for table cells and
  // columns.
  if (iSize.ConvertsToLength()) {
    hasSpecifiedISize = true;
    // Note: since ComputeISizeValue was designed to return content-box
    // isize, it will (in some cases) subtract the box-sizing edges.
    // We prevent this unwanted behavior by calling it with
    // aContentEdgeToBoxSizing and aBoxSizingToMarginEdge set to 0.
    nscoord c = aFrame->ComputeISizeValue(aRenderingContext, 0, 0, 0, iSize);
    // Quirk: A cell with "nowrap" set and a coord value for the
    // isize which is bigger than the intrinsic minimum isize uses
    // that coord value as the minimum isize.
    // This is kept up-to-date with dynamic changes to nowrap by code in
    // nsTableCellFrame::AttributeChanged
    if (aIsCell && c > minCoord && isQuirks &&
        aFrame->GetContent()->AsElement()->HasAttr(kNameSpaceID_None,
                                                   nsGkAtoms::nowrap)) {
      minCoord = c;
    }
    prefCoord = std::max(c, minCoord);
  } else if (iSize.ConvertsToPercentage()) {
    prefPercent = iSize.ToPercentage();
  } else if (iSize.IsExtremumLength() && aIsCell) {
    switch (iSize.AsExtremumLength()) {
      case StyleExtremumLength::MaxContent:
        // 'inline-size' only affects pref isize, not min
        // isize, so don't change anything
        break;
      case StyleExtremumLength::MinContent:
        prefCoord = minCoord;
        break;
      case StyleExtremumLength::MozFitContent:
      case StyleExtremumLength::MozAvailable:
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("unexpected enumerated value");
    }
  }

  StyleMaxSize maxISize = stylePos->MaxISize(aWM);
  if (maxISize.IsExtremumLength()) {
    if (!aIsCell ||
        maxISize.AsExtremumLength() == StyleExtremumLength::MozAvailable) {
      maxISize = StyleMaxSize::None();
    } else if (maxISize.AsExtremumLength() ==
               StyleExtremumLength::MozFitContent) {
      // for 'max-inline-size', '-moz-fit-content' is like 'max-content'
      maxISize = StyleMaxSize::ExtremumLength(StyleExtremumLength::MaxContent);
    }
  }
  // XXX To really implement 'max-inline-size' well, we'd need to store
  // it separately on the columns.
  if (maxISize.ConvertsToLength() || maxISize.IsExtremumLength()) {
    nscoord c = aFrame->ComputeISizeValue(aRenderingContext, 0, 0, 0, maxISize);
    minCoord = std::min(c, minCoord);
    prefCoord = std::min(c, prefCoord);
  } else if (maxISize.ConvertsToPercentage()) {
    float p = maxISize.ToPercentage();
    if (p < prefPercent) {
      prefPercent = p;
    }
  }

  StyleSize minISize = stylePos->MinISize(aWM);
  if (minISize.IsExtremumLength()) {
    if (!aIsCell ||
        minISize.AsExtremumLength() == StyleExtremumLength::MozAvailable) {
      minISize = StyleSize::LengthPercentage(LengthPercentage::Zero());
    } else if (minISize.AsExtremumLength() ==
               StyleExtremumLength::MozFitContent) {
      // for 'min-inline-size', '-moz-fit-content' is like 'min-content'
      minISize = StyleSize::ExtremumLength(StyleExtremumLength::MinContent);
    }
  }
  if (minISize.ConvertsToLength() || minISize.IsExtremumLength()) {
    nscoord c = aFrame->ComputeISizeValue(aRenderingContext, 0, 0, 0, minISize);
    minCoord = std::max(c, minCoord);
    prefCoord = std::max(c, prefCoord);
  } else if (minISize.ConvertsToPercentage()) {
    float p = minISize.ToPercentage();
    if (p > prefPercent) {
      prefPercent = p;
    }
  }

  // XXX Should col frame have border/padding considered?
  if (aIsCell) {
    minCoord += boxSizingToBorderEdge;
    prefCoord = NSCoordSaturatingAdd(prefCoord, boxSizingToBorderEdge);
  }

  return CellISizeInfo(minCoord, prefCoord, prefPercent, hasSpecifiedISize);
}

static inline CellISizeInfo GetCellISizeInfo(gfxContext* aRenderingContext,
                                             nsTableCellFrame* aCellFrame,
                                             WritingMode aWM) {
  return GetISizeInfo(aRenderingContext, aCellFrame, aWM, true);
}

static inline CellISizeInfo GetColISizeInfo(gfxContext* aRenderingContext,
                                            nsIFrame* aFrame, WritingMode aWM) {
  return GetISizeInfo(aRenderingContext, aFrame, aWM, false);
}

/**
 * The algorithm in this function, in addition to meeting the
 * requirements of Web-compatibility, is also invariant under reordering
 * of the rows within a table (something that most, but not all, other
 * browsers are).
 */
void BasicTableLayoutStrategy::ComputeColumnIntrinsicISizes(
    gfxContext* aRenderingContext) {
  nsTableFrame* tableFrame = mTableFrame;
  nsTableCellMap* cellMap = tableFrame->GetCellMap();
  WritingMode wm = tableFrame->GetWritingMode();

  mozilla::AutoStackArena arena;
  SpanningCellSorter spanningCells;

  // Loop over the columns to consider the columns and cells *without*
  // a colspan.
  int32_t col, col_end;
  for (col = 0, col_end = cellMap->GetColCount(); col < col_end; ++col) {
    nsTableColFrame* colFrame = tableFrame->GetColFrame(col);
    if (!colFrame) {
      NS_ERROR("column frames out of sync with cell map");
      continue;
    }
    colFrame->ResetIntrinsics();
    colFrame->ResetSpanIntrinsics();

    // Consider the isizes on the column.
    CellISizeInfo colInfo = GetColISizeInfo(aRenderingContext, colFrame, wm);
    colFrame->AddCoords(colInfo.minCoord, colInfo.prefCoord,
                        colInfo.hasSpecifiedISize);
    colFrame->AddPrefPercent(colInfo.prefPercent);

    // Consider the isizes on the column-group.  Note that we follow
    // what the HTML spec says here, and make the isize apply to
    // each column in the group, not the group as a whole.

    // If column has isize, column-group doesn't override isize.
    if (colInfo.minCoord == 0 && colInfo.prefCoord == 0 &&
        colInfo.prefPercent == 0.0f) {
      NS_ASSERTION(colFrame->GetParent()->IsTableColGroupFrame(),
                   "expected a column-group");
      colInfo = GetColISizeInfo(aRenderingContext, colFrame->GetParent(), wm);
      colFrame->AddCoords(colInfo.minCoord, colInfo.prefCoord,
                          colInfo.hasSpecifiedISize);
      colFrame->AddPrefPercent(colInfo.prefPercent);
    }

    // Consider the contents of and the isizes on the cells without
    // colspans.
    nsCellMapColumnIterator columnIter(cellMap, col);
    int32_t row, colSpan;
    nsTableCellFrame* cellFrame;
    while ((cellFrame = columnIter.GetNextFrame(&row, &colSpan))) {
      if (colSpan > 1) {
        spanningCells.AddCell(colSpan, row, col);
        continue;
      }

      CellISizeInfo info = GetCellISizeInfo(aRenderingContext, cellFrame, wm);

      colFrame->AddCoords(info.minCoord, info.prefCoord,
                          info.hasSpecifiedISize);
      colFrame->AddPrefPercent(info.prefPercent);
    }
#ifdef DEBUG_dbaron_off
    printf("table %p col %d nonspan: min=%d pref=%d spec=%d pct=%f\n",
           mTableFrame, col, colFrame->GetMinCoord(), colFrame->GetPrefCoord(),
           colFrame->GetHasSpecifiedCoord(), colFrame->GetPrefPercent());
#endif
  }
#ifdef DEBUG_TABLE_STRATEGY
  printf("ComputeColumnIntrinsicISizes single\n");
  mTableFrame->Dump(false, true, false);
#endif

  // Consider the cells with a colspan that we saved in the loop above
  // into the spanning cell sorter.  We consider these cells by seeing
  // if they require adding to the isizes resulting only from cells
  // with a smaller colspan, and therefore we must process them sorted
  // in increasing order by colspan.  For each colspan group, we
  // accumulate new values to accumulate in the column frame's Span*
  // members.
  //
  // Considering things only relative to the isizes resulting from
  // cells with smaller colspans (rather than incrementally including
  // the results from spanning cells, or doing spanning and
  // non-spanning cells in a single pass) means that layout remains
  // row-order-invariant and (except for percentage isizes that add to
  // more than 100%) column-order invariant.
  //
  // Starting with smaller colspans makes it more likely that we
  // satisfy all the constraints given and don't distribute space to
  // columns where we don't need it.
  SpanningCellSorter::Item* item;
  int32_t colSpan;
  while ((item = spanningCells.GetNext(&colSpan))) {
    NS_ASSERTION(colSpan > 1,
                 "cell should not have been put in spanning cell sorter");
    do {
      int32_t row = item->row;
      col = item->col;
      CellData* cellData = cellMap->GetDataAt(row, col);
      NS_ASSERTION(cellData && cellData->IsOrig(),
                   "bogus result from spanning cell sorter");

      nsTableCellFrame* cellFrame = cellData->GetCellFrame();
      NS_ASSERTION(cellFrame, "bogus result from spanning cell sorter");

      CellISizeInfo info = GetCellISizeInfo(aRenderingContext, cellFrame, wm);

      if (info.prefPercent > 0.0f) {
        DistributePctISizeToColumns(info.prefPercent, col, colSpan);
      }
      DistributeISizeToColumns(info.minCoord, col, colSpan, BTLS_MIN_ISIZE,
                               info.hasSpecifiedISize);
      DistributeISizeToColumns(info.prefCoord, col, colSpan, BTLS_PREF_ISIZE,
                               info.hasSpecifiedISize);
    } while ((item = item->next));

    // Combine the results of the span analysis into the main results,
    // for each increment of colspan.

    for (col = 0, col_end = cellMap->GetColCount(); col < col_end; ++col) {
      nsTableColFrame* colFrame = tableFrame->GetColFrame(col);
      if (!colFrame) {
        NS_ERROR("column frames out of sync with cell map");
        continue;
      }

      colFrame->AccumulateSpanIntrinsics();
      colFrame->ResetSpanIntrinsics();

#ifdef DEBUG_dbaron_off
      printf("table %p col %d span %d: min=%d pref=%d spec=%d pct=%f\n",
             mTableFrame, col, colSpan, colFrame->GetMinCoord(),
             colFrame->GetPrefCoord(), colFrame->GetHasSpecifiedCoord(),
             colFrame->GetPrefPercent());
#endif
    }
  }

  // Prevent percentages from adding to more than 100% by (to be
  // compatible with other browsers) treating any percentages that would
  // increase the total percentage to more than 100% as the number that
  // would increase it to only 100% (which is 0% if we've already hit
  // 100%).  This means layout depends on the order of columns.
  float pct_used = 0.0f;
  for (col = 0, col_end = cellMap->GetColCount(); col < col_end; ++col) {
    nsTableColFrame* colFrame = tableFrame->GetColFrame(col);
    if (!colFrame) {
      NS_ERROR("column frames out of sync with cell map");
      continue;
    }

    colFrame->AdjustPrefPercent(&pct_used);
  }

#ifdef DEBUG_TABLE_STRATEGY
  printf("ComputeColumnIntrinsicISizes spanning\n");
  mTableFrame->Dump(false, true, false);
#endif
}

void BasicTableLayoutStrategy::ComputeIntrinsicISizes(
    gfxContext* aRenderingContext) {
  ComputeColumnIntrinsicISizes(aRenderingContext);

  nsTableCellMap* cellMap = mTableFrame->GetCellMap();
  nscoord min = 0, pref = 0, max_small_pct_pref = 0, nonpct_pref_total = 0;
  float pct_total = 0.0f;  // always from 0.0f - 1.0f
  int32_t colCount = cellMap->GetColCount();
  // add a total of (colcount + 1) lots of cellSpacingX for columns where a
  // cell originates
  nscoord add = mTableFrame->GetColSpacing(colCount);

  for (int32_t col = 0; col < colCount; ++col) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(col);
    if (!colFrame) {
      NS_ERROR("column frames out of sync with cell map");
      continue;
    }
    if (mTableFrame->ColumnHasCellSpacingBefore(col)) {
      add += mTableFrame->GetColSpacing(col - 1);
    }
    min += colFrame->GetMinCoord();
    pref = NSCoordSaturatingAdd(pref, colFrame->GetPrefCoord());

    // Percentages are of the table, so we have to reverse them for
    // intrinsic isizes.
    float p = colFrame->GetPrefPercent();
    if (p > 0.0f) {
      nscoord colPref = colFrame->GetPrefCoord();
      nscoord new_small_pct_expand =
          (colPref == nscoord_MAX ? nscoord_MAX : nscoord(float(colPref) / p));
      if (new_small_pct_expand > max_small_pct_pref) {
        max_small_pct_pref = new_small_pct_expand;
      }
      pct_total += p;
    } else {
      nonpct_pref_total =
          NSCoordSaturatingAdd(nonpct_pref_total, colFrame->GetPrefCoord());
    }
  }

  nscoord pref_pct_expand = pref;

  // Account for small percentages expanding the preferred isize of
  // *other* columns.
  if (max_small_pct_pref > pref_pct_expand) {
    pref_pct_expand = max_small_pct_pref;
  }

  // Account for large percentages expanding the preferred isize of
  // themselves.  There's no need to iterate over the columns multiple
  // times, since when there is such a need, the small percentage
  // effect is bigger anyway.  (I think!)
  NS_ASSERTION(0.0f <= pct_total && pct_total <= 1.0f,
               "column percentage inline-sizes not adjusted down to 100%");
  if (pct_total == 1.0f) {
    if (nonpct_pref_total > 0) {
      pref_pct_expand = nscoord_MAX;
      // XXX Or should I use some smaller value?  (Test this using
      // nested tables!)
    }
  } else {
    nscoord large_pct_pref =
        (nonpct_pref_total == nscoord_MAX
             ? nscoord_MAX
             : nscoord(float(nonpct_pref_total) / (1.0f - pct_total)));
    if (large_pct_pref > pref_pct_expand) pref_pct_expand = large_pct_pref;
  }

  // border-spacing isn't part of the basis for percentages
  if (colCount > 0) {
    min += add;
    pref = NSCoordSaturatingAdd(pref, add);
    pref_pct_expand = NSCoordSaturatingAdd(pref_pct_expand, add);
  }

  mMinISize = min;
  mPrefISize = pref;
  mPrefISizePctExpand = pref_pct_expand;
}

/* virtual */
void BasicTableLayoutStrategy::MarkIntrinsicISizesDirty() {
  mMinISize = NS_INTRINSIC_ISIZE_UNKNOWN;
  mPrefISize = NS_INTRINSIC_ISIZE_UNKNOWN;
  mPrefISizePctExpand = NS_INTRINSIC_ISIZE_UNKNOWN;
  mLastCalcISize = nscoord_MIN;
}

/* virtual */
void BasicTableLayoutStrategy::ComputeColumnISizes(
    const ReflowInput& aReflowInput) {
  nscoord iSize = aReflowInput.ComputedISize();

  if (mLastCalcISize == iSize) {
    return;
  }
  mLastCalcISize = iSize;

  NS_ASSERTION((mMinISize == NS_INTRINSIC_ISIZE_UNKNOWN) ==
                   (mPrefISize == NS_INTRINSIC_ISIZE_UNKNOWN),
               "dirtyness out of sync");
  NS_ASSERTION((mMinISize == NS_INTRINSIC_ISIZE_UNKNOWN) ==
                   (mPrefISizePctExpand == NS_INTRINSIC_ISIZE_UNKNOWN),
               "dirtyness out of sync");
  // XXX Is this needed?
  if (mMinISize == NS_INTRINSIC_ISIZE_UNKNOWN) {
    ComputeIntrinsicISizes(aReflowInput.mRenderingContext);
  }

  nsTableCellMap* cellMap = mTableFrame->GetCellMap();
  int32_t colCount = cellMap->GetColCount();
  if (colCount <= 0) return;  // nothing to do

  DistributeISizeToColumns(iSize, 0, colCount, BTLS_FINAL_ISIZE, false);

#ifdef DEBUG_TABLE_STRATEGY
  printf("ComputeColumnISizes final\n");
  mTableFrame->Dump(false, true, false);
#endif
}

void BasicTableLayoutStrategy::DistributePctISizeToColumns(float aSpanPrefPct,
                                                           int32_t aFirstCol,
                                                           int32_t aColCount) {
  // First loop to determine:
  int32_t nonPctColCount = 0;  // number of spanned columns without % isize
  nscoord nonPctTotalPrefISize = 0;  // total pref isize of those columns
  // and to reduce aSpanPrefPct by columns that already have % isize

  int32_t scol, scol_end;
  nsTableCellMap* cellMap = mTableFrame->GetCellMap();
  for (scol = aFirstCol, scol_end = aFirstCol + aColCount; scol < scol_end;
       ++scol) {
    nsTableColFrame* scolFrame = mTableFrame->GetColFrame(scol);
    if (!scolFrame) {
      NS_ERROR("column frames out of sync with cell map");
      continue;
    }
    float scolPct = scolFrame->GetPrefPercent();
    if (scolPct == 0.0f) {
      nonPctTotalPrefISize += scolFrame->GetPrefCoord();
      if (cellMap->GetNumCellsOriginatingInCol(scol) > 0) {
        ++nonPctColCount;
      }
    } else {
      aSpanPrefPct -= scolPct;
    }
  }

  if (aSpanPrefPct <= 0.0f || nonPctColCount == 0) {
    // There's no %-isize on the colspan left over to distribute,
    // or there are no columns to which we could distribute %-isize
    return;
  }

  // Second loop, to distribute what remains of aSpanPrefPct
  // between the non-percent-isize spanned columns
  const bool spanHasNonPctPref = nonPctTotalPrefISize > 0;  // Loop invariant
  for (scol = aFirstCol, scol_end = aFirstCol + aColCount; scol < scol_end;
       ++scol) {
    nsTableColFrame* scolFrame = mTableFrame->GetColFrame(scol);
    if (!scolFrame) {
      NS_ERROR("column frames out of sync with cell map");
      continue;
    }

    if (scolFrame->GetPrefPercent() == 0.0f) {
      NS_ASSERTION((!spanHasNonPctPref || nonPctTotalPrefISize != 0) &&
                       nonPctColCount != 0,
                   "should not be zero if we haven't allocated "
                   "all pref percent");

      float allocatedPct;  // % isize to be given to this column
      if (spanHasNonPctPref) {
        // Group so we're multiplying by 1.0f when we need
        // to use up aSpanPrefPct.
        allocatedPct = aSpanPrefPct * (float(scolFrame->GetPrefCoord()) /
                                       float(nonPctTotalPrefISize));
      } else if (cellMap->GetNumCellsOriginatingInCol(scol) > 0) {
        // distribute equally when all pref isizes are 0
        allocatedPct = aSpanPrefPct / float(nonPctColCount);
      } else {
        allocatedPct = 0.0f;
      }
      // Allocate the percent
      scolFrame->AddSpanPrefPercent(allocatedPct);

      // To avoid accumulating rounding error from division,
      // subtract this column's values from the totals.
      aSpanPrefPct -= allocatedPct;
      nonPctTotalPrefISize -= scolFrame->GetPrefCoord();
      if (cellMap->GetNumCellsOriginatingInCol(scol) > 0) {
        --nonPctColCount;
      }

      if (!aSpanPrefPct) {
        // No more span-percent-isize to distribute --> we're done.
        NS_ASSERTION(
            spanHasNonPctPref ? nonPctTotalPrefISize == 0 : nonPctColCount == 0,
            "No more pct inline-size to distribute, "
            "but there are still cols that need some.");
        return;
      }
    }
  }
}

void BasicTableLayoutStrategy::DistributeISizeToColumns(
    nscoord aISize, int32_t aFirstCol, int32_t aColCount,
    BtlsISizeType aISizeType, bool aSpanHasSpecifiedISize) {
  NS_ASSERTION(
      aISizeType != BTLS_FINAL_ISIZE ||
          (aFirstCol == 0 &&
           aColCount == mTableFrame->GetCellMap()->GetColCount()),
      "Computing final column isizes, but didn't get full column range");

  nscoord subtract = 0;
  // aISize initially includes border-spacing for the boundaries in between
  // each of the columns. We start at aFirstCol + 1 because the first
  // in-between boundary would be at the left edge of column aFirstCol + 1
  for (int32_t col = aFirstCol + 1; col < aFirstCol + aColCount; ++col) {
    if (mTableFrame->ColumnHasCellSpacingBefore(col)) {
      // border-spacing isn't part of the basis for percentages.
      subtract += mTableFrame->GetColSpacing(col - 1);
    }
  }
  if (aISizeType == BTLS_FINAL_ISIZE) {
    // If we're computing final col-isize, then aISize initially includes
    // border spacing on the table's far istart + far iend edge, too.  Need
    // to subtract those out, too.
    subtract += (mTableFrame->GetColSpacing(-1) +
                 mTableFrame->GetColSpacing(aColCount));
  }
  aISize = NSCoordSaturatingSubtract(aISize, subtract, nscoord_MAX);

  /*
   * The goal of this function is to distribute |aISize| between the
   * columns by making an appropriate AddSpanCoords or SetFinalISize
   * call for each column.  (We call AddSpanCoords if we're
   * distributing a column-spanning cell's minimum or preferred isize
   * to its spanned columns.  We call SetFinalISize if we're
   * distributing a table's final isize to its columns.)
   *
   * The idea is to either assign one of the following sets of isizes
   * or a weighted average of two adjacent sets of isizes.  It is not
   * possible to assign values smaller than the smallest set of
   * isizes.  However, see below for handling the case of assigning
   * values larger than the largest set of isizes.  From smallest to
   * largest, these are:
   *
   * 1. [guess_min] Assign all columns their min isize.
   *
   * 2. [guess_min_pct] Assign all columns with percentage isizes
   * their percentage isize, and all other columns their min isize.
   *
   * 3. [guess_min_spec] Assign all columns with percentage isizes
   * their percentage isize, all columns with specified coordinate
   * isizes their pref isize (since it doesn't matter whether it's the
   * largest contributor to the pref isize that was the specified
   * contributor), and all other columns their min isize.
   *
   * 4. [guess_pref] Assign all columns with percentage isizes their
   * specified isize, and all other columns their pref isize.
   *
   * If |aISize| is *larger* than what we would assign in (4), then we
   * expand the columns:
   *
   *   a. if any columns without a specified coordinate isize or
   *   percent isize have nonzero pref isize, in proportion to pref
   *   isize [total_flex_pref]
   *
   *   b. otherwise, if any columns without a specified coordinate
   *   isize or percent isize, but with cells originating in them,
   *   have zero pref isize, equally between these
   *   [numNonSpecZeroISizeCols]
   *
   *   c. otherwise, if any columns without percent isize have nonzero
   *   pref isize, in proportion to pref isize [total_fixed_pref]
   *
   *   d. otherwise, if any columns have nonzero percentage isizes, in
   *   proportion to the percentage isizes [total_pct]
   *
   *   e. otherwise, equally.
   */

  // Loop #1 over the columns, to figure out the four values above so
  // we know which case we're dealing with.

  nscoord guess_min = 0, guess_min_pct = 0, guess_min_spec = 0, guess_pref = 0,
          total_flex_pref = 0, total_fixed_pref = 0;
  float total_pct = 0.0f;  // 0.0f to 1.0f
  int32_t numInfiniteISizeCols = 0;
  int32_t numNonSpecZeroISizeCols = 0;

  int32_t col;
  nsTableCellMap* cellMap = mTableFrame->GetCellMap();
  for (col = aFirstCol; col < aFirstCol + aColCount; ++col) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(col);
    if (!colFrame) {
      NS_ERROR("column frames out of sync with cell map");
      continue;
    }
    nscoord min_iSize = colFrame->GetMinCoord();
    guess_min += min_iSize;
    if (colFrame->GetPrefPercent() != 0.0f) {
      float pct = colFrame->GetPrefPercent();
      total_pct += pct;
      nscoord val = nscoord(float(aISize) * pct);
      if (val < min_iSize) {
        val = min_iSize;
      }
      guess_min_pct += val;
      guess_pref = NSCoordSaturatingAdd(guess_pref, val);
    } else {
      nscoord pref_iSize = colFrame->GetPrefCoord();
      if (pref_iSize == nscoord_MAX) {
        ++numInfiniteISizeCols;
      }
      guess_pref = NSCoordSaturatingAdd(guess_pref, pref_iSize);
      guess_min_pct += min_iSize;
      if (colFrame->GetHasSpecifiedCoord()) {
        // we'll add on the rest of guess_min_spec outside the
        // loop
        nscoord delta = NSCoordSaturatingSubtract(pref_iSize, min_iSize, 0);
        guess_min_spec = NSCoordSaturatingAdd(guess_min_spec, delta);
        total_fixed_pref = NSCoordSaturatingAdd(total_fixed_pref, pref_iSize);
      } else if (pref_iSize == 0) {
        if (cellMap->GetNumCellsOriginatingInCol(col) > 0) {
          ++numNonSpecZeroISizeCols;
        }
      } else {
        total_flex_pref = NSCoordSaturatingAdd(total_flex_pref, pref_iSize);
      }
    }
  }
  guess_min_spec = NSCoordSaturatingAdd(guess_min_spec, guess_min_pct);

  // Determine what we're flexing:
  enum Loop2Type {
    FLEX_PCT_SMALL,        // between (1) and (2) above
    FLEX_FIXED_SMALL,      // between (2) and (3) above
    FLEX_FLEX_SMALL,       // between (3) and (4) above
    FLEX_FLEX_LARGE,       // greater than (4) above, case (a)
    FLEX_FLEX_LARGE_ZERO,  // greater than (4) above, case (b)
    FLEX_FIXED_LARGE,      // greater than (4) above, case (c)
    FLEX_PCT_LARGE,        // greater than (4) above, case (d)
    FLEX_ALL_LARGE         // greater than (4) above, case (e)
  };

  Loop2Type l2t;
  // These are constants (over columns) for each case's math.  We use
  // a pair of nscoords rather than a float so that we can subtract
  // each column's allocation so we avoid accumulating rounding error.
  nscoord space;  // the amount of extra isize to allocate
  union {
    nscoord c;
    float f;
  } basis;  // the sum of the statistic over columns to divide it
  if (aISize < guess_pref) {
    if (aISizeType != BTLS_FINAL_ISIZE && aISize <= guess_min) {
      // Return early -- we don't have any extra space to distribute.
      return;
    }
    NS_ASSERTION(!(aISizeType == BTLS_FINAL_ISIZE && aISize < guess_min),
                 "Table inline-size is less than the "
                 "sum of its columns' min inline-sizes");
    if (aISize < guess_min_pct) {
      l2t = FLEX_PCT_SMALL;
      space = aISize - guess_min;
      basis.c = guess_min_pct - guess_min;
    } else if (aISize < guess_min_spec) {
      l2t = FLEX_FIXED_SMALL;
      space = aISize - guess_min_pct;
      basis.c =
          NSCoordSaturatingSubtract(guess_min_spec, guess_min_pct, nscoord_MAX);
    } else {
      l2t = FLEX_FLEX_SMALL;
      space = aISize - guess_min_spec;
      basis.c =
          NSCoordSaturatingSubtract(guess_pref, guess_min_spec, nscoord_MAX);
    }
  } else {
    space = NSCoordSaturatingSubtract(aISize, guess_pref, nscoord_MAX);
    if (total_flex_pref > 0) {
      l2t = FLEX_FLEX_LARGE;
      basis.c = total_flex_pref;
    } else if (numNonSpecZeroISizeCols > 0) {
      l2t = FLEX_FLEX_LARGE_ZERO;
      basis.c = numNonSpecZeroISizeCols;
    } else if (total_fixed_pref > 0) {
      l2t = FLEX_FIXED_LARGE;
      basis.c = total_fixed_pref;
    } else if (total_pct > 0.0f) {
      l2t = FLEX_PCT_LARGE;
      basis.f = total_pct;
    } else {
      l2t = FLEX_ALL_LARGE;
      basis.c = aColCount;
    }
  }

#ifdef DEBUG_dbaron_off
  printf(
      "ComputeColumnISizes: %d columns in isize %d,\n"
      "  guesses=[%d,%d,%d,%d], totals=[%d,%d,%f],\n"
      "  l2t=%d, space=%d, basis.c=%d\n",
      aColCount, aISize, guess_min, guess_min_pct, guess_min_spec, guess_pref,
      total_flex_pref, total_fixed_pref, total_pct, l2t, space, basis.c);
#endif

  for (col = aFirstCol; col < aFirstCol + aColCount; ++col) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(col);
    if (!colFrame) {
      NS_ERROR("column frames out of sync with cell map");
      continue;
    }
    nscoord col_iSize;

    float pct = colFrame->GetPrefPercent();
    if (pct != 0.0f) {
      col_iSize = nscoord(float(aISize) * pct);
      nscoord col_min = colFrame->GetMinCoord();
      if (col_iSize < col_min) {
        col_iSize = col_min;
      }
    } else {
      col_iSize = colFrame->GetPrefCoord();
    }

    nscoord col_iSize_before_adjust = col_iSize;

    switch (l2t) {
      case FLEX_PCT_SMALL:
        col_iSize = col_iSize_before_adjust = colFrame->GetMinCoord();
        if (pct != 0.0f) {
          nscoord pct_minus_min = nscoord(float(aISize) * pct) - col_iSize;
          if (pct_minus_min > 0) {
            float c = float(space) / float(basis.c);
            basis.c -= pct_minus_min;
            col_iSize += NSToCoordRound(float(pct_minus_min) * c);
          }
        }
        break;
      case FLEX_FIXED_SMALL:
        if (pct == 0.0f) {
          NS_ASSERTION(col_iSize == colFrame->GetPrefCoord(),
                       "wrong inline-size assigned");
          if (colFrame->GetHasSpecifiedCoord()) {
            nscoord col_min = colFrame->GetMinCoord();
            nscoord pref_minus_min = col_iSize - col_min;
            col_iSize = col_iSize_before_adjust = col_min;
            if (pref_minus_min != 0) {
              float c = float(space) / float(basis.c);
              basis.c -= pref_minus_min;
              col_iSize += NSToCoordRound(float(pref_minus_min) * c);
            }
          } else
            col_iSize = col_iSize_before_adjust = colFrame->GetMinCoord();
        }
        break;
      case FLEX_FLEX_SMALL:
        if (pct == 0.0f && !colFrame->GetHasSpecifiedCoord()) {
          NS_ASSERTION(col_iSize == colFrame->GetPrefCoord(),
                       "wrong inline-size assigned");
          nscoord col_min = colFrame->GetMinCoord();
          nscoord pref_minus_min =
              NSCoordSaturatingSubtract(col_iSize, col_min, 0);
          col_iSize = col_iSize_before_adjust = col_min;
          if (pref_minus_min != 0) {
            float c = float(space) / float(basis.c);
            // If we have infinite-isize cols, then the standard
            // adjustment to col_iSize using 'c' won't work,
            // because basis.c and pref_minus_min are both
            // nscoord_MAX and will cancel each other out in the
            // col_iSize adjustment (making us assign all the
            // space to the first inf-isize col).  To correct for
            // this, we'll also divide by numInfiniteISizeCols to
            // spread the space equally among the inf-isize cols.
            if (numInfiniteISizeCols) {
              if (colFrame->GetPrefCoord() == nscoord_MAX) {
                c = c / float(numInfiniteISizeCols);
                --numInfiniteISizeCols;
              } else {
                c = 0.0f;
              }
            }
            basis.c =
                NSCoordSaturatingSubtract(basis.c, pref_minus_min, nscoord_MAX);
            col_iSize += NSToCoordRound(float(pref_minus_min) * c);
          }
        }
        break;
      case FLEX_FLEX_LARGE:
        if (pct == 0.0f && !colFrame->GetHasSpecifiedCoord()) {
          NS_ASSERTION(col_iSize == colFrame->GetPrefCoord(),
                       "wrong inline-size assigned");
          if (col_iSize != 0) {
            if (space == nscoord_MAX) {
              basis.c -= col_iSize;
              col_iSize = nscoord_MAX;
            } else {
              float c = float(space) / float(basis.c);
              basis.c -= col_iSize;
              col_iSize += NSToCoordRound(float(col_iSize) * c);
            }
          }
        }
        break;
      case FLEX_FLEX_LARGE_ZERO:
        if (pct == 0.0f && !colFrame->GetHasSpecifiedCoord() &&
            cellMap->GetNumCellsOriginatingInCol(col) > 0) {
          NS_ASSERTION(col_iSize == 0 && colFrame->GetPrefCoord() == 0,
                       "Since we're in FLEX_FLEX_LARGE_ZERO case, "
                       "all auto-inline-size cols should have zero "
                       "pref inline-size.");
          float c = float(space) / float(basis.c);
          col_iSize += NSToCoordRound(c);
          --basis.c;
        }
        break;
      case FLEX_FIXED_LARGE:
        if (pct == 0.0f) {
          NS_ASSERTION(col_iSize == colFrame->GetPrefCoord(),
                       "wrong inline-size assigned");
          NS_ASSERTION(
              colFrame->GetHasSpecifiedCoord() || colFrame->GetPrefCoord() == 0,
              "wrong case");
          if (col_iSize != 0) {
            float c = float(space) / float(basis.c);
            basis.c -= col_iSize;
            col_iSize += NSToCoordRound(float(col_iSize) * c);
          }
        }
        break;
      case FLEX_PCT_LARGE:
        NS_ASSERTION(pct != 0.0f || colFrame->GetPrefCoord() == 0,
                     "wrong case");
        if (pct != 0.0f) {
          float c = float(space) / basis.f;
          col_iSize += NSToCoordRound(pct * c);
          basis.f -= pct;
        }
        break;
      case FLEX_ALL_LARGE: {
        float c = float(space) / float(basis.c);
        col_iSize += NSToCoordRound(c);
        --basis.c;
      } break;
    }

    // Only subtract from space if it's a real number.
    if (space != nscoord_MAX) {
      NS_ASSERTION(col_iSize != nscoord_MAX,
                   "How is col_iSize nscoord_MAX if space isn't?");
      NS_ASSERTION(
          col_iSize_before_adjust != nscoord_MAX,
          "How is col_iSize_before_adjust nscoord_MAX if space isn't?");
      space -= col_iSize - col_iSize_before_adjust;
    }

    NS_ASSERTION(col_iSize >= colFrame->GetMinCoord(),
                 "assigned inline-size smaller than min");

    // Apply the new isize
    switch (aISizeType) {
      case BTLS_MIN_ISIZE: {
        // Note: AddSpanCoords requires both a min and pref isize.
        // For the pref isize, we'll just pass in our computed
        // min isize, because the real pref isize will be at least
        // as big
        colFrame->AddSpanCoords(col_iSize, col_iSize, aSpanHasSpecifiedISize);
      } break;
      case BTLS_PREF_ISIZE: {
        // Note: AddSpanCoords requires both a min and pref isize.
        // For the min isize, we'll just pass in 0, because
        // the real min isize will be at least 0
        colFrame->AddSpanCoords(0, col_iSize, aSpanHasSpecifiedISize);
      } break;
      case BTLS_FINAL_ISIZE: {
        nscoord old_final = colFrame->GetFinalISize();
        colFrame->SetFinalISize(col_iSize);

        if (old_final != col_iSize) {
          mTableFrame->DidResizeColumns();
        }
      } break;
    }
  }
  NS_ASSERTION(
      (space == 0 || space == nscoord_MAX) &&
          ((l2t == FLEX_PCT_LARGE) ? (-0.001f < basis.f && basis.f < 0.001f)
                                   : (basis.c == 0 || basis.c == nscoord_MAX)),
      "didn't subtract all that we added");
}
