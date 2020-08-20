/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZInputBridge.h"

#include "InputData.h"                      // for MouseInput, etc
#include "InputBlockState.h"                // for InputBlockState
#include "mozilla/dom/WheelEventBinding.h"  // for WheelEvent constants
#include "mozilla/EventStateManager.h"      // for EventStateManager
#include "mozilla/layers/APZThreadUtils.h"  // for AssertOnControllerThread, etc
#include "mozilla/MouseEvents.h"            // for WidgetMouseEvent
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_general.h"
#include "mozilla/StaticPrefs_test.h"
#include "mozilla/TextEvents.h"           // for WidgetKeyboardEvent
#include "mozilla/TouchEvents.h"          // for WidgetTouchEvent
#include "mozilla/WheelHandlingHelper.h"  // for WheelDeltaHorizontalizer,
                                          //     WheelDeltaAdjustmentStrategy

namespace mozilla {
namespace layers {

APZEventResult::APZEventResult()
    : mStatus(nsEventStatus_eIgnore),
      mTargetIsRoot(false),
      mInputBlockId(InputBlockState::NO_BLOCK_ID),
      mHitRegionWithApzAwareListeners(false) {}

static bool WillHandleMouseEvent(const WidgetMouseEventBase& aEvent) {
  return aEvent.mMessage == eMouseMove || aEvent.mMessage == eMouseDown ||
         aEvent.mMessage == eMouseUp || aEvent.mMessage == eDragEnd ||
         (StaticPrefs::test_events_async_enabled() &&
          aEvent.mMessage == eMouseHitTest);
}

/* static */
Maybe<APZWheelAction> APZInputBridge::ActionForWheelEvent(
    WidgetWheelEvent* aEvent) {
  if (!(aEvent->mDeltaMode == dom::WheelEvent_Binding::DOM_DELTA_LINE ||
        aEvent->mDeltaMode == dom::WheelEvent_Binding::DOM_DELTA_PIXEL ||
        aEvent->mDeltaMode == dom::WheelEvent_Binding::DOM_DELTA_PAGE)) {
    return Nothing();
  }
  return EventStateManager::APZWheelActionFor(aEvent);
}

APZEventResult APZInputBridge::ReceiveInputEvent(WidgetInputEvent& aEvent) {
  APZThreadUtils::AssertOnControllerThread();

  APZEventResult result;

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
        input.mOrigin =
            ScreenPoint(mouseEvent.mRefPoint.x, mouseEvent.mRefPoint.y);

        result = ReceiveInputEvent(input);

        mouseEvent.mRefPoint.x = input.mOrigin.x;
        mouseEvent.mRefPoint.y = input.mOrigin.y;
        mouseEvent.mFlags.mHandledByAPZ = input.mHandledByAPZ;
        mouseEvent.mFocusSequenceNumber = input.mFocusSequenceNumber;
        aEvent.mLayersId = input.mLayersId;
        return result;
      }

      ProcessUnhandledEvent(&mouseEvent.mRefPoint, &result.mTargetGuid,
                            &aEvent.mFocusSequenceNumber, &aEvent.mLayersId);
      return result;
    }
    case eTouchEventClass: {
      WidgetTouchEvent& touchEvent = *aEvent.AsTouchEvent();
      MultiTouchInput touchInput(touchEvent);
      result = ReceiveInputEvent(touchInput);
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
      aEvent.mLayersId = touchInput.mLayersId;
      return result;
    }
    case eWheelEventClass: {
      WidgetWheelEvent& wheelEvent = *aEvent.AsWheelEvent();

      if (Maybe<APZWheelAction> action = ActionForWheelEvent(&wheelEvent)) {
        ScrollWheelInput::ScrollMode scrollMode =
            ScrollWheelInput::SCROLLMODE_INSTANT;
        if (StaticPrefs::general_smoothScroll() &&
            ((wheelEvent.mDeltaMode ==
                  dom::WheelEvent_Binding::DOM_DELTA_LINE &&
              StaticPrefs::general_smoothScroll_mouseWheel()) ||
             (wheelEvent.mDeltaMode ==
                  dom::WheelEvent_Binding::DOM_DELTA_PAGE &&
              StaticPrefs::general_smoothScroll_pages()))) {
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
          ScrollWheelInput input(
              wheelEvent.mTime, wheelEvent.mTimeStamp, 0, scrollMode,
              ScrollWheelInput::DeltaTypeForDeltaMode(wheelEvent.mDeltaMode),
              origin, wheelEvent.mDeltaX, wheelEvent.mDeltaY,
              wheelEvent.mAllowToOverrideSystemScrollSpeed, strategy);
          input.mAPZAction = action.value();

          // We add the user multiplier as a separate field, rather than
          // premultiplying it, because if the input is converted back to a
          // WidgetWheelEvent, then EventStateManager would apply the delta a
          // second time. We could in theory work around this by asking ESM to
          // customize the event much sooner, and then save the
          // "mCustomizedByUserPrefs" bit on ScrollWheelInput - but for now,
          // this seems easier.
          EventStateManager::GetUserPrefsForWheelEvent(
              &wheelEvent, &input.mUserDeltaMultiplierX,
              &input.mUserDeltaMultiplierY);

          result = ReceiveInputEvent(input);
          wheelEvent.mRefPoint.x = input.mOrigin.x;
          wheelEvent.mRefPoint.y = input.mOrigin.y;
          wheelEvent.mFlags.mHandledByAPZ = input.mHandledByAPZ;
          wheelEvent.mFocusSequenceNumber = input.mFocusSequenceNumber;
          aEvent.mLayersId = input.mLayersId;

          return result;
        }
      }

      UpdateWheelTransaction(aEvent.mRefPoint, aEvent.mMessage);
      ProcessUnhandledEvent(&aEvent.mRefPoint, &result.mTargetGuid,
                            &aEvent.mFocusSequenceNumber, &aEvent.mLayersId);
      result.mStatus = nsEventStatus_eIgnore;
      return result;
    }
    case eKeyboardEventClass: {
      WidgetKeyboardEvent& keyboardEvent = *aEvent.AsKeyboardEvent();

      KeyboardInput input(keyboardEvent);

      result = ReceiveInputEvent(input);

      keyboardEvent.mFlags.mHandledByAPZ = input.mHandledByAPZ;
      keyboardEvent.mFocusSequenceNumber = input.mFocusSequenceNumber;
      return result;
    }
    default: {
      UpdateWheelTransaction(aEvent.mRefPoint, aEvent.mMessage);
      ProcessUnhandledEvent(&aEvent.mRefPoint, &result.mTargetGuid,
                            &aEvent.mFocusSequenceNumber, &aEvent.mLayersId);
      return result;
    }
  }

  MOZ_ASSERT_UNREACHABLE("Invalid WidgetInputEvent type.");
  result.mStatus = nsEventStatus_eConsumeNoDefault;
  return result;
}

}  // namespace layers
}  // namespace mozilla
