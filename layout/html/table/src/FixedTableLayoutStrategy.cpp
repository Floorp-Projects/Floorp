/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "FixedTableLayoutStrategy.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsStyleConsts.h"
#include "nsVoidArray.h"

FixedTableLayoutStrategy::FixedTableLayoutStrategy(nsTableFrame *aFrame)
  : BasicTableLayoutStrategy(aFrame)
{
}

FixedTableLayoutStrategy::~FixedTableLayoutStrategy()
{
}

PRBool FixedTableLayoutStrategy::BalanceColumnWidths(const nsHTMLReflowState& aReflowState)
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
PRBool 
FixedTableLayoutStrategy::AssignNonPctColumnWidths(nscoord                  aComputedWidth,
                                                   const nsHTMLReflowState& aReflowState)
{
  // NS_ASSERTION(aComputedWidth != NS_UNCONSTRAINEDSIZE, "bad computed width");
  const nsStylePosition* tablePosition = mTableFrame->GetStylePosition();
  PRBool tableIsFixedWidth = eStyleUnit_Coord   == tablePosition->mWidth.GetUnit() ||
                             eStyleUnit_Percent == tablePosition->mWidth.GetUnit();

  PRInt32 numCols = mTableFrame->GetColCount();
  PRInt32 colX;
  float pixelToTwips = mTableFrame->GetPresContext()->ScaledPixelsToTwips();
  // availWidth is used as the basis for percentage width columns. It is aComputedWidth
  // minus table border, padding, & cellspacing
  nscoord spacingX = mTableFrame->GetCellSpacingX();
  mCellSpacingTotal = spacingX;
  for (colX = 0; colX < numCols; colX++){
    if (mTableFrame->GetNumCellsOriginatingInCol(colX) > 0) {
      mCellSpacingTotal += spacingX;
    }
  }
  nscoord availWidth = (NS_UNCONSTRAINEDSIZE == aComputedWidth) 
    ? NS_UNCONSTRAINEDSIZE
    : aComputedWidth - aReflowState.mComputedBorderPadding.left - 
      aReflowState.mComputedBorderPadding.right - 
      mCellSpacingTotal;
  
  PRInt32 specifiedCols = 0;  // the number of columns whose width is given
  nscoord totalColWidth = 0;  // the sum of the widths of the columns 

  nscoord* colWidths = new nscoord[numCols];
  if (!colWidths) return PR_FALSE;
  memset(colWidths, WIDTH_NOT_SET, numCols*sizeof(nscoord));

  nscoord* propInfo = new nscoord[numCols];
  if (!propInfo) {
    delete [] colWidths;
    return PR_FALSE;
  }

  memset(propInfo, 0, numCols*sizeof(nscoord));
  nscoord propTotal = 0;
  nscoord percTotal = 0;
  // for every column, determine its specified width
  for (colX = 0; colX < numCols; colX++) { 
    // Get column information
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    if (!colFrame) {
      NS_ASSERTION(PR_FALSE, "bad col frame");
      return PR_FALSE;
    }

    // Get the columns's style
    const nsStylePosition* colPosition = colFrame->GetStylePosition();

    // get the fixed width if available
    if (eStyleUnit_Coord == colPosition->mWidth.GetUnit()) { 
      colWidths[colX] = colPosition->mWidth.GetCoordValue();
      colFrame->SetWidth(MIN_CON, colWidths[colX]);
    } // get the percentage width
    else if ((eStyleUnit_Percent == colPosition->mWidth.GetUnit()) &&
             (aComputedWidth != NS_UNCONSTRAINEDSIZE)) { 
      // Only apply percentages if we're constrained.
      float percent = colPosition->mWidth.GetPercentValue();
      colWidths[colX] = nsTableFrame::RoundToPixel(NSToCoordRound(percent * (float)availWidth), pixelToTwips); 
      colFrame->SetWidth(PCT, colWidths[colX]);
      percTotal+=colWidths[colX];
    }
    else if (eStyleUnit_Proportional == colPosition->mWidth.GetUnit() &&
      colPosition->mWidth.GetIntValue() > 0) {
      propInfo[colX] = colPosition->mWidth.GetIntValue();
      propTotal += propInfo[colX];
    }
    else { // get width from the cell

      nsTableCellFrame* cellFrame = mTableFrame->GetCellFrameAt(0, colX);
      if (nsnull != cellFrame) {
        // Get the cell's style
        const nsStylePosition* cellPosition = cellFrame->GetStylePosition();

        nscoord cellWidth = 0;
        PRInt32 colSpan = mTableFrame->GetEffectiveColSpan(*cellFrame);
        // Get fixed cell width if available
        if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) {
          // need to add border and padding into fixed width
          nsMargin borderPadding = nsTableFrame::GetBorderPadding(nsSize(aReflowState.mComputedWidth, 0),
                                                                  pixelToTwips, cellFrame);
          cellWidth = cellPosition->mWidth.GetCoordValue() + borderPadding.left + borderPadding.right;
          colWidths[colX] = nsTableFrame::RoundToPixel(NSToCoordRound(((float) cellWidth) / ((float) colSpan)),
                                                                      pixelToTwips);
          colFrame->SetWidth(MIN_CON, colWidths[colX]);
        }
        else if ((eStyleUnit_Percent == cellPosition->mWidth.GetUnit()) &&
                 (aComputedWidth != NS_UNCONSTRAINEDSIZE)) {
          float percent = cellPosition->mWidth.GetPercentValue();
          // need to add border and padding into percent width
          nsMargin borderPadding = nsTableFrame::GetBorderPadding(nsSize(aReflowState.mComputedWidth, 0),
                                                                  pixelToTwips, cellFrame);
          cellWidth = NSToCoordRound(percent * (float) availWidth) + borderPadding.left + borderPadding.right;
          colWidths[colX] = nsTableFrame::RoundToPixel(NSToCoordRound(((float) cellWidth) / ((float) colSpan)),
                                                                      pixelToTwips); 
          colFrame->SetWidth(PCT, colWidths[colX]);
          percTotal += colWidths[colX];
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
  if(availWidth == NS_UNCONSTRAINEDSIZE)
    remainingWidth = 0; 
  if (remainingWidth >= 500000) {
    // let's put a cap on the width so that it doesn't become insane.
    remainingWidth = 100;
  }

  if (0 < remainingWidth) {
    if (propTotal > 0) {
      nscoord amountToAllocate = 0;
      for (colX = 0; colX < numCols; colX++) {
        if (propInfo[colX] > 0) {
          // We're proportional
          float percent = ((float)propInfo[colX])/((float)propTotal);
          amountToAllocate += NSToCoordRound(percent * (float)remainingWidth);
          colWidths[colX] = (amountToAllocate > 0) ?
            nsTableFrame::RoundToPixel(amountToAllocate, pixelToTwips,
                                       eRoundUpIfHalfOrMore) : 0;
          totalColWidth += colWidths[colX];
          amountToAllocate -= colWidths[colX];
          lastColAllocated = colX;
        }
      }  
    }
    else if (tableIsFixedWidth) {
      if (numCols > specifiedCols) {
        // allocate the extra space to the columns which have no width specified
        nscoord colAlloc =
          NSToCoordRound(((float)remainingWidth) /
                         (((float)numCols) - ((float)specifiedCols)));
        nscoord amountToAllocate = 0;
        for (colX = 0; colX < numCols; colX++) {
          if (-1 == colWidths[colX]) {
            amountToAllocate += colAlloc;
            colWidths[colX] = (amountToAllocate > 0) ?
              nsTableFrame::RoundToPixel(amountToAllocate,
                                         pixelToTwips,
                                         eRoundUpIfHalfOrMore) : 0;
            totalColWidth += colWidths[colX];
            amountToAllocate -= colWidths[colX];
            lastColAllocated = colX;
          }
        }
      }
      else { // allocate the extra space to the columns which have width specified
        float divisor = (float)totalColWidth;
        nscoord amountToAllocate = 0;
        for (colX = 0; colX < numCols; colX++) {
          if (colWidths[colX] > 0) {
            amountToAllocate += NSToCoordRound(remainingWidth * colWidths[colX] / divisor);
            nscoord colAlloc = (amountToAllocate > 0) ?
              nsTableFrame::RoundToPixel(amountToAllocate, pixelToTwips,
                                         eRoundUpIfHalfOrMore) : 0;
            colWidths[colX] += colAlloc;
            totalColWidth += colAlloc;
            amountToAllocate -= colAlloc;
            lastColAllocated = colX;
          }
        }
      }  
    }
  }

  nscoord overAllocation = ((availWidth >= 0) && (availWidth != NS_UNCONSTRAINEDSIZE))  
    ? totalColWidth - availWidth : 0;
  // set the column widths
  for (colX = 0; colX < numCols; colX++) {
    if (colWidths[colX] < 0) 
      colWidths[colX] = 0;
    // if there was too much allocated due to rounding, remove it from the last col
    if ((colX == lastColAllocated) && (overAllocation != 0)) {
      nscoord thisRemoval = nsTableFrame::RoundToPixel(overAllocation, pixelToTwips);
      colWidths[colX] -= thisRemoval;
      totalColWidth -= thisRemoval;
      
      totalColWidth -= colWidths[colX] - PR_MAX(0, colWidths[colX]);
      colWidths[colX] = PR_MAX(0, colWidths[colX]);
    }
  }
  overAllocation = ((availWidth >= 0) && (availWidth != NS_UNCONSTRAINEDSIZE))  
    ? totalColWidth - availWidth : 0;
  if(overAllocation > 0){
    // reduce over specified percent col
    nscoord amountToRemove = 0;
    for (colX = 0; colX < numCols; colX++) {
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
      if(( colFrame->GetWidth(PCT) > 0) && ( percTotal > 0)){
        amountToRemove += NSToCoordRound(overAllocation* colWidths[colX] / (float) percTotal);
        nscoord thisRemoval = (amountToRemove > 0) ?
          nsTableFrame::RoundToPixel(amountToRemove, pixelToTwips,
                                     eRoundUpIfHalfOrMore) : 0;
        colWidths[colX] -= thisRemoval;
        amountToRemove -= thisRemoval;
        totalColWidth -= thisRemoval;
        
        totalColWidth -= colWidths[colX] - PR_MAX(0, colWidths[colX]);
        colWidths[colX] = PR_MAX(0, colWidths[colX]);
        colFrame->SetWidth(PCT, colWidths[colX]);
      }
    }
  }
  for (colX = 0; colX < numCols; colX++) {
    mTableFrame->SetColumnWidth(colX, colWidths[colX]);
  }

  // clean up
  if (nsnull != colWidths) {
    delete [] colWidths;
  }

  if (nsnull != propInfo) {
    delete [] propInfo;
  }
  
  return PR_TRUE;
}




