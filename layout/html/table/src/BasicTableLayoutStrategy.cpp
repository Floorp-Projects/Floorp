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
#include "nsIPtr.h"
#include "nsHTMLIIDs.h"

NS_DEF_PTR(nsIStyleContext);


#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsDebugCLD = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsDebugCLD = PR_FALSE;
#endif

/* ---------- ProportionalColumnLayoutStruct ---------- */  
// TODO: make public so other subclasses can use it

/** useful info about a column for layout */
struct ProportionalColumnLayoutStruct
{
  ProportionalColumnLayoutStruct(PRInt32 aColIndex, 
                                 nscoord aMinColWidth, 
                                 nscoord aMaxColWidth, 
                                 PRInt32 aProportion)
  {
    mColIndex = aColIndex;
    mMinColWidth = aMinColWidth;
    mMaxColWidth = aMaxColWidth;
    mProportion = aProportion;
  };

  PRInt32 mColIndex;
  nscoord mMinColWidth; 
  nscoord mMaxColWidth; 
  PRInt32 mProportion;
};


/* ---------- ColSpanStruct ---------- */
struct ColSpanStruct
{
  PRInt32 colIndex;
  PRInt32 colSpan;
  nscoord width;

  ColSpanStruct(PRInt32 aSpan, PRInt32 aIndex, nscoord aWidth)
  {
    colSpan = aSpan;
    colIndex = aIndex;
    width = aWidth;
  }
};




/* ---------- BasicTableLayoutStrategy ---------- */

/* return true if the style indicates that the width is proportional 
 * for the purposes of column width determination
 */
PRBool BasicTableLayoutStrategy::IsFixedWidth(const nsStylePosition* aStylePosition)
{
  PRBool result = PR_FALSE; // assume that it is not fixed width
  PRInt32 unitType;
  if (nsnull == aStylePosition) {
    result=PR_TRUE;
  }
  else {
    unitType = aStylePosition->mWidth.GetUnit();
    if (eStyleUnit_Coord==unitType)
      result=PR_TRUE;
  }

  return result;
}


BasicTableLayoutStrategy::BasicTableLayoutStrategy(nsTableFrame *aFrame, PRInt32 aNumCols)
{
  NS_ASSERTION(nsnull!=aFrame, "bad frame arg");

  mTableFrame = aFrame;
  mNumCols = aNumCols;
  mMinTableWidth=0;
  mMaxTableWidth=0;
  mFixedTableWidth=0;

  //cache the value of the cols attribute
  nsIFrame * tableFrame = mTableFrame;
// begin REMOVE_ME_WHEN_TABLE_STYLE_IS_RESOLVED!   XXX
  tableFrame->GetGeometricParent(tableFrame);
// end REMOVE_ME_WHEN_TABLE_STYLE_IS_RESOLVED!     XXX
  nsStyleTable *tableStyle;
  tableFrame->GetStyleData(eStyleStruct_Table, (nsStyleStruct *&)tableStyle);
  mCols = tableStyle->mCols;
}

BasicTableLayoutStrategy::~BasicTableLayoutStrategy()
{
}

PRBool BasicTableLayoutStrategy::Initialize(nsSize* aMaxElementSize)
{
#ifdef DEBUG
  nsIFrame *tablePIF=nsnull;
  mTableFrame->GetPrevInFlow(tablePIF);
  NS_ASSERTION(nsnull==tablePIF, "never ever call me on a continuing frame!");
#endif

  PRBool result = PR_TRUE;

  // Step 1 - assign the width of all fixed-width columns
  AssignPreliminaryColumnWidths();

  // set aMaxElementSize here because we compute mMinTableWidth in AssignPreliminaryColumnWidths
  if (nsnull!=aMaxElementSize)
  {
    aMaxElementSize->height = 0;
    aMaxElementSize->width = mMinTableWidth;
    if (PR_TRUE==gsDebug) 
      printf("BTLS::Init setting aMaxElementSize->width = %d\n", aMaxElementSize->width);
  }

  return result;
}

PRBool BasicTableLayoutStrategy::BalanceColumnWidths(nsIStyleContext *aTableStyle,
                                                     const nsReflowState& aReflowState,
                                                     nscoord aMaxWidth)
{
#ifdef DEBUG
  nsIFrame *tablePIF=nsnull;
  mTableFrame->GetPrevInFlow(tablePIF);
  NS_ASSERTION(nsnull==tablePIF, "never ever call me on a continuing frame!");
#endif

  PRBool result;

  NS_ASSERTION(nsnull!=aTableStyle, "bad arg");
  if (nsnull==aTableStyle)
    return PR_FALSE;


  nscoord specifiedTableWidth = 0; // not cached as a data member because it can vary depending on aMaxWidth
  PRBool tableIsAutoWidth = nsTableFrame::TableIsAutoWidth(mTableFrame, aTableStyle, aReflowState, specifiedTableWidth);

  // Step 2 - determine how much space is really available
  PRInt32 availWidth = aMaxWidth - mFixedTableWidth;
  if (PR_FALSE==tableIsAutoWidth)
    availWidth = specifiedTableWidth - mFixedTableWidth;
  
  // Step 3 - assign the width of all proportional-width columns in the remaining space
  if (PR_TRUE==gsDebug) 
  {
    printf ("BalanceColumnWidths with aMaxWidth = %d, availWidth = %d\n", aMaxWidth, availWidth);
    printf ("\t\t specifiedTW = %d, min/maxTW = %d %d\n", specifiedTableWidth, mMinTableWidth, mMaxTableWidth);
  }
  if (PR_TRUE==gsDebug)
  {
    printf("\n%p: BEGIN BALANCE COLUMN WIDTHS\n", mTableFrame);
    for (PRInt32 i=0; i<mNumCols; i++)
      printf("  col %d assigned width %d\n", i, mTableFrame->GetColumnWidth(i));
    printf("\n");
  }
  result = BalanceProportionalColumns(aReflowState, availWidth, aMaxWidth,
                                      specifiedTableWidth, tableIsAutoWidth);

  if (PR_TRUE==gsDebug)
  {
    printf("\n%p: END BALANCE COLUMN WIDTHS\n", mTableFrame);
    for (PRInt32 i=0; i<mNumCols; i++)
      printf("  col %d assigned width %d\n", i, mTableFrame->GetColumnWidth(i));
    printf("\n");
  }
  return result;
}

// Step 1 - assign the width of all fixed-width columns, all other columns get there max, 
//          and calculate min/max table width
PRBool BasicTableLayoutStrategy::AssignPreliminaryColumnWidths()
{
  if (gsDebug==PR_TRUE) printf ("** %p: AssignPreliminaryColumnWidths **\n", mTableFrame);
  nsVoidArray *spanList=nsnull;
  nsVoidArray *colSpanList=nsnull;

  PRBool hasColsAttribute = (PRBool)(NS_STYLE_TABLE_COLS_NONE!=mCols);
  
  PRInt32 *minColWidthArray = nsnull;   // used for computing the effect of COLS attribute
  PRInt32 *maxColWidthArray = nsnull;   // used for computing the effect of COLS attribute
  if (PR_TRUE==hasColsAttribute)
  { 
    minColWidthArray = new PRInt32[mNumCols];
    maxColWidthArray = new PRInt32[mNumCols];
  }

  PRInt32 numRows = mTableFrame->GetRowCount();

  // for every column, determine it's min and max width, and keep track of the table width
  for (PRInt32 colIndex = 0; colIndex<mNumCols; colIndex++)
  { 
    nscoord minColWidth = 0;
    nscoord maxColWidth = 0;
    nscoord effectiveMinColumnWidth = 0;  // min col width ignoring cells with colspans
    nscoord effectiveMaxColumnWidth = 0;  // max col width ignoring cells with colspans

    // Get column information
    nsTableColFrame *colFrame = mTableFrame->GetColFrame(colIndex);
    NS_ASSERTION(nsnull!=colFrame, "bad col frame");

    // Get the columns's style
    const nsStylePosition* colPosition;
    colFrame->GetStyleData(eStyleStruct_Position, (nsStyleStruct*&)colPosition);

    // Get fixed column width if it has one
    PRBool haveColWidth = PR_FALSE;
    nscoord specifiedFixedColWidth=0;
    if (eStyleUnit_Coord==colPosition->mWidth.GetUnit()) 
    {
      haveColWidth = PR_TRUE;
      specifiedFixedColWidth = colPosition->mWidth.GetCoordValue();
    }

    /* Scan the column, simulatneously assigning column widths
     * and computing the min/max column widths
     */
    // first, deal with any cells that span into this column from a pervious column
    if (nsnull!=spanList)
    {
      PRInt32 spanCount = spanList->Count();
      // go through the list backwards so we can delete easily
      for (PRInt32 spanIndex=spanCount-1; 0<=spanIndex; spanIndex--)
      {
        SpanInfo *spanInfo = (SpanInfo *)(spanList->ElementAt(spanIndex));
        nscoord cellMinWidth = (spanInfo->cellMinWidth)/(spanInfo->initialColSpan);
        if (minColWidth < cellMinWidth)
          minColWidth = cellMinWidth;
        nscoord cellDesiredWidth = (spanInfo->cellDesiredWidth)/(spanInfo->initialColSpan);
        if (maxColWidth < cellDesiredWidth)
          maxColWidth = cellDesiredWidth;
        spanInfo->span--;
        if (gsDebug==PR_TRUE) 
          printf ("    for spanning cell %d with remaining span=%d, min = %d and  des = %d\n", 
                  spanIndex, spanInfo->span, cellMinWidth, cellDesiredWidth);
        if (0==spanInfo->span)
        {
          spanList->RemoveElementAt(spanIndex);
          delete spanInfo;
        }
      }
    }

    PRInt32 firstRowIndex = -1;
    PRInt32 maxColSpan = 1;
    for (PRInt32 rowIndex = 0; rowIndex<numRows; rowIndex++)
    {
      nsTableCellFrame * cellFrame = mTableFrame->GetCellAt(rowIndex, colIndex);
      if (nsnull==cellFrame)
      { // there is no cell in this row that corresponds to this column
        continue;
      }
      if (-1==firstRowIndex)
        firstRowIndex = rowIndex;
      if (rowIndex!=cellFrame->GetRowIndex()) {
        // For cells that span rows, we only figure it in once
        NS_ASSERTION(1 != cellFrame->GetRowSpan(), "row index does not match row span");  // sanity check
        continue;
      }
      PRInt32 colSpan = mTableFrame->GetEffectiveColSpan(colIndex, cellFrame);
      if (colSpan>maxColSpan)
        maxColSpan = colSpan;
      if (colIndex!=cellFrame->GetColIndex()) {
        // For cells that span cols, we figure in the row using previously-built SpanInfo 
        NS_ASSERTION(1 != cellFrame->GetColSpan(), "col index does not match row span");  // sanity check
        continue;
      }

      nsSize cellMinSize = cellFrame->GetPass1MaxElementSize();
      nsSize cellDesiredSize = cellFrame->GetPass1DesiredSize();
      nscoord cellDesiredWidth = cellDesiredSize.width/colSpan;
      if (gsDebug==PR_TRUE) 
        printf ("  for cell %d with colspan=%d, min = %d,%d  and  des = %d,%d\n", 
                rowIndex, colSpan, cellMinSize.width, cellMinSize.height,
                cellDesiredSize.width, cellDesiredSize.height);

      if (PR_TRUE==haveColWidth) 
      {
        // This col has a specified fixed width so set the minand max width to the larger of 
        // (specified width, largest max_element_size of the cells in the column)
        nscoord widthForThisCell = PR_MAX(cellMinSize.width, colPosition->mWidth.GetCoordValue());
        widthForThisCell = widthForThisCell/colSpan;
        mTableFrame->SetColumnWidth(colIndex, widthForThisCell);
        maxColWidth = widthForThisCell;
        minColWidth = widthForThisCell;
        if ((1==colSpan) && (effectiveMaxColumnWidth < widthForThisCell))
          effectiveMaxColumnWidth = widthForThisCell;
        if (gsDebug) 
          printf ("  setting min and max col width to specified fixed width %d\n", widthForThisCell);
      }
      else
      {
        if (maxColWidth < cellDesiredWidth)
          maxColWidth = cellDesiredWidth;
        if ((1==colSpan) && (effectiveMaxColumnWidth < maxColWidth))
          effectiveMaxColumnWidth = cellDesiredWidth;
      }

      // cellMinWidth can override fixed width, so factor it in here
      nscoord cellMinWidth = cellMinSize.width/colSpan;
      if (minColWidth < cellMinWidth)
        minColWidth = cellMinWidth;
      // effectiveMinColumnWidth is the min width as if no cells with colspans existed
      if ((1==colSpan) && (effectiveMinColumnWidth < cellMinWidth))
        effectiveMinColumnWidth = cellMinWidth;
      if (1<colSpan)
      {
        // add the column to our list of post-process columns
        ColSpanStruct *colSpanInfo = new ColSpanStruct(colSpan, colIndex, cellDesiredSize.width);
        if (nsnull==colSpanList)
          colSpanList = new nsVoidArray();
        colSpanList->AppendElement(colSpanInfo);

        // add the cell to our list of spanning cells
        SpanInfo *spanInfo = new SpanInfo(colSpan-1, cellMinWidth, cellDesiredWidth);
        if (nsnull==spanList)
          spanList = new nsVoidArray();
        spanList->AppendElement(spanInfo);
      }
      if (gsDebug) {
        printf ("    after cell %d, minColWidth = %d and maxColWidth = %d\n",
                rowIndex, minColWidth, maxColWidth);
      }
    } // end rowIndex for loop

    // do all the global bookkeeping, factoring in margins
    nscoord colInset = mTableFrame->GetCellSpacing();
    // keep a running total of the amount of space taken up by all fixed-width columns
    if (PR_TRUE==haveColWidth)
      mFixedTableWidth += specifiedFixedColWidth + (colInset*maxColSpan);
    if (gsDebug) {
      printf ("    after col %d, mFixedTableWidth = %d\n", colIndex, mFixedTableWidth);
    }

    // cache the computed column info
    colFrame->SetMinColWidth(minColWidth);
    colFrame->SetMaxColWidth(maxColWidth);
    colFrame->SetEffectiveMinColWidth(effectiveMinColumnWidth);
    colFrame->SetEffectiveMaxColWidth(effectiveMaxColumnWidth);
    mTableFrame->SetColumnWidth(colIndex, maxColWidth);

    // add col[i] metrics to the running totals for the table min/max width
    if (NS_UNCONSTRAINEDSIZE!=mMinTableWidth)
      mMinTableWidth += minColWidth + colInset;
    if (mMinTableWidth<=0) 
      mMinTableWidth = NS_UNCONSTRAINEDSIZE; // handle overflow
    if (NS_UNCONSTRAINEDSIZE!=mMaxTableWidth)
      mMaxTableWidth += maxColWidth + colInset;
    if (mMaxTableWidth<=0) 
      mMaxTableWidth = NS_UNCONSTRAINEDSIZE; // handle overflow
    if (PR_TRUE==hasColsAttribute)
    {
      minColWidthArray[colIndex] = minColWidth;
      maxColWidthArray[colIndex] = maxColWidth;
    }
    if (gsDebug==PR_TRUE) 
      printf ("  after col %d, minTableWidth = %d and maxTableWidth = %d\n", 
              colIndex, mMinTableWidth, mMaxTableWidth);
  }

  // now, post-process the computed values based on the table attributes
  // if there is a COLS attribute, fix up mMinTableWidth and mMaxTableWidth
  if (PR_TRUE==hasColsAttribute)
  {
    // for every effected column, subtract out its prior contribution and add back in the new value
    PRInt32 numColsEffected = mNumCols;
    if (NS_STYLE_TABLE_COLS_ALL!=mCols)
      numColsEffected = mCols;
    PRInt32 maxOfMinColWidths=0;
    PRInt32 maxOfMaxColWidths=0;
    PRInt32 effectedColIndex;
    for (effectedColIndex=0; effectedColIndex<numColsEffected; effectedColIndex++)
    {
      if (maxOfMinColWidths < minColWidthArray[effectedColIndex])
        maxOfMinColWidths = minColWidthArray[effectedColIndex];
      if (maxOfMaxColWidths < maxColWidthArray[effectedColIndex])
        maxOfMaxColWidths = maxColWidthArray[effectedColIndex];
    }
    for (effectedColIndex=0; effectedColIndex<numColsEffected; effectedColIndex++)
    {
      // subtract out the prior contributions of this column
      // and add back in the adjusted value
      if (NS_UNCONSTRAINEDSIZE!=mMinTableWidth)
      {
        mMinTableWidth -= minColWidthArray[effectedColIndex];
        mMinTableWidth += maxOfMinColWidths;
      }
      if (NS_UNCONSTRAINEDSIZE!=mMaxTableWidth)
      {
        mMaxTableWidth -= maxColWidthArray[effectedColIndex];
        mMaxTableWidth += maxOfMaxColWidths;
      }
      nsTableColFrame *colFrame;
      mTableFrame->GetColumnFrame(effectedColIndex, colFrame);
      colFrame->SetMaxColWidth(maxOfMaxColWidths);  // cache the new column max width (min width is uneffected)
      colFrame->SetEffectiveMaxColWidth(maxOfMaxColWidths);
    }
    delete [] minColWidthArray;
    delete [] maxColWidthArray;
  }
  if (nsnull!=colSpanList)
    DistributeFixedSpace(colSpanList);

  if (PR_TRUE==gsDebug)
    printf ("%p: aMinTW=%d, aMaxTW=%d\n", mTableFrame, mMinTableWidth, mMaxTableWidth);

  // clean up
  if (nsnull!=spanList)
    delete spanList;
  if (nsnull!=colSpanList)
  {
    PRInt32 colSpanListCount = colSpanList->Count();
    for (PRInt32 i=0; i<colSpanListCount; i++)
    {
      ColSpanStruct * colSpanInfo = (ColSpanStruct *)(colSpanList->ElementAt(i));
      if (nsnull!=colSpanInfo)
        delete colSpanInfo;
    }
    delete colSpanList;
  }
  return PR_TRUE;
}

// take the fixed space spanned by the columns in aColSpanList 
// and distribute it proportionately (based on desired width)
void BasicTableLayoutStrategy::DistributeFixedSpace(nsVoidArray *aColSpanList)
{
  nscoord excess = 0;
  if (PR_TRUE==gsDebug) printf ("** DistributeFixedSpace:\n");
  // for all fixed-width columns, determine the amount of the specified width each column spanned recieves
  PRInt32 numSpanningCells = aColSpanList->Count();
  for (PRInt32 nextSpanningCell=0; nextSpanningCell<numSpanningCells; nextSpanningCell++)
  { // proportionately distributed extra space, based on the column's fixed width
    ColSpanStruct * colInfo = (ColSpanStruct *)aColSpanList->ElementAt(nextSpanningCell);
    PRInt32 colIndex = colInfo->colIndex;  
    PRInt32 colSpan = colInfo->colSpan;  
    nscoord totalColWidth = colInfo->width;
    
    // 1. get the sum of the effective widths of the columns in the span
    nscoord totalEffectiveWidth=0;
    nsTableColFrame * colFrame;
    PRInt32 i;
    for (i = 0; i<colSpan; i++)
    {
      mTableFrame->GetColumnFrame(colIndex+i, colFrame);
      totalEffectiveWidth += colFrame->GetEffectiveMaxColWidth();
    }

    // 2. next, compute the proportion to be added to each column, and add it
    for (i = 0; i<colSpan; i++)
    {
      mTableFrame->GetColumnFrame(colIndex+i, colFrame);
      float percent;
      percent = ((float)(colFrame->GetEffectiveMaxColWidth()))/((float)totalEffectiveWidth);
      nscoord colWidth = (nscoord)(totalColWidth*percent);
      if (gsDebug==PR_TRUE) 
        printf("  assigning fixed col width for spanning cells:  column %d set to %d\n", 
               colIndex+i, colWidth);
      mTableFrame->SetColumnWidth(colIndex+i, colWidth);
      colFrame->SetMaxColWidth(colWidth);
    }
  }
}

/* assign column widths to all non-fixed-width columns (adjusting fixed-width columns if absolutely necessary)
 aAvailWidth is the amount of space left in the table to distribute after fixed-width columns are accounted for
 aMaxWidth is the space the parent gave us (minus border & padding) to fit ourselves into
 aTableIsAutoWidth is true if the table is auto-width, false if it is anything else (percent, fixed, etc)
 */
PRBool BasicTableLayoutStrategy::BalanceProportionalColumns(const nsReflowState& aReflowState,
                                                            nscoord aAvailWidth,
                                                            nscoord aMaxWidth,
                                                            nscoord aTableSpecifiedWidth,
                                                            PRBool  aTableIsAutoWidth)
{
  PRBool result = PR_TRUE;

  nscoord actualMaxWidth; // the real target width, depends on if we're auto or specified width
  if (PR_TRUE==aTableIsAutoWidth)
    actualMaxWidth = aMaxWidth;
  else
    actualMaxWidth = aTableSpecifiedWidth;

  if (NS_UNCONSTRAINEDSIZE==aMaxWidth  ||  NS_UNCONSTRAINEDSIZE==mMinTableWidth)
  { // the max width of the table fits comfortably in the available space
    if (gsDebug) printf ("  * auto table laying out in NS_UNCONSTRAINEDSIZE, calling BalanceColumnsTableFits\n");
    result = BalanceColumnsTableFits(aReflowState, aAvailWidth, 
                                     aMaxWidth, aTableSpecifiedWidth, aTableIsAutoWidth);
  }
  else if (mMinTableWidth > actualMaxWidth)
  { // the table doesn't fit in the available space
    if (gsDebug) printf ("  * auto table minTW does not fit, calling SetColumnsToMinWidth\n");
    result = SetColumnsToMinWidth();
  }
  else if (mMaxTableWidth <= actualMaxWidth)
  { // the max width of the table fits comfortably in the available space
    if (gsDebug) printf ("  * auto table desired size fits, calling BalanceColumnsTableFits\n");
    result = BalanceColumnsTableFits(aReflowState, aAvailWidth, 
                                     aMaxWidth, aTableSpecifiedWidth, aTableIsAutoWidth);
  }
  else
  { // the table fits somewhere between its min and desired size
    if (gsDebug) printf ("  * auto table desired size does not fit, calling BalanceColumnsConstrained\n");
    result = BalanceColumnsConstrained(aReflowState, aAvailWidth,
                                       actualMaxWidth);
  }

  return result;
}

// the table doesn't fit, so squeeze every column down to its minimum
PRBool BasicTableLayoutStrategy::SetColumnsToMinWidth()
{
  PRBool result = PR_TRUE;

  PRBool hasColsAttribute = (PRBool)(NS_STYLE_TABLE_COLS_NONE!=mCols);
  
  PRInt32 *minColWidthArray = nsnull;
  if (PR_TRUE==hasColsAttribute)
  {
    minColWidthArray = new PRInt32[mNumCols];
  }

  PRInt32 numRows = mTableFrame->GetRowCount();
  for (PRInt32 colIndex = 0; colIndex<mNumCols; colIndex++)
  { 
    PRInt32 minColWidth = 0;
    PRInt32 maxColWidth = 0;
    if (gsDebug==PR_TRUE) printf ("  for col %d\n", colIndex);

    nsTableColFrame *colFrame = mTableFrame->GetColFrame(colIndex);
    NS_ASSERTION(nsnull!=colFrame, "bad col frame");

    // Get the columns's style
    const nsStylePosition* colPosition;
    colFrame->GetStyleData(eStyleStruct_Position, (nsStyleStruct*&)colPosition);
    if (PR_FALSE==IsFixedWidth(colPosition))
    {
      minColWidth = colFrame->GetMinColWidth();
      mTableFrame->SetColumnWidth(colIndex, minColWidth);
      if (PR_TRUE==hasColsAttribute)
      {
        minColWidthArray[colIndex] = minColWidth;
      }
    }
    if (gsDebug==PR_TRUE) 
      printf ("  2: col %d, set to width = %d\n", colIndex, mTableFrame->GetColumnWidth(colIndex));
  }

  // now, post-process the computed values based on the table attributes
  // if there is a COLS attribute, fix up mMinTableWidth and mMaxTableWidth
  if (PR_TRUE==hasColsAttribute)
  { // for every effected column, subtract out its prior contribution and add back in the new value
    PRInt32 numColsEffected = mNumCols;
    if (NS_STYLE_TABLE_COLS_ALL!=mCols)
      numColsEffected = mCols;
    PRInt32 maxOfEffectedColWidths=0;
    PRInt32 effectedColIndex;
    // XXX need to fix this and all similar code if any fixed-width columns intersect COLS
    for (effectedColIndex=0; effectedColIndex<numColsEffected; effectedColIndex++)
    {
      if (maxOfEffectedColWidths < minColWidthArray[effectedColIndex])
        maxOfEffectedColWidths = minColWidthArray[effectedColIndex];
    }
    for (effectedColIndex=0; effectedColIndex<numColsEffected; effectedColIndex++)
    {
      // set each effected column to the size of the largest column in the group
      mTableFrame->SetColumnWidth(effectedColIndex, maxOfEffectedColWidths);
      if (PR_TRUE==gsDebug)
        printf("  2 (cols): setting %d to %d\n", effectedColIndex, maxOfEffectedColWidths); 
    }
    // we're guaranteed here that minColWidthArray has been allocated, and that
    // if we don't get here, it was never allocated
    delete [] minColWidthArray;
  }
  return result;
}

/* the table fits in the given space.  Set all columns to their desired width,
 * and if we are not an auto-width table add extra space to fluff out the total width
 */
PRBool BasicTableLayoutStrategy::BalanceColumnsTableFits(const nsReflowState& aReflowState,
                                                         nscoord aAvailWidth,
                                                         nscoord aMaxWidth,
                                                         nscoord aTableSpecifiedWidth,
                                                         PRBool  aTableIsAutoWidth)
{
  PRBool  result = PR_TRUE;
  nscoord tableWidth=0;               // the width of the table as a result of setting column widths
  nscoord widthOfFixedTableColumns=0; // the sum of the width of all fixed-width columns plus margins
                                      // tableWidth - widthOfFixedTableColumns is the width of columns computed in this method
  PRInt32 totalSlices=0;              // the total number of slices the proportional-width columns request
  nsVoidArray *proportionalColumnsList=nsnull; // a list of the columns that are proportional-width
  nsVoidArray *spanList=nsnull;       // a list of the cells that span columns
  PRInt32 numRows = mTableFrame->GetRowCount();

  for (PRInt32 colIndex = 0; colIndex<mNumCols; colIndex++)
  { 
    if (gsDebug==PR_TRUE) printf ("for col %d\n", colIndex);
    PRInt32 minColWidth = 0;
    PRInt32 maxColWidth = 0;
    // Get column information
    nsTableColFrame *colFrame = mTableFrame->GetColFrame(colIndex);
    NS_ASSERTION(nsnull!=colFrame, "bad col frame");
    PRInt32 rowIndex;
    PRInt32 firstRowIndex = -1;
    nscoord colInset = mTableFrame->GetCellSpacing();

    const nsStylePosition* colPosition;
    colFrame->GetStyleData(eStyleStruct_Position, (nsStyleStruct*&)colPosition);    
    if (PR_FALSE==IsFixedWidth(colPosition))
    {
      // first, deal with any cells that span into this column from a pervious column
      if (nsnull!=spanList)
      {
        PRInt32 spanCount = spanList->Count();
        // go through the list backwards so we can delete easily
        for (PRInt32 spanIndex=spanCount-1; 0<=spanIndex; spanIndex--)
        {
          SpanInfo *spanInfo = (SpanInfo *)(spanList->ElementAt(spanIndex));
          if (minColWidth < spanInfo->cellMinWidth)
            minColWidth = spanInfo->cellMinWidth;
          if (maxColWidth < spanInfo->cellDesiredWidth)
            maxColWidth = spanInfo->cellDesiredWidth;
          spanInfo->span--;
          if (gsDebug==PR_TRUE) 
            printf ("    for spanning cell %d with remaining span=%d, min = %d and  des = %d\n", 
                    spanIndex, spanInfo->span, spanInfo->cellMinWidth, spanInfo->cellDesiredWidth);
          if (0==spanInfo->span)
          {
            spanList->RemoveElementAt(spanIndex);
            delete spanInfo;
          }
        }
      }

      for (rowIndex = 0; rowIndex<numRows; rowIndex++)
      { // this col has proportional width, so determine its width requirements
        nsTableCellFrame * cellFrame = mTableFrame->GetCellAt(rowIndex, colIndex);
        if (nsnull==cellFrame)
        { // there is no cell in this row that corresponds to this column
          continue;
        }
        if (-1==firstRowIndex)
          firstRowIndex = rowIndex;
        if (rowIndex!=cellFrame->GetRowIndex()) {
          // For cells that span rows, we only figure it in once
          NS_ASSERTION(1 != cellFrame->GetRowSpan(), "row index does not match row span");  // sanity check
          continue;
        }
        if (colIndex!=cellFrame->GetColIndex()) {
          // For cells that span cols, we figure in the row using previously-built SpanInfo 
          NS_ASSERTION(1 != cellFrame->GetColSpan(), "col index does not match row span");  // sanity check
          continue;
        }
 
        PRInt32 colSpan = mTableFrame->GetEffectiveColSpan(colIndex, cellFrame);
        // distribute a portion of the spanning cell's min and max width to this column
        nsSize cellMinSize = cellFrame->GetPass1MaxElementSize();
        nsSize cellDesiredSize = cellFrame->GetPass1DesiredSize();
        
        nscoord cellMinWidth = cellMinSize.width/colSpan;
        // first get the desired size info from reflow pass 1
        nscoord cellDesiredWidth = cellDesiredSize.width/colSpan;  
        // then get the desired size info factoring in the cell style attributes
        nscoord specifiedCellWidth=-1;
        const nsStylePosition* cellPosition;
        cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)cellPosition);
        if (eStyleUnit_Percent==cellPosition->mWidth.GetUnit()) 
        { //QQQ what if table is auto width?
          float percent = cellPosition->mWidth.GetPercentValue();
          specifiedCellWidth = (PRInt32)(aTableSpecifiedWidth*percent);
          if (gsDebug) printf("specified percent width %f of %d = %d\n", 
                              percent, aTableSpecifiedWidth, specifiedCellWidth);
        }
        if (-1!=specifiedCellWidth)
        {
          if (specifiedCellWidth/colSpan>cellMinWidth)
          {
            if (gsDebug) printf("setting cellDesiredWidth from %d to %d\n", 
                                 cellDesiredWidth, specifiedCellWidth/colSpan);
            cellDesiredWidth = specifiedCellWidth/colSpan;
            //QQQ should be proportionately distributed as a post-process
          }
        }

        if (PR_TRUE==gsDebug)
          printf("factoring in cell %d with colSpan=%d\n  factoring in min=%d and desired=%d\n", 
                 rowIndex, colSpan, cellMinWidth, cellDesiredWidth);
        if (minColWidth < cellMinWidth)
          minColWidth = cellMinWidth;
        if (maxColWidth < cellDesiredWidth)
          maxColWidth = cellDesiredWidth;
        if (gsDebug==PR_TRUE) 
          printf ("    after cell %d, minColWidth=%d  maxColWidth=%d  effColWidth[%d]=%d\n", 
                  rowIndex, minColWidth, maxColWidth, 
                  colIndex, colFrame->GetEffectiveMaxColWidth());
        if ((1<colSpan) && (cellFrame->GetColIndex()==colIndex))
        { // add this cell to span list iff we are currently processing the column the cell starts in
          SpanInfo *spanInfo = new SpanInfo(colSpan-1, cellMinWidth, cellDesiredWidth);
          if (nsnull==spanList)
            spanList = new nsVoidArray();
          spanList->AppendElement(spanInfo);
        }
      } // end looping through cells in the column

      if (gsDebug==PR_TRUE) 
      {
        printf ("  for determining width of col %d %s:\n",
                colIndex, !IsFixedWidth(colPosition)? "(P)":"(A)");
        printf ("    minColWidth = %d and maxColWidth = %d\n", minColWidth, maxColWidth);
        printf ("    aAvailWidth = %d\n", aAvailWidth);
      }

      // Get column width if it has one
      nscoord specifiedProportionColumnWidth = -1;
      float specifiedPercentageColWidth = -1.0f;
      nscoord specifiedFixedColumnWidth = -1;
      PRBool isAutoWidth = PR_FALSE;
      switch (colPosition->mWidth.GetUnit()) {
        case eStyleUnit_Percent:
          specifiedPercentageColWidth = colPosition->mWidth.GetPercentValue();
          if (gsDebug) printf("column %d has specified percent width = %f\n", colIndex, specifiedPercentageColWidth);
          break;
        case eStyleUnit_Proportional:
          specifiedProportionColumnWidth = colPosition->mWidth.GetIntValue();
          if (gsDebug) printf("column %d has specified percent width = %d\n", colIndex, specifiedProportionColumnWidth);
          break;
        case eStyleUnit_Auto:
          isAutoWidth = PR_TRUE;
          break;
        default:
          break;
      }

      /* set the column width, knowing that the table fits in the available space */
      if (0==specifiedProportionColumnWidth || 0.0==specifiedPercentageColWidth)
      { // col width is specified to be the minimum
        mTableFrame->SetColumnWidth(colIndex, minColWidth);
        if (gsDebug==PR_TRUE) 
          printf ("  3 min: col %d set to min width = %d because style set proportionalWidth=0\n", 
                  colIndex, mTableFrame->GetColumnWidth(colIndex));
      }
      else if (PR_TRUE==isAutoWidth)
      {  // col width is determined by the cells' content,
         // so give each remaining column it's desired width (because we know we fit.)
         // if there is width left over, we'll factor that in after this loop is complete
        mTableFrame->SetColumnWidth(colIndex, maxColWidth);
        if (gsDebug==PR_TRUE) 
          printf ("  3 auto: col %d with availWidth %d, set to width = %d\n", 
                  colIndex, aAvailWidth, mTableFrame->GetColumnWidth(colIndex));
      }
      else if (-1!=specifiedProportionColumnWidth)
      { // we need to save these and do them after all other columns have been calculated
        /* the calculation will be: 
              sum up n, the total number of slices for the columns with proportional width
              compute the table "required" width, fixed-width + percentage-width +
                the sum of the proportional column's max widths (especially because in this routine I know the table fits)
              compute the remaining width: the required width - the used width (fixed + percentage)
              compute the width per slice
              set the width of each proportional-width column to it's number of slices * width per slice
        */
        mTableFrame->SetColumnWidth(colIndex, 0);   // set the column width to 0, since it isn't computed yet
        if (nsnull==proportionalColumnsList)
          proportionalColumnsList = new nsVoidArray();
        ProportionalColumnLayoutStruct * info = 
          new ProportionalColumnLayoutStruct(colIndex, minColWidth, 
                                             maxColWidth, specifiedProportionColumnWidth);
        proportionalColumnsList->AppendElement(info);
        totalSlices += specifiedProportionColumnWidth;  // keep track of the total number of proportions
        if (gsDebug==PR_TRUE) 
          printf ("  3 proportional: col %d with availWidth %d, gets %d slices with %d slices so far.\n", 
                  colIndex, aAvailWidth, specifiedProportionColumnWidth, totalSlices);
      }
      else
      {  // give the column a percentage of the remaining space
        PRInt32 percentage = -1;
        if (NS_UNCONSTRAINEDSIZE==aAvailWidth)
        { // since the "remaining space" is infinite, give the column it's max requested size
          mTableFrame->SetColumnWidth(colIndex, maxColWidth);
        }
        else
        {
          if (-1.0f != specifiedPercentageColWidth)
          {
            percentage = (PRInt32)(specifiedPercentageColWidth*100.0f); // TODO: rounding errors?
            // base the % on the total specified fixed width of the table
            mTableFrame->SetColumnWidth(colIndex, (percentage*aTableSpecifiedWidth)/100);
            if (gsDebug==PR_TRUE) 
              printf ("  3 percent specified: col %d given %d percent of aTableSpecifiedWidth %d, set to width = %d\n", 
                      colIndex, percentage, aTableSpecifiedWidth, mTableFrame->GetColumnWidth(colIndex));
          }
          if (-1==percentage)
          {
            percentage = 100/mNumCols;
            // base the % on the remaining available width 
            mTableFrame->SetColumnWidth(colIndex, (percentage*aAvailWidth)/100);
            if (gsDebug==PR_TRUE) 
              printf ("  3 percent default: col %d given %d percent of aAvailWidth %d, set to width = %d\n", 
                      colIndex, percentage, aAvailWidth, mTableFrame->GetColumnWidth(colIndex));
          }
          // if the column was computed to be too small, enlarge the column
          if (mTableFrame->GetColumnWidth(colIndex) <= minColWidth)
          {
            mTableFrame->SetColumnWidth(colIndex, minColWidth);
            if (PR_TRUE==gsDebug) printf("  enlarging column to it's minimum = %d\n", minColWidth);
          }
        }
      }
    }
    else
    { // need to maintain this so we know how much we have left over at the end
      widthOfFixedTableColumns += colFrame->GetMaxColWidth() + colInset;
    }
    tableWidth += mTableFrame->GetColumnWidth(colIndex) + colInset;
  }

  /* --- post-process if necessary --- */
  // first, assign a width to proportional-width columns
  if (nsnull!=proportionalColumnsList)
  {
    // first, figure out the amount of space per slice
    nscoord maxWidthForTable = (0!=aTableSpecifiedWidth) ? aTableSpecifiedWidth : aMaxWidth;
    if (NS_UNCONSTRAINEDSIZE!=maxWidthForTable)
    {
      nscoord widthRemaining = maxWidthForTable - tableWidth;
      nscoord widthPerSlice = widthRemaining/totalSlices;
      PRInt32 numSpecifiedProportionalColumns = proportionalColumnsList->Count();
      for (PRInt32 i=0; i<numSpecifiedProportionalColumns; i++)
      { // for every proportionally-sized column, set the col width to the computed width
        ProportionalColumnLayoutStruct * info = 
          (ProportionalColumnLayoutStruct *)(proportionalColumnsList->ElementAt(i));
        nscoord computedColWidth = info->mProportion*widthPerSlice;
        mTableFrame->SetColumnWidth(info->mColIndex, computedColWidth);
        if (gsDebug==PR_TRUE) 
          printf ("  3 proportional step 2: col %d given %d proportion of remaining space %d, set to width = %d\n", 
                  info->mColIndex, info->mProportion, widthRemaining, computedColWidth);
        tableWidth += computedColWidth;
//        effectiveColumnWidth = computedColWidth;
        delete info;
      }
    }
    else
    { // we're in an unconstrained situation, so give each column the max of the max column widths
      PRInt32 numSpecifiedProportionalColumns = proportionalColumnsList->Count();
      PRInt32 maxOfMaxColWidths = 0;
      PRInt32 i;
      for (i=0; i<numSpecifiedProportionalColumns; i++)
      {
        ProportionalColumnLayoutStruct * info = 
          (ProportionalColumnLayoutStruct *)(proportionalColumnsList->ElementAt(i));
        if (maxOfMaxColWidths < info->mMaxColWidth)
          maxOfMaxColWidths = info->mMaxColWidth;
      }
      for (i=0; i<numSpecifiedProportionalColumns; i++)
      {
        ProportionalColumnLayoutStruct * info = 
          (ProportionalColumnLayoutStruct *)(proportionalColumnsList->ElementAt(i));
        mTableFrame->SetColumnWidth(info->mColIndex, maxOfMaxColWidths);
        if (gsDebug==PR_TRUE) 
          printf ("  3 proportional step 2 (unconstrained!): col %d set to width = %d\n", 
                  info->mColIndex, maxOfMaxColWidths);
        tableWidth += maxOfMaxColWidths;
        nsTableColFrame *colFrame;
        mTableFrame->GetColumnFrame(info->mColIndex, colFrame);
        colFrame->SetEffectiveMaxColWidth(maxOfMaxColWidths);
        delete info;
      }
    }
    delete proportionalColumnsList;
  }

  // next, if the specified width of the table is greater than the table's computed width, expand the
  // table's computed width to match the specified width, giving the extra space to proportionately-sized
  // columns if possible.
  if ((PR_FALSE==aTableIsAutoWidth) && (aAvailWidth > (tableWidth-widthOfFixedTableColumns)))
  {
    DistributeExcessSpace(aAvailWidth, tableWidth, widthOfFixedTableColumns);
  }
  if (nsnull!=spanList)
    delete spanList;
  return result;
}

// take the extra space in the table and distribute it proportionately (based on desired width)
// give extra space to auto-width cells first, or if there are none to all cells
void BasicTableLayoutStrategy::DistributeExcessSpace(nscoord  aAvailWidth,
                                                     nscoord  aTableWidth, 
                                                     nscoord  aWidthOfFixedTableColumns)
{
  nscoord excess = 0;
  if (PR_TRUE==gsDebug) printf ("DistributeExcessSpace: fixed width %d > computed table width %d, woftc=%d\n",
                                aAvailWidth, aTableWidth, aWidthOfFixedTableColumns);
  nscoord  widthMinusFixedColumns = aTableWidth - aWidthOfFixedTableColumns;
  // if there are auto-sized columns, give them the extra space
  // the trick here is to do the math excluding non-auto width columns
  PRInt32 numAutoColumns=0;
  PRInt32 *autoColumns=nsnull;
  mTableFrame->GetColumnsByType(eStyleUnit_Auto, numAutoColumns, autoColumns);
  if (0!=numAutoColumns)
  {
    // there's at least one auto-width column, so give it (them) the extra space
    // proportionately distributed extra space, based on the column's desired size
    nscoord totalEffectiveWidthOfAutoColumns = 0;
    if (gsDebug==PR_TRUE) 
      printf("  aAvailWidth specified as %d, expanding columns by excess = %d\n", aAvailWidth, excess);
    // 1. first, get the total width of the auto columns
    PRInt32 i;
    for (i = 0; i<numAutoColumns; i++)
    {
      nsTableColFrame *colFrame;
      mTableFrame->GetColumnFrame(autoColumns[i], colFrame);
      nscoord effectiveColWidth = colFrame->GetEffectiveMaxColWidth();
      if (0!=effectiveColWidth)
        totalEffectiveWidthOfAutoColumns += effectiveColWidth;
      else
        totalEffectiveWidthOfAutoColumns += mTableFrame->GetColumnWidth(autoColumns[i]);
    }
    // excess is the amount of space that was available minus the computed available width
    // QQQ shouldn't it just be aMaxWidth - aTableWidth???
    excess = aAvailWidth - widthMinusFixedColumns;

    // 2. next, compute the proportion to be added to each column, and add it
    for (i = 0; i<numAutoColumns; i++)
    {
      PRInt32 colIndex = autoColumns[i];
      nsTableColFrame *colFrame;
      mTableFrame->GetColumnFrame(colIndex, colFrame);
      nscoord oldColWidth = mTableFrame->GetColumnWidth(colIndex);
      float percent;
      if (0!=totalEffectiveWidthOfAutoColumns)
        percent = ((float)(colFrame->GetEffectiveMaxColWidth()))/((float)totalEffectiveWidthOfAutoColumns);
      else
        percent = ((float)1)/((float)numAutoColumns);
      nscoord excessForThisColumn = (nscoord)(excess*percent);
      nscoord colWidth = excessForThisColumn+oldColWidth;
      if (gsDebug==PR_TRUE) 
        printf("  distribute excess to auto columns:  column %d was %d, now set to %d\n", 
               colIndex, oldColWidth, colWidth);
      mTableFrame->SetColumnWidth(colIndex, colWidth);
    }
  }
  // otherwise, distribute the space between all the columns 
  // (they must be all fixed and percentage-width columns, or we would have gone into the block above)
  else
  {
    excess = aAvailWidth - widthMinusFixedColumns;
    if (gsDebug==PR_TRUE) 
      printf("  aAvailWidth specified as %d, expanding columns by excess = %d\n", 
             aAvailWidth, excess);
    for (PRInt32 colIndex = 0; colIndex<mNumCols; colIndex++)
    {
      nscoord oldColWidth=0;
      if (0==oldColWidth)
        oldColWidth = mTableFrame->GetColumnWidth(colIndex);
      float percent;
      if (0!=aTableWidth)
        percent = (float)oldColWidth/(float)aTableWidth;
      else
        percent = (float)1/(float)mNumCols;
      nscoord excessForThisColumn = (nscoord)(excess*percent);
      nscoord colWidth = excessForThisColumn+oldColWidth;
      if (gsDebug==PR_TRUE) 
        printf("  distribute excess to all columns:  column %d was %d, now set to %d from % = %f\n", 
               colIndex, oldColWidth, colWidth, percent);
      mTableFrame->SetColumnWidth(colIndex, colWidth);
    }
  }
}

/* assign columns widths for a table whose max size doesn't fit in the available space
 */
PRBool BasicTableLayoutStrategy::BalanceColumnsConstrained( const nsReflowState& aReflowState,
                                                            nscoord aAvailWidth,
                                                            nscoord aMaxWidth)
{
#ifdef DEBUG
  nsIFrame *tablePIF=nsnull;
  mTableFrame->GetPrevInFlow(tablePIF);
  NS_ASSERTION(nsnull==tablePIF, "never ever call me on a continuing frame!");
#endif

  PRBool result = PR_TRUE;
  PRInt32 maxOfAllMinColWidths = 0;   // for the case where we have equal column widths, this is the smallest a column can be
  nscoord tableWidth;                 // the width of the table as a result of setting column widths
  tableWidth = aMaxWidth-aAvailWidth; // the starting table width is the sum of fixed-width columns
  PRInt32 totalSlices=0;              // the total number of slices the proportional-width columns request
  nsVoidArray *proportionalColumnsList=nsnull; // a list of the columns that are proportional-width
  PRBool equalWidthColumns = PR_TRUE; // remember if we're in the special case where all
                                      // proportional-width columns are equal (if any are anything other than 1)
  PRBool atLeastOneAutoWidthColumn = PR_FALSE;  // true if at least one column is auto-width, requiring us to post-process
  nsVoidArray *spanList=nsnull;       // a list of the cells that span columns
  PRInt32 numRows = mTableFrame->GetRowCount();

  for (PRInt32 colIndex = 0; colIndex<mNumCols; colIndex++)
  { 
    if (gsDebug==PR_TRUE) printf ("  for col %d\n", colIndex);
    nscoord minColWidth = 0;
    nscoord maxColWidth = 0;
    
    // Get column information
    nsTableColFrame *colFrame = mTableFrame->GetColFrame(colIndex);
    NS_ASSERTION(nsnull!=colFrame, "bad col frame");
    // Get the columns's style and margins
    PRInt32 rowIndex;
    PRInt32 firstRowIndex = -1;
    nscoord colInset = mTableFrame->GetCellSpacing();
    const nsStylePosition* colPosition;
    colFrame->GetStyleData(eStyleStruct_Position, (nsStyleStruct*&)colPosition);

    if (PR_FALSE==IsFixedWidth(colPosition))
    {
      // first, deal with any cells that span into this column from a pervious column
      if (nsnull!=spanList)
      {
        PRInt32 spanCount = spanList->Count();
        // go through the list backwards so we can delete easily
        for (PRInt32 spanIndex=spanCount-1; 0<=spanIndex; spanIndex--)
        {
          SpanInfo *spanInfo = (SpanInfo *)(spanList->ElementAt(spanIndex));
          if (minColWidth < spanInfo->cellMinWidth)
            minColWidth = spanInfo->cellMinWidth;
          if (maxColWidth < spanInfo->cellDesiredWidth)
            maxColWidth = spanInfo->cellDesiredWidth;
          spanInfo->span--;
          if (gsDebug==PR_TRUE) 
            printf ("    for spanning cell %d with remaining span=%d, min = %d and  des = %d\n", 
                    spanIndex, spanInfo->span, spanInfo->cellMinWidth, spanInfo->cellDesiredWidth);
          if (0==spanInfo->span)
          {
            spanList->RemoveElementAt(spanIndex);
            delete spanInfo;
          }
        }
      }
      for (rowIndex = 0; rowIndex<numRows; rowIndex++)
      {
        nsTableCellFrame * cellFrame = mTableFrame->GetCellAt(rowIndex, colIndex);
        if (nsnull==cellFrame)
        { // there is no cell in this row that corresponds to this column
          continue;
        }
        if (rowIndex!=cellFrame->GetRowIndex()) {
          // For cells that span rows, we only figure it in once
          NS_ASSERTION(1 != cellFrame->GetRowSpan(), "row index does not match row span");  // sanity check
          continue;
        }
        if (colIndex!=cellFrame->GetColIndex()) {
          // For cells that span cols, we figure in the row using previously-built SpanInfo 
          NS_ASSERTION(1 != cellFrame->GetColSpan(), "col index does not match row span");  // sanity check
          continue;
        }

        PRInt32 colSpan = mTableFrame->GetEffectiveColSpan(colIndex, cellFrame);
        nsSize cellMinSize = cellFrame->GetPass1MaxElementSize();
        nsSize cellDesiredSize = cellFrame->GetPass1DesiredSize();

        PRInt32 cellMinWidth = cellMinSize.width/colSpan;
        // first get the desired size info from reflow pass 1
        PRInt32 cellDesiredWidth = cellDesiredSize.width/colSpan;
        // then get the desired size info factoring in the cell style attributes
        nscoord specifiedCellWidth=-1;
        const nsStylePosition* cellPosition;
        cellFrame->GetStyleData(eStyleStruct_Position, (nsStyleStruct*&)cellPosition);
        switch (cellPosition->mWidth.GetUnit()) {
          case eStyleUnit_Percent: 
          {
            nscoord knownTableWidth=0;
            float percent = cellPosition->mWidth.GetPercentValue();
            specifiedCellWidth = (PRInt32)(aMaxWidth*percent);
            if (gsDebug) printf("specified percent width %f of %d = %d\n", 
                                percent, aMaxWidth, specifiedCellWidth);
            break;
          }
        }
        if (-1!=specifiedCellWidth)
        {
          if (specifiedCellWidth>cellMinWidth)
          {
            if (gsDebug) printf("setting cellDesiredWidth from %d to %d\n", 
                                 cellDesiredWidth, specifiedCellWidth);
            cellDesiredWidth = specifiedCellWidth;    // TODO:  some math needed here for colspans
          }
        }

        if (PR_TRUE==gsDebug)
          printf("factoring in cell %d with colSpan=%d\n  factoring in min=%d and desired=%d\n", 
                 rowIndex, colSpan, cellMinWidth, cellDesiredWidth);
        // remember the widest min cell width
        if (minColWidth < cellMinWidth)
          minColWidth = cellMinWidth;
        // remember the max desired cell width
        if (maxColWidth < cellDesiredWidth)
          maxColWidth = cellDesiredWidth;
        // effectiveMaxColumnWidth is the width as if no cells with colspans existed
        if ((1==colSpan) && (colFrame->GetEffectiveMaxColWidth() < maxColWidth))
          colFrame->SetEffectiveMaxColWidth(cellDesiredWidth);
        if (gsDebug==PR_TRUE) 
          printf ("    after cell %d, minColWidth=%d  maxColWidth=%d  effColWidth[%d]=%d,%d\n", 
                  rowIndex, minColWidth, maxColWidth, 
                  colIndex, colFrame->GetEffectiveMinColWidth(), colFrame->GetEffectiveMaxColWidth());
        if ((1<colSpan) && (cellFrame->GetColIndex()==colIndex))
        { // if this cell spans columns and we are processing the column that owns the cell
          // add the cell to our list of spanners
          SpanInfo *spanInfo = new SpanInfo(colSpan-1, cellMinWidth, cellDesiredWidth);
          if (nsnull==spanList)
            spanList = new nsVoidArray();
          spanList->AppendElement(spanInfo);
        }
      }

      if (gsDebug==PR_TRUE) 
      {
        printf ("  for determining width of col %d %s:\n",
                colIndex, !IsFixedWidth(colPosition)? "(P)":"(A)");
        printf ("    minTableWidth = %d and maxTableWidth = %d\n", 
                mMinTableWidth, mMaxTableWidth);
        printf ("    minColWidth = %d, maxColWidth = %d, eff=%d,%d\n", 
                minColWidth, maxColWidth, colFrame->GetEffectiveMinColWidth(), 
                colFrame->GetEffectiveMaxColWidth());
        printf ("    aAvailWidth = %d\n", aAvailWidth);
      }

      // Get column width if it has one
      nscoord specifiedProportionColumnWidth = -1;
      float specifiedPercentageColWidth = -1.0f;
      nscoord specifiedFixedColumnWidth = -1;
      PRBool isAutoWidth = PR_FALSE;
      switch (colPosition->mWidth.GetUnit()) {
        case eStyleUnit_Percent:
          specifiedPercentageColWidth = colPosition->mWidth.GetPercentValue();
          if (gsDebug) printf("column %d has specified percent width = %f\n", colIndex, specifiedPercentageColWidth);
          break;
        case eStyleUnit_Proportional:
          specifiedProportionColumnWidth = colPosition->mWidth.GetIntValue();
          if (gsDebug) printf("column %d has specified percent width = %d\n", colIndex, specifiedProportionColumnWidth);
          break;
        case eStyleUnit_Auto:
          isAutoWidth = PR_TRUE;
          break;
        default:
          break;
      }

      /* set the column width, knowing that the table fits in the available space */
      if (0==specifiedProportionColumnWidth || 0.0==specifiedPercentageColWidth)
      { // col width is specified to be the minimum
        mTableFrame->SetColumnWidth(colIndex, minColWidth);
        if (gsDebug==PR_TRUE) 
          printf ("  4 (0): col %d set to min width = %d because style set proportionalWidth=0\n", 
                  colIndex, mTableFrame->GetColumnWidth(colIndex));
      }
      else if (1==mNumCols)
      { // there is only one column, and we know that it's desired width doesn't fit
        // so the column should be as wide as the available space allows it to be
        if (gsDebug==PR_TRUE) printf ("  4 one-column:  col %d  set to width = %d\n", colIndex, aAvailWidth);
        mTableFrame->SetColumnWidth(colIndex, aAvailWidth);
        if (gsDebug==PR_TRUE) 
          printf ("  4 one-col:  col %d set to width = %d from available width %d\n", 
                  colIndex, mTableFrame->GetColumnWidth(colIndex), aAvailWidth);
      }
      else if (PR_TRUE==isAutoWidth)
      { // column's width is determined by its content, done in post-processing
        mTableFrame->SetColumnWidth(colIndex, 0); // set to 0 width for now
        atLeastOneAutoWidthColumn = PR_TRUE;
      }
      else if (-1!=specifiedProportionColumnWidth)
      { // we need to save these and do them after all other columns have been calculated
        /* the calculation will be: 
              sum up n, the total number of slices for the columns with proportional width
              compute the table "required" width, fixed-width + percentage-width +
                the sum of the proportional column's max widths (especially because in this routine I know the table fits)
              compute the remaining width: the required width - the used width (fixed + percentage)
              compute the width per slice
              set the width of each proportional-width column to it's number of slices * width per slice
        */
        mTableFrame->SetColumnWidth(colIndex, 0);   // set the column width to 0, since it isn't computed yet
        if (nsnull==proportionalColumnsList)
          proportionalColumnsList = new nsVoidArray();
        ProportionalColumnLayoutStruct * info = 
          new ProportionalColumnLayoutStruct(colIndex, minColWidth, maxColWidth, specifiedProportionColumnWidth);
        proportionalColumnsList->AppendElement(info);
        totalSlices += specifiedProportionColumnWidth;
        if (1!=specifiedProportionColumnWidth)
          equalWidthColumns = PR_FALSE;
      }
      else
      {  // give the column a percentage of the remaining space
        PRInt32 percentage = -1;
        if (NS_UNCONSTRAINEDSIZE==aAvailWidth)
        { // since the "remaining space" is infinite, give the column it's max requested size
          mTableFrame->SetColumnWidth(colIndex, maxColWidth);
        }
        else
        {
          if (-1.0f != specifiedPercentageColWidth)
          {
            percentage = (PRInt32)(specifiedPercentageColWidth*100.0f); // TODO: rounding errors?
            // base the % on the total specified fixed width of the table
            mTableFrame->SetColumnWidth(colIndex, (percentage*aMaxWidth)/100);
            if (gsDebug==PR_TRUE) 
              printf ("  4 percent specified: col %d given %d percent of aMaxWidth %d, set to width = %d\n", 
                      colIndex, percentage, aMaxWidth, mTableFrame->GetColumnWidth(colIndex));
          }
          if (-1==percentage)
          {
            percentage = 100/mNumCols;
            // base the % on the remaining available width 
            mTableFrame->SetColumnWidth(colIndex, (percentage*aAvailWidth)/100);
            if (gsDebug==PR_TRUE) 
              printf ("  4 percent default: col %d given %d percent of aAvailWidth %d, set to width = %d\n", 
                      colIndex, percentage, aAvailWidth, mTableFrame->GetColumnWidth(colIndex));
          }
          // if the column was computed to be too small, enlarge the column
          if (mTableFrame->GetColumnWidth(colIndex) <= minColWidth)
          {
            mTableFrame->SetColumnWidth(colIndex, minColWidth);
            if (PR_TRUE==gsDebug) printf("  enlarging column to it's minimum = %d\n", minColWidth);
            if (maxOfAllMinColWidths < minColWidth)
            {
              maxOfAllMinColWidths = minColWidth;
              if (PR_TRUE==gsDebug)
                printf("   and setting maxOfAllMins to %d\n", maxOfAllMinColWidths);
            }
          }
        }
      }
      tableWidth += mTableFrame->GetColumnWidth(colIndex);
    }
  }
  /* --- post-process if necessary --- */
  // first, assign autoWidth columns a width
  if (PR_TRUE==atLeastOneAutoWidthColumn)
  { // proportionately distribute the remaining space to autowidth columns
    DistributeRemainingSpace(aAvailWidth, tableWidth);
  }
  
  // second, fix up tables where column width attributes give us a table that is too wide or too narrow
  nscoord computedWidth=0;
  for (PRInt32 i=0; i<mNumCols; i++) {
    computedWidth += mTableFrame->GetColumnWidth(i);
  }
  if (computedWidth<tableWidth) {
    AdjustTableThatIsTooNarrow(computedWidth, tableWidth);
  }
  else if (computedWidth>aMaxWidth) {
    AdjustTableThatIsTooWide(computedWidth, aMaxWidth);
  }


  // finally, assign a width to proportional-width columns
  if (nsnull!=proportionalColumnsList)
  {
    // first, figure out the amount of space per slice
    nscoord widthRemaining = aMaxWidth - tableWidth;
    nscoord widthPerSlice = widthRemaining/totalSlices;
    PRInt32 numSpecifiedProportionalColumns = proportionalColumnsList->Count();
    for (PRInt32 i=0; i<numSpecifiedProportionalColumns; i++)
    {
      ProportionalColumnLayoutStruct * info = 
        (ProportionalColumnLayoutStruct *)(proportionalColumnsList->ElementAt(i));
      if (PR_TRUE==equalWidthColumns && 0!=maxOfAllMinColWidths)
      {
        if (gsDebug==PR_TRUE) 
          printf("  EqualColWidths specified and some column couldn't fit, so setting col %d width to %d\n", 
                 info->mColIndex, maxOfAllMinColWidths);
        mTableFrame->SetColumnWidth(info->mColIndex, maxOfAllMinColWidths);
      }
      else
      {
        nscoord computedColWidth = info->mProportion*widthPerSlice;
        mTableFrame->SetColumnWidth(info->mColIndex, computedColWidth);
        if (gsDebug==PR_TRUE) 
          printf ("  4 proportion: col %d given %d proportion of remaining space %d, set to width = %d\n", 
                  info->mColIndex, info->mProportion, widthRemaining, computedColWidth);
      }
      delete info;
    }
    delete proportionalColumnsList;
  }

  // clean up
  if (nsnull!=spanList)
  {
    NS_ASSERTION(0==spanList->Count(), "space leak, span list not empty");
    delete spanList;
  }

  return result;
}

//XXX: make sure aTFW takes into account fixed width cells already assigned
// take the remaining space in the table and distribute it proportionately 
// to the auto-width cells in the table (based on desired width)
void BasicTableLayoutStrategy::DistributeRemainingSpace(nscoord  aTableSpecifiedWidth,
                                                        nscoord  aComputedTableWidth)
{
  if (PR_TRUE==gsDebug) 
    printf ("DistributeRemainingSpace: fixed width %d > computed table width %d\n",
            aTableSpecifiedWidth, aComputedTableWidth);
  // if there are auto-sized columns, give them the extra space
  PRInt32 numAutoColumns=0;
  PRInt32 *autoColumns=nsnull;
  // availWidth is the difference between the total available width and the 
  // amount of space already assigned, assuming auto col widths were assigned 0.
  nscoord availWidth = aTableSpecifiedWidth - aComputedTableWidth;
  mTableFrame->GetColumnsByType(eStyleUnit_Auto, numAutoColumns, autoColumns);
  if (0!=numAutoColumns)
  {
    // there's at least one auto-width column, so give it (them) the extra space
    // proportionately distributed extra space, based on the column's desired size
    nscoord totalEffectiveWidthOfAutoColumns = 0;
    // 1. first, get the total width of the auto columns
    PRInt32 i;
    for (i = 0; i<numAutoColumns; i++)
    {
      nsTableColFrame *colFrame = mTableFrame->GetColFrame(autoColumns[i]);
      nscoord maxEffectiveColWidth = colFrame->GetEffectiveMaxColWidth();
      if (0!=maxEffectiveColWidth)
        totalEffectiveWidthOfAutoColumns += maxEffectiveColWidth;
      else
        totalEffectiveWidthOfAutoColumns += mTableFrame->GetColumnWidth(autoColumns[i]);
    }
    if (gsDebug==PR_TRUE) 
      printf("  aTableSpecifiedWidth specified as %d, availWidth is = %d\n", 
             aTableSpecifiedWidth, availWidth);
    
    // 2. next, compute the proportion to be added to each column, and add it
    for (i = 0; i<numAutoColumns; i++)
    {
      PRInt32 colIndex = autoColumns[i];
      nsTableColFrame *colFrame = mTableFrame->GetColFrame(colIndex);
      // if we actually have room to distribute, do it here
      // otherwise, the auto columns will remain 0-width until expanded to their minimum in EnsureCellMinWidths
      if (0<availWidth)
      {
        float percent;
        if (0!=aTableSpecifiedWidth)
          percent = ((float)(colFrame->GetEffectiveMaxColWidth()))/((float)totalEffectiveWidthOfAutoColumns);
        else
          percent = ((float)1)/((float)numAutoColumns);
        nscoord colWidth = (nscoord)(availWidth*percent);
        if (gsDebug==PR_TRUE) 
          printf("  distribute width to auto columns:  column %d was %d, now set to %d\n", 
                 colIndex, colFrame->GetEffectiveMaxColWidth(), colWidth);
        mTableFrame->SetColumnWidth(colIndex, colWidth);
      }
    }

    if (PR_TRUE==gsDebug)
    {
      nscoord tableWidth=0;
      for (PRInt32 i=0; i<mNumCols; i++)
        tableWidth += mTableFrame->GetColumnWidth(i);
      printf ("before EnsureCellMinWidth, computed table width is %d\n",tableWidth);
    }

    EnsureCellMinWidths();
  }
  if (PR_TRUE==gsDebug)
  {
    nscoord tableWidth=0;
    for (PRInt32 i=0; i<mNumCols; i++)
      tableWidth += mTableFrame->GetColumnWidth(i);
    printf ("after EnsureCellMinWidth, computed table width is %d\n",tableWidth);
  }
}

void BasicTableLayoutStrategy::EnsureCellMinWidths()
{
  PRBool atLeastOne = PR_TRUE;
  /* XXX  we'll use this if it turns out that fixed-width cells
     XXX  should NOT be shrunk to accomodate table width
  PRInt32 numFixedColumns;
  PRInt32 *fixedColumns=nsnull;
  mTableFrame->GetColumnsByType(eStyleUnit_Coord, numFixedColumns, fixedColumns);
  */
  while (PR_TRUE==atLeastOne)
  {
    atLeastOne=PR_FALSE;
    PRInt32 colIndex;
    nscoord addedWidth = 0;
    // first, bump up cells that are below their min to their min width
    for (colIndex=0; colIndex<mNumCols; colIndex++)
    {
      nsTableColFrame *colFrame = mTableFrame->GetColFrame(colIndex);
      nscoord minEffectiveColWidth = colFrame->GetEffectiveMinColWidth();
      nscoord colWidth = mTableFrame->GetColumnWidth(colIndex);
      if (colWidth<minEffectiveColWidth)
      {
        addedWidth += (minEffectiveColWidth-colWidth);
        mTableFrame->SetColumnWidth(colIndex, minEffectiveColWidth);
        atLeastOne=PR_TRUE;
      }
    }
    // second, remove the added space from cells that can afford to slim down
    //QQQ boy is this slow!  there has to be a faster way that is still guaranteed to be accurate...
    while ((addedWidth>0))
    { // while we still have some extra space, and last time we were able to take some off...
      PRInt32 startingAddedWidth = addedWidth;
      for (colIndex=0; colIndex<mNumCols; colIndex++)
      {
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(colIndex);
        nscoord minEffectiveColWidth = colFrame->GetEffectiveMinColWidth();
        nscoord colWidth = mTableFrame->GetColumnWidth(colIndex);
        if (colWidth>minEffectiveColWidth)
        {
          mTableFrame->SetColumnWidth(colIndex, colWidth-1);
          addedWidth--;
          if (0==addedWidth)
          { // we're run out of extra space
            break;  // for (colIndex=0; colIndex<mNumCols; colIndex++)
          }
        }
      }
      if (startingAddedWidth==addedWidth)
      { // we we unable to find any columns to remove space from
        break; // while ((addedWidth>0))
      }
    }
    if (0<addedWidth)
    { // all our cells are at their min width, so no use doing anything else
      break; // while (PR_TRUE==atLeastOne)
    }
  }
}

// for now, extra space just falls on the floor (that is, it stays in the table)
// this is in the case of having to set a col to its min width instead of just
// subtracting out excessPerColumn
void BasicTableLayoutStrategy::AdjustTableThatIsTooWide(nscoord aComputedWidth, 
                                                        nscoord aTableWidth)
{
  PRInt32 numFixedColumns=0;
  PRInt32 *fixedColumns=nsnull;
  //XXX todo: exclude fixed width columns
  //mTableFrame->GetColumnsByType(eStyleUnit_Coord, numFixedColumns, fixedColumns);
  nscoord excess = aComputedWidth - aTableWidth;
  if (0<excess && mNumCols!=numFixedColumns)
  {
    nscoord excessPerColumn = excess/(mNumCols-numFixedColumns);
    for (PRInt32 colIndex = 0; colIndex<mNumCols; colIndex++)
    {
      //XXX todo: exclude fixed width columns
      //if (PR_FALSE==IsFixedWidthColumn(colIndex, fixedColumns)
      {
        nscoord colWidth = mTableFrame->GetColumnWidth(colIndex);
        colWidth -= excessPerColumn;
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(colIndex);
        if (colWidth > colFrame->GetEffectiveMinColWidth())
          mTableFrame->SetColumnWidth(colIndex, colWidth);
        else
          mTableFrame->SetColumnWidth(colIndex, colFrame->GetEffectiveMinColWidth());
      }
    }
  }
}

void BasicTableLayoutStrategy::AdjustTableThatIsTooNarrow(nscoord aComputedWidth, 
                                                          nscoord aTableWidth)
{
  PRInt32 numFixedColumns=0;
  PRInt32 *fixedColumns=nsnull;
  //XXX todo: exclude fixed width columns
  //mTableFrame->GetColumnsByType(eStyleUnit_Coord, numFixedColumns, fixedColumns);
  nscoord excess = aTableWidth - aComputedWidth;
  if (0<excess && mNumCols!=numFixedColumns)
  {
    nscoord excessPerColumn = excess/(mNumCols-numFixedColumns);
    for (PRInt32 colIndex = 0; colIndex<mNumCols; colIndex++)
    {
      //XXX todo: exclude fixed width columns
      //if (PR_FALSE==IsFixedWidthColumn(colIndex, fixedColumns)
      {
        nscoord colWidth = mTableFrame->GetColumnWidth(colIndex);
        colWidth += excessPerColumn;
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(colIndex);
        if (colWidth >colFrame->GetEffectiveMinColWidth())
          mTableFrame->SetColumnWidth(colIndex, colWidth);
        else
          mTableFrame->SetColumnWidth(colIndex, colFrame->GetEffectiveMinColWidth());
      }
    }
  }
}

//PRBool BasicTableLayoutStrategy::IsFixedWidthColumn(PRInt32 colIndex, PRInt32 *colIndexes)


