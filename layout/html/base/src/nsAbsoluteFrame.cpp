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
#include "nsIContentDelegate.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsHTMLIIDs.h"

nsresult
nsAbsoluteFrame::NewFrame(nsIFrame**  aInstancePtrResult,
                          nsIContent* aContent,
                          nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsAbsoluteFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsAbsoluteFrame::nsAbsoluteFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsFrame(aContent, aParent)
{
}

nsAbsoluteFrame::~nsAbsoluteFrame()
{
}

NS_METHOD nsAbsoluteFrame::Reflow(nsIPresContext*      aPresContext,
                                  nsReflowMetrics&     aDesiredSize,
                                  const nsReflowState& aReflowState,
                                  nsReflowStatus&      aStatus)
{
  // Have we created the absolutely positioned item yet?
  if (nsnull == mFrame) {
    // If the content object is a container then wrap it in a body pseudo-frame
    if (mContent->CanContainChildren()) {
      nsBodyFrame::NewFrame(&mFrame, mContent, this);

      // Use our style context for the pseudo-frame
      mFrame->SetStyleContext(aPresContext, mStyleContext);

    } else {
      // Ask the content delegate to create the frame
      nsIContentDelegate* delegate = mContent->GetDelegate(aPresContext);
  
      nsresult rv = delegate->CreateFrame(aPresContext, mContent, this,
                                          mStyleContext, mFrame);
      NS_RELEASE(delegate);
      if (NS_OK != rv) {
        return rv;
      }
    }

    // Get the containing block
    nsIFrame* containingBlock = GetContainingBlock();
    NS_ASSERTION(nsnull != containingBlock, "no initial containing block");

    // Query for its nsIAbsoluteItems interface
    nsIAbsoluteItems* absoluteItemContainer;
    containingBlock->QueryInterface(kIAbsoluteItemsIID, (void**)&absoluteItemContainer);

    // Notify it that there's a new absolutely positioned frame, passing it the
    // anchor frame
    NS_ASSERTION(nsnull != absoluteItemContainer, "no nsIAbsoluteItems support");
    absoluteItemContainer->AddAbsoluteItem(this);
  }

  // Return our desired size as (0, 0)
  return nsFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
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
      break;
    }

    // Get the next contentual parent
    lastFrame = result;
    result->GetContentParent(result);
  }

  if (nsnull == result) {
    // Walk back down the tree until we find a frame that supports nsIAnchoredItems
    // XXX This is pretty yucky, but there isn't currently a better way to do
    // this...
    lastFrame->FirstChild(result);

    while (nsnull != result) {
      nsIAbsoluteItems* interface;
      if (NS_OK == result->QueryInterface(kIAbsoluteItemsIID, (void**)&interface)) {
        break;
      }

      result->FirstChild(result);      
    }
  }

  return result;
}

NS_METHOD nsAbsoluteFrame::ListTag(FILE* out) const
{
  fputs("*absolute", out);
  PRInt32 contentIndex;
  GetContentIndex(contentIndex);
  fprintf(out, "(%d)@%p", contentIndex, this);
  return NS_OK;
}
