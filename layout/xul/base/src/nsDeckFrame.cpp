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
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIReflowCommand.h"

nsresult
NS_NewDeckFrame ( nsIFrame*& aNewFrame )
{
  nsDeckFrame* it = new nsDeckFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  aNewFrame = it;
  return NS_OK;
  
} // NS_NewDeckFrame

/*
nsDeckFrame::nsDeckFrame()
{
}
*/


NS_IMETHODIMP
nsDeckFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  mSelectedChanged = PR_TRUE;
  return rv;
}


NS_IMETHODIMP
nsDeckFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsHTMLContainerFrame::AttributeChanged(aPresContext, aChild,
                                              aAttribute, aHint);


   // if the index changed hide the old element and make the now element visible
  if (aAttribute == nsHTMLAtoms::value) {
      if (nsnull != mSelected) {
        nsCOMPtr<nsIContent> content;
        mSelected->GetContent(getter_AddRefs(content));
         content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::style, "visibility: hidden ! important", PR_TRUE);
      }

      nsIFrame* frame = GetSelectedFrame();
      if (nsnull != frame)
      {
         mSelected = frame;
         nsCOMPtr<nsIContent> content;
         mSelected->GetContent(getter_AddRefs(content));
         content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::style, "visibility: visible ! important", PR_TRUE);
      }

	  nsRect rect(0, 0, mRect.width, mRect.height);
    Invalidate(rect, PR_TRUE);
  }

  /*
  // if the index changed hide the old element and make the now element visible
  if (aAttribute == nsHTMLAtoms::value) {
      if (nsnull != mSelected) {
        nsCOMPtr<nsIContent> content;
        mSelected->GetContent(getter_AddRefs(content));
        content->UnsetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, PR_TRUE);
      }

      nsIFrame* frame = GetSelectedFrame();
      if (nsnull != frame)
      {
         mSelected = frame;
         nsCOMPtr<nsIContent> content;
         mSelected->GetContent(getter_AddRefs(content));
         content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, "visible", PR_TRUE);
      }

	  nsRect rect(0, 0, mRect.width, mRect.height);
    Invalidate(rect, PR_TRUE);
  }
  */
  
  

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
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, value))
  {
    PRInt32 error;

    // convert it to an integer
    index = value.ToInteger(&error);
    printf("index=%d\n",index);
  } else {
    printf("no index set\n");

  }

  // get the child at that index. 
  nscoord count = 0;
  nsIFrame* childFrame = mFrames.FirstChild(); 
  while (nsnull != childFrame) 
  {  
     if (count == index)
        break;

     nsresult rv = childFrame->GetNextSibling(&childFrame);
     count++;
  }

  return childFrame;
}


NS_IMETHODIMP
nsDeckFrame::Paint(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{

   
  nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
 
  /*
  nsIFrame* selectedFrame = GetSelectedFrame();

  // draw nothing if the index is out of bounds
  if (nsnull == selectedFrame)
     return NS_OK;

  // paint the child
  selectedFrame->Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
*/

  return NS_OK;
}


NS_IMETHODIMP
nsDeckFrame::Reflow(nsIPresContext&   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{

   if (mSelectedChanged)
    {
    
      mSelectedChanged = PR_FALSE;
    }

  nsIFrame* incrementalChild = nsnull;
  if ( aReflowState.reason == eReflowReason_Incremental ) {
    nsIFrame* targetFrame;
    
    // See if it's targeted at us
    aReflowState.reflowCommand->GetTarget(targetFrame);

    if (this == targetFrame) {
       // if we are the target then we are done.
       aDesiredSize.width = mRect.width;
       aDesiredSize.height = mRect.height;
       return NS_OK;
    } else {
       aReflowState.reflowCommand->GetNext(incrementalChild);

       // ensure that the child's parent is us. If its not
       // something very bad has happened.
       nsIFrame* parent;
       incrementalChild->GetParent(&parent);
       NS_ASSERTION(this == parent,"incremental reflow error!");
    }
  } 

  aDesiredSize.width = 0;
  aDesiredSize.height = 0;

  // get our available size
  nsSize availableSize(0,0);

  PRBool fixedWidthContent = aReflowState.HaveFixedContentWidth();
  if (NS_INTRINSICSIZE == aReflowState.computedWidth) {
		fixedWidthContent = PR_FALSE;
  }

  PRBool fixedHeightContent = aReflowState.HaveFixedContentHeight();
  if (NS_INTRINSICSIZE == aReflowState.computedHeight) {
		fixedHeightContent = PR_FALSE;
  }
 
  if (fixedWidthContent) {
	    availableSize.width = aReflowState.computedWidth;
  } else
      availableSize.width = aReflowState.availableWidth;

  if (fixedHeightContent) {
	    availableSize.height = aReflowState.computedHeight;
  } else
      availableSize.height = aReflowState.availableHeight;

  // iterate though each child
  PRBool finished = PR_FALSE;
  nsIFrame* changedChild = nsnull;
  int passes = 0;

  while(!finished)
  {
    finished = PR_TRUE;
    nscoord count = 0;
    nsIFrame* childFrame = mFrames.FirstChild(); 
    while (nsnull != childFrame) 
    {  
      // if we hit the child that cause us to do a second pass
      // then break.
      if (changedChild == childFrame)
          break;

      // make sure we make the child as big as we are
      const nsStylePosition* pos;
      childFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)pos);

      nsStyleCoord* w = (nsStyleCoord*)&pos->mWidth;
      nsStyleCoord wcopy(*w);
      *w = nsStyleCoord();
      w->SetPercentValue(1);

      nsStyleCoord* h = (nsStyleCoord*)&pos->mHeight;
      nsStyleCoord hcopy(*h);
      *h = nsStyleCoord();
      h->SetPercentValue(1);

      aDesiredSize.width = 0;
      aDesiredSize.height = 0;

      // get its margin
      const nsStyleSpacing* spacing;
      nsresult rv = childFrame->GetStyleData(eStyleStruct_Spacing,
                     (const nsStyleStruct*&) spacing);

      nsMargin margin;
      spacing->GetMargin(margin);
      nsMargin border;
      spacing->GetBorderPadding(border);
      nsMargin total = margin + border;

      nsSize childSize(availableSize);

      // flow it
      nsHTMLReflowState reflowState(aPresContext, aReflowState, childFrame, childSize);

      // if we are incremental
      if (reflowState.reason == eReflowReason_Incremental) {
        // if it is this child then reset the incrementalChild
        if (incrementalChild == childFrame) {
          incrementalChild = nsnull;
        } else {
          // if not our child reflow as a resize
          reflowState.reason = eReflowReason_Resize;
        }
      }

      reflowState.computedWidth = childSize.width;
      reflowState.computedHeight = childSize.height;

      // only subrtact margin and border if the size is not intrinsic
      if (NS_INTRINSICSIZE != reflowState.computedWidth) 
        reflowState.computedWidth -= (total.left + total.right);


      if (NS_INTRINSICSIZE != reflowState.computedHeight) 
        reflowState.computedHeight -= (total.top + total.bottom);

      nsIHTMLReflow*      htmlReflow;

      rv = childFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
      NS_ASSERTION(rv == NS_OK,"failed to get htmlReflow interface.");

      htmlReflow->WillReflow(aPresContext);
      htmlReflow->Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

      *w = wcopy;
      *h = hcopy;

      // set the rect
      childFrame->SetRect(nsRect(margin.left,margin.right,aDesiredSize.width, aDesiredSize.height));

      // add the margin back in. The child should add its border automatically
      aDesiredSize.height += (margin.top + margin.bottom);
      aDesiredSize.width += (margin.left + margin.right);

      // if the area returned is greater than our size
      if (aDesiredSize.height > availableSize.height || aDesiredSize.width > availableSize.width)
      {
         // note the child that got bigger
         changedChild = childFrame;

         // set our size to be the new size
         availableSize.width = aDesiredSize.width;
         availableSize.height = aDesiredSize.height;

         // indicate we need to start another pass
         finished = PR_FALSE;
      }

      // get the next child
      rv = childFrame->GetNextSibling(&childFrame);
      count++;
    }

    // if we get over 10 passes something probably when wrong.
    passes++;
    if (passes > 5)
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("DeckFrame reflow bug"));

    NS_ASSERTION(passes <= 10,"DeckFrame: Error infinte loop too many passes");
  }

  // return the largest dimension
  aDesiredSize.width = availableSize.width;
  aDesiredSize.height = availableSize.height;

  // add in our border
  const nsMargin& borderPadding = aReflowState.mComputedBorderPadding;

  aDesiredSize.width += borderPadding.left + borderPadding.right;
  aDesiredSize.height += borderPadding.top + borderPadding.bottom;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  return NS_OK;
}

/*
NS_IMETHODIMP
nsDeckFrame::HandleEvent(nsIPresContext& aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus& aEventStatus)
{

  // send the event to the selected frame
  nsIFrame* selectedFrame = GetSelectedFrame();

  // if no selected frame we handle the event
  if (nsnull == selectedFrame)
    return nsHTMLContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  return selectedFrame->HandleEvent(aPresContext, aEvent, aEventStatus);
}
*/

NS_IMETHODIMP  nsDeckFrame::GetFrameForPoint(const nsPoint& aPoint, 
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
      return selectedFrame->GetFrameForPoint(p, aFrame);
    }
  }
    
  return NS_OK;
}

NS_IMETHODIMP
nsDeckFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  nsresult r = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  nsIFrame* frame = GetSelectedFrame();
  if (nsnull != frame)
  {
     mSelected = frame;
     nsCOMPtr<nsIContent> content;
     mSelected->GetContent(getter_AddRefs(content));
     content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::style, "visibility: visible ! important", PR_FALSE);
  }

  return r;
}




NS_IMETHODIMP
nsDeckFrame::RemoveFrame(nsIPresContext& aPresContext,
                           nsIPresShell& aPresShell,
                           nsIAtom* aListName,
                           nsIFrame* aOldFrame)
{
      // remove the child frame
      nsresult rv = nsHTMLContainerFrame::RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
      mFrames.DeleteFrame(aPresContext, aOldFrame);
      return rv;
}

NS_IMETHODIMP
nsDeckFrame::InsertFrames(nsIPresContext& aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aPrevFrame,
                            nsIFrame* aFrameList)
{
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  return nsHTMLContainerFrame::InsertFrames(aPresContext, aPresShell, aListName, aPrevFrame, aFrameList); 
}

NS_IMETHODIMP
nsDeckFrame::AppendFrames(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
   mFrames.AppendFrames(nsnull, aFrameList); 
   return nsHTMLContainerFrame::AppendFrames(aPresContext, aPresShell, aListName, aFrameList); 
}
