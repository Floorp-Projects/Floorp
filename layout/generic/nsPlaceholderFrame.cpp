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
#include "nsPlaceholderFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"

#include "nsIContent.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"

nsresult
NS_NewPlaceholderFrame(nsIFrame**  aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsPlaceholderFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

NS_IMETHODIMP
nsPlaceholderFrame::Reflow(nsIPresContext&          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  if (nsnull != aReflowState.lineLayout) {
    if (eReflowReason_Initial == aReflowState.reason) {
      aReflowState.lineLayout->InitFloater(this);
    }
    else {
      aReflowState.lineLayout->AddFloater(this);
    }
  }

  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.descent = 0;
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

NS_IMETHODIMP
nsPlaceholderFrame::Paint(nsIPresContext& aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect,
                          nsFramePaintLayer aWhichLayer)
{
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    float p2t;
    aPresContext.GetPixelsToTwips(&p2t);
    aRenderingContext.SetColor(NS_RGB(0, 255, 255));
    nscoord x = NSIntPixelsToTwips(-5, p2t);
    aRenderingContext.FillRect(x, 0, NSIntPixelsToTwips(13, p2t),
                               NSIntPixelsToTwips(3, p2t));
    nscoord y = NSIntPixelsToTwips(-10, p2t);
    aRenderingContext.FillRect(0, y, NSIntPixelsToTwips(3, p2t),
                               NSIntPixelsToTwips(10, p2t));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPlaceholderFrame::ContentChanged(nsIPresContext* aPresContext,
                                   nsIContent*     aChild,
                                   nsISupports*    aSubContent)
{
  NS_ASSERTION(mContent == aChild, "bad content-changed target");

  // Forward the notification to the floater
  if (nsnull != mAnchoredItem) {
    return mAnchoredItem->ContentChanged(aPresContext, aChild, aSubContent);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPlaceholderFrame::AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent* aChild,
                                     nsIAtom* aAttribute,
                                     PRInt32 aHint)
{
  // Forward the notification to the floater
  if (nsnull != mAnchoredItem) {
    return mAnchoredItem->AttributeChanged(aPresContext, aChild,
                                           aAttribute, aHint);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPlaceholderFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Placeholder", aResult);
}
