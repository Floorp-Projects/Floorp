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
#include "nsContainerFrame.h"
#include "nsHTMLParts.h"
#include "nsHTMLIIDs.h"
#include "nsLayoutAtoms.h"
#include "nsIViewManager.h"

static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);

/**
 * Viewport frame class.
 *
 * The viewport frame is the parent frame for the document element's frame.
 * It only supports having a single child frame which must be one of the
 * following:
 * - scroll frame
 * - root frame
 *
 * It has an additional named child list:
 * - "Fixed-list" which contains the fixed positioned frames
 */
class ViewportFrame : public nsContainerFrame {
public:
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom*& aListName) const;

  NS_IMETHOD FirstChild(nsIAtom* aListName, nsIFrame*& aFirstChild) const;

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  
  NS_IMETHOD GetFrameName(nsString& aResult) const;

protected:
  nsresult IncrementalReflow(nsIPresContext&          aPresContext,
                             const nsHTMLReflowState& aReflowState);

  nsresult ReflowFixedFrame(nsIPresContext&          aPresContext,
                            const nsHTMLReflowState& aReflowState,
                            nsIFrame*                aKidFrame,
                            PRBool                   aInitialReflow,
                            nsReflowStatus&          aStatus) const;

  void ReflowFixedFrames(nsIPresContext&          aPresContext,
                         const nsHTMLReflowState& aReflowState) const;

private:
  nsFrameList mFixedFrames;  // additional named child list
};

//----------------------------------------------------------------------

nsresult
NS_NewViewportFrame(nsIFrame*& aResult)
{
  ViewportFrame* frame = new ViewportFrame;
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  mFixedFrames.DeleteFrames(aPresContext);
  return nsContainerFrame::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
ViewportFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult  rv;

  if (nsLayoutAtoms::fixedList == aListName) {
    mFixedFrames.SetFrames(aChildList);
    rv = NS_OK;
  } else {
    rv = nsContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                          nsIAtom*& aListName) const
{
  nsIAtom*  atom = nsnull;

  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;

  } else if (0 == aIndex) {
    atom = nsLayoutAtoms::fixedList;
    NS_ADDREF(atom);
  }

  aListName = atom;
  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::FirstChild(nsIAtom* aListName, nsIFrame*& aFirstChild) const
{
  if (aListName == nsLayoutAtoms::fixedList) {
    aFirstChild = mFixedFrames.FirstChild();
    return NS_OK;
  }

  return nsContainerFrame::FirstChild(aListName, aFirstChild);
}

nsresult
ViewportFrame::ReflowFixedFrame(nsIPresContext&          aPresContext,
                                const nsHTMLReflowState& aReflowState,
                                nsIFrame*                aKidFrame,
                                PRBool                   aInitialReflow,
                                nsReflowStatus&          aStatus) const
{
  // Reflow the frame
  nsIHTMLReflow* htmlReflow;
  nsresult       rv;

  rv = aKidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  if (NS_SUCCEEDED(rv)) {
    htmlReflow->WillReflow(aPresContext);
    
    nsHTMLReflowMetrics kidDesiredSize(nsnull);
    nsSize              availSize(aReflowState.availableWidth, NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowState   kidReflowState(aPresContext, aKidFrame, aReflowState, availSize);
    
    // If it's the initial reflow, then override the reflow reason. This is
    // used when frames are inserted incrementally
    if (aInitialReflow) {
      kidReflowState.reason = eReflowReason_Initial;
    }
    
    // XXX Temporary hack until the block/inline code starts using 'computedWidth'
    kidReflowState.availableWidth = kidReflowState.computedWidth;
    htmlReflow->Reflow(aPresContext, kidDesiredSize, kidReflowState, aStatus);

    // XXX If the child had a fixed height, then make sure it respected it...
    if (NS_AUTOHEIGHT != kidReflowState.computedHeight) {
      if (kidDesiredSize.height < kidReflowState.computedHeight) {
        kidDesiredSize.height = kidReflowState.computedHeight;

        nsMargin  borderPadding;
        nsHTMLReflowState::ComputeBorderPaddingFor(aKidFrame, &aReflowState, borderPadding);
        kidDesiredSize.height += borderPadding.top + borderPadding.bottom;
      }
    }

    // Position the child
    nsRect  rect(kidReflowState.computedOffsets.left,
                 kidReflowState.computedOffsets.top,
                 kidDesiredSize.width, kidDesiredSize.height);
    aKidFrame->SetRect(rect);
    htmlReflow->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
  }

  return rv;
}

// Called by Reflow() to reflow all of the fixed positioned child frames.
// This is only done for 'initial', 'resize', and 'style change' reflow commands
void
ViewportFrame::ReflowFixedFrames(nsIPresContext&          aPresContext,
                                 const nsHTMLReflowState& aReflowState) const
{
  NS_PRECONDITION(eReflowReason_Incremental != aReflowState.reason,
                  "unexpected reflow reason");

  nsIFrame* kidFrame;
  for (kidFrame = mFixedFrames.FirstChild(); nsnull != kidFrame; kidFrame->GetNextSibling(kidFrame)) {
    // Reflow the frame using our reflow reason
    nsReflowStatus  kidStatus;
    ReflowFixedFrame(aPresContext, aReflowState, kidFrame, PR_FALSE,
                     kidStatus);
  }
}

/**
 * Called by Reflow() to handle the case where it's an incremental reflow
 * of a fixed child frame
 */
nsresult
ViewportFrame::IncrementalReflow(nsIPresContext&          aPresContext,
                                 const nsHTMLReflowState& aReflowState)
{
  nsIReflowCommand::ReflowType  type;
  nsIFrame*                     newFrames;
  PRInt32                       numFrames;

  // Get the type of reflow command
  aReflowState.reflowCommand->GetType(type);

  // Handle each specific type
  if (nsIReflowCommand::FrameAppended == type) {
    // Add the frames to our list of fixed position frames
    aReflowState.reflowCommand->GetChildFrame(newFrames);
    NS_ASSERTION(nsnull != newFrames, "null child list");
    numFrames = LengthOf(newFrames);
    mFixedFrames.AppendFrames(nsnull, newFrames);

  } else if (nsIReflowCommand::FrameRemoved == type) {
    // Get the new frame
    nsIFrame* childFrame;
    aReflowState.reflowCommand->GetChildFrame(childFrame);

    PRBool result = mFixedFrames.DeleteFrame(aPresContext, childFrame);
    NS_ASSERTION(result, "didn't find frame to delete");

  } else if (nsIReflowCommand::FrameInserted == type) {
    // Get the previous sibling
    nsIFrame* prevSibling;
    aReflowState.reflowCommand->GetPrevSiblingFrame(prevSibling);

    // Insert the new frames
    aReflowState.reflowCommand->GetChildFrame(newFrames);
    NS_ASSERTION(nsnull != newFrames, "null child list");
    numFrames = LengthOf(newFrames);
    mFixedFrames.InsertFrames(nsnull, prevSibling, newFrames);

  } else {
    NS_ASSERTION(PR_FALSE, "unexpected reflow type");
  }

  // For inserted and appended reflow commands we need to reflow the
  // newly added frames
  if ((nsIReflowCommand::FrameAppended == type) ||
      (nsIReflowCommand::FrameInserted == type)) {

    while (numFrames-- > 0) {
      nsReflowStatus  status;

      ReflowFixedFrame(aPresContext, aReflowState, newFrames, PR_TRUE, status);
      newFrames->GetNextSibling(newFrames);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::Reflow(nsIPresContext&          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("ViewportFrame::Reflow");
  NS_PRECONDITION(nsnull == aDesiredSize.maxElementSize, "unexpected request");

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  nsIFrame* nextFrame = nsnull;
  PRBool    isHandled = PR_FALSE;
  
  // Check for an incremental reflow
  if (eReflowReason_Incremental == aReflowState.reason) {
    // See if we're the target frame
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      nsIAtom*  listName;
      PRBool    isFixedChild;
      
      // It's targeted at us. It better be for a 'fixed' frame
      // reflow command
      aReflowState.reflowCommand->GetChildListName(listName);
      isFixedChild = nsLayoutAtoms::fixedList == listName;
      NS_IF_RELEASE(listName);
      NS_ASSERTION(isFixedChild, "unexpected child list");
      
      if (isFixedChild) {
        IncrementalReflow(aPresContext, aReflowState);
        isHandled = PR_TRUE;
      }

    } else {
      // Get the next frame in the reflow chain
      aReflowState.reflowCommand->GetNext(nextFrame);
    }
  }

  if (!isHandled) {
    if ((eReflowReason_Incremental == aReflowState.reason) &&
        (mFixedFrames.ContainsFrame(nextFrame))) {
      // Reflow the 'fixed' frame that's the next frame in the reflow path
      nsReflowStatus  kidStatus;
      ReflowFixedFrame(aPresContext, aReflowState, nextFrame, PR_FALSE,
                       kidStatus);

      // XXX Make sure the frame is repainted. For the time being, since we
      // have no idea what actually changed repaint it all...
      nsIView*  view;
      nextFrame->GetView(view);
      if (nsnull != view) {
        nsIViewManager* viewMgr;
        view->GetViewManager(viewMgr);
        if (nsnull != viewMgr) {
          viewMgr->UpdateView(view, (nsIRegion*)nsnull, NS_VMREFRESH_NO_SYNC);
          NS_RELEASE(viewMgr);
        }
      }

    } else {
      // Reflow our one and only principal child frame
      if (mFrames.NotEmpty()) {
        nsIFrame*           kidFrame = mFrames.FirstChild();
        nsHTMLReflowMetrics kidDesiredSize(nsnull);
        nsSize              availableSpace(aReflowState.availableWidth,
                                           aReflowState.availableHeight);
        nsHTMLReflowState   kidReflowState(aPresContext, kidFrame,
                                           aReflowState, availableSpace);

        // Reflow the frame
        nsIHTMLReflow* htmlReflow;
        if (NS_OK == kidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
          kidReflowState.computedHeight = aReflowState.availableHeight;
          ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                      aStatus);

          nsRect  rect(0, 0, kidDesiredSize.width, kidDesiredSize.height);
          kidFrame->SetRect(rect);

          // XXX We should resolve the details of who/when DidReflow()
          // notifications are sent...
          htmlReflow->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
        }
      }

      // If it's a 'initial', 'resize', or 'style change' reflow command (anything
      // but incremental), then reflow all the fixed positioned child frames
      if (eReflowReason_Incremental != aReflowState.reason) {
        ReflowFixedFrames(aPresContext, aReflowState);
      }
    }
  }

  // Return the max size as our desired size
  aDesiredSize.width = aReflowState.availableWidth;
  aDesiredSize.height = aReflowState.availableHeight;
  aDesiredSize.ascent = aReflowState.availableHeight;
  aDesiredSize.descent = 0;

  NS_FRAME_TRACE_REFLOW_OUT("ViewportFrame::Reflow", aStatus);
  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Viewport", aResult);
}
