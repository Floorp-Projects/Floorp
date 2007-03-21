/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla's table layout code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#undef  DEBUG_TABLE_STRATEGY 

BasicTableLayoutStrategy::BasicTableLayoutStrategy(nsTableFrame *aTableFrame)
  : mTableFrame(aTableFrame)
{
    MarkIntrinsicWidthsDirty();
}

/* virtual */
BasicTableLayoutStrategy::~BasicTableLayoutStrategy()
{
}

/* virtual */ nscoord
BasicTableLayoutStrategy::GetMinWidth(nsIRenderingContext* aRenderingContext)
{
    DISPLAY_MIN_WIDTH(mTableFrame, mMinWidth);
    if (mMinWidth == NS_INTRINSIC_WIDTH_UNKNOWN)
        ComputeIntrinsicWidths(aRenderingContext);
    return mMinWidth;
}

/* virtual */ nscoord
BasicTableLayoutStrategy::GetPrefWidth(nsIRenderingContext* aRenderingContext,
                                       PRBool aComputingSize)
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
                  float aPrefPercent, PRBool aHasSpecifiedWidth)
        : hasSpecifiedWidth(aHasSpecifiedWidth)
        , minCoord(aMinCoord)
        , prefCoord(aPrefCoord)
        , prefPercent(aPrefPercent)
    {
    }

    PRBool hasSpecifiedWidth;
    nscoord minCoord;
    nscoord prefCoord;
    float prefPercent;
};

// Used for both column and cell calculations.  The parts needed only
// for cells are skipped when aCellFrame is null.
static CellWidthInfo
GetWidthInfo(nsIRenderingContext *aRenderingContext,
             nsTableCellFrame *aCellFrame,
             const nsStylePosition *aStylePos)
{
    nscoord minCoord, prefCoord;
    if (aCellFrame) {
        minCoord = aCellFrame->GetMinWidth(aRenderingContext);
        prefCoord = aCellFrame->GetPrefWidth(aRenderingContext);
    } else {
        minCoord = 0;
        prefCoord = 0;
    }
    float prefPercent = 0.0f;
    PRBool hasSpecifiedWidth = PR_FALSE;

    switch (aStylePos->mWidth.GetUnit()) {
        case eStyleUnit_Coord: {
                hasSpecifiedWidth = PR_TRUE;
                nscoord w = aStylePos->mWidth.GetCoordValue();
                // Quirk: A cell with "nowrap" set and a coord value for the
                // width which is bigger than the intrinsic minimum width uses
                // that coord value as the minimum width.
                if (aCellFrame && w > minCoord &&
                    aCellFrame->GetPresContext()->CompatibilityMode() ==
                      eCompatibility_NavQuirks &&
                    aCellFrame->GetContent()->HasAttr(kNameSpaceID_None,
                                                      nsGkAtoms::nowrap)) {
                    minCoord = w;
                }
                prefCoord = PR_MAX(w, minCoord);
            }
            break;
        case eStyleUnit_Percent: {
                prefPercent = aStylePos->mWidth.GetPercentValue();
            }
            break;
        default:
            break;
    }

    switch (aStylePos->mMaxWidth.GetUnit()) {
        // XXX To really implement 'max-width' well, we'd need to store
        // it separately on the columns.
        case eStyleUnit_Coord: {
                nscoord w = aStylePos->mMaxWidth.GetCoordValue();
                if (w < minCoord)
                    minCoord = w;
                if (w < prefCoord)
                    prefCoord = w;
            }
            break;
        case eStyleUnit_Percent: {
                float p = aStylePos->mMaxWidth.GetPercentValue();
                if (p < prefPercent)
                    prefPercent = p;
            }
            break;
        default:
            break;
    }

    switch (aStylePos->mMinWidth.GetUnit()) {
        case eStyleUnit_Coord: {
                nscoord w = aStylePos->mMinWidth.GetCoordValue();
                if (w > minCoord)
                    minCoord = w;
                if (w > prefCoord)
                    prefCoord = w;
            }
            break;
        case eStyleUnit_Percent: {
                float p = aStylePos->mMinWidth.GetPercentValue();
                if (p > prefPercent)
                    prefPercent = p;
            }
            break;
        default:
            break;
    }

    // XXX Should col frame have border/padding considered?
    if (aCellFrame) {
        nsIFrame::IntrinsicWidthOffsetData offsets =
            aCellFrame->IntrinsicWidthOffsets(aRenderingContext);
        // XXX Should we ignore percentage padding?
        nscoord add = offsets.hPadding + offsets.hBorder;
        minCoord += add;
        prefCoord += add;
    }

    return CellWidthInfo(minCoord, prefCoord, prefPercent, hasSpecifiedWidth);
}

static inline CellWidthInfo
GetCellWidthInfo(nsIRenderingContext *aRenderingContext,
                 nsTableCellFrame *aCellFrame)
{
    return GetWidthInfo(aRenderingContext, aCellFrame,
                        aCellFrame->GetStylePosition());
}

static inline CellWidthInfo
GetColWidthInfo(nsIRenderingContext *aRenderingContext,
                nsIFrame *aFrame)
{
    return GetWidthInfo(aRenderingContext, nsnull, aFrame->GetStylePosition());
}


/**
 * The algorithm in this function, in addition to meeting the
 * requirements of Web-compatibility, is also invariant under reordering
 * of the rows within a table (something that most, but not all, other
 * browsers are).
 */
void
BasicTableLayoutStrategy::ComputeColumnIntrinsicWidths(nsIRenderingContext* aRenderingContext)
{
    nsTableFrame *tableFrame = mTableFrame;
    nsTableCellMap *cellMap = tableFrame->GetCellMap();

    nscoord spacing = tableFrame->GetCellSpacingX();
    SpanningCellSorter spanningCells(tableFrame->GetPresContext()->PresShell());

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
        // XXX Should we be doing this when we have widths on the column?
        NS_ASSERTION(colFrame->GetParent()->GetType() ==
                         nsGkAtoms::tableColGroupFrame,
                     "expected a column-group");
        colInfo = GetColWidthInfo(aRenderingContext, colFrame->GetParent());
        colFrame->AddCoords(colInfo.minCoord, colInfo.prefCoord,
                            colInfo.hasSpecifiedWidth);
        colFrame->AddPrefPercent(colInfo.prefPercent);

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
    mTableFrame->Dump(PR_FALSE, PR_TRUE, PR_FALSE);
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

            // Before looping over the spanned columns to distribute
            // this cell's width over the columns it spans, we first
            // compute totals over the spanned columns so we know how to
            // allocate the space.

            // Accumulate information about the spanned columns, and
            // subtract the already-used space from |info|.
            nscoord totalSPref = 0, totalSMin = 0; // total existing widths
            nscoord totalSNonPctPref = 0; // total pref width of columns
                                          // without percentage widths
            PRInt32 nonPctCount = 0; // # of columns without percentage widths
            PRInt32 scol, scol_end;
            for (scol = col, scol_end = col + colSpan;
                 scol < scol_end; ++scol) {
                nsTableColFrame *scolFrame = tableFrame->GetColFrame(scol);
                if (!scolFrame) {
                    NS_ERROR("column frames out of sync with cell map");
                    continue;
                }

                if (mTableFrame->GetNumCellsOriginatingInCol(scol) &&
                    scol != col) {
                    info.minCoord -= spacing;
                    info.prefCoord -= spacing;
                }

                nscoord curPref;
                if (info.hasSpecifiedWidth &&
                    !scolFrame->GetHasSpecifiedCoord()) {
                    curPref = scolFrame->GetMinCoord();
                } else {
                    curPref = scolFrame->GetPrefCoord();
                }

                totalSPref += curPref;
                totalSMin += scolFrame->GetMinCoord();
                float scolPct = scolFrame->GetPrefPercent();
                if (scolPct == 0.0f) {
                    totalSNonPctPref += curPref;
                    ++nonPctCount;
                } else {
                    info.prefPercent -= scolPct;
                }
                info.minCoord -= scolFrame->GetMinCoord();
                info.prefCoord -= curPref;
            }

            if (info.minCoord < 0)
                info.minCoord = 0;
            if (info.prefCoord < 0)
                info.prefCoord = 0;
            if (info.prefPercent < 0.0f)
                info.prefPercent = 0.0f;

            // The min-width of this cell that fits inside the
            // pref-width of the spanned columns gets distributed
            // according to different ratios.
            nscoord minWithinPref =
                PR_MIN(info.minCoord, totalSPref - totalSMin);
            NS_ASSERTION(minWithinPref >= 0, "neither value can be negative");
            nscoord minOutsidePref = info.minCoord - minWithinPref;

            // Loop invariants (that we might get confused about as we
            // subtract amounts for completed columns)
            const PRBool spanHasNonPctPref = totalSNonPctPref > 0;
            const PRBool spanHasPref = totalSPref > 0;
            const PRBool spanHasNonPct = nonPctCount > 0;

            // ... and actually do the distribution of the widths of
            // this cell exceeding the totals already in the spanned
            // columns.
            for (scol = col, scol_end = col + colSpan;
                 scol < scol_end; ++scol) {
                nsTableColFrame *scolFrame = tableFrame->GetColFrame(scol);
                if (!scolFrame) {
                    NS_ERROR("column frames out of sync with cell map");
                    continue;
                }

                nscoord curPref;
                if (info.hasSpecifiedWidth &&
                    !scolFrame->GetHasSpecifiedCoord()) {
                    curPref = scolFrame->GetMinCoord();
                } else {
                    curPref = scolFrame->GetPrefCoord();
                }

                // the percentage width (only to columns that don't
                // already have percentage widths, in proportion to
                // the existing pref widths)
                float allocatedPct = 0.0f;
                if (scolFrame->GetPrefPercent() == 0.0f &&
                    info.prefPercent != 0.0f) {
                    NS_ASSERTION((!spanHasNonPctPref ||
                                  totalSNonPctPref != 0) &&
                                 nonPctCount != 0,
                                 "should not be zero if we haven't allocated "
                                 "all pref percent");
                    if (spanHasNonPctPref) {
                        // Group so we're multiplying by 1.0f when we need
                        // to use up info.prefPercent.
                        allocatedPct = info.prefPercent *
                                           (float(curPref) /
                                            float(totalSNonPctPref));
                    } else {
                        // distribute equally when all pref widths are 0
                        allocatedPct = info.prefPercent / float(nonPctCount);
                    }
                    scolFrame->AddSpanPrefPercent(allocatedPct);
                }

                // the part of the min width that fits within the
                // existing pref width
                float minRatio = 0.0f;
                if (minWithinPref > 0) {
                    minRatio = float(curPref - scolFrame->GetMinCoord()) /
                               float(totalSPref - totalSMin);
                }

                // the rest of the min width, and the pref width (in
                // proportion to the existing pref widths)
                float coordRatio; // for both min and pref
                if (spanHasPref) {
                    if (curPref == 0) {
                        // We might have already subtracted all of
                        // totalSPref.
                        coordRatio = 0.0f;
                    } else {
                        coordRatio = float(curPref) / float(totalSPref);
                    }
                } else {
                    // distribute equally when all pref widths are 0
                    coordRatio = 1.0f / float(scol_end - scol);
                }

                // combine the two min-width distributions, and record
                // min and pref
                nscoord allocatedMinWithinPref =
                    NSToCoordRound(float(minWithinPref) * minRatio);
                nscoord allocatedMinOutsidePref =
                    NSToCoordRound(float(minOutsidePref) * coordRatio);
                nscoord allocatedPref =
                    NSToCoordRound(float(info.prefCoord) * coordRatio);
                nscoord spanMin = scolFrame->GetMinCoord() +
                        allocatedMinWithinPref + allocatedMinOutsidePref;
                nscoord spanPref = curPref + allocatedPref;
                scolFrame->AddSpanCoords(spanMin, spanPref,
                                         info.hasSpecifiedWidth);

                // To avoid accumulating rounding error from division,
                // subtract everything to do with the column we've
                // passed from the totals.
                minWithinPref -= allocatedMinWithinPref;
                minOutsidePref -= allocatedMinOutsidePref;
                info.prefCoord -= allocatedPref;
                info.prefPercent -= allocatedPct;
                totalSPref -= curPref;
                totalSMin -= scolFrame->GetMinCoord();
                if (scolFrame->GetPrefPercent() == 0.0f) {
                    totalSNonPctPref -= curPref;
                    --nonPctCount;
                }
            }

            // Note that we only distribute the percentage if
            // spanHasNonPct.
            NS_ASSERTION(totalSPref == 0 && totalSMin == 0 &&
                         totalSNonPctPref == 0 && nonPctCount == 0 &&
                         minOutsidePref == 0 && minWithinPref == 0 &&
                         info.prefCoord == 0 &&
                         (info.prefPercent == 0.0f || !spanHasNonPct),
                         "didn't subtract all that we added");
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
    mTableFrame->Dump(PR_FALSE, PR_TRUE, PR_FALSE);
#endif
}

void
BasicTableLayoutStrategy::ComputeIntrinsicWidths(nsIRenderingContext* aRenderingContext)
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
        if (mTableFrame->GetNumCellsOriginatingInCol(col)) {
            add += spacing;
        }
        min += colFrame->GetMinCoord();
        pref += colFrame->GetPrefCoord();

        // Percentages are of the table, so we have to reverse them for
        // intrinsic widths.
        float p = colFrame->GetPrefPercent();
        if (p > 0.0f) {
            nscoord new_small_pct_expand =
                nscoord(float(colFrame->GetPrefCoord()) / p);
            if (new_small_pct_expand > max_small_pct_pref) {
                max_small_pct_pref = new_small_pct_expand;
            }
            pct_total += p;
        } else {
            nonpct_pref_total += colFrame->GetPrefCoord();
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
        nscoord large_pct_pref = nscoord(float(nonpct_pref_total) /
                                         (1.0f - pct_total));
        if (large_pct_pref > pref_pct_expand)
            pref_pct_expand = large_pct_pref;
    }

    // border-spacing isn't part of the basis for percentages
    if (colCount > 0) {
        min += add;
        pref += add;
        pref_pct_expand += add;
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

    nscoord spacing = mTableFrame->GetCellSpacingX();

    nscoord min = mMinWidth;

    // border-spacing isn't part of the basis for percentages.
    nscoord subtract = spacing;
    for (PRInt32 col = 0; col < colCount; ++col) {
        if (mTableFrame->GetNumCellsOriginatingInCol(col)) {
            subtract += spacing;
        }
    }
    width -= subtract;
    min -= subtract;

    // XXX is |width| the right basis for percentage widths?

    /*
     * The goal of this function is to allocate |width| to the columns
     * by making an appropriate SetFinalWidth call to each column.
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
     * If |width| is *larger* than what we would assign in (4), then we
     * expand the columns:
     *
     *   a. if any columns without a specified coordinate width or
     *   percent width have nonzero pref width, in proportion to pref
     *   width [total_flex_pref]
     *
     *   b. otherwise, if any columns without percent width have nonzero
     *   pref width, in proportion to pref width [total_fixed_pref]
     *
     *   c. otherwise, if any columns have nonzero percentage widths, in
     *   proportion to the percentage widths [total_pct]
     *
     *   d. otherwise, equally.
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

    PRInt32 col;
    for (col = 0; col < colCount; ++col) {
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
            nscoord val = nscoord(float(width) * pct);
            if (val < min_width)
                val = min_width;
            guess_min_pct += val;
            guess_pref += val;
        } else {
            nscoord pref_width = colFrame->GetPrefCoord();
            guess_pref += pref_width;
            guess_min_pct += min_width;
            if (colFrame->GetHasSpecifiedCoord()) {
                // we'll add on the rest of guess_min_spec outside the
                // loop
                guess_min_spec += pref_width - min_width;
                total_fixed_pref += pref_width;
            } else {
                total_flex_pref += pref_width;
            }
        }
    }
    guess_min_spec += guess_min_pct;

    // Determine what we're flexing:
    enum Loop2Type {
        FLEX_PCT_SMALL, // between (1) and (2) above
        FLEX_FIXED_SMALL, // between (2) and (3) above
        FLEX_FLEX_SMALL, // between (3) and (4) above
        FLEX_FLEX_LARGE, // above (4) above, case (a)
        FLEX_FIXED_LARGE, // above (4) above, case (b)
        FLEX_PCT_LARGE, // above (4) above, case (c)
        FLEX_ALL_LARGE // above (4) above, case (d)
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
    if (width < guess_pref) {
        NS_ASSERTION(width >= guess_min, "bad width");
        if (width < guess_min_pct) {
            l2t = FLEX_PCT_SMALL;
            space = width - guess_min;
            basis.c = guess_min_pct - guess_min;
        } else if (width < guess_min_spec) {
            l2t = FLEX_FIXED_SMALL;
            space = width - guess_min_pct;
            basis.c = guess_min_spec - guess_min_pct;
        } else {
            l2t = FLEX_FLEX_SMALL;
            space = width - guess_min_spec;
            basis.c = guess_pref - guess_min_spec;
        }
    } else {
        space = width - guess_pref;
        if (total_flex_pref > 0) {
            l2t = FLEX_FLEX_LARGE;
            basis.c = total_flex_pref;
        } else if (total_fixed_pref > 0) {
            l2t = FLEX_FIXED_LARGE;
            basis.c = total_fixed_pref;
        } else if (total_pct > 0.0f) {
            l2t = FLEX_PCT_LARGE;
            basis.f = total_pct;
        } else {
            l2t = FLEX_ALL_LARGE;
            basis.c = colCount;
        }
    }

#ifdef DEBUG_dbaron_off
    printf("ComputeColumnWidths: %d columns in width %d,\n"
           "  guesses=[%d,%d,%d,%d], totals=[%d,%d,%f],\n"
           "  l2t=%d, space=%d, basis.c=%d\n",
           colCount, width,
           guess_min, guess_min_pct, guess_min_spec, guess_pref,
           total_flex_pref, total_fixed_pref, total_pct,
           l2t, space, basis.c);
#endif

    for (col = 0; col < colCount; ++col) {
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
        if (!colFrame) {
            NS_ERROR("column frames out of sync with cell map");
            continue;
        }
        nscoord col_width;

        float pct = colFrame->GetPrefPercent();
        if (pct != 0.0f) {
            col_width = nscoord(float(width) * pct);
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
                        nscoord(float(width) * pct) - col_width;
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
                    nscoord pref_minus_min = col_width - col_min;
                    col_width = col_width_before_adjust = col_min;
                    if (pref_minus_min != 0) {
                        float c = float(space) / float(basis.c);
                        basis.c -= pref_minus_min;
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
                        float c = float(space) / float(basis.c);
                        basis.c -= col_width;
                        col_width += NSToCoordRound(float(col_width) * c);
                    }
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

        space -= col_width - col_width_before_adjust;

        NS_ASSERTION(col_width >= colFrame->GetMinCoord(),
                     "assigned width smaller than min");

        nscoord old_final = colFrame->GetFinalWidth();
        colFrame->SetFinalWidth(col_width);

        if (old_final != col_width)
            mTableFrame->DidResizeColumns();
    }
    NS_ASSERTION(space == 0 &&
                 ((l2t == FLEX_PCT_LARGE)
                    ? (-0.001f < basis.f && basis.f < 0.001f)
                    : (basis.c == 0)),
                 "didn't subtract all that we added");
#ifdef DEBUG_TABLE_STRATEGY
    printf("ComputeColumnWidths final\n");
    mTableFrame->Dump(PR_FALSE, PR_TRUE, PR_FALSE);
#endif
}
