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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsOutlinerBodyFrame.h"
#include "nsXULAtoms.h"
#include "nsIContent.h"
#include "nsIStyleContext.h"


//
// NS_NewOutlinerFrame
//
// Creates a new TreeCell frame
//
nsresult
NS_NewOutlinerBodyFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsOutlinerBodyFrame* it = new (aPresShell) nsOutlinerBodyFrame(aPresShell);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewOutlinerFrame


// Constructor
nsOutlinerBodyFrame::nsOutlinerBodyFrame(nsIPresShell* aPresShell)
:nsLeafBoxFrame(aPresShell),
 mRowStyleCache(nsnull),
 mCellStyleCache(nsnull),
 mTopRowIndex(0)
{}

// Destructor
nsOutlinerBodyFrame::~nsOutlinerBodyFrame()
{
}

NS_IMETHODIMP_(nsrefcnt) 
nsOutlinerBodyFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsOutlinerBodyFrame::Release(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetStore(nsIOutlinerStore * *aStore)
{
  *aStore = mStore;
  NS_IF_ADDREF(*aStore);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::SetStore(nsIOutlinerStore * aStore)
{
  mStore = aStore;
  
  // Changing the store causes us to refetch our data.  This will
  // necessarily entail a full invalidation of the outliner.
  mTopRowIndex = 0;
  Invalidate();
  
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::Select(PRInt32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::ToggleSelect(PRInt32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::RangedSelect(PRInt32 aStartIndex, PRInt32 aEndIndex)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetSelectedRows(nsIOutlinerRangeList** aResult)
{
  *aResult = mSelectedRows;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::ClearSelection()
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvertSelection()
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::SelectAll()
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetIndexOfVisibleRow(PRInt32 *_retval)
{
  *_retval = mTopRowIndex;
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetPageCount(PRInt32 *_retval)
{
  *_retval = mPageCount;
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::Invalidate()
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvalidateRow(PRInt32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvalidateCell(PRInt32 aRow, const PRUnichar *aColID)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvalidateRange(PRInt32 aStart, PRInt32 aEnd)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvalidateScrollbar()
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetCellAt(PRInt32 x, PRInt32 y, PRInt32 *row, PRUnichar **colID)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::RowsAdded(PRInt32 index, PRInt32 count)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::RowsRemoved(PRInt32 index, PRInt32 count)
{
  return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsOutlinerBodyFrame)
  NS_INTERFACE_MAP_ENTRY(nsIOutlinerBoxObject)
NS_INTERFACE_MAP_END_INHERITING(nsLeafFrame)