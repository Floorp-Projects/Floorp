/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccessibleCaretManager_h
#define AccessibleCaretManager_h

#include "nsCOMPtr.h"
#include "nsCoord.h"
#include "nsIFrame.h"
#include "nsISelectionListener.h"
#include "nsRefPtr.h"
#include "nsWeakReference.h"
#include "mozilla/dom/CaretStateChangedEvent.h"
#include "mozilla/EventForwards.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"

class nsFrameSelection;
class nsIContent;
class nsIPresShell;
struct nsPoint;

namespace mozilla {

namespace dom {
class Selection;
} // namespace dom

class AccessibleCaret;

// -----------------------------------------------------------------------------
// AccessibleCaretManager does not deal with events or callbacks directly. It
// relies on AccessibleCaretEventHub to call its public methods to do the work.
// All codes needed to interact with PresShell, Selection, and AccessibleCaret
// should be written in AccessibleCaretManager.
//
// None the public methods in AccessibleCaretManager will flush layout or style
// prior to performing its task. The caller must ensure the layout is up to
// date.
//
class AccessibleCaretManager
{
public:
  explicit AccessibleCaretManager(nsIPresShell* aPresShell);
  virtual ~AccessibleCaretManager();

  // The aPoint in the following public methods should be relative to root
  // frame.

  // Press caret on the given point. Return NS_OK if the point is actually on
  // one of the carets.
  virtual nsresult PressCaret(const nsPoint& aPoint);

  // Drag caret to the given point. It's required to call PressCaret()
  // beforehand.
  virtual nsresult DragCaret(const nsPoint& aPoint);

  // Release caret from he previous press action. It's required to call
  // PressCaret() beforehand.
  virtual nsresult ReleaseCaret();

  // A quick single tap on caret on given point without dragging.
  virtual nsresult TapCaret(const nsPoint& aPoint);

  // Select a word or bring up paste shortcut (if Gaia is listening) under the
  // given point.
  virtual nsresult SelectWordOrShortcut(const nsPoint& aPoint);

  // Handle scroll-start event.
  virtual void OnScrollStart();

  // Handle scroll-end event.
  virtual void OnScrollEnd();

  // Handle NS_WHEEL_WHEEL event.
  virtual void OnScrolling();

  // Handle ScrollPositionChanged from nsIScrollObserver.
  virtual void OnScrollPositionChanged();

  // Handle reflow event from nsIReflowObserver.
  virtual void OnReflow();

  // Handle blur event from nsFocusManager.
  virtual void OnBlur();

  // Handle NotifySelectionChanged event from nsISelectionListener.
  virtual nsresult OnSelectionChanged(nsIDOMDocument* aDoc,
                                      nsISelection* aSel,
                                      int16_t aReason);
  // Handle key event.
  virtual void OnKeyboardEvent();

protected:
  // This enum representing the number of AccessibleCarets on the screen.
  enum class CaretMode : uint8_t {
    // No caret on the screen.
    None,

    // One caret, i.e. the selection is collapsed.
    Cursor,

    // Two carets, i.e. the selection is not collapsed.
    Selection
  };
  CaretMode GetCaretMode() const;

  void UpdateCarets();
  void HideCarets();

  void UpdateCaretsForCursorMode();
  void UpdateCaretsForSelectionMode();
  void UpdateCaretsForTilt();

  bool ChangeFocus(nsIFrame* aFrame) const;
  nsresult SelectWord(nsIFrame* aFrame, const nsPoint& aPoint) const;
  void SetSelectionDragState(bool aState) const;
  void SetSelectionDirection(nsDirection aDir) const;

  // If aBackward is false, find the first node from the first range in current
  // selection, and return the frame and the offset into that frame. If aBackward
  // is true, find the last node from the last range instead.
  nsIFrame* FindFirstNodeWithFrame(bool aBackward, int32_t* aOutOffset) const;

  nsresult DragCaretInternal(const nsPoint& aPoint);
  nsPoint AdjustDragBoundary(const nsPoint& aPoint) const;
  void ClearMaintainedSelection() const;

  dom::Selection* GetSelection() const;
  already_AddRefed<nsFrameSelection> GetFrameSelection() const;
  nsIContent* GetFocusedContent() const;

  // This function will call FlushPendingNotifications. So caller must ensure
  // everything exists after calling this method.
  void DispatchCaretStateChangedEvent(dom::CaretChangedReason aReason) const;

  // If we're dragging the first caret, we do not want to drag it over the
  // previous character of the second caret. Same as the second caret. So we
  // check if content offset exceeds the previous/next character of second/first
  // caret base the active caret.
  bool CompareRangeWithContentOffset(nsIFrame::ContentOffsets& aOffsets);

  // Timeout in milliseconds to hide the AccessibleCaret under cursor mode while
  // no one touches it.
  uint32_t CaretTimeoutMs() const;
  void LaunchCaretTimeoutTimer();
  void CancelCaretTimeoutTimer();

  // Member variables
  nscoord mOffsetYToCaretLogicalPosition = NS_UNCONSTRAINEDSIZE;

  // AccessibleCaretEventHub owns us. When it's Terminate() called by
  // PresShell::Destroy(), we will be destroyed. No need to worry we outlive
  // mPresShell.
  nsIPresShell* MOZ_NON_OWNING_REF const mPresShell = nullptr;

  // First caret is attached to nsCaret in cursor mode, and is attached to
  // selection highlight as the left caret in selection mode.
  UniquePtr<AccessibleCaret> mFirstCaret;

  // Second caret is used solely in selection mode, and is attached to selection
  // highlight as the right caret.
  UniquePtr<AccessibleCaret> mSecondCaret;

  // The caret being pressed or dragged.
  AccessibleCaret* mActiveCaret = nullptr;

  nsCOMPtr<nsITimer> mCaretTimeoutTimer;
  CaretMode mCaretMode = CaretMode::None;

  static const int32_t kAutoScrollTimerDelay = 30;
};

} // namespace mozilla

#endif // AccessibleCaretManager_h
