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
#include "nsBodyFrame.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIDeviceContext.h"
#include "nsIRunaround.h"
#include "nsSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsIView.h"
#include "nsViewsCID.h"
#include "nsAbsoluteFrame.h"
#include "nsHTMLIIDs.h"
#include "nsCSSBlockFrame.h"

nsresult nsBodyFrame::NewFrame(nsIFrame** aInstancePtrResult,
                               nsIContent* aContent,
                               nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsBodyFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsBodyFrame::nsBodyFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsHTMLContainerFrame(aContent, aParentFrame)
{
  mIsPseudoFrame = IsPseudoFrame();
  mSpaceManager = new nsSpaceManager(this);
  NS_ADDREF(mSpaceManager);
}

nsBodyFrame::~nsBodyFrame()
{
  NS_RELEASE(mSpaceManager);
}

/////////////////////////////////////////////////////////////////////////////
// nsIUnknown

nsresult
nsBodyFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIAnchoredItemsIID)) {
    *aInstancePtr = (void*) ((nsIAnchoredItems*) this);
    return NS_OK;
  }
  if (aIID.Equals(kIAbsoluteItemsIID)) {
    *aInstancePtr = (void*) ((nsIAbsoluteItems*) this);
    return NS_OK;
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

NS_METHOD nsBodyFrame::Reflow(nsIPresContext*      aPresContext,
                              nsReflowMetrics&     aDesiredSize,
                              const nsReflowState& aReflowState,
                              nsReflowStatus&      aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("nsBodyFrame::Reflow");

  aStatus = NS_FRAME_COMPLETE;  // initialize out parameter

  // Do we have any children?
  if (nsnull == mFirstChild) {
    // No, create a pseudo block frame
//XXX remove this code and uncomment the assertion when the table code plays nice
#ifdef NS_DEBUG
    if (eReflowReason_Initial != aReflowState.reason) {
      printf("XXX: table code isn't setting the reflow reason properly! [nsBodyFrame.cpp]\n");
    }
#endif
//XXX    NS_ASSERTION(eReflowReason_Initial == aReflowState.reason, "bad reason");
    CreateColumnFrame(aPresContext);
  }
  else {
//XXX remove this code and uncomment the assertion when the table code plays nice
#ifdef NS_DEBUG
    if (eReflowReason_Initial == aReflowState.reason) {
      printf("XXX: table code isn't setting the reflow reason properly! [nsBodyFrame.cpp]\n");
    }
#endif
//XXX    NS_ASSERTION(eReflowReason_Initial != aReflowState.reason, "bad reason");
  }

  if (eReflowReason_Incremental == aReflowState.reason) {
  
    // The reflow command should never be target for us
#ifdef NS_DEBUG
    NS_ASSERTION(nsnull != aReflowState.reflowCommand, "null reflow command");
    nsIFrame* target;
    aReflowState.reflowCommand->GetTarget(target);
    NS_ASSERTION(target != this, "bad reflow command target");
#endif

    // Is the next frame in the reflow chain the pseudo block-frame or a
    // floating frame?
    //
    // XXX Do we want the floating frame to be a reflow target or should its
    // placeholder frame be the target? The reason this is currently happening
    // is that the placeholder frame was changed to return the floating frame
    // as a child frame. That's wrong, because the floating frame now appears
    // to be both a geometric child of the body and of the placeholder. Yuck...
    nsIFrame* next;
    aReflowState.reflowCommand->GetNext(next);
    if (mFirstChild != next) {
      // It's a floating frame that's the target. Reflow the body making it
      // look like a resize occured. This will reflow the placeholder which will
      // resize the floating frame...
      nsReflowState reflowState(next, aReflowState, aReflowState.maxSize, eReflowReason_Resize);
      return Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
    }
  }

  // Reflow the child frame
  if (nsnull != mFirstChild) {
    // Get our border/padding info
    const nsStyleSpacing* mySpacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    nsMargin  borderPadding;
    mySpacing->CalcBorderPaddingFor(this, borderPadding);
  
    // Compute the child frame's max size
    nsSize  kidMaxSize = GetColumnAvailSpace(aPresContext, borderPadding,
                                             aReflowState.maxSize);
    mSpaceManager->Translate(borderPadding.left, borderPadding.top);
  
    if (eReflowReason_Resize == aReflowState.reason) {
      // Clear any regions that are marked as unavailable
      // XXX Temporary hack until everything is incremental...
      mSpaceManager->ClearRegions();
    }

    // Get the column's desired rect
    nsIRunaround* reflowRunaround;
    nsReflowState reflowState(mFirstChild, aReflowState, kidMaxSize);
    nsRect        desiredRect;

    mFirstChild->WillReflow(*aPresContext);
    mFirstChild->MoveTo(borderPadding.left, borderPadding.top);
    mFirstChild->QueryInterface(kIRunaroundIID, (void**)&reflowRunaround);
    reflowRunaround->Reflow(aPresContext, mSpaceManager, aDesiredSize,
                            reflowState, desiredRect, aStatus);

    // If the frame is complete, then check whether there's a next-in-flow that
    // needs to be deleted
    if (NS_FRAME_IS_COMPLETE(aStatus)) {
      nsIFrame* kidNextInFlow;
       
      mFirstChild->GetNextInFlow(kidNextInFlow);
      if (nsnull != kidNextInFlow) {
        // Remove all of the childs next-in-flows
        DeleteChildsNextInFlow(mFirstChild);
      }
    }

    mSpaceManager->Translate(-borderPadding.left, -borderPadding.top);
  
    // Place and size the frame
    desiredRect.x += borderPadding.left;
    desiredRect.y += borderPadding.top;
    mFirstChild->SetRect(desiredRect);
//    mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);
  
#if XXX
    // the block knows to PropagateContentOffsets so this isn't necessary

    // Set our last content offset and whether the last content is complete
    // based on the state of the pseudo frame
    nsCSSBlockFrame* blockPseudoFrame = (nsCSSBlockFrame*)mFirstChild;
    mLastContentOffset = blockPseudoFrame->GetLastContentOffset();
    mLastContentIsComplete = blockPseudoFrame->GetLastContentIsComplete();
#endif
  
    // Reflow any absolutely positioned frames that need reflowing
    ReflowAbsoluteItems(aPresContext, aReflowState);

    // Return our desired size
    ComputeDesiredSize(desiredRect, aReflowState.maxSize, borderPadding, aDesiredSize);
  }
  else {
    aDesiredSize.width = 0;
    aDesiredSize.height = 0;
    aDesiredSize.ascent = 0;
    aDesiredSize.descent = 0;
    if (nsnull != aDesiredSize.maxElementSize) {
      aDesiredSize.maxElementSize->width = 0;
      aDesiredSize.maxElementSize->height = 0;
    }
  }
  
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("exit nsBodyFrame::Reflow: status=%d width=%d height=%d",
                      aStatus, aDesiredSize.width, aDesiredSize.height));
  return NS_OK;
}

NS_METHOD nsBodyFrame::ContentAppended(nsIPresShell*   aShell,
                                       nsIPresContext* aPresContext,
                                       nsIContent*     aContainer)
{
  NS_ASSERTION(mContent == aContainer, "bad content-appended target");

  // Pass along the notification to our pseudo frame. It will generate a
  // reflow command
  return mFirstChild->ContentAppended(aShell, aPresContext, aContainer);
}

NS_METHOD nsBodyFrame::ContentInserted(nsIPresShell*   aShell,
                                       nsIPresContext* aPresContext,
                                       nsIContent*     aContainer,
                                       nsIContent*     aChild,
                                       PRInt32         aIndexInParent)
{
  NS_ASSERTION(mContent == aContainer, "bad content-inserted target");

  // Pass along the notification to our pseudo frame that maps all the content
  return mFirstChild->ContentInserted(aShell, aPresContext, aContainer,
                                      aChild, aIndexInParent);
}

NS_METHOD
nsBodyFrame::CreateContinuingFrame(nsIPresContext*  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame)
{
  nsBodyFrame* cf = new nsBodyFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aPresContext, aParent, aStyleContext, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Helper functions

void nsBodyFrame::CreateColumnFrame(nsIPresContext* aPresContext)
{
  nsIStyleContext* styleContext =
   aPresContext->ResolvePseudoStyleContextFor(nsHTMLAtoms::columnPseudo, this);

  // Do we have a prev-in-flow?
  if (nsnull == mPrevInFlow) {
    // No, create a column pseudo frame
    NS_NewCSSBlockFrame(&mFirstChild, mContent, this);
    mChildCount = 1;
    mFirstChild->SetStyleContext(aPresContext,styleContext);
  } else {
    // Create a continuing column
    nsBodyFrame*  prevBody = (nsBodyFrame*)mPrevInFlow;
    nsIFrame* prevColumn = prevBody->mFirstChild;
    NS_ASSERTION(prevBody->ChildIsPseudoFrame(prevColumn),
                 "bad previous column");

    prevColumn->CreateContinuingFrame(aPresContext, this, styleContext,
                                      mFirstChild);
    mChildCount = 1;
  }

  NS_RELEASE(styleContext);
}

nsSize nsBodyFrame::GetColumnAvailSpace(nsIPresContext*  aPresContext,
                                        const nsMargin&  aBorderPadding,
                                        const nsSize&    aMaxSize)
{
  nsSize  result(aMaxSize);

  // If we're not being used as a pseudo frame then make adjustments
  // for border/padding and a vertical scrollbar
  if (!mIsPseudoFrame) {
    // If our width is constrained then subtract for the border/padding
    if (aMaxSize.width != NS_UNCONSTRAINEDSIZE) {
      result.width -= aBorderPadding.left +
                      aBorderPadding.right;
      if (! aPresContext->IsPaginated()) {
        nsIDeviceContext* dc = aPresContext->GetDeviceContext();
        result.width -= NS_TO_INT_ROUND(dc->GetScrollBarWidth());
        NS_RELEASE(dc);
      }
    }
    // If our height is constrained then subtract for the border/padding
    if (aMaxSize.height != NS_UNCONSTRAINEDSIZE) {
      result.height -= aBorderPadding.top +
                       aBorderPadding.bottom;
    }
  }

  return result;
}

void
nsBodyFrame::ComputeDesiredSize(const nsRect&    aDesiredRect,
                                const nsSize&    aMaxSize,
                                const nsMargin&  aBorderPadding,
                                nsReflowMetrics& aDesiredSize)
{
  // Note: Body used as a pseudo-frame shrink wraps
  aDesiredSize.height = PR_MAX(aDesiredRect.YMost(), mSpaceManager->YMost());
  aDesiredSize.width = aDesiredRect.XMost();

  // Take into account absolutely positioned elements when computing the
  // desired size
  for (PRInt32 i = 0; i < mAbsoluteItems.Count(); i++) {
    // Get the anchor frame
    nsAbsoluteFrame*  anchorFrame = (nsAbsoluteFrame*)mAbsoluteItems[i];
    nsIFrame*         absoluteFrame = anchorFrame->GetAbsoluteFrame();
    nsRect            rect;

    absoluteFrame->GetRect(rect);
    if (rect.XMost() > aDesiredSize.width) {
      aDesiredSize.width = rect.XMost();
    }
    if (rect.YMost() > aDesiredSize.height) {
      aDesiredSize.height = rect.YMost();
    }
  }

  if (!mIsPseudoFrame) {
    // Make sure we're at least as wide as our available width
    aDesiredSize.width = PR_MAX(aDesiredSize.width, aMaxSize.width);
    aDesiredSize.height += aBorderPadding.top +
                           aBorderPadding.bottom;
  }
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
}

// Add the frame to the end of the child list
void nsBodyFrame::AddFrame(nsIFrame* aFrame)
{
  nsIFrame* lastChild;

  LastChild(lastChild);
  lastChild->SetNextSibling(aFrame);
  aFrame->SetNextSibling(nsnull);
  mChildCount++;
}

/////////////////////////////////////////////////////////////////////////////
// nsIAnchoredItems

void nsBodyFrame::AddAnchoredItem(nsIFrame*         aAnchoredItem,
                                  AnchoringPosition aPosition,
                                  nsIFrame*         aContainer)
{
  // Set the geometric parent and add the anchored frame to the child list
  aAnchoredItem->SetGeometricParent(this);
  AddFrame(aAnchoredItem);
}

void nsBodyFrame::RemoveAnchoredItem(nsIFrame* aAnchoredItem)
{
  NS_PRECONDITION(IsChild(aAnchoredItem), "bad anchored item");
  NS_ASSERTION(aAnchoredItem != mFirstChild, "unexpected anchored item");
  // Remove the anchored item from the child list
  // XXX Implement me
  mChildCount--;
}

/////////////////////////////////////////////////////////////////////////////
// nsIAbsoluteItems

NS_METHOD nsBodyFrame::AddAbsoluteItem(nsAbsoluteFrame* aAnchorFrame)
{
  // Add the absolute anchor frame to our list of absolutely positioned
  // items.
  mAbsoluteItems.AppendElement(aAnchorFrame);
  return NS_OK;
}

NS_METHOD nsBodyFrame::RemoveAbsoluteItem(nsAbsoluteFrame* aAnchorFrame)
{
  NS_NOTYETIMPLEMENTED("removing an absolutely positioned frame");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Called at the end of the Reflow() member function so we can process
// any abolutely positioned items that need to be reflowed
void nsBodyFrame::ReflowAbsoluteItems(nsIPresContext*      aPresContext,
                                      const nsReflowState& aReflowState)
{
  for (PRInt32 i = 0; i < mAbsoluteItems.Count(); i++) {
    // Get the anchor frame and its absolutely positioned frame
    nsAbsoluteFrame*  anchorFrame = (nsAbsoluteFrame*)mAbsoluteItems[i];
    nsIFrame*         absoluteFrame = anchorFrame->GetAbsoluteFrame();
    PRBool            placeFrame = PR_FALSE;
    PRBool            reflowFrame = PR_FALSE;
    nsReflowReason    reflowReason = eReflowReason_Resize;

    // Get its style information
    const nsStyleDisplay*  display;
    const nsStylePosition* position;
     
    absoluteFrame->GetStyleData(eStyleStruct_Display, (nsStyleStruct*&)display);
    absoluteFrame->GetStyleData(eStyleStruct_Position, (nsStyleStruct*&)position);

    // See whether the frame is a newly added frame
    nsIFrame* parent;
    absoluteFrame->GetGeometricParent(parent);
    if (parent != this) {
      // The absolutely position item hasn't yet been added to our child list
      absoluteFrame->SetGeometricParent(this);

      // Add the absolutely positioned frame to the end of the child list
      AddFrame(absoluteFrame);

      // Make sure the frame has a view
      nsIView*  view;
      absoluteFrame->GetView(view);
      if (nsnull == view) {
        view = CreateAbsoluteView(position, display);
        absoluteFrame->SetView(view);  
      }
      NS_RELEASE(view);

      // See whether this is the frame's initial reflow
      nsFrameState  frameState;
      absoluteFrame->GetFrameState(frameState);
      if (frameState & NS_FRAME_FIRST_REFLOW) {
        reflowReason = eReflowReason_Initial;
      }

      // We need to place and reflow the absolutely positioned frame
      placeFrame = reflowFrame = PR_TRUE;

    } else {
      // We need to place the frame if the left-offset or the top-offset are
      // auto or a percentage
      if ((eStyleUnit_Coord != position->mLeftOffset.GetUnit()) ||
          (eStyleUnit_Coord != position->mTopOffset.GetUnit())) {
        placeFrame = PR_TRUE;
      }

      // We need to reflow the frame if its width or its height is auto or
      // a percentage
      if ((eStyleUnit_Coord != position->mWidth.GetUnit()) ||
          (eStyleUnit_Coord != position->mHeight.GetUnit())) {
        reflowFrame = PR_TRUE;
      }
    }

    if (placeFrame || reflowFrame) {
      // Get the rect for the absolutely positioned element
      nsRect  rect;
      ComputeAbsoluteFrameBounds(anchorFrame, aReflowState, position, rect);
  
      absoluteFrame->WillReflow(*aPresContext);
      absoluteFrame->MoveTo(rect.x, rect.y);

      if (reflowFrame) {
        // Resize reflow the absolutely positioned element
        nsSize  availSize(rect.width, rect.height);
      
        if (NS_STYLE_OVERFLOW_VISIBLE == display->mOverflow) {
          // Don't constrain the height since the container should be enlarged
          // to contain overflowing frames
          availSize.height = NS_UNCONSTRAINEDSIZE;
        }
      
        nsReflowMetrics desiredSize(nsnull);
        nsReflowState   reflowState(absoluteFrame, aReflowState, availSize,
                                    reflowReason);
        nsReflowStatus  status;
        absoluteFrame->Reflow(aPresContext, desiredSize, reflowState, status);
        
        // Figure out what size to actually use. If we let the child choose its
        // size, then use what the child requested. Otherwise, use the value
        // specified in the style information
        if ((eStyleUnit_Auto == position->mWidth.GetUnit()) ||
            ((desiredSize.width > availSize.width) &&
             (NS_STYLE_OVERFLOW_VISIBLE == display->mOverflow))) {
          rect.width = desiredSize.width;
        }
        if ((eStyleUnit_Auto == position->mHeight.GetUnit()) ||
            (NS_UNCONSTRAINEDSIZE == rect.height) ||
            ((desiredSize.height > rect.height) &&
             (NS_STYLE_OVERFLOW_VISIBLE == display->mOverflow))) {
          rect.height = desiredSize.height;
        }
        absoluteFrame->SetRect(rect);
      }
    }
  }
}

nsIView* nsBodyFrame::CreateAbsoluteView(const nsStylePosition* aPosition,
                                         const nsStyleDisplay*  aDisplay) const
{
  nsIView*  view;

  static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
  static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

  // Create the view
  nsresult result = NSRepository::CreateInstance(kViewCID, 
                                                 nsnull, 
                                                 kIViewIID, 
                                                 (void **)&view);
  if (NS_OK == result) {
    // XXX Even though since we're absolutely positioned we should have a view,
    // we might not...
    nsIView*  containingView;
    GetView(containingView);
    if (nsnull == containingView) {
      nsPoint offset;
      GetOffsetFromView(offset, containingView);
    }
  
    // Initialize the view
    nsIViewManager* viewManager = containingView->GetViewManager();
    NS_ASSERTION(nsnull != viewManager, "null view manager");

    // Is there a clip rect specified?
    nsViewClip    clip = {0, 0, 0, 0};
    PRUint8       clipType = (aDisplay->mClipFlags & NS_STYLE_CLIP_TYPE_MASK);
    nsViewClip*   pClip = nsnull;

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

    // Get the z-index to use
    PRInt32 zIndex = 0;
    if (aPosition->mZIndex.GetUnit() == eStyleUnit_Integer) {
      zIndex = aPosition->mZIndex.GetIntValue();
    } else if (aPosition->mZIndex.GetUnit() == eStyleUnit_Auto) {
      zIndex = 0;
    } else if (aPosition->mZIndex.GetUnit() == eStyleUnit_Inherit) {
      // XXX need to handle z-index "inherit"
      NS_NOTYETIMPLEMENTED("zIndex: inherit");
    }
     
    // Initialize the view with a size of (0, 0). When we're done reflowing
    // the frame the view will be sized and positioned
    view->Init(viewManager, nsRect(0, 0, 0, 0), containingView, nsnull,
               nsnull, nsnull, zIndex, pClip);
    viewManager->InsertChild(containingView, view, 0);
    NS_RELEASE(viewManager);
    NS_RELEASE(containingView);
  }

  return view;
}

// Translate aPoint from aFrameFrom's coordinate space to our coordinate space
void nsBodyFrame::TranslatePoint(nsIFrame* aFrameFrom, nsPoint& aPoint) const
{
  nsIFrame* parent;

  aFrameFrom->GetGeometricParent(parent);
  while ((nsnull != parent) && (parent != (nsIFrame*)this)) {
    nsPoint origin;

    parent->GetOrigin(origin);
    aPoint += origin;
    parent->GetGeometricParent(parent);
  }
}

void nsBodyFrame::ComputeAbsoluteFrameBounds(nsIFrame*              aAnchorFrame,
                                             const nsReflowState&   aReflowState,
                                             const nsStylePosition* aPosition,
                                             nsRect&                aRect) const
{
  // Compute the offset and size of the view based on the position properties,
  // and the inner rect of the containing block (which we get from the reflow
  // state)
  //
  // If either the left or top are 'auto' then get the offset of the anchor
  // frame from this frame
  nsPoint offset;
  if ((eStyleUnit_Auto == aPosition->mLeftOffset.GetUnit()) ||
      (eStyleUnit_Auto == aPosition->mTopOffset.GetUnit())) {
    aAnchorFrame->GetOrigin(offset);
    TranslatePoint(aAnchorFrame, offset);
  }

  // left-offset
  if (eStyleUnit_Auto == aPosition->mLeftOffset.GetUnit()) {
    // Use the current x-offset of the anchor frame translated into our
    // coordinate space
    aRect.x = offset.x;
  } else if (eStyleUnit_Coord == aPosition->mLeftOffset.GetUnit()) {
    aRect.x = aPosition->mLeftOffset.GetCoordValue();
  } else {
    NS_ASSERTION(eStyleUnit_Percent == aPosition->mLeftOffset.GetUnit(),
                 "unexpected offset type");
    // Percentage values refer to the width of the containing block. If the
    // width is unconstrained then just use 0
    if (NS_UNCONSTRAINEDSIZE == aReflowState.maxSize.width) {
      aRect.x = 0;
    } else {
      aRect.x = (nscoord)((float)aReflowState.maxSize.width *
                          aPosition->mLeftOffset.GetPercentValue());
    }
  }

  // top-offset
  if (eStyleUnit_Auto == aPosition->mTopOffset.GetUnit()) {
    // Use the current y-offset of the anchor frame translated into our
    // coordinate space
    aRect.y = offset.y;
  } else if (eStyleUnit_Coord == aPosition->mTopOffset.GetUnit()) {
    aRect.y = aPosition->mTopOffset.GetCoordValue();
  } else {
    NS_ASSERTION(eStyleUnit_Percent == aPosition->mTopOffset.GetUnit(),
                 "unexpected offset type");
    // Percentage values refer to the height of the containing block. If the
    // height is unconstrained then interpret it like 'auto'
    if (NS_UNCONSTRAINEDSIZE == aReflowState.maxSize.height) {
      aRect.y = offset.y;
    } else {
      aRect.y = (nscoord)((float)aReflowState.maxSize.height *
                          aPosition->mTopOffset.GetPercentValue());
    }
  }

  // XXX We aren't properly handling 'auto' width and height
  // XXX The width/height represent the size of the content area only, and not
  // the frame size...

  // width
  if (eStyleUnit_Auto == aPosition->mWidth.GetUnit()) {
    // Use the right-edge of the containing block
    aRect.width = aReflowState.maxSize.width;
  } else if (eStyleUnit_Coord == aPosition->mWidth.GetUnit()) {
    aRect.width = aPosition->mWidth.GetCoordValue();
  } else {
    NS_ASSERTION(eStyleUnit_Percent == aPosition->mWidth.GetUnit(),
                 "unexpected width type");
    // Percentage values refer to the width of the containing block
    if (NS_UNCONSTRAINEDSIZE == aReflowState.maxSize.width) {
      aRect.width = NS_UNCONSTRAINEDSIZE;
    } else {
      aRect.width = (nscoord)((float)aReflowState.maxSize.width *
                              aPosition->mWidth.GetPercentValue());
    }
  }

  // height
  if (eStyleUnit_Auto == aPosition->mHeight.GetUnit()) {
    // Allow it to be as high as it wants
    aRect.height = NS_UNCONSTRAINEDSIZE;
  } else if (eStyleUnit_Coord == aPosition->mHeight.GetUnit()) {
    aRect.height = aPosition->mHeight.GetCoordValue();
  } else {
    NS_ASSERTION(eStyleUnit_Percent == aPosition->mHeight.GetUnit(),
                 "unexpected height type");
    // Percentage values refer to the height of the containing block. If the
    // height is unconstrained, then interpret it like 'auto' and make the
    // height unconstrained
    if (NS_UNCONSTRAINEDSIZE == aReflowState.maxSize.height) {
      aRect.height = NS_UNCONSTRAINEDSIZE;
    } else {
      aRect.height = (nscoord)((float)aReflowState.maxSize.height * 
                               aPosition->mHeight.GetPercentValue());
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// nsHTMLContainerFrame

// XXX use same logic as block frame?
PRIntn nsBodyFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}

/////////////////////////////////////////////////////////////////////////////
// Diagnostics

NS_METHOD nsBodyFrame::VerifyTree() const
{
#ifdef NS_DEBUG
  NS_ASSERTION(0 == (mState & NS_FRAME_IN_REFLOW), "frame is in reflow");

  // Check our child count
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
  nsIFrame* lastChild;
   
  LastChild(lastChild);
  if (len != 0) {
    NS_ASSERTION(nsnull != lastChild, "bad last child");
  }
  NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");

  // Make sure our content offsets are sane
  NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad offsets");

  // Verify child content offsets
  nsIFrame* child = mFirstChild;
  while (nsnull != child) {
    // Make sure that the child's tree is valid
    child->VerifyTree();
    child->GetNextSibling(child);
  }

  // Make sure that our flow blocks offsets are all correct
  if (nsnull == mPrevInFlow) {
    PRInt32 nextOffset = NextChildOffset();
    nsBodyFrame* nextInFlow = (nsBodyFrame*) mNextInFlow;
    while (nsnull != nextInFlow) {
      NS_ASSERTION(0 != nextInFlow->mChildCount, "empty next-in-flow");
      NS_ASSERTION(nextInFlow->GetFirstContentOffset() == nextOffset,
                    "bad next-in-flow first offset");
      nextOffset = nextInFlow->NextChildOffset();
      nextInFlow = (nsBodyFrame*) nextInFlow->mNextInFlow;
    }
  }
#endif
  return NS_OK;
}

