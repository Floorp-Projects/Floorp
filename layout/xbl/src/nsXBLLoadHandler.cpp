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
#include "nsXBLLoadHandler.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMEventReceiver.h"
#include "nsXBLBinding.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"

PRUint32 nsXBLLoadHandler::gRefCnt = 0;
nsIAtom* nsXBLLoadHandler::kLoadAtom = nsnull;
nsIAtom* nsXBLLoadHandler::kUnloadAtom = nsnull;
nsIAtom* nsXBLLoadHandler::kAbortAtom = nsnull;
nsIAtom* nsXBLLoadHandler::kErrorAtom = nsnull;

nsXBLLoadHandler::nsXBLLoadHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler)
:nsXBLEventHandler(aReceiver,aHandler)
{
  gRefCnt++;
  if (gRefCnt == 1) {
    kAbortAtom = NS_NewAtom("abort");
    kErrorAtom = NS_NewAtom("error");
    kLoadAtom = NS_NewAtom("load");
    kUnloadAtom = NS_NewAtom("unload");
  }
}

nsXBLLoadHandler::~nsXBLLoadHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kAbortAtom);
    NS_RELEASE(kLoadAtom);
    NS_RELEASE(kUnloadAtom);
    NS_RELEASE(kErrorAtom);
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXBLLoadHandler, nsXBLEventHandler, nsIDOMLoadListener)

nsresult nsXBLLoadHandler::Load(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kLoadAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLLoadHandler::Unload(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kUnloadAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLLoadHandler::Error(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kErrorAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLLoadHandler::Abort(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kAbortAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLLoadHandler(nsIDOMEventReceiver* aRec, nsIXBLPrototypeHandler* aHandler, 
                     nsXBLLoadHandler** aResult)
{
  *aResult = new nsXBLLoadHandler(aRec, aHandler);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
