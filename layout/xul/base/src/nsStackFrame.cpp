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

#include "nsStackFrame.h"
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
NS_NewStackFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsStackFrame* it = new (aPresShell) nsStackFrame(aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewDeckFrame

nsStackFrame::nsStackFrame(nsIPresShell* aPresShell):nsBoxFrame(aPresShell)
{
}


NS_IMETHODIMP
nsStackFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
   // Get the element's tag
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  return rv;
}

void
nsStackFrame::AddChildSize(nsBoxInfo& aInfo, nsBoxInfo& aChildInfo)
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


void
nsStackFrame::ComputeChildsNextPosition(nsCalculatedBoxInfo* aInfo, nscoord& aCurX, nscoord& aCurY, nscoord& aNextX, nscoord& aNextY, const nsSize& aCurrentChildSize, const nsRect& aBoxRect, nscoord aMaxAscent)
{
   // let everything layout on top of each other.
    aCurX = aNextX = aBoxRect.x;
    aCurY = aNextY = aBoxRect.y;
}

void
nsStackFrame::ChildResized(nsIFrame* aFrame, nsHTMLReflowMetrics& aDesiredSize, nsRect& aRect, nscoord& aMaxAscent, nsCalculatedBoxInfo& aInfo, PRBool*& aResized, PRInt32 aInfoCount, nscoord& aChangedIndex, PRBool& aFinished, nscoord aIndex, nsString& aReason)
{
  if (aDesiredSize.width > aRect.width) {
    aRect.width = aDesiredSize.width;
    InvalidateChildren();
    LayoutChildrenInRect(aRect, aMaxAscent);
    aReason = "child's width got bigger";
    aChangedIndex = aIndex;
    aFinished = PR_FALSE;
  } else if (aDesiredSize.height > aRect.height) {
    aRect.height = aDesiredSize.height;
    InvalidateChildren();
    LayoutChildrenInRect(aRect, aMaxAscent);
    aReason = "child's height got bigger";
    aChangedIndex = aIndex;
    aFinished = PR_FALSE;    
  }
}

void
nsStackFrame::LayoutChildrenInRect(nsRect& aGivenSize, nscoord& aMaxAscent)
{
  nsInfoList* list = GetInfoList();
  nsCalculatedBoxInfo* info = list->GetFirst();

  while(info) {
      info->calculatedSize.width = aGivenSize.width;
      info->calculatedSize.height = aGivenSize.height;
      info->mFlags |= NS_FRAME_BOX_SIZE_VALID;
      info = info->next;
  }

  aMaxAscent = 0;
}

NS_IMETHODIMP  
nsStackFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                             const nsPoint& aPoint, 
                             nsFramePaintLayer aWhichLayer,
                             nsIFrame**     aFrame)
{   
  nsRect r(mRect);

  // if it is not inside us or not in the layer in which we paint, fail
  if ((aWhichLayer != NS_FRAME_PAINT_LAYER_BACKGROUND) ||
      (!r.Contains(aPoint))) {
      return NS_ERROR_FAILURE;
  }

  // is it inside our border, padding, and debugborder or insets?
  nsMargin im(0,0,0,0);
  GetInset(im);
  nsMargin border(0,0,0,0);
  const nsStyleSpacing* spacing = (const nsStyleSpacing*)
  mStyleContext->GetStyleData(eStyleStruct_Spacing);
  spacing->GetBorderPadding(border);
  r.Deflate(im);
  r.Deflate(border);    

  // no? Then it must be in our border so return us.
  if (!r.Contains(aPoint)) {
      *aFrame = this;
      return NS_OK;
  }


  nsIFrame* first = mFrames.FirstChild();

 

  // look at the children in reverse order
  nsresult rv;
      
  if (first) {
      nsPoint tmp;
      tmp.MoveTo(aPoint.x - mRect.x, aPoint.y - mRect.y);
      rv = GetStackedFrameForPoint(aPresContext, first, nsRect(0,0,mRect.width, mRect.height), tmp, aFrame);
  } else
      rv = NS_ERROR_FAILURE;

  if (!NS_SUCCEEDED(rv)) {
        const nsStyleColor* color =
    (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

      PRBool        transparentBG = NS_STYLE_BG_COLOR_TRANSPARENT ==
                                    (color->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT);

      PRBool backgroundImage = (color->mBackgroundImage.Length() > 0);

      if (!transparentBG || backgroundImage)
      {
          *aFrame = this;
          rv = NS_OK;
      }
  }

  #ifdef NS_DEBUG
                            printf("\n------------");

      if (*aFrame)
      nsFrame::ListTag(stdout, *aFrame);
                            printf("--------------\n");
  #endif

  return rv;

}


nsresult
nsStackFrame::GetStackedFrameForPoint(nsIPresContext* aPresContext,
                                      nsIFrame* aChild,
                                      const nsRect& aRect,
                                      const nsPoint& aPoint, 
                                      nsIFrame**     aFrame)
{
    // look at all the children is reverse order. Use the stack to do 
    // this.
    nsIFrame* next;
    nsresult rv;
    aChild->GetNextSibling(&next);   
    if (next != nsnull) {
       rv = GetStackedFrameForPoint(aPresContext, next, aRect, aPoint, aFrame);
       if (NS_SUCCEEDED(rv) && *aFrame)  
           return rv;
    }

    rv = aChild->GetFrameForPoint(aPresContext, aPoint, NS_FRAME_PAINT_LAYER_FOREGROUND, aFrame);
    if (NS_SUCCEEDED(rv) && *aFrame)  
        return rv;
    return aChild->GetFrameForPoint(aPresContext, aPoint, NS_FRAME_PAINT_LAYER_BACKGROUND, aFrame);
}

void
nsStackFrame::PaintChildren(nsIPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect&        aDirtyRect,
                                nsFramePaintLayer    aWhichLayer)
{
  // we need to make sure we paint background then foreground of each child because they
  // are stacked. Otherwise the foreground of the first child could be on the top of the
  // background of the second.
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND)
  {
      nsBoxFrame::PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  }
}

// Paint one child frame
void
nsStackFrame::PaintChild(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsIFrame*            aFrame,
                             nsFramePaintLayer    aWhichLayer)
{
  // we need to make sure we paint background then foreground of each child because they
  // are stacked. Otherwise the foreground of the first child could be on the top of the
  // background of the second.
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND)
  {
    nsBoxFrame::PaintChild(aPresContext, aRenderingContext, aDirtyRect, aFrame, NS_FRAME_PAINT_LAYER_BACKGROUND);
    nsBoxFrame::PaintChild(aPresContext, aRenderingContext, aDirtyRect, aFrame, NS_FRAME_PAINT_LAYER_FOREGROUND);
  } 
}

