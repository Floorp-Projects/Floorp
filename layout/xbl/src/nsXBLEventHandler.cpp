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
 * Contributor(s): Brendan Eich (brendan@mozilla.org)
 */

#include "nsCOMPtr.h"
#include "nsIXBLPrototypeHandler.h"
#include "nsXBLEventHandler.h"
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
#include "nsIDOMEventListener.h"
#include "nsXBLBinding.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"

#include "nsIDOMFocusListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMenuListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMScrollListener.h"
#include "nsIDOMFormListener.h"

PRUint32 nsXBLEventHandler::gRefCnt = 0;
nsIAtom* nsXBLEventHandler::kKeyCodeAtom = nsnull;
nsIAtom* nsXBLEventHandler::kCharCodeAtom = nsnull;
nsIAtom* nsXBLEventHandler::kKeyAtom = nsnull;
nsIAtom* nsXBLEventHandler::kActionAtom = nsnull;
nsIAtom* nsXBLEventHandler::kCommandAtom = nsnull;
nsIAtom* nsXBLEventHandler::kClickCountAtom = nsnull;
nsIAtom* nsXBLEventHandler::kButtonAtom = nsnull;
nsIAtom* nsXBLEventHandler::kBindingAttachedAtom = nsnull;
nsIAtom* nsXBLEventHandler::kBindingDetachedAtom = nsnull;
nsIAtom* nsXBLEventHandler::kModifiersAtom = nsnull;

nsXBLEventHandler::nsXBLEventHandler(nsIDOMEventReceiver* aEventReceiver, nsIXBLPrototypeHandler* aHandler)
{
  NS_INIT_REFCNT();
  mEventReceiver = aEventReceiver;
  mProtoHandler = aHandler;
  mNextHandler = nsnull;
  gRefCnt++;
  if (gRefCnt == 1) {
    kKeyCodeAtom = NS_NewAtom("keycode");
    kKeyAtom = NS_NewAtom("key");
    kCharCodeAtom = NS_NewAtom("charcode");
    kModifiersAtom = NS_NewAtom("modifiers");
    kActionAtom = NS_NewAtom("action");
    kCommandAtom = NS_NewAtom("command");
    kClickCountAtom = NS_NewAtom("clickcount");
    kButtonAtom = NS_NewAtom("button");
    kBindingAttachedAtom = NS_NewAtom("bindingattached");
    kBindingDetachedAtom = NS_NewAtom("bindingdetached");
  }
}

nsXBLEventHandler::~nsXBLEventHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kKeyAtom);
    NS_RELEASE(kKeyCodeAtom);
    NS_RELEASE(kCharCodeAtom);
    NS_RELEASE(kModifiersAtom);
    NS_RELEASE(kActionAtom);
    NS_RELEASE(kCommandAtom);
    NS_RELEASE(kButtonAtom);
    NS_RELEASE(kClickCountAtom);
    NS_RELEASE(kBindingAttachedAtom);
    NS_RELEASE(kBindingDetachedAtom);
  }
}

NS_IMPL_ISUPPORTS1(nsXBLEventHandler, nsISupports)

NS_IMETHODIMP
nsXBLEventHandler::BindingAttached()
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  nsresult ret;
  if (eventName.get() == kBindingAttachedAtom) {
    nsMouseEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_MENU_ACTION;
    event.isShift = PR_FALSE;
    event.isControl = PR_FALSE;
    event.isAlt = PR_FALSE;
    event.isMeta = PR_FALSE;
    event.clickCount = 0;
    event.widget = nsnull;

    nsCOMPtr<nsIEventListenerManager> listenerManager;
    if (NS_FAILED(ret = mEventReceiver->GetListenerManager(getter_AddRefs(listenerManager)))) {
      NS_ERROR("Unable to instantiate a listener manager on this event.");
      return ret;
    }
    nsAutoString empty;

    nsCOMPtr<nsIDOMEvent> domEvent;
    if (NS_FAILED(ret = listenerManager->CreateEvent(nsnull, &event, empty, getter_AddRefs(domEvent)))) {
      NS_ERROR("The binding attach handler will fail without the ability to create the event early.");
      return ret;
    }
  
    // We need to explicitly set the target here, because the
    // DOM implementation will try to compute the target from
    // the frame. If we don't have a frame then that breaks.
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(domEvent);
    if (privateEvent) {
      privateEvent->SetTarget(mEventReceiver);
    }

    mProtoHandler->ExecuteHandler(mEventReceiver, domEvent);
  }

  if (mNextHandler)
    return mNextHandler->BindingAttached();
  
  return NS_OK;
}

NS_IMETHODIMP
nsXBLEventHandler::BindingDetached()
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsresult ret;
  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));
  if (eventName.get() == kBindingDetachedAtom) {
    nsMouseEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_MENU_ACTION;
    event.isShift = PR_FALSE;
    event.isControl = PR_FALSE;
    event.isAlt = PR_FALSE;
    event.isMeta = PR_FALSE;
    event.clickCount = 0;
    event.widget = nsnull;

    nsCOMPtr<nsIEventListenerManager> listenerManager;
    if (NS_FAILED(ret = mEventReceiver->GetListenerManager(getter_AddRefs(listenerManager)))) {
      NS_ERROR("Unable to instantiate a listener manager on this event.");
      return ret;
    }
    nsAutoString empty;

    nsCOMPtr<nsIDOMEvent> domEvent;
    if (NS_FAILED(ret = listenerManager->CreateEvent(nsnull, &event, empty, getter_AddRefs(domEvent)))) {
      NS_ERROR("The binding attach handler will fail without the ability to create the event early.");
      return ret;
    }
  
    // We need to explicitly set the target here, because the
    // DOM implementation will try to compute the target from
    // the frame. If we don't have a frame then that breaks.
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(domEvent);
    if (privateEvent) {
      privateEvent->SetTarget(mEventReceiver);
    }

    mProtoHandler->ExecuteHandler(mEventReceiver, domEvent);
  }

  if (mNextHandler)
    return mNextHandler->BindingDetached();
  
  return NS_OK;
}

void
nsXBLEventHandler::RemoveEventHandlers()
{
  if (mNextHandler)
    mNextHandler->RemoveEventHandlers();

  // Figure out if we're using capturing or not.
  if (!mProtoHandler)
    return;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  if (eventName.get() == kBindingAttachedAtom ||
      eventName.get() == kBindingDetachedAtom) {
    // Release and drop.
    mProtoHandler = nsnull;
    NS_RELEASE_THIS();
    return;
  }

  nsCOMPtr<nsIContent> handlerElement;
  mProtoHandler->GetHandlerElement(getter_AddRefs(handlerElement));
  mProtoHandler = nsnull;
  if (!handlerElement)
    return;
  
  PRBool useCapture = PR_FALSE;
  nsAutoString capturer;
  handlerElement->GetAttribute(kNameSpaceID_None, nsXBLBinding::kPhaseAtom, capturer);
  if (capturer == NS_LITERAL_STRING("capturing"))
    useCapture = PR_TRUE;

  nsAutoString type;
  handlerElement->GetAttribute(kNameSpaceID_None, nsXBLBinding::kEventAtom, type);
 
  PRBool found = PR_FALSE;
  nsIID iid;
  nsXBLBinding::GetEventHandlerIID(eventName, &iid, &found);

  nsCOMPtr<nsIDOMEventListener> listener(do_QueryInterface(this));

  if (found && listener)
    mEventReceiver->RemoveEventListener(type, listener, useCapture);
}

/// Helpers that are relegated to the end of the file /////////////////////////////

nsresult
nsXBLEventHandler::GetTextData(nsIContent *aParent, nsString& aResult)
{
  aResult.Truncate(0);

  nsCOMPtr<nsIContent> textChild;
  PRInt32 textCount;
  aParent->ChildCount(textCount);
  nsAutoString answer;
  for (PRInt32 j = 0; j < textCount; j++) {
    // Get the child.
    aParent->ChildAt(j, *getter_AddRefs(textChild));
    nsCOMPtr<nsIDOMText> text(do_QueryInterface(textChild));
    if (text) {
      nsAutoString data;
      text->GetData(data);
      aResult += data;
    }
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLEventHandler(nsIDOMEventReceiver* aRec, nsIXBLPrototypeHandler* aHandler, 
                      nsXBLEventHandler** aResult)
{
  *aResult = new nsXBLEventHandler(aRec, aHandler);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
