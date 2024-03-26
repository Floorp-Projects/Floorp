/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the caret is the text cursor used, e.g., when editing */

#ifndef nsCaret_h__
#define nsCaret_h__

#include "mozilla/MemoryReporting.h"
#include "mozilla/SelectionMovementUtils.h"
#include "nsCoord.h"
#include "nsIFrame.h"
#include "nsISelectionListener.h"
#include "nsPoint.h"
#include "nsRect.h"

class nsFrameSelection;
class nsIContent;
class nsIFrame;
class nsINode;
class nsITimer;

namespace mozilla {
class PresShell;
enum class CaretAssociationHint;
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

  using CaretAssociationHint = mozilla::CaretAssociationHint;

  nsresult Init(mozilla::PresShell*);
  void Terminate();

  void SetSelection(mozilla::dom::Selection*);
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
  /**
   * SetVisible will set the visibility of the caret
   *  @param aVisible true to show the caret, false to hide it
   */
  void SetVisible(bool aVisible);
  /**
   * IsVisible will get the visibility of the caret.
   * It does not take account of blinking or the caret being hidden because
   * we're in non-editable/disabled content.
   */
  bool IsVisible() const;

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
  void SetCaretReadOnly(bool aReadOnly);
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
  void SchedulePaint();

  nsIFrame* GetLastPaintedFrame() { return mLastPaintedFrame; }
  void SetLastPaintedFrame(nsIFrame* aFrame) { mLastPaintedFrame = aFrame; }

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

  /** The current caret position. */
  struct CaretPosition {
    nsCOMPtr<nsINode> mContent;
    int32_t mOffset = 0;
    CaretAssociationHint mHint{0};
    mozilla::intl::BidiEmbeddingLevel mBidiLevel;

    bool operator==(const CaretPosition& aOther) const {
      return mContent == aOther.mContent && mOffset == aOther.mOffset &&
             mHint == aOther.mHint && mBidiLevel == aOther.mBidiLevel;
    }
    explicit operator bool() const { return !!mContent; }
  };

  static CaretPosition CaretPositionFor(const mozilla::dom::Selection*);

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

  static nsRect GetGeometryForFrame(nsIFrame* aFrame, int32_t aFrameOffset,
                                    nscoord* aBidiIndicatorSize);

  // Get the frame and frame offset based on aPosition.
  static mozilla::CaretFrameData GetFrameAndOffset(
      const CaretPosition& aPosition);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 protected:
  static void CaretBlinkCallback(nsITimer* aTimer, void* aClosure);

  void CheckSelectionLanguageChange();
  void CaretVisibilityMaybeChanged();

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

  // If we're tracking the selection, this updates the caret position and
  // invalidates paint as needed.
  void UpdateCaretPositionFromSelectionIfNeeded();

  nsWeakPtr mPresShell;
  mozilla::WeakPtr<mozilla::dom::Selection> mDomSelectionWeak;

  nsCOMPtr<nsITimer> mBlinkTimer;

  CaretPosition mCaretPosition;

  // The last frame we painted the caret in.
  WeakFrame mLastPaintedFrame;

  /**
   * mBlinkCount is used to control the number of times to blink the caret
   * before stopping the blink. This is reset each time we reset the
   * blinking.
   */
  int32_t mBlinkCount = -1;
  /**
   * mBlinkRate is the rate of the caret blinking the last time we read it.
   * It is used as a way to optimize whether we need to reset the blinking
   * timer. 0 or a negative value means no blinking.
   */
  int32_t mBlinkRate = 0;
  /**
   * mHideCount is not 0, it means that somebody doesn't want the caret
   * to be visible.  See AddForceHide() and RemoveForceHide().
   */
  uint32_t mHideCount = 0;

  /**
   * mIsBlinkOn is true when we're in a blink cycle where the caret is on.
   */
  bool mIsBlinkOn = false;
  /**
   * mIsVisible is true when SetVisible was last called with 'true'.
   */
  bool mVisible = false;
  /**
   * mReadOnly is true when the caret is set to "read only" mode (i.e.,
   * it doesn't blink).
   */
  bool mReadOnly = false;
  /**
   * mShowDuringSelection is true when the caret should be shown even when
   * the selection is not collapsed.
   */
  bool mShowDuringSelection = false;
  /**
   * mIgnoreUserModify is true when the caret should be shown even when
   * it's in non-user-modifiable content.
   */
  bool mIgnoreUserModify = true;

  /**
   * If the caret position is fixed, it's been overridden externally and it
   * will not track the selection.
   */
  bool mFixedCaretPosition = false;

  /**
   * If we're currently hiding the caret due to the selection not being
   * collapsed. Can only be true if mShowDuringSelection is false.
   */
  bool mHiddenDuringSelection = false;
};

#endif  // nsCaret_h__
