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

#include "BasicTableLayoutStrategy.h"
#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsVoidArray.h"
#include "nsHTMLIIDs.h"

/* ---------- BasicTableLayoutStrategy ---------- */

/* return true if the style indicates that the width is fixed 
 * for the purposes of column width determination
 */
inline
PRBool BasicTableLayoutStrategy::IsFixedWidth(const nsStylePosition* aStylePosition,
                                              const nsStyleTable*    aStyleTable)
{
  return PRBool ((eStyleUnit_Coord==aStylePosition->mWidth.GetUnit()) ||
                 (eStyleUnit_Coord==aStyleTable->mSpanWidth.GetUnit()));
}


BasicTableLayoutStrategy::BasicTableLayoutStrategy(nsTableFrame *aFrame, PRBool aIsNavQuirks)
{
  NS_ASSERTION(nsnull != aFrame, "bad frame arg");

  mTableFrame            = aFrame;
  mMinTableContentWidth  = 0;
  mMaxTableContentWidth  = 0;
  mCellSpacingTotal      = 0;
  mIsNavQuirksMode       = aIsNavQuirks;
}

BasicTableLayoutStrategy::~BasicTableLayoutStrategy()
{
}

PRBool BasicTableLayoutStrategy::Initialize(nsSize* aMaxElementSize, 
                                            PRInt32 aNumCols,
                                            nscoord aMaxWidth)
{
  ContinuingFrameCheck();

  PRBool result = PR_TRUE;

  // re-init instance variables
  mNumCols              = aNumCols;
  mMinTableContentWidth = 0;
  mMaxTableContentWidth = 0;
  mCellSpacingTotal     = 0;
  mCols                 = mTableFrame->GetEffectiveCOLSAttribute();

  // assign the width of all fixed-width columns
  AssignPreliminaryColumnWidths(aMaxWidth);

  // set aMaxElementSize here because we compute mMinTableWidth in AssignPreliminaryColumnWidths
  if (nsnull != aMaxElementSize) {
    SetMaxElementSize(aMaxElementSize);
  }

  return result;
}

void BasicTableLayoutStrategy::SetMaxElementSize(nsSize* aMaxElementSize)
{
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->height = 0;
    nsMargin borderPadding;
    const nsStylePosition* tablePosition;
    const nsStyleSpacing* tableSpacing;
    mTableFrame->GetStyleData(eStyleStruct_Position, ((const nsStyleStruct *&)tablePosition));
    mTableFrame->GetStyleData(eStyleStruct_Spacing , ((const nsStyleStruct *&)tableSpacing));
    mTableFrame->GetTableBorder(borderPadding);
    nsMargin padding;
    tableSpacing->GetPadding(padding);
    borderPadding += padding;
    nscoord horBorderPadding = borderPadding.left + borderPadding.right;
    if (tablePosition->mWidth.GetUnit() == eStyleUnit_Coord) {
      aMaxElementSize->width = tablePosition->mWidth.GetCoordValue();
      if (mMinTableContentWidth + horBorderPadding > aMaxElementSize->width) {
        aMaxElementSize->width = mMinTableContentWidth + horBorderPadding;
      }
    }   
    else {
      aMaxElementSize->width = mMinTableContentWidth + horBorderPadding;
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

PRBool BCW_Wrapup(BasicTableLayoutStrategy* aStrategy, 
                  nsTableFrame*            aTableFrame, 
                  PRInt32*                 aAllocTypes)
{
  delete [] aAllocTypes;
  //aStrategy->Dump(0);
  //aTableFrame->Dump(PR_TRUE, PR_FALSE);
  return PR_TRUE;
}

PRBool 
BasicTableLayoutStrategy::BalanceColumnWidths(nsIStyleContext*         aTableStyle,
                                              const nsHTMLReflowState& aReflowState,
                                              nscoord                  aMaxWidthIn)
{
  //mTableFrame->Dump(PR_TRUE, PR_FALSE);
  ContinuingFrameCheck();
  if (!aTableStyle) {
    NS_ASSERTION(aTableStyle, "bad style arg");
    return PR_FALSE;
  }

  // determine if the table is auto/fixed and get the fixed width if available
  nscoord maxWidth = aMaxWidthIn; 
  nscoord specifiedTableWidth = 0;
  PRBool tableIsAutoWidth = 
    nsTableFrame::TableIsAutoWidth(mTableFrame, aTableStyle, aReflowState, specifiedTableWidth);
  // a specifiedTableWidth of <= 0 indicates percentage based 
  if (!tableIsAutoWidth && (specifiedTableWidth > 0)) {
    maxWidth = PR_MIN(specifiedTableWidth, aMaxWidthIn); // specifiedWidth usually == aMaxWidthIn for fixed table
  }
  // reduce the maxWidth by border and padding in some cases, since we will be dealing with content width
  if (!tableIsAutoWidth && (maxWidth != NS_UNCONSTRAINEDSIZE)) {
    const nsStylePosition* position;
    mTableFrame->GetStyleData(eStyleStruct_Position, ((const nsStyleStruct *&)position));
    if (eStyleUnit_Percent != position->mWidth.GetUnit()) {
      const nsStyleSpacing* spacing;
      mTableFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
      nsMargin borderPadding;
      spacing->CalcBorderPaddingFor(mTableFrame, borderPadding);
      maxWidth -= borderPadding.left + borderPadding.right;
      maxWidth = PR_MAX(0, maxWidth);
    }
  }
  // set the table's columns to the min width
  // initialize the col percent and cell percent values to 0.
  PRInt32 colX;
  for (colX = 0; colX < mNumCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    nscoord colMinWidth = colFrame->GetMinWidth();
    mTableFrame->SetColumnWidth(colX, colMinWidth);
    colFrame->SetWidth(PCT, WIDTH_NOT_SET);
    colFrame->SetWidth(PCT_ADJ, WIDTH_NOT_SET);
  }

  // if the max width available is less than the min content width for fixed table, we're done
  if (!tableIsAutoWidth && (maxWidth < mMinTableContentWidth)) {
    return PR_FALSE;
  }

  // set PCT and PCT_ADJ widths on col frames and for an auto table return 
  // a new table width based on percent cells/cols if they exist
  nscoord perAdjTableWidth = (maxWidth != NS_UNCONSTRAINEDSIZE) 
    ? AssignPercentageColumnWidths(maxWidth - mCellSpacingTotal, tableIsAutoWidth) : 0; 

  // if the max width available is less than the min content width for auto table
  // that had no % cells/cols, we're done
  if (tableIsAutoWidth && (maxWidth < mMinTableContentWidth) & (0 == perAdjTableWidth)) {
    return PR_TRUE;
  }

  PRInt32 cellSpacingTotal;
  // the following are of size NUM_WIDTHS, but only MIN_CON, DES_CON, FIX, PCT
  // are used and they account for colspan ADJusted values
  PRInt32 totalWidths[NUM_WIDTHS]; // sum of col widths of a particular type 
  PRInt32 totalCounts[NUM_WIDTHS]; // num of cols of a particular type
  PRInt32 minWidths[NUM_WIDTHS];
  PRInt32 num0Proportional;

  CalculateTotals(cellSpacingTotal, totalCounts, totalWidths, minWidths, num0Proportional);
  // auto width table's adjusted width needs cell spacing
  if (tableIsAutoWidth && perAdjTableWidth > 0) { 
    perAdjTableWidth = PR_MIN(perAdjTableWidth + cellSpacingTotal, maxWidth);
  }
  nscoord totalAllocated = totalWidths[MIN_CON] + cellSpacingTotal;
  
  // allocate and initialize arrays indicating what col gets set
  PRInt32* allocTypes = new PRInt32[mNumCols];
  if (!allocTypes) return PR_FALSE;
 
  for (colX = 0; colX < mNumCols; colX++) {
    allocTypes[colX] = -1;
  }

  // allocate percentage cols
  if (totalCounts[PCT] > 0) {
    if (totalAllocated + totalWidths[PCT] - minWidths[PCT] <= maxWidth) {
      AllocateFully(totalAllocated, allocTypes, PCT);
      NS_ASSERTION(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, PCT, totalCounts[PCT], 
                          totalWidths[PCT], PR_FALSE, allocTypes);
      return BCW_Wrapup(this, mTableFrame, allocTypes);
    }
  }
  // allocate fixed cols
  if (totalAllocated < maxWidth && totalCounts[FIX] > 0) {
    if (totalAllocated  + totalWidths[FIX] - minWidths[FIX] <= maxWidth) { 
      AllocateFully(totalAllocated, allocTypes, FIX);
      NS_ASSERTION(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, FIX, totalCounts[FIX], 
                          totalWidths[FIX], PR_TRUE, allocTypes);
      return BCW_Wrapup(this, mTableFrame, allocTypes);
    }
  }
  // allocate proportional cols up to their min proportional value
  if (totalAllocated < maxWidth && totalCounts[MIN_PRO] > 0) {
    if (totalAllocated + totalWidths[MIN_PRO] - minWidths[MIN_PRO] <= maxWidth) { 
      AllocateFully(totalAllocated, allocTypes, MIN_PRO, PR_FALSE);
      NS_ASSERTION(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, MIN_PRO, totalCounts[MIN_PRO], 
                          totalWidths[MIN_PRO], PR_FALSE, allocTypes);
      return BCW_Wrapup(this, mTableFrame, allocTypes);
    }
  }

  // allocate auto cols, considering even those that are proportional
  if (totalAllocated < maxWidth && totalCounts[DES_CON] > 0) {
    if (totalAllocated + totalWidths[DES_CON] - minWidths[DES_CON]<= maxWidth) { 
      AllocateFully(totalAllocated, allocTypes, DES_CON);
      NS_ASSERTION(totalAllocated <= maxWidth, "over allocated");
    }
    else {
      AllocateConstrained(maxWidth - totalAllocated, DES_CON, totalCounts[DES_CON], 
                          totalWidths[DES_CON], PR_FALSE, allocTypes);
      return BCW_Wrapup(this, mTableFrame, allocTypes);
    }
  }

  // if this is a nested table and pass1 reflow, we are done
  if (maxWidth == NS_UNCONSTRAINEDSIZE)  {
    return PR_TRUE;
  }

  // allocate the rest unconstrained
  PRBool skip0Proportional = totalCounts[DES_CON] > num0Proportional;
  if ( (tableIsAutoWidth && (perAdjTableWidth - totalAllocated > 0)) ||
       (!tableIsAutoWidth && (totalAllocated < maxWidth)) ) {
    if (totalCounts[PCT] != mNumCols) {
      PRBool onlyAuto = (totalCounts[DES_CON] > 0) && !mIsNavQuirksMode;
      for (colX = 0; colX < mNumCols; colX++) {
        if (PCT == allocTypes[colX]) {
          allocTypes[colX] = -1;
        }
        else if ((FIX == allocTypes[colX]) && onlyAuto) {
          allocTypes[colX] = -1;
        }
      }
    }
    if (tableIsAutoWidth) {
      AllocateUnconstrained(perAdjTableWidth - totalAllocated, allocTypes, skip0Proportional);
    }
    else {
      AllocateUnconstrained(maxWidth - totalAllocated, allocTypes, skip0Proportional);
    }
  }
  // give the proportional cols the rest up to the max width in quirks mode
  else if (tableIsAutoWidth && mIsNavQuirksMode && (totalCounts[MIN_PRO] > 0)) {
    for (colX = 0; colX < mNumCols; colX++) {
      if (DES_CON != allocTypes[colX]) {
        allocTypes[colX] = -1;
      }
    }
    AllocateUnconstrained(maxWidth - totalAllocated, allocTypes, skip0Proportional);
  }


  return BCW_Wrapup(this, mTableFrame, allocTypes);
}


// Allocate aWidthType values to all cols available in aIsAllocated 
void BasicTableLayoutStrategy::AllocateFully(nscoord&      aTotalAllocated,
                                             PRInt32*      aAllocTypes,
                                             PRInt32       aWidthType,
                                             PRBool        aMarkAllocated)
{
  for (PRInt32 colX = 0; colX < mNumCols; colX++) { 
    if (-1 != aAllocTypes[colX]) {
      continue;
    }
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    nscoord oldWidth = mTableFrame->GetColumnWidth(colX);
    nscoord newWidth = colFrame->GetWidth(aWidthType);
    // account for col span overrides with DES_CON and FIX
    if (DES_CON == aWidthType) {
      newWidth = PR_MAX(newWidth, colFrame->GetWidth(DES_ADJ));
    }
    else if (FIX == aWidthType) {
      newWidth = PR_MAX(newWidth, colFrame->GetWidth(FIX_ADJ));
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
                                                     PRBool   aSkip0Proportional)
{
  nscoord divisor = 0;
  PRInt32 numAvail = 0;
  PRInt32 colX;
  for (colX = 0; colX < mNumCols; colX++) { 
    if (-1 != aAllocTypes[colX]) {
      divisor += mTableFrame->GetColumnWidth(colX);
      numAvail++;
    }
  }
  for (colX = 0; colX < mNumCols; colX++) { 
    if (-1 != aAllocTypes[colX]) {
      if (aSkip0Proportional) {
        nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
        if (e0ProportionConstraint == colFrame->GetConstraint()) {
          continue;
        }
      }
      nscoord oldWidth = mTableFrame->GetColumnWidth(colX);
      float percent = (divisor == 0) 
        ? (1.0f / ((float)numAvail))
        : ((float)oldWidth) / ((float)divisor);
      nscoord addition = NSToCoordRound(((float)aAllocAmount) * percent);
      mTableFrame->SetColumnWidth(colX, oldWidth + addition);
    }
  }
}

// Determine min, desired, fixed, and proportional sizes for the cols and 
// and calculate min/max table width
PRBool BasicTableLayoutStrategy::AssignPreliminaryColumnWidths(nscoord aMaxWidth)
{
  PRBool rv = PR_FALSE;
  PRInt32 numRows = mTableFrame->GetRowCount();
  nscoord spacingX = mTableFrame->GetCellSpacingX();
  PRInt32 colX, rowX; 
  PRUint32 widthX;
  mCellSpacingTotal = 0;

  PRInt32 propTotal    = 0; // total of numbers of the type 1*, 2*, etc 
  PRInt32 propTotalMin = 0;
  PRInt32 numColsForColsAttr = 0; // Nav Quirks cols attribute for equal width cols
  if (NS_STYLE_TABLE_COLS_NONE != mCols) {
    numColsForColsAttr = (NS_STYLE_TABLE_COLS_ALL == mCols) ? mNumCols : mCols;
  }

  // For every column, determine it's min and desired width based on cell style
  // base on cells which do not span cols. Also, determine mCellSpacingTotal
  for (colX = 0; colX < mNumCols; colX++) { 
    nscoord minWidth = 0;
    nscoord desWidth = 0;
    nscoord fixWidth = WIDTH_NOT_SET;
    
    // Get column information
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    NS_ASSERTION(nsnull != colFrame, "bad col frame");
    colFrame->SetConstraint(eNoConstraint);

    if (mTableFrame->GetNumCellsOriginatingIn(colX) > 0) {
      mCellSpacingTotal += spacingX;
    }

    // Scan the cells in the col that have colspan = 1 and find the maximum
    // min, desired, and fixed cells.
    nsTableCellFrame* fixContributor = nsnull;
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
      desWidth = PR_MAX(desWidth, cellFrame->GetPass1DesiredSize().width);
      // see if the cell has a style width specified
      const nsStylePosition* cellPosition;
      cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)cellPosition);
      if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) {
        nscoord coordValue = cellPosition->mWidth.GetCoordValue();
        if (coordValue > 0) { // ignore if width == 0
          // need to add padding into fixed width
          const nsStyleSpacing* spacing;
          cellFrame->GetStyleData(eStyleStruct_Spacing,(const nsStyleStruct *&)spacing);
          nsMargin paddingMargin;
          spacing->CalcPaddingFor(cellFrame, paddingMargin); 
          nscoord newFixWidth = coordValue + paddingMargin.left + paddingMargin.right;
          if (newFixWidth > fixWidth) {
            fixWidth = newFixWidth;
            fixContributor = cellFrame;
          }
          fixWidth = PR_MAX(fixWidth, minWidth);
        }
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
      const nsStylePosition* colPosition;
      colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)colPosition);
      if (eStyleUnit_Proportional == colPosition->mWidth.GetUnit()) {
        proportion = colPosition->mWidth.GetIntValue();
      }
      else if (colX < numColsForColsAttr) {
        proportion = 1;
        if ((eStyleUnit_Percent == colPosition->mWidth.GetUnit()) &&
            (colPosition->mWidth.GetPercentValue() > 0.0f)) {
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
          nscoord minWidth = colFrame->GetWidth(MIN_CON);
          colFrame->SetWidth(DES_CON, minWidth);
          colFrame->SetWidth(MIN_PRO, minWidth);
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
    for (colX = 0; colX < mNumCols; colX++) { 
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
    for (colX = 0; colX < mNumCols; colX++) { 
      nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
      nscoord colProp = colFrame->GetWidth(MIN_PRO);
      if (colProp > 0) {
        nscoord minWidth = colFrame->GetWidth(MIN_CON);
        nscoord minProp = NSToCoordRound(((float)colProp * minPropTotal) / (float)propTotal);
        colFrame->SetWidth(MIN_PRO, minProp);
        colFrame->SetWidth(DES_CON, NSToCoordRound(((float)minProp) * mMinToDesProportionRatio));
      }
    }
  }

  // For each col, consider the cells originating in it with colspans > 1.
  // Adjust the cols that each cell spans if necessary. Iterate backwards 
  // so that nested and/or overlaping col spans handle the inner ones first, 
  // ensuring more accurated calculations.
  for (colX = mNumCols - 1; colX >= 0; colX--) { 
    for (rowX = 0; rowX < numRows; rowX++) {
      PRBool originates;
      PRInt32 colSpan;
      nsTableCellFrame* cellFrame = mTableFrame->GetCellInfoAt(rowX, colX, &originates, &colSpan);
      if (!originates || (1 == colSpan)) {
        continue;
      }
      nscoord cellWidths[NUM_WIDTHS];
      cellWidths[MIN_CON] = cellFrame->GetPass1MaxElementSize().width;
      cellWidths[DES_CON] = cellFrame->GetPass1DesiredSize().width;
      cellWidths[FIX] = WIDTH_NOT_SET;
      // see if the cell has a style width specified
      const nsStylePosition* cellPosition;
      cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)cellPosition);
      if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) {
        // need to add padding into fixed width
        const nsStyleSpacing* spacing;
        cellFrame->GetStyleData(eStyleStruct_Spacing,(const nsStyleStruct *&)spacing);
        nsMargin paddingMargin;
        spacing->CalcPaddingFor(cellFrame, paddingMargin); 
        nscoord padding = paddingMargin.left + paddingMargin.right;
        cellWidths[FIX] = cellPosition->mWidth.GetCoordValue() + padding;
        cellWidths[FIX] = PR_MAX(cellWidths[FIX], cellWidths[MIN_CON]);
      }

      // set MIN_ADJ, DES_ADJ, FIX_ADJ
      nscoord spanCellSpacing = 0;
      for (widthX = 0; widthX < NUM_MAJOR_WIDTHS; widthX++) {
        // skip des if there is a fix
        if ((DES_CON == widthX) && (cellWidths[FIX] > 0)) 
          continue; // FIX will take into account DES_CON
        nscoord spanTotal = 0;
        nscoord divisor   = 0;
        PRInt32 spanX;
        for (spanX = 0; spanX < colSpan; spanX++) {
          nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX + spanX);
          nscoord colWidth = PR_MAX(colFrame->GetWidth(widthX), 
                                    colFrame->GetWidth(widthX + NUM_MAJOR_WIDTHS));
          // need to get a contribution for every cell 
          colWidth = PR_MAX(colWidth, colFrame->GetMinWidth());
          spanTotal += colWidth;
          // accumulate divisor 
          if (widthX == FIX) {
            colWidth = PR_MAX(colWidth, colFrame->GetDesWidth());
          }
          else if (widthX == DES_CON) {
            nscoord fixWidth = colFrame->GetFixWidth();
            if (fixWidth > 0) {
              colWidth = fixWidth;
            }
            //colWidth = PR_MAX(colWidth, colFrame->GetFixWidth());
          }
          else if (widthX == MIN_CON) {
            colWidth = PR_MAX(colWidth, colFrame->GetFixWidth());
          }
          divisor += colWidth;
          if ((0 == widthX) && (spanX > 0) && (mTableFrame->GetNumCellsOriginatingIn(colX + spanX) > 0)) {
            spanCellSpacing += spacingX;
          }
        }
        spanTotal -= spanCellSpacing;
        nscoord cellWidth = cellWidths[widthX] - spanCellSpacing;
        if ((cellWidth > 0) && !((widthX == MIN_CON) && (cellWidth <= spanTotal))) {
          for (PRInt32 spanX = 0; spanX < colSpan; spanX++) {
            nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX + spanX);
            nscoord colWidth = PR_MAX(colFrame->GetWidth(widthX), 
                                      colFrame->GetWidth(widthX + NUM_MAJOR_WIDTHS));
            nscoord minWidth = colFrame->GetMinWidth();
            // accumulate numerator similarly to divisor
            colWidth = PR_MAX(colWidth, colFrame->GetMinWidth());
            if (widthX == FIX) {
              colWidth = PR_MAX(colWidth, colFrame->GetDesWidth());
            }
            else if (widthX == DES_CON) {
              nscoord fixWidth = colFrame->GetFixWidth();
              if (fixWidth > 0) {
                colWidth = fixWidth;
              }
              //colWidth = PR_MAX(colWidth, colFrame->GetFixWidth());
            }
            else if (widthX == MIN_CON) {
              colWidth = PR_MAX(colWidth, colFrame->GetFixWidth());
            }
            nscoord colAdjWidth = colFrame->GetWidth(widthX + NUM_MAJOR_WIDTHS);
            nscoord newColAdjWidth = (0 >= spanTotal) 
              ? NSToCoordRound( ((float)cellWidth) / ((float)colSpan) )
              : NSToCoordRound( (((float)colWidth) / ((float)divisor)) * cellWidth);
            if ((newColAdjWidth > colAdjWidth) && (newColAdjWidth > 0)) {
              newColAdjWidth = PR_MAX(newColAdjWidth, minWidth);
              if ((FIX != widthX) && (newColAdjWidth > colFrame->GetWidth(widthX))) {
                colFrame->SetWidth(widthX + NUM_MAJOR_WIDTHS, newColAdjWidth);
                colFrame->SetConstrainingCell(cellFrame);
              }
              // The next two conditions allows BalanceColumnWidths to assume that 
              // fixed allocations are a stopping point.
              // if new DES_CON exceeds FIX, set FIX_ADJ to DES_CON. 
              if (DES_CON == widthX) {
                nscoord fixWidth = colFrame->GetFixWidth();
                if ((fixWidth > 0) && (newColAdjWidth > fixWidth) && (newColAdjWidth > colFrame->GetWidth(FIX))) {
                  colFrame->SetWidth(FIX_ADJ, newColAdjWidth);
                  colFrame->SetConstrainingCell(cellFrame);
                }
              }
              // if new FIX_ADJ exceeds desired, set DES_ADJ to FIX_ADJ. 
              else if (FIX == widthX) {
                nscoord desWidth = colFrame->GetDesWidth();
                if ((newColAdjWidth > desWidth) && (newColAdjWidth > colFrame->GetWidth(DES_CON))) {
                  colFrame->SetWidth(DES_ADJ, newColAdjWidth);
                  colFrame->SetConstrainingCell(cellFrame);
                }
              }
            }
          }
        }
      }
    }
  }
  // if DES_ADJ exceeds FIX, pretend that FIX is not set. This allows 
  // BalanceColumnWidths to assume that fixed allocations are a stopping point.
  for (colX = 0; colX < mNumCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    nscoord desAdjWidth = colFrame->GetWidth(DES_ADJ);
    nscoord fixWidth = colFrame->GetWidth(FIX);
    if ((fixWidth >= 0) && (desAdjWidth > fixWidth)) {
      colFrame->SetWidth(FIX, WIDTH_NOT_SET);
      colFrame->SetConstrainingCell(nsnull);
    }
  }

  // Set the col's fixed width if present 
  // Set the table col width for each col to the content min. 
  for (colX = 0; colX < mNumCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    nscoord fixColWidth = colFrame->GetWidth(FIX);
    // use the style width of a col only if the col hasn't gotten a fixed width from any cell
    if (fixColWidth <= 0) {
      const nsStylePosition* colPosition;
      colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);
      if (eStyleUnit_Coord == colPosition->mWidth.GetUnit()) {
        fixColWidth = colPosition->mWidth.GetCoordValue();
        if (fixColWidth > 0) {
          colFrame->SetWidth(FIX, fixColWidth);
        }
      }
    }
    nscoord minWidth = colFrame->GetMinWidth();
    mTableFrame->SetColumnWidth(colX, minWidth);
  }
  SetMinAndMaxTableContentWidths();

  return rv;
}


// Determine percentage col widths for each col frame
nscoord BasicTableLayoutStrategy::AssignPercentageColumnWidths(nscoord aBasisIn,
                                                               PRBool  aTableIsAutoWidth)
{
  PRInt32 numRows = mTableFrame->GetRowCount();
  nscoord spacingX = mTableFrame->GetCellSpacingX();
  PRInt32 colX, rowX; 
  nscoord basis = aBasisIn;
  // For an auto table, determine the potentially new percent adjusted width based 
  // on percent cells/cols. 
  if (aTableIsAutoWidth) {
    nscoord fixWidthTotal = 0;
    basis = 0;
    PRInt32 numPerCols = 0;
    for (colX = 0; colX < mNumCols; colX++) { 
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
            nscoord desWidth = colFrame->GetDesWidth();
            if (colSpan > 1) { // sum up the DES_ADJ widths of the spanned cols
              for (PRInt32 spanX = 1; spanX < colSpan; spanX++) {
                nsTableColFrame* spanFrame = mTableFrame->GetColFrame(colX + spanX);
                desWidth += spanFrame->GetWidth(DES_ADJ);
              }
            }
            colBasis = NSToCoordRound((float)desWidth / percent);
          }
        }
      }
      if (-1 == colBasis) {
        // see if the col has a style percent width specified
        const nsStylePosition* colPosition;
        colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)colPosition);
        if (eStyleUnit_Percent == colPosition->mWidth.GetUnit()) {
          float percent = colPosition->mWidth.GetPercentValue();
          colBasis = 0;
          if (percent > 0.0f) {
            nscoord desWidth = colFrame->GetDesWidth();
            colBasis = NSToCoordRound((float)desWidth / percent);
          }
        }
      }
      basis = PR_MAX(basis, colBasis);
      if (colBasis >= 0) {
        numPerCols++;
      }
      fixWidthTotal += colFrame->GetFixWidth();
    } // end for (colX ..
    // If there is only one col and it is % based, it won't affect anything
    if ((1 == mNumCols) && (mNumCols == numPerCols)) {
      return 0;
    }
    basis = PR_MAX(basis, fixWidthTotal);
    basis = PR_MIN(basis, aBasisIn); 
  }

  nscoord colPctTotal = 0;
  nscoord* colPcts = new nscoord[mNumCols];
  if (!colPcts) return 0;
  
  // Determine the percentage contribution for cols and for cells with colspan = 1
  // Iterate backwards, similarly to the reasoning in AssignPreliminaryColumnWidths
  for (colX = mNumCols - 1; colX >= 0; colX--) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    nscoord maxColPctWidth = WIDTH_NOT_SET;
    float maxColPct = 0.0f;
    colPcts[colX] = 0;

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
            const nsStyleSpacing* spacing;
            cellFrame->GetStyleData(eStyleStruct_Spacing,(const nsStyleStruct *&)spacing);
            nsMargin paddingMargin;
            spacing->CalcPaddingFor(cellFrame, paddingMargin); 
            maxColPctWidth += paddingMargin.left + paddingMargin.right;
          }
        }
      }
    }
    if (WIDTH_NOT_SET == maxColPctWidth) {
      // see if the col has a style percent width specified
      const nsStylePosition* colPosition;
      colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct *&)colPosition);
      if (eStyleUnit_Percent == colPosition->mWidth.GetUnit()) {
        maxColPct = colPosition->mWidth.GetPercentValue();
        maxColPctWidth = NSToCoordRound( ((float)basis) * maxColPct );
      }
    }
    // conflicting pct/fixed widths are recorded. Nav 4.x may be changing the
    // fixed width value if it exceeds the pct value and not recording the pct
    // value. This is not being done and IE5 doesn't do it either.
    if (maxColPctWidth > 0) {
      nscoord minWidth = colFrame->GetMinWidth();
      if (minWidth > maxColPctWidth) {
        maxColPctWidth = minWidth;
        colPcts[colX] = NSToCoordRound( 100.0f * ((float)maxColPctWidth) / ((float)basis) );
      }
      else {
        colPcts[colX] = NSToCoordRound(maxColPct * 100.0f);
      }
      colFrame->SetWidth(PCT, maxColPctWidth);
      colFrame->SetConstrainingCell(percentContributor);
      colPctTotal += colPcts[colX];
    }
  }

  // For each col, consider the cells originating in it with colspans > 1.
  // Adjust the cols that each cell spans if necessary.
  for (colX = 0; colX < mNumCols; colX++) { 
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
        cellPctWidth = NSToCoordRound( ((float)basis) * cellPct );
        if (!mIsNavQuirksMode) { 
          // need to add padding 
          const nsStyleSpacing* spacing;
          cellFrame->GetStyleData(eStyleStruct_Spacing,(const nsStyleStruct *&)spacing);
          nsMargin paddingMargin;
          spacing->CalcPaddingFor(cellFrame, paddingMargin); 
          cellPctWidth += paddingMargin.left + paddingMargin.right;
        }
      }
      if (cellPctWidth > 0) {
        nscoord spanCellSpacing = 0;
        nscoord spanTotal = 0;
        nscoord colPctWidthTotal = 0;
        // accumulate the spanTotal as the max of MIN, DES, FIX, PCT
        for (PRInt32 spanX = 0; spanX < colSpan; spanX++) {
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
          if ((spanX > 0) && (mTableFrame->GetNumCellsOriginatingIn(colX + spanX) > 0)) {
            spanCellSpacing += spacingX;
          }
        }
        cellPctWidth += spanCellSpacing; // add it back in since it was subtracted from aBasisIn
        if (cellPctWidth <= 0) {
          continue;
        }
        if (colPctWidthTotal < cellPctWidth) { 
          // record the percent contributions for the spanned cols
          for (PRInt32 spanX = 0; spanX < colSpan; spanX++) {
            nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX + spanX);
            if (colFrame->GetWidth(PCT) > 0) { // skip pct cols
              continue;
            }
            nscoord minWidth = colFrame->GetMinWidth();
            nscoord colWidth = PR_MAX(minWidth, colFrame->GetFixWidth());
            colWidth = PR_MAX(colWidth, colFrame->GetDesWidth()); // XXX check this
            float colPctAdj = (0 == spanTotal) 
              ? cellPctWidth / ((float) colSpan)
              : cellPct * ((float)colWidth) / (float)spanTotal;
            if (colPctAdj > 0) {
              nscoord colPctAdjWidth = colFrame->GetWidth(PCT_ADJ);
              nscoord newColPctAdjWidth = NSToCoordRound(colPctAdj * (float)basis);
              if (newColPctAdjWidth > colPctAdjWidth) {
                if (colPctAdjWidth > 0) { // remove its contribution
                  colPctTotal -= colPcts[colX + spanX];
                }
                if (minWidth > newColPctAdjWidth) {
                  newColPctAdjWidth = minWidth;
                  colPcts[colX + spanX] = NSToCoordRound( 100.0f * ((float)newColPctAdjWidth) / ((float)basis) );
                }
                else {
                  colPcts[colX + spanX] = NSToCoordRound( 100.0f * colPctAdj );
                }
                if (newColPctAdjWidth > colFrame->GetWidth(PCT)) {
                  colFrame->SetWidth(PCT_ADJ, newColPctAdjWidth);
                  colFrame->SetConstrainingCell(cellFrame);
                }
                colPctTotal += colPcts[colX + spanX];
              }
            }
          }
        }
      }
    } // end for (rowX ..
  } // end for (colX ..

  // if the percent total went over 100%, adjustments need to be made to right most cols
  if (colPctTotal > 100) {
    for (colX = mNumCols - 1; colX >= 0; colX--) {
      if (colPcts[colX] > 0) {
        nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
        nscoord fixWidth = colFrame->GetFixWidth();
        nscoord pctWidth = colFrame->GetWidth(PCT);
        nscoord pctAdjWidth = colFrame->GetWidth(PCT_ADJ);
        nscoord newPct = colPcts[colX] - (colPctTotal - 100);
        if (newPct > 0) { // this col has enough percent alloc to handle it
          nscoord newPctWidth = NSToCoordRound( ((float)basis) * ((float)newPct) / 100.0f );
          newPctWidth = PR_MAX(newPctWidth, colFrame->GetMinWidth());
          // since we don't care which one contributed, set both
          colFrame->SetWidth(PCT, newPctWidth);
          colFrame->SetWidth(PCT_ADJ, newPctWidth);
          break;
        }
        else { // this col cannot handle all the reduction, reduce it down to zero
          colFrame->SetWidth(PCT,     WIDTH_NOT_SET);
          colFrame->SetWidth(PCT_ADJ, WIDTH_NOT_SET);
          colPctTotal -= colPcts[colX];
          if (colPctTotal <= 100) {
            break;
          }
        }
      }
    }
  }

  delete [] colPcts;
  return basis;
}

void BasicTableLayoutStrategy::SetMinAndMaxTableContentWidths()
{
  mMinTableContentWidth = 0;
  mMaxTableContentWidth = 0;

  nscoord spacingX = mTableFrame->GetCellSpacingX();
  for (PRInt32 colX = 0; colX < mNumCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    mMinTableContentWidth += colFrame->GetMinWidth();
    mMaxTableContentWidth += colFrame->GetDesWidth();
    if (mTableFrame->GetNumCellsOriginatingIn(colX) > 0) {
      mMaxTableContentWidth += spacingX;
      mMinTableContentWidth += spacingX;
    }
  }
  // if it is not a degenerate table, add the last spacing on the right
  if (mMinTableContentWidth > 0) {
    mMinTableContentWidth += spacingX;
    mMaxTableContentWidth += spacingX;
  }
}

// calculate totals by width type. A width type of a higher precedence will
// preclude one of a lower precedence for all types except MIN_CON
void BasicTableLayoutStrategy::CalculateTotals(PRInt32& aCellSpacing,
                                               PRInt32* aTotalCounts,
                                               PRInt32* aTotalWidths,
                                               PRInt32* aMinWidths,
                                               PRInt32& a0ProportionalCount)
{
  //mTableFrame->Dump(PR_TRUE, PR_FALSE);
  aCellSpacing = 0;
  for (PRInt32 widthX = 0; widthX < NUM_WIDTHS; widthX++) {
    aTotalCounts[widthX] = 0;
    aTotalWidths[widthX] = 0;
    aMinWidths[widthX]   = 0;
  }
  a0ProportionalCount = 0;

  nscoord spacingX = mTableFrame->GetCellSpacingX();
  for (PRInt32 colX = 0; colX < mNumCols; colX++) { 
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    if (mTableFrame->GetNumCellsOriginatingIn(colX) > 0) {
      aCellSpacing += spacingX;
    }
    nscoord minCol = colFrame->GetMinWidth();
    aTotalCounts[MIN_CON]++;
    aTotalWidths[MIN_CON] += minCol;

    if (e0ProportionConstraint == colFrame->GetConstraint()) {
      a0ProportionalCount++;
    }

    nscoord pctCol = colFrame->GetPctWidth();
    nscoord fixCol = colFrame->GetFixWidth();
    // pct wins in conflict with fix
    if (pctCol > 0) {
      aTotalCounts[PCT]++;
      aTotalWidths[PCT] += pctCol;
      aMinWidths[PCT] += minCol;
      continue;
    }
    else if (fixCol > 0) {
      aTotalCounts[FIX]++;
      aTotalWidths[FIX] += fixCol;
      aMinWidths[FIX] += minCol;
      continue;
    }

    if (eProportionConstraint == colFrame->GetConstraint()) {
      nscoord minProp = colFrame->GetWidth(MIN_PRO);
      aTotalCounts[MIN_PRO]++;
      aTotalWidths[MIN_PRO] += minProp;
      aTotalCounts[DES_CON]++;
      aTotalWidths[DES_CON] += NSToCoordRound(((float)minProp) * mMinToDesProportionRatio);
      aMinWidths[MIN_PRO] += minCol;
      aMinWidths[DES_CON] += minProp;
    }
    else {
      nscoord desCol = colFrame->GetDesWidth();
      aTotalCounts[DES_CON]++;
      aTotalWidths[DES_CON] += desCol;
      aMinWidths[DES_CON] += minCol;
    }
  }
  // if it is not a degenerate table, add the last spacing on the right
  if (mNumCols > 0) {
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
             PRInt32&    aAvailWidth)
{
  for (PRInt32 i = 0; i < aNumAutoCols; i++) {
    if ((aAvailWidth <= 0) || (aDivisor <= 0)) {
      break;
    }
    float percent = ((float)aColInfo[i]->mMaxWidth) / (float)aDivisor;
    aDivisor -= aColInfo[i]->mMaxWidth;
    nscoord addition = PR_MIN(aAvailWidth, NSToCoordRound(((float)(aAvailWidth)) * percent));
    addition = PR_MIN(addition, aColInfo[i]->mMaxWidth - aColInfo[i]->mWidth);
    // don't let the total additions exceed what is available
    if (i == aNumAutoCols - 1) {
      addition = PR_MIN(addition, aAvailWidth);
    }
    aColInfo[i]->mWidth += addition;
    aAvailWidth -= addition;
  }
}

void
AC_Decrease(PRInt32     aNumAutoCols,
             nsColInfo** aColInfo,
             PRInt32     aDivisor,
             PRInt32&    aExcess)
{
  for (PRInt32 i = 0; i < aNumAutoCols; i++) {
    if ((aExcess <= 0) || (aDivisor <= 0)) {
      break;
    }
    float percent = ((float)aColInfo[i]->mMaxWidth) / (float)aDivisor;
    aDivisor -= aColInfo[i]->mMaxWidth;
    nscoord reduction = PR_MIN(aExcess, NSToCoordRound(((float)(aExcess)) * percent));
    // don't go over the col min
    reduction = PR_MIN(reduction, aColInfo[i]->mWidth - aColInfo[i]->mMinWidth);
    // don't let the total reductions exceed what is available
    if (i == aNumAutoCols - 1) {
      reduction = PR_MIN(reduction, aExcess);
    }
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
                                                   PRInt32  aNumConstrainedCols,
                                                   PRInt32  aSumMaxConstraints,
                                                   PRBool   aStartAtMin,        
                                                   PRInt32* aAllocTypes)
{
  if ((0 == aAvailWidth) || (aWidthType < 0) || (aWidthType >= NUM_WIDTHS) || 
      (aNumConstrainedCols <= 0) || (aSumMaxConstraints <= 0)) {
    NS_ASSERTION(PR_TRUE, "invalid args to AllocateConstrained");
    return;
  }

  // allocate storage for the affected cols. Only they get adjusted.
  nsColInfo** colInfo = new nsColInfo*[aNumConstrainedCols];
  if (!colInfo) return;
  memset(colInfo, 0, aNumConstrainedCols * sizeof(nsColInfo *));

  PRInt32 maxMinDiff = 0;
  PRInt32 constrColX = 0;
  // set the col info entries for each constrained col
  for (PRInt32 colX = 0; colX < mNumCols; colX++) {
    if (-1 != aAllocTypes[colX]) {
      continue;
    }
    nsTableColFrame* colFrame = mTableFrame->GetColFrame(colX);
    //nscoord minWidth = colFrame->GetMinWidth();
    nscoord minWidth = mTableFrame->GetColumnWidth(colX);
    nscoord maxWidth = colFrame->GetWidth(aWidthType);
    if (DES_CON == aWidthType) {
      maxWidth = PR_MAX(maxWidth, colFrame->GetWidth(DES_ADJ));
    }
    else if (FIX == aWidthType) {
      maxWidth = PR_MAX(maxWidth, colFrame->GetWidth(FIX_ADJ));
    }
    else if (PCT == aWidthType) {
      maxWidth = PR_MAX(maxWidth, colFrame->GetWidth(PCT_ADJ));
    }
    if (maxWidth <= 0) {
      continue;
    }
    if (constrColX >= aNumConstrainedCols) {
      NS_ASSERTION(PR_FALSE, "AllocateConstrained called incorrectly");
      break;
    }
    maxWidth = PR_MAX(maxWidth, minWidth);
    maxMinDiff += maxWidth - minWidth;
    nscoord startWidth = (aStartAtMin) ? minWidth : maxWidth;
    colInfo[constrColX] = new nsColInfo(colFrame, colX, minWidth, startWidth, maxWidth);
    if (!colInfo[constrColX]) {
      AC_Wrapup(mTableFrame, aNumConstrainedCols, colInfo, PR_TRUE);
      return;
    }
    aAllocTypes[colX] = aWidthType;
    constrColX++;
  }
  if (constrColX < aNumConstrainedCols) {
    // some of the constrainted cols might have been 0 and skipped
    aNumConstrainedCols = constrColX;
  }
  PRInt32 i;
  if (aStartAtMin) { // allocate extra space 
    nscoord availWidth = aAvailWidth; 
    for (i = 0; i < aNumConstrainedCols; i++) {
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
    AC_Sort(colInfo, aNumConstrainedCols);
   
    // compute the proportion to be added to each column, don't go beyond the col's
    // max. This algorithm assumes that the Weight works as stated above
    AC_Increase(aNumConstrainedCols, colInfo, aSumMaxConstraints, availWidth);
  }
  else { // reduce each col width 
    nscoord reduceWidth = maxMinDiff - aAvailWidth;
    if (reduceWidth < 0) {
      NS_ASSERTION(PR_TRUE, "AllocateConstrained called incorrectly");
      AC_Wrapup(mTableFrame, aNumConstrainedCols, colInfo);
      return;
    }
    for (i = 0; i < aNumConstrainedCols; i++) {
      // the weight here is a relative metric for determining when cols reach their min. 
      // A col with a larger weight will reach its min before one with a smaller value.
      nscoord delta = colInfo[i]->mWidth - colInfo[i]->mMinWidth;
      colInfo[i]->mWeight = (delta <= 0) 
        ? 1000000 // cols which have already reached their min get a large value
        : ((float)colInfo[i]->mWidth) / ((float)delta);
    }
      
    // sort the cols based on the Weight 
    AC_Sort(colInfo, aNumConstrainedCols);
   
    // compute the proportion to be subtracted from each column, don't go beyond 
    // the col's min. This algorithm assumes that the Weight works as stated above
    AC_Decrease(aNumConstrainedCols, colInfo, aSumMaxConstraints, reduceWidth);
  }
  AC_Wrapup(mTableFrame, aNumConstrainedCols, colInfo);
}

// XXX this function will be improved after the colspan algorithms have been extracted
// from AssignPreliminarColumnWidths and AssignPercentageColumnWidths. For now, pessimistic
// assumptions are made
PRBool BasicTableLayoutStrategy::ColumnsCanBeInvalidatedBy(nsStyleCoord*           aPrevStyleWidth,
                                                           const nsTableCellFrame& aCellFrame,
                                                           PRBool                  aConsiderMinWidth) const
{
  if (aConsiderMinWidth || !mTableFrame) 
    return PR_TRUE;

  const nsStylePosition* cellPosition;
  aCellFrame.GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)cellPosition);
  const nsStyleCoord& styleWidth = cellPosition->mWidth;

  PRInt32 colIndex;
  aCellFrame.GetColIndex(colIndex);
  nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
  nscoord colSpan = mTableFrame->GetEffectiveColSpan(&aCellFrame);

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
    }
  }
  return PR_FALSE;
}


// XXX this function will be improved after the colspan algorithms have been extracted
// from AssignPreliminarColumnWidths and AssignPercentageColumnWidths. For now, pessimistic
// assumptions are made
PRBool BasicTableLayoutStrategy::ColumnsCanBeInvalidatedBy(const nsTableCellFrame& aCellFrame,
                                                           PRBool                  aConsiderMinWidth) const
{
  if (aConsiderMinWidth || !mTableFrame) 
    return PR_TRUE;

  const nsStylePosition* cellPosition;
  aCellFrame.GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)cellPosition);
  const nsStyleCoord& styleWidth = cellPosition->mWidth;

  PRInt32 colIndex;
  aCellFrame.GetColIndex(colIndex);
  nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
  nscoord colSpan = mTableFrame->GetEffectiveColSpan(&aCellFrame);

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
  const nsStylePosition* cellPosition;
  aCellFrame.GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)cellPosition);
  const nsStyleCoord& styleWidth = cellPosition->mWidth;

  PRInt32 colIndex;
  aCellFrame.GetColIndex(colIndex);
  nsTableColFrame* colFrame = mTableFrame->GetColFrame(colIndex);
  nscoord colSpan = mTableFrame->GetEffectiveColSpan(&aCellFrame);

  nscoord cellMin = aCellFrame.GetPass1MaxElementSize().width;
  nscoord cellDes = aCellFrame.GetPass1DesiredSize().width;
  nscoord colMin  = colFrame->GetMinWidth();
  nscoord colDes  = colFrame->GetDesWidth();

  PRBool minChanged = PR_TRUE;
  if (((cellMin > aPrevCellMin) && (cellMin <= colMin)) ||
      ((cellMin <= aPrevCellMin) && (aPrevCellMin <= colMin))) {
    minChanged = PR_FALSE;
  }
  if (minChanged) {
    return PR_TRUE; // XXX add cases where table has coord width and cell is constrained
  }

  PRBool desChanged = PR_FALSE;
  if (((cellDes > aPrevCellDes) && (cellDes <= colDes)) ||
      ((cellDes <= aPrevCellDes) && (aPrevCellDes <= colDes))) {
    desChanged = PR_FALSE;
  }
  
  if (1 == colSpan) {
    // see if the column width is constrained
    if ((colFrame->GetPctWidth() > 0) || (colFrame->GetFixWidth() > 0) ||
        (colFrame->GetWidth(MIN_PRO) > 0)) {
      if ((colFrame->GetWidth(PCT_ADJ) > 0) && (colFrame->GetWidth(PCT) <= 0)) {
        if (desChanged) {
          return PR_TRUE; // XXX add cases where table has coord width
        }
      }
      if ((colFrame->GetWidth(FIX_ADJ) > 0) && (colFrame->GetWidth(FIX) <= 0)) {
        if (desChanged) {
          return PR_TRUE; // its unfortunate that the balancing algorithms cause this
                          // XXX add cases where table has coord width
        }
      }
    }
    else { // the column width is not constrained
      if (desChanged) {
        return PR_TRUE;
      }
    }
  }
  else {
    return PR_TRUE; // XXX this needs a lot of cases
  }
  return PR_FALSE;
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
  const nsStylePosition* colPosition;
  colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);
  switch (colPosition->mWidth.GetUnit()) {
  case eStyleUnit_Coord:
    if (0 == colPosition->mWidth.GetCoordValue()) {
      result = PR_TRUE;
    }
    break;
  case eStyleUnit_Percent:
    {
      // total hack for now for 0% and 1% specifications
      // should compare percent to available parent width and see that it is below minimum
      // for this column
      float percent = colPosition->mWidth.GetPercentValue();
      if (0.0f == percent || 0.01f == percent) {  
        result = PR_TRUE;
      }
      break;
    }
  case eStyleUnit_Proportional:
    if (0 == colPosition->mWidth.GetIntValue()) {
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

  printf("%s**START BASIC STRATEGY DUMP** table=%X cols=%d numCols=%d",
    indent, mTableFrame, mCols, mNumCols);
  printf("\n%s minConWidth=%d maxConWidth=%d cellSpacing=%d propRatio=%.2f navQuirks=%d",
    indent, mMinTableContentWidth, mMaxTableContentWidth, mCellSpacingTotal, mMinToDesProportionRatio, mIsNavQuirksMode);
  printf(" **END BASIC STRATEGY DUMP** \n");
  delete [] indent;
}

