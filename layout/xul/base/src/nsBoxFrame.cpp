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
#include "nsIContent.h"
#include "nsSpaceManager.h"
#include "nsHTMLParts.h"
#include "nsIViewManager.h"

#define CONSTANT float(0.0)

nsresult
NS_NewBoxFrame ( nsIFrame** aNewFrame, PRUint32 aFlags )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsBoxFrame* it = new nsBoxFrame(aFlags);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewBoxFrame

nsBoxFrame::nsBoxFrame(PRUint32 aFlags)
{
  // if not otherwise specified boxes by default are horizontal.
  mHorizontal = PR_TRUE;
  mFlags = aFlags;
}

/**
 * Initialize us. This is a good time to get the alignment of the box
 */
NS_IMETHODIMP
nsBoxFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // see if we are a vertical or horizontal box.
  nsString value;
  mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value);
  if (value.EqualsIgnoreCase("vertical"))
    mHorizontal = PR_FALSE;
  else if (value.EqualsIgnoreCase("horizontal"))
    mHorizontal = PR_TRUE;

  nsSpaceManager* spaceManager = new nsSpaceManager(this);
  mSpaceManager = spaceManager;

  return rv;
}



/** 
 * Looks at the given frame and sees if its redefined preferred, min, or max sizes
 * if so it used those instead. Currently it gets its values from css
 */
void 
nsBoxFrame::GetRedefinedMinPrefMax(nsIFrame* aFrame, nsBoxInfo& aSize)
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

    // get the flexibility
    nsCOMPtr<nsIContent> content;
    aFrame->GetContent(getter_AddRefs(content));

    PRInt32 error;
    nsString value;

    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::flex, value))
    {
        value.Trim("%");
        // convert to a percent.
        aSize.flex = value.ToFloat(&error)/float(100.0);
    }
}

/**
 * Given a frame gets its box info. If it does not have a box info then it will merely
 * get the normally defined min, pref, max stuff.
 *
 */
nsresult
nsBoxFrame::GetChildBoxInfo(nsIPresContext& aPresContext, const nsHTMLReflowState& aReflowState, nsIFrame* aFrame, nsBoxInfo& aSize)
{
  aSize.clear();

  // see if the frame implements IBox interface
  
  // since frames are not refCounted, don't use nsCOMPtr with them
  //nsCOMPtr<nsIBox> ibox = do_QueryInterface(aFrame);

  // if it does ask it for its BoxSize and we are done
  nsIBox* ibox;
  if (NS_SUCCEEDED(aFrame->QueryInterface(nsIBox::GetIID(), (void**)&ibox)) && ibox) {
     ibox->GetBoxInfo(aPresContext, aReflowState, aSize); 
     // add in the border, padding, width, min, max
     GetRedefinedMinPrefMax(aFrame, aSize);
     return NS_OK;
  }   

  // set the pref width and height to be intrinsic.
  aSize.prefWidthIntrinsic = PR_TRUE;
  aSize.prefHeightIntrinsic = PR_TRUE;

  GetRedefinedMinPrefMax(aFrame, aSize);

  return NS_OK;
}

/**
 * Ok what we want to do here is get all the children, figure out
 * their flexibility, preferred, min, max sizes and then stretch or
 * shrink them to fit in the given space.
 *
 * So we will have 3 passes. 
 * 1) get our min,max,preferred size.
 * 2) flow all our children to fit into the size we are given layout in
 * 3) move all the children to the right locations.
 */
NS_IMETHODIMP
nsBoxFrame::Reflow(nsIPresContext&   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  // If we have a space manager, then set it in the reflow state
  if (mSpaceManager) {
    // Modify the reflow state and set the space manager
    nsHTMLReflowState&  reflowState = (nsHTMLReflowState&)aReflowState;
    reflowState.mSpaceManager = mSpaceManager;

    // Clear the spacemanager's regions.
    mSpaceManager->ClearRegions();
  }

  //--------------------------------------------------------------------
  //-------------- figure out the rect we need to fit into -------------
  //--------------------------------------------------------------------

  // this is the size of our box. Remember to subtract our our border. The size we are given
  // does not include it. So we have to adjust our rect accordingly.

  nscoord x = aReflowState.mComputedBorderPadding.left;
  nscoord y = aReflowState.mComputedBorderPadding.top;

  nsRect rect(x,y,aReflowState.mComputedWidth,aReflowState.mComputedHeight);
 
  //---------------------------------------------------------
  //------- handle incremental reflow --------------------
  //---------------------------------------------------------

  // if there is incremental we need to tell all the boxes below to blow away the
  // cached values for the children in the reflow list
  nsIFrame* incrementalChild = nsnull;
  if ( aReflowState.reason == eReflowReason_Incremental ) {
    Dirty(aReflowState,incrementalChild);
  } 
#if 0
ListTag(stdout);
printf(": begin reflow reason=%s", 
       aReflowState.reason == eReflowReason_Incremental ? "incremental" : "other");
if (incrementalChild) { printf(" frame="); nsFrame::ListTag(stdout, incrementalChild); }
printf("\n");
#endif

  //------------------------------------------------------------------------------------------------
  //------- Figure out what our box size is. This will calculate our children's sizes as well ------
  //------------------------------------------------------------------------------------------------

  // get our size. This returns a boxSize that contains our min, max, pref sizes. It also
  // calculates all of our children sizes as well. It does not include our border we will have to include that 
  // later
  nsBoxInfo ourSize;
  GetBoxInfo(aPresContext, aReflowState, ourSize);

  //------------------------------------------------------------------------------------------------
  //------- Make sure the space we need to layout into adhears to our min, max, pref sizes    ------
  //------------------------------------------------------------------------------------------------

  BoundsCheck(ourSize, rect);
 
  // subtract out the insets. Insets are so subclasses like toolbars can wedge controls in and around the 
  // box. GetBoxInfo automatically adds them in. But we want to know the size we need to layout our children 
  // in so lets subtract them our for now.
  nsMargin inset(0,0,0,0);
  GetInset(inset);

  rect.Deflate(inset);

  //-----------------------------------------------------------------------------------
  //------------------------- figure our our children's sizes  -------------------------
  //-----------------------------------------------------------------------------------

  // now that we know our child's min, max, pref sizes. Stretch our children out to fit into our size.
  // this will calculate each of our childs sizes.
  InvalidateChildren();
  LayoutChildrenInRect(rect);

  //-----------------------------------------------------------------------------------
  //------------------------- flow all the children -----------------------------------
  //-----------------------------------------------------------------------------------

  // flow each child at the new sizes we have calculated.
  FlowChildren(aPresContext, aDesiredSize, aReflowState, aStatus, rect, incrementalChild);

  //-----------------------------------------------------------------------------------
  //------------------------- Adjust each childs x, y location-------------------------
  //-----------------------------------------------------------------------------------
   // set the x,y locations of each of our children. Taking into acount their margins, our border,
  // and insets.
  PlaceChildren(rect);

  //-----------------------------------------------------------------------------------
  //------------------------- Add our border and insets in ----------------------------
  //-----------------------------------------------------------------------------------

  // the rect might have gotten bigger so recalc ourSize
  rect.Inflate(inset);
  rect.Inflate(aReflowState.mComputedBorderPadding);

  aDesiredSize.width = rect.width;
  aDesiredSize.height = rect.height;

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
 
  aStatus = NS_FRAME_COMPLETE;
  
  nsRect damageArea(0,0,0,0);
  damageArea.y = 0;
  damageArea.height = aDesiredSize.height;
  damageArea.width = aDesiredSize.width;

  if ((NS_BLOCK_DOCUMENT_ROOT & mFlags) && !damageArea.IsEmpty()) {
    Invalidate(damageArea);
  }
#if 0
ListTag(stdout); printf(": reflow done\n");
#endif

  return NS_OK;
}


/**
 * When all the childrens positions have been calculated and layed out. Flow each child
 * at its not size.
 */
nsresult
nsBoxFrame::FlowChildren(nsIPresContext&   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     nsRect& rect,
                     nsIFrame*& incrementalChild)
{
  //-----------------------------------
  // first pass flow all fixed children
  //-----------------------------------

  PRBool resized[100];
  int i;
  for (i=0; i < mSpringCount; i++)
     resized[i] = PR_FALSE;

  PRBool finished;
  nscoord passes = 0;
  nscoord changedIndex = -1;
  nscoord count = 0;
  nsIFrame* childFrame = mFrames.FirstChild(); 
  while (nsnull != childFrame) 
  {    
   
      if (!mSprings[count].collapsed)
      {
        // reflow only fixed children
        if (mSprings[count].flex == 0.0) {
          FlowChildAt(childFrame, aPresContext, aDesiredSize, aReflowState, aStatus, count, incrementalChild);

          // if its height greater than the max. Set the max to this height and set a flag
          // saying we will need to do another pass. But keep going there
          // may be another child that is bigger
          if (!mHorizontal) {
            if (rect.height == NS_INTRINSICSIZE || aDesiredSize.height > mSprings[count].calculatedSize.height) {
                mSprings[count].calculatedSize.height = aDesiredSize.height;
                resized[count] = PR_TRUE;
            } 
          } else {
            if (rect.width == NS_INTRINSICSIZE || aDesiredSize.width > mSprings[count].calculatedSize.width) {
                mSprings[count].calculatedSize.width = aDesiredSize.width;
                resized[count] = PR_TRUE;
            } 
          }
        }
      }      
    nsresult rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }


  // ----------------------
  // Flow all children 
  // ----------------------

  // ok what we want to do if flow each child at the location given in the spring.
  // unfortunately after flowing a child it might get bigger. We have not control over this
  // so it the child gets bigger or smaller than we expected we will have to do a 2nd, 3rd, 4th pass to 
  // adjust.

  changedIndex = -1;
  InvalidateChildren();

  for (i=0; i < mSpringCount; i++)
     mSprings[i].sizeValid = resized[i];

  LayoutChildrenInRect(rect);

  passes = 0;
  do 
  {
    finished = PR_TRUE;
    nscoord count = 0;
    nsIFrame* childFrame = mFrames.FirstChild(); 
    while (nsnull != childFrame) 
    {    
        // if we reached the index that changed we are done.
        if (count == changedIndex)
            break;

        if (!mSprings[count].collapsed)
        {
        // reflow if the child needs it or we are on a second pass
      //  if (mSprings[count].needsReflow || passes > 0) {
          FlowChildAt(childFrame, aPresContext, aDesiredSize, aReflowState, aStatus, count, incrementalChild);

          // if its height greater than the max. Set the max to this height and set a flag
          // saying we will need to do another pass. But keep going there
          // may be another child that is bigger
          if (mHorizontal) {
            if (rect.height == NS_INTRINSICSIZE || aDesiredSize.height > rect.height) {
                rect.height = aDesiredSize.height;
                finished = PR_FALSE;
                changedIndex = count;
                for (int i=0; i < mSpringCount; i++)
                   resized[i] = PR_FALSE;
            } 

            // if we are wider than we anticipated then
            // then this child can't get smaller then the size returned
            // so set its minSize to be the desired and restretch. Then
            // just start over because the springs are all messed up 
            // anyway.
            if (aDesiredSize.width > mSprings[count].calculatedSize.width) {
                mSprings[count].calculatedSize.width = aDesiredSize.width;
                resized[count] = PR_TRUE;

                for (int i=0; i < mSpringCount; i++)
                   mSprings[i].sizeValid = resized[i];
 
                finished = PR_FALSE;
                changedIndex = count;
            } 
        

          } else {
            if (rect.width == NS_INTRINSICSIZE || aDesiredSize.width > rect.width) {
                rect.width = aDesiredSize.width;
                finished = PR_FALSE;
                changedIndex = count;
                for (int i=0; i < mSpringCount; i++)
                    resized[i] = PR_FALSE;
            } 

        
            if (aDesiredSize.height > mSprings[count].calculatedSize.height) {
                mSprings[count].calculatedSize.height = aDesiredSize.height;

                resized[count] = PR_TRUE;

                for (int i=0; i < mSpringCount; i++)
                   mSprings[i].sizeValid = resized[i];

                LayoutChildrenInRect(rect);
                finished = PR_FALSE;
                changedIndex = count;
            }     
        }
      }
        
      nsresult rv = childFrame->GetNextSibling(&childFrame);
      NS_ASSERTION(rv == NS_OK,"failed to get next child");
      count++;
    }

    // if we get over 10 passes something probably when wrong.
    passes++;
    if (passes > 5) {
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("bug"));
    }

    //NS_ASSERTION(passes <= 10,"Error infinte loop too many passes");
    if (passes > 10) {
       break;
    }


  } while (PR_FALSE == finished);

  return NS_OK;
}

/*
void CollapseChildren(nsIFrame* frame)
{
  nsIFrame* childFrame = mFrames.FirstChild(); 
  nscoord count = 0;
  while (nsnull != childFrame) 
  {
    childFrame->SetRect(nsRect(0,0,0,0));
    // make the view really small as well
    nsIView* view = nsnull;
    childFrame->GetView(&view);

    if (view) {
      view->SetDimensions(0,0,PR_FALSE);
    }
 
    CollapseChildren(childFrame);
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
  }
}
*/

/**
 * Given the boxes rect. Set the x,y locations of all its children. Taking into account
 * their margins
 */
nsresult
nsBoxFrame::PlaceChildren(nsRect& boxRect)
{
  // ------- set the childs positions ---------
  nscoord x = boxRect.x;
  nscoord y = boxRect.y;

  nsIFrame* childFrame = mFrames.FirstChild(); 
  nscoord count = 0;
  while (nsnull != childFrame) 
  {
    nsresult rv;

    // make collapsed children not show up
    if (mSprings[count].collapsed) {
      childFrame->SetRect(nsRect(0,0,0,0));

      // make the view really small as well
      nsIView* view = nsnull;
      childFrame->GetView(&view);

      if (view) {
        nsCOMPtr<nsIViewManager> vm;
        view->GetViewManager(*getter_AddRefs(vm));
        vm->ResizeView(view, 0,0);
      }
    } else {
      const nsStyleSpacing* spacing;
      rv = childFrame->GetStyleData(eStyleStruct_Spacing,
                     (const nsStyleStruct*&) spacing);

      nsMargin margin(0,0,0,0);
      spacing->GetMargin(margin);

      if (mHorizontal) {
        x += margin.left;
        y = boxRect.y + margin.top;
      } else {
        y += margin.top;
        x = boxRect.x + margin.left;
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
        //width += rect.width + margin.left + margin.right;
      } else {
        y += rect.height;
        //height += rect.height + margin.top + margin.bottom;
      }
    }

    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }

  return NS_OK;
}

/**
 * Flow an individual child. Special args:
 * count: the spring that will be used to lay out the child
 * incrementalChild: If incremental reflow this is the child that need to be reflowed.
 *                   when we finally do reflow the child we will set the child to null
 */

nsresult
nsBoxFrame::FlowChildAt(nsIFrame* childFrame, 
                     nsIPresContext& aPresContext,
                     nsHTMLReflowMetrics&     desiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     nscoord spring,
                     nsIFrame*& incrementalChild)
{
#if 0
ListTag(stdout); printf(": reflowing ");
nsFrame::ListTag(stdout, childFrame);
printf("\n");
#endif
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
      if (reason == eReflowReason_Incremental && (nsnull == incrementalChild || incrementalChild != childFrame))
          reason = eReflowReason_Resize;

      // if we don't need a reflow then 
      // lets see if we are already that size. Yes? then don't even reflow. We are done.
      if (!mSprings[spring].needsReflow) {
          nsRect currentSize;
          childFrame->GetRect(currentSize);

          if (currentSize.width > 0 && currentSize.height > 0)
          {
            desiredSize.width = currentSize.width;
            desiredSize.height = currentSize.height;

            if (currentSize.width == mSprings[spring].calculatedSize.width && currentSize.height == mSprings[spring].calculatedSize.height)
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

        // tell the child what size they should be
        reflowState.mComputedWidth = mSprings[spring].calculatedSize.width;
        reflowState.mComputedHeight = mSprings[spring].calculatedSize.height;

        // only subrtact margin and border.
        if (reflowState.mComputedWidth != NS_INTRINSICSIZE)
            reflowState.mComputedWidth -= (total.left + total.right);

        if (reflowState.mComputedHeight != NS_INTRINSICSIZE)
            reflowState.mComputedHeight -= (total.top + total.bottom);

        // HTML frames do not implement nsIBox so unless they set both their width and height we do not know
        // what there preferred size is. We can assume a preferred width or height of 0 when flexable but when
        // not flexible we are in trouble. Why? Well if the child is fixed we really want its intrinsic size and
        // the only way to get it is to flow with NS_INTRINSIC. So lets do that if we have to.
        if (mSprings[spring].flex == CONSTANT) {
          if (mHorizontal) {
            if (mSprings[spring].prefWidthIntrinsic) 
               reflowState.mComputedWidth = NS_INTRINSICSIZE;
          } else {
            if (mSprings[spring].prefHeightIntrinsic) 
               reflowState.mComputedHeight = NS_INTRINSICSIZE;
          }
        }

        // HTML block don't seem to return the actually size they layed themselves
        // out in if they did not fit. So if the height is 0 indicating no one set it them. Get this 
        // fixed in blocks themselves.
        if (mHorizontal) {
          // if we could not get the height of the child because it did not implement nsIBox and
          // it did not provide a height via css and we are trying to lay it out with a height of 0
          if (mSprings[spring].prefHeightIntrinsic && reflowState.mComputedHeight != NS_INTRINSICSIZE) {
              nscoord oldHeight = mSprings[spring].calculatedSize.height;
              mSprings[spring].calculatedSize.height = NS_INTRINSICSIZE;
              FlowChildAt(childFrame, aPresContext, desiredSize, aReflowState, aStatus, spring, incrementalChild);

              mSprings[spring].calculatedSize.height = oldHeight;

              // remember that when we get the size back it has its margin and borderpadding
              // added to it! So we must remove it before we make the comparison
              desiredSize.width -= (total.left + total.right);
              desiredSize.height -= (total.top + total.bottom);

              // see if things are ok
              if (reflowState.mComputedHeight < desiredSize.height)
                 reflowState.mComputedHeight = desiredSize.height;

          }
            
        } 

        nsSize* oldMaxElementSize = desiredSize.maxElementSize;
        nsSize maxElementSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
        desiredSize.maxElementSize = &maxElementSize;
        
        // do the flow
        nsIHTMLReflow*      htmlReflow;

        rv = childFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
        NS_ASSERTION(rv == NS_OK,"failed to get htmlReflow interface.");

        htmlReflow->WillReflow(aPresContext);
        htmlReflow->Reflow(aPresContext, desiredSize, reflowState, aStatus);
        NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

        if (maxElementSize.width != NS_INTRINSICSIZE && maxElementSize.width > desiredSize.width)
            desiredSize.width = maxElementSize.width;

          // set the rect
        childFrame->SetRect(nsRect(0,0,desiredSize.width, desiredSize.height));

        // Stub out desiredSize.maxElementSize so that when go out of
        // scope, nothing bad happens!
        desiredSize.maxElementSize = oldMaxElementSize;

        // clear out the incremental child, so that we don't flow it incrementally again
        if (reason == eReflowReason_Incremental && incrementalChild == childFrame)
          incrementalChild = nsnull;
      
      }
      // add the margin back in. The child should add its border automatically
      desiredSize.height += (margin.top + margin.bottom);
      desiredSize.width += (margin.left + margin.right);

      mSprings[spring].needsReflow = PR_FALSE;

      return NS_OK;
}

/**
 * Given a box info object and a rect. Make sure the rect is not too small to layout the box and
 * not to big either.
 */
void
nsBoxFrame::BoundsCheck(const nsBoxInfo& aBoxInfo, nsRect& aRect)
{ 
  // if we are bieng flowed at our intrinsic width or height then set our width
  // to the biggest child.
  if (aRect.height == NS_INTRINSICSIZE ) 
      aRect.height = aBoxInfo.prefSize.height;

  if (aRect.width == NS_INTRINSICSIZE ) 
      aRect.width = aBoxInfo.prefSize.width;

  
  // make sure the available size is no bigger than the max size
  if (aRect.height > aBoxInfo.maxSize.height)
     aRect.height = aBoxInfo.maxSize.height;

  if (aRect.width > aBoxInfo.maxSize.width)
     aRect.width = aBoxInfo.maxSize.width;

  // make sure the available size is at least as big as the min size
  if (aRect.height < aBoxInfo.minSize.height)
     aRect.height = aBoxInfo.minSize.height;

  if (aRect.width < aBoxInfo.minSize.width)
     aRect.width = aBoxInfo.minSize.width;
     
}

/**
 * Ok when calculating a boxes size such as its min size we need to look at its children to figure it out.
 * But this isn't as easy as just adding up its childs min sizes. If the box is horizontal then we need to 
 * add up each child's min width but our min height should be the childs largest min height. This needs to 
 * be done for preferred size and max size as well. Of course for our max size we need to pick the smallest
 * max size. So this method facilitates the calculation. Just give it 2 sizes and a flag to ask whether is is
 * looking for the largest or smallest value (max needs smallest) and it will set the second value.
 */
void
nsBoxFrame::AddSize(const nsSize& a, nsSize& b, PRBool largest)
{

  // depending on the dimension switch either the width or the height component. 
  const nscoord& awidth  = mHorizontal ? a.width  : a.height;
  const nscoord& aheight = mHorizontal ? a.height : a.width;
  nscoord& bwidth  = mHorizontal ? b.width  : b.height;
  nscoord& bheight = mHorizontal ? b.height : b.width;

  // add up the widths make sure we check for intrinsic.
  if (bwidth != NS_INTRINSICSIZE) // if we are already intrinsic we are done
  {
    // otherwise if what we are adding is intrinsic then we just become instrinsic and we are done
    if (awidth == NS_INTRINSICSIZE)
       bwidth = NS_INTRINSICSIZE;
    else // add it on
       bwidth += awidth;
  }
  
  // store the largest or smallest height
  if ((largest && aheight > bheight) || (!largest && bheight < aheight)) 
      bheight = aheight;
}



void 
nsBoxFrame::GetInset(nsMargin& margin)
{
}

#define GET_WIDTH(size) (mHorizontal ? size.width : size.height)
#define GET_HEIGHT(size) (mHorizontal ? size.height : size.width)

void
nsBoxFrame::InvalidateChildren()
{
    for (int i=0; i < mSpringCount; i++) {
        mSprings[i].sizeValid = PR_FALSE;
    }
}

void
nsBoxFrame::LayoutChildrenInRect(nsRect& size)
{
      if (mSpringCount == 0)
          return;

      PRInt32 sizeRemaining;
       
      if (mHorizontal)
          sizeRemaining = size.width;
      else
          sizeRemaining = size.height;

      float springConstantsRemaining = (float)0.0;
      int i;

      for (i=0; i<mSpringCount; i++) {
          nsCalculatedBoxInfo& spring = mSprings[i];
 
          // ignore collapsed children
          if (spring.collapsed)
              continue;

          // figure out the direction of the box and get the correct value either the width or height
          nscoord& pref = GET_WIDTH(spring.prefSize);
          nscoord& max  = GET_WIDTH(spring.maxSize);
          nscoord& min  = GET_WIDTH(spring.minSize);
         
          GET_HEIGHT(spring.calculatedSize) = GET_HEIGHT(size);
        
          if (pref < min)
              pref = min;

          if (spring.sizeValid) { 
             sizeRemaining -= GET_WIDTH(spring.calculatedSize);
          } else {
            if (spring.flex == 0.0)
            {
              spring.sizeValid = PR_TRUE;
              GET_WIDTH(spring.calculatedSize) = pref;
            }
            sizeRemaining -= pref;
            springConstantsRemaining += spring.flex;
          } 
      }

      nscoord& sz = GET_WIDTH(size);
      if (sz == NS_INTRINSICSIZE) {
          sz = 0;
          for (i=0; i<mSpringCount; i++) {
              nsCalculatedBoxInfo& spring=mSprings[i];

              // ignore collapsed springs
              if (spring.collapsed)
                 continue;

              nscoord& calculated = GET_WIDTH(spring.calculatedSize);
              nscoord& pref = GET_WIDTH(spring.prefSize);

              if (!spring.sizeValid) 
              {
                // set the calculated size to be the preferred size
                calculated = pref;
                spring.sizeValid = PR_TRUE;
              }

              // changed the size returned to reflect
              sz += calculated;
          }
          return;
      }

      PRBool limit = PR_TRUE;
      for (int pass=1; PR_TRUE == limit; pass++) {
          limit = PR_FALSE;
          for (i=0; i<mSpringCount; i++) {
              nsCalculatedBoxInfo& spring=mSprings[i];
              // ignore collapsed springs
              if (spring.collapsed)
                 continue;

              nscoord& pref = GET_WIDTH(spring.prefSize);
              nscoord& max  = GET_WIDTH(spring.maxSize);
              nscoord& min  = GET_WIDTH(spring.minSize);
              nscoord& calculated = GET_WIDTH(spring.calculatedSize);
     
              if (spring.sizeValid==PR_FALSE) {
                  PRInt32 newSize = pref + NSToIntRound(sizeRemaining*(spring.flex/springConstantsRemaining));
                  if (newSize<=min) {
                      calculated = min;
                      springConstantsRemaining -= spring.flex;
                      sizeRemaining += pref;
                      sizeRemaining -= calculated;
 
                      spring.sizeValid = PR_TRUE;
                      limit = PR_TRUE;
                  }
                  else if (newSize>=max) {
                      calculated = max;
                      springConstantsRemaining -= spring.flex;
                      sizeRemaining += pref;
                      sizeRemaining -= calculated;
                      spring.sizeValid = PR_TRUE;
                      limit = PR_TRUE;
                  }
              }
          }
      }

      float stretchFactor = sizeRemaining/springConstantsRemaining;

        nscoord& s = GET_WIDTH(size);
        s = 0;
        for (i=0; i<mSpringCount; i++) {
             nsCalculatedBoxInfo& spring=mSprings[i];

             // ignore collapsed springs
             if (spring.collapsed)
                 continue;

             nscoord& pref = GET_WIDTH(spring.prefSize);
             nscoord& calculated = GET_WIDTH(spring.calculatedSize);
  
            if (spring.sizeValid==PR_FALSE) {
                calculated = pref + NSToIntFloor(spring.flex*stretchFactor);
                spring.sizeValid = PR_TRUE;
            }

            s += calculated;
        }
}


NS_IMETHODIMP
nsBoxFrame::RemoveFrame(nsIPresContext& aPresContext,
                           nsIPresShell& aPresShell,
                           nsIAtom* aListName,
                           nsIFrame* aOldFrame)
{
      // need to rebuild all the springs.
      for (int i=0; i < mSpringCount; i++) 
             mSprings[i].clear();
      
      // remove the child frame
      nsresult rv = nsHTMLContainerFrame::RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
      mFrames.DestroyFrame(aPresContext, aOldFrame);
      return rv;
}

NS_IMETHODIMP
nsBoxFrame::InsertFrames(nsIPresContext& aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aPrevFrame,
                            nsIFrame* aFrameList)
{
  // need to rebuild all the springs.
  for (int i=0; i < mSpringCount; i++) 
         mSprings[i].clear();

  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  return nsHTMLContainerFrame::InsertFrames(aPresContext, aPresShell, aListName, aPrevFrame, aFrameList); 
}

NS_IMETHODIMP
nsBoxFrame::AppendFrames(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{

  // need to rebuild all the springs.
  for (int i=0; i < mSpringCount; i++) 
      mSprings[i].clear();

   mFrames.AppendFrames(nsnull, aFrameList); 
   return nsHTMLContainerFrame::AppendFrames(aPresContext, aPresShell, aListName, aFrameList); 
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

/**
 * Goes though each child asking for its size to determine our size. Returns our box size minus our border.
 * This method is defined in nsIBox interface.
 */
NS_IMETHODIMP
nsBoxFrame::GetBoxInfo(nsIPresContext& aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
{
   nsresult rv;

   aSize.clear();
 
   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box
   nscoord count = 0;
   nsIFrame* childFrame = mFrames.FirstChild(); 

   /*
   // if we have any children assume we are intrinsic unless a child is not
   if (childFrame != nsnull) {
       aSize.prefHeightIntrinsic = PR_TRUE;
       aSize.prefWidthIntrinsic = PR_TRUE;
   }
   */

   while (nsnull != childFrame) 
   {  
    // if a child needs recalculation then ask it for its size. Otherwise
    // just use the size we already have.
    if (mSprings[count].needsRecalc)
    {
      // get the size of the child. This is the min, max, preferred, and spring constant
      // it does not include its border.
      rv = GetChildBoxInfo(aPresContext, aReflowState, childFrame, mSprings[count]);
      NS_ASSERTION(rv == NS_OK,"failed to child box info");
      if (NS_FAILED(rv))
         return rv;

      // see if the child is collapsed
      const nsStyleDisplay* disp;
      childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)disp));

      // if collapsed then the child will have no size
      if (disp->mVisible == NS_STYLE_VISIBILITY_COLLAPSE) 
         mSprings[count].collapsed = PR_TRUE;
      else {
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
        mSprings[count].minSize += m;
        mSprings[count].prefSize += m;

        spacing->GetBorderPadding(margin);
        nsSize b(margin.left+margin.right,margin.top+margin.bottom);
        mSprings[count].minSize += b;
        mSprings[count].prefSize += b;
      }

      // ok we don't need to calc this guy again
      mSprings[count].needsRecalc = PR_FALSE;
    } 

    /*
    // if a size is not intrinsic then our size is not intrinsic.
    if (!mSprings[count].prefWidthIntrinsic)
        aSize.prefWidthIntrinsic = PR_FALSE;

    if (!mSprings[count].prefHeightIntrinsic)
        aSize.prefHeightIntrinsic = PR_FALSE;
    */

    // now that we know our child's min, max, pref sizes figure OUR size from them.
    AddSize(mSprings[count].minSize,  aSize.minSize,  PR_FALSE);
    AddSize(mSprings[count].maxSize,  aSize.maxSize,  PR_TRUE);
    AddSize(mSprings[count].prefSize, aSize.prefSize, PR_FALSE);

    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    if (NS_FAILED(rv))
       return rv;

    count++;
  }

  mSpringCount = count;

  // add the insets into our size. This is merely some extra space subclasses like toolbars
  // can place around us. Toolbars use it to place extra control like grippies around things.
  nsMargin inset(0,0,0,0);
  GetInset(inset);
  
  nsSize in(inset.left+inset.right,inset.top+inset.bottom);
  aSize.minSize += in;
  aSize.prefSize += in;

  return rv;
}

/**
 * Called with a reflow command. This will dirty all boxes who need to be reflowed.
 * return the last child that is not a box. Part of nsIBox interface.
 */
NS_IMETHODIMP
nsBoxFrame::Dirty(const nsHTMLReflowState& aReflowState, nsIFrame*& incrementalChild)
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
        mSprings[count].clear();
        //nsCOMPtr<nsIBox> ibox = do_QueryInterface(childFrame);
        // can't use nsCOMPtr on non-refcounted things like frames
        nsIBox* ibox;
        if (NS_SUCCEEDED(childFrame->QueryInterface(nsIBox::GetIID(), (void**)&ibox)) && ibox)
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

NS_IMETHODIMP nsBoxFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
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
nsBoxFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsBoxFrame::Release(void)
{
    return NS_OK;
}

NS_IMETHODIMP
nsBoxFrame::GetFrameName(nsString& aResult) const
{
  aResult = "Box";
  return NS_OK;
}

nsCalculatedBoxInfo::nsCalculatedBoxInfo()
{
   clear();
}

void 
nsCalculatedBoxInfo::clear()
{       
    nsBoxInfo::clear();
    needsReflow = PR_TRUE;
    needsRecalc = PR_TRUE;
    collapsed = PR_FALSE;

    calculatedSize.width = 0;
    calculatedSize.height = 0;

    sizeValid = PR_FALSE;
}

