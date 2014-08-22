/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the caret is the text cursor used, e.g., when editing */

#ifndef nsCaret_h__
#define nsCaret_h__

#include "mozilla/MemoryReporting.h"
#include "nsCoord.h"
#include "nsISelectionListener.h"
#include "nsIWeakReferenceUtils.h"
#include "CaretAssociationHint.h"
#include "nsPoint.h"
#include "nsRect.h"

class nsDisplayListBuilder;
class nsFrameSelection;
class nsIContent;
class nsIDOMNode;
class nsIFrame;
class nsINode;
class nsIPresShell;
class nsITimer;
class nsRenderingContext;

namespace mozilla {
namespace dom {
class Selection;
}
}

//-----------------------------------------------------------------------------
class nsCaret MOZ_FINAL : public nsISelectionListener
{
  public:
    nsCaret();

  protected:
    virtual ~nsCaret();

  public:
    NS_DECL_ISUPPORTS

    typedef mozilla::CaretAssociationHint CaretAssociationHint;

    nsresult Init(nsIPresShell *inPresShell);
    void Terminate();

    void SetSelection(nsISelection *aDOMSel);
    nsISelection* GetSelection();

    /**
     * Sets whether the caret should only be visible in nodes that are not
     * user-modify: read-only, or whether it should be visible in all nodes.
     *
     * @param aIgnoreUserModify true to have the cursor visible in all nodes,
     *                          false to have it visible in all nodes except
     *                          those with user-modify: read-only
     */
    void SetIgnoreUserModify(bool aIgnoreUserModify);
    /** SetVisible will set the visibility of the caret
     *  @param inMakeVisible true to show the caret, false to hide it
     */
    void SetVisible(bool intMakeVisible);
    /** IsVisible will get the visibility of the caret.
     *  This returns false if the caret is hidden because it was set
     *  to not be visible, or because the selection is not collapsed, or
     *  because an open popup is hiding the caret.
     *  It does not take account of blinking or the caret being hidden
     *  because we're in non-editable/disabled content.
     */
    bool IsVisible();
    /** SetCaretReadOnly set the appearance of the caret
     *  @param inMakeReadonly true to show the caret in a 'read only' state,
     *         false to show the caret in normal, editing state
     */
    void SetCaretReadOnly(bool inMakeReadonly);
    /**
     * @param aVisibility true if the caret should be visible even when the
     * selection is not collapsed.
     */
    void SetVisibilityDuringSelection(bool aVisibility);

    /**
     * Set the caret's position explicitly to the specified node and offset
     * instead of tracking its selection.
     * Passing null for aNode would set the caret to track its selection again.
     **/
    void SetCaretPosition(nsIDOMNode* aNode, int32_t aOffset);

    /**
     * Schedule a repaint for the frame where the caret would appear.
     * Does not check visibility etc.
     */
    void SchedulePaint();

    /**
     * Returns a frame to paint in, and the bounds of the painted caret
     * relative to that frame.
     * The rectangle includes bidi decorations.
     * Returns null if the caret should not be drawn (including if it's blinked
     * off).
     */
    nsIFrame* GetPaintGeometry(nsRect* aRect);
    /**
     * A simple wrapper around GetGeometry. Does not take any caret state into
     * account other than the current selection.
     */
    nsIFrame* GetGeometry(nsRect* aRect)
    {
      return GetGeometry(GetSelection(), aRect);
    }

    /** PaintCaret
     *  Actually paint the caret onto the given rendering context.
     */
    void PaintCaret(nsDisplayListBuilder *aBuilder,
                    nsRenderingContext *aCtx,
                    nsIFrame *aForFrame,
                    const nsPoint &aOffset);

    //nsISelectionListener interface
    NS_DECL_NSISELECTIONLISTENER

    /**
     * Gets the position and size of the caret that would be drawn for
     * the focus node/offset of aSelection (assuming it would be drawn,
     * i.e., disregarding blink status). The geometry is stored in aRect,
     * and we return the frame aRect is relative to.
     * Only looks at the focus node of aSelection, so you can call it even if
     * aSelection is not collapsed.
     * This rect does not include any extra decorations for bidi.
     * @param aRect must be non-null
     */
    static nsIFrame* GetGeometry(nsISelection* aSelection,
                                 nsRect* aRect);
    static nsresult GetCaretFrameForNodeOffset(nsFrameSelection* aFrameSelection,
                                               nsIContent* aContentNode,
                                               int32_t aOffset,
                                               CaretAssociationHint aFrameHint,
                                               uint8_t aBidiLevel,
                                               nsIFrame** aReturnFrame,
                                               int32_t* aReturnOffset);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

protected:
    static void   CaretBlinkCallback(nsITimer *aTimer, void *aClosure);

    void          CheckSelectionLanguageChange();

    void          ResetBlinking();
    void          StopBlinking();

    mozilla::dom::Selection* GetSelectionInternal();

    struct Metrics {
      nscoord mBidiIndicatorSize; // width and height of bidi indicator
      nscoord mCaretWidth;        // full caret width including bidi indicator
    };
    static Metrics ComputeMetrics(nsIFrame* aFrame, int32_t aOffset,
                                  nscoord aCaretHeight);
    static nsRect GetGeometryForFrame(nsIFrame* aFrame,
                                      int32_t   aFrameOffset,
                                      nscoord*  aBidiIndicatorSize);

    void          ComputeCaretRects(nsIFrame* aFrame, int32_t aFrameOffset,
                                    nsRect* aCaretRect, nsRect* aHookRect);

    // Returns true if we should not draw the caret because of XUL menu popups.
    // The caret should be hidden if:
    // 1. An open popup contains the caret, but a menu popup exists before the
    //    caret-owning popup in the popup list (i.e. a menu is in front of the
    //    popup with the caret). If the menu itself contains the caret we don't
    //    hide it.
    // 2. A menu popup is open, but there is no caret present in any popup.
    // 3. The caret selection is empty.
    bool IsMenuPopupHidingCaret();

    nsWeakPtr             mPresShell;
    nsWeakPtr             mDomSelectionWeak;

    nsCOMPtr<nsITimer>    mBlinkTimer;

    /**
     * The content to draw the caret at. If null, we use mDomSelectionWeak's
     * focus node instead.
     */
    nsCOMPtr<nsINode>     mOverrideContent;
    /**
     * The character offset to draw the caret at.
     * Ignored if mOverrideContent is null.
     */
    int32_t               mOverrideOffset;

    /**
     * mIsBlinkOn is true when we're in a blink cycle where the caret is on.
     */
    bool                  mIsBlinkOn;
    /**
     * mIsVisible is true when SetVisible was last called with 'true'.
     */
    bool                  mVisible;
    /**
     * mReadOnly is true when the caret is set to "read only" mode (i.e.,
     * it doesn't blink).
     */
    bool                  mReadOnly;
    /**
     * mShowDuringSelection is true when the caret should be shown even when
     * the selection is not collapsed.
     */
    bool                  mShowDuringSelection;
    /**
     * mIgnoreUserModify is true when the caret should be shown even when
     * it's in non-user-modifiable content.
     */
    bool                  mIgnoreUserModify;
};

#endif //nsCaret_h__
