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
#include "nsTableCellFrame.h"
#include "nsBodyFrame.h"
#include "nsReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsCSSLayout.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
#else
static const PRBool gsDebug = PR_FALSE;
#endif

static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);
static NS_DEFINE_IID(kStyleBorderSID, NS_STYLEBORDER_SID);
static NS_DEFINE_IID(kStyleColorSID, NS_STYLECOLOR_SID);
static NS_DEFINE_IID(kStyleTextSID, NS_STYLETEXT_SID);

/**
  */
nsTableCellFrame::nsTableCellFrame(nsIContent* aContent,
                                   PRInt32     aIndexInParent,
                                   nsIFrame*   aParentFrame)
  : nsContainerFrame(aContent, aIndexInParent, aParentFrame)
{
}

nsTableCellFrame::~nsTableCellFrame()
{
}

NS_METHOD nsTableCellFrame::Paint(nsIPresContext& aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  const nsRect& aDirtyRect)
{
  nsStyleColor* myColor =
    (nsStyleColor*)mStyleContext->GetData(kStyleColorSID);
  nsStyleBorder* myBorder =
    (nsStyleBorder*)mStyleContext->GetData(kStyleBorderSID);
  NS_ASSERTION(nsnull!=myColor, "bad style color");
  NS_ASSERTION(nsnull!=myBorder, "bad style border");

  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                  aDirtyRect, mRect, *myColor);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                              aDirtyRect, mRect, *myBorder, 0);


  // for debug...
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,128,128));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
}

/**
*
* Align the cell's child frame within the cell
*
**/

void  nsTableCellFrame::VerticallyAlignChild(nsIPresContext* aPresContext)
{
  nsStyleSpacing* spacing =
      (nsStyleSpacing*)mStyleContext->GetData(kStyleSpacingSID);
  nsStyleText* textStyle =
      (nsStyleText*)mStyleContext->GetData(kStyleTextSID);
  
  nscoord topInset = spacing->mBorderPadding.top;
  nscoord bottomInset = spacing->mBorderPadding.bottom;
  PRUint8 verticalAlignFlags = textStyle->mVerticalAlignFlags;
  nscoord       height = mRect.height;

  nsRect        kidRect;  
  mFirstChild->GetRect(kidRect);
  nscoord       childHeight = kidRect.height;
  

  // Vertically align the child
  nscoord kidYTop = 0;
  switch (verticalAlignFlags) 
  {
    case NS_STYLE_VERTICAL_ALIGN_BASELINE:
    // Align the child's baseline at the max baseline
    //kidYTop = aMaxAscent - kidAscent;
    break;

    case NS_STYLE_VERTICAL_ALIGN_TOP:
    // Align the top of the child frame with the top of the box, 
    // minus the top padding
      kidYTop = topInset;
    break;

    case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
      kidYTop = height - childHeight - bottomInset;
    break;

    default:
    case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
      kidYTop = height/2 - childHeight/2;
  }
  mFirstChild->MoveTo(kidRect.x, kidYTop);
}

/** helper method to get the row span of this frame's content (which must be a cell) */
PRInt32 nsTableCellFrame::GetRowSpan()
{
  PRInt32 result = 0;
  nsTableCell *cellContent = (nsTableCell *)mContent;
  if (nsnull!=cellContent)
  {
    result = cellContent->GetRowSpan();
  }
  return result;
}

/** helper method to get the col span of this frame's content (which must be a cell) */
PRInt32 nsTableCellFrame::GetColSpan()
{
  PRInt32 result = 0;
  nsTableCell *cellContent = (nsTableCell *)mContent;
  if (nsnull!=cellContent)
  {
    result = cellContent->GetColSpan();
  }
  return result;
}

/** helper method to get the col index of this frame's content (which must be a cell) */
PRInt32 nsTableCellFrame::GetColIndex()
{
  PRInt32 result = 0;
  nsTableCell *cellContent = (nsTableCell *)mContent;
  if (nsnull!=cellContent)
  {
    result = cellContent->GetColIndex();
  }
  return result;
}

void nsTableCellFrame::CreatePsuedoFrame(nsIPresContext* aPresContext)
{
  // Do we have a prev-in-flow?
  if (nsnull == mPrevInFlow) {
    // No, create a column pseudo frame
    nsBodyFrame::NewFrame(&mFirstChild, mContent, mIndexInParent, this);
    mChildCount = 1;

    // Resolve style and set the style context
    nsIStyleContext* styleContext =
      aPresContext->ResolveStyleContextFor(mContent, this);             // styleContext: ADDREF++
    mFirstChild->SetStyleContext(styleContext);
    NS_RELEASE(styleContext);                                           // styleContext: ADDREF--
  } else {
    nsTableCellFrame* prevFrame = (nsTableCellFrame *)mPrevInFlow;

    nsIFrame* prevPseudoFrame = prevFrame->mFirstChild;
    NS_ASSERTION(prevFrame->ChildIsPseudoFrame(prevPseudoFrame), "bad previous pseudo-frame");

    // Create a continuing column
    prevPseudoFrame->CreateContinuingFrame(aPresContext, this, mFirstChild);
    mChildCount = 1;
  }
}

/**
  */
NS_METHOD nsTableCellFrame::ResizeReflow(nsIPresContext* aPresContext,
                                         nsReflowMetrics& aDesiredSize,
                                         const nsSize&   aMaxSize,
                                         nsSize*         aMaxElementSize,
                                         ReflowStatus&   aStatus)
{
  NS_PRECONDITION(nsnull!=aPresContext, "bad arg");

#ifdef NS_DEBUG
  //PreReflowCheck();
#endif

  aStatus = frComplete;
  if (gsDebug==PR_TRUE)
    printf("nsTableCellFrame::ResizeReflow: maxSize=%d,%d\n",
           aMaxSize.width, aMaxSize.height);

  mFirstContentOffset = mLastContentOffset = 0;

  nsSize availSize(aMaxSize);
  nsSize maxElementSize;
  nsSize *pMaxElementSize = aMaxElementSize;
  if (NS_UNCONSTRAINEDSIZE==aMaxSize.width)
    pMaxElementSize = &maxElementSize;
  nsReflowMetrics kidSize;
  nscoord x = 0;
  // SEC: what about ascent and decent???

  // Compute the insets (sum of border and padding)
  nsStyleSpacing* spacing =
    (nsStyleSpacing*)mStyleContext->GetData(kStyleSpacingSID);
  nscoord topInset = spacing->mBorderPadding.top;
  nscoord rightInset = spacing->mBorderPadding.right;
  nscoord bottomInset = spacing->mBorderPadding.bottom;
  nscoord leftInset = spacing->mBorderPadding.left;

  // reduce available space by insets
  if (NS_UNCONSTRAINEDSIZE!=availSize.width)
    availSize.width -= leftInset+rightInset;
  if (NS_UNCONSTRAINEDSIZE!=availSize.height)
    availSize.height -= topInset+bottomInset;

  mLastContentIsComplete = PR_TRUE;

  // get frame, creating one if needed
  if (nsnull==mFirstChild)
  {
    CreatePsuedoFrame(aPresContext);
  }

  // Try to reflow the child into the available space. It might not
  // fit or might need continuing.
  if (availSize.height < 0)
    availSize.height = 1;
  nsSize maxKidElementSize;
  if (gsDebug==PR_TRUE)
    printf("  nsTableCellFrame::ResizeReflow calling ReflowChild with availSize=%d,%d\n",
           availSize.width, availSize.height);
  aStatus = ReflowChild(mFirstChild, aPresContext, kidSize, availSize, pMaxElementSize);

  if (gsDebug==PR_TRUE)
  {
    if (nsnull!=pMaxElementSize)
      printf("  nsTableCellFrame::ResizeReflow: child returned desiredSize=%d,%d,\
             and maxElementSize=%d,%d\n",
             kidSize.width, kidSize.height,
             pMaxElementSize->width, pMaxElementSize->height);
    else
      printf("  nsTableCellFrame::ResizeReflow: child returned desiredSize=%d,%d,\
             and maxElementSize=nsnull\n",
             kidSize.width, kidSize.height);
  }

  SetFirstContentOffset(mFirstChild);
  if (gsDebug) printf("CELL: set first content offset to %d\n", GetFirstContentOffset()); //@@@
  SetLastContentOffset(mFirstChild);
  if (gsDebug) printf("CELL: set last content offset to %d\n", GetLastContentOffset()); //@@@

  // Place the child since some of it's content fit in us.
  mFirstChild->SetRect(nsRect(leftInset + x, topInset,
                           kidSize.width, kidSize.height));
  
    
  if (frNotComplete == aStatus) {
    // If the child didn't finish layout then it means that it used
    // up all of our available space (or needs us to split).
    mLastContentIsComplete = PR_FALSE;
  }

  // Return our size and our result
  PRInt32 kidWidth = kidSize.width;
  if (NS_UNCONSTRAINEDSIZE!=kidSize.width) //&& NS_UNCONSTRAINEDSIZE!=aMaxSize.width)
    kidWidth += leftInset + rightInset;
  PRInt32 kidHeight = kidSize.height;

  // height can be set w/o being restricted by aMaxSize.height
  if (NS_UNCONSTRAINEDSIZE!=kidSize.height)
    kidHeight += topInset + bottomInset;
  aDesiredSize.width = kidWidth;
  aDesiredSize.height = kidHeight;
  aDesiredSize.ascent = topInset;
  aDesiredSize.descent = bottomInset;

  if (nsnull!=aMaxElementSize)
  {
    *aMaxElementSize = *pMaxElementSize;
    aMaxElementSize->height += topInset + bottomInset;
    aMaxElementSize->width += leftInset + rightInset;
  }
  
  if (gsDebug==PR_TRUE)
    printf("  nsTableCellFrame::ResizeReflow returning aDesiredSize=%d,%d\n",
           aDesiredSize.width, aDesiredSize.height);

#ifdef NS_DEBUG
  //PostReflowCheck(result);
#endif

  return NS_OK;
}

NS_METHOD nsTableCellFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                              nsReflowMetrics& aDesiredSize,
                                              const nsSize&    aMaxSize,
                                              nsReflowCommand& aReflowCommand,
                                              ReflowStatus&    aStatus)
{
  if (gsDebug == PR_TRUE) printf("nsTableCellFrame::IncrementalReflow\n");
  // total hack for now, just some hard-coded values
  ResizeReflow(aPresContext, aDesiredSize, aMaxSize, nsnull, aStatus);
  return NS_OK;
}

NS_METHOD nsTableCellFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                                  nsIFrame*       aParent,
                                                  nsIFrame*&      aContinuingFrame)
{
  nsTableCellFrame* cf = new nsTableCellFrame(mContent, mIndexInParent, aParent);
  PrepareContinuingFrame(aPresContext, aParent, cf);
  aContinuingFrame = cf;
  return NS_OK;
}


/* ----- static methods ----- */

nsresult nsTableCellFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                    nsIContent* aContent,
                                    PRInt32     aIndexInParent,
                                    nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableCellFrame(aContent, aIndexInParent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}
