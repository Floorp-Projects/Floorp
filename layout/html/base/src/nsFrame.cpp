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
#include "nsIPresShell.h"
#include "nsHTMLIIDs.h"
#include "nsIEventStateManager.h"
#include "nsIFocusTracker.h"

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
static void RefreshContentFrames(nsIPresContext& aPresContext, nsIContent * aStartContent, nsIContent * aEndContent);

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

////////////////////////////////////////////////
// Debug Listing Helper Class
////////////////////////////////////////////////
class TableListFilter : public nsIListFilter
{
private:
  static char* kTagTable[];

public:

  TableListFilter() 
  {};

  virtual ~TableListFilter()
  {};

  virtual PRBool OutputTag(nsAutoString *aTag) const
  {
    PRBool result = PR_FALSE;
    if (nsnull!=aTag  && 0!=aTag->Length())
    {
      for (PRInt32 i=0; ; i++)
      {
        const char *tableTag = kTagTable[i];
        if (nsnull==tableTag)
          break;
        if (aTag->EqualsIgnoreCase(tableTag))
        {
          result = PR_TRUE;
          break;
        }
      }
    }
    return result;
  };

};

char* TableListFilter::kTagTable[] = {
  "table",
  "tbody", "thead", "tfoot",
  "tr", "td", "th",
  "colgroup", "col",
  //"caption", captions are left out because there's no caption frame
  // to hang a decent output method on.
  "" 
};

NS_LAYOUT nsIListFilter * nsIFrame::GetFilter(nsString *aFilterName)
{
  nsIListFilter *result = nsnull;
  if (nsnull!=aFilterName)
  {
    if (aFilterName->EqualsIgnoreCase("table"))
    {
      static nsIListFilter * tableListFilter;
      if (nsnull==tableListFilter)
        tableListFilter = new TableListFilter();
      result = tableListFilter;
    }
  }
  return result;
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
static NS_DEFINE_IID(kIFocusTracker, NS_IFOCUSTRACKER_IID);
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

void* nsFrame::operator new(size_t size)
{
  void* result = new char[size];

  nsCRT::zero(result, size);
  return result;
}

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
              nsIFrame*        aGeometricParent,
              nsIFrame*        aContentParent,
              nsIStyleContext* aContext)
{
  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mGeometricParent = aGeometricParent;
  mContentParent = aContentParent;
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

NS_IMETHODIMP nsFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  if (mState & NS_FRAME_EXTERNAL_REFERENCE) {
    nsIPresShell *shell = aPresContext.GetShell();
    if (nsnull != shell) {
      shell->ClearFrameRefs(this);
      NS_RELEASE(shell);
    }
  }

  //XXX Why is this done in nsFrame instead of some frame class
  // that actually loads images?
  aPresContext.StopLoadImage(this);

  //Set to prevent event dispatch during destruct
  if (nsnull != mView) {
    mView->SetClientData(nsnull);
  }

  delete this;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  SizeOfWithoutThis(aHandler);
  return NS_OK;
}

void
nsFrame::SizeOfWithoutThis(nsISizeOfHandler* aHandler) const
{
  // Note: style context's are accounted for via the style system's
  // sizeof support

  // Note: content is accounted for via the content system's sizeof
  // support
}

NS_IMETHODIMP nsFrame::GetContent(nsIContent*& aContent) const
{
  NS_IF_ADDREF(mContent);
  aContent = mContent;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::GetStyleContext(nsIStyleContext*& aStyleContext) const
{
  NS_ASSERTION(nsnull != mStyleContext, "frame should always have style context");
  NS_IF_ADDREF(mStyleContext);
  aStyleContext = mStyleContext;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetStyleContext(nsIPresContext* aPresContext,nsIStyleContext* aContext)
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

NS_IMETHODIMP nsFrame::ReResolveStyleContext(nsIPresContext* aPresContext,
                                             nsIStyleContext* aParentContext)
{
  NS_ASSERTION(nsnull != mStyleContext, "null style context");
  if (nsnull != mStyleContext) {
    nsIAtom*  pseudoTag = nsnull;
    mStyleContext->GetPseudoType(pseudoTag);
    nsIStyleContext*  newContext;
    if (nsnull != pseudoTag) {
      newContext = 
        aPresContext->ResolvePseudoStyleContextFor(mContent, pseudoTag, aParentContext);
    }
    else {
      newContext = aPresContext->ResolveStyleContextFor(mContent, aParentContext);
    }

    NS_ASSERTION(nsnull != newContext, "failed to get new style context");
    if (nsnull != newContext) {
      if (newContext != mStyleContext) {
        NS_RELEASE(mStyleContext);
        mStyleContext = newContext;
        DidSetStyleContext(aPresContext);
      }
      else {
        NS_RELEASE(newContext);
        mStyleContext->RemapStyle(aPresContext);
      }
    }
  }
  return NS_OK;
}

// Geometric and content parent member functions

NS_IMETHODIMP nsFrame::GetContentParent(nsIFrame*& aParent) const
{
  aParent = mContentParent;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetContentParent(const nsIFrame* aParent)
{
  mContentParent = (nsIFrame*)aParent;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::GetGeometricParent(nsIFrame*& aParent) const
{
  aParent = mGeometricParent;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetGeometricParent(const nsIFrame* aParent)
{
  mGeometricParent = (nsIFrame*)aParent;
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
      GetOffsetFromView(origin, parentWithView);
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
nsFrame::GetAdditionalChildListName(PRInt32 aIndex, nsIAtom*& aListName) const
{
  NS_PRECONDITION(aIndex >= 0, "invalid index number");
  aListName = nsnull;
  return aIndex < 0 ? NS_ERROR_INVALID_ARG : NS_OK;
}

NS_IMETHODIMP nsFrame::FirstChild(nsIAtom* aListName, nsIFrame*& aFirstChild) const
{
  aFirstChild = nsnull;
  return nsnull == aListName ? NS_OK : NS_ERROR_INVALID_ARG;
}

PRBool nsFrame::DisplaySelection(nsIPresContext& aPresContext, PRBool isOkToTurnOn)
{
  PRBool result = PR_FALSE;

  nsIPresShell *shell = aPresContext.GetShell();
  if (nsnull != shell) {
    nsIDocument *doc = shell->GetDocument();
    if (nsnull != doc) {
      result = doc->GetDisplaySelection();
      if (isOkToTurnOn && !result) {
        doc->SetDisplaySelection(PR_TRUE);
        result = PR_TRUE;
      }
      NS_RELEASE(doc);
    }
    NS_RELEASE(shell);
  }
  
  return result;
}

NS_IMETHODIMP
nsFrame::Paint(nsIPresContext&      aPresContext,
               nsIRenderingContext& aRenderingContext,
               const nsRect&        aDirtyRect,
               nsFramePaintLayer    aWhichLayer)
{
  if (eFramePaintLayer_Overlay == aWhichLayer) {
    if (DisplaySelection(aPresContext) == PR_FALSE)
      return NS_OK;

    PRBool clearAfterPaint = PR_FALSE;

    // Get Content
    nsIContent* content;
    nsresult rv = GetContent(content);
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
    } else if (aEvent->message == NS_MOUSE_MOVE && mDoingSelection ||
               aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
      // no-op
    } else {
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
NS_IMETHODIMP nsFrame::HandlePress(nsIPresContext& aPresContext, 
                               nsGUIEvent*     aEvent,
                               nsEventStatus&  aEventStatus)
{
  if (DisplaySelection(aPresContext, PR_TRUE) == PR_FALSE)
  {
    aEventStatus = nsEventStatus_eIgnore;
    return NS_OK;
  }

  mDoingSelection = PR_TRUE;
  mDidDrag        = PR_FALSE;
  nsIPresShell *shell = aPresContext.GetShell();
  if (shell){
    nsIRenderingContext * acx;      
    if (NS_SUCCEEDED(shell->CreateRenderingContext(this, acx))){
      PRInt32 startPos = 0;
      PRUint32 contentOffset = 0;
      if (NS_SUCCEEDED(GetPosition(aPresContext, acx, aEvent, this, contentOffset, startPos))){
        nsISelection *selection = nsnull;
        if (NS_SUCCEEDED(shell->GetSelection(&selection))){
          nsIFocusTracker *tracker;
          if (NS_SUCCEEDED(shell->QueryInterface(kIFocusTracker,(void **)&tracker))){
            selection->TakeFocus(tracker, this, startPos, contentOffset, PR_FALSE);
            NS_RELEASE(tracker);
          }
          NS_RELEASE(selection);
        }
        //no release 
      }
      NS_RELEASE(acx);
    }
    NS_RELEASE(shell);
  }

#if 0
//DEBUG MJUDGE
  nsFrame             * currentFrame   = this;
  nsIPresShell        * shell          = aPresContext.GetShell();
  nsMouseEvent        * mouseEvent     = (nsMouseEvent *)aEvent;
  nsIRenderingContext * acx;      
  
  gDoc = shell->GetDocument();

  nsISelection     * selection;
  gDoc->GetSelection(selection);

  shell->CreateRenderingContext(this, acx);

  mSelectionRange = selection->GetRange();

  PRUint32 actualOffset = 0;

  mDoingSelection = PR_TRUE;
  mDidDrag        = PR_FALSE;
  mCurrentFrame   = currentFrame;

  GetPosition(aPresContext, acx, aEvent, currentFrame, actualOffset, mStartPos);

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

  // Get the Current Start and End Content Pointers
  nsIContent * selStartContent = mSelectionRange->GetStartContent(); // ref counted
  nsIContent * selEndContent   = mSelectionRange->GetEndContent();   // ref counted

  if (SELECTION_DEBUG) printf("****** Shift[%s]\n", (mouseEvent->isShift?"Down":"Up"));

  PRBool inRange = gDoc->IsInRange(selStartContent, selEndContent, newContent);  

  if (inRange) {
    //resetContentTrackers();
    if (TRACKER_DEBUG) printf("Adding split range to removed selection. Shift[%s]\n", (mouseEvent->isShift?"Down":"Up"));

    if (mouseEvent->isShift) {
      if (newContent == selStartContent && newContent == selEndContent) {
        addRangeToSelectionTrackers(newContent, newContent,   kInsertInAddList);
      } else {
        if (selStartContent == newContent) {
          // Trackers just do painting so add all the content nodes in the remove list
          // even though you might think you shouldn't put the start content node there
          addRangeToSelectionTrackers(selStartContent, selEndContent,   kInsertInRemoveList);
        } else if (selEndContent == newContent) {
          // just repaint the end content node
          addRangeToSelectionTrackers(selEndContent, selEndContent, kInsertInAddList);
        } else {
          if (gDoc->IsBefore(newContent, selEndContent)) {
            addRangeToSelectionTrackers(newContent,  selEndContent, kInsertInRemoveList);
          } else {
            addRangeToSelectionTrackers(selEndContent,  newContent, kInsertInRemoveList);
          }
        }
      } 
      mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_FALSE);
    } else {
      addRangeToSelectionTrackers(selStartContent, selEndContent, kInsertInRemoveList); // removed from selection

      mStartSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);
      mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);

      addRangeToSelectionTrackers(newContent, newContent, kInsertInAddList); // add to selection
    }
    
  } else if (gDoc->IsBefore(newContent, selStartContent)) {
    if (mouseEvent->isShift) {
      if (mStartSelectionPoint->IsAnchor()) {
        if (SELECTION_DEBUG) printf("New Content is before,  Start will now be end\n");

        addRangeToSelectionTrackers(selStartContent, selEndContent, kInsertInRemoveList); // removed from selection

        mEndSelectionPoint->SetPoint(selStartContent, mStartSelectionPoint->GetOffset(), PR_TRUE);
        mStartSelectionPoint->SetPoint(newContent, mStartPos, PR_FALSE);

        // End Point has changed
        nsIContent * endcontent = mEndSelectionPoint->GetContent();   // ref counted
        addRangeToSelectionTrackers(newContent, endcontent, kInsertInAddList); // add to selection
        NS_RELEASE(endcontent);
      } else {
        if (SELECTION_DEBUG) printf("New Content is before,  Appending to Beginning\n");
        addRangeToSelectionTrackers(newContent, selEndContent, kInsertInAddList); // add to selection
        mStartSelectionPoint->SetPoint(newContent, mStartPos, PR_FALSE);
      }
    } else {
      if (SELECTION_DEBUG) printf("Adding full range to removed selection. (insert selection)\n");
      addRangeToSelectionTrackers(selStartContent, selEndContent, kInsertInRemoveList); // removed from selection

      mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);

      mStartSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);

      addRangeToSelectionTrackers(newContent, newContent, kInsertInAddList); // add to selection
    }
  } else { // Content is After End

    if (mouseEvent->isShift) {
      if (selStartContent == nsnull && selEndContent == nsnull) {
        // Shift Click without first clicking in the window
        // is interpreted the same as just clicking in a window
        mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);
        mStartSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);
        addRangeToSelectionTrackers(newContent, newContent, kInsertInAddList); // add to selection

      } else if (mStartSelectionPoint->IsAnchor()) {
        if (SELECTION_DEBUG) printf("New Content is after,  Append new content\n");
        addRangeToSelectionTrackers(selEndContent, newContent,  kInsertInAddList); // add to selection
        mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_FALSE);

      } else {
        if (SELECTION_DEBUG) printf("New Content is after,  End will now be Start\n");
        
        addRangeToSelectionTrackers(selStartContent, selEndContent, kInsertInRemoveList); // removed from selection
        mStartSelectionPoint->SetPoint(selEndContent, mEndSelectionPoint->GetOffset(), PR_TRUE);
        mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_FALSE);
        addRangeToSelectionTrackers(newContent, newContent,  kInsertInAddList); // add to selection
      }

    } else { 
      if (TRACKER_DEBUG) printf("Adding full range to removed selection.\n");
      addRangeToSelectionTrackers(selStartContent, selEndContent, kInsertInRemoveList); // removed from selection

      mEndSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);

      mStartSelectionPoint->SetPoint(newContent, mStartPos, PR_TRUE);

      addRangeToSelectionTrackers(newContent, newContent, kInsertInAddList); // add to selection
    }
  }


  mSelectionRange->SetStartPoint(mStartSelectionPoint);
  mSelectionRange->SetEndPoint(mEndSelectionPoint);

  //if (selStartContent) printf("selStartContent count %d\n", selStartContent->Release());
  //if (selEndContent)   printf("selEndContent   count %d\n", selEndContent->Release());
  //if (newContent)      printf("newContent      count %d\n", newContent->Release());

  NS_IF_RELEASE(selStartContent);
  NS_IF_RELEASE(selEndContent);
  NS_IF_RELEASE(newContent);

  RefreshFromContentTrackers(aPresContext);


  if (SELECTION_DEBUG) printf("HandleEvent::mSelectionRange %s\n", mSelectionRange->ToString());

  NS_IF_RELEASE(acx);

  // Force Update
  ForceDrawFrame(this);

  NS_RELEASE(shell);
  NS_RELEASE(selection);

  aEventStatus = nsEventStatus_eIgnore;
#endif //0
  return NS_OK;

}

NS_IMETHODIMP nsFrame::HandleDrag(nsIPresContext& aPresContext, 
                              nsGUIEvent*     aEvent,
                              nsEventStatus&  aEventStatus)
{
  if (DisplaySelection(aPresContext, PR_TRUE) == PR_FALSE)
  {
    aEventStatus = nsEventStatus_eIgnore;
    return NS_OK;
  }

  mDoingSelection = PR_TRUE;
  mDidDrag        = PR_FALSE;
  nsIPresShell *shell = aPresContext.GetShell();
  if (shell){
    nsIRenderingContext * acx;      
    if (NS_SUCCEEDED(shell->CreateRenderingContext(this, acx))){
      PRInt32 startPos = 0;
      PRUint32 contentOffset = 0;
      if (NS_SUCCEEDED(GetPosition(aPresContext, acx, aEvent, this, contentOffset, startPos))){
        nsISelection *selection = nsnull;
        if (NS_SUCCEEDED(shell->GetSelection(&selection))){
          nsIFocusTracker *tracker;
          if (NS_SUCCEEDED(shell->QueryInterface(kIFocusTracker,(void **)&tracker))){
            selection->TakeFocus(tracker, this, startPos, contentOffset, PR_TRUE); //TRUE IS THE DIFFERENCE
            NS_RELEASE(tracker);
          }
          NS_RELEASE(selection);
        }
        //no release 
      }
      NS_RELEASE(acx);
    }
    NS_RELEASE(shell);
  }
#if 0
//DEBUG MJUDGE
  if (DisplaySelection(aPresContext) == PR_FALSE)
  {
    aEventStatus = nsEventStatus_eIgnore;
    return NS_OK;
  }

  // Keep old start and end
  nsIContent * startContent = mSelectionRange->GetStartContent(); // ref counted
  nsIContent * endContent   = mSelectionRange->GetEndContent();   // ref counted

  mDidDrag = PR_TRUE;

  nsIPresShell        *ps = aPresContext.GetShell();
  nsIRenderingContext *acx;
  
  ps->CreateRenderingContext(this, acx);
  NS_RELEASE(ps);

  //if (aFrame != nsnull) {
    //printf("nsFrame::HandleDrag\n");

    // Check to see if we have changed frame
    if (this != mCurrentFrame) {
      // We are in a new Frame!
      if (SELECTION_DEBUG) printf("HandleDrag::Different Frame in selection!\n");

      // Get Content for current Frame
      nsIContent * currentContent;
      mCurrentFrame->GetContent(currentContent);

      // Get Content for New Frame
      nsIContent * newContent;
      this->GetContent(newContent);

      // Check to see if we are still in the same Content
      if (currentContent == newContent) {
        if (SELECTION_DEBUG) printf("HandleDrag::New Frame, same content.\n");

        AdjustPointsInSameContent(aPresContext, acx, aEvent);
        addRangeToSelectionTrackers(currentContent, currentContent, kInsertInAddList);

      } else if (gDoc->IsBefore(newContent, currentContent)) {
        if (SELECTION_DEBUG) printf("HandleDrag::New Frame, is Before.\n");

        resetContentTrackers();
        NewContentIsBefore(aPresContext, acx, aEvent, newContent, currentContent, this);

      } else { // Content is AFTER
        if (SELECTION_DEBUG) printf("HandleDrag::New Frame, is After.\n");

        resetContentTrackers();
        NewContentIsAfter(aPresContext, acx, aEvent, newContent, currentContent, this);
      }
      mCurrentFrame = this;

      NS_IF_RELEASE(currentContent);
      NS_IF_RELEASE(newContent);
    } else if ((nsnull != mStartSelectionPoint) && (nsnull != mEndSelectionPoint)) {
      if (SELECTION_DEBUG) printf("HandleDrag::Same Frame.\n");

      // Same Frame as before
      //if (SELECTION_DEBUG) printf("\nSame Start: %s\n", mStartSelectionPoint->ToString());
      //if (SELECTION_DEBUG) printf("Same End:   %s\n",   mEndSelectionPoint->ToString());
        // [TODO] Uncomment these soon

      nsIContent * selStartContent = mStartSelectionPoint->GetContent();
      nsIContent * selEndContent   = mEndSelectionPoint->GetContent();

      if (selStartContent == selEndContent) {
        if (SELECTION_DEBUG) printf("Start & End Frame are the same: \n");
        AdjustPointsInSameContent(aPresContext, acx, aEvent);
      } else {
        if (SELECTION_DEBUG) printf("Start & End Frame are different: \n");

        // Get Content for New Frame
        nsIContent * newContent;
        this->GetContent(newContent);
        PRInt32  newPos       = -1;
        PRUint32 actualOffset = 0;

        GetPosition(aPresContext, acx, aEvent, this, actualOffset, newPos);

        if (newContent == selStartContent) {
          if (SELECTION_DEBUG) printf("New Content equals Start Content\n");
          mStartSelectionPoint->SetOffset(newPos);
          mSelectionRange->SetStartPoint(mStartSelectionPoint);
        } else if (newContent == selEndContent) {
          if (SELECTION_DEBUG) printf("New Content equals End Content\n");
          mEndSelectionPoint->SetOffset(newPos);
          mSelectionRange->SetEndPoint(mEndSelectionPoint);
        } else {
          if (SELECTION_DEBUG) printf("=====\n=====\n=====\n=====\n=====\n=====\n Should NOT be here.\n");
        }

        //if (SELECTION_DEBUG) printf("*Same Start: "+mStartSelectionPoint->GetOffset()+
        //                               " "+mStartSelectionPoint->IsAnchor()+
        //                                "  End: "+mEndSelectionPoint->GetOffset()  +
        //                               " "+mEndSelectionPoint->IsAnchor());
        NS_IF_RELEASE(newContent);
      }
      NS_IF_RELEASE(selStartContent);
      NS_IF_RELEASE(selEndContent);
    }
  //}


  NS_IF_RELEASE(startContent);
  NS_IF_RELEASE(endContent);

  NS_IF_RELEASE(acx);

  // Force Update
  ForceDrawFrame(this);
  //RefreshContentFrames(aPresContext, startContent, endContent);
  RefreshFromContentTrackers(aPresContext);

  aEventStatus = nsEventStatus_eIgnore;
#endif //0
  return NS_OK;
}

NS_IMETHODIMP nsFrame::HandleRelease(nsIPresContext& aPresContext, 
                                 nsGUIEvent*     aEvent,
                                 nsEventStatus&  aEventStatus)
{
  mDoingSelection = PR_FALSE;
  aEventStatus = nsEventStatus_eIgnore;
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

  if ((NS_STYLE_CURSOR_AUTO == aCursor) && (nsnull != mContentParent)) {
    mContentParent->GetCursor(aPresContext, aPoint, aCursor);
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
nsFrame::GetFrameState(nsFrameState& aResult)
{
  aResult = mState;
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
      GetOffsetFromView(origin, parentWithView);
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
nsFrame::TrimTrailingWhiteSpace(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRC,
                                nscoord& aDeltaWidth)
{
  aDeltaWidth = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::MoveInSpaceManager(nsIPresContext& aPresContext,
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
  return NS_OK;
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

NS_IMETHODIMP nsFrame::CreateContinuingFrame(nsIPresContext&  aPresContext,
                                         nsIFrame*        aParent,
                                         nsIStyleContext* aStyleContext,
                                         nsIFrame*&       aContinuingFrame)
{
  NS_ERROR("not splittable");
  aContinuingFrame = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFrame::GetPrevInFlow(nsIFrame*& aPrevInFlow) const
{
  aPrevInFlow = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetPrevInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsFrame::GetNextInFlow(nsIFrame*& aNextInFlow) const
{
  aNextInFlow = nsnull;
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
NS_IMETHODIMP nsFrame::GetView(nsIView*& aView) const
{
  aView = mView;
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
NS_IMETHODIMP nsFrame::GetParentWithView(nsIFrame*& aParent) const
{
  aParent = mGeometricParent;

  while (nsnull != aParent) {
    nsIView* parView;
     
    aParent->GetView(parView);
    if (nsnull != parView) {
      break;
    }
    aParent->GetGeometricParent(aParent);
  }

  return NS_OK;
}

// Returns the offset from this frame to the closest geometric parent that
// has a view. Also returns the containing view or null in case of error
NS_IMETHODIMP nsFrame::GetOffsetFromView(nsPoint& aOffset, nsIView*& aView) const
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

NS_IMETHODIMP nsFrame::GetWindow(nsIWidget*& aWindow) const
{
  nsIFrame* frame = (nsIFrame*)this;

  aWindow = nsnull;
  while (nsnull != frame) {
    nsIView* view;
     
    frame->GetView(view);
    if (nsnull != view) {
      view->GetWidget(aWindow);
      if (nsnull != aWindow) {
        break;
      }
    }
    frame->GetParentWithView(frame);
  }
  NS_POSTCONDITION(nsnull != aWindow, "no window in frame tree");
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
  
    GetOffsetFromView(offset, view);
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

NS_IMETHODIMP nsFrame::GetAutoMarginSize(PRUint8 aSide, nscoord& aSize) const
{
  aSize = 0;  // XXX probably not right, subclass override?
  return NS_OK;
}


// Sibling pointer used to link together frames

NS_IMETHODIMP nsFrame::GetNextSibling(nsIFrame*& aNextSibling) const
{
  aNextSibling = mNextSibling;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetNextSibling(nsIFrame* aNextSibling)
{
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

  aFrame->GetContent(content);
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
nsFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const
{
  // if a filter is present, only output this frame if the filter says
  // we should
  nsAutoString tagString;
  if (nsnull != mContent) {
    nsIAtom* tag;
    mContent->GetTag(tag);
    if (tag != nsnull) {
      tag->ToString(tagString);
      NS_RELEASE(tag);
    }
  }

  if ((nsnull==aFilter) || aFilter->OutputTag(&tagString)) {
    // Indent
    IndentBy(out, aIndent);

    // Output the tag and rect
    ListTag(out);
    if (nsnull != mView) {
      fprintf(out, " [view=%p]", mView);
    }
    fputs(" ", out);
    out << mRect;
    if (0 != mState) {
      fprintf(out, " [state=%08x]", mState);
    }
    fputs("<>\n", out);
  }
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
    if (tag != nsnull) {
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
    PRUnichar ch = aString[i];
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
    else if (ch == '"') {
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
  fprintf(out, "<frame type=\"");
  nsAutoString name;
  GetFrameName(name);
  XMLQuote(name);
  fputs(name, out);
  fprintf(out, "\">\n");

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
  IndentBy(out, aIndent);
  fprintf(out, "<ident addr=\"%p\"/>\n", this);

  if (mContentParent != mGeometricParent) {
    IndentBy(out, aIndent);
    fprintf(out, "<content-parent addr=\"%p\"/>\n", mContentParent);
  }

  if (nsnull != mGeometricParent) {
    IndentBy(out, aIndent);
    fprintf(out, "<geometric-parent addr=\"%p\"/>\n", mGeometricParent);
  }

  if (nsnull != mNextSibling) {
    IndentBy(out, aIndent);
    fprintf(out, "<next-sibling addr=\"%p\"/>\n", mNextSibling);
  }

  if (0 != mState) {
    IndentBy(out, aIndent);
    fprintf(out, "<flags value=\"%x\"/>\n", mState);
  }

  if (nsnull != mView) {
    IndentBy(out, aIndent);
    fprintf(out, "<view addr=\"%x\">\n", mView);
    aIndent++;
    // XXX add in code to dump out view state too...
    aIndent--;
    IndentBy(out, aIndent);
    fprintf(out, "</view>\n");
  }

  IndentBy(out, aIndent);
  fprintf(out, "<bbox x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\"/>\n",
          mRect.x, mRect.y, mRect.width, mRect.height);

  // Now dump all of the children on all of the child lists
  nsIFrame* kid;
  nsIAtom* list = nsnull;
  PRInt32 listIndex = 0;
  do {
    nsresult rv = FirstChild(list, kid);
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
        kid->GetNextSibling(kid);
      }
      aIndent--;
      IndentBy(out, aIndent);
      fprintf(out, "</child-list>\n");
    }
    NS_IF_RELEASE(list);
    GetAdditionalChildListName(listIndex++, list);
  } while (nsnull != list);
}

NS_IMETHODIMP
nsFrame::VerifyTree() const
{
  NS_ASSERTION(0 == (mState & NS_FRAME_IN_REFLOW), "frame is in reflow");
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::SetSelected(PRBool aSelected, PRInt32 aBeginOffset, PRInt32 aEndOffset, PRBool aForceRedraw)
{
  if (mSelected != aSelected || aForceRedraw)
  {
    nsRect  damageRect;
    GetRect(damageRect);
    ForceDrawFrame(this);
    mSelected = aSelected;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetSelected(PRBool *aSelected, PRInt32 *aBeginOffset, PRInt32 *aEndOffset, PRInt32 *aBeginContentOffset)
{
  if (!aSelected || !aBeginOffset || !aEndOffset || !aBeginContentOffset)
    return NS_ERROR_NULL_POINTER;
  *aSelected = mSelected;
  *aBeginContentOffset = 0;
  return NS_OK;
}

//-----------------------------------------------------------------------------------

/********************************************************
* Handles a when the cursor enters new content that is before
* the content that the cursor is currently in
*********************************************************/
void nsFrame::NewContentIsBefore(nsIPresContext& aPresContext,
                                 nsIRenderingContext * aRendContext,
                                 nsGUIEvent * aEvent,
                                 nsIContent * aNewContent,
                                 nsIContent * aCurrentContent,
                                 nsIFrame   * aNewFrame)
{
#if 0
//DEBUG MJUDGE
  if (SELECTION_DEBUG) printf("New Frame, New content is before.\n");
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

  if ((nsnull == mStartSelectionPoint) || (nsnull == mEndSelectionPoint)) {
    return;
  }
  nsIPresShell * shell           = aPresContext.GetShell();
  nsIDocument  * doc             = shell->GetDocument();
  nsIContent   * selStartContent = mStartSelectionPoint->GetContent();
  nsIContent   * selEndContent   = mEndSelectionPoint->GetContent();

  PRBool         inRange = doc->IsInRange(selStartContent, selEndContent, aNewContent);  

  // Check to see if the new content is in the selection
  if (inRange) {

    // Case #1 - Remove Current Content from Selection (at End)
    if (SELECTION_DEBUG) printf("Case #1 - (Before) New Content is in selected Range.\n");


    if (aNewContent == selStartContent) {
      // [TODO] This is where we might have to delete from end to new content

      // Returns the new End Point, if Start and End are on the
      // same content then End Point's Cursor is set to Start's
      mEndSelectionPoint->SetContent(selStartContent);
      AdjustPointsInNewContent(aPresContext, aRendContext, aEvent, aNewFrame);

    } else {
      PRUint32 actualOffset = 0;
      PRInt32  newPos;
      GetPosition(aPresContext, aRendContext, aEvent, aNewFrame, actualOffset, newPos);
      mEndSelectionPoint->SetPoint(aNewContent, newPos, PR_FALSE);
      mSelectionRange->SetEndPoint(mEndSelectionPoint);
    }

    // The current content is being removed from Selection
    addRangeToSelectionTrackers(aNewContent, aCurrentContent, kInsertInRemoveList);
  } else {
    // Case #2 - Add new content to selection (At Start)
    if (SELECTION_DEBUG) printf("Case #2 - (Before) New Content is NOT in selected Range. Moving Start Backward.\n");

    PRUint32 actualOffset = 0;
    PRInt32  newPos;

    GetPosition(aPresContext, aRendContext, aEvent, aNewFrame, actualOffset, newPos);

    // Create new TextPoint and move Start Point backward
    mStartSelectionPoint->SetPoint(aNewContent, newPos, PR_FALSE); // position is set correctly in adjustTextPoints
    mSelectionRange->SetStartPoint(mStartSelectionPoint);

    // The New Content is added to Tracker
    addRangeToSelectionTrackers(aNewContent, aCurrentContent, kInsertInAddList);
  }
  NS_RELEASE(selStartContent);
  NS_RELEASE(selEndContent);
  NS_RELEASE(doc);
  NS_RELEASE(shell);
#endif //0
}

 /********************************************************
* Refreshes each content's frame
*********************************************************/
static void RefreshAllContentFrames(nsIFrame * aFrame, nsIContent * aContent)
{
  nsIContent* frameContent;
  aFrame->GetContent(frameContent);
  if (frameContent == aContent) {
    ForceDrawFrame((nsFrame *)aFrame);
  }
  NS_IF_RELEASE(frameContent);

  aFrame->FirstChild(nsnull, aFrame);
  while (aFrame) {
    RefreshAllContentFrames(aFrame, aContent);
    aFrame->GetNextSibling(aFrame);
  }
}

/********************************************************
* Refreshes each content's frame
*********************************************************/
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

/********************************************************
* Handles a when the cursor enters new content that is After
* the content that the cursor is currently in
*********************************************************/
void nsFrame::NewContentIsAfter(nsIPresContext& aPresContext,
                                nsIRenderingContext * aRendContext,
                                nsGUIEvent * aEvent,
                                nsIContent * aNewContent,
                                nsIContent * aCurrentContent,
                                nsIFrame   * aNewFrame)
{
#if 0
//DEBUG MJUDGE

  if (SELECTION_DEBUG) printf("New Frame, New content is after.\n");


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
  nsIPresShell * shell           = aPresContext.GetShell();
  nsIDocument  * doc             = shell->GetDocument();
  nsIContent   * selStartContent = mStartSelectionPoint->GetContent();
  nsIContent   * selEndContent   = mEndSelectionPoint->GetContent();

  PRBool inRange = doc->IsInRange(selStartContent, selEndContent, aNewContent);  

  if (inRange) {
    // Case #3 - Remove Content (from Start)
    if (SELECTION_DEBUG) printf("Case #3 - (After) New Content is in selected Range.\n");

    // Remove Current Content in Tracker, but leave New Content in Selection
    addRangeToSelectionTrackers(mStartSelectionPoint->GetContent(), aNewContent, kInsertInRemoveList);

    PRUint32 actualOffset = 0;
    // [TODO] Always get nearest Text content
    PRInt32 newPos;
    GetPosition(aPresContext, aRendContext, aEvent, aNewFrame, actualOffset, newPos);

    // Check to see if the new Content is the same as the End Point's
    if (aNewContent == selEndContent) {
      if (SELECTION_DEBUG) printf("New Content matches End Point\n");

      mStartSelectionPoint->SetContent(aNewContent);
      AdjustPointsInNewContent(aPresContext, aRendContext, aEvent, aNewFrame);

    } else {
      if (SELECTION_DEBUG) printf("New Content does NOT matches End Point\n");
      mStartSelectionPoint->SetPoint(aNewContent, newPos, PR_FALSE);
      mSelectionRange->SetStartPoint(mStartSelectionPoint);
    }

  } else {
    if (SELECTION_DEBUG)
      printf("Case #4 - (After) Adding New Content\n");

    // Case #2 - Adding Content (at End)
    PRUint32 actualOffset = 0;
    // The new content is not in the selection
    PRInt32 newPos;
    GetPosition(aPresContext, aRendContext, aEvent, aNewFrame, actualOffset, newPos);

    // Check to see if we need to create a new SelectionPoint and add it
    // or do we simply move the existing start or end point
    if (selStartContent == selEndContent) {
      if (SELECTION_DEBUG) printf("Case #4 - Start & End Content the Same\n");
      // Move start or end point
      // Get new Cursor Poition in the new content

      if (mStartSelectionPoint->IsAnchor()) {
        if (SELECTION_DEBUG) printf("Case #4 - Start is Anchor\n");
        // Since the Start is the Anchor just adjust the end

        // XXX Note this includes the current End point (it should be end->nextContent)
        addRangeToSelectionTrackers(selEndContent, aNewContent, kInsertInAddList);

        mEndSelectionPoint->SetPoint(aNewContent, newPos, PR_FALSE);
        mSelectionRange->SetEndPoint(mEndSelectionPoint);

      } else {
        if (SELECTION_DEBUG) printf("Case #4 - Start is NOT Anchor\n");
        // Because End was the anchor, we need to set the Start Point to
        // the End's Offset and set it to be the new anchor
        addRangeToSelectionTrackers(selStartContent,selEndContent, kInsertInRemoveList);

        int endPos = mEndSelectionPoint->GetOffset();
        mStartSelectionPoint->SetOffset(endPos);
        mStartSelectionPoint->SetAnchor(PR_TRUE);

        // The Start point was being moved so when it jumped to the new frame
        // we needed to make it the new End Point
        mEndSelectionPoint->SetPoint(aNewContent, newPos, PR_FALSE);
        mSelectionRange->SetRange(mStartSelectionPoint, mEndSelectionPoint);

        // The Content values have changed so go get new contents
        nsIContent   * startContent = mStartSelectionPoint->GetContent();
        nsIContent   * endContent   = mEndSelectionPoint->GetContent();
        addRangeToSelectionTrackers(startContent, endContent, kInsertInRemoveList);
        NS_RELEASE(startContent);
        NS_RELEASE(endContent);
      }
    } else {
      if (SELECTION_DEBUG) printf("Case #4 - Start & End Content NOT the Same\n");
      // Adjust the end point
      mEndSelectionPoint->SetPoint(aNewContent, newPos, PR_FALSE);
      mSelectionRange->SetRange(mStartSelectionPoint, mEndSelectionPoint);

      // Add New Content to Selection Tracker
      // The Content values have changed so go get new contents
      // NOTE: selEndContent holds the "old" end content pointer
      // and endContent hold the "new" content pointer
      nsIContent * endContent = mEndSelectionPoint->GetContent();
      addRangeToSelectionTrackers(selEndContent, endContent, kInsertInAddList);
      NS_RELEASE(endContent);
    }
  }

  NS_RELEASE(selStartContent);
  NS_RELEASE(selEndContent);
  NS_RELEASE(doc);
  NS_RELEASE(shell);
#endif //0
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
    nsIViewManager * viewMgr;
    view->GetViewManager(viewMgr);
    if (viewMgr != nsnull) {
      viewMgr->UpdateView(view, rect, 0);
      NS_RELEASE(viewMgr);
    }
    //viewMgr->UpdateView(view, rect, NS_VMREFRESH_DOUBLE_BUFFER | NS_VMREFRESH_IMMEDIATE);
  }

}


/////////////////////////////////////////////////
// Selection Tracker Methods
/////////////////////////////////////////////////

//----------------------------
//
//----------------------------
static void resetContentTrackers() {
  PRInt32 i;
  for (i=0;i<fTrackerRemoveListMax;i++) {
    NS_RELEASE(fTrackerContentArrayRemoveList[i]);
  }
  for (i=0;i<fTrackerAddListMax;i++) {
    NS_RELEASE(fTrackerContentArrayAddList[i]);
  }
  fTrackerRemoveListMax = 0;
  fTrackerAddListMax    = 0;
}

//----------------------------
//
//----------------------------
static void RefreshFromContentTrackers(nsIPresContext& aPresContext) {

  PRInt32 i;
  nsIPresShell *shell = aPresContext.GetShell();
  nsIFrame     *rootFrame;
  shell->GetRootFrame(rootFrame);
  for (i=0;i<fTrackerRemoveListMax;i++) {
    RefreshAllContentFrames(rootFrame, fTrackerContentArrayRemoveList[i]);
    //ForceDrawFrame((nsFrame *)shell->FindFrameWithContent(fTrackerContentArrayRemoveList[i]));
    if (SELECTION_DEBUG) printf("ForceDrawFrame (remove) content 0x%X\n", fTrackerContentArrayRemoveList[i]);
  }
  for (i=0;i<fTrackerAddListMax;i++) {
    //nsIFrame * frame = shell->FindFrameWithContent(fTrackerContentArrayAddList[i]);
    //ForceDrawFrame((nsFrame *)frame);
    RefreshAllContentFrames(rootFrame, fTrackerContentArrayAddList[i]);
    if (SELECTION_DEBUG) printf("ForceDrawFrame (add) content 0x%X\n", fTrackerContentArrayAddList[i]);
  }
  NS_RELEASE(shell);
  resetContentTrackers();
}

//----------------------------
//
//----------------------------
static void addRangeToSelectionTrackers(nsIContent * aStartContent, nsIContent * aEndContent, PRUint32 aType) 
{
  if (aStartContent == nsnull || aEndContent == nsnull) {
    return;
  }
  nsIContent ** contentList = (aType == kInsertInRemoveList?fTrackerContentArrayRemoveList:fTrackerContentArrayAddList);
  int           inx         = (aType == kInsertInRemoveList?fTrackerRemoveListMax:fTrackerAddListMax);

  NS_ADDREF(aStartContent);
  nsIContent * contentPtr = aStartContent;
  while (contentPtr != aEndContent) {
    contentList[inx++] = contentPtr;
    contentPtr = gDoc->GetNextContent(contentPtr); // This does an AddRef
    if (nsnull == contentPtr) {
      // XXX We didn't find the end content...
      if (aType == kInsertInRemoveList) {
        fTrackerRemoveListMax = inx;
      } else { // kInsertInAddList
        fTrackerAddListMax = inx;
      }
      return;
    }
  }
  contentList[inx++] = aEndContent;

  if (SELECTION_DEBUG) printf("Adding to %s  %d\n", (aType == kInsertInRemoveList?"Remove Array":"Add Array"), inx);

  if (aType == kInsertInRemoveList) {
    fTrackerRemoveListMax = inx;
  } else { // kInsertInAddList
    fTrackerAddListMax = inx;
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
