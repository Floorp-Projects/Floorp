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
 * Algorithms that determine column and table widths used for CSS2's
 * 'table-layout: fixed'.
 */

#include "FixedTableLayoutStrategy.h"
#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"

FixedTableLayoutStrategy::FixedTableLayoutStrategy(nsTableFrame *aTableFrame)
  : mTableFrame(aTableFrame)
{
    MarkIntrinsicWidthsDirty();
}

/* virtual */
FixedTableLayoutStrategy::~FixedTableLayoutStrategy()
{
}

/* virtual */ nscoord
FixedTableLayoutStrategy::GetMinWidth(nsIRenderingContext* aRenderingContext)
{
    DISPLAY_MIN_WIDTH(mTableFrame, mMinWidth);
    if (mMinWidth != NS_INTRINSIC_WIDTH_UNKNOWN)
        return mMinWidth;

    // It's theoretically possible to do something much better here that
    // depends only on the columns and the first row, but it wouldn't be
    // compatible with other browsers, or with the use of GetMinWidth by
    // nsHTMLReflowState to determine the width of a fixed-layout table,
    // since CSS2.1 says:
    //   The width of the table is then the greater of the value of the
    //   'width' property for the table element and the sum of the
    //   column widths (plus cell spacing or borders).

    // XXX Should we really ignore 'min-width' and 'max-width'?

    nsTableCellMap *cellMap = mTableFrame->GetCellMap();
    PRInt32 colCount = cellMap->GetColCount();
    nscoord spacing = mTableFrame->GetCellSpacingX();

    // XXX Should this code do any pixel rounding?

    nscoord result = 0;

    // XXX Consider widths on columns or column groups?

    if (colCount > 0) {
        // XXX Should only add columns that have cells originating in them!
        result += spacing * (colCount + 1);
    }

    for (PRInt32 col = 0; col < colCount; ++col) {
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
        if (!colFrame) {
            NS_ERROR("column frames out of sync with cell map");
            continue;
        }
        const nsStyleCoord *styleWidth =
            &colFrame->GetStylePosition()->mWidth;
        if (styleWidth->GetUnit() == eStyleUnit_Coord) {
            result += styleWidth->GetCoordValue();
        } else if (styleWidth->GetUnit() == eStyleUnit_Percent) {
            // do nothing
        } else {
            NS_ASSERTION(styleWidth->GetUnit() == eStyleUnit_Auto, "bad width");

            // The 'table-layout: fixed' algorithm considers only cells
            // in the first row.
            PRBool originates;
            PRInt32 colSpan;
            nsTableCellFrame *cellFrame =
                cellMap->GetCellInfoAt(0, col, &originates, &colSpan);
            if (cellFrame) {
                styleWidth = &cellFrame->GetStylePosition()->mWidth;
                if (styleWidth->GetUnit() == eStyleUnit_Coord) {
                    nscoord cellWidth = nsLayoutUtils::IntrinsicForContainer(
                        aRenderingContext, cellFrame, nsLayoutUtils::MIN_WIDTH);
                    if (colSpan > 1) {
                        // If a column-spanning cell is in the first
                        // row, split up the space evenly.  (XXX This
                        // isn't quite right if some of the columns it's
                        // in have specified widths.  Should we care?)
                        cellWidth = ((cellWidth + spacing) / colSpan) - spacing;
                    }
                    result += cellWidth;
                } else if (styleWidth->GetUnit() == eStyleUnit_Percent) {
                    if (colSpan > 1) {
                        // XXX Can this force columns to negative
                        // widths?
                        result -= spacing * (colSpan - 1);
                    }
                }
            }
        }
    }

    return (mMinWidth = result);
}

/* virtual */ nscoord
FixedTableLayoutStrategy::GetPrefWidth(nsIRenderingContext* aRenderingContext,
                                       PRBool aComputingSize)
{
    // It's theoretically possible to do something much better here that
    // depends only on the columns and the first row, but it wouldn't be
    // compatible with other browsers.
    nscoord result = nscoord_MAX;
    DISPLAY_PREF_WIDTH(mTableFrame, result);
    return result;
}

/* virtual */ void
FixedTableLayoutStrategy::MarkIntrinsicWidthsDirty()
{
    mMinWidth = NS_INTRINSIC_WIDTH_UNKNOWN;
    mLastCalcWidth = nscoord_MIN;
}

/* virtual */ void
FixedTableLayoutStrategy::ComputeColumnWidths(const nsHTMLReflowState& aReflowState)
{
    nscoord tableWidth = aReflowState.ComputedWidth();

    if (mLastCalcWidth == tableWidth)
        return;
    mLastCalcWidth = tableWidth;

    nsTableCellMap *cellMap = mTableFrame->GetCellMap();
    PRInt32 colCount = cellMap->GetColCount();
    nscoord spacing = mTableFrame->GetCellSpacingX();

    // XXX Should this code do any pixel rounding?

    // border-spacing isn't part of the basis for percentages.
    if (colCount > 0) {
        // XXX Should only add columns that have cells originating in them!
        nscoord subtract = spacing * (colCount + 1);
        tableWidth -= subtract;
    } else {
        // No Columns - nothing to compute
        return;
    }

    // XXX This ignores the 'min-width' and 'max-width' properties
    // throughout.  Then again, that's what the CSS spec says to do.

    // XXX Consider widths on columns or column groups?

    PRUint32 unassignedCount = 0;
    nscoord unassignedSpace = tableWidth;
    const nscoord unassignedMarker = nscoord_MIN;

    // We use the PrefPercent on the columns to store the percentages
    // used to compute column widths in case we need to reduce their
    // basis.
    float pctTotal = 0.0f;

    for (PRInt32 col = 0; col < colCount; ++col) {
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
        if (!colFrame) {
            NS_ERROR("column frames out of sync with cell map");
            continue;
        }
        colFrame->ResetPrefPercent();
        const nsStyleCoord *styleWidth =
            &colFrame->GetStylePosition()->mWidth;
        nscoord colWidth;
        if (styleWidth->GetUnit() == eStyleUnit_Coord) {
            colWidth = styleWidth->GetCoordValue();
        } else if (styleWidth->GetUnit() == eStyleUnit_Percent) {
            float pct = styleWidth->GetPercentValue();
            colWidth = NSToCoordFloor(pct * float(tableWidth));
            colFrame->AddPrefPercent(pct);
            pctTotal += pct;
        } else {
            NS_ASSERTION(styleWidth->GetUnit() == eStyleUnit_Auto, "bad width");

            // The 'table-layout: fixed' algorithm considers only cells
            // in the first row.
            PRBool originates;
            PRInt32 colSpan;
            nsTableCellFrame *cellFrame =
                cellMap->GetCellInfoAt(0, col, &originates, &colSpan);
            if (cellFrame) {
                styleWidth = &cellFrame->GetStylePosition()->mWidth;
                if (styleWidth->GetUnit() == eStyleUnit_Coord) {
                    colWidth = styleWidth->GetCoordValue();
                } else if (styleWidth->GetUnit() == eStyleUnit_Percent) {
                    float pct = styleWidth->GetPercentValue();
                    colWidth = NSToCoordFloor(pct * float(tableWidth));
                    pct /= float(colSpan);
                    colFrame->AddPrefPercent(pct);
                    pctTotal += pct;
                } else {
                    colWidth = unassignedMarker;
                }
                if (colWidth != unassignedMarker) {
                    // Add in cell's padding and border.
                    // XXX This should use real percentage padding
                    nsIFrame::IntrinsicWidthOffsetData offsets =
                        cellFrame->IntrinsicWidthOffsets(aReflowState.rendContext);
                    colWidth += offsets.hPadding + offsets.hBorder;

                    if (colSpan > 1) {
                        // If a column-spanning cell is in the first
                        // row, split up the space evenly.  (XXX This
                        // isn't quite right if some of the columns it's
                        // in have specified widths.  Should we care?)
                        colWidth = ((colWidth + spacing) / colSpan) - spacing;
                        if (colWidth < 0)
                            colWidth = 0;
                    }
                }
            } else {
                colWidth = unassignedMarker;
            }
        }

        colFrame->SetFinalWidth(colWidth);

        if (colWidth == unassignedMarker) {
            ++unassignedCount;
        } else {
            unassignedSpace -= colWidth;
        }
    }

    if (unassignedSpace < 0) {
        if (pctTotal > 0) {
            // If the columns took up too much space, reduce those that
            // had percentage widths.  The spec doesn't say to do this,
            // but we've always done it in the past, and so does WinIE6.
            nscoord pctUsed = NSToCoordFloor(pctTotal * float(tableWidth));
            nscoord reduce = PR_MIN(pctUsed, -unassignedSpace);
            float reduceRatio = float(reduce) / pctTotal;
            for (PRInt32 col = 0; col < colCount; ++col) {
                nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
                if (!colFrame) {
                    NS_ERROR("column frames out of sync with cell map");
                    continue;
                }
                nscoord colWidth = colFrame->GetFinalWidth();
                colWidth -= NSToCoordFloor(colFrame->GetPrefPercent() *
                                           reduceRatio);
                if (colWidth < 0)
                    colWidth = 0;
                colFrame->SetFinalWidth(colWidth);
            }
        }
        unassignedSpace = 0;
    }

    if (unassignedCount > 0) {
        nscoord toAssign = unassignedSpace / unassignedCount;
        for (PRInt32 col = 0; col < colCount; ++col) {
            nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
            if (!colFrame) {
                NS_ERROR("column frames out of sync with cell map");
                continue;
            }
            if (colFrame->GetFinalWidth() == unassignedMarker)
                colFrame->SetFinalWidth(toAssign);
        }
    } else if (unassignedSpace > 0) {
        // The spec says to distribute extra space evenly.  (That's not
        // what WinIE6 does, though.  It treats percentages and
        // nonpercentages differently.)
        nscoord toAdd = unassignedSpace / colCount;
        for (PRInt32 col = 0; col < colCount; ++col) {
            nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
            if (!colFrame) {
                NS_ERROR("column frames out of sync with cell map");
                continue;
            }
            colFrame->SetFinalWidth(colFrame->GetFinalWidth() + toAdd);
        }
    }
}
