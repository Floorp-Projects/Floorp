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

nsXBLEventHandler::nsXBLEventHandler(nsIDOMEventReceiver* aEventReceiver, nsIXBLPrototypeHandler* aHandler,
                                     const nsString& aEventName)
{
  NS_INIT_REFCNT();
  mEventReceiver = aEventReceiver;
  mProtoHandler = aHandler;
  mEventName.Assign(aEventName);
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

NS_IMPL_ISUPPORTS6(nsXBLEventHandler, nsIDOMKeyListener, nsIDOMMouseListener, nsIDOMMenuListener, 
                                      nsIDOMFocusListener, nsIDOMScrollListener, nsIDOMFormListener)

NS_IMETHODIMP
nsXBLEventHandler::BindingAttached()
{
  nsresult ret;
  if (mEventName == NS_LITERAL_STRING("bindingattached")) {
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
  // XXX Write me!!!!
  return NS_OK;
}

nsresult nsXBLEventHandler::HandleEvent(nsIDOMEvent* aEvent)
{
  // Nothing to do.
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyUp(nsIDOMEvent* aKeyEvent)
{
  if (mEventName != NS_LITERAL_STRING("keyup"))
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMKeyEvent> key(do_QueryInterface(aKeyEvent));
    mProtoHandler->KeyEventMatched(key, &matched);
  }

  if (matched)
    ExecuteHandler(NS_LITERAL_STRING("keyup"), aKeyEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyDown(nsIDOMEvent* aKeyEvent)
{
  if (mEventName != NS_LITERAL_STRING("keydown"))
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMKeyEvent> key(do_QueryInterface(aKeyEvent));
    mProtoHandler->KeyEventMatched(key, &matched);
  }

  if (matched)
    ExecuteHandler(NS_LITERAL_STRING("keydown"), aKeyEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyPress(nsIDOMEvent* aKeyEvent)
{
  if (mEventName != NS_LITERAL_STRING("keypress"))
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMKeyEvent> key(do_QueryInterface(aKeyEvent));
    mProtoHandler->KeyEventMatched(key, &matched);
  }

  if (matched)
    ExecuteHandler(NS_LITERAL_STRING("keypress"), aKeyEvent);
  return NS_OK;
}
   
nsresult nsXBLEventHandler::MouseDown(nsIDOMEvent* aMouseEvent)
{
  if (mEventName != NS_LITERAL_STRING("mousedown"))
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(NS_LITERAL_STRING("mousedown"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseUp(nsIDOMEvent* aMouseEvent)
{
  if (mEventName != NS_LITERAL_STRING("mouseup"))
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(NS_LITERAL_STRING("mouseup"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseClick(nsIDOMEvent* aMouseEvent)
{
  if (mEventName != NS_LITERAL_STRING("click"))
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(NS_LITERAL_STRING("click"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  if (mEventName != NS_LITERAL_STRING("dblclick"))
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(NS_LITERAL_STRING("dblclick"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseOver(nsIDOMEvent* aMouseEvent)
{
  if (mEventName != NS_LITERAL_STRING("mouseover"))
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(NS_LITERAL_STRING("mouseover"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseOut(nsIDOMEvent* aMouseEvent)
{
  if (mEventName != NS_LITERAL_STRING("mouseout"))
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(NS_LITERAL_STRING("mouseout"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Focus(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("focus"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("focus"), aEvent);
  return NS_OK;
}


nsresult nsXBLEventHandler::Blur(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("blur"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("blur"), aEvent);
  return NS_OK;
}


nsresult nsXBLEventHandler::Action(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("command"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("command"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Create(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("create"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("create"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Close(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("close"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("close"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Broadcast(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("broadcast"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("broadcast"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::CommandUpdate(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("commandupdate"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("commandupdate"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Overflow(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("overflow"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("overflow"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Underflow(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("underflow"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("underflow"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::OverflowChanged(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("underflowchanged"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("underflowchanged"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Destroy(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("destroy"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("destroy"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Submit(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("submit"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("submit"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Reset(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("reset"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("reset"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Select(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("select"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("select"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Change(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("change"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("change"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Input(nsIDOMEvent* aEvent)
{
  if (mEventName != NS_LITERAL_STRING("input"))
    return NS_OK;

  ExecuteHandler(NS_LITERAL_STRING("input"), aEvent);
  return NS_OK;
}


NS_IMETHODIMP
nsXBLEventHandler::ExecuteHandler(const nsAReadableString & aEventName, nsIDOMEvent* aEvent)
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
  onEvent += aEventName;
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

  if (mEventName == NS_LITERAL_STRING("bindingattached") ||
      mEventName == NS_LITERAL_STRING("bindingdetached")) {
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
 
  // Figure out our type.
  PRBool key = nsXBLBinding::IsKeyHandler(type);
  if (key) {
    mEventReceiver->RemoveEventListener(type, (nsIDOMKeyListener*)this, useCapture);
    return;
  }

  PRBool mouse = nsXBLBinding::IsMouseHandler(type);
  if (mouse) {
    mEventReceiver->RemoveEventListener(type, (nsIDOMMouseListener*)this, useCapture);
    return;
  }

  PRBool focus = nsXBLBinding::IsFocusHandler(type);
  if (focus) {
    mEventReceiver->RemoveEventListener(type, (nsIDOMFocusListener*)this, useCapture);
    return;
  }

  PRBool xul = nsXBLBinding::IsXULHandler(type);
  if (xul) {
    mEventReceiver->RemoveEventListener(type, (nsIDOMMenuListener*)this, useCapture);
    return;
  }

  PRBool scroll = nsXBLBinding::IsScrollHandler(type);
  if (scroll) {
    mEventReceiver->RemoveEventListener(type, (nsIDOMScrollListener*)this, useCapture);
    return;
  }

  PRBool form = nsXBLBinding::IsFormHandler(type);
  if (form) {
    mEventReceiver->RemoveEventListener(type, (nsIDOMFormListener*)this, useCapture);
    return;
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
                      const nsString& aEventName,
                      nsXBLEventHandler** aResult)
{
  *aResult = new nsXBLEventHandler(aRec, aHandler, aEventName);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
