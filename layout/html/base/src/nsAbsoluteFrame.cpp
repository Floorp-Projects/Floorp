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
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsIPresContext.h"
#include "nsIPtr.h"
#include "nsIScrollableView.h"
#include "nsIStyleContext.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsAbsoluteFrame.h"
#include "nsBodyFrame.h"
#include "nsStyleConsts.h"
#include "nsViewsCID.h"

static NS_DEFINE_IID(kStylePositionSID, NS_STYLEPOSITION_SID);
static NS_DEFINE_IID(kStyleMoleculeSID, NS_STYLEMOLECULE_SID);
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);

NS_DEF_PTR(nsIStyleContext);

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

nsIView* AbsoluteFrame::CreateView(nsIFrame*     aContainingBlock,
                                   const nsRect& aRect,
                                   PRInt32       aZIndex)
{
  nsIView*  containingView;
  nsIView*  view;

  // Create a view for the frame and position it
  aContainingBlock->GetView(containingView);
  static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
  static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

  nsresult result = NSRepository::CreateInstance(kViewCID, 
                                                 nsnull, 
                                                 kIViewIID, 
                                                 (void **)&view);
  if (NS_OK == result) {
    nsIViewManager* viewManager = containingView->GetViewManager();

    // Initialize the view
    NS_ASSERTION(nsnull != viewManager, "null view manager");

    // See if the containing view is a scroll view
    nsIScrollableView*  scrollView = nsnull;
    nsresult            result;
     
    result = containingView->QueryInterface(kIScrollableViewIID, (void**)&scrollView);
    if (NS_OK == result) {
      nsIView* scrolledView = scrollView->GetScrolledView();

      view->Init(viewManager, aRect, scrolledView, nsnull, nsnull, nsnull, aZIndex);
      viewManager->InsertChild(scrolledView, view, 0);
      NS_RELEASE(scrolledView);
      NS_RELEASE(scrollView);
    } else {
      view->Init(viewManager, aRect, containingView, nsnull, nsnull, nsnull, aZIndex);
      viewManager->InsertChild(containingView, view, 0);
    }
  
    NS_RELEASE(viewManager);
  }
  NS_RELEASE(containingView);
  return view;
}

void AbsoluteFrame::ComputeViewsRect(nsIFrame* aContainingBlock, nsRect& aRect)
{
  // Get the bounding rect for the containing block's view. This is used
  // when computing the size of our view
  nsIView*  containingView;
  nsRect    containingRect;

  aContainingBlock->GetView(containingView);
  NS_ASSERTION(nsnull != containingView, "no view");
  containingView->GetBounds(containingRect);
  NS_RELEASE(containingView);

  // Compute the offset and size for the view based on the position properties
  nsStylePosition*  position = (nsStylePosition*)mStyleContext->GetData(kStylePositionSID);

  if (NS_STYLE_POSITION_VALUE_AUTO == position->mLeftOffsetFlags) {
    // Left offset should be automatically computed
    if (NS_STYLE_POSITION_VALUE_AUTO == position->mWidthFlags) {
      // When both properties are 'auto' the width is the same as the width of
      // the containing block
      aRect.width = containingRect.width;
      aRect.x = 0;
    } else if (NS_STYLE_POSITION_VALUE_LENGTH == position->mWidthFlags) {
      aRect.width = position->mWidth;
      aRect.x = containingRect.width - aRect.width;
    } else {
      NS_ASSERTION(NS_STYLE_POSITION_VALUE_PCT == position->mWidthFlags, "unexpected width");
      aRect.width = containingRect.width * position->mWidth / 100;
      aRect.x = 0;
    }
  } else {
    aRect.x = position->mLeftOffset;

    if (NS_STYLE_POSITION_VALUE_AUTO == position->mWidthFlags) {
      aRect.width = containingRect.width - aRect.x;
    } else if (NS_STYLE_POSITION_VALUE_LENGTH == position->mWidthFlags) {
      aRect.width = position->mWidth;
    } else {
      NS_ASSERTION(NS_STYLE_POSITION_VALUE_PCT == position->mWidthFlags, "unexpected width");
      aRect.width = containingRect.width * position->mWidth / 100;
    }
  }

  if (NS_STYLE_POSITION_VALUE_AUTO == position->mTopOffsetFlags) {
    // Top offset should be automatically computed
    if (NS_STYLE_POSITION_VALUE_AUTO == position->mHeightFlags) {
      // When both properties are 'auto' the height is the same as the height of
      // the containing block
      aRect.height = containingRect.height;
      aRect.y = 0;
    } else if (NS_STYLE_POSITION_VALUE_LENGTH == position->mHeightFlags) {
      aRect.height = position->mHeight;
      aRect.y = containingRect.height - aRect.height;
    } else {
      NS_ASSERTION(NS_STYLE_POSITION_VALUE_PCT == position->mHeightFlags, "unexpected height");
      aRect.height = containingRect.height * position->mHeight / 100;
      aRect.y = 0;
    }
  } else {
    aRect.y = position->mTopOffset;

    if (NS_STYLE_POSITION_VALUE_AUTO == position->mHeightFlags) {
      aRect.height = containingRect.height - aRect.y;
    } else if (NS_STYLE_POSITION_VALUE_LENGTH == position->mHeightFlags) {
      aRect.height = position->mHeight;
    } else {
      NS_ASSERTION(NS_STYLE_POSITION_VALUE_PCT == position->mHeightFlags, "unexpected height");
      aRect.height = containingRect.height * position->mHeight / 100;
    }
  }
}

nsIFrame* AbsoluteFrame::GetContainingBlock()
{
  // Look for a containing frame that is absolutely positioned. If we don't
  // find one then use the initial containg block which is the root frame
  nsIFrame* lastFrame = this;
  nsIFrame* result;

  GetContentParent(result);
  while (nsnull != result) {
    nsStylePosition* position;

    // Get the style data
    result->GetStyleData(kStylePositionSID, (nsStyleStruct*&)position);

    if (position->mPosition == NS_STYLE_POSITION_ABSOLUTE) {
      break;
    }

    // Get the next contentual parent
    lastFrame = result;
    result->GetContentParent(result);
  }

  if (nsnull == result) {
    result = lastFrame;
  }

  return result;
}

NS_METHOD AbsoluteFrame::ResizeReflow(nsIPresContext*  aPresContext,
                                      nsReflowMetrics& aDesiredSize,
                                      const nsSize&    aMaxSize,
                                      nsSize*          aMaxElementSize,
                                      ReflowStatus&    aStatus)
{
  // Have we created the absolutely positioned item yet?
  if (nsnull == mFrame) {
    // If the content object is a container then wrap it in a body pseudo-frame
    if (mContent->CanContainChildren()) {
      nsBodyFrame::NewFrame(&mFrame, mContent, mIndexInParent, this);

      // Resolve style for the pseudo-frame. We can't use our style context
      nsIStyleContextPtr styleContext = aPresContext->ResolveStyleContextFor(mContent, this);
      mFrame->SetStyleContext(styleContext);

    } else {
      // Create the absolutely positioned item as a pseudo-frame child. We'll
      // also create a view
      nsIContentDelegate* delegate = mContent->GetDelegate(aPresContext);
  
      mFrame= delegate->CreateFrame(aPresContext, mContent, mIndexInParent, this);
      NS_RELEASE(delegate);
  
      // Set the style context for the frame
      mFrame->SetStyleContext(mStyleContext);
    }

    // Get the containing block
    nsIFrame* containingBlock = GetContainingBlock();

    // Determine the view's rect
    nsRect    rect;

    ComputeViewsRect(containingBlock, rect);

    // Create a view for the frame
    nsStylePosition* position = (nsStylePosition*)mStyleContext->GetData(kStylePositionSID);
    nsIView*         view = CreateView(containingBlock, rect, position->mZIndex);

    mFrame->SetView(view);  
    NS_RELEASE(view);

    // Resize reflow the absolutely positioned element
    nsSize  availSize(rect.width, rect.height);

    mFrame->ResizeReflow(aPresContext, aDesiredSize, availSize, nsnull, aStatus);
    mFrame->SizeTo(rect.width, rect.height);
  }

  // Return our desired size as (0, 0)
  return nsFrame::ResizeReflow(aPresContext, aDesiredSize, aMaxSize, aMaxElementSize, aStatus);
}

NS_METHOD AbsoluteFrame::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);

  // Output the tag
  fputs("*absolute", out);
  fprintf(out, "(%d)@%p ", mIndexInParent, this);

  // Output the rect
  out << mRect;

  // List the absolutely positioned frame
  fputs("<\n", out);
  mFrame->List(out, aIndent + 1);
  fputs(">\n", out);

  return NS_OK;
}
