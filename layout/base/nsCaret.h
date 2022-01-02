/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the caret is the text cursor used, e.g., when editing */

#ifndef nsCaret_h__
#define nsCaret_h__

#include "mozilla/intl/BidiEmbeddingLevel.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Selection.h"
#include "nsCoord.h"
#include "nsISelectionListener.h"
#include "nsIWeakReferenceUtils.h"
#include "CaretAssociationHint.h"
#include "nsPoint.h"
#include "nsRect.h"

class nsFrameSelection;
class nsIContent;
class nsIFrame;
class nsINode;
class nsITimer;

namespace mozilla {
class PresShell;
namespace gfx {
class DrawTarget;
}  // namespace gfx
}  // namespace mozilla

//-----------------------------------------------------------------------------
class nsCaret final : public nsISelectionListener {
  typedef mozilla::gfx::DrawTarget DrawTarget;

 public:
  nsCaret();

 protected:
  virtual ~nsCaret();

 public:
  NS_DECL_ISUPPORTS

  typedef mozilla::CaretAssociationHint CaretAssociationHint;

  nsresult Init(mozilla::PresShell* aPresShell);
  void Terminate();

  void SetSelection(mozilla::dom::Selection* aDOMSel);
  mozilla::dom::Selection* GetSelection();

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
  bool IsVisible(mozilla::dom::Selection* aSelection = nullptr) {
    if (!mVisible || mHideCount) {
      return false;
    }

    if (!mShowDuringSelection) {
      mozilla::dom::Selection* selection;
      if (aSelection) {
        selection = aSelection;
      } else {
        selection = GetSelection();
      }
      if (!selection || !selection->IsCollapsed()) {
        return false;
      }
    }

    if (IsMenuPopupHidingCaret()) {
      return false;
    }

    return true;
  }
  /**
   * AddForceHide() increases mHideCount and hide the caret even if
   * SetVisible(true) has been or will be called.  This is useful when the
   * caller wants to hide caret temporarily and it needs to cancel later.
   * Especially, in the latter case, it's too difficult to decide if the
   * caret should be actually visible or not because caret visible state
   * is set from a lot of event handlers.  So, it's very stateful.
   */
  void AddForceHide();
  /**
   * RemoveForceHide() decreases mHideCount if it's over 0.
   * If the value becomes 0, this may show the caret if SetVisible(true)
   * has been called.
   */
  void RemoveForceHide();
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
  void SetCaretPosition(nsINode* aNode, int32_t aOffset);

  /**
   * Schedule a repaint for the frame where the caret would appear.
   * Does not check visibility etc.
   */
  void SchedulePaint(mozilla::dom::Selection* aSelection = nullptr);

  /**
   * Returns a frame to paint in, and the bounds of the painted caret
   * relative to that frame.
   * The rectangle includes bidi decorations.
   * Returns null if the caret should not be drawn (including if it's blinked
   * off).
   */
  nsIFrame* GetPaintGeometry(nsRect* aRect);
  /**
   * Same as the overload above, but returns the caret and hook rects
   * separately, and also computes the color if requested.
   */
  nsIFrame* GetPaintGeometry(nsRect* aCaretRect, nsRect* aHookRect,
                             nscolor* aCaretColor = nullptr);
  /**
   * A simple wrapper around GetGeometry. Does not take any caret state into
   * account other than the current selection.
   */
  nsIFrame* GetGeometry(nsRect* aRect) {
    return GetGeometry(GetSelection(), aRect);
  }

  /** PaintCaret
   *  Actually paint the caret onto the given rendering context.
   */
  void PaintCaret(DrawTarget& aDrawTarget, nsIFrame* aForFrame,
                  const nsPoint& aOffset);

  // nsISelectionListener interface
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
  static nsIFrame* GetGeometry(const mozilla::dom::Selection* aSelection,
                               nsRect* aRect);
  static nsIFrame* GetCaretFrameForNodeOffset(
      nsFrameSelection* aFrameSelection, nsIContent* aContentNode,
      int32_t aOffset, CaretAssociationHint aFrameHint,
      mozilla::intl::BidiEmbeddingLevel aBidiLevel,
      nsIFrame** aReturnUnadjustedFrame, int32_t* aReturnOffset);
  static nsRect GetGeometryForFrame(nsIFrame* aFrame, int32_t aFrameOffset,
                                    nscoord* aBidiIndicatorSize);

  // Get the frame and frame offset based on the focus node and focus offset
  // of aSelection. If aOverrideNode and aOverride are provided, use them
  // instead.
  // @param aFrameOffset return the frame offset if non-null.
  // @param aUnadjustedFrame return the original frame that the selection is
  // targeting, without any adjustment for painting.
  // @return the frame of the focus node.
  static nsIFrame* GetFrameAndOffset(const mozilla::dom::Selection* aSelection,
                                     nsINode* aOverrideNode,
                                     int32_t aOverrideOffset,
                                     int32_t* aFrameOffset,
                                     nsIFrame** aUnadjustedFrame = nullptr);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 protected:
  static void CaretBlinkCallback(nsITimer* aTimer, void* aClosure);

  void CheckSelectionLanguageChange();

  void ResetBlinking();
  void StopBlinking();

  struct Metrics {
    nscoord mBidiIndicatorSize;  // width and height of bidi indicator
    nscoord mCaretWidth;         // full caret width including bidi indicator
  };
  static Metrics ComputeMetrics(nsIFrame* aFrame, int32_t aOffset,
                                nscoord aCaretHeight);
  void ComputeCaretRects(nsIFrame* aFrame, int32_t aFrameOffset,
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

  nsWeakPtr mPresShell;
  mozilla::WeakPtr<mozilla::dom::Selection> mDomSelectionWeak;

  nsCOMPtr<nsITimer> mBlinkTimer;

  /**
   * The content to draw the caret at. If null, we use mDomSelectionWeak's
   * focus node instead.
   */
  nsCOMPtr<nsINode> mOverrideContent;
  /**
   * The character offset to draw the caret at.
   * Ignored if mOverrideContent is null.
   */
  int32_t mOverrideOffset;
  /**
   * mBlinkCount is used to control the number of times to blink the caret
   * before stopping the blink. This is reset each time we reset the
   * blinking.
   */
  int32_t mBlinkCount;
  /**
   * mBlinkRate is the rate of the caret blinking the last time we read it.
   * It is used as a way to optimize whether we need to reset the blinking
   * timer.
   */
  uint32_t mBlinkRate;
  /**
   * mHideCount is not 0, it means that somebody doesn't want the caret
   * to be visible.  See AddForceHide() and RemoveForceHide().
   */
  uint32_t mHideCount;

  /**
   * mIsBlinkOn is true when we're in a blink cycle where the caret is on.
   */
  bool mIsBlinkOn;
  /**
   * mIsVisible is true when SetVisible was last called with 'true'.
   */
  bool mVisible;
  /**
   * mReadOnly is true when the caret is set to "read only" mode (i.e.,
   * it doesn't blink).
   */
  bool mReadOnly;
  /**
   * mShowDuringSelection is true when the caret should be shown even when
   * the selection is not collapsed.
   */
  bool mShowDuringSelection;
  /**
   * mIgnoreUserModify is true when the caret should be shown even when
   * it's in non-user-modifiable content.
   */
  bool mIgnoreUserModify;
};

#endif  // nsCaret_h__
