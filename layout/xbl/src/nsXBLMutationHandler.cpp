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
#include "nsXBLMutationHandler.h"
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

PRUint32 nsXBLMutationHandler::gRefCnt = 0;
nsIAtom* nsXBLMutationHandler::kSubtreeModifiedAtom = nsnull;
nsIAtom* nsXBLMutationHandler::kAttrModifiedAtom = nsnull;
nsIAtom* nsXBLMutationHandler::kCharacterDataModifiedAtom = nsnull;
nsIAtom* nsXBLMutationHandler::kNodeRemovedAtom = nsnull;
nsIAtom* nsXBLMutationHandler::kNodeInsertedAtom = nsnull;
nsIAtom* nsXBLMutationHandler::kNodeRemovedFromDocumentAtom = nsnull;
nsIAtom* nsXBLMutationHandler::kNodeInsertedIntoDocumentAtom = nsnull;

nsXBLMutationHandler::nsXBLMutationHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler)
:nsXBLEventHandler(aReceiver,aHandler)
{
  gRefCnt++;
  if (gRefCnt == 1) {
    kNodeRemovedAtom = NS_NewAtom("DOMNodeRemoved");
    kNodeInsertedAtom = NS_NewAtom("DOMNodeInserted");
    kNodeRemovedFromDocumentAtom = NS_NewAtom("DOMNodeRemovedFromDocument");
    kNodeInsertedIntoDocumentAtom = NS_NewAtom("DOMNodeInsertedIntoDocument");
    kSubtreeModifiedAtom = NS_NewAtom("DOMSubtreeModified");
    kAttrModifiedAtom = NS_NewAtom("DOMAttrModified");
    kCharacterDataModifiedAtom = NS_NewAtom("DOMCharacterDataModified");
  }
}

nsXBLMutationHandler::~nsXBLMutationHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kSubtreeModifiedAtom);
    NS_RELEASE(kAttrModifiedAtom);
    NS_RELEASE(kCharacterDataModifiedAtom);
    NS_RELEASE(kNodeInsertedAtom);
    NS_RELEASE(kNodeRemovedAtom);
    NS_RELEASE(kNodeInsertedIntoDocumentAtom);
    NS_RELEASE(kNodeRemovedFromDocumentAtom);
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXBLMutationHandler, nsXBLEventHandler, nsIDOMMutationListener)

nsresult nsXBLMutationHandler::SubtreeModified(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kSubtreeModifiedAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLMutationHandler::AttrModified(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kAttrModifiedAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLMutationHandler::CharacterDataModified(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kCharacterDataModifiedAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLMutationHandler::NodeInserted(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kNodeInsertedAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLMutationHandler::NodeRemoved(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kNodeRemovedAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLMutationHandler::NodeInsertedIntoDocument(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kNodeInsertedAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

nsresult nsXBLMutationHandler::NodeRemovedFromDocument(nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() != kNodeRemovedAtom)
    return NS_OK;

  mProtoHandler->ExecuteHandler(mEventReceiver, aEvent);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLMutationHandler(nsIDOMEventReceiver* aRec, nsIXBLPrototypeHandler* aHandler, 
                     nsXBLMutationHandler** aResult)
{
  *aResult = new nsXBLMutationHandler(aRec, aHandler);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
