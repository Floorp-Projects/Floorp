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
#include "nsHTMLParts.h"
#include "nsContainerFrame.h"
#include "nsHTMLIIDs.h"

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
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  
  NS_IMETHOD GetFrameName(nsString& aResult) const;
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
ViewportFrame::Reflow(nsIPresContext&          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("ViewportFrame::Reflow");
  NS_PRECONDITION(nsnull == aDesiredSize.maxElementSize, "unexpected request");

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  // Check for an incremental reflow
  if (eReflowReason_Incremental == aReflowState.reason) {
    // See if we're the target frame
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      // The viewport does not support being the target of an incremental
      // reflow command
      NS_ASSERTION(PR_FALSE, "unexpected reflow command target");

    } else {
      nsIFrame* nextFrame;
      // Get the next frame in the reflow chain
      aReflowState.reflowCommand->GetNext(nextFrame);
      NS_ASSERTION(nextFrame == mFrames.FirstChild(), "unexpected next reflow command frame");
    }
  }

  // Reflow our one and only child frame
  if (mFrames.NotEmpty()) {
    nsIFrame*           kidFrame = mFrames.FirstChild();
    nsHTMLReflowMetrics desiredSize(nsnull);
    nsSize              availableSpace(aReflowState.availableWidth,
                                       aReflowState.availableHeight);
    nsHTMLReflowState   kidReflowState(aPresContext, kidFrame,
                                       aReflowState, availableSpace);

    // Reflow the frame
    nsIHTMLReflow* htmlReflow;
    if (NS_OK == kidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      kidReflowState.computedHeight = aReflowState.availableHeight;
      ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState,
                  aStatus);
    
      nsRect  rect(0, 0, desiredSize.width, desiredSize.height);
      kidFrame->SetRect(rect);

      // XXX We should resolve the details of who/when DidReflow()
      // notifications are sent...
      htmlReflow->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
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
