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
#include "nsCSSLayout.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"

#include "nsIContent.h"
#include "nsIFloaterContainer.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"

nsresult
nsPlaceholderFrame::NewFrame(nsIFrame**  aInstancePtrResult,
                             nsIContent* aContent,
                             nsIFrame*   aParent,
                             nsIFrame*   aAnchoredItem)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsPlaceholderFrame(aContent, aParent, aAnchoredItem);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsPlaceholderFrame::nsPlaceholderFrame(nsIContent* aContent,
                                       nsIFrame*   aParent,
                                       nsIFrame*   aAnchoredItem)
  : nsFrame(aContent, aParent)
{
  mAnchoredItem = aAnchoredItem;
}

nsPlaceholderFrame::~nsPlaceholderFrame()
{
}

NS_IMETHODIMP
nsPlaceholderFrame::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIInlineReflowIID)) {
    nsIInlineReflow* tmp = this;
    *aInstancePtrResult = (void*) tmp;
    return NS_OK;
  }
  return nsFrame::QueryInterface(aIID, aInstancePtrResult);
}

NS_IMETHODIMP
nsPlaceholderFrame::FindTextRuns(nsLineLayout&     aLineLayout,
                                 nsIReflowCommand* aReflowCommand)
{
  aLineLayout.EndTextRun();
  return NS_OK;
}

NS_IMETHODIMP
nsPlaceholderFrame::InlineReflow(nsLineLayout&        aLineLayout,
                                 nsReflowMetrics&     aDesiredSize,
                                 const nsReflowState& aReflowState)
{
  nsIPresContext& presContext = aLineLayout.mPresContext;

  // Get the floater container in which we're inserted
  nsIFrame*             containingBlock;
  nsIFloaterContainer*  container = nsnull;

  for (containingBlock = mGeometricParent;
       nsnull != containingBlock;
       containingBlock->GetGeometricParent(containingBlock))
  {
    if (NS_OK == containingBlock->QueryInterface(kIFloaterContainerIID, (void**)&container)) {
      break;
    }
  }
  NS_ASSERTION(nsnull != container, "no floater container");

  if (eReflowReason_Initial == aReflowState.reason) {
    // By this point we expect to have been told which anchored item we're
    // associated with
    NS_ASSERTION(nsnull != mAnchoredItem, "no anchored item");

    // Notify our containing block that there's a new floater
    container->AddFloater(&presContext, aReflowState, mAnchoredItem, this);
  }

  // Let line layout know about the floater
  aLineLayout.AddFloater(this);

  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.descent = 0;
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  return NS_FRAME_COMPLETE;
}

NS_IMETHODIMP
nsPlaceholderFrame::Paint(nsIPresContext& aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect)
{
  if (nsIFrame::GetShowFrameBorders()) {
    float p2t = aPresContext.GetPixelsToTwips();
    aRenderingContext.SetColor(NS_RGB(0, 255, 255));
    nscoord x = NSIntPixelsToTwips(-5, p2t);
    aRenderingContext.FillRect(x, 0, NSIntPixelsToTwips(13, p2t), NSIntPixelsToTwips(3, p2t));
    nscoord y = NSIntPixelsToTwips(-10, p2t);
    aRenderingContext.FillRect(0, y, NSIntPixelsToTwips(3, p2t), NSIntPixelsToTwips(10, p2t));
  }
  return NS_OK;
}

// XXX CONSTRUCTION
#if 0
NS_IMETHODIMP nsPlaceholderFrame::ContentInserted(nsIPresShell*   aShell,
                                                  nsIPresContext* aPresContext,
                                                  nsIContent*     aContainer,
                                                  nsIContent*     aChild,
                                                  PRInt32         aIndexInParent)
{
  NS_ASSERTION(mContent == aContainer, "bad content-inserted target");

  // Forward the notification to the floater
  if (nsnull != mAnchoredItem) {
    return mAnchoredItem->ContentInserted(aShell, aPresContext, aContainer,
                                          aChild, aIndexInParent);
  }

  return NS_OK;
}
#endif

NS_IMETHODIMP nsPlaceholderFrame::ContentReplaced(nsIPresShell*   aShell,
                                                  nsIPresContext* aPresContext,
                                                  nsIContent*     aContainer,
                                                  nsIContent*     aOldChild,
                                                  nsIContent*     aNewChild,
                                                  PRInt32         aIndexInParent)
{
  NS_ASSERTION(mContent == aContainer, "bad content-replaced target");

  // Forward the notification to the floater
  if (nsnull != mAnchoredItem) {
    return mAnchoredItem->ContentReplaced(aShell, aPresContext, aContainer,
                                          aOldChild, aNewChild, aIndexInParent);
  }

  return NS_OK;
}

NS_IMETHODIMP nsPlaceholderFrame::ContentDeleted(nsIPresShell*   aShell,
                                                 nsIPresContext* aPresContext,
                                                 nsIContent*     aContainer,
                                                 nsIContent*     aChild,
                                                 PRInt32         aIndexInParent)
{
  NS_ASSERTION(mContent == aContainer, "bad content-deleted target");

  // Forward the notification to the floater
  if (nsnull != mAnchoredItem) {
    return mAnchoredItem->ContentDeleted(aShell, aPresContext, aContainer,
                                         aChild, aIndexInParent);
  }

  return NS_OK;
}

NS_IMETHODIMP nsPlaceholderFrame::ContentChanged(nsIPresShell*   aShell,
                                                 nsIPresContext* aPresContext,
                                                 nsIContent*     aChild,
                                                 nsISupports*    aSubContent)
{
  NS_ASSERTION(mContent == aChild, "bad content-changed target");

  // Forward the notification to the floater
  if (nsnull != mAnchoredItem) {
    return mAnchoredItem->ContentChanged(aShell, aPresContext, aChild, aSubContent);
  }

  return NS_OK;
}

NS_IMETHODIMP nsPlaceholderFrame::ListTag(FILE* out) const
{
  fputs("*placeholder", out);
  PRInt32 contentIndex;
  GetContentIndex(contentIndex);
  fprintf(out, "(%d)@%p", contentIndex, this);
  return NS_OK;
}
