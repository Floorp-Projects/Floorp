/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZInputBridge.h"

#include "AsyncPanZoomController.h"
#include "InputData.h"               // for MouseInput, etc
#include "InputBlockState.h"         // for InputBlockState
#include "OverscrollHandoffState.h"  // for OverscrollHandoffState
#include "mozilla/EventForwards.h"
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

void APZEventResult::SetStatusForTouchEvent(
    const InputBlockState& aBlock, TargetConfirmationFlags aFlags,
    PointerEventsConsumableFlags aConsumableFlags,
    const AsyncPanZoomController* aTarget) {
  // Note, we need to continue setting mStatus to eIgnore in the {mHasRoom=true,
  // mAllowedByTouchAction=false} case because this is the behaviour expected by
  // APZEventState::ProcessTouchEvent() when it determines when to send a
  // `pointercancel` event. TODO: Use something more descriptive than
  // nsEventStatus for this purpose.
  mStatus = aConsumableFlags.IsConsumable() ? nsEventStatus_eConsumeDoDefault
                                            : nsEventStatus_eIgnore;

  UpdateHandledResult(aBlock, aConsumableFlags, aTarget,
                      aFlags.mDispatchToContent);
}

void APZEventResult::UpdateHandledResult(
    const InputBlockState& aBlock,
    PointerEventsConsumableFlags aConsumableFlags,
    const AsyncPanZoomController* aTarget, bool aDispatchToContent) {
  // If the touch event's effect is disallowed by touch-action, treat it as if
  // a touch event listener had preventDefault()-ed it (i.e. return
  // HandledByContent, except we can do it eagerly rather than having to wait
  // for the listener to run).
  if (!aConsumableFlags.mAllowedByTouchAction) {
    mHandledResult =
        Some(APZHandledResult{APZHandledPlace::HandledByContent, aTarget});
    mHandledResult->mOverscrollDirections = ScrollDirections();
    return;
  }

  if (mHandledResult && !aDispatchToContent && !aConsumableFlags.mHasRoom) {
    // Set result to Unhandled if we have no room to scroll, unless it
    // was HandledByContent because we're over a dispatch-to-content region,
    // in which case it should remain HandledByContent.
    mHandledResult->mPlace = APZHandledPlace::Unhandled;
  }

  if (aTarget && !aTarget->IsRootContent()) {
    // If the event targets a subframe but the subframe and its ancestors
    // are all scrolled to the top, we want an upward swipe to allow
    // triggering pull-to-refresh.
    bool mayTriggerPullToRefresh =
        aBlock.GetOverscrollHandoffChain()->ScrollingUpWillTriggerPullToRefresh(
            aTarget);
    if (mayTriggerPullToRefresh) {
      // Similar to what is done for the dynamic toolbar, we need to ensure
      // that if the input has the dispatch to content flag, we need to change
      // the handled result to Nothing(), so that GeckoView can wait for the
      // result.
      mHandledResult = (aDispatchToContent)
                           ? Nothing()
                           : Some(APZHandledResult{APZHandledPlace::Unhandled,
                                                   aTarget, true});
    }

    auto [mayMoveDynamicToolbar, rootApzc] =
        aBlock.GetOverscrollHandoffChain()->ScrollingDownWillMoveDynamicToolbar(
            aTarget);
    if (mayMoveDynamicToolbar) {
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
      mHandledResult =
          aDispatchToContent
              ? Nothing()
              : Some(APZHandledResult{aConsumableFlags.IsConsumable()
                                          ? APZHandledPlace::HandledByRoot
                                          : APZHandledPlace::Unhandled,
                                      rootApzc});
    }
  }
}

void APZEventResult::SetStatusForFastFling(
    const TouchBlockState& aBlock, TargetConfirmationFlags aFlags,
    PointerEventsConsumableFlags aConsumableFlags,
    const AsyncPanZoomController* aTarget) {
  MOZ_ASSERT(aBlock.IsDuringFastFling());

  // Set eConsumeNoDefault for fast fling since we don't want to send the event
  // to content at all.
  mStatus = nsEventStatus_eConsumeNoDefault;

  // In the case of fast fling, the event will never be sent to content, so we
  // want a result where `aDispatchToContent` is false whatever the original
  // `aFlags.mDispatchToContent` is.
  UpdateHandledResult(aBlock, aConsumableFlags, aTarget, false /*
  aDispatchToContent */);
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

APZEventResult APZInputBridge::ReceiveInputEvent(
    WidgetInputEvent& aEvent, InputBlockCallback&& aCallback) {
  APZThreadUtils::AssertOnControllerThread();

  APZEventResult result;

  switch (aEvent.mClass) {
    case eMouseEventClass:
    case eDragEventClass: {
      WidgetMouseEvent& mouseEvent = *aEvent.AsMouseEvent();
      if (WillHandleMouseEvent(mouseEvent)) {
        MouseInput input(mouseEvent);
        input.mOrigin =
            ScreenPoint(mouseEvent.mRefPoint.x, mouseEvent.mRefPoint.y);

        result = ReceiveInputEvent(input, std::move(aCallback));

        mouseEvent.mRefPoint = TruncatedToInt(ViewAs<LayoutDevicePixel>(
            input.mOrigin,
            PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent));
        mouseEvent.mFlags.mHandledByAPZ = input.mHandledByAPZ;
        mouseEvent.mFocusSequenceNumber = input.mFocusSequenceNumber;
#ifdef XP_MACOSX
        // It's not assumed that the click event has already been prevented,
        // except mousedown event with ctrl key is pressed where we prevent
        // click event from widget on Mac platform.
        MOZ_ASSERT_IF(!mouseEvent.IsControl() ||
                          mouseEvent.mMessage != eMouseDown ||
                          mouseEvent.mButton != MouseButton::ePrimary,
                      !mouseEvent.mClickEventPrevented);
#else
        MOZ_ASSERT(
            !mouseEvent.mClickEventPrevented,
            "It's not assumed that the click event has already been prevented");
#endif
        mouseEvent.mClickEventPrevented |= input.mPreventClickEvent;
        MOZ_ASSERT_IF(mouseEvent.mClickEventPrevented,
                      mouseEvent.mMessage == eMouseDown ||
                          mouseEvent.mMessage == eMouseUp);
        aEvent.mLayersId = input.mLayersId;

        if (mouseEvent.IsReal()) {
          UpdateWheelTransaction(mouseEvent.mRefPoint, mouseEvent.mMessage,
                                 Some(result.mTargetGuid));
        }

        return result;
      }

      if (mouseEvent.IsReal()) {
        UpdateWheelTransaction(mouseEvent.mRefPoint, mouseEvent.mMessage,
                               Nothing());
      }

      ProcessUnhandledEvent(&mouseEvent.mRefPoint, &result.mTargetGuid,
                            &aEvent.mFocusSequenceNumber, &aEvent.mLayersId);
      return result;
    }
    case eTouchEventClass: {
      WidgetTouchEvent& touchEvent = *aEvent.AsTouchEvent();
      MultiTouchInput touchInput(touchEvent);
      result = ReceiveInputEvent(touchInput, std::move(aCallback));
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
              wheelEvent.mTimeStamp, 0, scrollMode,
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

          result = ReceiveInputEvent(input, std::move(aCallback));
          wheelEvent.mRefPoint = TruncatedToInt(ViewAs<LayoutDevicePixel>(
              input.mOrigin, PixelCastJustification::
                                 LayoutDeviceIsScreenForUntransformedEvent));
          wheelEvent.mFlags.mHandledByAPZ = input.mHandledByAPZ;
          wheelEvent.mFocusSequenceNumber = input.mFocusSequenceNumber;
          aEvent.mLayersId = input.mLayersId;

          return result;
        }
      }

      UpdateWheelTransaction(aEvent.mRefPoint, aEvent.mMessage, Nothing());
      ProcessUnhandledEvent(&aEvent.mRefPoint, &result.mTargetGuid,
                            &aEvent.mFocusSequenceNumber, &aEvent.mLayersId);
      MOZ_ASSERT(result.GetStatus() == nsEventStatus_eIgnore);
      return result;
    }
    case eKeyboardEventClass: {
      WidgetKeyboardEvent& keyboardEvent = *aEvent.AsKeyboardEvent();

      KeyboardInput input(keyboardEvent);

      result = ReceiveInputEvent(input, std::move(aCallback));

      keyboardEvent.mFlags.mHandledByAPZ = input.mHandledByAPZ;
      keyboardEvent.mFocusSequenceNumber = input.mFocusSequenceNumber;
      return result;
    }
    default: {
      UpdateWheelTransaction(aEvent.mRefPoint, aEvent.mMessage, Nothing());
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
                                   const AsyncPanZoomController* aTarget,
                                   bool aPopulateDirectionsForUnhandled)
    : mPlace(aPlace) {
  MOZ_ASSERT(aTarget);
  switch (aPlace) {
    case APZHandledPlace::Unhandled:
      if (aTarget && aPopulateDirectionsForUnhandled) {
        mScrollableDirections = aTarget->ScrollableDirections();
        mOverscrollDirections = aTarget->GetAllowedHandoffDirections();
      }
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
