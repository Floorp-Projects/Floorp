/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "FixedTableLayoutStrategy.h"
#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsVoidArray.h"
#include "nsIPtr.h"
#include "nsHTMLIIDs.h"

NS_DEF_PTR(nsIStyleContext);

FixedTableLayoutStrategy::FixedTableLayoutStrategy(nsTableFrame *aFrame)
  : BasicTableLayoutStrategy(aFrame)
{
}

FixedTableLayoutStrategy::~FixedTableLayoutStrategy()
{
}

PRBool FixedTableLayoutStrategy::BalanceColumnWidths(nsIStyleContext*         aTableStyle,
                                                     const nsHTMLReflowState& aReflowState,
                                                     nscoord                  aMaxWidth)
{
  return PR_TRUE;
}

/*
 * assign the width of all columns
 * if there is a colframe with a width attribute, use it as the column width
 * otherwise if there is a cell in the first row and it has a width attribute, use it
 *  if this cell includes a colspan, width is divided equally among spanned columns
 * otherwise the cell get a proportion of the remaining space 
 *  as determined by the table width attribute.  If no table width attribute, it gets 0 width
 */
PRBool FixedTableLayoutStrategy::AssignPreliminaryColumnWidths(nscoord aComputedWidth)
{
  // NS_ASSERTION(aComputedWidth != NS_UNCONSTRAINEDSIZE, "bad computed width");

  const nsStylePosition* tablePosition;
  mTableFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)tablePosition);
  PRBool tableIsFixedWidth = eStyleUnit_Coord   == tablePosition->mWidth.GetUnit() ||
                             eStyleUnit_Percent == tablePosition->mWidth.GetUnit();

  const nsStyleSpacing* tableSpacing;
  mTableFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)tableSpacing);
  nsMargin borderPadding;
  tableSpacing->CalcBorderPaddingFor(mTableFrame, borderPadding); 

  PRInt32 colX;
  // availWidth is used as the basis for percentage width columns. It is aComputedWidth
  // minus table border, padding, & cellspacing
  nscoord availWidth = aComputedWidth - borderPadding.left - borderPadding.right - 
                       ((mNumCols + 1) * mTableFrame->GetCellSpacingX());
  
  PRInt32 specifiedCols = 0;  // the number of columns whose width is given
  nscoord totalColWidth = 0;  // the sum of the widths of the columns 

  nscoord* colWidths = new PRBool[mNumCols];
  nsCRT::memset(colWidths, -1, mNumCols*sizeof(nscoord));

  // for every column, determine its specified width
  for (colX = 0; colX < mNumCols; colX++) { 
    // Get column information
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    if (!colFrame) {
      NS_ASSERTION(PR_FALSE, "bad col frame");
      return PR_FALSE;
    }

    // Get the columns's style
    const nsStylePosition* colPosition;
    colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);

    // get the fixed width if available
    if (eStyleUnit_Coord == colPosition->mWidth.GetUnit()) { 
      colWidths[colX] = colPosition->mWidth.GetCoordValue();
    } // get the percentage width
    else if ((eStyleUnit_Percent == colPosition->mWidth.GetUnit()) &&
             (aComputedWidth != NS_UNCONSTRAINEDSIZE)) { 
      // Only apply percentages if we're unconstrained.
      float percent = colPosition->mWidth.GetPercentValue();
      colWidths[colX] = NSToCoordRound(percent * (float)availWidth); 
    }
    else { // get width from the cell
      nsTableCellFrame* cellFrame = mTableFrame->GetCellFrameAt(0, colX);
      if (nsnull != cellFrame) {
        // Get the cell's style
        const nsStylePosition* cellPosition;
        cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)cellPosition);

        PRInt32 colSpan = mTableFrame->GetEffectiveColSpan(cellFrame);
        // Get fixed cell width if available
        if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) {
          colWidths[colX] = cellPosition->mWidth.GetCoordValue() / colSpan;
        }
        else if ((eStyleUnit_Percent == cellPosition->mWidth.GetUnit()) &&
                 (aComputedWidth != NS_UNCONSTRAINEDSIZE)) {
          float percent = cellPosition->mWidth.GetPercentValue();
          colWidths[colX] = NSToCoordRound(percent * (float)availWidth / (float)colSpan); 
        }
      }
    }
    if (colWidths[colX] >= 0) {
      totalColWidth += colWidths[colX];
      specifiedCols++;
    }
  }
 
  nscoord lastColAllocated = -1;
  nscoord remainingWidth = availWidth - totalColWidth;
  if (remainingWidth >= 500000) {
    // let's put a cap on the width so that it doesn't become insane.
    remainingWidth = 100;
  }

  if (tableIsFixedWidth && (0 < remainingWidth)) {
    if (mNumCols > specifiedCols) {
      // allocate the extra space to the columns which have no width specified
      nscoord colAlloc = NSToCoordRound( ((float)remainingWidth) / (((float)mNumCols) - ((float)specifiedCols)));
      for (colX = 0; colX < mNumCols; colX++) {
        if (-1 == colWidths[colX]) {
          colWidths[colX] = colAlloc;
          totalColWidth += colAlloc; 
          lastColAllocated = colX;
        }
      }
    }
    else { // allocate the extra space to the columns which have width specified
      float divisor = (float)totalColWidth;
      for (colX = 0; colX < mNumCols; colX++) {
        if (colWidths[colX] > 0) {
          nscoord colAlloc = NSToCoordRound(remainingWidth * colWidths[colX] / divisor);
          colWidths[colX] += colAlloc;
          totalColWidth += colAlloc; 
          lastColAllocated = colX;
        }
      }
    }  
  }

  nscoord overAllocation = (availWidth >= 0) 
    ? totalColWidth - availWidth : 0;
  // set the column widths
  for (colX = 0; colX < mNumCols; colX++) {
    if (colWidths[colX] < 0) 
      colWidths[colX] = 0;
    // if there was too much allocated due to rounding, remove it from the last col
    if ((colX == lastColAllocated) && (overAllocation != 0)) {
      colWidths[colX] += overAllocation;
      colWidths[colX] = PR_MAX(0, colWidths[colX]);
    }
    mTableFrame->SetColumnWidth(colX, colWidths[colX]);
  }

  // min/max TW is min/max of (specified table width, sum of specified column(cell) widths)
  mMinTableContentWidth = mMaxTableContentWidth = totalColWidth;

  // clean up
  if (nsnull != colWidths) {
    delete [] colWidths;
  }
  
  return PR_TRUE;
}

PRBool FixedTableLayoutStrategy::ColumnsCanBeInvalidatedBy(nsStyleCoord*           aPrevStyleWidth,
                                                           const nsTableCellFrame& aCellFrame,
                                                           PRBool                  aConsiderMinWidth) const
{
  return ColumnsCanBeInvalidatedBy(aCellFrame);
}

PRBool FixedTableLayoutStrategy::ColumnsCanBeInvalidatedBy(const nsTableCellFrame& aCellFrame,
                                                           PRBool                  aConsiderMinWidth) const

{
  nscoord rowIndex;
  aCellFrame.GetRowIndex(rowIndex);
  if (0 == rowIndex) {
    // It is not worth the effort to determine if the col or cell determined the col
    // width. Since rebalancing the columns is fairly trival in this strategy, just force it.
    return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool FixedTableLayoutStrategy::ColumnsAreValidFor(const nsTableCellFrame& aCellFrame,
                                                    nscoord                 aPrevCellMin,
                                                    nscoord                 aPrevCellDes) const
{
  // take the easy way out, see comments above.
  return ColumnsCanBeInvalidatedBy(aCellFrame);
}



