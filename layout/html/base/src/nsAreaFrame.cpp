/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsAreaFrame.h"
#include "nsBlockBandData.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsIView.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLValue.h"
#include "nsHTMLParts.h"
#include "nsLayoutAtoms.h"

#undef NOISY_MAX_ELEMENT_SIZE

static NS_DEFINE_IID(kAreaFrameIID, NS_IAREAFRAME_IID);

nsresult
NS_NewAreaFrame(nsIFrame** aNewFrame, PRUint32 aFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsAreaFrame* it = new nsAreaFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
}

nsAreaFrame::nsAreaFrame()
{
}

nsAreaFrame::~nsAreaFrame()
{
  NS_IF_RELEASE(mSpaceManager);
}

/////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMETHODIMP
nsAreaFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kAreaFrameIID)) {
    nsIAreaFrame* tmp = (nsIAreaFrame*)this;
    *aInstancePtr = (void*)tmp;
    return NS_OK;
  }
  return nsBlockFrame::QueryInterface(aIID, aInstancePtr);
}

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

NS_IMETHODIMP
nsAreaFrame::Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow)
{
  nsresult  rv;

  // Let the block frame do its initialization
  rv = nsBlockFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // Create a space manager if requested
  if (0 == (mFlags & NS_AREA_NO_SPACE_MGR)) {
    mSpaceManager = new nsSpaceManager(this);
    if (!mSpaceManager) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mSpaceManager);
  }

  return rv;
}

NS_IMETHODIMP
nsAreaFrame::Destroy(nsIPresContext& aPresContext)
{
  mAbsoluteContainer.DestroyFrames(this, aPresContext);
  return nsBlockFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsAreaFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.SetInitialChildList(this, aPresContext, aListName, aChildList);
  } else {
    rv = nsBlockFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  }

  return rv;
}

NS_IMETHODIMP
nsAreaFrame::AppendFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.AppendFrames(this, aPresContext, aPresShell, aListName,
                                         aFrameList);
  } else {
    rv = nsBlockFrame::AppendFrames(aPresContext, aPresShell, aListName,
                                    aFrameList);
  }

  return rv;
}

NS_IMETHODIMP
nsAreaFrame::InsertFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.InsertFrames(this, aPresContext, aPresShell, aListName,
                                         aPrevFrame, aFrameList);
  } else {
    rv = nsBlockFrame::InsertFrames(aPresContext, aPresShell, aListName,
                                    aPrevFrame, aFrameList);
  }

  return rv;
}

NS_IMETHODIMP
nsAreaFrame::RemoveFrame(nsIPresContext& aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.RemoveFrame(this, aPresContext, aPresShell, aListName, aOldFrame);
  } else {
    rv = nsBlockFrame::RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
  }

  return rv;
}

NS_IMETHODIMP
nsAreaFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const
{
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  if (aIndex <= NS_BLOCK_FRAME_LAST_LIST_INDEX) {
    return nsBlockFrame::GetAdditionalChildListName(aIndex, aListName);
  }
  
  *aListName = nsnull;
  if (NS_AREA_FRAME_ABSOLUTE_LIST_INDEX == aIndex) {
    *aListName = nsLayoutAtoms::absoluteList;
    NS_ADDREF(*aListName);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAreaFrame::FirstChild(nsIAtom* aListName, nsIFrame** aFirstChild) const
{
  NS_PRECONDITION(nsnull != aFirstChild, "null OUT parameter pointer");
  if (aListName == nsLayoutAtoms::absoluteList) {
    return mAbsoluteContainer.FirstChild(this, aListName, aFirstChild);
  }

  return nsBlockFrame::FirstChild(aListName, aFirstChild);
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsAreaFrame::Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer)
{
  // Note: all absolutely positioned elements have views so we don't
  // need to worry about painting them
  nsresult rv = nsBlockFrame::Paint(aPresContext, aRenderingContext,
                                    aDirtyRect, aWhichLayer);

  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    // Render the bands in the spacemanager
    nsISpaceManager* sm = mSpaceManager;

    if (nsnull != sm) {
      nsBlockBandData band;
      band.Init(sm, nsSize(mRect.width, mRect.height));
      nscoord y = 0;
      while (y < mRect.height) {
        nsRect availArea;
        band.GetAvailableSpace(y, availArea);
  
        // Render a box and a diagonal line through the band
        aRenderingContext.SetColor(NS_RGB(0,255,0));
        aRenderingContext.DrawRect(0, availArea.y,
                                   mRect.width, availArea.height);
        aRenderingContext.DrawLine(0, availArea.y,
                                   mRect.width, availArea.YMost());
  
        // Render boxes and opposite diagonal lines around the
        // unavailable parts of the band.
        PRInt32 i;
        for (i = 0; i < band.GetTrapezoidCount(); i++) {
          const nsBandTrapezoid* trapezoid = band.GetTrapezoid(i);
          if (nsBandTrapezoid::Available != trapezoid->mState) {
            nsRect r;
            trapezoid->GetRect(r);
            if (nsBandTrapezoid::OccupiedMultiple == trapezoid->mState) {
              aRenderingContext.SetColor(NS_RGB(0,255,128));
            }
            else {
              aRenderingContext.SetColor(NS_RGB(128,255,0));
            }
            aRenderingContext.DrawRect(r);
            aRenderingContext.DrawLine(r.x, r.YMost(), r.XMost(), r.y);
          }
        }
        y = availArea.YMost();
      }
    }
  }

  return rv;
}
#endif

// Return the x-most and y-most for the child absolutely positioned
// elements
NS_IMETHODIMP
nsAreaFrame::GetPositionedInfo(nscoord& aXMost, nscoord& aYMost) const
{
  nsresult  rv = mAbsoluteContainer.GetPositionedInfo(this, aXMost, aYMost);

  // If we have child frames that stick outside of our box, and they should
  // be visible, then include them too so the total size is correct
  if (mState & NS_FRAME_OUTSIDE_CHILDREN) {
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);

    if (NS_STYLE_OVERFLOW_VISIBLE == display->mOverflow) {
      if (mCombinedArea.XMost() > aXMost) {
        aXMost = mCombinedArea.XMost();
      }
      if (mCombinedArea.YMost() > aYMost) {
        aYMost = mCombinedArea.YMost();
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsAreaFrame::Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsAreaFrame::Reflow: maxSize=%d,%d reason=%d",
                  aReflowState.availableWidth,
                  aReflowState.availableHeight,
                  aReflowState.reason));

  nsresult  rv = NS_OK;

  // See if it's an incremental reflow command
  if (eReflowReason_Incremental == aReflowState.reason) {
    // Give the absolute positioning code a chance to handle it
    PRBool  handled;
    
    mAbsoluteContainer.IncrementalReflow(this, aPresContext, aReflowState, handled);

    // If the incremental reflow command was handled by the absolute positioning
    // code, then we're all done
    if (handled) {
      // Just return our current size as our desired size
      aDesiredSize.width = mRect.width;
      aDesiredSize.height = mRect.height;
      aDesiredSize.ascent = mRect.height;
      aDesiredSize.descent = 0;
  
      // Whether or not we're complete hasn't changed
      aStatus = (nsnull != mNextInFlow) ? NS_FRAME_NOT_COMPLETE : NS_FRAME_COMPLETE;
      return rv;
    }
  }

  // If we have a space manager, then set it in the reflow state
  if (nsnull != mSpaceManager) {
    // Modify the existing reflow state
    nsHTMLReflowState&  reflowState = (nsHTMLReflowState&)aReflowState;
    reflowState.mSpaceManager = mSpaceManager;

    // Clear the spacemanager's regions.
    mSpaceManager->ClearRegions();
  }

  // Let the block frame do its reflow first
  rv = nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // Let the absolutely positioned container reflow any absolutely positioned
  // child frames that need to be reflowed, e.g., elements with a percentage
  // based width/height
  if (NS_SUCCEEDED(rv)) {
    rv = mAbsoluteContainer.Reflow(this, aPresContext, aReflowState);
  }

  if (mFlags & NS_AREA_WRAP_SIZE) {
    // When the area frame is supposed to wrap around all in-flow
    // children, make sure its big enough to include those that stick
    // outside the box.
    if (NS_FRAME_OUTSIDE_CHILDREN & mState) {
      nscoord xMost = aDesiredSize.mCombinedArea.XMost();
      if (xMost > aDesiredSize.width) {
        aDesiredSize.width = xMost;
      }
      nscoord yMost = aDesiredSize.mCombinedArea.YMost();
      if (yMost > aDesiredSize.height) {
        aDesiredSize.height = yMost;
      }
    }
  }

#ifdef NOISY_MAX_ELEMENT_SIZE
  ListTag(stdout);
  printf(": maxElementSize=%d,%d desiredSize=%d,%d\n",
         aDesiredSize.maxElementSize ? aDesiredSize.maxElementSize->width : 0,
         aDesiredSize.maxElementSize ? aDesiredSize.maxElementSize->height : 0,
         aDesiredSize.width, aDesiredSize.height);
#endif

  // If we have children that stick outside our box, then remember the
  // combined area, because we'll need it later when sizing our view
  if (mState & NS_FRAME_OUTSIDE_CHILDREN) {
    mCombinedArea = aDesiredSize.mCombinedArea;
  }

  return rv;
}

NS_IMETHODIMP
nsAreaFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::areaFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
nsAreaFrame::DidReflow(nsIPresContext& aPresContext,
                       nsDidReflowStatus aStatus)
{
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    // If we should position our view, and we have child frames that stick
    // outside our box, then we need to size our view large enough to include
    // those child frames
    if ((mState & NS_FRAME_SYNC_FRAME_AND_VIEW) &&
        (mState & NS_FRAME_OUTSIDE_CHILDREN)) {
      
      nsIView*              view;
      const nsStyleDisplay* display = (const nsStyleDisplay*)
        mStyleContext->GetStyleData(eStyleStruct_Display);

      GetView(&view);
      if (view && (NS_STYLE_OVERFLOW_VISIBLE == display->mOverflow)) {
        // Don't let our base class position the view since we're doing it
        mState &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;
  
        // Set the view's bit that indicates that it has transparent content
        nsIViewManager* vm;
        
        view->GetViewManager(vm);
        vm->SetViewContentTransparency(view, PR_TRUE);
      
        // Position and size view relative to its parent, not relative to our
        // parent frame (our parent frame may not have a view).
        nsIView*  parentWithView;
        nsPoint   origin;

        // XXX We need to handle the case where child frames stick out on the
        // left and top edges as well...
        GetOffsetFromView(origin, &parentWithView);
        vm->ResizeView(view, mCombinedArea.XMost(), mCombinedArea.YMost());
        vm->MoveViewTo(view, origin.x, origin.y);
        NS_RELEASE(vm);
  
        // Call our base class
        nsresult  rv = nsBlockFrame::DidReflow(aPresContext, aStatus);
  
        // Set the flag again...
        mState |= NS_FRAME_SYNC_FRAME_AND_VIEW;
        return rv;
      }
    }
  }

  return nsBlockFrame::DidReflow(aPresContext, aStatus);
}

/////////////////////////////////////////////////////////////////////////////
// Diagnostics

NS_IMETHODIMP
nsAreaFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Area", aResult);
}

#ifdef DEBUG
NS_IMETHODIMP
nsAreaFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsBlockFrame::SizeOf(aHandler, aResult);
  *aResult += sizeof(*this) - sizeof(nsBlockFrame);
  return NS_OK;
}
#endif
