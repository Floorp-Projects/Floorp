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
#include "nsIAtom.h"
#include "nsXBLPrototypeHandler.h"
#include "nsXBLXULHandler.h"
#include "nsIContent.h"
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

PRUint32 nsXBLXULHandler::gRefCnt = 0;
nsIAtom* nsXBLXULHandler::kCreateAtom = nsnull;
nsIAtom* nsXBLXULHandler::kCloseAtom = nsnull;
nsIAtom* nsXBLXULHandler::kCommandUpdateAtom = nsnull;
nsIAtom* nsXBLXULHandler::kBroadcastAtom = nsnull;
nsIAtom* nsXBLXULHandler::kDestroyAtom = nsnull;

nsXBLXULHandler::nsXBLXULHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler)
:nsXBLEventHandler(aReceiver,aHandler)
{
  gRefCnt++;
  if (gRefCnt == 1) {
    kCreateAtom = NS_NewAtom("create");
    kCloseAtom = NS_NewAtom("close");
    kDestroyAtom = NS_NewAtom("destroy");
    kCommandUpdateAtom = NS_NewAtom("commandupdate");
    kBroadcastAtom = NS_NewAtom("broadcast");
  }
}

nsXBLXULHandler::~nsXBLXULHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kCloseAtom);
    NS_RELEASE(kCommandUpdateAtom);
    NS_RELEASE(kBroadcastAtom);
    NS_RELEASE(kCreateAtom);
    NS_RELEASE(kDestroyAtom);
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXBLXULHandler, nsXBLEventHandler, nsIDOMMenuListener)

nsresult nsXBLXULHandler::Action(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != nsXBLPrototypeHandler::kCommandAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLXULHandler::Create(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kCreateAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLXULHandler::Close(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kCloseAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLXULHandler::Broadcast(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kBroadcastAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLXULHandler::CommandUpdate(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kCommandUpdateAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLXULHandler::Destroy(nsIDOMEvent* aEvent)
{
 if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kDestroyAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLXULHandler(nsIDOMEventReceiver* aRec, nsIXBLPrototypeHandler* aHandler, 
                    nsXBLXULHandler** aResult)
{
  *aResult = new nsXBLXULHandler(aRec, aHandler);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
