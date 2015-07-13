/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SelectionCarets_h__
#define SelectionCarets_h__

#include "nsDirection.h"
#include "nsIReflowObserver.h"
#include "nsIScrollObserver.h"
#include "nsISelectionListener.h"
#include "nsWeakPtr.h"
#include "nsWeakReference.h"
#include "Units.h"
#include "mozilla/dom/SelectionStateChangedEvent.h"
#include "mozilla/EventForwards.h"
#include "mozilla/WeakPtr.h"

class nsDocShell;
class nsFrameSelection;
class nsIContent;
class nsIPresShell;
class nsITimer;

namespace mozilla {

namespace dom {
class Selection;
} // namespace dom

/**
 * The SelectionCarets draw a pair of carets when the selection is not
 * collapsed, one at each end of the selection.
 * SelectionCarets also handle visibility, dragging caret and selecting word
 * when long tap event fired.
 *
 * The DOM structure is 2 div elements for showing start and end caret.
 * Each div element has a child div element. That is, each caret consist of
 * outer div and inner div. Outer div takes responsibility for detecting two
 * carets are overlapping. Inner div is for actual appearance.
 *
 * Here is an explanation of the html class names:
 *   .moz-selectioncaret-left: Indicates start DIV.
 *   .moz-selectioncaret-right: Indicates end DIV.
 *   .hidden: This class name is set by SetVisibility,
 *            SetStartFrameVisibility and SetEndFrameVisibility. Element
 *            with this class name become hidden.
 *   .tilt: This class name is set by SetTilted. According to the
 *          UX spec, when selection carets are overlapping, the image of
 *          caret becomes tilt.
 */
class SelectionCarets final : public nsIReflowObserver,
                              public nsISelectionListener,
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

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREFLOWOBSERVER
  NS_DECL_NSISELECTIONLISTENER

  // Notify selection carets about the blur event to hidden itself
  void NotifyBlur(bool aIsLeavingDocument);

  // nsIScrollObserver
  virtual void ScrollPositionChanged() override;

  // AsyncPanZoom started/stopped callbacks from nsIScrollObserver
  virtual void AsyncPanZoomStarted() override;
  virtual void AsyncPanZoomStopped() override;

  void Init();
  void Terminate();

  nsEventStatus HandleEvent(WidgetEvent* aEvent);

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

  /**
   * Set visibility for selection caret.
   */
  void SetVisibility(bool aVisible);

  /**
   * Update selection caret position base on current selection range.
   */
  void UpdateSelectionCarets();

private:
  virtual ~SelectionCarets();

  SelectionCarets() = delete;

  /**
   * Select a word base on current position, which activates only if element is
   * selectable. Triggered by long tap event.
   */
  nsresult SelectWord();

  /**
   * Move selection base on current touch/mouse point
   */
  nsEventStatus DragSelection(const nsPoint &movePoint);

  /**
   * Get the vertical center position of selection caret relative to root
   * frame.
   */
  nscoord GetCaretYCenterPosition();

  /**
   * Simulate drag state when we change the selection range.
   * Hence, the selection change event will fire normally.
   */
  void SetSelectionDragState(bool aState);

  void SetSelectionDirection(nsDirection aDir);

  /**
   * Move start frame of selection caret based on current caret pos.
   * In app units.
   */
  void SetStartFramePos(const nsRect& aCaretRect);

  /**
   * Move end frame of selection caret based on current caret pos.
   * In app units.
   */
  void SetEndFramePos(const nsRect& aCaretRect);

  /**
   * Check if aPosition is on the start or end frame of the
   * selection caret's inner div element.
   *
   * @param aPosition should be relative to document's root frame
   * in app units
   */
  bool IsOnStartFrameInner(const nsPoint& aPosition);
  bool IsOnEndFrameInner(const nsPoint& aPosition);

  /**
   * Get rect of selection caret's outer div element relative
   * to document's root frame, in app units.
   */
  nsRect GetStartFrameRect();
  nsRect GetEndFrameRect();

  /**
   * Get rect of selection caret's inner div element relative
   * to document's root frame, in app units.
   */
  nsRect GetStartFrameRectInner();
  nsRect GetEndFrameRectInner();

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

  // Utility functions
  dom::Selection* GetSelection();
  already_AddRefed<nsFrameSelection> GetFrameSelection();
  nsIContent* GetFocusedContent();
  void DispatchSelectionStateChangedEvent(dom::Selection* aSelection,
                                          dom::SelectionState aState);
  void DispatchSelectionStateChangedEvent(dom::Selection* aSelection,
                                          const dom::Sequence<dom::SelectionState>& aStates);
  void DispatchCustomEvent(const nsAString& aEvent);

  /**
   * Detecting long tap using timer
   */
  void LaunchLongTapDetector();
  void CancelLongTapDetector();
  static void FireLongTap(nsITimer* aTimer, void* aSelectionCarets);

  void LaunchScrollEndDetector();
  void CancelScrollEndDetector();
  static void FireScrollEnd(nsITimer* aTimer, void* aSelectionCarets);

  nsIPresShell* mPresShell;
  WeakPtr<nsDocShell> mDocShell;

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

  // The horizontal boundary is defined by the first selected frame which
  // determines the start-caret position. When users drag the end-caret up,
  // the touch input(pos.y) will be changed to not cross this boundary.
  // Otherwise, the selection range changes to one character only
  // which causes the bad user experience.
  nscoord mDragUpYBoundary;
  // The horizontal boundary is defined by the last selected frame which
  // determines the end-caret position. When users drag the start-caret down,
  // the touch input(pos.y) will be changed to not cross this boundary.
  // Otherwise, the selection range changes to one character only
  // which causes the bad user experience.
  nscoord mDragDownYBoundary;

  DragMode mDragMode;

  // True if async-pan-zoom should be used for selection carets.
  bool mUseAsyncPanZoom;
  // True if AsyncPanZoom is started
  bool mInAsyncPanZoomGesture;

  bool mEndCaretVisible;
  bool mStartCaretVisible;
  bool mSelectionVisibleInScrollFrames;
  bool mVisible;

  // Preference
  static int32_t sSelectionCaretsInflateSize;
  static bool sSelectionCaretDetectsLongTap;
  static bool sCaretManagesAndroidActionbar;
  static bool sSelectionCaretObservesCompositions;

  // Unique ID of current Mobile ActionBar view.
  uint32_t mActionBarViewID;
};

} // namespace mozilla

#endif //SelectionCarets_h__
