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
#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsFrameList.h"
#include "nsLineLayout.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsIStyleContext.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIPresContext.h"
#include "nsCRT.h"
#include "nsGUIEvent.h"
#include "nsDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIPresShell.h"
#include "prlog.h"
#include "prprf.h"
#include <stdarg.h>
#include "nsIPtr.h"
#include "nsISizeOfHandler.h"

#include "nsIDOMText.h"
#include "nsDocument.h"
#include "nsIDeviceContext.h"
#include "nsHTMLIIDs.h"
#include "nsIEventStateManager.h"
#include "nsIDOMSelection.h"
#include "nsIFrameSelection.h"
#include "nsIFocusTracker.h"
#include "nsHTMLParts.h"
#include "nsLayoutAtoms.h"

#include "nsFrameTraversal.h"
#include "nsCOMPtr.h"
#include "nsStyleChangeList.h"


// Some Misc #defines
#define SELECTION_DEBUG        0
#define FORCE_SELECTION_UPDATE 1
#define TRACKER_DEBUG          0
#define CALC_DEBUG             0

// Tracker Data
#define kInsertInRemoveList 0
#define kInsertInAddList    1

// Kludged Content stuff
nsIFrame   * fFrameArray[1024];
nsIContent * fContentArray[1024];
PRInt32      fMax = -1;

nsIContent * fTrackerContentArrayRemoveList[1024];
PRInt32      fTrackerRemoveListMax = 0;
nsIContent * fTrackerContentArrayAddList[1024];
PRInt32      fTrackerAddListMax = 0;

nsIDocument      *gDoc = nsnull;

// [HACK] Foward Declarations
void ForceDrawFrame(nsFrame * aFrame);
#if 0
static void RefreshContentFrames(nsIPresContext& aPresContext, nsIContent * aStartContent, nsIContent * aEndContent);
#endif

PRBool      nsFrame::mDoingSelection = PR_FALSE;
PRBool      nsFrame::mDidDrag        = PR_FALSE;


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
static PRLogModuleInfo* gLogModule;

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
  if (nsnull == gLogModule) {
    gLogModule = PR_NewLogModule("frame");
  }
  return gLogModule;
}

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);
static NS_DEFINE_IID(kIFocusTracker, NS_IFOCUSTRACKER_IID);
static NS_DEFINE_IID(kIFrameSelection, NS_IFRAMESELECTION_IID);
nsresult
NS_NewEmptyFrame(nsIFrame**  aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

NS_IMPL_ZEROING_OPERATOR_NEW(nsFrame)

nsFrame::nsFrame()
{
  mState = NS_FRAME_FIRST_REFLOW | NS_FRAME_SYNC_FRAME_AND_VIEW;
  mSelected = PR_FALSE;
}

nsFrame::~nsFrame()
{
  NS_IF_RELEASE(mContent);
  NS_IF_RELEASE(mStyleContext);
  if (nsnull != mView) {
    // Break association between view and frame
    mView->Destroy();
    mView = nsnull;
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
  if (aIID.Equals(kIHTMLReflowIID)) {
    *aInstancePtr = (void*)(nsIHTMLReflow*)this;
    return NS_OK;
  } else if (aIID.Equals(kClassIID) || aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)this;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsrefcnt nsFrame::AddRef(void)
{
  NS_WARNING("not supported for frames");
  return 1;
}

nsrefcnt nsFrame::Release(void)
{
  NS_WARNING("not supported for frames");
  return 1;
}

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

NS_IMETHODIMP
nsFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mParent = aParent;
  return SetStyleContext(&aPresContext, aContext);
}

NS_IMETHODIMP nsFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                           nsIAtom*        aListName,
                                           nsIFrame*       aChildList)
{
  // XXX This shouldn't be getting called at all, but currently is for backwards
  // compatility reasons...
#if 0
  NS_ERROR("not a container");
  return NS_ERROR_UNEXPECTED;
#else
  NS_ASSERTION(nsnull == aChildList, "not a container");
  return NS_OK;
#endif
}

NS_IMETHODIMP
nsFrame::AppendFrames(nsIPresContext& aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aFrameList)
{
#if 0
  return NS_ERROR_UNEXPECTED;
#else
#ifdef NS_DEBUG
  nsFrameList tmp(aFrameList);
  tmp.VerifyParent(this);
#endif
  // XXX temporary code until frame containers stop using the old
  // reflow command api's
  nsIReflowCommand* reflowCmd = nsnull;
  nsresult rv;
  rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                               nsIReflowCommand::FrameAppended,
                               aFrameList);
  if (NS_SUCCEEDED(rv)) {
    if (nsnull != aListName) {
      reflowCmd->SetChildListName(aListName);
    }
    aPresShell.AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }
  return rv;
#endif
}

NS_IMETHODIMP
nsFrame::InsertFrames(nsIPresContext& aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aPrevFrame,
                      nsIFrame*       aFrameList)
{
#if 0
  return NS_ERROR_UNEXPECTED;
#else
#ifdef NS_DEBUG
  nsFrameList tmp(aFrameList);
  tmp.VerifyParent(this);
#endif

  // XXX temporary code until frame containers stop using the old
  // reflow command api's

  // By default, the reflow command is aimed at the first-in-flow. We
  // map that to the flow block that contains aPrevFrame for
  // compatability.
  nsIFrame* target = this;
  if (nsnull != aPrevFrame) {
    aPrevFrame->GetParent(&target);
  }

  nsIReflowCommand* reflowCmd = nsnull;
  nsresult rv;
  rv = NS_NewHTMLReflowCommand(&reflowCmd, target, aFrameList, aPrevFrame);
  if (NS_SUCCEEDED(rv)) {
    if (nsnull != aListName) {
      reflowCmd->SetChildListName(aListName);
    }
    aPresShell.AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }
  return rv;
#endif
}

NS_IMETHODIMP
nsFrame::RemoveFrame(nsIPresContext& aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame)
{
#if 0
  return NS_ERROR_UNEXPECTED;
#else
#ifdef NS_DEBUG
  nsIFrame* parent;
  aOldFrame->GetParent(&parent);
  NS_ASSERTION(parent == this, "bad child parent");
#endif

  // XXX temporary code until frame containers stop using the old
  // reflow command api's

  // By default, the reflow command is aimed at the first-in-flow. We
  // map that to the flow block that contains aPrevFrame for
  // compatability.
  nsIFrame* target = this;
  if (nsnull != aOldFrame) {
    aOldFrame->GetParent(&target);
  }

  nsIReflowCommand* reflowCmd = nsnull;
  nsresult rv;
  rv = NS_NewHTMLReflowCommand(&reflowCmd, target,
                               nsIReflowCommand::FrameRemoved,
                               aOldFrame);
  if (NS_SUCCEEDED(rv)) {
    if (nsnull != aListName) {
      reflowCmd->SetChildListName(aListName);
    }
    aPresShell.AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }
  return rv;
#endif
}

NS_IMETHODIMP
nsFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  if (mState & NS_FRAME_EXTERNAL_REFERENCE || mSelected) {
    nsCOMPtr<nsIPresShell> shell;
    nsresult rv = aPresContext.GetShell(getter_AddRefs(shell));
    if (NS_SUCCEEDED(rv) && shell) {
      shell->ClearFrameRefs(this);
    }
  }

  //XXX Why is this done in nsFrame instead of some frame class
  // that actually loads images?
  aPresContext.StopAllLoadImagesFor(this);

  //Set to prevent event dispatch during destruct
  if (nsnull != mView) {
    mView->SetClientData(nsnull);
  }

  delete this;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetContent(nsIContent** aContent) const
{
  NS_PRECONDITION(nsnull != aContent, "null OUT parameter pointer");
  NS_IF_ADDREF(mContent);
  *aContent = mContent;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetOffsets(PRInt32 &aStart, PRInt32 &aEnd) const
{
  aStart = 0;
  aEnd = 0;
  return NS_OK;
}


NS_IMETHODIMP
nsFrame::GetStyleContext(nsIStyleContext** aStyleContext) const
{
  NS_PRECONDITION(nsnull != aStyleContext, "null OUT parameter pointer");
  NS_ASSERTION(nsnull != mStyleContext, "frame should always have style context");
  NS_IF_ADDREF(mStyleContext);
  *aStyleContext = mStyleContext;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetStyleContext(nsIPresContext* aPresContext,nsIStyleContext* aContext)
{
//  NS_PRECONDITION(0 == (mState & NS_FRAME_IN_REFLOW), "Shouldn't set style context during reflow");
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
NS_IMETHODIMP nsFrame::DidSetStyleContext(nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP nsFrame::GetStyleData(nsStyleStructID aSID, const nsStyleStruct*& aStyleStruct) const
{
  NS_ASSERTION(mStyleContext!=nsnull,"null style context");
  if (mStyleContext) {
    aStyleStruct = mStyleContext->GetStyleData(aSID);
  } else {
    aStyleStruct = nsnull;
  }
  return NS_OK;
}

void nsFrame::CaptureStyleChangeFor(nsIFrame* aFrame,
                                    nsIStyleContext* aOldContext, 
                                    nsIStyleContext* aNewContext,
                                    PRInt32 aParentChange,
                                    nsStyleChangeList* aChangeList,
                                    PRInt32* aLocalChange)
{
  if (aChangeList && aLocalChange) {  // does caller really want change data?
    PRInt32 change = NS_STYLE_HINT_NONE;
    if (aOldContext) {
      aNewContext->CalcStyleDifference(aOldContext, change);
    }
    else {
      nsIStyleContext* parent = aNewContext->GetParent();
      if (parent) {
        aNewContext->CalcStyleDifference(parent, change);
        NS_RELEASE(parent);
      }
    }
    if (aParentChange < change) { // found larger change, record it
      aChangeList->AppendChange(aFrame, change);
      *aLocalChange = change;
    }
    else {
      *aLocalChange = aParentChange;
    }
  }
}

NS_IMETHODIMP nsFrame::ReResolveStyleContext(nsIPresContext* aPresContext,
                                             nsIStyleContext* aParentContext,
                                             PRInt32 aParentChange,
                                             nsStyleChangeList* aChangeList,
                                             PRInt32* aLocalChange)
{
// XXX TURN THIS ON  NS_PRECONDITION(0 == (mState & NS_FRAME_IN_REFLOW), "Shouldn't set style context during reflow");
  NS_ASSERTION(nsnull != mStyleContext, "null style context");

  nsresult result = NS_COMFALSE;

  if (nsnull != mStyleContext) {
    nsIAtom*  pseudoTag = nsnull;
    mStyleContext->GetPseudoType(pseudoTag);
    nsIStyleContext*  newContext;
    if (nsnull != pseudoTag) {
      result = aPresContext->ResolvePseudoStyleContextFor(mContent, pseudoTag,
                                                          aParentContext,
                                                          PR_FALSE, &newContext);
    }
    else {
      result = aPresContext->ResolveStyleContextFor(mContent, aParentContext,
                                                    PR_FALSE, &newContext);
    }

    NS_ASSERTION(nsnull != newContext, "failed to get new style context");
    if (nsnull != newContext) {
      if (newContext != mStyleContext) {
        nsIStyleContext* oldContext = mStyleContext;
        mStyleContext = newContext;
        result = DidSetStyleContext(aPresContext);
        CaptureStyleChangeFor(this, oldContext, newContext, 
                              aParentChange, aChangeList, aLocalChange);
        NS_RELEASE(oldContext);
      }
      else {
        NS_RELEASE(newContext);
        result = NS_COMFALSE;
      }
    }
  }
  return result;
}

// Geometric parent member functions

NS_IMETHODIMP nsFrame::GetParent(nsIFrame** aParent) const
{
  NS_PRECONDITION(nsnull != aParent, "null OUT parameter pointer");
  *aParent = mParent;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetParent(const nsIFrame* aParent)
{
  mParent = (nsIFrame*)aParent;
  return NS_OK;
}

// Bounding rect member functions

NS_IMETHODIMP nsFrame::GetRect(nsRect& aRect) const
{
  aRect = mRect;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::GetOrigin(nsPoint& aPoint) const
{
  aPoint.x = mRect.x;
  aPoint.y = mRect.y;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::GetSize(nsSize& aSize) const
{
  aSize.width = mRect.width;
  aSize.height = mRect.height;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetRect(const nsRect& aRect)
{
  MoveTo(aRect.x, aRect.y);
  SizeTo(aRect.width, aRect.height);
  return NS_OK;
}

NS_IMETHODIMP nsFrame::MoveTo(nscoord aX, nscoord aY)
{
  mRect.x = aX;
  mRect.y = aY;

  if (nsnull != mView) {
    // If we should keep the view position and size in sync with the frame
    // then position the view. Don't do this if we're in the middle of reflow.
    // Instead wait until the DidReflow() notification
    if (NS_FRAME_SYNC_FRAME_AND_VIEW == (mState & (NS_FRAME_IN_REFLOW |
                                                   NS_FRAME_SYNC_FRAME_AND_VIEW))) {
      // Position view relative to its parent, not relative to our
      // parent frame (our parent frame may not have a view).
      nsIView* parentWithView;
      nsPoint origin;
      GetOffsetFromView(origin, &parentWithView);
      nsIViewManager  *vm;
      mView->GetViewManager(vm);
      vm->MoveViewTo(mView, origin.x, origin.y);
      NS_RELEASE(vm);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  mRect.width = aWidth;
  mRect.height = aHeight;

  // Let the view know
  if (nsnull != mView) {
    // If we should keep the view position and size in sync with the frame
    // then resize the view. Don't do this if we're in the middle of reflow.
    // Instead wait until the DidReflow() notification
    if (NS_FRAME_SYNC_FRAME_AND_VIEW == (mState & (NS_FRAME_IN_REFLOW |
                                                   NS_FRAME_SYNC_FRAME_AND_VIEW))) {
      // Resize the view to be the same size as the frame
      nsIViewManager  *vm;
      mView->GetViewManager(vm);
      vm->ResizeView(mView, aWidth, aHeight);
      NS_RELEASE(vm);
    }
  }

  return NS_OK;
}

// Child frame enumeration

NS_IMETHODIMP
nsFrame::GetAdditionalChildListName(PRInt32 aIndex, nsIAtom** aListName) const
{
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  NS_PRECONDITION(aIndex >= 0, "invalid index number");
  *aListName = nsnull;
  return aIndex < 0 ? NS_ERROR_INVALID_ARG : NS_OK;
}

NS_IMETHODIMP nsFrame::FirstChild(nsIAtom* aListName, nsIFrame** aFirstChild) const
{
  *aFirstChild = nsnull;
  return nsnull == aListName ? NS_OK : NS_ERROR_INVALID_ARG;
}

PRBool
nsFrame::DisplaySelection(nsIPresContext& aPresContext, PRBool isOkToTurnOn)
{
  PRBool result = PR_FALSE;

  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext.GetShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv) && shell) {
    nsCOMPtr<nsIDocument> doc;
    rv = shell->GetDocument(getter_AddRefs(doc));
    if (NS_SUCCEEDED(rv) && doc) {
      result = doc->GetDisplaySelection();
      if (isOkToTurnOn && !result) {
        doc->SetDisplaySelection(PR_TRUE);
        result = PR_TRUE;
      }
    }
  }

  return result;
}

void
nsFrame::SetClipRect(nsIRenderingContext& aRenderingContext)
{
  PRBool clipState;
  const nsStyleDisplay* display;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) display);

  // Start with the auto version of the clip rect. Then overlay on top
  // of it specific offsets.
  nscoord top = 0;
  nscoord right = mRect.width;
  nscoord bottom = mRect.height;
  nscoord left = 0;
  if (0 == (NS_STYLE_CLIP_TOP_AUTO & display->mClipFlags)) {
    top += display->mClip.top;
  }
  if (0 == (NS_STYLE_CLIP_RIGHT_AUTO & display->mClipFlags)) {
    right += display->mClip.right;
  }
  if (0 == (NS_STYLE_CLIP_BOTTOM_AUTO & display->mClipFlags)) {
    bottom += display->mClip.bottom;
  }
  if (0 == (NS_STYLE_CLIP_LEFT_AUTO & display->mClipFlags)) {
    left += display->mClip.left;
  }

  // Set updated clip-rect into the rendering context
  nsRect clipRect(top, left, right - left, bottom - top);
  aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
}

NS_IMETHODIMP
nsFrame::Paint(nsIPresContext&      aPresContext,
               nsIRenderingContext& aRenderingContext,
               const nsRect&        aDirtyRect,
               nsFramePaintLayer    aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    if (DisplaySelection(aPresContext) == PR_FALSE)
      return NS_OK;

    // Get Content
    nsIContent* content;
    nsresult rv = GetContent(&content);
    if (NS_FAILED(rv) || (nsnull == content)) {
      return rv;
    }

    PRInt32 n;
    content->ChildCount(n);
    if ((n == 0) && mSelected) {
      nsRect rect;
      GetRect(rect);
      rect.width--;
      rect.height--;
      aRenderingContext.SetColor(NS_RGB(0,0,255));
      aRenderingContext.DrawRect(rect);
      aRenderingContext.DrawLine(rect.x, rect.y, rect.XMost(), rect.YMost());
      aRenderingContext.DrawLine(rect.x, rect.YMost(), rect.XMost(), rect.y);
    }
    NS_RELEASE(content);
  }
  return NS_OK;
}

/**
  *
 */
NS_IMETHODIMP
nsFrame::HandleEvent(nsIPresContext& aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus&  aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  if (DisplaySelection(aPresContext) == PR_FALSE) {
    if (aEvent->message != NS_MOUSE_LEFT_BUTTON_DOWN) {
      return NS_OK;
    }
  }

  if(nsEventStatus_eConsumeNoDefault != aEventStatus) {
    if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    } 
    else if (aEvent->message == NS_MOUSE_MOVE && mDoingSelection ||
               aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
      // no-op
    } 
    else {
      return NS_OK;
    }
    if (SELECTION_DEBUG) printf("Message: %d-------------------------------------------------------------\n",aEvent->message);


    if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
      if (mDoingSelection) {
        HandleRelease(aPresContext, aEvent, aEventStatus);
      }
    } else if (aEvent->message == NS_MOUSE_MOVE) {
      mDidDrag = PR_TRUE;
      HandleDrag(aPresContext, aEvent, aEventStatus);
//DEBUG MJUDGE DEBUG MESSAGE FOR DRAGGING FROM SELECTION
    } else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
      HandlePress(aPresContext, aEvent, aEventStatus);
    }
  }

  return NS_OK;
}

/**
  * Handles the Mouse Press Event for the frame
 */
NS_IMETHODIMP
nsFrame::HandlePress(nsIPresContext& aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus&  aEventStatus)
{
  if (!DisplaySelection(aPresContext, PR_TRUE)) {
    return NS_OK;
  }

  mDoingSelection = PR_TRUE;
  mDidDrag        = PR_FALSE;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext.GetShell(getter_AddRefs(shell));
  nsInputEvent *inputEvent = (nsInputEvent *)aEvent;
  if (NS_SUCCEEDED(rv) && shell) {
    nsCOMPtr<nsIRenderingContext> acx;      
    rv = shell->CreateRenderingContext(this, getter_AddRefs(acx));
    if (NS_SUCCEEDED(rv)){
      PRInt32 startPos = 0;
      PRUint32 contentOffset = 0;
      if (NS_SUCCEEDED(GetPosition(aPresContext, acx, aEvent, this, contentOffset, startPos))){
        nsIDOMSelection *selection = nsnull;
        if (NS_SUCCEEDED(shell->GetSelection(&selection))){
          nsIFocusTracker *tracker;
          if (NS_SUCCEEDED(shell->QueryInterface(kIFocusTracker,(void **)&tracker))){
            nsIFrameSelection *frameselection = nsnull;
            if (NS_SUCCEEDED(selection->QueryInterface(kIFrameSelection,
                                                  (void **)&frameselection))) {
              frameselection->EnableFrameNotification(PR_FALSE);
              frameselection->TakeFocus(tracker, this, startPos, contentOffset, inputEvent->isShift);
              frameselection->EnableFrameNotification(PR_TRUE);//prevent cyclic call to reset selection.
              NS_RELEASE(frameselection);
            }
            NS_RELEASE(tracker);
          }
          NS_RELEASE(selection);
        }
        //no release 
      }
    }
  }
  return NS_OK;

}

NS_IMETHODIMP nsFrame::HandleDrag(nsIPresContext& aPresContext, 
                              nsGUIEvent*     aEvent,
                              nsEventStatus&  aEventStatus)
{
  if (!DisplaySelection(aPresContext, PR_TRUE)) {
    return NS_OK;
  }

  mDoingSelection = PR_TRUE;
  mDidDrag        = PR_FALSE;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext.GetShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv) && shell) {
    nsCOMPtr<nsIRenderingContext> acx;      
    rv = shell->CreateRenderingContext(this, getter_AddRefs(acx));
    if (NS_SUCCEEDED(rv)) {
      PRInt32 startPos = 0;
      PRUint32 contentOffset = 0;
      if (NS_SUCCEEDED(GetPosition(aPresContext, acx, aEvent, this, contentOffset, startPos))){
        nsIDOMSelection *selection = nsnull;
        if (NS_SUCCEEDED(shell->GetSelection(&selection))){
          nsIFocusTracker *tracker;
          if (NS_SUCCEEDED(shell->QueryInterface(kIFocusTracker,(void **)&tracker))) {
            nsIFrameSelection* frameselection;
            if (NS_SUCCEEDED(selection->QueryInterface(kIFrameSelection, (void **)&frameselection))) {
              frameselection->EnableFrameNotification(PR_FALSE);
              frameselection->TakeFocus(tracker, this, startPos, contentOffset, PR_TRUE); //TRUE IS THE DIFFERENCE
              frameselection->EnableFrameNotification(PR_TRUE);//prevent cyclic call to reset selection.
              NS_RELEASE(frameselection);
            }
            NS_RELEASE(tracker);
          }
          NS_RELEASE(selection);
        }
        //no release 
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsFrame::HandleRelease(nsIPresContext& aPresContext, 
                                 nsGUIEvent*     aEvent,
                                 nsEventStatus&  aEventStatus)
{
  mDoingSelection = PR_FALSE;
  NS_IF_RELEASE(gDoc);
  return NS_OK;
}

//--------------------------------------------------------------------------
//-- GetPosition
//--------------------------------------------------------------------------
NS_IMETHODIMP nsFrame::GetPosition(nsIPresContext&        aPresContext,
                                   nsIRenderingContext *  aRendContext,
                                   nsGUIEvent *           aEvent,
                                   nsIFrame *             aNewFrame,
                                   PRUint32&              aAcutalContentOffset,
                                   PRInt32&               aOffset)
{

  //PRInt32 offset; 
  //PRInt32 width;
  //CalcCursorPosition(aPresContext, aEvent, aNewFrame, offset, width);
  //offset += aNewFrame->GetContentOffset();

  //return offset;
  aAcutalContentOffset = 0;
  aOffset = -1;

  return NS_OK;
}

/********************************************************
* Adjusts the Starting and Ending TextPoint for a Range
*********************************************************/
void nsFrame::AdjustPointsInNewContent(nsIPresContext& aPresContext,
                                       nsIRenderingContext * aRendContext,
                                       nsGUIEvent * aEvent,
                                       nsIFrame  * aNewFrame) {
#if 0
  PRUint32 actualOffset = 0;

  // Get new Cursor Poition in the new content
  PRInt32 newPos;
//DEBUG MJUDGE

  GetPosition(aPresContext, aRendContext, aEvent, aNewFrame, actualOffset, newPos);

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
    if (SELECTION_DEBUG) printf("--\n--\n--\n--\n--\n--\n--\n Should be here. #102\n");
    //return;
  }
#endif //0
}

/********************************************************
* Adjusts the Starting and Ending TextPoint for a Range
*********************************************************/
void nsFrame::AdjustPointsInSameContent(nsIPresContext& aPresContext,
                                        nsIRenderingContext * aRendContext,
                                        nsGUIEvent    * aEvent) {
#if 0
  PRUint32 actualOffset = 0;

  // Get new Cursor Poition in the same content
  PRInt32 newPos;
  GetPosition(aPresContext, aRendContext, aEvent, mCurrentFrame, actualOffset, newPos);
  //newPos += actualOffset;
  if (SELECTION_DEBUG) printf("AdjustTextPointsInSameContent newPos: %d\n", newPos);

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
    if (SELECTION_DEBUG) printf("--\n--\n--\n--\n--\n--\n--\n Should be here. #101\n");
    //return;
  }

    if (SELECTION_DEBUG) printf("Start %s  End %s\n", mStartSelectionPoint->ToString(), mEndSelectionPoint->ToString());
  //}
#endif //0
}

NS_IMETHODIMP
nsFrame::GetCursor(nsIPresContext& aPresContext,
                   nsPoint& aPoint,
                   PRInt32& aCursor)
{
  const nsStyleColor* styleColor;
  GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&)styleColor);
  aCursor = styleColor->mCursor;

  if ((NS_STYLE_CURSOR_AUTO == aCursor) && (nsnull != mParent)) {
    mParent->GetCursor(aPresContext, aPoint, aCursor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
  *aFrame = this;
  return NS_OK;
}

// Resize and incremental reflow
NS_IMETHODIMP
nsFrame::GetFrameState(nsFrameState* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null OUT parameter pointer");
  *aResult = mState;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::SetFrameState(nsFrameState aNewState)
{
  mState = aNewState;
  return NS_OK;
}

// nsIHTMLReflow member functions

NS_IMETHODIMP
nsFrame::WillReflow(nsIPresContext& aPresContext)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("WillReflow: oldState=%x", mState));
  mState |= NS_FRAME_IN_REFLOW;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::DidReflow(nsIPresContext& aPresContext,
                   nsDidReflowStatus aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("nsFrame::DidReflow: aStatus=%d", aStatus));
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    mState &= ~(NS_FRAME_IN_REFLOW | NS_FRAME_FIRST_REFLOW);

    // Size and position the view if requested
    if ((nsnull != mView) && (NS_FRAME_SYNC_FRAME_AND_VIEW & mState)) {
      // Position and size view relative to its parent, not relative to our
      // parent frame (our parent frame may not have a view).
      nsIView* parentWithView;
      nsPoint origin;
      GetOffsetFromView(origin, &parentWithView);
      nsIViewManager  *vm;
      mView->GetViewManager(vm);
      vm->ResizeView(mView, mRect.width, mRect.height);
      vm->MoveViewTo(mView, origin.x, origin.y);
      NS_RELEASE(vm);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::Reflow(nsIPresContext&          aPresContext,
                nsHTMLReflowMetrics&     aDesiredSize,
                const nsHTMLReflowState& aReflowState,
                nsReflowStatus&          aStatus)
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
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::FindTextRuns(nsLineLayout& aLineLayout)
{
  aLineLayout.EndTextRun();
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace)
{
  aUsedSpace = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::TrimTrailingWhiteSpace(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRC,
                                nscoord& aDeltaWidth)
{
  aDeltaWidth = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::MoveInSpaceManager(nsIPresContext* aPresContext,
                            nsISpaceManager* aSpaceManager,
                            nscoord aDeltaX, nscoord aDeltaY)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::ContentChanged(nsIPresContext* aPresContext,
                        nsIContent*     aChild,
                        nsISupports*    aSubContent)
{
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv) && shell) {
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                 nsIReflowCommand::ContentChanged);
    if (NS_SUCCEEDED(rv)) {
      shell->AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsFrame::AttributeChanged(nsIPresContext* aPresContext,
                          nsIContent*     aChild,
                          nsIAtom*        aAttribute,
                          PRInt32         aHint)
{
  return NS_OK;
}

// Flow member functions

NS_IMETHODIMP nsFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_NOT_SPLITTABLE;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::GetPrevInFlow(nsIFrame** aPrevInFlow) const
{
  *aPrevInFlow = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetPrevInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFrame::GetNextInFlow(nsIFrame** aNextInFlow) const
{
  *aNextInFlow = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetNextInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFrame::AppendToFlow(nsIFrame* aAfterFrame)
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFrame::PrependToFlow(nsIFrame* aBeforeFrame)
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFrame::RemoveFromFlow()
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFrame::BreakFromPrevFlow()
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFrame::BreakFromNextFlow()
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Associated view object
NS_IMETHODIMP nsFrame::GetView(nsIView** aView) const
{
  NS_PRECONDITION(nsnull != aView, "null OUT parameter pointer");
  *aView = mView;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetView(nsIView* aView)
{
  nsresult  rv;

  if (nsnull != aView) {
    mView = aView;
    aView->SetClientData(this);
    rv = NS_OK;
  }
  else
    rv = NS_OK;

  return rv;
}

// Find the first geometric parent that has a view
NS_IMETHODIMP nsFrame::GetParentWithView(nsIFrame** aParent) const
{
  NS_PRECONDITION(nsnull != aParent, "null OUT parameter pointer");

  nsIFrame* parent;
  for (parent = mParent; nsnull != parent; parent->GetParent(&parent)) {
    nsIView* parView;
     
    parent->GetView(&parView);
    if (nsnull != parView) {
      break;
    }
  }

  *aParent = parent;
  return NS_OK;
}

// Returns the offset from this frame to the closest geometric parent that
// has a view. Also returns the containing view or null in case of error
NS_IMETHODIMP nsFrame::GetOffsetFromView(nsPoint& aOffset, nsIView** aView) const
{
  NS_PRECONDITION(nsnull != aView, "null OUT parameter pointer");
  nsIFrame* frame = (nsIFrame*)this;

  *aView = nsnull;
  aOffset.MoveTo(0, 0);
  do {
    nsPoint origin;

    frame->GetOrigin(origin);
    aOffset += origin;
    frame->GetParent(&frame);
    if (nsnull != frame) {
      frame->GetView(aView);
    }
  } while ((nsnull != frame) && (nsnull == *aView));
  return NS_OK;
}

NS_IMETHODIMP nsFrame::GetWindow(nsIWidget** aWindow) const
{
  NS_PRECONDITION(nsnull != aWindow, "null OUT parameter pointer");
  
  nsIFrame*  frame;
  nsIWidget* window = nsnull;
  for (frame = (nsIFrame*)this; nsnull != frame; frame->GetParentWithView(&frame)) {
    nsIView* view;
     
    frame->GetView(&view);
    if (nsnull != view) {
      view->GetWidget(window);
      if (nsnull != window) {
        break;
      }
    }
  }
  NS_POSTCONDITION(nsnull != window, "no window in frame tree");
  *aWindow = window;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsnull;
  return NS_OK;
}

void
nsFrame::Invalidate(const nsRect& aDamageRect,
                    PRBool aImmediate) const
{
  nsIViewManager* viewManager = nsnull;

  PRUint32 flags = aImmediate ? NS_VMREFRESH_IMMEDIATE : NS_VMREFRESH_NO_SYNC;
  if (nsnull != mView) {
    mView->GetViewManager(viewManager);
    viewManager->UpdateView(mView, aDamageRect, flags);
    
  } else {
    nsRect    rect(aDamageRect);
    nsPoint   offset;
    nsIView*  view;
  
    GetOffsetFromView(offset, &view);
    NS_ASSERTION(nsnull != view, "no view");
    rect += offset;
    view->GetViewManager(viewManager);
    viewManager->UpdateView(view, rect, flags);
  }

  NS_IF_RELEASE(viewManager);
}

// Style sizing methods
NS_IMETHODIMP nsFrame::IsPercentageBase(PRBool& aBase) const
{
  const nsStylePosition* position;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);
  if (position->mPosition != NS_STYLE_POSITION_NORMAL) {
    aBase = PR_TRUE;
  }
  else {
    const nsStyleDisplay* display;
    GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
    if ((display->mDisplay == NS_STYLE_DISPLAY_BLOCK) || 
        (display->mDisplay == NS_STYLE_DISPLAY_LIST_ITEM) ||
        (display->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL)) {
      aBase = PR_TRUE;
    }
    else {
      aBase = PR_FALSE;
    }
  }
  return NS_OK;
}

// Sibling pointer used to link together frames

NS_IMETHODIMP nsFrame::GetNextSibling(nsIFrame** aNextSibling) const
{
  NS_PRECONDITION(nsnull != aNextSibling, "null OUT parameter pointer");
  *aNextSibling = mNextSibling;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetNextSibling(nsIFrame* aNextSibling)
{
  NS_ASSERTION(aNextSibling != this, "attempt to create circular frame list");
  mNextSibling = aNextSibling;
  return NS_OK;
}

// Transparency query
NS_IMETHODIMP nsFrame::IsTransparent(PRBool& aTransparent) const
{
  //XXX this needs to be overridden in just about every leaf class? MMP
  aTransparent = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::Scrolled(nsIView *aView)
{
  return NS_OK;
}

PRInt32 nsFrame::ContentIndexInContainer(const nsIFrame* aFrame)
{
  nsIContent* content;
  PRInt32     result = -1;

  aFrame->GetContent(&content);
  if (nsnull != content) {
    nsIContent* parentContent;

    content->GetParent(parentContent);
    if (nsnull != parentContent) {
      parentContent->IndexOf(content, result);
      NS_RELEASE(parentContent);
    }
    NS_RELEASE(content);
  }

  return result;
}

// Debugging
NS_IMETHODIMP
nsFrame::List(FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
  if (nsnull != mView) {
    fprintf(out, " [view=%p]", mView);
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  fputs("<>\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Frame", aResult);
}

nsresult
nsFrame::MakeFrameName(const char* aType, nsString& aResult) const
{
  aResult = aType;
  if (nsnull != mContent) {
    nsIAtom* tag;
    mContent->GetTag(tag);
    if ((tag != nsnull) && (tag != nsLayoutAtoms::textTagName)) {
      aResult.Append("(");
      nsAutoString buf;
      tag->ToString(buf);
      aResult.Append(buf);
      NS_RELEASE(tag);
      aResult.Append(")");
    }
  }
  char buf[40];
  PR_snprintf(buf, sizeof(buf), "(%d)", ContentIndexInContainer(this));
  aResult.Append(buf);
  return NS_OK;
}

void
nsFrame::XMLQuote(nsString& aString)
{
  PRInt32 i, len = aString.Length();
  for (i = 0; i < len; i++) {
    PRUnichar ch = aString.CharAt(i);
    if (ch == '<') {
      nsAutoString tmp("&lt;");
      aString.Cut(i, 1);
      aString.Insert(tmp, i);
      len += 3;
      i += 3;
    }
    else if (ch == '>') {
      nsAutoString tmp("&gt;");
      aString.Cut(i, 1);
      aString.Insert(tmp, i);
      len += 3;
      i += 3;
    }
    else if (ch == '\"') {
      nsAutoString tmp("&quot;");
      aString.Cut(i, 1);
      aString.Insert(tmp, i);
      len += 5;
      i += 5;
    }
  }
}

NS_IMETHODIMP
nsFrame::DumpRegressionData(FILE* out, PRInt32 aIndent)
{
  IndentBy(out, aIndent);
  fprintf(out, "<frame va=\"%ld\" type=\"", PRUptrdiff(this));
  nsAutoString name;
  GetFrameName(name);
  XMLQuote(name);
  fputs(name, out);
  fprintf(out, "\" state=\"%d\" parent=\"%ld\">\n",
          mState, PRUptrdiff(mParent));

  aIndent++;
  DumpBaseRegressionData(out, aIndent);
  aIndent--;

  IndentBy(out, aIndent);
  fprintf(out, "</frame>\n");

  return NS_OK;
}

void
nsFrame::DumpBaseRegressionData(FILE* out, PRInt32 aIndent)
{
  if (nsnull != mNextSibling) {
    IndentBy(out, aIndent);
    fprintf(out, "<next-sibling va=\"%ld\"/>\n", PRUptrdiff(mNextSibling));
  }

  if (nsnull != mView) {
    IndentBy(out, aIndent);
    fprintf(out, "<view va=\"%ld\">\n", PRUptrdiff(mView));
    aIndent++;
    // XXX add in code to dump out view state too...
    aIndent--;
    IndentBy(out, aIndent);
    fprintf(out, "</view>\n");
  }

  IndentBy(out, aIndent);
  fprintf(out, "<bbox x=\"%d\" y=\"%d\" w=\"%d\" h=\"%d\"/>\n",
          mRect.x, mRect.y, mRect.width, mRect.height);

  // Now dump all of the children on all of the child lists
  nsIFrame* kid;
  nsIAtom* list = nsnull;
  PRInt32 listIndex = 0;
  do {
    nsresult rv = FirstChild(list, &kid);
    if (NS_SUCCEEDED(rv) && (nsnull != kid)) {
      IndentBy(out, aIndent);
      if (nsnull != list) {
        nsAutoString listName;
        list->ToString(listName);
        fprintf(out, "<child-list name=\"");
        XMLQuote(listName);
        fputs(listName, out);
        fprintf(out, "\">\n");
      }
      else {
        fprintf(out, "<child-list>\n");
      }
      aIndent++;
      while (nsnull != kid) {
        kid->DumpRegressionData(out, aIndent);
        kid->GetNextSibling(&kid);
      }
      aIndent--;
      IndentBy(out, aIndent);
      fprintf(out, "</child-list>\n");
    }
    NS_IF_RELEASE(list);
    GetAdditionalChildListName(listIndex++, &list);
  } while (nsnull != list);
}

NS_IMETHODIMP
nsFrame::VerifyTree() const
{
  NS_ASSERTION(0 == (mState & NS_FRAME_IN_REFLOW), "frame is in reflow");
  return NS_OK;
}

/*this method may.. invalidate if the state was changed or if aForceRedraw is PR_TRUE
  it will not update immediately.*/
NS_IMETHODIMP
nsFrame::SetSelected(nsSelectionStruct *aSelStruct)
{
  if (!aSelStruct)
    return NS_ERROR_NULL_POINTER;
  if (mSelected != (PRBool)(aSelStruct->mType & nsSelectionStruct::SELON ) || aSelStruct->mForceRedraw)
  {
    mSelected = aSelStruct->mType & nsSelectionStruct::SELON ;
    
/*    nsRect    rect;
    GetRect(rect);
    nsIFrame *frame = this;
    nsIFrame *firstframe = nsnull;
    while (NS_SUCCEEDED(frame->GetPrevInFlow(&frame)) && frame){
      firstframe =  frame;
    }
    
    if (firstframe){
      nsRect    rect2;
      firstframe->GetRect(rect2);
      rect.y-= rect.y-rect2.y;
    }
*/
    ForceDrawFrame(this);//invalidate does not work in all cases.
    //Invalidate(rect,PR_FALSE); //false is for not immediate
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::SetSelectedContentOffsets(nsSelectionStruct *aSS, 
                                   nsIFocusTracker *aTracker,
                                   nsIFrame **aActualSelected)
{
  if (!aActualSelected || !aSS)
    return NS_ERROR_NULL_POINTER;
  nsIFrame *child = nsnull;
  nsresult result = FirstChild(nsnull, &child);
  if (NS_FAILED(result)){
    *aActualSelected = this;
    if (aSS->mAnchorOffset > 0)
        aTracker->SetFocus(nsnull,this);
    if (aSS->mFocusOffset > 0)
        aTracker->SetFocus(this,nsnull);
    return SetSelected(aSS);
  }
  *aActualSelected = nsnull;
  //wont actually bother with selection here on "this" frame
  while (child && NS_SUCCEEDED(result)){
    result |= child->SetSelectedContentOffsets(aSS, aTracker, aActualSelected);
    if (NS_SUCCEEDED(result) && aActualSelected)
      return result; //done.
    result |= child->GetNextSibling(&child);
  }
  return result;
}

NS_IMETHODIMP
nsFrame::GetSelected(PRBool *aSelected, PRInt32 *aBeginOffset, PRInt32 *aEndOffset, PRInt32 *aBeginContentOffset)
{
  if (!aSelected )
    return NS_ERROR_NULL_POINTER;
  *aSelected = mSelected;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetPointFromOffset(nsIPresContext* inPresContext, nsIRenderingContext* inRendContext, PRInt32 inOffset, nsPoint* outPoint)
{
  NS_PRECONDITION(outPoint != nsnull, "Null parameter");
	nsPoint		bottomLeft(0, 0);
  *outPoint = bottomLeft;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetChildFrameContainingOffset(PRInt32 inContentOffset, PRInt32* outFrameContentOffset, nsIFrame **outChildFrame)
{
  NS_PRECONDITION(outChildFrame && outFrameContentOffset, "Null parameter");
  *outFrameContentOffset = 0;
  *outChildFrame = this;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::PeekOffset(nsSelectionAmount aAmount, nsDirection aDirection, PRInt32 aStartOffset,
                    nsIFrame **aResultFrame, PRInt32 *aFrameOffset, PRInt32 *aContentOffset,
                    PRBool aEatingWS)
{
  //this will use the nsFrameTraversal as the default peek method.
  //this should change to use geometry and also look to ALL the child lists
  nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;
  nsresult result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),LEAF,this);
  if (NS_FAILED(result))
    return result;
  nsISupports *isupports = nsnull;
  if (aDirection == eDirNext)
    result = frameTraversal->Next();
  else 
    result = frameTraversal->Prev();

  if (NS_FAILED(result))
    return result;
  result = frameTraversal->CurrentItem(&isupports);
  if (NS_FAILED(result))
    return result;
  if (!isupports)
    return NS_ERROR_NULL_POINTER;
  //we must CAST here to an nsIFrame. nsIFrame doesnt really follow the rules
  //for speed reasons
  nsIFrame *newFrame = (nsIFrame *)isupports;
  return newFrame->PeekOffset(aAmount, aDirection, aStartOffset, aResultFrame,
                          aFrameOffset, aContentOffset, aEatingWS);
}

//-----------------------------------------------------------------------------------


 /********************************************************
* Refreshes each content's frame
*********************************************************/
static void RefreshAllContentFrames(nsIFrame * aFrame, nsIContent * aContent)
{
  nsIContent* frameContent;
  aFrame->GetContent(&frameContent);
  if (frameContent == aContent) {
    ForceDrawFrame((nsFrame *)aFrame);
  }
  NS_IF_RELEASE(frameContent);

  aFrame->FirstChild(nsnull, &aFrame);
  while (aFrame) {
    RefreshAllContentFrames(aFrame, aContent);
    aFrame->GetNextSibling(&aFrame);
  }
}

/********************************************************
* Refreshes each content's frame
*********************************************************/
#if 0
static void RefreshContentFrames(nsIPresContext& aPresContext,
                          nsIContent * aStartContent,
                          nsIContent * aEndContent)
{
  //-------------------------------------
  // Undraw all the current selected frames
  // XXX Kludge for now
  nsIPresShell *shell     = aPresContext.GetShell();
  nsIFrame     *rootFrame;
   
  shell->GetRootFrame(rootFrame);

  PRBool foundStart = PR_FALSE;
  for (PRInt32 i=0;i<fMax;i++) {
    nsIContent * node = (nsIContent *)fContentArray[i];
    if (node == aStartContent) {
      foundStart = PR_TRUE;
      //ForceDrawFrame((nsFrame *)shell->FindFrameWithContent(node));
      RefreshAllContentFrames(rootFrame, node);
      if (aStartContent == aEndContent) {
        break;
      }
    } else if (foundStart) {
      //ForceDrawFrame((nsFrame *)shell->FindFrameWithContent(node));
      RefreshAllContentFrames(rootFrame, node);
    } else if (aEndContent == node) {
      //ForceDrawFrame((nsFrame *)shell->FindFrameWithContent(node));
      RefreshAllContentFrames(rootFrame, node);
      break;
    }
  }
  //NS_RELEASE(rootFrame);
  NS_RELEASE(shell);
  //-------------------------------------
}
#endif


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
  aFrame->GetOffsetFromView(pnt, &view);
  aFrame->GetRect(rect);
  rect.x = pnt.x;
  rect.y = pnt.y;
  if (view != nsnull) {
    nsIViewManager * viewMgr;
    view->GetViewManager(viewMgr);
    if (viewMgr != nsnull) {
      viewMgr->UpdateView(view, rect, 0);
      NS_RELEASE(viewMgr);
    }
    //viewMgr->UpdateView(view, rect, NS_VMREFRESH_DOUBLE_BUFFER | NS_VMREFRESH_IMMEDIATE);
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
    nsIAtom* tag;
    aContent->GetTag(tag);
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
