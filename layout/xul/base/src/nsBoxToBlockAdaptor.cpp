/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsFrameNavigator.h"
#include "nsCSSRendering.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsBoxToBlockAdaptor.h"
#include "nsILineIterator.h"
#include "nsIFontMetrics.h"
#include "nsHTMLContainerFrame.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);

//#define DEBUG_REFLOW
//#define DEBUG_GROW


#ifdef DEBUG_REFLOW
PRInt32 gIndent2 = 0;

void
nsAdaptorAddIndents()
{
    for(PRInt32 i=0; i < gIndent2; i++)
    {
        printf(" ");
    }
}

void
nsAdaptorPrintReason(nsHTMLReflowState& aReflowState)
{
    char* reflowReasonString;

    switch(aReflowState.reason) 
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
        {
           nsIReflowCommand::ReflowType  type;
            aReflowState.reflowCommand->GetType(type);
            switch (type) {
              case nsIReflowCommand::StyleChanged:
                 reflowReasonString = "incremental (StyleChanged)";
              break;
              case nsIReflowCommand::ReflowDirty:
                 reflowReasonString = "incremental (ReflowDirty)";
              break;
              default:
                 reflowReasonString = "incremental (Unknown)";
            }
        }                             
        break;
        default:
          reflowReasonString = "unknown";
          break;
    }

    printf("%s",reflowReasonString);
}

#endif

nsBoxToBlockAdaptor::nsBoxToBlockAdaptor(nsIPresShell* aPresShell, nsIFrame* aFrame):nsBox(aPresShell)
{
  mSizeSet = PR_FALSE;
  mFrame = aFrame;
  mSpaceManager = nsnull;
  mWasCollapsed = PR_FALSE;
  mCachedMaxElementHeight = 0;
  mStyleChange = PR_FALSE;
  mOverflow.width = 0;
  mOverflow.height = 0;
  mIncludeOverflow = PR_TRUE;
  mPresShell = aPresShell;
  NeedsRecalc();
}

NS_IMETHODIMP
nsBoxToBlockAdaptor::SetParentBox(nsIBox* aParent)
{
  nsresult rv = nsBox::SetParentBox(aParent);

  nsIBox* parent = aParent;

  
  if (parent) {
    PRBool needsWidget = PR_FALSE;
    parent->ChildrenMustHaveWidgets(needsWidget);
    if (needsWidget) {
        nsCOMPtr<nsIPresContext> context;
        mPresShell->GetPresContext(getter_AddRefs(context));
        nsIView* view = nsnull;
        mFrame->GetView(context, &view);

        if (!view) {
           nsCOMPtr<nsIStyleContext> style;
           mFrame->GetStyleContext(getter_AddRefs(style));
           nsHTMLContainerFrame::CreateViewForFrame(context,mFrame,style,nsnull,PR_TRUE); 
           mFrame->GetView(context, &view);
        }

        nsCOMPtr<nsIWidget> widget;
        view->GetWidget(*getter_AddRefs(widget));

        if (!widget)
           view->CreateWidget(kWidgetCID);   
    }
  }
  

  return rv;
}

PRBool
nsBoxToBlockAdaptor::HasStyleChange()
{
  return mStyleChange;
}

void
nsBoxToBlockAdaptor::GetBoxName(nsAutoString& aName)
{
#ifdef DEBUG
   nsIFrameDebug*  frameDebug;
   nsAutoString name;
   if (NS_SUCCEEDED(mFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->GetFrameName(name);
   }

  aName = name;
#endif
}

void
nsBoxToBlockAdaptor::SetStyleChangeFlag(PRBool aDirty)
{
  nsBox::SetStyleChangeFlag(aDirty);
  mStyleChange = PR_TRUE;
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
  delete mSpaceManager;
}


NS_IMETHODIMP
nsBoxToBlockAdaptor::NeedsRecalc()
{
  mMinWidth = -1;
  mPrefNeedsRecalc = PR_TRUE;
  SizeNeedsRecalc(mMinSize);
  SizeNeedsRecalc(mMaxSize);
  CoordNeedsRecalc(mFlex);
  CoordNeedsRecalc(mAscent);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxToBlockAdaptor::GetOverflow(nsSize& aOverflow)
{
  aOverflow = mOverflow;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxToBlockAdaptor::SetIncludeOverflow(PRBool aInclude)
{
  mIncludeOverflow = aInclude;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxToBlockAdaptor::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  if (!mPrefNeedsRecalc) {
     aSize = mPrefSize;
     return NS_OK;
  }

  // if we are collapsed its easy our size on 0,0
  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed) {
    mPrefSize.width = 0;
    mPrefSize.height = 0;
    mPrefNeedsRecalc = PR_FALSE;
    return NS_OK;
  }

  nsresult rv = NS_OK;

  // see if anything is defined in css. If pref size is competely
  // defined in css we are golden. We do nothing. But is its not
  // in css we will have to reflow the child.
  
  PRBool completelyRedefined = nsIBox::AddCSSPrefSize(aState, this, mPrefSize);

  if (!completelyRedefined) {

     // ok we are forced to compute the preferred size. This sucks. Why? well because you can't just ask 
     // a html frame for its preferred size. And what is the definition of preferred size of an html block then?
     // is the width and height of the block if it were given an infinite canvas to flow itself into. We can compute
     // this by reflowing it with a computed size of (NS_INTRINSICSIZE, NS_INTRINSICSIZE).
     //
     // then the desired size that comes back is our pref size. But there are some problems we need to handle:
     // 1) If we flow the child at its intrinsic size it is going to actually move all the children all onto a single
     //    most likely. So here is the problem. Say you have div that contains a large sentence. And in css its width
     //    was set to be 100px. So we reflowed with a computed size of 100px wide and set its rect and text now
     //    is 100px wide and take up 3 lines of height. Well if we need to get its pref size then we have to 
     //    flow it intrinsically. That would give it infinite space and it would all be in 1 line and be say 500px wide.
     //    That would mean we need to flow it back to it original size after asking it for its preferred.
     //
     // 2) Block rely on max element size. So we need to compute it if it is asked for.
 
     // if we do have a reflow state
     const nsHTMLReflowState* reflowState = aState.GetReflowState();
     if (reflowState) {
       nsIPresContext* presContext = aState.GetPresContext();
       nsReflowStatus status = NS_FRAME_COMPLETE;
       nsHTMLReflowMetrics desiredSize(nsnull);

       // now are we dirty? When we reflow to get the preferred size that will undirty it. We need to set it 
       // back to hold onto it here.
       PRBool isDirty = PR_FALSE;
       IsDirty(isDirty);

       // remember item 1? Well we need to hold onto the old size so we can set it back if it changes.
       nsRect oldRect(0,0,0,0);
       if (mSizeSet)
          GetBounds(oldRect);

       // item 2. We need to handle max element size. If get were aske to get it then 
       // create a size and pass it down.
       nsSize* currentSize = nsnull;
       aState.GetMaxElementSize(&currentSize);
       nsSize size(0,0);

       if (currentSize) {
        desiredSize.maxElementSize = &size;
       }

       // reflow our mFrame. Notice we are doing it intrinsically to get the pref size.
       rv = Reflow(aState,
                   presContext, 
                  desiredSize, 
                  *reflowState, 
                  status,
                  0,
                  0,
                  NS_INTRINSICSIZE,
                  NS_INTRINSICSIZE,
                  PR_FALSE);

     // set the max element size
     if (currentSize) {
        desiredSize.maxElementSize = nsnull;

        if (size.width > currentSize->width)
          currentSize->width = size.width;

        if (size.height > currentSize->height)
           currentSize->height = size.height;

        mCachedMaxElementHeight = size.height;
      }

     // item 1. Someone may have already set the size. If so then set it back.
     if (mSizeSet) {
          
          SetBounds(aState, oldRect);

          // mark it dirty so it will be layed out ok. 
          nsFrameState frameState = 0;
          mFrame->GetFrameState(&frameState);
          frameState |= NS_FRAME_IS_DIRTY;
          mFrame->SetFrameState(frameState);

          Layout(aState);
     }

     // make sure if we were originally dirty we put it back. Reflowing would have undone this flag.
     if (isDirty) {
       nsFrameState frameState = 0;
       mFrame->GetFrameState(&frameState);
       frameState |= NS_FRAME_IS_DIRTY;
       mFrame->SetFrameState(frameState);
     }

     // store our preferred size.
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

  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed) {
    mMinSize.width = 0;
    mMinSize.height = 0;
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

  aAscent = 0;

  return NS_OK;

  /*
  nsSize size;
  GetPrefSize(aState, size);

  aAscent = mAscent;
  */
  return NS_OK;
}

NS_IMETHODIMP
nsBoxToBlockAdaptor::IsCollapsed(nsBoxLayoutState& aState, PRBool& aCollapsed)
{
  return nsBox::IsCollapsed(aState, aCollapsed);
}

nsresult
nsBoxToBlockAdaptor::DoLayout(nsBoxLayoutState& aState)
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

    nscoord origWidth = ourRect.width;

     rv = Reflow(aState,
                 presContext, 
                  desiredSize, 
                  *reflowState, 
                  status,
                  ourRect.x,
                  ourRect.y,
                  ourRect.width,
                  ourRect.height);

    if (desiredSize.width > origWidth && mMinWidth != desiredSize.width) {
      mMinWidth = desiredSize.width;
      SizeNeedsRecalc(mMinSize);
    }
     
    if (currentSize) {
      desiredSize.maxElementSize = nsnull;

      if (size.width > currentSize->width)
         currentSize->width = size.width;
  
      if (mCachedMaxElementHeight > currentSize->height) {
        currentSize->height = mCachedMaxElementHeight;
      }
    }

     PRBool collapsed = PR_FALSE;
     IsCollapsed(aState, collapsed);
     if (collapsed) {
       mAscent = 0;
       mFrame->SizeTo(presContext, 0, 0);
     } else {
       mAscent = desiredSize.ascent;

       // if our child needs to be bigger. This might happend with wrapping text. There is no way to predict its
       // height until we reflow it. Now that we know the height reshuffle upward.
       if (desiredSize.width > ourRect.width || desiredSize.height > ourRect.height) {

#ifdef DEBUG_GROW
            DumpBox(stdout);
            printf(" GREW from (%d,%d) -> (%d,%d)\n", ourRect.width, ourRect.height, desiredSize.width, desiredSize.height);
#endif

         if (desiredSize.width > ourRect.width)
            ourRect.width = desiredSize.width;

         if (desiredSize.height > ourRect.height)
            ourRect.height = desiredSize.height;
       }

       // ensure our size is what we think is should be. Someone could have
       // reset the frame to be smaller or something dumb like that. 
       mFrame->SizeTo(presContext, ourRect.width, ourRect.height);
     }
   }

   SyncLayout(aState);

   mSizeSet = PR_TRUE;

   return rv;
}

nsresult
nsBoxToBlockAdaptor::Reflow(nsBoxLayoutState& aState,
                     nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     nscoord aX,
                     nscoord aY,
                     nscoord aWidth,
                     nscoord aHeight,
                     PRBool aMoveFrame)
{
  DO_GLOBAL_REFLOW_COUNT("nsBoxToBlockAdaptor", aReflowState.reason);

#ifdef DEBUG_REFLOW
  nsAdaptorAddIndents();
  printf("Reflowing: ");
  nsFrame::ListTag(stdout, mFrame);
  printf("\n");
  gIndent2++;
#endif

  //printf("width=%d, height=%d\n", aWidth, aHeight);
  nsIBox* parent;
  GetParentBox(&parent);
  nsIFrame* frame;
  parent->GetFrame(&frame);
  nsFrameState frameState = 0;
  frame->GetFrameState(&frameState);

 // if (frameState & NS_STATE_CURRENTLY_IN_DEBUG)
  //   printf("In debug\n");
  
  if (!mSpaceManager) {
      nsCOMPtr<nsIPresShell> shell;
      aPresContext->GetShell(getter_AddRefs(shell));
      mSpaceManager = new nsSpaceManager(shell, mFrame);
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
              Reflow(aState, aPresContext, aDesiredSize, state, aStatus, aX, aY, aWidth, aHeight, aMoveFrame);
          } else {
              // convert to initial if not incremental.
              reason = eReflowReason_Initial;
          }

      }
  } else if (reason == eReflowReason_Initial) {
      reason = eReflowReason_Resize;
  }

  PRBool redrawAfterReflow = PR_FALSE;

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
      
      // if the increment child is out child then
      // pop it off and continue sending it down
      if (incrementalChild == mFrame ) {
          needsReflow = PR_TRUE;

          //nsIFrame* targetFrame = nsnull;
          //aReflowState.reflowCommand->GetTarget(targetFrame);

          //if (targetFrame == mFrame) 
          //  aState.mChainUnwound = PR_TRUE;

          aReflowState.reflowCommand->GetNext(incrementalChild);

          // if we hit the target then we have used up the chain.
          // next time a layout 
          break;
      } else if (incrementalChild == nsnull) {
         
          // if there are not more incremental children meaning we have completely
          // unwound the chain then see if the target of the chain then see if the target
          // was us. If it is then it should become a resize.
          nsIFrame* targetFrame = nsnull;
          aReflowState.reflowCommand->GetTarget(targetFrame);

          /*
          if (targetFrame == mFrame) {
              reason = eReflowReason_Resize;
              needsReflow = PR_TRUE;
          }
          */    
      }
      // fall into dirty if the incremental child was use. It should be treated as a 
   }

   // if its dirty then see if the child we want to reflow is dirty. If it is then
   // mark it as needing to be reflowed.
   case eReflowReason_Dirty: {
        // XXX nsBlockFrames don't seem to be able to handle a reason of Dirty. So we  
        // send down a resize instead. If we did send down the dirty we would have wrapping problems. If you 
        // look at the main page it will initially come up ok but will have a unneeded horizontal 
        // scrollbar if you resize it will fix it self. The real fix is to fix block frame but
        // this will fix it for beta3.
        reason = eReflowReason_Resize;

        // get the frame state to see if it needs reflow
        needsReflow = mStyleChange || (childState & NS_FRAME_IS_DIRTY) || (childState & NS_FRAME_HAS_DIRTY_CHILDREN);

        // but of course by definition dirty reflows are supposed to redraw so
        // lets signal that we need to do that. We want to do it after as well because
        // the object may have changed size.
        if (needsReflow) {
           Redraw(aState);
           redrawAfterReflow = PR_TRUE;
           //printf("Redrawing!!!/n");
        }

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
                             
  // ok now reflow the child into the spacers calculated space
  if (needsReflow) {

    nsMargin border(0,0,0,0);
    GetBorderAndPadding(border);


    aDesiredSize.width = 0;
    aDesiredSize.height = 0;

    nsSize size(aWidth, aHeight);

    // create a reflow state to tell our child to flow at the given size.
    if (size.height != NS_INTRINSICSIZE) {
        size.height -= (border.top + border.bottom);
        if (size.height < 0)
          size.height = 0;
    }

    if (size.width != NS_INTRINSICSIZE) {
        size.width -= (border.left + border.right);
        if (size.width < 0)
          size.width = 0;
    }

    nsHTMLReflowState   reflowState(aPresContext, aReflowState, mFrame, nsSize(size.width, NS_INTRINSICSIZE));
    reflowState.reason = reason;
    if (reason != eReflowReason_Incremental)
       reflowState.reflowCommand = nsnull;

    // XXX this needs to subtract out the border and padding of mFrame since it is content size
    reflowState.mComputedWidth = size.width;
    reflowState.mComputedHeight = size.height;

    // if we were marked for style change.
    // 1) see if we are just supposed to do a resize if so convert to a style change. Kill 2 birds
    //    with 1 stone.
    // 2) If the command is incremental. See if its style change. If it is everything is ok if not
    //    we need to do a second reflow with the style change.
    if (mStyleChange) {
      if (reflowState.reason == eReflowReason_Resize) 
         reflowState.reason = eReflowReason_StyleChange;
      else if (reason == eReflowReason_Incremental) {
         nsIReflowCommand::ReflowType  type;
          reflowState.reflowCommand->GetType(type);

          if (type != nsIReflowCommand::StyleChanged) {
             #ifdef DEBUG_REFLOW
                nsAdaptorAddIndents();
                printf("Size=(%d,%d)\n",reflowState.mComputedWidth, reflowState.mComputedHeight);
                nsAdaptorAddIndents();
                nsAdaptorPrintReason(reflowState);
                printf("\n");
             #endif

             mFrame->WillReflow(aPresContext);
             mFrame->Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
             mFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
             reflowState.mComputedWidth = aDesiredSize.width - (border.left + border.right);
             reflowState.availableWidth = reflowState.mComputedWidth;
             reflowState.reason = eReflowReason_StyleChange;
             reflowState.reflowCommand = nsnull;
          }
      }

      mStyleChange = PR_FALSE;
    }

    #ifdef DEBUG_REFLOW
      nsAdaptorAddIndents();
      printf("Size=(%d,%d)\n",reflowState.mComputedWidth, reflowState.mComputedHeight);
      nsAdaptorAddIndents();
      nsAdaptorPrintReason(reflowState);
      printf("\n");
    #endif

       // place the child and reflow
    mFrame->WillReflow(aPresContext);

    mFrame->Reflow(aPresContext, aDesiredSize, reflowState, aStatus);

    NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

    nsFrameState  kidState;
    mFrame->GetFrameState(&kidState);

   // printf("width: %d, height: %d\n", aDesiredSize.mCombinedArea.width, aDesiredSize.mCombinedArea.height);

    // see if the overflow option is set. If it is then if our child's bounds overflow then
    // we will set the child's rect to include the overflow size.
       if (kidState & NS_FRAME_OUTSIDE_CHILDREN) {
         // make sure we store the overflow size
         mOverflow.width  = aDesiredSize.mOverflowArea.width;
         mOverflow.height = aDesiredSize.mOverflowArea.height;

         // include the overflow size in our child's rect?
         if (mIncludeOverflow) {
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
                 #ifdef DEBUG_REFLOW
                  nsAdaptorAddIndents();
                  nsAdaptorPrintReason(reflowState);
                  printf("\n");
                 #endif
                 mFrame->WillReflow(aPresContext);
                 mFrame->Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
                 mFrame->GetFrameState(&kidState);
                 if (kidState & NS_FRAME_OUTSIDE_CHILDREN)
                    aDesiredSize.height = aDesiredSize.mOverflowArea.height;

              }
             }
         }
       } else {
         mOverflow.width  = aDesiredSize.width;
         mOverflow.height = aDesiredSize.height;
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
        rv = rc.GetFontMetrics(fm);
        if (NS_SUCCEEDED(rv) && (nsnull != fm)) {
          fm->GetMaxAscent(aDesiredSize.ascent);
          NS_RELEASE(fm);
        }
        rv = NS_OK;
        aDesiredSize.ascent += rect.y;

      }
    }

    if (redrawAfterReflow) {
       frame = nsnull;
       GetFrame(&frame);
       nsRect r;
       frame->GetRect(r);
       r.width = aDesiredSize.width;
       r.height = aDesiredSize.height;
       Redraw(aState, &r);
    }

    PRBool changedSize = PR_FALSE;

    if (mLastSize.width != aDesiredSize.width || mLastSize.height != aDesiredSize.height)
       changedSize = PR_TRUE;
  
    nsContainerFrame::FinishReflowChild(mFrame, aPresContext, aDesiredSize, aX, aY, NS_FRAME_NO_MOVE_FRAME);
  }    
  
#ifdef DEBUG_REFLOW
  if (aHeight != NS_INTRINSICSIZE && aDesiredSize.height != aHeight)
  {
          nsAdaptorAddIndents();
          printf("*****got taller!*****\n");
         
  }
  if (aWidth != NS_INTRINSICSIZE && aDesiredSize.width != aWidth)
  {
          nsAdaptorAddIndents();
          printf("*****got wider!******\n");
         
  }
#endif

  if (aWidth == NS_INTRINSICSIZE)
     aWidth = aDesiredSize.width;

  if (aHeight == NS_INTRINSICSIZE)
     aHeight = aDesiredSize.height;

  mLastSize.width = aDesiredSize.width;
  mLastSize.height = aDesiredSize.height;

#ifdef DEBUG_REFLOW
  gIndent2--;
#endif

  return NS_OK;
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
  if (NS_SUCCEEDED(mFrame->QueryInterface(aIID, aInstancePtr)))                             
    return NS_OK;                 
  else
NS_INTERFACE_MAP_END_INHERITING(nsBox)

