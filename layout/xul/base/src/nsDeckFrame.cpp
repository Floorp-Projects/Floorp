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
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIReflowCommand.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsStyleChangeList.h"
#include "nsCSSRendering.h"

/*
void
ApplyRenderingChangeToTree(nsIPresContext* aPresContext,
                           nsIFrame* aFrame);
*/

nsresult
NS_NewDeckFrame ( nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsDeckFrame* it = new nsDeckFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
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
   // Get the element's tag
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

    /*
      nsCOMPtr<nsIAtom> show ( getter_AddRefs(NS_NewAtom(":-moz-deck-showing")) );
      nsCOMPtr<nsIAtom> hide ( getter_AddRefs(NS_NewAtom(":-moz-deck-hidden")) );

      if (nsnull != mSelected) 
        ForceResolveToPseudoElement(*aPresContext,mSelected, hide);
      */

      /*
         // reflow
      nsCOMPtr<nsIPresShell> shell;
      aPresContext->GetShell(getter_AddRefs(shell));
    
      nsCOMPtr<nsIReflowCommand> reflowCmd;
      nsresult rv = NS_NewHTMLReflowCommand(getter_AddRefs(reflowCmd), this,
                                            nsIReflowCommand::StyleChanged);
      if (NS_SUCCEEDED(rv)) 
        shell->AppendReflowCommand(reflowCmd);
        */

      /*
      if (nsnull != frame)
      {
         mSelected = frame;
         ForceResolveToPseudoElement(*aPresContext,mSelected, show);
      }
*/
   // ApplyRenderingChangeToTree(aPresContext, this);

  /*
	  nsRect rect(0, 0, mRect.width, mRect.height);
    Invalidate(rect, PR_TRUE);
    */
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
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, value))
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
nsDeckFrame::Paint(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{
  // if a tab is hidden all its children are too.
 	const nsStyleDisplay* disp = (const nsStyleDisplay*)
	mStyleContext->GetStyleData(eStyleStruct_Display);
	if (!disp->mVisible)
		return NS_OK;

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    const nsStyleDisplay* disp = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);
    if (disp->mVisible && mRect.width && mRect.height) {
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


NS_IMETHODIMP
nsDeckFrame::Reflow(nsIPresContext&   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{

  // if there is incremental we need to tell all nsIBoxes below to blow away the
  // cached values for the children in the reflow list
  nsIFrame* incrementalChild = nsnull;
  if ( aReflowState.reason == eReflowReason_Incremental ) {
    Dirty(aReflowState,incrementalChild);
  } 

  // get our available size
  nsSize availableSize(aReflowState.mComputedWidth,aReflowState.mComputedHeight);

  // if the width or height are intrinsic then lay us our children out at our preferred size
  if (aReflowState.mComputedWidth == NS_INTRINSICSIZE || aReflowState.mComputedHeight == NS_INTRINSICSIZE)
  {
    // get our size
    nsBoxInfo ourSize;
    GetBoxInfo(aPresContext, aReflowState, ourSize);

    if (aReflowState.mComputedWidth == NS_INTRINSICSIZE)
       availableSize.width = ourSize.prefSize.width;
  
    if (aReflowState.mComputedHeight == NS_INTRINSICSIZE)
       availableSize.height = ourSize.prefSize.height;
  }
  
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;

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

      FlowChildAt(childFrame, aPresContext, aDesiredSize, aReflowState, aStatus, availableSize, incrementalChild);

      // if the area returned is greater than our size
      if (aDesiredSize.height > availableSize.height || aDesiredSize.width > availableSize.width)
      {
         // note the child that got bigger
         changedChild = childFrame;

         // set our size to be the new size
         if (aDesiredSize.width > availableSize.width)
             availableSize.width = aDesiredSize.width;

         if (aDesiredSize.height > availableSize.height)
            availableSize.height = aDesiredSize.height;

         // indicate we need to start another pass
         finished = PR_FALSE;
      }

      // get the next child
      nsresult rv = childFrame->GetNextSibling(&childFrame);
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

nsresult
nsDeckFrame::FlowChildAt(nsIFrame* childFrame, 
                     nsIPresContext& aPresContext,
                     nsHTMLReflowMetrics&     desiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     const nsSize& size,
                     nsIFrame*& incrementalChild)
{

      // subtract out the childs margin and border 
      const nsStyleSpacing* spacing;
      nsresult rv = childFrame->GetStyleData(eStyleStruct_Spacing,
                     (const nsStyleStruct*&) spacing);

      nsMargin margin;
      spacing->GetMargin(margin);
      nsMargin border;
      spacing->GetBorderPadding(border);
      nsMargin total = margin + border;

      const nsStylePosition* position;
      rv = childFrame->GetStyleData(eStyleStruct_Position,
                     (const nsStyleStruct*&) position);

      nsReflowReason reason = aReflowState.reason;
      PRBool shouldReflow = PR_TRUE;

      // so if we are incremental and have already reflowed the incremental child or there is an incremental child
      // and its not this one make sure we change the reason to resize.
      if (reason == eReflowReason_Incremental && (nsnull == incrementalChild || incrementalChild != childFrame)) {
          reason = eReflowReason_Resize;
          nsRect currentSize;
          childFrame->GetRect(currentSize);

          if (currentSize.width > 0 && currentSize.height > 0)
          {
            desiredSize.width = currentSize.width;
            desiredSize.height = currentSize.height;

            if (currentSize.width == size.width && currentSize.height == size.height)
                  shouldReflow = PR_FALSE;
          }
      }      

      // ok now reflow the child into the springs calculated space
      if (shouldReflow) {

        desiredSize.width = 0;
        desiredSize.height = 0;

        // create a reflow state to tell our child to flow at the given size.
        nsHTMLReflowState   reflowState(aPresContext, aReflowState, childFrame, nsSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE));
        reflowState.reason = reason;

        reflowState.mComputedWidth = size.width;
        reflowState.mComputedHeight = size.height;

        // only subrtact margin and border.
        reflowState.mComputedWidth -= (total.left + total.right);
        reflowState.mComputedHeight -= (total.top + total.bottom);

        nsSize maxElementSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
        
        // do the flow
        nsIHTMLReflow*      htmlReflow;

        rv = childFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
        NS_ASSERTION(rv == NS_OK,"failed to get htmlReflow interface.");

        htmlReflow->WillReflow(aPresContext);
        htmlReflow->Reflow(aPresContext, desiredSize, reflowState, aStatus);
        NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

        // set the rect
        childFrame->SetRect(nsRect(aReflowState.mComputedBorderPadding.left,aReflowState.mComputedBorderPadding.top,desiredSize.width, desiredSize.height));
      }

      // add the margin back in. The child should add its border automatically
      desiredSize.height += (margin.top + margin.bottom);
      desiredSize.width += (margin.left + margin.right);
    
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

/*
NS_IMETHODIMP
nsDeckFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  nsresult r = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  nsIFrame* frame = GetSelectedFrame();

  nsIFrame* childFrame = mFrames.FirstChild(); 
  while (nsnull != childFrame) 
  {  
      // if we hit the child that cause us to do a second pass
      // then break.
      if (childFrame != frame)
      {
        mSelected = frame;
        AddStyle(mSelected, gVisibleStyle);
        mSelected->ReResolveStyleContext(&aPresContext, mStyleContext, 
                                         NS_STYLE_HINT_REFLOW,
                                         nsnull, nsnull);
      } else {
        RemoveStyle(mSelected, gHiddenStyle);
        mSelected->ReResolveStyleContext(&aPresContext, mStyleContext, 
                                         NS_STYLE_HINT_REFLOW,
                                         nsnull, nsnull);

      }
  }

  return r;
}
*/


NS_IMETHODIMP
nsDeckFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  nsresult r = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // now that all the children are added. ReResolve our children
  // so we hide everything that is hidden in the deck
  ReResolveStyleContext(&aPresContext, mStyleContext, 
                        NS_STYLE_HINT_REFLOW,
                        nsnull, nsnull);
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

/**
 * Goes though each child asking for its size to determine our size. Returns our deck size minus our border.
 * This method is defined in nsIBox interface.
 */
NS_IMETHODIMP
nsDeckFrame::GetBoxInfo(nsIPresContext& aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
{
   nsresult rv;

   aSize.clear();

   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the deck
   nscoord count = 0;
   nsIFrame* childFrame = mFrames.FirstChild(); 

   while (nsnull != childFrame) 
   {  
      nsBoxInfo info;
      // get the size of the child. This is the min, max, preferred, and spring constant
      // it does not include its border.
      rv = GetChildBoxInfo(aPresContext, aReflowState, childFrame, info);
      NS_ASSERTION(rv == NS_OK,"failed to get child box info");
      if (NS_FAILED(rv))
         return rv;

      // add in the child's margin and border/padding if there is one.
      const nsStyleSpacing* spacing;
      nsresult rv = childFrame->GetStyleData(eStyleStruct_Spacing,
                    (const nsStyleStruct*&) spacing);

      NS_ASSERTION(rv == NS_OK,"failed to get spacing info");
      if (NS_FAILED(rv))
         return rv;

      nsMargin margin;
      spacing->GetMargin(margin);
      nsSize m(margin.left+margin.right,margin.top+margin.bottom);
      info.minSize += m;
      info.prefSize += m;

      spacing->GetBorderPadding(margin);
      nsSize b(margin.left+margin.right,margin.top+margin.bottom);
      info.minSize += b;
      info.prefSize += b;

      // largest preferred size
      if (info.prefSize.width > aSize.prefSize.width)
        aSize.prefSize.width = info.prefSize.width;

      if (info.prefSize.height > aSize.prefSize.height)
        aSize.prefSize.height = info.prefSize.height;

      // largest min size
      if (info.minSize.width > aSize.minSize.width)
        aSize.minSize.width = info.minSize.width;

      if (info.minSize.height > aSize.minSize.height)
        aSize.minSize.height = info.minSize.height;

      // smallest max size
      if (info.maxSize.width < aSize.maxSize.width)
        aSize.maxSize.width = info.maxSize.width;

      if (info.maxSize.height < aSize.maxSize.height)
        aSize.maxSize.height = info.maxSize.height;

    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    if (NS_FAILED(rv))
       return rv;

    count++;
  }

  return rv;
}

nsresult
nsDeckFrame::GetChildBoxInfo(nsIPresContext& aPresContext, const nsHTMLReflowState& aReflowState, nsIFrame* aFrame, nsBoxInfo& aSize)
{
  aSize.clear();

  // see if the frame implements IBox interface
  nsCOMPtr<nsIBox> ibox = do_QueryInterface(aFrame);

  // if it does ask it for its BoxSize and we are done
  if (ibox) {
     ibox->GetBoxInfo(aPresContext, aReflowState, aSize); 
     // add in the border, padding, width, min, max
     GetRedefinedMinPrefMax(aFrame, aSize);
     return NS_OK;
  } else {
     GetRedefinedMinPrefMax(aFrame, aSize);
  }



  // set the pref width and height to be intrinsic.
  aSize.prefWidthIntrinsic = PR_TRUE;
  aSize.prefHeightIntrinsic = PR_TRUE;;

  return NS_OK;
}

/** 
 * Looks at the given frame and sees if its redefined preferred, min, or max sizes
 * if so it used those instead. Currently it gets its values from css
 */
void 
nsDeckFrame::GetRedefinedMinPrefMax(nsIFrame* aFrame, nsBoxInfo& aSize)
{
  // add in the css min, max, pref
    const nsStylePosition* position;
    nsresult rv = aFrame->GetStyleData(eStyleStruct_Position,
                  (const nsStyleStruct*&) position);

    // see if the width or height was specifically set
    if (position->mWidth.GetUnit() == eStyleUnit_Coord)  {
        aSize.prefSize.width = position->mWidth.GetCoordValue();
        aSize.prefWidthIntrinsic = PR_FALSE;
    }

    if (position->mHeight.GetUnit() == eStyleUnit_Coord) {
        aSize.prefSize.height = position->mHeight.GetCoordValue();     
        aSize.prefHeightIntrinsic = PR_FALSE;
    }
    
    // same for min size. Unfortunately min size is always set to 0. So for now
    // we will assume 0 means not set.
    if (position->mMinWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinWidth.GetCoordValue();
        if (min != 0)
           aSize.minSize.width = min;
    }

    if (position->mMinHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinHeight.GetCoordValue();
        if (min != 0)
           aSize.minSize.height = min;
    }

    // and max
    if (position->mMaxWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxWidth.GetCoordValue();
        aSize.maxSize.width = max;
    }

    if (position->mMaxHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxHeight.GetCoordValue();
        aSize.maxSize.height = max;
    }
}

/**
 * Called with a reflow command. This will dirty all boxes who need to be reflowed.
 * return the last child that is not a box. Part of nsIBox interface.
 */
NS_IMETHODIMP
nsDeckFrame::Dirty(const nsHTMLReflowState& aReflowState, nsIFrame*& incrementalChild)
{
  incrementalChild = nsnull;
  nsresult rv = NS_OK;

  // Dirty any children that need it.
  nsIFrame* frame;
  aReflowState.reflowCommand->GetNext(frame);
  nscoord count = 0;
  nsIFrame* childFrame = mFrames.FirstChild(); 
  while (nsnull != childFrame) 
  {
    if (childFrame == frame) {
        // clear the spring so it is recalculated on the flow
        nsCOMPtr<nsIBox> ibox = do_QueryInterface(childFrame);
        if (ibox) 
            ibox->Dirty(aReflowState, incrementalChild);
        else
            incrementalChild = frame;
        break;
    }

    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    if (NS_FAILED(rv))
      return rv;

    count++;
  }

  return rv;
}

NS_IMETHODIMP nsDeckFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIBoxIID)) {                                         
    *aInstancePtr = (void*)(nsIBox*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }   

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);                                     
}

NS_IMETHODIMP_(nsrefcnt) 
nsDeckFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsDeckFrame::Release(void)
{
    return NS_OK;
}

NS_IMETHODIMP
nsDeckFrame :: ReResolveStyleContext ( nsIPresContext* aPresContext, nsIStyleContext* aParentContext,
                                               PRInt32 aParentChange, nsStyleChangeList* aChangeList,
                                               PRInt32* aLocalChange)
{
  // calculate our style context
  PRInt32 ourChange = aParentChange;
  nsresult result = nsFrame::ReResolveStyleContext(aPresContext, aParentContext, 
                                                   ourChange, aChangeList, &ourChange);
  if (NS_FAILED(result)) {
    return result;
  }

  // get our hidden pseudo
  nsCOMPtr<nsIAtom> hide ( getter_AddRefs(NS_NewAtom(":-moz-deck-hidden")) );

  nsIStyleContext* newSC;

  // get a style content for the hidden pseudo element
  aPresContext->ResolvePseudoStyleContextFor(mContent,
                                          hide,
                                          mStyleContext,
                                          PR_FALSE, &newSC);

  // get the index from the tab
  int index = 0;

  // get the index attribute
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, value))
  {
    PRInt32 error;

    // convert it to an integer
    index = value.ToInteger(&error);
  } 


  if (aLocalChange) {
    *aLocalChange = ourChange;
  }


  // resolve all our children that are not selected with the hidden pseudo elements
  // style context
    nsIFrame* childFrame = mFrames.FirstChild(); 
    PRInt32 count = 0;
    while (nsnull != childFrame) 
    {
      
      nsIStyleContext* oldS = nsnull;
      nsIStyleContext* newS = nsnull;

  
      if (count == index)
      {
        childFrame->GetStyleContext(&oldS);
        childFrame->ReResolveStyleContext(aPresContext, mStyleContext, 
                                              ourChange, aChangeList, &ourChange);
      } else {
        // use hidden style context
        childFrame->GetStyleContext(&oldS);
        childFrame->ReResolveStyleContext(aPresContext, newSC, 
                                              ourChange, aChangeList, &ourChange);

      }

      childFrame->GetStyleContext(&newS);

      //CaptureStyleChangeFor(childFrame, oldS, newS, 
      //      ourChange, aChangeList, &ourChange);

      if (oldS != newS)
      {
        if (aChangeList) 
            aChangeList->AppendChange(childFrame, NS_STYLE_HINT_VISUAL);
      }

      NS_RELEASE(oldS);
      NS_RELEASE(newS);

      childFrame->GetNextSibling(&childFrame); 
      count++;
    }

  return result;
  
} // ReResolveStyleContext

