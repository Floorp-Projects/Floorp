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
#include "nsIDOMWindow.h"
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

PRInt32 nsXBLEventHandler::kAccessKey = -1;

const PRInt32 nsXBLEventHandler::cShift = (1<<1);
const PRInt32 nsXBLEventHandler::cAlt = (1<<2);
const PRInt32 nsXBLEventHandler::cControl = (1<<3);
const PRInt32 nsXBLEventHandler::cMeta = (1<<4);

nsXBLEventHandler::nsXBLEventHandler(nsIDOMEventReceiver* aEventReceiver, nsIContent* aHandlerElement,
                                     const nsString& aEventName)
{
  NS_INIT_REFCNT();
  mEventReceiver = aEventReceiver;
  mHandlerElement = aHandlerElement;
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

    // Get the primary accelerator key.
    InitAccessKey();
  }

  // Make sure our key mask is initialized.
  ConstructKeyMask();
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

NS_IMPL_ISUPPORTS5(nsXBLEventHandler, nsIDOMKeyListener, nsIDOMMouseListener, nsIDOMMenuListener, nsIDOMFocusListener, nsIDOMScrollListener)

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

  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (KeyEventMatched(keyEvent))
    ExecuteHandler(NS_LITERAL_STRING("keyup"), aKeyEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyDown(nsIDOMEvent* aKeyEvent)
{
  if (mEventName != NS_LITERAL_STRING("keydown"))
    return NS_OK;

  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (KeyEventMatched(keyEvent))
    ExecuteHandler(NS_LITERAL_STRING("keydown"), aKeyEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyPress(nsIDOMEvent* aKeyEvent)
{
  if (mEventName != NS_LITERAL_STRING("keypress"))
    return NS_OK;

  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (KeyEventMatched(keyEvent))
    ExecuteHandler(NS_LITERAL_STRING("keypress"), aKeyEvent);
  return NS_OK;
}
   
nsresult nsXBLEventHandler::MouseDown(nsIDOMEvent* aMouseEvent)
{
  if (mEventName != NS_LITERAL_STRING("mousedown"))
    return NS_OK;

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler(NS_LITERAL_STRING("mousedown"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseUp(nsIDOMEvent* aMouseEvent)
{
  if (mEventName != NS_LITERAL_STRING("mouseup"))
    return NS_OK;

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler(NS_LITERAL_STRING("mouseup"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseClick(nsIDOMEvent* aMouseEvent)
{
  if (mEventName != NS_LITERAL_STRING("click"))
    return NS_OK;

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler(NS_LITERAL_STRING("click"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  if (mEventName != NS_LITERAL_STRING("dblclick"))
    return NS_OK;

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler(NS_LITERAL_STRING("dblclick"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseOver(nsIDOMEvent* aMouseEvent)
{
  if (mEventName != NS_LITERAL_STRING("mouseover"))
    return NS_OK;

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler(NS_LITERAL_STRING("mouseover"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseOut(nsIDOMEvent* aMouseEvent)
{
  if (mEventName != NS_LITERAL_STRING("mouseout"))
    return NS_OK;

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
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

/////////////////////////////////////////////////////////////////////////////
// Get the menu access key from prefs.
// XXX Eventually pick up using CSS3 key-equivalent property or somesuch
void
nsXBLEventHandler::InitAccessKey()
{
  if (kAccessKey >= 0)
    return;

  // Compiled-in defaults, in case we can't get the pref --
  // mac doesn't have menu shortcuts, other platforms use alt.
#ifndef XP_MAC
  kAccessKey = nsIDOMKeyEvent::DOM_VK_ALT;
#else
  kAccessKey = 0;
#endif

  // Get the menu access key value from prefs, overriding the default:
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
  if (NS_SUCCEEDED(rv) && prefs)
  {
    rv = prefs->GetIntPref("ui.key.menuAccessKey", &kAccessKey);
  }
}


PRBool 
nsXBLEventHandler::KeyEventMatched(nsIDOMKeyEvent* aKeyEvent)
{
  if (!mHandlerElement)
    return PR_FALSE;

  nsAutoString trueString; trueString.AssignWithConversion("true");
  nsAutoString falseString; falseString.AssignWithConversion("false");

  // Get the keycode and charcode of the key event.
  PRUint32 keyCode, charCode;
  aKeyEvent->GetKeyCode(&keyCode);
  aKeyEvent->GetCharCode(&charCode);

  PRBool keyMatched = PR_TRUE;
  
  nsAutoString key, keyCodeStr, charCodeStr;
  mHandlerElement->GetAttribute(kNameSpaceID_None, kKeyAtom, key);
  mHandlerElement->GetAttribute(kNameSpaceID_None, kKeyCodeAtom, keyCodeStr);
  mHandlerElement->GetAttribute(kNameSpaceID_None, kCharCodeAtom, charCodeStr);
  
  if (key.IsEmpty() && keyCodeStr.IsEmpty() && charCodeStr.IsEmpty())
    return PR_TRUE;

  if (!key.IsEmpty())
    keyMatched = IsMatchingCharCode(charCode, key);
  else if (!keyCodeStr.IsEmpty())
    keyMatched = IsMatchingKeyCode(keyCode, keyCodeStr);
  else if (!charCodeStr.IsEmpty())
    keyMatched = IsMatchingCharCode(charCode, charCodeStr);
  
  if (!keyMatched)
    return PR_FALSE;

  // Now check modifier keys
  return MatchesMask(aKeyEvent);
}

PRBool 
nsXBLEventHandler::MouseEventMatched(nsIDOMMouseEvent* aMouseEvent)
{
  if (!mHandlerElement)
    return PR_FALSE;

  nsAutoString trueString; trueString.AssignWithConversion("true");
  nsAutoString falseString; falseString.AssignWithConversion("false");

  // Check for button and modifier keys.
  // Check button and clickcounts
  nsAutoString buttonStr, clickCountStr;
  mHandlerElement->GetAttribute(kNameSpaceID_None, kClickCountAtom, clickCountStr);
  mHandlerElement->GetAttribute(kNameSpaceID_None, kButtonAtom, buttonStr);
  
  if (buttonStr.IsEmpty() && clickCountStr.IsEmpty())
    return PR_TRUE;

  if (!clickCountStr.IsEmpty()) {
    PRInt32 val, error, clickcount;
    val = clickCountStr.ToInteger(&error);
    aMouseEvent->GetDetail(&clickcount);
    if (val != clickcount)
      return PR_FALSE;
  }

  if (!buttonStr.IsEmpty()) {
    PRInt32 val, error;
    unsigned short button;
    val = buttonStr.ToInteger(&error);
    aMouseEvent->GetButton(&button);
    if (val != button)
      return PR_FALSE;
  }
  
  return MatchesMask(aMouseEvent);
}

NS_IMETHODIMP
nsXBLEventHandler::ExecuteHandler(const nsAReadableString & aEventName, nsIDOMEvent* aEvent)
{
  if (!mHandlerElement)
    return NS_ERROR_FAILURE;

  // This is a special-case optimization to make command handling fast.
  // It isn't really a part of XBL, but it helps speed things up.
  nsAutoString command;
  mHandlerElement->GetAttribute(kNameSpaceID_None, kCommandAtom, command);
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
  mHandlerElement->GetAttribute(kNameSpaceID_None, kActionAtom, handlerText);
  if (handlerText.IsEmpty()) {
    // look to see if action content is contained by the handler element
    GetTextData(mHandlerElement, handlerText);
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
    nsCOMPtr<nsIDOMWindow> domWindow(do_QueryInterface(mEventReceiver));
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
  // XXX Handle unhooking listeners attached to the document or window!
  if (mNextHandler)
    mNextHandler->RemoveEventHandlers();

  if (mEventName == NS_LITERAL_STRING("bindingattached") ||
      mEventName == NS_LITERAL_STRING("bindingdetached")) {
    // Release and drop.
    NS_RELEASE_THIS();
    return;
  }

  // Figure out if we're using capturing or not.
  PRBool useCapture = PR_FALSE;
  nsAutoString capturer;
  mHandlerElement->GetAttribute(kNameSpaceID_None, nsXBLBinding::kPhaseAtom, capturer);
  if (capturer == NS_LITERAL_STRING("capturing"))
    useCapture = PR_TRUE;

  // XXX Will potentially be comma-separated
  nsAutoString type;
  mHandlerElement->GetAttribute(kNameSpaceID_None, nsXBLBinding::kEventAtom, type);
 
  // Figure out our type.
  PRBool mouse = nsXBLBinding::IsMouseHandler(type);
  PRBool key = nsXBLBinding::IsKeyHandler(type);
  PRBool focus = nsXBLBinding::IsFocusHandler(type);
  PRBool xul = nsXBLBinding::IsXULHandler(type);
  PRBool scroll = nsXBLBinding::IsScrollHandler(type);

  // Remove the event listener.
  if (mouse)
    mEventReceiver->RemoveEventListener(type, (nsIDOMMouseListener*)this, useCapture);
  else if(key)
    mEventReceiver->RemoveEventListener(type, (nsIDOMKeyListener*)this, useCapture);
  else if(focus)
    mEventReceiver->RemoveEventListener(type, (nsIDOMFocusListener*)this, useCapture);
  else if(scroll)
    mEventReceiver->RemoveEventListener(type, (nsIDOMScrollListener*)this, useCapture);
  else if (xul)
    mEventReceiver->RemoveEventListener(type, (nsIDOMMenuListener*)this, useCapture);
}

/// Helpers that are relegated to the end of the file /////////////////////////////

enum {
    VK_CANCEL = 3,
    VK_BACK = 8,
    VK_TAB = 9,
    VK_CLEAR = 12,
    VK_RETURN = 13,
    VK_ENTER = 14,
    VK_SHIFT = 16,
    VK_CONTROL = 17,
    VK_ALT = 18,
    VK_PAUSE = 19,
    VK_CAPS_LOCK = 20,
    VK_ESCAPE = 27,
    VK_SPACE = 32,
    VK_PAGE_UP = 33,
    VK_PAGE_DOWN = 34,
    VK_END = 35,
    VK_HOME = 36,
    VK_LEFT = 37,
    VK_UP = 38,
    VK_RIGHT = 39,
    VK_DOWN = 40,
    VK_PRINTSCREEN = 44,
    VK_INSERT = 45,
    VK_DELETE = 46,
    VK_0 = 48,
    VK_1 = 49,
    VK_2 = 50,
    VK_3 = 51,
    VK_4 = 52,
    VK_5 = 53,
    VK_6 = 54,
    VK_7 = 55,
    VK_8 = 56,
    VK_9 = 57,
    VK_SEMICOLON = 59,
    VK_EQUALS = 61,
    VK_A = 65,
    VK_B = 66,
    VK_C = 67,
    VK_D = 68,
    VK_E = 69,
    VK_F = 70,
    VK_G = 71,
    VK_H = 72,
    VK_I = 73,
    VK_J = 74,
    VK_K = 75,
    VK_L = 76,
    VK_M = 77,
    VK_N = 78,
    VK_O = 79,
    VK_P = 80,
    VK_Q = 81,
    VK_R = 82,
    VK_S = 83,
    VK_T = 84,
    VK_U = 85,
    VK_V = 86,
    VK_W = 87,
    VK_X = 88,
    VK_Y = 89,
    VK_Z = 90,
    VK_NUMPAD0 = 96,
    VK_NUMPAD1 = 97,
    VK_NUMPAD2 = 98,
    VK_NUMPAD3 = 99,
    VK_NUMPAD4 = 100,
    VK_NUMPAD5 = 101,
    VK_NUMPAD6 = 102,
    VK_NUMPAD7 = 103,
    VK_NUMPAD8 = 104,
    VK_NUMPAD9 = 105,
    VK_MULTIPLY = 106,
    VK_ADD = 107,
    VK_SEPARATOR = 108,
    VK_SUBTRACT = 109,
    VK_DECIMAL = 110,
    VK_DIVIDE = 111,
    VK_F1 = 112,
    VK_F2 = 113,
    VK_F3 = 114,
    VK_F4 = 115,
    VK_F5 = 116,
    VK_F6 = 117,
    VK_F7 = 118,
    VK_F8 = 119,
    VK_F9 = 120,
    VK_F10 = 121,
    VK_F11 = 122,
    VK_F12 = 123,
    VK_F13 = 124,
    VK_F14 = 125,
    VK_F15 = 126,
    VK_F16 = 127,
    VK_F17 = 128,
    VK_F18 = 129,
    VK_F19 = 130,
    VK_F20 = 131,
    VK_F21 = 132,
    VK_F22 = 133,
    VK_F23 = 134,
    VK_F24 = 135,
    VK_NUM_LOCK = 144,
    VK_SCROLL_LOCK = 145,
    VK_COMMA = 188,
    VK_PERIOD = 190,
    VK_SLASH = 191,
    VK_BACK_QUOTE = 192,
    VK_OPEN_BRACKET = 219,
    VK_BACK_SLASH = 220,
    VK_CLOSE_BRACKET = 221,
    VK_QUOTE = 222
};

PRBool nsXBLEventHandler::IsMatchingKeyCode(const PRUint32 aChar, const nsString& aKeyName)
{
  PRBool ret = PR_FALSE;

  switch(aChar) {
    case VK_CANCEL:
      if(aKeyName.EqualsIgnoreCase("VK_CANCEL"))
        ret = PR_TRUE;
        break;
    case VK_BACK:
      if(aKeyName.EqualsIgnoreCase("VK_BACK"))
        ret = PR_TRUE;
        break;
    case VK_TAB:
      if(aKeyName.EqualsIgnoreCase("VK_TAB"))
        ret = PR_TRUE;
        break;
    case VK_CLEAR:
      if(aKeyName.EqualsIgnoreCase("VK_CLEAR"))
        ret = PR_TRUE;
        break;
    case VK_RETURN:
      if(aKeyName.EqualsIgnoreCase("VK_RETURN"))
        ret = PR_TRUE;
        break;
    case VK_ENTER:
      if(aKeyName.EqualsIgnoreCase("VK_ENTER"))
        ret = PR_TRUE;
        break;
    case VK_SHIFT:
      if(aKeyName.EqualsIgnoreCase("VK_SHIFT"))
        ret = PR_TRUE;
        break;
    case VK_CONTROL:
      if(aKeyName.EqualsIgnoreCase("VK_CONTROL"))
        ret = PR_TRUE;
        break;
    case VK_ALT:
      if(aKeyName.EqualsIgnoreCase("VK_ALT"))
        ret = PR_TRUE;
        break;
    case VK_PAUSE:
      if(aKeyName.EqualsIgnoreCase("VK_PAUSE"))
        ret = PR_TRUE;
        break;
    case VK_CAPS_LOCK:
      if(aKeyName.EqualsIgnoreCase("VK_CAPS_LOCK"))
        ret = PR_TRUE;
        break;
    case VK_ESCAPE:
      if(aKeyName.EqualsIgnoreCase("VK_ESCAPE"))
        ret = PR_TRUE;
        break;
    case VK_SPACE:
      if(aKeyName.EqualsIgnoreCase("VK_SPACE"))
        ret = PR_TRUE;
        break;
    case VK_PAGE_UP:
      if(aKeyName.EqualsIgnoreCase("VK_PAGE_UP"))
        ret = PR_TRUE;
        break;
    case VK_PAGE_DOWN:
      if(aKeyName.EqualsIgnoreCase("VK_PAGE_DOWN"))
        ret = PR_TRUE;
        break;
    case VK_END:
      if(aKeyName.EqualsIgnoreCase("VK_END"))
        ret = PR_TRUE;
        break;
    case VK_HOME:
      if(aKeyName.EqualsIgnoreCase("VK_HOME"))
        ret = PR_TRUE;
        break;
    case VK_LEFT:
      if(aKeyName.EqualsIgnoreCase("VK_LEFT"))
        ret = PR_TRUE;
        break;
    case VK_UP:
      if(aKeyName.EqualsIgnoreCase("VK_UP"))
        ret = PR_TRUE;
        break;
    case VK_RIGHT:
      if(aKeyName.EqualsIgnoreCase("VK_RIGHT"))
        ret = PR_TRUE;
        break;
    case VK_DOWN:
      if(aKeyName.EqualsIgnoreCase("VK_DOWN"))
        ret = PR_TRUE;
        break;
    case VK_PRINTSCREEN:
      if(aKeyName.EqualsIgnoreCase("VK_PRINTSCREEN"))
        ret = PR_TRUE;
        break;
    case VK_INSERT:
      if(aKeyName.EqualsIgnoreCase("VK_INSERT"))
        ret = PR_TRUE;
        break;
    case VK_DELETE:
      if(aKeyName.EqualsIgnoreCase("VK_DELETE"))
        ret = PR_TRUE;
        break;
    case VK_0:
      if(aKeyName.EqualsIgnoreCase("VK_0"))
        ret = PR_TRUE;
        break;
    case VK_1:
      if(aKeyName.EqualsIgnoreCase("VK_1"))
        ret = PR_TRUE;
        break;
    case VK_2:
      if(aKeyName.EqualsIgnoreCase("VK_2"))
        ret = PR_TRUE;
        break;
    case VK_3:
      if(aKeyName.EqualsIgnoreCase("VK_3"))
        ret = PR_TRUE;
        break;
    case VK_4:
      if(aKeyName.EqualsIgnoreCase("VK_4"))
        ret = PR_TRUE;
        break;
    case VK_5:
      if(aKeyName.EqualsIgnoreCase("VK_5"))
        ret = PR_TRUE;
        break;
    case VK_6:
      if(aKeyName.EqualsIgnoreCase("VK_6"))
        ret = PR_TRUE;
        break;
    case VK_7:
      if(aKeyName.EqualsIgnoreCase("VK_7"))
        ret = PR_TRUE;
        break;
    case VK_8:
      if(aKeyName.EqualsIgnoreCase("VK_8"))
        ret = PR_TRUE;
        break;
    case VK_9:
      if(aKeyName.EqualsIgnoreCase("VK_9"))
        ret = PR_TRUE;
        break;
    case VK_SEMICOLON:
      if(aKeyName.EqualsIgnoreCase("VK_SEMICOLON"))
        ret = PR_TRUE;
        break;
    case VK_EQUALS:
      if(aKeyName.EqualsIgnoreCase("VK_EQUALS"))
        ret = PR_TRUE;
        break;
    case VK_A:
      if(aKeyName.EqualsIgnoreCase("VK_A"))
        ret = PR_TRUE;
        break;
    case VK_B:
      if(aKeyName.EqualsIgnoreCase("VK_B"))
        ret = PR_TRUE;
    break;
    case VK_C:
      if(aKeyName.EqualsIgnoreCase("VK_C"))
        ret = PR_TRUE;
        break;
    case VK_D:
      if(aKeyName.EqualsIgnoreCase("VK_D"))
        ret = PR_TRUE;
        break;
    case VK_E:
      if(aKeyName.EqualsIgnoreCase("VK_E"))
        ret = PR_TRUE;
        break;
    case VK_F:
      if(aKeyName.EqualsIgnoreCase("VK_F"))
        ret = PR_TRUE;
        break;
    case VK_G:
      if(aKeyName.EqualsIgnoreCase("VK_G"))
        ret = PR_TRUE;
        break;
    case VK_H:
      if(aKeyName.EqualsIgnoreCase("VK_H"))
        ret = PR_TRUE;
        break;
    case VK_I:
      if(aKeyName.EqualsIgnoreCase("VK_I"))
        ret = PR_TRUE;
        break;
    case VK_J:
      if(aKeyName.EqualsIgnoreCase("VK_J"))
        ret = PR_TRUE;
        break;
    case VK_K:
      if(aKeyName.EqualsIgnoreCase("VK_K"))
        ret = PR_TRUE;
        break;
    case VK_L:
      if(aKeyName.EqualsIgnoreCase("VK_L"))
        ret = PR_TRUE;
        break;
    case VK_M:
      if(aKeyName.EqualsIgnoreCase("VK_M"))
        ret = PR_TRUE;
        break;
    case VK_N:
      if(aKeyName.EqualsIgnoreCase("VK_N"))
        ret = PR_TRUE;
        break;
    case VK_O:
      if(aKeyName.EqualsIgnoreCase("VK_O"))
        ret = PR_TRUE;
        break;
    case VK_P:
      if(aKeyName.EqualsIgnoreCase("VK_P"))
        ret = PR_TRUE;
        break;
    case VK_Q:
      if(aKeyName.EqualsIgnoreCase("VK_Q"))
        ret = PR_TRUE;
        break;
    case VK_R:
      if(aKeyName.EqualsIgnoreCase("VK_R"))
        ret = PR_TRUE;
        break;
    case VK_S:
      if(aKeyName.EqualsIgnoreCase("VK_S"))
        ret = PR_TRUE;
        break;
    case VK_T:
      if(aKeyName.EqualsIgnoreCase("VK_T"))
        ret = PR_TRUE;
        break;
    case VK_U:
      if(aKeyName.EqualsIgnoreCase("VK_U"))
        ret = PR_TRUE;
        break;
    case VK_V:
      if(aKeyName.EqualsIgnoreCase("VK_V"))
        ret = PR_TRUE;
        break;
    case VK_W:
      if(aKeyName.EqualsIgnoreCase("VK_W"))
        ret = PR_TRUE;
        break;
    case VK_X:
      if(aKeyName.EqualsIgnoreCase("VK_X"))
        ret = PR_TRUE;
        break;
    case VK_Y:
      if(aKeyName.EqualsIgnoreCase("VK_Y"))
        ret = PR_TRUE;
        break;
    case VK_Z:
      if(aKeyName.EqualsIgnoreCase("VK_Z"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD0:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD0"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD1:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD1"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD2:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD2"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD3:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD3"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD4:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD4"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD5:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD5"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD6:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD6"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD7:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD7"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD8:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD8"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD9:
      if(aKeyName.EqualsIgnoreCase("VK_NUMPAD9"))
        ret = PR_TRUE;
        break;
    case VK_MULTIPLY:
      if(aKeyName.EqualsIgnoreCase("VK_MULTIPLY"))
        ret = PR_TRUE;
        break;
    case VK_ADD:
      if(aKeyName.EqualsIgnoreCase("VK_ADD"))
        ret = PR_TRUE;
        break;
    case VK_SEPARATOR:
      if(aKeyName.EqualsIgnoreCase("VK_SEPARATOR"))
        ret = PR_TRUE;
        break;
    case VK_SUBTRACT:
      if(aKeyName.EqualsIgnoreCase("VK_SUBTRACT"))
        ret = PR_TRUE;
        break;
    case VK_DECIMAL:
      if(aKeyName.EqualsIgnoreCase("VK_DECIMAL"))
        ret = PR_TRUE;
        break;
    case VK_DIVIDE:
      if(aKeyName.EqualsIgnoreCase("VK_DIVIDE"))
        ret = PR_TRUE;
        break;
    case VK_F1:
      if(aKeyName.EqualsIgnoreCase("VK_F1"))
        ret = PR_TRUE;
        break;
    case VK_F2:
      if(aKeyName.EqualsIgnoreCase("VK_F2"))
        ret = PR_TRUE;
        break;
    case VK_F3:
      if(aKeyName.EqualsIgnoreCase("VK_F3"))
        ret = PR_TRUE;
        break;
    case VK_F4:
      if(aKeyName.EqualsIgnoreCase("VK_F4"))
        ret = PR_TRUE;
        break;
    case VK_F5:
      if(aKeyName.EqualsIgnoreCase("VK_F5"))
        ret = PR_TRUE;
        break;
    case VK_F6:
      if(aKeyName.EqualsIgnoreCase("VK_F6"))
        ret = PR_TRUE;
        break;
    case VK_F7:
      if(aKeyName.EqualsIgnoreCase("VK_F7"))
        ret = PR_TRUE;
        break;
    case VK_F8:
      if(aKeyName.EqualsIgnoreCase("VK_F8"))
        ret = PR_TRUE;
        break;
    case VK_F9:
      if(aKeyName.EqualsIgnoreCase("VK_F9"))
        ret = PR_TRUE;
        break;
    case VK_F10:
      if(aKeyName.EqualsIgnoreCase("VK_F10"))
        ret = PR_TRUE;
        break;
    case VK_F11:
      if(aKeyName.EqualsIgnoreCase("VK_F11"))
        ret = PR_TRUE;
        break;
    case VK_F12:
      if(aKeyName.EqualsIgnoreCase("VK_F12"))
        ret = PR_TRUE;
        break;
    case VK_F13:
      if(aKeyName.EqualsIgnoreCase("VK_F13"))
        ret = PR_TRUE;
        break;
    case VK_F14:
      if(aKeyName.EqualsIgnoreCase("VK_F14"))
        ret = PR_TRUE;
        break;
    case VK_F15:
      if(aKeyName.EqualsIgnoreCase("VK_F15"))
        ret = PR_TRUE;
        break;
    case VK_F16:
      if(aKeyName.EqualsIgnoreCase("VK_F16"))
        ret = PR_TRUE;
        break;
    case VK_F17:
      if(aKeyName.EqualsIgnoreCase("VK_F17"))
        ret = PR_TRUE;
        break;
    case VK_F18:
      if(aKeyName.EqualsIgnoreCase("VK_F18"))
        ret = PR_TRUE;
        break;
    case VK_F19:
      if(aKeyName.EqualsIgnoreCase("VK_F19"))
        ret = PR_TRUE;
        break;
    case VK_F20:
      if(aKeyName.EqualsIgnoreCase("VK_F20"))
        ret = PR_TRUE;
        break;
    case VK_F21:
      if(aKeyName.EqualsIgnoreCase("VK_F21"))
        ret = PR_TRUE;
        break;
    case VK_F22:
      if(aKeyName.EqualsIgnoreCase("VK_F22"))
        ret = PR_TRUE;
        break;
    case VK_F23:
      if(aKeyName.EqualsIgnoreCase("VK_F23"))
        ret = PR_TRUE;
        break;
    case VK_F24:
      if(aKeyName.EqualsIgnoreCase("VK_F24"))
        ret = PR_TRUE;
        break;
    case VK_NUM_LOCK:
      if(aKeyName.EqualsIgnoreCase("VK_NUM_LOCK"))
        ret = PR_TRUE;
        break;
    case VK_SCROLL_LOCK:
      if(aKeyName.EqualsIgnoreCase("VK_SCROLL_LOCK"))
        ret = PR_TRUE;
        break;
    case VK_COMMA:
      if(aKeyName.EqualsIgnoreCase("VK_COMMA"))
        ret = PR_TRUE;
        break;
    case VK_PERIOD:
      if(aKeyName.EqualsIgnoreCase("VK_PERIOD"))
        ret = PR_TRUE;
        break;
    case VK_SLASH:
      if(aKeyName.EqualsIgnoreCase("VK_SLASH"))
        ret = PR_TRUE;
        break;
    case VK_BACK_QUOTE:
      if(aKeyName.EqualsIgnoreCase("VK_BACK_QUOTE"))
        ret = PR_TRUE;
        break;
    case VK_OPEN_BRACKET:
      if(aKeyName.EqualsIgnoreCase("VK_OPEN_BRACKET"))
        ret = PR_TRUE;
        break;
    case VK_BACK_SLASH:
      if(aKeyName.EqualsIgnoreCase("VK_BACK_SLASH"))
        ret = PR_TRUE;
        break;
    case VK_CLOSE_BRACKET:
      if(aKeyName.EqualsIgnoreCase("VK_CLOSE_BRACKET"))
        ret = PR_TRUE;
        break;
    case VK_QUOTE:
      if(aKeyName.EqualsIgnoreCase("VK_QUOTE"))
        ret = PR_TRUE;
        break;
  }

  return ret;
}

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

PRBool 
nsXBLEventHandler::IsMatchingCharCode(const PRUint32 aChar, const nsString& aKeyName)
{
  char tempChar[2];
  tempChar[0] = aChar;
  tempChar[1] = 0;
  nsAutoString tempChar2; tempChar2.AssignWithConversion(tempChar);
  
  return tempChar2.EqualsIgnoreCase(aKeyName);
}

void
nsXBLEventHandler::ConstructKeyMask()
{
  mKeyMask = 0;

  nsAutoString modifiers;
  mHandlerElement->GetAttribute(kNameSpaceID_None, kModifiersAtom, modifiers);
  if (modifiers.IsEmpty())
    return;

  char* str = modifiers.ToNewCString();
  char* newStr;
  char* token = nsCRT::strtok( str, ", ", &newStr );
  while( token != NULL ) {
    if (PL_strcmp(token, "shift") == 0)
      mKeyMask |= cShift;
    else if (PL_strcmp(token, "alt") == 0)
      mKeyMask |= cAlt;
    else if (PL_strcmp(token, "meta") == 0)
      mKeyMask |= cMeta;
    else if (PL_strcmp(token, "control") == 0)
      mKeyMask |= cControl;
    else if (PL_strcmp(token, "primary") == 0) {
      switch (kAccessKey)
      {
        case nsIDOMKeyEvent::DOM_VK_META:
          mKeyMask |= cMeta;
          break;

        case nsIDOMKeyEvent::DOM_VK_ALT:
          mKeyMask |= cAlt;
          break;

        case nsIDOMKeyEvent::DOM_VK_CONTROL:
        default:
          mKeyMask |= cControl;
      }
    }
    
    token = nsCRT::strtok( newStr, ", ", &newStr );
  }

  nsMemory::Free(str);
}

PRBool
nsXBLEventHandler::MatchesMask(nsIDOMUIEvent* aEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> key(do_QueryInterface(aEvent));
  nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aEvent));

  PRBool keyPresent;
  key ? key->GetMetaKey(&keyPresent) : mouse->GetMetaKey(&keyPresent);
  if (keyPresent != ((mKeyMask & cMeta) != 0))
    return PR_FALSE;

  key ? key->GetShiftKey(&keyPresent) : mouse->GetShiftKey(&keyPresent);
  if (keyPresent != ((mKeyMask & cShift) != 0))
    return PR_FALSE;
  
  key ? key->GetAltKey(&keyPresent) : mouse->GetAltKey(&keyPresent);
  if (keyPresent != ((mKeyMask & cAlt) != 0))
    return PR_FALSE;

  key ? key->GetCtrlKey(&keyPresent) : mouse->GetCtrlKey(&keyPresent);
  if (keyPresent != ((mKeyMask & cControl) != 0))
    return PR_FALSE;

  return PR_TRUE;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLEventHandler(nsIDOMEventReceiver* aRec, nsIContent* aHandlerElement, 
                      const nsString& aEventName,
                      nsXBLEventHandler** aResult)
{
  *aResult = new nsXBLEventHandler(aRec, aHandlerElement, aEventName);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
