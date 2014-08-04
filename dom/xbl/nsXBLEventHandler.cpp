/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
nsXBLKeyEventHandler::ExecuteMatchedHandlers(nsIDOMKeyEvent* aKeyEvent,
                                             uint32_t aCharCode,
                                             bool aIgnoreShiftKey)
{
  bool trustedEvent = false;
  aKeyEvent->GetIsTrusted(&trustedEvent);

  nsCOMPtr<EventTarget> target = aKeyEvent->InternalDOMEvent()->GetCurrentTarget();

  bool executed = false;
  for (uint32_t i = 0; i < mProtoHandlers.Length(); ++i) {
    nsXBLPrototypeHandler* handler = mProtoHandlers[i];
    bool hasAllowUntrustedAttr = handler->HasAllowUntrustedAttr();
    if ((trustedEvent ||
        (hasAllowUntrustedAttr && handler->AllowUntrustedEvents()) ||
        (!hasAllowUntrustedAttr && !mIsBoundToChrome && !mUsingContentXBLScope)) &&
        handler->KeyEventMatched(aKeyEvent, aCharCode, aIgnoreShiftKey)) {
      handler->ExecuteHandler(target, aKeyEvent);
      executed = true;
    }
  }
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

  nsAutoTArray<nsShortcutCandidate, 10> accessKeys;
  nsContentUtils::GetAccelKeyCandidates(key, accessKeys);

  if (accessKeys.IsEmpty()) {
    ExecuteMatchedHandlers(key, 0, false);
    return NS_OK;
  }

  for (uint32_t i = 0; i < accessKeys.Length(); ++i) {
    if (ExecuteMatchedHandlers(key, accessKeys[i].mCharCode,
                               accessKeys[i].mIgnoreShift))
      return NS_OK;
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLEventHandler(nsXBLPrototypeHandler* aHandler,
                      nsIAtom* aEventType,
                      nsXBLEventHandler** aResult)
{
  switch (nsContentUtils::GetEventClassID(nsDependentAtomString(aEventType))) {
    case eDragEventClass:
    case eMouseEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eSimpleGestureEventClass:
      *aResult = new nsXBLMouseEventHandler(aHandler);
      break;
    default:
      *aResult = new nsXBLEventHandler(aHandler);
      break;
  }

  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);

  return NS_OK;
}

nsresult
NS_NewXBLKeyEventHandler(nsIAtom* aEventType, uint8_t aPhase, uint8_t aType,
                         nsXBLKeyEventHandler** aResult)
{
  *aResult = new nsXBLKeyEventHandler(aEventType, aPhase, aType);

  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);

  return NS_OK;
}
