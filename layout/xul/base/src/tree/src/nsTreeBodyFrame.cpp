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
#include "nsISupportsArray.h"
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
 mTopRowIndex(0)
{
  NS_NewISupportsArray(getter_AddRefs(mScratchArray));
}

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

NS_IMETHODIMP nsOutlinerBodyFrame::GetView(nsIOutlinerView * *aView)
{
  *aView = mView;
  NS_IF_ADDREF(*aView);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::SetView(nsIOutlinerView * aView)
{
  mView = aView;
  
  // Changing the store causes us to refetch our data.  This will
  // necessarily entail a full invalidation of the outliner.
  mTopRowIndex = 0;
  Invalidate();
  
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

NS_IMETHODIMP nsOutlinerBodyFrame::RowsAppended(PRInt32 count)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::RowsInserted(PRInt32 index, PRInt32 count)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::RowsRemoved(PRInt32 index, PRInt32 count)
{
  return NS_OK;
}

PRInt32 nsOutlinerBodyFrame::GetRowHeight()
{
  // Look up the correct height in our cache.
  nsCOMPtr<nsIStyleContext> rowContext;
  mScratchArray->Clear();
  mScratchArray->AppendElement(nsXULAtoms::mozoutlinerrow);
  GetPseudoStyleContext(getter_AddRefs(rowContext));
  if (rowContext) {
    const nsStylePosition* myPosition = (const nsStylePosition*)
          rowContext->GetStyleData(eStyleStruct_Position);
    if (myPosition->mHeight.GetUnit() == eStyleUnit_Coord)  {
      PRInt32 val = myPosition->mHeight.GetCoordValue();
      if (val > 0) return val;
    }
  }
  return 16;
}

PRInt32 nsOutlinerBodyFrame::GetTotalHeight()
{
  nsRect r(0,0,mRect.width, mRect.height);
  nsMargin m(0,0,0,0);
  const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)
        mStyleContext->GetStyleData(eStyleStruct_Spacing);
  mySpacing->GetBorderPadding(m);
  r.Deflate(m);
  return r.height;
}

// Painting routines
NS_IMETHODIMP nsOutlinerBodyFrame::Paint(nsIPresContext*      aPresContext,
                                         nsIRenderingContext& aRenderingContext,
                                         const nsRect&        aDirtyRect,
                                         nsFramePaintLayer    aWhichLayer)
{
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);
  if (!disp->IsVisibleOrCollapsed())
    return NS_OK; // We're invisible.  Don't paint.

  // Handles painting our background, border, and outline.
  nsresult rv = nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  if (NS_FAILED(rv)) return rv;

  // Update our page count, our available height and our row height.
  mRowHeight = GetRowHeight();
  mTotalHeight = GetTotalHeight();
  mPageCount = mTotalHeight/mRowHeight;
  PRInt32 rowCount = 0;
  if (mView)
    mView->GetRowCount(&rowCount);
  
  // Loop through each line.
  for (PRInt32 i = mTopRowIndex; i < rowCount && i < mTopRowIndex+mPageCount+1; i++) {
    nsRect rowRect;
  }

  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::PaintRow(int aRowIndex, 
                                            nsIPresContext*      aPresContext,
                                            nsIRenderingContext& aRenderingContext,
                                            const nsRect&        aDirtyRect,
                                            nsFramePaintLayer    aWhichLayer)
{
  return NS_OK;
}
  
NS_IMETHODIMP nsOutlinerBodyFrame::PaintCell(int aRowIndex, 
                                             const PRUnichar* aColID, 
                                             nsIPresContext*      aPresContext,
                                             nsIRenderingContext& aRenderingContext,
                                             const nsRect&        aDirtyRect,
                                             nsFramePaintLayer    aWhichLayer)
{
  return NS_OK;
}

// The style cache.
nsresult 
nsOutlinerBodyFrame::GetPseudoStyleContext(nsIStyleContext** aResult)
{
  *aResult = nsnull;
  return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsOutlinerBodyFrame)
  NS_INTERFACE_MAP_ENTRY(nsIOutlinerBoxObject)
NS_INTERFACE_MAP_END_INHERITING(nsLeafFrame)