/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZEventState.h"

#include "ActiveElementManager.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Preferences.h"
#include "nsCOMPtr.h"
#include "nsDocShell.h"
#include "nsITimer.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIWidget.h"

#define APZES_LOG(...)
// #define APZES_LOG(...) printf_stderr("APZCCH: " __VA_ARGS__)

namespace mozilla {
namespace layers {

static int32_t sActiveDurationMs = 10;
static bool sActiveDurationMsSet = false;

APZEventState::APZEventState(nsIWidget* aWidget,
                             const nsRefPtr<ContentReceivedInputBlockCallback>& aCallback)
  : mWidget(nullptr)  // initialized in constructor body
  , mActiveElementManager(new ActiveElementManager())
  , mContentReceivedInputBlockCallback(aCallback)
  , mPendingTouchPreventedResponse(false)
  , mPendingTouchPreventedBlockId(0)
  , mEndTouchIsClick(false)
  , mTouchEndCancelled(false)
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

class DelayedFireSingleTapEvent MOZ_FINAL : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS

  DelayedFireSingleTapEvent(nsWeakPtr aWidget,
                            LayoutDevicePoint& aPoint,
                            nsITimer* aTimer)
    : mWidget(aWidget)
    , mPoint(aPoint)
    // Hold the reference count until we are called back.
    , mTimer(aTimer)
  {
  }

  NS_IMETHODIMP Notify(nsITimer*) MOZ_OVERRIDE
  {
    if (nsCOMPtr<nsIWidget> widget = do_QueryReferent(mWidget)) {
      APZCCallbackHelper::FireSingleTapEvent(mPoint, widget);
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
  nsCOMPtr<nsITimer> mTimer;
};

NS_IMPL_ISUPPORTS(DelayedFireSingleTapEvent, nsITimerCallback)

void
APZEventState::ProcessSingleTap(const CSSPoint& aPoint,
                                const ScrollableLayerGuid& aGuid,
                                float aPresShellResolution)
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
      APZCCallbackHelper::ApplyCallbackTransform(aPoint, aGuid, aPresShellResolution)
    * widget->GetDefaultScale();;
  if (!mActiveElementManager->ActiveElementUsesStyle()) {
    // If the active element isn't visually affected by the :active style, we
    // have no need to wait the extra sActiveDurationMs to make the activation
    // visually obvious to the user.
    APZCCallbackHelper::FireSingleTapEvent(currentPoint, widget);
    return;
  }

  APZES_LOG("Active element uses style, scheduling timer for click event\n");
  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
  nsRefPtr<DelayedFireSingleTapEvent> callback =
    new DelayedFireSingleTapEvent(mWidget, currentPoint, timer);
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
APZEventState::ProcessLongTap(const nsCOMPtr<nsIDOMWindowUtils>& aUtils,
                              const CSSPoint& aPoint,
                              const ScrollableLayerGuid& aGuid,
                              uint64_t aInputBlockId,
                              float aPresShellResolution)
{
  APZES_LOG("Handling long tap at %s\n", Stringify(aPoint).c_str());

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return;
  }

  SendPendingTouchPreventedResponse(false, aGuid);

  bool eventHandled =
      APZCCallbackHelper::DispatchMouseEvent(aUtils, NS_LITERAL_STRING("contextmenu"),
                         APZCCallbackHelper::ApplyCallbackTransform(aPoint, aGuid, aPresShellResolution),
                         2, 1, 0, true,
                         nsIDOMMouseEvent::MOZ_SOURCE_TOUCH);

  APZES_LOG("Contextmenu event handled: %d\n", eventHandled);

  // If no one handle context menu, fire MOZLONGTAP event
  if (!eventHandled) {
    LayoutDevicePoint currentPoint =
        APZCCallbackHelper::ApplyCallbackTransform(aPoint, aGuid, aPresShellResolution)
      * widget->GetDefaultScale();
    int time = 0;
    nsEventStatus status =
        APZCCallbackHelper::DispatchSynthesizedMouseEvent(NS_MOUSE_MOZLONGTAP, time, currentPoint, widget);
    eventHandled = (status == nsEventStatus_eConsumeNoDefault);
    APZES_LOG("MOZLONGTAP event handled: %d\n", eventHandled);
  }

  mContentReceivedInputBlockCallback->Run(aGuid, aInputBlockId, eventHandled);
}

void
APZEventState::ProcessLongTapUp(const CSSPoint& aPoint,
                                const ScrollableLayerGuid& aGuid,
                                float aPresShellResolution)
{
  APZES_LOG("Handling long tap up at %s\n", Stringify(aPoint).c_str());

  ProcessSingleTap(aPoint, aGuid, aPresShellResolution);
}

void
APZEventState::ProcessTouchEvent(const WidgetTouchEvent& aEvent,
                                 const ScrollableLayerGuid& aGuid,
                                 uint64_t aInputBlockId)
{
  if (aEvent.message == NS_TOUCH_START && aEvent.touches.Length() > 0) {
    mActiveElementManager->SetTargetElement(aEvent.touches[0]->GetTarget());
  }

  bool isTouchPrevented = nsIPresShell::gPreventMouseEvents ||
      aEvent.mFlags.mMultipleActionsPrevented;
  switch (aEvent.message) {
  case NS_TOUCH_START: {
    mTouchEndCancelled = false;
    if (mPendingTouchPreventedResponse) {
      // We can enter here if we get two TOUCH_STARTs in a row and didn't
      // respond to the first one. Respond to it now.
      mContentReceivedInputBlockCallback->Run(mPendingTouchPreventedGuid,
          mPendingTouchPreventedBlockId, false);
      mPendingTouchPreventedResponse = false;
    }
    if (isTouchPrevented) {
      mContentReceivedInputBlockCallback->Run(aGuid, aInputBlockId, isTouchPrevented);
    } else {
      mPendingTouchPreventedResponse = true;
      mPendingTouchPreventedGuid = aGuid;
      mPendingTouchPreventedBlockId = aInputBlockId;
    }
    break;
  }

  case NS_TOUCH_END:
    if (isTouchPrevented) {
      mTouchEndCancelled = true;
      mEndTouchIsClick = false;
    }
    // fall through
  case NS_TOUCH_CANCEL:
    mActiveElementManager->HandleTouchEnd(mEndTouchIsClick);
    // fall through
  case NS_TOUCH_MOVE: {
    SendPendingTouchPreventedResponse(isTouchPrevented, aGuid);
    break;
  }

  default:
    NS_WARNING("Unknown touch event type");
  }
}

void
APZEventState::ProcessWheelEvent(const WidgetWheelEvent& aEvent,
                                 const ScrollableLayerGuid& aGuid,
                                 uint64_t aInputBlockId)
{
  mContentReceivedInputBlockCallback->Run(aGuid, aInputBlockId, aEvent.mFlags.mDefaultPrevented);
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
    nsIScrollbarMediator* scrollbarMediator = do_QueryFrame(sf);
    if (scrollbarMediator) {
      scrollbarMediator->ScrollbarActivityStarted();
    }

    if (aDocument) {
      nsCOMPtr<nsIDocShell> docshell(aDocument->GetDocShell());
      if (docshell && sf) {
        nsDocShell* nsdocshell = static_cast<nsDocShell*>(docshell.get());
        nsdocshell->NotifyAsyncPanZoomStarted(sf->GetScrollPositionCSSPixels());
      }
    }
    break;
  }
  case APZStateChange::TransformEnd:
  {
    nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(aViewId);
    nsIScrollbarMediator* scrollbarMediator = do_QueryFrame(sf);
    if (scrollbarMediator) {
      scrollbarMediator->ScrollbarActivityStopped();
    }

    if (aDocument) {
      nsCOMPtr<nsIDocShell> docshell(aDocument->GetDocShell());
      if (docshell && sf) {
        nsDocShell* nsdocshell = static_cast<nsDocShell*>(docshell.get());
        nsdocshell->NotifyAsyncPanZoomStopped(sf->GetScrollPositionCSSPixels());
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
    mActiveElementManager->HandlePanStart();
    break;
  }
  case APZStateChange::EndTouch:
  {
    mEndTouchIsClick = aArg;
    break;
  }
  default:
    // APZStateChange has a 'sentinel' value, and the compiler complains
    // if an enumerator is not handled and there is no 'default' case.
    break;
  }
}

void
APZEventState::SendPendingTouchPreventedResponse(bool aPreventDefault,
                                                 const ScrollableLayerGuid& aGuid)
{
  if (mPendingTouchPreventedResponse) {
    MOZ_ASSERT(aGuid == mPendingTouchPreventedGuid);
    mContentReceivedInputBlockCallback->Run(mPendingTouchPreventedGuid,
        mPendingTouchPreventedBlockId, aPreventDefault);
    mPendingTouchPreventedResponse = false;
  }
}

already_AddRefed<nsIWidget>
APZEventState::GetWidget() const
{
  nsCOMPtr<nsIWidget> result = do_QueryReferent(mWidget);
  return result.forget();
}

}
}

