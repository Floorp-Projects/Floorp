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


#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
#endif

FixedTableLayoutStrategy::FixedTableLayoutStrategy(nsTableFrame *aFrame)
  : BasicTableLayoutStrategy(aFrame)
{
}

FixedTableLayoutStrategy::~FixedTableLayoutStrategy()
{
}

PRBool FixedTableLayoutStrategy::BalanceColumnWidths(nsIStyleContext *aTableStyle,
                                                     const nsHTMLReflowState& aReflowState,
                                                     nscoord aMaxWidth)
{
#ifdef NS_DEBUG
  nsIFrame *tablePIF=nsnull;
  mTableFrame->GetPrevInFlow(&tablePIF);
  NS_ASSERTION(nsnull==tablePIF, "never ever call me on a continuing frame!");
#endif

  PRBool result = PR_TRUE;

  NS_ASSERTION(nsnull!=aTableStyle, "bad arg");
  if (nsnull==aTableStyle)
    return PR_FALSE;


  if (PR_TRUE==gsDebug)
  {
    printf("\n%p: BALANCE COLUMN WIDTHS\n", mTableFrame);
    for (PRInt32 i=0; i<mNumCols; i++)
      printf("  col %d assigned width %d\n", i, mTableFrame->GetColumnWidth(i));
    printf("\n");
  }

  // XXX Hey Cujo, result has never been initialized...
  return result;
}

/* assign the width of all columns
 * if there is a colframe with a width attribute, use it as the column width
 * otherwise if there is a cell in the first row and it has a width attribute, use it
 *  if this cell includes a colspan, width is divided equally among spanned columns
 * otherwise the cell get a proportion of the remaining space 
 *  as determined by the table width attribute.  If no table width attribute, it gets 0 width
 */
PRBool FixedTableLayoutStrategy::AssignPreliminaryColumnWidths()
{
  if (gsDebug==PR_TRUE) printf ("** %p: AssignPreliminaryColumnWidths **\n", mTableFrame);
  
  PRInt32 colIndex;
  PRInt32 specifiedColumns=0;   // the number of columns whose width is given
  nscoord totalWidth = 0;       // the sum of the widths of the columns whose width is given

  PRBool * autoWidthColumns = new PRBool[mNumCols];
  nsCRT::memset(autoWidthColumns, PR_TRUE, mNumCols*sizeof(PRBool));

  // for every column, determine it's specified width
  for (colIndex = 0; colIndex<mNumCols; colIndex++)
  { 
    // Get column information
    nsTableColFrame *colFrame = mTableFrame->GetColFrame(colIndex);
    NS_ASSERTION(nsnull!=colFrame, "bad col frame");

    // Get the columns's style
    const nsStylePosition* colPosition;
    colFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)colPosition);

    // Get fixed column width if it has one
    if (eStyleUnit_Coord==colPosition->mWidth.GetUnit()) 
    {
      nscoord colWidth = colPosition->mWidth.GetCoordValue();
      mTableFrame->SetColumnWidth(colIndex, colWidth);
      totalWidth += colWidth;
      specifiedColumns++;
      autoWidthColumns[colIndex]=PR_FALSE;
      if (PR_TRUE==gsDebug)
        printf ("from col, col %d set to width %d\n", colIndex, colWidth);
    }
    else
    {
      nsTableCellFrame *cellFrame = mTableFrame->GetCellFrameAt(0, colIndex);
      if (nsnull!=cellFrame)
      {
        // Get the cell's style
        const nsStylePosition* cellPosition;
        cellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)cellPosition);

        // Get fixed column width if it has one
        if (eStyleUnit_Coord==cellPosition->mWidth.GetUnit()) 
        {
          PRInt32 colSpan = mTableFrame->GetEffectiveColSpan(colIndex, cellFrame);
          nscoord effectiveWidth = cellPosition->mWidth.GetCoordValue()/colSpan;
          mTableFrame->SetColumnWidth(colIndex, effectiveWidth);
          totalWidth += effectiveWidth;
          specifiedColumns++;
          autoWidthColumns[colIndex]=PR_FALSE;
          if (PR_TRUE==gsDebug)
              printf ("from cell, col %d set to width %d\n", colIndex, effectiveWidth);
        }
      }
    }
  }
  
  // for every column that did not have a specified width, compute its width from the remaining space
  if (mNumCols > specifiedColumns)
  {
    // Get the table's style
    const nsStylePosition* tablePosition;
    mTableFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)tablePosition);
    if (eStyleUnit_Coord==tablePosition->mWidth.GetUnit())
    {
      nscoord specifiedTableWidth = tablePosition->mWidth.GetCoordValue();
      nscoord remainingTableWidth = specifiedTableWidth - totalWidth;
      if (PR_TRUE==gsDebug)
        printf ("%p: specifiedTW=%d, remainingTW=%d\n", 
                mTableFrame, specifiedTableWidth, remainingTableWidth);
      if (0<remainingTableWidth)
      {
        nscoord widthPerColumn = remainingTableWidth/(mNumCols-specifiedColumns);
        for (colIndex = 0; colIndex<mNumCols; colIndex++)
        {
          if (PR_TRUE==autoWidthColumns[colIndex])
          {
            mTableFrame->SetColumnWidth(colIndex, widthPerColumn);
            totalWidth += widthPerColumn; 
            if (PR_TRUE==gsDebug)
              printf ("auto col %d set to width %d\n", colIndex, widthPerColumn);
          }
        }
      }
    }
  }
  // min/MaxTW is max of (specified table width, sum of specified column(cell) widths)
  mMinTableWidth = mMaxTableWidth = totalWidth;
  if (PR_TRUE==gsDebug)
    printf ("%p: aMinTW=%d, aMaxTW=%d\n", mTableFrame, mMinTableWidth, mMaxTableWidth);

  // clean up
  if (nsnull!=autoWidthColumns)
  {
    delete [] autoWidthColumns;
  }
  
  return PR_TRUE;
}




