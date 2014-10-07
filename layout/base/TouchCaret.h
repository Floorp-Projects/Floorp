/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TouchCaret_h__
#define mozilla_TouchCaret_h__

#include "nsISelectionListener.h"
#include "nsIScrollObserver.h"
#include "nsIWeakReferenceUtils.h"
#include "nsITimer.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TouchEvents.h"
#include "Units.h"

class nsIFrame;
class nsIPresShell;

namespace mozilla {

/**
 * The TouchCaret places a touch caret according to caret position when the
 * caret is shown.
 * TouchCaret is also responsible for touch caret visibility. Touch caret
 * won't be shown when timer expires or while key event causes selection change.
 */
class TouchCaret MOZ_FINAL : public nsISelectionListener
{
public:
  explicit TouchCaret(nsIPresShell* aPresShell);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISELECTIONLISTENER

  void Terminate()
  {
    mPresShell = nullptr;
  }

  /**
   * Handle mouse and touch event only.
   * Depends on visibility and position of touch caret, HandleEvent may consume
   * that input event and return nsEventStatus_eConsumeNoDefault to the caller.
   * In that case, caller should stop bubble up that input event.
   */
  nsEventStatus HandleEvent(WidgetEvent* aEvent);

  void SyncVisibilityWithCaret();

  void UpdatePositionIfNeeded();

  /**
   * GetVisibility will get the visibility of the touch caret.
   */
  bool GetVisibility() const
  {
    return mVisible;
  }

private:
  // Hide default constructor.
  TouchCaret() MOZ_DELETE;

  ~TouchCaret();

  bool IsDisplayable();

  void UpdatePosition();

  /**
   * SetVisibility will set the visibility of the touch caret.
   * SetVisibility performs an attribute-changed notification which could, in
   * theory, destroy frames.
   */
  void SetVisibility(bool aVisible);

  /**
   * Helper function to get caret's focus frame and caret's bounding rect.
   */
  nsIFrame* GetCaretFocusFrame(nsRect* aOutRect = nullptr);

  /**
   * Find the nsCanvasFrame which holds the touch caret.
   */
  nsIFrame* GetCanvasFrame();

  /**
   * Retrieve the bounding rectangle of the touch caret.
   *
   * @returns A nsRect representing the bounding rectangle of this touch caret.
   *          The returned offset is relative to the canvas frame.
   */
  nsRect GetTouchFrameRect();

  /**
   * Retrieve the bounding rectangle where the caret can be positioned.
   * If we're positioning a caret in an input field, make sure the touch caret
   * stays within the bounds of the field.
   *
   * @returns A nsRect representing the bounding rectangle of this valid area.
   *          The returned offset is relative to the canvas frame.
   */
  nsRect GetContentBoundary();

  /**
   * Retrieve the center y position of the caret.
   * The returned point is relative to the canvas frame.
   */
  nscoord GetCaretYCenterPosition();

  /**
   * Set the position of the touch caret.
   * Touch caret is an absolute positioned div.
   */
  void SetTouchFramePos(const nsPoint& aOrigin);

  void LaunchExpirationTimer();
  void CancelExpirationTimer();
  static void DisableTouchCaretCallback(nsITimer* aTimer, void* aPresShell);

  /**
   * Move the caret to movePoint which is relative to the canvas frame.
   * Caret will be scrolled into view.
   *
   * @param movePoint tap location relative to the canvas frame.
   */
  void MoveCaret(const nsPoint& movePoint);

  /**
   * Check if aPoint is inside the touch caret frame.
   *
   * @param aPoint tap location relative to the canvas frame.
   */
  bool IsOnTouchCaret(const nsPoint& aPoint);

  /**
   * These Handle* functions comprise input alphabet of the TouchCaret
   * finite-state machine triggering state transitions.
   */
  nsEventStatus HandleMouseMoveEvent(WidgetMouseEvent* aEvent);
  nsEventStatus HandleMouseUpEvent(WidgetMouseEvent* aEvent);
  nsEventStatus HandleMouseDownEvent(WidgetMouseEvent* aEvent);
  nsEventStatus HandleTouchMoveEvent(WidgetTouchEvent* aEvent);
  nsEventStatus HandleTouchUpEvent(WidgetTouchEvent* aEvent);
  nsEventStatus HandleTouchDownEvent(WidgetTouchEvent* aEvent);

  /**
   * Get the coordinates of a given touch event, relative to canvas frame.
   * @param aEvent the event
   * @param aIdentifier the mIdentifier of the touch which is to be converted.
   * @return the point, or (NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE) if
   * for some reason the coordinates for the touch are not known (e.g.,
   * the mIdentifier touch is not found).
   */
  nsPoint GetEventPosition(WidgetTouchEvent* aEvent, int32_t aIdentifier);

  /**
   * Set mouse down state in nsFrameSelection, we'll set state to true when
   * user start dragging caret and set state to false when user release the
   * caret. The reason for setting this state is it will fire drag reason
   * when moving caret and fire mouseup reason when releasing caret. So that
   * the display behavior of copy/paste menu becomes more reasonable.
   */
  void SetSelectionDragState(bool aState);

  /**
   * Get the coordinates of a given mouse event, relative to canvas frame.
   * @param aEvent the event
   * @return the point, or (NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE) if
   * for some reason the coordinates for the mouse are not known.
   */
  nsPoint GetEventPosition(WidgetMouseEvent* aEvent);

  /**
   * States of TouchCaret finite-state machine.
   */
  enum TouchCaretState {
    // In this state, either there is no touch/mouse event going on, or the
    // first stroke does not hit the touch caret.
    // Will enter TOUCHCARET_TOUCHDRAG_ACTIVE state if the first touch stroke
    // hits the touch caret. Will enter TOUCHCARET_MOUSEDRAG_ACTIVE state if
    // mouse (left button) down hits the touch caret.
    // Allowed next state: TOUCHCARET_MOUSEDRAG_ACTIVE,
    //                     TOUCHCARET_TOUCHDRAG_ACTIVE.
    TOUCHCARET_NONE,
    // The first (left button) mouse down hits on the touch caret and is
    // alive. Will enter TOUCHCARET_NONE state if the left button is release.
    // Allowed next states: TOUCHCARET_NONE.
    TOUCHCARET_MOUSEDRAG_ACTIVE,
    // The first touch start event hits on touch caret and is alive.
    // Will enter TOUCHCARET_NONE state if the finger on touch caret is
    // removed and there are no more fingers on the screen; will enter
    // TOUCHCARET_TOUCHDRAG_INACTIVE state if the finger on touch caret is
    // removed but still has fingers touching on the screen.
    // Allowed next states: TOUCHCARET_NONE, TOUCHCARET_TOUCHDRAG_INACTIVE.
    TOUCHCARET_TOUCHDRAG_ACTIVE,
    // The first touch stroke, which hit on touch caret, is dead, but still has
    // fingers touching on the screen.
    // Will enter TOUCHCARET_NONE state if all the fingers are removed from the
    // screen.
    // Allowed next state: TOUCHCARET_NONE.
    TOUCHCARET_TOUCHDRAG_INACTIVE,
  };

  /**
   * Do actual state transition and reset substates.
   */
  void SetState(TouchCaretState aState);

  /**
   * Current state we're dealing with.
   */
  TouchCaretState mState;

  /**
   * Array containing all active touch IDs. When a touch happens, it gets added
   * to this array, even if we choose not to handle it. When it ends, we remove
   * it. We need to maintain this array in order to detect the end of the
   * "multitouch" states because touch start events contain all current touches,
   * but touch end events contain only those touches that have gone.
   */
  nsTArray<int32_t> mTouchesId;

  /**
   * The mIdentifier of the touch which is on the touch caret.
   */
  int32_t mActiveTouchId;

  /**
   * The offset between the tap location and the center of caret along y axis.
   */
  nscoord mCaretCenterToDownPointOffsetY;

  /**
   * Get from pref "touchcaret.inflatesize.threshold". This will inflate the
   * size of the touch caret frame when checking if user clicks on the caret
   * or not. In app units.
   */
  static int32_t TouchCaretInflateSize() { return sTouchCaretInflateSize; }

  static int32_t TouchCaretExpirationTime()
  {
    return sTouchCaretExpirationTime;
  }

  nsWeakPtr mPresShell;

  // Touch caret visibility
  bool mVisible;
  // Touch caret timer
  nsCOMPtr<nsITimer> mTouchCaretExpirationTimer;

  // Preference
  static int32_t sTouchCaretInflateSize;
  static int32_t sTouchCaretExpirationTime;

  // The auto scroll timer's interval in miliseconds.
  friend class SelectionCarets;
  static const int32_t sAutoScrollTimerDelay = 30;
};
} //namespace mozilla
#endif //mozilla_TouchCaret_h__
