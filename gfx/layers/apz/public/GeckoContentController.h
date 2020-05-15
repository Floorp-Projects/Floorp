/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_GeckoContentController_h
#define mozilla_layers_GeckoContentController_h

#include "InputData.h"              // for PinchGestureInput
#include "LayersTypes.h"            // for ScrollDirection
#include "Units.h"                  // for CSSPoint, CSSRect, etc
#include "mozilla/Assertions.h"     // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"     // for MOZ_CAN_RUN_SCRIPT
#include "mozilla/DefineEnum.h"     // for MOZ_DEFINE_ENUM
#include "mozilla/EventForwards.h"  // for Modifiers
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/MatrixMessage.h"        // for MatrixMessage
#include "mozilla/layers/RepaintRequest.h"       // for RepaintRequest
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid, etc
#include "nsISupportsImpl.h"

namespace mozilla {

class Runnable;

namespace layers {

class GeckoContentController {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GeckoContentController)

  /**
   * Notifies the content side of the most recently computed transforms for
   * each layers subtree to the root. The nsTArray will contain one
   *  MatrixMessage for each layers id in the current APZ tree, along with the
   * corresponding transform.
   */
  virtual void NotifyLayerTransforms(
      const nsTArray<MatrixMessage>& aTransforms) = 0;

  /**
   * Requests a paint of the given RepaintRequest |aRequest| from Gecko.
   * Implementations per-platform are responsible for actually handling this.
   *
   * This method must always be called on the repaint thread, which depends
   * on the GeckoContentController. For ChromeProcessController it is the
   * Gecko main thread, while for RemoteContentController it is the compositor
   * thread where it can send IPDL messages.
   */
  virtual void RequestContentRepaint(const RepaintRequest& aRequest) = 0;

  /**
   * Different types of tap-related events that can be sent in
   * the HandleTap function. The names should be relatively self-explanatory.
   * Note that the eLongTapUp will always be preceded by an eLongTap, but not
   * all eLongTap notifications will be followed by an eLongTapUp (for instance,
   * if the user moves their finger after triggering the long-tap but before
   * lifting it).
   * The difference between eDoubleTap and eSecondTap is subtle - the eDoubleTap
   * is for an actual double-tap "gesture" while eSecondTap is for the same user
   * input but where a double-tap gesture is not allowed. This is used to fire
   * a click event with detail=2 to web content (similar to what a mouse double-
   * click would do).
   */
  // clang-format off
  MOZ_DEFINE_ENUM_CLASS_AT_CLASS_SCOPE(
    TapType, (
      eSingleTap,
      eDoubleTap,
      eSecondTap,
      eLongTap,
      eLongTapUp
  ));
  // clang-format on

  /**
   * Requests handling of a tap event. |aPoint| is in LD pixels, relative to the
   * current scroll offset.
   */
  MOZ_CAN_RUN_SCRIPT
  virtual void HandleTap(TapType aType, const LayoutDevicePoint& aPoint,
                         Modifiers aModifiers, const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId) = 0;

  /**
   * When the apz.allow_zooming pref is set to false, the APZ will not
   * translate pinch gestures to actual zooming. Instead, it will call this
   * method to notify gecko of the pinch gesture, and allow it to deal with it
   * however it wishes. Note that this function is not called if the pinch is
   * prevented by content calling preventDefault() on the touch events, or via
   * use of the touch-action property.
   * @param aType One of PINCHGESTURE_START, PINCHGESTURE_SCALE,
   *        PINCHGESTURE_FINGERLIFTED, or PINCHGESTURE_END, indicating the phase
   *        of the pinch.
   * @param aGuid The guid of the APZ that is detecting the pinch. This is
   *        generally the root APZC for the layers id.
   * @param aSpanChange For the START or END event, this is always 0.
   *        For a SCALE event, this is the difference in span between the
   *        previous state and the new state.
   * @param aModifiers The keyboard modifiers depressed during the pinch.
   */
  virtual void NotifyPinchGesture(PinchGestureInput::PinchGestureType aType,
                                  const ScrollableLayerGuid& aGuid,
                                  LayoutDeviceCoord aSpanChange,
                                  Modifiers aModifiers) = 0;

  /**
   * Schedules a runnable to run on the controller thread at some time
   * in the future.
   * This method must always be called on the controller thread.
   */
  virtual void PostDelayedTask(already_AddRefed<Runnable> aRunnable,
                               int aDelayMs) {
    APZThreadUtils::DelayedDispatch(std::move(aRunnable), aDelayMs);
  }

  /**
   * Returns true if we are currently on the thread that can send repaint
   * requests.
   */
  virtual bool IsRepaintThread() = 0;

  /**
   * Runs the given task on the "repaint" thread.
   */
  virtual void DispatchToRepaintThread(already_AddRefed<Runnable> aTask) = 0;

  // clang-format off
  MOZ_DEFINE_ENUM_CLASS_AT_CLASS_SCOPE(
    APZStateChange, (
      /**
       * APZ started modifying the view (including panning, zooming, and fling).
       */
      eTransformBegin,
      /**
       * APZ finished modifying the view.
       */
      eTransformEnd,
      /**
       * APZ started a touch.
       * |aArg| is 1 if touch can be a pan, 0 otherwise.
       */
      eStartTouch,
      /**
       * APZ started a pan.
       */
      eStartPanning,
      /**
       * APZ finished processing a touch.
       * |aArg| is 1 if touch was a click, 0 otherwise.
       */
      eEndTouch
  ));
  // clang-format on

  /**
   * General notices of APZ state changes for consumers.
   * |aGuid| identifies the APZC originating the state change.
   * |aChange| identifies the type of state change
   * |aArg| is used by some state changes to pass extra information (see
   *        the documentation for each state change above)
   */
  virtual void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                    APZStateChange aChange, int aArg = 0) {}

  /**
   * Notify content of a MozMouseScrollFailed event.
   */
  virtual void NotifyMozMouseScrollEvent(
      const ScrollableLayerGuid::ViewID& aScrollId, const nsString& aEvent) {}

  /**
   * Notify content that the repaint requests have been flushed.
   */
  virtual void NotifyFlushComplete() = 0;

  /**
   * If the async scrollbar-drag initiation code kicks in on the APZ side, then
   * we need to let content know that we are dragging the scrollbar. Otherwise,
   * by the time the mousedown events is handled by content, the scrollthumb
   * could already have been moved via a RequestContentRepaint message at a
   * new scroll position, and the mousedown might end up triggering a click-to-
   * scroll on where the thumb used to be.
   */
  virtual void NotifyAsyncScrollbarDragInitiated(
      uint64_t aDragBlockId, const ScrollableLayerGuid::ViewID& aScrollId,
      ScrollDirection aDirection) = 0;
  virtual void NotifyAsyncScrollbarDragRejected(
      const ScrollableLayerGuid::ViewID& aScrollId) = 0;

  virtual void NotifyAsyncAutoscrollRejected(
      const ScrollableLayerGuid::ViewID& aScrollId) = 0;

  virtual void CancelAutoscroll(const ScrollableLayerGuid& aGuid) = 0;

  virtual void UpdateOverscrollVelocity(float aX, float aY,
                                        bool aIsRootContent) {}
  virtual void UpdateOverscrollOffset(float aX, float aY, bool aIsRootContent) {
  }

  GeckoContentController() = default;

  /**
   * Needs to be called on the main thread.
   */
  virtual void Destroy() {}

  /**
   * Whether this is RemoteContentController.
   */
  virtual bool IsRemote() { return false; }

 protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~GeckoContentController() = default;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_GeckoContentController_h
