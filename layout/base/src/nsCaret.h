
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


#include "nsCoord.h"
#include "nsIDOMSelectionListener.h"
#include "nsICaret.h"

class nsITimer;
class nsCaretProperties;
class nsIView;
class nsIRenderingContext;

// {E14B66F6-BFC5-11d2-B57E-00105AA83B2F}
#define NS_CARET_CID \
{ 0xe14b66f6, 0xbfc5, 0x11d2, { 0xb5, 0x7e, 0x0, 0x10, 0x5a, 0xa8, 0x3b, 0x2f } };

//-----------------------------------------------------------------------------

class nsCaret : public nsICaret,
                public nsIDOMSelectionListener
{
	public:

 	                nsCaret();
		virtual				~nsCaret();
        
  	NS_DECL_ISUPPORTS

	public:
	
	  // nsICaret interface
  	NS_IMETHOD    Init(nsIPresShell *inPresShell, nsCaretProperties *inCaretProperties);

 		NS_IMETHOD    SetCaretVisible(PRBool inMakeVisible);
  	NS_IMETHOD    SetCaretReadOnly(PRBool inMakeReadonly);
  	NS_IMETHOD    Refresh(nsIView *aView, nsIRenderingContext& inRendContext, const nsRect& aDirtyRect);
		NS_IMETHOD 		ClearFrameRefs(nsIFrame* aFrame);
	
	  //nsIDOMSelectionListener interface
	  NS_IMETHOD    NotifySelectionChanged();
	  		               				
		static void		CaretBlinkCallback(nsITimer *aTimer, void *aClosure);
	
	protected:

		void					KillTimer();
		nsresult			PrimeTimer();
		
		nsresult			StartBlinking();
		nsresult			StopBlinking();
		
		void					GetViewForRendering(nsPoint &viewOffset, nsIView* &outView);
		PRBool				SetupDrawingFrameAndOffset();
		void					RefreshDrawCaret(nsIView *aView, nsIRenderingContext& inRendContext, const nsRect& aDirtyRect);
		void 					DrawCaretWithContext(nsIRenderingContext& inRendContext);

		void					DrawCaret();
		void					ToggleDrawnStatus()	{ 	mDrawn = !mDrawn; }

	  nsIPresShell  *mPresShell;				// we rely on the nsEditor to refCount this
		nsITimer*			mBlinkTimer;

		PRUint32			mBlinkRate;					// time for one cyle (off then on), in milliseconds
		nscoord				mCaretWidth;				// caret width in twips
		
		PRBool				mVisible;						// is the caret blinking
		PRBool				mReadOnly;					// it the caret in readonly state (draws differently)
		
	private:
	
		PRBool								mDrawn;							// this should be mutable
		
		nsRect								mCaretRect;					// the last caret rect
		nsIRenderingContext*	mRendContext;				// rendering context. We have to keep this around so that we can
																							// erase the caret without doing all the frame searching again
		nsIFrame*							mLastCaretFrame;		// store the frame the caret was last drawn in.
		PRInt32								mLastContentOffset;
};

