/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Description of TouchManager class.
 * Incapsulate code related with work of touch events.
 */

#ifndef TouchManager_h_
#define TouchManager_h_

#include "Units.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TouchEvents.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
class PresShell;
class TimeStamp;

class TouchManager {
 public:
  // Initialize and release static variables
  static void InitializeStatics();
  static void ReleaseStatics();

  void Init(PresShell* aPresShell, dom::Document* aDocument);
  void Destroy();

  // Perform hit test and setup the event targets for touchstart. Other touch
  // events are dispatched to the same target as touchstart.
  static nsIFrame* SetupTarget(WidgetTouchEvent* aEvent, nsIFrame* aFrame);

  /**
   * This function checks whether all touch points hit elements in the same
   * document. If not, we try to find its cross document parent which is in the
   * same document of the existing target as the event target. We mark the
   * touch point as suppressed if can't find it. The suppressed touch points are
   * removed in TouchManager::PreHandleEvent so that we don't dispatch them to
   * content.
   *
   * @param aEvent    A touch event to be checked.
   *
   * @return          The targeted frame of aEvent.
   */
  static nsIFrame* SuppressInvalidPointsAndGetTargetedFrame(
      WidgetTouchEvent* aEvent);

  bool PreHandleEvent(mozilla::WidgetEvent* aEvent, nsEventStatus* aStatus,
                      bool& aTouchIsNew,
                      nsCOMPtr<nsIContent>& aCurrentEventContent);
  void PostHandleEvent(const mozilla::WidgetEvent* aEvent,
                       const nsEventStatus* aStatus);

  static already_AddRefed<nsIContent> GetAnyCapturedTouchTarget();
  static bool HasCapturedTouch(int32_t aId);
  static already_AddRefed<dom::Touch> GetCapturedTouch(int32_t aId);
  static bool ShouldConvertTouchToPointer(const dom::Touch* aTouch,
                                          const WidgetTouchEvent* aEvent);

  // This should be called after PostHandleEvent() is called.  Note that this
  // cannot check touches outside this process.  So, this should not be used for
  // actual user input handling.  This is designed for a fallback path to
  // dispatch mouse events for touch events synthesized without APZ.
  static bool IsSingleTapEndToDoDefault(const WidgetTouchEvent* aTouchEndEvent);

  // Returns true if the preceding `pointerdown` was consumed by content of
  // the last active pointers of touches.
  static bool IsPrecedingTouchPointerDownConsumedByContent();

 private:
  void EvictTouches(dom::Document* aLimitToDocument = nullptr);
  static void EvictTouchPoint(RefPtr<dom::Touch>& aTouch,
                              dom::Document* aLimitToDocument);
  static void AppendToTouchList(WidgetTouchEvent::TouchArrayBase* aTouchList);

  RefPtr<PresShell> mPresShell;
  RefPtr<dom::Document> mDocument;

  struct TouchInfo {
    RefPtr<mozilla::dom::Touch> mTouch;
    nsCOMPtr<nsIContent> mNonAnonymousTarget;
    bool mConvertToPointer;
  };

  static StaticAutoPtr<nsTHashMap<nsUint32HashKey, TouchInfo>>
      sCaptureTouchList;
  static layers::LayersId sCaptureTouchLayersId;
  // The last start of a single tap.  This will be set to "Null" if the tap is
  // consumed or becomes not a single tap.
  // NOTE: This is used for touches without APZ, i.e., if they are synthesized
  // in-process for tests.
  static TimeStamp sSingleTouchStartTimeStamp;
  // The last start point of the single tap tracked with
  // sSingleTouchStartTimeStamp.
  // NOTE: This is used for touches without APZ, i.e., if they are synthesized
  // in-process for tests.
  static LayoutDeviceIntPoint sSingleTouchStartPoint;
  // Whether the preceding `pointerdown` of the last active touches is consumed
  // by content or not.  If APZ is enabled, same state is managed by
  // APZEventState.
  // NOTE: This is used for touches without APZ, i.e., if they are synthesized
  // in-process for tests.
  static bool sPrecedingTouchPointerDownConsumedByContent;
};

}  // namespace mozilla

#endif /* !defined(TouchManager_h_) */
