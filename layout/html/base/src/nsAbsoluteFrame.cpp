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
#include "nsAbsoluteFrame.h"
#include "nsBodyFrame.h"
#include "nsIAbsoluteItems.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsHTMLIIDs.h"

nsresult
NS_NewAbsoluteFrame(nsIFrame**  aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsAbsoluteFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsAbsoluteFrame::~nsAbsoluteFrame()
{
}

NS_IMETHODIMP nsAbsoluteFrame::Reflow(nsIPresContext&          aPresContext,
                                      nsHTMLReflowMetrics&     aDesiredSize,
                                      const nsHTMLReflowState& aReflowState,
                                      nsReflowStatus&          aStatus)
{
  if (eReflowReason_Initial == aReflowState.reason) {
    // By this point we expect to have been told which absolute frame we're
    // associated with
    NS_ASSERTION(nsnull != mFrame, "no absolute frame");

    // Get the containing block
    nsIFrame* containingBlock = GetContainingBlock();
    NS_ASSERTION(nsnull != containingBlock, "no initial containing block");

    // Query for its nsIAbsoluteItems interface
    nsIAbsoluteItems* absoluteItemContainer;
    nsresult          rv;

    rv = containingBlock->QueryInterface(kIAbsoluteItemsIID, (void**)&absoluteItemContainer);
    NS_ASSERTION(NS_SUCCEEDED(rv), "no nsIAbsoluteItems support");

    // Notify it that there's a new absolutely positioned frame, passing it the
    // anchor frame
    absoluteItemContainer->AddAbsoluteItem(this);
  }

  // Return our desired size as (0, 0)
  return nsFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

NS_IMETHODIMP nsAbsoluteFrame::ContentChanged(nsIPresContext* aPresContext,
                                              nsIContent*     aChild,
                                              nsISupports*    aSubContent)
{
  NS_ASSERTION(mContent == aChild, "bad content-changed target");

  // Forward the notification to the absolutely positioned frame
  if (nsnull != mFrame) {
    return mFrame->ContentChanged(aPresContext, aChild, aSubContent);
  }

  return NS_OK;
}

NS_IMETHODIMP nsAbsoluteFrame::AttributeChanged(nsIPresContext* aPresContext,
                                                nsIContent*     aChild,
                                                nsIAtom*        aAttribute,
                                                PRInt32         aHint)
{
  NS_ASSERTION(mContent == aChild, "bad content-changed target");

  // Forward the notification to the absolutely positioned frame
  if (nsnull != mFrame) {
    return mFrame->AttributeChanged(aPresContext, aChild, aAttribute, aHint);
  }
  return NS_OK;
}


nsIFrame* nsAbsoluteFrame::GetContainingBlock() const
{
  // Look for a containing frame that is absolutely positioned. If we don't
  // find one then use the initial containg block which is the BODY
  nsIFrame* lastFrame = (nsIFrame*)this;
  nsIFrame* result;

  GetContentParent(result);
  while (nsnull != result) {
    const nsStylePosition* position;

    // Get the style data
    result->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);

    if (position->mPosition == NS_STYLE_POSITION_ABSOLUTE) {
      // XXX This needs cleaning up...
      // Make sure the frame supports the nsIAbsoluteItems interface. If not,
      // walk the geometric parent hierarchy and find the nearest one that does...
      nsIAbsoluteItems* interface;
      while ((nsnull != result) &&
             NS_FAILED(result->QueryInterface(kIAbsoluteItemsIID, (void**)&interface))) {
        result->GetGeometricParent(result);
      }
      break;
    }

    // Get the next contentual parent
    lastFrame = result;
    result->GetContentParent(result);
  }

  if (nsnull == result) {
    // Walk back down the tree until we find a frame that supports
    // nsIAbsoluteItems
    // XXX This is pretty yucky, but there isn't currently a better way to do
    // this...
    lastFrame->FirstChild(nsnull, result);

    while (nsnull != result) {
      nsIAbsoluteItems* interface;
      if (NS_SUCCEEDED(result->QueryInterface(kIAbsoluteItemsIID, (void**)&interface))) {
        break;
      }

      result->FirstChild(nsnull, result);      
    }
  }

  return result;
}

NS_IMETHODIMP
nsAbsoluteFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Absolute", aResult);
}
