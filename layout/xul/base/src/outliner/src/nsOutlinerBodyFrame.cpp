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
#include "nsIPresContext.h"
#include "nsOutlinerBodyFrame.h"
#include "nsXULAtoms.h"
#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIContent.h"
#include "nsICSSStyleRule.h"

// The style context cache impl
nsresult 
nsOutlinerStyleCache::GetStyleContext(nsICSSPseudoComparator* aComparator,
                                      nsIPresContext* aPresContext, nsIContent* aContent, 
                                      nsIStyleContext* aContext, nsISupportsArray* aInputWord,
                                      nsIStyleContext** aResult)
{
  *aResult = nsnull;
  
  nsCOMPtr<nsIStyleContext> nextContext;
  nsIAtom* pseudo = NS_NewAtom(":-moz-outliner-row");
  aPresContext->ResolvePseudoStyleWithComparator(aContent, pseudo,
                                             aContext, PR_FALSE,
                                             aComparator, getter_AddRefs(nextContext));

  PRUint32 count;
  aInputWord->Count(&count);
  nsDFAState startState(0);
  nsDFAState* currState = &startState;
  nsCOMPtr<nsIStyleContext> currContext = aContext;
  for (PRUint32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIAtom> pseudo = getter_AddRefs(NS_STATIC_CAST(nsIAtom*, aInputWord->ElementAt(i)));
    nsTransitionKey key(currState->GetStateID(), pseudo);
    if (!mTransitionTable) {
      // Automatic miss. Build the table
      mTransitionTable = new nsHashtable;
    }
    else
      currState = NS_STATIC_CAST(nsDFAState*, mTransitionTable->Get(&key));

    if (!currState) {
      // We had a miss. Make a new state and add it to our hash.
      currState = new nsDFAState(mNextState);
      mNextState++;
      mTransitionTable->Put(&key, currState);
    }

    // Look up our style context at this step of the computation.
    nsCOMPtr<nsIStyleContext> nextContext;
    if (mCache)
      nextContext = getter_AddRefs(NS_STATIC_CAST(nsIStyleContext*, mCache->Get(currState)));
    if (!nextContext) {
      // We missed. Resolve this pseudo-style.
      aPresContext->ResolvePseudoStyleWithComparator(aContent, pseudo,
                                                     currContext, PR_FALSE,
                                                     aComparator,
                                                     getter_AddRefs(nextContext));
      // Put it in our table.
      if (!mCache)
        mCache = new nsSupportsHashtable;
      mCache->Put(currState, nextContext);
    }

    // Advance to the next context.
    currContext = nextContext;
  }

  if (currState == &startState)
    // No input word supplied.
    return NS_OK;

  // We're in some final state. Return the last context resolved.
  *aResult = currContext;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

//
// NS_NewOutlinerFrame
//
// Creates a new outliner frame
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
 mTopRowIndex(0), mColumns(nsnull)
{
  NS_NewISupportsArray(getter_AddRefs(mScratchArray));
}

// Destructor
nsOutlinerBodyFrame::~nsOutlinerBodyFrame()
{
  delete mColumns;
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

PRInt32 nsOutlinerBodyFrame::GetRowHeight(nsIPresContext* aPresContext)
{
  // Look up the correct height in our cache.
  nsCOMPtr<nsIStyleContext> rowContext;
  mScratchArray->Clear();
  mScratchArray->AppendElement(nsXULAtoms::mozoutlinerrow);
  GetPseudoStyleContext(aPresContext, getter_AddRefs(rowContext));
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

nsRect nsOutlinerBodyFrame::GetInnerBox()
{
  nsRect r(0,0,mRect.width, mRect.height);
  nsMargin m(0,0,0,0);
  const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)
        mStyleContext->GetStyleData(eStyleStruct_Spacing);
  mySpacing->GetBorderPadding(m);
  r.Deflate(m);
  return r;
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
  mRowHeight = GetRowHeight(aPresContext);
  mInnerBox = GetInnerBox();
  mPageCount = mInnerBox.height/mRowHeight;
  PRInt32 rowCount = 0;
  if (mView)
    mView->GetRowCount(&rowCount);
  
  // Ensure our column info is built.
  EnsureColumns(aPresContext);

  // Loop through our onscreen rows.
  for (PRInt32 i = mTopRowIndex; i < rowCount && i < mTopRowIndex+mPageCount+1; i++) {
    nsRect rowRect(0, mRowHeight*(i-mTopRowIndex), mInnerBox.width, mRowHeight);
    nsRect dirtyRect;
    if (dirtyRect.IntersectRect(aDirtyRect, rowRect))
      PaintRow(i, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
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
nsOutlinerBodyFrame::GetPseudoStyleContext(nsIPresContext* aPresContext, nsIStyleContext** aResult)
{
  return mStyleCache.GetStyleContext(this, aPresContext, mContent, mStyleContext, mScratchArray, aResult);
}

// Our comparator for resolving our complex pseudos
NS_IMETHODIMP
nsOutlinerBodyFrame::PseudoMatches(nsIAtom* aTag, nsCSSSelector* aSelector, PRBool* aResult)
{
  if (aSelector->mTag == aTag) {
    printf("LA!");
  }
  return NS_OK;

}

void
nsOutlinerBodyFrame::EnsureColumns(nsIPresContext* aPresContext)
{
  if (!mColumns) {
    nsCOMPtr<nsIContent> parent;
    mContent->GetParent(*getter_AddRefs(parent));
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(parent));

    nsCOMPtr<nsIDOMNodeList> cols;
    elt->GetElementsByTagName(NS_LITERAL_STRING("outlinercol"), getter_AddRefs(cols));

    nsCOMPtr<nsIPresShell> shell; 
    aPresContext->GetShell(getter_AddRefs(shell));

    PRUint32 count;
    cols->GetLength(&count);

    nsOutlinerColumn* currCol = nsnull;
    for (PRUint32 i = 0; i < count; i++) {
      nsCOMPtr<nsIDOMNode> node;
      cols->Item(i, getter_AddRefs(node));
      nsCOMPtr<nsIContent> child(do_QueryInterface(node));
      
      // Get the frame for this column.
      nsIFrame* frame;
      shell->GetPrimaryFrameFor(child, &frame);
      
      // Create a new column structure.
      nsOutlinerColumn* col = new nsOutlinerColumn(child, frame);
      if (currCol)
        currCol->SetNext(col);
      else mColumns = col;
      currCol = col;
    }
  }
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsOutlinerBodyFrame)
  NS_INTERFACE_MAP_ENTRY(nsIOutlinerBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsICSSPseudoComparator)
NS_INTERFACE_MAP_END_INHERITING(nsLeafFrame)