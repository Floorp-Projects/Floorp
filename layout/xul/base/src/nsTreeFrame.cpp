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
#include "nsIContent.h"
#include "nsCSSRendering.h"
#include "nsTreeCellFrame.h"
#include "nsCellMap.h"

//
// NS_NewTreeFrame
//
// Creates a new tree frame
//
nsresult
NS_NewTreeFrame (nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeFrame* it = new nsTreeFrame;
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTreeFrame


// Constructor
nsTreeFrame::nsTreeFrame()
:nsTableFrame(),mSlatedForReflow(PR_FALSE) { }

// Destructor
nsTreeFrame::~nsTreeFrame()
{
}

void nsTreeFrame::SetSelection(nsIPresContext& aPresContext, nsTreeCellFrame* aFrame)
{
  PRInt32 count = mSelectedItems.Count();
  if (count == 1) {
    // See if we're already selected.
    nsTreeCellFrame* frame = (nsTreeCellFrame*)mSelectedItems[0];
    if (frame == aFrame)
      return;
  }
			
	ClearSelection(aPresContext);
	mSelectedItems.AppendElement(aFrame);
	aFrame->Select(aPresContext, PR_TRUE);

  FireChangeHandler(aPresContext);
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
					cellFrame = cellData->mSpanData->mOrigCell;
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

  FireChangeHandler(aPresContext);
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
				cellFrame = cellData->mSpanData->mOrigCell;
		}

		// We now have the cell that should be selected. 
		nsTreeCellFrame* pTreeCell = NS_STATIC_CAST(nsTreeCellFrame*, cellFrame);
		mSelectedItems.AppendElement(pTreeCell);
		pTreeCell->Select(aPresContext, PR_TRUE);
	}

  FireChangeHandler(aPresContext);
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

void
nsTreeFrame::RemoveFromSelection(nsIPresContext& aPresContext, nsTreeCellFrame* frame)
{
  PRInt32 count = mSelectedItems.Count();
  for (PRInt32 i = 0; i < count; i++)
	{
		// Remove the tree cell from the selection.
		nsTreeCellFrame* theFrame = (nsTreeCellFrame*)mSelectedItems[i];
    if (theFrame == frame) {
      mSelectedItems.RemoveElementAt(i);
	  FireChangeHandler(aPresContext);
      return;
    }
	}

}

void nsTreeFrame::MoveUp(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame)
{
	PRInt32 rowIndex;
  pFrame->GetRowIndex(rowIndex);
	PRInt32 colIndex;
  pFrame->GetColIndex(colIndex);
	if (rowIndex > 0)
	{
		MoveToRowCol(aPresContext, rowIndex-1, colIndex, pFrame);
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
		MoveToRowCol(aPresContext, rowIndex+1, colIndex, pFrame);
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
		MoveToRowCol(aPresContext, rowIndex, colIndex-1, pFrame);
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
		MoveToRowCol(aPresContext, rowIndex, colIndex+1, pFrame);
	}
}

void nsTreeFrame::MoveToRowCol(nsIPresContext& aPresContext, PRInt32 row, PRInt32 col, nsTreeCellFrame* pFrame)
{
	nsTableCellFrame *cellFrame = mCellMap->GetCellFrameAt(row, col);
	if (nsnull==cellFrame)
	{
		CellData *cellData = mCellMap->GetCellAt(row, col);
		if (nsnull!=cellData)
			cellFrame = cellData->mSpanData->mOrigCell;
	}

	// We now have the cell that should be selected. 
	nsTreeCellFrame* pTreeCell = NS_STATIC_CAST(nsTreeCellFrame*, cellFrame);
	SetSelection(aPresContext, pTreeCell);

  FireChangeHandler(aPresContext);
}

void nsTreeFrame::FireChangeHandler(nsIPresContext& aPresContext)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsEvent event;
  event.eventStructType = NS_EVENT;

  event.message = NS_FORM_CHANGE;
  if (nsnull != mContent) {

    // Set up the target by doing a PreHandleEvent

    mContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);
  }
}

NS_IMETHODIMP 
nsTreeFrame::Destroy(nsIPresContext& aPresContext)
{
  ClearSelection(aPresContext);
  return nsTableFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsTreeFrame::Reflow(nsIPresContext&          aPresContext,
							      nsHTMLReflowMetrics&     aMetrics,
							      const nsHTMLReflowState& aReflowState,
							      nsReflowStatus&          aStatus)
{
  mSlatedForReflow = PR_FALSE;
  return nsTableFrame::Reflow(aPresContext, aMetrics, aReflowState, aStatus);
}
