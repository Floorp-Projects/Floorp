/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsGkAtoms.h"
#include "nsXBLPrototypeHandler.h"
#include "nsIDOMNSEvent.h"
#include "nsGUIEvent.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"

nsXBLEventHandler::nsXBLEventHandler(nsXBLPrototypeHandler* aHandler)
  : mProtoHandler(aHandler)
{
}

nsXBLEventHandler::~nsXBLEventHandler()
{
}

NS_IMPL_ISUPPORTS1(nsXBLEventHandler, nsIDOMEventListener)

NS_IMETHODIMP
nsXBLEventHandler::HandleEvent(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  PRUint8 phase = mProtoHandler->GetPhase();
  if (phase == NS_PHASE_TARGET) {
    PRUint16 eventPhase;
    aEvent->GetEventPhase(&eventPhase);
    if (eventPhase != nsIDOMEvent::AT_TARGET)
      return NS_OK;
  }

  if (!EventMatched(aEvent))
    return NS_OK;

  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetCurrentTarget(getter_AddRefs(target));

  mProtoHandler->ExecuteHandler(target, aEvent);

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

nsXBLKeyEventHandler::nsXBLKeyEventHandler(nsIAtom* aEventType, PRUint8 aPhase,
                                           PRUint8 aType)
  : mEventType(aEventType),
    mPhase(aPhase),
    mType(aType),
    mIsBoundToChrome(false)
{
}

nsXBLKeyEventHandler::~nsXBLKeyEventHandler()
{
}

NS_IMPL_ISUPPORTS1(nsXBLKeyEventHandler, nsIDOMEventListener)

bool
nsXBLKeyEventHandler::ExecuteMatchedHandlers(nsIDOMKeyEvent* aKeyEvent,
                                             PRUint32 aCharCode,
                                             bool aIgnoreShiftKey)
{
  nsCOMPtr<nsIDOMNSEvent> domNSEvent = do_QueryInterface(aKeyEvent);
  bool trustedEvent = false;
  if (domNSEvent)
    domNSEvent->GetIsTrusted(&trustedEvent);

  nsCOMPtr<nsIDOMEventTarget> target;
  aKeyEvent->GetCurrentTarget(getter_AddRefs(target));

  bool executed = false;
  for (PRUint32 i = 0; i < mProtoHandlers.Length(); ++i) {
    nsXBLPrototypeHandler* handler = mProtoHandlers[i];
    bool hasAllowUntrustedAttr = handler->HasAllowUntrustedAttr();
    if ((trustedEvent ||
        (hasAllowUntrustedAttr && handler->AllowUntrustedEvents()) ||
        (!hasAllowUntrustedAttr && !mIsBoundToChrome)) &&
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
  PRUint32 count = mProtoHandlers.Length();
  if (count == 0)
    return NS_ERROR_FAILURE;

  if (mPhase == NS_PHASE_TARGET) {
    PRUint16 eventPhase;
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

  for (PRUint32 i = 0; i < accessKeys.Length(); ++i) {
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
  switch (nsContentUtils::GetEventCategory(nsDependentAtomString(aEventType))) {
    case NS_DRAG_EVENT:
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
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
NS_NewXBLKeyEventHandler(nsIAtom* aEventType, PRUint8 aPhase, PRUint8 aType,
                         nsXBLKeyEventHandler** aResult)
{
  *aResult = new nsXBLKeyEventHandler(aEventType, aPhase, aType);

  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);

  return NS_OK;
}
