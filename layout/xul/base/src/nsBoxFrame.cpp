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

#include "nsBoxFrame.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsCOMPtr.h"
#include "nsHTMLIIDs.h"
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"


//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewBoxFrame ( nsIFrame*& aNewFrame )
{
  nsBoxFrame* it = new nsBoxFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  // it->SetFlags(aFlags);
  aNewFrame = it;
  return NS_OK;
  
} // NS_NewBoxFrame

nsBoxFrame::nsBoxFrame()
{
  mHorizontal = PR_TRUE;
}


NS_IMETHODIMP
nsBoxFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  nsString value;
  mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value);
  if (value.EqualsIgnoreCase("vertical"))
    mHorizontal = PR_FALSE;
  else
    mHorizontal = PR_TRUE;

  return rv;
}


NS_IMETHODIMP
nsBoxFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsHTMLContainerFrame::AttributeChanged(aPresContext, aChild,
                                              aAttribute, aHint);

  if (NS_OK != rv) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::Paint(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{	
	nsresult result = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  return NS_OK;
}

class nsBoxDataSpring {
public:
    nscoord preferredSize;
    nscoord minSize;
    nscoord maxSize;
    float  springConstant;

    nscoord calculatedSize;
    PRBool  sizeValid;

    void init()
    {
        preferredSize = 0;
        springConstant = 0.0;
        minSize = 0;
        maxSize = NS_INTRINSICSIZE;
        calculatedSize = 0;
        sizeValid = PR_FALSE;
    }

};


NS_IMETHODIMP
nsBoxFrame::FlowChildAt(nsIFrame* childFrame, 
                     nsIPresContext& aPresContext,
                     nsHTMLReflowMetrics&     desiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     const nsSize& size)
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


      // start off with an empty size
      desiredSize.width = 0; 
      desiredSize.height = 0;

      // flow the child
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

      nsHTMLReflowState   reflowState(aPresContext, aReflowState, childFrame, size);

      reflowState.computedWidth = size.width;
      reflowState.computedHeight = size.height;

      // only subrtact margin and border if the size is not intrinsic
      if (NS_INTRINSICSIZE != reflowState.computedWidth) 
      {
        reflowState.computedWidth -= (total.left + total.right);
      }


      if (NS_INTRINSICSIZE != reflowState.computedHeight) 
      {
        reflowState.computedHeight -= (total.top + total.bottom);
      }
      
      nsIHTMLReflow*      htmlReflow;

      rv = childFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
      NS_ASSERTION(rv == NS_OK,"failed to get htmlReflow interface.");

      htmlReflow->WillReflow(aPresContext);
      htmlReflow->Reflow(aPresContext, desiredSize, reflowState, aStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

      *w = wcopy;
      *h = hcopy;

      // set the rect
      childFrame->SetRect(nsRect(0,0,desiredSize.width, desiredSize.height));

      // add the margin back in. The child should add its border automatically
      desiredSize.height += (margin.top + margin.bottom);
      desiredSize.width += (margin.left + margin.right); 
      
      return NS_OK;
}

/*
 * This method flows the given child at the given size.
 * Its a little complex because it more or less has to trick the 
 * reflow system to flowing it at the size we want.
 * this is achieved by creating a new reflow state and mucking 
 * with the parent computed size so that when the child figures
 * its computed size it ends up with the one we want. 
 *
NS_IMETHODIMP
nsBoxFrame::FlowChildAt(nsIFrame* childFrame, 
                     nsIPresContext& aPresContext,
                     nsHTMLReflowMetrics&     desiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     const nsSize& size)
{
  
      // create a parent state that will hold the size we will set
      nsHTMLReflowState parentState(aPresContext, aReflowState, this,
                                    size);

      // set the computed size to what we want
      parentState.computedWidth = size.width;
      parentState.computedHeight = size.height;

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

      
      // adjust it by the percent so when the child scales itself it 
      // will come out the right size.
      nsStyleUnit wunit;
      nsStyleUnit hunit;

      // only subrtact margin and border if the size is not intrinsic
      if (NS_INTRINSICSIZE != parentState.computedWidth) 
      {
        parentState.computedWidth -= (total.left + total.right);
        
        wunit = position->mWidth.GetUnit();
        if (wunit == eStyleUnit_Percent) {
          float percent = position->mWidth.GetPercentValue();
          parentState.computedWidth = nscoord(float(parentState.computedWidth)/percent);
        }
      }


      if (NS_INTRINSICSIZE != parentState.computedHeight) 
      {
        parentState.computedHeight -= (total.top + total.bottom);

        
        hunit =  position->mHeight.GetUnit();
        if (hunit == eStyleUnit_Percent) {
          float percent = position->mHeight.GetPercentValue();
          parentState.computedHeight = nscoord(float(parentState.computedHeight)/percent);
        }
        
      }
      

      // start off with an empty size
      desiredSize.width = 0; 
      desiredSize.height = 0;

      nsHTMLReflowState   reflowState(aPresContext, parentState, childFrame, size);



      nsIHTMLReflow*      htmlReflow;

      rv = childFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
      NS_ASSERTION(rv == NS_OK,"failed to get htmlReflow interface.");

      htmlReflow->WillReflow(aPresContext);
      htmlReflow->Reflow(aPresContext, desiredSize, reflowState, aStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

      // set the rect
      childFrame->SetRect(nsRect(0,0,desiredSize.width, desiredSize.height));

      // add the margin back in. The child should add its border automatically
      desiredSize.height += (margin.top + margin.bottom);
      desiredSize.width += (margin.left + margin.right); 
      
      return NS_OK;
}
*/

/**
 * Ok what we want to do here is get all the children, figure out
 * their flexibility, preferred, min, max sizes and then stretch or
 * shrink them to fit in the given space.
 *
 * So we will have 3 passes. 
 * 1) get each childs flex, min, max, pref 
 * 2) flow all the children stretched out to fit in the given area
 * 3) move all the children to the right locations.
 */
NS_IMETHODIMP
nsBoxFrame::Reflow(nsIPresContext&   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  // first figure out our size. Are we intrinsic? Computed?
  const nsMargin& borderPadding = aReflowState.mComputedBorderPadding;

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


  // ------ first pass build springs -------
  
  nsBoxDataSpring springs[100];
  
  // go though each child
  nsHTMLReflowMetrics desiredSize = aDesiredSize;
  
  nscoord count = 0;
  nsIFrame* childFrame = mFrames.FirstChild(); 
  while (nsnull != childFrame) 
  {  
    // create a spring
    springs[count].init();

    desiredSize.width = 0;
    desiredSize.height = 0;

    // get the preferred size
    const nsStylePosition* position;
    nsresult rv = childFrame->GetStyleData(eStyleStruct_Position,
                  (const nsStyleStruct*&) position);

    nsStyleUnit wunit = position->mWidth.GetUnit();
    nsStyleUnit hunit = position->mHeight.GetUnit();

    // if we don't know the exact width or height then
    // get the instrinsic size for our preferred.
    if (wunit != eStyleUnit_Coord || hunit != eStyleUnit_Coord)
       FlowChildAt(childFrame,aPresContext, desiredSize, aReflowState, aStatus, nsSize(NS_INTRINSICSIZE,NS_INTRINSICSIZE));

    if (wunit == eStyleUnit_Coord) 
        desiredSize.width = position->mWidth.GetCoordValue();

    if (hunit == eStyleUnit_Coord)  
        desiredSize.height = position->mHeight.GetCoordValue();

    if (NS_INTRINSICSIZE == desiredSize.width)
         desiredSize.width = 0;

    if (NS_INTRINSICSIZE == desiredSize.height)
         desiredSize.height = 0;

    if (mHorizontal)
        springs[count].preferredSize = desiredSize.width;
    else
        springs[count].preferredSize = desiredSize.height;


    const nsStyleCoord* min;
    const nsStyleCoord* max;

    if (mHorizontal)
    {
       min = &position->mMinWidth;
       max = &position->mMaxWidth;
    } else {
       min = &position->mMinHeight;
       max = &position->mMaxHeight;
    }

    if (min->GetUnit() == eStyleUnit_Coord) 
      springs[count].minSize = min->GetCoordValue();

    if (max->GetUnit() == eStyleUnit_Coord) 
      springs[count].maxSize = max->GetCoordValue();


    // get the content
    nsCOMPtr<nsIContent> content;
    childFrame->GetContent(getter_AddRefs(content));

    PRInt32 error;
    nsString value;

    /*
     float p2t;
     aPresContext.GetScaledPixelsToTwips(&p2t);
     nscoord onePixel = NSIntPixelsToTwips(1, p2t);

    // get min size
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::min, value))
    {
       PRInt32 min = value.ToInteger(&error);
       springs[count].minSize = min*onePixel;
    }

    // get max size
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::max, value))
    {
       PRInt32 max = value.ToInteger(&error);
       springs[count].maxSize = max*onePixel;
    }
    */
    
    // get the spring constant
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::flex, value))
    {
      value.Trim("%");
      float percent = value.ToFloat(&error);
      springs[count].springConstant = percent;    
    }
    
    if (mHorizontal) {
      if (wunit == eStyleUnit_Percent) 
          springs[count].springConstant = position->mWidth.GetPercentValue();
    } else {
      if (hunit == eStyleUnit_Percent)  
          springs[count].springConstant = position->mHeight.GetPercentValue();
    }

    // if its height greater than the max. Set the max to this height and set a flag
    // saying we will need to do another pass. But keep going there
    // may be another child that is bigger
    if (availableSize.height == NS_INTRINSICSIZE || desiredSize.height > availableSize.height) {
        availableSize.height = desiredSize.height;
    } 
    if (availableSize.width == NS_INTRINSICSIZE || desiredSize.width > availableSize.width) {
        availableSize.width = desiredSize.width;
    }       
  
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }

  // total number of children
  nscoord totalCount = count;
 
  // ------ second pass flow -------

  // now stretch it to fit the size we were given
  nscoord s;
  if (mHorizontal)
      s = availableSize.width;
  else
      s = availableSize.height;

  Stretch(springs, totalCount, s);

  // flow everyone. 
  PRBool changedHeight;
  int passes = 0;
  do 
  {
    changedHeight = PR_FALSE;
    count = 0;
    childFrame = mFrames.FirstChild(); 
    while (nsnull != childFrame) 
    {    
        desiredSize.width = 0; 
        desiredSize.height = 0;

        // set the size to be based on the spring
        nsSize size = availableSize;

        if (mHorizontal)
          size.width = springs[count].calculatedSize;
        else
          size.height = springs[count].calculatedSize;

        // flow child
        FlowChildAt(childFrame,aPresContext, desiredSize, aReflowState, aStatus, size);
      
        // if its height greater than the max. Set the max to this height and set a flag
        // saying we will need to do another pass. But keep going there
        // may be another child that is bigger
        if (mHorizontal) {
          if (availableSize.height == NS_INTRINSICSIZE || desiredSize.height > availableSize.height) {
              availableSize.height = desiredSize.height;
              changedHeight = PR_TRUE;
          } 

          // if we are wider than we anticipated then
          // then this child can't get smaller then the size returned
          // so set its minSize to be the desired and restretch. Then
          // just start over because the springs are all messed up 
          // anyway.
          
          if (desiredSize.width > size.width) {
              springs[count].preferredSize = desiredSize.width;
              springs[count].minSize = desiredSize.width;
              Stretch(springs, totalCount, availableSize.width);
              changedHeight = PR_TRUE;
              break;
          } 
          
 
        } else {
          if (availableSize.width == NS_INTRINSICSIZE || desiredSize.width > availableSize.width) {
              availableSize.width = desiredSize.width;
              changedHeight = PR_TRUE;
          } 

          
          if (desiredSize.height > size.height) {
              springs[count].preferredSize = desiredSize.height;
              springs[count].minSize = desiredSize.height;
              Stretch(springs, totalCount, availableSize.height);
              changedHeight = PR_TRUE;
              break;
          } 
          
        }

      nsresult rv = childFrame->GetNextSibling(&childFrame);
      NS_ASSERTION(rv == NS_OK,"failed to get next child");
      count++;
    }

    // if we get over 10 passes something probably when wrong.
    passes++;
    if (passes > 5)
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("bug"));

    NS_ASSERTION(passes <= 10,"Error infinte loop too many passes");

  } while (PR_TRUE == changedHeight);
  
  // ---- 3th pass. Layout ---
  nscoord x = borderPadding.left;
  nscoord y = borderPadding.top;
  nscoord width = 0;
  nscoord height = 0;

  childFrame = mFrames.FirstChild(); 
  while (nsnull != childFrame) 
  {
    // add in the left margin
    const nsStyleSpacing* spacing;
    nsresult rv = childFrame->GetStyleData(eStyleStruct_Spacing,
                 (const nsStyleStruct*&) spacing);
    nsMargin margin;
    spacing->GetMargin(margin);

    if (mHorizontal) {
      x += margin.left;
      y = borderPadding.top + margin.top;
    } else {
      y += margin.top;
      x = borderPadding.left + margin.left;
    }

    nsRect rect;
    childFrame->GetRect(rect);
    rect.x = x;
    rect.y = y;
    childFrame->SetRect(rect);

    // add in the right margin
    if (mHorizontal)
      x += margin.right;
    else
      y += margin.bottom;
     
    if (mHorizontal) {
      x += rect.width;
      width += rect.width + margin.left + margin.right;
    } else {
      y += rect.height;
      height += rect.height + margin.top + margin.bottom;
    }

    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
  }

  
  if (mHorizontal) {
      height = availableSize.height;
  } else {
      width = availableSize.width;
  }
   // if the width is set
  if (fixedWidthContent) 
	    aDesiredSize.width = aReflowState.computedWidth;
  else
      aDesiredSize.width = width;

  // if the height is set
  if (fixedHeightContent) 
	    aDesiredSize.height = aReflowState.computedHeight; 
  else
      aDesiredSize.height = height;


  if (width > aDesiredSize.width)
      aDesiredSize.width = width;

  if (height > aDesiredSize.height)
      aDesiredSize.height = height;


  aDesiredSize.width += borderPadding.left + borderPadding.right;
  aDesiredSize.height += borderPadding.top + borderPadding.bottom;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
 
  aStatus = NS_FRAME_COMPLETE;
  
  return NS_OK;
}



void
nsBoxFrame::Stretch(nsBoxDataSpring* springs, PRInt32 nSprings, nscoord& size)
{
 
      PRInt32 sizeRemaining = size;
      float springConstantsRemaining = (float)0.0;
      int i;

      for (i=0; i<nSprings; i++) {
          nsBoxDataSpring& spring = springs[i];
          spring.sizeValid = PR_FALSE;
          if (spring.springConstant == 0.0)
          {
              spring.springConstant = (float)0.001;
              spring.maxSize = spring.preferredSize;
              spring.minSize = spring.preferredSize;
          }
          springConstantsRemaining += spring.springConstant;
          sizeRemaining -= spring.preferredSize;
      }

      if (NS_INTRINSICSIZE == size) {
          size = 0;
          for (i=0; i<nSprings; i++) {
              nsBoxDataSpring& spring=springs[i];
              spring.calculatedSize = spring.preferredSize;
              spring.sizeValid = PR_TRUE;
              size += spring.calculatedSize;
          }
          return;
      }

      PRBool limit = PR_TRUE;
      for (int pass=1; PR_TRUE == limit; pass++) {
          limit = PR_FALSE;
          for (i=0; i<nSprings; i++) {
              nsBoxDataSpring& spring=springs[i];
              if (spring.sizeValid==PR_FALSE) {
                  PRInt32 newSize = spring.preferredSize + NSToIntRound(sizeRemaining*(spring.springConstant/springConstantsRemaining));
                  if (newSize<=spring.minSize) {
                      spring.calculatedSize = spring.minSize;
                      springConstantsRemaining -= spring.springConstant;
                      //sizeRemaining -= spring.minSize-spring.preferredSize;
                      sizeRemaining += spring.preferredSize;
                      sizeRemaining -= spring.calculatedSize;
 
                      spring.sizeValid = PR_TRUE;
                      limit = PR_TRUE;
                  }
                  else if (newSize>=spring.maxSize) {
                      spring.calculatedSize = spring.maxSize;
                      springConstantsRemaining -= spring.springConstant;
                      //sizeRemaining -= spring.minSize-spring.preferredSize;
                      sizeRemaining += spring.preferredSize;
                      sizeRemaining -= spring.calculatedSize;
                      spring.sizeValid = PR_TRUE;
                      limit = PR_TRUE;
                  }
              }
          }
      }
      float stretchFactor = sizeRemaining/springConstantsRemaining;
      size = 0;
      for (i=0; i<nSprings; i++) {
           nsBoxDataSpring& spring=springs[i];
          if (spring.sizeValid==PR_FALSE) {
              spring.calculatedSize = spring.preferredSize + NSToIntFloor(spring.springConstant*stretchFactor);
              spring.sizeValid = PR_TRUE;
          }
          size += spring.calculatedSize;
      }
}

    
void
nsBoxFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{

}


NS_IMETHODIMP
nsBoxFrame::HandleEvent(nsIPresContext& aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus& aEventStatus)
{
  nsresult rv = nsHTMLContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  return rv;
}

//
// ReResolveStyleContext
//
// When the style context changes, make sure that all of our styles are still up to date.
//
NS_IMETHODIMP
nsBoxFrame :: ReResolveStyleContext ( nsIPresContext* aPresContext, nsIStyleContext* aParentContext,
                                                PRInt32 aParentChange, nsStyleChangeList* aChangeList, 
                                                PRInt32* aLocalChange)
{

  nsCOMPtr<nsIStyleContext> old ( dont_QueryInterface(mStyleContext) );
  
  // this re-resolves |mStyleContext|, so it may change
  nsresult rv = nsFrame::ReResolveStyleContext(aPresContext, aParentContext, aParentChange, aChangeList, aLocalChange); 
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;  
} // ReResolveStyleContext


