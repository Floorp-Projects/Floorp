/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the caret is the text cursor used, e.g., when editing */

#ifndef nsCaret_h__
#define nsCaret_h__

#include "nsCoord.h"
#include "nsISelectionListener.h"
#include "nsIWeakReferenceUtils.h"
#include "nsFrameSelection.h"

class nsRenderingContext;
class nsDisplayListBuilder;
class nsITimer;

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
     *  @param inMakeVisible true it is shown, false it is hidden
     *  @return false if and only if inMakeVisible is null, otherwise true 
     */
    virtual nsresult    GetCaretVisible(bool *outMakeVisible);

    /** SetCaretVisible will set the visibility of the caret
     *  @param inMakeVisible true to show the caret, false to hide it
     */
    void    SetCaretVisible(bool intMakeVisible);

    /** SetCaretReadOnly set the appearance of the caret
     *  @param inMakeReadonly true to show the caret in a 'read only' state,
     *	    false to show the caret in normal, editing state
     */
    void    SetCaretReadOnly(bool inMakeReadonly);

    /** GetCaretReadOnly get the appearance of the caret
     *	@return true if the caret is in 'read only' state, otherwise,
     *	    returns false
     */
    bool GetCaretReadOnly()
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
                                  nscoord* aBidiIndicatorSize = nullptr);

    /** EraseCaret
     *  this will erase the caret if its drawn and reset drawn status
     */
    void    EraseCaret();

    void    SetVisibilityDuringSelection(bool aVisibility);

    /** DrawAtPosition
     *
     *  Draw the caret explicitly, at the specified node and offset.
     *  To avoid drawing glitches, you should call EraseCaret()
     *  after each call to DrawAtPosition().
     *
     *  Note: This call breaks the caret's ability to blink at all.
     **/
    nsresult    DrawAtPosition(nsIDOMNode* aNode, int32_t aOffset);

    /** GetCaretFrame
     *  Get the current frame that the caret should be drawn in. If the caret is
     *  not currently visible (i.e., it is between blinks), then this will
     *  return null.
     *
     *  @param aOffset is result of the caret offset in the content.
     */
    nsIFrame*     GetCaretFrame(int32_t *aOffset = nullptr);

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
                         nsRenderingContext *aCtx,
                         nsIFrame *aForFrame,
                         const nsPoint &aOffset);
    /**
     * Sets whether the caret should only be visible in nodes that are not
     * user-modify: read-only, or whether it should be visible in all nodes.
     *
     * @param aIgnoreUserModify true to have the cursor visible in all nodes,
     *                          false to have it visible in all nodes except
     *                          those with user-modify: read-only
     */

    void SetIgnoreUserModify(bool aIgnoreUserModify);

    //nsISelectionListener interface
    NS_DECL_NSISELECTIONLISTENER

    static void   CaretBlinkCallback(nsITimer *aTimer, void *aClosure);

    nsresult      GetCaretFrameForNodeOffset(nsIContent* aContentNode,
                                             int32_t aOffset,
                                             nsFrameSelection::HINT aFrameHint,
                                             uint8_t aBidiLevel,
                                             nsIFrame** aReturnFrame,
                                             int32_t* aReturnOffset);

    void CheckCaretDrawingState();

protected:

    void          KillTimer();
    nsresult      PrimeTimer();

    void          StartBlinking();
    void          StopBlinking();

    bool          DrawAtPositionWithHint(nsIDOMNode* aNode,
                                         int32_t aOffset,
                                         nsFrameSelection::HINT aFrameHint,
                                         uint8_t aBidiLevel,
                                         bool aInvalidate);

    struct Metrics {
      nscoord mBidiIndicatorSize; // width and height of bidi indicator
      nscoord mCaretWidth;        // full caret width including bidi indicator
    };
    Metrics ComputeMetrics(nsIFrame* aFrame, int32_t aOffset, nscoord aCaretHeight);
    nsresult GetGeometryForFrame(nsIFrame* aFrame,
                                 int32_t   aFrameOffset,
                                 nsRect*   aRect,
                                 nscoord*  aBidiIndicatorSize);

    // Returns true if the caret should be drawn. When |mDrawn| is true,
    // this returns true, so that we erase the drawn caret. If |aIgnoreDrawnState|
    // is true, we don't take into account whether the caret is currently
    // drawn or not. This can be used to determine if the caret is drawn when
    // it shouldn't be.
    bool          MustDrawCaret(bool aIgnoreDrawnState);

    void          DrawCaret(bool aInvalidate);
    void          DrawCaretAfterBriefDelay();
    bool          UpdateCaretRects(nsIFrame* aFrame, int32_t aFrameOffset);
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

    // Returns true if we should not draw the caret because of XUL menu popups.
    // The caret should be hidden if:
    // 1. An open popup contains the caret, but a menu popup exists before the
    //    caret-owning popup in the popup list (i.e. a menu is in front of the
    //    popup with the caret). If the menu itself contains the caret we don't
    //    hide it.
    // 2. A menu popup is open, but there is no caret present in any popup.
    // 3. The caret selection is empty.
    bool IsMenuPopupHidingCaret();

protected:

    nsWeakPtr             mPresShell;
    nsWeakPtr             mDomSelectionWeak;

    nsCOMPtr<nsITimer>    mBlinkTimer;

    // XXX these fields should go away and the values be acquired as needed,
    // probably by ComputeMetrics.
    uint32_t              mBlinkRate;         // time for one cyle (on then off), in milliseconds
    nscoord               mCaretWidthCSSPx;   // caret width in CSS pixels
    float                 mCaretAspectRatio;  // caret width/height aspect ratio
    
    bool                  mVisible;           // is the caret blinking

    bool                  mDrawn;             // Denotes when the caret is physically drawn on the screen.
    bool                  mPendingDraw;       // True when the last on-state draw was suppressed.

    bool                  mReadOnly;          // it the caret in readonly state (draws differently)      
    bool                  mShowDuringSelection; // show when text is selected

    bool                  mIgnoreUserModify;

#ifdef IBMBIDI
    bool                  mKeyboardRTL;       // is the keyboard language right-to-left
    bool                  mBidiUI;            // is bidi UI turned on
    nsRect                mHookRect;          // directional hook on the caret
    uint8_t               mLastBidiLevel;     // saved bidi level of the last draw request, to use when we erase
#endif
    nsRect                mCaretRect;         // the last caret rect, in the coodinates of the last frame.

    nsCOMPtr<nsIContent>  mLastContent;       // store the content the caret was last requested to be drawn
                                              // in (by DrawAtPosition()/DrawCaret()),
                                              // note that this can be different than where it was
                                              // actually drawn (anon <BR> in text control)
    int32_t               mLastContentOffset; // the offset for the last request

    nsFrameSelection::HINT mLastHint;        // the hint associated with the last request, see also
                                              // mLastBidiLevel below

};

#endif //nsCaret_h__
