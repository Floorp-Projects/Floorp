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
#include "nsIReflowCommand.h"

#define CONSTANT (float)0.001;
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
  else if (value.EqualsIgnoreCase("horizontal"))
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

NS_IMETHODIMP
nsBoxFrame::FlowChildAt(nsIFrame* childFrame, 
                     nsIPresContext& aPresContext,
                     nsHTMLReflowMetrics&     desiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     const nsSize& size,
                     nsIFrame* incrementalChild)
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

      nsReflowReason reason = aReflowState.reason;
      PRBool shouldReflow = PR_TRUE;

      if (nsnull != incrementalChild && incrementalChild != childFrame) {
              // if we are flowing because of incremental and this is not
              // the child then see if the size is already what we want.
              // if it is then keep it and don't bother reflowing.
              nsRect currentSize;
              childFrame->GetRect(currentSize);

              desiredSize.width = currentSize.width;
              desiredSize.height = currentSize.height;

              // XXX ok there needs to be a little more work here. The child
              // could move another child out of the way causing a reflow of
              // a different child. In this case we need to reflow the affected
              // area. So yes we need to reflow. Make sure we change the reason
              // so we don't lose the incremental reflow path.

              /*
              PRBool wsame = (mHorizontal && NS_INTRINSICSIZE == size.width);
              PRBool hsame = (!mHorizontal && NS_INTRINSICSIZE == size.height);

              if ((NS_INTRINSICSIZE == size.height && NS_INTRINSICSIZE == size.width) || (wsame || currentSize.width == size.width) && (hsame || currentSize.height == size.height))
                    shouldReflow = PR_FALSE;
              else 
                    reason = eReflowReason_StyleChange;
                    */
             if (currentSize.width == size.width && currentSize.height == size.height)
                    shouldReflow = PR_FALSE;
              else 
                    reason = eReflowReason_StyleChange;
 
      } 
      
      if (shouldReflow) {

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
        reflowState.reason = reason;

        reflowState.computedWidth = size.width;
        reflowState.computedHeight = size.height;

        // only subrtact margin and border if the size is not intrinsic
        if (NS_INTRINSICSIZE != reflowState.computedWidth) 
          reflowState.computedWidth -= (total.left + total.right);


        if (NS_INTRINSICSIZE != reflowState.computedHeight) 
          reflowState.computedHeight -= (total.top + total.bottom);

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
      }

   
      return NS_OK;
}

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

  nsMargin inset(0,0,0,0);
  GetInset(inset);
  
  if (NS_INTRINSICSIZE != availableSize.width)
     availableSize.width -= (inset.left + inset.right);

  if (NS_INTRINSICSIZE != availableSize.height)
     availableSize.height -= (inset.top + inset.bottom);

  // handle incremental reflow
  // get the child and only reflow it
  nsIFrame* incrementalChild = nsnull;
  if ( aReflowState.reason == eReflowReason_Incremental ) {
    aReflowState.reflowCommand->GetNext(incrementalChild);
  } 

  // ------ first pass build springs -------
  nscoord totalCount = 0;

    // go though each child
  nsHTMLReflowMetrics desiredSize = aDesiredSize;

  nsSize minSize;
  nsSize preferredSize;
  nsSize maxSize;

  nscoord LargestMinSize = 0;
  nscoord LargestPreferredSize = 0;
  nscoord SmallestMaxSize = NS_INTRINSICSIZE;
  
  nscoord count = 0;
  nsIFrame* childFrame = mFrames.FirstChild(); 
  while (nsnull != childFrame) 
  {  
    minSize.width = 0;
    minSize.height = 0;
    preferredSize.width = 0;
    preferredSize.height = 0;
    maxSize.width = NS_INTRINSICSIZE;
    maxSize.height = NS_INTRINSICSIZE;

    springs[count].wasFlowed = PR_FALSE;

    // if this is not incremental reflow or it is incremental reflow
    // and this child is the one we need to reflow then
    // we need to go out and do some work to get the preferred, min, max
    // if its not then we can just use out cached information.
    if (nsnull == incrementalChild || incrementalChild == childFrame)
    {
      // clear out spring
      springs[count].init();

      desiredSize.width = 0;
      desiredSize.height = 0;

      // get the size of our child
      const nsStylePosition* position;
      nsresult rv = childFrame->GetStyleData(eStyleStruct_Position,
                    (const nsStyleStruct*&) position);

      // get the min and max size    
      const nsStyleCoord* minWidth;
      const nsStyleCoord* minHeight;
      const nsStyleCoord* maxWidth;
      const nsStyleCoord* maxHeight;

      minWidth = &position->mMinWidth;
      minHeight = &position->mMinHeight;
      maxWidth = &position->mMaxWidth;
      maxHeight = &position->mMaxHeight;

      if (minWidth->GetUnit() == eStyleUnit_Coord) 
        minSize.width = minWidth->GetCoordValue();

      if (minHeight->GetUnit() == eStyleUnit_Coord) 
        minSize.height = minHeight->GetCoordValue();

      if (maxWidth->GetUnit() == eStyleUnit_Coord) 
        maxSize.width = maxWidth->GetCoordValue();

      if (maxHeight->GetUnit() == eStyleUnit_Coord) 
        maxSize.height = maxHeight->GetCoordValue();

      if (mHorizontal) {
         springs[count].minSize = minSize.width;
         springs[count].maxSize = maxSize.width;
         springs[count].altMinSize = minSize.height;
         springs[count].altMaxSize = maxSize.height;
      } else {
         springs[count].minSize = minSize.height;
         springs[count].maxSize = maxSize.height;
         springs[count].altMinSize = minSize.width;
         springs[count].altMaxSize = maxSize.width;
      }

      // get the spring constant
      nsCOMPtr<nsIContent> content;
      childFrame->GetContent(getter_AddRefs(content));

      PRInt32 error;
      nsString value;
    
      if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::flex, value))
      {
        value.Trim("%");
        float percent = value.ToFloat(&error);
        springs[count].springConstant = percent;    
      }

    } else {
        // if we are in incremental reflow and this child is not the
        // child we need to reflow then just get its cached information
        if (mHorizontal)
        {
          minSize.width = springs[count].minSize;
          maxSize.width = springs[count].maxSize;
          preferredSize.width = springs[count].preferredSize;

          minSize.height = springs[count].altMinSize;
          maxSize.height = springs[count].altMaxSize;
          preferredSize.height = springs[count].altPreferredSize;
        } else {
          minSize.width = springs[count].altMinSize;
          maxSize.width = springs[count].altMaxSize;
          preferredSize.width = springs[count].altPreferredSize;

          minSize.height = springs[count].minSize;
          maxSize.height = springs[count].maxSize;
          preferredSize.height = springs[count].preferredSize;
        }
    }

    // get the largest min size
    if (mHorizontal) {
      if (minSize.height > LargestMinSize) 
        LargestMinSize = minSize.height;
    } else {
      if (minSize.width > LargestMinSize) 
        LargestMinSize = minSize.width;
    }

    // get the smallest max size
    if (mHorizontal) {
      if (maxSize.height < SmallestMaxSize) 
        SmallestMaxSize = maxSize.height;
    } else {
      if (maxSize.width < SmallestMaxSize) 
        SmallestMaxSize = maxSize.width;
    }
    
    // ok if we are not incremental or incremental but and this is 
    // the child to reflow then lets flow the child
    if (nsnull == incrementalChild || incrementalChild == childFrame)
    {
      const nsStylePosition* position;
      nsresult rv = childFrame->GetStyleData(eStyleStruct_Position,
                    (const nsStyleStruct*&) position);

      // see if the width or height was specifically set
      nsStyleUnit wunit = position->mWidth.GetUnit();
      nsStyleUnit hunit = position->mHeight.GetUnit();

      // set the size to flow at. It should be at least as big
      // as out min height
      nsSize flexSize(availableSize);
      if (mHorizontal) {
         if (flexSize.height < LargestMinSize)
            flexSize.height = LargestMinSize;

         flexSize.width = NS_INTRINSICSIZE;
      } else { 
         if (flexSize.width < LargestMinSize)
            flexSize.width = LargestMinSize;

         flexSize.height = NS_INTRINSICSIZE;
      }
 
      // only flow if fixed and width or height was not set
      if (springs[count].springConstant == 0.0 && (wunit != eStyleUnit_Coord || hunit != eStyleUnit_Coord)) 
      {
            FlowChildAt(childFrame,aPresContext, desiredSize, aReflowState, aStatus, flexSize, incrementalChild);

            // if it got bigger that expected set that as our min size
            if (mHorizontal) {
              if (desiredSize.height > flexSize.height) {
                  minSize.height = desiredSize.height;
                  springs[count].altMinSize = minSize.height;
                  if (minSize.height > LargestMinSize)
                      LargestMinSize = minSize.height;
              }          
 
            } else {
              if (desiredSize.width > flexSize.width) {
                  minSize.width = desiredSize.width;
                  springs[count].altMinSize = minSize.width;
                  if (minSize.width > LargestMinSize)
                      LargestMinSize = minSize.width;
  
              } 
            }   
            
           //springs[count].wasFlowed = PR_TRUE;
           preferredSize.width = desiredSize.width;
           preferredSize.height = desiredSize.height;
      }

      // make sure the preferred size is not instrinic
      if (NS_INTRINSICSIZE == preferredSize.width)
         preferredSize.width = 0;

      if (NS_INTRINSICSIZE == preferredSize.height)
         preferredSize.height = 0;

      // get the preferred size from css
      if (wunit == eStyleUnit_Coord) 
          preferredSize.width = position->mWidth.GetCoordValue();

      if (hunit == eStyleUnit_Coord)  
          preferredSize.height = position->mHeight.GetCoordValue();

      // set the preferred size on the spring
      if (mHorizontal) {
          springs[count].preferredSize = preferredSize.width;
          springs[count].altPreferredSize = preferredSize.height;
      } else {
          springs[count].preferredSize = preferredSize.height;
          springs[count].altPreferredSize = preferredSize.width;
      }
    } 

    // get the largest preferred size
    if (mHorizontal) {
      if (preferredSize.height > LargestPreferredSize) 
        LargestPreferredSize = preferredSize.height;
    } else {
      if (preferredSize.width > LargestPreferredSize) 
        LargestPreferredSize = preferredSize.width;
    }

    nsresult rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }

  // if we are bieng flowed at our intrinsic width or height then set our width
  // to the biggest child.
  if (mHorizontal) {
    if (availableSize.height == NS_INTRINSICSIZE ) 
        availableSize.height = LargestPreferredSize;
  } else {
    if (availableSize.width == NS_INTRINSICSIZE ) 
        availableSize.width = LargestPreferredSize;
  }

  // make sure the available size is no bigger than the smallest max size
  if (mHorizontal)
  {
    if (availableSize.height > SmallestMaxSize)
       availableSize.height = SmallestMaxSize;
  } else {
    if (availableSize.width > SmallestMaxSize)
       availableSize.width = SmallestMaxSize;
  }

  // make sure the available size is at least as big as the largest
  // min size.
  if (mHorizontal)
  {
    if (availableSize.height < LargestMinSize)
       availableSize.height = LargestMinSize;
  } else {
    if (availableSize.width < LargestMinSize)
       availableSize.width = LargestMinSize;
  }

  // total number of children
  totalCount = count;
 
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
 
        // if we are on a second pass or we were need reflow
        // flow us.
        if (passes > 0 || springs[count].wasFlowed == PR_FALSE) {
         
          // do not do incremental reflow on a second pass
          nsIFrame* ichild = nsnull;

          if (passes == 0)
             ichild = incrementalChild;

          FlowChildAt(childFrame,aPresContext, desiredSize, aReflowState, aStatus, size, ichild);
          springs[count].wasFlowed = PR_TRUE;

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
  nscoord x = borderPadding.left + inset.left;
  nscoord y = borderPadding.top + inset.top;
  nscoord width = inset.left + inset.right;
  nscoord height = inset.top + inset.bottom;

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
      y = borderPadding.top + inset.top + margin.top;
    } else {
      y += margin.top;
      x = borderPadding.left + inset.left + margin.left;
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

  if (NS_INTRINSICSIZE != availableSize.width)
     availableSize.width += (inset.left + inset.right);

  if (NS_INTRINSICSIZE != availableSize.height)
     availableSize.height += (inset.top + inset.bottom);

  aDesiredSize.width += borderPadding.left + borderPadding.right;
  aDesiredSize.height += borderPadding.top + borderPadding.bottom;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
 
  aStatus = NS_FRAME_COMPLETE;
  
  return NS_OK;
}

void 
nsBoxFrame::GetInset(nsMargin& margin)
{
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
          if (spring.preferredSize < spring.minSize)
              spring.preferredSize = spring.minSize;

          if (spring.springConstant == 0.0)
          {
              spring.springConstant = CONSTANT;
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
                      sizeRemaining += spring.preferredSize;
                      sizeRemaining -= spring.calculatedSize;
 
                      spring.sizeValid = PR_TRUE;
                      limit = PR_TRUE;
                  }
                  else if (newSize>=spring.maxSize) {
                      spring.calculatedSize = spring.maxSize;
                      springConstantsRemaining -= spring.springConstant;
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


