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

#include "nsBoxLayoutState.h"
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
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsGenericHTMLElement.h"
#include "nsFrameNavigator.h"
#include "nsCSSRendering.h"
#include "nsISelfScrollingFrame.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsBoxToBlockAdaptor.h"
#include "nsILineIterator.h"
#include "nsIFontMetrics.h"

nsBoxToBlockAdaptor::nsBoxToBlockAdaptor(nsIPresShell* aPresShell, nsIFrame* aFrame):nsBox(aPresShell)
{
  mFrame = aFrame;
  mSpaceManager = nsnull;
  mWasCollapsed = PR_FALSE;
  mCachedMaxElementHeight = 0;
  NeedsRecalc();
}

void* 
nsBoxToBlockAdaptor::operator new(size_t sz, nsIPresShell* aPresShell)
{
    return nsBoxLayoutState::Allocate(sz,aPresShell);
}

NS_IMETHODIMP 
nsBoxToBlockAdaptor::Recycle(nsIPresShell* aPresShell)
{
  delete this;
  nsBoxLayoutState::RecycleFreedMemory(aPresShell, this);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxToBlockAdaptor::GetFrame(nsIFrame** aFrame)
{
  *aFrame = mFrame;  
  return NS_OK;
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsBoxToBlockAdaptor::operator delete(void* aPtr, size_t sz)
{
    nsBoxLayoutState::Free(aPtr, sz);
}

nsBoxToBlockAdaptor::~nsBoxToBlockAdaptor()
{
  if (mSpaceManager)
     NS_RELEASE(mSpaceManager);
}


NS_IMETHODIMP
nsBoxToBlockAdaptor::NeedsRecalc()
{
    nsIBox* parent;
  GetParentBox(&parent);
  nsIFrame* frame;
  if (parent) {
    parent->GetFrame(&frame);
    nsFrameState frameState = 0;
    frame->GetFrameState(&frameState);
  }

  mMinWidth = -1;
  mPrefNeedsRecalc = PR_TRUE;
  SizeNeedsRecalc(mMinSize);
  SizeNeedsRecalc(mMaxSize);
  CoordNeedsRecalc(mFlex);
  CoordNeedsRecalc(mAscent);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxToBlockAdaptor::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{

  if (!mPrefNeedsRecalc) {
     aSize = mPrefSize;
     return NS_OK;
  }

  nsresult rv = NS_OK;

  // see if anything is defined in css. If pref size is competely
  // defined in css we are golden. We do nothing. But is its not
  // in css we will have to reflow the child.
  nsSize pref(0,0);
  
  PRBool completelyRedefined = nsIBox::AddCSSPrefSize(aState, this, mPrefSize);

  if (!completelyRedefined) {
     const nsHTMLReflowState* reflowState = aState.GetReflowState();
     if (reflowState) {
       nsIPresContext* presContext = aState.GetPresContext();
       nsReflowStatus status = NS_FRAME_COMPLETE;
       nsHTMLReflowMetrics desiredSize(nsnull);

       PRBool isDirty = PR_FALSE;
       IsDirty(isDirty);

       nsSize* currentSize = nsnull;
       aState.GetMaxElementSize(&currentSize);
       nsSize size(0,0);

       if (currentSize) {
        desiredSize.maxElementSize = &size;
       }

       rv = Reflow(presContext, 
                  desiredSize, 
                  *reflowState, 
                  status,
                  0,
                  0,
                  NS_INTRINSICSIZE,
                  NS_INTRINSICSIZE,
                  PR_FALSE);

     if (currentSize) {
        desiredSize.maxElementSize = nsnull;

        if (size.width > currentSize->width)
          currentSize->width = size.width;

        if (size.height > currentSize->height)
           currentSize->height = size.height;

        mCachedMaxElementHeight = size.height;
      }

       nsFrameState frameState = 0;
       mFrame->GetFrameState(&frameState);
       frameState |= NS_FRAME_IS_DIRTY;
       mFrame->SetFrameState(frameState);

       mPrefSize.width = desiredSize.width;
       mPrefSize.height = desiredSize.height;
       mAscent = desiredSize.ascent;

       nsMargin m(0,0,0,0);
       GetInset(m);
       mAscent += m.top;

       // ok the sizes might be bigger than our css preferred size.
       // check it.
       AddInset(mPrefSize);

       nsIBox::AddCSSPrefSize(aState, this, mPrefSize);
       mPrefNeedsRecalc = PR_FALSE;
    }
  }

  aSize = mPrefSize;
  
  return rv;
}

NS_IMETHODIMP
nsBoxToBlockAdaptor::GetMinSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  if (!DoesNeedRecalc(mMinSize)) {
     aSize = mMinSize;
     return NS_OK;
  }

  mMinSize.width  = 0;
  mMinSize.height = 0;

  AddBorderAndPadding(mMinSize);
  AddInset(mMinSize);

  if (mMinWidth != -1)
    mMinSize.width = mMinWidth;

  nsIBox::AddCSSMinSize(aState, this, mMinSize);

  aSize = mMinSize;

  return NS_OK;
}

NS_IMETHODIMP
nsBoxToBlockAdaptor::GetMaxSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  if (!DoesNeedRecalc(mMaxSize)) {
     aSize = mMaxSize;
     return NS_OK;
  }

  nsresult rv = nsBox::GetMaxSize(aState, mMaxSize);
  aSize = mMaxSize;

  return rv;
}

NS_IMETHODIMP
nsBoxToBlockAdaptor::GetFlex(nsBoxLayoutState& aState, nscoord& aFlex)
{
  if (!DoesNeedRecalc(mFlex)) {
     aFlex = mFlex;
     return NS_OK;
  }

  mFlex = 0;
  nsBox::GetFlex(aState, mFlex);

  aFlex = mFlex;

  return NS_OK;
}

NS_IMETHODIMP
nsBoxToBlockAdaptor::GetAscent(nsBoxLayoutState& aState, nscoord& aAscent)
{
  if (!DoesNeedRecalc(mAscent)) {
    aAscent = mAscent;
    return NS_OK;
  }

  nsSize size;
  GetPrefSize(aState, size);

  aAscent = mAscent;

  return NS_OK;
}

NS_IMETHODIMP
nsBoxToBlockAdaptor::IsCollapsed(nsBoxLayoutState& aState, PRBool& aCollapsed)
{
  return nsBox::IsCollapsed(aState, aCollapsed);
}

nsresult
nsBoxToBlockAdaptor::Layout(nsBoxLayoutState& aState)
{
   nsRect ourRect(0,0,0,0);
   GetBounds(ourRect);

   const nsHTMLReflowState* reflowState = aState.GetReflowState();
   nsIPresContext* presContext = aState.GetPresContext();
   nsReflowStatus status = NS_FRAME_COMPLETE;
   nsHTMLReflowMetrics desiredSize(nsnull);
   nsresult rv;
 
   if (reflowState) {

    nsSize* currentSize = nsnull;
    aState.GetMaxElementSize(&currentSize);
    nsSize size(0,0);

    if (currentSize) {
      desiredSize.maxElementSize = &size;
    }

     rv = Reflow(presContext, 
                  desiredSize, 
                  *reflowState, 
                  status,
                  ourRect.x,
                  ourRect.y,
                  ourRect.width,
                  ourRect.height);
     
    if (currentSize) {
      desiredSize.maxElementSize = nsnull;

      if (size.width > currentSize->width)
         currentSize->width = size.width;
  
      if (mCachedMaxElementHeight > currentSize->height) {
        currentSize->height = mCachedMaxElementHeight;
      }
    }


     mAscent = desiredSize.ascent;
     mFrame->SizeTo(presContext, desiredSize.width, desiredSize.height);
   }

   SyncLayout(aState);

   return rv;
}

nsresult
nsBoxToBlockAdaptor::Reflow(nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     nscoord aX,
                     nscoord aY,
                     nscoord aWidth,
                     nscoord aHeight,
                     PRBool aMoveFrame)
{

  //printf("width=%d, height=%d\n", aWidth, aHeight);

      nsIBox* parent;
  GetParentBox(&parent);
  nsIFrame* frame;
  parent->GetFrame(&frame);
  nsFrameState frameState = 0;
  frame->GetFrameState(&frameState);

 // if (frameState & NS_STATE_CURRENTLY_IN_DEBUG)
  //   printf("In debug\n");
  
  if (mSpaceManager == nsnull) {
      mSpaceManager = new nsSpaceManager(mFrame);
      NS_ADDREF(mSpaceManager);
  }

  // Modify the reflow state and set the space manager
  nsHTMLReflowState&  nonConstState = (nsHTMLReflowState&)aReflowState;
  nonConstState.mSpaceManager = mSpaceManager;

  // Clear the spacemanager's regions.
  mSpaceManager->ClearRegions();
  

  aStatus = NS_FRAME_COMPLETE;
  PRBool needsReflow = PR_FALSE;

  // get our frame its size and the reflow state

  nsReflowReason reason = aReflowState.reason;

  nsFrameState childState;
  mFrame->GetFrameState(&childState);
 // childState |= NS_BLOCK_SPACE_MGR;
 // mFrame->SetFrameState(childState);


  if (childState & NS_FRAME_FIRST_REFLOW) {
      if (reason != eReflowReason_Initial)
      {
          // if incremental then make sure we send a initial reflow first.
          if (reason == eReflowReason_Incremental) {
              nsHTMLReflowState state(aReflowState);
              state.reason = eReflowReason_Initial;
              state.reflowCommand = nsnull;
              Reflow(aPresContext, aDesiredSize, state, aStatus, aX, aY, aWidth, aHeight, aMoveFrame);
          } else {
              // convert to initial if not incremental.
              reason = eReflowReason_Initial;
          }

      }
  } else if (reason == eReflowReason_Initial) {
      reason = eReflowReason_Resize;
  }

  // handle or different types of reflow
  switch(reason)
  {
   // if the child we are reflowing is the child we popped off the incremental 
   // reflow chain then we need to reflow it no matter what.
   // if its not the child we got from the reflow chain then this child needs reflow
   // because as a side effect of the incremental child changing aize andit needs to be resized.
   // This will happen with a lot when a box that contains 2 children with different flexabilities
   // if on child gets bigger the other is affected because it is proprotional to the first.
   // so it might need to be resized. But we don't need to reflow it. If it is already the
   // needed size then we will do nothing. 
   case eReflowReason_Incremental: {

      // if incremental see if the next child in the chain is the child. If so then
      // we will just let it go down. If not then convert it to a dirty. It will get converted back when 
      // needed in the case just below this one.
      nsIFrame* incrementalChild = nsnull;
      aReflowState.reflowCommand->GetNext(incrementalChild, PR_FALSE);
      if (incrementalChild == nsnull)
          aReflowState.reflowCommand->GetTarget(incrementalChild);

      if (incrementalChild == mFrame) {
          needsReflow = PR_TRUE;
          aReflowState.reflowCommand->GetNext(incrementalChild);
          break;
      }

      // fall into dirty
   }

   // if its dirty then see if the child we want to reflow is dirty. If it is then
   // mark it as needing to be reflowed.
   case eReflowReason_Dirty: {
        // only frames that implement nsIBox seem to be able to handle a reason of Dirty. For everyone else
        // send down a resize.
        reason = eReflowReason_Resize;

        // get the frame state to see if it needs reflow
        needsReflow = (childState & NS_FRAME_IS_DIRTY) || (childState & NS_FRAME_HAS_DIRTY_CHILDREN);

   } break;

   // if the a resize reflow then it doesn't need to be reflowed. Only if the size is different
   // from the new size would we actually do a reflow
   case eReflowReason_Resize:
       needsReflow = PR_FALSE;
   break;

   // if its an initial reflow we must place the child.
   // otherwise we might think it was already placed when it wasn't
   case eReflowReason_Initial:
       aMoveFrame = PR_TRUE;
       needsReflow = PR_TRUE;
   break;

   default:
       needsReflow = PR_TRUE;
 
  }

  //nsMargin margin(0,0,0,0);
 // GetMargin(margin);

  // if we don't need a reflow then 
  // lets see if we are already that size. Yes? then don't even reflow. We are done.
  if (!needsReflow) {
      
      if (aWidth != NS_INTRINSICSIZE && aHeight != NS_INTRINSICSIZE) {
      
          // if the new calculated size has a 0 width or a 0 height
          if ((mLastSize.width == 0 || mLastSize.height == 0) && (aWidth == 0 || aHeight == 0)) {
               needsReflow = PR_FALSE;
               aDesiredSize.width = aWidth; 
               aDesiredSize.height = aHeight; 
               mFrame->SizeTo(aPresContext, aDesiredSize.width, aDesiredSize.height);
          } else {
            aDesiredSize.width = mLastSize.width;
            aDesiredSize.height = mLastSize.height;

            // remove the margin. The rect of our child does not include it but our calculated size does.
            nscoord calcWidth = aWidth; 
            nscoord calcHeight = aHeight; 
            // don't reflow if we are already the right size
            if (mLastSize.width == calcWidth && mLastSize.height == calcHeight)
                  needsReflow = PR_FALSE;
            else
                  needsReflow = PR_TRUE;
   
          }
      } else {
          // if the width or height are intrinsic alway reflow because
          // we don't know what it should be.
         needsReflow = PR_TRUE;
      }
  }
                             
  // ok now reflow the child into the springs calculated space
  if (needsReflow) {

    nsMargin border(0,0,0,0);
    GetBorderAndPadding(border);


    aDesiredSize.width = 0;
    aDesiredSize.height = 0;

    nsSize size(aWidth, aHeight);

    // create a reflow state to tell our child to flow at the given size.

#ifdef DEBUG_REFLOW

    char* reflowReasonString;

    switch(reason) 
    {
        case eReflowReason_Initial:
          reflowReasonString = "initial";
          break;

        case eReflowReason_Resize:
          reflowReasonString = "resize";
          break;
        case eReflowReason_Dirty:
          reflowReasonString = "dirty";
          break;
        case eReflowReason_StyleChange:
          reflowReasonString = "stylechange";
          break;
        case eReflowReason_Incremental:
          reflowReasonString = "incremental";
          break;
        default:
          reflowReasonString = "unknown";
          break;
    }

    AddIndents();
    nsFrame::ListTag(stdout, childFrame);
    char ch[100];
    aReason.ToCString(ch,100);

    printf(" reason=%s %s",reflowReasonString,ch);
#endif


    nsHTMLReflowState   reflowState(aPresContext, aReflowState, mFrame, nsSize(size.width, NS_INTRINSICSIZE));
    reflowState.reason = reason;

    if (size.height != NS_INTRINSICSIZE) {
        size.height -= (border.top + border.bottom);
        NS_ASSERTION(size.height >= 0,"Error top bottom border too large");
    }

    if (size.width != NS_INTRINSICSIZE) {
        size.width -= (border.left + border.right);
        NS_ASSERTION(size.height >= 0,"Error left right border too large");
    }

    reflowState.mComputedWidth = size.width;
    reflowState.mComputedHeight = size.height;
#ifdef DEBUG_REFLOW
  printf(" Size=(%d,%d)\n",reflowState.mComputedWidth, reflowState.mComputedHeight);
#endif

  // place the child and reflow
    mFrame->WillReflow(aPresContext);

   // if (aMoveFrame) {
   //       PlaceChild(aPresContext, mFrame, aX + margin.left, aY + margin.top);
   // }

    // html doesn't like 0 sizes.
    if (reflowState.mComputedWidth == 0)
      reflowState.mComputedWidth = 1;
    if (reflowState.mComputedHeight == 0)
      reflowState.mComputedHeight = 1;

    mFrame->Reflow(aPresContext, aDesiredSize, reflowState, aStatus);

    NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

    nsFrameState  kidState;
    mFrame->GetFrameState(&kidState);

   // printf("width: %d, height: %d\n", aDesiredSize.mCombinedArea.width, aDesiredSize.mCombinedArea.height);

    // XXX EDV hack to fix bug in block
   //if (reflowState.reason != eReflowReason_Initial) {


    /*
        if (kidState & NS_FRAME_OUTSIDE_CHILDREN) {
             printf("OutsideChildren width=%d, height=%d\n", aDesiredSize.mOverflowArea.width, aDesiredSize.mOverflowArea.height);

             if (aDesiredSize.mOverflowArea.width > aWidth)
             {
                 reflowState.mComputedWidth = aDesiredSize.mOverflowArea.width - (border.left + border.right);
                 reflowState.availableWidth = reflowState.mComputedWidth;
                 //Reflow(aPresContext, aDesiredSize, aReflowState, aStatus, aX, aY, aDesiredSize.mOverflowArea.width, aHeight, aMoveFrame);
                 mFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
                 mFrame->WillReflow(aPresContext);
                 mFrame->Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
                 mFrame->GetFrameState(&kidState);
                 if (kidState & NS_FRAME_OUTSIDE_CHILDREN)
                    aDesiredSize.height = aDesiredSize.mOverflowArea.height;
             } else {
                aDesiredSize.height = aDesiredSize.mOverflowArea.height;
             }
        }
*/

        if (kidState & NS_FRAME_OUTSIDE_CHILDREN) {
             //printf("OutsideChildren width=%d, height=%d\n", aDesiredSize.mOverflowArea.width, aDesiredSize.mOverflowArea.height);
             aDesiredSize.width = aDesiredSize.mOverflowArea.width;
             if (aDesiredSize.width <= aWidth)
               aDesiredSize.height = aDesiredSize.mOverflowArea.height;
             else {
              if (aDesiredSize.width > aWidth)
              {
                 reflowState.mComputedWidth = aDesiredSize.width - (border.left + border.right);
                 reflowState.availableWidth = reflowState.mComputedWidth;
                 reflowState.reason = eReflowReason_Resize;
                 reflowState.reflowCommand = nsnull;
                 mFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
                 mFrame->WillReflow(aPresContext);
                 mFrame->Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
                 mFrame->GetFrameState(&kidState);
                 if (kidState & NS_FRAME_OUTSIDE_CHILDREN)
                    aDesiredSize.height = aDesiredSize.mOverflowArea.height;
              }
             }
        }
          

    // ok we need the max ascent of the items on the line. So to do this
    // ask the block for its line iterator. Get the max ascent.
    nsresult rv;
    nsCOMPtr<nsILineIterator> lines = do_QueryInterface(mFrame, &rv);
    if (NS_SUCCEEDED(rv) && lines) 
    {
      nsIFrame* firstFrame = nsnull;
      PRInt32 framesOnLine;
      nsRect lineBounds;
      PRUint32 lineFlags;
      lines->GetLine(0, &firstFrame, &framesOnLine, lineBounds, &lineFlags);

      if (firstFrame) {
        nsRect rect(0,0,0,0);
        firstFrame->GetRect(rect);
     
        const nsStyleFont* font;
        firstFrame->GetStyleData(eStyleStruct_Font,
                            (const nsStyleStruct*&) font);
        nsIRenderingContext& rc = *aReflowState.rendContext;
        rc.SetFont(font->mFont);
        nsIFontMetrics* fm;
        nsresult rv = rc.GetFontMetrics(fm);
        if (NS_SUCCEEDED(rv) && (nsnull != fm)) {
          fm->GetMaxAscent(aDesiredSize.ascent);
          NS_RELEASE(fm);
        }
        rv = NS_OK;
        aDesiredSize.ascent += rect.y;

         /*
         nsHTMLReflowMetrics d(nsnull);
         nsHTMLReflowState lineReflowState(aPresContext, reflowState, firstFrame, nsSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE));
         lineReflowState.mComputedWidth = rect.width - (lineReflowState.mComputedBorderPadding.left + lineReflowState.mComputedBorderPadding.right);
         lineReflowState.mComputedHeight = NS_INTRINSICSIZE;
         reason = eReflowReason_Resize;
         firstFrame->WillReflow(aPresContext);
         firstFrame->Reflow(aPresContext, d, lineReflowState, aStatus);
         aDesiredSize.ascent = d.ascent + rect.y + border.top;
         */
      }
    }

      // look at the first child
      /*
      const nsStyleFont* font;
      firstFrame->GetStyleData(eStyleStruct_Font,
                          (const nsStyleStruct*&) font);
      nsIRenderingContext& rc = *aReflowState.mReflowState.rendContext;
      rc.SetFont(font->mFont);
      nsIFontMetrics* fm;
      rv = rc.GetFontMetrics(fm);
      if (NS_SUCCEEDED(rv) && (nsnull != fm)) {
        fm->GetMaxAscent(aDesiredSize.ascent);
        NS_RELEASE(fm);
      }
      rv = NS_OK;
      

      // add top border and padding to the ascent
      nsRect rect(0,0,0,0);
      firstFrame->GetRect(&rect);

      // now our max ascent is acent + the y offset of the the first child
      aDesiredSize.ascent += rect.y;
    }
    */

    PRBool changedSize = PR_FALSE;

    if (mLastSize.width != aDesiredSize.width || mLastSize.height != aDesiredSize.height)
       changedSize = PR_TRUE;
  
    // if the child got bigger then make sure the new size in our min max range
    if (changedSize) {
    
      // redraw if we changed size.
      //aRedraw = PR_TRUE;
    }

    nsContainerFrame::FinishReflowChild(mFrame, aPresContext, aDesiredSize, aX, aY, NS_FRAME_NO_MOVE_FRAME);


  } else {
    /*
      if (aMoveFrame) {
           PlaceChild(aPresContext, mFrame, aX + margin.left, aY + margin.top);
      }
      */
  }    
  
  // add the margin back in. The child should add its border automatically
  //aDesiredSize.height += (margin.top + margin.bottom);
  //aDesiredSize.width += (margin.left + margin.right);

#ifdef DEBUG_REFLOW
  if (aHeight != NS_INTRINSICSIZE && aDesiredSize.height != aHeight)
  {
          AddIndents();
          printf("**** Child ");
          nsFrame::ListTag(stdout, childFrame);
          printf(" got taller!******\n");
         
  }
  if (aWidth != NS_INTRINSICSIZE && aDesiredSize.width != aWidth)
  {
          AddIndents();
          printf("**** Child ");
          nsFrame::ListTag(stdout, childFrame);
          printf(" got wider!******\n");
         
  }
#endif

  if (aWidth == NS_INTRINSICSIZE)
     aWidth = aDesiredSize.width;

  if (aHeight == NS_INTRINSICSIZE)
     aHeight = aDesiredSize.height;

  mLastSize.width = aDesiredSize.width;
  mLastSize.height = aDesiredSize.height;

  if (aDesiredSize.width > aWidth && mMinWidth != aDesiredSize.width) {
    mMinWidth = aDesiredSize.width;
    SizeNeedsRecalc(mMinSize);
  }
    
  return NS_OK;
}


void
nsBoxToBlockAdaptor::PlaceChild(nsIPresContext* aPresContext, nsIFrame* aFrame, nscoord aX, nscoord aY)
{
      nsPoint curOrigin;
      aFrame->GetOrigin(curOrigin);

      // only if the origin changed
    if ((curOrigin.x != aX) || (curOrigin.y != aY)) {
        aFrame->MoveTo(aPresContext, aX, aY);

        nsIView*  view;
        aFrame->GetView(aPresContext, &view);
        if (view) {
            nsContainerFrame::PositionFrameView(aPresContext, aFrame, view);
        } else
            nsContainerFrame::PositionChildViews(aPresContext, aFrame);
    }
}

/*
void
nsBoxToBlockAdaptor::BuildReflowChain(nsIFrame* aFrame, nsIFrame* aTargetParent, const nsHTMLReflowState& aRoot, nsHTMLReflowState& aState)
{
   nsIFrame* parent;
   aFrame->GetParent(parent);

   if (aFrame != aTargetParent)
   {
     BuildReflowChain(parent, aTargetFrame, aRoot, aState);
     nsHTMLReflowState state(aPresContext, aRoot, aFrame, nsSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE));
     aRoot = state;
   }
}
*/


PRBool
nsBoxToBlockAdaptor::GetWasCollapsed(nsBoxLayoutState& aState)
{
  return mWasCollapsed;
}

void
nsBoxToBlockAdaptor::SetWasCollapsed(nsBoxLayoutState& aState, PRBool aCollapsed)
{
  mWasCollapsed = aCollapsed;
}

NS_IMPL_ADDREF_INHERITED(nsBoxToBlockAdaptor, nsBox);
NS_IMPL_RELEASE_INHERITED(nsBoxToBlockAdaptor, nsBox);

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsBoxToBlockAdaptor)
  NS_INTERFACE_MAP_ENTRY(nsIBoxToBlockAdaptor)
NS_INTERFACE_MAP_END_INHERITING(nsBox)

