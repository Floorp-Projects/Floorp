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

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include "nsIComponentManager.h"
#include "nsIFrameSelection.h"
#include "nsIFrame.h"
#include "nsIDOMNode.h"
#include "nsIDOMRange.h"
#include "nsIDOMSelection.h"
#include "nsIDOMCharacterData.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsIView.h"
#include "nsIScrollableView.h"
#include "nsIViewManager.h"
#include "nsIPresContext.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"     // for NS_LOOKANDFEEL_CID
#include "nsBlockFrame.h"
#include "nsISelectionController.h"

#include "nsCaret.h"

// Because of drawing issues, we currently always make a new RC. See bug 28068
// Before removing this, stuff will need to be fixed and tested on all platforms.
// For example, turning this off on Mac right now causes drawing problems on pages
// with form elements.
#define DONT_REUSE_RENDERING_CONTEXT

static NS_DEFINE_IID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);

//-----------------------------------------------------------------------------

nsCaret::nsCaret()
: mPresShell(nsnull)
, mBlinkRate(500)
, mCaretTwipsWidth(-1)
, mCaretPixelsWidth(1)
, mVisible(PR_FALSE)
, mReadOnly(PR_TRUE)
, mDrawn(PR_FALSE)
, mLastCaretFrame(nsnull)
, mLastCaretView(nsnull)
, mLastContentOffset(0)
{
  NS_INIT_REFCNT();
}


//-----------------------------------------------------------------------------
nsCaret::~nsCaret()
{
  KillTimer();
}

//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::Init(nsIPresShell *inPresShell)
{
  if (!inPresShell)
    return NS_ERROR_NULL_POINTER;
  
  mPresShell = getter_AddRefs(NS_GetWeakReference(inPresShell));    // the presshell owns us, so no addref

  nsILookAndFeel* touchyFeely;
  if (NS_SUCCEEDED(nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, NS_GET_IID(nsILookAndFeel), (void**)&touchyFeely)))
  {
    PRInt32 tempInt;
    
    if (NS_SUCCEEDED(touchyFeely->GetMetric(nsILookAndFeel::eMetric_SingleLineCaretWidth, tempInt)))
      mCaretTwipsWidth = (nscoord)tempInt;
    if (NS_SUCCEEDED(touchyFeely->GetMetric(nsILookAndFeel::eMetric_CaretBlinkTime, tempInt)))
      mBlinkRate = (PRUint32)tempInt;
    
    NS_RELEASE(touchyFeely);
  }
  
  // get the selection from the pres shell, and set ourselves up as a selection
  // listener
  
  nsCOMPtr<nsIDOMSelection> domSelection;
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mPresShell);
  if (selCon)
  {
    if (NS_SUCCEEDED(selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSelection))))
    {
      domSelection->AddSelectionListener(this);
      mDomSelectionWeak = getter_AddRefs( NS_GetWeakReference(domSelection) );
    }
  }
  else
    return NS_ERROR_FAILURE;
  
  // set up the blink timer
  if (mVisible)
  {
    nsresult  err = StartBlinking();
    if (NS_FAILED(err))
      return err;
  }
  
  return NS_OK;
}



//-----------------------------------------------------------------------------
NS_IMPL_ADDREF(nsCaret);
NS_IMPL_RELEASE(nsCaret);
//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::QueryInterface(const nsIID& aIID,
                                     void** aInstancePtrResult)
{
  NS_PRECONDITION(aInstancePtrResult, "null pointer");
  if (!aInstancePtrResult)
  return NS_ERROR_NULL_POINTER;

  nsISupports* foundInterface;  

  if (aIID.Equals(NS_GET_IID(nsISupports)))
    foundInterface = (nsISupports*)(nsICaret*)this;   // whoo boy
  else if (aIID.Equals(NS_GET_IID(nsICaret)))
    foundInterface = (nsICaret*)this;
  else if (aIID.Equals(NS_GET_IID(nsIDOMSelectionListener)))
    foundInterface = (nsIDOMSelectionListener*)this;
  else
    foundInterface = nsnull;

  nsresult status;
  if (! foundInterface)
    status = NS_NOINTERFACE;
  else
  {
    NS_ADDREF(foundInterface);
    status = NS_OK;
  }

  *aInstancePtrResult = foundInterface;
  return status;
}

//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::SetCaretDOMSelection(nsIDOMSelection *aDOMSel)
{
  NS_ENSURE_ARG_POINTER(aDOMSel);
  mDomSelectionWeak = getter_AddRefs( NS_GetWeakReference(aDOMSel) );   // weak reference to pres shell
  return NS_OK;
}


//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::SetCaretVisible(PRBool inMakeVisible)
{
  mVisible = inMakeVisible;
  nsresult  err = NS_OK;
  if (mVisible)
    err = StartBlinking();
  else
    err = StopBlinking();
    
  return err;
}

NS_IMETHODIMP nsCaret::GetCaretVisible(PRBool *outMakeVisible)
{
  NS_ENSURE_ARG_POINTER(outMakeVisible);
  *outMakeVisible = mVisible;
  return NS_OK;
}


//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::SetCaretReadOnly(PRBool inMakeReadonly)
{
  mReadOnly = inMakeReadonly;
  return NS_OK;
}


//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::GetWindowRelativeCoordinates(nsRect& outCoordinates, PRBool& outIsCollapsed, nsIDOMSelection *aDOMSel)
{
  if (!mPresShell)
    return NS_ERROR_NOT_INITIALIZED;

  mDomSelectionWeak = getter_AddRefs( NS_GetWeakReference(aDOMSel) );   // weak reference to pres shell
    
  nsCOMPtr<nsIDOMSelection> domSelection = aDOMSel;
  nsresult err;
  if (!domSelection)
    return NS_ERROR_NOT_INITIALIZED;    // no selection
  
  // fill in defaults for failure
  outCoordinates.x = -1;
  outCoordinates.y = -1;
  outCoordinates.width = -1;
  outCoordinates.height = -1;
  outIsCollapsed = PR_FALSE;
  
  err = domSelection->GetIsCollapsed(&outIsCollapsed);
  if (NS_FAILED(err)) 
    return err;
    
  // code in progress
  nsCOMPtr<nsIDOMNode>  focusNode;
  
  err = domSelection->GetFocusNode(getter_AddRefs(focusNode));
  if (NS_FAILED(err))
    return err;
  if (!focusNode)
    return NS_ERROR_FAILURE;
  
  PRInt32 focusOffset;
  err = domSelection->GetFocusOffset(&focusOffset);
  if (NS_FAILED(err))
    return err;
    
/*
  // is this a text node?
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(focusNode);
  // note that we only work with text nodes here, unlike when drawing the caret.
  // this is because this routine is intended for IME support, which only cares about text.
  if (!nodeAsText)
    return NS_ERROR_UNEXPECTED;
*/  
  nsCOMPtr<nsIContent>contentNode = do_QueryInterface(focusNode);
  if (!contentNode)
    return NS_ERROR_FAILURE;

  //get frame selection and find out what frame to use...
  nsCOMPtr<nsIFrameSelection> frameSelection;
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (presShell)
    err = presShell->GetFrameSelection(getter_AddRefs(frameSelection));
  else
    return NS_ERROR_FAILURE;
  if (NS_FAILED(err) || !frameSelection)
    return err?err : NS_ERROR_FAILURE;  

  // find the frame that contains the content node that has focus
  nsIFrame*       theFrame = nsnull;
  PRInt32         theFrameOffset = 0;
  PRBool hintRight;
  domSelection->GetHint(&hintRight);//translate hint.
  nsIFrameSelection::HINT hint;
  if (hintRight)
    hint = nsIFrameSelection::HINTRIGHT;
  else
    hint = nsIFrameSelection::HINTLEFT;
  err = frameSelection->GetFrameForNodeOffset(contentNode, focusOffset, hint, &theFrame, &theFrameOffset);
  if (NS_FAILED(err) || !theFrame)
    return err;
  
  nsPoint   viewOffset(0, 0);
  nsRect    clipRect;
  nsIView   *drawingView;     // views are not refcounted
  GetViewForRendering(theFrame, eTopLevelWindowCoordinates, viewOffset, clipRect, drawingView);
  if (!drawingView)
    return NS_ERROR_UNEXPECTED;

  // ramp up to make a rendering context for measuring text.
  // First, we get the pres context ...
  nsCOMPtr<nsIPresContext> presContext;
  err = presShell->GetPresContext(getter_AddRefs(presContext));
  if (NS_FAILED(err))
    return err;
  
  // ... then get a device context
  nsCOMPtr<nsIDeviceContext>    dx;
  err = presContext->GetDeviceContext(getter_AddRefs(dx));
  if (NS_FAILED(err))
    return err;
  if (!dx)
    return NS_ERROR_UNEXPECTED;

  // ... then tell it to make a rendering context
  nsCOMPtr<nsIRenderingContext> rendContext;  
  err = dx->CreateRenderingContext(drawingView, *getter_AddRefs(rendContext));            
  if (NS_FAILED(err))
    return err;
  if (!rendContext)
    return NS_ERROR_UNEXPECTED;

  // now we can measure the offset into the frame.
  nsPoint   framePos(0, 0);
  theFrame->GetPointFromOffset(presContext, rendContext, theFrameOffset, &framePos);

  nsRect          frameRect;
  theFrame->GetRect(frameRect);
  
  // now add the frame offset to the view offset, and we're done
  viewOffset += framePos;
  outCoordinates.x = viewOffset.x;
  outCoordinates.y = viewOffset.y;
  outCoordinates.height = frameRect.height;
  outCoordinates.width  = frameRect.width;
  
  return NS_OK;
}

//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::ClearFrameRefs(nsIFrame* aFrame)
{

  if (mLastCaretFrame == aFrame)
  {
    mLastCaretFrame = nsnull;     // frames are not refcounted.
    mLastCaretView = nsnull;
    mLastContentOffset = 0;
  }
  
  mDrawn = PR_FALSE;    // assume that the view has been cleared, and ensure
                        // that we don't try to use the frame.
  
  // should we just call StopBlinking() here?
  
  return NS_OK; 
}

NS_IMETHODIMP nsCaret::EraseCaret()
{
  if (mDrawn)
    DrawCaret();
  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#endif

//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::NotifySelectionChanged(nsIDOMDocument *, nsIDOMSelection *aDomSel, short aReason)
{
  if (aReason & nsIDOMSelectionListener::MOUSEUP_REASON)//this wont do
    return NS_OK;
  if (mVisible)
    StopBlinking();
  nsCOMPtr<nsIDOMSelection> domSel(do_QueryReferent(mDomSelectionWeak));
  if (domSel.get() != aDomSel)
    return NS_OK; //ignore this then.
  if (mVisible)
    StartBlinking();
  
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif

//-----------------------------------------------------------------------------
void nsCaret::KillTimer()
{
  if (mBlinkTimer)
  {
    mBlinkTimer->Cancel();
  }
}


//-----------------------------------------------------------------------------
nsresult nsCaret::PrimeTimer()
{
  KillTimer();
  
  // set up the blink timer
  if (!mReadOnly && mBlinkRate > 0)
  {
    nsresult  err;
    mBlinkTimer = do_CreateInstance("@mozilla.org/timer;1", &err);
    
    if (NS_FAILED(err))
      return err;
    
    mBlinkTimer->Init(CaretBlinkCallback, this, mBlinkRate, NS_PRIORITY_HIGH, NS_TYPE_REPEATING_PRECISE);
  }

  return NS_OK;
}


//-----------------------------------------------------------------------------
nsresult nsCaret::StartBlinking()
{
  PrimeTimer();

  //NS_ASSERTION(!mDrawn, "Caret should not be drawn here");
  DrawCaret();    // draw it right away
  
  return NS_OK;
}


//-----------------------------------------------------------------------------
nsresult nsCaret::StopBlinking()
{
  if (mDrawn)     // erase the caret if necessary
    DrawCaret();
    
  KillTimer();
  
  return NS_OK;
}


//-----------------------------------------------------------------------------
// Get the nsIFrame and the content offset for the current caret position.
// Returns PR_TRUE if we should go ahead and draw, PR_FALSE otherwise. 
//
PRBool nsCaret::SetupDrawingFrameAndOffset()
{
  if (!mDomSelectionWeak)
    return PR_FALSE;
  
  nsCOMPtr<nsIDOMSelection> domSelection = do_QueryReferent(mDomSelectionWeak);
  if (!domSelection) return PR_FALSE;
  
  PRBool isCollapsed = PR_FALSE;
  domSelection->GetIsCollapsed(&isCollapsed);
  if (!isCollapsed) return PR_FALSE;

  // start and end parent should be the same since we are collapsed
  nsCOMPtr<nsIDOMNode>  focusNode;
  domSelection->GetFocusNode(getter_AddRefs(focusNode));
  if (!focusNode) return PR_FALSE;
  
  PRInt32 contentOffset;
  if (NS_FAILED(domSelection->GetFocusOffset(&contentOffset)))
    return PR_FALSE;
  
  nsCOMPtr<nsIContent> contentNode = do_QueryInterface(focusNode);
  if (!contentNode) return PR_FALSE;
      
  //get frame selection and find out what frame to use...
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell)
    return PR_FALSE;
    
  nsCOMPtr<nsIFrameSelection> frameSelection;
  presShell->GetFrameSelection(getter_AddRefs(frameSelection));
  if (!frameSelection)
    return PR_FALSE;

  PRBool hintRight;
  domSelection->GetHint(&hintRight);//translate hint.
  nsIFrameSelection::HINT hint;
  hint = (hintRight) ? nsIFrameSelection::HINTRIGHT : nsIFrameSelection::HINTLEFT;

  nsIFrame* theFrame = nsnull;
  PRInt32   theFrameOffset = 0;

  nsresult rv = frameSelection->GetFrameForNodeOffset(contentNode, contentOffset, hint, &theFrame, &theFrameOffset);
  if (NS_FAILED(rv) || !theFrame)
    return PR_FALSE;

  // now we have a frame, check whether it's appropriate to show the caret here
  const nsStyleUserInterface* userinterface;
  theFrame->GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)userinterface);
  if (userinterface)
  {
    if (
#ifdef SUPPORT_USER_MODIFY
          // editable content still defaults to NS_STYLE_USER_MODIFY_READ_ONLY at present. See bug 15284
          (userinterface->mUserModify == NS_STYLE_USER_MODIFY_READ_ONLY) ||
#endif          
          (userinterface->mUserInput == NS_STYLE_USER_INPUT_NONE) ||
          (userinterface->mUserInput == NS_STYLE_USER_INPUT_DISABLED))
    {
      return PR_FALSE;
    }  
  }
  
  // mark the frame, so we get notified on deletion.
  // frames are never unmarked, which means that we'll touch every frame we visit.
  // this is not ideal.
  nsFrameState frameState;
  theFrame->GetFrameState(&frameState);
  frameState |= NS_FRAME_EXTERNAL_REFERENCE;
  theFrame->SetFrameState(frameState);
  
  mLastCaretFrame = theFrame;
  mLastContentOffset = theFrameOffset;
  return PR_TRUE;
}


//-----------------------------------------------------------------------------
void nsCaret::GetViewForRendering(nsIFrame *caretFrame, EViewCoordinates coordType, nsPoint &viewOffset, nsRect& outClipRect, nsIView* &outView)
{
  outView = nsnull;
  
  if (!caretFrame)
    return;
    
  NS_ASSERTION(caretFrame, "Should have frame here");
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell)
    return;
  
  nsCOMPtr<nsIPresContext>  presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));

  viewOffset.x = 0;
  viewOffset.y = 0;
  
  nsPoint   withinViewOffset(0, 0);
  // get the offset of this frame from its parent view (walks up frame hierarchy)
  nsIView* theView = nsnull;
  caretFrame->GetOffsetFromView(presContext, withinViewOffset, &theView);
  if (theView == nsnull) return;
  
  nsIView*    returnView = nsnull;    // views are not refcounted
  
  nscoord   x, y;
  
  // coorinates relative to the view we are going to use for drawing
  if (coordType == eViewCoordinates)
  {
    nsIView*            startingView = theView;
    nsIScrollableView*  scrollableView = nsnull;
  
    nsPoint             drawViewOffset(0, 0);         // offset to the view we are using to draw
    
    // walk up to the first view with a widget
    do {
      theView->GetPosition(&x, &y);

      //is this a scrollable view?
      if (!scrollableView)
        theView->QueryInterface(NS_GET_IID(nsIScrollableView), (void **)&scrollableView);

      PRBool hasWidget;
      theView->HasWidget(&hasWidget);
      if (hasWidget)
      {
        returnView = theView;
        break;
      }
      drawViewOffset.x += x;
      drawViewOffset.y += y;
      
      theView->GetParent(theView);
    } while (theView);
    
    viewOffset = withinViewOffset;
    viewOffset += drawViewOffset;
    
    if (scrollableView)
    {
      const nsIView*      clipView = nsnull;
      scrollableView->GetClipView(&clipView);
      if (!clipView) return;      // should always have one
      
      nsRect  bounds;
      clipView->GetBounds(bounds);
      scrollableView->GetScrollPosition(bounds.x, bounds.y);
      
      bounds += drawViewOffset;   // offset to coords of returned view
      outClipRect = bounds;
    }
    else
    {
      returnView->GetBounds(outClipRect);
    }
    
  }
  else
  {
    // window-relative coordinates (walk right to the top of the view hierarchy)
    // we don't do anything with clipping here
    
    do {
      theView->GetPosition(&x, &y);

      if (!returnView)
      {
        PRBool hasWidget;
        theView->HasWidget(&hasWidget);
        
        if (hasWidget)
          returnView = theView;
      }
      // is this right?
      viewOffset.x += x;
      viewOffset.y += y;
      
      theView->GetParent(theView);
    } while (theView);
  
  }
  
  
  outView = returnView;
}


/*-----------------------------------------------------------------------------

  MustDrawCaret
  
  FInd out if we need to do any caret drawing. This returns true if
  either a) or b)
  a) caret has been drawn, and we need to erase it.
  b) caret is not drawn, and selection is collapsed
  
----------------------------------------------------------------------------- */
PRBool nsCaret::MustDrawCaret()
{
  if (mDrawn)
    return PR_TRUE;
    
  nsCOMPtr<nsIDOMSelection> domSelection = do_QueryReferent(mDomSelectionWeak);
  if (!domSelection)
    return PR_FALSE;
  PRBool isCollapsed;

  if (NS_FAILED(domSelection->GetIsCollapsed(&isCollapsed)))
    return PR_FALSE;
    
  return isCollapsed;
}


/*-----------------------------------------------------------------------------

  DrawCaret
    
----------------------------------------------------------------------------- */

void nsCaret::DrawCaret()
{
  // do we need to draw the caret at all?
  if (!MustDrawCaret())
    return;
  
  // if we are drawing, not erasing, then set up the frame etc.
  if (!mDrawn)
  {
    if (!SetupDrawingFrameAndOffset())
      return;
  }
  
  NS_ASSERTION(mLastCaretFrame != nsnull, "Should have a frame here");
  
  nsRect    frameRect;
  mLastCaretFrame->GetRect(frameRect);
  frameRect.x = 0;      // the origin is accounted for in GetViewForRendering()
  frameRect.y = 0;
  
  if (frameRect.height == 0)    // we're in a BR frame which has zero height.
  {
    frameRect.height = 200;
    frameRect.y -= 200;
  }
  
  nsPoint   viewOffset(0, 0);
  nsRect    clipRect;
  nsIView   *drawingView;
  GetViewForRendering(mLastCaretFrame, eViewCoordinates, viewOffset, clipRect, drawingView);

  if (drawingView == nsnull)
    return;
  
  frameRect += viewOffset;

  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell) return;
  
  nsCOMPtr<nsIPresContext> presContext;
  if (NS_FAILED(presShell->GetPresContext(getter_AddRefs(presContext))))
    return;

  // if the view changed, or we don't have a rendering context, make one
  // because of drawing issues, always make a new RC at the momemt. See bug 28068
  if (
#ifdef DONT_REUSE_RENDERING_CONTEXT
      PR_TRUE ||
#endif
      (mLastCaretView != drawingView) || !mRendContext)
  {
    mRendContext = nsnull;    // free existing one if we have one
    
    nsCOMPtr<nsIDeviceContext>    dx;
    if (NS_FAILED(presContext->GetDeviceContext(getter_AddRefs(dx))) || !dx)
      return;
      
    if (NS_FAILED(dx->CreateRenderingContext(drawingView, *getter_AddRefs(mRendContext))) || !mRendContext)
      return;      
  }

  // push a known good state
  mRendContext->PushState();

  // views are not refcounted
  mLastCaretView = drawingView;

  if (!mDrawn)
  {
    nsPoint   framePos(0, 0);
    nsRect    caretRect = frameRect;
    
    mLastCaretFrame->GetPointFromOffset(presContext, mRendContext, mLastContentOffset, &framePos);
    caretRect += framePos;
    
    //printf("Content offset %ld, frame offset %ld\n", focusOffset, framePos.x);
    if(mCaretTwipsWidth < 0)
    {// need to re-compute the pixel width
      mCaretTwipsWidth  = 15 * mCaretPixelsWidth;//uhhhh...
    }
    caretRect.width = mCaretTwipsWidth;

    // Avoid view redraw problems by making sure the
    // caret doesn't hang outside the right edge of
    // the frame. This ensures that the caret gets
    // erased properly if the frame's right edge gets
    // invalidated.

    nscoord cX = caretRect.x + caretRect.width;
    nscoord fX = frameRect.x + frameRect.width;

    if (caretRect.x <= fX && cX > fX)
    {
      caretRect.x -= cX - fX;

      if (caretRect.x < frameRect.x)
        caretRect.x = frameRect.x;
    }

    mCaretRect.IntersectRect(clipRect, caretRect);
  }
    /*
    if (mReadOnly)
      inRendContext.SetColor(NS_RGB(85, 85, 85));   // we are drawing it; gray
    */
    
  mRendContext->SetColor(NS_RGB(255,255,255));
  mRendContext->InvertRect(mCaretRect);

  PRBool emptyClip;   // I know what you're thinking. "Did he fire six shots or only five?"
  mRendContext->PopState(emptyClip);

  ToggleDrawnStatus();

#ifdef DONT_REUSE_RENDERING_CONTEXT
  mRendContext = nsnull;
#endif
}

#ifdef XP_MAC
#pragma mark -
#endif

//-----------------------------------------------------------------------------
/* static */
void nsCaret::CaretBlinkCallback(nsITimer *aTimer, void *aClosure)
{
  nsCaret   *theCaret = NS_REINTERPRET_CAST(nsCaret*, aClosure);
  if (!theCaret) return;
  
  theCaret->DrawCaret();

#ifndef REPEATING_TIMERS
  theCaret->PrimeTimer();
#endif
}


//-----------------------------------------------------------------------------
nsresult NS_NewCaret(nsICaret** aInstancePtrResult)
{
  NS_PRECONDITION(aInstancePtrResult, "null ptr");
  
  nsCaret* caret = new nsCaret();
  if (nsnull == caret)
      return NS_ERROR_OUT_OF_MEMORY;
      
  return caret->QueryInterface(NS_GET_IID(nsICaret), (void**) aInstancePtrResult);
}

NS_IMETHODIMP nsCaret::SetCaretWidth(nscoord aPixels)
{
  if(!aPixels)
    return NS_ERROR_FAILURE;
  else
  { //no need to optimize this, but if it gets too slow, we can check for case aPixels==mCaretPixelsWidth
    mCaretPixelsWidth = aPixels;
    mCaretTwipsWidth = -1;
  }
  return NS_OK;
}
