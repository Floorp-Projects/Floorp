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
#include "nsColLayoutData.h"
#include "nsCellLayoutData.h"
#include "nsTableCol.h"
#include "nsTableCellFrame.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPtr.h"

NS_DEF_PTR(nsTableCol);
NS_DEF_PTR(nsTableCell);
NS_DEF_PTR(nsIStyleContext);


#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsDebugCLD = PR_FALSE;
static PRBool gsTiming = PR_FALSE;
static PRBool gsDebugMBP = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsDebugCLD = PR_FALSE;
static const PRBool gsTiming = PR_FALSE;
static const PRBool gsDebugMBP = PR_FALSE;
#endif

//ZZZ TOTAL HACK
PRBool isTableAutoWidth = PR_TRUE;
PRBool isAutoColumnWidths = PR_TRUE;

PRBool TableIsAutoWidth()
{ // ZZZ: TOTAL HACK
  return isTableAutoWidth; 
}

PRBool AutoColumnWidths()
{ // ZZZ: TOTAL HACK
  return isAutoColumnWidths;
}

BasicTableLayoutStrategy::BasicTableLayoutStrategy(nsTableFrame *aFrame)
{
  mTableFrame = aFrame;
}

BasicTableLayoutStrategy::~BasicTableLayoutStrategy()
{
}

PRBool BasicTableLayoutStrategy::BalanceColumnWidths(nsIPresContext* aPresContext,
                                                     PRInt32 aMaxWidth, 
                                                     PRInt32 aNumCols,
                                                     PRInt32 &aTotalFixedWidth,
                                                     PRInt32 &aMinTableWidth,
                                                     PRInt32 &aMaxTableWidth,
                                                     nsSize* aMaxElementSize)
{
  PRBool result = PR_TRUE;
  // Step 1 - assign the width of all fixed-width columns
  AssignFixedColumnWidths(aPresContext, aMaxWidth, aNumCols,
                          aTotalFixedWidth, aMinTableWidth, aMaxTableWidth);

  if (nsnull!=aMaxElementSize)
  {
    aMaxElementSize->width = aMinTableWidth;
    if (gsDebug) printf("  setting aMaxElementSize->width = %d\n", aMaxElementSize->width);
  }
  else
  {
    if (gsDebug) printf("  nsnull aMaxElementSize\n");
  }

  // Step 2 - assign the width of all proportional-width columns in the remaining space
  PRInt32 availWidth = aMaxWidth - aTotalFixedWidth;
  if (gsDebug==PR_TRUE) printf ("Step 2...\n  availWidth = %d\n", availWidth);
  if (TableIsAutoWidth())
  {
    if (gsDebug==PR_TRUE) printf ("  calling BalanceProportionalColumnsForAutoWidthTable\n");
    result = BalanceProportionalColumnsForAutoWidthTable(aPresContext,
                                                         availWidth, aMaxWidth,
                                                         aMinTableWidth, aMaxTableWidth);
  }
  else
  {
    if (gsDebug==PR_TRUE) printf ("  calling BalanceProportionalColumnsForSpecifiedWidthTable\n");
    result = BalanceProportionalColumnsForSpecifiedWidthTable(aPresContext,
                                                              availWidth, aMaxWidth,
                                                              aMinTableWidth, aMaxTableWidth);
  }
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
    nsTableColPtr col = colData->GetCol();    // col: ADDREF++
    NS_ASSERTION(col.IsNotNull(), "bad col");

    // need to track min/max column width for setting min/max table widths
    PRInt32 minColWidth = 0;
    PRInt32 maxColWidth = 0;
    nsVoidArray *cells = colData->GetCells();
    PRInt32 numCells = cells->Count();
    if (gsDebug==PR_TRUE) printf ("  for column %d  numCells = %d\n", colIndex, numCells);

#if XXX_need_access_to_column_frame_help
    // Get the columns's style
    nsIStyleContextPtr colSC;
    colFrame->GetStyleContext(aPresContext, colSC.AssignRef());
    nsStylePosition* colPosition = (nsStylePosition*)
      colSC->GetData(eStyleStruct_Position);

    // Get column width if it has one
    PRBool haveColWidth = PR_FALSE;
    nscoord colWidth;
    switch (colPosition->mWidth.GetUnit()) {
    default:
    case eStyleUnit_Auto:
    case eStyleUnit_Inherit:
      break;

    case eStyleUnit_Coord:
      haveColWidth = PR_TRUE;
      colWidth = colPosition->mWidth.GetCoordValue;
      break;

    case eStyleUnit_Percent:
    case eStyleUnit_Proportional:
      //XXX haveColWidth = PR_TRUE;
      //XXX colWidth = colPosition->mWidthPCT * something;
      break;
    }
#endif

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

      PRBool haveCellWidth = PR_FALSE;
      nscoord cellWidth;

      /*
       * The first cell in a column (in row 0) has special standing.
       * if the first cell has a width specification, it overrides the
       * COL width
       */
      if (0==cellIndex)
      {
        nsCellLayoutData * data = (nsCellLayoutData *)(cells->ElementAt(0));
        nsMargin  margin;
        nsresult  result = data->GetMargin(margin);        
        nsTableCellFrame *cellFrame = data->GetCellFrame();
        nsTableCellPtr cell;
        cellFrame->GetContent((nsIContent*&)(cell.AssignRef()));          // cell: REFCNT++

        // Get the cell's style
        nsIStyleContextPtr cellSC;
        cellFrame->GetStyleContext(aPresContext, cellSC.AssignRef());
        nsStylePosition* cellPosition = (nsStylePosition*)
          cellSC->GetData(eStyleStruct_Position);
        switch (cellPosition->mWidth.GetUnit()) {
        case eStyleUnit_Coord:
          haveCellWidth = PR_TRUE;
          cellWidth = cellPosition->mWidth.GetCoordValue();
          break;

        case eStyleUnit_Percent:
        case eStyleUnit_Proportional:
          // XXX write me when pct/proportional are supported
          // XXX haveCellWidth = PR_TRUE;
          // XXX cellWidth = cellPosition->mWidth;
          break;

        default:
        case eStyleUnit_Inherit:
        case eStyleUnit_Auto:
          break;
        }
      }

#if XXX_need_access_to_column_frame_help
      switch (colPosition->mWidth.GetUnit()) {
      default:
      case eStyleUnit_Auto:
      case eStyleUnit_Inherit:
        break;

      case eStyleUnit_Coord:
        {
          // This col has a fixed width, so set the cell's width to the
          // larger of (specified width, largest max_element_size of the
          // cells in the column)
          PRInt32 widthForThisCell = max(cellMinSize->width, colPosition->mWidth.GetCoordValue());
          if (mTableFrame->GetColumnWidth(colIndex) < widthForThisCell)
          {
            if (gsDebug) printf ("    setting fixed width to %d\n",widthForThisCell);
            mTableFrame->SetColumnWidth(colIndex, widthForThisCell);
            maxColWidth = widthForThisCell;
          }
        }
        break;

      case NS_STYLE_POSITION_VALUE_PCT:
      case NS_STYLE_POSITION_VALUE_PROPORTIONAL:
        // XXX write me when pct/proportional are supported
        break;
      }
#endif

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

#if 0
    // if the col is fixed-width, expand the col to the specified
    // fixed width if necessary
    if (colStyle->fixedWidth > mTableFrame->GetColumnWidth(colIndex))
      mTableFrame->SetColumnWidth(colIndex, colStyle->fixedWidth);

    // keep a running total of the amount of space taken up by all
    // fixed-width columns
    aTotalFixedWidth += mTableFrame->GetColumnWidths(colIndex);
    if (gsDebug) {
      printf ("    after col %d, aTotalFixedWidth = %d\n",
              colIndex, aTotalFixedWidth);
    }
#endif

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

PRBool BasicTableLayoutStrategy::BalanceProportionalColumnsForSpecifiedWidthTable(nsIPresContext* aPresContext,
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

  if (NS_UNCONSTRAINEDSIZE==aMaxWidth)
  { // the max width of the table fits comfortably in the available space
    if (gsDebug) printf ("  * table laying out in NS_UNCONSTRAINEDSIZE, calling BalanceColumnsTableFits\n");
    result = BalanceColumnsTableFits(aPresContext, aAvailWidth);
  }
  else if (aMinTableWidth > aMaxWidth)
  { // the table doesn't fit in the available space
    if (gsDebug) printf ("  * min table does not fit, calling SetColumnsToMinWidth\n");
    result = SetColumnsToMinWidth(aPresContext);
  }
  else if (aMaxTableWidth <= aMaxWidth)
  { // the max width of the table fits comfortably in the available space
    if (gsDebug) printf ("  * table desired size fits, calling BalanceColumnsTableFits\n");
    result = BalanceColumnsTableFits(aPresContext, aAvailWidth);
  }
  else
  { // the table fits somewhere between its min and desired size
    if (gsDebug) printf ("  * table desired size does not fit, calling BalanceColumnsConstrained\n");
    result = BalanceColumnsConstrained(aPresContext, aAvailWidth,
                                       aMaxWidth, aMinTableWidth, aMaxTableWidth);
  }
  return result;
}

PRBool BasicTableLayoutStrategy::BalanceProportionalColumnsForAutoWidthTable( nsIPresContext* aPresContext,
                                                                  PRInt32 aAvailWidth,
                                                                  PRInt32 aMaxWidth,
                                                                  PRInt32 aMinTableWidth, 
                                                                  PRInt32 aMaxTableWidth)
{
  PRBool result = PR_TRUE;

  if (NS_UNCONSTRAINEDSIZE==aMaxWidth)
  { // the max width of the table fits comfortably in the available space
    if (gsDebug) printf ("  * table laying out in NS_UNCONSTRAINEDSIZE, calling BalanceColumnsTableFits\n");
    result = BalanceColumnsTableFits(aPresContext, aAvailWidth);
  }
  else if (aMinTableWidth > aMaxWidth)
  { // the table doesn't fit in the available space
    if (gsDebug) printf ("  * min table does not fit, calling SetColumnsToMinWidth\n");
    result = SetColumnsToMinWidth(aPresContext);
  }
  else if (aMaxTableWidth <= aMaxWidth)
  { // the max width of the table fits comfortably in the available space
    if (gsDebug) printf ("  * table desired size fits, calling BalanceColumnsTableFits\n");
    result = BalanceColumnsTableFits(aPresContext, aAvailWidth);
  }
  else
  { // the table fits somewhere between its min and desired size
    if (gsDebug) printf ("  * table desired size does not fit, calling BalanceColumnsConstrained\n");
    result = BalanceColumnsConstrained(aPresContext, aAvailWidth,
                                       aMaxWidth, aMinTableWidth, aMaxTableWidth);
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
    nsStylePosition* colPosition = nsnull;

    if (PR_TRUE==mTableFrame->IsProportionalWidth(colPosition))
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
                 colIndex, mTableFrame->IsProportionalWidth(colPosition)? "(P)":"(A)");
        printf ("    minColWidth = %d and maxColWidth = %d\n", minColWidth, maxColWidth);
      }

      // XXX BUG: why are we asking this again? this if is already in a
      // IsProportionalWidth == PR_TRUE case!
      if (PR_TRUE==mTableFrame->IsProportionalWidth(colPosition))
      { // this col has proportional width, so set its width based on the table width
        mTableFrame->SetColumnWidth(colIndex, minColWidth);
        if (gsDebug==PR_TRUE) 
          printf ("  2: col %d, set to width = %d\n", colIndex, mTableFrame->GetColumnWidth(colIndex));
      }
    }
  }
  return result;
}

PRBool BasicTableLayoutStrategy::BalanceColumnsTableFits(nsIPresContext* aPresContext, 
                                                         PRInt32 aAvailWidth)
{
#ifdef DEBUG
  nsIFrame *tablePIF=nsnull;
  mTableFrame->GetPrevInFlow(tablePIF);
  NS_ASSERTION(nsnull==tablePIF, "never ever call me on a continuing frame!");
#endif

  PRBool result = PR_TRUE;
  nsVoidArray *spanList=nsnull;
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

    // XXX Need columnFrame to ask the style question
    nsStylePosition* colPosition = nsnull;

    if (PR_TRUE==mTableFrame->IsProportionalWidth(colPosition))
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
        PRInt32 cellDesiredWidth = cellDesiredSize->width/colSpan + marginWidth;

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
                colIndex, mTableFrame->IsProportionalWidth(colPosition)? "(P)":"(A)");
        printf ("    minColWidth = %d and maxColWidth = %d\n", minColWidth, maxColWidth);
        printf ("    aAvailWidth = %d\n", aAvailWidth);
      }

      // XXX BUG: why are we asking this again? this if is already in a
      // IsProportionalWidth == PR_TRUE case!
      if (PR_TRUE==mTableFrame->IsProportionalWidth(colPosition))
      { // this col has proportional width, so set its width based on the table width
#if XXX_bug_kipp_about_this
        if (0==colStyle->proportionalWidth)
        { // col width is specified to be the minimum
          mTableFrame->SetColumnWidth(colIndex, minColWidth);
          if (gsDebug==PR_TRUE) 
            printf ("  3 (0): col %d set to min width = %d because style set proportionalWidth=0\n", 
                    colIndex, mTableFrame->GetColumnWidth(colIndex));
        }
        else // BUG? else? other code below has the else
#endif
        if (PR_TRUE==AutoColumnWidths())
        {  // give each remaining column it's desired width
           // if there is width left over, we'll factor that in after this loop is complete
          mTableFrame->SetColumnWidth(colIndex, maxColWidth);
          if (gsDebug==PR_TRUE) 
            printf ("  3a: col %d with availWidth %d, set to width = %d\n", 
                    colIndex, aAvailWidth, mTableFrame->GetColumnWidth(colIndex));
        }
        else
        {  // give each remaining column an equal percentage of the remaining space
          PRInt32 percentage = -1;
          if (NS_UNCONSTRAINEDSIZE==aAvailWidth)
          {
            mTableFrame->SetColumnWidth(colIndex, maxColWidth);
          }
          else
          {
#if XXX_bug_kipp_about_this
            percentage = colStyle->proportionalWidth;
            if (-1==percentage)
#endif
              percentage = 100/numCols;
            mTableFrame->SetColumnWidth(colIndex, (percentage*aAvailWidth)/100);
            // if the column was computed to be too small, enlarge the column
            if (mTableFrame->GetColumnWidth(colIndex) <= minColWidth)
              mTableFrame->SetColumnWidth(colIndex, minColWidth);
          }
          if (gsDebug==PR_TRUE) 
            printf ("  3b: col %d given %d percent of availWidth %d, set to width = %d\n", 
                    colIndex, percentage, aAvailWidth, mTableFrame->GetColumnWidth(colIndex));
        }
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
  PRInt32 maxOfAllMinColWidths = 0;
  nsVoidArray *spanList=nsnull;
  nsVoidArray *columnLayoutData = mTableFrame->GetColumnLayoutData();
  PRInt32 numCols = columnLayoutData->Count();
  for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
  { 
    nsColLayoutData * colData = (nsColLayoutData *)(columnLayoutData->ElementAt(colIndex));
    nsTableColPtr col = colData->GetCol();  // col: ADDREF++

#if XXX_bug_kipp_about_this
    // XXX BUG: mStyleContext is for the table frame not for the column.
    nsStyleMolecule* colStyle =
      (nsStyleMolecule*)mStyleContext->GetData(eStyleStruct_Molecule);
#else
    nsStylePosition* colPosition = nsnull;
#endif

    nsVoidArray *cells = colData->GetCells();
    PRInt32 minColWidth = 0;
    PRInt32 maxColWidth = 0;
    PRInt32 numCells = cells->Count();
    if (gsDebug==PR_TRUE) printf ("  for col %d\n", colIndex);

    if (PR_TRUE==mTableFrame->IsProportionalWidth(colPosition))
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
                colIndex, mTableFrame->IsProportionalWidth(colPosition)? "(P)":"(A)");
        printf ("    minTableWidth = %d and maxTableWidth = %d\n", aMinTableWidth, aMaxTableWidth);
        printf ("    minColWidth = %d and maxColWidth = %d\n", minColWidth, maxColWidth);
        printf ("    aAvailWidth = %d\n", aAvailWidth);
      }

      // XXX BUG: why are we asking this again? this if is already in a
      // IsProportionalWidth == PR_TRUE case!
      if (PR_TRUE==mTableFrame->IsProportionalWidth(colPosition))
      { // this col has proportional width, so set its width based on the table width
        // the table fits in the space somewhere between its min and max size
        // so dole out the available space appropriately
#if XXX_bug_kipp_about_this
        if (0==colStyle->proportionalWidth)
        { // col width is specified to be the minimum
          mTableFrame->SetColumnWidth(colIndex, minColWidth);
          if (gsDebug==PR_TRUE) 
            printf ("  4 (0): col %d set to min width = %d because style set proportionalWidth=0\n", 
                    colIndex, mTableFrame->GetColumnWidth(colIndex));
        }
        else
#endif
        if (AutoColumnWidths())
        {
          PRInt32 W = aMaxWidth - aMinTableWidth;
          PRInt32 D = aMaxTableWidth - aMinTableWidth;
          PRInt32 d = maxColWidth - minColWidth;
          mTableFrame->SetColumnWidth(colIndex, minColWidth + ((d*W)/D));
          if (gsDebug==PR_TRUE) 
            printf ("  4 auto-width:  col %d  W=%d  D=%d  d=%d, set to width = %d\n", 
                    colIndex, W, D, d, mTableFrame->GetColumnWidth(colIndex));
        }
        else
        {  // give each remaining column an equal percentage of the remaining space
#if XXX_bug_kipp_about_this
          PRInt32 percentage = colStyle->proportionalWidth;
          if (-1==percentage)
#endif
            PRInt32 percentage = 100/numCols;
          mTableFrame->SetColumnWidth(colIndex, (percentage*aAvailWidth)/100);
          // if the column was computed to be too small, enlarge the column
          if (mTableFrame->GetColumnWidth(colIndex) <= minColWidth)
          {
            mTableFrame->SetColumnWidth(colIndex, minColWidth);
            if (maxOfAllMinColWidths < minColWidth)
              maxOfAllMinColWidths = minColWidth;
          }
          if (gsDebug==PR_TRUE) 
          {
            printf ("  4 equal width: col %d given %d percent of availWidth %d, set to width = %d\n", 
                    colIndex, percentage, aAvailWidth, mTableFrame->GetColumnWidth(colIndex));
            if (0!=maxOfAllMinColWidths)
              printf("   and setting maxOfAllMins to %d\n", maxOfAllMinColWidths);
          }
        }
      }
    }
  }

  // post-process if necessary

  // if columns have equal width, and some column's content couldn't squeeze into the computed size, 
  // then expand every column to the min size of the column with the largest min size
  if (!AutoColumnWidths() && 0!=maxOfAllMinColWidths)
  {
    if (gsDebug==PR_TRUE) printf("  EqualColWidths specified, so setting all col widths to %d\n", maxOfAllMinColWidths);
    for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
      mTableFrame->SetColumnWidth(colIndex, maxOfAllMinColWidths);
  }

  if (nsnull!=spanList)
    delete spanList;

  return result;
}


