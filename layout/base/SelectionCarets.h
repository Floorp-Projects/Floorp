/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SelectionCarets_h__
#define SelectionCarets_h__

#include "nsIScrollObserver.h"
#include "nsISelectionListener.h"
#include "nsWeakPtr.h"
#include "nsWeakReference.h"
#include "Units.h"
#include "mozilla/EventForwards.h"

class nsCanvasFrame;
class nsIDocument;
class nsIFrame;
class nsIPresShell;
class nsITimer;
class nsIWidget;
class nsPresContext;

namespace mozilla {

/**
 * The SelectionCarets draw a pair of carets when the selection is not
 * collapsed, one at each end of the selection.
 * SelectionCarets also handle visibility, dragging caret and selecting word
 * when long tap event fired.
 *
 * The DOM structure is 2 div elements for showing start and end caret.
 *
 * Here is an explanation of the html class names:
 *   .moz-selectioncaret-left: Indicates start DIV.
 *   .moz-selectioncaret-right: Indicates end DIV.
 *   .hidden: This class name is set by SetVisibility,
 *            SetStartFrameVisibility and SetEndFrameVisibility. Element
 *            with this class name become hidden.
 *   .tilt: This class name is set by SetTilted. According to the
 *          UX spec, when selection contains only one characters, the image of
 *          caret becomes tilt.
 */
class SelectionCarets MOZ_FINAL : public nsISelectionListener,
                                  public nsIScrollObserver,
                                  public nsSupportsWeakReference
{
public:
  /**
   * Indicate which part of caret we are dragging at.
   */
  enum DragMode {
    NONE,
    START_FRAME,
    END_FRAME
  };

  explicit SelectionCarets(nsIPresShell *aPresShell);
  virtual ~SelectionCarets();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISELECTIONLISTENER

  // nsIScrollObserver
  virtual void ScrollPositionChanged() MOZ_OVERRIDE;

  void Terminate()
  {
    mPresShell = nullptr;
  }

  nsEventStatus HandleEvent(WidgetEvent* aEvent);

  /**
   * Set visibility for selection caret.
   */
  void SetVisibility(bool aVisible);

  bool GetVisibility() const
  {
    return mVisible;
  }

  /**
   * Get from pref "selectioncaret.inflatesize.threshold". This will inflate size of
   * caret frame when we checking if user click on caret or not. In app units.
   */
  static int32_t SelectionCaretsInflateSize()
  {
    return sSelectionCaretsInflateSize;
  }

private:
  SelectionCarets() MOZ_DELETE;

  /**
   * Update selection caret position base on current selection range.
   */
  void UpdateSelectionCarets();

  /**
   * Select word base on current position, only active when element
   * is focused. Triggered by long tap event.
   */
  nsresult SelectWord();

  /**
   * Move selection base on current touch/mouse point
   */
  nsEventStatus DragSelection(const nsPoint &movePoint);

  /**
   * Get the vertical center position of selection caret relative to canvas
   * frame.
   */
  nscoord GetCaretYCenterPosition();

  /**
   * Simulate mouse down state when we change the selection range.
   * Hence, the selection change event will fire normally.
   */
  void SetMouseDownState(bool aState);

  void SetSelectionDirection(bool aForward);

  /**
   * Move start frame of selection caret to given position.
   * In app units.
   */
  void SetStartFramePos(const nsPoint& aPosition);

  /**
   * Move end frame of selection caret to given position.
   * In app units.
   */
  void SetEndFramePos(const nsPoint& aPosition);

  /**
   * Get rect of selection caret's start frame relative
   * to document's canvas frame, in app units.
   */
  nsRect GetStartFrameRect();

  /**
   * Get rect of selection caret's end frame relative
   * to document's canvas frame, in app units.
   */
  nsRect GetEndFrameRect();

  /**
   * Set visibility for start part of selection caret, this function
   * only affects css property of start frame. So it doesn't change
   * mVisible member. When caret overflows element's box we'll hide
   * it by calling this function.
   */
  void SetStartFrameVisibility(bool aVisible);

  /**
   * Same as above function but for end frame of selection caret.
   */
  void SetEndFrameVisibility(bool aVisible);

  /**
   * Set tilt class name to start and end frame of selection caret.
   */
  void SetTilted(bool aIsTilt);

  // Utility function
  nsIFrame* GetCaretFocusFrame();
  bool GetCaretVisible();
  nsISelection* GetSelection();

  /**
   * Detecting long tap using timer
   */
  void LaunchLongTapDetector();
  void CancelLongTapDetector();
  static void FireLongTap(nsITimer* aTimer, void* aSelectionCarets);

  void LaunchScrollEndDetector();
  static void FireScrollEnd(nsITimer* aTimer, void* aSelectionCarets);

  nsIPresShell* mPresShell;

  // This timer is used for detecting long tap fire. If content process
  // has APZC, we'll use APZC for long tap detecting. Otherwise, we use this
  // timer to detect long tap.
  nsCOMPtr<nsITimer> mLongTapDetectorTimer;

  // This timer is used for detecting scroll end. We don't have
  // scroll end event now, so we will fire this event with a
  // const time when we scroll. So when timer triggers, we treat it
  // as scroll end event.
  nsCOMPtr<nsITimer> mScrollEndDetectorTimer;

  // When touch or mouse down, we save the position for detecting
  // drag distance
  nsPoint mDownPoint;

  // For filter multitouch event
  int32_t mActiveTouchId;

  nscoord mCaretCenterToDownPointOffsetY;
  DragMode mDragMode;
  bool mVisible;
  bool mStartCaretVisible;
  bool mEndCaretVisible;

  // Preference
  static int32_t sSelectionCaretsInflateSize;
};
} // namespace mozilla

#endif //SelectionCarets_h__
