
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
    nscoord               mBidiIndicatorTwipsSize;   // width and height of bidi indicator
    static const kMinBidiIndicatorPixels = 2;

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

