
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
 */


#include "nsCoord.h"
#include "nsISelectionListener.h"
#include "nsIRenderingContext.h"
#include "nsITimer.h"
#include "nsICaret.h"
#include "nsWeakPtr.h"
#ifdef IBMBIDI
#include "nsIBidiKeyboard.h"
#endif

class nsIView;

//-----------------------------------------------------------------------------

class nsCaret : public nsICaret,
                public nsISelectionListener
{
  public:

                  nsCaret();
    virtual       ~nsCaret();
        
    NS_DECL_ISUPPORTS

  public:
  
    // nsICaret interface
    NS_IMETHOD    Init(nsIPresShell *inPresShell);
    NS_IMETHOD    Terminate();

    NS_IMETHOD    GetCaretDOMSelection(nsISelection **outDOMSel);
    NS_IMETHOD    SetCaretDOMSelection(nsISelection *inDOMSel);
    NS_IMETHOD    GetCaretVisible(PRBool *outMakeVisible);
    NS_IMETHOD    SetCaretVisible(PRBool intMakeVisible);
    NS_IMETHOD    SetCaretReadOnly(PRBool inMakeReadonly);
    NS_IMETHOD    GetCaretCoordinates(EViewCoordinates aRelativeToType, nsISelection *inDOMSel, nsRect* outCoordinates, PRBool* outIsCollapsed, nsIView **outView);
    NS_IMETHOD    ClearFrameRefs(nsIFrame* aFrame);
    NS_IMETHOD    EraseCaret();

    NS_IMETHOD    SetCaretWidth(nscoord aPixels);
    NS_IMETHOD    SetVisibilityDuringSelection(PRBool aVisibility);
    NS_IMETHOD    DrawAtPosition(nsIDOMNode* aNode, PRInt32 aOffset);

    //nsISelectionListener interface
    NS_IMETHOD    NotifySelectionChanged(nsIDOMDocument *aDoc, nsISelection *aSel, short aReason);
                              
    static void   CaretBlinkCallback(nsITimer *aTimer, void *aClosure);
  
  protected:

    void          KillTimer();
    nsresult      PrimeTimer();
    
    nsresult      StartBlinking();
    nsresult      StopBlinking();
    
    void          GetViewForRendering(nsIFrame *caretFrame, EViewCoordinates coordType, nsPoint &viewOffset, nsRect& outClipRect, nsIView **outRenderingView, nsIView **outRelativeView);
    PRBool        SetupDrawingFrameAndOffset(nsIDOMNode* aNode, PRInt32 aOffset, nsIFrameSelection::HINT aFrameHint);
    PRBool        MustDrawCaret();
    void          DrawCaret();
    void          GetCaretRectAndInvert();
    void          ToggleDrawnStatus() {   mDrawn = !mDrawn; }

protected:

    nsWeakPtr             mPresShell;
    nsWeakPtr             mDomSelectionWeak;

    nsCOMPtr<nsITimer>              mBlinkTimer;
    nsCOMPtr<nsIRenderingContext>   mRendContext;

    PRUint32              mBlinkRate;         // time for one cyle (off then on), in milliseconds

    nscoord               mCaretTwipsWidth;   // caret width in twips. this gets calculated laziiy
    nscoord               mCaretPixelsWidth;  // caret width in pixels
    
    PRPackedBool          mVisible;           // is the caret blinking
    PRPackedBool          mDrawn;             // this should be mutable
    PRPackedBool          mReadOnly;          // it the caret in readonly state (draws differently)      
    PRPackedBool          mShowDuringSelection; // show when text is selected

    nsRect                mCaretRect;         // the last caret rect
    nsIFrame*             mLastCaretFrame;    // store the frame the caret was last drawn in.
    nsIView*              mLastCaretView;     // last view that we used for drawing. Cached so we can tell when we need to make a new RC
    PRInt32               mLastContentOffset;
#ifdef IBMBIDI
    nsRect                mHookRect;          // directional hook on the caret
    nsCOMPtr<nsIBidiKeyboard> mBidiKeyboard;  // Bidi keyboard object to set and query keyboard language
    PRPackedBool          mKeyboardRTL;       // is the keyboard language right-to-left
#endif
};

