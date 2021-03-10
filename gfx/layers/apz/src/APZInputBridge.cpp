/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZInputBridge.h"

#include "AsyncPanZoomController.h"
#include "InputData.h"                      // for MouseInput, etc
#include "InputBlockState.h"                // for InputBlockState
#include "OverscrollHandoffState.h"         // for OverscrollHandoffState
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
      mInputBlockId(InputBlockState::NO_BLOCK_ID) {}

APZEventResult::APZEventResult(
    const RefPtr<AsyncPanZoomController>& aInitialTarget,
    TargetConfirmationFlags aFlags)
    : APZEventResult() {
  mHandledResult = [&]() -> Maybe<APZHandledResult> {
    if (!aInitialTarget->IsRootContent()) {
      // If the initial target is not the root, this will definitely not be
      // handled by the root. (The confirmed target is either the initial
      // target, or a descendant.)
      return Some(
          APZHandledResult{APZHandledPlace::HandledByContent, aInitialTarget});
    }

    if (!aFlags.mDispatchToContent) {
      // If the initial target is the root and we don't need to dispatch to
      // content, the event will definitely be handled by the root.
      return Some(
          APZHandledResult{APZHandledPlace::HandledByRoot, aInitialTarget});
    }

    // Otherwise, we're not sure.
    return Nothing();
  }();
  aInitialTarget->GetGuid(&mTargetGuid);
}

void APZEventResult::SetStatusAsConsumeDoDefault(
    const InputBlockState& aBlock) {
  SetStatusAsConsumeDoDefault(aBlock.GetTargetApzc());
}

void APZEventResult::SetStatusAsConsumeDoDefault(
    const RefPtr<AsyncPanZoomController>& aTarget) {
  mStatus = nsEventStatus_eConsumeDoDefault;
  mHandledResult =
      Some(aTarget && aTarget->IsRootContent()
               ? APZHandledResult{APZHandledPlace::HandledByRoot, aTarget}
               : APZHandledResult{APZHandledPlace::HandledByContent, aTarget});
}

void APZEventResult::SetStatusAsConsumeDoDefaultWithTargetConfirmationFlags(
    const InputBlockState& aBlock, TargetConfirmationFlags aFlags,
    const AsyncPanZoomController& aTarget) {
  mStatus = nsEventStatus_eConsumeDoDefault;

  if (!aTarget.IsRootContent()) {
    auto [result, rootApzc] =
        aBlock.GetOverscrollHandoffChain()->ScrollingDownWillMoveDynamicToolbar(
            &aTarget);
    if (result) {
      MOZ_ASSERT(rootApzc && rootApzc->IsRootContent());
      // The event is actually consumed by a non-root APZC but scroll
      // positions in all relevant APZCs are at the bottom edge, so if there's
      // still contents covered by the dynamic toolbar we need to move the
      // dynamic toolbar to make the covered contents visible, thus we need
      // to tell it to GeckoView so we handle it as if it's consumed in the
      // root APZC.
      // IMPORTANT NOTE: If the incoming TargetConfirmationFlags has
      // mDispatchToContent, we need to change it to Nothing() so that
      // GeckoView can properly wait for results from the content on the
      // main-thread.
      mHandledResult = aFlags.mDispatchToContent
                           ? Nothing()
                           : Some(APZHandledResult{
                                 APZHandledPlace::HandledByRoot, rootApzc});
    }
  }
}

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
      MOZ_ASSERT(result.GetStatus() == nsEventStatus_eIgnore);
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
  result.SetStatusAsConsumeNoDefault();
  return result;
}

APZHandledResult::APZHandledResult(APZHandledPlace aPlace,
                                   const AsyncPanZoomController* aTarget)
    : mPlace(aPlace) {
  MOZ_ASSERT(aTarget);
  switch (aPlace) {
    case APZHandledPlace::Unhandled:
      break;
    case APZHandledPlace::HandledByContent:
      if (aTarget) {
        mScrollableDirections = aTarget->ScrollableDirections();
        mOverscrollDirections = aTarget->GetAllowedHandoffDirections();
      }
      break;
    case APZHandledPlace::HandledByRoot: {
      MOZ_ASSERT(aTarget->IsRootContent());
      if (aTarget) {
        mScrollableDirections = aTarget->ScrollableDirections();
        mOverscrollDirections = aTarget->GetAllowedHandoffDirections();
      }
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid APZHandledPlace");
      break;
  }
}

std::ostream& operator<<(std::ostream& aOut, const SideBits& aSideBits) {
  if ((aSideBits & SideBits::eAll) == SideBits::eAll) {
    aOut << "all";
  } else {
    AutoTArray<nsCString, 4> strings;
    if (aSideBits & SideBits::eTop) {
      strings.AppendElement("top"_ns);
    }
    if (aSideBits & SideBits::eRight) {
      strings.AppendElement("right"_ns);
    }
    if (aSideBits & SideBits::eBottom) {
      strings.AppendElement("bottom"_ns);
    }
    if (aSideBits & SideBits::eLeft) {
      strings.AppendElement("left"_ns);
    }
    aOut << strings;
  }
  return aOut;
}

std::ostream& operator<<(std::ostream& aOut,
                         const ScrollDirections& aScrollDirections) {
  if (aScrollDirections.contains(EitherScrollDirection)) {
    aOut << "either";
  } else if (aScrollDirections.contains(HorizontalScrollDirection)) {
    aOut << "horizontal";
  } else if (aScrollDirections.contains(VerticalScrollDirection)) {
    aOut << "vertical";
  } else {
    aOut << "none";
  }
  return aOut;
}

std::ostream& operator<<(std::ostream& aOut,
                         const APZHandledPlace& aHandledPlace) {
  switch (aHandledPlace) {
    case APZHandledPlace::Unhandled:
      aOut << "unhandled";
      break;
    case APZHandledPlace::HandledByRoot: {
      aOut << "handled-by-root";
      break;
    }
    case APZHandledPlace::HandledByContent: {
      aOut << "handled-by-content";
      break;
    }
    case APZHandledPlace::Invalid: {
      aOut << "INVALID";
      break;
    }
  }
  return aOut;
}

std::ostream& operator<<(std::ostream& aOut,
                         const APZHandledResult& aHandledResult) {
  aOut << "handled: " << aHandledResult.mPlace << ", ";
  aOut << "scrollable: " << aHandledResult.mScrollableDirections << ", ";
  aOut << "overscroll: " << aHandledResult.mOverscrollDirections << std::endl;
  return aOut;
}

}  // namespace layers
}  // namespace mozilla
