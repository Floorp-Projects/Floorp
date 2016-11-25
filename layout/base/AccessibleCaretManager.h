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
#include "nsIDOMMouseEvent.h"
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
// https://wiki.mozilla.org/AccessibleCaret
//
class AccessibleCaretManager
{
public:
  explicit AccessibleCaretManager(nsIPresShell* aPresShell);
  virtual ~AccessibleCaretManager();

  // Called by AccessibleCaretEventHub to inform us that PresShell is destroyed.
  void Terminate();

  // The aPoint in the following public methods should be relative to root
  // frame.

  // Press caret on the given point. Return NS_OK if the point is actually on
  // one of the carets.
  virtual nsresult PressCaret(const nsPoint& aPoint, EventClassID aEventClass);

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

  // The canvas frame holding the accessible caret anonymous content elements
  // was reconstructed, resulting in the content elements getting cloned.
  virtual void OnFrameReconstruction();

  // Called by AccessibleCaretEventHub to inform us that the document
  // becomes visible.
  virtual void OnDocumentVisible();

  // Update the manager with the last input source that was observed. This
  // is used in part to determine if the carets should be shown or hidden.
  void SetLastInputSource(uint16_t aInputSource);

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

  // Update carets based on current selection status. This function will flush
  // layout, so caller must ensure the PresShell is still valid after calling
  // this method.
  void UpdateCarets(UpdateCaretsHint aHint = UpdateCaretsHint::Default);

  // Force hiding all carets regardless of the current selection status.
  void HideCarets();

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

  // Called to extend a selection if possible that it's a phone number.
  void SelectMoreIfPhoneNumber() const;
  // Extend the current phone number selection in the requested direction.
  void ExtendPhoneNumberSelection(const nsAString& aDirection) const;

  void SetSelectionDirection(nsDirection aDir) const;

  // If aDirection is eDirNext, get the frame for the range start in the first
  // range from the current selection, and return the offset into that frame as
  // well as the range start node and the node offset. Otherwise, get the frame
  // and offset for the range end in the last range instead.
  nsIFrame* GetFrameForFirstRangeStartOrLastRangeEnd(
    nsDirection aDirection, int32_t* aOutOffset, nsINode** aOutNode = nullptr,
    int32_t* aOutNodeOffset = nullptr) const;

  nsresult DragCaretInternal(const nsPoint& aPoint);
  nsPoint AdjustDragBoundary(const nsPoint& aPoint) const;
  void ClearMaintainedSelection() const;

  // Caller is responsible to use IsTerminated() to check whether PresShell is
  // still valid.
  void FlushLayout() const;

  dom::Element* GetEditingHostForFrame(nsIFrame* aFrame) const;
  dom::Selection* GetSelection() const;
  already_AddRefed<nsFrameSelection> GetFrameSelection() const;

  // Get the union of all the child frame scrollable overflow rects for aFrame,
  // which is used as a helper function to restrict the area where the caret can
  // be dragged. Returns the rect relative to aFrame.
  nsRect GetAllChildFrameRectsUnion(nsIFrame* aFrame) const;

  // Restrict the active caret's dragging position based on
  // sCaretsAllowDraggingAcrossOtherCaret. If the active caret is the first
  // caret, the `limit` will be the previous character of the second caret.
  // Otherwise, the `limit` will be the next character of the first caret.
  //
  // @param aOffsets is the new position of the active caret, and it will be set
  // to the `limit` when 1) sCaretsAllowDraggingAcrossOtherCaret is false and
  // it's being dragged past the limit. 2) sCaretsAllowDraggingAcrossOtherCaret
  // is true and the active caret's position is the same as the inactive's
  // position.
  // @return true if the aOffsets is suitable for changing the selection.
  bool RestrictCaretDraggingOffsets(nsIFrame::ContentOffsets& aOffsets);

  // Timeout in milliseconds to hide the AccessibleCaret under cursor mode while
  // no one touches it.
  uint32_t CaretTimeoutMs() const;
  void LaunchCaretTimeoutTimer();
  void CancelCaretTimeoutTimer();

  // ---------------------------------------------------------------------------
  // The following functions are made virtual for stubbing or mocking in gtest.
  //
  // @return true if Terminate() had been called.
  virtual bool IsTerminated() const { return !mPresShell; }

  // Get caret mode based on current selection.
  virtual CaretMode GetCaretMode() const;

  // @return true if aStartFrame comes before aEndFrame.
  virtual bool CompareTreePosition(nsIFrame* aStartFrame,
                                   nsIFrame* aEndFrame) const;

  // Check if the two carets is overlapping to become tilt.
  virtual void UpdateCaretsForOverlappingTilt();

  // Make the two carets always tilt.
  virtual void UpdateCaretsForAlwaysTilt(nsIFrame* aStartFrame,
                                         nsIFrame* aEndFrame);

  // Check whether AccessibleCaret is displayable in cursor mode or not.
  // @param aOutFrame returns frame of the cursor if it's displayable.
  // @param aOutOffset returns frame offset as well.
  virtual bool IsCaretDisplayableInCursorMode(nsIFrame** aOutFrame = nullptr,
                                              int32_t* aOutOffset = nullptr) const;

  virtual bool HasNonEmptyTextContent(nsINode* aNode) const;

  // This function will flush layout, so caller must ensure the PresShell is
  // still valid after calling this method.
  virtual void DispatchCaretStateChangedEvent(dom::CaretChangedReason aReason) const;

  // ---------------------------------------------------------------------------
  // Member variables
  //
  nscoord mOffsetYToCaretLogicalPosition = NS_UNCONSTRAINEDSIZE;

  // AccessibleCaretEventHub owns us by a UniquePtr. When it's destroyed, we'll
  // also be destroyed. No need to worry if we outlive mPresShell.
  //
  // mPresShell will be set to nullptr in Terminate(). Therefore mPresShell is
  // nullptr either we are in gtest or PresShell::IsDestroying() is true.
  nsIPresShell* MOZ_NON_OWNING_REF mPresShell = nullptr;

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

  // Store the appearance of the carets when calling OnScrollStart() so that it
  // can be restored in OnScrollEnd().
  AccessibleCaret::Appearance mFirstCaretAppearanceOnScrollStart =
                                 AccessibleCaret::Appearance::None;
  AccessibleCaret::Appearance mSecondCaretAppearanceOnScrollStart =
                                 AccessibleCaret::Appearance::None;

  // The last input source that the event hub saw. We use this to decide whether
  // or not show the carets when the selection is updated, as we want to hide
  // the carets for mouse-triggered selection changes but show them for other
  // input types such as touch.
  uint16_t mLastInputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;

  static const int32_t kAutoScrollTimerDelay = 30;

  // Clicking on the boundary of input or textarea will move the caret to the
  // front or end of the content. To avoid this, we need to deflate the content
  // boundary by 61 app units, which is 1 pixel + 1 app unit as defined in
  // AppUnit.h.
  static const int32_t kBoundaryAppUnits = 61;

  // Preference to show selection bars at the two ends in selection mode. The
  // selection bar is always disabled in cursor mode.
  static bool sSelectionBarEnabled;

  // Preference to allow smarter selection of phone numbers,
  // when user long presses text to start.
  static bool sExtendSelectionForPhoneNumber;

  // Preference to show caret in cursor mode when long tapping on an empty
  // content. This also changes the default update behavior in cursor mode,
  // which is based on the emptiness of the content, into something more
  // heuristic. See UpdateCaretsForCursorMode() for the details.
  static bool sCaretShownWhenLongTappingOnEmptyContent;

  // Preference to make carets always tilt in selection mode. By default, the
  // carets become tilt only when they are overlapping.
  static bool sCaretsAlwaysTilt;

  // Preference to allow carets always show when scrolling (either panning or
  // zooming) the page. When set to false, carets will hide during scrolling,
  // and show again after the user lifts the finger off the screen.
  static bool sCaretsAlwaysShowWhenScrolling;

  // By default, javascript content selection changes closes AccessibleCarets and
  // UI interactions. Optionally, we can try to maintain the active UI, keeping
  // carets and ActionBar available.
  static bool sCaretsScriptUpdates;

  // Preference to allow one caret to be dragged across the other caret without
  // any limitation. When set to false, one caret cannot be dragged across the
  // other one.
  static bool sCaretsAllowDraggingAcrossOtherCaret;

  // AccessibleCaret pref for haptic feedback behaviour on longPress.
  static bool sHapticFeedback;

  // Preference to keep carets hidden when the selection is being manipulated
  // by mouse input (as opposed to touch/pen/etc.).
  static bool sHideCaretsForMouseInput;
};

std::ostream& operator<<(std::ostream& aStream,
                         const AccessibleCaretManager::CaretMode& aCaretMode);

std::ostream& operator<<(std::ostream& aStream,
                         const AccessibleCaretManager::UpdateCaretsHint& aResult);

} // namespace mozilla

#endif // AccessibleCaretManager_h
