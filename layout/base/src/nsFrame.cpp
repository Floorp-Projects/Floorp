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
#include "nsFrame.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsIStyleContext.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIPresContext.h"
#include "nsCRT.h"
#include "nsGUIEvent.h"
#include "nsStyleConsts.h"

#define DO_SELECTION 0

#if DO_SELECTION
#include "nsIDOMText.h"
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);

///////////////////////////////////
// Important Selection Variables
///////////////////////////////////
nsIFrame * mCurrentFrame   = nsnull;
PRBool     mDoingSelection = PR_FALSE;
PRBool     mDidDrag        = PR_FALSE;

#define gSelectionDebug 0

// [HACK] Foward Declarations
void BuildContentList(nsIContent*aContent);
PRBool IsInRange(nsIContent * aStartContent, nsIContent * aEndContent, nsIContent * aContent);
PRBool IsBefore(nsIContent * aNewContent, nsIContent * aCurrentContent);
nsIContent * GetPrevContent(nsIContent * aContent);
nsIContent * GetNextContent(nsIContent * aContent);
#endif


static PRBool gShowFrameBorders = PR_FALSE;

NS_LAYOUT void nsIFrame::ShowFrameBorders(PRBool aEnable)
{
  gShowFrameBorders = aEnable;
}

NS_LAYOUT PRBool nsIFrame::GetShowFrameBorders()
{
  return gShowFrameBorders;
}

static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);

nsresult
nsFrame::NewFrame(nsIFrame** aInstancePtrResult,
                  nsIContent* aContent,
                  PRInt32     aIndexInParent,
                  nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsFrame(aContent, aIndexInParent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

void* nsFrame::operator new(size_t size)
{
  void* result = new char[size];

  nsCRT::zero(result, size);
  return result;
}

nsFrame::nsFrame(nsIContent* aContent,
                 PRInt32     aIndexInParent,
                 nsIFrame*   aParent)
  : mContent(aContent), mIndexInParent(aIndexInParent), mContentParent(aParent),
    mGeometricParent(aParent)
{
  NS_ADDREF(mContent);
}

nsFrame::~nsFrame()
{
  NS_RELEASE(mContent);
  NS_IF_RELEASE(mStyleContext);
  if (nsnull != mView) {
    // Break association between view and frame
    mView->SetFrame(nsnull);
    NS_RELEASE(mView);
  }
}

/////////////////////////////////////////////////////////////////////////////
// nsISupports

nsresult nsFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kIFrameIID);
  if (aIID.Equals(kClassIID) || (aIID.Equals(kISupportsIID))) {
    *aInstancePtr = (void*)this;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsrefcnt nsFrame::AddRef(void)
{
  NS_ERROR("not supported");
  return 0;
}

nsrefcnt nsFrame::Release(void)
{
  NS_ERROR("not supported");
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

NS_METHOD nsFrame::DeleteFrame()
{
  nsIView* view = mView;
  if (nsnull == view) {
    nsIFrame* parent;
     
    GetParentWithView(parent);
    if (nsnull != parent) {
      parent->GetView(view);
    }
  }
  if (nsnull != view) {
    nsIViewManager* vm = view->GetViewManager();
    nsIPresContext* cx = vm->GetPresContext();
    // XXX Is this a really good ordering for the releases? MMP
    NS_RELEASE(vm);
    NS_RELEASE(view);
    cx->StopLoadImage(this);
    NS_RELEASE(cx);
  }

  delete this;
  return NS_OK;
}

NS_METHOD nsFrame::GetContent(nsIContent*& aContent) const
{
  if (nsnull != mContent) {
    NS_ADDREF(mContent);
  }
  aContent = mContent;
  return NS_OK;
}

NS_METHOD nsFrame::GetIndexInParent(PRInt32& aIndexInParent) const
{
  aIndexInParent = mIndexInParent;
  return NS_OK;
}

NS_METHOD nsFrame::SetIndexInParent(PRInt32 aIndexInParent)
{
  mIndexInParent = aIndexInParent;
  return NS_OK;
}

NS_METHOD nsFrame::GetStyleContext(nsIPresContext*   aPresContext,
                                   nsIStyleContext*& aStyleContext)
{
  if ((nsnull == mStyleContext) && (nsnull != aPresContext)) {
    mStyleContext = aPresContext->ResolveStyleContextFor(mContent, mGeometricParent); // XXX should be content parent???
    if (nsnull != mStyleContext) {
      DidSetStyleContext();
    }
  }
  NS_IF_ADDREF(mStyleContext);
  aStyleContext = mStyleContext;
  return NS_OK;
}

NS_METHOD nsFrame::SetStyleContext(nsIStyleContext* aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null ptr");
  if (aContext != mStyleContext) {
    NS_IF_RELEASE(mStyleContext);
    if (nsnull != aContext) {
      mStyleContext = aContext;
      NS_ADDREF(aContext);
      DidSetStyleContext();
    }
  }

  return NS_OK;
}

// Subclass hook for style post processing
NS_METHOD nsFrame::DidSetStyleContext(void)
{
  return NS_OK;
}

NS_METHOD nsFrame::GetStyleData(const nsIID& aSID, nsStyleStruct*& aStyleStruct)
{
  NS_ASSERTION(mStyleContext!=nsnull,"null style context");
  if (mStyleContext) {
    aStyleStruct = mStyleContext->GetData(aSID);
  } else {
    aStyleStruct = nsnull;
  }
  return NS_OK;
}

// Geometric and content parent member functions

NS_METHOD nsFrame::GetContentParent(nsIFrame*& aParent) const
{
  aParent = mContentParent;
  return NS_OK;
}

NS_METHOD nsFrame::SetContentParent(const nsIFrame* aParent)
{
  mContentParent = (nsIFrame*)aParent;
  return NS_OK;
}

NS_METHOD nsFrame::GetGeometricParent(nsIFrame*& aParent) const
{
  aParent = mGeometricParent;
  return NS_OK;
}

NS_METHOD nsFrame::SetGeometricParent(const nsIFrame* aParent)
{
  mGeometricParent = (nsIFrame*)aParent;
  return NS_OK;
}

// Bounding rect member functions

NS_METHOD nsFrame::GetRect(nsRect& aRect) const
{
  aRect = mRect;
  return NS_OK;
}

NS_METHOD nsFrame::GetOrigin(nsPoint& aPoint) const
{
  aPoint.x = mRect.x;
  aPoint.y = mRect.y;
  return NS_OK;
}

NS_METHOD nsFrame::GetSize(nsSize& aSize) const
{
  aSize.width = mRect.width;
  aSize.height = mRect.height;
  return NS_OK;
}

NS_METHOD nsFrame::SetRect(const nsRect& aRect)
{
  MoveTo(aRect.x, aRect.y);
  SizeTo(aRect.width, aRect.height);
  return NS_OK;
}

NS_METHOD nsFrame::MoveTo(nscoord aX, nscoord aY)
{
  if ((aX != mRect.x) || (aY != mRect.y)) {
    mRect.x = aX;
    mRect.y = aY;

    // Let the view know
    if (nsnull != mView) {
      mView->SetPosition(aX, aY);
    }
  }

  return NS_OK;
}

NS_METHOD nsFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  //I have commented out this if test since we really need to
  //always pass the fact that a resize was attempted to the
  //view system. Today is 23-apr-98. If this change still looks
  //good on or after 23-may-98, I'll kill the commented code
  //altogether. Pretty scientific, huh? MMP

//  if ((aWidth != mRect.width) || (aHeight != mRect.height)) {
    mRect.width = aWidth;
    mRect.height = aHeight;

    // Let the view know
    if (nsnull != mView) {
      mView->SetDimensions(aWidth, aHeight);
    }
//  }

  return NS_OK;
}

// Child frame enumeration

NS_METHOD nsFrame::ChildCount(PRInt32& aChildCount) const
{
  aChildCount = 0;
  return NS_OK;
}

NS_METHOD nsFrame::ChildAt(PRInt32 aIndex, nsIFrame*& aFrame) const
{
  NS_ERROR("not a container");
  aFrame = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const
{
  NS_ERROR("not a container");
  aIndex = -1;
  return NS_OK;
}

NS_METHOD nsFrame::FirstChild(nsIFrame*& aFirstChild) const
{
  aFirstChild = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::NextChild(const nsIFrame* aChild, nsIFrame*& aNextChild) const
{
  NS_ERROR("not a container");
  aNextChild = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const
{
  NS_ERROR("not a container");
  aPrevChild = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::LastChild(nsIFrame*& aLastChild) const
{
  aLastChild = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::Paint(nsIPresContext&      aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect&        aDirtyRect)
{
  return NS_OK;
}

/**
  *
 */
NS_METHOD nsFrame::HandleEvent(nsIPresContext& aPresContext, 
                               nsGUIEvent*     aEvent,
                               nsEventStatus&  aEventStatus)
{
#if DO_SELECTION

  if (aEvent->message == NS_MOUSE_MOVE && mDoingSelection ||
      aEvent->message == NS_MOUSE_LEFT_BUTTON_UP || 
      aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
  } else {
    aEventStatus = nsEventStatus_eIgnore;
    return NS_OK;
  }

  //printf("-------------------------------------------------------------\n");

  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
    if (!mDidDrag && mDoingSelection) {
      //mEndSelect = 0;
      HandleRelease(aPresContext, aEvent, aEventStatus, this);
    }
  } else if (aEvent->message == NS_MOUSE_MOVE) {
    mDidDrag = PR_TRUE;

    HandleDrag(aPresContext, aEvent, aEventStatus, this);
    //if (gSelectionDebug) printf("HandleEvent(Drag)::fTextRange %s\n", fTextRange->ToString());

  } else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    nsIContent * content;
    GetContent(content);
    BuildContentList(content);

    HandlePress(aPresContext, aEvent, aEventStatus, this);
  }
#endif

  aEventStatus = nsEventStatus_eIgnore;
  return NS_OK;
}


NS_METHOD nsFrame::HandlePress(nsIPresContext& aPresContext, 
                               nsGUIEvent*     aEvent,
                               nsEventStatus&  aEventStatus,
                               nsFrame *       aFrame)
{
#if DO_SELECTION
  nsFrame * currentFrame = aFrame;

  PRInt32 offset = 0;
  PRInt32 width  = 0;

  mDoingSelection = PR_TRUE;
  mDidDrag        = PR_FALSE;
  mCurrentFrame   = currentFrame;

  /*fStartPos = GetPosition(aPresContext, aEvent, currentFrame);

 // CalcCursorPosition(aPresContext, aEvent, currentFrame, fStartPos, width);

  // Click count is 1
  nsIContent * newContent = currentFrame->GetContent();

  // Fix this next section
  if (fTextRange == nsnull) {
    fTextRange = new nsTextRange(newContent, fStartPos, PR_TRUE,
                                 newContent, fStartPos, PR_TRUE);
  } else {
    //selection.resetContentTrackers();

    if (IsInRange(newContent, fTextRange)) {
      //??selection.resetContentTrackers();
      if (gTrackerDebug) printf("Adding split range to removed selection.\n");

      nsIContent * prevContent = GetPrevContent(newContent);
      nsIContent * nextContent = GetNextContent(newContent);


      //??addRangeToSelectionTrackers(selection, fTextRange.getStartContent(),
      //??                            prevContent, kInsertInRemoveList);
      //??addRangeToSelectionTrackers(selection, nextContent,
      //??                            fTextRange.getEndContent(), kInsertInRemoveList);
      
    } else {
      if (gTrackerDebug) printf("Adding full range to removed selection.\n");
      //addRangeToSelectionTrackers(selection, fTextRange.getStartContent(),
      //                            fTextRange.getEndContent(), kInsertInRemoveList); // removed from selection
    }
    fTextRange->SetRange(newContent, fStartPos, PR_TRUE,
                         newContent, fStartPos, PR_TRUE);
  }
  if (fStartTextPoint == nsnull) {
    fStartTextPoint = new nsTextPoint(nsnull, 0, PR_FALSE);
  }
  if (fEndTextPoint == nsnull) {
    fEndTextPoint = new nsTextPoint(nsnull, 0, PR_FALSE);
  }
  fTextRange->GetStartTextPoint(fStartTextPoint);
  fTextRange->GetEndTextPoint(fEndTextPoint);

  // [TODO] Recycle sub-selections here?
  ////fSubSelection = new SubSelection(fTextRange);
  ////selection.setSelection(fSubSelection);
  //aPart.setSelection(fTextRange);
  //updateSelection();
  ////selection.resetContentTrackers();

  //addRangeToSelectionTrackers(selection, newContent, nsnull, kInsertInAddList);

  //// [TODO] Set position of caret at start point,
  //// [TODO] Turn on caret and have it rendered
  ////printf("Selection Start Point set.");

  fDoingSelection = PR_TRUE;
  mCurrentFrame   = currentFrame;

  //buildContentTree(currentFrame);

  //fCurrentParentContentList = eventStrategy.getParentList();
  printf("HandleEvent::fTextRange %s\n", fTextRange->ToString());

  // Force Update
  if (FORCE_SELECTION_UPDATE) {
    nsRect rect;
    GetRect(rect);
    nsIView * view = GetView();
    if (view == nsnull) {
      nsIFrame * frame = GetParentWithView();
      view = GetViewWithWindow(frame->GetView());
    }
    rect.height *= 2;
    nsIViewManager      * viewMgr       = view->GetViewManager();
    nsIDeviceContext    * devContext    = aPresContext.GetDeviceContext();
    nsIRenderingContext * renderContext = devContext->CreateRenderingContext(view);
    viewMgr->Refresh(view, renderContext, &rect, NS_VMREFRESH_IMMEDIATE|NS_VMREFRESH_DOUBLE_BUFFER);
    printf("***** %d\n", rect.height);
    NS_RELEASE(renderContext);
    NS_RELEASE(devContext);
    NS_RELEASE(view);
  }*/

#endif
  aEventStatus = nsEventStatus_eIgnore;
  return NS_OK;

}

NS_METHOD nsFrame::HandleDrag(nsIPresContext& aPresContext, 
                              nsGUIEvent*     aEvent,
                              nsEventStatus&  aEventStatus,
                              nsFrame *       aFrame)
{
#if DO_SELECTION
  PRBool isNewFrame = PR_FALSE; // for testing (rcs)

  mDidDrag = PR_TRUE;

  if (aFrame != nsnull) {

    // Check to see if we have changed frame
    if (aFrame != mCurrentFrame) {
      isNewFrame = PR_TRUE;
      // We are in a new Frame!
      if (gSelectionDebug) printf("HandleDrag::Different Frame in selection!\n");

      // Get Content for current Frame
      nsIContent * currentContent;
      mCurrentFrame->GetContent(currentContent);

      // Get Content for New Frame
      nsIContent * newContent;
      aFrame->GetContent(newContent);

      // Check to see if we are still in the same Content
      if (currentContent == newContent) {
        if (gSelectionDebug) printf("HandleDrag::New Frame, same content.\n");

        // AdjustTextPointsInNewContent(aPresContext, aEvent, newFrame);

      } else if (IsBefore(newContent, currentContent)) {
        if (gSelectionDebug) printf("HandleDrag::New Frame, is Before.\n");

        //??selection.resetContentTrackers();
        NewContentIsBefore(aPresContext, aEvent, newContent, currentContent, aFrame);

      } else { // Content is AFTER
        if (gSelectionDebug) printf("HandleDrag::New Frame, is After.\n");

        //??selection.resetContentTrackers();
        NewContentIsAfter(aPresContext, aEvent, newContent, currentContent, aFrame);
      }
      mCurrentFrame = aFrame;
    } else {
      if (gSelectionDebug) printf("HandleDrag::Same Frame.\n");

      // Same Frame as before
      //if (gSelectionDebug) printf("\nSame Start: %s\n", fStartTextPoint->ToString());
      //if (gSelectionDebug) printf("Same End:   %s\n",   fEndTextPoint->ToString());
        // [TODO] Uncomment these soon
/*
      if (fStartTextPoint->GetContent() == fEndTextPoint->GetContent()) {
        if (gSelectionDebug) printf("Start & End Frame are the same: \n");
        //AdjustTextPointsInSameContent(aPresContext, aEvent);
      } else {
        if (gSelectionDebug) printf("Start & End Frame are different: \n");

        // Get Content for New Frame
        nsIContent * newContent;
        newFrame->GetContent(newContent);
        PRInt32 newPos = -1;

        newPos = GetPosition(aPresContext, aEvent, newFrame);

        if (newContent == fStartTextPoint->GetContent()) {
          if (gSelectionDebug) printf("New Content equals Start Content\n");
          fStartTextPoint->SetOffset(newPos);
          fTextRange->SetStartPoint(fStartTextPoint);
        } else if (newContent == fEndTextPoint->GetContent()) {
          if (gSelectionDebug) printf("New Content equals End Content\n");
          fEndTextPoint->SetOffset(newPos);
          fTextRange->SetEndPoint(fEndTextPoint);
        } else {
          if (gSelectionDebug) printf("=====\n=====\n=====\n=====\n=====\n=====\n Should NOT be here.\n");
        }

        //if (gSelectionDebug) printf("*Same Start: "+fStartTextPoint->GetOffset()+
        //                               " "+fStartTextPoint->IsAnchor()+
        //                               "  End: "+fEndTextPoint->GetOffset()  +
        //                               " "+fEndTextPoint->IsAnchor());
      }*/
      //??doDragRepaint(mCurrentFrame);
    }
  }
#endif

  aEventStatus = nsEventStatus_eIgnore;
  return NS_OK;
}

NS_METHOD nsFrame::HandleRelease(nsIPresContext& aPresContext, 
                                 nsGUIEvent*     aEvent,
                                 nsEventStatus&  aEventStatus,
                                 nsFrame *       aFrame)
{
#if DO_SELECTION
  mDoingSelection = PR_FALSE;
#endif
  aEventStatus = nsEventStatus_eIgnore;
  return NS_OK;
}


NS_METHOD nsFrame::GetCursorAt(nsIPresContext& aPresContext,
                               const nsPoint&  aPoint,
                               nsIFrame**      aFrame,
                               PRInt32&        aCursor)
{
  aCursor = NS_STYLE_CURSOR_INHERIT;
  return NS_OK;
}

// Resize and incremental reflow

NS_METHOD nsFrame::ResizeReflow(nsIPresContext*  aPresContext,
                                nsReflowMetrics& aDesiredSize,
                                const nsSize&    aMaxSize,
                                nsSize*          aMaxElementSize,
                                ReflowStatus&    aStatus)
{
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.descent = 0;
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }
  aStatus = frComplete;
  return NS_OK;
}

NS_METHOD nsFrame::JustifyReflow(nsIPresContext* aPresContext,
                                 nscoord         aAvailableSpace)
{
  return NS_OK;
}

NS_METHOD nsFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                     nsReflowMetrics& aDesiredSize,
                                     const nsSize&    aMaxSize,
                                     nsReflowCommand& aReflowCommand,
                                     ReflowStatus&    aStatus)
{
  NS_ERROR("not a reflow command handler");
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.descent = 0;
  aStatus = frComplete;
  return NS_OK;
}

NS_METHOD nsFrame::ContentAppended(nsIPresShell*   aShell,
                                   nsIPresContext* aPresContext,
                                   nsIContent*     aContainer)
{
  return NS_OK;
}

NS_METHOD nsFrame::ContentInserted(nsIPresShell*   aShell,
                                   nsIPresContext* aPresContext,
                                   nsIContent*     aContainer,
                                   nsIContent*     aChild,
                                   PRInt32         aIndexInParent)
{
  return NS_OK;
}

NS_METHOD nsFrame::ContentReplaced(nsIPresShell*   aShell,
                                   nsIPresContext* aPresContext,
                                   nsIContent*     aContainer,
                                   nsIContent*     aOldChild,
                                   nsIContent*     aNewChild,
                                   PRInt32         aIndexInParent)
{
  return NS_OK;
}

NS_METHOD nsFrame::ContentDeleted(nsIPresShell*   aShell,
                                  nsIPresContext* aPresContext,
                                  nsIContent*     aContainer,
                                  nsIContent*     aChild,
                                  PRInt32         aIndexInParent)
{
  return NS_OK;
}

NS_METHOD nsFrame::GetReflowMetrics(nsIPresContext*  aPresContext,
                                    nsReflowMetrics& aMetrics)
{
  aMetrics.width = mRect.width;
  aMetrics.height = mRect.height;
  aMetrics.ascent = mRect.height;
  aMetrics.descent = 0;
  return NS_OK;
}

// Flow member functions

NS_METHOD nsFrame::IsSplittable(SplittableType& aIsSplittable) const
{
  aIsSplittable = frNotSplittable;
  return NS_OK;
}

NS_METHOD nsFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                         nsIFrame*       aParent,
                                         nsIFrame*&      aContinuingFrame)
{
  NS_ERROR("not splittable");
  aContinuingFrame = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::GetPrevInFlow(nsIFrame*& aPrevInFlow) const
{
  aPrevInFlow = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::SetPrevInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
  return NS_OK;
}

NS_METHOD nsFrame::GetNextInFlow(nsIFrame*& aNextInFlow) const
{
  aNextInFlow = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::SetNextInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
  return NS_OK;
}

NS_METHOD nsFrame::AppendToFlow(nsIFrame* aAfterFrame)
{
  NS_ERROR("not splittable");
  return NS_OK;
}

NS_METHOD nsFrame::PrependToFlow(nsIFrame* aBeforeFrame)
{
  NS_ERROR("not splittable");
  return NS_OK;
}

NS_METHOD nsFrame::RemoveFromFlow()
{
  NS_ERROR("not splittable");
  return NS_OK;
}

NS_METHOD nsFrame::BreakFromPrevFlow()
{
  NS_ERROR("not splittable");
  return NS_OK;
}

NS_METHOD nsFrame::BreakFromNextFlow()
{
  NS_ERROR("not splittable");
  return NS_OK;
}

// Associated view object
NS_METHOD nsFrame::GetView(nsIView*& aView) const
{
  aView = mView;
  NS_IF_ADDREF(aView);
  return NS_OK;
}

NS_METHOD nsFrame::SetView(nsIView* aView)
{
  NS_IF_RELEASE(mView);
  if (nsnull != aView) {
    mView = aView;
    aView->SetFrame(this);
    NS_ADDREF(aView);
  }
  return NS_OK;
}

// Find the first geometric parent that has a view
NS_METHOD nsFrame::GetParentWithView(nsIFrame*& aParent) const
{
  aParent = mGeometricParent;

  while (nsnull != aParent) {
    nsIView* parView;
     
    aParent->GetView(parView);
    if (nsnull != parView) {
      NS_RELEASE(parView);
      break;
    }
    aParent->GetGeometricParent(aParent);
  }

  return NS_OK;
}

// Returns the offset from this frame to the closest geometric parent that
// has a view. Also returns the containing view or null in case of error
NS_METHOD nsFrame::GetOffsetFromView(nsPoint& aOffset, nsIView*& aView) const
{
  nsIFrame* frame = (nsIFrame*)this;

  aView = nsnull;
  aOffset.MoveTo(0, 0);
  do {
    nsPoint origin;

    frame->GetOrigin(origin);
    aOffset += origin;
    frame->GetGeometricParent(frame);
    if (nsnull != frame) {
      frame->GetView(aView);
    }
  } while ((nsnull != frame) && (nsnull == aView));

  return NS_OK;
}

NS_METHOD nsFrame::GetWindow(nsIWidget*& aWindow) const
{
  nsIFrame* frame = (nsIFrame*)this;

  aWindow = nsnull;
  while (nsnull != frame) {
    nsIView* view;
     
    frame->GetView(view);
    if (nsnull != view) {
      aWindow = view->GetWidget();
      NS_RELEASE(view);
      if (nsnull != aWindow) {
        break;
      }
    }
    frame->GetParentWithView(frame);
  }
  NS_POSTCONDITION(nsnull != aWindow, "no window in frame tree");
  return NS_OK;
}

// Sibling pointer used to link together frames

NS_METHOD nsFrame::GetNextSibling(nsIFrame*& aNextSibling) const
{
  aNextSibling = mNextSibling;
  return NS_OK;
}

NS_METHOD nsFrame::SetNextSibling(nsIFrame* aNextSibling)
{
  mNextSibling = aNextSibling;
  return NS_OK;
}

// Debugging
NS_METHOD nsFrame::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);

  // Output the tag and rect
  ListTag(out);
  fputs(" ", out);
  out << mRect;
  fputs("<>\n", out);
  return NS_OK;
}

// Output the frame's tag
NS_METHOD nsFrame::ListTag(FILE* out) const
{
  nsIAtom* tag = mContent->GetTag();
  if (tag != nsnull) {
    nsAutoString buf;
    tag->ToString(buf);
    fputs(buf, out);
    NS_RELEASE(tag);
  }
  fprintf(out, "(%d)@%p", mIndexInParent, this);
  return NS_OK;
}

NS_METHOD nsFrame::VerifyTree() const
{
  return NS_OK;
}

//-----------------------------------------------------------------------------------
#if DO_SELECTION
//-----------------------------------------------------------------------------------

nsIContent * fContentArray[1024];
PRInt32      fMax = -1;

//------------------------------
void WalkTree(nsIContent * aParent) {
  fContentArray[fMax++] = aParent;
  for (PRInt32 i=0;i<aParent->ChildCount();i++) {
    nsIContent * node = aParent->ChildAt(i);
    WalkTree(node);
  }
}

//------------------------------
void PrintContent() {
  //static NS_DEFINE_IID(kITextContentIID, NS_ITEXTCONTENT_IID);
  static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);

  nsIDOMText * textContent;

  for (int i=0;i<fMax;i++) {
    nsresult status = fContentArray[i]->QueryInterface(kIDOMTextIID, (void**) &textContent);
    if (NS_OK == status) {
      nsString text;
      //textContent->GetText(text, 0, textContent->GetLength());
      textContent->GetData(text);
      char * str = text.ToNewCString();
      printf("%i [%s]\n", i, str);
      delete str;
    }
  }
}


//------------------------------
void BuildContentList(nsIContent * Start) {

  if (fMax > -1) {
    return;
  }

  fMax = 0;
  nsIContent * node   = Start;
  nsIContent * parent = node->GetParent();
  // root content
  while (parent != nsnull) {
    parent = node->GetParent();
    if (parent != nsnull) {
      node = parent;
    }
  } // while
  //fContentArray = new nsIContent[1024];
  WalkTree(node);
  PrintContent();
}


//--------------------------------------------------------------------------
// Determines whether a piece of content in question is inbetween the two 
// piece of content
//--------------------------------------------------------------------------
PRBool IsInRange(nsIContent * aStartContent,
                            nsIContent * aEndContent,
                            nsIContent * aContent) {
  // Start and End Content is the same, so check against the start
  if (aStartContent == aEndContent) {
    return PRBool(aContent == aStartContent);
  }

  // Check to see if it is equal to the start or the end
  if (aContent == aStartContent || aContent == aEndContent) {
    return PR_TRUE;
  }

  PRBool foundStart = PR_FALSE;
  if (fMax == -1) {
    BuildContentList(aContent);
  }

  for (PRInt32 i=0;i<fMax;i++) {
    nsIContent * node = (nsIContent *)fContentArray[i];
    if (node == aStartContent) {
      foundStart = PR_TRUE;
    } else if (aContent == node) {
      return foundStart;
    } else if (aEndContent == node) {
      return PR_FALSE;
    }
  }
  return PR_FALSE;
}
//--------------------------------------------------------------------------
// Determines whether a piece of content in question is inbetween the two 
// piece of content
//--------------------------------------------------------------------------
PRBool IsBefore(nsIContent * aNewContent,
                nsIContent * aCurrentContent) {

  // Start and End Content is the same, so check against the start
  if (aNewContent == aCurrentContent) {
    if (gSelectionDebug) printf("IsBefore::New content equals current content\n");
    return PR_FALSE;
  }

  if (fMax == -1) {
    BuildContentList(aCurrentContent);
  }

  for (PRInt32 i=0;i<fMax;i++) {
    nsIContent * node = (nsIContent *)fContentArray[i];
    if (node == aCurrentContent) {
      if (gSelectionDebug) printf("IsBefore::Found Current content\n");
      return PR_FALSE;
    } else if (node == aNewContent) {
      if (gSelectionDebug) printf("IsBefore::Found New content\n");
      return PR_TRUE;
    }
  }
  if (gSelectionDebug) printf("IsBefore::Didn't find either\n");
  return PR_FALSE;
}

//--------------------------------------------------------------------------
// Return the next previous piece of content
//--------------------------------------------------------------------------
nsIContent * GetPrevContent(nsIContent * aContent) {
  for (PRInt32 i=0;i<fMax;i++) {
    if (aContent == fContentArray[i]) {
      if (i == 0) {
        return nsnull;
      } else {
        return fContentArray[i-1];
      }
    }
  }
  return nsnull;
}

//--------------------------------------------------------------------------
// Return the next previous piece of content
//--------------------------------------------------------------------------
nsIContent * GetNextContent(nsIContent * aContent) {
  for (PRInt32 i=0;i<fMax;i++) {
    if (aContent == fContentArray[i]) {
      if (i == fMax-1) {
        return nsnull;
      } else {
        return fContentArray[i+1];
      }
    }
  }
  return nsnull;
}


/********************************************************
* Handles a when the cursor enters new content that is before
* the content that the cursor is currently in
*********************************************************/
void nsFrame::NewContentIsBefore(nsIPresContext& aPresContext,
                                 nsGUIEvent * aEvent,
                                 nsIContent * aNewContent,
                                 nsIContent * aCurrentContent,
                                 nsIFrame   * aNewFrame)
{
  if (gSelectionDebug) printf("New Frame, New content is before.\n");
#if 0
  // Dragging mouse to the left or backward in content
  //
  // 1) The cursor is being dragged backward in the content
  //    and the mouse is "after" the anchor in the content
  //    and the current piece of content is being removed from the selection
  //
  // This section cover two cases:
  // 2) The cursor is being dragged backward in the content
  //    and the mouse is "before" the anchor in the content
  //    and each new piece of content is being added to the selection


  // Check to see if the new content is in the selection
  if (IsInRange(fStartTextPoint->GetContent(), fEndTextPoint->GetContent(), aNewContent)) {

    // Case #1 - Remove Current Content from Selection (at End)
    if (gSelectionDebug) printf("Case #1 - (Before) New Content is in selected Range.\n");

    if (aNewContent == fStartTextPoint->GetContent()) {
      // [TODO] This is where we might have to delete from end to new content

      // Returns the new End Point, if Start and End are on the
      // same content then End Point's Cursor is set to Start's
      fEndTextPoint->SetContent(fStartTextPoint->GetContent());
      AdjustTextPointsInNewContent(aPresContext, aEvent, aNewFrame);

    } else {
      PRInt32 newPos = GetPosition(aPresContext, aEvent, aNewFrame);
      fEndTextPoint->SetPoint(aNewContent, newPos, PR_FALSE);
      fTextRange->SetEndPoint(fEndTextPoint);
    }

    // The current content is being removed from Selection
    //??addRangeToSelectionTrackers(aSelection, null, aCurrentContent, kInsertInRemoveList);
  } else {
    // Case #2 - Add new content to selection (At Start)
    if (gSelectionDebug) printf("Case #2 - (Before) New Content is NOT in selected Range. Moving Start Backward.\n");

    PRInt32 newPos = GetPosition(aPresContext, aEvent, aNewFrame);

    // Create new TextPoint and move Start Point backward
    fStartTextPoint->SetPoint(aNewContent, newPos, PR_FALSE); // position is set correctly in adjustTextPoints

    // The New Content is added to Tracker
    //??addRangeToSelectionTrackers(aSelection, aNewContent, null, kInsertInAddList);
  }
  //??doDragRepaint(fCurrentFrame);
  //??doDragRepaint(aNewFrame);
  //??if (gSelectionDebug) fTextRange.printContent();
#endif
}

/********************************************************
* Handles a when the cursor enters new content that is After
* the content that the cursor is currently in
*********************************************************/
void nsFrame::NewContentIsAfter(nsIPresContext& aPresContext,
                                nsGUIEvent * aEvent,
                                nsIContent * aNewContent,
                                nsIContent * aCurrentContent,
                                nsIFrame   * aNewFrame)
{
  if (gSelectionDebug) printf("New Frame, New content is after.\n");
#if 0

  // Dragging Mouse to the Right
  //
  // 3) The cursor is being dragged foward in the content
  //    and the mouse is "before" the anchor in the content
  //    and the current piece of content is being removed from the selection
  //
  // This section cover two cases:
  // 4) The cursor is being dragged foward in the content
  //    and the mouse is "after" the anchor in the content
  //    and each new piece of content is being added to the selection
  //

  // Check to see if the new content is in the selection
  if (IsInRange(fStartTextPoint->GetContent(), fEndTextPoint->GetContent(), aNewContent)) {

    // Case #3 - Remove Content (from Start)
    if (gSelectionDebug) printf("Case #3 - (After) New Content is in selected Range.\n");

    // [TODO] Always get nearest Text content
    PRInt32 newPos = GetPosition(aPresContext, aEvent, aNewFrame);

    // Check to see if the new Content is the same as the End Point's
    if (aNewContent == fEndTextPoint->GetContent()) {
      if (gSelectionDebug) printf("New Content matches End Point\n");

      fStartTextPoint->SetContent(aNewContent);
      AdjustTextPointsInNewContent(aPresContext, aEvent, aNewFrame);

    } else {
      if (gSelectionDebug) printf("New Content does NOT matches End Point\n");
      fStartTextPoint->SetPoint(aNewContent, newPos, PR_FALSE);
      fTextRange->SetStartPoint(fStartTextPoint);
    }

    // Remove Current Content in Tracker, but leave New Content in Selection
    //??addRangeToSelectionTrackers(aSelection, aCurrentContent, null, kInsertInRemoveList);
  } else {
    if (gSelectionDebug)
      printf("Case #4 - (After) Adding New Content\n");

    // Case #2 - Adding Content (at End)
    // The new content is not in the selection
    PRInt32 newPos = GetPosition(aPresContext, aEvent, aNewFrame);

    // Check to see if we need to create a new TextPoint and add it
    // or do we simply move the existing start or end point
    if (fStartTextPoint->GetContent() == fEndTextPoint->GetContent()) {
      if (gSelectionDebug) printf("Case #4 - Start & End Content the Same\n");
      // Move start or end point
      // Get new Cursor Poition in the new content

      if (fStartTextPoint->IsAnchor()) {
        if (gSelectionDebug) printf("Case #4 - Start is Anchor\n");
        // Since the Start is the Anchor just adjust the end
        // What is up Here????????
        if (gSelectionDebug) printf("Start: %s\n", fStartTextPoint->ToString());
        if (gSelectionDebug) printf("End:   %s\n", fEndTextPoint->ToString());
        if (gSelectionDebug) printf("---->");

        fEndTextPoint->SetPoint(aNewContent, newPos, PR_FALSE);
        fTextRange->SetEndPoint(fEndTextPoint);

        if (gSelectionDebug) printf("Start: %s\n", fStartTextPoint->ToString());
        if (gSelectionDebug) printf("End:   %s\n", fEndTextPoint->ToString());

      } else {
        if (gSelectionDebug) printf("Case #4 - Start is NOT Anchor\n");
        // Because End was the anchor, we need to set the Start Point to
        // the End's Offset and set it to be the new anchor

        int endPos = fEndTextPoint->GetOffset();
        fStartTextPoint->SetOffset(endPos);
        fStartTextPoint->SetAnchor(PR_TRUE);

        // The Start point was being moved so when it jumped to the new frame
        // we needed to make it the new End Point
        fEndTextPoint->SetPoint(aNewContent, newPos, PR_FALSE);
        fTextRange->SetRange(fStartTextPoint, fEndTextPoint);
      }
      // Same Content so don't do anything with Selection Tracker
      // Add New Content to Selection Tracker
      //??addRangeToSelectionTrackers(aSelection, aNewContent, null, kInsertInAddList);
    } else {
      if (gSelectionDebug) printf("Case #4 - Start & End Content NOT the Same\n");
      // Adjust the end point
      fEndTextPoint->SetPoint(aNewContent, newPos, PR_FALSE);
      fTextRange->SetRange(fStartTextPoint, fEndTextPoint);

      // Add New Content to Selection Tracker
     //?? addRangeToSelectionTrackers(aSelection, aNewContent, null, kInsertInAddList);
    }
  }
  //??if (gSelectionDebug) fTextRange.printContent();
#endif
}

#endif