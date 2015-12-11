/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccessibleCaretManager_h
#define AccessibleCaretManager_h

#include "AccessibleCaret.h"
#include "nsCOMPtr.h"
#include "nsCoord.h"
#include "nsIFrame.h"
#include "nsISelectionListener.h"
#include "mozilla/RefPtr.h"
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
class Element;
class Selection;
} // namespace dom

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
// Please see the wiki page for more information.
// https://wiki.mozilla.org/Copy_n_Paste
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

  // Handle ScrollPositionChanged from nsIScrollObserver. This might be called
  // at anytime, not necessary between OnScrollStart and OnScrollEnd.
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

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const CaretMode& aCaretMode);

  enum class UpdateCaretsHint : uint8_t {
    // Update everything including appearance and position.
    Default,

    // Update everything while respecting the old appearance. For example, if
    // the caret in cursor mode is hidden due to timeout, do not change its
    // appearance to Normal.
    RespectOldAppearance
  };

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const UpdateCaretsHint& aResult);

  // Update carets based on current selection status.
  void UpdateCarets(UpdateCaretsHint aHint = UpdateCaretsHint::Default);

  // Force hiding all carets regardless of the current selection status.
  void HideCarets();

  // Force carets to be "present" logically, but not visible. Allows ActionBar
  // to stay open when carets visibility is supressed during scroll.
  void DoNotShowCarets();

  void UpdateCaretsForCursorMode(UpdateCaretsHint aHint);
  void UpdateCaretsForSelectionMode(UpdateCaretsHint aHint);

  // Provide haptic / touch feedback, primarily for select on longpress.
  void ProvideHapticFeedback();

  // Get the nearest enclosing focusable frame of aFrame.
  // @return focusable frame if there is any; nullptr otherwise.
  nsIFrame* GetFocusableFrame(nsIFrame* aFrame) const;

  // Change focus to aFrame if it isn't nullptr. Otherwise, clear the old focus
  // then re-focus the window.
  void ChangeFocusToOrClearOldFocus(nsIFrame* aFrame) const;

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
  void FlushLayout() const;
  dom::Element* GetEditingHostForFrame(nsIFrame* aFrame) const;
  dom::Selection* GetSelection() const;
  already_AddRefed<nsFrameSelection> GetFrameSelection() const;

  // Get the bounding rectangle for aFrame where the caret under cursor mode can
  // be positioned. The rectangle is relative to the root frame.
  nsRect GetContentBoundaryForFrame(nsIFrame* aFrame) const;

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

  // ---------------------------------------------------------------------------
  // The following functions are made virtual for stubbing or mocking in gtest.
  //
  // Get caret mode based on current selection.
  virtual CaretMode GetCaretMode() const;

  // @return true if aStartFrame comes before aEndFrame.
  virtual bool CompareTreePosition(nsIFrame* aStartFrame,
                                   nsIFrame* aEndFrame) const;

  // Check if the two carets is overlapping to become tilt.
  virtual void UpdateCaretsForTilt();

  // Check whether AccessibleCaret is displayable in cursor mode or not.
  // @param aOutFrame returns frame of the cursor if it's displayable.
  // @param aOutOffset returns frame offset as well.
  virtual bool IsCaretDisplayableInCursorMode(nsIFrame** aOutFrame = nullptr,
                                              int32_t* aOutOffset = nullptr) const;

  virtual bool HasNonEmptyTextContent(nsINode* aNode) const;

  // This function will call FlushPendingNotifications. So caller must ensure
  // everything exists after calling this method.
  virtual void DispatchCaretStateChangedEvent(dom::CaretChangedReason aReason) const;

  // ---------------------------------------------------------------------------
  // Member variables
  //
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

  // The timer for hiding the caret in cursor mode after timeout behind the
  // preference "layout.accessiblecaret.timeout_ms".
  nsCOMPtr<nsITimer> mCaretTimeoutTimer;

  // The caret mode since last update carets.
  CaretMode mLastUpdateCaretMode = CaretMode::None;

  // Store the appearance of the first caret when calling OnScrollStart so that
  // it can be restored in OnScrollEnd.
  AccessibleCaret::Appearance mFirstCaretAppearanceOnScrollStart =
                                 AccessibleCaret::Appearance::None;

  static const int32_t kAutoScrollTimerDelay = 30;

  // Clicking on the boundary of input or textarea will move the caret to the
  // front or end of the content. To avoid this, we need to deflate the content
  // boundary by 61 app units, which is 1 pixel + 1 app unit as defined in
  // AppUnit.h.
  static const int32_t kBoundaryAppUnits = 61;

  // AccessibleCaret visibility preference. Used to avoid hiding caret during
  // (NO_REASON) selection change notifications generated by keyboard IME, and to
  // maintain a visible ActionBar while carets NotShown during scroll.
  static bool sCaretsExtendedVisibility;

  // AccessibleCaret pref for haptic feedback behaviour on longPress.
  static bool sHapticFeedback;
};

std::ostream& operator<<(std::ostream& aStream,
                         const AccessibleCaretManager::CaretMode& aCaretMode);

std::ostream& operator<<(std::ostream& aStream,
                         const AccessibleCaretManager::UpdateCaretsHint& aResult);

} // namespace mozilla

#endif // AccessibleCaretManager_h
