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

#include <stdio.h>
#include "BasicTableLayoutStrategy.h"
#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsColLayoutData.h"
#include "nsCellLayoutData.h"
#include "nsTableCol.h"
#include "nsTableCellFrame.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPtr.h"
#include "nsHTMLIIDs.h"

NS_DEF_PTR(nsTableCol);
NS_DEF_PTR(nsTableCell);
NS_DEF_PTR(nsIStyleContext);


#ifdef NS_DEBUG
static PRBool gsDebug = PR_TRUE;
static PRBool gsDebugCLD = PR_FALSE;
static PRBool gsTiming = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsDebugCLD = PR_FALSE;
static const PRBool gsTiming = PR_FALSE;
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



/* ---------- BasicTableLayoutStrategy ---------- */

/* return true if the style indicates that the width is proportional 
 * for the purposes of column width determination
 */
PRBool BasicTableLayoutStrategy::IsFixedWidth(const nsStylePosition* aStylePosition)
{
  PRBool result = PR_FALSE; // assume that it is not fixed width
  PRInt32 unitType;
  if (nsnull == aStylePosition) {
    unitType = eStyleUnit_Auto;
  }
  else {
    unitType = aStylePosition->mWidth.GetUnit();
  }

  switch (unitType) {
    case eStyleUnit_Coord:
      result = PR_TRUE;

    case eStyleUnit_Auto:
    case eStyleUnit_Proportional:
    case eStyleUnit_Percent:
      break;

    // TODO
    case eStyleUnit_Inherit:
      break;

    default:
      NS_ASSERTION(PR_FALSE, "illegal style type in IsFixedWidth");
      break;
  }
  return result;
}

PRBool BasicTableLayoutStrategy::IsAutoWidth(const nsStylePosition* aStylePosition)
{
  PRBool result = PR_TRUE; // assume that it is
  if (nsnull!=aStylePosition)
  {
    result = (PRBool)(eStyleUnit_Auto==aStylePosition->mWidth.GetUnit());
  }
  return result;
}


BasicTableLayoutStrategy::BasicTableLayoutStrategy(nsTableFrame *aFrame)
{
  mTableFrame = aFrame;
}

BasicTableLayoutStrategy::~BasicTableLayoutStrategy()
{
}

PRBool BasicTableLayoutStrategy::BalanceColumnWidths(nsIPresContext* aPresContext,
                                                     nsIStyleContext *aTableStyle,
                                                     const nsReflowState& aReflowState,
                                                     PRInt32 aMaxWidth, 
                                                     PRInt32 aNumCols,
                                                     PRInt32 &aTotalFixedWidth,
                                                     PRInt32 &aMinTableWidth,
                                                     PRInt32 &aMaxTableWidth,
                                                     nsSize* aMaxElementSize)
{
  PRBool result = PR_TRUE;
  // initialize out parameters
  aTotalFixedWidth=aMinTableWidth=aMaxTableWidth=0;

  // Step 1 - assign the width of all fixed-width columns
  AssignFixedColumnWidths(aPresContext, aMaxWidth, aNumCols,
                          aTotalFixedWidth, aMinTableWidth, aMaxTableWidth);

  if (nsnull!=aMaxElementSize)
  { // this is where we initialize maxElementSize if it is non-null
    aMaxElementSize->height = 0;
    aMaxElementSize->width = aMinTableWidth;
    if (gsDebug) printf("  setting aMaxElementSize->width = %d\n", aMaxElementSize->width);
  }
  else
  {
    if (gsDebug) printf("  nsnull aMaxElementSize\n");
  }

  // Step 2 - determine how much space is really available
  PRInt32 availWidth = aMaxWidth - aTotalFixedWidth;
  nscoord tableWidth = 0;
  if (PR_FALSE==nsTableFrame::TableIsAutoWidth(mTableFrame, aTableStyle, aReflowState, tableWidth))
    availWidth = tableWidth - aTotalFixedWidth;
  
  // Step 3 - assign the width of all proportional-width columns in the remaining space
  if (gsDebug==PR_TRUE) printf ("Step 2...\n  availWidth = %d\n", availWidth);
  result = BalanceProportionalColumns(aPresContext, aReflowState,
                                      availWidth, aMaxWidth,
                                      aMinTableWidth, aMaxTableWidth,
                                      tableWidth);
  return result;
}

/*
STEP 1
for every col
  if the col has a fixed width
    set the width to max(fixed width, maxElementSize)
    if the cell spans columns, divide the cell width between the columns
  else skip it for now

take borders and padding into account

STEP 2
determine the min and max size for the table width
if col is proportionately specified
  if (col width specified to 0)
    col width = minColWidth
  else if (minTableWidth >= aMaxSize.width)
    set col widths to min, install a hor. scroll bar
  else if (maxTableWidth <= aMaxSize.width)
    set each col to its max size
  else
    W = aMaxSize.width -  minTableWidth
    D = maxTableWidth - minTableWidth
    for each col
      d = maxColWidth - minColWidth
      col width = minColWidth + ((d*W)/D)

STEP 3
if there is space left over
  for every col
    if col is proportionately specified
      add space to col width until it is that proportion of the table width
      do this non-destructively in case there isn't enough space
  if there isn't enough space as determined in the prior step,
    add space in proportion to the proportionate width attribute
  */


// Step 1 - assign the width of all fixed-width columns, 
//          and calculate min/max table width
PRBool BasicTableLayoutStrategy::AssignFixedColumnWidths(nsIPresContext* aPresContext,
                                                         PRInt32 maxWidth, 
                                                         PRInt32 aNumCols,
                                                         PRInt32 &aTotalFixedWidth,
                                                         PRInt32 &aMinTableWidth,
                                                         PRInt32 &aMaxTableWidth)
{
#ifdef DEBUG
  nsIFrame *tablePIF=nsnull;
  mTableFrame->GetPrevInFlow(tablePIF);
  NS_ASSERTION(nsnull==tablePIF, "never ever call me on a continuing frame!");
#endif

  if (gsDebug==PR_TRUE) printf ("  AssignFixedColumnWidths\n");
  nsVoidArray *spanList=nsnull;
  for (PRInt32 colIndex = 0; colIndex<aNumCols; colIndex++)
  { 
    nsVoidArray *columnLayoutData = mTableFrame->GetColumnLayoutData();
    nsColLayoutData * colData = (nsColLayoutData *)(columnLayoutData->ElementAt(colIndex));
    NS_ASSERTION(nsnull != colData, "bad column data");
    nsTableColFrame *colFrame = colData->GetColFrame();
    NS_ASSERTION(nsnull!=colFrame, "bad col frame");
    nsTableColPtr col = colData->GetCol();    // col: ADDREF++
    NS_ASSERTION(col.IsNotNull(), "bad col");

    // need to track min/max column width for setting min/max table widths
    PRInt32 minColWidth = 0;
    PRInt32 maxColWidth = 0;
    nsVoidArray *cells = colData->GetCells();
    PRInt32 numCells = cells->Count();
    if (gsDebug==PR_TRUE) printf ("  for column %d  numCells = %d\n", colIndex, numCells);

    // Get the columns's style
    nsIStyleContextPtr colSC;
    colFrame->GetStyleContext(aPresContext, colSC.AssignRef());
    const nsStylePosition* colPosition = (const nsStylePosition*) colSC->GetStyleData(eStyleStruct_Position);

    // Get column width if it has one
    PRBool haveColWidth = PR_FALSE;
    nscoord specifiedFixedColWidth=0;
    switch (colPosition->mWidth.GetUnit()) {
      case eStyleUnit_Coord:
        haveColWidth = PR_TRUE;
        specifiedFixedColWidth = colPosition->mWidth.GetCoordValue();
        mTableFrame->SetColumnWidth(colIndex, specifiedFixedColWidth);
        break;

      default:
        break;
    }

    // TODO - use specifiedFixedColWidth to influence the width when both col span and fixed col width are given
    /* Scan the column, simulatneously assigning fixed column widths
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

    for (PRInt32 cellIndex = 0; cellIndex<numCells; cellIndex++)
    {
      nsCellLayoutData * data = (nsCellLayoutData *)(cells->ElementAt(cellIndex));
      NS_ASSERTION(nsnull != data, "bad data");
      
      nsMargin  margin;
      nsresult  result = data->GetMargin(margin);

      nsSize * cellMinSize = data->GetMaxElementSize();
      nsReflowMetrics * cellDesiredSize = data->GetDesiredSize();
      NS_ASSERTION(nsnull != cellDesiredSize, "bad cellDesiredSize");
      PRInt32 colSpan = data->GetCellFrame()->GetColSpan();
      if (gsDebug==PR_TRUE) 
        printf ("    for cell %d with colspan=%d, min = %d,%d  and  des = %d,%d\n", 
                cellIndex, colSpan, cellMinSize->width, cellMinSize->height,
                cellDesiredSize->width, cellDesiredSize->height);

      switch (colPosition->mWidth.GetUnit()) {
        case eStyleUnit_Coord:
          {
            // This col has a fixed width, so set the cell's width to the
            // larger of (specified width, largest max_element_size of the
            // cells in the column)
            PRInt32 widthForThisCell = PR_MAX(cellMinSize->width, colPosition->mWidth.GetCoordValue());
            if (mTableFrame->GetColumnWidth(colIndex) < widthForThisCell)
            {
              if (gsDebug) printf ("    setting fixed width to %d\n",widthForThisCell);
              mTableFrame->SetColumnWidth(colIndex, widthForThisCell);
              maxColWidth = widthForThisCell;
            }
          }
          break;

        default:
          break;

      }

      // regardless of the width specification, keep track of the
      // min/max column widths
      nscoord cellMinWidth = cellMinSize->width/colSpan;
      nscoord cellDesiredWidth = cellDesiredSize->width/colSpan;

      cellMinWidth += margin.left + margin.right;
      cellDesiredWidth += margin.left + margin.right;

      if (minColWidth < cellMinWidth)
        minColWidth = cellMinWidth;
      if (maxColWidth < cellDesiredWidth)
        maxColWidth = cellDesiredWidth;
      if (1<colSpan)
      {
        // add the cell to our list of spanners
        SpanInfo *spanInfo = new SpanInfo(colSpan-1, cellMinWidth, cellDesiredWidth);
        if (nsnull==spanList)
          spanList = new nsVoidArray();
        spanList->AppendElement(spanInfo);
      }
      if (gsDebug) {
        printf ("    after cell %d, minColWidth = %d and maxColWidth = %d\n",
                cellIndex, minColWidth, maxColWidth);
      }
    }

    // keep a running total of the amount of space taken up by all fixed-width columns
    if (PR_TRUE==haveColWidth)
      aTotalFixedWidth += mTableFrame->GetColumnWidth(colIndex);

    if (gsDebug) {
      printf ("    after col %d, aTotalFixedWidth = %d\n",
              colIndex, aTotalFixedWidth);
    }

    // add col[i] metrics to the running totals for the table min/max width
    if (NS_UNCONSTRAINEDSIZE!=aMinTableWidth)
      aMinTableWidth += minColWidth;  // SEC: insets!
    if (aMinTableWidth<=0) 
      aMinTableWidth = NS_UNCONSTRAINEDSIZE; // handle overflow
    if (NS_UNCONSTRAINEDSIZE!=aMaxTableWidth)
      aMaxTableWidth += maxColWidth;  // SEC: insets!
    if (aMaxTableWidth<=0) 
      aMaxTableWidth = NS_UNCONSTRAINEDSIZE; // handle overflow
    if (gsDebug==PR_TRUE) 
      printf ("  after this col, minTableWidth = %d and maxTableWidth = %d\n", aMinTableWidth, aMaxTableWidth);
  
  } // end Step 1 for fixed-width columns
  if (nsnull!=spanList)
    delete spanList;
  return PR_TRUE;
}


PRBool BasicTableLayoutStrategy::BalanceProportionalColumns(nsIPresContext* aPresContext,
                                                            const nsReflowState& aReflowState,
                                                            nscoord aAvailWidth,
                                                            nscoord aMaxWidth,
                                                            nscoord aMinTableWidth, 
                                                            nscoord aMaxTableWidth,
                                                            nscoord aTableFixedWidth)
{
  PRBool result = PR_TRUE;

  if (0==aTableFixedWidth)
  {
    if (NS_UNCONSTRAINEDSIZE==aMaxWidth  ||  NS_UNCONSTRAINEDSIZE==aMinTableWidth)
    { // the max width of the table fits comfortably in the available space
      if (gsDebug) printf ("  * table laying out in NS_UNCONSTRAINEDSIZE, calling BalanceColumnsTableFits\n");
      result = BalanceColumnsTableFits(aPresContext, aReflowState, aAvailWidth, aMaxWidth, aTableFixedWidth);
    }
    else if (aMinTableWidth > aMaxWidth)
    { // the table doesn't fit in the available space
      if (gsDebug) printf ("  * min table does not fit, calling SetColumnsToMinWidth\n");
      result = SetColumnsToMinWidth(aPresContext);
    }
    else if (aMaxTableWidth <= aMaxWidth)
    { // the max width of the table fits comfortably in the available space
      if (gsDebug) printf ("  * table desired size fits, calling BalanceColumnsTableFits\n");
      result = BalanceColumnsTableFits(aPresContext, aReflowState, aAvailWidth, aMaxWidth, aTableFixedWidth);
    }
    else
    { // the table fits somewhere between its min and desired size
      if (gsDebug) printf ("  * table desired size does not fit, calling BalanceColumnsConstrained\n");
      result = BalanceColumnsConstrained(aPresContext, aAvailWidth,
                                         aMaxWidth, aMinTableWidth, aMaxTableWidth);
    }
  }
  else
  { // tables with fixed-width have their own rules
    if (NS_UNCONSTRAINEDSIZE==aMinTableWidth)
    { // the table has empty content, and needs to be streched to the specified width
      if (gsDebug) printf ("  * specified width table > maxTableWidth, calling BalanceColumnsTableFits\n");
      result = BalanceColumnsTableFits(aPresContext, aReflowState, aAvailWidth, aMaxWidth, aTableFixedWidth);
    }
    else if (aTableFixedWidth<aMinTableWidth)
    { // the table's specified width doesn't fit in the available space
      if (gsDebug) printf ("  * specified width table with width<minTableWidth, calling SetColumnsToMinWidth\n");
      result = SetColumnsToMinWidth(aPresContext);
    }
    else if (aTableFixedWidth<aMaxTableWidth)
    { // the table's specified width is between it's min and max, so fit the columns in proportionately
      if (gsDebug) printf ("  * specified width table < maxTableWidth, calling BalanceColumnsConstrained\n");
      result = BalanceColumnsConstrained(aPresContext, aAvailWidth,
                                         aAvailWidth, aMinTableWidth, aMaxTableWidth);
      // should 3rd param above be aAvailWidth?
    }
    else
    { // the table's specified width is >= its max width, so give each column its max requested size
      if (gsDebug) printf ("  * specified width table > maxTableWidth, calling BalanceColumnsTableFits\n");
      result = BalanceColumnsTableFits(aPresContext, aReflowState, aAvailWidth, aMaxWidth, aTableFixedWidth);
    }
  }
  return result;
}

PRBool BasicTableLayoutStrategy::SetColumnsToMinWidth(nsIPresContext* aPresContext)
{
  PRBool result = PR_TRUE;
  nsVoidArray *columnLayoutData = mTableFrame->GetColumnLayoutData();
  PRInt32 numCols = columnLayoutData->Count();
  for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
  { 
    nsColLayoutData * colData = (nsColLayoutData *)(columnLayoutData->ElementAt(colIndex));
    nsTableColPtr col = colData->GetCol();  // col: ADDREF++
    nsVoidArray *cells = colData->GetCells();
    PRInt32 minColWidth = 0;
    PRInt32 maxColWidth = 0;
    PRInt32 numCells = cells->Count();
    if (gsDebug==PR_TRUE) printf ("  for col %d\n", colIndex);

    // XXX need column frame to ask this question
    const nsStylePosition* colPosition = nsnull;

    if (PR_FALSE==IsFixedWidth(colPosition))
    {
      for (PRInt32 cellIndex = 0; cellIndex<numCells; cellIndex++)
      { // this col has proportional width, so determine its width requirements
        nsCellLayoutData * data = (nsCellLayoutData *)(cells->ElementAt(cellIndex));
        NS_ASSERTION(nsnull != data, "bad data");
        nsSize * cellMinSize = data->GetMaxElementSize();
        NS_ASSERTION(nsnull != cellMinSize, "bad cellMinSize");
        nsReflowMetrics * cellDesiredSize = data->GetDesiredSize();
        NS_ASSERTION(nsnull != cellDesiredSize, "bad cellDesiredSize");
        if (minColWidth < cellMinSize->width)
          minColWidth = cellMinSize->width;
        if (maxColWidth < cellDesiredSize->width)
          maxColWidth = cellDesiredSize->width;
        /*
        if (gsDebug==PR_TRUE) 
          printf ("    after cell %d, minColWidth = %d and maxColWidth = %d\n", 
                  cellIndex, minColWidth, maxColWidth);
        */
      }

      if (gsDebug==PR_TRUE) 
      {
        printf ("  for determining width of col %d %s:\n",
                 colIndex, !IsFixedWidth(colPosition)? "(P)":"(A)");
        printf ("    minColWidth = %d and maxColWidth = %d\n", minColWidth, maxColWidth);
      }

      mTableFrame->SetColumnWidth(colIndex, minColWidth);
      if (gsDebug==PR_TRUE) 
        printf ("  2: col %d, set to width = %d\n", colIndex, mTableFrame->GetColumnWidth(colIndex));
    }
  }
  return result;
}

PRBool BasicTableLayoutStrategy::BalanceColumnsTableFits(nsIPresContext* aPresContext,
                                                         const nsReflowState& aReflowState,
                                                         nscoord aAvailWidth,
                                                         nscoord aMaxWidth,
                                                         nscoord aTableFixedWidth)
{
#ifdef DEBUG
  nsIFrame *tablePIF=nsnull;
  mTableFrame->GetPrevInFlow(tablePIF);
  NS_ASSERTION(nsnull==tablePIF, "never ever call me on a continuing frame!");
#endif

  PRBool result = PR_TRUE;
  nscoord tableWidth=0;               // the width of the table as a result of setting column widths
  PRInt32 totalSlices=0;              // the total number of slices the proportional-width columns request
  nsVoidArray *proportionalColumnsList=nsnull; // a list of the columns that are proportional-width
  nsVoidArray *spanList=nsnull;       // a list of the cells that span columns
  nsVoidArray *columnLayoutData = mTableFrame->GetColumnLayoutData();
  PRInt32 numCols = columnLayoutData->Count();
  for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
  { 
    if (gsDebug==PR_TRUE) printf ("  for col %d\n", colIndex);
    PRInt32 minColWidth = 0;
    PRInt32 maxColWidth = 0;
    // Get column information
    nsColLayoutData * colData = (nsColLayoutData *)(columnLayoutData->ElementAt(colIndex));
    nsTableColPtr col = colData->GetCol();  // col: ADDREF++
    nsVoidArray *cells = colData->GetCells();
    PRInt32 numCells = cells->Count();

    // Get the columns's style
    nsTableColFrame *colFrame = colData->GetColFrame();
    NS_ASSERTION(nsnull!=colFrame, "bad col frame");
    nsIStyleContextPtr colSC;
    colFrame->GetStyleContext(aPresContext, colSC.AssignRef());
    const nsStylePosition* colPosition = (const nsStylePosition*) colSC->GetStyleData(eStyleStruct_Position);
    if (gsDebug) printf("col %d has frame %p with style %p and pos %p\n", 
                         colIndex, colFrame, (nsIStyleContext *)colSC, colPosition);

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

      for (PRInt32 cellIndex = 0; cellIndex<numCells; cellIndex++)
      { // this col has proportional width, so determine its width requirements
        nsCellLayoutData * data = (nsCellLayoutData *)(cells->ElementAt(cellIndex));
        NS_ASSERTION(nsnull != data, "bad data");

 
        PRInt32 colSpan = data->GetCellFrame()->GetColSpan();
        // distribute a portion of the spanning cell's min and max width to this column
        nsSize * cellMinSize = data->GetMaxElementSize();
        NS_ASSERTION(nsnull != cellMinSize, "bad cellMinSize");
        nsReflowMetrics * cellDesiredSize = data->GetDesiredSize();
        NS_ASSERTION(nsnull != cellDesiredSize, "bad cellDesiredSize");
        
        nsMargin  margin;
        nsresult  result = data->GetMargin(margin);        
        nscoord marginWidth = margin.left + margin.right;
        PRInt32 cellMinWidth = cellMinSize->width/colSpan + marginWidth;
        // first get the desired size info from reflow pass 1
        PRInt32 cellDesiredWidth = cellDesiredSize->width/colSpan + marginWidth;  
        // then get the desired size info factoring in the cell style attributes
        nscoord specifiedCellWidth=-1;
        nsIStyleContextPtr cellSC;
        data->GetCellFrame()->GetStyleContext(aPresContext, cellSC.AssignRef());
        const nsStylePosition* cellPosition = (const nsStylePosition*)
          cellSC->GetStyleData(eStyleStruct_Position);
        switch (cellPosition->mWidth.GetUnit()) {
          case eStyleUnit_Coord:
            specifiedCellWidth = cellPosition->mWidth.GetCoordValue();
            if (gsDebug) printf("cell has specified coord width = %d\n", specifiedCellWidth);
            break;

          case eStyleUnit_Percent: 
          {
            nscoord tableWidth=0;
            nsIStyleContextPtr tableSC;
            mTableFrame->GetStyleContext(aPresContext, tableSC.AssignRef());
            PRBool tableIsAutoWidth = nsTableFrame::TableIsAutoWidth(mTableFrame, tableSC, aReflowState, tableWidth);
            float percent = cellPosition->mWidth.GetPercentValue();
            specifiedCellWidth = (PRInt32)(tableWidth*percent);
            if (gsDebug) printf("specified percent width %f of %d = %d\n", 
                                percent, tableWidth, specifiedCellWidth);
            break;
          }

          case eStyleUnit_Inherit:
            // XXX for now, do nothing
          default:
          case eStyleUnit_Auto:
            break;
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
                 cellIndex, colSpan, cellMinWidth, cellDesiredWidth);
        if (minColWidth < cellMinWidth)
          minColWidth = cellMinWidth;
        if (maxColWidth < cellDesiredWidth)
          maxColWidth = cellDesiredWidth;
        if (gsDebug==PR_TRUE) 
          printf ("    after cell %d, minColWidth = %d and maxColWidth = %d\n", 
                  cellIndex, minColWidth, maxColWidth);
        if (1<colSpan)
        { // add the cell to our list of spanners
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
        printf ("    minColWidth = %d and maxColWidth = %d\n", minColWidth, maxColWidth);
        printf ("    aAvailWidth = %d\n", aAvailWidth);
      }

      // Get column width if it has one
      nscoord specifiedProportionColumnWidth = -1;
      float specifiedPercentageColWidth = -1.0f;
      nscoord specifiedFixedColumnWidth = -1;
      PRBool isAutoWidth = PR_FALSE;
      switch (colPosition->mWidth.GetUnit()) {
        case eStyleUnit_Coord:
          specifiedFixedColumnWidth = colPosition->mWidth.GetCoordValue();
          if (gsDebug) printf("column %d has specified coord width = %d\n", colIndex, specifiedFixedColumnWidth);
          break;
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
      else if (-1!=specifiedFixedColumnWidth)
      {
        // if the column was computed to be too small, enlarge the column
        nscoord resolvedWidth = specifiedFixedColumnWidth;
        if (specifiedFixedColumnWidth <= minColWidth)
          resolvedWidth = minColWidth;
        mTableFrame->SetColumnWidth(colIndex, resolvedWidth);
        if (gsDebug==PR_TRUE) 
          printf ("  3 fixed: col %d given %d\n", colIndex, resolvedWidth);
      }
      else
      {  // give each remaining column a percentage of the remaining space
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
            mTableFrame->SetColumnWidth(colIndex, (percentage*aTableFixedWidth)/100);
            if (gsDebug==PR_TRUE) 
              printf ("  3 percent specified: col %d given %d percent of aTableFixedWidth %d, set to width = %d\n", 
                      colIndex, percentage, aTableFixedWidth, mTableFrame->GetColumnWidth(colIndex));
          }
          if (-1==percentage)
          {
            percentage = 100/numCols;
            // base the % on the remaining available width 
            mTableFrame->SetColumnWidth(colIndex, (percentage*aAvailWidth)/100);
          }
          // if the column was computed to be too small, enlarge the column
          if (mTableFrame->GetColumnWidth(colIndex) <= minColWidth)
            mTableFrame->SetColumnWidth(colIndex, minColWidth);
        }
      }
    }
    tableWidth += mTableFrame->GetColumnWidth(colIndex);
  }

  /* --- post-process if necessary --- */
  // first, assign a width to proportional-width columns
  if (nsnull!=proportionalColumnsList)
  {
    // first, figure out the amount of space per slice
    nscoord maxWidthForTable = (0!=aTableFixedWidth) ? aTableFixedWidth : aMaxWidth;
    nscoord widthRemaining = maxWidthForTable - tableWidth;
    nscoord widthPerSlice = widthRemaining/totalSlices;
    PRInt32 numSpecifiedProportionalColumns = proportionalColumnsList->Count();
    for (PRInt32 i=0; i<numSpecifiedProportionalColumns; i++)
    {
      ProportionalColumnLayoutStruct * info = 
        (ProportionalColumnLayoutStruct *)(proportionalColumnsList->ElementAt(i));
      nscoord computedColWidth = info->mProportion*widthPerSlice;
      mTableFrame->SetColumnWidth(info->mColIndex, computedColWidth);
      if (gsDebug==PR_TRUE) 
        printf ("  3 proportional step 2: col %d given %d proportion of remaining space %d, set to width = %d\n", 
                info->mColIndex, info->mProportion, widthRemaining, computedColWidth);
      tableWidth += computedColWidth;
      delete info;
    }
    delete proportionalColumnsList;
  }

  // next, if the specified width of the table is greater than the table's computed width, expand the
  // table's computed width to match the specified width, giving the extra space to proportionately-sized
  // columns if possible.
  if (aTableFixedWidth > tableWidth)
  {
    nscoord excess = aTableFixedWidth - tableWidth;
    if (gsDebug) printf ("fixed width %d > computed table width %d, excess = %d\n",
                          aTableFixedWidth, tableWidth, excess);
    // if there are auto-sized columns, give them the extra space
    PRInt32 numAutoColumns;
    PRInt32 *autoColumns;
    mTableFrame->GetColumnsByType(eStyleUnit_Auto, numAutoColumns, autoColumns);
    if (0!=numAutoColumns)
    {
      // TODO - should extra space be proportionately distributed?
      nscoord excessPerColumn = excess/numAutoColumns;
      if (gsDebug==PR_TRUE) 
        printf("  aTableFixedWidth specified as %d, expanding columns by excess = %d\n", aTableFixedWidth, excess);
      for (PRInt32 i = 0; i<numAutoColumns; i++)
      {
        PRInt32 colIndex = autoColumns[i];
        nscoord colWidth = excessPerColumn+mTableFrame->GetColumnWidth(colIndex);
        if (gsDebug==PR_TRUE) 
          printf("  column %d was %d, now set to %d\n", colIndex, mTableFrame->GetColumnWidth(colIndex), colWidth);
        mTableFrame->SetColumnWidth(colIndex, colWidth);
      }
    }
    // otherwise, distribute the space evenly between all the columns (they must be all fixed and percentage-width columns)
    // TODO - should extra space be proportionately distributed?
    else
    {
      nscoord excessPerColumn = excess/numCols;
      if (gsDebug==PR_TRUE) 
        printf("  aTableFixedWidth specified as %d, expanding columns by excess = %d\n", aTableFixedWidth, excess);
      for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
      {
        nscoord colWidth = excessPerColumn+mTableFrame->GetColumnWidth(colIndex);
        mTableFrame->SetColumnWidth(colIndex, colWidth);
      }
    }
  }

  if (nsnull!=spanList)
    delete spanList;
  return result;
}

PRBool BasicTableLayoutStrategy::BalanceColumnsConstrained( nsIPresContext* aPresContext,
                                                            PRInt32 aAvailWidth,
                                                            PRInt32 aMaxWidth,
                                                            PRInt32 aMinTableWidth, 
                                                            PRInt32 aMaxTableWidth)
{
#ifdef DEBUG
  nsIFrame *tablePIF=nsnull;
  mTableFrame->GetPrevInFlow(tablePIF);
  NS_ASSERTION(nsnull==tablePIF, "never ever call me on a continuing frame!");
#endif

  PRBool result = PR_TRUE;
  PRInt32 maxOfAllMinColWidths = 0;   // for the case where we have equal column widths, this is the smallest a column can be
  nscoord tableWidth=0;               // the width of the table as a result of setting column widths
  PRInt32 totalSlices=0;              // the total number of slices the proportional-width columns request
  nsVoidArray *proportionalColumnsList=nsnull; // a list of the columns that are proportional-width
  PRBool equalWidthColumns = PR_TRUE; // remember if we're in the special case where all
                                      // proportional-width columns are equal (if any are anything other than 1)
  nsVoidArray *spanList=nsnull;       // a list of the cells that span columns
  nsVoidArray *columnLayoutData = mTableFrame->GetColumnLayoutData();
  PRInt32 numCols = columnLayoutData->Count();
  for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
  { 
    if (gsDebug==PR_TRUE) printf ("  for col %d\n", colIndex);
    PRInt32 minColWidth = 0;
    PRInt32 maxColWidth = 0;
    // Get column information
    nsColLayoutData * colData = (nsColLayoutData *)(columnLayoutData->ElementAt(colIndex));
    nsTableColPtr col = colData->GetCol();  // col: ADDREF++
    nsVoidArray *cells = colData->GetCells();
    PRInt32 numCells = cells->Count();

    // Get the columns's style
    nsTableColFrame *colFrame = colData->GetColFrame();
    NS_ASSERTION(nsnull!=colFrame, "bad col frame");
    nsIStyleContextPtr colSC;
    colFrame->GetStyleContext(aPresContext, colSC.AssignRef());
    const nsStylePosition* colPosition = (const nsStylePosition*) colSC->GetStyleData(eStyleStruct_Position);

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
      for (PRInt32 cellIndex = 0; cellIndex<numCells; cellIndex++)
      { // this col has proportional width, so determine its width requirements
        nsCellLayoutData * data = (nsCellLayoutData *)(cells->ElementAt(cellIndex));
        NS_ASSERTION(nsnull != data, "bad data");
        PRInt32 colSpan = data->GetCellFrame()->GetColSpan();
        nsSize * cellMinSize = data->GetMaxElementSize();
        NS_ASSERTION(nsnull != cellMinSize, "bad cellMinSize");
        nsReflowMetrics * cellDesiredSize = data->GetDesiredSize();
        NS_ASSERTION(nsnull != cellDesiredSize, "bad cellDesiredSize");

        nsMargin  margin;
        nsresult  result = data->GetMargin(margin);        
        nscoord marginWidth = margin.left + margin.right;

        PRInt32 cellMinWidth = cellMinSize->width/colSpan + marginWidth;
        PRInt32 cellDesiredWidth = cellDesiredSize->width/colSpan + marginWidth;
        if (minColWidth < cellMinWidth)
          minColWidth = cellMinWidth;
        if (maxColWidth < cellDesiredWidth)
          maxColWidth = cellDesiredWidth;
        if (1<colSpan)
        {
          // add the cell to our list of spanners
          SpanInfo *spanInfo = new SpanInfo(colSpan-1, cellMinWidth, cellDesiredWidth);
          if (nsnull==spanList)
            spanList = new nsVoidArray();
          spanList->AppendElement(spanInfo);
        }
        if (gsDebug==PR_TRUE) 
          printf ("    after cell %d, minColWidth = %d and maxColWidth = %d\n", 
                  cellIndex, minColWidth, maxColWidth);
      }

      if (gsDebug==PR_TRUE) 
      {
        printf ("  for determining width of col %d %s:\n",
                colIndex, !IsFixedWidth(colPosition)? "(P)":"(A)");
        printf ("    minTableWidth = %d and maxTableWidth = %d\n", aMinTableWidth, aMaxTableWidth);
        printf ("    minColWidth = %d and maxColWidth = %d\n", minColWidth, maxColWidth);
        printf ("    aAvailWidth = %d\n", aAvailWidth);
      }

      // Get column width if it has one
      nscoord specifiedProportion = -1;
      float specifiedPercentageColWidth = -1.0f;
      PRBool isAutoWidth = PR_FALSE;
      switch (colPosition->mWidth.GetUnit()) {
        case eStyleUnit_Percent:
          specifiedPercentageColWidth = colPosition->mWidth.GetPercentValue();
          break;
        case eStyleUnit_Proportional:
          specifiedProportion = colPosition->mWidth.GetIntValue();
          break;
        case eStyleUnit_Auto:
          isAutoWidth = PR_TRUE;
          break;
        default:
          break;
      }

      // the table fits in the space somewhere between its min and max size
      // so dole out the available space appropriately
      if (0==specifiedProportion || 0.0==specifiedPercentageColWidth)
      { // col width is specified to be the minimum
        mTableFrame->SetColumnWidth(colIndex, minColWidth);
        if (gsDebug==PR_TRUE) 
          printf ("  4 (0): col %d set to min width = %d because style set proportionalWidth=0\n", 
                  colIndex, mTableFrame->GetColumnWidth(colIndex));
      }
      else if (1==numCols)
      { // there is only one column, so it should be as wide as the available space allows it to be
        if (gsDebug==PR_TRUE) printf ("  4 one-column:  col %d  set to width = %d\n", colIndex, aAvailWidth);
        mTableFrame->SetColumnWidth(colIndex, aAvailWidth);
      }
      else if (PR_TRUE==isAutoWidth)
      { // column's width is determined by its content
        PRUint32 W = aMaxWidth - aMinTableWidth;
        PRUint32 D = aMaxTableWidth - aMinTableWidth;
        if (0==D) // fixed-size table
          D=1;
        PRUint32 d = maxColWidth - minColWidth;
        PRInt32 width = (d*W)/D;
        mTableFrame->SetColumnWidth(colIndex, minColWidth + width);
        if (gsDebug==PR_TRUE) 
          printf ("  4 auto-width:  col %d  W=%d  D=%d  d=%d, set to width = %d\n", 
                  colIndex, W, D, d, mTableFrame->GetColumnWidth(colIndex));
      }
      else if (0!=specifiedProportion)
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
          new ProportionalColumnLayoutStruct(colIndex, minColWidth, maxColWidth, specifiedProportion);
        proportionalColumnsList->AppendElement(info);
        totalSlices += specifiedProportion;
        if (1!=specifiedProportion)
          equalWidthColumns = PR_FALSE;
      }
      else
      {  // give each remaining column a percentage of the remaining space
        PRInt32 percentage = -1;
        if (NS_UNCONSTRAINEDSIZE==aAvailWidth)
        { // since the "remaining space" is infinite, give the column it's max requested size
          mTableFrame->SetColumnWidth(colIndex, maxColWidth);
        }
        else
        {
          if (-1.0f != specifiedPercentageColWidth)
            percentage = (PRInt32)(specifiedPercentageColWidth*100.0f); // TODO: rounding errors?
          if (-1==percentage)
            percentage = 100/numCols;
          // base the % on the total max width 
          mTableFrame->SetColumnWidth(colIndex, (percentage*aMaxWidth)/100);
          // if the column was computed to be too small, enlarge the column
          if (mTableFrame->GetColumnWidth(colIndex) <= minColWidth)
          {
            mTableFrame->SetColumnWidth(colIndex, minColWidth);
            if (maxOfAllMinColWidths < minColWidth)
              maxOfAllMinColWidths = minColWidth;
          }
        }
        if (gsDebug==PR_TRUE) 
        {
          printf ("  4b: col %d given %d percent of maxWidth %d, set to width = %d\n", 
                  colIndex, percentage, aMaxWidth, mTableFrame->GetColumnWidth(colIndex));
          if (0!=maxOfAllMinColWidths)
            printf("   and setting maxOfAllMins to %d\n", maxOfAllMinColWidths);
        }
      }
    }
  }
  /* --- post-process if necessary --- */
  // first, assign a width to proportional-width columns
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
          printf ("  3c: col %d given %d proportion of remaining space %d, set to width = %d\n", 
                  info->mColIndex, info->mProportion, widthRemaining, computedColWidth);
      }
      delete info;
    }
    delete proportionalColumnsList;
  }

  /*
  // if columns have equal width, and some column's content couldn't squeeze into the computed size, 
  // then expand every column to the min size of the column with the largest min size
  if (equalWidthColumns && 0!=maxOfAllMinColWidths)
  {
    if (gsDebug==PR_TRUE) printf("  EqualColWidths specified, so setting all col widths to %d\n", maxOfAllMinColWidths);
    for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
      mTableFrame->SetColumnWidth(colIndex, maxOfAllMinColWidths);
  }
  */

  if (nsnull!=spanList)
    delete spanList;

  return result;
}


