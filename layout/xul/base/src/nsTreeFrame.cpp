/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsTreeFrame.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsTreeCellFrame.h"
#include "nsCellMap.h"

//
// NS_NewTreeFrame
//
// Creates a new tree frame
//
nsresult
NS_NewTreeFrame (nsIFrame*& aNewFrame)
{
  nsTreeFrame* theFrame = new nsTreeFrame;
  if (theFrame == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  aNewFrame = theFrame;
  return NS_OK;
  
} // NS_NewTreeFrame


// Constructor
nsTreeFrame::nsTreeFrame()
:nsTableFrame() { }

// Destructor
nsTreeFrame::~nsTreeFrame()
{
}

void nsTreeFrame::SetSelection(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame)
{
	ClearSelection(aPresContext);
	mSelectedItems.AppendElement(pFrame);
	pFrame->Select(aPresContext, PR_TRUE);
}

void nsTreeFrame::ToggleSelection(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame)
{
	PRInt32 inArray = mSelectedItems.IndexOf((void*)pFrame);
	if (inArray == -1)
	{
		// Add this row to our array of items.
		PRInt32 count = mSelectedItems.Count();
		if (count > 0)
		{
			// Some column is already selected.  This means we want to select the
			// cell in our row that is found in this column.
			nsTreeCellFrame* pOtherFrame = (nsTreeCellFrame*)mSelectedItems[0];
			
			PRInt32 colIndex;
      pOtherFrame->GetColIndex(colIndex);
			PRInt32 rowIndex;
      pFrame->GetRowIndex(rowIndex);

			// We now need to find the cell at this particular row and column.
			// This is the cell we should really select.
			nsTableCellFrame *cellFrame = mCellMap->GetCellFrameAt(rowIndex, colIndex);
			if (nsnull==cellFrame)
			{
				CellData *cellData = mCellMap->GetCellAt(rowIndex, colIndex);
				if (nsnull!=cellData)
					cellFrame = cellData->mRealCell->mCell;
			}

			// Select this cell frame.
			mSelectedItems.AppendElement(cellFrame);
			((nsTreeCellFrame*)cellFrame)->Select(aPresContext, PR_TRUE);
		}
		else
		{
			mSelectedItems.AppendElement(pFrame);
			pFrame->Select(aPresContext, PR_TRUE);
		}
	}
	else
	{
		// Remove this from our array of items.
		mSelectedItems.RemoveElementAt(inArray);
		pFrame->Select(aPresContext, PR_FALSE);
	}
}

void nsTreeFrame::RangedSelection(nsIPresContext& aPresContext, nsTreeCellFrame* pEndFrame)
{
	nsTreeCellFrame* pStartFrame = nsnull;
	PRInt32 count = mSelectedItems.Count();
	if (count == 0)
		pStartFrame = pEndFrame;
	else
		pStartFrame = (nsTreeCellFrame*)mSelectedItems[0];

	ClearSelection(aPresContext);

	// Select all cells between the two frames, but only in the start frame's column.
	PRInt32 colIndex;
  pStartFrame->GetColIndex(colIndex); // The column index of the selection.
	PRInt32 startRow;
  pStartFrame->GetRowIndex(startRow); // The starting row for the selection.
	PRInt32 endRow;
  pEndFrame->GetRowIndex(endRow);	   // The ending row for the selection.

	PRInt32 start = startRow > endRow ? endRow : startRow;
	PRInt32 end = startRow > endRow ? startRow : endRow;

	for (PRInt32 i = start; i <= end; i++)
	{
		// Select the cell at the appropriate index
		nsTableCellFrame *cellFrame = mCellMap->GetCellFrameAt(i, colIndex);
		if (nsnull==cellFrame)
		{
			CellData *cellData = mCellMap->GetCellAt(i, colIndex);
			if (nsnull!=cellData)
				cellFrame = cellData->mRealCell->mCell;
		}

		// We now have the cell that should be selected. 
		nsTreeCellFrame* pTreeCell = NS_STATIC_CAST(nsTreeCellFrame*, cellFrame);
		mSelectedItems.AppendElement(pTreeCell);
		pTreeCell->Select(aPresContext, PR_TRUE);
	}
}

void nsTreeFrame::ClearSelection(nsIPresContext& aPresContext)
{
	PRInt32 count = mSelectedItems.Count();
	for (PRInt32 i = 0; i < count; i++)
	{
		// Tell the tree cell to clear its selection.
		nsTreeCellFrame* pFrame = (nsTreeCellFrame*)mSelectedItems[i];
		pFrame->Select(aPresContext, PR_FALSE);
	}

	mSelectedItems.Clear();
}

void nsTreeFrame::MoveUp(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame)
{
	PRInt32 rowIndex;
  pFrame->GetRowIndex(rowIndex);
	PRInt32 colIndex;
  pFrame->GetColIndex(colIndex);
	if (rowIndex > 0)
	{
		MoveTo(aPresContext, rowIndex-1, colIndex, pFrame);
	}
}

void nsTreeFrame::MoveDown(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame)
{
	PRInt32 rowIndex;
  pFrame->GetRowIndex(rowIndex);
	PRInt32 colIndex;
  pFrame->GetColIndex(colIndex);
	PRInt32 totalRows = mCellMap->GetRowCount();

	if (rowIndex < totalRows-1)
	{
		MoveTo(aPresContext, rowIndex+1, colIndex, pFrame);
	}
}

void nsTreeFrame::MoveLeft(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame)
{
	PRInt32 rowIndex;
  pFrame->GetRowIndex(rowIndex);
	PRInt32 colIndex;
  pFrame->GetColIndex(colIndex);
	if (colIndex > 0)
	{
		MoveTo(aPresContext, rowIndex, colIndex-1, pFrame);
	}
}

void nsTreeFrame::MoveRight(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame)
{
	PRInt32 rowIndex;
  pFrame->GetRowIndex(rowIndex);
	PRInt32 colIndex;
  pFrame->GetColIndex(colIndex);
	PRInt32 totalCols = mCellMap->GetColCount();

	if (colIndex < totalCols-1)
	{
		MoveTo(aPresContext, rowIndex, colIndex+1, pFrame);
	}
}

void nsTreeFrame::MoveTo(nsIPresContext& aPresContext, PRInt32 row, PRInt32 col, nsTreeCellFrame* pFrame)
{
	nsTableCellFrame *cellFrame = mCellMap->GetCellFrameAt(row, col);
	if (nsnull==cellFrame)
	{
		CellData *cellData = mCellMap->GetCellAt(row, col);
		if (nsnull!=cellData)
			cellFrame = cellData->mRealCell->mCell;
	}

	// We now have the cell that should be selected. 
	nsTreeCellFrame* pTreeCell = NS_STATIC_CAST(nsTreeCellFrame*, cellFrame);
	SetSelection(aPresContext, pTreeCell);
}
