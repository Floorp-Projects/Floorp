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
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsIFloaterContainer.h"
#include "nsBodyFrame.h"
#include "nsIStyleContext.h"
#include "nsCSSLayout.h"
#include "nsIView.h"
#include "nsHTMLIIDs.h"

nsresult
nsPlaceholderFrame::NewFrame(nsIFrame**  aInstancePtrResult,
                             nsIContent* aContent,
                             nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsPlaceholderFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsPlaceholderFrame::nsPlaceholderFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsFrame(aContent, aParent)
{
}

nsPlaceholderFrame::~nsPlaceholderFrame()
{
}

NS_IMETHODIMP
nsPlaceholderFrame::Reflow(nsIPresContext*      aPresContext,
                           nsReflowMetrics&     aDesiredSize,
                           const nsReflowState& aReflowState,
                           nsReflowStatus&      aStatus)
{
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

  // Have we created the anchored item yet?
  if (nsnull == mAnchoredItem) {
    // If the content object is a container then wrap it in a body pseudo-frame

    // XXX begin hack
    // XXX More hack. We don't want to blindly wrap a body pseudo-frame around 
    // a table content object, because the body pseudo-frame will map the table's
    // content children rather than create a table frame and let it map the
    // children...
    PRBool select = PR_FALSE;
    nsIAtom* atom = mContent->GetTag();
    nsAutoString tmp;
    if (nsnull != atom) {
      atom->ToString(tmp);
      if (tmp.EqualsIgnoreCase("select") ||
          tmp.EqualsIgnoreCase("table")) {
        select = PR_TRUE;
      }
      NS_RELEASE(atom);
    }
    // XXX end hack

    if (mContent->CanContainChildren() && !select) {
      nsBodyFrame::NewFrame(&mAnchoredItem, mContent, this);

      // Use our style context for the pseudo-frame
      mAnchoredItem->SetStyleContext(aPresContext, mStyleContext);
    } else {
      // Create the anchored item
      nsIContentDelegate* delegate = mContent->GetDelegate(aPresContext);
      nsresult rv = delegate->CreateFrame(aPresContext, mContent,
                                          mGeometricParent, mStyleContext,
                                          mAnchoredItem);
      NS_RELEASE(delegate);
      if (NS_OK != rv) {
        return rv;
      }
    }

    // Notify our containing block that there's a new floater
    container->AddFloater(aPresContext, aReflowState, mAnchoredItem, this);

  } else {
    // XXX This causes anchored-items sizes to get fixed up; this is
    // not quite right because this class should be implementing one
    // of the incremental reflow methods and propagating things down
    // properly to the contained frame.
    nsReflowMetrics desiredSize(nsnull);
    nsReflowState   reflowState(mAnchoredItem, aReflowState, aReflowState.maxSize,
                                eReflowReason_Resize);
    mAnchoredItem->WillReflow(*aPresContext);
    mAnchoredItem->Reflow(aPresContext, desiredSize, reflowState, aStatus);
    mAnchoredItem->SizeTo(desiredSize.width, desiredSize.height);

//XXXdeprecated    container->PlaceFloater(aPresContext, mAnchoredItem, this);
    mAnchoredItem->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);
  }

  return nsFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

NS_IMETHODIMP nsPlaceholderFrame::ContentAppended(nsIPresShell*   aShell,
                                                  nsIPresContext* aPresContext,
                                                  nsIContent*     aContainer)
{
  NS_ASSERTION(mContent == aContainer, "bad content-appended target");

  // Forward the notification to the floater
  if (nsnull != mAnchoredItem) {
    return mAnchoredItem->ContentAppended(aShell, aPresContext, aContainer);
  }

  return NS_OK;
}

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
