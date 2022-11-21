/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZInputBridge_h
#define mozilla_layers_APZInputBridge_h

#include "Units.h"                  // for LayoutDeviceIntPoint
#include "mozilla/EventForwards.h"  // for WidgetInputEvent, nsEventStatus
#include "mozilla/layers/APZPublicUtils.h"       // for APZWheelAction
#include "mozilla/layers/LayersTypes.h"          // for ScrollDirections
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid

namespace mozilla {

class InputData;

namespace layers {

class APZInputBridgeParent;
class AsyncPanZoomController;
class InputBlockState;
struct ScrollableLayerGuid;
struct TargetConfirmationFlags;

enum class APZHandledPlace : uint8_t {
  Unhandled = 0,         // we know for sure that the event will not be handled
                         // by either the root APZC or others
  HandledByRoot = 1,     // we know for sure that the event will be handled
                         // by the root content APZC
  HandledByContent = 2,  // we know for sure it will be handled by a non-root
                         // APZC or by an event listener using preventDefault()
                         // in a document
  Invalid = 3,
  Last = Invalid
};

struct APZHandledResult {
  APZHandledPlace mPlace = APZHandledPlace::Invalid;
  SideBits mScrollableDirections = SideBits::eNone;
  ScrollDirections mOverscrollDirections = ScrollDirections();

  APZHandledResult() = default;
  // A constructor for cases where we have the target of the input block this
  // event is part of, the target might be adjusted to be the root in the
  // ScrollingDownWillMoveDynamicToolbar case.
  //
  // NOTE: There's a case where |aTarget| is the APZC for the root content but
  // |aPlace| has to be `HandledByContent`, for example, the root content has
  // an event handler using preventDefault() in the callback, so call sites of
  // this function should be responsible to set a proper |aPlace|.
  APZHandledResult(APZHandledPlace aPlace,
                   const AsyncPanZoomController* aTarget);
  APZHandledResult(APZHandledPlace aPlace, SideBits aScrollableDirections,
                   ScrollDirections aOverscrollDirections)
      : mPlace(aPlace),
        mScrollableDirections(aScrollableDirections),
        mOverscrollDirections(aOverscrollDirections) {}

  bool IsHandledByContent() const {
    return mPlace == APZHandledPlace::HandledByContent;
  }
  bool IsHandledByRoot() const {
    return mPlace == APZHandledPlace::HandledByRoot;
  }
  bool operator==(const APZHandledResult& aOther) const {
    return mPlace == aOther.mPlace &&
           mScrollableDirections == aOther.mScrollableDirections &&
           mOverscrollDirections == aOther.mOverscrollDirections;
  }
};

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
   * Creates a result with a status of eIgnore, no block ID, the guid of the
   * given initial target, and an APZHandledResult if we are sure the event
   * is not going to be dispatched to contents.
   */
  APZEventResult(const RefPtr<AsyncPanZoomController>& aInitialTarget,
                 TargetConfirmationFlags aFlags);

  void SetStatusAsConsumeNoDefault() {
    mStatus = nsEventStatus_eConsumeNoDefault;
  }

  void SetStatusAsIgnore() { mStatus = nsEventStatus_eIgnore; }

  // Set mStatus to nsEventStatus_eConsumeDoDefault and set mHandledResult
  // depending on |aTarget|.
  void SetStatusAsConsumeDoDefault(
      const RefPtr<AsyncPanZoomController>& aTarget);
  // Set mStatus to nsEventStatus_eConsumeDoDefault and set mHandledResult
  // depending on |aBlock|'s target APZC.
  void SetStatusAsConsumeDoDefault(const InputBlockState& aBlock);
  // Smilar to above two functions, but we need to use this function if it's
  // possible that the event needs to be handled as if it's consumed by the root
  // APZC in the case where the target APZC area is covered by dynamic toolbar
  // so that browser apps can move the toolbar corresponding to the event.
  void SetStatusAsConsumeDoDefaultWithTargetConfirmationFlags(
      const InputBlockState& aBlock, TargetConfirmationFlags aFlags,
      const AsyncPanZoomController& aTarget);

  // DO NOT USE THIS UpdateStatus DIRECTLY. THIS FUNCTION IS ONLY FOR
  // SERIALIZATION / DESERIALIZATION OF THIS STRUCT IN IPC.
  void UpdateStatus(nsEventStatus aStatus) { mStatus = aStatus; }
  nsEventStatus GetStatus() const { return mStatus; };

  // DO NOT USE THIS UpdateHandledResult DIRECTLY. THIS FUNCTION IS ONLY FOR
  // SERIALIZATION / DESERIALIZATION OF THIS STRUCT IN IPC.
  void UpdateHandledResult(const Maybe<APZHandledResult>& aHandledResult) {
    mHandledResult = aHandledResult;
  }
  const Maybe<APZHandledResult>& GetHandledResult() const {
    return mHandledResult;
  }

  bool WillHaveDelayedResult() const {
    return GetStatus() == nsEventStatus_eConsumeDoDefault &&
           !GetHandledResult();
  }

 private:
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
   * This is:
   *  - set to HandledByRoot if we know for sure that the event will be handled
   *    by the root content APZC;
   *  - set to HandledByContent if we know for sure it will not be;
   *  - left empty if we are unsure.
   */
  Maybe<APZHandledResult> mHandledResult;

 public:
  /**
   * The guid of the APZC initially targeted by this event.
   * This will usually be the APZC that handles the event, but in cases
   * where the event is dispatched to content, it may end up being
   * handled by a different APZC.
   */
  ScrollableLayerGuid mTargetGuid;
  /**
   * If this event started or was added to an input block, the id of that
   * input block, otherwise InputBlockState::NO_BLOCK_ID.
   */
  uint64_t mInputBlockId;
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
  using InputBlockCallback = std::function<void(
      uint64_t aInputBlockId, const APZHandledResult& aHandledResult)>;

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
   * @param aCallback an optional callback to be invoked when the input block is
   * ready for handling,
   * @return The result of processing the event. Refer to the documentation of
   * APZEventResult and its field.
   */
  virtual APZEventResult ReceiveInputEvent(
      InputData& aEvent,
      InputBlockCallback&& aCallback = InputBlockCallback()) = 0;

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
  APZEventResult ReceiveInputEvent(
      WidgetInputEvent& aEvent,
      InputBlockCallback&& aCallback = InputBlockCallback());

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

  virtual void UpdateWheelTransaction(
      LayoutDeviceIntPoint aRefPoint, EventMessage aEventMessage,
      const Maybe<ScrollableLayerGuid>& aTargetGuid) = 0;

  virtual ~APZInputBridge() = default;
};

std::ostream& operator<<(std::ostream& aOut,
                         const APZHandledResult& aHandledResult);

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZInputBridge_h
