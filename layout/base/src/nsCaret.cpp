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

#include "nsITimer.h"
#include "nsITimerCallback.h"

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

#include "nsCaretProperties.h"

#include "nsCaret.h"


static NS_DEFINE_IID(kISupportsIID, 		NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMSelectionIID, NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kICaretID, NS_ICARET_IID);

//-----------------------------------------------------------------------------
nsCaret::nsCaret()
:	mPresShell(nsnull)
,	mBlinkTimer(nsnull)
,	mBlinkRate(500)
,	mVisible(PR_FALSE)
,	mReadOnly(PR_TRUE)
,	mDrawn(PR_FALSE)
, mRendContext(nsnull)
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
NS_IMETHODIMP nsCaret::Init(nsIPresShell *inPresShell, nsCaretProperties *inCaretProperties)
{
	if (!inPresShell)
		return NS_ERROR_NULL_POINTER;

	if (!inCaretProperties)
		return NS_ERROR_NULL_POINTER;
	
	mPresShell = inPresShell;		// the presshell owns us, so no addref
	
	mBlinkRate = inCaretProperties->GetCaretBlinkRate();
	mCaretWidth = inCaretProperties->GetCaretWidth();
	
	// get the selection from the pres shell, and set ourselves up as a selection
	// listener
	
  nsCOMPtr<nsIDOMSelection> domSelection;
  if (NS_SUCCEEDED(mPresShell->GetSelection(getter_AddRefs(domSelection))))
  {
		domSelection->AddSelectionListener(this);
	}
	
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
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsICaret*)this;		// whoo boy
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsICaret::GetIID())) {
    *aInstancePtrResult = (void*)(nsICaret*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMSelectionListener::GetIID())) {
    *aInstancePtrResult = (void*)(nsIDOMSelectionListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return !NS_OK;
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
NS_IMETHODIMP nsCaret::Refresh(nsIView *aView, nsIRenderingContext& inRendContext, const nsRect& aDirtyRect)
{
	// a refresh is different from a simple redraw, in that it does not affect
	// the timer, or toggle the drawn status. It's used to redraw the caret
	// when painting happens, beacuse this will have drawn over the caret.
	if (mVisible && mDrawn)
	{
		RefreshDrawCaret(aView, inRendContext, aDirtyRect);
	}
	
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
	
	// blow away the rendering context, because the frames may well get
	// reflowed before we draw again
	NS_IF_RELEASE(mRendContext);		// this nulls the ptr

	// should we just call StopBlinking() here?
	
	return NS_OK;	
}


#ifdef XP_MAC
#pragma mark -
#endif

//-----------------------------------------------------------------------------
NS_IMETHODIMP nsCaret::NotifySelectionChanged()
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
		
		mBlinkTimer->Init(CaretBlinkCallback, this, mBlinkRate);
	}

	return NS_OK;
}


//-----------------------------------------------------------------------------
nsresult nsCaret::StartBlinking()
{
	PrimeTimer();

	ToggleDrawnStatus();
	DrawCaret();		// draw it right away
	
	return NS_OK;
}


//-----------------------------------------------------------------------------
nsresult nsCaret::StopBlinking()
{
	if (mDrawn)			// erase the caret if necessary
	{
		ToggleDrawnStatus();
		DrawCaret();
	}
	
	// blow away the rendering context, because the frames may well get
	// reflowed before we draw again
	NS_IF_RELEASE(mRendContext);		// this nulls the ptr
	
	KillTimer();
	
	return NS_OK;
}


//-----------------------------------------------------------------------------
// Get the nsIFrame and the content offset for the current caret position.
// Returns PR_TRUE if we should go ahead and draw, PR_FALSE otherwise.
//
PRBool nsCaret::SetupDrawingFrameAndOffset()
{
	if (!mDrawn)
	{
		return (mLastCaretFrame != nsnull);		// if we are going to erase, we just need to have the old frame
	}
	
	nsCOMPtr<nsIDOMSelection> domSelection;
	nsresult err = mPresShell->GetSelection(getter_AddRefs(domSelection));
	if (!NS_SUCCEEDED(err) || !domSelection)
		return PR_FALSE;

	PRBool isCollapsed;

	if (domSelection && NS_SUCCEEDED(domSelection->IsCollapsed(&isCollapsed)) && isCollapsed)
	{
		// start and end parent should be the same since we are collapsed
		nsCOMPtr<nsIDOMNode>	focusNode;
		PRInt32	focusOffset;
		
		if (NS_SUCCEEDED(domSelection->GetFocusNodeAndOffset(getter_AddRefs(focusNode), &focusOffset)) &&
						focusNode)
		{
			// is this a text node?
			nsCOMPtr<nsIDOMCharacterData>	nodeAsText = do_QueryInterface(focusNode);
			
			if (PR_TRUE || nodeAsText)
			{
				nsCOMPtr<nsIContent>contentNode = do_QueryInterface(focusNode);
	      
				if (contentNode)
				{
					nsIFrame*	theFrame = nsnull;
					PRInt32 	contentOffset = focusOffset;
					
					if (NS_SUCCEEDED(mPresShell->GetPrimaryFrameFor(contentNode, &theFrame)) &&
						 theFrame && NS_SUCCEEDED(theFrame->GetChildFrameContainingOffset(focusOffset, &focusOffset, &theFrame)))
					{

						// mark the frame, so we get notified on deletion.
						// frames are never unmarked, which means that we'll touch every frame we visit.
						// this is not ideal.
						nsFrameState state;
						theFrame->GetFrameState(&state);
						state |= NS_FRAME_EXTERNAL_REFERENCE;
						theFrame->SetFrameState(state);
						
						mLastCaretFrame = theFrame;
						mLastContentOffset = contentOffset;
						return PR_TRUE;
					}
				}
			}
		}
	}

	return PR_FALSE;
}


//-----------------------------------------------------------------------------
void nsCaret::GetViewForRendering(nsPoint &viewOffset, nsIView* &outView)
{
	outView = nsnull;
	
	nsIView* theView = nsnull;
	mLastCaretFrame->GetOffsetFromView(viewOffset, &theView);
	if (theView == nsnull) return;
	
	nscoord		x, y;
	
	do {
		theView->GetPosition(&x, &y);
		viewOffset.x += x;
		viewOffset.y += y;

		nsCOMPtr<nsIWidget>	viewWidget;
		theView->GetWidget(*getter_AddRefs(viewWidget));
		if (viewWidget) break;
		
		theView->GetParent(theView);
	} while (theView);
		
	outView = theView;
}


//-----------------------------------------------------------------------------
void nsCaret::DrawCaretWithContext(nsIRenderingContext& inRendContext)
{
	
	NS_ASSERTION(mLastCaretFrame != nsnull, "Should have a frame here");
	
	nsRect		frameRect;
	mLastCaretFrame->GetRect(frameRect);
	frameRect.x = 0;			// the origin is accounted for in GetViewForRendering()
	frameRect.y = 0;
	
	if (0 == frameRect.height)
	{
	  frameRect.height = 200;
	  frameRect.y += 200;
	}
	
	nsPoint		viewOffset(0, 0);
	nsIView		*drawingView;
	GetViewForRendering(viewOffset, drawingView);

	if (drawingView == nsnull)
		return;
	
	frameRect += viewOffset;

	inRendContext.PushState();
	
	nsCOMPtr<nsIPresContext> presContext;
	if (NS_SUCCEEDED(mPresShell->GetPresContext(getter_AddRefs(presContext))))
	{
		nsPoint		framePos(0, 0);
		
		mLastCaretFrame->GetPointFromOffset(presContext, &inRendContext, mLastContentOffset, &framePos);
		frameRect += framePos;
		
		//printf("Content offset %ld, frame offset %ld\n", focusOffset, framePos.x);
		
		frameRect.width = mCaretWidth;
		mCaretRect = frameRect;
		
		if (mDrawn)
		{
		if (mReadOnly)
			inRendContext.SetColor(NS_RGB(85, 85, 85));		// we are drawing it; gray
		else
			inRendContext.SetColor(NS_RGB(0, 0, 0));			// we are drawing it; black
		}
		else
		inRendContext.SetColor(NS_RGB(255, 255, 255));	// we are erasing it; white

		inRendContext.FillRect(mCaretRect);
	}
	
	PRBool dummy;
	inRendContext.PopState(dummy);
}

//-----------------------------------------------------------------------------
void nsCaret::DrawCaret()
{
	if (! SetupDrawingFrameAndOffset())
		return;
	
	NS_ASSERTION(mLastCaretFrame != nsnull, "Should have a frame here");
	
	nsPoint		viewOffset(0, 0);
	nsIView		*drawingView;
	GetViewForRendering(viewOffset, drawingView);
	
	if (drawingView)
	{
		// make a rendering context for the first view that has a widget
		if (!mRendContext)
		{
			nsCOMPtr<nsIPresContext> presContext;
			if (NS_SUCCEEDED(mPresShell->GetPresContext(getter_AddRefs(presContext))))
			{
				nsCOMPtr<nsIDeviceContext> 		dx;

				if (NS_SUCCEEDED(presContext->GetDeviceContext(getter_AddRefs(dx))) && dx)
				{
					dx->CreateRenderingContext(drawingView, mRendContext);						
					if (!mRendContext) return;
				}
			}

			mRendContext->PushState();		// save the state. We'll pop then push on drawing
		}
		
		DrawCaretWithContext(*mRendContext);
		NS_RELEASE(mRendContext);
	}
	
}


//-----------------------------------------------------------------------------
void nsCaret::RefreshDrawCaret(nsIView *aView, nsIRenderingContext& inRendContext, const nsRect& aDirtyRect)
{
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
		
	DrawCaretWithContext(inRendContext);
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
	
	theCaret->ToggleDrawnStatus();
	theCaret->DrawCaret();
	theCaret->PrimeTimer();
}


//-----------------------------------------------------------------------------
nsresult NS_NewCaret(nsICaret** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  
  nsCaret* caret = new nsCaret();
  if (nsnull == caret)
      return NS_ERROR_OUT_OF_MEMORY;
      
  return caret->QueryInterface(kICaretID, (void**) aInstancePtrResult);
}

