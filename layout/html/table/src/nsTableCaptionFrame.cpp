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
#include "nsTableCaptionFrame.h"
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
#else
static const PRBool gsDebug = PR_FALSE;
#endif

static NS_DEFINE_IID(kStyleBorderSID, NS_STYLEBORDER_SID);
static NS_DEFINE_IID(kStyleColorSID, NS_STYLECOLOR_SID);
static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);
static NS_DEFINE_IID(kStyleTextSID, NS_STYLETEXT_SID);

/**
  */
nsTableCaptionFrame::nsTableCaptionFrame(nsIContent* aContent,
                                         nsIFrame*   aParentFrame)
  : nsContainerFrame(aContent, aParentFrame),
  mMinWidth(0),
  mMaxWidth(0)
{
}

nsTableCaptionFrame::~nsTableCaptionFrame()
{
}

/** return the min legal width for this caption
  * minWidth is the minWidth of it's content plus any insets
  */
PRInt32 nsTableCaptionFrame::GetMinWidth() 
{
  return mMinWidth;
}
  
/** return the max legal width for this caption
  * maxWidth is the maxWidth of it's content when given an infinite space 
  * plus any insets
  */
PRInt32 nsTableCaptionFrame::GetMaxWidth() 
{
  return mMaxWidth;
}


void nsTableCaptionFrame::CreatePsuedoFrame(nsIPresContext* aPresContext)
{
  // Do we have a prev-in-flow?
  if (nsnull == mPrevInFlow) {
    // No, create a column pseudo frame
    nsBodyFrame::NewFrame(&mFirstChild, mContent, this);
    mChildCount = 1;

    // Resolve style and set the style context
    nsIStyleContext* styleContext =
      aPresContext->ResolveStyleContextFor(mContent, this);             // styleContext: ADDREF++
    mFirstChild->SetStyleContext(aPresContext,styleContext);
    NS_RELEASE(styleContext);                                           // styleContext: ADDREF--
  } else {
    nsTableCaptionFrame* prevFrame = (nsTableCaptionFrame *)mPrevInFlow;

    nsIFrame* prevPseudoFrame = prevFrame->mFirstChild;
    NS_ASSERTION(prevFrame->ChildIsPseudoFrame(prevPseudoFrame), "bad previous pseudo-frame");

    // Create a continuing column
    nsIStyleContext* kidSC;
    prevPseudoFrame->GetStyleContext(aPresContext, kidSC);
    prevPseudoFrame->CreateContinuingFrame(aPresContext, this, kidSC,
                                           mFirstChild);
    NS_RELEASE(kidSC);
    mChildCount = 1;
  }
}

NS_METHOD nsTableCaptionFrame::Paint(nsIPresContext& aPresContext,
                                     nsIRenderingContext& aRenderingContext,
                                     const nsRect& aDirtyRect)
{
  nsStyleBorder* myBorder =
    (nsStyleBorder*)mStyleContext->GetData(kStyleBorderSID);
  nsStyleColor* myColor =
    (nsStyleColor*)mStyleContext->GetData(kStyleColorSID);
  NS_ASSERTION(nsnull!=myColor, "bad style color");
  NS_ASSERTION(nsnull!=myBorder, "bad style border");
  if (nsnull==myBorder) return NS_OK;

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
 * Align the frame's child frame within the caption
 */
void  nsTableCaptionFrame::VerticallyAlignChild(nsIPresContext* aPresContext)
{
  
  nsStyleText* textStyle =
    (nsStyleText*)mStyleContext->GetData(kStyleTextSID);
  nsStyleSpacing* spacing =
    (nsStyleSpacing*)mStyleContext->GetData(kStyleSpacingSID);
  
  nscoord topInset = spacing->mBorderPadding.top;
  nscoord bottomInset = spacing->mBorderPadding.bottom;
  PRUint8 verticalAlignFlags = NS_STYLE_VERTICAL_ALIGN_MIDDLE;
  
  if (textStyle->mVerticalAlign.GetUnit() == eStyleUnit_Enumerated) {
    verticalAlignFlags = textStyle->mVerticalAlign.GetIntValue();
  }

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

/**
  */
NS_METHOD nsTableCaptionFrame::ResizeReflow(nsIPresContext* aPresContext,
                                            nsReflowMetrics& aDesiredSize,
                                            const nsSize&   aMaxSize,
                                            nsSize*         aMaxElementSize,
                                            nsReflowStatus& aStatus)
{
  NS_PRECONDITION(nsnull!=aPresContext, "bad arg");

#ifdef NS_DEBUG
  PreReflowCheck();
#endif

  aStatus = NS_FRAME_COMPLETE;
  if (gsDebug==PR_TRUE)
    printf("nsTableCaptionFrame::ResizeReflow %p: maxSize=%d,%d\n",
           this, aMaxSize.width, aMaxSize.height);

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
  nsSize maxKidElementSize;
  if (gsDebug==PR_TRUE)
    printf("  nsTableCaptionFrame::ResizeReflow calling ReflowChild with availSize=%d,%d\n",
           availSize.width, availSize.height);
  aStatus = ReflowChild(mFirstChild, aPresContext, kidSize, availSize, pMaxElementSize);

  if (gsDebug==PR_TRUE)
  {
    if (nsnull!=pMaxElementSize)
      printf("  nsTableCaptionFrame::ResizeReflow: child returned %s with desiredSize=%d,%d,\
             and maxElementSize=%d,%d\n",
             NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
             kidSize.width, kidSize.height,
             pMaxElementSize->width, pMaxElementSize->height);
    else
      printf("  nsTableCaptionFrame::ResizeReflow: child returned %s with desiredSize=%d,%d,\
             and maxElementSize=nsnull\n",
             NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
             kidSize.width, kidSize.height);
  }

  SetFirstContentOffset(mFirstChild);
  if (gsDebug) printf("CAPTION: set first content offset to %d\n", GetFirstContentOffset()); //@@@
  SetLastContentOffset(mFirstChild);
  if (gsDebug) printf("CAPTION: set last content offset to %d\n", GetLastContentOffset()); //@@@

  // Place the child since some of it's content fit in us.
  mFirstChild->SetRect(nsRect(leftInset + x, topInset,
                              kidSize.width, kidSize.height));
  
  
  if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
    // If the child didn't finish layout then it means that it used
    // up all of our available space (or needs us to split).
    mLastContentIsComplete = PR_FALSE;
  }

  // Return our size and our result
  PRInt32 kidWidth = kidSize.width;
  if (NS_UNCONSTRAINEDSIZE!=kidSize.width)
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
    *aMaxElementSize = *pMaxElementSize;

  // in the UNCONSTRAINED case, cache the min and max widths
  if (NS_UNCONSTRAINEDSIZE==aMaxSize.width)
  {
    mMaxWidth = kidWidth;
    mMinWidth = pMaxElementSize->width;
    if (PR_TRUE==gsDebug)
      printf("  caption frame setting min/max to %d,%d\n", mMinWidth, mMaxWidth);
  }
  
  if (gsDebug==PR_TRUE) 
  {
    if (nsnull!=aMaxElementSize)
      printf("nsTableCaptionFrame::RR returning: %s with aDesiredSize=%d,%d, aMES=%d,%d\n",
              NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
              aDesiredSize.width, aDesiredSize.height,
              aMaxElementSize->width, aMaxElementSize->height);
    else
      printf("nsTableCaptionFrame::RR returning: %s with aDesiredSize=%d,%d, aMES=NSNULL\n", 
             NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
             aDesiredSize.width, aDesiredSize.height);
  }

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif

  return NS_OK;
}

NS_METHOD nsTableCaptionFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                                 nsReflowMetrics& aDesiredSize,
                                                 const nsSize&    aMaxSize,
                                                 nsReflowCommand& aReflowCommand,
                                                 nsReflowStatus&  aStatus)
{
  if (gsDebug == PR_TRUE) printf("nsTableCaptionFrame::IncrementalReflow\n");
  // total hack for now, just some hard-coded values
  ResizeReflow(aPresContext, aDesiredSize, aMaxSize, nsnull, aStatus);
  return NS_OK;
}

NS_METHOD
nsTableCaptionFrame::CreateContinuingFrame(nsIPresContext*  aPresContext,
                                           nsIFrame*        aParent,
                                           nsIStyleContext* aStyleContext,
                                           nsIFrame*&       aContinuingFrame)
{
  if (PR_TRUE==gsDebug) printf("nsTableCaptionFrame::CreateContinuingFrame called\n");
  nsTableCaptionFrame* cf = new nsTableCaptionFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aPresContext, aParent, aStyleContext, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

/* ----- static methods ----- */

nsresult nsTableCaptionFrame::NewFrame( nsIFrame** aInstancePtrResult,
                                        nsIContent* aContent,
                                        nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableCaptionFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}
