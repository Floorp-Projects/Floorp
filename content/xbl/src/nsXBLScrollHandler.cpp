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
#include "nsXBLScrollHandler.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
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

PRUint32 nsXBLScrollHandler::gRefCnt = 0;
nsIAtom* nsXBLScrollHandler::kOverflowAtom = nsnull;
nsIAtom* nsXBLScrollHandler::kUnderflowAtom = nsnull;
nsIAtom* nsXBLScrollHandler::kOverflowChangedAtom = nsnull;

nsXBLScrollHandler::nsXBLScrollHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler)
:nsXBLEventHandler(aReceiver,aHandler)
{
  gRefCnt++;
  if (gRefCnt == 1) {
    kOverflowAtom = NS_NewAtom("overflow");
    kUnderflowAtom = NS_NewAtom("underflow");
    kOverflowChangedAtom = NS_NewAtom("overflowchanged");
  }
}

nsXBLScrollHandler::~nsXBLScrollHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kOverflowAtom);
    NS_RELEASE(kUnderflowAtom);
    NS_RELEASE(kOverflowChangedAtom);
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXBLScrollHandler, nsXBLEventHandler, nsIDOMScrollListener)

nsresult nsXBLScrollHandler::Overflow(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kOverflowAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLScrollHandler::Underflow(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kUnderflowAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLScrollHandler::OverflowChanged(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kOverflowChangedAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLScrollHandler(nsIDOMEventReceiver* aRec, nsIXBLPrototypeHandler* aHandler, 
                       nsXBLScrollHandler** aResult)
{
  *aResult = new nsXBLScrollHandler(aRec, aHandler);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
