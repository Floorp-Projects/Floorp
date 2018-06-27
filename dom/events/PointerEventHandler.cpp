/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PointerEventHandler.h"
#include "nsIFrame.h"
#include "PointerEvent.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/MouseEventBinding.h"

namespace mozilla {

using namespace dom;

static bool sPointerEventEnabled = true;
static bool sPointerEventImplicitCapture = false;

class PointerInfo final
{
public:
  uint16_t mPointerType;
  bool mActiveState;
  bool mPrimaryState;
  bool mPreventMouseEventByContent;
  explicit PointerInfo(bool aActiveState, uint16_t aPointerType,
                       bool aPrimaryState)
    : mPointerType(aPointerType)
    , mActiveState(aActiveState)
    , mPrimaryState(aPrimaryState)
    , mPreventMouseEventByContent(false)
  {
  }
};

// Keeps a map between pointerId and element that currently capturing pointer
// with such pointerId. If pointerId is absent in this map then nobody is
// capturing it. Additionally keep information about pending capturing content.
static nsClassHashtable<nsUint32HashKey,
                        PointerCaptureInfo>* sPointerCaptureList;

// Keeps information about pointers such as pointerId, activeState, pointerType,
// primaryState
static nsClassHashtable<nsUint32HashKey, PointerInfo>* sActivePointersIds;

/* static */ void
PointerEventHandler::Initialize()
{
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;
  Preferences::AddBoolVarCache(&sPointerEventEnabled,
                               "dom.w3c_pointer_events.enabled", true);
  Preferences::AddBoolVarCache(&sPointerEventImplicitCapture,
                               "dom.w3c_pointer_events.implicit_capture", true);
}

/* static */ void
PointerEventHandler::InitializeStatics()
{
  MOZ_ASSERT(!sPointerCaptureList, "InitializeStatics called multiple times!");
  sPointerCaptureList =
    new nsClassHashtable<nsUint32HashKey, PointerCaptureInfo>;
  sActivePointersIds = new nsClassHashtable<nsUint32HashKey, PointerInfo>;
}

/* static */ void
PointerEventHandler::ReleaseStatics()
{
  MOZ_ASSERT(sPointerCaptureList, "ReleaseStatics called without Initialize!");
  delete sPointerCaptureList;
  sPointerCaptureList = nullptr;
  delete sActivePointersIds;
  sActivePointersIds = nullptr;
}

/* static */ bool
PointerEventHandler::IsPointerEventEnabled()
{
  return sPointerEventEnabled;
}

/* static */ bool
PointerEventHandler::IsPointerEventImplicitCaptureForTouchEnabled()
{
  return sPointerEventEnabled && sPointerEventImplicitCapture;
}

/* static */ void
PointerEventHandler::UpdateActivePointerState(WidgetMouseEvent* aEvent)
{
  if (!IsPointerEventEnabled() || !aEvent) {
    return;
  }
  switch (aEvent->mMessage) {
  case eMouseEnterIntoWidget:
    // In this case we have to know information about available mouse pointers
    sActivePointersIds->Put(aEvent->pointerId,
                            new PointerInfo(false, aEvent->inputSource, true));
    break;
  case ePointerDown:
    // In this case we switch pointer to active state
    if (WidgetPointerEvent* pointerEvent = aEvent->AsPointerEvent()) {
      sActivePointersIds->Put(pointerEvent->pointerId,
                              new PointerInfo(true, pointerEvent->inputSource,
                                              pointerEvent->mIsPrimary));
    }
    break;
  case ePointerCancel:
    // pointercancel means a pointer is unlikely to continue to produce pointer
    // events. In that case, we should turn off active state or remove the
    // pointer from active pointers.
  case ePointerUp:
    // In this case we remove information about pointer or turn off active state
    if (WidgetPointerEvent* pointerEvent = aEvent->AsPointerEvent()) {
      if(pointerEvent->inputSource != MouseEvent_Binding::MOZ_SOURCE_TOUCH) {
        sActivePointersIds->Put(pointerEvent->pointerId,
                                new PointerInfo(false,
                                                pointerEvent->inputSource,
                                                pointerEvent->mIsPrimary));
      } else {
        sActivePointersIds->Remove(pointerEvent->pointerId);
      }
    }
    break;
  case eMouseExitFromWidget:
    // In this case we have to remove information about disappeared mouse
    // pointers
    sActivePointersIds->Remove(aEvent->pointerId);
    break;
  default:
    break;
  }
}

/* static */ void
PointerEventHandler::SetPointerCaptureById(uint32_t aPointerId,
                                           nsIContent* aContent)
{
  MOZ_ASSERT(aContent);
  if (MouseEvent_Binding::MOZ_SOURCE_MOUSE == GetPointerType(aPointerId)) {
    nsIPresShell::SetCapturingContent(aContent, CAPTURE_PREVENTDRAG);
  }

  PointerCaptureInfo* pointerCaptureInfo = GetPointerCaptureInfo(aPointerId);
  if (pointerCaptureInfo) {
    pointerCaptureInfo->mPendingContent = aContent;
  } else {
    sPointerCaptureList->Put(aPointerId, new PointerCaptureInfo(aContent));
  }
}

/* static */ PointerCaptureInfo*
PointerEventHandler::GetPointerCaptureInfo(uint32_t aPointerId)
{
  PointerCaptureInfo* pointerCaptureInfo = nullptr;
  sPointerCaptureList->Get(aPointerId, &pointerCaptureInfo);
  return pointerCaptureInfo;
}

/* static */ void
PointerEventHandler::ReleasePointerCaptureById(uint32_t aPointerId)
{
  PointerCaptureInfo* pointerCaptureInfo = GetPointerCaptureInfo(aPointerId);
  if (pointerCaptureInfo && pointerCaptureInfo->mPendingContent) {
    if (MouseEvent_Binding::MOZ_SOURCE_MOUSE == GetPointerType(aPointerId)) {
      nsIPresShell::SetCapturingContent(nullptr, CAPTURE_PREVENTDRAG);
    }
    pointerCaptureInfo->mPendingContent = nullptr;
  }
}

/* static */ void
PointerEventHandler::ReleaseAllPointerCapture()
{
  for (auto iter = sPointerCaptureList->Iter(); !iter.Done(); iter.Next()) {
    PointerCaptureInfo* data = iter.UserData();
    if (data && data->mPendingContent) {
      ReleasePointerCaptureById(iter.Key());
    }
  }
}

/* static */ bool
PointerEventHandler::GetPointerInfo(uint32_t aPointerId, bool& aActiveState)
{
  PointerInfo* pointerInfo = nullptr;
  if (sActivePointersIds->Get(aPointerId, &pointerInfo) && pointerInfo) {
    aActiveState = pointerInfo->mActiveState;
    return true;
  }
  return false;
}

/* static */ void
PointerEventHandler::MaybeProcessPointerCapture(WidgetGUIEvent* aEvent)
{
  switch (aEvent->mClass) {
  case eMouseEventClass:
    ProcessPointerCaptureForMouse(aEvent->AsMouseEvent());
    break;
  case eTouchEventClass:
    ProcessPointerCaptureForTouch(aEvent->AsTouchEvent());
    break;
  default:
    break;
  }
}

/* static */ void
PointerEventHandler::ProcessPointerCaptureForMouse(WidgetMouseEvent* aEvent)
{
  if (!ShouldGeneratePointerEventFromMouse(aEvent)) {
    return;
  }

  PointerCaptureInfo* info = GetPointerCaptureInfo(aEvent->pointerId);
  if (!info || info->mPendingContent == info->mOverrideContent) {
    return;
  }
  WidgetPointerEvent localEvent(*aEvent);
  InitPointerEventFromMouse(&localEvent, aEvent, eVoidEvent);
  CheckPointerCaptureState(&localEvent);
}

/* static */ void
PointerEventHandler::ProcessPointerCaptureForTouch(WidgetTouchEvent* aEvent)
{
  if (!ShouldGeneratePointerEventFromTouch(aEvent)) {
    return;
  }

  for (uint32_t i = 0; i < aEvent->mTouches.Length(); ++i) {
    Touch* touch = aEvent->mTouches[i];
    if (!TouchManager::ShouldConvertTouchToPointer(touch, aEvent)) {
      continue;
    }
    PointerCaptureInfo* info = GetPointerCaptureInfo(touch->Identifier());
    if (!info || info->mPendingContent == info->mOverrideContent) {
      continue;
    }
    WidgetPointerEvent event(aEvent->IsTrusted(), eVoidEvent, aEvent->mWidget);
    InitPointerEventFromTouch(&event, aEvent, touch, i == 0);
    CheckPointerCaptureState(&event);
  }
}

/* static */ void
PointerEventHandler::CheckPointerCaptureState(WidgetPointerEvent* aEvent)
{
  // Handle pending pointer capture before any pointer events except
  // gotpointercapture / lostpointercapture.
  if (!aEvent) {
    return;
  }
  MOZ_ASSERT(IsPointerEventEnabled());
  MOZ_ASSERT(aEvent->mClass == ePointerEventClass);

  PointerCaptureInfo* captureInfo = GetPointerCaptureInfo(aEvent->pointerId);

  if (!captureInfo ||
      captureInfo->mPendingContent == captureInfo->mOverrideContent) {
    return;
  }
  // cache captureInfo->mPendingContent since it may be changed in the pointer
  // event listener
  nsIContent* pendingContent = captureInfo->mPendingContent.get();
  if (captureInfo->mOverrideContent) {
    DispatchGotOrLostPointerCaptureEvent(/* aIsGotCapture */ false, aEvent,
                                         captureInfo->mOverrideContent);
  }
  if (pendingContent) {
    DispatchGotOrLostPointerCaptureEvent(/* aIsGotCapture */ true, aEvent,
                                         pendingContent);
  }

  captureInfo->mOverrideContent = pendingContent;
  if (captureInfo->Empty()) {
    sPointerCaptureList->Remove(aEvent->pointerId);
  }
}

/* static */ void
PointerEventHandler::ImplicitlyCapturePointer(nsIFrame* aFrame,
                                              WidgetEvent* aEvent)
{
  MOZ_ASSERT(aEvent->mMessage == ePointerDown);
  if (!aFrame || !IsPointerEventEnabled() ||
      !IsPointerEventImplicitCaptureForTouchEnabled()) {
    return;
  }
  WidgetPointerEvent* pointerEvent = aEvent->AsPointerEvent();
  NS_WARNING_ASSERTION(pointerEvent,
                       "Call ImplicitlyCapturePointer with non-pointer event");
  if (pointerEvent->inputSource != MouseEvent_Binding::MOZ_SOURCE_TOUCH) {
    // We only implicitly capture the pointer for touch device.
    return;
  }
  nsCOMPtr<nsIContent> target;
  aFrame->GetContentForEvent(aEvent, getter_AddRefs(target));
  while (target && !target->IsElement()) {
    target = target->GetParent();
  }
  if (NS_WARN_IF(!target)) {
    return;
  }
  SetPointerCaptureById(pointerEvent->pointerId, target);
}

/* static */ void
PointerEventHandler::ImplicitlyReleasePointerCapture(WidgetEvent* aEvent)
{
  MOZ_ASSERT(aEvent);
  if (aEvent->mMessage != ePointerUp && aEvent->mMessage != ePointerCancel) {
    return;
  }
  WidgetPointerEvent* pointerEvent = aEvent->AsPointerEvent();
  ReleasePointerCaptureById(pointerEvent->pointerId);
  CheckPointerCaptureState(pointerEvent);
}

/* static */ nsIContent*
PointerEventHandler::GetPointerCapturingContent(uint32_t aPointerId)
{
  PointerCaptureInfo* pointerCaptureInfo = GetPointerCaptureInfo(aPointerId);
  if (pointerCaptureInfo) {
    return pointerCaptureInfo->mOverrideContent;
  }
  return nullptr;
}

/* static */ nsIContent*
PointerEventHandler::GetPointerCapturingContent(WidgetGUIEvent* aEvent)
{
  if (!IsPointerEventEnabled() || (aEvent->mClass != ePointerEventClass &&
                                   aEvent->mClass != eMouseEventClass) ||
      aEvent->mMessage == ePointerDown || aEvent->mMessage == eMouseDown) {
    // Pointer capture should only be applied to all pointer events and mouse
    // events except ePointerDown and eMouseDown;
    return nullptr;
  }

  WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
  if (!mouseEvent) {
    return nullptr;
  }
  return GetPointerCapturingContent(mouseEvent->pointerId);
}

/* static */ void
PointerEventHandler::ReleaseIfCaptureByDescendant(nsIContent* aContent)
{
  // We should check that aChild does not contain pointer capturing elements.
  // If it does we should release the pointer capture for the elements.
  for (auto iter = sPointerCaptureList->Iter(); !iter.Done(); iter.Next()) {
    PointerCaptureInfo* data = iter.UserData();
    if (data && data->mPendingContent &&
        nsContentUtils::ContentIsDescendantOf(data->mPendingContent,
                                              aContent)) {
      ReleasePointerCaptureById(iter.Key());
    }
  }
}

/* static */ void
PointerEventHandler::PreHandlePointerEventsPreventDefault(
                       WidgetPointerEvent* aPointerEvent,
                       WidgetGUIEvent* aMouseOrTouchEvent)
{
  if (!aPointerEvent->mIsPrimary || aPointerEvent->mMessage == ePointerDown) {
    return;
  }
  PointerInfo* pointerInfo = nullptr;
  if (!sActivePointersIds->Get(aPointerEvent->pointerId, &pointerInfo) ||
      !pointerInfo) {
    // The PointerInfo for active pointer should be added for normal cases. But
    // in some cases, we may receive mouse events before adding PointerInfo in
    // sActivePointersIds. (e.g. receive mousemove before eMouseEnterIntoWidget
    // or change preference 'dom.w3c_pointer_events.enabled' from off to on).
    // In these cases, we could ignore them because they are not the events
    // between a DefaultPrevented pointerdown and the corresponding pointerup.
    return;
  }
  if (!pointerInfo->mPreventMouseEventByContent) {
    return;
  }
  aMouseOrTouchEvent->PreventDefault(false);
  aMouseOrTouchEvent->mFlags.mOnlyChromeDispatch = true;
  if (aPointerEvent->mMessage == ePointerUp) {
    pointerInfo->mPreventMouseEventByContent = false;
  }
}

/* static */ void
PointerEventHandler::PostHandlePointerEventsPreventDefault(
                       WidgetPointerEvent* aPointerEvent,
                       WidgetGUIEvent* aMouseOrTouchEvent)
{
  if (!aPointerEvent->mIsPrimary || aPointerEvent->mMessage != ePointerDown ||
      !aPointerEvent->DefaultPreventedByContent()) {
    return;
  }
  PointerInfo* pointerInfo = nullptr;
  if (!sActivePointersIds->Get(aPointerEvent->pointerId, &pointerInfo) ||
      !pointerInfo) {
    // We already added the PointerInfo for active pointer when
    // PresShell::HandleEvent handling pointerdown event.
#ifdef DEBUG
    MOZ_CRASH("Got ePointerDown w/o active pointer info!!");
#endif // #ifdef DEBUG
    return;
  }
  // PreventDefault only applied for active pointers.
  if (!pointerInfo->mActiveState) {
    return;
  }
  aMouseOrTouchEvent->PreventDefault(false);
  aMouseOrTouchEvent->mFlags.mOnlyChromeDispatch = true;
  pointerInfo->mPreventMouseEventByContent = true;
}

/* static */ void
PointerEventHandler::InitPointerEventFromMouse(
                       WidgetPointerEvent* aPointerEvent,
                       WidgetMouseEvent* aMouseEvent,
                       EventMessage aMessage)
{
  MOZ_ASSERT(aPointerEvent);
  MOZ_ASSERT(aMouseEvent);
  aPointerEvent->pointerId = aMouseEvent->pointerId;
  aPointerEvent->inputSource = aMouseEvent->inputSource;
  aPointerEvent->mMessage = aMessage;
  aPointerEvent->button = aMouseEvent->mMessage == eMouseMove ?
                            WidgetMouseEvent::eNoButton : aMouseEvent->button;

  aPointerEvent->buttons = aMouseEvent->buttons;
  aPointerEvent->pressure = aPointerEvent->buttons ?
                              aMouseEvent->pressure ?
                                aMouseEvent->pressure : 0.5f :
                              0.0f;
}

/* static */ void
PointerEventHandler::InitPointerEventFromTouch(
                       WidgetPointerEvent* aPointerEvent,
                       WidgetTouchEvent* aTouchEvent,
                       mozilla::dom::Touch* aTouch,
                       bool aIsPrimary)
{
  MOZ_ASSERT(aPointerEvent);
  MOZ_ASSERT(aTouchEvent);

  int16_t button = aTouchEvent->mMessage == eTouchMove ?
                     WidgetMouseEvent::eNoButton :
                     WidgetMouseEvent::eLeftButton;

  int16_t buttons = aTouchEvent->mMessage == eTouchEnd ?
                      WidgetMouseEvent::eNoButtonFlag :
                      WidgetMouseEvent::eLeftButtonFlag;

  aPointerEvent->mIsPrimary = aIsPrimary;
  aPointerEvent->pointerId = aTouch->Identifier();
  aPointerEvent->mRefPoint = aTouch->mRefPoint;
  aPointerEvent->mModifiers = aTouchEvent->mModifiers;
  aPointerEvent->mWidth = aTouch->RadiusX(CallerType::System);
  aPointerEvent->mHeight = aTouch->RadiusY(CallerType::System);
  aPointerEvent->tiltX = aTouch->tiltX;
  aPointerEvent->tiltY = aTouch->tiltY;
  aPointerEvent->mTime = aTouchEvent->mTime;
  aPointerEvent->mTimeStamp = aTouchEvent->mTimeStamp;
  aPointerEvent->mFlags = aTouchEvent->mFlags;
  aPointerEvent->button = button;
  aPointerEvent->buttons = buttons;
  aPointerEvent->inputSource = MouseEvent_Binding::MOZ_SOURCE_TOUCH;
}

/* static */ void
PointerEventHandler::DispatchPointerFromMouseOrTouch(
                       PresShell* aShell,
                       nsIFrame* aFrame,
                       nsIContent* aContent,
                       WidgetGUIEvent* aEvent,
                       bool aDontRetargetEvents,
                       nsEventStatus* aStatus,
                       nsIContent** aTargetContent)
{
  MOZ_ASSERT(IsPointerEventEnabled());
  MOZ_ASSERT(aFrame || aContent);
  MOZ_ASSERT(aEvent);

  EventMessage pointerMessage = eVoidEvent;
  if (aEvent->mClass == eMouseEventClass) {
    WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
    // 1. If it is not mouse then it is likely will come as touch event
    // 2. We don't synthesize pointer events for those events that are not
    //    dispatched to DOM.
    if (!mouseEvent->convertToPointer ||
        !aEvent->IsAllowedToDispatchDOMEvent()) {
      return;
    }
    int16_t button = mouseEvent->button;
    switch (mouseEvent->mMessage) {
    case eMouseMove:
      button = WidgetMouseEvent::eNoButton;
      pointerMessage = ePointerMove;
      break;
    case eMouseUp:
      pointerMessage = mouseEvent->buttons ? ePointerMove : ePointerUp;
      break;
    case eMouseDown:
      pointerMessage =
        mouseEvent->buttons & ~nsContentUtils::GetButtonsFlagForButton(button) ?
        ePointerMove : ePointerDown;
      break;
    default:
      return;
    }

    WidgetPointerEvent event(*mouseEvent);
    InitPointerEventFromMouse(&event, mouseEvent, pointerMessage);
    event.convertToPointer = mouseEvent->convertToPointer = false;
    RefPtr<PresShell> shell(aShell);
    if (!aFrame) {
      shell = PresShell::GetShellForEventTarget(nullptr, aContent);
      if (!shell) {
        return;
      }
    }
    PreHandlePointerEventsPreventDefault(&event, aEvent);
    // Dispatch pointer event to the same target which is found by the
    // corresponding mouse event.
    shell->HandleEventWithTarget(&event, aFrame, aContent, aStatus, true,
                                 aTargetContent);
    PostHandlePointerEventsPreventDefault(&event, aEvent);
  } else if (aEvent->mClass == eTouchEventClass) {
    WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
    // loop over all touches and dispatch pointer events on each touch
    // copy the event
    switch (touchEvent->mMessage) {
    case eTouchMove:
      pointerMessage = ePointerMove;
      break;
    case eTouchEnd:
      pointerMessage = ePointerUp;
      break;
    case eTouchStart:
      pointerMessage = ePointerDown;
      break;
    case eTouchCancel:
    case eTouchPointerCancel:
      pointerMessage = ePointerCancel;
      break;
    default:
      return;
    }

    RefPtr<PresShell> shell(aShell);
    for (uint32_t i = 0; i < touchEvent->mTouches.Length(); ++i) {
      Touch* touch = touchEvent->mTouches[i];
      if (!TouchManager::ShouldConvertTouchToPointer(touch, touchEvent)) {
        continue;
      }

      WidgetPointerEvent event(touchEvent->IsTrusted(), pointerMessage,
                               touchEvent->mWidget);

      InitPointerEventFromTouch(&event, touchEvent, touch, i == 0);
      event.convertToPointer = touch->convertToPointer = false;
      if (aEvent->mMessage == eTouchStart) {
        // We already did hit test for touchstart in PresShell. We should
        // dispatch pointerdown to the same target as touchstart.
        nsCOMPtr<nsIContent> content = do_QueryInterface(touch->mTarget);
        if (!content) {
          continue;
        }

        nsIFrame* frame = content->GetPrimaryFrame();
        shell = PresShell::GetShellForEventTarget(frame, content);
        if (!shell) {
          continue;
        }

        PreHandlePointerEventsPreventDefault(&event, aEvent);
        shell->HandleEventWithTarget(&event, frame, content, aStatus, true,
                                     nullptr);
        PostHandlePointerEventsPreventDefault(&event, aEvent);
      } else {
        // We didn't hit test for other touch events. Spec doesn't mention that
        // all pointer events should be dispatched to the same target as their
        // corresponding touch events. Call PresShell::HandleEvent so that we do
        // hit test for pointer events.
        PreHandlePointerEventsPreventDefault(&event, aEvent);
        shell->HandleEvent(aFrame, &event, aDontRetargetEvents, aStatus);
        PostHandlePointerEventsPreventDefault(&event, aEvent);
      }
    }
  }
}

/* static */ uint16_t
PointerEventHandler::GetPointerType(uint32_t aPointerId)
{
  PointerInfo* pointerInfo = nullptr;
  if (sActivePointersIds->Get(aPointerId, &pointerInfo) && pointerInfo) {
    return pointerInfo->mPointerType;
  }
  return MouseEvent_Binding::MOZ_SOURCE_UNKNOWN;
}

/* static */ bool
PointerEventHandler::GetPointerPrimaryState(uint32_t aPointerId)
{
  PointerInfo* pointerInfo = nullptr;
  if (sActivePointersIds->Get(aPointerId, &pointerInfo) && pointerInfo) {
    return pointerInfo->mPrimaryState;
  }
  return false;
}

/* static */ void
PointerEventHandler::DispatchGotOrLostPointerCaptureEvent(
                       bool aIsGotCapture,
                       const WidgetPointerEvent* aPointerEvent,
                       nsIContent* aCaptureTarget)
{
  nsIDocument* targetDoc = aCaptureTarget->OwnerDoc();
  nsCOMPtr<nsIPresShell> shell = targetDoc->GetShell();
  if (NS_WARN_IF(!shell)) {
    return;
  }

  if (!aIsGotCapture && !aCaptureTarget->IsInUncomposedDoc()) {
    // If the capturing element was removed from the DOM tree, fire
    // ePointerLostCapture at the document.
    PointerEventInit init;
    init.mPointerId = aPointerEvent->pointerId;
    init.mBubbles = true;
    init.mComposed = true;
    ConvertPointerTypeToString(aPointerEvent->inputSource, init.mPointerType);
    init.mIsPrimary = aPointerEvent->mIsPrimary;
    RefPtr<PointerEvent> event;
    event = PointerEvent::Constructor(aCaptureTarget,
                                      NS_LITERAL_STRING("lostpointercapture"),
                                      init);
    targetDoc->DispatchEvent(*event);
    return;
  }
  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetPointerEvent localEvent(aPointerEvent->IsTrusted(),
                                aIsGotCapture ? ePointerGotCapture :
                                                ePointerLostCapture,
                                aPointerEvent->mWidget);

  localEvent.AssignPointerEventData(*aPointerEvent, true);
  DebugOnly<nsresult> rv = shell->HandleEventWithTarget(
                                    &localEvent,
                                    aCaptureTarget->GetPrimaryFrame(),
                                    aCaptureTarget, &status);

  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "DispatchGotOrLostPointerCaptureEvent failed");
}

} // namespace mozilla
