/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsFrameList.h"
#include "nsLineLayout.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIArena.h"
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
#include "nsIFrameManager.h"

#include "nsIDOMText.h"
#include "nsDocument.h"
#include "nsIDeviceContext.h"
#include "nsHTMLIIDs.h"
#include "nsIEventStateManager.h"
#include "nsIDOMSelection.h"
#include "nsIFrameSelection.h"
#include "nsHTMLParts.h"
#include "nsLayoutAtoms.h"

#include "nsFrameTraversal.h"
#include "nsCOMPtr.h"
#include "nsStyleChangeList.h"
#include "nsIDOMRange.h"
#include "nsITableLayout.h"    //selection neccesity
#include "nsITableCellLayout.h"//  "


// Some Misc #defines
#define SELECTION_DEBUG        0
#define FORCE_SELECTION_UPDATE 1
#define CALC_DEBUG             0


#include "nsICaret.h"
#include "nsILineIterator.h"
// [HACK] Foward Declarations
void ForceDrawFrame(nsIPresContext* aPresContext, nsFrame * aFrame);
static PRBool IsSelectable(nsIFrame *aFrame); //checks style to see if we can selected

//non Hack prototypes
#if 0
static void RefreshContentFrames(nsIPresContext* aPresContext, nsIContent * aStartContent, nsIContent * aEndContent);
#endif



//----------------------------------------------------------------------

#ifdef NS_DEBUG
static PRBool gShowFrameBorders = PR_FALSE;

NS_LAYOUT void nsIFrameDebug::ShowFrameBorders(PRBool aEnable)
{
  gShowFrameBorders = aEnable;
}

NS_LAYOUT PRBool nsIFrameDebug::GetShowFrameBorders()
{
  return gShowFrameBorders;
}

static PRBool gShowEventTargetFrameBorder = PR_FALSE;

NS_LAYOUT void nsIFrameDebug::ShowEventTargetFrameBorder(PRBool aEnable)
{
  gShowEventTargetFrameBorder = aEnable;
}

NS_LAYOUT PRBool nsIFrameDebug::GetShowEventTargetFrameBorder()
{
  return gShowEventTargetFrameBorder;
}

/**
 * Note: the log module is created during library initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule;

static PRLogModuleInfo* gFrameVerifyTreeLogModuleInfo;

static PRBool gFrameVerifyTreeEnable = PRBool(0x55);

NS_LAYOUT PRBool
nsIFrameDebug::GetVerifyTreeEnable()
{
  if (gFrameVerifyTreeEnable == PRBool(0x55)) {
    if (nsnull == gFrameVerifyTreeLogModuleInfo) {
      gFrameVerifyTreeLogModuleInfo = PR_NewLogModule("frameverifytree");
      gFrameVerifyTreeEnable = 0 != gFrameVerifyTreeLogModuleInfo->level;
      printf("Note: frameverifytree is %sabled\n",
             gFrameVerifyTreeEnable ? "en" : "dis");
    }
  }
  return gFrameVerifyTreeEnable;
}

NS_LAYOUT void
nsIFrameDebug::SetVerifyTreeEnable(PRBool aEnabled)
{
  gFrameVerifyTreeEnable = aEnabled;
}

static PRLogModuleInfo* gStyleVerifyTreeLogModuleInfo;

static PRBool gStyleVerifyTreeEnable = PRBool(0x55);

NS_LAYOUT PRBool
nsIFrameDebug::GetVerifyStyleTreeEnable()
{
  if (gStyleVerifyTreeEnable == PRBool(0x55)) {
    if (nsnull == gStyleVerifyTreeLogModuleInfo) {
      gStyleVerifyTreeLogModuleInfo = PR_NewLogModule("styleverifytree");
      gStyleVerifyTreeEnable = 0 != gStyleVerifyTreeLogModuleInfo->level;
      printf("Note: styleverifytree is %sabled\n",
             gStyleVerifyTreeEnable ? "en" : "dis");
    }
  }
  return gStyleVerifyTreeEnable;
}

NS_LAYOUT void
nsIFrameDebug::SetVerifyStyleTreeEnable(PRBool aEnabled)
{
  gStyleVerifyTreeEnable = aEnabled;
}

NS_LAYOUT PRLogModuleInfo*
nsIFrameDebug::GetLogModuleInfo()
{
  if (nsnull == gLogModule) {
    gLogModule = PR_NewLogModule("frame");
  }
  return gLogModule;
}
#endif

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);
static NS_DEFINE_IID(kIFrameSelection, NS_IFRAMESELECTION_IID);
nsresult
NS_NewEmptyFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsFrame* it = new (aPresShell) nsFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

MOZ_DECL_CTOR_COUNTER(nsFrame);

// Overloaded new operator. Initializes the memory to 0 and relies on an arena
// (which comes from the presShell) to perform the allocation.
void* 
nsFrame::operator new(size_t sz, nsIPresShell* aPresShell)
{
  // Check the recycle list first.
  void* result = nsnull;
  aPresShell->AllocateFrame(sz, &result);
  
  if (result) {
    nsCRT::zero(result, sz);
  }

  return result;
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsFrame::operator delete(void* aPtr, size_t sz)
{
  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.

  // Stash the size of the object in the first four bytes of the
  // freed up memory.  The Destroy method can then use this information
  // to recycle the object.
  size_t* szPtr = (size_t*)aPtr;
  *szPtr = sz;
}


nsFrame::nsFrame()
{
  MOZ_COUNT_CTOR(nsFrame);

  mState = NS_FRAME_FIRST_REFLOW | NS_FRAME_SYNC_FRAME_AND_VIEW |
    NS_FRAME_IS_DIRTY;
}

nsFrame::~nsFrame()
{
  MOZ_COUNT_DTOR(nsFrame);

  NS_IF_RELEASE(mContent);
  NS_IF_RELEASE(mStyleContext);
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

#ifdef DEBUG
  if (aIID.Equals(NS_GET_IID(nsIFrameDebug))) {
    *aInstancePtr = (void*)(nsIFrameDebug*)this;
    return NS_OK;
  }
#endif

  if (aIID.Equals(kClassIID) || aIID.Equals(kISupportsIID)) {
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
nsFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mParent = aParent;

  if (aPrevInFlow) {
    // Make sure the general flags bits are the same
    nsFrameState  state;
    aPrevInFlow->GetFrameState(&state);
    if ((state & NS_FRAME_SYNC_FRAME_AND_VIEW) == 0) {
      mState &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;
    }
    if (state & NS_FRAME_REPLACED_ELEMENT) {
      mState |= NS_FRAME_REPLACED_ELEMENT;
    }
    if (state & NS_FRAME_SELECTED_CONTENT) {
      mState |= NS_FRAME_SELECTED_CONTENT;
    }
  }

  return SetStyleContext(aPresContext, aContext);
}

NS_IMETHODIMP nsFrame::SetInitialChildList(nsIPresContext* aPresContext,
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
nsFrame::AppendFrames(nsIPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrame::InsertFrames(nsIPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aPrevFrame,
                      nsIFrame*       aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrame::RemoveFrame(nsIPresContext* aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrame::ReplaceFrame(nsIPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aOldFrame,
                      nsIFrame*       aNewFrame)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrame::Destroy(nsIPresContext* aPresContext)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  // Get the view pointer now before the frame properties disappear
  // when we call NotifyDestroyingFrame()
  nsIView*  view;
  GetView(aPresContext, &view);
  
  // XXX Rather than always doing this it would be better if it was part of
  // a frame observer mechanism and the pres shell could register as an
  // observer of the frame while the reflow command is pending...
  if (shell) {
    shell->NotifyDestroyingFrame(this);
  }

  if ((mState & NS_FRAME_EXTERNAL_REFERENCE) ||
      (mState & NS_FRAME_SELECTED_CONTENT)) {
    if (shell) {
      shell->ClearFrameRefs(this);
    }
  }

  //XXX Why is this done in nsFrame instead of some frame class
  // that actually loads images?
  aPresContext->StopAllLoadImagesFor(this);

  if (view) {
    // Break association between view and frame
    view->SetClientData(nsnull);
    
    // Destroy the view
    view->Destroy();
  }

  // Deleting the frame doesn't really free the memory, since we're using an
  // nsIArena for allocation, but we will get our destructors called.
  delete this;

  // Now that we're totally cleaned out, we need to add ourselves to the presshell's
  // recycler.
  size_t* sz = (size_t*)this;
  shell->FreeFrame(*sz, (void*)this);

  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetContent(nsIContent** aContent) const
{
  NS_PRECONDITION(nsnull != aContent, "null OUT parameter pointer");
  *aContent = mContent;
  NS_IF_ADDREF(*aContent);
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

NS_IMETHODIMP
nsFrame::GetAdditionalStyleContext(PRInt32 aIndex, 
                                   nsIStyleContext** aStyleContext) const
{
  NS_PRECONDITION(aIndex >= 0, "invalid index number");
  NS_ASSERTION(aStyleContext, "null ptr");
  if (! aStyleContext) {
    return NS_ERROR_NULL_POINTER;
  }
  *aStyleContext = nsnull;
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsFrame::SetAdditionalStyleContext(PRInt32 aIndex, 
                                   nsIStyleContext* aStyleContext)
{
  NS_PRECONDITION(aIndex >= 0, "invalid index number");
  return ((aIndex < 0) ? NS_ERROR_INVALID_ARG : NS_OK);
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

NS_IMETHODIMP nsFrame::SetRect(nsIPresContext* aPresContext,
                               const nsRect&   aRect)
{
  MoveTo(aPresContext, aRect.x, aRect.y);
  SizeTo(aPresContext, aRect.width, aRect.height);
  return NS_OK;
}

NS_IMETHODIMP nsFrame::MoveTo(nsIPresContext* aPresContext, nscoord aX, nscoord aY)
{
  mRect.x = aX;
  mRect.y = aY;
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SizeTo(nsIPresContext* aPresContext, nscoord aWidth, nscoord aHeight)
{
  mRect.width = aWidth;
  mRect.height = aHeight;
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

NS_IMETHODIMP nsFrame::FirstChild(nsIPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame**      aFirstChild) const
{
  *aFirstChild = nsnull;
  return nsnull == aListName ? NS_OK : NS_ERROR_INVALID_ARG;
}

PRBool
nsFrame::DisplaySelection(nsIPresContext* aPresContext, PRBool isOkToTurnOn)
{
  PRBool result = PR_FALSE;

  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv) && shell) {
    nsCOMPtr<nsIDocument> doc;
    rv = shell->GetDocument(getter_AddRefs(doc));
    if (NS_SUCCEEDED(rv) && doc) {
      result = doc->GetDisplaySelection();
		  if (result) {
				// check whether style allows selection
		    const nsStyleUserInterface* userinterface;
		    GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)userinterface);
		    if (userinterface) {
					if (userinterface->mUserSelect == NS_STYLE_USER_SELECT_AUTO) {
							// if 'user-select' isn't set for this frame, use the parent's
							if (mParent) {
								mParent->GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)userinterface);
							}
					}
		      if (userinterface->mUserSelect == NS_STYLE_USER_SELECT_NONE) {
		        result = PR_FALSE;
		        isOkToTurnOn = PR_FALSE;
		      }
		    }
		  }
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
    right -= display->mClip.right;
  }
  if (0 == (NS_STYLE_CLIP_BOTTOM_AUTO & display->mClipFlags)) {
    bottom -= display->mClip.bottom;
  }
  if (0 == (NS_STYLE_CLIP_LEFT_AUTO & display->mClipFlags)) {
    left += display->mClip.left;
  }

  // Set updated clip-rect into the rendering context
  nsRect clipRect(left, top, right - left, bottom - top);
  aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
}

NS_IMETHODIMP
nsFrame::Paint(nsIPresContext*      aPresContext,
               nsIRenderingContext& aRenderingContext,
               const nsRect&        aDirtyRect,
               nsFramePaintLayer    aWhichLayer)
{
  //if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
/** GetDocument
*/
    nsCOMPtr<nsIDocument> doc;
    nsresult result; 
    nsCOMPtr<nsIPresShell> shell;
    result = aPresContext->GetShell(getter_AddRefs(shell));
    if (NS_FAILED(result))
      return result;

    PRBool displaySelection = PR_TRUE;
    result = shell->GetDisplayNonTextSelection(&displaySelection);
    if (NS_FAILED(result))
      return result;
    if (!displaySelection)
      return NS_OK;
    if (mContent) {
      result = mContent->GetDocument(*getter_AddRefs(doc));
    }
    if (!doc || NS_FAILED(result)) {
      if (NS_SUCCEEDED(result) && shell) {
        result = shell->GetDocument(getter_AddRefs(doc));
      }
    }

    displaySelection = doc->GetDisplaySelection();
    nsFrameState  frameState;
    PRBool        isSelected;
    GetFrameState(&frameState);
    isSelected = (frameState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;
//    PRInt32 selectionStartOffset = 0;//frame coordinates
//    PRInt32 selectionEndOffset = 0;//frame coordinates

    if (!displaySelection || !isSelected)
      return NS_OK;

    nsCOMPtr<nsIDOMSelection> selection;
    nsCOMPtr<nsIFrameSelection> frameSelection;

    nsCOMPtr<nsIContent> newContent;
    result = mContent->GetParent(*getter_AddRefs(newContent));

    SelectionDetails *details;
    PRInt32 offset;
    if (NS_SUCCEEDED(result) && newContent){
      result = newContent->IndexOf(mContent, offset);
      if (NS_FAILED(result)) 
      {
        return result;
      }
    }

    if (NS_SUCCEEDED(result) && shell){
      result = shell->GetFrameSelection(getter_AddRefs(frameSelection));
      if (NS_SUCCEEDED(result) && frameSelection){
        result = frameSelection->LookUpSelection(newContent, offset, 
                              1, &details, PR_FALSE);
      }
    }
  //}
  if (details)
  {
    nsRect rect;
    GetRect(rect);
    rect.width-=2;
    rect.height-=2;
    rect.x++;
    rect.y++;
    aRenderingContext.SetColor(NS_RGB(0,0,255));
    nsRect drawrect(1, 1, rect.width, rect.height);
    aRenderingContext.DrawRect(drawrect );
    SelectionDetails *deletingDetails = details;
    while((deletingDetails = details->mNext) != nsnull) { 
      delete details;
      details = deletingDetails;
    }
    delete details;
    //aRenderingContext.DrawLine(rect.x, rect.y, rect.XMost(), rect.YMost());
    //aRenderingContext.DrawLine(rect.x, rect.YMost(), rect.XMost(), rect.y);
  }
  return NS_OK;
}

/**
  *
 */
NS_IMETHODIMP  
nsFrame::GetContentForEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIContent** aContent)
{
  return GetContent(aContent);
}

/**
  *
 */
NS_IMETHODIMP
nsFrame::HandleEvent(nsIPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

/*i have no idea why this is here keeping incase..
  if (DisplaySelection(aPresContext) == PR_FALSE) {
    if (aEvent->message != NS_MOUSE_LEFT_BUTTON_DOWN) {
      return NS_OK;
    }
  }
*/
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
  switch (aEvent->message)
  {
  case NS_MOUSE_MOVE:
    {
    if (NS_SUCCEEDED(rv)){
      nsCOMPtr<nsIFrameSelection> frameselection;
      if (NS_SUCCEEDED(shell->GetFrameSelection(getter_AddRefs(frameselection))) && frameselection){
          PRBool mouseDown = PR_FALSE;
          if (NS_SUCCEEDED(frameselection->GetMouseDownState(&mouseDown)) && mouseDown)
            HandleDrag(aPresContext, aEvent, aEventStatus);            
        }
      }
    }break;
  case NS_MOUSE_LEFT_BUTTON_DOWN:
    {
      nsCOMPtr<nsIFrameSelection> frameselection;
      if (NS_SUCCEEDED(shell->GetFrameSelection(getter_AddRefs(frameselection))) && frameselection)
      {
        frameselection->SetMouseDownState( PR_TRUE);//not important if it fails here
        if (!IsMouseCaptured(aPresContext))
          CaptureMouse(aPresContext, PR_TRUE);
      }
      HandlePress(aPresContext, aEvent, aEventStatus);
    }break;
  case NS_MOUSE_LEFT_BUTTON_UP:
    HandleRelease(aPresContext, aEvent, aEventStatus);
    break;
  default:
    break;
  }//end switch
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetDataForTableSelection(nsMouseEvent *aMouseEvent, nsIContent **aParentContent,
                                  PRInt32 *aContentOffset, PRUint32 *aTarget)
{
  if (!aMouseEvent || !aParentContent || !aContentOffset || !aTarget)
    return NS_ERROR_NULL_POINTER;

  *aParentContent = nsnull;
  *aContentOffset = 0;
  *aTarget = 0;

  // Test if special 'table selection' key is pressed
  PRBool doTableSelection;

#ifdef XP_MAC
  doTableSelection = aMouseEvent->isMeta;
#else
  doTableSelection = aMouseEvent->isControl;
#endif

  if (!doTableSelection) return NS_OK;

  // Get the cell frame or table frame (or parent) of the current content node
  nsIFrame *frame = this;
  nsresult result = NS_OK;
  PRBool foundCell = PR_FALSE;
  PRBool foundTable = PR_FALSE;

  while (frame && NS_SUCCEEDED(result))
  {
    // Check for a table cell by querying to a known CellFrame interface
    nsITableCellLayout *cellElement;
    result = (frame)->QueryInterface(nsITableCellLayout::GetIID(), (void **)&cellElement);
    if (NS_SUCCEEDED(result) && cellElement)
    {
      foundCell = PR_TRUE;
      break;
    }
    else
    {
      // If not a cell, check for table
      // This will happen when starting frame is the table or child of a table,
      //  such as a row (we were inbetween cells or in table border)
      nsITableLayout *tableElement;
      result = (frame)->QueryInterface(nsITableLayout::GetIID(), (void **)&tableElement);
      if (NS_SUCCEEDED(result) && tableElement)
      {
        foundTable = PR_TRUE;
        break;
      } else {
        result = frame->GetParent(&frame);
      }
    }
  }
  // We aren't in a cell or table
  if (!foundCell && !foundTable) return NS_OK;

  nsCOMPtr<nsIContent> tableOrCellContent;
  result = frame->GetContent(getter_AddRefs(tableOrCellContent));
  if (NS_FAILED(result)) return result;
  if (!tableOrCellContent) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> parentContent;
  result = tableOrCellContent->GetParent(*getter_AddRefs(parentContent));
  if (NS_FAILED(result)) return result;
  if (!parentContent) return NS_ERROR_FAILURE;

  PRInt32 offset;
  result = parentContent->IndexOf(tableOrCellContent, offset);
  if (NS_FAILED(result)) return result;
  // Not likely?
  if (offset < 0) return NS_ERROR_FAILURE;

  // Everything is OK -- set the return values
  *aParentContent = parentContent;
  NS_ADDREF(*aParentContent);

  *aContentOffset = offset;

  if (foundCell)
    *aTarget = TABLESELECTION_CELL;
 
  if (foundTable)
  {
    //TODO: Put logic to find "hit" spots for selecting column or row here
    *aTarget = TABLESELECTION_TABLE;
  }

  return NS_OK;
}

static PRBool IsSelectable(nsIFrame *aFrame) //checks style to see if we can selected
{
  if (!aFrame)
    return PR_FALSE;
  nsIFrame *parent;
  if (NS_FAILED(aFrame->GetParent(&parent)))
    return PR_FALSE;
	const nsStyleUserInterface* userinterface;
	aFrame->GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)userinterface);
	if (userinterface) {
		if (userinterface->mUserSelect == NS_STYLE_USER_SELECT_AUTO) {
				// if 'user-select' isn't set for this frame, use the parent's
				if (parent) {
					parent->GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)userinterface);
				}
		}
		if (userinterface->mUserSelect == NS_STYLE_USER_SELECT_NONE) {
		  return PR_FALSE;//do not continue no selection for this frame.
		}
	}
  return PR_TRUE;//default to true.
}


/**
  * Handles the Mouse Press Event for the frame
 */
NS_IMETHODIMP
nsFrame::HandlePress(nsIPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{
  if (!DisplaySelection(aPresContext)) {
    return NS_OK;
  }

  nsMouseEvent *me = (nsMouseEvent *)aEvent;
  if (me->clickCount >1 )
    return HandleMultiplePress(aPresContext,aEvent,aEventStatus);


	// check whether style allows selection
  // if not dont tell selection the mouse event even occured.
  if (!IsSelectable(this))
    return NS_OK;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv) && shell) {
    PRInt32 startPos = 0;
//    PRUint32 contentOffset = 0;
    PRInt32 contentOffsetEnd = 0;
    nsCOMPtr<nsIContent> newContent;
    PRBool beginContent;
    if (NS_SUCCEEDED(GetContentAndOffsetsFromPoint(aPresContext, aEvent->point,
                                 getter_AddRefs(newContent),
                                 startPos, contentOffsetEnd, beginContent)))
    {
      nsCOMPtr<nsIFrameSelection> frameselection;
      if (NS_SUCCEEDED(shell->GetFrameSelection(getter_AddRefs(frameselection))) && frameselection)
      {
        frameselection->SetMouseDownState(PR_TRUE);//not important if it fails here
        if (!IsMouseCaptured(aPresContext))
          CaptureMouse(aPresContext, PR_TRUE);

        nsCOMPtr<nsIContent>parentContent;
        PRInt32  contentOffset;
        PRUint32 target;
        nsresult result = GetDataForTableSelection(me, getter_AddRefs(parentContent), &contentOffset, &target);
        if (NS_SUCCEEDED(result) && parentContent)
          frameselection->HandleTableSelection(parentContent, contentOffset, target, me);
        else
          frameselection->HandleClick(newContent, startPos , contentOffsetEnd , me->isShift, PR_FALSE, beginContent);
      }
    }
  }
  return NS_OK;

}
 
/**
  * Handles the Multiple Mouse Press Event for the frame
 */
NS_IMETHODIMP
nsFrame::HandleMultiplePress(nsIPresContext* aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus*  aEventStatus)
{
   if (!DisplaySelection(aPresContext)) {
    return NS_OK;
  }
  nsMouseEvent *me = (nsMouseEvent *)aEvent;
  if (me->clickCount <3 )
    return NS_OK;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
   

  if (NS_SUCCEEDED(rv) && shell) {
    nsCOMPtr<nsIRenderingContext> acx;      
    nsCOMPtr<nsIFocusTracker> tracker;
    tracker = do_QueryInterface(shell, &rv);
    if (NS_FAILED(rv) || !tracker)
      return rv;
    rv = shell->CreateRenderingContext(this, getter_AddRefs(acx));
    if (NS_SUCCEEDED(rv)){
      PRInt32 startPos = 0;
      PRInt32 contentOffsetEnd = 0;
      nsCOMPtr<nsIContent> newContent;
      PRBool beginContent = PR_FALSE;
      if (NS_SUCCEEDED(GetContentAndOffsetsFromPoint(aPresContext, aEvent->point,
                                   getter_AddRefs(newContent),
                                   startPos, contentOffsetEnd,beginContent))) {
        // find which word needs to be selected! use peek offset one
        // way then the other
        nsCOMPtr<nsIContent> startContent;
        nsCOMPtr<nsIDOMNode> startNode;
        nsCOMPtr<nsIContent> endContent;
        nsCOMPtr<nsIDOMNode> endNode;
        //peeks{}
        nsPeekOffsetStruct startpos;
        startpos.SetData(tracker, 
                        0, 
                        eSelectBeginLine,
                        eDirPrevious,
                        startPos,
                        PR_FALSE,
                        PR_TRUE,
                        PR_TRUE);
        rv = PeekOffset(aPresContext, &startpos);
        if (NS_FAILED(rv))
          return rv;
        nsPeekOffsetStruct endpos;
        endpos.SetData(tracker, 
                        0, 
                        eSelectEndLine,
                        eDirNext,
                        startPos,
                        PR_FALSE,
                        PR_FALSE,
                        PR_TRUE);
        rv = PeekOffset(aPresContext, &endpos);
        if (NS_FAILED(rv))
          return rv;

        endNode = do_QueryInterface(endpos.mResultContent,&rv);
        if (NS_FAILED(rv))
          return rv;
        startNode = do_QueryInterface(startpos.mResultContent,&rv);
        if (NS_FAILED(rv))
          return rv;

        nsCOMPtr<nsIDOMSelection> selection;
        if (NS_SUCCEEDED(shell->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection)))){
          rv = selection->Collapse(startNode,startpos.mContentOffset);
          if (NS_FAILED(rv))
            return rv;
          rv = selection->Extend(endNode,endpos.mContentOffset);
          if (NS_FAILED(rv))
            return rv;
        }
        //no release 
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsFrame::HandleDrag(nsIPresContext* aPresContext, 
                                  nsGUIEvent*     aEvent,
                                  nsEventStatus*  aEventStatus)
{
  if (!DisplaySelection(aPresContext)) {
    return NS_OK;
  }
  nsresult result;

  nsCOMPtr<nsIPresShell> presShell;

  result = aPresContext->GetShell(getter_AddRefs(presShell));

  if (NS_FAILED(result))
    return result;

  nsCOMPtr<nsIFrameSelection> frameselection;

  result = presShell->GetFrameSelection(getter_AddRefs(frameselection));

  if (NS_SUCCEEDED(result) && frameselection)
  {
    frameselection->StopAutoScrollTimer();

    // Check if we are dragging in a table cell
    nsCOMPtr<nsIContent> parentContent;
    PRInt32 contentOffset;
    PRUint32 target;
    nsMouseEvent *me = (nsMouseEvent *)aEvent;
    result = GetDataForTableSelection(me, getter_AddRefs(parentContent), &contentOffset, &target);
    if (NS_SUCCEEDED(result) && parentContent)
      frameselection->HandleTableSelection(parentContent, contentOffset, target, me);
    else
      frameselection->HandleDrag(aPresContext, this, aEvent->point);

    frameselection->StartAutoScrollTimer(aPresContext, this, aEvent->point, 30);
  }

  return NS_OK;
}

NS_IMETHODIMP nsFrame::HandleRelease(nsIPresContext* aPresContext, 
                                     nsGUIEvent*     aEvent,
                                     nsEventStatus*  aEventStatus)
{
  if (!DisplaySelection(aPresContext))
    return NS_OK;

  nsresult result;

  nsCOMPtr<nsIPresShell> presShell;

  result = aPresContext->GetShell(getter_AddRefs(presShell));

  if (NS_SUCCEEDED(result))
  {
    nsCOMPtr<nsIFrameSelection> frameselection;

    result = presShell->GetFrameSelection(getter_AddRefs(frameselection));
    if (IsMouseCaptured(aPresContext))
      CaptureMouse(aPresContext, PR_FALSE);

    if (NS_SUCCEEDED(result) && frameselection)
      frameselection->StopAutoScrollTimer();
  }

  return NS_OK;
}


nsresult nsFrame::GetContentAndOffsetsFromPoint(nsIPresContext* aCX,
                                                const nsPoint&  aPoint,
                                                nsIContent **   aNewContent,
                                                PRInt32&        aContentOffset,
                                                PRInt32&        aContentOffsetEnd,
                                                PRBool&         aBeginFrameContent)
{
  nsresult result = NS_ERROR_FAILURE;

  if (!aNewContent)
    return NS_ERROR_NULL_POINTER;

  // Traverse through children and look for the best one to give this
  // to if it fails the getposition call, make it yourself also only
  // look at primary list
  nsIView  *view         = nsnull;
  nsIFrame *kid          = nsnull;
  nsIFrame *closestFrame = nsnull;

  result = GetClosestViewForFrame(aCX, this, &view);

  if (NS_FAILED(result))
    return result;

  result = FirstChild(aCX, nsnull, &kid);

  if (NS_SUCCEEDED(result) && nsnull != kid) {

#define HUGE_DISTANCE 999999 //some HUGE number that will always fail first comparison

    PRInt32 closestXDistance = HUGE_DISTANCE;
    PRInt32 closestYDistance = HUGE_DISTANCE;

    while (nsnull != kid) {

      // Skip over generated content kid frames, or frames
      // that don't have a proper parent-child relationship!

      PRBool skipThisKid = PR_FALSE;
      nsFrameState frameState;
      result = kid->GetFrameState(&frameState);

      if (NS_FAILED(result))
        return result;

      if (frameState & NS_FRAME_GENERATED_CONTENT) {
        // It's generated content, so skip it!
        skipThisKid = PR_TRUE;
      }
      else {
        // The frame's content is not generated. Now check
        // if it is anonymous content!

        nsCOMPtr<nsIContent> kidContent;

        result = kid->GetContent(getter_AddRefs(kidContent));

        if (NS_SUCCEEDED(result) && kidContent) {
          nsCOMPtr<nsIContent> content;

          result = kidContent->GetParent(*getter_AddRefs(content));

          if (NS_SUCCEEDED(result) && content) {
            PRInt32 kidCount = 0;

            result = content->ChildCount(kidCount);
            if (NS_SUCCEEDED(result)) {

              PRInt32 kidIndex = 0;
              result = content->IndexOf(kidContent, kidIndex);

              // IndexOf() should return -1 for the index if it doesn't
              // find kidContent in it's child list.

              if (NS_SUCCEEDED(result) && (kidIndex < 0 || kidIndex >= kidCount)) {
                // Must be anonymous content! So skip it!
                skipThisKid = PR_TRUE;
              }
            }
          }
        }
      }

      if (skipThisKid) {
        kid->GetNextSibling(&kid);
        continue;
      }

      // Kid frame has content that has a proper parent-child
      // relationship. Now see if the aPoint inside it's bounding
      // rect or close by.

      nsRect rect;
      nsPoint offsetPoint(0,0);
      nsIView * kidView = nsnull;

      kid->GetRect(rect);
      kid->GetOffsetFromView(aCX, offsetPoint, &kidView);

      rect.x = offsetPoint.x;
      rect.y = offsetPoint.y;

      nscoord ya = rect.y;
      nscoord yb = rect.y + rect.height;

      PRInt32 yDistance = PR_MIN(abs(ya - aPoint.y),abs(yb - aPoint.y));

      if (yDistance <= closestYDistance && rect.width > 0 && rect.height > 0)
      {
        if (yDistance < closestYDistance)
          closestXDistance = HUGE_DISTANCE;

        nscoord xa = rect.x;
        nscoord xb = rect.x + rect.width;

        if (xa <= aPoint.x && xb >= aPoint.x && ya <= aPoint.y && yb >= aPoint.y)
        {
          closestFrame = kid;
          break;
        }

        PRInt32 xDistance = PR_MIN(abs(xa - aPoint.x),abs(xb - aPoint.x));

        if (xDistance < closestXDistance || (xDistance == closestXDistance && rect.x <= aPoint.x))
        {
          closestXDistance = xDistance;
          closestYDistance = yDistance;
          closestFrame     = kid;
        }
        // else if (xDistance > closestXDistance)
        //   break;//done
      }
      
      kid->GetNextSibling(&kid);
    }
    if (closestFrame) {

      // If we cross a view boundary, we need to adjust
      // the coordinates because GetPosition() expects
      // them to be relative to the closest view.

      nsPoint newPoint     = aPoint;
      nsIView *closestView = nsnull;

      result = GetClosestViewForFrame(aCX, closestFrame, &closestView);

      if (NS_FAILED(result))
        return result;

      if (closestView && view != closestView)
      {
        nscoord vX = 0, vY = 0;
        result = closestView->GetPosition(&vX, &vY);
        if (NS_SUCCEEDED(result))
        {
          newPoint.x -= vX;
          newPoint.y -= vY;
        }
      }

      // printf("      0x%.8x   0x%.8x  %4d  %4d\n",
      //        closestFrame, closestView, closestXDistance, closestYDistance);

      return closestFrame->GetContentAndOffsetsFromPoint(aCX, newPoint, aNewContent,
                                                         aContentOffset, aContentOffsetEnd,aBeginFrameContent);
    }
  }

  if (!mContent)
    return NS_ERROR_NULL_POINTER;
  nsRect thisRect;
  result = GetRect(thisRect);
  if (NS_FAILED(result))
    return result;
  nsPoint offsetPoint;
  GetOffsetFromView(aCX, offsetPoint, &view);
  thisRect.x = offsetPoint.x;
  thisRect.y = offsetPoint.y;

  result = mContent->GetParent(*aNewContent);
  if (*aNewContent){
    result = (*aNewContent)->IndexOf(mContent, aContentOffset);
    if (NS_FAILED(result) || aContentOffset < 0) 
    {
      return (result?result:NS_ERROR_FAILURE);
    }
    aBeginFrameContent = PR_TRUE;
    if (thisRect.Contains(aPoint))
      aContentOffsetEnd = aContentOffset +1;
    else 
    {
      if ((thisRect.x + thisRect.width) < aPoint.x  || thisRect.y > aPoint.y)
      {
        aBeginFrameContent = PR_FALSE;
        aContentOffset++;
      }
      aContentOffsetEnd = aContentOffset;
    }
  }
  return result;
}



NS_IMETHODIMP
nsFrame::GetCursor(nsIPresContext* aPresContext,
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
nsFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                          const nsPoint& aPoint,
                          nsFramePaintLayer aWhichLayer,
                          nsIFrame** aFrame)
{
  if ((aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND) &&
      (mRect.Contains(aPoint))) {
    const nsStyleDisplay* disp = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);
    if (disp->IsVisible()) {
      *aFrame = this;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
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
nsFrame::WillReflow(nsIPresContext* aPresContext)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("WillReflow: oldState=%x", mState));
  mState |= NS_FRAME_IN_REFLOW;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::DidReflow(nsIPresContext* aPresContext,
                   nsDidReflowStatus aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("nsFrame::DidReflow: aStatus=%d", aStatus));
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    mState &= ~(NS_FRAME_IN_REFLOW | NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
                NS_FRAME_HAS_DIRTY_CHILDREN);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFrame::Reflow(nsIPresContext*          aPresContext,
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
                          PRInt32         aNameSpaceID,
                          nsIAtom*        aAttribute,
                          PRInt32         aHint)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::ContentStateChanged(nsIPresContext* aPresContext,
                             nsIContent*     aChild,
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

NS_IMETHODIMP nsFrame::SetPrevInFlow(nsIFrame* aPrevInFlow)
{
  // Ignore harmless requests to set it to NULL
  if (aPrevInFlow) {
    NS_ERROR("not splittable");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
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

// Associated view object
NS_IMETHODIMP nsFrame::GetView(nsIPresContext* aPresContext, nsIView** aView) const
{
  NS_ENSURE_ARG_POINTER(aView);

  // Initialize OUT parameter
  *aView = nsnull;

  // Check the frame state bit and see if the frame has a view
  if (mState & NS_FRAME_HAS_VIEW) {
    // Check for a property on the frame
    nsCOMPtr<nsIPresShell>     presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
  
    if (presShell) {
      nsCOMPtr<nsIFrameManager>  frameManager;
      presShell->GetFrameManager(getter_AddRefs(frameManager));
    
      if (frameManager) {
        void* value;
        frameManager->GetFrameProperty((nsIFrame*)this, nsLayoutAtoms::viewProperty, 0, &value);
        *aView = (nsIView*)value;
        NS_ASSERTION(value != 0, "frame state bit was set but frame has no view");
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetView(nsIPresContext* aPresContext, nsIView* aView)
{
  if (aView) {
    aView->SetClientData(this);

    // Set a property on the frame
    nsCOMPtr<nsIPresShell>  presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    
    if (presShell) {
      nsCOMPtr<nsIFrameManager>  frameManager;
      presShell->GetFrameManager(getter_AddRefs(frameManager));
    
      if (frameManager) {
        frameManager->SetFrameProperty(this, nsLayoutAtoms::viewProperty,
                                       aView, nsnull);
      }
    }

    // Set the frame state bit that says the frame has a view
    mState |= NS_FRAME_HAS_VIEW;
  }

  return NS_OK;
}

// Find the first geometric parent that has a view
NS_IMETHODIMP nsFrame::GetParentWithView(nsIPresContext* aPresContext,
                                         nsIFrame**      aParent) const
{
  NS_PRECONDITION(nsnull != aParent, "null OUT parameter pointer");

  nsIFrame* parent;
  for (parent = mParent; nsnull != parent; parent->GetParent(&parent)) {
    nsIView* parView;
     
    parent->GetView(aPresContext, &parView);
    if (nsnull != parView) {
      break;
    }
  }

  *aParent = parent;
  return NS_OK;
}

// Returns the offset from this frame to the closest geometric parent that
// has a view. Also returns the containing view or null in case of error
NS_IMETHODIMP nsFrame::GetOffsetFromView(nsIPresContext* aPresContext,
                                         nsPoint&        aOffset,
                                         nsIView**       aView) const
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
      frame->GetView(aPresContext, aView);
    }
  } while ((nsnull != frame) && (nsnull == *aView));
  return NS_OK;
}

NS_IMETHODIMP nsFrame::GetWindow(nsIPresContext* aPresContext,
                                 nsIWidget**     aWindow) const
{
  NS_PRECONDITION(nsnull != aWindow, "null OUT parameter pointer");
  
  nsIFrame*  frame;
  nsIWidget* window = nsnull;
  for (frame = (nsIFrame*)this; nsnull != frame; frame->GetParentWithView(aPresContext, &frame)) {
    nsIView* view;
     
    frame->GetView(aPresContext, &view);
    if (nsnull != view) {
      view->GetWidget(window);
      if (nsnull != window) {
        break;
      }
    }
  }

  if (nsnull == window) {
    // Ask the view manager for the widget

    // First we have to get to a frame with a view
    nsIView* view;
    GetView(aPresContext, &view);
    if (nsnull == view) {
      GetParentWithView(aPresContext, &frame);
      if (nsnull != frame) {
        GetView(aPresContext, &view);
      }
    }
     // From the view get the view manager
    if (nsnull != view) {
      nsCOMPtr<nsIViewManager> vm;
      view->GetViewManager(*getter_AddRefs(vm));
      vm->GetWidget(&window);
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
nsFrame::Invalidate(nsIPresContext* aPresContext,
                    const nsRect&   aDamageRect,
                    PRBool          aImmediate) const
{
  nsIViewManager* viewManager = nsnull;
  nsRect damageRect(aDamageRect);

  // Checks to see if the damaged rect should be infalted 
  // to include the outline
  const nsStyleSpacing* spacing;
  GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)spacing);
  nscoord width;
  spacing->GetOutlineWidth(width);
  if (width > 0) {
    damageRect.Inflate(width, width);
  }

  PRUint32 flags = aImmediate ? NS_VMREFRESH_IMMEDIATE : NS_VMREFRESH_NO_SYNC;
  nsIView* view;

  GetView(aPresContext, &view);
  if (view) {
    view->GetViewManager(viewManager);
    viewManager->UpdateView(view, damageRect, flags);
    
  } else {
    nsRect    rect(damageRect);
    nsPoint   offset;
  
    GetOffsetFromView(aPresContext, offset, &view);
    NS_ASSERTION(nsnull != view, "no view");
    rect += offset;
    view->GetViewManager(viewManager);
    viewManager->UpdateView(view, rect, flags);
  }

  NS_IF_RELEASE(viewManager);
}

#define MAX_REFLOW_DEPTH 500

PRBool
nsFrame::IsFrameTreeTooDeep(const nsHTMLReflowState& aReflowState,
                            nsHTMLReflowMetrics& aMetrics)
{
  if (aReflowState.mReflowDepth > MAX_REFLOW_DEPTH) {
    mState |= NS_FRAME_IS_UNFLOWABLE;
    mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
    aMetrics.width = 0;
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.descent = 0;
    aMetrics.mCarriedOutBottomMargin = 0;
    aMetrics.mOverflowArea.x = 0;
    aMetrics.mOverflowArea.y = 0;
    aMetrics.mOverflowArea.width = 0;
    aMetrics.mOverflowArea.height = 0;
    if (aMetrics.maxElementSize) {
      aMetrics.maxElementSize->width = 0;
      aMetrics.maxElementSize->height = 0;
    }
    return PR_TRUE;
  }
  mState &= ~NS_FRAME_IS_UNFLOWABLE;
  return PR_FALSE;
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

#ifdef NS_DEBUG
// Debugging
NS_IMETHODIMP
nsFrame::List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);

  nsIView*  view;
  GetView(aPresContext, &view);
  if (view) {
    fprintf(out, " [view=%p]", view);
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  fputs("\n", out);
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
  aResult.AssignWithConversion(aType);
  if (nsnull != mContent) {
    nsIAtom* tag;
    mContent->GetTag(tag);
    if ((tag != nsnull) && (tag != nsLayoutAtoms::textTagName)) {
      aResult.AppendWithConversion("(");
      nsAutoString buf;
      tag->ToString(buf);
      aResult.Append(buf);
      NS_RELEASE(tag);
      aResult.AppendWithConversion(")");
    }
  }
  char buf[40];
  PR_snprintf(buf, sizeof(buf), "(%d)", ContentIndexInContainer(this));
  aResult.AppendWithConversion(buf);
  return NS_OK;
}
#endif

void
nsFrame::XMLQuote(nsString& aString)
{
  PRInt32 i, len = aString.Length();
  for (i = 0; i < len; i++) {
    PRUnichar ch = aString.CharAt(i);
    if (ch == '<') {
      nsAutoString tmp; tmp.AssignWithConversion("&lt;");
      aString.Cut(i, 1);
      aString.Insert(tmp, i);
      len += 3;
      i += 3;
    }
    else if (ch == '>') {
      nsAutoString tmp; tmp.AssignWithConversion("&gt;");
      aString.Cut(i, 1);
      aString.Insert(tmp, i);
      len += 3;
      i += 3;
    }
    else if (ch == '\"') {
      nsAutoString tmp; tmp.AssignWithConversion("&quot;");
      aString.Cut(i, 1);
      aString.Insert(tmp, i);
      len += 5;
      i += 5;
    }
  }
}

PRBool
nsFrame::ParentDisablesSelection() const
{
  return PR_FALSE;//depricating method perhaps.
/*
  PRBool selected;
  if (NS_FAILED(GetSelected(&selected)))
    return PR_FALSE;
  if (selected)
    return PR_FALSE; //if this frame is selected and no one has overridden the selection from "higher up"
                     //then no one below us will be disabled by this frame.
  nsIFrame* target;
  GetParent(&target);
  if (target)
    return ((nsFrame *)target)->ParentDisablesSelection();
  return PR_FALSE; //default this does not happen
  */
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsFrame::DumpRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent)
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
  DumpBaseRegressionData(aPresContext, out, aIndent);
  aIndent--;

  IndentBy(out, aIndent);
  fprintf(out, "</frame>\n");

  return NS_OK;
}

void
nsFrame::DumpBaseRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  if (nsnull != mNextSibling) {
    IndentBy(out, aIndent);
    fprintf(out, "<next-sibling va=\"%ld\"/>\n", PRUptrdiff(mNextSibling));
  }

  nsIView*  view;
  GetView(aPresContext, &view);
  if (view) {
    IndentBy(out, aIndent);
    fprintf(out, "<view va=\"%ld\">\n", PRUptrdiff(view));
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
    nsresult rv = FirstChild(aPresContext, list, &kid);
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
        nsIFrameDebug*  frameDebug;

        if (NS_SUCCEEDED(kid->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
          frameDebug->DumpRegressionData(aPresContext, out, aIndent);
        }
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
nsFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
#ifdef DEBUG
  *aResult = sizeof(*this);
#else
  *aResult = 0;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::VerifyTree() const
{
  NS_ASSERTION(0 == (mState & NS_FRAME_IN_REFLOW), "frame is in reflow");
  return NS_OK;
}
#endif

/*this method may.. invalidate if the state was changed or if aForceRedraw is PR_TRUE
  it will not update immediately.*/
NS_IMETHODIMP
nsFrame::SetSelected(nsIPresContext* aPresContext, nsIDOMRange *aRange,PRBool aSelected, nsSpread aSpread)
{
  if (aSelected && ParentDisablesSelection())
    return NS_OK;
	// check whether style allows selection
  if (!IsSelectable(this))
    return NS_OK;

/*  nsresult rv;

  if (eSpreadDown == aSpread){
    nsIFrame* kid;
    rv = FirstChild(nsnull, &kid);
    if (NS_SUCCEEDED(rv)) {
      while (nsnull != kid) {
        kid->SetSelected(nsnull,aSelected,aSpread);
        kid->GetNextSibling(&kid);
      }
    }
  }
*/
  nsFrameState  frameState;
  GetFrameState(&frameState);
  PRBool isSelected = ((frameState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT);
  if (aSelected == isSelected) //allready set thanks
  {
    return NS_OK;
  }
  if ( aSelected ){
    frameState |=  NS_FRAME_SELECTED_CONTENT;
  }
  else
    frameState &= ~NS_FRAME_SELECTED_CONTENT;
  SetFrameState(frameState);
  nsRect frameRect;
  GetRect(frameRect);
  nsRect rect(0, 0, frameRect.width, frameRect.height);
  Invalidate(aPresContext, rect, PR_FALSE);
#if 0
  if (aRange) {
    //lets see if the range contains us, if so we must redraw!
    nsCOMPtr<nsIDOMNode> endNode;
    nsCOMPtr<nsIDOMNode> startNode;
    aRange->GetEndParent(getter_AddRefs(endNode));
    aRange->GetStartParent(getter_AddRefs(startNode));
    nsCOMPtr<nsIContent> content;
    rv = GetContent(getter_AddRefs(content));
    nsCOMPtr<nsIDOMNode> thisNode;
    thisNode = do_QueryInterface(content);

//we must tell the siblings about the set selected call
//since the getprimaryframe call is done with this content node.
    if (thisNode != startNode && thisNode != endNode)
    { //whole node selected
      nsIFrame *frame;
      rv = GetNextSibling(&frame);
      while (NS_SUCCEEDED(rv) && frame)
      {
        frame->SetSelected(aRange,aSelected,eSpreadDown);
        rv = frame->GetNextSibling(&frame);
        if (NS_FAILED(rv))
          break;
      }
    }
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetSelected(PRBool *aSelected) const
{
  if (!aSelected )
    return NS_ERROR_NULL_POINTER;
  *aSelected = (PRBool)(mState & NS_FRAME_SELECTED_CONTENT);
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetPointFromOffset(nsIPresContext* inPresContext, nsIRenderingContext* inRendContext, PRInt32 inOffset, nsPoint* outPoint)
{
  NS_PRECONDITION(outPoint != nsnull, "Null parameter");
	nsPoint		bottomLeft(0, 0);
  if (mContent)
  {
    nsCOMPtr<nsIContent> newContent;
    PRInt32 newOffset;
    nsresult result = mContent->GetParent(*getter_AddRefs(newContent));
    if (newContent){
      result = newContent->IndexOf(mContent, newOffset);
      if (NS_FAILED(result)) 
      {
        return result;
      }
      nsRect rect;
      result = GetRect(rect);
      if (NS_FAILED(result))
      {
        return result;
      }
      if (inOffset > newOffset)
        bottomLeft.x = rect.width;
    }
  }
  *outPoint = bottomLeft;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetChildFrameContainingOffset(PRInt32 inContentOffset, PRBool inHint, PRInt32* outFrameContentOffset, nsIFrame **outChildFrame)
{
  NS_PRECONDITION(outChildFrame && outFrameContentOffset, "Null parameter");
  *outFrameContentOffset = (PRInt32)inHint;
  *outChildFrame = this;
  return NS_OK;
}

nsresult
nsFrame::GetNextPrevLineFromeBlockFrame(nsIPresContext* aPresContext,
                                        nsPeekOffsetStruct *aPos,
                                        nsIFrame *aBlockFrame, 
                                        PRInt32 aLineStart, 
                                        PRInt8 aOutSideLimit
                                        )
{
  //magic numbers aLineStart will be -1 for end of block 0 will be start of block
  if (!aBlockFrame || !aPos)
    return NS_ERROR_NULL_POINTER;

  aPos->mResultFrame = nsnull;
  aPos->mResultContent = nsnull;
  aPos->mPreferLeft = (aPos->mDirection == eDirNext);

   nsresult result;
  nsCOMPtr<nsILineIterator> it; 
  result = aBlockFrame->QueryInterface(NS_GET_IID(nsILineIterator),getter_AddRefs(it));
  if (NS_FAILED(result) || !it)
    return result;
  PRInt32 searchingLine = aLineStart;
  PRInt32 countLines;
  result = it->GetNumLines(&countLines);
  if (aOutSideLimit > 0) //start at end
    searchingLine = countLines;
  else if (aOutSideLimit <0)//start at begining
    searchingLine = -1;//"next" will be 0
  else 
  if ((aPos->mDirection == eDirPrevious && searchingLine == 0) || 
      (aPos->mDirection == eDirNext && searchingLine >= (countLines -1) )){
      //we need to jump to new block frame.
    return NS_ERROR_FAILURE;
  }
  PRInt32 lineFrameCount;
  nsIFrame *resultFrame = nsnull;
  nsIFrame *farStoppingFrame = nsnull; //we keep searching until we find a "this" frame then we go to next line
  nsIFrame *nearStoppingFrame = nsnull; //if we are backing up from edge, stop here
  nsIFrame *firstFrame;
  nsIFrame *lastFrame;
  nsRect  nonUsedRect;
  PRBool isBeforeFirstFrame, isAfterLastFrame;
  PRBool found = PR_FALSE;
  while (!found)
  {
    if (aPos->mDirection == eDirPrevious)
      searchingLine --;
    else
      searchingLine ++;
    if ((aPos->mDirection == eDirPrevious && searchingLine < 0) || 
       (aPos->mDirection == eDirNext && searchingLine >= countLines ))
    {
        //we need to jump to new block frame.
      return NS_ERROR_FAILURE;
    }
    PRUint32 lineFlags;
    result = it->GetLine(searchingLine, &firstFrame, &lineFrameCount,
                         nonUsedRect, &lineFlags);
    if (!lineFrameCount)
      continue;
    if (NS_SUCCEEDED(result)){
      lastFrame = firstFrame;
      for (;lineFrameCount > 1;lineFrameCount --){
        result = lastFrame->GetNextSibling(&lastFrame);
        if (NS_FAILED(result)){
          NS_ASSERTION(0,"should not be reached nsFrame\n");
          continue;
        }
      }
      GetLastLeaf(aPresContext, &lastFrame);

      if (aPos->mDirection == eDirNext){
        nearStoppingFrame = firstFrame;
        farStoppingFrame = lastFrame;
      }
      else{
        nearStoppingFrame = lastFrame;
        farStoppingFrame = firstFrame;
      }
      nsPoint offset;
      nsIView * view; //used for call of get offset from view
      aBlockFrame->GetOffsetFromView(aPresContext, offset,&view);
      nscoord newDesiredX  = aPos->mDesiredX - offset.x;//get desired x into blockframe coordinates!
      result = it->FindFrameAt(searchingLine, newDesiredX, &resultFrame, &isBeforeFirstFrame, &isAfterLastFrame);
    }

    if (NS_SUCCEEDED(result) && resultFrame)
    {
      nsCOMPtr<nsILineIterator> newIt; 
      //check to see if this is ANOTHER blockframe inside the other one if so then call into its lines
      result = resultFrame->QueryInterface(NS_GET_IID(nsILineIterator),getter_AddRefs(newIt));
      if (NS_SUCCEEDED(result) && newIt)
      {
        aPos->mResultFrame = resultFrame;
        return NS_OK;
      }
      //resultFrame is not a block frame

      nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;
      result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal), LEAF,
                                    aPresContext, resultFrame);
      if (NS_FAILED(result))
        return result;
      nsISupports *isupports = nsnull;
      nsIFrame *storeOldResultFrame = resultFrame;
      while ( !found ){
        nsCOMPtr<nsIPresContext> context;
        result = aPos->mTracker->GetPresContext(getter_AddRefs(context));
        nsPoint point;
        point.x = aPos->mDesiredX;
        point.y = 0;
        result = NS_ERROR_FAILURE;
        nsIView*  view;//if frame has a view. then stop. no doubleclicking into views
        if (NS_FAILED(resultFrame->GetView(aPresContext, &view)) || !view)
        {
          result = resultFrame->GetContentAndOffsetsFromPoint(context,point,
                                          getter_AddRefs(aPos->mResultContent),
                                          aPos->mContentOffset,
                                          aPos->mContentOffsetEnd,
                                          aPos->mPreferLeft);
        }
        if (NS_SUCCEEDED(result))
        {
          found = PR_TRUE;
        }
        else {
          if (aPos->mDirection == eDirPrevious && (resultFrame == farStoppingFrame))
            break;
          if (aPos->mDirection == eDirNext && (resultFrame == nearStoppingFrame))
            break;
          //always try previous on THAT line if that fails go the other way
          result = frameTraversal->Prev();
          if (NS_FAILED(result))
            break;
          result = frameTraversal->CurrentItem(&isupports);
          if (NS_FAILED(result) || !isupports)
            return result;
          //we must CAST here to an nsIFrame. nsIFrame doesnt really follow the rules
          resultFrame = (nsIFrame *)isupports;
        }
      }

      if (!found){
        resultFrame = storeOldResultFrame;
        result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal), LEAF,
                                      aPresContext, resultFrame);
      }
      while ( !found ){
        nsCOMPtr<nsIPresContext> context;
        result = aPos->mTracker->GetPresContext(getter_AddRefs(context));

        nsPoint point;
        point.x = aPos->mDesiredX;
        point.y = 0;

        result = resultFrame->GetContentAndOffsetsFromPoint(context,point,
                                          getter_AddRefs(aPos->mResultContent), aPos->mContentOffset,
                                          aPos->mContentOffsetEnd, aPos->mPreferLeft);
        if (!IsSelectable(resultFrame))
          return NS_ERROR_FAILURE;//cant go to unselectable frame
        if (NS_SUCCEEDED(result))
        {
          found = PR_TRUE;
          if (resultFrame == farStoppingFrame)
            aPos->mPreferLeft = PR_FALSE;
          else
            aPos->mPreferLeft = PR_TRUE;
        }
        else {
          if (aPos->mDirection == eDirPrevious && (resultFrame == nearStoppingFrame))
            break;
          if (aPos->mDirection == eDirNext && (resultFrame == farStoppingFrame))
            break;
          //previous didnt work now we try "next"
          result = frameTraversal->Next();
          if (NS_FAILED(result))
            break;
          result = frameTraversal->CurrentItem(&isupports);
          if (NS_FAILED(result) || !isupports)
            return result;
          //we must CAST here to an nsIFrame. nsIFrame doesnt really follow the rules
          resultFrame = (nsIFrame *)isupports;
        }
      }
      aPos->mResultFrame = resultFrame;
    }
    else {
        //we need to jump to new block frame.
      aPos->mAmount = eSelectLine;
      aPos->mStartOffset = 0;
      aPos->mEatingWS = PR_FALSE;
      aPos->mPreferLeft = !(aPos->mDirection == eDirNext);
      if (aPos->mDirection == eDirPrevious)
        aPos->mStartOffset = -1;//start from end
     return aBlockFrame->PeekOffset(aPresContext, aPos);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsFrame::PeekOffset(nsIPresContext* aPresContext, nsPeekOffsetStruct *aPos)
{
  if (!aPos || !aPos->mTracker )
    return NS_ERROR_NULL_POINTER;
  nsresult result = NS_ERROR_FAILURE; 
  PRInt32 endoffset;
  nsPoint point;
  point.x = aPos->mDesiredX;
  point.y = 0;
  switch (aPos->mAmount){
  case eSelectCharacter : case eSelectWord:
    {
      if (mContent)
      {
        nsCOMPtr<nsIContent> newContent;
        PRInt32 newOffset;
        result = mContent->GetParent(*getter_AddRefs(newContent));
        if (newContent){
          aPos->mResultContent = newContent;
          result = newContent->IndexOf(mContent, newOffset);
          if (aPos->mStartOffset < 0)//start at "end"
            aPos->mStartOffset = newOffset + 1;
          if (NS_FAILED(result)) 
          {
            return result;
          }
          if ((aPos->mDirection == eDirNext && newOffset < aPos->mStartOffset) || //need to go to next one
              (aPos->mDirection == eDirPrevious && newOffset >= aPos->mStartOffset))
          {
            result = GetFrameFromDirection(aPresContext, aPos);
            if (NS_FAILED(result) || !aPos->mResultFrame || !IsSelectable(aPos->mResultFrame))
            {
              return result?result:NS_ERROR_FAILURE;
            }
            return aPos->mResultFrame->PeekOffset(aPresContext, aPos);
          }
          else
          {
            if (aPos->mDirection == eDirNext)
              aPos->mContentOffset = newOffset +1;
            else
              aPos->mContentOffset = newOffset;//to beginning of frame
          }
        }
      }
      break;
    }//drop into no amount
    case eSelectNoAmount:
    {
      nsCOMPtr<nsIPresContext> context;
      result = aPos->mTracker->GetPresContext(getter_AddRefs(context));
      if (NS_FAILED(result) || !context)
        return result;
      result = GetContentAndOffsetsFromPoint(context,point,
                             getter_AddRefs(aPos->mResultContent),
                             aPos->mContentOffset,
                             endoffset,
                             aPos->mPreferLeft);
    }break;
    case eSelectLine :
    {
      nsCOMPtr<nsILineIterator> it; 
      nsIFrame *blockFrame = this;
      nsIFrame *thisBlock = this;
      PRInt32   thisLine;

      while (NS_FAILED(result)){
        thisBlock = blockFrame;
        result = blockFrame->GetParent(&blockFrame);
        if (NS_FAILED(result) || !blockFrame) //if at line 0 then nothing to do
          return result;
        result = blockFrame->QueryInterface(NS_GET_IID(nsILineIterator),getter_AddRefs(it));
        while (NS_FAILED(result) && blockFrame)
        {
          thisBlock = blockFrame;
          result = blockFrame->GetParent(&blockFrame);
          if (NS_SUCCEEDED(result) && blockFrame){
            result = blockFrame->QueryInterface(NS_GET_IID(nsILineIterator),getter_AddRefs(it));
          }
        }
        //this block is now one child down from blockframe
        if (NS_FAILED(result) || !it || !blockFrame || !thisBlock)
          return ((result) ? result : NS_ERROR_FAILURE);
        result = it->FindLineContaining(thisBlock, &thisLine);
        if (NS_FAILED(result) || thisLine <0)
          return result;
        int edgeCase = 0;//no edge case. this should look at thisLine
        PRBool doneLooping = PR_FALSE;//tells us when no more block frames hit.
        //this part will find a frame or a block frame. if its a block frame
        //it will "drill down" to find a viable frame or it will return an error.
        do {

          result = GetNextPrevLineFromeBlockFrame(aPresContext,
                                        aPos, 
                                        blockFrame, 
                                        thisLine, 
                                        edgeCase //start from thisLine
                                        );
          if (aPos->mResultFrame == this)//we came back to same spot! keep going
          {
            aPos->mResultFrame = nsnull;
            if (aPos->mDirection == eDirPrevious)
              thisLine--;
            else
              thisLine++;
          }
          else
            doneLooping = PR_TRUE; //do not continue with while loop
          if (NS_SUCCEEDED(result) && aPos->mResultFrame){
            result = aPos->mResultFrame->QueryInterface(NS_GET_IID(nsILineIterator),getter_AddRefs(it));
            if (NS_SUCCEEDED(result) && it)//we have struck another block element!
            {
              doneLooping = PR_FALSE;
              if (aPos->mDirection == eDirPrevious)
                edgeCase = 1;//far edge, search from end backwards
              else
                edgeCase = -1;//near edge search from beginning onwards
              thisLine=0;//this line means nothing now.
              //everything else means something so keep looking "inside" the block
              blockFrame = aPos->mResultFrame;

            }
            else
              result = NS_OK;//THIS is to mean that everything is ok to the containing while loop
          }
        }while(!doneLooping);

      }
      break;
    }
    case eSelectBeginLine:
    case eSelectEndLine:
    {
      nsCOMPtr<nsILineIterator> it; 
      nsIFrame *blockFrame = this;
      nsIFrame *thisBlock = this;
      PRInt32   thisLine;
      result = blockFrame->GetParent(&blockFrame);
      if (NS_FAILED(result) || !blockFrame) //if at line 0 then nothing to do
        return result;
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIterator),getter_AddRefs(it));
      while (NS_FAILED(result) && blockFrame)
      {
        thisBlock = blockFrame;
        result = blockFrame->GetParent(&blockFrame);
        if (NS_SUCCEEDED(result) && blockFrame){
          result = blockFrame->QueryInterface(NS_GET_IID(nsILineIterator),getter_AddRefs(it));
        }
      }
      //this block is now one child down from blockframe
      if (NS_FAILED(result) || !it || !blockFrame || !thisBlock)
        return result;
      result = it->FindLineContaining(thisBlock, &thisLine);
      if (NS_FAILED(result) || thisLine < 0 )
        return result;
      nsCOMPtr<nsIPresContext> context;
      result = aPos->mTracker->GetPresContext(getter_AddRefs(context));
      if (NS_FAILED(result) || !context)
        return result;
      PRInt32 lineFrameCount;
      nsIFrame *firstFrame;
      nsRect  usedRect; 
      PRUint32 lineFlags;
      result = it->GetLine(thisLine, &firstFrame, &lineFrameCount,usedRect,
                           &lineFlags);
      if (eSelectBeginLine == aPos->mAmount)
      {
        if (firstFrame)
        {
          nsPoint offsetPoint; //used for offset of result frame
          nsIView * view; //used for call of get offset from view
          firstFrame->GetOffsetFromView(aPresContext, offsetPoint, &view);

          offsetPoint.x = 0;//all the way to the left
          result = firstFrame->GetContentAndOffsetsFromPoint(context,
                                                             offsetPoint,
                                           getter_AddRefs(aPos->mResultContent),
                                           aPos->mContentOffset,
                                           endoffset,
                                           aPos->mPreferLeft);
        }
      }
      else
      {
        if (firstFrame)
        {
          PRBool found = PR_FALSE;
          while(!found)
          {
            nsIFrame *nextFrame = firstFrame;;
            for (PRInt32 i=1;i<lineFrameCount;i++)//allready have 1st frame
              nextFrame->GetNextSibling(&nextFrame);

            nsPoint offsetPoint; //used for offset of result frame
            nsIView * view; //used for call of get offset from view
            nextFrame->GetOffsetFromView(aPresContext, offsetPoint, &view);

            offsetPoint.x += 2* usedRect.width; //2* just to be sure we are off the edge

            result = nextFrame->GetContentAndOffsetsFromPoint(context,
                                            offsetPoint,
                                            getter_AddRefs(aPos->mResultContent),
                                            aPos->mContentOffset,
                                            endoffset,
                                            aPos->mPreferLeft);
            if (NS_SUCCEEDED(result))
              found = PR_TRUE;
            else
            {
              lineFrameCount--;
              if (lineFrameCount == 0)
                break;//just fail out
            }
          }
        }
      }
    }break;

    default: 
    {
      if (NS_SUCCEEDED(result))
        result = aPos->mResultFrame->PeekOffset(aPresContext, aPos);
    }
  }                          
  return result;
}

PRInt32
nsFrame::GetLineNumber(nsIFrame *aFrame)
{
  nsIFrame *blockFrame = aFrame;
  nsIFrame *thisBlock;
  PRInt32   thisLine;
  nsCOMPtr<nsILineIterator> it; 
  nsresult result = NS_ERROR_FAILURE;
  while (NS_FAILED(result) && blockFrame)
  {
    thisBlock = blockFrame;
    result = blockFrame->GetParent(&blockFrame);
    if (NS_SUCCEEDED(result) && blockFrame){
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIterator),getter_AddRefs(it));
    }
    else
      blockFrame = nsnull;
  }
  if (!blockFrame || !it)
    return NS_ERROR_FAILURE;
  result = it->FindLineContaining(thisBlock, &thisLine);
  if (NS_FAILED(result))
    return -1;
  return thisLine;
}


//this will use the nsFrameTraversal as the default peek method.
//this should change to use geometry and also look to ALL the child lists
//we need to set up line information to make sure we dont jump across line boundaries
NS_IMETHODIMP
nsFrame::GetFrameFromDirection(nsIPresContext* aPresContext, nsPeekOffsetStruct *aPos)
{
  nsIFrame *blockFrame = this;
  nsIFrame *thisBlock;
  PRInt32   thisLine;
  nsCOMPtr<nsILineIterator> it; 
  nsresult result = NS_ERROR_FAILURE;
  while (NS_FAILED(result) && blockFrame)
  {
    thisBlock = blockFrame;
    result = blockFrame->GetParent(&blockFrame);
    if (NS_SUCCEEDED(result) && blockFrame){
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIterator),getter_AddRefs(it));
    }
    else
      blockFrame = nsnull;
  }
  if (!blockFrame || !it)
    return NS_ERROR_FAILURE;
  result = it->FindLineContaining(thisBlock, &thisLine);
  if (NS_FAILED(result))
    return result;

  nsIFrame *firstFrame;
  nsIFrame *lastFrame;
  nsRect  nonUsedRect;
  PRInt32 lineFrameCount;
  PRUint32 lineFlags;

  result = it->GetLine(thisLine, &firstFrame, &lineFrameCount,nonUsedRect,
                       &lineFlags);
  if (NS_FAILED(result))
    return result;

  lastFrame = firstFrame;
  for (;lineFrameCount > 1;lineFrameCount --){
    result = lastFrame->GetNextSibling(&lastFrame);
    if (NS_FAILED(result)){
      NS_ASSERTION(0,"should not be reached nsFrame\n");
      return NS_ERROR_FAILURE;
    }
  }
 
  GetFirstLeaf(aPresContext, &firstFrame);
  GetLastLeaf(aPresContext, &lastFrame);
  //END LINE DATA CODE
  if ((aPos->mDirection == eDirNext && lastFrame == this)
    ||(aPos->mDirection == eDirPrevious && firstFrame == this))
  {
    if (aPos->mJumpLines != PR_TRUE)
      return NS_ERROR_FAILURE;//we are done. cannot jump lines
    if (aPos->mAmount != eSelectWord)
    {
      aPos->mPreferLeft = (PRBool)!(aPos->mPreferLeft);//drift to other side
      aPos->mAmount = eSelectNoAmount;
    }
    else{
      if (aPos->mEatingWS)//done finding what we wanted
        return NS_ERROR_FAILURE;
      if (aPos->mDirection == eDirNext)
      {
        aPos->mPreferLeft = (PRBool)!(aPos->mPreferLeft);//drift to other side
        aPos->mAmount = eSelectNoAmount;
      }
    }

  }
  if (aPos->mAmount == eSelectDir)
    aPos->mAmount = eSelectNoAmount;//just get to next frame.
  nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;
  result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),LEAF, aPresContext, this);
  if (NS_FAILED(result))
    return result;
  nsISupports *isupports = nsnull;
  if (aPos->mDirection == eDirNext)
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
  if (!IsSelectable(newFrame))
    return NS_ERROR_FAILURE;
  if (aPos->mDirection == eDirNext)
    aPos->mStartOffset = 0;
  else
    aPos->mStartOffset = -1;
  aPos->mResultFrame = newFrame;
  return NS_OK;
}

nsresult nsFrame::GetClosestViewForFrame(nsIPresContext* aPresContext,
                                         nsIFrame *aFrame,
                                         nsIView **aView)
{
  if (!aView)
    return NS_ERROR_NULL_POINTER;

  nsresult result = NS_OK;

  *aView = 0;

  nsIFrame *parent = aFrame;

  while (parent && !*aView)
  {
    result = parent->GetView(aPresContext, aView);

    if (NS_FAILED(result))
      return result;

    if (!*aView)
    {
      result = parent->GetParent(&parent);

      if (NS_FAILED(result))
        return result;
    }
  }

  return result;
}


NS_IMETHODIMP
nsFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
  NS_ASSERTION(0, "nsFrame::ReflowDirtyChild() should never be called.");  
  return NS_ERROR_NOT_IMPLEMENTED;    
}

//-----------------------------------------------------------------------------------


 /********************************************************
* Refreshes each content's frame
*********************************************************/
static void RefreshAllContentFrames(nsIPresContext* aPresContext, nsIFrame * aFrame, nsIContent * aContent)
{
  nsIContent* frameContent;
  aFrame->GetContent(&frameContent);
  if (frameContent == aContent) {
    ForceDrawFrame(aPresContext, (nsFrame *)aFrame);
  }
  NS_IF_RELEASE(frameContent);

  aFrame->FirstChild(aPresContext, nsnull, &aFrame);
  while (aFrame) {
    RefreshAllContentFrames(aPresContext, aFrame, aContent);
    aFrame->GetNextSibling(&aFrame);
  }
}

/********************************************************
* Refreshes each content's frame
*********************************************************/

/**
  *
 */
void ForceDrawFrame(nsIPresContext* aPresContext, nsFrame * aFrame)//, PRBool)
{
  if (aFrame == nsnull) {
    return;
  }
  nsRect    rect;
  nsIView * view;
  nsPoint   pnt;
  aFrame->GetOffsetFromView(aPresContext, pnt, &view);
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

void
nsFrame::GetLastLeaf(nsIPresContext* aPresContext, nsIFrame **aFrame)
{
  if (!aFrame || !*aFrame)
    return;
  nsIFrame *child = *aFrame;
  nsresult result;
  nsIFrame *lookahead = nsnull;
  while (1){
    result = child->FirstChild(aPresContext, nsnull, &lookahead);
    if (NS_FAILED(result) || !lookahead)
      return;//nothing to do
    child = lookahead;
    while (NS_SUCCEEDED(child->GetNextSibling(&lookahead)) && lookahead)
      child = lookahead;
    *aFrame = child;
  }
  *aFrame = child;
}

void
nsFrame::GetFirstLeaf(nsIPresContext* aPresContext, nsIFrame **aFrame)
{
  if (!aFrame || !*aFrame)
    return;
  nsIFrame *child = *aFrame;
  nsIFrame *lookahead;
  nsresult result;
  while (1){
    result = child->FirstChild(aPresContext, nsnull, &lookahead);
    if (NS_FAILED(result) || !lookahead)
      return;//nothing to do
    child = lookahead;
    *aFrame = child;
  }
}

nsresult nsFrame::CreateAndPostReflowCommand(nsIPresShell*                aPresShell,
                                             nsIFrame*                    aTargetFrame,
                                             nsIReflowCommand::ReflowType aReflowType,
                                             nsIFrame*                    aChildFrame,
                                             nsIAtom*                     aAttribute,
                                             nsIAtom*                     aListName)
{
  nsresult rv;

  if (!aPresShell || !aTargetFrame) {
    rv = NS_ERROR_NULL_POINTER;
  }
  else {
    nsCOMPtr<nsIReflowCommand> reflowCmd;
    rv = NS_NewHTMLReflowCommand(getter_AddRefs(reflowCmd), aTargetFrame,
                                 aReflowType, aChildFrame, 
                                 aAttribute);
    if (NS_SUCCEEDED(rv)) {
      if (nsnull != aListName) {
        reflowCmd->SetChildListName(aListName);
      }
      aPresShell->AppendReflowCommand(reflowCmd);    
    }
  } 

  return rv;
}


nsresult
nsFrame::CaptureMouse(nsIPresContext* aPresContext, PRBool aGrabMouseEvents)
{
    // get its view
  nsIView* view = nsnull;
  nsIFrame *parent;//might be THIS frame thats ok
  nsresult rv = GetParentWithView(aPresContext, &parent);
  if (!parent || NS_FAILED(rv))
    return rv?rv:NS_ERROR_FAILURE;
  parent->GetView(aPresContext,&view);

  nsCOMPtr<nsIViewManager> viewMan;
  PRBool result;

  if (view) {
    view->GetViewManager(*getter_AddRefs(viewMan));
    if (viewMan) {
      if (aGrabMouseEvents) {
        viewMan->GrabMouseEvents(view,result);
      } else {
        viewMan->GrabMouseEvents(nsnull,result);
      }
    }
  }

  return NS_OK;
}

PRBool
nsFrame::IsMouseCaptured(nsIPresContext* aPresContext)
{
    // get its view
  nsIView* view = nsnull;

  nsIFrame *parent;//might be THIS frame thats ok
  nsresult rv = GetParentWithView(aPresContext, &parent);
  if (!parent || NS_FAILED(rv))
    return rv?rv:NS_ERROR_FAILURE;
  parent->GetView(aPresContext,&view);

  nsCOMPtr<nsIViewManager> viewMan;
  
  if (view) {
    view->GetViewManager(*getter_AddRefs(viewMan));

    if (viewMan) {
        nsIView* grabbingView;
        viewMan->GetMouseEventGrabber(grabbingView);
        if (grabbingView == view)
          return PR_TRUE;
    }
  }

  return PR_FALSE;
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

void
nsFrame::VerifyDirtyBitSet(nsIFrame* aFrameList)
{
  for (nsIFrame*f = aFrameList; f; f->GetNextSibling(&f)) {
    nsFrameState  frameState;
    f->GetFrameState(&frameState);
    NS_ASSERTION(frameState & NS_FRAME_IS_DIRTY, "dirty bit not set");
  }
}


#endif
