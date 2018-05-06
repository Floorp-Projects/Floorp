/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZInputBridge.h"

#include "gfxPrefs.h"                       // for gfxPrefs
#include "InputData.h"                      // for MouseInput, etc
#include "mozilla/dom/WheelEventBinding.h"  // for WheelEvent constants
#include "mozilla/EventStateManager.h"      // for EventStateManager
#include "mozilla/layers/APZThreadUtils.h"  // for AssertOnControllerThread, etc
#include "mozilla/MouseEvents.h"            // for WidgetMouseEvent
#include "mozilla/TextEvents.h"             // for WidgetKeyboardEvent
#include "mozilla/TouchEvents.h"            // for WidgetTouchEvent
#include "mozilla/WheelHandlingHelper.h"    // for WheelDeltaHorizontalizer,
                                            //     WheelDeltaAdjustmentStrategy

namespace mozilla {
namespace layers {

static bool
WillHandleMouseEvent(const WidgetMouseEventBase& aEvent)
{
  return aEvent.mMessage == eMouseMove ||
         aEvent.mMessage == eMouseDown ||
         aEvent.mMessage == eMouseUp ||
         aEvent.mMessage == eDragEnd ||
         (gfxPrefs::TestEventsAsyncEnabled() && aEvent.mMessage == eMouseHitTest);
}

/* static */ Maybe<APZWheelAction>
APZInputBridge::ActionForWheelEvent(WidgetWheelEvent* aEvent)
{
  if (!(aEvent->mDeltaMode == dom::WheelEventBinding::DOM_DELTA_LINE ||
        aEvent->mDeltaMode == dom::WheelEventBinding::DOM_DELTA_PIXEL ||
        aEvent->mDeltaMode == dom::WheelEventBinding::DOM_DELTA_PAGE)) {
    return Nothing();
  }
  return EventStateManager::APZWheelActionFor(aEvent);
}

nsEventStatus
APZInputBridge::ReceiveInputEvent(
    WidgetInputEvent& aEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  APZThreadUtils::AssertOnControllerThread();

  // Initialize aOutInputBlockId to a sane value, and then later we overwrite
  // it if the input event goes into a block.
  if (aOutInputBlockId) {
    *aOutInputBlockId = 0;
  }

  switch (aEvent.mClass) {
    case eMouseEventClass:
    case eDragEventClass: {

      WidgetMouseEvent& mouseEvent = *aEvent.AsMouseEvent();

      // Note, we call this before having transformed the reference point.
      if (mouseEvent.IsReal()) {
        UpdateWheelTransaction(mouseEvent.mRefPoint, mouseEvent.mMessage);
      }

      if (WillHandleMouseEvent(mouseEvent)) {

        MouseInput input(mouseEvent);
        input.mOrigin = ScreenPoint(mouseEvent.mRefPoint.x, mouseEvent.mRefPoint.y);

        nsEventStatus status = ReceiveInputEvent(input, aOutTargetGuid, aOutInputBlockId);

        mouseEvent.mRefPoint.x = input.mOrigin.x;
        mouseEvent.mRefPoint.y = input.mOrigin.y;
        mouseEvent.mFlags.mHandledByAPZ = input.mHandledByAPZ;
        mouseEvent.mFocusSequenceNumber = input.mFocusSequenceNumber;
        return status;

      }

      ProcessUnhandledEvent(&mouseEvent.mRefPoint, aOutTargetGuid, &aEvent.mFocusSequenceNumber);
      return nsEventStatus_eIgnore;
    }
    case eTouchEventClass: {

      WidgetTouchEvent& touchEvent = *aEvent.AsTouchEvent();
      MultiTouchInput touchInput(touchEvent);
      nsEventStatus result = ReceiveInputEvent(touchInput, aOutTargetGuid, aOutInputBlockId);
      // touchInput was modified in-place to possibly remove some
      // touch points (if we are overscrolled), and the coordinates were
      // modified using the APZ untransform. We need to copy these changes
      // back into the WidgetInputEvent.
      touchEvent.mTouches.Clear();
      touchEvent.mTouches.SetCapacity(touchInput.mTouches.Length());
      for (size_t i = 0; i < touchInput.mTouches.Length(); i++) {
        *touchEvent.mTouches.AppendElement() =
          touchInput.mTouches[i].ToNewDOMTouch();
      }
      touchEvent.mFlags.mHandledByAPZ = touchInput.mHandledByAPZ;
      touchEvent.mFocusSequenceNumber = touchInput.mFocusSequenceNumber;
      return result;

    }
    case eWheelEventClass: {
      WidgetWheelEvent& wheelEvent = *aEvent.AsWheelEvent();

      if (Maybe<APZWheelAction> action = ActionForWheelEvent(&wheelEvent)) {

        ScrollWheelInput::ScrollMode scrollMode = ScrollWheelInput::SCROLLMODE_INSTANT;
        if (gfxPrefs::SmoothScrollEnabled() &&
            ((wheelEvent.mDeltaMode == dom::WheelEventBinding::DOM_DELTA_LINE &&
              gfxPrefs::WheelSmoothScrollEnabled()) ||
             (wheelEvent.mDeltaMode == dom::WheelEventBinding::DOM_DELTA_PAGE &&
              gfxPrefs::PageSmoothScrollEnabled())))
        {
          scrollMode = ScrollWheelInput::SCROLLMODE_SMOOTH;
        }

        WheelDeltaAdjustmentStrategy strategy =
          EventStateManager::GetWheelDeltaAdjustmentStrategy(wheelEvent);
        // Adjust the delta values of the wheel event if the current default
        // action is to horizontalize scrolling. I.e., deltaY values are set to
        // deltaX and deltaY and deltaZ values are set to 0.
        // If horizontalized, the delta values will be restored and its overflow
        // deltaX will become 0 when the WheelDeltaHorizontalizer instance is
        // being destroyed.
        WheelDeltaHorizontalizer horizontalizer(wheelEvent);
        if (WheelDeltaAdjustmentStrategy::eHorizontalize == strategy) {
          horizontalizer.Horizontalize();
        }

        // If the wheel event becomes no-op event, don't handle it as scroll.
        if (wheelEvent.mDeltaX || wheelEvent.mDeltaY) {
          ScreenPoint origin(wheelEvent.mRefPoint.x, wheelEvent.mRefPoint.y);
          ScrollWheelInput input(wheelEvent.mTime, wheelEvent.mTimeStamp, 0,
                                 scrollMode,
                                 ScrollWheelInput::DeltaTypeForDeltaMode(
                                                     wheelEvent.mDeltaMode),
                                 origin,
                                 wheelEvent.mDeltaX, wheelEvent.mDeltaY,
                                 wheelEvent.mAllowToOverrideSystemScrollSpeed,
                                 strategy);
          input.mAPZAction = action.value();

          // We add the user multiplier as a separate field, rather than premultiplying
          // it, because if the input is converted back to a WidgetWheelEvent, then
          // EventStateManager would apply the delta a second time. We could in theory
          // work around this by asking ESM to customize the event much sooner, and
          // then save the "mCustomizedByUserPrefs" bit on ScrollWheelInput - but for
          // now, this seems easier.
          EventStateManager::GetUserPrefsForWheelEvent(&wheelEvent,
            &input.mUserDeltaMultiplierX,
            &input.mUserDeltaMultiplierY);

          nsEventStatus status = ReceiveInputEvent(input, aOutTargetGuid, aOutInputBlockId);
          wheelEvent.mRefPoint.x = input.mOrigin.x;
          wheelEvent.mRefPoint.y = input.mOrigin.y;
          wheelEvent.mFlags.mHandledByAPZ = input.mHandledByAPZ;
          wheelEvent.mFocusSequenceNumber = input.mFocusSequenceNumber;
          return status;
        }
      }

      UpdateWheelTransaction(aEvent.mRefPoint, aEvent.mMessage);
      ProcessUnhandledEvent(&aEvent.mRefPoint, aOutTargetGuid, &aEvent.mFocusSequenceNumber);
      return nsEventStatus_eIgnore;

    }
    case eKeyboardEventClass: {
      WidgetKeyboardEvent& keyboardEvent = *aEvent.AsKeyboardEvent();

      KeyboardInput input(keyboardEvent);

      nsEventStatus status = ReceiveInputEvent(input, aOutTargetGuid, aOutInputBlockId);

      keyboardEvent.mFlags.mHandledByAPZ = input.mHandledByAPZ;
      keyboardEvent.mFocusSequenceNumber = input.mFocusSequenceNumber;
      return status;
    }
    default: {

      UpdateWheelTransaction(aEvent.mRefPoint, aEvent.mMessage);
      ProcessUnhandledEvent(&aEvent.mRefPoint, aOutTargetGuid, &aEvent.mFocusSequenceNumber);
      return nsEventStatus_eIgnore;

    }
  }

  MOZ_ASSERT_UNREACHABLE("Invalid WidgetInputEvent type.");
  return nsEventStatus_eConsumeNoDefault;
}

} // namespace layers
} // namespace mozilla
