/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "BasicTableLayoutStrategy.h"
#include "nsIPresContext.h"
#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsVoidArray.h"
#include "nsHTMLIIDs.h"

#if 1
static PRBool gsDebugAssign  = PR_FALSE;
static PRBool gsDebugBalance = PR_FALSE;
#else
static PRBool gsDebugAssign  = PR_TRUE;
static PRBool gsDebugBalance = PR_TRUE;
#endif
static PRInt32 gsDebugCount = 0;

// The priority of allocations for columns is as follows
//   1) max(MIN, MIN_ADJ)
//   2) max (PCT, PCT_ADJ) 
//   3) FIX 
//   4) FIX_ADJ
//   5) PROportional 
//   6) max(DES_CON, DES_ADJ)
//   7) for a fixed width table, the column may get more 
//      space if the sum of the col allocations is insufficient 


// the logic here is kept in synch with that in CalculateTotals.
PRBool CanAllocate(PRInt32          aTypeToAllocate,
                   PRInt32          aTypeAlreadyAllocated,
                   nsTableColFrame* aColFrame,
                   PRBool           aIsNavQuirks,
                   PRBool&          aConsiderAdj)
{
  aConsiderAdj = PR_TRUE;
  if ((aTypeToAllocate == FIX_ADJ) && (aTypeAlreadyAllocated == FIX)) {
    return PR_TRUE;
  }
  if (aIsNavQuirks && (aTypeToAllocate == DES_CON) && (aTypeAlreadyAllocated == FIX_ADJ)) {
    aConsiderAdj = PR_FALSE;
    if (aColFrame->GetWidth(FIX) <= 0) { // there is no FIX width
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

/* ---------- BasicTableLayoutStrategy ---------- */

MOZ_DECL_CTOR_COUNTER(BasicTableLayoutStrategy);


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

PRBool BasicTableLayoutStrategy::Initialize(nsIPresContext*          aPresContext,
                                            nsSize*                  aMaxElementSize,
                                            nscoord                  aMaxWidth,
                                            const nsHTMLReflowState& aReflowState)
{
  ContinuingFrameCheck();

  PRBool result = PR_TRUE;

  // re-init instance variables
  mCellSpacingTotal     = 0;
  mCols                 = mTableFrame->GetEffectiveCOLSAttribute();
  // assign the width of all fixed-width columns
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  AssignNonPctColumnWidths(aPresContext, aMaxWidth, aReflowState, p2t);

  // set aMaxElementSize here because we compute mMinTableWidth in AssignNonPctColumnWidths
  if (nsnull != aMaxElementSize) {
    SetMaxElementSize(aMaxElementSize, aReflowState.mComputedPadding);
  }

  return result;
}

void 
BasicTableLayoutStrategy::SetMaxElementSize(nsSize*         aMaxElementSize,
                                            const nsMargin& aPadding)
{
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->height = 0;
    nsMargin borderPadding;
    const nsStylePosition* tablePosition;
    mTableFrame->GetStyleData(eStyleStruct_Position, ((const nsStyleStruct *&)tablePosition));
    mTableFrame->GetTableBorder(borderPadding);
    borderPadding += aPadding;
    nscoord horBorderPadding = borderPadding.left + borderPadding.right;
    nscoord minTableWidth = GetTableMinWidth();
    if (tablePosition->mWidth.GetUnit() == eStyleUnit_Coord) {
      aMaxElementSize->width = tablePosition->mWidth.GetCoordValue();
      if (minTableWidth + horBorderPadding > aMaxElementSize->width) {
        aMaxElementSize->width = minTableWidth + horBorderPadding;
      }
    }   
    else {
      aMaxElementSize->width = minTableWidth + horBorderPadding;
    }
  }
}

void BasicTableLayoutStrategy::ContinuingFrameCheck()
{
#ifdef NS_DEBUG
  nsIFrame* tablePIF = nsnull;
  mTableFrame->GetPrevInFlow(&tablePIF);
  NS_ASSERTION(!tablePIF, "never ever call me on a continuing frame!");
#endif
}

PRBool BCW_Wrapup(nsIPresContext*           aPresContext,
                  BasicTableLayoutStrategy* aStrategy, 
                  nsTableFrame*             aTableFrame, 
                  PRInt32*                  aAllocTypes)
{
  if (aAllocTypes)
    delete [] aAllocTypes;
  if (gsDebugBalance) {printf("BalanceColumnWidths ex \n"); aTableFrame->Dump(aPresContext, PR_FALSE, PR_TRUE, PR_FALSE);}
  return PR_TRUE;
}

PRBool 
BasicTableLayoutStrategy::BalanceColumnWidths(nsIPresContext*          aPresContext,
                                              nsIStyleContext*         aTableStyle,
                                              const nsHTMLReflowState& aReflowState,
                                              nscoord                  aMaxWidthIn)
{
  if (gsDebugBalance) {printf("BalanceColumnWidths en max=%d count=%d \n", aMaxWidthIn, gsDebugCount++); mTableFrame->Dump(aPresContext, PR_FALSE, PR_TRUE, PR_FALSE);}
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);

  ContinuingFrameCheck();
  if (!aTableStyle) {
    NS_ASSERTION(aTableStyle, "bad style arg");
    return PR_FALSE;
  }

  PRInt32 numCols = mTableFrame->GetColCount();

  // determine if the table is auto/fixed and get the fixed width if available
  nscoord maxWidth = aMaxWidthIn; 
  nscoord specifiedTableWidth = mTableFrame->CalcBorderBoxWidth(aReflowState);
  PRBool tableIsAutoWidth = mTableFrame->IsAutoWidth();
  // a specifiedTableWidth of <= 0 indicates percentage based 
  if (!tableIsAutoWidth && (specifiedTableWidth > 0)) {
    maxWidth = PR_MIN(specifiedTableWidth, aMaxWidthIn); // specifiedWidth usually == aMaxWidthIn for fixed table
  }
  // reduce the maxWidth by border and padding, since we will be dealing with content width
  // XXX should this be done in aMaxWidthIn by the caller?
  if (maxWidth != NS_UNCONSTRAINEDSIZE) {
    maxWidth -= aReflowState.mComputedBorderPadding.left + 
                aReflowState.mComputedBorderPadding.right;
    maxWidth = PR_MAX(0, maxWidth);
  }
  // initialize the col percent and cell percent values to 0.
  PRInt32 colX;
  for (colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    colFrame->SetWidth(PCT, WIDTH_NOT_SET);
    colFrame->SetWidth(PCT_ADJ, WIDTH_NOT_SET);
  }
  // set PCT and PCT_ADJ widths on col frames. An auto table returns 
  // a new table width based on percent cells/cols if they exist
  nscoord perAdjTableWidth = 0;
  if ((NS_UNCONSTRAINEDSIZE != maxWidth) || (tableIsAutoWidth)) {
    // for an auto width table, use a large basis just so that the quirky
    // auto table sizing will get as big as it should
    nscoord basis = (NS_UNCONSTRAINEDSIZE == maxWidth) 
                    ? NS_UNCONSTRAINEDSIZE : maxWidth - mCellSpacingTotal;
    // this may have to reallocate MIN_ADJ, FIX_ADJ, DES_ADJ if there are
    // cells spanning cols which have PCT values
    perAdjTableWidth = AssignPctColumnWidths(aReflowState, basis, tableIsAutoWidth, p2t);
  }

  // set the table's columns to the min width
  for (colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    nscoord colMinWidth = colFrame->GetMinWidth();
    mTableFrame->SetColumnWidth(colX, colMinWidth);
  }

  // if the max width available is less than the min content width for fixed table, we're done
  nscoord minTableWidth = GetTableMinWidth();
  if (!tableIsAutoWidth && (maxWidth < minTableWidth)) {
    return BCW_Wrapup(aPresContext, this, mTableFrame, nsnull);
  }

  // if the max width available is less than the min content width for auto table
  // that had no % cells/cols, we're done
  if (tableIsAutoWidth && (maxWidth < minTableWidth) && (0 == perAdjTableWidth)) {
    return BCW_Wrapup(aPresContext, this, mTableFrame, nsnull);
  }

  PRInt32 cellSpacingTotal;
  // the following are of size NUM_WIDTHS, but only MIN_CON, DES_CON, FIX, FIX_ADJ, PCT
  // are used and they account for colspan ADJusted values
  PRInt32 totalWidths[NUM_WIDTHS]; // sum of col widths of a particular type 
  PRInt32 totalAvailWidths[NUM_WIDTHS]; 
  PRInt32 totalCounts[NUM_WIDTHS]; // num of cols of a particular type
  PRInt32 minWidths[NUM_WIDTHS];
  PRInt32 num0Proportional;

  CalculateTotals(cellSpacingTotal, totalCounts, totalWidths, totalAvailWidths, minWidths, num0Proportional);
  // auto width table's adjusted width needs cell spacing
  if (tableIsAutoWidth && perAdjTableWidth > 0) { 
    perAdjTableWidth = PR_MIN(perAdjTableWidth + cellSpacingTotal, maxWidth);
  }
  nscoord totalAllocated = totalWidths[MIN_CON] + cellSpacingTotal;
  
  // allocate and initialize arrays indicating what col gets set
  PRInt32* allocTypes = new PRInt32[numCols];
  if (!allocTypes) return PR_FALSE;
 
  for (colX = 0; colX < numCols; colX++) {
    allocTypes[colX] = -1;
  }

  // allocate percentage cols
  if (totalCounts[PCT] > 0) {
    if (totalAllocated + totalAvailWidths[PCT] - minWidths[PCT] <= maxWidth) {
      AllocateFully(totalAllocated, allocTypes, PCT);
      //NS_WARN_IF_FALSE(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, PCT, PR_FALSE, allocTypes, p2t);
      return BCW_Wrapup(aPresContext, this, mTableFrame, allocTypes);
    }
  }
  // allocate fixed cols
  if (totalAllocated < maxWidth && totalCounts[FIX] > 0) {
    if (totalAllocated + totalAvailWidths[FIX] - minWidths[FIX] <= maxWidth) { 
      AllocateFully(totalAllocated, allocTypes, FIX);
      //NS_WARN_IF_FALSE(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, FIX, PR_TRUE, allocTypes, p2t);
      return BCW_Wrapup(aPresContext, this, mTableFrame, allocTypes);
    }
  }
  // allocate fixed adjusted cols
  if (totalAllocated < maxWidth && totalCounts[FIX_ADJ] > 0) {
    if (totalAllocated + totalAvailWidths[FIX_ADJ] - minWidths[FIX_ADJ] <= maxWidth) { 
      AllocateFully(totalAllocated, allocTypes, FIX_ADJ);
      //NS_WARN_IF_FALSE(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, FIX_ADJ, PR_TRUE, allocTypes, p2t);
      return BCW_Wrapup(aPresContext, this, mTableFrame, allocTypes);
    }
  }
  // allocate proportional cols up to their min proportional value
  if (totalAllocated < maxWidth && totalCounts[MIN_PRO] > 0) {
    if (totalAllocated + totalAvailWidths[MIN_PRO] - minWidths[MIN_PRO] <= maxWidth) { 
      AllocateFully(totalAllocated, allocTypes, MIN_PRO, PR_FALSE);
      //NS_WARN_IF_FALSE(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, MIN_PRO, PR_FALSE, allocTypes, p2t);
      return BCW_Wrapup(aPresContext, this, mTableFrame, allocTypes);
    }
  }

  // allocate auto cols, considering even those that are proportional
  if (totalAllocated < maxWidth && totalCounts[DES_CON] > 0) {
    if (totalAllocated + totalAvailWidths[DES_CON] - minWidths[DES_CON]<= maxWidth) { 
      AllocateFully(totalAllocated, allocTypes, DES_CON);
      //NS_WARN_IF_FALSE(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, DES_CON, PR_TRUE, allocTypes, p2t);
      return BCW_Wrapup(aPresContext, this, mTableFrame, allocTypes);
    }
  }

  // if this is a nested non auto table and pass1 reflow, we are done
  if ((maxWidth == NS_UNCONSTRAINEDSIZE) && (!tableIsAutoWidth))  {
    return BCW_Wrapup(aPresContext, this, mTableFrame, allocTypes);
  }

  // allocate the rest unconstrained
  PRBool skip0Proportional = totalCounts[DES_CON] > num0Proportional;
  if ( (tableIsAutoWidth && (perAdjTableWidth - totalAllocated > 0)) ||
       (!tableIsAutoWidth && (totalAllocated < maxWidth)) ) {
    if (totalCounts[PCT] != numCols) {
      PRBool onlyAllocateAutoCols = (totalCounts[DES_CON] > 0) && !mIsNavQuirksMode;
      for (colX = 0; colX < numCols; colX++) {
        if (PCT == allocTypes[colX]) {
          allocTypes[colX] = -1;
        }
        else if (onlyAllocateAutoCols && (DES_CON != allocTypes[colX]) && 
                                         (DES_ADJ != allocTypes[colX])) {
          allocTypes[colX] = -1;
        }
      }
    }
    if (tableIsAutoWidth) {
      AllocateUnconstrained(perAdjTableWidth - totalAllocated, allocTypes, skip0Proportional, p2t);
    }
    else {
      AllocateUnconstrained(maxWidth - totalAllocated, allocTypes, skip0Proportional, p2t);
    }
  }
  // give the proportional cols the rest up to the max width in quirks mode
  else if (tableIsAutoWidth && mIsNavQuirksMode && (totalCounts[MIN_PRO] > 0)) {
    for (colX = 0; colX < numCols; colX++) {
      if (DES_CON != allocTypes[colX]) {
        allocTypes[colX] = -1;
      }
    }
    AllocateUnconstrained(maxWidth - totalAllocated, allocTypes, skip0Proportional, p2t);
  }


  return BCW_Wrapup(aPresContext, this, mTableFrame, allocTypes);
}


// Allocate aWidthType values to all cols available in aIsAllocated 
void BasicTableLayoutStrategy::AllocateFully(nscoord&      aTotalAllocated,
                                             PRInt32*      aAllocTypes,
                                             PRInt32       aWidthType,
                                             PRBool        aMarkAllocated)
{
  PRInt32 numCols = mTableFrame->GetColCount();
  for (PRInt32 colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    nscoord oldWidth = mTableFrame->GetColumnWidth(colX);
    nscoord newWidth = colFrame->GetWidth(aWidthType);

    PRBool useAdj;
    if (!CanAllocate(aWidthType, aAllocTypes[colX], colFrame, mIsNavQuirksMode, useAdj)) {
      if (-1 != aAllocTypes[colX]) {
        continue;
      }
    }
    // account for col span overrides with DES_CON and FIX
    if ((DES_CON == aWidthType) && useAdj) {
      newWidth = PR_MAX(newWidth, colFrame->GetWidth(DES_ADJ));
    }
    else if (PCT == aWidthType) {
      newWidth = PR_MAX(newWidth, colFrame->GetWidth(PCT_ADJ));
    }
    if (WIDTH_NOT_SET == newWidth) {
      continue;
    }
    if (newWidth > oldWidth) {
      mTableFrame->SetColumnWidth(colX, newWidth);
      aTotalAllocated += newWidth - oldWidth;
    }
    if (aMarkAllocated) {
      aAllocTypes[colX] = aWidthType;
    }
  }
}

void BasicTableLayoutStrategy::AllocateUnconstrained(PRInt32  aAllocAmount,
                                                     PRInt32* aAllocTypes,
                                                     PRBool   aSkip0Proportional,
                                                     float    aPixelToTwips)
{
  nscoord divisor          = 0;
  PRInt32 numColsAllocated = 0; 
  PRInt32 totalAllocated   = 0;
  PRInt32 colX;
  PRInt32 numCols = mTableFrame->GetColCount();
  for (colX = 0; colX < numCols; colX++) { 
    if (-1 != aAllocTypes[colX]) {
      divisor += mTableFrame->GetColumnWidth(colX);
      numColsAllocated++;
    }
  }
  for (colX = 0; colX < numCols; colX++) { 
    if (-1 != aAllocTypes[colX]) {
      if (aSkip0Proportional) {
        nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
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
        mTableFrame->SetColumnWidth(colX, oldWidth + (aAllocAmount - totalAllocated));
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

#define LIMIT_PCT  0
#define LIMIT_FIX  1
#define LIMIT_DES  2
#define LIMIT_NONE 3

void 
BasicTableLayoutStrategy::ComputeNonPctColspanWidths(const nsHTMLReflowState& aReflowState,
                                                     PRBool                   aConsiderPct,
                                                     float                    aPixelToTwips)
{
  PRInt32 numCols = mTableFrame->GetColCount();
  // zero out prior ADJ values 

  PRInt32 colX;
  for (colX = numCols - 1; colX >= 0; colX--) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    colFrame->SetWidth(MIN_ADJ, WIDTH_NOT_SET);
    colFrame->SetWidth(FIX_ADJ, WIDTH_NOT_SET);
    colFrame->SetWidth(DES_ADJ, WIDTH_NOT_SET);
  }

  // For each col, consider the cells originating in it with colspans > 1.
  // Adjust the cols that each cell spans if necessary. Iterate backwards 
  // so that nested and/or overlaping col spans handle the inner ones first, 
  // ensuring more accurated calculations.
  PRInt32 numRows = mTableFrame->GetRowCount();
  for (colX = numCols - 1; colX >= 0; colX--) { 
    for (PRInt32 rowX = 0; rowX < numRows; rowX++) {
      PRBool originates;
      PRInt32 colSpan;
      nsTableCellFrame* cellFrame = mTableFrame->GetCellInfoAt(rowX, colX, &originates, &colSpan);
      if (!originates || (1 == colSpan)) {
        continue;
      }
      // set MIN_ADJ, DES_ADJ, FIX_ADJ
      for (PRInt32 widthX = 0; widthX < NUM_MAJOR_WIDTHS; widthX++) {
        nscoord cellWidth = 0;
        if (MIN_CON == widthX) {
          cellWidth = cellFrame->GetPass1MaxElementSize().width;
        }
        else if (DES_CON == widthX) {
          cellWidth = cellFrame->GetMaximumWidth();
        }
        else { // FIX width
          // see if the cell has a style width specified
          const nsStylePosition* cellPosition;
          cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)cellPosition);
          if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) {
            // need to add padding into fixed width
            nsMargin padding = nsTableFrame::GetPadding(nsSize(aReflowState.mComputedWidth, 0), cellFrame);
            cellWidth = cellPosition->mWidth.GetCoordValue() + padding.left + padding.right;
            cellWidth = PR_MAX(cellWidth, cellFrame->GetPass1MaxElementSize().width);
          }
        }

        if (0 >= cellWidth) continue;

        PRInt32 limit = LIMIT_NONE;
        if (MIN_CON == widthX) {
          // for min width, first allocate pct cells up to their value if aConsiderPct, then
          // allocate fixed cells up to their value, then allocate auto cells up to their value, 
          // then allocate auto cells proportionally.
          limit = (aConsiderPct) ? LIMIT_PCT : LIMIT_FIX;
          while (limit <= LIMIT_NONE) {
            if (ComputeNonPctColspanWidths(widthX, cellFrame, cellWidth, colX, colSpan, 
                                           limit, aPixelToTwips)) {
              break;
            }
            limit++;
          }
        } else {
          // for des width, just allocate auto cells
          ComputeNonPctColspanWidths(widthX, cellFrame, cellWidth, colX, colSpan, 
                                     limit, aPixelToTwips);
        }
      }
    }
  }
}


PRBool 
BasicTableLayoutStrategy::ComputeNonPctColspanWidths(PRInt32           aWidthIndex,
                                                     nsTableCellFrame* aCellFrame,
                                                     nscoord           aCellWidth,
                                                     PRInt32           aColIndex,
                                                     PRInt32           aColSpan,
                                                     PRInt32&          aLimitType,
                                                     float             aPixelToTwips)
{
  PRBool result = PR_TRUE;

  nscoord spanCellSpacing = 0; // total cell spacing cells being spanned
  nscoord spanTotal       = 0; // total width of type aWidthIndex of spanned cells
  nscoord divisorFixPct   = 0; // the sum of fix or pct widths of spanned cells
  nscoord divisorDes      = 0; // the sum of desired widths
  // the following are only used for MIN_CON calculations. Takings differences between
  // actual and target values allows target values to be reached and not exceeded. This
  // is not as accurate as the method in AllocateConstrained, but it is a lot cheaper.
  nscoord divisorPct      = 0; // the sum of pct widths of spanned cells. 
  nscoord divisorFix      = 0; // the sum of fix widths of spanned cells
  nscoord divisorPctLimit = 0; // the sum of differences between pct widths and the current col widths
  nscoord divisorFixLimit = 0; // the sum of differences between fix widths and the current col widths
  nscoord divisorDesLimit = 0; // the sum of differences between des widths and the current col widths

  nscoord spacingX = mTableFrame->GetCellSpacingX();
  PRInt32 spanX;
  // accumulate the various divisors to be used later
  for (spanX = 0; spanX < aColSpan; spanX++) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(aColIndex + spanX);
    nscoord colWidth = PR_MAX(colFrame->GetWidth(aWidthIndex), 
                              colFrame->GetWidth(aWidthIndex + NUM_MAJOR_WIDTHS));
    colWidth = PR_MAX(colWidth, 0);
    if (aWidthIndex == DES_CON) {
      NS_ASSERTION(aLimitType == LIMIT_NONE, "invalid limit type in ComputeNonPctColspanWidths");
      nscoord fixPctWidth = GetConstrainedWidth(colFrame, PR_TRUE); 
      if (fixPctWidth <= 0) {
        divisorDes += colWidth;
        spanTotal  += colWidth;
      }
      else {
        divisorFixPct += fixPctWidth;
        spanTotal     += fixPctWidth;
      }
    }
    else if (aWidthIndex == MIN_CON) {
      nscoord pctWidth  = colFrame->GetPctWidth();
      nscoord fixWidth  = colFrame->GetFixWidth();
      nscoord desWidth  = colFrame->GetDesWidth();
      if (pctWidth > 0) {
        divisorPct += pctWidth;
        divisorFixPct += pctWidth;
        divisorPctLimit += PR_MAX(pctWidth - colWidth, 0);
      }
      else if (fixWidth > 0) {
        divisorFix += fixWidth;
        divisorFixPct += fixWidth;
        divisorFixLimit += PR_MAX(fixWidth - colWidth, 0);
      }
      else {
        divisorDes += desWidth;
        divisorDesLimit += PR_MAX(desWidth - colWidth, 0);
      }
      spanTotal += colWidth;
    }
    else { // FIX width
      NS_ASSERTION(aLimitType == LIMIT_NONE, "invalid limit type in ComputeNonPctColspanWidths");
      if (colWidth > 0) {
        divisorFixPct += colWidth;
      }
      else {
        divisorDes += colFrame->GetDesWidth();
      }
      spanTotal += colWidth;
    }
    if ((spanX > 0) && (mTableFrame->GetNumCellsOriginatingInCol(aColIndex + spanX) > 0)) {
      spanCellSpacing += spacingX;
    }
  }

  if (MIN_CON == aWidthIndex) {
    // this method gets called up to 4 times, first to let pct cols reach their target,
    // then to let fixed cols reach their target, next to let auto cols reach their target,
    // and finally to let auto col fill out the remainder. Below are some optimizations 
    // which can skip steps.

    // if there are no pct cols to focus on, focus on fix cols
    if ((LIMIT_PCT == aLimitType) && (0 == divisorPctLimit)) {
      aLimitType = LIMIT_FIX;
    }
    // if there are no fix cols to focus on, focus on auto cols
    if ((LIMIT_FIX == aLimitType) && (0 == divisorFixLimit)) {
      aLimitType = LIMIT_DES;
    }
    // if there are no auto cols to focus on, focus on nothing
    if ((LIMIT_DES == aLimitType) && (0 == divisorDesLimit)) {
      aLimitType = LIMIT_NONE;
    }
  }

  nscoord availWidth = aCellWidth - spanTotal - spanCellSpacing;
  // there are 3 cases where the target will not be reached
  if ((LIMIT_PCT == aLimitType) && (divisorPctLimit < availWidth)) {
    availWidth = divisorPctLimit;
    result = PR_FALSE;
  }
  if ((LIMIT_FIX == aLimitType) && (divisorFixLimit < availWidth)) {
    availWidth = divisorFixLimit;
    result = PR_FALSE;
  }
  if ((LIMIT_DES == aLimitType) && (divisorDesLimit < availWidth)) {
    availWidth = divisorDesLimit;
    result = PR_FALSE;
  }
  if (availWidth > 0) {
    // get the correct numerator in a similar fashion to getting the divisor
    for (spanX = 0; spanX < aColSpan; spanX++) {
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(aColIndex + spanX);
      nscoord colWidth = PR_MAX(colFrame->GetWidth(aWidthIndex), 
                                colFrame->GetWidth(aWidthIndex + NUM_MAJOR_WIDTHS));
      nscoord minWidth = colFrame->GetMinWidth();

      colWidth = PR_MAX(colWidth, 0);
      nscoord numeratorPct         = 0;
      nscoord numeratorPctLimit    = 0;
      nscoord numeratorFix         = 0;
      nscoord numeratorFixLimit    = 0;
      nscoord numeratorFixPct      = 0;
      nscoord numeratorFixPctLimit = 0;
      nscoord numeratorDes         = 0;
      nscoord numeratorDesLimit    = 0;
      if (aWidthIndex == DES_CON) {
        nscoord fixPctWidth = GetConstrainedWidth(colFrame, PR_TRUE);
        if (fixPctWidth <= 0) {
          numeratorDes = colWidth;
        }
        else {
          numeratorFixPct = fixPctWidth;
        }
      }
      else if (aWidthIndex == MIN_CON) {
        nscoord pctWidth  = colFrame->GetPctWidth();
        nscoord fixWidth  = colFrame->GetFixWidth();
        nscoord desWidth  = colFrame->GetDesWidth();
        if (pctWidth > 0) {
          numeratorPct = pctWidth;
          numeratorFixPct = pctWidth;
          numeratorPctLimit = PR_MAX(pctWidth - colWidth, 0);
        }
        else if (fixWidth > 0) {
          numeratorFix = fixWidth;
          numeratorFixPct = fixWidth;
          numeratorFixLimit = PR_MAX(fixWidth - colWidth, 0);
        }
        else {
          numeratorDes = desWidth;
          numeratorDesLimit = PR_MAX(desWidth - colWidth, 0);
        }
      }
      else { // FIX width
        if (colWidth <= 0) {
          numeratorDes = colFrame->GetDesWidth();
        }
        else {
          numeratorFixPct = colWidth;
        }
      }

      nscoord divisor;
      nscoord numerator;
      if (divisorDes > 0) { // there were auto cols
        divisor   = divisorDes;
        numerator = numeratorDes;
      }
      else {                // there were only constrained cols
        divisor   = divisorFixPct;
        numerator = numeratorFixPct;
      }
      // let pct cols reach their target 
      if (LIMIT_PCT == aLimitType) {
        if (divisorPctLimit > 0) {
          divisor   = divisorPctLimit;
          numerator = numeratorPctLimit;
        }
      }
      // let constrained cols reach their target 
      if (LIMIT_FIX == aLimitType) {
        if (divisorFixLimit > 0) {
          divisor   = divisorFixLimit;
          numerator = numeratorFixLimit;
        }
      }
      // let auto cols reach their target 
      else if (LIMIT_DES == aLimitType) {
        if (divisorDesLimit > 0) {
          divisor   = divisorDesLimit;
          numerator = numeratorDesLimit;
        }
      }

      // calculate the amount of additional width the col will get
      nscoord addition = (0 >= divisor) 
        ? NSToCoordRound( ((float)availWidth) / ((float)aColSpan) )
        : NSToCoordRound( (((float)numerator) / ((float)divisor)) * availWidth);
      if (addition > 0) {
        addition = nsTableFrame::RoundToPixel(addition, aPixelToTwips);
        nscoord newColAdjWidth = colWidth + addition;
        if (newColAdjWidth > minWidth) {
          if (FIX == aWidthIndex) {
            // a colspan cannot fix a column below what a cell desires on its own
            nscoord desCon = colFrame->GetWidth(DES_CON); // do not consider DES_ADJ
            if (desCon > newColAdjWidth) {
              newColAdjWidth = desCon;
            }
          }
          colFrame->SetWidth(aWidthIndex + NUM_MAJOR_WIDTHS, newColAdjWidth);
          colFrame->SetConstrainingCell(aCellFrame); // XXX is this right?
        }
      }
    }
  }
  return result;
}

// XXX percent left and right padding are not figured in the calculations
// The table frame needs to be used as the percent base because the reflow
// state may have an unconstrained width. There should probably be a frame
// state bit indicating that horizontal padding is percentage based.

// Determine min, desired, fixed, and proportional sizes for the cols and 
// and calculate min/max table width
PRBool 
BasicTableLayoutStrategy::AssignNonPctColumnWidths(nsIPresContext*          aPresContext,
                                                   nscoord                  aMaxWidth,
                                                   const nsHTMLReflowState& aReflowState,
                                                   float                    aPixelToTwips)
{
  if (gsDebugAssign) {printf("AssignNonPctColWidths en max=%d count=%d \n", aMaxWidth, gsDebugCount++); mTableFrame->Dump(aPresContext, PR_FALSE, PR_TRUE, PR_FALSE);}
  PRBool rv = PR_FALSE;
  PRInt32 numRows = mTableFrame->GetRowCount();
  PRInt32 numCols = mTableFrame->GetColCount();
  nscoord spacingX = mTableFrame->GetCellSpacingX();
  PRInt32 colX, rowX; 
  mCellSpacingTotal = 0;

  PRInt32 propTotal    = 0; // total of numbers of the type 1*, 2*, etc 
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
      minWidth = PR_MAX(minWidth, cellFrame->GetPass1MaxElementSize().width);
      nscoord cellDesWidth = cellFrame->GetMaximumWidth();
      if (cellDesWidth > desWidth) {
        desContributor = cellFrame;
        desWidth = cellDesWidth;
      }
      // see if the cell has a style width specified
      const nsStylePosition* cellPosition;
      cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)cellPosition);
      if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) {
        nscoord coordValue = cellPosition->mWidth.GetCoordValue();
        if (coordValue > 0) { // ignore if width == 0
          // need to add padding into fixed width
          nsSize percentBase(aReflowState.mComputedWidth, 0);
          nsMargin padding = nsTableFrame::GetPadding(percentBase, cellFrame);
          nscoord newFixWidth = coordValue + padding.left + padding.right;
          // 2nd part of condition is Nav/IE Quirk like below
          if ((newFixWidth > fixWidth) || ((newFixWidth == fixWidth) && (desContributor == cellFrame))) {
            fixWidth = newFixWidth;
            fixContributor = cellFrame;
          }
        }
      }
    }

    // Nav/IE Quirk like above
    if (fixWidth > 0) {
      if ((desWidth > fixWidth) && (fixContributor != desContributor)) {
        fixWidth = WIDTH_NOT_SET;
        fixContributor = nsnull;
      }
      else {
        fixWidth = PR_MAX(fixWidth, minWidth);
      }
    }
    desWidth = PR_MAX(desWidth, minWidth);

    // cache the computed column info
    colFrame->SetWidth(MIN_CON, minWidth);
    colFrame->SetWidth(DES_CON, desWidth);
    colFrame->SetConstrainingCell(fixContributor);
    if (fixWidth > 0) {
      colFrame->SetWidth(FIX, PR_MAX(fixWidth, minWidth));
    }

    // determine if there is a proportional column either from html4 
    // proportional width on a col or Nav Quirks cols attr
    if (fixWidth <= 0) {
      nscoord proportion = WIDTH_NOT_SET;
      nsStyleCoord colStyleWidth = colFrame->GetStyleWidth();
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
        colFrame->SetWidth(MIN_PRO, proportion);
        if (proportion > 0) {
          propTotal += proportion;
          colFrame->SetConstraint(eProportionConstraint);
        }
        else {
          colFrame->SetConstraint(e0ProportionConstraint);
          // override the desired, proportional widths
          nscoord colMinWidth = colFrame->GetWidth(MIN_CON);
          colFrame->SetWidth(DES_CON, colMinWidth);
          colFrame->SetWidth(MIN_PRO, 0);
        }
      }
    }
  }
  if (mCellSpacingTotal > 0) {
    mCellSpacingTotal += spacingX; // add last cell spacing on right
  }

  // figure the proportional width for porportional cols
  if (propTotal > 0)  {
    nscoord minPropTotal = 0;
    nscoord desPropTotal = 0;
    // figure the totals of all proportional cols which support every min and desired width
    for (colX = 0; colX < numCols; colX++) { 
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
      nscoord colProp = colFrame->GetWidth(MIN_PRO);
      if (colProp > 0) {
        nscoord minWidth = colFrame->GetWidth(MIN_CON);
        nscoord desWidth = colFrame->GetWidth(DES_CON);
        minPropTotal = PR_MAX(minPropTotal, NSToCoordRound(((float)propTotal * minWidth) / (float)colProp));
        desPropTotal = PR_MAX(desPropTotal, NSToCoordRound(((float)propTotal * desWidth) / (float)colProp));
      }
    }
    // store a ratio to allow going from a min to a desired proportional width
    if (minPropTotal > 0) {
      mMinToDesProportionRatio = ((float)desPropTotal) / ((float)minPropTotal);
    }
    // figure the cols proportional min width based on the new totals
    for (colX = 0; colX < numCols; colX++) { 
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
      nscoord colProp = colFrame->GetWidth(MIN_PRO);
      if (colProp > 0) {
        nscoord minProp = NSToCoordRound(((float)colProp * minPropTotal) / (float)propTotal);
        colFrame->SetWidth(MIN_PRO, minProp);
        colFrame->SetWidth(DES_CON, NSToCoordRound(((float)minProp) * mMinToDesProportionRatio));
      }
    }
  }

  ComputeNonPctColspanWidths(aReflowState, PR_FALSE, aPixelToTwips);

  // Set the col's fixed width if present 
  // Set the table col width for each col to the content min. 
  for (colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
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
    nscoord minWidth = colFrame->GetMinWidth();
    mTableFrame->SetColumnWidth(colX, minWidth);
  }

  if (gsDebugAssign) {printf("AssignNonPctColWidths ex\n"); mTableFrame->Dump(aPresContext, PR_FALSE, PR_TRUE, PR_FALSE);}
  return rv;
}

void
BasicTableLayoutStrategy::ReduceOverSpecifiedPctCols(nscoord aExcess)
{
  nscoord numCols = mTableFrame->GetColCount();
  for (PRInt32 colX = numCols - 1; (colX >= 0) && (aExcess > 0); colX--) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
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

// Determine percentage col widths for each col frame
nscoord 
BasicTableLayoutStrategy::AssignPctColumnWidths(const nsHTMLReflowState aReflowState,
                                                nscoord                 aBasisIn,
                                                PRBool                  aTableIsAutoWidth,
                                                float                   aPixelToTwips)
{
  mTableFrame->SetHasCellSpanningPctCol(PR_FALSE); // this gets refigured below
  PRInt32 numRows = mTableFrame->GetRowCount();
  PRInt32 numCols = mTableFrame->GetColCount();
  nscoord spacingX = mTableFrame->GetCellSpacingX();
  PRInt32 colX, rowX; 
  nscoord basis; // basis to use for percentage based calculations
  if (!aTableIsAutoWidth) {
    if (NS_UNCONSTRAINEDSIZE == aBasisIn) {
      return 0; // don't do the calculations on unconstrained basis
    }
    basis = aBasisIn;
  }
  else {
    // For an auto table, determine the potentially new percent adjusted width based 
    // on percent cells/cols. This probably should only be a NavQuirks thing, since
    // a percentage based cell or column on an auto table should force the column to auto
    basis = 0;                 
    nscoord fixWidthTotal = 0; // total of fixed widths of all cols
    float perTotal = 0.0f;     // total of percentage constrained cols and/or cells in cols
    PRInt32 numPerCols = 0;    // number of colums that have percentage constraints
    nscoord prefWidthTotal = 0;// total of des/fix widths of cols that don't have percentage constraints
    for (colX = 0; colX < numCols; colX++) { 
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
      nscoord colBasis = -1;
      // Scan the cells in the col 
      for (rowX = 0; rowX < numRows; rowX++) {
        PRBool originates;
        PRInt32 colSpan;
        nsTableCellFrame* cellFrame = mTableFrame->GetCellInfoAt(rowX, colX, &originates, &colSpan);
        if (!originates) { // skip  cells that don't originate in the col
          continue;
        }
        // see if the cell has a style percent width specified
        const nsStylePosition* cellPosition;
        cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)cellPosition);
        if (eStyleUnit_Percent == cellPosition->mWidth.GetUnit()) {
          float percent = cellPosition->mWidth.GetPercentValue();
          colBasis = 0;
          if (percent > 0.0f) {
            // calculate the preferred width of the cell based on fixWidth and desWidth
            nscoord cellDesWidth  = 0;
            for (PRInt32 spanX = 0; spanX < colSpan; spanX++) {
              nsTableColFrame* spanFrame = mTableFrame->GetColFrame(colX + spanX);
              cellDesWidth += spanFrame->GetDesWidth();
            }
            // figure the basis using the cell's desired width and percent
            colBasis = nsTableFrame::RoundToPixel(NSToCoordRound((float)cellDesWidth / percent), aPixelToTwips);
            perTotal += percent;
          }
        }
      }
      if (-1 == colBasis) {
        // see if the col has a style percent width specified
        nsStyleCoord colStyleWidth = colFrame->GetStyleWidth();
        if (eStyleUnit_Percent == colStyleWidth.GetUnit()) {
          float percent = colStyleWidth.GetPercentValue();
          colBasis = 0;
          if (percent > 0.0f) {
            nscoord desWidth = colFrame->GetDesWidth();
            colBasis = nsTableFrame::RoundToPixel(NSToCoordRound((float)desWidth / percent), aPixelToTwips);
          }
        }
      }
      basis = PR_MAX(basis, colBasis);
      nscoord fixWidth = colFrame->GetFixWidth();
      fixWidthTotal += fixWidth;
      if (colBasis >= 0) {
        numPerCols++;
      }
      else {
        prefWidthTotal += (fixWidth > 0) ? fixWidth : colFrame->GetDesWidth();
      }
    } // end for (colX ..
    // If there are no pct cells or cols, there is nothing to do.
    if (0 == numPerCols) {
      return 0;
    }
    // If there is only one col and it is % based, it won't affect anything
    if ((1 == numCols) && (numCols == numPerCols)) {
      return 0;
    }
    // compute a basis considering total percentages and the desired width of everything else
    if (perTotal < 1.0f) {
      if (perTotal > 0.0f) {
        nscoord otherBasis = nsTableFrame::RoundToPixel(NSToCoordRound((float)prefWidthTotal / (1.0f - perTotal)), aPixelToTwips);
        basis = PR_MAX(basis, otherBasis);
      }
    }
    else if ((prefWidthTotal > 0) && (NS_UNCONSTRAINEDSIZE != aBasisIn)) { // make the basis as big as possible 
      basis = aBasisIn;
    }

    basis = PR_MAX(basis, fixWidthTotal);
    basis = PR_MIN(basis, aBasisIn); 
  }

  nscoord colPctTotal = 0;
  
  // Determine the percentage contribution for cols and for cells with colspan = 1
  // Iterate backwards, similarly to the reasoning in AssignNonPctColumnWidths
  for (colX = numCols - 1; colX >= 0; colX--) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    nscoord maxColPctWidth = WIDTH_NOT_SET;
    float maxColPct = 0.0f;

    nsTableCellFrame* percentContributor = nsnull;
    // Scan the cells in the col that have colspan = 1; assign PER widths
    for (rowX = 0; rowX < numRows; rowX++) {
      PRBool originates;
      PRInt32 colSpan;
      nsTableCellFrame* cellFrame = mTableFrame->GetCellInfoAt(rowX, colX, &originates, &colSpan);
      // skip cells that don't originate at (rowX, colX); colspans are handled in the
      // next pass, row spans don't need to be handled
      if (!cellFrame || !originates || (colSpan > 1)) { 
        continue;
      }
      // see if the cell has a style percent width specified
      const nsStylePosition* cellPosition;
      cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)cellPosition);
      if (eStyleUnit_Percent == cellPosition->mWidth.GetUnit()) {
        float percent = cellPosition->mWidth.GetPercentValue();
        if (percent > maxColPct) {
          maxColPct = percent;
          maxColPctWidth = NSToCoordRound( ((float)basis) * maxColPct );
          percentContributor = cellFrame;
          if (!mIsNavQuirksMode) { 
            // need to add padding
            nsMargin padding = nsTableFrame::GetPadding(nsSize(basis, 0), cellFrame);
            maxColPctWidth += padding.left + padding.right;
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
      colFrame->SetConstrainingCell(percentContributor);
      colPctTotal += NSToCoordRound(100.0f * (float)maxColPct);
    }
  }

  // if the percent total went over 100%, adjustments need to be made to right most cols
  if (colPctTotal > 100) {
    ReduceOverSpecifiedPctCols(NSToCoordRound(((float)(colPctTotal - 100)) * 0.01f * (float)basis));
    colPctTotal = 100;
  }

  // check to see if a cell spans a percentage col. This will cause the MIN_ADJ,
  // FIX_ADJ, and DES_ADJ values to be recomputed 
  for (colX = 0; colX < numCols; colX++) { 
    for (rowX = 0; rowX < numRows; rowX++) {
      PRBool originates;
      PRInt32 colSpan;
      nsTableCellFrame* cellFrame = mTableFrame->GetCellInfoAt(rowX, colX, &originates, &colSpan);
      if (!originates || (1 == colSpan)) {
        continue;
      }
      // determine if the cell spans cols which have a pct value
      for (PRInt32 spanX = 0; spanX < colSpan; spanX++) {
        nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX + spanX);
        if (colFrame->GetWidth(PCT) > 0) {
          mTableFrame->SetHasCellSpanningPctCol(PR_TRUE);
          // recompute the MIN_ADJ, FIX_ADJ, and DES_ADJ values
          ComputeNonPctColspanWidths(aReflowState, PR_TRUE, aPixelToTwips);
          break;
        }
      }
    }
  }

  // For each col, consider the cells originating in it with colspans > 1.
  // Adjust the cols that each cell spans if necessary.
  for (colX = 0; colX < numCols; colX++) { 
    for (rowX = 0; rowX < numRows; rowX++) {
      PRBool originates;
      PRInt32 colSpan;
      nsTableCellFrame* cellFrame = mTableFrame->GetCellInfoAt(rowX, colX, &originates, &colSpan);
      if (!originates || (1 == colSpan)) {
        continue;
      }
      nscoord cellPctWidth = WIDTH_NOT_SET;
      // see if the cell has a style percentage width specified
      const nsStylePosition* cellPosition;
      cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)cellPosition);
      float cellPct = 0.0f;
      if (eStyleUnit_Percent == cellPosition->mWidth.GetUnit()) {
        cellPct = cellPosition->mWidth.GetPercentValue();
        cellPctWidth = nsTableFrame::RoundToPixel(NSToCoordRound( ((float)basis) * cellPct ), aPixelToTwips);
        if (!mIsNavQuirksMode) { 
          // need to add padding 
          nsMargin padding = nsTableFrame::GetPadding(nsSize(basis, 0), cellFrame);
          cellPctWidth += padding.left + padding.right;
        }
      }
      if (cellPctWidth > 0) {
        nscoord spanCellSpacing = 0;
        nscoord spanTotal = 0;
        nscoord colPctWidthTotal = 0;
        // accumulate the spanTotal as the max of MIN, DES, FIX, PCT
        PRInt32 spanX;
        for (spanX = 0; spanX < colSpan; spanX++) {
          nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX + spanX);
          nscoord colPctWidth = colFrame->GetWidth(PCT);
          if (colPctWidth > 0) { // skip pct cols
            colPctWidthTotal += colPctWidth;
            continue;
          }
          nscoord colWidth = PR_MAX(colFrame->GetMinWidth(), colFrame->GetFixWidth());
          colWidth = PR_MAX(colWidth, colFrame->GetDesWidth()); // XXX check this
          //colWidth = PR_MAX(colWidth, colFrame->GetPctWidth());
          spanTotal += colWidth;
          if ((spanX > 0) && (mTableFrame->GetNumCellsOriginatingInCol(colX + spanX) > 0)) {
            spanCellSpacing += spacingX;
          }
        }
        cellPctWidth += spanCellSpacing; // add it back in since it was subtracted from aBasisIn
        if (cellPctWidth <= 0) {
          continue;
        }
        if (colPctWidthTotal < cellPctWidth) { 
          // record the percent contributions for the spanned cols
          for (spanX = 0; spanX < colSpan; spanX++) {
            nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX + spanX);
            if (colFrame->GetWidth(PCT) > 0) { // skip pct cols
              continue;
            }
            nscoord minWidth = colFrame->GetMinWidth();
            nscoord colWidth = PR_MAX(minWidth, colFrame->GetFixWidth());
            colWidth = PR_MAX(colWidth, colFrame->GetDesWidth()); // XXX check this
            float avail = (float)PR_MAX(cellPctWidth - colPctWidthTotal, 0);
            float colPctAdj = (0 == spanTotal) 
                              ? avail / ((float) colSpan)
                              : (avail / (float)basis) * (((float)colWidth) / (float)spanTotal);
            if (colPctAdj > 0) {
              nscoord colPctAdjWidth = colFrame->GetWidth(PCT_ADJ);
              nscoord newColPctAdjWidth = nsTableFrame::RoundToPixel(NSToCoordRound(colPctAdj * (float)basis), aPixelToTwips);
              if (newColPctAdjWidth > colPctAdjWidth) {
                newColPctAdjWidth = PR_MAX(newColPctAdjWidth, minWidth); 
                if (newColPctAdjWidth > colFrame->GetWidth(PCT)) {
                  colFrame->SetWidth(PCT_ADJ, newColPctAdjWidth);
                  colFrame->SetConstrainingCell(cellFrame);
                }
              }
            }
          }
        }
      }
    } // end for (rowX ..
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    colPctTotal += NSToCoordRound(100.0f * (float)colFrame->GetWidth(PCT_ADJ) / (float)basis);
  } // end for (colX ..

  // if the percent total went over 100%, adjustments need to be made to right most cols
  if (colPctTotal > 100) {
    ReduceOverSpecifiedPctCols(NSToCoordRound(((float)(colPctTotal - 100)) * 0.01f * (float)basis));
  }

  return basis;
}

nscoord BasicTableLayoutStrategy::GetTableMinWidth() const
{
  nscoord minWidth = 0;
  nscoord spacingX = mTableFrame->GetCellSpacingX();
  PRInt32 numCols = mTableFrame->GetColCount();
  for (PRInt32 colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    minWidth += PR_MAX(colFrame->GetMinWidth(), colFrame->GetWidth(MIN_ADJ));
    if (mTableFrame->GetNumCellsOriginatingInCol(colX) > 0) {
      minWidth += spacingX;
    }
  }
  // if it is not a degenerate table, add the last spacing on the right
  if (minWidth > 0) {
    minWidth += spacingX;
  }
  return minWidth;
}

nscoord BasicTableLayoutStrategy::GetTableMaxWidth() const
{
  nscoord spacingX = mTableFrame->GetCellSpacingX();
  PRInt32 numCols = mTableFrame->GetColCount();
  nscoord maxWidth = 0;
  for (PRInt32 colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    nscoord width = colFrame->GetPctWidth();
    if (width <= 0) {
      width = colFrame->GetFixWidth();
      if (width <= 0) {
        width = colFrame->GetWidth(MIN_PRO);
        if (width <= 0) {
          width = colFrame->GetDesWidth();
        }
      }
    }
    maxWidth += width;
    if (mTableFrame->GetNumCellsOriginatingInCol(colX) > 0) {
      maxWidth += spacingX;
    }
  }
  // if it is not a degenerate table, add the last spacing on the right
  if (maxWidth > 0) {
    maxWidth += spacingX;
  }
  return maxWidth;
}

// calculate totals by width type. The logic here is kept in synch with 
// that in CanAllocate.
void BasicTableLayoutStrategy::CalculateTotals(PRInt32& aCellSpacing,
                                               PRInt32* aTotalCounts,
                                               PRInt32* aTotalWidths,
                                               PRInt32* aTotalAvailWidths,
                                               PRInt32* aMinWidths,
                                               PRInt32& a0ProportionalCount)
{
  //mTableFrame->Dump(PR_TRUE, PR_FALSE);
  aCellSpacing = 0;
  for (PRInt32 widthX = 0; widthX < NUM_WIDTHS; widthX++) {
    aTotalCounts[widthX]      = 0;
    aTotalWidths[widthX]      = 0;
    aTotalAvailWidths[widthX] = 0;
    aMinWidths[widthX]        = 0;
  }
  a0ProportionalCount = 0;

  nscoord spacingX = mTableFrame->GetCellSpacingX();
  PRInt32 numCols = mTableFrame->GetColCount();
  PRInt32 colX;

  for (colX = 0; colX < numCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    if (mTableFrame->GetNumCellsOriginatingInCol(colX) > 0) {
      aCellSpacing += spacingX;
    }
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
      aTotalWidths[PCT] += pct;
      aTotalAvailWidths[PCT] += pct;
      aMinWidths[PCT] += minCol;
      continue;
    }
    if ((fix > 0) || (fixAdj > 0)) {
      if (fix > 0) {
        aTotalCounts[FIX]++;
        aTotalWidths[FIX] += fix;
        aTotalAvailWidths[FIX] += fix;
        aMinWidths[FIX] += minCol;
      }
      nscoord desCon = colFrame->GetWidth(DES_CON);
      if (fixAdj > 0) {
        if (fixAdj > fix) {
          aTotalCounts[FIX_ADJ]++;
          aTotalWidths[FIX_ADJ] += fixAdj;
          aTotalAvailWidths[FIX_ADJ] += fixAdj;
          if (fix > 0) {
            aTotalAvailWidths[FIX_ADJ] -= fix;
          }
          else { // there was no fix
            aMinWidths[FIX_ADJ] += minCol;
          }
          // include the DES_CON of a FIX_ADJ that has no FIX 
          if (mIsNavQuirksMode && (desCon > fixAdj) && (fix <= 0)) { 
            aTotalCounts[DES_CON]++;
            aTotalWidths[DES_CON] += desCon;
            aTotalAvailWidths[DES_CON] += desCon;
            if (fixAdj > 0) {
              aTotalAvailWidths[DES_CON] -= fixAdj;
            }
          }
        }
      }
      continue;
    }

    if (eProportionConstraint == colFrame->GetConstraint()) {
      nscoord minProp = colFrame->GetWidth(MIN_PRO);
      aTotalCounts[MIN_PRO]++;
      aTotalWidths[MIN_PRO] += minProp;
      aTotalAvailWidths[MIN_PRO] = aTotalWidths[MIN_PRO];
      aTotalCounts[DES_CON]++;
      aTotalWidths[DES_CON] += NSToCoordRound(((float)minProp) * mMinToDesProportionRatio);
      aTotalAvailWidths[DES_CON] = aTotalWidths[DES_CON];
      aMinWidths[MIN_PRO] += minCol;
      aMinWidths[DES_CON] += minProp;
      continue;
    }

    // desired alone is lowest priority
    aTotalCounts[DES_CON]++;
    aTotalWidths[DES_CON] += colFrame->GetDesWidth();
    aTotalAvailWidths[DES_CON] = aTotalWidths[DES_CON];
    aMinWidths[DES_CON] += minCol;
  }
  // if it is not a degenerate table, add the last spacing on the right
  if (numCols > 0) {
    aCellSpacing += spacingX;
  }
}


struct nsColInfo {
  nsColInfo(nsTableColFrame* aFrame,
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
          nsColInfo**   aColInfo,
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
            nsColInfo** aColInfo,
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
            nsColInfo** aColInfo,
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
AC_Sort(nsColInfo** aColInfo, PRInt32 aNumCols)
{
  // sort the cols based on the Weight 
  for (PRInt32 j = aNumCols - 1; j > 0; j--) {
    for (PRInt32 i = 0; i < j; i++) { 
      if (aColInfo[i]->mWeight < aColInfo[i+1]->mWeight) { // swap them
        nsColInfo* save = aColInfo[i];
        aColInfo[i]     = aColInfo[i+1];
        aColInfo[i+1]   = save;
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
  PRBool useAdj = PR_TRUE;
  PRInt32 colX;
  // find out how many constrained cols there are
  for (colX = 0; colX < numCols; colX++) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    if (!CanAllocate(aWidthType, aAllocTypes[colX], colFrame, mIsNavQuirksMode, useAdj)) {
      if (-1 != aAllocTypes[colX]) {
        continue;
      }
    }
    numConstrainedCols++;
    nscoord width = colFrame->GetWidth(aWidthType);
    if ((DES_CON == aWidthType) && useAdj) {
      width = PR_MAX(width, colFrame->GetWidth(DES_ADJ));
    }
    else if (PCT == aWidthType) {
      width = PR_MAX(width, colFrame->GetWidth(PCT_ADJ));
    }
  }

  // allocate storage for the constrained cols. Only they get adjusted.
  nsColInfo** colInfo = new nsColInfo*[numConstrainedCols];
  if (!colInfo) return;
  memset(colInfo, 0, numConstrainedCols * sizeof(nsColInfo *));

  PRInt32 maxMinDiff = 0;
  PRInt32 constrColX = 0;
  // set the col info entries for each constrained col
  for (colX = 0; colX < numCols; colX++) {
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    if (!CanAllocate(aWidthType, aAllocTypes[colX], colFrame, mIsNavQuirksMode, useAdj)) {
      if (-1 != aAllocTypes[colX]) {
        continue;
      }
    }
    nscoord minWidth = mTableFrame->GetColumnWidth(colX);
    nscoord maxWidth = colFrame->GetWidth(aWidthType);
    if ((DES_CON == aWidthType) && useAdj) {
      maxWidth = PR_MAX(maxWidth, colFrame->GetWidth(DES_ADJ));
    }
    else if (PCT == aWidthType) {
      maxWidth = PR_MAX(maxWidth, colFrame->GetWidth(PCT_ADJ));
    }
    if (maxWidth <= 0) {
      continue;
    }
    sumMaxConstraints += maxWidth;
    sumMinConstraints += minWidth;

    maxWidth = PR_MAX(maxWidth, minWidth);
    maxMinDiff += maxWidth - minWidth;
    nscoord startWidth = (aStartAtMin) ? minWidth : maxWidth;
    colInfo[constrColX] = new nsColInfo(colFrame, colX, minWidth, startWidth, maxWidth);
    if (!colInfo[constrColX]) {
      AC_Wrapup(mTableFrame, numConstrainedCols, colInfo, PR_TRUE);
      return;
    }
    aAllocTypes[colX] = aWidthType;
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

// XXX this function will be improved after the colspan algorithms have been extracted
// from AssignNonPctColumnWidths and AssignPctColumnWidths. For now, pessimistic
// assumptions are made
PRBool BasicTableLayoutStrategy::ColumnsCanBeInvalidatedBy(nsStyleCoord*           aPrevStyleWidth,
                                                           const nsTableCellFrame& aCellFrame) const
{
  if (!mTableFrame) 
    return PR_TRUE;

  const nsStylePosition* cellPosition;
  aCellFrame.GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)cellPosition);
  const nsStyleCoord& styleWidth = cellPosition->mWidth;

  PRInt32 colIndex;
  aCellFrame.GetColIndex(colIndex);
  nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
  nscoord colSpan = mTableFrame->GetEffectiveColSpan(aCellFrame);

  if (aPrevStyleWidth) {
    nsTableColFrame* colSpanFrame = colFrame;
    // see if this cell is responsible for setting a fixed or percentage based col
    for (PRInt32 span = 1; span <= colSpan; span++) {
      if (&aCellFrame == colSpanFrame->GetConstrainingCell()) 
        return PR_TRUE; // assume that the style change will affect cols
      if (span < colSpan) 
        colSpanFrame = mTableFrame->GetColFrame(colIndex + span);
    }
    // if we get here, the cell was not responsible for a fixed or percentage col
    switch(aPrevStyleWidth->GetUnit()) {
    case eStyleUnit_Percent: 
      if (eStyleUnit_Percent == styleWidth.GetUnit()) {
        // PCT to PCT 
        if (aPrevStyleWidth->GetPercentValue() < styleWidth.GetPercentValue()) 
          return PR_TRUE; // XXX see comments above
      }
      // PCT to FIX and PCT to AUTO changes have no effect since PCT allocations 
      // are the highest priority and the cell's previous PCT value did not 
      // cause it to be responsible for setting any cells PCT_ADJ 
    case eStyleUnit_Coord: 
      if (eStyleUnit_Percent == styleWidth.GetUnit()) {
        // FIX to PCT 
        return PR_TRUE; // XXX see comments above
      } 
      else if (eStyleUnit_Coord == styleWidth.GetUnit()) {
        // FIX to FIX
        nscoord newWidth = styleWidth.GetCoordValue();
        if (aPrevStyleWidth->GetCoordValue() < newWidth) {
          if (colSpan > 1) 
            return PR_TRUE; // XXX see comments above
          if (newWidth > colFrame->GetFixWidth()) 
            return PR_TRUE; // XXX see comments above          
        }
      }
      // FIX to AUTO results in no column changes here
    case eStyleUnit_Auto: 
      if (eStyleUnit_Percent == styleWidth.GetUnit()) {
        // AUTO to PCT 
        return PR_TRUE; // XXX see comments above
      } 
      else if (eStyleUnit_Coord == styleWidth.GetUnit()) {
        // AUTO to FIX
        return PR_TRUE; // XXX see comments above
      } 
      // AUTO to AUTO is not a style change
    default:
      break;
    }
  }
  return PR_FALSE;
}


// XXX this function will be improved after the colspan algorithms have been extracted
// from AssignNonPctColumnWidths and AssignPctColumnWidths. For now, pessimistic
// assumptions are made
PRBool BasicTableLayoutStrategy::ColumnsCanBeInvalidatedBy(const nsTableCellFrame& aCellFrame,
                                                           PRBool                  aConsiderMinWidth) const
{
  if (aConsiderMinWidth || !mTableFrame) 
    return PR_TRUE;

  PRInt32 colIndex;
  aCellFrame.GetColIndex(colIndex);
  nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
  nscoord colSpan = mTableFrame->GetEffectiveColSpan(aCellFrame);

  // check to see if DES_CON can affect columns
  nsTableColFrame* spanFrame = colFrame;
  for (PRInt32 span = 0; span < colSpan; span++) {
    // see if the column width is constrained
    if ((spanFrame->GetPctWidth() > 0) || (spanFrame->GetFixWidth() > 0) ||
        (spanFrame->GetWidth(MIN_PRO) > 0)) {
      if ((spanFrame->GetWidth(PCT_ADJ) > 0) && (spanFrame->GetWidth(PCT) <= 0)) {
        return PR_TRUE; 
      }
      if ((spanFrame->GetWidth(FIX_ADJ) > 0) && (spanFrame->GetWidth(FIX) <= 0)) {
        return PR_TRUE; // its unfortunate that the balancing algorithms cause this
      }
    }
    else {
      return PR_TRUE; // XXX need to add cases if table has coord width specified
    }
    if (span < colSpan - 1) 
      spanFrame = mTableFrame->GetColFrame(colIndex + span + 1);
  }
  return PR_FALSE;
}

PRBool BasicTableLayoutStrategy::ColumnsAreValidFor(const nsTableCellFrame& aCellFrame,
                                                    nscoord                 aPrevCellMin,
                                                    nscoord                 aPrevCellDes) const
{
  PRInt32 colIndex;
  aCellFrame.GetColIndex(colIndex);
  nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
  nscoord colSpan = mTableFrame->GetEffectiveColSpan(aCellFrame);

  nscoord cellMin = aCellFrame.GetPass1MaxElementSize().width;
  nscoord cellDes = aCellFrame.GetMaximumWidth();
  nscoord colMin  = colFrame->GetMinWidth();
  nscoord colDes  = colFrame->GetDesWidth();

  PRBool minChanged = PR_TRUE;
  if (((cellMin > aPrevCellMin) && (cellMin <= colMin)) ||
      ((cellMin <= aPrevCellMin) && (aPrevCellMin <= colMin))) {
    minChanged = PR_FALSE;
  }
  if (minChanged) {
    return PR_FALSE; // XXX add cases where table has coord width and cell is constrained
  }

  PRBool desChanged = PR_TRUE;
  if (((cellDes > aPrevCellDes) && (cellDes <= colDes)) ||
      (cellDes == aPrevCellDes)) {
    // XXX This next check causes a problem if the cell's desired width shrinks,
    // because the comparison (aPresCellDes <= colDes) will always be TRUE and
    // so we always end up setting desChanged to PR_FALSE. That means the column
    // won't shrink like it should
#if 0
      || ((cellDes < aPrevCellDes) && (aPrevCellDes <= colDes))) {
#endif
    desChanged = PR_FALSE;
  }
  
  if (1 == colSpan) {
    // see if the column width is constrained
    if ((colFrame->GetPctWidth() > 0) || (colFrame->GetFixWidth() > 0) ||
        (colFrame->GetWidth(MIN_PRO) > 0)) {
      if ((colFrame->GetWidth(PCT_ADJ) > 0) && (colFrame->GetWidth(PCT) <= 0)) {
        if (desChanged) {
          return PR_FALSE; // XXX add cases where table has coord width
        }
      }
      if ((colFrame->GetWidth(FIX_ADJ) > 0) && (colFrame->GetWidth(FIX) <= 0)) {
        if (desChanged) {
          return PR_FALSE; // its unfortunate that the balancing algorithms cause this
                          // XXX add cases where table has coord width
        }
      }
    }
    else { // the column width is not constrained
      if (desChanged) {
        return PR_FALSE;
      }
    }
  }
  else {
    return PR_FALSE; // XXX this needs a lot of cases
  }
  return PR_TRUE;
}
  
PRBool BasicTableLayoutStrategy::IsColumnInList(const PRInt32 colIndex, 
                                                PRInt32*      colIndexes, 
                                                PRInt32       aNumFixedColumns)
{
  PRBool result = PR_FALSE;
  for (PRInt32 i = 0; i < aNumFixedColumns; i++) {
    if (colIndex == colIndexes[i]) {
      result = PR_TRUE;
      break;
    }
    else if (colIndex<colIndexes[i]) {
      break;
    }
  }
  return result;
}

PRBool BasicTableLayoutStrategy::ColIsSpecifiedAsMinimumWidth(PRInt32 aColIndex)
{
  PRBool result = PR_FALSE;
  nsTableColFrame* colFrame;
  mTableFrame->GetColumnFrame(aColIndex, colFrame);
  nsStyleCoord colStyleWidth = colFrame->GetStyleWidth();
  switch (colStyleWidth.GetUnit()) {
  case eStyleUnit_Coord:
    if (0 == colStyleWidth.GetCoordValue()) {
      result = PR_TRUE;
    }
    break;
  case eStyleUnit_Percent:
    {
      // total hack for now for 0% and 1% specifications
      // should compare percent to available parent width and see that it is below minimum
      // for this column
      float percent = colStyleWidth.GetPercentValue();
      if (0.0f == percent || 0.01f == percent) {  
        result = PR_TRUE;
      }
      break;
    }
  case eStyleUnit_Proportional:
    if (0 == colStyleWidth.GetIntValue()) {
      result = PR_TRUE;
    }

  default:
    break;
  }

  return result;
}

void BasicTableLayoutStrategy::Dump(PRInt32 aIndent)
{
  char* indent = new char[aIndent + 1];
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

