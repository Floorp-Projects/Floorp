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

#ifndef nsCaret_h__
#define nsCaret_h__

#include "nsCoord.h"
#include "nsISelectionListener.h"
#include "nsITimer.h"
#include "nsWeakPtr.h"
#include "nsFrameSelection.h"

class nsIRenderingContext;
class nsDisplayListBuilder;

//-----------------------------------------------------------------------------
class nsCaret : public nsISelectionListener
{
  public:

                  nsCaret();
    virtual       ~nsCaret();

    enum EViewCoordinates {
      eTopLevelWindowCoordinates,
      eRenderingViewCoordinates,
      eClosestViewCoordinates
    };

  public:

    NS_DECL_ISUPPORTS

    nsresult    Init(nsIPresShell *inPresShell);
    void    Terminate();

    nsISelection*    GetCaretDOMSelection();
    nsresult    SetCaretDOMSelection(nsISelection *inDOMSel);

    /** GetCaretVisible will get the visibility of the caret
     *  This function is virtual so that it can be used by nsCaretAccessible
     *  without linking
     *  @param inMakeVisible PR_TRUE it is shown, PR_FALSE it is hidden
     *  @return false if and only if inMakeVisible is null, otherwise true 
     */
    virtual nsresult    GetCaretVisible(PRBool *outMakeVisible);

    /** SetCaretVisible will set the visibility of the caret
     *  @param inMakeVisible PR_TRUE to show the caret, PR_FALSE to hide it
     */
    void    SetCaretVisible(PRBool intMakeVisible);

    /** SetCaretReadOnly set the appearance of the caret
     *  @param inMakeReadonly PR_TRUE to show the caret in a 'read only' state,
     *	    PR_FALSE to show the caret in normal, editing state
     */
    void    SetCaretReadOnly(PRBool inMakeReadonly);

    /** GetCaretReadOnly get the appearance of the caret
     *	@return PR_TRUE if the caret is in 'read only' state, otherwise,
     *	    returns PR_FALSE
     */
    PRBool GetCaretReadOnly()
    {
      return mReadOnly;
    }

    /**
     * Gets the position and size of the caret that would be drawn for
     * the focus node/offset of aSelection (assuming it would be drawn,
     * i.e., disregarding blink status). The geometry is stored in aRect,
     * and we return the frame aRect is relative to.
     * @param aRect must be non-null
     * @param aBidiIndicatorSize if non-null, set to the bidi indicator size.
     */
    virtual nsIFrame* GetGeometry(nsISelection* aSelection,
                                  nsRect* aRect,
                                  nscoord* aBidiIndicatorSize = nsnull);

    /** EraseCaret
     *  this will erase the caret if its drawn and reset drawn status
     */
    void    EraseCaret();

    void    SetVisibilityDuringSelection(PRBool aVisibility);

    /** DrawAtPosition
     *
     *  Draw the caret explicitly, at the specified node and offset.
     *  To avoid drawing glitches, you should call EraseCaret()
     *  after each call to DrawAtPosition().
     *
     *  Note: This call breaks the caret's ability to blink at all.
     **/
    nsresult    DrawAtPosition(nsIDOMNode* aNode, PRInt32 aOffset);

    /** GetCaretFrame
     *  Get the current frame that the caret should be drawn in. If the caret is
     *  not currently visible (i.e., it is between blinks), then this will
     *  return null.
     *
     *  @param aOffset is result of the caret offset in the content.
     */
    nsIFrame*     GetCaretFrame(PRInt32 *aOffset = nsnull);

    /** GetCaretRect
     *  Get the current caret rect. Only call this when GetCaretFrame returns
     *  non-null.
     */
    nsRect        GetCaretRect()
    {
      nsRect r;
      r.UnionRect(mCaretRect, GetHookRect());
      return r;
    }

    /** InvalidateOutsideCaret
     *  Invalidate the area that the caret currently occupies if the caret is
     *  outside of its frame's overflow area. This is used when the content that
     *  the caret is currently drawn is is being deleted or reflowed.
     */
    void      InvalidateOutsideCaret();

    /** UpdateCaretPosition
     *  Update the caret's current frame and rect, but don't draw yet. This is
     *  useful for flickerless moving of the caret (e.g., when the frame the
     *  caret is in reflows and is moved).
     */
    void      UpdateCaretPosition();

    /** PaintCaret
     *  Actually paint the caret onto the given rendering context.
     */
    void      PaintCaret(nsDisplayListBuilder *aBuilder,
                         nsIRenderingContext *aCtx,
                         nsIFrame *aForFrame,
                         const nsPoint &aOffset);
    /**
     * Sets whether the caret should only be visible in nodes that are not
     * user-modify: read-only, or whether it should be visible in all nodes.
     *
     * @param aIgnoreUserModify PR_TRUE to have the cursor visible in all nodes,
     *                          PR_FALSE to have it visible in all nodes except
     *                          those with user-modify: read-only
     */

    void SetIgnoreUserModify(PRBool aIgnoreUserModify);

    //nsISelectionListener interface
    NS_DECL_NSISELECTIONLISTENER

    static void   CaretBlinkCallback(nsITimer *aTimer, void *aClosure);

    nsresult      GetCaretFrameForNodeOffset(nsIContent* aContentNode,
                                             PRInt32 aOffset,
                                             nsFrameSelection::HINT aFrameHint,
                                             PRUint8 aBidiLevel,
                                             nsIFrame** aReturnFrame,
                                             PRInt32* aReturnOffset);

    NS_IMETHOD CheckCaretDrawingState();

protected:

    void          KillTimer();
    nsresult      PrimeTimer();

    void          StartBlinking();
    void          StopBlinking();
    
    PRBool        DrawAtPositionWithHint(nsIDOMNode* aNode,
                                         PRInt32 aOffset,
                                         nsFrameSelection::HINT aFrameHint,
                                         PRUint8 aBidiLevel,
                                         PRBool aInvalidate);

    struct Metrics {
      nscoord mBidiIndicatorSize; // width and height of bidi indicator
      nscoord mCaretWidth;        // full caret width including bidi indicator
    };
    Metrics ComputeMetrics(nsIFrame* aFrame, PRInt32 aOffset, nscoord aCaretHeight);
    nsresult GetGeometryForFrame(nsIFrame* aFrame,
                                 PRInt32   aFrameOffset,
                                 nsRect*   aRect,
                                 nscoord*  aBidiIndicatorSize);

    // Returns true if the caret should be drawn. When |mDrawn| is true,
    // this returns true, so that we erase the drawn caret. If |aIgnoreDrawnState|
    // is true, we don't take into account whether the caret is currently
    // drawn or not. This can be used to determine if the caret is drawn when
    // it shouldn't be.
    PRBool        MustDrawCaret(PRBool aIgnoreDrawnState);

    void          DrawCaret(PRBool aInvalidate);
    void          DrawCaretAfterBriefDelay();
    PRBool        UpdateCaretRects(nsIFrame* aFrame, PRInt32 aFrameOffset);
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

    already_AddRefed<nsFrameSelection> GetFrameSelection();

    // Returns true if we should not draw the caret because of XUL menu popups.
    // The caret should be hidden if:
    // 1. An open popup contains the caret, but a menu popup exists before the
    //    caret-owning popup in the popup list (i.e. a menu is in front of the
    //    popup with the caret). If the menu itself contains the caret we don't
    //    hide it.
    // 2. A menu popup is open, but there is no caret present in any popup.
    // 3. The caret selection is empty.
    PRBool IsMenuPopupHidingCaret();

protected:

    nsWeakPtr             mPresShell;
    nsWeakPtr             mDomSelectionWeak;

    nsCOMPtr<nsITimer>    mBlinkTimer;

    // XXX these fields should go away and the values be acquired as needed,
    // probably by ComputeMetrics.
    PRUint32              mBlinkRate;         // time for one cyle (on then off), in milliseconds
    nscoord               mCaretWidthCSSPx;   // caret width in CSS pixels
    float                 mCaretAspectRatio;  // caret width/height aspect ratio
    
    PRPackedBool          mVisible;           // is the caret blinking

    PRPackedBool          mDrawn;             // Denotes when the caret is physically drawn on the screen.
    PRPackedBool          mPendingDraw;       // True when the last on-state draw was suppressed.

    PRPackedBool          mReadOnly;          // it the caret in readonly state (draws differently)      
    PRPackedBool          mShowDuringSelection; // show when text is selected

    PRPackedBool          mIgnoreUserModify;

#ifdef IBMBIDI
    PRPackedBool          mKeyboardRTL;       // is the keyboard language right-to-left
    PRPackedBool          mBidiUI;            // is bidi UI turned on
    nsRect                mHookRect;          // directional hook on the caret
    PRUint8               mLastBidiLevel;     // saved bidi level of the last draw request, to use when we erase
#endif
    nsRect                mCaretRect;         // the last caret rect, in the coodinates of the last frame.

    nsCOMPtr<nsIContent>  mLastContent;       // store the content the caret was last requested to be drawn
                                              // in (by DrawAtPosition()/DrawCaret()),
                                              // note that this can be different than where it was
                                              // actually drawn (anon <BR> in text control)
    PRInt32               mLastContentOffset; // the offset for the last request

    nsFrameSelection::HINT mLastHint;        // the hint associated with the last request, see also
                                              // mLastBidiLevel below

};

nsresult
NS_NewCaret(nsCaret** aInstancePtrResult);

// handy stack-based class for temporarily disabling the caret

class StCaretHider
{
public:
               StCaretHider(nsCaret* aSelCon)
               : mWasVisible(PR_FALSE), mCaret(aSelCon)
               {
                 if (mCaret)
                 {
                   mCaret->GetCaretVisible(&mWasVisible);
                   if (mWasVisible)
                     mCaret->SetCaretVisible(PR_FALSE);
                 }
               }

               ~StCaretHider()
               {
                 if (mCaret && mWasVisible)
                   mCaret->SetCaretVisible(PR_TRUE);
                 // nsCOMPtr releases mPresShell
               }

protected:

    PRBool                  mWasVisible;
    nsCOMPtr<nsCaret>  mCaret;
};

#endif //nsCaret_h__
