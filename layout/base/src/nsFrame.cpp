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
#include "nsReflowCommand.h"
#include "nsIPresShell.h"
#include "prlog.h"
#include "prprf.h"
#include <stdarg.h>

#define DO_SELECTION 0

#include "nsIDOMText.h"
#include "nsSelectionRange.h"
#include "nsDocument.h"
#include "nsIDeviceContext.h"
#include "nsIPresShell.h"

// Kludged Content stuff
nsIFrame   * fFrameArray[1024];
nsIContent * fContentArray[1024];
PRInt32      fMax = -1;

// Tracker Data
#define kInsertInRemoveList 0
#define kInsertInAddList    1

nsIContent * fTrackerContentArrayRemoveList[1024];
PRInt32      fTrackerRemoveListMax = 0;
nsIContent * fTrackerContentArrayAddList[1024];
PRInt32      fTrackerAddListMax = 0;

// Some Misc #defines
#define gSelectionDebug 0
#define FORCE_SELECTION_UPDATE 1
PRBool  gTrackerDebug   = PR_FALSE;
PRBool  gCalcDebug      = PR_FALSE;

// [HACK] Foward Declarations
void BuildContentList(nsIContent*aContent);
PRBool IsInRange(nsIContent * aStartContent, nsIContent * aEndContent, nsIContent * aContent);
PRBool IsBefore(nsIContent * aNewContent, nsIContent * aCurrentContent);
nsIContent * GetPrevContent(nsIContent * aContent);
nsIContent * GetNextContent(nsIContent * aContent);
void RefreshContentFrames(nsIPresContext& aPresContext, nsIContent * aStartContent, nsIContent * aEndContent);
void ForceDrawFrame(nsFrame * aFrame);
void resetContentTrackers();
void RefreshFromContentTrackers(nsIPresContext& aPresContext);
void addRangeToSelectionTrackers(nsIContent * aStartContent, nsIContent * aEndContent, PRUint32 aType);
void PrintIndex(char * aStr, nsIContent * aContent);




// Initialize Global Selection Data
nsIFrame *  nsFrame::mCurrentFrame   = nsnull;
PRBool      nsFrame::mDoingSelection = PR_FALSE;
PRBool      nsFrame::mDidDrag        = PR_FALSE;
PRInt32     nsFrame::mStartPos       = 0;

// Selection data is valid only from the Mouse Press to the Mouse Release
nsSelectionRange * nsFrame::mSelectionRange      = nsnull;
nsISelection     * nsFrame::mSelection           = nsnull;

nsSelectionPoint * nsFrame::mStartSelectionPoint = nsnull;
nsSelectionPoint * nsFrame::mEndSelectionPoint   = nsnull;

//----------------------------------------------------------------------

static PRBool gShowFrameBorders = PR_FALSE;

NS_LAYOUT void nsIFrame::ShowFrameBorders(PRBool aEnable)
{
  gShowFrameBorders = aEnable;
}

NS_LAYOUT PRBool nsIFrame::GetShowFrameBorders()
{
  return gShowFrameBorders;
}

/**
 * Note: the log module is created during library initialization which
 * means that you cannot perform logging before then.
 */
PRLogModuleInfo* nsIFrame::gLogModule = PR_NewLogModule("frame");

static PRLogModuleInfo* gFrameVerifyTreeLogModuleInfo;

static PRBool gFrameVerifyTreeEnable = PRBool(0x55);

NS_LAYOUT PRBool
nsIFrame::GetVerifyTreeEnable()
{
#ifdef NS_DEBUG
  if (gFrameVerifyTreeEnable == PRBool(0x55)) {
    if (nsnull == gFrameVerifyTreeLogModuleInfo) {
      gFrameVerifyTreeLogModuleInfo = PR_NewLogModule("frameverifytree");
      gFrameVerifyTreeEnable = 0 != gFrameVerifyTreeLogModuleInfo->level;
      printf("Note: frameverifytree is %sabled\n",
             gFrameVerifyTreeEnable ? "en" : "dis");
    }
  }
#endif
  return gFrameVerifyTreeEnable;
}

NS_LAYOUT void
nsIFrame::SetVerifyTreeEnable(PRBool aEnabled)
{
  gFrameVerifyTreeEnable = aEnabled;
}

NS_LAYOUT PRLogModuleInfo*
nsIFrame::GetLogModuleInfo()
{
  return gLogModule;
}

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);

nsresult
nsFrame::NewFrame(nsIFrame**  aInstancePtrResult,
                  nsIContent* aContent,
                  nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsFrame(aContent, aParent);
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

nsFrame::nsFrame(nsIContent* aContent, nsIFrame*   aParent)
  : mContent(aContent), mContentParent(aParent), mGeometricParent(aParent)
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

NS_METHOD nsFrame::GetContentIndex(PRInt32& aIndexInParent) const
{
  nsIContent* parent = mContent->GetParent();
  if (nsnull != parent) {
    aIndexInParent = parent->IndexOf(mContent);
    NS_RELEASE(parent);
  }
  else {
    aIndexInParent = 0;
  }
  return NS_OK;
}

NS_METHOD nsFrame::GetStyleContext(nsIPresContext*   aPresContext,
                                   nsIStyleContext*& aStyleContext)
{
  if ((nsnull == mStyleContext) && (nsnull != aPresContext)) {
    mStyleContext = aPresContext->ResolveStyleContextFor(mContent, mGeometricParent); // XXX should be content parent???
    if (nsnull != mStyleContext) {
      DidSetStyleContext(aPresContext);
    }
  }
  NS_IF_ADDREF(mStyleContext);
  aStyleContext = mStyleContext;
  return NS_OK;
}

NS_METHOD nsFrame::SetStyleContext(nsIPresContext* aPresContext,nsIStyleContext* aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null ptr");
  if (aContext != mStyleContext) {
    NS_IF_RELEASE(mStyleContext);
    if (nsnull != aContext) {
      mStyleContext = aContext;
      NS_ADDREF(aContext);
      DidSetStyleContext(aPresContext);
    }
  }

  return NS_OK;
}

// Subclass hook for style post processing
NS_METHOD nsFrame::DidSetStyleContext(nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_METHOD nsFrame::GetStyleData(nsStyleStructID aSID, nsStyleStruct*& aStyleStruct) const
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
      // Position view relative to it's parent, not relative to our
      // parent frame (our parent frame may not have a view).
      nsIView* parentWithView;
      nsPoint origin;
      GetOffsetFromView(origin, parentWithView);
      mView->SetPosition(origin.x, origin.y);
      NS_IF_RELEASE(parentWithView);
    }
  }

  return NS_OK;
}

NS_METHOD nsFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  mRect.width = aWidth;
  mRect.height = aHeight;

  // Let the view know
  if (nsnull != mView) {
    mView->SetDimensions(aWidth, aHeight);
  }
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
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsFrame::IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const
{
  NS_ERROR("not a container");
  aIndex = -1;
  return NS_ERROR_NOT_IMPLEMENTED;
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
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsFrame::PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const
{
  NS_ERROR("not a container");
  aPrevChild = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
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
#if DO_SELECTION

  PRBool clearAfterPaint = PR_FALSE;

  // Get Content
  nsIContent * content;
  GetContent(content);
  if (content->ChildCount() > 0) {
    NS_RELEASE(content);
    return NS_OK;
  }

  if (mSelectionRange == nsnull) { // Get Selection Object
    nsIPresShell     * shell     = aPresContext.GetShell();
    nsIDocument      * doc       = shell->GetDocument();
    nsISelection     * selection = doc->GetSelection();

    mSelectionRange = selection->GetRange(); 

    clearAfterPaint = PR_TRUE;
    NS_RELEASE(shell);
    NS_RELEASE(selection);
  }

  if (IsInRange(mSelectionRange->GetStartContent(), mSelectionRange->GetEndContent(), content)) {
    nsRect rect;
    GetRect(rect);
    rect.width--;
    rect.height--;
    aRenderingContext.SetColor(NS_RGB(0,0,255));
    aRenderingContext.DrawRect(rect);
    aRenderingContext.DrawLine(rect.x, rect.y, rect.x+rect.width, rect.y+rect.height);
    aRenderingContext.DrawLine(rect.x, rect.y+rect.height, rect.x+rect.width, rect.y);
  }

  if (clearAfterPaint) {
    mSelectionRange = nsnull;
  }
  NS_RELEASE(content);

#endif
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
  if (gSelectionDebug) printf("Message: %d-------------------------------------------------------------\n",aEvent->message);


  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
    if (mDoingSelection) {
      //mEndSelect = 0;
      HandleRelease(aPresContext, aEvent, aEventStatus, this);
    }
  } else if (aEvent->message == NS_MOUSE_MOVE) {
    mDidDrag = PR_TRUE;

    //if (gSelectionDebug) printf("HandleEvent(Drag)::mSelectionRange %s\n", mSelectionRange->ToString());
    HandleDrag(aPresContext, aEvent, aEventStatus, this);
    if (gSelectionDebug) printf("HandleEvent(Drag)::mSelectionRange %s\n", mSelectionRange->ToString());

  } else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    nsIContent * content;
    GetContent(content);
    BuildContentList(content);

    NS_RELEASE(content);

    HandlePress(aPresContext, aEvent, aEventStatus, this);
  }
#endif

  aEventStatus = nsEventStatus_eIgnore;
  return NS_OK;
}

/**
  * Handles the Mouse Press Event for the frame
 */
NS_METHOD nsFrame::HandlePress(nsIPresContext& aPresContext, 
                               nsGUIEvent*     aEvent,
                               nsEventStatus&  aEventStatus,
                               nsFrame *       aFrame)
{
#if DO_SELECTION 
  nsFrame          * currentFrame   = aFrame;
  nsIPresShell     * shell          = aPresContext.GetShell();
  nsIDocument      * doc            = shell->GetDocument();
  nsISelection     * selection      = doc->GetSelection();
  nsMouseEvent     * mouseEvent     = (nsMouseEvent *)aEvent;

  mSelectionRange = selection->GetRange();
  
  NS_RELEASE(shell);
  NS_RELEASE(doc);
  NS_RELEASE(selection);


  PRInt32 offset = 0;
  PRInt32 width  = 0;

  mDoingSelection = PR_TRUE;
  mDidDrag        = PR_FALSE;
  mCurrentFrame   = currentFrame;

  mStartPos = GetPosition(aPresContext, aEvent, currentFrame);

  // Click count is 1
  nsIContent * newContent;
  currentFrame->GetContent(newContent);

  mCurrentFrame = currentFrame;

  if (mStartSelectionPoint == nsnull) {
    mStartSelectionPoint = new nsSelectionPoint(nsnull, -1, PR_TRUE);
  }
  if (mEndSelectionPoint == nsnull) {
    mEndSelectionPoint = new nsSelectionPoint(nsnull, -1, PR_TRUE);
  }

  mSelectionRange->GetStartPoint(mStartSelectionPoint);
  mSelectionRange->GetEndPoint(mEndSelectionPoint);

  resetContentTrackers();

  // keep old start and end
  nsIContent * startContent = mSelectionRange->GetStartContent();
  nsIContent * endContent   = mSelectionRange->GetEndContent();

  if (gSelectionDebug) printf("****** Shift[%s]\n", (mouseEvent->isShift?"Down":"Up"));

  if (IsInRange(mSelectionRange->GetStartContent(), mSelectionRange->GetEndContent(), newContent)) {
    //resetContentTrackers();
    if (gTrackerDebug) printf("Adding split range to removed selection. Shift[%s]\n", (mouseEvent->isShift?"Down":"Up"));

    if (mouseEvent->isShift) {
      nsIContent * prevContent = GetPrevContent(newContent);
      nsIContent * nextContent = GetNextContent(newContent);

      addRangeToSelectionTrackers(mSelectionRange->GetStartContent(), prevContent, kInsertInRemoveList);
      addRangeToSelectionTrackers(nextContent, mSelectionRange->GetEndContent(), kInsertInRemoveList);

      mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_FALSE);
    } else {
      addRangeToSelectionTrackers(mSelectionRange->GetStartContent(), mSelectionRange->GetEndContent(), kInsertInRemoveList); // removed from selection

      mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);
      mStartSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);

      addRangeToSelectionTrackers(newContent, newContent, kInsertInAddList); // add to selection
    }
    
  } else if (IsBefore(newContent, startContent)) {
    if (mouseEvent->isShift) {
      if (mStartSelectionPoint->IsAnchor()) {
        if (gSelectionDebug) printf("New Content is before,  Start will now be end\n");

        addRangeToSelectionTrackers(mSelectionRange->GetStartContent(), mSelectionRange->GetEndContent(), kInsertInRemoveList); // removed from selection

        nsIContent * prevContent = GetPrevContent(newContent);
        nsIContent * nextContent = GetNextContent(newContent);

        mEndSelectionPoint->SetPoint(mStartSelectionPoint->GetContent(), mStartSelectionPoint->GetOffset(), PR_TRUE);
        mStartSelectionPoint->SetPoint(newContent, mStartPos, PR_FALSE);

        addRangeToSelectionTrackers(newContent, mEndSelectionPoint->GetContent(), kInsertInAddList); // add to selection
      } else {
        if (gSelectionDebug) printf("New Content is before,  Appending to Beginning\n");
        addRangeToSelectionTrackers(newContent, mEndSelectionPoint->GetContent(), kInsertInAddList); // add to selection
        mStartSelectionPoint->SetPoint(newContent, mStartPos, PR_FALSE);
      }
    } else {
      if (gSelectionDebug) printf("Adding full range to removed selection. (insert selection)\n");
      addRangeToSelectionTrackers(mSelectionRange->GetStartContent(), mSelectionRange->GetEndContent(), kInsertInRemoveList); // removed from selection

      mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);
      mStartSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);

      addRangeToSelectionTrackers(newContent, newContent, kInsertInAddList); // add to selection
    }
  } else { // Content is After End

    if (mouseEvent->isShift) {
      if (mStartSelectionPoint->IsAnchor()) {
        if (gSelectionDebug) printf("New Content is after,  Append new content\n");
        addRangeToSelectionTrackers(mEndSelectionPoint->GetContent(), newContent,  kInsertInAddList); // add to selection
        mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_FALSE);
      } else {
        if (gSelectionDebug) printf("New Content is after,  End will now be Start\n");
        
        addRangeToSelectionTrackers(mSelectionRange->GetStartContent(), mSelectionRange->GetEndContent(), kInsertInRemoveList); // removed from selection

        nsIContent * prevContent = GetPrevContent(newContent);
        nsIContent * nextContent = GetNextContent(newContent);

        mStartSelectionPoint->SetPoint(mEndSelectionPoint->GetContent(), mEndSelectionPoint->GetOffset(), PR_TRUE);
        mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_FALSE);

        addRangeToSelectionTrackers(mEndSelectionPoint->GetContent(), newContent,  kInsertInAddList); // add to selection
      }

    } else { 
      if (gTrackerDebug) printf("Adding full range to removed selection.\n");
      addRangeToSelectionTrackers(mSelectionRange->GetStartContent(), mSelectionRange->GetEndContent(), kInsertInRemoveList); // removed from selection

      mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);
      mStartSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);

      addRangeToSelectionTrackers(newContent, newContent, kInsertInAddList); // add to selection
    }
  }

  /*if (mStartSelectionPoint == nsnull) {
    mStartSelectionPoint = new nsSelectionPoint(newContent, mStartPos, PR_TRUE);
  } else {
    mStartSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);
  }
  if (mEndSelectionPoint == nsnull) {
    mEndSelectionPoint = new nsSelectionPoint(newContent, mStartPos, PR_TRUE);
  } else {
    mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);
  }*/

  mSelectionRange->SetStartPoint(mStartSelectionPoint);
  mSelectionRange->SetEndPoint(mEndSelectionPoint);


  // [TODO] Recycle sub-selections here?
  ////fSubSelection = new SubSelection(mSelectionRange);
  ////selection.setSelection(fSubSelection);
  //aPart.setSelection(mSelectionRange);
  //updateSelection();
  //resetContentTrackers();

  //addRangeToSelectionTrackers(newContent, newContent, kInsertInAddList);

  //RefreshContentFrames(aPresContext, startContent, endContent);
  RefreshFromContentTrackers(aPresContext);

  //// [TODO] Set position of caret at start point,
  //// [TODO] Turn on caret and have it rendered
  ////printf("Selection Start Point set.");


  //buildContentTree(currentFrame);

  //fCurrentParentContentList = eventStrategy.getParentList();
  if (gSelectionDebug) printf("HandleEvent::mSelectionRange %s\n", mSelectionRange->ToString());

  // Force Update
  ForceDrawFrame(this);

  NS_RELEASE(newContent);

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
  // keep old start and end
  nsIContent * startContent = mSelectionRange->GetStartContent();
  nsIContent * endContent   = mSelectionRange->GetEndContent();

  PRBool isNewFrame = PR_FALSE; // for testing (rcs)

  mDidDrag = PR_TRUE;

  if (aFrame != nsnull) {
    //printf("nsFrame::HandleDrag\n");

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

        AdjustPointsInNewContent(aPresContext, aEvent, aFrame);

      } else if (IsBefore(newContent, currentContent)) {
        if (gSelectionDebug) printf("HandleDrag::New Frame, is Before.\n");

        resetContentTrackers();
        NewContentIsBefore(aPresContext, aEvent, newContent, currentContent, aFrame);

      } else { // Content is AFTER
        if (gSelectionDebug) printf("HandleDrag::New Frame, is After.\n");

        resetContentTrackers();
        NewContentIsAfter(aPresContext, aEvent, newContent, currentContent, aFrame);
      }
      mCurrentFrame = aFrame;

      NS_RELEASE(currentContent);
      NS_RELEASE(newContent);
    } else {
      //if (gSelectionDebug) printf("HandleDrag::Same Frame.\n");

      // Same Frame as before
      //if (gSelectionDebug) printf("\nSame Start: %s\n", mStartSelectionPoint->ToString());
      //if (gSelectionDebug) printf("Same End:   %s\n",   mEndSelectionPoint->ToString());
        // [TODO] Uncomment these soon

      if (mStartSelectionPoint->GetContent() == mEndSelectionPoint->GetContent()) {
        //if (gSelectionDebug) printf("Start & End Frame are the same: \n");
        if (gSelectionDebug) printf("******** Code is need here!\n********\n");
        //AdjustPointsInSameContent(aPresContext, aEvent);
      } else {
        //if (gSelectionDebug) printf("Start & End Frame are different: \n");

        // Get Content for New Frame
        nsIContent * newContent;
        aFrame->GetContent(newContent);
        PRInt32 newPos = -1;

        newPos = GetPosition(aPresContext, aEvent, aFrame);

        if (newContent == mStartSelectionPoint->GetContent()) {
          //if (gSelectionDebug) printf("New Content equals Start Content\n");
          mStartSelectionPoint->SetOffset(newPos);
          mSelectionRange->SetStartPoint(mStartSelectionPoint);
        } else if (newContent == mEndSelectionPoint->GetContent()) {
          //if (gSelectionDebug) printf("New Content equals End Content\n");
          mEndSelectionPoint->SetOffset(newPos);
          mSelectionRange->SetEndPoint(mEndSelectionPoint);
        } else {
          if (gSelectionDebug) printf("=====\n=====\n=====\n=====\n=====\n=====\n Should NOT be here.\n");
        }

        //if (gSelectionDebug) printf("*Same Start: "+mStartSelectionPoint->GetOffset()+
        //                               " "+mStartSelectionPoint->IsAnchor()+
         //                               "  End: "+mEndSelectionPoint->GetOffset()  +
        //                               " "+mEndSelectionPoint->IsAnchor());
        NS_RELEASE(newContent);
      }
      //??doDragRepaint(mCurrentFrame);
    }
  }
  // Force Update
  ForceDrawFrame(this);
  //RefreshContentFrames(aPresContext, startContent, endContent);
  RefreshFromContentTrackers(aPresContext);

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

//--------------------------------------------------------------------------
//-- GetPosition
//--------------------------------------------------------------------------
PRInt32 nsFrame::GetPosition(nsIPresContext& aPresContext,
                             nsGUIEvent * aEvent,
                             nsIFrame * aNewFrame) {

  //PRInt32 offset; 
  //PRInt32 width;
  //CalcCursorPosition(aPresContext, aEvent, aNewFrame, offset, width);
  //offset += aNewFrame->GetContentOffset();

  //return offset;
  return -1;
}

/********************************************************
* Adjusts the Starting and Ending TextPoint for a Range
*********************************************************/
void nsFrame::AdjustPointsInNewContent(nsIPresContext& aPresContext,
                                       nsGUIEvent * aEvent,
                                       nsIFrame  * aNewFrame) {
  // Get new Cursor Poition in the new content
  PRInt32 newPos = GetPosition(aPresContext, aEvent, aNewFrame);//, fText);

  if (mStartSelectionPoint->IsAnchor()) {
    if (newPos == mStartSelectionPoint->GetOffset()) {
      mEndSelectionPoint->SetOffset(newPos);
      mEndSelectionPoint->SetAnchor(PR_TRUE);
      mSelectionRange->SetEndPoint(mEndSelectionPoint);
    } else if (newPos < mStartSelectionPoint->GetOffset()) {
      mEndSelectionPoint->SetOffset(mStartSelectionPoint->GetOffset());
      mEndSelectionPoint->SetAnchor(PR_TRUE);
      mStartSelectionPoint->SetOffset(newPos);
      mStartSelectionPoint->SetAnchor(PR_FALSE);
      mSelectionRange->SetRange(mStartSelectionPoint, mEndSelectionPoint);
    } else {
      mEndSelectionPoint->SetOffset(newPos);
      mSelectionRange->SetEndPoint(mEndSelectionPoint);
    }
  } else if (mEndSelectionPoint->IsAnchor()) {
    int endPos = mEndSelectionPoint->GetOffset();
    if (newPos == mEndSelectionPoint->GetOffset()) {
      mStartSelectionPoint->SetOffset(newPos);
      mStartSelectionPoint->SetAnchor(PR_TRUE);
      mSelectionRange->SetStartPoint(mStartSelectionPoint);
    } else if (newPos > mEndSelectionPoint->GetOffset()) {
      mEndSelectionPoint->SetOffset(newPos);
      mEndSelectionPoint->SetAnchor(PR_FALSE);
      mStartSelectionPoint->SetOffset(endPos);
      mStartSelectionPoint->SetAnchor(PR_TRUE);
      mSelectionRange->SetRange(mStartSelectionPoint, mEndSelectionPoint);
    } else {
      mStartSelectionPoint->SetOffset(newPos);
      mSelectionRange->SetStartPoint(mStartSelectionPoint);
    }
  } else {
    // [TODO] Should get here
    // throw exception
    if (gSelectionDebug) printf("--\n--\n--\n--\n--\n--\n--\n Should be here. #102\n");
    //return;
  }
}

/********************************************************
* Adjusts the Starting and Ending TextPoint for a Range
*********************************************************/
void nsFrame::AdjustPointsInSameContent(nsIPresContext& aPresContext,
                                        nsGUIEvent    * aEvent) {
  // Get new Cursor Poition in the same content
  PRInt32 newPos = GetPosition(aPresContext, aEvent, mCurrentFrame);
  if (gSelectionDebug) printf("AdjustTextPointsInSameContent newPos: %d\n", newPos);

  if (mStartSelectionPoint->IsAnchor()) {
    if (newPos == mStartSelectionPoint->GetOffset()) {
      mEndSelectionPoint->SetOffset(newPos);
      mEndSelectionPoint->SetAnchor(PR_TRUE);
      mSelectionRange->SetEndPoint(mEndSelectionPoint);
    } else if (newPos < mStartSelectionPoint->GetOffset()) {
      mStartSelectionPoint->SetOffset(newPos);
      mStartSelectionPoint->SetAnchor(PR_FALSE);
      mEndSelectionPoint->SetAnchor(PR_TRUE);
      mSelectionRange->SetRange(mStartSelectionPoint, mEndSelectionPoint);
    } else {
      mEndSelectionPoint->SetAnchor(PR_FALSE);
      mEndSelectionPoint->SetOffset(newPos);
      mSelectionRange->SetEndPoint(mEndSelectionPoint);
    }
  } else if (mEndSelectionPoint->IsAnchor()) {
    if (newPos == mEndSelectionPoint->GetOffset()) {
      mStartSelectionPoint->SetOffset(newPos);
      mStartSelectionPoint->SetAnchor(PR_TRUE);
      mSelectionRange->SetStartPoint(mStartSelectionPoint);
    } else if (newPos > mEndSelectionPoint->GetOffset()) {
      mEndSelectionPoint->SetOffset(newPos);
      mEndSelectionPoint->SetAnchor(PR_FALSE);
      mStartSelectionPoint->SetAnchor(PR_TRUE);
      mSelectionRange->SetRange(mStartSelectionPoint, mEndSelectionPoint);
    } else {
      mStartSelectionPoint->SetAnchor(PR_FALSE);
      mStartSelectionPoint->SetOffset(newPos);
      mSelectionRange->SetStartPoint(mStartSelectionPoint);
    }
  } else {
    // [TODO] Should get here
    // throw exception
    if (gSelectionDebug) printf("--\n--\n--\n--\n--\n--\n--\n Should be here. #101\n");
    //return;
  }

    if (gSelectionDebug) printf("Start %s  End %s\n", mStartSelectionPoint->ToString(), mEndSelectionPoint->ToString());
  //}
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
NS_METHOD
nsFrame::GetFrameState(nsFrameState& aResult)
{
  aResult = mState;
  return NS_OK;
}

NS_METHOD
nsFrame::SetFrameState(nsFrameState aNewState)
{
  mState = aNewState;
  return NS_OK;
}

// Resize reflow methods

NS_METHOD
nsFrame::WillReflow(nsIPresContext& aPresContext)
{
  NS_FRAME_TRACE_MSG(("WillReflow: oldState=%x", mState));
  mState |= NS_FRAME_IN_REFLOW;
  return NS_OK;
}

NS_METHOD
nsFrame::DidReflow(nsIPresContext& aPresContext,
                   nsDidReflowStatus aStatus)
{
  NS_FRAME_TRACE_MSG(("nsFrame::DidReflow: aStatus=%d", aStatus));
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    mState &= ~NS_FRAME_IN_REFLOW;
  }
  return NS_OK;
}

NS_METHOD nsFrame::Reflow(nsIPresContext*      aPresContext,
                          nsReflowMetrics&     aDesiredSize,
                          const nsReflowState& aReflowState,
                          nsReflowStatus&      aStatus)
{
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.descent = 0;
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }
  aStatus = NS_FRAME_COMPLETE;

  if (eReflowReason_Incremental == aReflowState.reason) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
}

NS_METHOD nsFrame::JustifyReflow(nsIPresContext* aPresContext,
                                 nscoord         aAvailableSpace)
{
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

NS_METHOD nsFrame::ContentChanged(nsIPresShell*   aShell,
                                  nsIPresContext* aPresContext,
                                  nsIContent*     aChild,
                                  nsISupports*    aSubContent)
{
  // Generate a reflow command with this frame as the target frame
  nsReflowCommand* cmd = new nsReflowCommand(aPresContext, this,
                                             nsReflowCommand::ContentChanged);
  aShell->AppendReflowCommand(cmd);
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

NS_METHOD nsFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_NOT_SPLITTABLE;
  return NS_OK;
}

NS_METHOD nsFrame::CreateContinuingFrame(nsIPresContext*  aPresContext,
                                         nsIFrame*        aParent,
                                         nsIStyleContext* aStyleContext,
                                         nsIFrame*&       aContinuingFrame)
{
  NS_ERROR("not splittable");
  aContinuingFrame = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsFrame::GetPrevInFlow(nsIFrame*& aPrevInFlow) const
{
  aPrevInFlow = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::SetPrevInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsFrame::GetNextInFlow(nsIFrame*& aNextInFlow) const
{
  aNextInFlow = nsnull;
  return NS_OK;
}

NS_METHOD nsFrame::SetNextInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsFrame::AppendToFlow(nsIFrame* aAfterFrame)
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsFrame::PrependToFlow(nsIFrame* aBeforeFrame)
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsFrame::RemoveFromFlow()
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsFrame::BreakFromPrevFlow()
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsFrame::BreakFromNextFlow()
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
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


// Style sizing methods
NS_METHOD nsFrame::IsPercentageBase(PRBool& aBase) const
{
  nsStylePosition* position;
  GetStyleData(eStyleStruct_Position, (nsStyleStruct*&)position);
  if (position->mPosition != NS_STYLE_POSITION_NORMAL) {
    aBase = PR_TRUE;
  }
  else {
    nsStyleDisplay* display;
    GetStyleData(eStyleStruct_Display, (nsStyleStruct*&)display);
    if ((display->mDisplay == NS_STYLE_DISPLAY_BLOCK) || 
        (display->mDisplay == NS_STYLE_DISPLAY_LIST_ITEM)) {
      aBase = PR_TRUE;
    }
    else {
      aBase = PR_FALSE;
    }
  }
  return NS_OK;
}

NS_METHOD nsFrame::GetAutoMarginSize(PRUint8 aSide, nscoord& aSize) const
{
  aSize = 0;  // XXX probably not right, subclass override?
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
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
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
  PRInt32 contentIndex;

  GetContentIndex(contentIndex);
  fprintf(out, "(%d)@%p", contentIndex, this);
  return NS_OK;
}

NS_METHOD nsFrame::VerifyTree() const
{
  return NS_OK;
}

//-----------------------------------------------------------------------------------


//------------------------------
void WalkTree(nsIContent * aParent) {

  /*if (aParent->ChildCount() == 0) {
    nsIDOMText * textContent;
    nsresult status = aParent->QueryInterface(kIDOMTextIID, (void**) &textContent);
    if (NS_OK == status) {
      nsString text;
      textContent->GetData(text);
      if (text.Length() == 1) {
        char * str = text.ToNewCString();
        if (str[0] >= 32 && str[0] != '\n') {
          fContentArray[fMax++] = aParent;
        }
        delete str;
      } else {
        fContentArray[fMax++] = aParent;
      }

    }
    return;
  }*/
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
      if (text.Length() > 1) {
        printf("%i [%s]\n", i, str);
      } else {
        printf("%i [%s] %d\n", i, str, str[0]);
      }
      delete str;
    }
  }
  printf("Total pieces of Content: %d\n", fMax);
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
  if (gSelectionDebug) PrintContent();
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
    return aContent == aStartContent;
  }

  // Check to see if it is equal to the start or the end
  if (aContent == aStartContent || aContent == aEndContent) {
    return PR_TRUE;
  }

  if (fMax == -1) {
    BuildContentList(aContent);
  }

  PRBool foundStart = PR_FALSE;
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
  if (IsInRange(mStartSelectionPoint->GetContent(), mEndSelectionPoint->GetContent(), aNewContent)) {

    // Case #1 - Remove Current Content from Selection (at End)
    if (gSelectionDebug) printf("Case #1 - (Before) New Content is in selected Range.\n");


    if (aNewContent == mStartSelectionPoint->GetContent()) {
      // [TODO] This is where we might have to delete from end to new content

      // Returns the new End Point, if Start and End are on the
      // same content then End Point's Cursor is set to Start's
      mEndSelectionPoint->SetContent(mStartSelectionPoint->GetContent());
      AdjustPointsInNewContent(aPresContext, aEvent, aNewFrame);

    } else {
      PRInt32 newPos = GetPosition(aPresContext, aEvent, aNewFrame);
      mEndSelectionPoint->SetPoint(aNewContent, newPos, PR_FALSE);
      mSelectionRange->SetEndPoint(mEndSelectionPoint);
    }

    // The current content is being removed from Selection
    addRangeToSelectionTrackers(aNewContent, aCurrentContent, kInsertInRemoveList);
  } else {
    // Case #2 - Add new content to selection (At Start)
    if (gSelectionDebug) printf("Case #2 - (Before) New Content is NOT in selected Range. Moving Start Backward.\n");

    PRInt32 newPos = GetPosition(aPresContext, aEvent, aNewFrame);

    // Create new TextPoint and move Start Point backward
    mStartSelectionPoint->SetPoint(aNewContent, newPos, PR_FALSE); // position is set correctly in adjustTextPoints
    mSelectionRange->SetStartPoint(mStartSelectionPoint);

    // The New Content is added to Tracker
    addRangeToSelectionTrackers(aNewContent, aCurrentContent, kInsertInAddList);
  }
  //??doDragRepaint(fCurrentFrame);
  //??doDragRepaint(aNewFrame);
  //??if (gSelectionDebug) mSelectionRange.printContent();
}

/********************************************************
* Refreshes each content's frame
*********************************************************/
void RefreshContentFrames(nsIPresContext& aPresContext,
                          nsIContent * aStartContent,
                          nsIContent * aEndContent)
{
  //-------------------------------------
  // Undraw all the current selected frames
  // XXX Kludge for now
  nsIPresShell * shell = aPresContext.GetShell();
  PRBool foundStart = PR_FALSE;
  for (PRInt32 i=0;i<fMax;i++) {
    nsIContent * node = (nsIContent *)fContentArray[i];
    if (node == aStartContent) {
      foundStart = PR_TRUE;
      ForceDrawFrame((nsFrame *)shell->FindFrameWithContent(node));
      if (aStartContent == aEndContent) {
        break;
      }
    } else if (foundStart) {
      ForceDrawFrame((nsFrame *)shell->FindFrameWithContent(node));
    } else if (aEndContent == node) {
      ForceDrawFrame((nsFrame *)shell->FindFrameWithContent(node));
      break;
    }
  }
  //-------------------------------------
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
  if (IsInRange(mStartSelectionPoint->GetContent(), mEndSelectionPoint->GetContent(), aNewContent)) {

    // Case #3 - Remove Content (from Start)
    if (gSelectionDebug) printf("Case #3 - (After) New Content is in selected Range.\n");

    PrintIndex("Start:   ", mStartSelectionPoint->GetContent());
    PrintIndex("Current: ", aNewContent);
    // Remove Current Content in Tracker, but leave New Content in Selection
    addRangeToSelectionTrackers(mStartSelectionPoint->GetContent(), aNewContent, kInsertInRemoveList);

    // [TODO] Always get nearest Text content
    PRInt32 newPos = GetPosition(aPresContext, aEvent, aNewFrame);

    // Check to see if the new Content is the same as the End Point's
    if (aNewContent == mEndSelectionPoint->GetContent()) {
      if (gSelectionDebug) printf("New Content matches End Point\n");

      mStartSelectionPoint->SetContent(aNewContent);
      AdjustPointsInNewContent(aPresContext, aEvent, aNewFrame);

    } else {
      if (gSelectionDebug) printf("New Content does NOT matches End Point\n");
      mStartSelectionPoint->SetPoint(aNewContent, newPos, PR_FALSE);
      mSelectionRange->SetStartPoint(mStartSelectionPoint);
    }

  } else {
    if (gSelectionDebug)
      printf("Case #4 - (After) Adding New Content\n");

    // Case #2 - Adding Content (at End)
    // The new content is not in the selection
    PRInt32 newPos = GetPosition(aPresContext, aEvent, aNewFrame);

    // Check to see if we need to create a new SelectionPoint and add it
    // or do we simply move the existing start or end point
    if (mStartSelectionPoint->GetContent() == mEndSelectionPoint->GetContent()) {
      if (gSelectionDebug) printf("Case #4 - Start & End Content the Same\n");
      // Move start or end point
      // Get new Cursor Poition in the new content

      if (mStartSelectionPoint->IsAnchor()) {
        if (gSelectionDebug) printf("Case #4 - Start is Anchor\n");
        // Since the Start is the Anchor just adjust the end
        // What is up Here????????
        //if (gSelectionDebug) printf("Start: %s\n", mStartSelectionPoint->ToString());
        //if (gSelectionDebug) printf("End:   %s\n", mEndSelectionPoint->ToString());
        //if (gSelectionDebug) printf("---->");

        // XXX Note this includes the current End point (it should be end->nextContent)
        addRangeToSelectionTrackers(mEndSelectionPoint->GetContent(), aNewContent, kInsertInAddList);

        mEndSelectionPoint->SetPoint(aNewContent, newPos, PR_FALSE);
        mSelectionRange->SetEndPoint(mEndSelectionPoint);


        //if (gSelectionDebug) printf("Start: %s\n", mStartSelectionPoint->ToString());
        //if (gSelectionDebug) printf("End:   %s\n", mEndSelectionPoint->ToString());

      } else {
        if (gSelectionDebug) printf("Case #4 - Start is NOT Anchor\n");
        // Because End was the anchor, we need to set the Start Point to
        // the End's Offset and set it to be the new anchor
        addRangeToSelectionTrackers(mStartSelectionPoint->GetContent(), 
                                    mEndSelectionPoint->GetContent(), kInsertInRemoveList);

        int endPos = mEndSelectionPoint->GetOffset();
        mStartSelectionPoint->SetOffset(endPos);
        mStartSelectionPoint->SetAnchor(PR_TRUE);

        // The Start point was being moved so when it jumped to the new frame
        // we needed to make it the new End Point
        mEndSelectionPoint->SetPoint(aNewContent, newPos, PR_FALSE);
        mSelectionRange->SetRange(mStartSelectionPoint, mEndSelectionPoint);

        addRangeToSelectionTrackers(mStartSelectionPoint->GetContent(), 
                                    mEndSelectionPoint->GetContent(), kInsertInRemoveList);
      }
    } else {
      if (gSelectionDebug) printf("Case #4 - Start & End Content NOT the Same\n");
      nsIContent * oldEnd = mEndSelectionPoint->GetContent();
      // Adjust the end point
      mEndSelectionPoint->SetPoint(aNewContent, newPos, PR_FALSE);
      mSelectionRange->SetRange(mStartSelectionPoint, mEndSelectionPoint);

      // Add New Content to Selection Tracker
      addRangeToSelectionTrackers(oldEnd, mEndSelectionPoint->GetContent(), kInsertInAddList);
    }
  }
  //??if (gSelectionDebug) mSelectionRange.printContent();

}

/**
  *
 */
void ForceDrawFrame(nsFrame * aFrame)//, PRBool)
{
  if (aFrame == nsnull) {
    return;
  }
  nsRect    rect;
  nsIView * view;
  nsPoint   pnt;
  aFrame->GetOffsetFromView(pnt, view);
  aFrame->GetRect(rect);
  rect.x = pnt.x;
  rect.y = pnt.y;
  if (view != nsnull) {
    nsIViewManager * viewMgr = view->GetViewManager();
    if (viewMgr != nsnull) {
      viewMgr->UpdateView(view, rect, 0);
    }
  }
  //viewMgr->UpdateView(view, rect, NS_VMREFRESH_DOUBLE_BUFFER | NS_VMREFRESH_IMMEDIATE);

  //NS_RELEASE(view);
}


/////////////////////////////////////////////////
// Selection Tracker Methods
/////////////////////////////////////////////////

//----------------------------
//
//----------------------------
void resetContentTrackers() {
  //nsIContent * fTrackerContentArrayRemoveList[1024];
  //nsIContent * fTrackerContentArrayAddList[1024];
  fTrackerRemoveListMax = 0;
  fTrackerAddListMax = 0;
}

//----------------------------
//
//----------------------------
void RefreshFromContentTrackers(nsIPresContext& aPresContext) {
  /*FILE * fd = fopen("data.txt", "w");
  PRBool foundStart = PR_FALSE;
  for (PRInt32 j=0;j<fMax;j++) {
    nsIContent * node = (nsIContent *)fContentArray[j];
    if (node == mSelectionRange->GetStartContent()) {
      foundStart = PR_TRUE;
      printf("%d -> 0x%X\n", j, node);
    } else if (mSelectionRange->GetEndContent() == node) {
      printf("%d -> 0x%X\n", j, node);
      break;
    } else if (foundStart) {
      printf("%d -> 0x%X\n", j, node);
    }
  }
  fflush(fd);
  fclose(fd);*/


  PRInt32 i;
  nsIPresShell * shell = aPresContext.GetShell();
  for (i=0;i<fTrackerRemoveListMax;i++) {
    ForceDrawFrame((nsFrame *)shell->FindFrameWithContent(fTrackerContentArrayRemoveList[i]));
    if (gSelectionDebug) printf("ForceDrawFrame (remove) content 0x%X\n", fTrackerContentArrayRemoveList[i]);
  }
  for (i=0;i<fTrackerAddListMax;i++) {
    ForceDrawFrame((nsFrame *)shell->FindFrameWithContent(fTrackerContentArrayAddList[i]));
    if (gSelectionDebug) printf("ForceDrawFrame (add) content 0x%X\n", fTrackerContentArrayAddList[i]);
  }
  resetContentTrackers();
}

//----------------------------
//
//----------------------------
void addRangeToSelectionTrackers(nsIContent * aStartContent, nsIContent * aEndContent, PRUint32 aType) 
{
  if (aStartContent == nsnull || aEndContent == nsnull) {
    return;
  }
  nsIContent ** content = (aType == kInsertInRemoveList?fTrackerContentArrayRemoveList:fTrackerContentArrayAddList);
  int           inx     = (aType == kInsertInRemoveList?fTrackerRemoveListMax:fTrackerAddListMax);

  //FILE * fd  = fopen("data.txt", "w");
  //fprintf(fd, "Inx before %d\n", inx);

  PRBool foundStart = PR_FALSE;
  for (PRInt32 i=0;i<fMax;i++) {
    nsIContent * node = (nsIContent *)fContentArray[i];
    //fprintf(fd, "Checking 0x%X 0x%X 0x%X\n",  node, aStartContent, aEndContent);
    if (node == aStartContent) {
      //fprintf(fd, "Found start %d\n", i);
      content[inx++] = node;
      foundStart = PR_TRUE;
      if (aStartContent == aEndContent) {
        break;
      }
    } else if (aEndContent == node) {
      //fprintf(fd, "Found end %d\n", i);
      content[inx++] = node;
      break;
    } else if (foundStart) {
      //fprintf(fd, "adding %d\n", i);
      content[inx++] = node;
    }
    if (inx > 10) {
      int x = 0;
    }
  }
  //fprintf(fd, "Inx after  %d\n", inx);
  //printf("Inx after  %d\n", inx);
  //fflush(fd);
  //fclose(fd);
  if (inx > 100) {
    int x = 0;
  }
  if (gSelectionDebug) printf("Adding to %s  %d\n", (aType == kInsertInRemoveList?"Remove Array":"Add Array"), inx);
  if (aType == kInsertInRemoveList) {
    fTrackerRemoveListMax = inx;
  } else { // kInsertInAddList
    fTrackerAddListMax = inx;
  }
}

void PrintIndex(char * aStr, nsIContent * aContent) {
  for (PRInt32 j=0;j<fMax;j++) {
    nsIContent * node = (nsIContent *)fContentArray[j];
    if (node == aContent) {
      printf("%s %d\n", aStr, j);
      return;
    }
  }
}

#ifdef NS_DEBUG
static void
GetTagName(nsFrame* aFrame, nsIContent* aContent, PRIntn aResultSize,
           char* aResult)
{
  char namebuf[40];
  namebuf[0] = 0;
  if (nsnull != aContent) {
    nsIAtom* tag = aContent->GetTag();
    if (nsnull != tag) {
      nsAutoString tmp;
      tag->ToString(tmp);
      tmp.ToCString(namebuf, sizeof(namebuf));
      NS_RELEASE(tag);
    }
  }
  PR_snprintf(aResult, aResultSize, "%s@%p", namebuf, aFrame);
}

void
nsFrame::Trace(const char* aMethod, PRBool aEnter)
{
  if (NS_FRAME_LOG_TEST(gLogModule, NS_FRAME_TRACE_CALLS)) {
    char tagbuf[40];
    GetTagName(this, mContent, sizeof(tagbuf), tagbuf);
    PR_LogPrint("%s: %s %s", tagbuf, aEnter ? "enter" : "exit", aMethod);
  }
}

void
nsFrame::Trace(const char* aMethod, PRBool aEnter, nsReflowStatus aStatus)
{
  if (NS_FRAME_LOG_TEST(gLogModule, NS_FRAME_TRACE_CALLS)) {
    char tagbuf[40];
    GetTagName(this, mContent, sizeof(tagbuf), tagbuf);
    PR_LogPrint("%s: %s %s, status=%scomplete%s",
                tagbuf, aEnter ? "enter" : "exit", aMethod,
                NS_FRAME_IS_NOT_COMPLETE(aStatus) ? "not" : "",
                (NS_FRAME_REFLOW_NEXTINFLOW & aStatus) ? "+reflow" : "");
  }
}

void
nsFrame::TraceMsg(const char* aFormatString, ...)
{
  if (NS_FRAME_LOG_TEST(gLogModule, NS_FRAME_TRACE_CALLS)) {
    // Format arguments into a buffer
    char argbuf[200];
    va_list ap;
    va_start(ap, aFormatString);
    PR_vsnprintf(argbuf, sizeof(argbuf), aFormatString, ap);
    va_end(ap);

    char tagbuf[40];
    GetTagName(this, mContent, sizeof(tagbuf), tagbuf);
    PR_LogPrint("%s: %s", tagbuf, argbuf);
  }
}
#endif
