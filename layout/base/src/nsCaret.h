
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
#include "nsIDOMSelectionListener.h"
#include "nsICaret.h"
#include "nsWeakPtr.h"

class nsITimer;
class nsIView;
class nsIRenderingContext;
class nsISelectionController;

// {E14B66F6-BFC5-11d2-B57E-00105AA83B2F}
#define NS_CARET_CID \
{ 0xe14b66f6, 0xbfc5, 0x11d2, { 0xb5, 0x7e, 0x0, 0x10, 0x5a, 0xa8, 0x3b, 0x2f } };

//-----------------------------------------------------------------------------

class nsCaret : public nsICaret,
                public nsIDOMSelectionListener
{
  public:

                  nsCaret();
    virtual       ~nsCaret();
        
    NS_DECL_ISUPPORTS

  public:
  
    // nsICaret interface
    NS_IMETHOD    Init(nsIPresShell *inPresShell);

    NS_IMETHOD    SetCaretDOMSelection(nsIDOMSelection *inDOMSel);
    NS_IMETHOD    GetCaretVisible(PRBool *outMakeVisible);
    NS_IMETHOD    SetCaretVisible(PRBool intMakeVisible);
    NS_IMETHOD    SetCaretReadOnly(PRBool inMakeReadonly);
    NS_IMETHOD    GetWindowRelativeCoordinates(nsRect& outCoordinates, PRBool& outIsCollapsed, nsIDOMSelection *inDOMSel);
    NS_IMETHOD    ClearFrameRefs(nsIFrame* aFrame);
    NS_IMETHOD    EraseCaret();
    NS_IMETHOD    SetCaretWidth(nscoord aPixels);
  
    //nsIDOMSelectionListener interface
    NS_IMETHOD    NotifySelectionChanged(nsIDOMDocument *aDoc, nsIDOMSelection *aSel, short aReason);
                              
    static void   CaretBlinkCallback(nsITimer *aTimer, void *aClosure);
  
  protected:

    void          KillTimer();
    nsresult      PrimeTimer();
    
    nsresult      StartBlinking();
    nsresult      StopBlinking();
    
    enum EViewCoordinates {
      eTopLevelWindowCoordinates,
      eViewCoordinates
    };
    
    void          GetViewForRendering(nsIFrame *caretFrame, EViewCoordinates coordType, nsPoint &viewOffset, nsRect& outClipRect, nsIView* &outView);
    PRBool        SetupDrawingFrameAndOffset();
    PRBool        MustDrawCaret();
    void          RefreshDrawCaret(nsIView *aView, nsIRenderingContext& inRendContext, const nsRect& aDirtyRect);
    void          DrawCaretWithContext(nsIRenderingContext* inRendContext);

    void          DrawCaret();
    void          ToggleDrawnStatus() {   mDrawn = !mDrawn; }

    nsCOMPtr<nsIWeakReference> mPresShell;

    nsCOMPtr<nsITimer> mBlinkTimer;

    PRUint32      mBlinkRate;         // time for one cyle (off then on), in milliseconds
    nscoord       mCaretTwipsWidth;   // caret width in twips
    nscoord       mCaretPixelsWidth;  // caret width in pixels
    
    PRBool        mVisible;           // is the caret blinking
    PRBool        mReadOnly;          // it the caret in readonly state (draws differently)
    
  private:
  
    PRBool                mDrawn;             // this should be mutable
    
    nsRect                mCaretRect;         // the last caret rect
    nsIFrame*             mLastCaretFrame;    // store the frame the caret was last drawn in.
    PRInt32               mLastContentOffset;
    nsWeakPtr mDomSelectionWeak;
};

