/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Web-compatible algorithms that determine column and table widths,
 * used for CSS2's 'table-layout: auto'.
 */

#include "BasicTableLayoutStrategy.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsLayoutUtils.h"
#include "nsGkAtoms.h"
#include "SpanningCellSorter.h"

using namespace mozilla;
using namespace mozilla::layout;

namespace css = mozilla::css;

#undef  DEBUG_TABLE_STRATEGY 

BasicTableLayoutStrategy::BasicTableLayoutStrategy(nsTableFrame *aTableFrame)
  : nsITableLayoutStrategy(nsITableLayoutStrategy::Auto)
  , mTableFrame(aTableFrame)
{
    MarkIntrinsicWidthsDirty();
}

/* virtual */
BasicTableLayoutStrategy::~BasicTableLayoutStrategy()
{
}

/* virtual */ nscoord
BasicTableLayoutStrategy::GetMinWidth(nsRenderingContext* aRenderingContext)
{
    DISPLAY_MIN_WIDTH(mTableFrame, mMinWidth);
    if (mMinWidth == NS_INTRINSIC_WIDTH_UNKNOWN)
        ComputeIntrinsicWidths(aRenderingContext);
    return mMinWidth;
}

/* virtual */ nscoord
BasicTableLayoutStrategy::GetPrefWidth(nsRenderingContext* aRenderingContext,
                                       bool aComputingSize)
{
    DISPLAY_PREF_WIDTH(mTableFrame, mPrefWidth);
    NS_ASSERTION((mPrefWidth == NS_INTRINSIC_WIDTH_UNKNOWN) ==
                 (mPrefWidthPctExpand == NS_INTRINSIC_WIDTH_UNKNOWN),
                 "dirtyness out of sync");
    if (mPrefWidth == NS_INTRINSIC_WIDTH_UNKNOWN)
        ComputeIntrinsicWidths(aRenderingContext);
    return aComputingSize ? mPrefWidthPctExpand : mPrefWidth;
}

struct CellWidthInfo {
    CellWidthInfo(nscoord aMinCoord, nscoord aPrefCoord,
                  float aPrefPercent, bool aHasSpecifiedWidth)
        : hasSpecifiedWidth(aHasSpecifiedWidth)
        , minCoord(aMinCoord)
        , prefCoord(aPrefCoord)
        , prefPercent(aPrefPercent)
    {
    }

    bool hasSpecifiedWidth;
    nscoord minCoord;
    nscoord prefCoord;
    float prefPercent;
};

// Used for both column and cell calculations.  The parts needed only
// for cells are skipped when aIsCell is false.
static CellWidthInfo
GetWidthInfo(nsRenderingContext *aRenderingContext,
             nsIFrame *aFrame, bool aIsCell)
{
    nscoord minCoord, prefCoord;
    const nsStylePosition *stylePos = aFrame->GetStylePosition();
    bool isQuirks = aFrame->PresContext()->CompatibilityMode() ==
                    eCompatibility_NavQuirks;
    nscoord boxSizingToBorderEdge = 0;
    if (aIsCell) {
        // If aFrame is a container for font size inflation, then shrink
        // wrapping inside of it should not apply font size inflation.
        AutoMaybeDisableFontInflation an(aFrame);

        minCoord = aFrame->GetMinWidth(aRenderingContext);
        prefCoord = aFrame->GetPrefWidth(aRenderingContext);
        // Until almost the end of this function, minCoord and prefCoord
        // represent the box-sizing based width values (which mean they
        // should include horizontal padding and border width when
        // box-sizing is set to border-box).
        // Note that this function returns border-box width, we add the
        // outer edges near the end of this function.

        // XXX Should we ignore percentage padding?
        nsIFrame::IntrinsicWidthOffsetData offsets = aFrame->IntrinsicWidthOffsets(aRenderingContext);

        // In quirks mode, table cell width should be content-box,
        // but height should be border box.
        // Because of this historic anomaly, we do not use quirk.css.
        // (We can't specify one value of box-sizing for width and another
        // for height).
        // For this reason, we also do not use box-sizing for just one of
        // them, as this may be confusing.
        if (isQuirks) {
            boxSizingToBorderEdge = offsets.hPadding + offsets.hBorder;
        }
        else {
            switch (stylePos->mBoxSizing) {
                case NS_STYLE_BOX_SIZING_CONTENT:
                    boxSizingToBorderEdge = offsets.hPadding + offsets.hBorder;
                    break;
                case NS_STYLE_BOX_SIZING_PADDING:
                    minCoord += offsets.hPadding;
                    prefCoord += offsets.hPadding;
                    boxSizingToBorderEdge = offsets.hBorder;
                    break;
                default:
                    // NS_STYLE_BOX_SIZING_BORDER
                    minCoord += offsets.hPadding + offsets.hBorder;
                    prefCoord += offsets.hPadding + offsets.hBorder;
                    break;
            }
        }
    } else {
        minCoord = 0;
        prefCoord = 0;
    }
    float prefPercent = 0.0f;
    bool hasSpecifiedWidth = false;

    const nsStyleCoord &width = stylePos->mWidth;
    nsStyleUnit unit = width.GetUnit();
    // NOTE: We're ignoring calc() units here, for lack of a sensible
    // idea for what to do with them.  This means calc() is basically
    // handled like 'auto' for table cells and columns.
    if (unit == eStyleUnit_Coord) {
        hasSpecifiedWidth = true;
        // Note: since ComputeWidthValue was designed to return content-box
        // width, it will (in some cases) subtract the box-sizing edges.
        // We prevent this unwanted behavior by calling it with
        // aContentEdgeToBoxSizing and aBoxSizingToMarginEdge set to 0.
        nscoord w = nsLayoutUtils::ComputeWidthValue(aRenderingContext,
                                                     aFrame, 0, 0, 0, width);
        // Quirk: A cell with "nowrap" set and a coord value for the
        // width which is bigger than the intrinsic minimum width uses
        // that coord value as the minimum width.
        // This is kept up-to-date with dynamic changes to nowrap by code in
        // nsTableCellFrame::AttributeChanged
        if (aIsCell && w > minCoord && isQuirks &&
            aFrame->GetContent()->HasAttr(kNameSpaceID_None,
                                          nsGkAtoms::nowrap)) {
            minCoord = w;
        }
        prefCoord = NS_MAX(w, minCoord);
    } else if (unit == eStyleUnit_Percent) {
        prefPercent = width.GetPercentValue();
    } else if (unit == eStyleUnit_Enumerated && aIsCell) {
        switch (width.GetIntValue()) {
            case NS_STYLE_WIDTH_MAX_CONTENT:
                // 'width' only affects pref width, not min
                // width, so don't change anything
                break;
            case NS_STYLE_WIDTH_MIN_CONTENT:
                prefCoord = minCoord;
                break;
            case NS_STYLE_WIDTH_FIT_CONTENT:
            case NS_STYLE_WIDTH_AVAILABLE:
                // act just like 'width: auto'
                break;
            default:
                NS_NOTREACHED("unexpected enumerated value");
        }
    }

    nsStyleCoord maxWidth(stylePos->mMaxWidth);
    if (maxWidth.GetUnit() == eStyleUnit_Enumerated) {
        if (!aIsCell || maxWidth.GetIntValue() == NS_STYLE_WIDTH_AVAILABLE)
            maxWidth.SetNoneValue();
        else if (maxWidth.GetIntValue() == NS_STYLE_WIDTH_FIT_CONTENT)
            // for 'max-width', '-moz-fit-content' is like
            // '-moz-max-content'
            maxWidth.SetIntValue(NS_STYLE_WIDTH_MAX_CONTENT,
                                 eStyleUnit_Enumerated);
    }
    unit = maxWidth.GetUnit();
    // XXX To really implement 'max-width' well, we'd need to store
    // it separately on the columns.
    if (unit == eStyleUnit_Coord || unit == eStyleUnit_Enumerated) {
        nscoord w =
            nsLayoutUtils::ComputeWidthValue(aRenderingContext, aFrame,
                                             0, 0, 0, maxWidth);
        if (w < minCoord)
            minCoord = w;
        if (w < prefCoord)
            prefCoord = w;
    } else if (unit == eStyleUnit_Percent) {
        float p = stylePos->mMaxWidth.GetPercentValue();
        if (p < prefPercent)
            prefPercent = p;
    }
    // treat calc() on max-width just like 'none'.

    nsStyleCoord minWidth(stylePos->mMinWidth);
    if (minWidth.GetUnit() == eStyleUnit_Enumerated) {
        if (!aIsCell || minWidth.GetIntValue() == NS_STYLE_WIDTH_AVAILABLE)
            minWidth.SetCoordValue(0);
        else if (minWidth.GetIntValue() == NS_STYLE_WIDTH_FIT_CONTENT)
            // for 'min-width', '-moz-fit-content' is like
            // '-moz-min-content'
            minWidth.SetIntValue(NS_STYLE_WIDTH_MIN_CONTENT,
                                 eStyleUnit_Enumerated);
    }
    unit = minWidth.GetUnit();
    if (unit == eStyleUnit_Coord || unit == eStyleUnit_Enumerated) {
        nscoord w =
            nsLayoutUtils::ComputeWidthValue(aRenderingContext, aFrame,
                                             0, 0, 0, minWidth);
        if (w > minCoord)
            minCoord = w;
        if (w > prefCoord)
            prefCoord = w;
    } else if (unit == eStyleUnit_Percent) {
        float p = stylePos->mMinWidth.GetPercentValue();
        if (p > prefPercent)
            prefPercent = p;
    }
    // treat calc() on min-width just like '0'.

    // XXX Should col frame have border/padding considered?
    if (aIsCell) {
        minCoord += boxSizingToBorderEdge;
        prefCoord = NSCoordSaturatingAdd(prefCoord, boxSizingToBorderEdge);
    }

    return CellWidthInfo(minCoord, prefCoord, prefPercent, hasSpecifiedWidth);
}

static inline CellWidthInfo
GetCellWidthInfo(nsRenderingContext *aRenderingContext,
                 nsTableCellFrame *aCellFrame)
{
    return GetWidthInfo(aRenderingContext, aCellFrame, true);
}

static inline CellWidthInfo
GetColWidthInfo(nsRenderingContext *aRenderingContext,
                nsIFrame *aFrame)
{
    return GetWidthInfo(aRenderingContext, aFrame, false);
}


/**
 * The algorithm in this function, in addition to meeting the
 * requirements of Web-compatibility, is also invariant under reordering
 * of the rows within a table (something that most, but not all, other
 * browsers are).
 */
void
BasicTableLayoutStrategy::ComputeColumnIntrinsicWidths(nsRenderingContext* aRenderingContext)
{
    nsTableFrame *tableFrame = mTableFrame;
    nsTableCellMap *cellMap = tableFrame->GetCellMap();

    mozilla::AutoStackArena arena;
    SpanningCellSorter spanningCells;

    // Loop over the columns to consider the columns and cells *without*
    // a colspan.
    PRInt32 col, col_end;
    for (col = 0, col_end = cellMap->GetColCount(); col < col_end; ++col) {
        nsTableColFrame *colFrame = tableFrame->GetColFrame(col);
        if (!colFrame) {
            NS_ERROR("column frames out of sync with cell map");
            continue;
        }
        colFrame->ResetIntrinsics();
        colFrame->ResetSpanIntrinsics();

        // Consider the widths on the column.
        CellWidthInfo colInfo = GetColWidthInfo(aRenderingContext, colFrame);
        colFrame->AddCoords(colInfo.minCoord, colInfo.prefCoord,
                            colInfo.hasSpecifiedWidth);
        colFrame->AddPrefPercent(colInfo.prefPercent);

        // Consider the widths on the column-group.  Note that we follow
        // what the HTML spec says here, and make the width apply to
        // each column in the group, not the group as a whole.

        // If column has width, column-group doesn't override width.
        if (colInfo.minCoord == 0 && colInfo.prefCoord == 0 &&
            colInfo.prefPercent == 0.0f) {
            NS_ASSERTION(colFrame->GetParent()->GetType() ==
                             nsGkAtoms::tableColGroupFrame,
                         "expected a column-group");
            colInfo = GetColWidthInfo(aRenderingContext, colFrame->GetParent());
            colFrame->AddCoords(colInfo.minCoord, colInfo.prefCoord,
                                colInfo.hasSpecifiedWidth);
            colFrame->AddPrefPercent(colInfo.prefPercent);
        }

        // Consider the contents of and the widths on the cells without
        // colspans.
        nsCellMapColumnIterator columnIter(cellMap, col);
        PRInt32 row, colSpan;
        nsTableCellFrame* cellFrame;
        while ((cellFrame = columnIter.GetNextFrame(&row, &colSpan))) {
            if (colSpan > 1) {
                spanningCells.AddCell(colSpan, row, col);
                continue;
            }

            CellWidthInfo info = GetCellWidthInfo(aRenderingContext, cellFrame);

            colFrame->AddCoords(info.minCoord, info.prefCoord,
                                info.hasSpecifiedWidth);
            colFrame->AddPrefPercent(info.prefPercent);
        }
#ifdef DEBUG_dbaron_off
        printf("table %p col %d nonspan: min=%d pref=%d spec=%d pct=%f\n",
               mTableFrame, col, colFrame->GetMinCoord(),
               colFrame->GetPrefCoord(), colFrame->GetHasSpecifiedCoord(),
               colFrame->GetPrefPercent());
#endif
    }
#ifdef DEBUG_TABLE_STRATEGY
    printf("ComputeColumnIntrinsicWidths single\n");
    mTableFrame->Dump(false, true, false);
#endif

    // Consider the cells with a colspan that we saved in the loop above
    // into the spanning cell sorter.  We consider these cells by seeing
    // if they require adding to the widths resulting only from cells
    // with a smaller colspan, and therefore we must process them sorted
    // in increasing order by colspan.  For each colspan group, we
    // accumulate new values to accumulate in the column frame's Span*
    // members.
    //
    // Considering things only relative to the widths resulting from
    // cells with smaller colspans (rather than incrementally including
    // the results from spanning cells, or doing spanning and
    // non-spanning cells in a single pass) means that layout remains
    // row-order-invariant and (except for percentage widths that add to
    // more than 100%) column-order invariant.
    //
    // Starting with smaller colspans makes it more likely that we
    // satisfy all the constraints given and don't distribute space to
    // columns where we don't need it.
    SpanningCellSorter::Item *item;
    PRInt32 colSpan;
    while ((item = spanningCells.GetNext(&colSpan))) {
        NS_ASSERTION(colSpan > 1,
                     "cell should not have been put in spanning cell sorter");
        do {
            PRInt32 row = item->row;
            col = item->col;
            CellData *cellData = cellMap->GetDataAt(row, col);
            NS_ASSERTION(cellData && cellData->IsOrig(),
                         "bogus result from spanning cell sorter");

            nsTableCellFrame *cellFrame = cellData->GetCellFrame();
            NS_ASSERTION(cellFrame, "bogus result from spanning cell sorter");

            CellWidthInfo info = GetCellWidthInfo(aRenderingContext, cellFrame);

            if (info.prefPercent > 0.0f) {
                DistributePctWidthToColumns(info.prefPercent,
                                            col, colSpan);
            }
            DistributeWidthToColumns(info.minCoord, col, colSpan, 
                                     BTLS_MIN_WIDTH, info.hasSpecifiedWidth);
            DistributeWidthToColumns(info.prefCoord, col, colSpan, 
                                     BTLS_PREF_WIDTH, info.hasSpecifiedWidth);
        } while ((item = item->next));

        // Combine the results of the span analysis into the main results,
        // for each increment of colspan.

        for (col = 0, col_end = cellMap->GetColCount(); col < col_end; ++col) {
            nsTableColFrame *colFrame = tableFrame->GetColFrame(col);
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
        nsTableColFrame *colFrame = tableFrame->GetColFrame(col);
        if (!colFrame) {
            NS_ERROR("column frames out of sync with cell map");
            continue;
        }

        colFrame->AdjustPrefPercent(&pct_used);
    }

#ifdef DEBUG_TABLE_STRATEGY
    printf("ComputeColumnIntrinsicWidths spanning\n");
    mTableFrame->Dump(false, true, false);
#endif
}

void
BasicTableLayoutStrategy::ComputeIntrinsicWidths(nsRenderingContext* aRenderingContext)
{
    ComputeColumnIntrinsicWidths(aRenderingContext);

    nsTableCellMap *cellMap = mTableFrame->GetCellMap();
    nscoord min = 0, pref = 0, max_small_pct_pref = 0, nonpct_pref_total = 0;
    float pct_total = 0.0f; // always from 0.0f - 1.0f
    PRInt32 colCount = cellMap->GetColCount();
    nscoord spacing = mTableFrame->GetCellSpacingX();
    nscoord add = spacing; // add (colcount + 1) * spacing for columns 
                           // where a cell originates

    for (PRInt32 col = 0; col < colCount; ++col) {
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
        if (!colFrame) {
            NS_ERROR("column frames out of sync with cell map");
            continue;
        }
        if (mTableFrame->ColumnHasCellSpacingBefore(col)) {
            add += spacing;
        }
        min += colFrame->GetMinCoord();
        pref = NSCoordSaturatingAdd(pref, colFrame->GetPrefCoord());

        // Percentages are of the table, so we have to reverse them for
        // intrinsic widths.
        float p = colFrame->GetPrefPercent();
        if (p > 0.0f) {
            nscoord colPref = colFrame->GetPrefCoord();
            nscoord new_small_pct_expand = 
                (colPref == nscoord_MAX ?
                 nscoord_MAX : nscoord(float(colPref) / p));
            if (new_small_pct_expand > max_small_pct_pref) {
                max_small_pct_pref = new_small_pct_expand;
            }
            pct_total += p;
        } else {
            nonpct_pref_total = NSCoordSaturatingAdd(nonpct_pref_total, 
                                                     colFrame->GetPrefCoord());
        }
    }

    nscoord pref_pct_expand = pref;

    // Account for small percentages expanding the preferred width of
    // *other* columns.
    if (max_small_pct_pref > pref_pct_expand) {
        pref_pct_expand = max_small_pct_pref;
    }

    // Account for large percentages expanding the preferred width of
    // themselves.  There's no need to iterate over the columns multiple
    // times, since when there is such a need, the small percentage
    // effect is bigger anyway.  (I think!)
    NS_ASSERTION(0.0f <= pct_total && pct_total <= 1.0f,
                 "column percentage widths not adjusted down to 100%");
    if (pct_total == 1.0f) {
        if (nonpct_pref_total > 0) {
            pref_pct_expand = nscoord_MAX;
            // XXX Or should I use some smaller value?  (Test this using
            // nested tables!)
        }
    } else {
        nscoord large_pct_pref =
            (nonpct_pref_total == nscoord_MAX ?
             nscoord_MAX :
             nscoord(float(nonpct_pref_total) / (1.0f - pct_total)));
        if (large_pct_pref > pref_pct_expand)
            pref_pct_expand = large_pct_pref;
    }

    // border-spacing isn't part of the basis for percentages
    if (colCount > 0) {
        min += add;
        pref = NSCoordSaturatingAdd(pref, add);
        pref_pct_expand = NSCoordSaturatingAdd(pref_pct_expand, add);
    }

    mMinWidth = min;
    mPrefWidth = pref;
    mPrefWidthPctExpand = pref_pct_expand;
}

/* virtual */ void
BasicTableLayoutStrategy::MarkIntrinsicWidthsDirty()
{
    mMinWidth = NS_INTRINSIC_WIDTH_UNKNOWN;
    mPrefWidth = NS_INTRINSIC_WIDTH_UNKNOWN;
    mPrefWidthPctExpand = NS_INTRINSIC_WIDTH_UNKNOWN;
    mLastCalcWidth = nscoord_MIN;
}

/* virtual */ void
BasicTableLayoutStrategy::ComputeColumnWidths(const nsHTMLReflowState& aReflowState)
{
    nscoord width = aReflowState.ComputedWidth();

    if (mLastCalcWidth == width)
        return;
    mLastCalcWidth = width;

    NS_ASSERTION((mMinWidth == NS_INTRINSIC_WIDTH_UNKNOWN) ==
                 (mPrefWidth == NS_INTRINSIC_WIDTH_UNKNOWN),
                 "dirtyness out of sync");
    NS_ASSERTION((mMinWidth == NS_INTRINSIC_WIDTH_UNKNOWN) ==
                 (mPrefWidthPctExpand == NS_INTRINSIC_WIDTH_UNKNOWN),
                 "dirtyness out of sync");
    // XXX Is this needed?
    if (mMinWidth == NS_INTRINSIC_WIDTH_UNKNOWN)
        ComputeIntrinsicWidths(aReflowState.rendContext);

    nsTableCellMap *cellMap = mTableFrame->GetCellMap();
    PRInt32 colCount = cellMap->GetColCount();
    if (colCount <= 0)
        return; // nothing to do

    DistributeWidthToColumns(width, 0, colCount, BTLS_FINAL_WIDTH, false);

#ifdef DEBUG_TABLE_STRATEGY
    printf("ComputeColumnWidths final\n");
    mTableFrame->Dump(false, true, false);
#endif
}

void
BasicTableLayoutStrategy::DistributePctWidthToColumns(float aSpanPrefPct,
                                                      PRInt32 aFirstCol,
                                                      PRInt32 aColCount)
{
    // First loop to determine:
    PRInt32 nonPctColCount = 0; // number of spanned columns without % width
    nscoord nonPctTotalPrefWidth = 0; // total pref width of those columns
    // and to reduce aSpanPrefPct by columns that already have % width

    PRInt32 scol, scol_end;
    for (scol = aFirstCol, scol_end = aFirstCol + aColCount;
         scol < scol_end; ++scol) {
        nsTableColFrame *scolFrame = mTableFrame->GetColFrame(scol);
        if (!scolFrame) {
            NS_ERROR("column frames out of sync with cell map");
            continue;
        }
        float scolPct = scolFrame->GetPrefPercent();
        if (scolPct == 0.0f) {
            nonPctTotalPrefWidth += scolFrame->GetPrefCoord();
            ++nonPctColCount;
        } else {
            aSpanPrefPct -= scolPct;
        }
    }

    if (aSpanPrefPct <= 0.0f || nonPctColCount == 0) {
        // There's no %-width on the colspan left over to distribute,
        // or there are no columns to which we could distribute %-width
        return;
    }

    // Second loop, to distribute what remains of aSpanPrefPct
    // between the non-percent-width spanned columns
    const bool spanHasNonPctPref = nonPctTotalPrefWidth > 0; // Loop invariant
    for (scol = aFirstCol, scol_end = aFirstCol + aColCount;
         scol < scol_end; ++scol) {
        nsTableColFrame *scolFrame = mTableFrame->GetColFrame(scol);
        if (!scolFrame) {
            NS_ERROR("column frames out of sync with cell map");
            continue;
        }

        if (scolFrame->GetPrefPercent() == 0.0f) {
            NS_ASSERTION((!spanHasNonPctPref ||
                          nonPctTotalPrefWidth != 0) &&
                         nonPctColCount != 0,
                         "should not be zero if we haven't allocated "
                         "all pref percent");

            float allocatedPct; // % width to be given to this column
            if (spanHasNonPctPref) {
                // Group so we're multiplying by 1.0f when we need
                // to use up aSpanPrefPct.
                allocatedPct = aSpanPrefPct *
                    (float(scolFrame->GetPrefCoord()) /
                     float(nonPctTotalPrefWidth));
            } else {
                // distribute equally when all pref widths are 0
                allocatedPct = aSpanPrefPct / float(nonPctColCount);
            }
            // Allocate the percent
            scolFrame->AddSpanPrefPercent(allocatedPct);
            
            // To avoid accumulating rounding error from division,
            // subtract this column's values from the totals.
            aSpanPrefPct -= allocatedPct;
            nonPctTotalPrefWidth -= scolFrame->GetPrefCoord();
            --nonPctColCount;

            if (!aSpanPrefPct) {
                // No more span-percent-width to distribute --> we're done.
                NS_ASSERTION(spanHasNonPctPref ? 
                             nonPctTotalPrefWidth == 0 :
                             nonPctColCount == 0,
                             "No more pct width to distribute, but there are "
                             "still cols that need some.");
                return;
            }
        }
    }
}

void
BasicTableLayoutStrategy::DistributeWidthToColumns(nscoord aWidth, 
                                                   PRInt32 aFirstCol, 
                                                   PRInt32 aColCount,
                                                   BtlsWidthType aWidthType,
                                                   bool aSpanHasSpecifiedWidth)
{
    NS_ASSERTION(aWidthType != BTLS_FINAL_WIDTH || 
                 (aFirstCol == 0 && 
                  aColCount == mTableFrame->GetCellMap()->GetColCount()),
            "Computing final column widths, but didn't get full column range");

    // border-spacing isn't part of the basis for percentages.
    nscoord spacing = mTableFrame->GetCellSpacingX();
    nscoord subtract = 0;    
    // aWidth initially includes border-spacing for the boundaries in between
    // each of the columns. We start at aFirstCol + 1 because the first
    // in-between boundary would be at the left edge of column aFirstCol + 1
    for (PRInt32 col = aFirstCol + 1; col < aFirstCol + aColCount; ++col) {
        if (mTableFrame->ColumnHasCellSpacingBefore(col)) {
            subtract += spacing;
        }
    }
    if (aWidthType == BTLS_FINAL_WIDTH) {
        // If we're computing final col-width, then aWidth initially includes
        // border spacing on the table's far left + far right edge, too.  Need
        // to subtract those out, too.
        subtract += spacing * 2;
    }
    aWidth = NSCoordSaturatingSubtract(aWidth, subtract, nscoord_MAX);

    /*
     * The goal of this function is to distribute |aWidth| between the
     * columns by making an appropriate AddSpanCoords or SetFinalWidth
     * call for each column.  (We call AddSpanCoords if we're 
     * distributing a column-spanning cell's minimum or preferred width
     * to its spanned columns.  We call SetFinalWidth if we're 
     * distributing a table's final width to its columns.)
     *
     * The idea is to either assign one of the following sets of widths
     * or a weighted average of two adjacent sets of widths.  It is not
     * possible to assign values smaller than the smallest set of
     * widths.  However, see below for handling the case of assigning
     * values larger than the largest set of widths.  From smallest to
     * largest, these are:
     *
     * 1. [guess_min] Assign all columns their min width.
     *
     * 2. [guess_min_pct] Assign all columns with percentage widths
     * their percentage width, and all other columns their min width.
     *
     * 3. [guess_min_spec] Assign all columns with percentage widths
     * their percentage width, all columns with specified coordinate
     * widths their pref width (since it doesn't matter whether it's the
     * largest contributor to the pref width that was the specified
     * contributor), and all other columns their min width.
     *
     * 4. [guess_pref] Assign all columns with percentage widths their
     * specified width, and all other columns their pref width.
     *
     * If |aWidth| is *larger* than what we would assign in (4), then we
     * expand the columns:
     *
     *   a. if any columns without a specified coordinate width or
     *   percent width have nonzero pref width, in proportion to pref
     *   width [total_flex_pref]
     *
     *   b. (NOTE: this case is for BTLS_FINAL_WIDTH only) otherwise, if
     *   any columns without a specified coordinate width or percent
     *   width, but with cells originating in them have zero pref width,
     *   equally between these [numNonSpecZeroWidthCols]
     *
     *   c. otherwise, if any columns without percent width have nonzero
     *   pref width, in proportion to pref width [total_fixed_pref]
     *
     *   d. otherwise, if any columns have nonzero percentage widths, in
     *   proportion to the percentage widths [total_pct]
     *
     *   e. otherwise, equally.
     */

    // Loop #1 over the columns, to figure out the four values above so
    // we know which case we're dealing with.

    nscoord guess_min = 0,
            guess_min_pct = 0,
            guess_min_spec = 0,
            guess_pref = 0,
            total_flex_pref = 0,
            total_fixed_pref = 0;
    float total_pct = 0.0f; // 0.0f to 1.0f
    PRInt32 numInfiniteWidthCols = 0;
    PRInt32 numNonSpecZeroWidthCols = 0;

    PRInt32 col;
    nsTableCellMap *cellMap = mTableFrame->GetCellMap();
    for (col = aFirstCol; col < aFirstCol + aColCount; ++col) {
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
        if (!colFrame) {
            NS_ERROR("column frames out of sync with cell map");
            continue;
        }
        nscoord min_width = colFrame->GetMinCoord();
        guess_min += min_width;
        if (colFrame->GetPrefPercent() != 0.0f) {
            float pct = colFrame->GetPrefPercent();
            total_pct += pct;
            nscoord val = nscoord(float(aWidth) * pct);
            if (val < min_width)
                val = min_width;
            guess_min_pct += val;
            guess_pref = NSCoordSaturatingAdd(guess_pref, val);
        } else {
            nscoord pref_width = colFrame->GetPrefCoord();
            if (pref_width == nscoord_MAX) {
                ++numInfiniteWidthCols;
            }
            guess_pref = NSCoordSaturatingAdd(guess_pref, pref_width);
            guess_min_pct += min_width;
            if (colFrame->GetHasSpecifiedCoord()) {
                // we'll add on the rest of guess_min_spec outside the
                // loop
                nscoord delta = NSCoordSaturatingSubtract(pref_width, 
                                                          min_width, 0);
                guess_min_spec = NSCoordSaturatingAdd(guess_min_spec, delta);
                total_fixed_pref = NSCoordSaturatingAdd(total_fixed_pref, 
                                                        pref_width);
            } else if (pref_width == 0) {
                if (aWidthType == BTLS_FINAL_WIDTH &&
                    cellMap->GetNumCellsOriginatingInCol(col) > 0) {
                    ++numNonSpecZeroWidthCols;
                }
            } else {
                total_flex_pref = NSCoordSaturatingAdd(total_flex_pref,
                                                       pref_width);
            }
        }
    }
    guess_min_spec = NSCoordSaturatingAdd(guess_min_spec, guess_min_pct);

    // Determine what we're flexing:
    enum Loop2Type {
        FLEX_PCT_SMALL, // between (1) and (2) above
        FLEX_FIXED_SMALL, // between (2) and (3) above
        FLEX_FLEX_SMALL, // between (3) and (4) above
        FLEX_FLEX_LARGE, // greater than (4) above, case (a)
        FLEX_FLEX_LARGE_ZERO, // greater than (4) above, case (b)
        FLEX_FIXED_LARGE, // greater than (4) above, case (c)
        FLEX_PCT_LARGE, // greater than (4) above, case (d)
        FLEX_ALL_LARGE // greater than (4) above, case (e)
    };

    Loop2Type l2t;
    // These are constants (over columns) for each case's math.  We use
    // a pair of nscoords rather than a float so that we can subtract
    // each column's allocation so we avoid accumulating rounding error.
    nscoord space; // the amount of extra width to allocate
    union {
        nscoord c;
        float f;
    } basis; // the sum of the statistic over columns to divide it
    if (aWidth < guess_pref) {
        if (aWidthType != BTLS_FINAL_WIDTH && aWidth <= guess_min) {
            // Return early -- we don't have any extra space to distribute.
            return;
        }
        NS_ASSERTION(!(aWidthType == BTLS_FINAL_WIDTH && aWidth < guess_min),
                     "Table width is less than the "
                     "sum of its columns' min widths");
        if (aWidth < guess_min_pct) {
            l2t = FLEX_PCT_SMALL;
            space = aWidth - guess_min;
            basis.c = guess_min_pct - guess_min;
        } else if (aWidth < guess_min_spec) {
            l2t = FLEX_FIXED_SMALL;
            space = aWidth - guess_min_pct;
            basis.c = NSCoordSaturatingSubtract(guess_min_spec, guess_min_pct,
                                                nscoord_MAX);
        } else {
            l2t = FLEX_FLEX_SMALL;
            space = aWidth - guess_min_spec;
            basis.c = NSCoordSaturatingSubtract(guess_pref, guess_min_spec,
                                                nscoord_MAX);
        }
    } else {
        space = NSCoordSaturatingSubtract(aWidth, guess_pref, nscoord_MAX);
        if (total_flex_pref > 0) {
            l2t = FLEX_FLEX_LARGE;
            basis.c = total_flex_pref;
        } else if (numNonSpecZeroWidthCols > 0) {
            NS_ASSERTION(aWidthType == BTLS_FINAL_WIDTH,
                         "numNonSpecZeroWidthCols should only "
                         "be set when we're setting final width.");
            l2t = FLEX_FLEX_LARGE_ZERO;
            basis.c = numNonSpecZeroWidthCols;
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
    printf("ComputeColumnWidths: %d columns in width %d,\n"
           "  guesses=[%d,%d,%d,%d], totals=[%d,%d,%f],\n"
           "  l2t=%d, space=%d, basis.c=%d\n",
           aColCount, aWidth,
           guess_min, guess_min_pct, guess_min_spec, guess_pref,
           total_flex_pref, total_fixed_pref, total_pct,
           l2t, space, basis.c);
#endif

    for (col = aFirstCol; col < aFirstCol + aColCount; ++col) {
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
        if (!colFrame) {
            NS_ERROR("column frames out of sync with cell map");
            continue;
        }
        nscoord col_width;

        float pct = colFrame->GetPrefPercent();
        if (pct != 0.0f) {
            col_width = nscoord(float(aWidth) * pct);
            nscoord col_min = colFrame->GetMinCoord();
            if (col_width < col_min)
                col_width = col_min;
        } else {
            col_width = colFrame->GetPrefCoord();
        }

        nscoord col_width_before_adjust = col_width;

        switch (l2t) {
            case FLEX_PCT_SMALL:
                col_width = col_width_before_adjust = colFrame->GetMinCoord();
                if (pct != 0.0f) {
                    nscoord pct_minus_min =
                        nscoord(float(aWidth) * pct) - col_width;
                    if (pct_minus_min > 0) {
                        float c = float(space) / float(basis.c);
                        basis.c -= pct_minus_min;
                        col_width += NSToCoordRound(float(pct_minus_min) * c);
                    }
                }
                break;
            case FLEX_FIXED_SMALL:
                if (pct == 0.0f) {
                    NS_ASSERTION(col_width == colFrame->GetPrefCoord(),
                                 "wrong width assigned");
                    if (colFrame->GetHasSpecifiedCoord()) {
                        nscoord col_min = colFrame->GetMinCoord();
                        nscoord pref_minus_min = col_width - col_min;
                        col_width = col_width_before_adjust = col_min;
                        if (pref_minus_min != 0) {
                            float c = float(space) / float(basis.c);
                            basis.c -= pref_minus_min;
                            col_width += NSToCoordRound(
                                float(pref_minus_min) * c);
                        }
                    } else
                        col_width = col_width_before_adjust =
                            colFrame->GetMinCoord();
                }
                break;
            case FLEX_FLEX_SMALL:
                if (pct == 0.0f &&
                    !colFrame->GetHasSpecifiedCoord()) {
                    NS_ASSERTION(col_width == colFrame->GetPrefCoord(),
                                 "wrong width assigned");
                    nscoord col_min = colFrame->GetMinCoord();
                    nscoord pref_minus_min = 
                        NSCoordSaturatingSubtract(col_width, col_min, 0);
                    col_width = col_width_before_adjust = col_min;
                    if (pref_minus_min != 0) {
                        float c = float(space) / float(basis.c);
                        // If we have infinite-width cols, then the standard
                        // adjustment to col_width using 'c' won't work,
                        // because basis.c and pref_minus_min are both
                        // nscoord_MAX and will cancel each other out in the
                        // col_width adjustment (making us assign all the
                        // space to the first inf-width col).  To correct for
                        // this, we'll also divide by numInfiniteWidthCols to
                        // spread the space equally among the inf-width cols.
                        if (numInfiniteWidthCols) {
                            if (colFrame->GetPrefCoord() == nscoord_MAX) {
                                c = c / float(numInfiniteWidthCols);
                                --numInfiniteWidthCols;
                            } else {
                                c = 0.0f;
                            }
                        }
                        basis.c = NSCoordSaturatingSubtract(basis.c, 
                                                            pref_minus_min,
                                                            nscoord_MAX);
                        col_width += NSToCoordRound(
                            float(pref_minus_min) * c);
                    }
                }
                break;
            case FLEX_FLEX_LARGE:
                if (pct == 0.0f &&
                    !colFrame->GetHasSpecifiedCoord()) {
                    NS_ASSERTION(col_width == colFrame->GetPrefCoord(),
                                 "wrong width assigned");
                    if (col_width != 0) {
                        if (space == nscoord_MAX) {
                            basis.c -= col_width;
                            col_width = nscoord_MAX;
                        } else {
                            float c = float(space) / float(basis.c);
                            basis.c -= col_width;
                            col_width += NSToCoordRound(float(col_width) * c);
                        }
                    }
                }
                break;
            case FLEX_FLEX_LARGE_ZERO:
                NS_ASSERTION(aWidthType == BTLS_FINAL_WIDTH,
                             "FLEX_FLEX_LARGE_ZERO only should be hit "
                             "when we're setting final width.");
                if (pct == 0.0f &&
                    !colFrame->GetHasSpecifiedCoord() &&
                    cellMap->GetNumCellsOriginatingInCol(col) > 0) {

                    NS_ASSERTION(col_width == 0 &&
                                 colFrame->GetPrefCoord() == 0,
                                 "Since we're in FLEX_FLEX_LARGE_ZERO case, "
                                 "all auto-width cols should have zero pref "
                                 "width.");
                    float c = float(space) / float(basis.c);
                    col_width += NSToCoordRound(c);
                    --basis.c;
                }
                break;
            case FLEX_FIXED_LARGE:
                if (pct == 0.0f) {
                    NS_ASSERTION(col_width == colFrame->GetPrefCoord(),
                                 "wrong width assigned");
                    NS_ASSERTION(colFrame->GetHasSpecifiedCoord() ||
                                 colFrame->GetPrefCoord() == 0,
                                 "wrong case");
                    if (col_width != 0) {
                        float c = float(space) / float(basis.c);
                        basis.c -= col_width;
                        col_width += NSToCoordRound(float(col_width) * c);
                    }
                }
                break;
            case FLEX_PCT_LARGE:
                NS_ASSERTION(pct != 0.0f || colFrame->GetPrefCoord() == 0,
                             "wrong case");
                if (pct != 0.0f) {
                    float c = float(space) / basis.f;
                    col_width += NSToCoordRound(pct * c);
                    basis.f -= pct;
                }
                break;
            case FLEX_ALL_LARGE:
                {
                    float c = float(space) / float(basis.c);
                    col_width += NSToCoordRound(c);
                    --basis.c;
                }
                break;
        }

        // Only subtract from space if it's a real number.
        if (space != nscoord_MAX) {
            NS_ASSERTION(col_width != nscoord_MAX,
                 "How is col_width nscoord_MAX if space isn't?");
            NS_ASSERTION(col_width_before_adjust != nscoord_MAX,
                 "How is col_width_before_adjust nscoord_MAX if space isn't?");
            space -= col_width - col_width_before_adjust;
        }

        NS_ASSERTION(col_width >= colFrame->GetMinCoord(),
                     "assigned width smaller than min");
        
        // Apply the new width
        switch (aWidthType) {
            case BTLS_MIN_WIDTH:
                {
                    // Note: AddSpanCoords requires both a min and pref width.
                    // For the pref width, we'll just pass in our computed
                    // min width, because the real pref width will be at least
                    // as big
                    colFrame->AddSpanCoords(col_width, col_width, 
                                            aSpanHasSpecifiedWidth);
                }
                break;
            case BTLS_PREF_WIDTH:
                {
                    // Note: AddSpanCoords requires both a min and pref width.
                    // For the min width, we'll just pass in 0, because
                    // the real min width will be at least 0
                    colFrame->AddSpanCoords(0, col_width, 
                                            aSpanHasSpecifiedWidth);
                }
                break;
            case BTLS_FINAL_WIDTH:
                {
                    nscoord old_final = colFrame->GetFinalWidth();
                    colFrame->SetFinalWidth(col_width);
                    
                    if (old_final != col_width)
                        mTableFrame->DidResizeColumns();
                }
                break;                
        }
    }
    NS_ASSERTION((space == 0 || space == nscoord_MAX) &&
                 ((l2t == FLEX_PCT_LARGE)
                    ? (-0.001f < basis.f && basis.f < 0.001f)
                    : (basis.c == 0 || basis.c == nscoord_MAX)),
                 "didn't subtract all that we added");
}
