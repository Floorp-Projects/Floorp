/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 */

#include "nsCOMPtr.h"
#include "nsIXBLPrototypeHandler.h"
#include "nsXBLKeyHandler.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIJSEventListener.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMText.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMEventReceiver.h"
#include "nsXBLBinding.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"

PRUint32 nsXBLKeyHandler::gRefCnt = 0;
nsIAtom* nsXBLKeyHandler::kKeyDownAtom = nsnull;
nsIAtom* nsXBLKeyHandler::kKeyUpAtom = nsnull;
nsIAtom* nsXBLKeyHandler::kKeyPressAtom = nsnull;

nsXBLKeyHandler::nsXBLKeyHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler)
:nsXBLEventHandler(aReceiver,aHandler)
{
  gRefCnt++;
  if (gRefCnt == 1) {
    kKeyUpAtom = NS_NewAtom("keyup");
    kKeyDownAtom = NS_NewAtom("keydown");
    kKeyPressAtom = NS_NewAtom("keypress");
  }
}

nsXBLKeyHandler::~nsXBLKeyHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kKeyUpAtom);
    NS_RELEASE(kKeyDownAtom);
    NS_RELEASE(kKeyPressAtom);
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXBLKeyHandler, nsXBLEventHandler, nsIDOMKeyListener)

static inline nsresult DoKey(nsIAtom* aEventType, nsIXBLPrototypeHandler* aHandler, nsIDOMEvent* aKeyEvent,
                             nsIDOMEventReceiver* aReceiver)
{
  if (!aHandler)
    return NS_OK;

  PRBool matched = PR_FALSE;
  nsCOMPtr<nsIDOMKeyEvent> key(do_QueryInterface(aKeyEvent));
  aHandler->KeyEventMatched(aEventType, key, &matched);
  if (matched)
    aHandler->ExecuteHandler(aReceiver, aKeyEvent);
  return NS_OK;
}

nsresult nsXBLKeyHandler::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return DoKey(kKeyUpAtom, mProtoHandler, aKeyEvent, mEventReceiver);
}

nsresult nsXBLKeyHandler::KeyDown(nsIDOMEvent* aKeyEvent)
{
  return DoKey(kKeyDownAtom, mProtoHandler, aKeyEvent, mEventReceiver);
}

nsresult nsXBLKeyHandler::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return DoKey(kKeyPressAtom, mProtoHandler, aKeyEvent, mEventReceiver);
}
 
///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLKeyHandler(nsIDOMEventReceiver* aRec, nsIXBLPrototypeHandler* aHandler, 
                    nsXBLKeyHandler** aResult)
{
  *aResult = new nsXBLKeyHandler(aRec, aHandler);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
