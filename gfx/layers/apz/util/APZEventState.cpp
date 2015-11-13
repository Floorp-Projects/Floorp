/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZEventState.h"

#include "ActiveElementManager.h"
#include "APZCCallbackHelper.h"
#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Move.h"
#include "mozilla/Preferences.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "nsCOMPtr.h"
#include "nsDocShell.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMWindowUtils.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsITimer.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIWidget.h"
#include "nsLayoutUtils.h"
#include "nsQueryFrame.h"
#include "TouchManager.h"
#include "nsIDOMMouseEvent.h"
#include "nsLayoutUtils.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollbarMediator.h"
#include "mozilla/TouchEvents.h"

#define APZES_LOG(...)
// #define APZES_LOG(...) printf_stderr("APZES: " __VA_ARGS__)

// Static helper functions
namespace {

int32_t
WidgetModifiersToDOMModifiers(mozilla::Modifiers aModifiers)
{
  int32_t result = 0;
  if (aModifiers & mozilla::MODIFIER_SHIFT) {
    result |= nsIDOMWindowUtils::MODIFIER_SHIFT;
  }
  if (aModifiers & mozilla::MODIFIER_CONTROL) {
    result |= nsIDOMWindowUtils::MODIFIER_CONTROL;
  }
  if (aModifiers & mozilla::MODIFIER_ALT) {
    result |= nsIDOMWindowUtils::MODIFIER_ALT;
  }
  if (aModifiers & mozilla::MODIFIER_META) {
    result |= nsIDOMWindowUtils::MODIFIER_META;
  }
  if (aModifiers & mozilla::MODIFIER_ALTGRAPH) {
    result |= nsIDOMWindowUtils::MODIFIER_ALTGRAPH;
  }
  if (aModifiers & mozilla::MODIFIER_CAPSLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_CAPSLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_FN) {
    result |= nsIDOMWindowUtils::MODIFIER_FN;
  }
  if (aModifiers & mozilla::MODIFIER_FNLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_FNLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_NUMLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_NUMLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_SCROLLLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_SCROLLLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_SYMBOL) {
    result |= nsIDOMWindowUtils::MODIFIER_SYMBOL;
  }
  if (aModifiers & mozilla::MODIFIER_SYMBOLLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_SYMBOLLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_OS) {
    result |= nsIDOMWindowUtils::MODIFIER_OS;
  }
  return result;
}

} // namespace

namespace mozilla {
namespace layers {

static int32_t sActiveDurationMs = 10;
static bool sActiveDurationMsSet = false;

APZEventState::APZEventState(nsIWidget* aWidget,
                             ContentReceivedInputBlockCallback&& aCallback)
  : mWidget(nullptr)  // initialized in constructor body
  , mActiveElementManager(new ActiveElementManager())
  , mContentReceivedInputBlockCallback(Move(aCallback))
  , mPendingTouchPreventedResponse(false)
  , mPendingTouchPreventedBlockId(0)
  , mEndTouchIsClick(false)
  , mTouchEndCancelled(false)
  , mActiveAPZTransforms(0)
{
  nsresult rv;
  mWidget = do_GetWeakReference(aWidget, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "APZEventState constructed with a widget that"
      " does not support weak references. APZ will NOT work!");

  if (!sActiveDurationMsSet) {
    Preferences::AddIntVarCache(&sActiveDurationMs,
                                "ui.touch_activation.duration_ms",
                                sActiveDurationMs);
    sActiveDurationMsSet = true;
  }
}

APZEventState::~APZEventState()
{}

class DelayedFireSingleTapEvent final : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS

  DelayedFireSingleTapEvent(nsWeakPtr aWidget,
                            LayoutDevicePoint& aPoint,
                            Modifiers aModifiers,
                            nsITimer* aTimer)
    : mWidget(aWidget)
    , mPoint(aPoint)
    , mModifiers(aModifiers)
    // Hold the reference count until we are called back.
    , mTimer(aTimer)
  {
  }

  NS_IMETHODIMP Notify(nsITimer*) override
  {
    if (nsCOMPtr<nsIWidget> widget = do_QueryReferent(mWidget)) {
      APZCCallbackHelper::FireSingleTapEvent(mPoint, mModifiers, widget);
    }
    mTimer = nullptr;
    return NS_OK;
  }

  void ClearTimer() {
    mTimer = nullptr;
  }

private:
  ~DelayedFireSingleTapEvent()
  {
  }

  nsWeakPtr mWidget;
  LayoutDevicePoint mPoint;
  Modifiers mModifiers;
  nsCOMPtr<nsITimer> mTimer;
};

NS_IMPL_ISUPPORTS(DelayedFireSingleTapEvent, nsITimerCallback)

void
APZEventState::ProcessSingleTap(const CSSPoint& aPoint,
                                Modifiers aModifiers,
                                const ScrollableLayerGuid& aGuid)
{
  APZES_LOG("Handling single tap at %s on %s with %d\n",
    Stringify(aPoint).c_str(), Stringify(aGuid).c_str(), mTouchEndCancelled);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return;
  }

  if (mTouchEndCancelled) {
    return;
  }

  LayoutDevicePoint currentPoint =
      APZCCallbackHelper::ApplyCallbackTransform(aPoint, aGuid)
    * widget->GetDefaultScale();;
  if (!mActiveElementManager->ActiveElementUsesStyle()) {
    // If the active element isn't visually affected by the :active style, we
    // have no need to wait the extra sActiveDurationMs to make the activation
    // visually obvious to the user.
    APZCCallbackHelper::FireSingleTapEvent(currentPoint, aModifiers, widget);
    return;
  }

  APZES_LOG("Active element uses style, scheduling timer for click event\n");
  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
  RefPtr<DelayedFireSingleTapEvent> callback =
    new DelayedFireSingleTapEvent(mWidget, currentPoint, aModifiers, timer);
  nsresult rv = timer->InitWithCallback(callback,
                                        sActiveDurationMs,
                                        nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    // Make |callback| not hold the timer, so they will both be destructed when
    // we leave the scope of this function.
    callback->ClearTimer();
  }
}

void
APZEventState::ProcessLongTap(const nsCOMPtr<nsIPresShell>& aPresShell,
                              const CSSPoint& aPoint,
                              Modifiers aModifiers,
                              const ScrollableLayerGuid& aGuid,
                              uint64_t aInputBlockId)
{
  APZES_LOG("Handling long tap at %s\n", Stringify(aPoint).c_str());

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return;
  }

  SendPendingTouchPreventedResponse(false);

  // Converting the modifiers to DOM format for the DispatchMouseEvent call
  // is the most useless thing ever because nsDOMWindowUtils::SendMouseEvent
  // just converts them back to widget format, but that API has many callers,
  // including in JS code, so it's not trivial to change.
  bool eventHandled =
      APZCCallbackHelper::DispatchMouseEvent(aPresShell, NS_LITERAL_STRING("contextmenu"),
                         APZCCallbackHelper::ApplyCallbackTransform(aPoint, aGuid),
                         2, 1, WidgetModifiersToDOMModifiers(aModifiers), true,
                         nsIDOMMouseEvent::MOZ_SOURCE_TOUCH);

  APZES_LOG("Contextmenu event handled: %d\n", eventHandled);
  if (eventHandled) {
    // If the contextmenu event was handled then we're showing a contextmenu,
    // and so we should remove any activation
    mActiveElementManager->ClearActivation();
  } else {
    // If no one handle context menu, fire MOZLONGTAP event
    LayoutDevicePoint currentPoint =
        APZCCallbackHelper::ApplyCallbackTransform(aPoint, aGuid)
      * widget->GetDefaultScale();
    int time = 0;
    nsEventStatus status =
        APZCCallbackHelper::DispatchSynthesizedMouseEvent(eMouseLongTap, time,
                                                          currentPoint,
                                                          aModifiers, widget);
    eventHandled = (status == nsEventStatus_eConsumeNoDefault);
    APZES_LOG("MOZLONGTAP event handled: %d\n", eventHandled);
  }

  mContentReceivedInputBlockCallback(aGuid, aInputBlockId, eventHandled);
}

void
APZEventState::ProcessTouchEvent(const WidgetTouchEvent& aEvent,
                                 const ScrollableLayerGuid& aGuid,
                                 uint64_t aInputBlockId,
                                 nsEventStatus aApzResponse,
                                 nsEventStatus aContentResponse)
{
  if (aEvent.mMessage == eTouchStart && aEvent.touches.Length() > 0) {
    mActiveElementManager->SetTargetElement(aEvent.touches[0]->GetTarget());
  }

  bool isTouchPrevented = aContentResponse == nsEventStatus_eConsumeNoDefault;
  bool sentContentResponse = false;
  APZES_LOG("Handling event type %d\n", aEvent.mMessage);
  switch (aEvent.mMessage) {
  case eTouchStart: {
    mTouchEndCancelled = false;
    sentContentResponse = SendPendingTouchPreventedResponse(false);
    // sentContentResponse can be true here if we get two TOUCH_STARTs in a row
    // and just responded to the first one.
    if (!aEvent.mFlags.mHandledByAPZ) {
      // This condition being true means this touchstart is synthetic and is
      // coming from TabParent.injectTouchEvent.
      // Since APZ doesn't know about it we don't want to send a response for
      // this block; we want to just skip over it from the point of view of
      // prevent-default notifications.
      break;
    }
    if (isTouchPrevented) {
      mContentReceivedInputBlockCallback(aGuid, aInputBlockId, isTouchPrevented);
      sentContentResponse = true;
    } else {
      APZES_LOG("Event not prevented; pending response for %" PRIu64 " %s\n",
        aInputBlockId, Stringify(aGuid).c_str());
      mPendingTouchPreventedResponse = true;
      mPendingTouchPreventedGuid = aGuid;
      mPendingTouchPreventedBlockId = aInputBlockId;
    }
    break;
  }

  case eTouchEnd:
    if (isTouchPrevented) {
      mTouchEndCancelled = true;
      mEndTouchIsClick = false;
    }
    MOZ_FALLTHROUGH;
  case eTouchCancel:
    mActiveElementManager->HandleTouchEndEvent(mEndTouchIsClick);
    MOZ_FALLTHROUGH;
  case eTouchMove: {
    if (mPendingTouchPreventedResponse) {
      MOZ_ASSERT(aGuid == mPendingTouchPreventedGuid);
    }
    sentContentResponse = SendPendingTouchPreventedResponse(isTouchPrevented);
    break;
  }

  default:
    NS_WARNING("Unknown touch event type");
  }

  if (sentContentResponse &&
        aApzResponse == nsEventStatus_eConsumeDoDefault &&
        gfxPrefs::PointerEventsEnabled()) {
    WidgetTouchEvent cancelEvent(aEvent);
    cancelEvent.mMessage = eTouchCancel;
    cancelEvent.mFlags.mCancelable = false; // mMessage != eTouchCancel;
    for (uint32_t i = 0; i < cancelEvent.touches.Length(); ++i) {
      if (mozilla::dom::Touch* touch = cancelEvent.touches[i]) {
        touch->convertToPointer = true;
      }
    }
    nsEventStatus status;
    cancelEvent.widget->DispatchEvent(&cancelEvent, status);
  }
}

void
APZEventState::ProcessWheelEvent(const WidgetWheelEvent& aEvent,
                                 const ScrollableLayerGuid& aGuid,
                                 uint64_t aInputBlockId)
{
  // If this event starts a swipe, indicate that it shouldn't result in a
  // scroll by setting defaultPrevented to true.
  bool defaultPrevented =
    aEvent.mFlags.mDefaultPrevented || aEvent.TriggersSwipe();
  mContentReceivedInputBlockCallback(aGuid, aInputBlockId, defaultPrevented);
}

void
APZEventState::ProcessMouseEvent(const WidgetMouseEvent& aEvent,
                                 const ScrollableLayerGuid& aGuid,
                                 uint64_t aInputBlockId)
{
  // If we get here and the input block has not been confirmed then
  // no scrollbar reacted to the event thus APZC should ignore this block.
  bool defaultPrevented = false;
  mContentReceivedInputBlockCallback(aGuid, aInputBlockId, defaultPrevented);
}

void
APZEventState::ProcessAPZStateChange(const nsCOMPtr<nsIDocument>& aDocument,
                                     ViewID aViewId,
                                     APZStateChange aChange,
                                     int aArg)
{
  switch (aChange)
  {
  case APZStateChange::TransformBegin:
  {
    nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(aViewId);
    if (sf) {
      sf->SetTransformingByAPZ(true);
    }
    nsIScrollbarMediator* scrollbarMediator = do_QueryFrame(sf);
    if (scrollbarMediator) {
      scrollbarMediator->ScrollbarActivityStarted();
    }

    if (aDocument && mActiveAPZTransforms == 0) {
      nsCOMPtr<nsIDocShell> docshell(aDocument->GetDocShell());
      if (docshell && sf) {
        nsDocShell* nsdocshell = static_cast<nsDocShell*>(docshell.get());
        nsdocshell->NotifyAsyncPanZoomStarted();
      }
    }
    mActiveAPZTransforms++;
    break;
  }
  case APZStateChange::TransformEnd:
  {
    mActiveAPZTransforms--;
    nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(aViewId);
    if (sf) {
      sf->SetTransformingByAPZ(false);
    }
    nsIScrollbarMediator* scrollbarMediator = do_QueryFrame(sf);
    if (scrollbarMediator) {
      scrollbarMediator->ScrollbarActivityStopped();
    }

    if (aDocument && mActiveAPZTransforms == 0) {
      nsCOMPtr<nsIDocShell> docshell(aDocument->GetDocShell());
      if (docshell && sf) {
        nsDocShell* nsdocshell = static_cast<nsDocShell*>(docshell.get());
        nsdocshell->NotifyAsyncPanZoomStopped();
      }
    }
    break;
  }
  case APZStateChange::StartTouch:
  {
    mActiveElementManager->HandleTouchStart(aArg);
    break;
  }
  case APZStateChange::StartPanning:
  {
    // The user started to pan, so we don't want anything to be :active.
    mActiveElementManager->ClearActivation();
    break;
  }
  case APZStateChange::EndTouch:
  {
    mEndTouchIsClick = aArg;
    mActiveElementManager->HandleTouchEnd();
    break;
  }
  default:
    // APZStateChange has a 'sentinel' value, and the compiler complains
    // if an enumerator is not handled and there is no 'default' case.
    break;
  }
}

void
APZEventState::ProcessClusterHit()
{
  // If we hit a cluster of links then we shouldn't activate any of them,
  // as we will be showing the zoomed view. (This is only called on Fennec).
#ifndef MOZ_ANDROID_APZ
  MOZ_ASSERT(false);
#endif
  mActiveElementManager->ClearActivation();
}

bool
APZEventState::SendPendingTouchPreventedResponse(bool aPreventDefault)
{
  if (mPendingTouchPreventedResponse) {
    APZES_LOG("Sending response %d for pending guid: %s\n", aPreventDefault,
      Stringify(mPendingTouchPreventedGuid).c_str());
    mContentReceivedInputBlockCallback(mPendingTouchPreventedGuid,
        mPendingTouchPreventedBlockId, aPreventDefault);
    mPendingTouchPreventedResponse = false;
    return true;
  }
  return false;
}

already_AddRefed<nsIWidget>
APZEventState::GetWidget() const
{
  nsCOMPtr<nsIWidget> result = do_QueryReferent(mWidget);
  return result.forget();
}

} // namespace layers
} // namespace mozilla
