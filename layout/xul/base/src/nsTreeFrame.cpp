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

#include "nsCOMPtr.h"
#include "nsTreeFrame.h"
#include "nsIStyleContext.h"
#include "nsIContent.h"
#include "nsCSSRendering.h"
#include "nsTreeCellFrame.h"
#include "nsCellMap.h"
#include "nsIDOMXULTreeElement.h"

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
  nsCOMPtr<nsIContent> cellContent;
  aFrame->GetContent(getter_AddRefs(cellContent));

  nsCOMPtr<nsIContent> rowContent;
  cellContent->GetParent(*getter_AddRefs(rowContent));

  nsCOMPtr<nsIContent> itemContent;
  rowContent->GetParent(*getter_AddRefs(itemContent));

  nsCOMPtr<nsIDOMXULTreeElement> treeElement = do_QueryInterface(mContent);
  nsCOMPtr<nsIDOMXULElement> cellElement = do_QueryInterface(cellContent);
  nsCOMPtr<nsIDOMXULElement> itemElement = do_QueryInterface(itemContent);
  treeElement->SelectItem(itemElement);
  treeElement->SelectCell(cellElement);

  FireChangeHandler(aPresContext);
}

void nsTreeFrame::ToggleSelection(nsIPresContext& aPresContext, nsTreeCellFrame* aFrame)
{
	nsCOMPtr<nsIContent> cellContent;
  aFrame->GetContent(getter_AddRefs(cellContent));

  nsCOMPtr<nsIContent> rowContent;
  cellContent->GetParent(*getter_AddRefs(rowContent));

  nsCOMPtr<nsIContent> itemContent;
  rowContent->GetParent(*getter_AddRefs(itemContent));

  nsCOMPtr<nsIDOMXULTreeElement> treeElement = do_QueryInterface(mContent);
  nsCOMPtr<nsIDOMXULElement> cellElement = do_QueryInterface(cellContent);
  nsCOMPtr<nsIDOMXULElement> itemElement = do_QueryInterface(itemContent);

  treeElement->ToggleItemSelection(itemElement);
  treeElement->ToggleCellSelection(cellElement);

  FireChangeHandler(aPresContext);
}

void nsTreeFrame::RangedSelection(nsIPresContext& aPresContext, nsTreeCellFrame* pEndFrame)
{
 // XXX Re-implement!
 // FireChangeHandler(aPresContext);
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
	nsTableCellFrame *cellFrame = mCellMap->GetCellInfoAt(row, col);

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
