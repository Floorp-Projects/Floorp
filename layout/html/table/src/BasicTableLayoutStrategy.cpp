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

#define PL_ARENA_CONST_ALIGN_MASK (sizeof(void*)-1)
#include "plarena.h"

#include "BasicTableLayoutStrategy.h"
#include "nsIPresContext.h"
#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"
#include "nsStyleConsts.h"
#include "nsVoidArray.h"
#include "nsQuickSort.h"


#define ARENA_ALLOCATE(var, pool, num, type) \
    {void *_tmp_; PL_ARENA_ALLOCATE(_tmp_, pool, (num*sizeof(type))); \
    var = NS_REINTERPRET_CAST(type*, _tmp_); }

// Local class used to hold information about table cells so we do not
// have to look it up multiple times
struct CellInfo {
  nsTableCellFrame *cellFrame;
  PRInt32 colSpan;
};

#ifdef DEBUG_TABLE_STRATEGY
static PRInt32 gsDebugCount = 0;
#endif
// The priority of allocations for columns is as follows
//   1) max(MIN, MIN_ADJ)
//   2) max (PCT, PCT_ADJ) 
//   3) FIX 
//   4) FIX_ADJ
//   5) max(DES_CON, DES_ADJ), but use MIN_PRO if present
//   6) for a fixed width table, the column may get more 
//      space if the sum of the col allocations is insufficient 


// the logic here is kept in synch with that in CalculateTotals.
PRBool CanAllocate(PRInt32          aType,
                   PRInt32          aPrevType,
                   nsTableColFrame* aColFrame)
{
  switch(aType) {
  case PCT:
  case FIX:
  case DES_CON:
    return (WIDTH_NOT_SET == aPrevType);
  case FIX_ADJ:
    return (WIDTH_NOT_SET == aPrevType) || (FIX == aPrevType);
  default:
    NS_ASSERTION(PR_FALSE, "invalid call");
  }
  return PR_FALSE;
}

// this doesn't work for a col frame which might get its width from a col
PRBool
HasPctValue(const nsIFrame* aFrame) 
{
  const nsStylePosition* position = aFrame->GetStylePosition();
  if (eStyleUnit_Percent == position->mWidth.GetUnit()) {
    float percent = position->mWidth.GetPercentValue();
    if (percent > 0.0f) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

/* ---------- BasicTableLayoutStrategy ---------- */

MOZ_DECL_CTOR_COUNTER(BasicTableLayoutStrategy)


BasicTableLayoutStrategy::BasicTableLayoutStrategy(nsTableFrame *aFrame, PRBool aIsNavQuirks)
{
  MOZ_COUNT_CTOR(BasicTableLayoutStrategy);
  NS_ASSERTION(nsnull != aFrame, "bad frame arg");

  mTableFrame            = aFrame;
  mCellSpacingTotal      = 0;
  mIsNavQuirksMode       = aIsNavQuirks;
}

BasicTableLayoutStrategy::~BasicTableLayoutStrategy()
{
  MOZ_COUNT_DTOR(BasicTableLayoutStrategy);
}

PRBool BasicTableLayoutStrategy::Initialize(const nsHTMLReflowState& aReflowState)
{
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugTimeMethod(nsTableFrame::eInit, *mTableFrame, (nsHTMLReflowState&)aReflowState, PR_TRUE);
#endif
  ContinuingFrameCheck();

  PRBool result = PR_TRUE;

  // re-init instance variables
  mCellSpacingTotal = 0;
  mCols             = mTableFrame->GetEffectiveCOLSAttribute();

  mTableFrame->SetHasPctCol(PR_FALSE);

  nscoord boxWidth = mTableFrame->CalcBorderBoxWidth(aReflowState);
  PRBool hasPctCol = AssignNonPctColumnWidths(boxWidth, aReflowState);

  mTableFrame->SetHasPctCol(hasPctCol);

  // calc the min, desired, preferred widths from what we know so far
  nscoord minWidth, prefWidth;
  mTableFrame->CalcMinAndPreferredWidths(aReflowState, PR_FALSE, minWidth, prefWidth);
  if (hasPctCol && mTableFrame->IsAutoWidth()) {
    prefWidth = CalcPctAdjTableWidth(aReflowState, boxWidth);
  }
  // calc the desired width, considering if there is a specified width. 
  // don't use nsTableFrame::CalcDesiredWidth because it is based on table column widths.
  nscoord desWidth = (mTableFrame->IsAutoWidth()) ? PR_MIN(prefWidth, aReflowState.availableWidth)
                                                  : prefWidth;
  desWidth = PR_MAX(desWidth, minWidth);

  mTableFrame->SetMinWidth(minWidth);
  mTableFrame->SetDesiredWidth(desWidth);
  mTableFrame->SetPreferredWidth(prefWidth);

  mTableFrame->SetNeedStrategyInit(PR_FALSE);

#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugTimeMethod(nsTableFrame::eInit, *mTableFrame, (nsHTMLReflowState&)aReflowState, PR_FALSE);
#endif
  return result;
}

void BasicTableLayoutStrategy::ContinuingFrameCheck()
{
#ifdef NS_DEBUG
  nsIFrame* tablePIF = nsnull;
  mTableFrame->GetPrevInFlow(&tablePIF);
  NS_ASSERTION(!tablePIF, "never ever call me on a continuing frame!");
#endif
}

PRBool BCW_Wrapup(const nsHTMLReflowState&  aReflowState,
                  BasicTableLayoutStrategy* aStrategy, 
                  nsTableFrame*             aTableFrame, 
                  PRInt32*                  aAllocTypes)
{
  if (aAllocTypes)
    delete [] aAllocTypes;
#ifdef DEBUG_TABLE_STRATEGY
  printf("BalanceColumnWidths ex \n"); aTableFrame->Dump(PR_FALSE, PR_TRUE, PR_FALSE);
#endif
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugTimeMethod(nsTableFrame::eBalanceCols, *aTableFrame, (nsHTMLReflowState&)aReflowState, PR_FALSE);
#endif
  return PR_TRUE;
}

void
ResetPctValues(nsTableFrame* aTableFrame,
               PRInt32       aNumCols)
{
  // initialize the col percent and cell percent values to 0.
  PRInt32 colX;
  for (colX = 0; colX < aNumCols; colX++) { 
    nsTableColFrame* colFrame = aTableFrame->GetColFrame(colX); 
    if (colFrame) {
      colFrame->SetWidth(PCT, WIDTH_NOT_SET);
      colFrame->SetWidth(PCT_ADJ, WIDTH_NOT_SET);
    }
  }
}

PRBool 
BasicTableLayoutStrategy::BalanceColumnWidths(const nsHTMLReflowState& aReflowState)
{
#ifdef DEBUG_TABLE_STRATEGY
  printf("BalanceColumnWidths en count=%d \n", gsDebugCount++); mTableFrame->Dump(PR_FALSE, PR_TRUE, PR_FALSE);
#endif
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugTimeMethod(nsTableFrame::eBalanceCols, *mTableFrame, (nsHTMLReflowState&)aReflowState, PR_TRUE);
#endif
  float p2t = mTableFrame->GetPresContext()->ScaledPixelsToTwips();

  ContinuingFrameCheck();

  PRInt32 numCols = mTableFrame->GetColCount();
  PRBool tableIsAutoWidth = mTableFrame->IsAutoWidth();

  nscoord horOffset; 
  // get the reduction in available horizontal space due to borders and padding
  nsMargin offset = mTableFrame->GetChildAreaOffset(&aReflowState);
  horOffset = offset.left + offset.right;

  // determine if the table is auto/fixed and get the fixed width if available
  nscoord maxWidth = mTableFrame->CalcBorderBoxWidth(aReflowState);
  if (NS_UNCONSTRAINEDSIZE == maxWidth) {
    maxWidth = PR_MIN(maxWidth, aReflowState.availableWidth);
    if (NS_UNCONSTRAINEDSIZE == maxWidth) {
      NS_ASSERTION(NS_UNCONSTRAINEDSIZE != maxWidth, "cannot balance with an unconstrained width");
      return PR_FALSE;
    }
  }
  // initialize the col percent and cell percent values to 0.
  ResetPctValues(mTableFrame, numCols);

  // An auto table returns a new table width based on percent cells/cols if they exist
  nscoord perAdjTableWidth = 0;
  if (mTableFrame->HasPctCol()) {
    perAdjTableWidth = AssignPctColumnWidths(aReflowState, maxWidth, tableIsAutoWidth, p2t);
    if (perAdjTableWidth > 0) {
      // if an auto table has a pct col or cell, set the preferred table width 
      // here so that CalcPctAdjTableWidth wont't need to be called by the table
      mTableFrame->SetPreferredWidth(perAdjTableWidth);
    }
    perAdjTableWidth = PR_MIN(perAdjTableWidth, maxWidth);
    perAdjTableWidth -= horOffset;
    perAdjTableWidth = PR_MAX(perAdjTableWidth, 0);
  }

  // reduce the maxWidth by border and padding, since we will be dealing with content width
  maxWidth -= horOffset;
  maxWidth = PR_MAX(0, maxWidth);

  // we will re-calc mCellSpacingTotal in case longer rows were added after Initialize was called
  mCellSpacingTotal = 0;
  nscoord spacingX = mTableFrame->GetCellSpacingX();

  PRInt32 numNonZeroWidthCols = 0;
  // set the table's columns to the min width
  nscoord minTableWidth = 0;
  PRInt32 colX;
  for (colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    nscoord colMinWidth = colFrame->GetMinWidth();
    mTableFrame->SetColumnWidth(colX, colMinWidth);
    minTableWidth += colMinWidth;
    if ((colFrame->GetMinWidth() > 0) || (colFrame->GetDesWidth() > 0) ||
        (colFrame->GetFixWidth() > 0) || (colFrame->GetPctWidth() > 0) || 
        (colFrame->GetWidth(MIN_PRO) > 0)) {
      numNonZeroWidthCols++;
    }
    if (mTableFrame->GetNumCellsOriginatingInCol(colX) > 0) {
      mCellSpacingTotal += spacingX;
    }
  }
  if (mCellSpacingTotal > 0) {
    mCellSpacingTotal += spacingX; // add last cell spacing on right
  }
  minTableWidth += mCellSpacingTotal;

  // if the max width available is less than the min content width for fixed table, we're done
  if (!tableIsAutoWidth && (maxWidth < minTableWidth)) {
    return BCW_Wrapup(aReflowState, this, mTableFrame, nsnull);
  }

  // if the max width available is less than the min content width for auto table
  // that had no % cells/cols, we're done
  if (tableIsAutoWidth && (maxWidth < minTableWidth) && (0 == perAdjTableWidth)) {
    return BCW_Wrapup(aReflowState, this, mTableFrame, nsnull);
  }

  // the following are of size NUM_WIDTHS, but only MIN_CON, DES_CON, FIX, FIX_ADJ, PCT
  // are used and they account for colspan ADJusted values
  PRInt32 totalWidths[NUM_WIDTHS]; // sum of col widths of a particular type 
  PRInt32 totalCounts[NUM_WIDTHS]; // num of cols of a particular type
  PRInt32 dupedWidths[NUM_WIDTHS];
  PRInt32 num0Proportional;

  CalculateTotals(totalCounts, totalWidths, dupedWidths, num0Proportional);
  // auto width table's adjusted width needs cell spacing
  if (tableIsAutoWidth && perAdjTableWidth > 0) { 
    maxWidth = perAdjTableWidth;
  }
  nscoord totalAllocated = totalWidths[MIN_CON] + mCellSpacingTotal;
  
  // allocate and initialize arrays indicating what col gets set
  PRInt32* allocTypes = new PRInt32[numCols];
  if (!allocTypes) return PR_FALSE;
 
  for (colX = 0; colX < numCols; colX++) {
    allocTypes[colX] = -1;
  }

  // allocate PCT/PCT_ADJ cols
  if (totalCounts[PCT] > 0) {
    if (totalAllocated + totalWidths[PCT] - dupedWidths[PCT] <= maxWidth) {
      AllocateFully(totalAllocated, allocTypes, PCT);
      //NS_WARN_IF_FALSE(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, PCT, PR_FALSE, allocTypes, p2t);
      return BCW_Wrapup(aReflowState, this, mTableFrame, allocTypes);
    }
  }
  // allocate FIX cols
  if ((totalAllocated < maxWidth) && (totalCounts[FIX] > 0)) {
    if (totalAllocated + totalWidths[FIX] - dupedWidths[FIX] <= maxWidth) { 
      AllocateFully(totalAllocated, allocTypes, FIX);
      //NS_WARN_IF_FALSE(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, FIX, PR_TRUE, allocTypes, p2t);
      return BCW_Wrapup(aReflowState, this, mTableFrame, allocTypes);
    }
  }
  // allocate fixed adjusted cols
  if ((totalAllocated < maxWidth) && (totalCounts[FIX_ADJ] > 0)) {
    if (totalAllocated + totalWidths[FIX_ADJ] - dupedWidths[FIX_ADJ] <= maxWidth) { 
      AllocateFully(totalAllocated, allocTypes, FIX_ADJ);
      //NS_WARN_IF_FALSE(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, FIX_ADJ, PR_TRUE, allocTypes, p2t);
      return BCW_Wrapup(aReflowState, this, mTableFrame, allocTypes);
    }
  }

  // allocate proportional and auto cols together
  if ((totalAllocated < maxWidth) && (totalCounts[MIN_PRO] + totalCounts[DES_CON] > 0)) {
    if (totalAllocated + totalWidths[MIN_PRO] - dupedWidths[MIN_PRO] +
        totalWidths[DES_CON] - dupedWidths[DES_CON] <= maxWidth) { 
      AllocateFully(totalAllocated, allocTypes, DES_CON);
      //NS_WARN_IF_FALSE(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, DES_CON, PR_TRUE, allocTypes, p2t);
      return BCW_Wrapup(aReflowState, this, mTableFrame, allocTypes);
    }
  }

  // if this is a nested non auto table and pass1 reflow, we are done
  if ((maxWidth == NS_UNCONSTRAINEDSIZE) && (!tableIsAutoWidth))  {
    return BCW_Wrapup(aReflowState, this, mTableFrame, allocTypes);
  }

  // allocate the rest to auto columns, with some exceptions
  if ( (tableIsAutoWidth && (perAdjTableWidth - totalAllocated > 0)) ||
       (!tableIsAutoWidth && (totalAllocated < maxWidth)) ) {
    // determine how many cols have width either because of a min, fixed, des, or pct value
    PRBool excludePct  = (totalCounts[PCT] != numNonZeroWidthCols);
    PRBool excludeFix  = (totalCounts[PCT] + totalCounts[FIX] + totalCounts[FIX_ADJ] < numNonZeroWidthCols);
    PRBool excludePro  = (totalCounts[DES_CON] > 0);
    PRBool exclude0Pro = (totalCounts[MIN_PRO] != num0Proportional);
    if (tableIsAutoWidth) {
      AllocateUnconstrained(perAdjTableWidth - totalAllocated, allocTypes, excludePct,
                            excludeFix, excludePro, exclude0Pro, p2t);
    }
    else {
      AllocateUnconstrained(maxWidth - totalAllocated, allocTypes, excludePct,
                            excludeFix, excludePro, exclude0Pro, p2t);
    }
  }

  return BCW_Wrapup(aReflowState, this, mTableFrame, allocTypes);
}

nscoord GetColWidth(nsTableColFrame* aColFrame,
                    PRInt32          aWidthType)
{
  switch(aWidthType) {
  case DES_CON:
    return aColFrame->GetDesWidth();
  case FIX:
  case FIX_ADJ:
    return aColFrame->GetWidth(aWidthType);
  case PCT:
    return aColFrame->GetPctWidth();
  default:
    NS_ASSERTION(PR_FALSE, "invalid call");
    return WIDTH_NOT_SET;
  }
}

// Allocate aWidthType values to all cols available in aAllocTypes 
void BasicTableLayoutStrategy::AllocateFully(nscoord&        aTotalAllocated,
                                             PRInt32*        aAllocTypes,
                                             PRInt32         aWidthType)
{
  PRInt32 numCols = mTableFrame->GetColCount(); 
  for (PRInt32 colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    if (!CanAllocate(aWidthType, aAllocTypes[colX], colFrame)) {
      continue;
    }
    nscoord oldWidth = mTableFrame->GetColumnWidth(colX);
    nscoord newWidth = GetColWidth(colFrame, aWidthType);
    // proportional and desired widths are handled together
    PRBool haveProWidth = PR_FALSE;
    if (DES_CON == aWidthType) {
      nscoord proWidth = colFrame->GetWidth(MIN_PRO);
      if (proWidth >= 0) {
        haveProWidth = PR_TRUE;
        newWidth = proWidth;
      }
    }

    if (WIDTH_NOT_SET == newWidth) continue;
   
    if (newWidth > oldWidth) {
      mTableFrame->SetColumnWidth(colX, newWidth);
      aTotalAllocated += newWidth - oldWidth;
    }
    aAllocTypes[colX] = (haveProWidth) ? MIN_PRO : aWidthType;
  }
}

// This method is the last allocation method to get called, but just in case
// code gets added later after, set the allocated cols to some final value.
#define FINISHED 99 
void BasicTableLayoutStrategy::AllocateUnconstrained(PRInt32  aAllocAmount,
                                                     PRInt32* aAllocTypes,
                                                     PRBool   aExcludePct,
                                                     PRBool   aExcludeFix,
                                                     PRBool   aExcludePro,
                                                     PRBool   aExclude0Pro,
                                                     float    aPixelToTwips)
{
  // set up allocTypes to exclude anything but auto cols if possible
  PRInt32 colX;
  PRInt32 numCols = mTableFrame->GetColCount();
  for (colX = 0; colX < numCols; colX++) {
    if (aExcludePct && (PCT == aAllocTypes[colX])) {
      aAllocTypes[colX] = FINISHED;
    }
    else if (aExcludeFix && ((FIX == aAllocTypes[colX]) || (FIX_ADJ == aAllocTypes[colX]))) {
      aAllocTypes[colX] = FINISHED;
    }
    else if (MIN_PRO == aAllocTypes[colX]) {
      if (aExcludePro) {
        aAllocTypes[colX] = FINISHED;
      }
      else {
        if (aExclude0Pro) {
          nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
          if (!colFrame) continue;
          if (colFrame->GetConstraint() == e0ProportionConstraint) {
            aAllocTypes[colX] = FINISHED;
          }
        }
      }
    }
  }
    
  nscoord divisor          = 0;
  PRInt32 numColsAllocated = 0; 
  PRInt32 totalAllocated   = 0;
  for (colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue; 
    PRBool skipColumn = aExclude0Pro && (e0ProportionConstraint == colFrame->GetConstraint());
    if (FINISHED != aAllocTypes[colX] && !skipColumn ) {
      divisor += mTableFrame->GetColumnWidth(colX);
      numColsAllocated++;
    }
  }
  if (!numColsAllocated) {
    // redistribute the space to all columns and prevent a division by zero
    numColsAllocated = numCols;
  }
  for (colX = 0; colX < numCols; colX++) { 
    if (FINISHED != aAllocTypes[colX]) {
      if (aExclude0Pro) {
        nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
        if (!colFrame) continue;
        if (e0ProportionConstraint == colFrame->GetConstraint()) {
          continue;
        }
      }
      nscoord oldWidth = mTableFrame->GetColumnWidth(colX);
      float percent = (divisor == 0) 
        ? (1.0f / ((float)numColsAllocated))
        : ((float)oldWidth) / ((float)divisor);
      nscoord addition = nsTableFrame::RoundToPixel(NSToCoordRound(((float)aAllocAmount) * percent), aPixelToTwips);
      if (addition > (aAllocAmount - totalAllocated)) {
        addition = nsTableFrame::RoundToPixel(aAllocAmount - totalAllocated, aPixelToTwips);
        mTableFrame->SetColumnWidth(colX, oldWidth + addition);
        break;
      }
      mTableFrame->SetColumnWidth(colX, oldWidth + addition);
      totalAllocated += addition;
    }
  }
}

nscoord GetConstrainedWidth(nsTableColFrame* colFrame,
                            PRBool           aConsiderPct)
                           
{
  nscoord conWidth = WIDTH_NOT_SET;
  if (aConsiderPct) {
    conWidth = colFrame->GetPctWidth();
  }
  if (conWidth <= 0 ) {
    conWidth = colFrame->GetFixWidth();
  }
  return conWidth;
}

#define LIMIT_PCT   0 // retrict allocations to only the pct  width cols
#define LIMIT_FIX   1 // retrict allocations to only the fix  width cols
#define LIMIT_DES   2 // retrict allocations to only the auto width cols
#define LIMIT_NONE  3 // retrict allocations to only the auto widths unless there are none
                      // and then restrict fix or pct.

static int RowSortCB(const void *a, const void *b, void *)
{
  const CellInfo *aa = NS_STATIC_CAST(const CellInfo*,a);
  const CellInfo *bb = NS_STATIC_CAST(const CellInfo*,b);

  return aa->colSpan - bb->colSpan;
}

/**
  * This function finds all the rows which have a colspan greater than 1,
  * and places those rows into the cellInfo array.  This array is then
  * sorted so that the rows with the largest colspan are last.
  * @param TableFrame - The table we are working on
  * @param colX       - The column in the table we are considering
  * @param numRows    - The number of rows in the table
  * @param cellInfo   - The array to store the result in.
  * @return           - The number of entries stored in the array
  */
static PRInt32
GetSortedFrames(nsTableFrame *TableFrame,
                PRInt32       colX,
                PRInt32       numRows,
                CellInfo     *cellInfo)
{
  PRInt32 rowX, index = 0;
  for (rowX = 0; rowX < numRows; rowX++) {
    CellInfo *inf = &cellInfo[index];
    PRBool orig;
    inf->cellFrame =
      TableFrame->GetCellInfoAt(rowX, colX, &orig, &inf->colSpan);

    // If the cell is good we keep it.
    if(inf->cellFrame && orig && 1 != inf->colSpan) {
      index++;
    }
  }

  if(index>1) {
    NS_QuickSort(cellInfo, index, sizeof(cellInfo[0]), RowSortCB, 0);
  }

  return index;
}

void 
BasicTableLayoutStrategy::ComputeNonPctColspanWidths(const nsHTMLReflowState& aReflowState,
                                                     PRBool                   aConsiderPct,
                                                     float                    aPixelToTwips,
                                                     PRBool*                  aHasPctCol)
{
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugTimeMethod(nsTableFrame::eNonPctColspans, *mTableFrame, (nsHTMLReflowState&)aReflowState, PR_TRUE);
#endif
  PRInt32 numCols = mTableFrame->GetColCount();
  PRInt32 numEffCols = mTableFrame->GetEffectiveColCount();
  // zero out prior ADJ values 

  PRInt32 colX;
  for (colX = numCols - 1; colX >= 0; colX--) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    colFrame->SetWidth(MIN_ADJ, WIDTH_NOT_SET);
    colFrame->SetWidth(FIX_ADJ, WIDTH_NOT_SET);
    colFrame->SetWidth(DES_ADJ, WIDTH_NOT_SET);
  }

  // For each col, consider the cells originating in it with colspans > 1.
  // Adjust the cols that each cell spans if necessary. Iterate backwards 
  // so that nested and/or overlaping col spans handle the inner ones first, 
  // ensuring more accurated calculations.
  // if more than one  colspan originate in one column, resort the access to 
  // the rows so that the inner colspans are handled first
  PRInt32 numRows = mTableFrame->GetRowCount();

  CellInfo* cellInfo = new CellInfo[numRows];

  if(!cellInfo) {
    return;
  }

  for (colX = numEffCols - 1; colX >= 0; colX--) { // loop only over effective columns
    PRInt32 spannedRows = GetSortedFrames(mTableFrame, colX, numRows, cellInfo);

    for (PRInt32 i = 0; i < spannedRows; i++) {
      const CellInfo *inf = &cellInfo[i];
      const nsTableCellFrame* cellFrame = inf->cellFrame;

      const PRInt32 colSpan = PR_MIN(inf->colSpan, numEffCols - colX);
      // set MIN_ADJ, DES_ADJ, FIX_ADJ
      for (PRInt32 widthX = 0; widthX < NUM_MAJOR_WIDTHS; widthX++) {
        nscoord cellWidth = 0;
        if (MIN_CON == widthX) {
          cellWidth = cellFrame->GetPass1MaxElementWidth();
        }
        else if (DES_CON == widthX) {
          cellWidth = cellFrame->GetMaximumWidth();
        }
        else { // FIX width
          // see if the cell has a style width specified
          const nsStylePosition* cellPosition = cellFrame->GetStylePosition();
          if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) {
            // need to add borde and padding into fixed width
            nsMargin borderPadding = nsTableFrame::GetBorderPadding(nsSize(aReflowState.mComputedWidth, 0),
                                                                    aPixelToTwips, cellFrame);
            cellWidth = cellPosition->mWidth.GetCoordValue() + borderPadding.left + borderPadding.right;
            cellWidth = PR_MAX(cellWidth, cellFrame->GetPass1MaxElementWidth());
          }
        }

        if (0 >= cellWidth) continue;

        // first allocate pct cells up to their value if aConsiderPct, then
        // allocate fixed cells up to their value, then allocate auto cells 
        // up to their value, then allocate auto cells proportionally.
        PRInt32 limit = (aConsiderPct) ? LIMIT_PCT : LIMIT_FIX;
        if (MIN_CON != widthX) { // FIX and DES only need to allocate NONE
          limit = LIMIT_NONE;
        }
        while (limit <= LIMIT_NONE) {
          if (ComputeNonPctColspanWidths(widthX, cellFrame, cellWidth, colX, colSpan, 
                                         limit, aPixelToTwips)) {
            break;
          }
          limit++;
        }
      }
      // determine if there is a pct col if we are requested to do so
      if (aHasPctCol && !*aHasPctCol) {
        *aHasPctCol = HasPctValue(cellFrame);
      }
    }
  }
  delete [] cellInfo;
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugTimeMethod(nsTableFrame::eNonPctColspans, *mTableFrame, (nsHTMLReflowState&)aReflowState, PR_FALSE);
#endif
}

#ifdef DEBUG
static char* widths[] = {"MIN", "DES", "FIX"};
static char* limits[] = {"PCT", "FIX", "DES", "NONE"};
static PRInt32 dumpCount = 0;
void DumpColWidths(nsTableFrame& aTableFrame,
                   char*         aMessage,
                   const nsTableCellFrame* aCellFrame,
                   PRInt32       aColIndex,
                   PRInt32       aWidthType,
                   PRInt32       aLimitType)
{
  PRInt32 rowIndex;
  aCellFrame->GetRowIndex(rowIndex);
  printf ("%s (row,col)=(%d,%d) width=%s limit=%s count=%d\n", aMessage, rowIndex, aColIndex, 
          widths[aWidthType], limits[aLimitType], dumpCount++);
  for (PRInt32 i = 0; i < aTableFrame.GetColCount(); i++) {
    printf(" col %d = ", i);
    nsTableColFrame* colFrame = aTableFrame.GetColFrame(i);
    for (PRInt32 j = 0; j < NUM_WIDTHS; j++) {
      printf("%d ", colFrame->GetWidth(j));
    }
    printf("\n");
  }
}
#endif

// (aWidthIndex == MIN_CON) is called with:
//   (aLimitType == LIMIT_PCT)  to give MIN_ADJ to PCT cols (up to their PCT value) 
//   (aLimitType == LIMIT_FIX)  to give MIN_ADJ to FIX cols (up to their FIX value) 
//   (aLimitType == LIMIT_DES)  to give MIN_ADJ to auto cols (up to their preferred value) 
//   (aLimitType == LIMIT_NONE) to give MIN_ADJ to auto cols  
// (aWidthIndex == FIX), (aWidthIndex == DES) is called with:
//   (aLimitType == LIMIT_NONE) to give FIX_ADJ to auto cols  
PRBool 
BasicTableLayoutStrategy::ComputeNonPctColspanWidths(PRInt32           aWidthIndex,
                                                     const nsTableCellFrame* aCellFrame,
                                                     nscoord           aCellWidth,
                                                     PRInt32           aColIndex,
                                                     PRInt32           aColSpan,
                                                     PRInt32&          aLimitType,
                                                     float             aPixelToTwips)
{
#ifdef DEBUG_TABLE_STRATEGY
  DumpColWidths(*mTableFrame, "enter ComputeNonPctColspanWidths", aCellFrame, aColIndex, aWidthIndex, aLimitType);
#endif  
  PRBool result = PR_TRUE;

  nscoord spanCellSpacing   = 0; // total cell spacing cells being spanned
  nscoord minTotal          = 0; // the sum of min widths of spanned cells. 
  nscoord autoMinTotal      = 0; // the sum of min widths of spanned auto cells. 
  nscoord fixMinTotal       = 0; // the sum of min widths of spanned fix cells. 
  nscoord pctMinTotal       = 0; // the sum of min widths of spanned pct cells. 
  nscoord pctTotal          = 0; // the sum of pct widths of spanned cells. 
  nscoord fixTotal          = 0; // the sum of fix widths of spanned cells
  nscoord autoDesTotal      = 0; // the sum of desired widths of spanned cells which are auto
  nscoord pctLimitTotal     = 0; // the sum of pct widths not exceeding aCellWidth
  nscoord fixLimitTotal     = 0; // the sum of fix widths not exceeding aCellWidth
  nscoord desLimitTotal     = 0; // the sum of des widths not exceeding aCellWidth
  PRInt32 numAutoCols       = 0;

  nscoord spacingX = mTableFrame->GetCellSpacingX();
  PRInt32 spanX;
  // accumulate the various divisors to be used later
  for (spanX = 0; spanX < aColSpan; spanX++) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(aColIndex + spanX); 
    if (!colFrame) continue;
    nscoord minWidth = PR_MAX(colFrame->GetMinWidth(), 0);
    nscoord pctWidth = PR_MAX(colFrame->GetPctWidth(), 0);
    nscoord fixWidth = PR_MAX(colFrame->GetFixWidth(), 0);
    nscoord desWidth = PR_MAX(colFrame->GetDesWidth(), minWidth);

    minTotal += minWidth;

    if (pctWidth > 0) {
      pctTotal += pctWidth;
      pctMinTotal += minWidth;
    }
    else if (fixWidth > 0) {
      fixTotal += fixWidth;
      fixMinTotal += minWidth;
    }
    if (colFrame->GetWidth(PCT) + colFrame->GetWidth(FIX) <= 0) {
      autoDesTotal += desWidth;
      autoMinTotal += minWidth;
      numAutoCols++;
    }
    if ((spanX > 0) && (mTableFrame->GetNumCellsOriginatingInCol(aColIndex + spanX) > 0)) {
      spanCellSpacing += spacingX;
    }
  }
  nscoord availWidth = aCellWidth - spanCellSpacing;
  if (MIN_CON == aWidthIndex) {
    // MIN widths (unlike the others) are added to existing values, so subtract out the min total
    availWidth -= minTotal;

    pctLimitTotal = ((pctTotal - pctMinTotal) < availWidth) ? pctTotal - pctMinTotal : availWidth;
    fixLimitTotal = ((fixTotal - fixMinTotal) < availWidth) ? fixTotal - fixMinTotal : availWidth;
    desLimitTotal = ((autoDesTotal - autoMinTotal) < availWidth) ? autoDesTotal - autoMinTotal : availWidth;
  }
  else { // FIX or DES_CON with LIMIT_NONE
    if (FIX == aWidthIndex) {
      if (autoDesTotal > 0) {
        availWidth -= pctTotal + fixTotal;
      }
      else if (fixTotal > 0) {
        availWidth -= pctTotal;
      }
    }
    else if (DES_CON == aWidthIndex) {
      availWidth -= pctTotal + fixTotal; 
    }
  }

  // Below are some optimizations which can skip steps.
  if (LIMIT_PCT == aLimitType) {
    NS_ASSERTION(MIN_CON == aWidthIndex, "invalid call to ComputeNonPctColspanWidths");
    // if there are no pct cols, limit fix cols
    if (0 == pctTotal) {
      aLimitType = LIMIT_FIX;
    }
  }
  if (LIMIT_FIX == aLimitType) {
    NS_ASSERTION(MIN_CON == aWidthIndex, "invalid call to ComputeNonPctColspanWidths");
    // if there are no fix cols, limit des
    if (0 == fixTotal) {
      aLimitType = LIMIT_DES;
    }
  }
  if (LIMIT_DES == aLimitType) {
    NS_ASSERTION(MIN_CON == aWidthIndex, "invalid call to ComputeNonPctColspanWidths");
    // if there are no auto cols, limit none
    if (0 == autoDesTotal) {
      aLimitType = LIMIT_NONE;
    }
  }

  // there are some cases where allocating up to aCellWidth will not be satisfied
  switch (aLimitType) {
  case LIMIT_PCT:
    if (pctLimitTotal < availWidth) {
      // the pct cols won't satisfy the request alone
      availWidth = pctLimitTotal;
      result = PR_FALSE;
    }
    break;
  case LIMIT_FIX:
    if (fixLimitTotal < availWidth) {
      // the fix cols won't satisfy the request alone
      availWidth = fixLimitTotal;
      result = PR_FALSE;
    }
    break;
  case LIMIT_DES:
    if (desLimitTotal < availWidth) {
      // the auto cols won't satisfy the request alone
      availWidth = desLimitTotal;
      result = PR_FALSE;
    }
    break;
  default: // LIMIT_NONE
    break;
  }

  if (availWidth > 0) {
    nscoord usedWidth = 0;
    // get the correct numerator in a similar fashion to getting the divisor
    for (spanX = 0; spanX < aColSpan; spanX++) {
      if (usedWidth >= availWidth) break;
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(aColIndex + spanX); 
      if (!colFrame) continue;
      nscoord minWidth = colFrame->GetMinWidth();
      nscoord pctWidth = PR_MAX(colFrame->GetPctWidth(), 0);
      nscoord fixWidth = PR_MAX(colFrame->GetFixWidth(), 0);
      nscoord desWidth = PR_MAX(colFrame->GetDesWidth(), minWidth);

      nscoord colWidth = PR_MAX(colFrame->GetWidth(aWidthIndex), 
                                colFrame->GetWidth(aWidthIndex + NUM_MAJOR_WIDTHS));
      colWidth = PR_MAX(colWidth, minWidth);

      nscoord numeratorPct     = 0;
      nscoord numeratorFix     = 0;
      nscoord numeratorAutoDes = 0;

      if (pctWidth > 0) {
        numeratorPct = pctWidth;
      }
      else if (fixWidth > 0) {
        numeratorFix = fixWidth;
      }
      if (colFrame->GetWidth(PCT) + colFrame->GetWidth(FIX) <= 0) {
        numeratorAutoDes = desWidth;
      }

      nscoord divisor = 0;
      nscoord numerator = 0;

      // let pct cols reach their target 
      if (LIMIT_PCT == aLimitType) {
        if (pctLimitTotal > 0) {
          divisor   = pctTotal - pctMinTotal;
          numerator = PR_MAX(numeratorPct - minWidth, 0);
        }
      }
      // let fix cols reach their target 
      else if (LIMIT_FIX == aLimitType) {
        if (fixLimitTotal > 0) {
          divisor   = fixTotal - fixMinTotal;
          numerator = PR_MAX(numeratorFix - minWidth, 0);
        }
      }
      // let auto cols reach their target 
      else if (LIMIT_DES == aLimitType) {
        if (desLimitTotal > 0) {
          if (autoDesTotal > 0) { // there were auto cols
            divisor   = autoDesTotal - autoMinTotal;
            numerator = PR_MAX(numeratorAutoDes - minWidth, 0);
          }
          else if (fixTotal > 0) {  // there were fix cols but no auto
            divisor   = fixTotal - fixMinTotal;
            numerator = PR_MAX(numeratorFix - minWidth, 0);
          }
          else if (pctTotal > 0) {  // there were only pct cols
            divisor   = pctTotal - pctMinTotal;
            numerator = PR_MAX(numeratorPct - minWidth, 0);
          }
          else continue;
        }
      }
      else if (LIMIT_NONE == aLimitType) { 
        if ((MIN_CON == aWidthIndex) && (0 == minTotal)) {
          // divide evenly among the spanned cols
          divisor = aColSpan;
          numerator = 1;
        }
        else {
          if (autoDesTotal > 0) { // there were auto cols
            divisor   = autoDesTotal;
            numerator = numeratorAutoDes;
          }
          else if (fixTotal > 0) {  // there were fix cols but no auto
            divisor   = fixTotal;
            numerator = numeratorFix;
          }
          else if (pctTotal > 0) {  // there were only pct cols
            divisor   = pctTotal;
            numerator = numeratorPct;
          }
          else continue;

        }
      }
      // calculate the adj col width
      nscoord newColAdjWidth = (0 >= divisor) 
        ? NSToCoordRound( ((float)availWidth) / ((float)aColSpan) )
        : NSToCoordRound( (((float)numerator) / ((float)divisor)) * availWidth);
      newColAdjWidth = nsTableFrame::RoundToPixel(newColAdjWidth, aPixelToTwips);
      // don't let the new allocation exceed the avail total
      newColAdjWidth = PR_MIN(newColAdjWidth, availWidth - usedWidth);
      // put the remainder of the avail total in the last spanned col
      if (spanX == aColSpan - 1) {
        newColAdjWidth = availWidth - usedWidth;
      }
      usedWidth += newColAdjWidth;
      // MIN_CON gets added to what is there, the others don't
      if (MIN_CON == aWidthIndex) {
        newColAdjWidth += colWidth;
      }
      if (newColAdjWidth > colWidth) { 
        if (FIX == aWidthIndex) {
          // a colspan cannot fix a column below what a cell desires on its own
          nscoord desCon = colFrame->GetWidth(DES_CON); // do not consider DES_ADJ
          if (desCon > newColAdjWidth) {
            newColAdjWidth = desCon;
          }
        }
        colFrame->SetWidth(aWidthIndex + NUM_MAJOR_WIDTHS, newColAdjWidth);
      }
      else {        
        if((newColAdjWidth > 0) && (FIX == aWidthIndex)) {
          if(colFrame->GetWidth(FIX) < 0) {
            nscoord desCon = colFrame->GetWidth(DES_CON);
            if (desCon > newColAdjWidth) {
              newColAdjWidth = desCon;
            }
          }
          if(colFrame->GetWidth(FIX_ADJ) < newColAdjWidth) {
            colFrame->SetWidth(FIX_ADJ, PR_MAX(colWidth,newColAdjWidth));
          }
        }
      }
    }
  }
#ifdef DEBUG_TABLE_STRATEGY
  DumpColWidths(*mTableFrame, "exit ComputeNonPctColspanWidths", aCellFrame, aColIndex, aWidthIndex, aLimitType);
#endif
  return result;
}


// XXX percent left and right padding are not figured in the calculations
// The table frame needs to be used as the percent base because the reflow
// state may have an unconstrained width. There should probably be a frame
// state bit indicating that horizontal padding is percentage based.

// Determine min, desired, fixed, and proportional sizes for the cols and 
// and calculate min/max table width. Return true if there is at least one pct cell or col
PRBool 
BasicTableLayoutStrategy::AssignNonPctColumnWidths(nscoord                  aMaxWidth,
                                                   const nsHTMLReflowState& aReflowState)
{
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugTimeMethod(nsTableFrame::eNonPctCols, *mTableFrame, (nsHTMLReflowState&)aReflowState, PR_TRUE);
#endif
#ifdef DEBUG_TABLE_STRATEGY
  printf("AssignNonPctColWidths en max=%d count=%d \n", aMaxWidth, gsDebugCount++); mTableFrame->Dump(PR_FALSE, PR_TRUE, PR_FALSE);
#endif
  PRInt32 numRows = mTableFrame->GetRowCount();
  PRInt32 numCols = mTableFrame->GetColCount();
  nscoord spacingX = mTableFrame->GetCellSpacingX();
  PRInt32 colX, rowX; 
  mCellSpacingTotal = 0;
  PRBool hasPctCol = PR_FALSE; // return value
  float pixelToTwips = mTableFrame->GetPresContext()->ScaledPixelsToTwips();

  PRInt32 rawPropTotal = -1; // total of numbers of the type 1*, 2*, etc 
  PRInt32 numColsForColsAttr = 0; // Nav Quirks cols attribute for equal width cols
  if (NS_STYLE_TABLE_COLS_NONE != mCols) {
    numColsForColsAttr = (NS_STYLE_TABLE_COLS_ALL == mCols) ? numCols : mCols;
  }

  // For every column, determine it's min and desired width based on cell style
  // base on cells which do not span cols. Also, determine mCellSpacingTotal
  for (colX = 0; colX < numCols; colX++) { 
    nscoord minWidth = 0;
    nscoord desWidth = 0;
    nscoord fixWidth = WIDTH_NOT_SET;
    
    // Get column frame and reset it
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    NS_ASSERTION(nsnull != colFrame, "bad col frame");
    colFrame->ResetSizingInfo();

    if (mTableFrame->GetNumCellsOriginatingInCol(colX) > 0) {
      mCellSpacingTotal += spacingX;
    }

    // Scan the cells in the col that have colspan = 1 and find the maximum
    // min, desired, and fixed cells.
    nsTableCellFrame* fixContributor = nsnull;
    nsTableCellFrame* desContributor = nsnull;
    for (rowX = 0; rowX < numRows; rowX++) {
      PRBool originates;
      PRInt32 colSpan;
      nsTableCellFrame* cellFrame = mTableFrame->GetCellInfoAt(rowX, colX, &originates, &colSpan);
      // skip cells that don't originate at (rowX, colX); colspans are handled in the
      // next pass, row spans don't need to be handled
      if (!cellFrame || !originates || (colSpan > 1)) { 
        continue;
      }
      // these values include borders and padding
      minWidth = PR_MAX(minWidth, cellFrame->GetPass1MaxElementWidth());
      nscoord cellDesWidth = cellFrame->GetMaximumWidth();
      if (cellDesWidth > desWidth) {
        desContributor = cellFrame;
        desWidth = cellDesWidth;
      }
      // see if the cell has a style width specified
      const nsStylePosition* cellPosition = cellFrame->GetStylePosition();
      if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) {
        nscoord coordValue = cellPosition->mWidth.GetCoordValue();
        if (coordValue > 0) { // ignore if width == 0
          // need to add border and padding into fixed width
          nsSize percentBase(aReflowState.mComputedWidth, 0);
          nsMargin borderPadding = nsTableFrame::GetBorderPadding(percentBase, pixelToTwips, cellFrame);
          nscoord newFixWidth = coordValue + borderPadding.left + borderPadding.right;
          // 2nd part of condition is Nav/IE Quirk like below
          if ((newFixWidth > fixWidth) || ((newFixWidth == fixWidth) && (desContributor == cellFrame))) {
            fixWidth = newFixWidth;
            fixContributor = cellFrame;
          }
        }
      }
      if (!hasPctCol && HasPctValue(cellFrame)) { // see if there is a pct cell
        hasPctCol = PR_TRUE;
      }
    }

    // Nav/IE Quirk like above
    if (fixWidth > 0) {
      if (mIsNavQuirksMode && (desWidth > fixWidth) && (fixContributor != desContributor)) {
        fixWidth = WIDTH_NOT_SET;
        fixContributor = nsnull;
      }
    }
    desWidth = PR_MAX(desWidth, minWidth);

    // cache the computed column info
    colFrame->SetWidth(MIN_CON, minWidth);
    colFrame->SetWidth(DES_CON, desWidth);
    if (fixWidth > 0) {
      colFrame->SetWidth(FIX, fixWidth);
    }

    nsStyleCoord colStyleWidth = colFrame->GetStyleWidth();
    // determine if there is a proportional column either from html4 
    // proportional width on a col or Nav Quirks cols attr
    if (fixWidth <= 0) {
      nscoord proportion = WIDTH_NOT_SET;
      if (eStyleUnit_Proportional == colStyleWidth.GetUnit()) {
        proportion = colStyleWidth.GetIntValue();
      }
      else if (colX < numColsForColsAttr) {
        proportion = 1;
        if ((eStyleUnit_Percent == colStyleWidth.GetUnit()) &&
            (colStyleWidth.GetPercentValue() > 0.0f)) {
          proportion = WIDTH_NOT_SET;
        }
      }
      if (proportion >= 0) {
        rawPropTotal = PR_MAX(rawPropTotal, 0); // it was initialized to -1
        colFrame->SetWidth(MIN_PRO, proportion);
        nsColConstraint colConstraint = (0 == proportion) 
          ? e0ProportionConstraint : eProportionConstraint;
        rawPropTotal += proportion;
        colFrame->SetConstraint(colConstraint);
      }
    }
    if (!hasPctCol) { // see if there is a pct col
      if (eStyleUnit_Percent == colStyleWidth.GetUnit()) {
        float percent = colStyleWidth.GetPercentValue();
        if (percent > 0.0f) {
          hasPctCol = PR_TRUE;
        }
      }
    }
  }
  if (mCellSpacingTotal > 0) {
    mCellSpacingTotal += spacingX; // add last cell spacing on right
  }
//set the table col fixed width if present
  for (colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    nscoord fixColWidth = colFrame->GetWidth(FIX);
    // use the style width of a col only if the col hasn't gotten a fixed width from any cell
    if (fixColWidth <= 0) {
      nsStyleCoord colStyleWidth = colFrame->GetStyleWidth();
      if (eStyleUnit_Coord == colStyleWidth.GetUnit()) {
        fixColWidth = colStyleWidth.GetCoordValue();
        if (fixColWidth > 0) {
          colFrame->SetWidth(FIX, fixColWidth);
        }
      }
    }
  }
  PRBool* pctRequest = (hasPctCol) ? nsnull : &hasPctCol;
  ComputeNonPctColspanWidths(aReflowState, PR_FALSE, pixelToTwips, pctRequest);
  PRInt32 numEffCols = mTableFrame->GetEffectiveColCount();
  // figure the proportional widths for porportional cols
  if (rawPropTotal > 0)  {
    // find the largest combined prop size considering each prop col and
    // its desired size
    nscoord maxPropTotal = 0;
    for (colX = 0; colX < numEffCols; colX++) { 
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
      nscoord rawProp = colFrame->GetWidth(MIN_PRO);
      if (rawProp > 0) {
        nscoord desWidth = colFrame->GetDesWidth();
        nscoord propTotal = NSToCoordRound( ((float)desWidth) * ((float)rawPropTotal) / (float)rawProp );
        propTotal = nsTableFrame::RoundToPixel(propTotal, pixelToTwips);
        maxPropTotal = PR_MAX(maxPropTotal, propTotal);
      }
    }
    // set MIN_PRO widths based on the maxPropTotal
    for (colX = 0; colX < numEffCols; colX++) { 
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
      if (!colFrame) continue;
      nscoord rawProp = colFrame->GetWidth(MIN_PRO);
      if (0 == rawProp) {
        // a 0* col gets only the min width
        colFrame->SetWidth(MIN_PRO, colFrame->GetMinWidth());
      }
      else if ((rawProp > 0) && (rawPropTotal > 0)) {
        nscoord propWidth = NSToCoordRound( ((float)maxPropTotal) * ((float)rawProp) / (float)rawPropTotal ) ;
        propWidth = nsTableFrame::RoundToPixel(propWidth, pixelToTwips);
        colFrame->SetWidth(MIN_PRO, PR_MAX(propWidth, colFrame->GetMinWidth()));
      }
    }
  }

  // Set the table col width for each col to the content min. 
  for (colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    nscoord minWidth = colFrame->GetMinWidth();
    mTableFrame->SetColumnWidth(colX, minWidth);
  }

#ifdef DEBUG_TABLE_STRATEGY
  printf("AssignNonPctColWidths ex\n"); mTableFrame->Dump(PR_FALSE, PR_TRUE, PR_FALSE);
#endif
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugTimeMethod(nsTableFrame::eNonPctCols, *mTableFrame, (nsHTMLReflowState&)aReflowState, PR_FALSE);
#endif
  return hasPctCol;
}

void
BasicTableLayoutStrategy::ReduceOverSpecifiedPctCols(nscoord aExcess)
{
  nscoord numCols = mTableFrame->GetColCount();
  for (PRInt32 colX = numCols - 1; (colX >= 0) && (aExcess > 0); colX--) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    nscoord pctWidth = colFrame->GetWidth(PCT);
    nscoord reduction = 0;
    if (pctWidth > 0) {
      reduction = (aExcess > pctWidth) ? pctWidth : aExcess;
      nscoord newPctWidth = (reduction == pctWidth) ? WIDTH_NOT_SET : pctWidth - reduction;
      colFrame->SetWidth(PCT, PR_MAX(newPctWidth, colFrame->GetMinWidth()));
    }
    else {
      nscoord pctAdjWidth = colFrame->GetWidth(PCT_ADJ);
      if (pctAdjWidth > 0) {
        reduction = (aExcess > pctAdjWidth) ? pctAdjWidth : aExcess;
        nscoord newPctAdjWidth = (reduction == pctAdjWidth) ? WIDTH_NOT_SET : pctAdjWidth - reduction;
        colFrame->SetWidth(PCT_ADJ, PR_MAX(newPctAdjWidth, colFrame->GetMinWidth()));
      }
    }
    aExcess -= reduction;
  }
}

#ifdef DEBUG_TABLE_REFLOW_TIMING
nscoord WrapupAssignPctColumnWidths(nsTableFrame*            aTableFrame,
                                    const nsHTMLReflowState& aReflowState,
                                    nscoord                  aValue) 
{
  nsTableFrame::DebugTimeMethod(nsTableFrame::ePctCols, *aTableFrame, (nsHTMLReflowState&)aReflowState, PR_FALSE);
  return aValue;
}
#else
inline nscoord WrapupAssignPctColumnWidths(nsTableFrame*            aTableFrame,
                                           const nsHTMLReflowState& aReflowState,
                                           nscoord                  aValue) 
{
  return aValue;
}
#endif

nscoord 
BasicTableLayoutStrategy::CalcPctAdjTableWidth(const nsHTMLReflowState& aReflowState,
                                               nscoord                  aAvailWidthIn)
{
  NS_ASSERTION(mTableFrame->IsAutoWidth() && mTableFrame->HasPctCol(), "invalid call");

  PRInt32 numRows  = mTableFrame->GetRowCount();
  PRInt32 numCols  = mTableFrame->GetColCount(); // consider cols at end without orig cells 
  PRInt32 colX, rowX; 
  float pixelToTwips = mTableFrame->GetPresContext()->ScaledPixelsToTwips();

  // For an auto table, determine the potentially new percent adjusted width based 
  // on percent cells/cols. This probably should only be a NavQuirks thing, since
  // a percentage based cell or column on an auto table should force the column to auto
  nscoord basis = 0;                 
  float* rawPctValues = new float[numCols]; // store the raw pct values, allow for spans past the effective numCols
  if (!rawPctValues) return NS_ERROR_OUT_OF_MEMORY;
  for (colX = 0; colX < numCols; colX++) {
    rawPctValues[colX] = 0.0f;
  }

  nsMargin borderPadding = mTableFrame->GetContentAreaOffset(&aReflowState);
  nscoord availWidth = aAvailWidthIn;
  if (NS_UNCONSTRAINEDSIZE != availWidth) {
    // adjust the avail width to exclude table border, padding and cell spacing
    availWidth -= borderPadding.left + borderPadding.right + mCellSpacingTotal;
  }

  for (colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    nscoord maxColBasis = -1;
    // Scan the cells in the col 
    for (rowX = 0; rowX < numRows; rowX++) {
      PRBool originates;
      PRInt32 colSpan;
      nsTableCellFrame* cellFrame = mTableFrame->GetCellInfoAt(rowX, colX, &originates, &colSpan);
      if (!originates) continue; // skip  cells that don't originate in the col
      // see if the cell has a style percent width specified
      const nsStylePosition* cellPosition = cellFrame->GetStylePosition();
      if (eStyleUnit_Percent == cellPosition->mWidth.GetUnit()) {
        float percent = cellPosition->mWidth.GetPercentValue();
        if (percent > 0.0f) {
          // calculate the preferred width of the cell based on fix, des, widths of the cols it spans
          nscoord cellDesWidth  = 0;
          float spanPct = percent / float(colSpan);
          for (PRInt32 spanX = 0; spanX < colSpan; spanX++) {
            nsTableColFrame* spanFrame = mTableFrame->GetColFrame(colX + spanX);
            if (!spanFrame) continue;
            cellDesWidth += spanFrame->GetWidth(DES_CON); // don't consider DES_ADJ
            // crudely allocate pct values to the spanning cols so that we can check if they exceed 100 pct below
            rawPctValues[colX + spanX] = PR_MAX(rawPctValues[colX + spanX], spanPct);
          }
          // consider the cell's preferred width 
          cellDesWidth = PR_MAX(cellDesWidth, cellFrame->GetMaximumWidth());
          nscoord colBasis = nsTableFrame::RoundToPixel(NSToCoordRound((float)cellDesWidth / percent), pixelToTwips);
          maxColBasis = PR_MAX(maxColBasis, colBasis);
        }
      }
    }
    if (-1 == maxColBasis) {
      // see if the col has a style percent width specified
      nsStyleCoord colStyleWidth = colFrame->GetStyleWidth();
      if (eStyleUnit_Percent == colStyleWidth.GetUnit()) {
        float percent = colStyleWidth.GetPercentValue();
        maxColBasis = 0;
        if (percent > 0.0f) {
          rawPctValues[colX] = PR_MAX(rawPctValues[colX], percent);
          nscoord desWidth = colFrame->GetWidth(DES_CON); // don't consider DES_ADJ
          maxColBasis = nsTableFrame::RoundToPixel(NSToCoordRound((float)desWidth / percent), pixelToTwips);
        }
      }
    }
    basis = PR_MAX(basis, maxColBasis);
  } // end for (colX ..

  float   perTotal         = 0.0f; // total of percentage constrained cols and/or cells in cols
  PRInt32 numPerCols       = 0;    // number of colums that have percentage constraints
  nscoord fixDesTotal      = 0;    // total of fix or des widths of cols 
  nscoord fixDesTotalNoPct = 0;    // total of fix or des widths of cols without pct

  for (colX = 0; colX < numCols; colX++) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    nscoord fixWidth = colFrame->GetFixWidth();
    nscoord fixDesWidth = (fixWidth > 0) ? fixWidth : colFrame->GetDesWidth();
    fixDesTotal += fixDesWidth;
    if (rawPctValues[colX] + perTotal > 1.0f) {
      rawPctValues[colX] = PR_MAX(1.0f - perTotal, 0.0f);
    }
    if (rawPctValues[colX] > 0.0f) {
      numPerCols++;
      perTotal += rawPctValues[colX];
    }
    else {
      fixDesTotalNoPct += fixDesWidth;
    }
  }
  delete [] rawPctValues; // destroy the raw pct values
  // If there are no pct cells or cols, there is nothing to do.
  if ((0 == numPerCols) || (0.0f == perTotal)) {
    NS_ASSERTION(PR_FALSE, "invalid call");
    return basis; 
  }
  // If there is only one col and it is % based, it won't affect anything
  if ((1 == numCols) && (numCols == numPerCols)) {
    return basis + borderPadding.left + borderPadding.right + mCellSpacingTotal;
  }

  // compute a basis considering total percentages and the desired width of everything else
  if ((perTotal > 0.0f) && (perTotal < 1.0f)) {
    nscoord otherBasis = nsTableFrame::RoundToPixel(NSToCoordRound((float)fixDesTotalNoPct / (1.0f - perTotal)), pixelToTwips);
    basis = PR_MAX(basis, otherBasis);
  }
  else if ((fixDesTotalNoPct > 0) && (NS_UNCONSTRAINEDSIZE != availWidth)) { // make the basis as big as possible 
    basis = availWidth; // the 100% cols force as big a width as possible
  }
  basis = PR_MAX(basis, fixDesTotal);
  basis = PR_MIN(basis, availWidth); // don't exceed the max we were given

  if (NS_UNCONSTRAINEDSIZE != availWidth) {
    // add back the table border, padding and cell spacing
    basis += borderPadding.left + borderPadding.right + mCellSpacingTotal;
  }

  return basis;
}

// Determine percentage col widths for each col frame
nscoord 
BasicTableLayoutStrategy::AssignPctColumnWidths(const nsHTMLReflowState& aReflowState,
                                                nscoord                  aAvailWidth,
                                                PRBool                   aTableIsAutoWidth,
                                                float                    aPixelToTwips)
{
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugTimeMethod(nsTableFrame::ePctCols, *mTableFrame, (nsHTMLReflowState&)aReflowState, PR_TRUE);
#endif
  mTableFrame->SetHasCellSpanningPctCol(PR_FALSE); // this gets refigured below
  PRInt32 numRows = mTableFrame->GetRowCount();
  PRInt32 numCols = mTableFrame->GetColCount(); // consider cols at end without orig cells 
  PRInt32 numEffCols = mTableFrame->GetEffectiveColCount();
  nscoord spacingX = mTableFrame->GetCellSpacingX();
  PRInt32 colX, rowX; 

  NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aAvailWidth, "AssignPctColumnWidths has unconstrained avail width");  
  // For an auto table, determine the potentially new percent adjusted width based 
  // on percent cells/cols. This probably should only be a NavQuirks thing, since
  // a percentage based cell or column on an auto table should force the column to auto
  nscoord basis = (aTableIsAutoWidth) 
                  ? CalcPctAdjTableWidth(aReflowState, aAvailWidth)
                  : aAvailWidth;

  // adjust the basis to exclude table border, padding and cell spacing
  nsMargin borderPadding = mTableFrame->GetContentAreaOffset(&aReflowState);
  basis -= borderPadding.left + borderPadding.right + mCellSpacingTotal;

  nscoord colPctTotal = 0;

  struct nsSpannedEle {
    nsSpannedEle *next, *prev;
    PRInt32 col, colSpan;
    nsTableCellFrame *cellFrame;
  };

  nsSpannedEle *spanList = 0;
  nsSpannedEle *spanTail = 0;
  PLArenaPool   spanArena;
  
  PL_INIT_ARENA_POOL(&spanArena, "AssignPctColumnWidths", 512);
  // Determine the percentage contribution for cols and for cells with colspan = 1
  // Iterate backwards, similarly to the reasoning in AssignNonPctColumnWidths
  for (colX = numCols - 1; colX >= 0; colX--) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    nscoord maxColPctWidth = WIDTH_NOT_SET;
    float maxColPct = 0.0f;

    nsTableCellFrame* percentContributor = nsnull;
    // Scan the cells in the col that have colspan = 1; assign PER widths
    for (rowX = 0; rowX < numRows; rowX++) {
      PRBool originates;
      PRInt32 colSpan;
      nsTableCellFrame* cellFrame = mTableFrame->GetCellInfoAt(rowX, colX, &originates, &colSpan);
      // skip cells that don't originate at (rowX, colX)
      if (!cellFrame || !originates) { 
        continue;
      }

      // colspans are handled in the next pass, but we record them here so we dont have to
      // search for the again.  row spans do not need to be handled.
      if(colSpan>1) {
        nsSpannedEle *spanned;
        ARENA_ALLOCATE(spanned, &spanArena, 1, nsSpannedEle);
        if(spanned) {
          spanned->col = colX;
          spanned->colSpan = colSpan;
          spanned->cellFrame = cellFrame;
          spanned->next = spanList;
          spanned->prev = nsnull;
          if(spanList) {
            spanList->prev = spanned;
          } else {
            spanTail = spanned;
          }
          spanList = spanned;
        }
        continue;
      }
      // see if the cell has a style percent width specified
      const nsStylePosition* cellPosition = cellFrame->GetStylePosition();
      if (eStyleUnit_Percent == cellPosition->mWidth.GetUnit()) {
        float percent = cellPosition->mWidth.GetPercentValue();
        if (percent > maxColPct) {
          maxColPct = percent;
          maxColPctWidth = nsTableFrame::RoundToPixel(NSToCoordRound( ((float)basis) * maxColPct ), aPixelToTwips);
          percentContributor = cellFrame;
          if (!mIsNavQuirksMode) { 
            // need to add border and padding
            nsMargin borderPadding = nsTableFrame::GetBorderPadding(nsSize(basis, 0), aPixelToTwips, cellFrame);
            maxColPctWidth += borderPadding.left + borderPadding.right;
          }
        }
      }
    }
    if (WIDTH_NOT_SET == maxColPctWidth) {
      // see if the col has a style percent width specified
      nsStyleCoord colStyleWidth = colFrame->GetStyleWidth();
      if (eStyleUnit_Percent == colStyleWidth.GetUnit()) {
        maxColPct = colStyleWidth.GetPercentValue();
        maxColPctWidth = nsTableFrame::RoundToPixel(NSToCoordRound( ((float)basis) * maxColPct ), aPixelToTwips);
      }
    }
    // conflicting pct/fixed widths are recorded. Nav 4.x may be changing the
    // fixed width value if it exceeds the pct value and not recording the pct
    // value. This is not being done and IE5 doesn't do it either.
    if (maxColPctWidth > 0) {
      maxColPctWidth = PR_MAX(maxColPctWidth, colFrame->GetWidth(MIN_CON));
      colFrame->SetWidth(PCT, maxColPctWidth);
      colPctTotal += NSToCoordRound(100.0f * (float)maxColPct);
    }
  }

  // if the percent total went over 100%, adjustments need to be made to right most cols
  if (colPctTotal > 100) {
    ReduceOverSpecifiedPctCols(nsTableFrame::RoundToPixel(NSToCoordRound(((float)(colPctTotal - 100)) * 0.01f * (float)basis), aPixelToTwips));
    colPctTotal = 100;
  }

  // check to see if a cell spans a percentage col. This will cause the MIN_ADJ,
  // FIX_ADJ, and DES_ADJ values to be recomputed 
  PRBool done = PR_FALSE;
  nsSpannedEle *spannedCell;
  for(spannedCell=spanList; !done && spannedCell; spannedCell=spannedCell->next)
  {
    colX = spannedCell->col;
    PRInt32 colSpan = spannedCell->colSpan;
    // determine if the cell spans cols which have a pct value
    for (PRInt32 spanX = 0; spanX < colSpan; spanX++) {
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX + spanX); 
      if (!colFrame) continue;
      if (colFrame->GetWidth(PCT) > 0) {
        mTableFrame->SetHasCellSpanningPctCol(PR_TRUE);
        // recompute the MIN_ADJ, FIX_ADJ, and DES_ADJ values
        ComputeNonPctColspanWidths(aReflowState, PR_TRUE, aPixelToTwips, nsnull);
        done = PR_TRUE;
        break;
      }
    }
  }

  // For each col, consider the cells originating in it with colspans > 1.
  // Adjust the cols that each cell spans if necessary.
  // if more than one  colspan originate in one column, resort the access to 
  // the rows so that the inner colspans are handled first

  CellInfo* cellInfo = nsnull;

  if(spanTail)
    ARENA_ALLOCATE(cellInfo, &spanArena, numRows, CellInfo);

  if(cellInfo) for(spannedCell=spanTail; spannedCell;) {
    PRInt32 spannedRows = 0;
    PRInt32 colX = spannedCell->col;
    do {
      cellInfo[spannedRows].cellFrame = spannedCell->cellFrame;
      cellInfo[spannedRows].colSpan   = spannedCell->colSpan;
      ++spannedRows;
    } while((spannedCell=spannedCell->prev) && (spannedCell->col==colX));
    if(spannedRows>1) {
      NS_QuickSort(cellInfo, spannedRows, sizeof(cellInfo[0]), RowSortCB, 0);
    }

    for (PRInt32 i = 0; i < spannedRows; i++) {
      const CellInfo *inf = &cellInfo[i];
      const nsTableCellFrame* cellFrame = inf->cellFrame;

      const PRInt32 colSpan = PR_MIN(inf->colSpan, numEffCols - colX);
      nscoord cellPctWidth = WIDTH_NOT_SET;
      // see if the cell has a style percentage width specified
      const nsStylePosition* cellPosition = cellFrame->GetStylePosition();
      float cellPct = 0.0f;
      if (eStyleUnit_Percent == cellPosition->mWidth.GetUnit()) {
        cellPct = cellPosition->mWidth.GetPercentValue();
        if (colSpan == numEffCols)
          cellPct = 1.0f; // overwrite spurious percent colspan width's - bug 46944
        cellPctWidth = nsTableFrame::RoundToPixel(NSToCoordRound( ((float)basis) * cellPct ), aPixelToTwips);
        if (!mIsNavQuirksMode) { 
          // need to add border and padding 
          nsMargin borderPadding = nsTableFrame::GetBorderPadding(nsSize(basis, 0), aPixelToTwips, cellFrame);
          cellPctWidth += borderPadding.left + borderPadding.right;
        }
      }
      if (cellPctWidth > 0) {
        nscoord spanCellSpacing = 0;
        nscoord spanTotal = 0;
        nscoord spanTotalNoPctAdj = 0;
        nscoord colPctWidthTotal = 0; // accumulate the PCT width 
        nscoord colPctAdjTotal = 0;   // accumulate the PCT_ADJ width or  the max of PCT_ADJ and PCT width if a 
                                      // PCT width is specified
        PRBool canSkipPctAdj = PR_FALSE;
        // accumulate the spanTotal as the max of MIN, DES, FIX, PCT
        PRInt32 spanX;
        for (spanX = 0; spanX < colSpan; spanX++) {
          nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX + spanX); 
          if (!colFrame) continue;
          nscoord colPctWidth = colFrame->GetWidth(PCT);
          if (colPctWidth > 0) { // skip pct cols
            colPctWidthTotal += colPctWidth;
            colPctAdjTotal += PR_MAX(colFrame->GetWidth(PCT_ADJ), colPctWidth);
            continue;
          }
          colPctAdjTotal += PR_MAX(colFrame->GetWidth(PCT_ADJ), 0);
          nscoord colWidth = PR_MAX(colFrame->GetMinWidth(), colFrame->GetFixWidth());
          colWidth = PR_MAX(colWidth, colFrame->GetDesWidth()); // XXX check this
          //colWidth = PR_MAX(colWidth, colFrame->GetPctWidth());
          spanTotal += colWidth;
          if (colFrame->GetWidth(PCT_ADJ) <= 0) { // if we have a cell that is neither PCT or PCT_ADJ we have
                                                  // other places where we can drop the width
            canSkipPctAdj = PR_TRUE; 
            spanTotalNoPctAdj += colWidth;
          }
          if ((spanX > 0) && (mTableFrame->GetNumCellsOriginatingInCol(colX + spanX) > 0)) {
            spanCellSpacing += spacingX;
          }
        }
        cellPctWidth += spanCellSpacing; // add it back in since it was subtracted from aBasisIn
        if (cellPctWidth <= 0) {
          continue;
        }
        spanTotal = canSkipPctAdj ? spanTotalNoPctAdj: spanTotal; // if we can skip the PCT_ADJ ignore theire width
      
        colPctWidthTotal = canSkipPctAdj ? colPctAdjTotal: colPctWidthTotal; 
        
        if ((PR_MAX(colPctWidthTotal, colPctAdjTotal)+ spanCellSpacing) < cellPctWidth) { 
          // we have something to distribute ...
          // record the percent contributions for the spanned cols
          PRInt32 usedColumns = colSpan;
          for (spanX = colSpan-1; spanX >= 0; spanX--) {
            nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX + spanX); 
            if (!colFrame) continue;
            if ((colFrame->GetWidth(PCT) > 0) || (canSkipPctAdj && (colFrame->GetWidth(PCT_ADJ) > 0))) {
              // dont use pct cols or if we can skip the pct adj event do not take the PCT_ADJ cols
              usedColumns--;
              continue;
            } // count the pieces 
            if (usedColumns == 0)
              usedColumns = 1; // avoid division by 0 later
            nscoord minWidth = colFrame->GetMinWidth();
            nscoord colWidth = PR_MAX(minWidth, colFrame->GetFixWidth());
            colWidth = PR_MAX(colWidth, colFrame->GetDesWidth()); // XXX check this
            float avail = (float)PR_MAX(cellPctWidth - colPctWidthTotal, 0);
            float colPctAdj = (0 == spanTotal) 
                              ? avail / ((float) usedColumns) / ((float)basis)
                              : (avail / (float)basis) * (((float)colWidth) / (float)spanTotal);
            if (colPctAdj > 0) {
              nscoord colPctAdjWidth = colFrame->GetWidth(PCT_ADJ);
              nscoord newColPctAdjWidth = nsTableFrame::RoundToPixel(NSToCoordRound(colPctAdj * (float)basis), aPixelToTwips);
              if (newColPctAdjWidth > colPctAdjWidth) {
                newColPctAdjWidth = PR_MAX(newColPctAdjWidth, minWidth); 
                if (newColPctAdjWidth > colFrame->GetWidth(PCT)) {
                  colFrame->SetWidth(PCT_ADJ, newColPctAdjWidth);
                  if(0 != basis) { // I am paranoid
                    colPctTotal += NSToCoordRound(100.0f *(newColPctAdjWidth-colPctAdjWidth) / (float)basis); 
                    // accumulate the new distributed percents
                  }
                }
              }
              else {
                usedColumns--;
                colPctWidthTotal += colPctAdjWidth; // accumulate the already distributed percents
              }
            }
          }
        }
      }
    } // end for (index ..
  } // end for (colX ..

  // We are done with our spanlist, get rid of it now
  PL_FinishArenaPool(&spanArena);

  // if the percent total went over 100%, adjustments need to be made to right most cols
  if (colPctTotal > 100) {
    ReduceOverSpecifiedPctCols(nsTableFrame::RoundToPixel(NSToCoordRound(((float)(colPctTotal - 100)) * 0.01f * (float)basis), aPixelToTwips));
  }

  // adjust the basis to include table border, padding and cell spacing
  basis += borderPadding.left + borderPadding.right + mCellSpacingTotal;
  return WrapupAssignPctColumnWidths(mTableFrame, aReflowState, basis); 
}


// calculate totals by width type. The logic here is kept in synch with 
// that in CanAllocate. aDupedWidths (duplicatd) are widths that will be 
// allocated in BalanceColumnWidths before aTotalsWidths (e.g. aTotalWidths[PCT] 
// will have aDuplicatedWidths[PCT] consisting of the MIN widths of cols which
// have a PCT width).
void BasicTableLayoutStrategy::CalculateTotals(PRInt32* aTotalCounts,
                                               PRInt32* aTotalWidths,
                                               PRInt32* aDupedWidths, 
                                               PRInt32& a0ProportionalCount)
{
  //mTableFrame->Dump(PR_TRUE, PR_FALSE);
  for (PRInt32 widthX = 0; widthX < NUM_WIDTHS; widthX++) {
    aTotalCounts[widthX]      = 0;
    aTotalWidths[widthX]      = 0;
    aDupedWidths[widthX]      = 0;
  }
  a0ProportionalCount = 0;

  PRInt32 numEffCols = mTableFrame->GetEffectiveColCount();
  PRInt32 colX;

  for (colX = 0; colX < numEffCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    nscoord minCol = colFrame->GetMinWidth();
    aTotalCounts[MIN_CON]++;
    aTotalWidths[MIN_CON] += minCol;

    if (e0ProportionConstraint == colFrame->GetConstraint()) {
      a0ProportionalCount++;
    }

    nscoord pct    = colFrame->GetPctWidth();
    nscoord fix    = colFrame->GetWidth(FIX);
    nscoord fixAdj = colFrame->GetWidth(FIX_ADJ);
    // if there is a pct width then no others are considered
    if (pct > 0) {
      aTotalCounts[PCT]++;
      aTotalWidths[PCT] += PR_MAX(pct, minCol);
      aDupedWidths[PCT] += minCol;
    }
    else if ((fix > 0) || (fixAdj > 0)) {
      if (fix > 0) {
        aTotalCounts[FIX]++;
        aTotalWidths[FIX] += PR_MAX(fix, minCol);
        aDupedWidths[FIX] += minCol;
      }
      if (fixAdj > 0) {
        if (fixAdj > fix) {
          aTotalCounts[FIX_ADJ]++;
          aTotalWidths[FIX_ADJ] += PR_MAX(fixAdj, minCol);
          if (fix > 0) {
            aDupedWidths[FIX_ADJ] += fix;
          }
          else { // there was no fix
            aDupedWidths[FIX_ADJ] += minCol;
          }
        }
      }
    }
    else if ((eProportionConstraint == colFrame->GetConstraint()) ||
             (e0ProportionConstraint == colFrame->GetConstraint())) {
      aTotalCounts[MIN_PRO]++;
      aTotalWidths[MIN_PRO] += PR_MAX(colFrame->GetWidth(MIN_PRO), minCol);
      aDupedWidths[MIN_PRO] += minCol;
    }
    else {
      // desired alone is lowest priority
      aTotalCounts[DES_CON]++;
      aTotalWidths[DES_CON] += PR_MAX(colFrame->GetDesWidth(), minCol);
      aDupedWidths[DES_CON] += minCol;
    }
  }
}


struct ColInfo {
  ColInfo(nsTableColFrame* aFrame,
          PRInt32          aIndex,
          PRInt32          aMinWidth,
          PRInt32          aWidth,
          PRInt32          aMaxWidth)
    : mFrame(aFrame), mIndex(aIndex), mMinWidth(aMinWidth), 
      mWidth(aWidth), mMaxWidth(aMaxWidth), mWeight(0)
  {}
  nsTableColFrame* mFrame;
  PRInt32          mIndex;
  PRInt32          mMinWidth;
  PRInt32          mWidth;
  PRInt32          mMaxWidth;
  float            mWeight;
};

void
AC_Wrapup(nsTableFrame* aTableFrame,
          PRInt32       aNumItems, 
          ColInfo**     aColInfo,
          PRBool        aAbort = PR_FALSE)
{
  if (aColInfo) {
    for (PRInt32 i = 0; i < aNumItems; i++) {
      if (aColInfo[i]) {
        if (!aAbort) {
          aTableFrame->SetColumnWidth(aColInfo[i]->mIndex, aColInfo[i]->mWidth);
        }
        delete aColInfo[i];
      }
    }
    delete [] aColInfo;
  }
}

void
AC_Increase(PRInt32     aNumAutoCols,
            ColInfo**   aColInfo,
            PRInt32     aDivisor,
            PRInt32&    aAvailWidth,
            float       aPixelToTwips)
{
  for (PRInt32 i = 0; i < aNumAutoCols; i++) {
    if ((aAvailWidth <= 0) || (aDivisor <= 0)) {
      break;
    }
    // aDivisor represents the sum of unallocated space (diff between max and min values)
    float percent = ((float)aColInfo[i]->mMaxWidth - (float)aColInfo[i]->mMinWidth) / (float)aDivisor;
    aDivisor -= aColInfo[i]->mMaxWidth - aColInfo[i]->mMinWidth;

    nscoord addition = nsTableFrame::RoundToPixel(NSToCoordRound(((float)(aAvailWidth)) * percent), aPixelToTwips);
    // if its the last col, try to give what's left to it
    if ((i == aNumAutoCols - 1) && (addition < aAvailWidth)) {
      addition = aAvailWidth;
    }
    // don't let the addition exceed what is available to add
    addition = PR_MIN(addition, aAvailWidth);
    // don't go over the col max
    addition = PR_MIN(addition, aColInfo[i]->mMaxWidth - aColInfo[i]->mWidth);
    aColInfo[i]->mWidth += addition;
    aAvailWidth -= addition;
  }
}

void
AC_Decrease(PRInt32     aNumAutoCols,
            ColInfo**   aColInfo,
            PRInt32     aDivisor,
            PRInt32&    aExcess,
            float       aPixelToTwips)
{
  for (PRInt32 i = 0; i < aNumAutoCols; i++) {
    if ((aExcess <= 0) || (aDivisor <= 0)) {
      break;
    }
    float percent = ((float)aColInfo[i]->mMaxWidth) / (float)aDivisor;
    aDivisor -= aColInfo[i]->mMaxWidth;
    nscoord reduction = nsTableFrame::RoundToPixel(NSToCoordRound(((float)(aExcess)) * percent), aPixelToTwips);
    // if its the last col, try to remove the remaining excess from it
    if ((i == aNumAutoCols - 1) && (reduction < aExcess)) {
      reduction = aExcess;
    }
    // don't let the reduction exceed what is available to reduce
    reduction = PR_MIN(reduction, aExcess);
    // don't go under the col min
    reduction = PR_MIN(reduction, aColInfo[i]->mWidth - aColInfo[i]->mMinWidth);
    aColInfo[i]->mWidth -= reduction;
    aExcess -= reduction;
  }
}

void 
AC_Sort(ColInfo** aColInfo, PRInt32 aNumCols)
{
  // sort the cols based on the Weight 
  for (PRInt32 j = aNumCols - 1; j > 0; j--) {
    for (PRInt32 i = 0; i < j; i++) { 
      if (aColInfo[i]->mWeight < aColInfo[i+1]->mWeight) { // swap them
        ColInfo* save = aColInfo[i];
        aColInfo[i]   = aColInfo[i+1];
        aColInfo[i+1] = save;
      }
    }
  }
}

// this assumes that the table has set the width for each col to be its min
void BasicTableLayoutStrategy::AllocateConstrained(PRInt32  aAvailWidth,
                                                   PRInt32  aWidthType,
                                                   PRBool   aStartAtMin,        
                                                   PRInt32* aAllocTypes,
                                                   float    aPixelToTwips)
{
  if ((0 == aAvailWidth) || (aWidthType < 0) || (aWidthType >= NUM_WIDTHS)) {
    NS_ASSERTION(PR_TRUE, "invalid args to AllocateConstrained");
    return;
  }

  PRInt32 numCols = mTableFrame->GetColCount();
  PRInt32 numConstrainedCols = 0;
  nscoord sumMaxConstraints  = 0;
  nscoord sumMinConstraints  = 0;
  PRInt32 colX;
  // find out how many constrained cols there are
  for (colX = 0; colX < numCols; colX++) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    if (!CanAllocate(aWidthType, aAllocTypes[colX], colFrame)) {
      continue;
    }
    numConstrainedCols++;
  }

  // allocate storage for the constrained cols. Only they get adjusted.
  ColInfo** colInfo = new ColInfo*[numConstrainedCols];
  if (!colInfo) return;
  memset(colInfo, 0, numConstrainedCols * sizeof(ColInfo *));

  PRInt32 maxMinDiff = 0;
  PRInt32 constrColX = 0;
  // set the col info entries for each constrained col
  for (colX = 0; colX < numCols; colX++) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX); 
    if (!colFrame) continue;
    if (!CanAllocate(aWidthType, aAllocTypes[colX], colFrame)) {
      continue;
    }
    nscoord minWidth = mTableFrame->GetColumnWidth(colX);
    nscoord maxWidth = GetColWidth(colFrame, aWidthType);
    // proportional and desired widths are handled together
    PRBool haveProWidth = PR_FALSE;
    if (DES_CON == aWidthType) {
      // Take into acount MIN_adj width as it has been included in totals before
      maxWidth = PR_MAX(maxWidth, colFrame->GetWidth(MIN_ADJ));
      nscoord proWidth = colFrame->GetWidth(MIN_PRO);
      if (proWidth >= 0) {
        haveProWidth = PR_TRUE;
        maxWidth = proWidth;
      }
    }

    if (maxWidth <= 0) continue;

    sumMaxConstraints += maxWidth;
    sumMinConstraints += minWidth;

    maxWidth = PR_MAX(maxWidth, minWidth);
    maxMinDiff += maxWidth - minWidth;
    nscoord startWidth = (aStartAtMin) ? minWidth : maxWidth;
    colInfo[constrColX] = new ColInfo(colFrame, colX, minWidth, startWidth, maxWidth);
    if (!colInfo[constrColX]) {
      AC_Wrapup(mTableFrame, numConstrainedCols, colInfo, PR_TRUE);
      return;
    }
    aAllocTypes[colX] = (haveProWidth) ? MIN_PRO : aWidthType;
    constrColX++;
  }

  if (constrColX < numConstrainedCols) {
    // some of the constrainted cols might have been 0 and skipped
    numConstrainedCols = constrColX;
  }

  PRInt32 i;
  if (aStartAtMin) { // allocate extra space 
    nscoord availWidth = aAvailWidth; 
    for (i = 0; i < numConstrainedCols; i++) {
      // the weight here is a relative metric for determining when cols reach their max constraint. 
      // A col with a larger weight will reach its max before one with a smaller value.
      nscoord delta = colInfo[i]->mMaxWidth - colInfo[i]->mWidth;
      colInfo[i]->mWeight = (delta <= 0) 
        ? 1000000 // cols which have already reached their max get a large value
        : ((float)colInfo[i]->mMaxWidth) / ((float)delta);
    }
      
    // sort the cols based on the weight so that in one pass cols with higher 
    // weights will get their max earlier than ones with lower weights
    // This is an innefficient bubble sort, but unless there are an unlikely 
    // large number of cols, it is not an issue.
    AC_Sort(colInfo, numConstrainedCols);
   
    // compute the proportion to be added to each column, don't go beyond the col's
    // max. This algorithm assumes that the Weight works as stated above
    AC_Increase(numConstrainedCols, colInfo, sumMaxConstraints - sumMinConstraints, 
                availWidth, aPixelToTwips);
  }
  else { // reduce each col width 
    nscoord reduceWidth = maxMinDiff - aAvailWidth;
    if (reduceWidth < 0) {
      NS_ASSERTION(PR_TRUE, "AllocateConstrained called incorrectly");
      AC_Wrapup(mTableFrame, numConstrainedCols, colInfo);
      return;
    }
    for (i = 0; i < numConstrainedCols; i++) {
      // the weight here is a relative metric for determining when cols reach their min. 
      // A col with a larger weight will reach its min before one with a smaller value.
      nscoord delta = colInfo[i]->mWidth - colInfo[i]->mMinWidth;
      colInfo[i]->mWeight = (delta <= 0) 
        ? 1000000 // cols which have already reached their min get a large value
        : ((float)colInfo[i]->mWidth) / ((float)delta);
    }
      
    // sort the cols based on the Weight 
    AC_Sort(colInfo, numConstrainedCols);
   
    // compute the proportion to be subtracted from each column, don't go beyond 
    // the col's min. This algorithm assumes that the Weight works as stated above
    AC_Decrease(numConstrainedCols, colInfo, sumMaxConstraints, reduceWidth, aPixelToTwips);
  }
  AC_Wrapup(mTableFrame, numConstrainedCols, colInfo);
}

#ifdef DEBUG_TABLE_STRATEGY
void BasicTableLayoutStrategy::Dump(PRInt32 aIndent)
{
  char* indent = new char[aIndent + 1];
  if (!indent) return;
  for (PRInt32 i = 0; i < aIndent + 1; i++) {
    indent[i] = ' ';
  }
  indent[aIndent] = 0;

  printf("%s**START BASIC STRATEGY DUMP** table=%p cols=%X",
         indent, mTableFrame, mCols);
  printf("\n%s cellSpacing=%d propRatio=%.2f navQuirks=%d",
    indent, mCellSpacingTotal, mMinToDesProportionRatio, mIsNavQuirksMode);
  printf(" **END BASIC STRATEGY DUMP** \n");
  delete [] indent;
}
#endif
