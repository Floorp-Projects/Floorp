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

static NS_DEFINE_IID(kAreaFrameIID, NS_IAREAFRAME_IID);

nsresult
NS_NewAreaFrame(nsIFrame*& aResult, PRUint32 aFlags)
{
  nsAreaFrame* it = new nsAreaFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetFlags(aFlags);
  aResult = it;
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
    NS_ADDREF(mSpaceManager);
  }

  return rv;
}

NS_IMETHODIMP
nsAreaFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  mAbsoluteContainer.DeleteFrames(aPresContext);
  return nsBlockFrame::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
nsAreaFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.SetInitialChildList(aPresContext, aListName, aChildList);
  } else {
    rv = nsBlockFrame::SetInitialChildList(aPresContext, aListName, aChildList);
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
    return mAbsoluteContainer.FirstChild(aListName, aFirstChild);
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
          if (nsBandTrapezoid::Available != trapezoid->state) {
            nsRect r;
            trapezoid->GetRect(r);
            if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
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
  return mAbsoluteContainer.GetPositionedInfo(aXMost, aYMost);
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
    
    mAbsoluteContainer.IncrementalReflow(aPresContext, aReflowState, handled);

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

  // If we have one then set the space manager
  if (nsnull != mSpaceManager) {
    // Modify the reflow state and set the space manager
    nsHTMLReflowState&  reflowState = (nsHTMLReflowState&)aReflowState;
    reflowState.spaceManager = mSpaceManager;

    // Clear the spacemanager's regions.
    mSpaceManager->ClearRegions();
  }

  // Let the block frame do its reflow first
  rv = nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // Let the absolutely positioned container reflow any absolutely positioned
  // child frames that need to be reflowed
  if (NS_SUCCEEDED(rv)) {
    rv = mAbsoluteContainer.Reflow(aPresContext, aReflowState);
  }

  // Compute our desired size taking into account floaters that stick outside
  // our box. Note that if this frame has a height specified by CSS then we
  // don't do this
  if ((NS_UNCONSTRAINEDSIZE == aReflowState.computedHeight) &&
      (NS_FRAME_OUTSIDE_CHILDREN & mState)) {
    nscoord contentYMost = aDesiredSize.height;
    nscoord yMost = aDesiredSize.mCombinedArea.YMost();
    if (yMost > contentYMost) {
      // retain the border+padding for this element after the bottom
      // most object.
      aDesiredSize.height = yMost;
    }
  }

  // XXX This code is really temporary; the lower level frame
  // classes need to contribute to the area that needs damage
  // repair. This class should only worry about damage repairing
  // it's border+padding area.
  nsRect  damageArea(0, 0, 0, 0);

  // Decide how much to repaint based on the reflow type.
  // Note: we don't have to handle the initial reflow case and the
  // resize reflow case, because they're handled by the scroll frame
  if (eReflowReason_Incremental == aReflowState.reason) {
    nsIReflowCommand::ReflowType  reflowType;
    aReflowState.reflowCommand->GetType(reflowType);

    // For append reflow commands that target the flowed frames just
    // repaint the newly added part of the frame.
    if (nsIReflowCommand::FrameAppended == reflowType) {
      //this blows. we're repainting everything, but we have no choice
      //since we don't know how we got here. see the XXX above for a
      //real fix.
#if 1
      damageArea.y = 0;
      damageArea.height = aDesiredSize.height;
      damageArea.width = aDesiredSize.width;
#else
      // It's an append reflow command
      damageArea.y = mRect.YMost();
      damageArea.width = aDesiredSize.width;
      damageArea.height = aDesiredSize.height - mRect.height;
      if ((damageArea.height < 0) ||
          (aDesiredSize.height == mRect.height)) {
        // Since we don't know what changed, assume it all changed.
        damageArea.y = 0;
        damageArea.height = aDesiredSize.height;
      }
#endif
    } else {
      // Ideally the frame that is the target of the reflow command
      // (or its parent frame) would generate a damage rect, but
      // since none of the frame classes know how to do this then
      // for the time being just repaint the entire frame
      damageArea.width = aDesiredSize.width;
      // If the new height is smaller than the old height then make
      // sure we erase whatever used to be displayed
      damageArea.height = PR_MAX(aDesiredSize.height, mRect.height);
    }
  }

  // If this is really the body, force a repaint of the damage area
  if ((NS_BLOCK_DOCUMENT_ROOT & mFlags) && !damageArea.IsEmpty()) {
    Invalidate(damageArea);
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

/////////////////////////////////////////////////////////////////////////////
// Diagnostics

NS_IMETHODIMP
nsAreaFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Area", aResult);
}
