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
#include "nsIControllers.h" // XXX Will go away
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

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

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

nsIAtom* nsXBLEventHandler::kKeyDownAtom = nsnull;
nsIAtom* nsXBLEventHandler::kKeyUpAtom = nsnull;
nsIAtom* nsXBLEventHandler::kKeyPressAtom = nsnull;
nsIAtom* nsXBLEventHandler::kMouseDownAtom = nsnull;
nsIAtom* nsXBLEventHandler::kMouseUpAtom = nsnull;
nsIAtom* nsXBLEventHandler::kMouseClickAtom = nsnull;
nsIAtom* nsXBLEventHandler::kMouseDblClickAtom = nsnull;
nsIAtom* nsXBLEventHandler::kMouseOverAtom = nsnull;
nsIAtom* nsXBLEventHandler::kMouseOutAtom = nsnull;
nsIAtom* nsXBLEventHandler::kFocusAtom = nsnull;
nsIAtom* nsXBLEventHandler::kBlurAtom = nsnull;
nsIAtom* nsXBLEventHandler::kCreateAtom = nsnull;
nsIAtom* nsXBLEventHandler::kCloseAtom = nsnull;
nsIAtom* nsXBLEventHandler::kCommandUpdateAtom = nsnull;
nsIAtom* nsXBLEventHandler::kBroadcastAtom = nsnull;
nsIAtom* nsXBLEventHandler::kDestroyAtom = nsnull;
nsIAtom* nsXBLEventHandler::kOverflowAtom = nsnull;
nsIAtom* nsXBLEventHandler::kUnderflowAtom = nsnull;
nsIAtom* nsXBLEventHandler::kOverflowChangedAtom = nsnull;
nsIAtom* nsXBLEventHandler::kSubmitAtom = nsnull;
nsIAtom* nsXBLEventHandler::kResetAtom = nsnull;
nsIAtom* nsXBLEventHandler::kInputAtom = nsnull;
nsIAtom* nsXBLEventHandler::kSelectAtom = nsnull;
nsIAtom* nsXBLEventHandler::kChangeAtom = nsnull;

nsXBLEventHandler::nsXBLEventHandler(nsIDOMEventReceiver* aEventReceiver, nsIXBLPrototypeHandler* aHandler,
                                     nsIAtom* aEventName)
{
  NS_INIT_REFCNT();
  mEventReceiver = aEventReceiver;
  mProtoHandler = aHandler;
  mEventName = aEventName;
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

    kKeyUpAtom = NS_NewAtom("keyup");
    kKeyDownAtom = NS_NewAtom("keydown");
    kKeyPressAtom = NS_NewAtom("keypress");
    kMouseDownAtom = NS_NewAtom("mousedown");
    kMouseUpAtom = NS_NewAtom("mouseup");
    kMouseClickAtom = NS_NewAtom("click");
    kMouseDblClickAtom = NS_NewAtom("dblclick");
    kMouseOverAtom = NS_NewAtom("mouseover");
    kMouseOutAtom = NS_NewAtom("mouseout");

    kFocusAtom = NS_NewAtom("focus");
    kBlurAtom = NS_NewAtom("blur");
    kCreateAtom = NS_NewAtom("create");
    kCloseAtom = NS_NewAtom("close");
    kDestroyAtom = NS_NewAtom("destroy");
    kCommandUpdateAtom = NS_NewAtom("commandupdate");
    kBroadcastAtom = NS_NewAtom("broadcast");
    kOverflowAtom = NS_NewAtom("overflow");
    kUnderflowAtom = NS_NewAtom("underflow");
    kOverflowChangedAtom = NS_NewAtom("overflowchanged");
    kInputAtom = NS_NewAtom("input");
    kSelectAtom = NS_NewAtom("select");
    kChangeAtom = NS_NewAtom("change");
    kSubmitAtom = NS_NewAtom("submit");
    kResetAtom = NS_NewAtom("reset");
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

    NS_RELEASE(kKeyUpAtom);
    NS_RELEASE(kKeyDownAtom);
    NS_RELEASE(kKeyPressAtom);
    NS_RELEASE(kMouseUpAtom);
    NS_RELEASE(kMouseDownAtom);
    NS_RELEASE(kMouseClickAtom);
    NS_RELEASE(kMouseDblClickAtom);
    NS_RELEASE(kMouseOverAtom);
    NS_RELEASE(kMouseOutAtom);

    NS_RELEASE(kFocusAtom);
    NS_RELEASE(kBlurAtom);
    NS_RELEASE(kCloseAtom);
    NS_RELEASE(kCommandUpdateAtom);
    NS_RELEASE(kBroadcastAtom);
    NS_RELEASE(kCreateAtom);
    NS_RELEASE(kDestroyAtom);
    NS_RELEASE(kOverflowAtom);
    NS_RELEASE(kUnderflowAtom);
    NS_RELEASE(kOverflowChangedAtom);
    NS_RELEASE(kInputAtom);
    NS_RELEASE(kSubmitAtom);
    NS_RELEASE(kResetAtom);
    NS_RELEASE(kSelectAtom);
    NS_RELEASE(kChangeAtom);
  }
}

NS_IMPL_ISUPPORTS6(nsXBLEventHandler, nsIDOMKeyListener, nsIDOMMouseListener, nsIDOMMenuListener, 
                                      nsIDOMFocusListener, nsIDOMScrollListener, nsIDOMFormListener)

NS_IMETHODIMP
nsXBLEventHandler::BindingAttached()
{
  nsresult ret;
  if (mEventName.get() == kBindingAttachedAtom) {
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

    ExecuteHandler(mEventName, domEvent);
  }

  if (mNextHandler)
    return mNextHandler->BindingAttached();
  
  return NS_OK;
}

NS_IMETHODIMP
nsXBLEventHandler::BindingDetached()
{
  nsresult ret;
  if (mEventName.get() == kBindingDetachedAtom) {
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

    ExecuteHandler(mEventName, domEvent);
  }

  if (mNextHandler)
    return mNextHandler->BindingAttached();
  
  return NS_OK;
}

nsresult nsXBLEventHandler::HandleEvent(nsIDOMEvent* aEvent)
{
  // Nothing to do.
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyUp(nsIDOMEvent* aKeyEvent)
{
  if (mEventName.get() != kKeyUpAtom)
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMKeyEvent> key(do_QueryInterface(aKeyEvent));
    mProtoHandler->KeyEventMatched(key, &matched);
  }

  if (matched)
    ExecuteHandler(mEventName, aKeyEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyDown(nsIDOMEvent* aKeyEvent)
{
  if (mEventName.get() != kKeyDownAtom)
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMKeyEvent> key(do_QueryInterface(aKeyEvent));
    mProtoHandler->KeyEventMatched(key, &matched);
  }

  if (matched)
    ExecuteHandler(mEventName, aKeyEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyPress(nsIDOMEvent* aKeyEvent)
{
  if (mEventName.get() != kKeyPressAtom)
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMKeyEvent> key(do_QueryInterface(aKeyEvent));
    mProtoHandler->KeyEventMatched(key, &matched);
  }

  if (matched)
    ExecuteHandler(mEventName, aKeyEvent);
  return NS_OK;
}
   
nsresult nsXBLEventHandler::MouseDown(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseDownAtom)
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(mEventName, aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseUp(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseUpAtom)
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(mEventName, aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseClick(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseClickAtom)
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(mEventName, aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseDblClickAtom)
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(mEventName, aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseOver(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseOverAtom)
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(mEventName, aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseOut(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseOutAtom)
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(mEventName, aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Focus(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kFocusAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}


nsresult nsXBLEventHandler::Blur(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kBlurAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}


nsresult nsXBLEventHandler::Action(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kCommandAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Create(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kCreateAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Close(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kCloseAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Broadcast(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kBroadcastAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::CommandUpdate(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kCommandUpdateAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Overflow(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kOverflowAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Underflow(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kUnderflowAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::OverflowChanged(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kOverflowChangedAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Destroy(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kDestroyAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Submit(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kSubmitAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Reset(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kResetAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Select(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kSelectAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Change(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kChangeAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Input(nsIDOMEvent* aEvent)
{
  if (mEventName.get() != kInputAtom)
    return NS_OK;

  ExecuteHandler(mEventName, aEvent);
  return NS_OK;
}


NS_IMETHODIMP
nsXBLEventHandler::ExecuteHandler(nsIAtom* aEventName, nsIDOMEvent* aEvent)
{
  if (!mProtoHandler)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> handlerElement;
  mProtoHandler->GetHandlerElement(getter_AddRefs(handlerElement));
  if (!handlerElement)
    return NS_ERROR_FAILURE;

  // This is a special-case optimization to make command handling fast.
  // It isn't really a part of XBL, but it helps speed things up.
  nsAutoString command;
  handlerElement->GetAttribute(kNameSpaceID_None, kCommandAtom, command);
  if (!command.IsEmpty()) {
    // Make sure the XBL doc is chrome.
    // Fix for bug #45989
    if (!mProtoHandler)
      return NS_OK;
    nsCOMPtr<nsIContent> handler;
    mProtoHandler->GetHandlerElement(getter_AddRefs(handler));
    nsCOMPtr<nsIDocument> document;
    handler->GetDocument(*getter_AddRefs(document));
    nsCOMPtr<nsIURI> url = getter_AddRefs(document->GetDocumentURL());
    nsXPIDLCString scheme;
    url->GetScheme(getter_Copies(scheme));
    if (PL_strcmp(scheme, "chrome") != 0)
      return NS_OK;

    // We are the default action for this command.
    // Stop any other default action from executing.
    aEvent->PreventDefault();

    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(aEvent);
    if(privateEvent) 
    {
      PRBool dispatchStopped;
      privateEvent->IsDispatchStopped(&dispatchStopped);
      if(dispatchStopped)
        return NS_OK;
    }

    // Instead of executing JS, let's get the controller for the bound
    // element and call doCommand on it.
    nsCOMPtr<nsIController> controller;
    GetController(getter_AddRefs(controller));

    if (controller) {
      controller->DoCommand(command.GetUnicode());
    }

    return NS_OK;
  }

  // Look for a compiled handler on the element. 
  // Should be compiled and bound with "on" in front of the name.
  nsAutoString onEvent; onEvent.AssignWithConversion("onxbl");
  nsAutoString str;
  mEventName->ToString(str);
  onEvent += str;
  nsCOMPtr<nsIAtom> onEventAtom = getter_AddRefs(NS_NewAtom(onEvent));

  void* handler = nsnull;
  
  // Compile the event handler.
  nsAutoString handlerText;
  handlerElement->GetAttribute(kNameSpaceID_None, kActionAtom, handlerText);
  if (handlerText.IsEmpty()) {
    // look to see if action content is contained by the handler element
    GetTextData(handlerElement, handlerText);
    if (handlerText.IsEmpty())
      return NS_OK; // For whatever reason, they didn't give us anything to do.
  }
  
  // Compile the handler and bind it to the element.
  nsCOMPtr<nsIScriptGlobalObject> boundGlobal(do_QueryInterface(mEventReceiver));
  if (!boundGlobal) {
    nsCOMPtr<nsIDocument> boundDocument(do_QueryInterface(mEventReceiver));
    if (!boundDocument) {
      // We must be an element.
      nsCOMPtr<nsIContent> content(do_QueryInterface(mEventReceiver));
      content->GetDocument(*getter_AddRefs(boundDocument));
      if (!boundDocument)
        return NS_OK;
    }

    boundDocument->GetScriptGlobalObject(getter_AddRefs(boundGlobal));
  }

  nsCOMPtr<nsIScriptContext> boundContext;
  boundGlobal->GetContext(getter_AddRefs(boundContext));

  nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(mEventReceiver));
  void* scriptObject;
  owner->GetScriptObject(boundContext, &scriptObject);
  
  boundContext->CompileEventHandler(scriptObject, onEventAtom, handlerText,
                               PR_TRUE, &handler);

  // Temporarily bind it to the bound element
  boundContext->BindCompiledEventHandler(scriptObject, onEventAtom, handler);

  // Execute it.
  nsCOMPtr<nsIDOMEventListener> eventListener;
  NS_NewJSEventListener(getter_AddRefs(eventListener), boundContext, owner);

  nsCOMPtr<nsIJSEventListener> jsListener(do_QueryInterface(eventListener));
  jsListener->SetEventName(onEventAtom);
  eventListener->HandleEvent(aEvent);

  // Now unbind it.
  boundContext->BindCompiledEventHandler(scriptObject, onEventAtom, nsnull);

  return NS_OK;
}

NS_IMETHODIMP
nsXBLEventHandler::GetController(nsIController** aResult)
{
  // XXX Fix this so there's a generic interface that describes controllers, 
  // This code should have no special knowledge of what objects might have controllers.
  nsCOMPtr<nsIControllers> controllers;

  nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(mEventReceiver));
  if (xulElement)
    xulElement->GetControllers(getter_AddRefs(controllers));

  if (!controllers) {
    nsCOMPtr<nsIDOMNSHTMLTextAreaElement> htmlTextArea(do_QueryInterface(mEventReceiver));
    if (htmlTextArea)
      htmlTextArea->GetControllers(getter_AddRefs(controllers));
  }

  if (!controllers) {
    nsCOMPtr<nsIDOMNSHTMLInputElement> htmlInputElement(do_QueryInterface(mEventReceiver));
    if (htmlInputElement)
      htmlInputElement->GetControllers(getter_AddRefs(controllers));
  }

  if (!controllers) {
    nsCOMPtr<nsIDOMWindowInternal> domWindow(do_QueryInterface(mEventReceiver));
    if (domWindow)
      domWindow->GetControllers(getter_AddRefs(controllers));
  }

  // Return the first controller.
  // XXX This code should be checking the command name and using supportscommand and
  // iscommandenabled.
  if (controllers) {
    controllers->GetControllerAt(0, aResult);
  }
  else *aResult = nsnull;

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

  if (mEventName.get() == kBindingAttachedAtom ||
      mEventName.get() == kBindingDetachedAtom) {
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
  nsXBLBinding::GetEventHandlerIID(mEventName, &iid, &found);

  if (found) {
    if (iid.Equals(NS_GET_IID(nsIDOMMouseListener)))
      mEventReceiver->AddEventListener(type, (nsIDOMMouseListener*)this, useCapture);
    else if(iid.Equals(NS_GET_IID(nsIDOMKeyListener)))
      mEventReceiver->AddEventListener(type, (nsIDOMKeyListener*)this, useCapture);
    else if(iid.Equals(NS_GET_IID(nsIDOMFocusListener)))
      mEventReceiver->AddEventListener(type, (nsIDOMFocusListener*)this, useCapture);
    else if (iid.Equals(NS_GET_IID(nsIDOMMenuListener)))
      mEventReceiver->AddEventListener(type, (nsIDOMMenuListener*)this, useCapture);
    else if (iid.Equals(NS_GET_IID(nsIDOMScrollListener)))
      mEventReceiver->AddEventListener(type, (nsIDOMScrollListener*)this, useCapture);
    else if (iid.Equals(NS_GET_IID(nsIDOMFormListener)))
      mEventReceiver->AddEventListener(type, (nsIDOMFormListener*)this, useCapture);
  }
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
                      nsIAtom* aEventName,
                      nsXBLEventHandler** aResult)
{
  *aResult = new nsXBLEventHandler(aRec, aHandler, aEventName);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
