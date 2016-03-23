/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsXBLPrototypeHandler.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Event.h" // for nsIDOMEvent::InternalDOMEvent()
#include "mozilla/dom/EventTarget.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;
using namespace mozilla::dom;

nsXBLEventHandler::nsXBLEventHandler(nsXBLPrototypeHandler* aHandler)
  : mProtoHandler(aHandler)
{
}

nsXBLEventHandler::~nsXBLEventHandler()
{
}

NS_IMPL_ISUPPORTS(nsXBLEventHandler, nsIDOMEventListener)

NS_IMETHODIMP
nsXBLEventHandler::HandleEvent(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  uint8_t phase = mProtoHandler->GetPhase();
  if (phase == NS_PHASE_TARGET) {
    uint16_t eventPhase;
    aEvent->GetEventPhase(&eventPhase);
    if (eventPhase != nsIDOMEvent::AT_TARGET)
      return NS_OK;
  }

  if (!EventMatched(aEvent))
    return NS_OK;

  mProtoHandler->ExecuteHandler(aEvent->InternalDOMEvent()->GetCurrentTarget(),
                                aEvent);

  return NS_OK;
}

nsXBLMouseEventHandler::nsXBLMouseEventHandler(nsXBLPrototypeHandler* aHandler)
  : nsXBLEventHandler(aHandler)
{
}

nsXBLMouseEventHandler::~nsXBLMouseEventHandler()
{
}

bool
nsXBLMouseEventHandler::EventMatched(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aEvent));
  return mouse && mProtoHandler->MouseEventMatched(mouse);
}

nsXBLKeyEventHandler::nsXBLKeyEventHandler(nsIAtom* aEventType, uint8_t aPhase,
                                           uint8_t aType)
  : mEventType(aEventType),
    mPhase(aPhase),
    mType(aType),
    mIsBoundToChrome(false),
    mUsingContentXBLScope(false)
{
}

nsXBLKeyEventHandler::~nsXBLKeyEventHandler()
{
}

NS_IMPL_ISUPPORTS(nsXBLKeyEventHandler, nsIDOMEventListener)

bool
nsXBLKeyEventHandler::ExecuteMatchedHandlers(
                        nsIDOMKeyEvent* aKeyEvent,
                        uint32_t aCharCode,
                        const IgnoreModifierState& aIgnoreModifierState)
{
  WidgetEvent* event = aKeyEvent->AsEvent()->WidgetEventPtr();
  nsCOMPtr<EventTarget> target = aKeyEvent->AsEvent()->InternalDOMEvent()->GetCurrentTarget();

  bool executed = false;
  for (uint32_t i = 0; i < mProtoHandlers.Length(); ++i) {
    nsXBLPrototypeHandler* handler = mProtoHandlers[i];
    bool hasAllowUntrustedAttr = handler->HasAllowUntrustedAttr();
    if ((event->IsTrusted() ||
        (hasAllowUntrustedAttr && handler->AllowUntrustedEvents()) ||
        (!hasAllowUntrustedAttr && !mIsBoundToChrome && !mUsingContentXBLScope)) &&
        handler->KeyEventMatched(aKeyEvent, aCharCode, aIgnoreModifierState)) {
      handler->ExecuteHandler(target, aKeyEvent->AsEvent());
      executed = true;
    }
  }
#ifdef XP_WIN
  // Windows native applications ignore Windows-Logo key state when checking
  // shortcut keys even if the key is pressed.  Therefore, if there is no
  // shortcut key which exactly matches current modifier state, we should
  // retry to look for a shortcut key without the Windows-Logo key press.
  if (!executed && !aIgnoreModifierState.mOS) {
    WidgetKeyboardEvent* keyEvent = event->AsKeyboardEvent();
    if (keyEvent && keyEvent->IsOS()) {
      IgnoreModifierState ignoreModifierState(aIgnoreModifierState);
      ignoreModifierState.mOS = true;
      return ExecuteMatchedHandlers(aKeyEvent, aCharCode, ignoreModifierState);
    }
  }
#endif
  return executed;
}

NS_IMETHODIMP
nsXBLKeyEventHandler::HandleEvent(nsIDOMEvent* aEvent)
{
  uint32_t count = mProtoHandlers.Length();
  if (count == 0)
    return NS_ERROR_FAILURE;

  if (mPhase == NS_PHASE_TARGET) {
    uint16_t eventPhase;
    aEvent->GetEventPhase(&eventPhase);
    if (eventPhase != nsIDOMEvent::AT_TARGET)
      return NS_OK;
  }

  nsCOMPtr<nsIDOMKeyEvent> key(do_QueryInterface(aEvent));
  if (!key)
    return NS_OK;

  WidgetKeyboardEvent* nativeKeyboardEvent =
    aEvent->WidgetEventPtr()->AsKeyboardEvent();
  MOZ_ASSERT(nativeKeyboardEvent);
  AutoShortcutKeyCandidateArray shortcutKeys;
  nativeKeyboardEvent->GetShortcutKeyCandidates(shortcutKeys);

  if (shortcutKeys.IsEmpty()) {
    ExecuteMatchedHandlers(key, 0, IgnoreModifierState());
    return NS_OK;
  }

  for (uint32_t i = 0; i < shortcutKeys.Length(); ++i) {
    IgnoreModifierState ignoreModifierState;
    ignoreModifierState.mShift = shortcutKeys[i].mIgnoreShift;
    if (ExecuteMatchedHandlers(key, shortcutKeys[i].mCharCode,
                               ignoreModifierState)) {
      return NS_OK;
    }
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

already_AddRefed<nsXBLEventHandler>
NS_NewXBLEventHandler(nsXBLPrototypeHandler* aHandler,
                      nsIAtom* aEventType)
{
  RefPtr<nsXBLEventHandler> handler;

  switch (nsContentUtils::GetEventClassID(nsDependentAtomString(aEventType))) {
    case eDragEventClass:
    case eMouseEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eSimpleGestureEventClass:
      handler = new nsXBLMouseEventHandler(aHandler);
      break;
    default:
      handler = new nsXBLEventHandler(aHandler);
      break;
  }

  return handler.forget();
}
