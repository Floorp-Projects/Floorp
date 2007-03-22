/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
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

/* the caret is the text cursor used, e.g., when editing */

#include "nsCoord.h"
#include "nsISelectionListener.h"
#include "nsIRenderingContext.h"
#include "nsITimer.h"
#include "nsICaret.h"
#include "nsWeakPtr.h"

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
    virtual PRBool GetCaretReadOnly()
    {
      return mReadOnly;
    }
    NS_IMETHOD    GetCaretCoordinates(EViewCoordinates aRelativeToType,
                                      nsISelection *inDOMSel,
                                      nsRect* outCoordinates,
                                      PRBool* outIsCollapsed,
                                      nsIView **outView);
    NS_IMETHOD    EraseCaret();

    NS_IMETHOD    SetVisibilityDuringSelection(PRBool aVisibility);
    NS_IMETHOD    DrawAtPosition(nsIDOMNode* aNode, PRInt32 aOffset);
    nsIFrame*     GetCaretFrame();
    nsRect        GetCaretRect()
    {
      nsRect r;
      r.UnionRect(mCaretRect, GetHookRect());
      return r;
    }
    nsIContent*   GetCaretContent()
    {
      if (mDrawn)
        return mLastContent;

      return nsnull;
    }

    void      InvalidateOutsideCaret();
    void      UpdateCaretPosition();

    void      PaintCaret(nsDisplayListBuilder *aBuilder,
                         nsIRenderingContext *aCtx,
                         const nsPoint &aOffset,
                         nscolor aColor);

    //nsISelectionListener interface
    NS_DECL_NSISELECTIONLISTENER

    static void   CaretBlinkCallback(nsITimer *aTimer, void *aClosure);
  
    NS_IMETHOD    GetCaretFrameForNodeOffset(nsIContent* aContentNode,
                                             PRInt32 aOffset,
                                             nsFrameSelection::HINT aFrameHint,
                                             PRUint8 aBidiLevel,
                                             nsIFrame** aReturnFrame,
                                             PRInt32* aReturnOffset);
  protected:

    void          KillTimer();
    nsresult      PrimeTimer();

    nsresult      StartBlinking();
    nsresult      StopBlinking();
    
    void          GetViewForRendering(nsIFrame *caretFrame,
                                      EViewCoordinates coordType,
                                      nsPoint &viewOffset,
                                      nsIView **outRenderingView,
                                      nsIView **outRelativeView);
    PRBool        DrawAtPositionWithHint(nsIDOMNode* aNode,
                                         PRInt32 aOffset,
                                         nsFrameSelection::HINT aFrameHint,
                                         PRUint8 aBidiLevel,
                                         PRBool aInvalidate);
    PRBool        MustDrawCaret();
    void          DrawCaret(PRBool aInvalidate);
    void          DrawCaretAfterBriefDelay();
    nsresult      UpdateCaretRects(nsIFrame* aFrame, PRInt32 aFrameOffset);
    nsresult      UpdateHookRect(nsPresContext* aPresContext);
    static void   InvalidateRects(const nsRect &aRect, const nsRect &aHook,
                                  nsIFrame *aFrame);
    nsRect        GetHookRect()
    {
#ifdef IBMBIDI
      return mHookRect;
#else
      return nsRect();
#endif
    }
    void          ToggleDrawnStatus() { mDrawn = !mDrawn; }

    nsFrameSelection* GetFrameSelection();

protected:

    nsWeakPtr             mPresShell;
    nsWeakPtr             mDomSelectionWeak;

    nsCOMPtr<nsITimer>              mBlinkTimer;
    nsCOMPtr<nsIRenderingContext>   mRendContext;

    PRUint32              mBlinkRate;         // time for one cyle (off then on), in milliseconds

    nscoord               mCaretWidth;   // caret width. this gets calculated laziiy
    nscoord               mBidiIndicatorSize;   // width and height of bidi indicator

    PRPackedBool          mVisible;           // is the caret blinking
    PRPackedBool          mDrawn;             // this should be mutable
    PRPackedBool          mReadOnly;          // it the caret in readonly state (draws differently)      
    PRPackedBool          mShowDuringSelection; // show when text is selected

    nsRect                mCaretRect;         // the last caret rect, in the coodinates of the last frame.

    nsCOMPtr<nsIContent>  mLastContent;       // store the content the caret was last requested to be drawn
                                              // in (by DrawAtPosition()/DrawCaret()),
                                              // note that this can be different than where it was
                                              // actually drawn (anon <BR> in text control)
    PRInt32               mLastContentOffset; // the offset for the last request

    nsFrameSelection::HINT mLastHint;        // the hint associated with the last request, see also
                                              // mLastBidiLevel below

#ifdef IBMBIDI
    nsRect                mHookRect;          // directional hook on the caret
    PRUint8               mLastBidiLevel;     // saved bidi level of the last draw request, to use when we erase
    PRPackedBool          mKeyboardRTL;       // is the keyboard language right-to-left
#endif
};

