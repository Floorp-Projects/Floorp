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
{
  NS_INIT_REFCNT();
}


//-----------------------------------------------------------------------------
nsCaret::~nsCaret()
{
	KillTimer();
}

//-----------------------------------------------------------------------------
NS_METHOD nsCaret::Init(nsIPresShell *inPresShell, nsCaretProperties *inCaretProperties)
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
nsresult nsCaret::QueryInterface(const nsIID& aIID,
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
NS_METHOD nsCaret::SetCaretVisible(PRBool inMakeVisible)
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
NS_METHOD nsCaret::SetCaretReadOnly(PRBool inMakeReadonly)
{
	mReadOnly = inMakeReadonly;
	return NS_OK;
}


//-----------------------------------------------------------------------------
NS_METHOD nsCaret::Refresh()
{
	// a refresh is different from a simple redraw, in that it does not affect
	// the timer, or toggle the drawn status. It's used to redraw the caret
	// when layout is redrawn on a timer.
	if (mVisible && mDrawn)
	{
		DrawCaret();
	}
	
	return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#endif

//-----------------------------------------------------------------------------
NS_METHOD nsCaret::NotifySelectionChanged()
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
	if (mBlinkRate > 0)
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

  // The following assertion causes a crash if the blink rate is zero:
  //NS_ASSERTION(!mDrawn, "Caret should not be drawn here");
  
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
	
	KillTimer();
	
	return NS_OK;
}


//-----------------------------------------------------------------------------
void nsCaret::DrawCaret()
{
  nsCOMPtr<nsIDOMSelection> domSelection;
  nsresult err = mPresShell->GetSelection(getter_AddRefs(domSelection));
  if (!NS_SUCCEEDED(err) || !domSelection)
  	return;
  	
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
			
			if (nodeAsText)
			{
	      nsCOMPtr<nsIContent>contentNode = do_QueryInterface(focusNode);
	      
				if (contentNode)
				{
					nsIFrame*	theFrame = nsnull;
					PRInt32 	contentOffset = focusOffset;
					
					if (NS_SUCCEEDED(mPresShell->GetPrimaryFrameFor(contentNode, &theFrame)) &&
						 theFrame && NS_SUCCEEDED(theFrame->GetChildFrameContainingOffset(focusOffset, &focusOffset, &theFrame)))
					{
						nsRect		frameRect;
						theFrame->GetRect(frameRect);
            if (0 == frameRect.height){
              frameRect.height = 200;
              frameRect.y += 200;
            }

						nsCOMPtr<nsIPresContext> presContext;
						mPresShell->GetPresContext(getter_AddRefs(presContext));
						
						nsIView * view = nsnull;
						nsPoint   offset;
						theFrame->GetOffsetFromView(offset, &view);
						frameRect.x = offset.x;
						frameRect.y = offset.y;
						
						if (presContext && view)		// when can this fail?
						{
							nsIView*	nextView = view;
							nscoord		x, y;
							
							do {
								nextView->GetPosition(&x, &y);
								frameRect.x += x;
								frameRect.y += y;

								nsCOMPtr<nsIWidget>	viewWidget;
  							nextView->GetWidget(*getter_AddRefs(viewWidget));
								if (viewWidget) break;
								
								nextView->GetParent(nextView);
							} while (nextView);
							
							// make a rendering context for the first view that has a widget
							nsCOMPtr<nsIRenderingContext>	aContext;
							nsCOMPtr<nsIDeviceContext> 		dx;

							if (NS_SUCCEEDED(presContext->GetDeviceContext(getter_AddRefs(dx))) && dx)
							{
								if (nextView)
									dx->CreateRenderingContext(nextView, *getter_AddRefs(aContext));
								else
									dx->CreateRenderingContext(*getter_AddRefs(aContext));
							}

							if (aContext)
							{
								nsPoint		framePos(0, 0);
								
								theFrame->GetPointFromOffset(presContext, aContext, contentOffset, &framePos);
								
								//printf("Content offset %ld, frame offset %ld\n", focusOffset, framePos.x);
								
								frameRect.x += framePos.x;
								frameRect.y += framePos.y;

								frameRect.width = mCaretWidth;
								
								// XXX need to use XOR mode when GFX supports that. For now, just
								// draw and erase
								if (mDrawn)
									aContext->SetColor(NS_RGB(0, 0, 0));				// we are drawing it; black
								else
									aContext->SetColor(NS_RGB(255, 255, 255));	// we are erasing it; white

								aContext->FillRect(frameRect);
							}
						}
					}
	      }
      }
		}
  }

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

