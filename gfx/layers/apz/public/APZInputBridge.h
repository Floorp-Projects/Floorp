/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZInputBridge_h
#define mozilla_layers_APZInputBridge_h

#include "APZUtils.h"               // for APZWheelAction
#include "mozilla/EventForwards.h"  // for WidgetInputEvent, nsEventStatus
#include "Units.h"                  // for LayoutDeviceIntPoint

namespace mozilla {

class InputData;

namespace layers {

class APZInputBridgeParent;
struct ScrollableLayerGuid;

/**
 * Represents the outcome of APZ receiving and processing an input event.
 * This is returned from APZInputBridge::ReceiveInputEvent() and related APIs.
 */
struct APZEventResult {
  /**
   * Creates a default result with a status of eIgnore, no block ID, and empty
   * target guid.
   */
  APZEventResult();

  /**
   * A status flag indicated how APZ handled the event.
   * The interpretation of each value is as follows:
   *
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
   */
  nsEventStatus mStatus;
  /**
   * The guid of the APZC this event was delivered to.
   */
  ScrollableLayerGuid mTargetGuid;
  /**
   * Whether or not mTargetGuid refers to the root content APZC
   */
  bool mTargetIsRoot;
  /**
   * If this event started or was added to an input block, the id of that
   * input block, otherwise InputBlockState::NO_BLOCK_ID.
   */
  uint64_t mInputBlockId;
  /**
   * True if the event is targeting a region with non-passive APZ-aware
   * listeners, that is, a region where we need to dispatch the event to Gecko
   * to see if a listener will prevent-default it.
   * Notes:
   *   1) This is currently only set for touch events.
   *   2) For non-WebRender, this will have some false positives; it will
   *      be set in some cases where we need to dispatch the event to Gecko
   *      before handling for other reasons than APZ-aware listeners.
   */
  bool mHitRegionWithApzAwareListeners;
};

/**
 * This class lives in the main process, and is accessed via the controller
 * thread (which is the process main thread for desktop, and the Java UI
 * thread for Android). This class exposes a synchronous API to deliver
 * incoming input events to APZ and modify them in-place to unapply the APZ
 * async transform. If there is a GPU process, then this class does sync IPC
 * calls over to the GPU process in order to accomplish this. Otherwise,
 * APZCTreeManager overrides and implements these methods directly.
 */
class APZInputBridge {
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
   * @param aEvent input event object; is modified in-place
   * @return The result of processing the event. Refer to the documentation of
   * APZEventResult and its field.
   */
  virtual APZEventResult ReceiveInputEvent(InputData& aEvent) = 0;

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
  APZEventResult ReceiveInputEvent(WidgetInputEvent& aEvent);

  // Returns the kind of wheel event action, if any, that will be (or was)
  // performed by APZ. If this returns true, the event must not perform a
  // synchronous scroll.
  //
  // Even if this returns Nothing(), all wheel events in APZ-aware widgets must
  // be sent through APZ so they are transformed correctly for BrowserParent.
  static Maybe<APZWheelAction> ActionForWheelEvent(WidgetWheelEvent* aEvent);

 protected:
  friend class APZInputBridgeParent;

  // Methods to help process WidgetInputEvents (or manage conversion to/from
  // InputData)

  virtual void ProcessUnhandledEvent(LayoutDeviceIntPoint* aRefPoint,
                                     ScrollableLayerGuid* aOutTargetGuid,
                                     uint64_t* aOutFocusSequenceNumber,
                                     LayersId* aOutLayersId) = 0;

  virtual void UpdateWheelTransaction(LayoutDeviceIntPoint aRefPoint,
                                      EventMessage aEventMessage) = 0;

  virtual ~APZInputBridge() = default;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZInputBridge_h
