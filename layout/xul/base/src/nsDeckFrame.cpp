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
 * Contributor(s): 
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsDeckFrame.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsHTMLIIDs.h"
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIReflowCommand.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsStyleChangeList.h"
#include "nsCSSRendering.h"
#include "nsIViewManager.h"


nsresult
NS_NewDeckFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsDeckFrame* it = new (aPresShell) nsDeckFrame(aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewDeckFrame


nsDeckFrame::nsDeckFrame(nsIPresShell* aPresShell):nsStackFrame(aPresShell)
{
}

NS_IMETHODIMP
nsDeckFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsStackFrame::AttributeChanged(aPresContext, aChild,
                                              aNameSpaceID, aAttribute, aHint);


   // if the index changed hide the old element and make the now element visible
  if (aAttribute == nsHTMLAtoms::index) {

    Invalidate(aPresContext, nsRect(0,0,mRect.width,mRect.height), PR_FALSE);

     int index = 0;

      // get the index attribute
      nsAutoString value;
      if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::index, value))
      {
        PRInt32 error;

        // convert it to an integer
        index = value.ToInteger(&error);
      }

      nsIFrame* childFrame = mFrames.FirstChild(); 
      nscoord count = 0;
      while (nsnull != childFrame) 
      {
        // make collapsed children not show up
        if (index != count) 
           CollapseChild(aPresContext, childFrame, PR_TRUE);
        else
           CollapseChild(aPresContext, childFrame, PR_FALSE);

        rv = childFrame->GetNextSibling(&childFrame);
        NS_ASSERTION(rv == NS_OK,"failed to get next child");
        count++;
      }

  }


  

  if (NS_OK != rv) {
    return rv;
  }

  return NS_OK;
}

nsIFrame* 
nsDeckFrame::GetSelectedFrame()
{
 // ok we want to paint only the child that as at the given index

  // default index is 0
  int index = 0;

  // get the index attribute
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::index, value))
  {
    PRInt32 error;

    // convert it to an integer
    index = value.ToInteger(&error);
  }

  // get the child at that index. 
  nsIFrame* childFrame = mFrames.FrameAt(index); 
  return childFrame;
}


NS_IMETHODIMP
nsDeckFrame::Paint(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{
  // if a tab is hidden all its children are too.
 	const nsStyleDisplay* disp = (const nsStyleDisplay*)
	mStyleContext->GetStyleData(eStyleStruct_Display);
	if (!disp->IsVisibleOrCollapsed())
		return NS_OK;

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    if (disp->IsVisibleOrCollapsed() && mRect.width && mRect.height) {
      // Paint our background and border
      PRIntn skipSides = GetSkipSides();
      const nsStyleColor* color = (const nsStyleColor*)
        mStyleContext->GetStyleData(eStyleStruct_Color);
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        mStyleContext->GetStyleData(eStyleStruct_Spacing);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *color, *spacing, 0, 0);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *spacing, mStyleContext, skipSides);
    }
  }

  nsIFrame* frame = GetSelectedFrame();

  if (frame != nsnull)
    PaintChild(aPresContext, aRenderingContext, aDirtyRect, frame, aWhichLayer);

  return NS_OK;

}


NS_IMETHODIMP  nsDeckFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                             const nsPoint& aPoint, 
                                             nsIFrame**     aFrame)
{
  // if its not in our child just return us.
  *aFrame = this;

  // get the selected frame and see if the point is in it.
  nsIFrame* selectedFrame = GetSelectedFrame();

  if (nsnull != selectedFrame)
  {
    nsRect childRect;
    selectedFrame->GetRect(childRect);

    if (childRect.Contains(aPoint)) {
      // adjust the point
      nsPoint p = aPoint;
      p.x -= childRect.x;
      p.y -= childRect.y;
      return selectedFrame->GetFrameForPoint(aPresContext, p, aFrame);
    }
  }
    
  return NS_OK;
}

NS_IMETHODIMP
nsDeckFrame::DidReflow(nsIPresContext* aPresContext,
                      nsDidReflowStatus aStatus)
{
  int index = 0;

  // get the index attribute
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::index, value))
  {
    PRInt32 error;

    // convert it to an integer
    index = value.ToInteger(&error);
  }

  nsresult rv = nsBoxFrame::DidReflow(aPresContext, aStatus);
  NS_ASSERTION(rv == NS_OK,"DidReflow failed");

  nsIFrame* childFrame = mFrames.FirstChild(); 
  nscoord count = 0;
  while (nsnull != childFrame) 
  {
    // make collapsed children not show up
    if (index != count) 
       CollapseChild(aPresContext, childFrame, PR_TRUE);
    else
       CollapseChild(aPresContext, childFrame, PR_FALSE);

    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }

  return rv;
}

