/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_IAPZCTreeManager_h
#define mozilla_layers_IAPZCTreeManager_h

#include <stdint.h>                     // for uint64_t, uint32_t

#include "FrameMetrics.h"               // for FrameMetrics, etc
#include "mozilla/EventForwards.h"      // for WidgetInputEvent, nsEventStatus
#include "mozilla/layers/APZUtils.h"    // for HitTestResult
#include "nsTArrayForwardDeclare.h"     // for nsTArray, nsTArray_Impl, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "Units.h"                      // for CSSPoint, CSSRect, etc

namespace mozilla {
class InputData;

namespace layers {

enum AllowedTouchBehavior {
  NONE =               0,
  VERTICAL_PAN =       1 << 0,
  HORIZONTAL_PAN =     1 << 1,
  PINCH_ZOOM =         1 << 2,
  DOUBLE_TAP_ZOOM =    1 << 3,
  UNKNOWN =            1 << 4
};

enum ZoomToRectBehavior : uint32_t {
  DEFAULT_BEHAVIOR =   0,
  DISABLE_ZOOM_OUT =   1 << 0,
  PAN_INTO_VIEW_ONLY = 1 << 1,
  ONLY_ZOOM_TO_DEFAULT_SCALE  = 1 << 2
};

class AsyncDragMetrics;

class IAPZCTreeManager {
  NS_INLINE_DECL_THREADSAFE_VIRTUAL_REFCOUNTING(IAPZCTreeManager)

public:

  /**
   * General handler for incoming input events. Manipulates the frame metrics
   * based on what type of input it is. For example, a PinchGestureEvent will
   * cause scaling. This should only be called externally to this class, and
   * must be called on the controller thread.
   *
   * This function transforms |aEvent| to have its coordinates in DOM space.
   * This is so that the event can be passed through the DOM and content can
   * handle them. The event may need to be converted to a WidgetInputEvent
   * by the caller if it wants to do this.
   *
   * The following values may be returned by this function:
   * nsEventStatus_eConsumeNoDefault is returned to indicate the
   *   APZ is consuming this event and the caller should discard the event with
   *   extreme prejudice. The exact scenarios under which this is returned is
   *   implementation-dependent and may vary.
   * nsEventStatus_eIgnore is returned to indicate that the APZ code didn't
   *   use this event. This might be because it was directed at a point on
   *   the screen where there was no APZ, or because the thing the user was
   *   trying to do was not allowed. (For example, attempting to pan a
   *   non-pannable document).
   * nsEventStatus_eConsumeDoDefault is returned to indicate that the APZ
   *   code may have used this event to do some user-visible thing. Note that
   *   in some cases CONSUMED is returned even if the event was NOT used. This
   *   is because we cannot always know at the time of event delivery whether
   *   the event will be used or not. So we err on the side of sending
   *   CONSUMED when we are uncertain.
   *
   * @param aEvent input event object; is modified in-place
   * @param aOutTargetGuid returns the guid of the apzc this event was
   * delivered to. May be null.
   * @param aOutInputBlockId returns the id of the input block that this event
   * was added to, if that was the case. May be null.
   */
  virtual nsEventStatus ReceiveInputEvent(
      InputData& aEvent,
      ScrollableLayerGuid* aOutTargetGuid,
      uint64_t* aOutInputBlockId) = 0;

  /**
   * WidgetInputEvent handler. Transforms |aEvent| (which is assumed to be an
   * already-existing instance of an WidgetInputEvent which may be an
   * WidgetTouchEvent) to have its coordinates in DOM space. This is so that the
   * event can be passed through the DOM and content can handle them.
   *
   * NOTE: Be careful of invoking the WidgetInputEvent variant. This can only be
   * called on the main thread. See widget/InputData.h for more information on
   * why we have InputData and WidgetInputEvent separated. If this function is
   * used, the controller thread must be the main thread, or undefined behaviour
   * may occur.
   * NOTE: On unix, mouse events are treated as touch and are forwarded
   * to the appropriate apz as such.
   *
   * See documentation for other ReceiveInputEvent above.
   */
  nsEventStatus ReceiveInputEvent(
      WidgetInputEvent& aEvent,
      ScrollableLayerGuid* aOutTargetGuid,
      uint64_t* aOutInputBlockId);

  /**
   * Kicks an animation to zoom to a rect. This may be either a zoom out or zoom
   * in. The actual animation is done on the compositor thread after being set
   * up. |aRect| must be given in CSS pixels, relative to the document.
   * |aFlags| is a combination of the ZoomToRectBehavior enum values.
   */
  virtual void ZoomToRect(
      const ScrollableLayerGuid& aGuid,
      const CSSRect& aRect,
      const uint32_t aFlags = DEFAULT_BEHAVIOR) = 0;

  /**
   * If we have touch listeners, this should always be called when we know
   * definitively whether or not content has preventDefaulted any touch events
   * that have come in. If |aPreventDefault| is true, any touch events in the
   * queue will be discarded. This function must be called on the controller
   * thread.
   */
  virtual void ContentReceivedInputBlock(
      uint64_t aInputBlockId,
      bool aPreventDefault) = 0;

  /**
   * When the event regions code is enabled, this function should be invoked to
   * to confirm the target of the input block. This is only needed in cases
   * where the initial input event of the block hit a dispatch-to-content region
   * but is safe to call for all input blocks. This function should always be
   * invoked on the controller thread.
   * The different elements in the array of targets correspond to the targets
   * for the different touch points. In the case where the touch point has no
   * target, or the target is not a scrollable frame, the target's |mScrollId|
   * should be set to FrameMetrics::NULL_SCROLL_ID.
   */
  virtual void SetTargetAPZC(
      uint64_t aInputBlockId,
      const nsTArray<ScrollableLayerGuid>& aTargets) = 0;

  /**
   * Updates any zoom constraints contained in the <meta name="viewport"> tag.
   * If the |aConstraints| is Nothing() then previously-provided constraints for
   * the given |aGuid| are cleared.
   */
  virtual void UpdateZoomConstraints(
      const ScrollableLayerGuid& aGuid,
      const Maybe<ZoomConstraints>& aConstraints) = 0;

  /**
   * Cancels any currently running animation. Note that all this does is set the
   * state of the AsyncPanZoomController back to NOTHING, but it is the
   * animation's responsibility to check this before advancing.
   */
  virtual void CancelAnimation(const ScrollableLayerGuid &aGuid) = 0;

  virtual void SetDPI(float aDpiValue) = 0;

  /**
   * Sets allowed touch behavior values for current touch-session for specific
   * input block (determined by aInputBlock).
   * Should be invoked by the widget. Each value of the aValues arrays
   * corresponds to the different touch point that is currently active.
   * Must be called after receiving the TOUCH_START event that starts the
   * touch-session.
   * This must be called on the controller thread.
   */
  virtual void SetAllowedTouchBehavior(
      uint64_t aInputBlockId,
      const nsTArray<TouchBehaviorFlags>& aValues) = 0;

  virtual void StartScrollbarDrag(
      const ScrollableLayerGuid& aGuid,
      const AsyncDragMetrics& aDragMetrics) = 0;

  /**
   * Function used to disable LongTap gestures.
   *
   * On slow running tests, drags and touch events can be misinterpreted
   * as a long tap. This allows tests to disable long tap gesture detection.
   */
  virtual void SetLongTapEnabled(bool aTapGestureEnabled) = 0;

  /**
   * Process touch velocity.
   * Sometimes the touch move event will have a velocity even though no scrolling
   * is occurring such as when the toolbar is being hidden/shown in Fennec.
   * This function can be called to have the y axis' velocity queue updated.
   */
  virtual void ProcessTouchVelocity(uint32_t aTimestampMs, float aSpeedY) = 0;

protected:

  // Methods to help process WidgetInputEvents (or manage conversion to/from InputData)

  virtual void TransformEventRefPoint(
      LayoutDeviceIntPoint* aRefPoint,
      ScrollableLayerGuid* aOutTargetGuid) = 0;

  virtual void UpdateWheelTransaction(
      LayoutDeviceIntPoint aRefPoint,
      EventMessage aEventMessage) = 0;

  // Discourage destruction outside of decref

  virtual ~IAPZCTreeManager() { }
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_IAPZCTreeManager_h
