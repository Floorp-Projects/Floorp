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
  mState &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;  // let us handle it
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
  mAbsoluteFrames.DeleteFrames(aPresContext);
  return nsBlockFrame::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
nsAreaFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    mAbsoluteFrames.SetFrames(aChildList);
    rv = NS_OK;
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
    *aFirstChild = mAbsoluteFrames.FirstChild();
    return NS_OK;
  }

  return nsBlockFrame::FirstChild(aListName, aFirstChild);
}

#ifdef NS_DEBUG
void
nsAreaFrame::BandData::ComputeAvailSpaceRect()
{
  nsBandTrapezoid*  trapezoid = data;

  if (count > 1) {
    // If there's more than one trapezoid that means there are floaters
    PRInt32 i;

    // Stop when we get to space occupied by a right floater, or when we've
    // looked at every trapezoid and none are right floaters
    for (i = 0; i < count; i++) {
      nsBandTrapezoid*  trapezoid = &data[i];
      if (trapezoid->state != nsBandTrapezoid::Available) {
        const nsStyleDisplay* display;
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 j, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (j = 0; j < numFrames; j++) {
            nsIFrame* f = (nsIFrame*)trapezoid->frames->ElementAt(j);
            f->GetStyleData(eStyleStruct_Display,
                            (const nsStyleStruct*&)display);
            if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
              goto foundRightFloater;
            }
          }
        } else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                         (const nsStyleStruct*&)display);
          if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
            break;
          }
        }
      }
    }
  foundRightFloater:

    if (i > 0) {
      trapezoid = &data[i - 1];
    }
  }

  if (nsBandTrapezoid::Available == trapezoid->state) {
    // The trapezoid is available
    trapezoid->GetRect(availSpace);
  } else {
    const nsStyleDisplay* display;

    // The trapezoid is occupied. That means there's no available space
    trapezoid->GetRect(availSpace);

    // XXX Better handle the case of multiple frames
    if (nsBandTrapezoid::Occupied == trapezoid->state) {
      trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                     (const nsStyleStruct*&)display);
      if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
        availSpace.x = availSpace.XMost();
      }
    }
    availSpace.width = 0;
  }
}

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

  if ((eFramePaintLayer_Overlay == aWhichLayer) && nsIFrame::GetShowFrameBorders()) {
    // Render the bands in the spacemanager
    nsISpaceManager* sm = mSpaceManager;

    if (nsnull != sm) {
      BandData band;
      nscoord y = 0;
      while (y < mRect.height) {
        sm->GetBandData(y, nsSize(mRect.width, mRect.height - y), band);
        band.ComputeAvailSpaceRect();
  
        // Render a box and a diagonal line through the band
        aRenderingContext.SetColor(NS_RGB(0,255,0));
        aRenderingContext.DrawRect(0, band.availSpace.y,
                                   mRect.width, band.availSpace.height);
        aRenderingContext.DrawLine(0, band.availSpace.y,
                                   mRect.width, band.availSpace.YMost());
  
        // Render boxes and opposite diagonal lines around the
        // unavailable parts of the band.
        PRInt32 i;
        for (i = 0; i < band.count; i++) {
          nsBandTrapezoid* trapezoid = &band.data[i];
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
        y = band.availSpace.YMost();
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
  aXMost = aYMost = 0;
  for (nsIFrame* f = mAbsoluteFrames.FirstChild(); nsnull != f; f->GetNextSibling(&f)) {
    // Get the frame's x-most and y-most. This is for its flowed content only
    nsRect  rect;
    f->GetRect(rect);

    if (rect.XMost() > aXMost) {
      aXMost = rect.XMost();
    }
    if (rect.YMost() > aYMost) {
      aYMost = rect.YMost();
    }

    // If the child frame is also an area frame, then take into account its child
    // absolutely positioned elements
    nsIAreaFrame* areaFrame;
    if (NS_SUCCEEDED(f->QueryInterface(kAreaFrameIID, (void**)&areaFrame))) {
      nscoord xMost, yMost;

      areaFrame->GetPositionedInfo(xMost, yMost);
      // Convert to our coordinate space
      xMost += rect.x;
      yMost += rect.y;

      if (xMost > aXMost) {
        aXMost = xMost;
      }
      if (yMost > aYMost) {
        aYMost = yMost;
      }
    }
  }

  return NS_OK;
}

// XXX Temporary until the view code allows positioned child views to extend
// outside their parent without being clipped...
NS_IMETHODIMP
nsAreaFrame::DidReflow(nsIPresContext&   aPresContext,
                       nsDidReflowStatus aStatus)
{
  nsresult  rv = nsBlockFrame::DidReflow(aPresContext, aStatus);

  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    // Size and position the view if requested
    // XXX Not if we're being scrolled, because the scroll-frame handles
    // this...
    nsIAtom*  pseudoType;
    mStyleContext->GetPseudoType(pseudoType);
    PRBool  isScrolled = nsHTMLAtoms::scrolledContentPseudo == pseudoType;
    NS_IF_RELEASE(pseudoType);

    if (!isScrolled) {
      nsIView*  view;
      GetView(&view);
      if (nsnull != view) {
        // Position and size view relative to its parent, not relative to our
        // parent frame (our parent frame may not have a view).
        nsIView* parentWithView;
        nsPoint origin;
        GetOffsetFromView(origin, &parentWithView);
        nsIViewManager  *vm;
        view->GetViewManager(vm);
  
        // Take into account any absolutely positioned children
        nscoord xMost, yMost;
        GetPositionedInfo(xMost, yMost);
  
        vm->ResizeView(view, PR_MAX(mRect.width, xMost), PR_MAX(mRect.height, yMost));
        vm->MoveViewTo(view, origin.x, origin.y);
        NS_RELEASE(vm);
      }
    }
  }

  return rv;
}

/**
 * Called by Reflow() to handle the case where it's an incremental reflow
 * targeted at us of an absolutely positioned child frame
 */
nsresult
nsAreaFrame::IncrementalReflow(nsIPresContext&          aPresContext,
                               const nsHTMLReflowState& aReflowState)
{
  nsIReflowCommand::ReflowType  type;
  nsIFrame*                     newFrames;
  PRInt32                       numFrames = 0;

  // Get the type of reflow command
  aReflowState.reflowCommand->GetType(type);

  // Handle each specific type
  if (nsIReflowCommand::FrameAppended == type) {
    // Add the frames to our list of absolutely position frames
    aReflowState.reflowCommand->GetChildFrame(newFrames);
    NS_ASSERTION(nsnull != newFrames, "null child list");
    numFrames = LengthOf(newFrames);
    mAbsoluteFrames.AppendFrames(nsnull, newFrames);

  } else if (nsIReflowCommand::FrameRemoved == type) {
    // Get the new frame
    nsIFrame* childFrame;
    aReflowState.reflowCommand->GetChildFrame(childFrame);

    PRBool result = mAbsoluteFrames.DeleteFrame(aPresContext, childFrame);
    NS_ASSERTION(result, "didn't find frame to delete");

  } else if (nsIReflowCommand::FrameInserted == type) {
    // Get the previous sibling
    nsIFrame* prevSibling;
    aReflowState.reflowCommand->GetPrevSiblingFrame(prevSibling);

    // Insert the new frames
    aReflowState.reflowCommand->GetChildFrame(newFrames);
    NS_ASSERTION(nsnull != newFrames, "null child list");
    numFrames = LengthOf(newFrames);
    mAbsoluteFrames.InsertFrames(nsnull, prevSibling, newFrames);

  } else {
    NS_ASSERTION(PR_FALSE, "unexpected reflow type");
  }

  // For inserted and appended reflow commands we need to reflow the
  // newly added frames
  if ((nsIReflowCommand::FrameAppended == type) ||
      (nsIReflowCommand::FrameInserted == type)) {

    while (numFrames-- > 0) {
      nsReflowStatus  status;

      ReflowAbsoluteFrame(aPresContext, aReflowState, newFrames, PR_TRUE, status);
      newFrames->GetNextSibling(&newFrames);
    }
  }

  return NS_OK;
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
    PRBool  handled = PR_FALSE;

    // See if it's targeted at us
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);

    if (this == targetFrame) {
      nsIAtom*  listName;
      PRBool    isAbsoluteChild;

      // It's targeted at us. See if the child frame is absolutely positioned
      aReflowState.reflowCommand->GetChildListName(listName);
      isAbsoluteChild = nsLayoutAtoms::absoluteList == listName;
      NS_IF_RELEASE(listName);

      if (isAbsoluteChild) {
        rv = IncrementalReflow(aPresContext, aReflowState);
        handled = PR_TRUE;
      }

    } else {
      // Peek at the next frame in the reflow path
      nsIFrame* nextFrame;
      aReflowState.reflowCommand->GetNext(nextFrame, PR_FALSE);

      // See if it's one of our absolutely positioned child frames
      NS_ASSERTION(nsnull != nextFrame, "next frame in reflow command is null."); 
      if (mAbsoluteFrames.ContainsFrame(nextFrame)) {
        // Remove the next frame from the reflow path
        aReflowState.reflowCommand->GetNext(nextFrame, PR_TRUE);

        nsReflowStatus  kidStatus;
        rv = ReflowAbsoluteFrame(aPresContext, aReflowState, nextFrame,
                                 PR_FALSE, kidStatus);
        // XXX Make sure the frame is repainted. For the time being, since we
        // have no idea what actually changed repaint it all...
        nsIView*  view;
        nextFrame->GetView(&view);
        if (nsnull != view) {
          nsIViewManager* viewMgr;
          view->GetViewManager(viewMgr);
          if (nsnull != viewMgr) {
            viewMgr->UpdateView(view, (nsIRegion*)nsnull, NS_VMREFRESH_NO_SYNC);
            NS_RELEASE(viewMgr);
          }
        }
        handled = PR_TRUE;
      }
    }

    // If we handled the incremental reflow, then we're all done
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

  // Let the block frame code handle it. Make a copy of the reflow state so we can
  // set the space manager.
  // Note: we don't do this for absolutely positioned child frames, because they
  // have their own space manager
  nsHTMLReflowState reflowState(aReflowState);

  if (nsnull != mSpaceManager) {
    reflowState.spaceManager = mSpaceManager;

    // Clear the spacemanager's regions.
    mSpaceManager->ClearRegions();
  }

  rv = nsBlockFrame::Reflow(aPresContext, aDesiredSize, reflowState, aStatus);

  // Reflow the absolutely positioned child frames. We need to do this even for
  // an incremental reflow, because some of the absolutely positioned child frames
  // may have 'auto' for an offset
  ReflowAbsoluteFrames(aPresContext, reflowState);

  // Compute our desired size taking into account any floaters. Note
  // that if this frame has a height specified by CSS then we don't do
  // this!
  if ((nsnull != mSpaceManager) &&
      (NS_UNCONSTRAINEDSIZE == reflowState.computedHeight)) {
    nscoord floaterYMost;
    mSpaceManager->YMost(floaterYMost);
    if (floaterYMost > 0) {
      // What we need to check for is if the bottom most floater extends below
      // the content area of the desired size
      nscoord   contentYMost = aDesiredSize.height -
                               aReflowState.mComputedBorderPadding.bottom;
  
      if (floaterYMost > contentYMost) {
        aDesiredSize.height += floaterYMost - contentYMost;
      }
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

/////////////////////////////////////////////////////////////////////////////
// Helper functions

// XXX Optimize the case where it's a resize reflow and the absolutely
// positioned child has the exact same size and position and skip the
// reflow...
nsresult
nsAreaFrame::ReflowAbsoluteFrame(nsIPresContext&          aPresContext,
                                 const nsHTMLReflowState& aReflowState,
                                 nsIFrame*                aKidFrame,
                                 PRBool                   aInitialReflow,
                                 nsReflowStatus&          aStatus)
{
  nsresult  rv;
  nsMargin  border;
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  spacing->GetBorder(border);
  
  nsIHTMLReflow*  htmlReflow;
  rv = aKidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  if (NS_SUCCEEDED(rv)) {
    htmlReflow->WillReflow(aPresContext);

    nsSize              availSize(aReflowState.computedWidth, NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowMetrics kidDesiredSize(nsnull);
    nsHTMLReflowState   kidReflowState(aPresContext, aReflowState, aKidFrame,
                                       availSize);

    // If it's the initial reflow, then override the reflow reason. This is
    // used when frames are inserted incrementally
    if (aInitialReflow) {
      kidReflowState.reason = eReflowReason_Initial;
    }

    rv = htmlReflow->Reflow(aPresContext, kidDesiredSize, kidReflowState, aStatus);

    // XXX If the child had a fixed height, then make sure it respected it...
    if (NS_AUTOHEIGHT != kidReflowState.computedHeight) {
      if (kidDesiredSize.height < kidReflowState.computedHeight) {
        kidDesiredSize.height = kidReflowState.computedHeight;
        kidDesiredSize.height += kidReflowState.mComputedBorderPadding.top +
                                 kidReflowState.mComputedBorderPadding.bottom;
      }
    }
    
    // Position the child relative to our padding edge
    nsRect  rect(border.left + kidReflowState.computedOffsets.left + kidReflowState.computedMargin.left,
                 border.top + kidReflowState.computedOffsets.top + kidReflowState.computedMargin.top,
                 kidDesiredSize.width, kidDesiredSize.height);
    aKidFrame->SetRect(rect);
  }

  return rv;
}

// Called by Reflow() to reflow the absolutely positioned child frames.
void
nsAreaFrame::ReflowAbsoluteFrames(nsIPresContext&          aPresContext,
                                  const nsHTMLReflowState& aReflowState)
{
  // Make a copy of the reflow state. If the reason is eReflowReason_Incremental,
  // then change it to eReflowReason_Resize
  nsHTMLReflowState reflowState(aReflowState);
  if (eReflowReason_Incremental == reflowState.reason) {
    reflowState.reason = eReflowReason_Resize;
  }

  nsIFrame* kidFrame;
  for (kidFrame = mAbsoluteFrames.FirstChild(); nsnull != kidFrame; kidFrame->GetNextSibling(&kidFrame)) {
    // Reflow the frame
    nsReflowStatus  kidStatus;
    ReflowAbsoluteFrame(aPresContext, reflowState, kidFrame, PR_FALSE,
                        kidStatus);
  }
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
