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
#include "nsTableColGroupFrame.h"
#include "nsReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIPtr.h"
#include "nsIContentDelegate.h"
#include "nsTablePart.h"
#include "nsTableContent.h"
#include "nsHTMLAtoms.h"

NS_DEF_PTR(nsIContent);
NS_DEF_PTR(nsIStyleContext);

static PRBool gsDebug = PR_FALSE;


nsTableColGroupFrame::nsTableColGroupFrame(nsIContent* aContent,
                     nsIFrame*   aParentFrame)
  : nsContainerFrame(aContent, aParentFrame)
{
}

nsTableColGroupFrame::~nsTableColGroupFrame()
{
}

NS_METHOD nsTableColGroupFrame::Paint(nsIPresContext& aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect&        aDirtyRect)
{
  if (gsDebug==PR_TRUE) printf("nsTableColGroupFrame::Paint\n");
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
}

// TODO:  content changed notifications
NS_METHOD nsTableColGroupFrame::Reflow(nsIPresContext*      aPresContext,
                                       nsReflowMetrics&     aDesiredSize,
                                       const nsReflowState& aReflowState,
                                       nsReflowStatus&      aStatus)
{
  NS_ASSERTION(nsnull!=aPresContext, "bad arg");
  NS_ASSERTION(nsnull!=mContent, "bad state -- null content for frame");

  if (nsnull == mFirstChild) 
  { // if we don't have any children, create them
    nsIFrame* kidFrame = nsnull;
    nsIFrame* prevKidFrame;
   
    LastChild(prevKidFrame);  // XXX remember this...
    PRInt32 kidIndex = 0;
    for (;;)
    {
      nsIContentPtr kid = mContent->ChildAt(kidIndex);   // kid: REFCNT++
      if (kid.IsNull()) {
        break;
      }

      // Resolve style
      nsIStyleContextPtr kidSC =
        aPresContext->ResolveStyleContextFor(kid, this);
      nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
        kidSC->GetData(eStyleStruct_Spacing);

      // Create a child frame
      nsIContentDelegate* kidDel = nsnull;
      kidDel = kid->GetDelegate(aPresContext);
      nsresult rv = kidDel->CreateFrame(aPresContext, kid, this, kidSC,
                                        kidFrame);
      NS_RELEASE(kidDel);

      // give the child frame a chance to reflow, even though we know it'll have 0 size
      nsReflowMetrics kidSize(nsnull);
      nsReflowState   kidReflowState(kidFrame, aReflowState, nsSize(0,0), eReflowReason_Initial);
      kidFrame->WillReflow(*aPresContext);
      nsReflowStatus status = ReflowChild(kidFrame,aPresContext, kidSize,
                                          kidReflowState);
      // note that DidReflow is called as the result of some ancestor firing off a DidReflow above me
      kidFrame->SetRect(nsRect(0,0,0,0));

      // Link child frame into the list of children
      if (nsnull != prevKidFrame) {
        prevKidFrame->SetNextSibling(kidFrame);
      } else {
        mFirstChild = kidFrame;  // our first child
        SetFirstContentOffset(kidFrame);
      }
      prevKidFrame = kidFrame;
      mChildCount++;
      kidIndex++;
    }
    // now that I have all my COL children, adjust their style
    SetStyleContextForFirstPass(aPresContext);
  }
  aDesiredSize.width=0;
  aDesiredSize.height=0;
  if (nsnull!=aDesiredSize.maxElementSize)
  {
    aDesiredSize.maxElementSize->width=0;
    aDesiredSize.maxElementSize->height=0;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

// Subclass hook for style post processing
NS_METHOD nsTableColGroupFrame::SetStyleContextForFirstPass(nsIPresContext* aPresContext)
{
  nsTablePart* table = ((nsTableContent*)mContent)->GetTable();
  NS_ASSERTION(table,"Table Must not be null");
  if (!table)
    return NS_OK;  // TODO:  error!

  // check for the COLS attribute
  nsContentAttr result;
  nsHTMLValue value;
  result = table->GetAttribute(nsHTMLAtoms::cols, value);
  
  // if COLS is set, then map it into the COL frames
  // chicken and egg problem here.  I don't have any children yet, so I 
  // don't know how many columns there are, so I can't do this!
  // I need to just set some state, and let the individual columns query up into me
  // in their own hooks
  PRInt32 numCols;
  if (result == eContentAttr_NoValue)
    ChildCount(numCols);
  else if (result == eContentAttr_HasValue)
  {
    nsHTMLUnit unit = value.GetUnit();
    if (eHTMLUnit_Empty==unit)
      ChildCount(numCols);
    else
      numCols = value.GetIntValue();
  }

  PRInt32 colIndex=0;
  nsIFrame *colFrame;
  FirstChild(colFrame);
  for (; colIndex<numCols; colIndex++)
  {
    if (nsnull==colFrame)
      break;  // the attribute value specified was greater than the actual number of columns
    nsStylePosition * colPosition=nsnull;
    colFrame->GetStyleData(eStyleStruct_Position, (nsStyleStruct*&)colPosition);
    nsStyleCoord width (1, eStyleUnit_Proportional);
    colPosition->mWidth = width;
    colFrame->GetNextSibling(colFrame);
  }
  // if there are more columns, there width is set to "minimum"
  PRInt32 numChildFrames;
  ChildCount(numChildFrames);
  for (; colIndex<numChildFrames; colIndex++)
  {
    nsStylePosition * colPosition=nsnull;
    colFrame->GetStyleData(eStyleStruct_Position, (nsStyleStruct*&)colPosition);
    nsStyleCoord width (0, eStyleUnit_Integer);
    colPosition->mWidth = width;
    colFrame->GetNextSibling(colFrame);
  }

  mStyleContext->RecalcAutomaticData(aPresContext);
  return NS_OK;
}

nsresult nsTableColGroupFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                        nsIContent* aContent,
                                        nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableColGroupFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}
