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
#include "nsTableRowFrame.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsTableCell.h"
#include "nsCellLayoutData.h"
#include "nsIView.h"

#ifdef NS_DEBUG
static PRBool gsDebug1 = PR_FALSE;
static PRBool gsDebug2 = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
#else
static const PRBool gsDebug1 = PR_FALSE;
static const PRBool gsDebug2 = PR_FALSE;
#endif

static NS_DEFINE_IID(kStyleMoleculeSID, NS_STYLEMOLECULE_SID);

nsTableRowFrame::nsTableRowFrame(nsIContent* aContent,
                     PRInt32     aIndexInParent,
                     nsIFrame*   aParentFrame)
  : nsContainerFrame(aContent, aIndexInParent, aParentFrame),
    mTallestCell(0)
{
}

nsTableRowFrame::~nsTableRowFrame()
{
}

void nsTableRowFrame::ResetMaxChildHeight()
{
  mTallestCell=0;
}

void nsTableRowFrame::SetMaxChildHeight(PRInt32 aChildHeight)
{
  if (mTallestCell<aChildHeight)
    mTallestCell = aChildHeight;
}

NS_METHOD nsTableRowFrame::Paint(nsIPresContext& aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect& aDirtyRect)
{
  // for debug...
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,0,128));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
}

// aDirtyRect is in our coordinate system
// child rect's are also in our coordinate system
/** overloaded method from nsContainerFrame.  The difference is that 
  * we don't want to clip our children, so a cell can do a rowspan
  */
void nsTableRowFrame::PaintChildren(nsIPresContext&      aPresContext,
                                     nsIRenderingContext& aRenderingContext,
                                     const nsRect&        aDirtyRect)
{
  nsIFrame* kid = mFirstChild;
  while (nsnull != kid) {
    nsIView *pView;
     
    kid->GetView(pView);
    if (nsnull == pView) {
      nsRect kidRect;
      kid->GetRect(kidRect);
      nsRect damageArea(aDirtyRect);
      // Translate damage area into kid's coordinate system
      nsRect kidDamageArea(damageArea.x - kidRect.x, damageArea.y - kidRect.y,
                           damageArea.width, damageArea.height);
      aRenderingContext.PushState();
      aRenderingContext.Translate(kidRect.x, kidRect.y);
      kid->Paint(aPresContext, aRenderingContext, kidDamageArea);
      if (nsIFrame::GetShowFrameBorders()) {
        aRenderingContext.SetColor(NS_RGB(255,0,0));
        aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
      }
      aRenderingContext.PopState();
    } else {
      NS_RELEASE(pView);
    }
    kid->GetNextSibling(kid);
  }
}


/** returns the tallest child in this row (ignoring any cell with rowspans) */
PRInt32 nsTableRowFrame::GetTallestChild() const
{
  return mTallestCell;
}

/*

want to use min space the child can fit in to
determine its real size, unless fixed columns sizes
are present.

  */

/** Layout the entire row.
  * This method stacks rows horizontally according to HTML 4.0 rules.
  * Rows are responsible for layout of their children.
  */
NS_METHOD
nsTableRowFrame::ResizeReflow(nsIPresContext*  aPresContext,
                              nsReflowMetrics& aDesiredSize,
                              const nsSize&    aMaxSize,
                              nsSize*          aMaxElementSize,
                              ReflowStatus&    aStatus)
{
  if (gsDebug1==PR_TRUE) 
    printf("nsTableRowFrame::ResizeReflow - %p with aMaxSize = %d, %d\n",
            this, aMaxSize.width, aMaxSize.height);
#ifdef NS_DEBUG
  PreReflowCheck();
#endif

  PRInt32 maxCellHeight = 0;
  ResetMaxChildHeight();

  aStatus = frComplete;

  mFirstContentOffset = mLastContentOffset = 0;

  nsIContent* c = mContent;

  nsSize availSize(aMaxSize);
  nsSize maxSize(0, 0);
  nsSize kidMaxSize(0,0);
  nsSize kidMaxElementSize(0,0);
  nsReflowMetrics kidSize;
  nscoord cellXOffset = 0;
  nscoord maxAscent = 0;
  nscoord maxDescent = 0;
  PRInt32 kidIndex = 0;
  PRInt32 lastIndex = c->ChildCount();
  nsIFrame* prevKidFrame = nsnull;/* XXX incremental reflow! */

// Row doesn't factor in insets, the cells do that

  nsTableFrame *tableFrame;
   
  mContentParent->GetContentParent((nsIFrame*&)tableFrame);

  for (;;) {
    nsIContent* kid = c->ChildAt(kidIndex);   // kid: REFCNT++
    if (nsnull == kid) {
      aStatus = frComplete;
      break;
    }

    // get frame, creating one if needed
    nsIFrame* kidFrame;
     
    ChildAt(kidIndex, kidFrame);
    if (nsnull==kidFrame)
    {
      nsIContentDelegate* kidDel;
      kidDel = kid->GetDelegate(aPresContext);
      kidFrame = kidDel->CreateFrame(aPresContext, kid, kidIndex, this);
      mChildCount++;
      NS_RELEASE(kidDel);
      // Resolve style
      nsIStyleContext* kidStyleContext =
        aPresContext->ResolveStyleContextFor(kid, this);
      NS_ASSERTION(nsnull!=kidStyleContext, "bad style context for kid.");
      kidFrame->SetStyleContext(kidStyleContext);
      NS_RELEASE(kidStyleContext);
    }

    nsTableCell* cell;
     
    kidFrame->GetContent((nsIContent*&)cell);  // cell: ADDREF++
    // Try to reflow the child into the available space.
    if (NS_UNCONSTRAINEDSIZE == availSize.width)
    {  // Each cell is given the entire row width to try to lay out into
      aStatus = ReflowChild(kidFrame, aPresContext, kidSize, availSize, &kidMaxSize);
      if (gsDebug1) printf("reflow of cell returned result = %s with desired=%d,%d, min = %d,%d\n",
                            aStatus==frComplete?"complete":"NOT complete", 
                            kidSize.width, kidSize.height, kidMaxSize.width, kidMaxSize.height);
      nsCellLayoutData kidLayoutData((nsTableCellFrame *)kidFrame, &kidSize, &kidMaxSize);
      tableFrame->SetCellLayoutData(&kidLayoutData, cell);
    }
    else
    { // we're in a constrained situation, so get the avail width from the known column widths
      nsTableFrame *innerTableFrame;
       
      mContentParent->GetContentParent((nsIFrame*&)innerTableFrame);
      nsCellLayoutData *cellData = innerTableFrame->GetCellLayoutData(cell);
      PRInt32 cellStartingCol = cell->GetColIndex();
      PRInt32 cellColSpan = cell->GetColSpan();
      PRInt32 availWidth = 0;
      for (PRInt32 numColSpan=0; numColSpan<cellColSpan; numColSpan++)
        availWidth += innerTableFrame->GetColumnWidth(cellStartingCol+numColSpan);
      NS_ASSERTION(0<availWidth, "illegal width for this column");
      nsSize availSizeWithSpans(availWidth, availSize.height);
      aStatus = ReflowChild(kidFrame, aPresContext, kidSize, availSizeWithSpans, &kidMaxSize);
      if (gsDebug1) printf("reflow of cell returned result = %s with desired=%d,%d, min = %d,%d\n",
                            aStatus==frComplete?"complete":"NOT complete", 
                            kidSize.width, kidSize.height, kidMaxSize.width, kidMaxSize.height);

      // SEC: TODO: this next block could be smarter, 
      //            with this frame caching the column x offsets along with the widths
      cellXOffset = 0;
      for (PRInt32 colIndex=0; colIndex<cell->GetColIndex(); colIndex++)
        cellXOffset += innerTableFrame->GetColumnWidth(colIndex);
    }
    PRInt32 rowSpan = cell->GetRowSpan();
    if ((1==rowSpan) && (maxCellHeight<kidSize.height))
      maxCellHeight = kidSize.height;
    NS_RELEASE(cell);                                             // cell: ADDREF--

    // SEC: the next line is probably unnecessary
    // Place the child since some of it's content fit in us.
    kidFrame->SetRect(nsRect(cellXOffset, 0, kidSize.width, kidSize.height));
    if (kidSize.width > maxSize.width) {
      maxSize.width = kidSize.width;
    }
    if (NS_UNCONSTRAINEDSIZE==aMaxSize.height)
    {
      if ((1==rowSpan) && (kidSize.height > maxSize.height))
        maxSize.height = kidSize.height;
      if (kidMaxSize.height > kidMaxElementSize.height)
        kidMaxElementSize.height = kidMaxSize.height;
    }
    else
    { // in the constrained height case, our maxElementSize is the height of our tallest cell
      if ((1==rowSpan) && (kidSize.height > maxSize.height))
        maxSize.height = kidSize.height;
      if (maxSize.height > kidMaxElementSize.height)
        kidMaxElementSize.height = maxSize.height;
    }
    kidMaxElementSize.width += kidMaxSize.width;
    if (NS_UNCONSTRAINEDSIZE!=cellXOffset)
      cellXOffset+=kidSize.width;
    if (cellXOffset<=0)
      cellXOffset = NS_UNCONSTRAINEDSIZE;   // handle overflow

    // Link child frame into the list of children
    if (nsnull != prevKidFrame) {
      prevKidFrame->SetNextSibling(kidFrame);
    } else {
      // Our first child (**blush**)
      if (gsDebug1) printf ("row frame %p, setting first child to %p\n", this, kidFrame);
      mFirstChild = kidFrame;
      SetFirstContentOffset(kidFrame);
      if (gsDebug1) printf("ROW: set first content offset to %d\n", GetFirstContentOffset()); //@@@
    }
    prevKidFrame = kidFrame;

    kidIndex++;
    if (frNotComplete == aStatus) {
      // If the child didn't finish layout then it means that it used
      // up all of our available space (or needs us to split).
      mLastContentIsComplete = PR_FALSE;
      break;
    }
    NS_RELEASE(kid);  // kid: REFCNT--
  }

  if (nsnull != prevKidFrame) {
    if (!IsLastChild(prevKidFrame))
    {
      mGeometricParent->List();
    }
    NS_ASSERTION(IsLastChild(prevKidFrame), "unexpected last child");
    SetLastContentOffset(prevKidFrame);
  }

  // Return our size and our result
  aDesiredSize.width = cellXOffset;
  aDesiredSize.height = maxSize.height;
  aDesiredSize.ascent = maxAscent;
  aDesiredSize.descent = maxDescent;
  if (nsnull != aMaxElementSize) {
    *aMaxElementSize = kidMaxElementSize;
  }
  SetMaxChildHeight(maxCellHeight);  // remember height of tallest child who doesn't have a row span

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif

  if (gsDebug1==PR_TRUE) 
  {
    if (nsnull!=aMaxElementSize)
      printf("nsTableRowFrame::RR returning: %s with aDesiredSize=%d,%d, aMES=%d,%d\n",
              aStatus==frComplete?"Complete":"Not Complete",
              aDesiredSize.width, aDesiredSize.height,
              aMaxElementSize->width, aMaxElementSize->height);
    else
      printf("nsTableRowFrame::RR returning: %s with aDesiredSize=%d,%d, aMES=NSNULL\n", 
             aStatus==frComplete?"Complete":"Not Complete",
             aDesiredSize.width, aDesiredSize.height);
  }
  
  // testing...
  aStatus = frComplete;
  return NS_OK;
}

NS_METHOD
nsTableRowFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                             nsReflowMetrics& aDesiredSize,
                             const nsSize&    aMaxSize,
                             nsReflowCommand& aReflowCommand,
                             ReflowStatus&    aStatus)
{
  if (gsDebug1==PR_TRUE) printf("nsTableRowFrame::IncrementalReflow\n");
  aStatus = frComplete;
  return NS_OK;
}

NS_METHOD nsTableRowFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                                 nsIFrame*       aParent,
                                                 nsIFrame*&      aContinuingFrame)
{
  if (gsDebug1==PR_TRUE) printf("nsTableRowFrame::CreateContinuingFrame\n");
  nsTableRowFrame* cf = new nsTableRowFrame(mContent, mIndexInParent, aParent);
  PrepareContinuingFrame(aPresContext, aParent, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

nsresult nsTableRowFrame::NewFrame( nsIFrame** aInstancePtrResult,
                                    nsIContent* aContent,
                                    PRInt32     aIndexInParent,
                                    nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableRowFrame(aContent, aIndexInParent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

