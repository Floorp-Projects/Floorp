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
#include "nsIViewManager.h"


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

NS_IMETHODIMP
nsDeckFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
   // Get the element's tag
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  //CreateViewForFrame(aPresContext,this,aContext,PR_TRUE);
  return rv;
}


NS_IMETHODIMP
nsDeckFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                              aAttribute, aHint);


   // if the index changed hide the old element and make the now element visible
  if (aAttribute == nsHTMLAtoms::index) {
            nsCOMPtr<nsIPresShell> shell;
            aPresContext->GetShell(getter_AddRefs(shell));

            nsCOMPtr<nsIReflowCommand> reflowCmd;
            nsresult rv = NS_NewHTMLReflowCommand(getter_AddRefs(reflowCmd), this,
                                                  nsIReflowCommand::StyleChanged);
            if (NS_SUCCEEDED(rv)) 
              shell->AppendReflowCommand(reflowCmd);
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
  nsresult r = nsBoxFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  return r;
}



void
nsDeckFrame::AddChildSize(nsBoxInfo& aInfo, nsBoxInfo& aChildInfo)
{
     // largest preferred size
    if (aChildInfo.prefSize.width > aInfo.prefSize.width)
      aInfo.prefSize.width = aChildInfo.prefSize.width;

    if (aChildInfo.prefSize.height > aInfo.prefSize.height)
      aInfo.prefSize.height = aChildInfo.prefSize.height;

    // largest min size
    if (aChildInfo.minSize.width > aInfo.minSize.width)
      aInfo.minSize.width = aChildInfo.minSize.width;

    if (aChildInfo.minSize.height > aInfo.minSize.height)
      aInfo.minSize.height = aChildInfo.minSize.height;

    // smallest max size
    if (aChildInfo.maxSize.width < aInfo.maxSize.width)
      aInfo.maxSize.width = aChildInfo.maxSize.width;

    if (aChildInfo.maxSize.height < aInfo.maxSize.height)
      aInfo.maxSize.height = aChildInfo.maxSize.height;
}

nsresult
nsDeckFrame::PlaceChildren(nsIPresContext& aPresContext, nsRect& boxRect)
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

  // ------- set the childs positions ---------
  nsIFrame* childFrame = mFrames.FirstChild(); 
  nscoord count = 0;
  while (nsnull != childFrame) 
  {
    nsresult rv;

    /*
    // see if the child has a view. If it doesn't make one for it.
    nsIView* childView = nsnull;
    childFrame->GetView(&childView);
    if (childView == nsnull) {
       nsCOMPtr<nsIStyleContext> context;
       childFrame->GetStyleContext(getter_AddRefs(context));
       CreateViewForFrame(aPresContext,childFrame,context,PR_TRUE);
       childFrame->GetView(&childView);
       NS_ASSERTION(childView != nsnull, "Deck could not create a view for its child!!!");
    }
    */

    //nsCOMPtr<nsIViewManager> vm;
   // childView->GetViewManager(*getter_AddRefs(vm));
  
    // make collapsed children not show up
    if (mSprings[count].collapsed || count != index) {
       CollapseChild(childFrame);
  //  } if (count != index) {
      // if the child is not in view then make sure its view is 0 so it clips
      // out all its children.
      //vm->ResizeView(childView, 0,0);
      //childFrame->SizeTo(0,0);
      //childView->SetParent(nsnull);
    } else {
      ////nsIView* pv;
      //GetView(&pv);
      //childView->SetParent(pv);
      childFrame->MoveTo(boxRect.x, boxRect.y);
    }

    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }

  return NS_OK;
}

void
nsDeckFrame::ChildResized(nsHTMLReflowMetrics& aDesiredSize, nsRect& aRect, nsCalculatedBoxInfo& aInfo, PRBool* aResized, nscoord& aChangedIndex, PRBool& aFinished, nscoord aIndex, nsString& aReason)
{
  if (aDesiredSize.width > aRect.width) {
    aRect.width = aDesiredSize.width;
    InvalidateChildren();
    LayoutChildrenInRect(aRect);
    aReason = "child's width got bigger";
    aChangedIndex = aIndex;
    aFinished = PR_FALSE;
  } else if (aDesiredSize.height > aRect.height) {
    aRect.height = aDesiredSize.height;
    InvalidateChildren();
    LayoutChildrenInRect(aRect);
    aReason = "child's height got bigger";
    aChangedIndex = aIndex;
    aFinished = PR_FALSE;    
  }
}

void
nsDeckFrame::LayoutChildrenInRect(nsRect& size)
{
  for (int i=0; i<mSpringCount; i++) {
      mSprings[i].calculatedSize.width = size.width;
      mSprings[i].calculatedSize.height = size.height;
      mSprings[i].sizeValid = PR_TRUE;
  }
}

