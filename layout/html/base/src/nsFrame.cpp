/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
#include "nsIDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIPresShell.h"
#include "prlog.h"
#include "prprf.h"
#include <stdarg.h>
#include "nsIPtr.h"
#include "nsISizeOfHandler.h"
#include "nsIFrameManager.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#include "nsIAccessible.h"
#endif

#include "nsIDOMText.h"
#include "nsIDeviceContext.h"
#include "nsHTMLIIDs.h"
#include "nsIEventStateManager.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIFrameSelection.h"
#include "nsHTMLParts.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContentSink.h" 

#include "nsFrameTraversal.h"
#include "nsCOMPtr.h"
#include "nsStyleChangeList.h"
#include "nsIDOMRange.h"
#include "nsITableLayout.h"    //selection neccesity
#include "nsITableCellLayout.h"//  "
#include "nsIGfxTextControlFrame.h"
#include "nsINameSpaceManager.h"

// For triple-click pref
#include "nsIPref.h"
#include "nsIServiceManager.h"
static NS_DEFINE_CID(kPrefCID,     NS_PREF_CID);//for tripple click pref


// Some Misc #defines
#define SELECTION_DEBUG        0
#define FORCE_SELECTION_UPDATE 1
#define CALC_DEBUG             0


#include "nsICaret.h"
#include "nsILineIterator.h"
// [HACK] Foward Declarations
void ForceDrawFrame(nsIPresContext* aPresContext, nsFrame * aFrame);

//non Hack prototypes
#if 0
static void RefreshContentFrames(nsIPresContext* aPresContext, nsIContent * aStartContent, nsIContent * aEndContent);
#endif

#include "prenv.h"

// start nsIFrameDebug

#ifdef NS_DEBUG
static PRBool gShowFrameBorders = PR_FALSE;

void nsIFrameDebug::ShowFrameBorders(PRBool aEnable)
{
  gShowFrameBorders = aEnable;
}

PRBool nsIFrameDebug::GetShowFrameBorders()
{
  return gShowFrameBorders;
}

static PRBool gShowEventTargetFrameBorder = PR_FALSE;

void nsIFrameDebug::ShowEventTargetFrameBorder(PRBool aEnable)
{
  gShowEventTargetFrameBorder = aEnable;
}

PRBool nsIFrameDebug::GetShowEventTargetFrameBorder()
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

PRBool
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

void
nsIFrameDebug::SetVerifyTreeEnable(PRBool aEnabled)
{
  gFrameVerifyTreeEnable = aEnabled;
}

static PRLogModuleInfo* gStyleVerifyTreeLogModuleInfo;

static PRBool gStyleVerifyTreeEnable = PRBool(0x55);

PRBool
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

void
nsIFrameDebug::SetVerifyStyleTreeEnable(PRBool aEnabled)
{
  gStyleVerifyTreeEnable = aEnabled;
}

PRLogModuleInfo*
nsIFrameDebug::GetLogModuleInfo()
{
  if (nsnull == gLogModule) {
    gLogModule = PR_NewLogModule("frame");
  }
  return gLogModule;
}

void
nsIFrameDebug::RootFrameList(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  if((nsnull == aPresContext) || (nsnull == out))
    return;

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  if (nsnull != shell) {
    nsIFrame* frame;
    shell->GetRootFrame(&frame);
    if(nsnull != frame) {
      nsIFrameDebug* debugFrame;
      nsresult rv;
      rv = frame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&debugFrame);
      if(NS_SUCCEEDED(rv))
        debugFrame->List(aPresContext, out, aIndent);
    }
  }
}
#endif
// end nsIFrameDebug

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

MOZ_DECL_CTOR_COUNTER(nsFrame)

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
  NS_PRECONDITION(aInstancePtr, "null out param");

#ifdef DEBUG
  if (aIID.Equals(NS_GET_IID(nsIFrameDebug))) {
    *aInstancePtr = NS_STATIC_CAST(void*,NS_STATIC_CAST(nsIFrameDebug*,this));
    return NS_OK;
  }
#endif

  if (aIID.Equals(NS_GET_IID(nsIFrame)) ||
      aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(void*,NS_STATIC_CAST(nsIFrame*,this));
    return NS_OK;
  }

  *aInstancePtr = nsnull;
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
  nsFrameState  state;

  if (aPrevInFlow) {
    // Make sure the general flags bits are the same
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
    if (state & NS_FRAME_INDEPENDENT_SELECTION) {
      mState |= NS_FRAME_INDEPENDENT_SELECTION;
    }
  }
  if(mParent)
  {
    mParent->GetFrameState(&state);
    if (state & NS_FRAME_INDEPENDENT_SELECTION) {
      mState |= NS_FRAME_INDEPENDENT_SELECTION;
    }
    if (state & NS_FRAME_GENERATED_CONTENT){
      mState |= NS_FRAME_GENERATED_CONTENT;
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
  aPresContext->StopImagesFor(this);

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
nsFrame::GetOffsets(PRInt32 &aStart, PRInt32 &aEnd) const
{
  aStart = 0;
  aEnd = 0;
  return NS_OK;
}

// Subclass hook for style post processing
NS_IMETHODIMP nsFrame::DidSetStyleContext(nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP  nsFrame::CalcBorderPadding(nsMargin& aBorderPadding) const {
  NS_ASSERTION(mStyleContext!=nsnull,"null style context");
  if (mStyleContext) {
    nsStyleBorderPadding bpad;
    mStyleContext->GetBorderPaddingFor(bpad);
    bpad.GetBorderPadding(aBorderPadding);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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

PRInt16
nsFrame::DisplaySelection(nsIPresContext* aPresContext, PRBool isOkToTurnOn)
{
  PRInt16 selType = nsISelectionController::SELECTION_OFF;

  nsCOMPtr<nsISelectionController> selCon;
  nsresult result = GetSelectionController(aPresContext, getter_AddRefs(selCon));
  if (NS_SUCCEEDED(result) && selCon) {
    result = selCon->GetDisplaySelection(&selType);
    if (NS_SUCCEEDED(result) && (selType != nsISelectionController::SELECTION_OFF)) {
      // Check whether style allows selection.
      PRBool selectable;
      IsSelectable(&selectable, nsnull);
      if (!selectable) {
        selType = nsISelectionController::SELECTION_OFF;
        isOkToTurnOn = PR_FALSE;
      }
    }
    if (isOkToTurnOn && (selType == nsISelectionController::SELECTION_OFF)) {
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
      selType = nsISelectionController::SELECTION_ON;
    }
  }
  return selType;
}

void
nsFrame::SetOverflowClipRect(nsIRenderingContext& aRenderingContext)
{
  // 'overflow-clip' only applies to block-level elements and replaced
  // elements that have 'overflow' set to 'hidden', and it is relative
  // to the content area and applies to content only (not border or background)
  const nsStyleBorder* borderStyle;
  const nsStylePadding* paddingStyle;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)borderStyle);
  GetStyleData(eStyleStruct_Padding, (const nsStyleStruct*&)paddingStyle);
  
  // Start with the 'auto' values and then factor in user specified values
  nsRect  clipRect(0, 0, mRect.width, mRect.height);

  // XXX We don't support the 'overflow-clip' property yet, so just use the
  // content area (which is the default value) as the clip shape
  nsMargin  border, padding;

  borderStyle->GetBorder(border);
  clipRect.Deflate(border);
  // XXX We need to handle percentage padding
  if (paddingStyle->GetPadding(padding)) {
    clipRect.Deflate(padding);
  }

  // Set updated clip-rect into the rendering context
  PRBool clipState;
  aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
}

NS_IMETHODIMP
nsFrame::Paint(nsIPresContext*      aPresContext,
               nsIRenderingContext& aRenderingContext,
               const nsRect&        aDirtyRect,
               nsFramePaintLayer    aWhichLayer,
               PRUint32             aFlags)
{
  if (aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND)
    return NS_OK;
  
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

//check frame selection state
  PRBool isSelected;
  nsFrameState  frameState;
  GetFrameState(&frameState);
  isSelected = (frameState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;
//if not selected then return 
  if (!isSelected)
    return NS_OK; //nothing to do

//get the selection controller
  nsCOMPtr<nsISelectionController> selCon;
  result = GetSelectionController(aPresContext, getter_AddRefs(selCon));
  PRInt16 selectionValue;
  selCon->GetDisplaySelection(&selectionValue);
  displaySelection = selectionValue > nsISelectionController::SELECTION_HIDDEN;
//check display selection state.
  if (!displaySelection)
    return NS_OK; //if frame does not allow selection. do nothing


  nsCOMPtr<nsIContent> newContent;
  result = mContent->GetParent(*getter_AddRefs(newContent));

//check to see if we are anonymouse content
  PRInt32 offset;
  if (NS_SUCCEEDED(result) && newContent){
    result = newContent->IndexOf(mContent, offset);
    if (NS_FAILED(result)) 
      return result;
  }

  SelectionDetails *details;
  if (NS_SUCCEEDED(result) && shell)
  {
    nsCOMPtr<nsIFrameSelection> frameSelection;
    if (NS_SUCCEEDED(result) && selCon)
    {
      frameSelection = do_QueryInterface(selCon); //this MAY implement
    }
    if (!frameSelection)
      result = shell->GetFrameSelection(getter_AddRefs(frameSelection));
    if (NS_SUCCEEDED(result) && frameSelection){
      result = frameSelection->LookUpSelection(newContent, offset, 
                            1, &details, PR_FALSE);//look up to see what selection(s) are on this frame
    }
  }
  
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
    aRenderingContext.DrawRect(drawrect);
    SelectionDetails *deletingDetails = details;
    while ((deletingDetails = details->mNext) != nsnull) {
      delete details;
      details = deletingDetails;
    }
    delete details;
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
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
  switch (aEvent->message)
  {
  case NS_MOUSE_MOVE:
    {
      if (NS_SUCCEEDED(rv))
        rv = HandleDrag(aPresContext, aEvent, aEventStatus);
    }break;
  case NS_MOUSE_LEFT_BUTTON_DOWN:
    {
      if (NS_SUCCEEDED(rv))
        HandlePress(aPresContext, aEvent, aEventStatus);
    }break;
  case NS_MOUSE_LEFT_BUTTON_UP:
    {
      if (NS_SUCCEEDED(rv))
        HandleRelease(aPresContext, aEvent, aEventStatus);
    } break;
#ifdef ACCESSIBILITY
  case NS_GETACCESSIBLE:
    {
      // get the accessible
     // if(content) {
         //nsCOMPtr<nsIDOMNode> node = do_QueryInterface(content);
         nsresult rv = NS_OK;
         nsCOMPtr<nsIAccessibilityService> accService = 
                  do_GetService("@mozilla.org/accessibilityService;1", &rv);
         if (accService) {
           // get an accessible for the dom node
           nsISupports* f = (nsISupports*)(nsIFrame*)this;
           nsIAccessible* acc;
           accService->CreateRootAccessible(aPresContext,mParent ? f : nsnull,&acc);
           NS_STATIC_CAST(nsAccessibleEvent*, aEvent)->accessible = acc;
         }
      //}
    } break;
#endif
  default:
    break;
  }//end switch
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetDataForTableSelection(nsIFrameSelection *aFrameSelection, nsMouseEvent *aMouseEvent, 
                                  nsIContent **aParentContent, PRInt32 *aContentOffset, PRInt32 *aTarget)
{
  if (!aFrameSelection || !aMouseEvent || !aParentContent || !aContentOffset || !aTarget)
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

  if (!doTableSelection) 
  {
    // We allow table selection when just Shift is pressed
    //  only if already in table/cell selection mode
    if (aMouseEvent->isShift)
      aFrameSelection->GetTableCellSelection(&doTableSelection);

    if (!doTableSelection) 
      return NS_OK;
  }

  // Get the cell frame or table frame (or parent) of the current content node
  nsIFrame *frame = this;
  nsresult result = NS_OK;
  PRBool foundCell = PR_FALSE;
  PRBool foundTable = PR_FALSE;

  // Get the limiting node to stop parent frame search
  nsCOMPtr<nsIContent> limiter;
  aFrameSelection->GetLimiter(getter_AddRefs(limiter));

  //We don't initiate row/col selection from here now,
  //  but we may in future
  //PRBool selectColumn = PR_FALSE;
  //PRBool selectRow = PR_FALSE;

  while (frame && NS_SUCCEEDED(result))
  {
    // Check for a table cell by querying to a known CellFrame interface
    nsITableCellLayout *cellElement;
    result = (frame)->QueryInterface(NS_GET_IID(nsITableCellLayout), (void **)&cellElement);
    if (NS_SUCCEEDED(result) && cellElement)
    {
      foundCell = PR_TRUE;
      //TODO: If we want to use proximity to top or left border
      //      for row and column selection, this is the place to do it
      break;
    }
    else
    {
      // If not a cell, check for table
      // This will happen when starting frame is the table or child of a table,
      //  such as a row (we were inbetween cells or in table border)
      nsITableLayout *tableElement;
      result = (frame)->QueryInterface(NS_GET_IID(nsITableLayout), (void **)&tableElement);
      if (NS_SUCCEEDED(result) && tableElement)
      {
        foundTable = PR_TRUE;
        //TODO: How can we select row when along left table edge
        //  or select column when along top edge?
        break;
      } else {
        result = frame->GetParent(&frame);
        // Stop if we have hit the selection's limiting content node
        if (frame)
        {
          nsIContent* frameContent;
          frame->GetContent(&frameContent);
          if (frameContent == limiter.get())
            break;
        }
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

#if 0
  if (selectRow)
    *aTarget = nsISelectionPrivate::TABLESELECTION_ROW;
  else if (selectColumn)
    *aTarget = nsISelectionPrivate::TABLESELECTION_COLUMN;
  else 
#endif
  if (foundCell)
    *aTarget = nsISelectionPrivate::TABLESELECTION_CELL;
  else if (foundTable)
    *aTarget = nsISelectionPrivate::TABLESELECTION_TABLE;

  return NS_OK;
}

/*
NS_IMETHODIMP
nsFrame::FrameOrParentHasSpecialSelectionStyle(PRUint8 aSelectionStyle, nsIFrame* *foundFrame)
{
  nsIFrame* thisFrame = this;
  
  while (thisFrame)
  {
	  const nsStyleUserInterface* userinterface;
	  thisFrame->GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)userinterface);
  
    if (userinterface->mUserSelect == aSelectionStyle)
    {
      *foundFrame = thisFrame;
      return NS_OK;
    }
  
    thisFrame->GetParent(&thisFrame);
  }
  
  *foundFrame = nsnull;
  return NS_OK;
}
*/

NS_IMETHODIMP
nsFrame::IsSelectable(PRBool* aSelectable, PRUint8* aSelectStyle) const
{
  if (!aSelectable && !aSelectStyle)
    return NS_ERROR_NULL_POINTER;

  // Like 'visibility', we must check all the parents: if a parent
  // is not selectable, none of its children is selectable.
  //
  // The -moz-all value acts similarly: if a frame has 'user-select:-moz-all',
  // all its children are selectable, even those with 'user-select:none'.
  //
  // As a result, if 'none' and '-moz-all' are not present in the frame hierarchy,
  // aSelectStyle returns the first style that is not AUTO. If these values
  // are present in the frame hierarchy, aSelectStyle returns the style of the
  // topmost parent that has either 'none' or '-moz-all'.
  //
  // For instance, if the frame hierarchy is:
  //    AUTO     -> _MOZ_ALL -> NONE -> TEXT,     the returned value is _MOZ_ALL
  //    TEXT     -> NONE     -> AUTO -> _MOZ_ALL, the returned value is NONE
  //    _MOZ_ALL -> TEXT     -> AUTO -> AUTO,     the returned value is _MOZ_ALL
  //    AUTO     -> CELL     -> TEXT -> AUTO,     the returned value is TEXT
  //
  nsresult result      = NS_OK;
  PRUint8 selectStyle  = NS_STYLE_USER_SELECT_AUTO;
  nsIFrame* frame      = (nsIFrame*)this;

  while (frame && NS_SUCCEEDED(result)) {
    const nsStyleUIReset* userinterface;
    frame->GetStyleData(eStyleStruct_UIReset, (const nsStyleStruct*&)userinterface);
    if (userinterface) {
      switch (userinterface->mUserSelect) {
        case NS_STYLE_USER_SELECT_NONE:
        case NS_STYLE_USER_SELECT_MOZ_ALL:
          // override the previous values
          selectStyle = userinterface->mUserSelect;
          break;

        default:
          // otherwise return the first value which is not 'auto'
          if (selectStyle == NS_STYLE_USER_SELECT_AUTO) {
            selectStyle = userinterface->mUserSelect;
          }
          break;
      }
    }
    result = frame->GetParent(&frame);
  }

  // convert internal values to standard values
  if (selectStyle == NS_STYLE_USER_SELECT_AUTO)
    selectStyle = NS_STYLE_USER_SELECT_TEXT;
  else
  if (selectStyle == NS_STYLE_USER_SELECT_MOZ_ALL)
    selectStyle = NS_STYLE_USER_SELECT_ALL;

  // return stuff
  if (aSelectable)
    *aSelectable = (selectStyle != NS_STYLE_USER_SELECT_NONE);
  if (aSelectStyle)
    *aSelectStyle = selectStyle;
  if (mState & NS_FRAME_GENERATED_CONTENT)
    *aSelectable = PR_FALSE;
  return NS_OK;
}


/**
  * Handles the Mouse Press Event for the frame
 */
NS_IMETHODIMP
nsFrame::HandlePress(nsIPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  if (!shell)
    return NS_ERROR_FAILURE;

  // if we are in Navigator and the click is in a link, we don't want to start
  // selection because we don't want to interfere with a potential drag of said
  // link and steal all its glory.
  PRBool isEditor = PR_FALSE;
  shell->GetDisplayNonTextSelection ( &isEditor );
  nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
  if (!isEditor && !keyEvent->isAlt) {
    nsCOMPtr<nsIContent> content;
    GetContent (getter_AddRefs(content));
    while (content) {
       // are we an anchor with an href? If so, bail out now!
       nsCOMPtr<nsIAtom> tag;
       content->GetTag(*getter_AddRefs(tag));
       if ( tag.get() == nsHTMLAtoms::a ) {
         // Fix for bug #53326: Make sure we bail only
         // in the presence of an href with a value!
         nsAutoString href;
         if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::href, href))
         {// fix for bug #55921
           nsIView *dummyView = 0;
           nsRect frameRect = mRect;
           nsPoint offsetPoint;

           GetOffsetFromView(aPresContext, offsetPoint, &dummyView);

           frameRect.x = offsetPoint.x;
           frameRect.y = offsetPoint.y;

           if (frameRect.x <= aEvent->point.x && (frameRect.x + frameRect.width >= aEvent->point.x) &&
               frameRect.y <= aEvent->point.y && (frameRect.y + frameRect.height >= aEvent->point.y))
           return NS_OK;
         }
       }
  
       // now try the parent
       nsIContent* parent;
       content->GetParent(parent);
       content = dont_AddRef(parent);
  
    } // if browser, not editor
  }

  // check whether style allows selection
  // if not, don't tell selection the mouse event even occurred.  
  PRBool  selectable;
  PRUint8 selectStyle;
  rv = IsSelectable(&selectable, &selectStyle);
  if (NS_FAILED(rv)) return rv;
  
  // check for select: none
  if (!selectable)
    return NS_OK;

  // When implementing NS_STYLE_USER_SELECT_ELEMENT, NS_STYLE_USER_SELECT_ELEMENTS and
  // NS_STYLE_USER_SELECT_TOGGLE, need to change this logic
  PRBool useFrameSelection = (selectStyle == NS_STYLE_USER_SELECT_TEXT);

  if (!IsMouseCaptured(aPresContext))
    CaptureMouse(aPresContext, PR_TRUE);

  PRInt16 displayresult = nsISelectionController::SELECTION_OFF;
  nsCOMPtr<nsISelectionController> selCon;
  rv = GetSelectionController(aPresContext, getter_AddRefs(selCon));
  //get the selection controller
  if (NS_SUCCEEDED(rv) && selCon) 
  {
    selCon->GetDisplaySelection(&displayresult);
    if (displayresult == nsISelectionController::SELECTION_OFF)
      return NS_OK;//nothing to do we cannot affect selection from here
  }

  //get the frame selection from sel controller

  // nsFrameState  state;
  // GetFrameState(&state);
  // if (state & NS_FRAME_INDEPENDENT_SELECTION) 
  nsCOMPtr<nsIFrameSelection> frameselection;

  if (useFrameSelection)
    frameselection = do_QueryInterface(selCon); //this MAY implement

  if (!frameselection)//if we must get it from the pres shell's
    rv = shell->GetFrameSelection(getter_AddRefs(frameselection));

  if (!frameselection)
    return NS_ERROR_FAILURE;

  nsMouseEvent *me = (nsMouseEvent *)aEvent;

#ifdef XP_MAC
  if (me->isControl)
    return NS_OK;//short ciruit. hard coded for mac due to time restraints.
#endif
    
  if (me->clickCount >1 )
  {
    rv = frameselection->SetMouseDownState( PR_TRUE );
    return HandleMultiplePress(aPresContext,aEvent,aEventStatus);
  }

  nsCOMPtr<nsIContent> content;
  PRInt32 startOffset = 0, endOffset = 0;
  PRBool  beginFrameContent = PR_FALSE;

  rv = GetContentAndOffsetsFromPoint(aPresContext, aEvent->point, getter_AddRefs(content), startOffset, endOffset, beginFrameContent);
  // do we have CSS that changes selection behaviour?
  {
    PRBool    changeSelection;
    nsCOMPtr<nsIContent>  selectContent;
    PRInt32   newStart, newEnd;
    if (NS_SUCCEEDED(frameselection->AdjustOffsetsFromStyle(this, &changeSelection, getter_AddRefs(selectContent), &newStart, &newEnd))
      && changeSelection)
    {
      content = selectContent;
      startOffset = newStart;
      endOffset = newEnd;
    }
  }

  if (NS_FAILED(rv))
    return rv;

  // Let Ctrl/Cmd+mouse down do table selection instead of drag initiation
  nsCOMPtr<nsIContent>parentContent;
  PRInt32  contentOffset;
  PRInt32 target;
  rv = GetDataForTableSelection(frameselection, me, getter_AddRefs(parentContent), &contentOffset, &target);
  if (NS_SUCCEEDED(rv) && parentContent)
  {
    rv = frameselection->SetMouseDownState( PR_TRUE );
    if (NS_FAILED(rv)) return rv;
  
    return frameselection->HandleTableSelection(parentContent, contentOffset, target, me);
  }

  PRBool supportsDelay = PR_FALSE;

  frameselection->GetDelayCaretOverExistingSelection(&supportsDelay);
  frameselection->SetDelayedCaretData(0);

  if (supportsDelay)
  {
    // Check if any part of this frame is selected, and if the
    // user clicked inside the selected region. If so, we delay
    // starting a new selection since the user may be trying to
    // drag the selected region to some other app.

    SelectionDetails *details = 0;
    nsFrameState  frameState;
    GetFrameState(&frameState);
    PRBool isSelected = ((frameState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT);

    if (isSelected)
    {
      rv = frameselection->LookUpSelection(content, 0, endOffset, &details, PR_FALSE);

      if (NS_FAILED(rv))
        return rv;

      //
      // If there are any details, check to see if the user clicked
      // within any selected region of the frame.
      //

      if (details)
      {
        SelectionDetails *curDetail = details;

        while (curDetail)
        {
          //
          // If the user clicked inside a selection, then just
          // return without doing anything. We will handle placing
          // the caret later on when the mouse is released.
          //
          if (curDetail->mStart <= startOffset && endOffset <= curDetail->mEnd)
          {
            delete details;
            rv = frameselection->SetMouseDownState( PR_FALSE );

            if (NS_FAILED(rv))
              return rv;

            return frameselection->SetDelayedCaretData(me);
          }

          curDetail = curDetail->mNext;
        }

        delete details;
      }
    }
  }

  rv = frameselection->SetMouseDownState( PR_TRUE );

  if (NS_FAILED(rv))
    return rv;

  return frameselection->HandleClick(content, startOffset , endOffset, me->isShift, PR_FALSE, beginFrameContent);
}
 
/**
  * Multiple Mouse Press -- line or paragraph selection -- for the frame.
  * Wouldn't it be nice if this didn't have to be hardwired into Frame code?
 */
NS_IMETHODIMP
nsFrame::HandleMultiplePress(nsIPresContext* aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  nsresult rv;
  if (DisplaySelection(aPresContext) == nsISelectionController::SELECTION_OFF) {
    return NS_OK;
  }

  // Find out whether we're doing line or paragraph selection.
  // Currently, triple-click selects line unless the user sets
  // browser.triple_click_selects_paragraph; quadruple-click
  // selects paragraph, if any platform actually implements it.
  PRBool selectPara = PR_FALSE;
  nsMouseEvent *me = (nsMouseEvent *)aEvent;
  if (!me) return NS_OK;

  if (me->clickCount == 4)
    selectPara = PR_TRUE;
  else if (me->clickCount == 3)
  {
    nsCOMPtr<nsIPref> prefsService( do_GetService(kPrefCID, &rv) );
    if (NS_SUCCEEDED(rv) && prefsService)
      prefsService->GetBoolPref("browser.triple_click_selects_paragraph", &selectPara);
  }
  else
    return NS_OK;
#ifdef DEBUG_akkana
  if (selectPara) printf("Selecting Paragraph\n");
  else printf("Selecting Line\n");
#endif

  // Line or paragraph selection:
  PRInt32 startPos = 0;
  PRInt32 contentOffsetEnd = 0;
  nsCOMPtr<nsIContent> newContent;
  PRBool beginContent = PR_FALSE;
  rv = GetContentAndOffsetsFromPoint(aPresContext,
                                     aEvent->point,
                                     getter_AddRefs(newContent),
                                     startPos,
                                     contentOffsetEnd,
                                     beginContent);
  if (NS_FAILED(rv)) return rv;

  return PeekBackwardAndForward(selectPara ? eSelectParagraph
                                           : eSelectBeginLine,
                                selectPara ? eSelectParagraph
                                           : eSelectEndLine,
                                startPos, aPresContext, PR_TRUE);
}

NS_IMETHODIMP
nsFrame::PeekBackwardAndForward(nsSelectionAmount aAmountBack,
                                nsSelectionAmount aAmountForward,
                                PRInt32 aStartPos,
                                nsIPresContext* aPresContext,
                                PRBool aJumpLines)
{
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
  nsCOMPtr<nsISelectionController> selcon;
  if (NS_SUCCEEDED(rv))
    rv = GetSelectionController(aPresContext, getter_AddRefs(selcon));
  if (NS_FAILED(rv)) return rv;
  if (!shell || !selcon)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIFocusTracker> tracker;
  tracker = do_QueryInterface(shell, &rv);
  if (NS_FAILED(rv) || !tracker)
    return rv;

  // Use peek offset one way then the other:
  nsCOMPtr<nsIContent> startContent;
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIContent> endContent;
  nsCOMPtr<nsIDOMNode> endNode;
  nsPeekOffsetStruct startpos;
  startpos.SetData(tracker, 
                   0, 
                   aAmountBack,
                   eDirPrevious,
                   aStartPos,
                   PR_FALSE,
                   PR_TRUE,
                   aJumpLines);
  rv = PeekOffset(aPresContext, &startpos);
  if (NS_FAILED(rv))
    return rv;
  nsPeekOffsetStruct endpos;
  endpos.SetData(tracker, 
                 0, 
                 aAmountForward,
                 eDirNext,
                 aStartPos,
                 PR_FALSE,
                 PR_FALSE,
                 aJumpLines);
  rv = PeekOffset(aPresContext, &endpos);
  if (NS_FAILED(rv))
    return rv;

  endNode = do_QueryInterface(endpos.mResultContent, &rv);
  if (NS_FAILED(rv))
    return rv;
  startNode = do_QueryInterface(startpos.mResultContent, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISelection> selection;
  if (NS_SUCCEEDED(selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                        getter_AddRefs(selection)))){
    rv = selection->Collapse(startNode,startpos.mContentOffset);
    if (NS_FAILED(rv))
      return rv;
    rv = selection->Extend(endNode,endpos.mContentOffset);
    if (NS_FAILED(rv))
      return rv;
  }
  //no release 
  return NS_OK;
}

NS_IMETHODIMP nsFrame::HandleDrag(nsIPresContext* aPresContext, 
                                  nsGUIEvent*     aEvent,
                                  nsEventStatus*  aEventStatus)
{
  PRBool  selectable;
  IsSelectable(&selectable, nsnull);
  if (!selectable)
    return NS_OK;
  if (DisplaySelection(aPresContext) == nsISelectionController::SELECTION_OFF) {
    return NS_OK;
  }
  nsresult result;

  nsCOMPtr<nsIPresShell> presShell;

  result = aPresContext->GetShell(getter_AddRefs(presShell));

  if (NS_FAILED(result))
    return result;

  nsCOMPtr<nsIFrameSelection> frameselection;
  nsCOMPtr<nsISelectionController> selCon;
  result = GetSelectionController(aPresContext, getter_AddRefs(selCon));
  if (NS_SUCCEEDED(result) && selCon)
  {
    frameselection = do_QueryInterface(selCon); //this MAY implement
  }
  if (!frameselection)
    result = presShell->GetFrameSelection(getter_AddRefs(frameselection));

  if (NS_SUCCEEDED(result) && frameselection)
  {
    PRBool mouseDown = PR_FALSE;
    if (NS_SUCCEEDED(frameselection->GetMouseDownState(&mouseDown)) && !mouseDown)
      return NS_OK;            

    // check whether style allows selection
    // if not, don't tell selection the mouse event even occurred.  
    PRBool  selectable;
    PRUint8 selectStyle;
    result = IsSelectable(&selectable, &selectStyle);
    if (NS_FAILED(result)) 
      return result;

    // check for select: none
    if (selectable)
    {
      frameselection->StopAutoScrollTimer();
      // Check if we are dragging in a table cell
      nsCOMPtr<nsIContent> parentContent;
      PRInt32 contentOffset;
      PRInt32 target;
      nsMouseEvent *me = (nsMouseEvent *)aEvent;
      result = GetDataForTableSelection(frameselection, me, getter_AddRefs(parentContent), &contentOffset, &target);
      if (NS_SUCCEEDED(result) && parentContent)
        frameselection->HandleTableSelection(parentContent, contentOffset, target, me);
      else
        frameselection->HandleDrag(aPresContext, this, aEvent->point);
      frameselection->StartAutoScrollTimer(aPresContext, this, aEvent->point, 30);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsFrame::HandleRelease(nsIPresContext* aPresContext, 
                                     nsGUIEvent*     aEvent,
                                     nsEventStatus*  aEventStatus)
{
  if (IsMouseCaptured(aPresContext))
    CaptureMouse(aPresContext, PR_FALSE);

  if (DisplaySelection(aPresContext) == nsISelectionController::SELECTION_OFF)
    return NS_OK;

  nsresult result;

  nsCOMPtr<nsIPresShell> presShell;

  result = aPresContext->GetShell(getter_AddRefs(presShell));

  if (NS_FAILED(result))
    return result;

  if (!presShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIFrameSelection> frameselection;
  nsCOMPtr<nsISelectionController> selCon;
  
  result = GetSelectionController(aPresContext, getter_AddRefs(selCon));
    
  if (NS_SUCCEEDED(result) && selCon)
    frameselection = do_QueryInterface(selCon); //this MAY implement
  if (!frameselection)
    result = presShell->GetFrameSelection(getter_AddRefs(frameselection));

  if (NS_FAILED(result))
    return result;

  if (!frameselection)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault != *aEventStatus) {
    PRBool supportsDelay = PR_FALSE;

    frameselection->GetDelayCaretOverExistingSelection(&supportsDelay);

    if (supportsDelay)
    {
      // Check if the frameselection recorded the mouse going down.
      // If not, the user must have clicked in a part of the selection.
      // Place the caret before continuing!

      PRBool mouseDown = PR_FALSE;

      result = frameselection->GetMouseDownState(&mouseDown);

      if (NS_FAILED(result))
        return result;

      nsMouseEvent *me = 0;

      result = frameselection->GetDelayedCaretData(&me);

      if (NS_SUCCEEDED(result) && !mouseDown && me && me->clickCount < 2)
      {
        // We are doing this to simulate what we would have done on HandlePress
        result = frameselection->SetMouseDownState( PR_TRUE );

        nsCOMPtr<nsIContent> content;
        PRInt32 startOffset = 0, endOffset = 0;
        PRBool  beginFrameContent = PR_FALSE;

        result = GetContentAndOffsetsFromPoint(aPresContext, me->point, getter_AddRefs(content), startOffset, endOffset, beginFrameContent);
        if (NS_FAILED(result)) return result;

      // do we have CSS that changes selection behaviour?
      {
        PRBool    changeSelection;
        nsCOMPtr<nsIContent>  selectContent;
        PRInt32   newStart, newEnd;
        if (NS_SUCCEEDED(frameselection->AdjustOffsetsFromStyle(this, &changeSelection, getter_AddRefs(selectContent), &newStart, &newEnd))
          && changeSelection)
        {
          content = selectContent;
          startOffset = newStart;
          endOffset = newEnd;
        }
      }

        result = frameselection->HandleClick(content, startOffset , endOffset, me->isShift, PR_FALSE, beginFrameContent);
        if (NS_FAILED(result)) return result;
      }
      else
      {
        me = (nsMouseEvent *)aEvent;
        nsCOMPtr<nsIContent>parentContent;
        PRInt32  contentOffset;
        PRInt32 target;
        result = GetDataForTableSelection(frameselection, me, getter_AddRefs(parentContent), &contentOffset, &target);

        if (NS_SUCCEEDED(result) && parentContent)
        {
          frameselection->SetMouseDownState( PR_FALSE );
          result = frameselection->HandleTableSelection(parentContent, contentOffset, target, me);
          if (NS_FAILED(result)) return result;
        }
      }
      result = frameselection->SetDelayedCaretData(0);
    }
  }

  // Now handle the normal HandleRelase business.

  if (NS_SUCCEEDED(result) && frameselection) {
    frameselection->SetMouseDownState( PR_FALSE );
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
#if 0
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
#endif //XXX we USED to skip anonymous content i dont think we should anymore leaving this here as a flah

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

      nscoord fromTop = aPoint.y - rect.y;
      nscoord fromBottom = aPoint.y - rect.y - rect.height;

      PRInt32 yDistance;
      if (fromTop > 0 && fromBottom < 0)
        yDistance = 0;
      else
        yDistance = PR_MIN(abs(fromTop), abs(fromBottom));

      if (yDistance <= closestYDistance && rect.width > 0 && rect.height > 0)
      {
        if (yDistance < closestYDistance)
          closestXDistance = HUGE_DISTANCE;

        nscoord fromLeft = aPoint.x - rect.x;
        nscoord fromRight = aPoint.x - rect.x - rect.width;

        PRInt32 xDistance;
        if (fromLeft > 0 && fromRight < 0)
          xDistance = 0;
        else
          xDistance = PR_MIN(abs(fromLeft), abs(fromRight));

        if (xDistance == 0 && yDistance == 0)
        {
          closestFrame = kid;
          break;
        }

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
    
    PRInt32 contentOffset(aContentOffset); //temp to hold old value in case of failure
    
    result = (*aNewContent)->IndexOf(mContent, contentOffset);
    if (NS_FAILED(result) || contentOffset < 0) 
    {
      return (result?result:NS_ERROR_FAILURE);
    }
    aContentOffset = contentOffset; //its clear save the result

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
  const nsStyleUserInterface* styleUserInterface;
  GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)styleUserInterface);
  aCursor = styleUserInterface->mCursor;
  if (NS_STYLE_CURSOR_AUTO == aCursor) {
    aCursor = NS_STYLE_CURSOR_DEFAULT;
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
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)((nsIStyleContext*)mStyleContext)->GetStyleData(eStyleStruct_Visibility);
    if (vis->IsVisible()) {
      *aFrame = this;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

// Resize and incremental reflow

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
nsFrame::CanContinueTextRun(PRBool& aContinueTextRun) const
{
  // By default, a frame will *not* allow a text run to be continued
  // through it.
  aContinueTextRun = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::Reflow(nsIPresContext*          aPresContext,
                nsHTMLReflowMetrics&     aDesiredSize,
                const nsHTMLReflowState& aReflowState,
                nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFrame", aReflowState.reason);
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
                          PRInt32         aModType, 
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
        if (NS_SUCCEEDED(frameManager->GetFrameProperty((nsIFrame*)this, nsLayoutAtoms::viewProperty, 0, &value))) {
          *aView = (nsIView*)value;
          NS_ASSERTION(value != 0, "frame state bit was set but frame has no view");
        }
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

     
    // Let all of the containing frames know they have a child 
    // with a view until a frame is hit that has the 
    // NS_FRAME_HAS_CHILD_WITH_VIEW bit set already or a we
    // reached the top of the frame tree.
    nsIFrame* parent = nsnull;
    GetParent(&parent);
    
    while (parent) {
      nsFrameState frameState;
      parent->GetFrameState(&frameState);

      if (frameState & NS_FRAME_HAS_CHILD_WITH_VIEW)
        break;

      frameState |= NS_FRAME_HAS_CHILD_WITH_VIEW;
      parent->SetFrameState(frameState);
      parent->GetParent(&parent);
    }
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
#ifdef DEBUG
  if (aDamageRect.IsEmpty()) {
    NS_WARNING("empty damage rect: update caller to avoid fcn call overhead");
    return;
  }
#endif

  if (aPresContext) {
    // Don't allow invalidates to do anything when
    // painting is suppressed.
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    if (shell) {
      PRBool suppressed = PR_FALSE;
      shell->IsPaintingSuppressed(&suppressed);
      if (suppressed)
        return;
    }
  }

  nsIViewManager* viewManager = nsnull;
  nsRect damageRect(aDamageRect);

#if 0
  // NOTE: inflating the damagerect is to account for outlines but 
  // ONLY WHEN outlines are actually drawn outside of the frame. This
  // assumes that they are *but they are not* and it also assumes that the
  // entire frame is being invalidated, which it often is not
  // - therefore, this code is invalid and has been removed

  // Checks to see if the damaged rect should be infalted 
  // to include the outline
  const nsStyleOutline* outline;
  GetStyleData(eStyleStruct_Outline, (const nsStyleStruct*&)outline);
  nscoord width;
  outline->GetOutlineWidth(width);
  if (width > 0) {
    damageRect.Inflate(width, width);
  }
#endif

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

// Define the MAX_FRAME_DEPTH to be the ContentSink's MAX_REFLOW_DEPTH plus
// 4 for the frames above the document's frames: 
//  the Viewport, GFXScroll, ScrollPort, and Canvas
#define MAX_FRAME_DEPTH (MAX_REFLOW_DEPTH+4)

PRBool
nsFrame::IsFrameTreeTooDeep(const nsHTMLReflowState& aReflowState,
                            nsHTMLReflowMetrics& aMetrics)
{
  if (aReflowState.mReflowDepth >  MAX_FRAME_DEPTH) {
    mState |= NS_FRAME_IS_UNFLOWABLE;
    mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
    aMetrics.width = 0;
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.descent = 0;
    aMetrics.mCarriedOutBottomMargin.Zero();
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
  const nsStyleDisplay* display;
    GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
    
  if (display->mPosition != NS_STYLE_POSITION_NORMAL) {
    aBase = PR_TRUE;
  }
  else {
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

#ifdef DEBUG_waterson

/**
 * List a single frame to stdout. Meant to be called from gdb.
 */
void
DebugListFrame(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  ((nsFrame*) aFrame)->List(aPresContext, stdout, 0);
  printf("\n");
}

/**
 * List a frame tree to stdout. Meant to be called from gdb.
 */
void
DebugListFrameTree(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  nsIFrameDebug* fdbg;
  aFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**) &fdbg);
  if (fdbg)
    fdbg->List(aPresContext, stdout, 0);
}

#endif


// Debugging
NS_IMETHODIMP
nsFrame::List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", NS_STATIC_CAST(void*, mParent));
#endif
  nsIView*  view;
  GetView(aPresContext, &view);
  if (view) {
    fprintf(out, " [view=%p]", NS_STATIC_CAST(void*, view));
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  fprintf(out, " [content=%p]", NS_STATIC_CAST(void*, mContent));
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
/*
  // should never be called now
  nsIFrame* parent;
  GetParent(&parent);
  if (parent) {
	  PRBool selectable;
	  parent->IsSelectable(selectable);
    return (selectable ? PR_FALSE : PR_TRUE);
  }
  return PR_FALSE;
*/
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
  
  return PR_FALSE;
}

nsresult 
nsFrame::GetSelectionForVisCheck(nsIPresContext * aPresContext, nsISelection** aSelection)
{
  *aSelection = nsnull;

  // start by checking to see if we are paginated which probably means
  // we are in print preview or printing
  PRBool isPaginated;
  nsresult rv = aPresContext->IsPaginated(&isPaginated);
  if (NS_SUCCEEDED(rv) && isPaginated) {
    // now see if we are rendering selection only
    PRBool isRendingSelection;
    rv = aPresContext->IsRenderingOnlySelection(&isRendingSelection);
    if (NS_SUCCEEDED(rv) && isRendingSelection) {
      // Check the quick way first (typically only leaf nodes)
      PRBool isSelected = (mState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;
      // if we aren't selected in the mState,
      // we could be a container so check to see if we are in the selection range
      // this is a expensive
      if (!isSelected) {
        nsCOMPtr<nsIPresShell> shell;
        rv = aPresContext->GetShell(getter_AddRefs(shell));
        if (NS_SUCCEEDED(rv) && shell) {
          nsCOMPtr<nsISelectionController> selcon(do_QueryInterface(shell));
          if (selcon) {
            rv = selcon->GetSelection(nsISelectionController::SELECTION_NORMAL, aSelection);
          }
        }
      }
    }
  } 

  return rv;
}


NS_IMETHODIMP
nsFrame::IsVisibleForPainting(nsIPresContext *     aPresContext, 
                              nsIRenderingContext& aRenderingContext,
                              PRBool               aCheckVis,
                              PRBool*              aIsVisible)
{
  // first check to see if we are visible
  if (aCheckVis) {
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)((nsIStyleContext*)mStyleContext)->GetStyleData(eStyleStruct_Visibility);
    if (!vis->IsVisible()) {
      *aIsVisible = PR_FALSE;
      return NS_OK;
    }
  }

  // Start by assuming we are visible and need to be painted
  *aIsVisible = PR_TRUE;

  // NOTE: GetSelectionforVisCheck checks the pagination to make sure we are printing
  // In otherwords, the selection will ALWAYS be null if we are not printing, meaning
  // the visibility will be TRUE in that case
  nsCOMPtr<nsISelection> selection;
  nsresult rv = GetSelectionForVisCheck(aPresContext, getter_AddRefs(selection));
  if (NS_SUCCEEDED(rv) && selection) {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
    selection->ContainsNode(node, PR_TRUE, aIsVisible);
  }

  return rv;
}

NS_IMETHODIMP
nsFrame::IsEmpty(PRBool aIsQuirkMode, PRBool aIsPre, PRBool *aResult)
{
  *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetSelectionController(nsIPresContext *aPresContext, nsISelectionController **aSelCon)
{
  if (!aPresContext || !aSelCon)
    return NS_ERROR_INVALID_ARG;
  nsFrameState  state;
  GetFrameState(&state);
  if (state & NS_FRAME_INDEPENDENT_SELECTION) 
  {
    nsIFrame *tmp = this;
    while (tmp)
    {
      nsIGfxTextControlFrame2 *tcf;
      if (NS_SUCCEEDED(tmp->QueryInterface(NS_GET_IID(nsIGfxTextControlFrame2),(void**)&tcf)))
      {
        return tcf->GetSelectionContr(aSelCon);
      }
      if (NS_FAILED(tmp->GetParent(&tmp)))
        break;
    }
  }
  nsCOMPtr<nsIPresShell> shell;
  if (NS_SUCCEEDED(aPresContext->GetShell(getter_AddRefs(shell))) && shell)
  {
    nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(shell);
    NS_IF_ADDREF(*aSelCon = selCon);
    return NS_OK;
  }
  return NS_OK;
}



#ifdef NS_DEBUG
NS_IMETHODIMP
nsFrame::DumpRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent, PRBool aIncludeStyleData)
{
  IndentBy(out, aIndent);
  fprintf(out, "<frame va=\"%ld\" type=\"", PRUptrdiff(this));
  nsAutoString name;
  GetFrameName(name);
  XMLQuote(name);
  fputs(NS_LossyConvertUCS2toASCII(name).get(), out);
  fprintf(out, "\" state=\"%d\" parent=\"%ld\">\n",
          mState, PRUptrdiff(mParent));

  aIndent++;
  DumpBaseRegressionData(aPresContext, out, aIndent, aIncludeStyleData);
  aIndent--;

  IndentBy(out, aIndent);
  fprintf(out, "</frame>\n");

  return NS_OK;
}

void
nsFrame::DumpBaseRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent, PRBool aIncludeStyleData)
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

  if(aIncludeStyleData) {
    if(mStyleContext) {
      IndentBy(out, aIndent);
      fprintf(out, "<stylecontext va=\"%ld\">\n", PRUptrdiff(mStyleContext));
      aIndent++;
      // Dump style context regression data
      mStyleContext->DumpRegressionData(aPresContext, out, aIndent);
      aIndent--;
      IndentBy(out, aIndent);
      fprintf(out, "</stylecontext>\n");
    }
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
        fputs(NS_LossyConvertUCS2toASCII(listName).get(), out);
        fprintf(out, "\">\n");
      }
      else {
        fprintf(out, "<child-list>\n");
      }
      aIndent++;
      while (nsnull != kid) {
        nsIFrameDebug*  frameDebug;

        if (NS_SUCCEEDED(kid->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
          frameDebug->DumpRegressionData(aPresContext, out, aIndent, aIncludeStyleData);
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
  *aResult = sizeof(*this);
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
nsFrame::SetSelected(nsIPresContext* aPresContext, nsIDOMRange *aRange, PRBool aSelected, nsSpread aSpread)
{
/*
  if (aSelected && ParentDisablesSelection())
    return NS_OK;
*/

  // check whether style allows selection
  PRBool  selectable;
  IsSelectable(&selectable, nsnull);
  if (!selectable)
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
  if (!rect.IsEmpty()) {
    Invalidate(aPresContext, rect, PR_FALSE);
  }
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
#ifdef IBMBIDI
  PRInt32 start, end;
  nsIFrame* frame;
  GetNextSibling(&frame);
  if (frame){
    GetFirstLeaf(aPresContext, &frame);
    GetOffsets(start, end);
    if (start && end) {
      frame->SetSelected(aPresContext, aRange, aSelected, aSpread);
    }
  }
#endif // IBMBIDI
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
  nsPoint bottomLeft(0, 0);
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

//
// What I've pieced together about this routine:
// Starting with a block frame (from which a line frame can be gotten)
// and a line number, drill down and get the first/last selectable
// frame on that line, depending on aPos->mDirection.
// aOutSideLimit != 0 means ignore aLineStart, instead work from
// the end (if > 0) or beginning (if < 0).
//
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
  nsCOMPtr<nsILineIteratorNavigator> it; 
  result = aBlockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
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
        //result = lastFrame->GetNextSibling(&lastFrame, searchingLine);
        result = it->GetNextSiblingOnLine(lastFrame, searchingLine);
        if (NS_FAILED(result) || !lastFrame){
          NS_ASSERTION(0,"should not be reached nsFrame\n");
          return NS_ERROR_FAILURE;
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
#ifdef IBMBIDI
      PRBool bidiEnabled;
      aPresContext->GetBidiEnabled(&bidiEnabled);
      result = it->FindFrameAt(searchingLine, newDesiredX, bidiEnabled, &resultFrame, &isBeforeFirstFrame, &isAfterLastFrame);
#else
      result = it->FindFrameAt(searchingLine, newDesiredX, &resultFrame, &isBeforeFirstFrame, &isAfterLastFrame);
#endif // IBMBIDI
      if(NS_FAILED(result))
        continue;
    }

    if (NS_SUCCEEDED(result) && resultFrame)
    {
      nsCOMPtr<nsILineIteratorNavigator> newIt; 
      //check to see if this is ANOTHER blockframe inside the other one if so then call into its lines
      result = resultFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(newIt));
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
        if (NS_FAILED(result))
          return result;
        nsPoint point;
        point.x = aPos->mDesiredX;

        nsRect tempRect;
        nsRect& tempRectRef = tempRect;
        resultFrame->GetRect(tempRectRef);
        nsPoint offset;
        nsIView * view; //used for call of get offset from view
        result = resultFrame->GetOffsetFromView(aPresContext, offset,&view);
        if (NS_FAILED(result))
          return result;
        point.y = tempRect.height + offset.y;

        //special check. if we allow non-text selection then we can allow a hit location to fall before a table. 
        //otherwise there is no way to get and click signal to fall before a table (it being a line iterator itself)
        PRBool isEditor = PR_FALSE;
        nsCOMPtr<nsIPresShell> shell;
        aPresContext->GetShell(getter_AddRefs(shell));
        if (!shell)
          return NS_ERROR_FAILURE;
        shell->GetDisplayNonTextSelection ( &isEditor );
        if ( isEditor ) 
        {
          nsIAtom *resultFrameType;
          if(NS_SUCCEEDED(resultFrame->GetFrameType(&resultFrameType)) && resultFrameType)
          {
            if (resultFrameType ==  nsLayoutAtoms::tableOuterFrame)
            {
              if (((point.x - offset.x + tempRect.x)<0) ||  ((point.x - offset.x+ tempRect.x)>tempRect.width))//off left/right side
              {
                nsCOMPtr<nsIContent> content;
                resultFrame->GetContent(getter_AddRefs(content));
                nsCOMPtr<nsIContent> parent;
                if (content)
                {
                  content->GetParent(*getter_AddRefs(parent));
                  if (parent)
                  {
                    aPos->mResultContent = parent;
                    parent->IndexOf(content, aPos->mContentOffset);
                    aPos->mPreferLeft = PR_FALSE;
                    if ((point.x - offset.x+ tempRect.x)>tempRect.width)
                    {
                      aPos->mContentOffset++;//go to end of this frame
                      aPos->mPreferLeft = PR_TRUE;
                    }
                    aPos->mContentOffsetEnd = aPos->mContentOffset;
                    //result frame is the result frames parent.
                    resultFrame->GetParent(&aPos->mResultFrame);
                    return NS_COMFALSE;
                  }
                }
              }
            }
          }
        }

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
			  PRBool selectable;
        resultFrame->IsSelectable(&selectable, nsnull);
			  if (!selectable)
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

// Get a frame which can contain a line iterator
// (which generally means it's a block frame).
static nsILineIterator*
GetBlockFrameAndLineIter(nsIFrame* aFrame, nsIFrame** aBlockFrame)
{
  nsILineIterator* it;
  nsIFrame *blockFrame = aFrame;
  nsIFrame *thisBlock = aFrame;

  nsresult result = blockFrame->GetParent(&blockFrame);
  if (NS_FAILED(result) || !blockFrame) //if at line 0 then nothing to do
    return 0;
  result = blockFrame->QueryInterface(NS_GET_IID(nsILineIterator), (void**)&it);
  if (NS_SUCCEEDED(result) && it)
  {
    if (aBlockFrame)
      *aBlockFrame = blockFrame;
    return it;
  }

  while (blockFrame)
  {
    thisBlock = blockFrame;
    result = blockFrame->GetParent(&blockFrame);
    if (NS_SUCCEEDED(result) && blockFrame){
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIterator),
                                          (void**)&it);
      if (NS_SUCCEEDED(result) && it)
      {
        if (aBlockFrame)
          *aBlockFrame = blockFrame;
        return it;
      }
    }
  }
  return 0;
}

nsresult
nsFrame::PeekOffsetParagraph(nsIPresContext* aPresContext,
                             nsPeekOffsetStruct *aPos)
{
#ifdef DEBUG_paragraph
  printf("Selecting paragraph\n");
#endif
  nsIFrame* blockFrame;
  nsCOMPtr<nsILineIterator> iter (getter_AddRefs(GetBlockFrameAndLineIter(this, &blockFrame)));
  if (!blockFrame || !iter)
    return NS_ERROR_UNEXPECTED;

  PRInt32 thisLine;
  nsresult result = iter->FindLineContaining(this, &thisLine);
#ifdef DEBUG_paragraph
  printf("Looping %s from line %d\n",
         aPos->mDirection == eDirPrevious ? "back" : "forward",
         thisLine);
#endif
  if (NS_FAILED(result) || thisLine < 0)
    return result ? result : NS_ERROR_UNEXPECTED;

  // Now, theoretically, we should be able to loop over lines
  // looking for lines that end in breaks.
  PRInt32 di = (aPos->mDirection == eDirPrevious ? -1 : 1);
  for (PRInt32 i = thisLine; ; i += di)
  {
    nsIFrame* firstFrameOnLine;
    PRInt32 numFramesOnLine;
    nsRect lineBounds;
    PRUint32 lineFlags;
    if (i >= 0)
    {
      result = iter->GetLine(i, &firstFrameOnLine, &numFramesOnLine,
                             lineBounds, &lineFlags);
      if (NS_FAILED(result) || !firstFrameOnLine || !numFramesOnLine)
      {
#ifdef DEBUG_paragraph
        printf("End loop at line %d\n", i);
#endif
        break;
      }
    }
    if (lineFlags & NS_LINE_FLAG_ENDS_IN_BREAK || i < 0)
    {
      // Fill in aPos with the info on the new position
#ifdef DEBUG_paragraph
      printf("Found a paragraph break at line %d\n", i);
#endif

      // Save the old direction, but now go one line back the other way
      nsDirection oldDirection = aPos->mDirection;
      if (oldDirection == eDirPrevious)
        aPos->mDirection = eDirNext;
      else
        aPos->mDirection = eDirPrevious;
#ifdef SIMPLE /* nope */
      result = GetNextPrevLineFromeBlockFrame(aPresContext,
                                              aPos,
                                              blockFrame,
                                              i,
                                              0);
      if (NS_FAILED(result))
        printf("GetNextPrevLineFromeBlockFrame failed\n");

#else /* SIMPLE -- alas, nope */
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
        if (NS_SUCCEEDED(result) && aPos->mResultFrame)
        {
          result = aPos->mResultFrame->QueryInterface(NS_GET_IID(nsILineIterator),
                                                      getter_AddRefs(iter));
          if (NS_SUCCEEDED(result) && iter)//we've struck another block element!
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
      } while (!doneLooping);

#endif /* SIMPLE -- alas, nope */

      // Restore old direction before returning:
      aPos->mDirection = oldDirection;
      return result;
    }
  }

  return NS_OK;
}

// Line and paragraph selection (and probably several other cases)
// can get a containing frame from a line iterator, but then need
// to "drill down" to get the content and offset corresponding to
// the last child subframe.  Hence:
// Alas, this doesn't entirely work; it's blocked by some style changes.
static nsresult
DrillDownToEndOfLine(nsIFrame* aFrame, PRInt32 aLineNo, PRInt32 aLineFrameCount,
                     nsRect& aUsedRect,
                     nsIPresContext* aPresContext, nsPeekOffsetStruct* aPos)
{
  if (!aFrame)
    return NS_ERROR_UNEXPECTED;
  nsresult rv = NS_ERROR_FAILURE;
  PRBool found = PR_FALSE;
  while (!found)  // I have no idea why this loop exists.  Mike?
  {
    nsIFrame *nextFrame = aFrame;
    nsIFrame *currentFrame = aFrame;
    PRInt32 i;
    for (i=1; i<aLineFrameCount && nextFrame; i++) //already have 1st frame
    {
		  currentFrame = nextFrame;
      // If we do GetNextSibling, we don't go far enough
      // (is aLineFrameCount too small?)
      // If we do GetNextInFlow, we hit a null.
      currentFrame->GetNextSibling(&nextFrame);
    }
    if (!nextFrame) //premature leaving of loop.
    {
      nextFrame = currentFrame; //back it up. lets show a warning
      NS_WARNING("lineFrame Count lied to us from nsILineIterator!\n");
    }
	
    nsPoint offsetPoint; //used for offset of result frame
    nsIView * view; //used for call of get offset from view
    nextFrame->GetOffsetFromView(aPresContext, offsetPoint, &view);

    offsetPoint.x += 2* aUsedRect.width; //2* just to be sure we are off the edge
    // This doesn't seem very efficient since GetPosition
    // has to do a binary search.

    nsCOMPtr<nsIPresContext> context;
    rv = aPos->mTracker->GetPresContext(getter_AddRefs(context));
    if (NS_FAILED(rv)) return rv;
    PRInt32 endoffset;
    rv = nextFrame->GetContentAndOffsetsFromPoint(context,
                                                  offsetPoint,
                                                  getter_AddRefs(aPos->mResultContent),
                                                  aPos->mContentOffset,
                                                  endoffset,
                                                  aPos->mPreferLeft);
    if (NS_SUCCEEDED(rv))
      return PR_TRUE;

#ifdef DEBUG_paragraph
    NS_ASSERTION(PR_FALSE, "Looping around in PeekOffset\n");
#endif
    aLineFrameCount--;
    if (aLineFrameCount == 0)
      break;//just fail out
  }
  return rv;
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
            if (NS_FAILED(result))
              return result;
					  PRBool selectable = PR_FALSE;
					  if (aPos->mResultFrame)
  					  aPos->mResultFrame->IsSelectable(&selectable, nsnull);
            if (NS_FAILED(result) || !aPos->mResultFrame || !selectable)
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
      nsCOMPtr<nsILineIteratorNavigator> iter; 
      nsIFrame *blockFrame = this;
      nsIFrame *thisBlock = this;
      PRInt32   thisLine;

      while (NS_FAILED(result)){
        thisBlock = blockFrame;
        result = blockFrame->GetParent(&blockFrame);
        if (NS_FAILED(result) || !blockFrame) //if at line 0 then nothing to do
          return result;
        result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(iter));
        while (NS_FAILED(result) && blockFrame)
        {
          thisBlock = blockFrame;
          result = blockFrame->GetParent(&blockFrame);
          if (NS_SUCCEEDED(result) && blockFrame){
            result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(iter));
          }
        }
        //this block is now one child down from blockframe
        if (NS_FAILED(result) || !iter || !blockFrame || !thisBlock)
        {
          return ((result) ? result : NS_ERROR_FAILURE);
        }
        result = iter->FindLineContaining(thisBlock, &thisLine);

        if (NS_FAILED(result))
           return result;

        if (thisLine < 0) 
          return  NS_ERROR_FAILURE;

        int edgeCase = 0;//no edge case. this should look at thisLine
        PRBool doneLooping = PR_FALSE;//tells us when no more block frames hit.
        //this part will find a frame or a block frame. if its a block frame
        //it will "drill down" to find a viable frame or it will return an error.
        nsIFrame *lastFrame = this;
        do {
          result = GetNextPrevLineFromeBlockFrame(aPresContext,
                                                  aPos, 
                                                  blockFrame, 
                                                  thisLine, 
                                                  edgeCase //start from thisLine
            );
          if (NS_SUCCEEDED(result) && (!aPos->mResultFrame || aPos->mResultFrame == lastFrame))//we came back to same spot! keep going
          {
            aPos->mResultFrame = nsnull;
            if (aPos->mDirection == eDirPrevious)
              thisLine--;
            else
              thisLine++;
          }
          else //if failure or success with different frame.
            doneLooping = PR_TRUE; //do not continue with while loop

          lastFrame = aPos->mResultFrame; //set last frame 

          if (NS_SUCCEEDED(result) && aPos->mResultFrame 
            && blockFrame != aPos->mResultFrame)// make sure block element is not the same as the one we had before
          {
/* SPECIAL CHECK FOR TABLE NAVIGATION
  tables need to navigate also and the frame that supports it is nsTableRowGroupFrame which is INSIDE
  nsTableOuterFrame.  if we have stumbled onto an nsTableOuter we need to drill into nsTableRowGroup
  if we hit a header or footer thats ok just go into them,
*/
            PRBool searchTableBool = PR_FALSE;
            nsIAtom *resultFrameType;
            if(NS_SUCCEEDED(aPos->mResultFrame->GetFrameType(&resultFrameType)) && resultFrameType == nsLayoutAtoms::tableOuterFrame)
            {
              nsIFrame *frame = aPos->mResultFrame;
              result = frame->FirstChild(aPresContext, nsnull,&frame);
              //got the table frame now
              while(frame) //ok time to drill down to find iterator
              {
                result = frame->FirstChild(aPresContext, nsnull,&frame);
                if (NS_SUCCEEDED(result))
                {
                  result = frame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),
                                                            getter_AddRefs(iter));
                  if (NS_SUCCEEDED(result))
                  {
                    aPos->mResultFrame = frame;
                    searchTableBool = PR_TRUE;
                    break; //while aPos->mResultFrame
                  }
                }
                else
                  break;
              }
            }
            if (!searchTableBool)
              result = aPos->mResultFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),
                                                        getter_AddRefs(iter));
            if (NS_SUCCEEDED(result) && iter)//we've struck another block element!
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
            {
              result = NS_OK;//THIS is to mean that everything is ok to the containing while loop
              break;
            }
          }
        } while (!doneLooping);
      }
      break;
    }

    case eSelectParagraph:
      return PeekOffsetParagraph(aPresContext, aPos);

    case eSelectBeginLine:
    case eSelectEndLine:
    {
      nsCOMPtr<nsILineIteratorNavigator> it; 
      nsIFrame *blockFrame = this;
      nsIFrame *thisBlock = this;
      result = blockFrame->GetParent(&blockFrame);
      if (NS_FAILED(result) || !blockFrame) //if at line 0 then nothing to do
        return result;
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
      while (NS_FAILED(result) && blockFrame)
      {
        thisBlock = blockFrame;
        result = blockFrame->GetParent(&blockFrame);
        if (NS_SUCCEEDED(result) && blockFrame){
          result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
        }
      }
      if (NS_FAILED(result) || !it || !blockFrame || !thisBlock)
        return result;
      //this block is now one child down from blockframe

      PRInt32   thisLine;
      result = it->FindLineContaining(thisBlock, &thisLine);
      if (NS_FAILED(result) || thisLine < 0 )
        return result;

      PRInt32 lineFrameCount;
      nsIFrame *firstFrame;
      nsRect  usedRect; 
      PRUint32 lineFlags;
      result = it->GetLine(thisLine, &firstFrame, &lineFrameCount,usedRect,
                           &lineFlags);

      if (eSelectBeginLine == aPos->mAmount)
      {
        nsCOMPtr<nsIPresContext> context;
        result = aPos->mTracker->GetPresContext(getter_AddRefs(context));
        if (NS_FAILED(result) || !context)
          return result;

        while (firstFrame)
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
          if (NS_SUCCEEDED(result))
            break;
          result = it->GetNextSiblingOnLine(firstFrame,thisLine);
          if (NS_FAILED(result))
            break;
        }
      }
      else  // eSelectEndLine
      {
        // We have the last frame, but we need to drill down
        // to get the last offset in the last content represented
        // by that frame.
        return DrillDownToEndOfLine(firstFrame, thisLine, lineFrameCount,
                                    usedRect, aPresContext, aPos);
      }
      return result;
    }
    break;

    default: 
    {
      if (NS_SUCCEEDED(result))
        result = aPos->mResultFrame->PeekOffset(aPresContext, aPos);
    }
  }                          
  return result;
}


NS_IMETHODIMP
nsFrame::CheckVisibility(nsIPresContext* , PRInt32 , PRInt32 , PRBool , PRBool *, PRBool *)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


PRInt32
nsFrame::GetLineNumber(nsIFrame *aFrame)
{
  nsIFrame *blockFrame = aFrame;
  nsIFrame *thisBlock;
  PRInt32   thisLine;
  nsCOMPtr<nsILineIteratorNavigator> it; 
  nsresult result = NS_ERROR_FAILURE;
  while (NS_FAILED(result) && blockFrame)
  {
    thisBlock = blockFrame;
    result = blockFrame->GetParent(&blockFrame);
    if (NS_SUCCEEDED(result) && blockFrame){
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
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
  nsCOMPtr<nsILineIteratorNavigator> it; 
  nsresult result = NS_ERROR_FAILURE;
  while (NS_FAILED(result) && blockFrame)
  {
    thisBlock = blockFrame;
    result = blockFrame->GetParent(&blockFrame);
    if (NS_SUCCEEDED(result) && blockFrame){
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
    }
    else
      blockFrame = nsnull;
  }
  if (!blockFrame || !it)
    return NS_ERROR_FAILURE;
  result = it->FindLineContaining(thisBlock, &thisLine);
  if (NS_FAILED(result))
    return result;

  nsIFrame *traversedFrame = this;

  nsIFrame *firstFrame;
  nsIFrame *lastFrame;
  nsRect  nonUsedRect;
  PRInt32 lineFrameCount;
  PRUint32 lineFlags;

#ifdef IBMBIDI
  /* Check whether the visual and logical order of the frames are different */
  PRBool lineIsReordered = PR_FALSE;
  nsIFrame *firstVisual;
  nsIFrame *lastVisual;
  PRBool lineIsRTL;
  PRBool lineJump = PR_FALSE;

  it->GetDirection(&lineIsRTL);
  result = it->CheckLineOrder(thisLine, &lineIsReordered, &firstVisual, &lastVisual);
  if (NS_FAILED(result))
    return result;

  if (lineIsReordered) {
    firstFrame = firstVisual;
    lastFrame = lastVisual;
  }
  else
#endif
  {
    //check to see if "this" is a blockframe if so we need to advance the linenum immediately
    //only if its dir is "next"
    if (aPos->mDirection == eDirNext)
    {
      nsCOMPtr<nsILineIteratorNavigator> tempLi; 
      if (NS_SUCCEEDED(QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(tempLi))))
      {
        thisLine++;
        result = it->GetLine(thisLine, &firstFrame, &lineFrameCount,nonUsedRect,
                           &lineFlags);
        if (NS_SUCCEEDED(result) && firstFrame)
          traversedFrame = firstFrame;
        else
        {
          return NS_ERROR_FAILURE;
        }
      }
    }

    result = it->GetLine(thisLine, &firstFrame, &lineFrameCount,nonUsedRect,
                         &lineFlags);
    if (NS_FAILED(result))
      return result;

    lastFrame = firstFrame;
    for (;lineFrameCount > 1;lineFrameCount --){
      result = it->GetNextSiblingOnLine(lastFrame, thisLine);
      if (NS_FAILED(result) || !lastFrame){
        NS_ASSERTION(0,"should not be reached nsFrame\n");
        return NS_ERROR_FAILURE;
      }
    }
  }

  GetFirstLeaf(aPresContext, &firstFrame);
  GetLastLeaf(aPresContext, &lastFrame);
  //END LINE DATA CODE
  if 
#ifndef IBMBIDI  // jumping lines in RTL paragraphs
      ((aPos->mDirection == eDirNext && lastFrame == this)
       ||(aPos->mDirection == eDirPrevious && firstFrame == this))
#else
      (((lineIsRTL) &&
        ((aPos->mDirection == eDirNext && firstFrame == this)
         ||(aPos->mDirection == eDirPrevious && lastFrame == this)))
       ||
       ((!lineIsRTL) &&
        ((aPos->mDirection == eDirNext && lastFrame == this)
         ||(aPos->mDirection == eDirPrevious && firstFrame == this))))
#endif
  {
    if (aPos->mJumpLines != PR_TRUE)
      return NS_ERROR_FAILURE;//we are done. cannot jump lines
#ifdef IBMBIDI
    lineJump = PR_TRUE;
#endif
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
#ifdef IBMBIDI
  result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                (lineIsReordered) ? VISUAL : LEAF,
                                aPresContext, 
                                (lineJump && lineIsRTL)
                                ? (aPos->mDirection == eDirNext) ? lastFrame : firstFrame
                                : traversedFrame);
#else
  //if we are a container frame we MUST init with last leaf for eDirNext
  //
  result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),LEAF, aPresContext, traversedFrame);
#endif
  if (NS_FAILED(result))
    return result;
  nsISupports *isupports = nsnull;
  nsRect testRect;
#ifdef IBMBIDI
  nsIFrame *newFrame;
  PRBool selectable =  PR_TRUE; //usually fine

  while (testRect.IsEmpty() || !selectable) {
    if (lineIsRTL && lineJump) 
      if (aPos->mDirection == eDirPrevious)
        result = frameTraversal->Next();
      else 
        result = frameTraversal->Prev();
    else
#endif
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
#ifndef IBMBIDI
  nsIFrame *newFrame = (nsIFrame *)isupports;
#else
  
  newFrame = (nsIFrame *)isupports;
  newFrame->GetRect(testRect);
  if (testRect.IsEmpty()) { // this must be a non-renderable frame creatd at the end of the line by Bidi reordering
    lineJump = PR_TRUE;
    aPos->mAmount = eSelectNoAmount;
  }
  PRBool newLineIsRTL = PR_FALSE;
  if (lineJump) {
    blockFrame = newFrame;
    nsresult result = NS_ERROR_FAILURE;
    while (NS_FAILED(result) && blockFrame)
    {
      thisBlock = blockFrame;
      result = blockFrame->GetParent(&blockFrame);
      if (NS_SUCCEEDED(result) && blockFrame){
        result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
      }
      else
        blockFrame = nsnull;
    }
    if (!blockFrame || !it)
      return NS_ERROR_FAILURE;
    result = it->FindLineContaining(thisBlock, &thisLine);
    if (NS_FAILED(result))
      return result;
    it->GetDirection(&newLineIsRTL);

    result = it->CheckLineOrder(thisLine, &lineIsReordered, &firstVisual, &lastVisual);
    if (NS_FAILED(result))
      return result;

    if (lineIsReordered)
    {
      firstFrame = firstVisual;
      lastFrame = lastVisual;
    }
    else
    {
      result = it->GetLine(thisLine, &firstFrame, &lineFrameCount,nonUsedRect,
                           &lineFlags);
      if (NS_FAILED(result))
        return result;

      lastFrame = firstFrame;
      for (;lineFrameCount > 1;lineFrameCount --){
        result = it->GetNextSiblingOnLine(lastFrame, thisLine);
        if (NS_FAILED(result) || !lastFrame){
          NS_ASSERTION(0,"should not be reached nsFrame\n");
          return NS_ERROR_FAILURE;
        }
      }
    }

    GetFirstLeaf(aPresContext, &firstFrame);
    GetLastLeaf(aPresContext, &lastFrame);

    if (newLineIsRTL) {
      if (aPos->mDirection == eDirPrevious)
        newFrame = firstFrame;
      else
        newFrame = lastFrame;
    }
    else {
      if (aPos->mDirection == eDirNext)
        newFrame = firstFrame;
      else
        newFrame = lastFrame;
      }
    }
    newFrame->IsSelectable(&selectable, nsnull);
    if (!selectable)
      lineJump = PR_FALSE;
  }
#endif // IBMBIDI
  if (aPos->mDirection == eDirNext)
    aPos->mStartOffset = 0;
  else
    aPos->mStartOffset = -1;
#ifdef IBMBIDI
  PRUint8 oldLevel, newLevel, baseLevel;
  GetBidiProperty(aPresContext, nsLayoutAtoms::embeddingLevel, (void**)&oldLevel,sizeof(oldLevel));
  newFrame->GetBidiProperty(aPresContext, nsLayoutAtoms::embeddingLevel, (void**)&newLevel,sizeof(newLevel));
  newFrame->GetBidiProperty(aPresContext, nsLayoutAtoms::baseLevel, (void**)&baseLevel,sizeof(baseLevel));
  if (newLevel & 1) // The new frame is RTL, go to the other end
    aPos->mStartOffset = -1 - aPos->mStartOffset;

  if ((aPos->mAmount == eSelectNoAmount) && ((newLevel & 1) != (oldLevel & 1)))  {
    aPos->mPreferLeft = !(aPos->mPreferLeft);
  }
#endif
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


#ifdef ACCESSIBILITY
NS_IMETHODIMP
nsFrame::GetAccessible(nsIAccessible** aAccessible)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

NS_IMETHODIMP
nsFrame::ReflowCommandNotify(nsIPresShell*     aShell,
                             nsIReflowCommand* aRC,
                             PRBool            aCommandAdded)

{
  return NS_OK;
}

NS_IMETHODIMP 
nsFrame::GetParentStyleContextProvider(nsIPresContext* aPresContext,
                                       nsIFrame** aProviderFrame, 
                                       nsContextProviderRelationship& aRelationship)
{
  NS_ASSERTION(aPresContext && aProviderFrame, "null arguments: aPresContext and-or aProviderFrame");
  if (aProviderFrame) {
    // parent context provider is the parent frame by default
    *aProviderFrame = mParent;
    aRelationship = eContextProvider_Ancestor;
  }
  return ((aProviderFrame != nsnull) && (*aProviderFrame != nsnull)) ? NS_OK : NS_ERROR_FAILURE;
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
  //if we are a block frame then go for the last line of 'this'
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


NS_IMETHODIMP
nsFrame::CaptureMouse(nsIPresContext* aPresContext, PRBool aGrabMouseEvents)
{
    // get its view
  nsIView* view = nsnull;
  nsIFrame *parent;//might be THIS frame thats ok
  GetView(aPresContext, & view);
  if (!view)
  {
    nsresult rv = GetParentWithView(aPresContext, &parent);
    if (NS_FAILED(rv))
      return rv;
    if (!parent)
      return NS_ERROR_FAILURE;
    parent->GetView(aPresContext,&view);
  }

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
  GetView(aPresContext, & view);
  if (!view)
  {
    nsIFrame *parent;//might be THIS frame thats ok
    nsresult rv = GetParentWithView(aPresContext, &parent);
    if (NS_FAILED(rv))
      return rv;
    if (!parent)
      return NS_ERROR_FAILURE;

    parent->GetView(aPresContext,&view);
  }
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

void
nsFrame::SetDefaultBackgroundColor(nsIPresContext* aPresContext)
{
  const nsStyleBackground* bg = 
    (const nsStyleBackground*)mStyleContext->GetStyleData(eStyleStruct_Background);

  if ((bg->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT) != 0) {
    NS_WARNING("Frame setting default background color but it has transparent background!");
  } else {
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));

    if (shell) {
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));

      if (vm) {
        vm->SetDefaultBackgroundColor(bg->mBackgroundColor);
      }
    }
  }
}

#ifdef IBMBIDI
/**
 *  retrieve Bidi property of this frame
 *  @lina 5/1/2000
 */

NS_IMETHODIMP nsFrame::GetBidiProperty(nsIPresContext* aPresContext,
                                  nsIAtom*        aPropertyName,
                                  void**          aPropertyValue,
                                  size_t          aSize) const
{
  if (!aPropertyValue || !aPropertyName) {
    return NS_ERROR_NULL_POINTER;
  }
  if ( (aSize < 1) || (aSize > sizeof(void*) ) ) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsCRT::zero(aPropertyValue, aSize);
  void* val = nsnull;

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell) );

  if (presShell) {
    nsCOMPtr<nsIFrameManager> frameManager;
    presShell->GetFrameManager(getter_AddRefs(frameManager) );

    if (frameManager) {
      frameManager->GetFrameProperty( (nsIFrame*)this, aPropertyName, 0, &val);
      if (val) {
        nsCRT::memcpy(aPropertyValue, &val, aSize);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetBidiProperty(nsIPresContext* aPresContext,
                                  nsIAtom*        aPropertyName,
                                  void*           aPropertyValue) 
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell) );
  if (shell) {
    nsCOMPtr<nsIFrameManager> frameManager;
    shell->GetFrameManager(getter_AddRefs(frameManager) );
    if (frameManager) {
      rv = frameManager->SetFrameProperty( (nsIFrame*) this, aPropertyName, aPropertyValue, nsnull);
    }
  }
  return rv;
}
#endif // IBMBIDI

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


// Start Display Reflow
#ifdef DEBUG

DR_cookie::DR_cookie(nsIFrame*                aFrame, 
                     const nsHTMLReflowState& aReflowState,
                     nsHTMLReflowMetrics&     aMetrics,
                     nsReflowStatus&          aStatus)
  :mFrame(aFrame), mReflowState(aReflowState), mMetrics(aMetrics), mStatus(aStatus)
{
  mValue = nsFrame::DisplayReflowEnter(mFrame, mReflowState);
}

DR_cookie::~DR_cookie()
{
  nsFrame::DisplayReflowExit(mFrame, mMetrics, mStatus, mValue);
}

struct DR_FrameTypeInfo;
struct DR_FrameTreeNode;
struct DR_Rule;

struct DR_State
{
  DR_State();
  ~DR_State();
  void Init();
  void AddFrameTypeInfo(nsIAtom* aFrameType,
                        char*    aFrameNameAbbrev,
                        char*    aFrameName);
  DR_FrameTypeInfo* GetFrameTypeInfo(nsIAtom* aFrameType);
  DR_FrameTypeInfo* GetFrameTypeInfo(char* aFrameName);
  void InitFrameTypeTable();
  DR_FrameTreeNode* CreateTreeNode(nsIFrame*                aFrame,
                                   const nsHTMLReflowState& aReflowState);
  void FindMatchingRule(DR_FrameTreeNode& aNode);
  PRBool RuleMatches(DR_Rule&          aRule,
                     DR_FrameTreeNode& aNode);
  PRBool GetToken(FILE* aFile,
                  char* aBuf);
  DR_Rule* ParseRule(FILE* aFile);
  void ParseRulesFile();
  void AddRule(nsVoidArray& aRules,
               DR_Rule&     aRule);
  PRBool IsWhiteSpace(int c);
  PRBool GetNumber(char*    aBuf, 
                 PRInt32&  aNumber);
  void PrettyUC(nscoord aSize,
                char*   aBuf);
  void DisplayFrameTypeInfo(nsIFrame* aFrame,
                            PRInt32   aIndent);
  void DeleteTreeNode(DR_FrameTreeNode& aNode);

  PRBool      mInited;
  PRBool      mActive;
  PRInt32     mCount;
  nsVoidArray mWildRules;
  PRInt32     mAssert;
  PRInt32     mIndentStart;
  PRBool      mIndentUndisplayedFrames;
  nsVoidArray mFrameTypeTable;

  // reflow specific state
  nsVoidArray mFrameTreeLeaves;
};

static DR_State DR_state; // the one and only DR_State

struct DR_RulePart 
{
  DR_RulePart(nsIAtom* aFrameType) : mFrameType(aFrameType), mNext(0) {}
  void Destroy();

  nsIAtom*     mFrameType;
  DR_RulePart* mNext;
};

void DR_RulePart::Destroy()
{
  if (mNext) {
    mNext->Destroy();
  }
  delete this;
}

struct DR_Rule 
{
  DR_Rule() : mLength(0), mTarget(nsnull), mDisplay(PR_FALSE) {}
  ~DR_Rule() { if (mTarget) mTarget->Destroy(); }
  void AddPart(nsIAtom* aFrameType);

  PRUint32      mLength;
  DR_RulePart*  mTarget;
  PRBool        mDisplay;
};

void DR_Rule::AddPart(nsIAtom* aFrameType)
{
  DR_RulePart* newPart = new DR_RulePart(aFrameType);
  newPart->mNext = mTarget;
  mTarget = newPart;
  mLength++;
}

struct DR_FrameTypeInfo
{
  DR_FrameTypeInfo(nsIAtom* aFrmeType, char* aFrameNameAbbrev, char* aFrameName);
  ~DR_FrameTypeInfo() { 
      PRInt32 numElements;
      numElements = mRules.Count();
      for (PRInt32 i = numElements - 1; i >= 0; i--) {
        delete (DR_Rule *)mRules.ElementAt(i);
      }
   }

  nsIAtom*    mType;
  char        mNameAbbrev[16];
  char        mName[32];
  nsVoidArray mRules;
};

DR_FrameTypeInfo::DR_FrameTypeInfo(nsIAtom* aFrameType, 
                                   char*    aFrameNameAbbrev, 
                                   char*    aFrameName)
{
  mType = aFrameType;
  strcpy(mNameAbbrev, aFrameNameAbbrev);
  strcpy(mName, aFrameName);
}

struct DR_FrameTreeNode
{
  DR_FrameTreeNode(nsIFrame* aFrame, DR_FrameTreeNode* aParent) : mFrame(aFrame), mParent(aParent), mDisplay(0), mIndent(0) {}

  nsIFrame*         mFrame;
  DR_FrameTreeNode* mParent;
  PRBool            mDisplay;
  PRUint32          mIndent;
};

// DR_State implementation

DR_State::DR_State() 
: mInited(PR_FALSE), mActive(PR_FALSE), mCount(0), mAssert(-1), mIndentStart(0), mIndentUndisplayedFrames(PR_FALSE)
{}

void DR_State::Init() 
{
  char* env = PR_GetEnv("GECKO_DISPLAY_REFLOW_ASSERT");
  PRInt32 num;
  if (env) {
    if (GetNumber(env, num)) 
      mAssert = num;
    else 
      printf("GECKO_DISPLAY_REFLOW_ASSERT - invalid value = %s", env);
  }

  env = PR_GetEnv("GECKO_DISPLAY_REFLOW_INDENT_START");
  if (env) {
    if (GetNumber(env, num)) 
      mIndentStart = num;
    else 
      printf("GECKO_DISPLAY_REFLOW_INDENT_START - invalid value = %s", env);
  }

  env = PR_GetEnv("GECKO_DISPLAY_REFLOW_INDENT_UNDISPLAYED_FRAMES");
  if (env) {
    if (GetNumber(env, num)) 
      mIndentUndisplayedFrames = num;
    else 
      printf("GECKO_DISPLAY_REFLOW_INDENT_UNDISPLAYED_FRAMES - invalid value = %s", env);
  }
  InitFrameTypeTable();
  ParseRulesFile();
  mInited = PR_TRUE;
}

DR_State::~DR_State()
{
  PRInt32 numElements, i;
  numElements = mWildRules.Count();
  for (i = numElements - 1; i >= 0; i--) {
    delete (DR_Rule *)mWildRules.ElementAt(i);
  }
  numElements = mFrameTreeLeaves.Count();
  for (i = numElements - 1; i >= 0; i--) {
    delete (DR_FrameTreeNode *)mFrameTreeLeaves.ElementAt(i);
  }
  numElements = mFrameTypeTable.Count();
  for (i = numElements - 1; i >= 0; i--) {
    delete (DR_FrameTypeInfo *)mFrameTypeTable.ElementAt(i);
  }
}

PRBool DR_State::GetNumber(char*     aBuf, 
                           PRInt32&  aNumber)
{
  if (sscanf(aBuf, "%d", &aNumber) > 0) 
    return PR_TRUE;
  else 
    return PR_FALSE;
}

PRBool DR_State::IsWhiteSpace(int c) {
  return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r');
}

PRBool DR_State::GetToken(FILE* aFile,
                          char* aBuf)
{
  PRBool haveToken = PR_FALSE;
  aBuf[0] = 0;
  // get the 1st non whitespace char
  int c = -1;
  for (c = getc(aFile); (c > 0) && IsWhiteSpace(c); c = getc(aFile)) {
  }

  if (c > 0) {
    haveToken = PR_TRUE;
    aBuf[0] = c;
    // get everything up to the next whitespace char
    PRInt32 cX;
    for (cX = 1, c = getc(aFile); ; cX++, c = getc(aFile)) {
      if (c < 0) { // EOF
        ungetc(' ', aFile); 
        break;
      }
      else {
        if (IsWhiteSpace(c)) {
          break;
        }
        else {
          aBuf[cX] = c;
        }
      }
    }
    aBuf[cX] = 0;
  }
  return haveToken;
}

DR_Rule* DR_State::ParseRule(FILE* aFile)
{
  char buf[128];
  PRInt32 doDisplay;
  DR_Rule* rule = nsnull;
  while (GetToken(aFile, buf)) {
    if (GetNumber(buf, doDisplay)) {
      if (rule) { 
        rule->mDisplay = (PRBool)doDisplay;
        break;
      }
      else {
        printf("unexpected token - %s \n", buf);
      }
    }
    else {
      if (!rule) {
        rule = new DR_Rule;
      }
      if (strcmp(buf, "*") == 0) {
        rule->AddPart(nsnull);
      }
      else {
        DR_FrameTypeInfo* info = GetFrameTypeInfo(buf);
        if (info) {
          rule->AddPart(info->mType);
        }
        else {
          printf("invalid frame type - %s \n", buf);
        }
      }
    }
  }
  return rule;
}

void DR_State::AddRule(nsVoidArray& aRules,
                       DR_Rule&     aRule)
{
  PRInt32 numRules = aRules.Count();
  for (PRInt32 ruleX = 0; ruleX < numRules; ruleX++) {
    DR_Rule* rule = (DR_Rule*)aRules.ElementAt(ruleX);
    NS_ASSERTION(rule, "program error");
    if (aRule.mLength > rule->mLength) {
      aRules.InsertElementAt(&aRule, ruleX);
      return;
    }
  }
  aRules.AppendElement(&aRule);
}

void DR_State::ParseRulesFile()
{
  char* path = PR_GetEnv("GECKO_DISPLAY_REFLOW_RULES_FILE");
  if (path) {
    FILE* inFile = fopen(path, "r");
    if (inFile) {
      for (DR_Rule* rule = ParseRule(inFile); rule; rule = ParseRule(inFile)) {
        if (rule->mTarget) {
          nsIAtom* fType = rule->mTarget->mFrameType;
          if (fType) {
            DR_FrameTypeInfo* info = GetFrameTypeInfo(fType);
            if (info) {
              AddRule(info->mRules, *rule);
            }
          }
          else {
            AddRule(mWildRules, *rule);
          }
          mActive = PR_TRUE;
        }
      }
    }
  }
}


void DR_State::AddFrameTypeInfo(nsIAtom* aFrameType,
                                char*    aFrameNameAbbrev,
                                char*    aFrameName)
{
  mFrameTypeTable.AppendElement(new DR_FrameTypeInfo(aFrameType, aFrameNameAbbrev, aFrameName));
}

DR_FrameTypeInfo* DR_State::GetFrameTypeInfo(nsIAtom* aFrameType)
{
  PRInt32 numEntries = mFrameTypeTable.Count();
  for (PRInt32 i = 0; i < numEntries; i++) {
    DR_FrameTypeInfo* info = (DR_FrameTypeInfo*)mFrameTypeTable.ElementAt(i);
    if (info && (info->mType == aFrameType)) {
      return info;
    }
  }
  return (DR_FrameTypeInfo*)mFrameTypeTable.ElementAt(numEntries - 1); // return unknown frame type
}

DR_FrameTypeInfo* DR_State::GetFrameTypeInfo(char* aFrameName)
{
  PRInt32 numEntries = mFrameTypeTable.Count();
  for (PRInt32 i = 0; i < numEntries; i++) {
    DR_FrameTypeInfo* info = (DR_FrameTypeInfo*)mFrameTypeTable.ElementAt(i);
    if (info && ((strcmp(aFrameName, info->mName) == 0) || (strcmp(aFrameName, info->mNameAbbrev) == 0))) {
      return info;
    }
  }
  return (DR_FrameTypeInfo*)mFrameTypeTable.ElementAt(numEntries - 1); // return unknown frame type
}

void DR_State::InitFrameTypeTable()
{  
  AddFrameTypeInfo(nsLayoutAtoms::areaFrame,             "area",      "area");
  AddFrameTypeInfo(nsLayoutAtoms::blockFrame,            "block",     "block");
  AddFrameTypeInfo(nsLayoutAtoms::brFrame,               "br",        "br");
  AddFrameTypeInfo(nsLayoutAtoms::bulletFrame,           "bullet",    "bullet");
  AddFrameTypeInfo(nsLayoutAtoms::gfxButtonControlFrame, "button",    "gfxButtonControl");
  AddFrameTypeInfo(nsLayoutAtoms::htmlFrameInnerFrame,   "frameI",    "htmlFrameInner");
  AddFrameTypeInfo(nsLayoutAtoms::htmlFrameOuterFrame,   "frameO",    "htmlFrameOuter");
  AddFrameTypeInfo(nsLayoutAtoms::imageFrame,            "img",       "image");
  AddFrameTypeInfo(nsLayoutAtoms::inlineFrame,           "inline",    "inline");
  AddFrameTypeInfo(nsLayoutAtoms::letterFrame,           "letter",    "letter");
  AddFrameTypeInfo(nsLayoutAtoms::lineFrame,             "line",      "line");
  AddFrameTypeInfo(nsLayoutAtoms::listControlFrame,      "select",    "select");
  AddFrameTypeInfo(nsLayoutAtoms::objectFrame,           "obj",       "object");
  AddFrameTypeInfo(nsLayoutAtoms::pageFrame,             "page",      "page");
  AddFrameTypeInfo(nsLayoutAtoms::placeholderFrame,      "place",     "placeholder");
  AddFrameTypeInfo(nsLayoutAtoms::positionedInlineFrame, "posInline", "positionedInline");
  AddFrameTypeInfo(nsLayoutAtoms::canvasFrame,           "canvas",    "canvas");
  AddFrameTypeInfo(nsLayoutAtoms::rootFrame,             "root",      "root");
  AddFrameTypeInfo(nsLayoutAtoms::scrollFrame,           "scroll",    "scroll");
  AddFrameTypeInfo(nsLayoutAtoms::tableCaptionFrame,     "caption",   "tableCaption");
  AddFrameTypeInfo(nsLayoutAtoms::tableCellFrame,        "cell",      "tableCell");
  AddFrameTypeInfo(nsLayoutAtoms::tableColFrame,         "col",       "tableCol");
  AddFrameTypeInfo(nsLayoutAtoms::tableColGroupFrame,    "colG",      "tableColGroup");
  AddFrameTypeInfo(nsLayoutAtoms::tableFrame,            "tbl",       "table");
  AddFrameTypeInfo(nsLayoutAtoms::tableOuterFrame,       "tblO",      "tableOuter");
  AddFrameTypeInfo(nsLayoutAtoms::tableRowGroupFrame,    "rowG",      "tableRowGroup");
  AddFrameTypeInfo(nsLayoutAtoms::tableRowFrame,         "row",       "tableRow");
  AddFrameTypeInfo(nsLayoutAtoms::textInputFrame,        "textCtl",   "textInput");
  AddFrameTypeInfo(nsLayoutAtoms::textFrame,             "text",      "text");
  AddFrameTypeInfo(nsLayoutAtoms::viewportFrame,         "VP",        "viewport");
  AddFrameTypeInfo(nsnull,                               "unknown",   "unknown");
}


void DR_State::DisplayFrameTypeInfo(nsIFrame* aFrame,
                                    PRInt32   aIndent)
{ 
  nsCOMPtr<nsIAtom> fType;
  aFrame->GetFrameType(getter_AddRefs(fType));

  DR_FrameTypeInfo* frameTypeInfo = GetFrameTypeInfo(fType);
  if (frameTypeInfo) {
    for (PRInt32 i = 0; i < aIndent; i++) {
      printf(" ");
    }
    printf("%s %p ", frameTypeInfo->mNameAbbrev, aFrame);
  }
}

PRBool DR_State::RuleMatches(DR_Rule&          aRule,
                             DR_FrameTreeNode& aNode)
{
  NS_ASSERTION(aRule.mTarget, "program error");

  DR_RulePart* rulePart;
  DR_FrameTreeNode* parentNode;
  for (rulePart = aRule.mTarget->mNext, parentNode = aNode.mParent;
       rulePart && parentNode;
       rulePart = rulePart->mNext, parentNode = parentNode->mParent) {
    if (rulePart->mFrameType) {
      if (parentNode->mFrame) {
        nsCOMPtr<nsIAtom> fNodeType;
        parentNode->mFrame->GetFrameType(getter_AddRefs(fNodeType));
        if (rulePart->mFrameType != fNodeType) {
          return PR_FALSE;
        }
      }
      else NS_ASSERTION(PR_FALSE, "program error");
    }
    // else wild card match
  }
  return PR_TRUE;
}

void DR_State::FindMatchingRule(DR_FrameTreeNode& aNode)
{
  if (!aNode.mFrame) {
    NS_ASSERTION(PR_FALSE, "invalid DR_FrameTreeNode \n");
    return;
  }

  PRBool matchingRule = PR_FALSE;

  nsCOMPtr<nsIAtom> fType;
  aNode.mFrame->GetFrameType(getter_AddRefs(fType));
  DR_FrameTypeInfo* info = GetFrameTypeInfo(fType.get());
  NS_ASSERTION(info, "program error");
  PRInt32 numRules = info->mRules.Count();
  for (PRInt32 ruleX = 0; ruleX < numRules; ruleX++) {
    DR_Rule* rule = (DR_Rule*)info->mRules.ElementAt(ruleX);
    if (rule && RuleMatches(*rule, aNode)) {
      aNode.mDisplay = rule->mDisplay;
      matchingRule = PR_TRUE;
      break;
    }
  }
  if (!matchingRule) {
    PRInt32 numWildRules = mWildRules.Count();
    for (PRInt32 ruleX = 0; ruleX < numWildRules; ruleX++) {
      DR_Rule* rule = (DR_Rule*)mWildRules.ElementAt(ruleX);
      if (rule && RuleMatches(*rule, aNode)) {
        aNode.mDisplay = rule->mDisplay;
        break;
      }
    }
  }

  if (aNode.mParent) {
    aNode.mIndent = aNode.mParent->mIndent;
    if (aNode.mDisplay || mIndentUndisplayedFrames) {
      aNode.mIndent++;
    }
  }
}
    
DR_FrameTreeNode* DR_State::CreateTreeNode(nsIFrame*                aFrame,
                                           const nsHTMLReflowState& aReflowState)
{
  // find the frame of the parent reflow state (usually just the parent of aFrame)
  const nsHTMLReflowState* parentRS = aReflowState.parentReflowState;
  nsIFrame* parentFrame = (parentRS) ? parentRS->frame : nsnull;

  // find the parent tree node leaf
  DR_FrameTreeNode* parentNode = nsnull;
  DR_FrameTreeNode* lastLeaf = (DR_FrameTreeNode*)mFrameTreeLeaves.ElementAt(mFrameTreeLeaves.Count() - 1);
  if (lastLeaf) {
    for (parentNode = lastLeaf; parentNode && (parentNode->mFrame != parentFrame); parentNode = parentNode->mParent) {
    }
  }
  DR_FrameTreeNode* newNode = new DR_FrameTreeNode(aFrame, parentNode);
  FindMatchingRule(*newNode);
  if (lastLeaf == parentNode) {
    mFrameTreeLeaves.RemoveElementAt(mFrameTreeLeaves.Count() - 1);
  }
  mFrameTreeLeaves.AppendElement(newNode);
  mCount++;

  return newNode;
}

void DR_State::PrettyUC(nscoord aSize,
                        char*   aBuf)
{
  if (NS_UNCONSTRAINEDSIZE == aSize) {
    strcpy(aBuf, "UC");
  }
  else {
    sprintf(aBuf, "%d", aSize);
  }
}

void DR_State::DeleteTreeNode(DR_FrameTreeNode& aNode)
{
  mFrameTreeLeaves.RemoveElement(&aNode);
  PRInt32 numLeaves = mFrameTreeLeaves.Count();
  if ((0 == numLeaves) || (aNode.mParent != (DR_FrameTreeNode*)mFrameTreeLeaves.ElementAt(numLeaves - 1))) {
    mFrameTreeLeaves.AppendElement(aNode.mParent);
  }
  // delete the tree node 
  delete &aNode;
}

void* nsFrame::DisplayReflowEnter(nsIFrame*                aFrame,
                                  const nsHTMLReflowState& aReflowState)
{
  if (!DR_state.mInited) DR_state.Init();
  if (!DR_state.mActive) return nsnull;

  NS_ASSERTION(aFrame, "invalid call");

  DR_FrameTreeNode* treeNode = DR_state.CreateTreeNode(aFrame, aReflowState);
  if (treeNode->mDisplay) {
    DR_state.DisplayFrameTypeInfo(aFrame, treeNode->mIndent);

    char width[16];
    char height[16];
    DR_state.PrettyUC(aReflowState.availableWidth, width);
    DR_state.PrettyUC(aReflowState.availableHeight, height);
    printf("r=%d a=%s,%s ", aReflowState.reason, width, height); 

    DR_state.PrettyUC(aReflowState.mComputedWidth, width);
    DR_state.PrettyUC(aReflowState.mComputedHeight, height);
    printf("c=%s,%s ", width, height);

    nsIFrame* inFlow;
    aFrame->GetPrevInFlow(&inFlow);
    if (inFlow) {
      printf("pif=%p ", inFlow);
    }
    aFrame->GetNextInFlow(&inFlow);
    if (inFlow) {
      printf("nif=%p ", inFlow);
    }
    printf("cnt=%d \n", DR_state.mCount);
  }
  return treeNode;
}

void nsFrame::DisplayReflowExit(nsIFrame*            aFrame,
                                nsHTMLReflowMetrics& aMetrics,
                                nsReflowStatus       aStatus,
                                void*                aFrameTreeNode)
{
  if (!DR_state.mActive) return;

  NS_ASSERTION(aFrame, "DisplayReflowExit - invalid call");
  if (!aFrameTreeNode) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aFrameTreeNode;
  if (treeNode->mDisplay) {
    DR_state.DisplayFrameTypeInfo(aFrame, treeNode->mIndent);

    char width[16];
    char height[16];
    DR_state.PrettyUC(aMetrics.width, width);
    DR_state.PrettyUC(aMetrics.height, height);
    printf("d=%s,%s ", width, height);

    if (aMetrics.maxElementSize) {
      DR_state.PrettyUC(aMetrics.maxElementSize->width, width);
      printf("me=%s ", width);
    }
    if (aMetrics.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
      DR_state.PrettyUC(aMetrics.mMaximumWidth, width);
      printf("m=%s ", width);
    }
    if (NS_FRAME_COMPLETE != aStatus) {
      printf("status=%d", aStatus);
    }
    printf("\n");
  }
  DR_state.DeleteTreeNode(*treeNode);
}

#endif
// End Display Reflow

#endif
