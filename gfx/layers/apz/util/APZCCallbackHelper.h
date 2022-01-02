/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZCCallbackHelper_h
#define mozilla_layers_APZCCallbackHelper_h

#include "InputData.h"
#include "LayersTypes.h"
#include "Units.h"
#include "mozilla/EventForwards.h"
#include "mozilla/layers/MatrixMessage.h"
#include "nsRefreshObservers.h"

#include <functional>

class nsIContent;
class nsIScrollableFrame;
class nsIWidget;
class nsPresContext;
template <class T>
struct already_AddRefed;
template <class T>
class nsCOMPtr;

namespace mozilla {

class PresShell;
enum class PreventDefaultResult : uint8_t;

namespace layers {

struct RepaintRequest;

typedef std::function<void(uint64_t, const nsTArray<TouchBehaviorFlags>&)>
    SetAllowedTouchBehaviorCallback;

/* Refer to documentation on SendSetTargetAPZCNotification for this class */
class DisplayportSetListener : public ManagedPostRefreshObserver {
 public:
  DisplayportSetListener(nsIWidget* aWidget, nsPresContext*,
                         const uint64_t& aInputBlockId,
                         nsTArray<ScrollableLayerGuid>&& aTargets);
  virtual ~DisplayportSetListener();
  void Register();

 private:
  RefPtr<nsIWidget> mWidget;
  uint64_t mInputBlockId;
  nsTArray<ScrollableLayerGuid> mTargets;

  void OnPostRefresh();
};

/* This class contains some helper methods that facilitate implementing the
   GeckoContentController callback interface required by the
   AsyncPanZoomController. Since different platforms need to implement this
   interface in similar-but- not-quite-the-same ways, this utility class
   provides some helpful methods to hold code that can be shared across the
   different platform implementations.
 */
class APZCCallbackHelper {
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;

 public:
  static void NotifyLayerTransforms(const nsTArray<MatrixMessage>& aTransforms);

  /* Applies the scroll and zoom parameters from the given RepaintRequest object
     to the root frame for the given metrics' scrollId. If tiled thebes layers
     are enabled, this will align the displayport to tile boundaries. Setting
     the scroll position can cause some small adjustments to be made to the
     actual scroll position. */
  static void UpdateRootFrame(const RepaintRequest& aRequest);

  /* Applies the scroll parameters from the given RepaintRequest object to the
     subframe corresponding to given metrics' scrollId. If tiled thebes
     layers are enabled, this will align the displayport to tile boundaries.
     Setting the scroll position can cause some small adjustments to be made
     to the actual scroll position. */
  static void UpdateSubFrame(const RepaintRequest& aRequest);

  /* Get the presShellId and view ID for the given content element.
   * If the view ID does not exist, one is created.
   * The pres shell ID should generally already exist; if it doesn't for some
   * reason, false is returned. */
  static bool GetOrCreateScrollIdentifiers(
      nsIContent* aContent, uint32_t* aPresShellIdOut,
      ScrollableLayerGuid::ViewID* aViewIdOut);

  /* Initialize a zero-margin displayport on the root document element of the
     given presShell. */
  static void InitializeRootDisplayport(PresShell* aPresShell);

  /* Get the pres context associated with the document enclosing |aContent|. */
  static nsPresContext* GetPresContextForContent(nsIContent* aContent);

  /* Get the pres shell associated with the root content document enclosing
   * |aContent|. */
  static PresShell* GetRootContentDocumentPresShellForContent(
      nsIContent* aContent);

  /* Dispatch a widget event via the widget stored in the event, if any.
   * In a child process, allows the BrowserParent event-capture mechanism to
   * intercept the event. */
  static nsEventStatus DispatchWidgetEvent(WidgetGUIEvent& aEvent);

  /* Synthesize a mouse event with the given parameters, and dispatch it
   * via the given widget. */
  static nsEventStatus DispatchSynthesizedMouseEvent(
      EventMessage aMsg, uint64_t aTime, const LayoutDevicePoint& aRefPoint,
      Modifiers aModifiers, int32_t aClickCount, nsIWidget* aWidget);

  /* Dispatch a mouse event with the given parameters.
   * Return whether or not any listeners have called preventDefault on the
   * event.
   * This is a lightweight wrapper around nsContentUtils::SendMouseEvent()
   * and as such expects |aPoint| to be in layout coordinates. */
  MOZ_CAN_RUN_SCRIPT
  static PreventDefaultResult DispatchMouseEvent(
      PresShell* aPresShell, const nsString& aType, const CSSPoint& aPoint,
      int32_t aButton, int32_t aClickCount, int32_t aModifiers,
      unsigned short aInputSourceArg, uint32_t aPointerId);

  /* Fire a single-tap event at the given point. The event is dispatched
   * via the given widget. */
  static void FireSingleTapEvent(const LayoutDevicePoint& aPoint,
                                 Modifiers aModifiers, int32_t aClickCount,
                                 nsIWidget* aWidget);

  /* Perform hit-testing on the touch points of |aEvent| to determine
   * which scrollable frames they target. If any of these frames don't have
   * a displayport, set one.
   *
   * If any displayports need to be set, this function returns a heap-allocated
   * object. The caller is responsible for calling Register() on that object.
   *
   * The object registers itself as a post-refresh observer on the presShell
   * and ensures that notifications get sent to APZ correctly after the
   * refresh.
   *
   * Having the caller manage this object is desirable in case they want to
   * (a) know about the fact that a displayport needs to be set, and
   * (b) register a post-refresh observer of their own that will run in
   *     a defined ordering relative to the APZ messages.
   */
  static already_AddRefed<DisplayportSetListener> SendSetTargetAPZCNotification(
      nsIWidget* aWidget, mozilla::dom::Document* aDocument,
      const WidgetGUIEvent& aEvent, const LayersId& aLayersId,
      uint64_t aInputBlockId);

  /* Figure out the allowed touch behaviors of each touch point in |aEvent|
   * and send that information to the provided callback. Also returns the
   * allowed touch behaviors. */
  static nsTArray<TouchBehaviorFlags> SendSetAllowedTouchBehaviorNotification(
      nsIWidget* aWidget, mozilla::dom::Document* aDocument,
      const WidgetTouchEvent& aEvent, uint64_t aInputBlockId,
      const SetAllowedTouchBehaviorCallback& aCallback);

  /* Notify content of a mouse scroll testing event. */
  static void NotifyMozMouseScrollEvent(
      const ScrollableLayerGuid::ViewID& aScrollId, const nsString& aEvent);

  /* Notify content that the repaint flush is complete. */
  static void NotifyFlushComplete(PresShell* aPresShell);

  static void NotifyAsyncScrollbarDragInitiated(
      uint64_t aDragBlockId, const ScrollableLayerGuid::ViewID& aScrollId,
      ScrollDirection aDirection);
  static void NotifyAsyncScrollbarDragRejected(
      const ScrollableLayerGuid::ViewID& aScrollId);
  static void NotifyAsyncAutoscrollRejected(
      const ScrollableLayerGuid::ViewID& aScrollId);

  static void CancelAutoscroll(const ScrollableLayerGuid::ViewID& aScrollId);

  /*
   * Check if the scrollable frame is currently in the middle of a main thread
   * async or smooth scroll, or has already requested some other apz scroll that
   * hasn't been acknowledged by apz.
   *
   * We want to discard apz updates to the main-thread scroll offset if this is
   * true to prevent clobbering higher priority origins.
   */
  static bool IsScrollInProgress(nsIScrollableFrame* aFrame);

  /* Notify content of the progress of a pinch gesture that APZ won't do
   * zooming for (because the apz.allow_zooming pref is false). This function
   * will dispatch appropriate WidgetSimpleGestureEvent events to gecko.
   */
  static void NotifyPinchGesture(PinchGestureInput::PinchGestureType aType,
                                 const LayoutDevicePoint& aFocusPoint,
                                 LayoutDeviceCoord aSpanChange,
                                 Modifiers aModifiers,
                                 const nsCOMPtr<nsIWidget>& aWidget);

 private:
  static uint64_t sLastTargetAPZCNotificationInputBlock;
};

}  // namespace layers
}  // namespace mozilla

#endif /* mozilla_layers_APZCCallbackHelper_h */
