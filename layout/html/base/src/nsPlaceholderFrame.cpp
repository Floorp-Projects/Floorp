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

static NS_DEFINE_IID(kIFloaterContainerIID, NS_IFLOATERCONTAINER_IID);

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
  : nsContainerFrame(aContent, aParent)
{
}

nsPlaceholderFrame::~nsPlaceholderFrame()
{
}

NS_IMETHODIMP
nsPlaceholderFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_NOT_SPLITTABLE;
  return NS_OK;
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
  if (nsnull == mFirstChild) {
    // If the content object is a container then wrap it in a body pseudo-frame

    // XXX begin hack
    PRBool select = PR_FALSE;
    nsIAtom* atom = mContent->GetTag();
    nsAutoString tmp;
    if (nsnull != atom) {
      atom->ToString(tmp);
      if (tmp.EqualsIgnoreCase("select")) {
        select = PR_TRUE;
      }
    }
    // XXX end hack

    if (mContent->CanContainChildren() && !select) {
      nsBodyFrame::NewFrame(&mFirstChild, mContent, this);

      // Use our style context for the pseudo-frame
      mFirstChild->SetStyleContext(aPresContext, mStyleContext);
    } else {
      // Create the anchored item
      nsIContentDelegate* delegate = mContent->GetDelegate(aPresContext);
      nsresult rv = delegate->CreateFrame(aPresContext, mContent,
                                          mGeometricParent, mStyleContext,
                                          mFirstChild);
      NS_RELEASE(delegate);
      if (NS_OK != rv) {
        return rv;
      }
    }
    mChildCount = 1;

    // Compute the available space for the floater. Use the default
    // 'auto' width and height values
    nsSize  kidAvailSize(0, NS_UNCONSTRAINEDSIZE);
    nsSize  styleSize;
    PRIntn  styleSizeFlags = nsCSSLayout::GetStyleSize(aPresContext, aReflowState,
                                                       styleSize);

    // XXX The width and height are for the content area only. Add in space for
    // border and padding
    if (styleSizeFlags & NS_SIZE_HAS_WIDTH) {
      kidAvailSize.width = styleSize.width;
    }
    if (styleSizeFlags & NS_SIZE_HAS_HEIGHT) {
      kidAvailSize.height = styleSize.height;
    }

    // Resize reflow the anchored item into the available space
    // XXX Check for complete?
    nsReflowMetrics desiredSize(nsnull);
    nsReflowState   reflowState(mFirstChild, aReflowState, kidAvailSize,
                                eReflowReason_Initial);
    mFirstChild->WillReflow(*aPresContext);
    mFirstChild->Reflow(aPresContext, desiredSize, reflowState, aStatus);
    mFirstChild->SizeTo(desiredSize.width, desiredSize.height);

    // Now notify our containing block that there's a new floater
    container->AddFloater(aPresContext, mFirstChild, this);
    mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);

  } else {
    // XXX This causes anchored-items sizes to get fixed up; this is
    // not quite right because this class should be implementing one
    // of the incremental reflow methods and propagating things down
    // properly to the contained frame.
    nsReflowMetrics desiredSize(nsnull);
    nsReflowState   reflowState(mFirstChild, aReflowState, aReflowState.maxSize,
                                eReflowReason_Resize);
    mFirstChild->WillReflow(*aPresContext);
    mFirstChild->Reflow(aPresContext, desiredSize, reflowState, aStatus);
    mFirstChild->SizeTo(desiredSize.width, desiredSize.height);
    container->PlaceFloater(aPresContext, mFirstChild, this);
    mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);
  }

  return nsFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

NS_METHOD nsPlaceholderFrame::ListTag(FILE* out) const
{
  fputs("*placeholder", out);
  PRInt32 contentIndex;
  GetContentIndex(contentIndex);
  fprintf(out, "(%d)@%p", contentIndex, this);
  return NS_OK;
}
