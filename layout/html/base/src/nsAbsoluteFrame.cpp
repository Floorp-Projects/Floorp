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
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"

nsresult
AbsoluteFrame::NewFrame(nsIFrame**  aInstancePtrResult,
                        nsIContent* aContent,
                        PRInt32     aIndexInParent,
                        nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new AbsoluteFrame(aContent, aIndexInParent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

AbsoluteFrame::AbsoluteFrame(nsIContent* aContent,
                             PRInt32     aIndexInParent,
                             nsIFrame*   aParent)
  : nsFrame(aContent, aIndexInParent, aParent)
{
  mFrame = nsnull;
}

AbsoluteFrame::~AbsoluteFrame()
{
}

NS_METHOD AbsoluteFrame::ResizeReflow(nsIPresContext*  aPresContext,
                                      nsReflowMetrics& aDesiredSize,
                                      const nsSize&    aMaxSize,
                                      nsSize*          aMaxElementSize,
                                      ReflowStatus&    aStatus)
{
  // Have we created the absolutely positioned item yet?
  if (nsnull == mFrame) {
    // Create the absolutely positioned item as a pseudo-frame child. We'll
    // also create a view
    nsIContentDelegate* delegate = mContent->GetDelegate(aPresContext);

    mFrame= delegate->CreateFrame(aPresContext, mContent, mIndexInParent, this);
    NS_RELEASE(delegate);

    // Set the style context for the frame
    mFrame->SetStyleContext(mStyleContext);

    // Resize reflow the anchored item into the available space
    mFrame->ResizeReflow(aPresContext, aDesiredSize, aMaxSize, nsnull, aStatus);
    mFrame->SizeTo(aDesiredSize.width, aDesiredSize.height);

    // Create a view for the frame and position it
    // XXX This isn't correct. We should look for a containing frame that is absolutely
    // positioned and use its local coordinate system, or the root content frame
    nsIFrame* parent;
    nsIView*  parentView;
    nsIView*  view;

    GetParentWithView(parent);
    parent->GetView(parentView);
    static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
    static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
  
    nsresult result = NSRepository::CreateInstance(kViewCID, 
                                                   nsnull, 
                                                   kIViewIID, 
                                                   (void **)&view);
    if (NS_OK == result) {
      nsRect          rect(0, 0, aDesiredSize.width, aDesiredSize.height);
      nsIViewManager* viewManager = parentView->GetViewManager();
  
      // Initialize the view
      NS_ASSERTION(nsnull != viewManager, "null view manager");
  
      view->Init(viewManager, rect, parentView);
      viewManager->InsertChild(parentView, view, 0);
  
      NS_RELEASE(viewManager);
      mFrame->SetView(view);  
      NS_RELEASE(view);
    }
    NS_RELEASE(parentView);
  }

  // Return our desired size as (0, 0)
  return nsFrame::ResizeReflow(aPresContext, aDesiredSize, aMaxSize, aMaxElementSize, aStatus);
}

NS_METHOD AbsoluteFrame::ListTag(FILE* out) const
{
  fputs("*absolute", out);
  fprintf(out, "(%d)@%p", mIndexInParent, this);
  return NS_OK;
}
