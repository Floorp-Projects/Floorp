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

static NS_DEFINE_IID(kIFloaterContainerIID, NS_IFLOATERCONTAINER_IID);

nsresult
PlaceholderFrame::NewFrame(nsIFrame**  aInstancePtrResult,
                           nsIContent* aContent,
                           PRInt32     aIndexInParent,
                           nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new PlaceholderFrame(aContent, aIndexInParent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

PlaceholderFrame::PlaceholderFrame(nsIContent* aContent,
                                   PRInt32     aIndexInParent,
                                   nsIFrame*   aParent)
  : nsFrame(aContent, aIndexInParent, aParent)
{
  mAnchoredItem = nsnull;
}

PlaceholderFrame::~PlaceholderFrame()
{
}

NS_METHOD PlaceholderFrame::ResizeReflow(nsIPresContext*  aPresContext,
                                         nsReflowMetrics& aDesiredSize,
                                         const nsSize&    aMaxSize,
                                         nsSize*          aMaxElementSize,
                                         ReflowStatus&    aStatus)
{
  // Get the floater container in which we're inserted
  nsIFloaterContainer*  container = nsnull;

  for (nsIFrame* parent = mGeometricParent; parent; parent->GetGeometricParent(parent)) {
    if (NS_OK == parent->QueryInterface(kIFloaterContainerIID, (void**)&container)) {
      break;
    }
  }
  NS_ASSERTION(nsnull != container, "no floater container");

  // Have we created the anchored item yet?
  if (nsnull == mAnchoredItem) {
    // Create the anchored item
    nsIContentDelegate* delegate = mContent->GetDelegate(aPresContext);

    mAnchoredItem = delegate->CreateFrame(aPresContext, mContent, mIndexInParent,
                                          mGeometricParent);
    NS_RELEASE(delegate);

    // Set the style context for the frame
    mAnchoredItem->SetStyleContext(mStyleContext);

    // Resize reflow the anchored item into the available space
    // XXX Check for complete?
    mAnchoredItem->ResizeReflow(aPresContext, aDesiredSize, aMaxSize, nsnull, aStatus);
    mAnchoredItem->SizeTo(aDesiredSize.width, aDesiredSize.height);

    // Now notify our containing block that there's a new floater
    container->AddFloater(aPresContext, mAnchoredItem, this);
  } else {
    container->PlaceFloater(aPresContext, mAnchoredItem, this);
  }

  return nsFrame::ResizeReflow(aPresContext, aDesiredSize, aMaxSize, aMaxElementSize, aStatus);
}

NS_METHOD PlaceholderFrame::ListTag(FILE* out) const
{
  fputs("*placeholder", out);
  fprintf(out, "(%d)@%p", mIndexInParent, this);
  return NS_OK;
}
