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
#include "nsIViewManager.h"
#include "nsIPresContext.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"			// for NS_LOOKANDFEEL_CID
#include "nsBlockFrame.h"
#include "nsISelectionController.h"

#include "nsCaret.h"


static NS_DEFINE_IID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);

//-----------------------------------------------------------------------------

nsCaret::nsCaret()
:	mPresShell(nsnull)
,	mBlinkTimer(nsnull)
,	mBlinkRate(500)
, mCaretWidth(20)
,	mVisible(PR_FALSE)
,	mReadOnly(PR_TRUE)
,	mDrawn(PR_FALSE)
, mLastCaretFrame(nsnull)
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
	
	mPresShell = getter_AddRefs(NS_GetWeakReference(inPresShell));		// the presshell owns us, so no addref

  nsILookAndFeel* touchyFeely;
  if (NS_SUCCEEDED(nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, NS_GET_IID(nsILookAndFeel), (void**)&touchyFeely)))
  {
    PRInt32	tempInt;
    
    if (NS_SUCCEEDED(touchyFeely->GetMetric(nsILookAndFeel::eMetric_CaretWidthTwips, tempInt)))
      mCaretWidth = (nscoord)tempInt;
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
	  }
  }
  else
    return NS_ERROR_FAILURE;
	
	// set up the blink timer
	if (mVisible)
	{
		nsresult	err = StartBlinking();
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
		foundInterface = (nsISupports*)(nsICaret*)this;		// whoo boy
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
NS_IMETHODIMP nsCaret::SetCaretVisible(PRBool inMakeVisible)
{
	mVisible = inMakeVisible;
	nsresult	err = NS_OK;
	if (mVisible)
		err = StartBlinking();
	else
		err = StopBlinking();
		
	return err;
}


//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::SetCaretReadOnly(PRBool inMakeReadonly)
{
	mReadOnly = inMakeReadonly;
	return NS_OK;
}


//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::GetWindowRelativeCoordinates(nsRect& outCoordinates, PRBool& outIsCollapsed)
{
	if (!mPresShell)
		return NS_ERROR_NOT_INITIALIZED;
		
	nsCOMPtr<nsIDOMSelection> domSelection;
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mPresShell);
  nsresult err;
  if (selCon)
  {
    err = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,getter_AddRefs(domSelection));
	  if (NS_FAILED(err))
		  return err;
  }
  else
    return NS_ERROR_FAILURE;
		
	if (!domSelection)
		return NS_ERROR_NOT_INITIALIZED;		// no selection
	
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
	nsCOMPtr<nsIDOMNode>	focusNode;
	
	err = domSelection->GetFocusNode(getter_AddRefs(focusNode));
	if (NS_FAILED(err))
		return err;
	if (!focusNode)
		return NS_ERROR_FAILURE;
	
	PRInt32	focusOffset;
	err = domSelection->GetFocusOffset(&focusOffset);
	if (NS_FAILED(err))
		return err;
		
/*
	// is this a text node?
	nsCOMPtr<nsIDOMCharacterData>	nodeAsText = do_QueryInterface(focusNode);
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
  err = frameSelection->GetFrameForNodeOffset(contentNode, focusOffset, &theFrame, &theFrameOffset);
	if (NS_FAILED(err) || !theFrame)
		return err;
	
	nsPoint		viewOffset(0, 0);
	nsIView		*drawingView;			// views are not refcounted
	GetViewForRendering(theFrame, eTopLevelWindowCoordinates, viewOffset, drawingView);
	if (!drawingView)
		return NS_ERROR_UNEXPECTED;

	// ramp up to make a rendering context for measuring text.
	// First, we get the pres context ...
	nsCOMPtr<nsIPresContext> presContext;
	err = presShell->GetPresContext(getter_AddRefs(presContext));
	if (NS_FAILED(err))
		return err;
	
	// ... then get a device context
	nsCOMPtr<nsIDeviceContext> 		dx;
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
	nsPoint		framePos(0, 0);
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
		mLastCaretFrame = nsnull;			// frames are not refcounted.
		mLastContentOffset = 0;
	}
	
	mDrawn = PR_FALSE;		// assume that the view has been cleared, and ensure
												// that we don't try to use the frame.
	
	// should we just call StopBlinking() here?
	
	return NS_OK;	
}


#ifdef XP_MAC
#pragma mark -
#endif

//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::NotifySelectionChanged(nsIDOMDocument *, nsIDOMSelection *, short)
{

	if (mVisible)
	{
		StopBlinking();
		StartBlinking();
	}
	
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
		NS_RELEASE(mBlinkTimer);
	}
}


//-----------------------------------------------------------------------------
nsresult nsCaret::PrimeTimer()
{
	KillTimer();
	
	// set up the blink timer
	if (!mReadOnly && mBlinkRate > 0)
	{
		nsresult	err = NS_NewTimer(&mBlinkTimer);
		
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
	DrawCaret();		// draw it right away
	
	return NS_OK;
}


//-----------------------------------------------------------------------------
nsresult nsCaret::StopBlinking()
{
	if (mDrawn)			// erase the caret if necessary
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
	nsCOMPtr<nsIDOMSelection> domSelection;
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mPresShell);
  nsresult err;
  if (selCon)
  {
	  err = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSelection));
	  if (!NS_SUCCEEDED(err) || !domSelection)
	  	return PR_FALSE;
  }
  else
    return NS_ERROR_FAILURE;
	PRBool isCollapsed;

	if (domSelection && NS_SUCCEEDED(domSelection->GetIsCollapsed(&isCollapsed)) && isCollapsed)
	{
		// start and end parent should be the same since we are collapsed
		nsCOMPtr<nsIDOMNode>	focusNode;
		PRInt32	contentOffset;
		
		if (NS_SUCCEEDED(domSelection->GetFocusNode(getter_AddRefs(focusNode))) && focusNode &&
				NS_SUCCEEDED(domSelection->GetFocusOffset(&contentOffset)))
		{
			nsCOMPtr<nsIContent>contentNode = do_QueryInterface(focusNode);
      
			if (contentNode)
			{
		      // see if we have an offset between child nodes, or an offset into a text
		      // node.
#if NOT_NEEDED
				PRBool  canContainChildren;
				if (NS_SUCCEEDED(contentNode->CanContainChildren(canContainChildren)) && canContainChildren)
				{
					// point the caret to the start of the child node
					nsCOMPtr<nsIContent> childNode;
					contentNode->ChildAt(contentOffset, *getter_AddRefs(childNode));
					if (childNode)
					{
						contentNode = childNode;
						contentOffset = 0;
					}
				}
				else
				{
					// are we in a text node?
					//nsCOMPtr<nsIDOMCharacterData>	nodeAsText = do_QueryInterface(focusNode);

					// we can be in a text node, or a BR node here.
				}
#endif // NOT_NEEDED
				nsIFrame*	theFrame = nsnull;
				PRInt32         theFrameOffset = 0;

				//get frame selection and find out what frame to use...
				nsCOMPtr<nsIFrameSelection> frameSelection;
        nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
        if (presShell)
				  err = presShell->GetFrameSelection(getter_AddRefs(frameSelection));
        else
          return NS_ERROR_FAILURE;
				if (NS_FAILED(err) || !frameSelection)
					return PR_FALSE;
				
				err = frameSelection->GetFrameForNodeOffset(contentNode, contentOffset, &theFrame, &theFrameOffset);
				if (NS_FAILED(err))
					return PR_FALSE;
				else
				{

					// mark the frame, so we get notified on deletion.
					// frames are never unmarked, which means that we'll touch every frame we visit.
					// this is not ideal.
					nsFrameState state;
					theFrame->GetFrameState(&state);
					state |= NS_FRAME_EXTERNAL_REFERENCE;
					theFrame->SetFrameState(state);
					
					mLastCaretFrame = theFrame;
					mLastContentOffset = theFrameOffset;
					return PR_TRUE;
				}
			}
		}
	}

	return PR_FALSE;
}


//-----------------------------------------------------------------------------
void nsCaret::GetViewForRendering(nsIFrame *caretFrame, EViewCoordinates coordType, nsPoint &viewOffset, nsIView* &outView)
{
  if (!caretFrame) return;
	outView = nsnull;
	
	NS_ASSERTION(caretFrame, "Should have a frame here");	
	if (!caretFrame)
		return;
		
	nsIView* theView = nsnull;
	NS_ASSERTION(caretFrame, "Should have frame here");
  nsCOMPtr<nsIPresContext>  presContext;
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (presShell)
    presShell->GetPresContext(getter_AddRefs(presContext));
  else
    return;
	caretFrame->GetOffsetFromView(presContext, viewOffset, &theView);
	if (theView == nsnull) return;
	
	nsIView* returnView = nsnull;
	
	nscoord		x, y;
	
	do {
		theView->GetPosition(&x, &y);
		viewOffset.x += x;
		viewOffset.y += y;

		if (!returnView)
		{
			nsCOMPtr<nsIWidget>	viewWidget;
			theView->GetWidget(*getter_AddRefs(viewWidget));
			
			if (viewWidget)
			{
				returnView = theView;
			
				if (coordType == eViewCoordinates)
					break;
			}
		}		
		
		theView->GetParent(theView);
	} while (theView);
		
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
		
	nsCOMPtr<nsIDOMSelection> domSelection;
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mPresShell);
  if (selCon)
  {
	  nsresult err = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSelection));
	  if (NS_FAILED(err) || !domSelection)
	  	return PR_FALSE;
  }
  else
    return NS_ERROR_FAILURE;
	PRBool isCollapsed;

	if (NS_FAILED(domSelection->GetIsCollapsed(&isCollapsed)))
		return PR_FALSE;
		
	return isCollapsed;
}


/*-----------------------------------------------------------------------------

	DrawCaretWithContext
	
	By this point, the caret rect should have been set up.
	
----------------------------------------------------------------------------- */

void nsCaret::DrawCaretWithContext(nsIRenderingContext* inRendContext)
{
	
	NS_ASSERTION(mLastCaretFrame != nsnull, "Should have a frame here");
	
	nsRect		frameRect;
	mLastCaretFrame->GetRect(frameRect);
	frameRect.x = 0;			// the origin is accounted for in GetViewForRendering()
	frameRect.y = 0;
	
	if (frameRect.height == 0)		// we're in a BR frame which has zero height.
	{
		frameRect.height = 200;
		frameRect.y -= 200;
	}
	
	nsPoint		viewOffset(0, 0);
	nsIView		*drawingView;
	GetViewForRendering(mLastCaretFrame, eViewCoordinates, viewOffset, drawingView);

	if (drawingView == nsnull)
		return;
	
	frameRect += viewOffset;

	nsCOMPtr<nsIPresContext> presContext;
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (presShell)
  {
    if (NS_FAILED(presShell->GetPresContext(getter_AddRefs(presContext))))
		  return;
  }
  else
    return;

	// make a rendering context, if we didn't get passed one
	nsCOMPtr<nsIRenderingContext>	localRC = do_QueryInterface(inRendContext);		// OK if inRendContext is null
	if (!localRC)
	{
		nsCOMPtr<nsIDeviceContext> 		dx;

		if (NS_FAILED(presContext->GetDeviceContext(getter_AddRefs(dx))) || !dx)
			return;
			
		if (NS_FAILED(dx->CreateRenderingContext(drawingView, *getter_AddRefs(localRC))) || !localRC)
			return;
	}
	
	if (!mDrawn)
	{
		nsPoint		framePos(0, 0);
		nsRect		caretRect = frameRect;
		
		mLastCaretFrame->GetPointFromOffset(presContext, localRC, mLastContentOffset, &framePos);
		caretRect += framePos;

		
		//printf("Content offset %ld, frame offset %ld\n", focusOffset, framePos.x);
		
		caretRect.width = mCaretWidth;

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

		mCaretRect = caretRect;
	}
		/*
		if (mReadOnly)
			inRendContext.SetColor(NS_RGB(85, 85, 85));		// we are drawing it; gray
	  */
	  
	localRC->SetColor(NS_RGB(255,255,255));
	localRC->InvertRect(mCaretRect);
	ToggleDrawnStatus();
}

//-----------------------------------------------------------------------------
void nsCaret::DrawCaret()
{
	// do we need to draw the caret at all?
	if (!MustDrawCaret())
		return;
	
	// if we are drawing, not erasing, then set up the frame etc.
	if (!mDrawn)
	{
		if (! SetupDrawingFrameAndOffset())
			return;
	}
			
	DrawCaretWithContext(nsnull);
}


//-----------------------------------------------------------------------------
void nsCaret::RefreshDrawCaret(nsIView *aView, nsIRenderingContext& inRendContext, const nsRect& aDirtyRect)
{
/*	
	if (! SetupDrawingFrameAndOffset())
		return;

	NS_ASSERTION(mLastCaretFrame != nsnull, "Should have a frame here");
	
	nsPoint		viewOffset(0, 0);
	nsIView		*drawingView;
	//GetViewForRendering(viewOffset, drawingView);

	mLastCaretFrame->GetOffsetFromView(viewOffset, &drawingView);

	// are we in the view that is being painted?
	if (drawingView == nsnull || drawingView != aView)
		return;
	
	mDrawn = PR_FALSE;		// we're rendering to a view that is being redrawn
	DrawCaretWithContext(inRendContext);
*/
}

#ifdef XP_MAC
#pragma mark -
#endif

//-----------------------------------------------------------------------------
/* static */
void nsCaret::CaretBlinkCallback(nsITimer *aTimer, void *aClosure)
{
	nsCaret		*theCaret = NS_REINTERPRET_CAST(nsCaret*, aClosure);
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

