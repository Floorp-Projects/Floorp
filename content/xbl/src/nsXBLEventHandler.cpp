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
#include "nsIDOMEventGroup.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMText.h"
#include "nsIDOM3EventTarget.h"
#include "nsGkAtoms.h"
#include "nsXBLPrototypeHandler.h"
#include "nsIDOMNSEvent.h"
#include "nsContentUtils.h"

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
  nsCOMPtr<nsIDOMEventReceiver> receiver = do_QueryInterface(target);

  mProtoHandler->ExecuteHandler(receiver, aEvent);

  return NS_OK;
}

nsXBLMouseEventHandler::nsXBLMouseEventHandler(nsXBLPrototypeHandler* aHandler)
  : nsXBLEventHandler(aHandler)
{
}

nsXBLMouseEventHandler::~nsXBLMouseEventHandler()
{
}

PRBool
nsXBLMouseEventHandler::EventMatched(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aEvent));
  return mProtoHandler->MouseEventMatched(mouse);
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

NS_IMETHODIMP
nsXBLKeyEventHandler::HandleEvent(nsIDOMEvent* aEvent)
{
  PRUint32 count = mProtoHandlers.Count();
  if (count == 0)
    return NS_ERROR_FAILURE;

  if (mPhase == NS_PHASE_TARGET) {
    PRUint16 eventPhase;
    aEvent->GetEventPhase(&eventPhase);
    if (eventPhase != nsIDOMEvent::AT_TARGET)
      return NS_OK;
  }

  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetCurrentTarget(getter_AddRefs(target));
  nsCOMPtr<nsIDOMEventReceiver> receiver = do_QueryInterface(target);

  nsCOMPtr<nsIDOMKeyEvent> key(do_QueryInterface(aEvent));

  nsCOMPtr<nsIDOMNSEvent> domNSEvent = do_QueryInterface(aEvent);
  PRBool trustedEvent = PR_FALSE;
  if (domNSEvent) {
    domNSEvent->GetIsTrusted(&trustedEvent);
  }

  PRUint32 i;
  for (i = 0; i < count; ++i) {
    nsXBLPrototypeHandler* handler = NS_STATIC_CAST(nsXBLPrototypeHandler*,
                                                    mProtoHandlers[i]);
    PRBool hasAllowUntrustedAttr = handler->HasAllowUntrustedAttr();
    if ((trustedEvent ||
        (hasAllowUntrustedAttr && handler->AllowUntrustedEvents()) ||
        (!hasAllowUntrustedAttr && !mIsBoundToChrome)) &&
        handler->KeyEventMatched(key)) {
      handler->ExecuteHandler(receiver, aEvent);
    }
  }

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLEventHandler(nsXBLPrototypeHandler* aHandler,
                      nsIAtom* aEventType,
                      nsXBLEventHandler** aResult)
{
  if (aEventType == nsGkAtoms::mousedown ||
      aEventType == nsGkAtoms::mouseup ||
      aEventType == nsGkAtoms::click ||
      aEventType == nsGkAtoms::dblclick ||
      aEventType == nsGkAtoms::mouseover ||
      aEventType == nsGkAtoms::mouseout ||
      aEventType == nsGkAtoms::mousemove ||
      aEventType == nsGkAtoms::contextmenu ||
      aEventType == nsGkAtoms::dragenter ||
      aEventType == nsGkAtoms::dragover ||
      aEventType == nsGkAtoms::dragdrop ||
      aEventType == nsGkAtoms::dragexit ||
      aEventType == nsGkAtoms::draggesture) {
    *aResult = new nsXBLMouseEventHandler(aHandler);
  }
  else {
    *aResult = new nsXBLEventHandler(aHandler);
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
