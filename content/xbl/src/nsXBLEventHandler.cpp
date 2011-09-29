/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brendan Eich (brendan@mozilla.org)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    mIsBoundToChrome(PR_FALSE)
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
      executed = PR_TRUE;
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
    ExecuteMatchedHandlers(key, 0, PR_FALSE);
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
