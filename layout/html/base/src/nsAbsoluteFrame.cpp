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
#include "nsIContentDelegate.h"
#include "nsIPresContext.h"
#include "nsIPtr.h"
#include "nsIScrollableView.h"
#include "nsIStyleContext.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsBodyFrame.h"
#include "nsStyleConsts.h"
#include "nsViewsCID.h"

static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);

NS_DEF_PTR(nsIStyleContext);

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
  mFrame = nsnull;
}

nsAbsoluteFrame::~nsAbsoluteFrame()
{
}

nsIView* nsAbsoluteFrame::CreateView(nsIView*         aContainingView,
                                     const nsRect&    aRect,
                                     nsStylePosition* aPosition,
                                     nsStyleDisplay*  aDisplay) const
{
  nsIView*  view;

  static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
  static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

  nsresult result = NSRepository::CreateInstance(kViewCID, 
                                                 nsnull, 
                                                 kIViewIID, 
                                                 (void **)&view);
  if (NS_OK == result) {
    nsIViewManager* viewManager = aContainingView->GetViewManager();

    // Initialize the view
    NS_ASSERTION(nsnull != viewManager, "null view manager");

    // See if the containing view is a scroll view
    nsIScrollableView*  scrollView = nsnull;
    nsresult            result;
    nsViewClip          clip = {0, 0, 0, 0};
    PRUint8             clipType = (aDisplay->mClipFlags & NS_STYLE_CLIP_TYPE_MASK);
    nsViewClip*         pClip = nsnull;

    // Is there a clip rect specified?
    if (NS_STYLE_CLIP_RECT == clipType) {
      if ((NS_STYLE_CLIP_LEFT_AUTO & aDisplay->mClipFlags) == 0) {
        clip.mLeft = aDisplay->mClip.left;
      }
      if ((NS_STYLE_CLIP_RIGHT_AUTO & aDisplay->mClipFlags) == 0) {
        clip.mRight = aDisplay->mClip.right;
      }
      if ((NS_STYLE_CLIP_TOP_AUTO & aDisplay->mClipFlags) == 0) {
        clip.mTop = aDisplay->mClip.top;
      }
      if ((NS_STYLE_CLIP_BOTTOM_AUTO & aDisplay->mClipFlags) == 0) {
        clip.mBottom = aDisplay->mClip.bottom;
      }
      pClip = &clip;
    }
    else if (NS_STYLE_CLIP_INHERIT == clipType) {
      // XXX need to handle clip inherit (get from parent style context)
      NS_NOTYETIMPLEMENTED("clip inherit");
    }

    PRInt32 zIndex = 0;
    if (aPosition->mZIndex.GetUnit() == eStyleUnit_Integer) {
      zIndex = aPosition->mZIndex.GetIntValue();
    } else if (aPosition->mZIndex.GetUnit() == eStyleUnit_Auto) {
      zIndex = 0;
    } else if (aPosition->mZIndex.GetUnit() == eStyleUnit_Inherit) {
      // XXX need to handle z-index "inherit"
      NS_NOTYETIMPLEMENTED("zIndex: inherit");
    }
     
    result = aContainingView->QueryInterface(kIScrollableViewIID, (void**)&scrollView);
    if (NS_OK == result) {
      nsIView* scrolledView = scrollView->GetScrolledView();

      view->Init(viewManager, aRect, scrolledView, nsnull, nsnull, nsnull,
        zIndex, pClip); 
      viewManager->InsertChild(scrolledView, view, 0);
      NS_RELEASE(scrolledView);
      NS_RELEASE(scrollView);
    } else {
      view->Init(viewManager, aRect, aContainingView, nsnull, nsnull, nsnull,
        zIndex, pClip);
      viewManager->InsertChild(aContainingView, view, 0);
    }
  
    NS_RELEASE(viewManager);
  }

  return view;
}

// Returns the offset from this frame to aFrameTo
void nsAbsoluteFrame::GetOffsetFromFrame(nsIFrame* aFrameTo, nsPoint& aOffset) const
{
  nsIFrame* frame = (nsIFrame*)this;

  aOffset.MoveTo(0, 0);
  do {
    nsPoint origin;

    frame->GetOrigin(origin);
    aOffset += origin;
    frame->GetGeometricParent(frame);
  } while ((nsnull != frame) && (frame != aFrameTo));
}

void nsAbsoluteFrame::ComputeViewBounds(nsIFrame*        aContainingBlock,
                                        nsStylePosition* aPosition,
                                        nsRect&          aRect) const
{
  nsRect    containingRect;

  // XXX We should be using the inner rect, and not just the bounding rect.
  // Because of the way the frame sizing protocol works (it's top-down, and
  // the size of a container is set after reflowing its children), get the
  // rect from the containing block's view
  nsIView*  containingView;
  aContainingBlock->GetView(containingView);
  containingView->GetBounds(containingRect);
  NS_RELEASE(containingView);
  containingRect.x = containingRect.y = 0;

  // Compute the offset and size of the view based on the position properties
  // and the inner rect of the containing block
  nsStylePosition*  position = (nsStylePosition*)mStyleContext->GetData(eStyleStruct_Position);

  // If either the left or top are 'auto' then get the offset of our frame from
  // the containing block
  nsPoint offset;  // offset from containing block
  if ((eStyleUnit_Auto == position->mLeftOffset.GetUnit()) ||
      (eStyleUnit_Auto == position->mTopOffset.GetUnit())) {
    GetOffsetFromFrame(aContainingBlock, offset);
  }

  // x-offset
  if (eStyleUnit_Auto == position->mLeftOffset.GetUnit()) {
    // Use the current x-offset of our frame translated into the coordinate space
    // of the containing block
    aRect.x = offset.x;
  } else if (eStyleUnit_Coord == position->mLeftOffset.GetUnit()) {
    aRect.x = position->mLeftOffset.GetCoordValue();
  } else {
    NS_ASSERTION(eStyleUnit_Percent == position->mLeftOffset.GetUnit(),
                 "unexpected offset type");
    // Percentage values refer to the width of the containing block
    aRect.x = containingRect.x + 
              (nscoord)((float)containingRect.width *
                        position->mLeftOffset.GetPercentValue());
  }

  // y-offset
  if (eStyleUnit_Auto == position->mTopOffset.GetUnit()) {
    // Use the current y-offset of our frame translated into the coordinate space
    // of the containing block
    aRect.y = offset.y;
  } else if (eStyleUnit_Coord == position->mTopOffset.GetUnit()) {
    aRect.y = position->mTopOffset.GetCoordValue();
  } else {
    NS_ASSERTION(eStyleUnit_Percent == position->mTopOffset.GetUnit(),
                 "unexpected offset type");
    // Percentage values refer to the height of the containing block
    aRect.y = containingRect.y + 
              (nscoord)((float)containingRect.height *
                        position->mTopOffset.GetPercentValue());
  }

  // width
  if (eStyleUnit_Auto == position->mWidth.GetUnit()) {
    // Use the right-edge of the containing block
    aRect.width = containingRect.width - aRect.x;
  } else if (eStyleUnit_Coord == position->mWidth.GetUnit()) {
    aRect.width = position->mWidth.GetCoordValue();
  } else {
    NS_ASSERTION(eStyleUnit_Percent == position->mWidth.GetUnit(),
                 "unexpected width type");
    aRect.width = (nscoord)((float)containingRect.width * 
                            position->mWidth.GetPercentValue());
  }

  // height
  if (eStyleUnit_Auto == position->mHeight.GetUnit()) {
    // Allow it to be as high as it wants
    aRect.height = NS_UNCONSTRAINEDSIZE;
  } else if (eStyleUnit_Coord == position->mHeight.GetUnit()) {
    aRect.height = position->mHeight.GetCoordValue();
  } else {
    NS_ASSERTION(eStyleUnit_Percent == position->mHeight.GetUnit(),
                 "unexpected height type");
    aRect.height = (nscoord)((float)containingRect.height * 
                             position->mHeight.GetPercentValue());
  }
}

nsIFrame* nsAbsoluteFrame::GetContainingBlock() const
{
  // Look for a containing frame that is absolutely positioned. If we don't
  // find one then use the initial containg block which is the root frame
  nsIFrame* lastFrame = (nsIFrame*)this;
  nsIFrame* result;

  GetContentParent(result);
  while (nsnull != result) {
    nsStylePosition* position;

    // Get the style data
    result->GetStyleData(eStyleStruct_Position, (nsStyleStruct*&)position);

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
      // Create the absolutely positioned item as a pseudo-frame child. We'll
      // also create a view
      nsIContentDelegate* delegate = mContent->GetDelegate(aPresContext);
  
      nsresult rv = delegate->CreateFrame(aPresContext, mContent, this,
                                          mStyleContext, mFrame);
      NS_RELEASE(delegate);
      if (NS_OK != rv) {
        return rv;
      }
    }

    // Get the containing block, and its associated view
    nsIFrame* containingBlock = GetContainingBlock();
    
    // Use the position properties to determine the offset and size
    nsStylePosition*  position = (nsStylePosition*)mStyleContext->GetData(eStyleStruct_Position);
    nsRect            rect;

    ComputeViewBounds(containingBlock, position, rect);

    // Create a view for the frame
    nsStyleDisplay*  display = (nsStyleDisplay*)mStyleContext->GetData(eStyleStruct_Display);
    nsIView*         containingView;
    containingBlock->GetView(containingView);
    nsIView*         view = CreateView(containingView, rect, position, display);
    NS_RELEASE(containingView);

    mFrame->SetView(view);  
    NS_RELEASE(view);

    // Resize reflow the absolutely positioned element
    nsSize  availSize(rect.width, rect.height);

    if (NS_STYLE_OVERFLOW_VISIBLE == display->mOverflow) {
      // Don't constrain the height since the container should be enlarged to
      // contain overflowing frames
      availSize.height = NS_UNCONSTRAINEDSIZE;
    }

    nsReflowMetrics desiredSize(nsnull);
    nsReflowState   reflowState(eReflowReason_Initial, availSize);
    mFrame->WillReflow(*aPresContext);
    mFrame->Reflow(aPresContext, desiredSize, reflowState, aStatus);

    // Figure out what size to actually use. If the position style is 'auto' or
    // the container should be enlarged to contain overflowing frames then use
    // the desired size
    if ((eStyleUnit_Auto == position->mWidth.GetUnit()) ||
        ((desiredSize.width > availSize.width) &&
         (NS_STYLE_OVERFLOW_VISIBLE == display->mOverflow))) {
      rect.width = desiredSize.width;
    }
    if (eStyleUnit_Auto == position->mHeight.GetUnit()) {
      rect.height = desiredSize.height;
    }
    mFrame->SizeTo(rect.width, rect.height);
    mFrame->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);
  }

  // Return our desired size as (0, 0)
  return nsFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

NS_METHOD
nsAbsoluteFrame::ChildCount(PRInt32& aChildCount) const
{
  aChildCount = 1;
  return NS_OK;
}

NS_METHOD
nsAbsoluteFrame::ChildAt(PRInt32 aIndex, nsIFrame*& aFrame) const
{
  aFrame = (0 == aIndex) ? mFrame : nsnull;
  return NS_OK;
}

NS_METHOD
nsAbsoluteFrame::IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const
{
  aIndex = (aChild == mFrame) ? 0 : -1;
  return NS_OK;
}

NS_METHOD
nsAbsoluteFrame::FirstChild(nsIFrame*& aFirstChild) const
{
  aFirstChild = mFrame;
  return NS_OK;
}

NS_METHOD
nsAbsoluteFrame::NextChild(const nsIFrame* aChild, nsIFrame*& aNextChild) const
{
  aNextChild = nsnull;
  return NS_OK;
}

NS_METHOD
nsAbsoluteFrame::PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const
{
  aPrevChild = nsnull;
  return NS_OK;
}

NS_METHOD
nsAbsoluteFrame::LastChild(nsIFrame*& aLastChild) const
{
  aLastChild = mFrame;
  return NS_OK;
}

NS_METHOD nsAbsoluteFrame::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);

  // Output the tag
  fputs("*absolute", out);

  PRInt32 contentIndex;
  GetContentIndex(contentIndex);
  fprintf(out, "(%d)@%p ", contentIndex, this);

  // Output the rect
  out << mRect;

  // List the absolutely positioned frame
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  fputs("<\n", out);
  mFrame->List(out, aIndent + 1);
  fputs(">\n", out);

  return NS_OK;
}
