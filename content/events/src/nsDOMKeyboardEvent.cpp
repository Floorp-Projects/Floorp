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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steve Clark (buster@netscape.com)
 *   Ilya Konstantinov (mozilla-code@future.shiny.co.il)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDOMKeyboardEvent.h"
#include "nsContentUtils.h"

nsDOMKeyboardEvent::nsDOMKeyboardEvent(nsPresContext* aPresContext,
                                       nsKeyEvent* aEvent)
  : nsDOMUIEvent(aPresContext, aEvent ? aEvent :
                 new nsKeyEvent(PR_FALSE, 0, nsnull))
{
  NS_ASSERTION(mEvent->eventStructType == NS_KEY_EVENT, "event type mismatch");

  if (aEvent) {
    mEventIsInternal = PR_FALSE;
  }
  else {
    mEventIsInternal = PR_TRUE;
    mEvent->time = PR_Now();
  }
}

nsDOMKeyboardEvent::~nsDOMKeyboardEvent()
{
  if (mEventIsInternal) {
    delete static_cast<nsKeyEvent*>(mEvent);
    mEvent = nsnull;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMKeyboardEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMKeyboardEvent, nsDOMUIEvent)

DOMCI_DATA(KeyboardEvent, nsDOMKeyboardEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMKeyboardEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(KeyboardEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)

NS_IMETHODIMP
nsDOMKeyboardEvent::GetAltKey(PRBool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ((nsInputEvent*)mEvent)->isAlt;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetCtrlKey(PRBool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ((nsInputEvent*)mEvent)->isControl;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetShiftKey(PRBool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ((nsInputEvent*)mEvent)->isShift;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetMetaKey(PRBool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ((nsInputEvent*)mEvent)->isMeta;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetCharCode(PRUint32* aCharCode)
{
  NS_ENSURE_ARG_POINTER(aCharCode);

  switch (mEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_DOWN:
    *aCharCode = 0;
    break;
  case NS_KEY_PRESS:
    *aCharCode = ((nsKeyEvent*)mEvent)->charCode;
    break;
  default:
    break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetKeyCode(PRUint32* aKeyCode)
{
  NS_ENSURE_ARG_POINTER(aKeyCode);

  switch (mEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_PRESS:
  case NS_KEY_DOWN:
    *aKeyCode = ((nsKeyEvent*)mEvent)->keyCode;
    break;
  default:
    *aKeyCode = 0;
    break;
  }

  return NS_OK;
}

/* virtual */
nsresult
nsDOMKeyboardEvent::Which(PRUint32* aWhich)
{
  NS_ENSURE_ARG_POINTER(aWhich);

  switch (mEvent->message) {
    case NS_KEY_UP:
    case NS_KEY_DOWN:
      return GetKeyCode(aWhich);
    case NS_KEY_PRESS:
      //Special case for 4xp bug 62878.  Try to make value of which
      //more closely mirror the values that 4.x gave for RETURN and BACKSPACE
      {
        PRUint32 keyCode = ((nsKeyEvent*)mEvent)->keyCode;
        if (keyCode == NS_VK_RETURN || keyCode == NS_VK_BACK) {
          *aWhich = keyCode;
          return NS_OK;
        }
        return GetCharCode(aWhich);
      }
      break;
    default:
      *aWhich = 0;
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::InitKeyEvent(const nsAString& aType, PRBool aCanBubble, PRBool aCancelable,
                                 nsIDOMWindow* aView, PRBool aCtrlKey, PRBool aAltKey,
                                 PRBool aShiftKey, PRBool aMetaKey,
                                 PRUint32 aKeyCode, PRUint32 aCharCode)
{
  nsresult rv = nsDOMUIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  nsKeyEvent* keyEvent = static_cast<nsKeyEvent*>(mEvent);
  keyEvent->isControl = aCtrlKey;
  keyEvent->isAlt = aAltKey;
  keyEvent->isShift = aShiftKey;
  keyEvent->isMeta = aMetaKey;
  keyEvent->keyCode = aKeyCode;
  keyEvent->charCode = aCharCode;

  return NS_OK;
}

nsresult NS_NewDOMKeyboardEvent(nsIDOMEvent** aInstancePtrResult,
                                nsPresContext* aPresContext,
                                nsKeyEvent *aEvent)
{
  nsDOMKeyboardEvent* it = new nsDOMKeyboardEvent(aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
