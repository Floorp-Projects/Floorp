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
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsXBLEventHandler.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDOMKeyEvent.h"
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

PRUint32 nsXBLEventHandler::gRefCnt = 0;
nsIAtom* nsXBLEventHandler::kKeyCodeAtom = nsnull;
nsIAtom* nsXBLEventHandler::kCharCodeAtom = nsnull;
nsIAtom* nsXBLEventHandler::kKeyAtom = nsnull;
nsIAtom* nsXBLEventHandler::kPrimaryAtom = nsnull;
nsIAtom* nsXBLEventHandler::kShiftAtom = nsnull;
nsIAtom* nsXBLEventHandler::kControlAtom = nsnull;
nsIAtom* nsXBLEventHandler::kMetaAtom = nsnull;
nsIAtom* nsXBLEventHandler::kAltAtom = nsnull;
nsIAtom* nsXBLEventHandler::kValueAtom = nsnull;
nsIAtom* nsXBLEventHandler::kCommandAtom = nsnull;

nsXBLEventHandler::nsXBLEventHandler(nsIContent* aBoundElement, nsIContent* aHandlerElement,
                                     const nsString& aEventName)
{
  NS_INIT_REFCNT();
  mBoundElement = aBoundElement;
  mHandlerElement = aHandlerElement;
  mEventName = aEventName;
  gRefCnt++;
  if (gRefCnt == 1) {
    kKeyCodeAtom = NS_NewAtom("keycode");
    kKeyAtom = NS_NewAtom("key");
    kCharCodeAtom = NS_NewAtom("charcode");
    kPrimaryAtom = NS_NewAtom("primary");
    kShiftAtom = NS_NewAtom("shift");
    kControlAtom = NS_NewAtom("control");
    kAltAtom = NS_NewAtom("alt");
    kMetaAtom = NS_NewAtom("meta");
    kValueAtom = NS_NewAtom("value");
    kCommandAtom = NS_NewAtom("command");
  }
}

nsXBLEventHandler::~nsXBLEventHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kKeyAtom);
    NS_RELEASE(kKeyCodeAtom);
    NS_RELEASE(kCharCodeAtom);
    NS_RELEASE(kPrimaryAtom);
    NS_RELEASE(kShiftAtom);
    NS_RELEASE(kControlAtom);
    NS_RELEASE(kAltAtom);
    NS_RELEASE(kMetaAtom);
    NS_RELEASE(kValueAtom);
    NS_RELEASE(kCommandAtom);
  }
}

NS_IMPL_ISUPPORTS3(nsXBLEventHandler, nsIDOMKeyListener, nsIDOMMouseListener, nsIDOMMenuListener)

nsresult nsXBLEventHandler::HandleEvent(nsIDOMEvent* aEvent)
{
  // Nothing to do.
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyUp(nsIDOMEvent* aKeyEvent)
{
  if (!mEventName.Equals("keyup"))
    return NS_OK;

  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (KeyEventMatched(keyEvent))
    ExecuteHandler(nsAutoString("keyup"), aKeyEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyDown(nsIDOMEvent* aKeyEvent)
{
  if (!mEventName.Equals("keydown"))
    return NS_OK;

  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (KeyEventMatched(keyEvent))
    ExecuteHandler(nsAutoString("keydown"), aKeyEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::KeyPress(nsIDOMEvent* aKeyEvent)
{
  if (!mEventName.Equals("keypress"))
    return NS_OK;

  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (KeyEventMatched(keyEvent))
    ExecuteHandler(nsAutoString("keypress"), aKeyEvent);
  return NS_OK;
}
   
nsresult nsXBLEventHandler::MouseDown(nsIDOMEvent* aMouseEvent)
{
  if (!mEventName.Equals("mousedown"))
    return NS_OK;

  nsCOMPtr<nsIDOMUIEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler(nsAutoString("mousedown"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseUp(nsIDOMEvent* aMouseEvent)
{
  if (!mEventName.Equals("mouseup"))
    return NS_OK;

  nsCOMPtr<nsIDOMUIEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler(nsAutoString("mouseup"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseClick(nsIDOMEvent* aMouseEvent)
{
  if (!mEventName.Equals("click"))
    return NS_OK;

  nsCOMPtr<nsIDOMUIEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler(nsAutoString("click"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  if (!mEventName.Equals("dblclick"))
    return NS_OK;

  nsCOMPtr<nsIDOMUIEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler(nsAutoString("dblclick"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseOver(nsIDOMEvent* aMouseEvent)
{
  if (!mEventName.Equals("mouseover"))
    return NS_OK;

  nsCOMPtr<nsIDOMUIEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler(nsAutoString("mouseover"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::MouseOut(nsIDOMEvent* aMouseEvent)
{
  if (!mEventName.Equals("mouseout"))
    return NS_OK;

  nsCOMPtr<nsIDOMUIEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (MouseEventMatched(mouseEvent))
    ExecuteHandler(nsAutoString("mouseout"), aMouseEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Action(nsIDOMEvent* aEvent)
{
  if (!mEventName.Equals("command"))
    return NS_OK;

  ExecuteHandler(nsAutoString("command"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Create(nsIDOMEvent* aEvent)
{
  if (!mEventName.Equals("create"))
    return NS_OK;

  ExecuteHandler(nsAutoString("create"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Close(nsIDOMEvent* aEvent)
{
  if (!mEventName.Equals("close"))
    return NS_OK;

  ExecuteHandler(nsAutoString("close"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Broadcast(nsIDOMEvent* aEvent)
{
  if (!mEventName.Equals("broadcast"))
    return NS_OK;

  ExecuteHandler(nsAutoString("broadcast"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::CommandUpdate(nsIDOMEvent* aEvent)
{
  if (!mEventName.Equals("commandupdate"))
    return NS_OK;

  ExecuteHandler(nsAutoString("commandupdate"), aEvent);
  return NS_OK;
}

nsresult nsXBLEventHandler::Destroy(nsIDOMEvent* aEvent)
{
  if (!mEventName.Equals("destroy"))
    return NS_OK;

  ExecuteHandler(nsAutoString("destroy"), aEvent);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

PRBool 
nsXBLEventHandler::KeyEventMatched(nsIDOMKeyEvent* aKeyEvent)
{
  nsAutoString trueString = "true";
  nsAutoString falseString = "false";

  // Get the keycode and charcode of the key event.
  PRUint32 keyCode, charCode;
  aKeyEvent->GetKeyCode(&keyCode);
  aKeyEvent->GetCharCode(&charCode);

  PRBool keyMatched = PR_FALSE;

  nsAutoString key;
  mHandlerElement->GetAttribute(kNameSpaceID_None, kKeyAtom, key);
  if (!key.IsEmpty())
    keyMatched = IsMatchingCharCode(charCode, key);
  
  key = "";
  mHandlerElement->GetAttribute(kNameSpaceID_None, kKeyCodeAtom, key);
  if (!key.IsEmpty())
    keyMatched = IsMatchingKeyCode(keyCode, key);
  
  key = "";
  mHandlerElement->GetAttribute(kNameSpaceID_None, kCharCodeAtom, key);
  if (!key.IsEmpty())
    keyMatched = IsMatchingCharCode(charCode, key);
  
  if (!keyMatched)
    return PR_FALSE;

  // Now check modifier keys
  nsAutoString modifier;
  PRBool isModifierPresent;
  mHandlerElement->GetAttribute(kNameSpaceID_None, kPrimaryAtom, modifier);
  if (modifier == trueString) {
    // The XUL key must be set.  Hard code for now.
    // XXX Eventually pick up using CSS3 key-equivalent property or somesuch
#ifdef XP_MAC
    aKeyEvent->GetMetaKey(&isModifierPresent);
#elif XP_UNIX
    aKeyEvent->GetAltKey(&isModifierPresent);
#else
    aKeyEvent->GetCtrlKey(&isModifierPresent);
#endif

    if (!isModifierPresent)
      return PR_FALSE;
  }
  else if (modifier == falseString) {
#ifdef XP_MAC
    aKeyEvent->GetMetaKey(&isModifierPresent);
#elif XP_UNIX
    aKeyEvent->GetAltKey(&isModifierPresent);
#else
    aKeyEvent->GetCtrlKey(&isModifierPresent);
#endif

    if (isModifierPresent)
      return PR_FALSE;
  }

  // Check for shift.
  mHandlerElement->GetAttribute(kNameSpaceID_None, kShiftAtom, modifier);
  if (modifier == trueString) {
    aKeyEvent->GetShiftKey(&isModifierPresent);
    if (!isModifierPresent)
      return PR_FALSE;
  }
  else if (modifier == falseString) {
    aKeyEvent->GetShiftKey(&isModifierPresent);
    if (isModifierPresent)
      return PR_FALSE;
  }

  // Check for ctrl.
  mHandlerElement->GetAttribute(kNameSpaceID_None, kControlAtom, modifier);
  if (modifier == trueString) {
    aKeyEvent->GetCtrlKey(&isModifierPresent);
    if (!isModifierPresent)
      return PR_FALSE;
  }
  else if (modifier == falseString) {
    aKeyEvent->GetCtrlKey(&isModifierPresent);
    if (isModifierPresent)
      return PR_FALSE;
  }

  // Check for meta.
  mHandlerElement->GetAttribute(kNameSpaceID_None, kMetaAtom, modifier);
  if (modifier == trueString) {
    aKeyEvent->GetMetaKey(&isModifierPresent);
    if (!isModifierPresent)
      return PR_FALSE;
  }
  else if (modifier == falseString) {
    aKeyEvent->GetMetaKey(&isModifierPresent);
    if (isModifierPresent)
      return PR_FALSE;
  }

  // Check for alt.
  mHandlerElement->GetAttribute(kNameSpaceID_None, kAltAtom, modifier);
  if (modifier == trueString) {
    aKeyEvent->GetAltKey(&isModifierPresent);
    if (!isModifierPresent)
      return PR_FALSE;
  }
  else if (modifier == falseString) {
    aKeyEvent->GetAltKey(&isModifierPresent);
    if (isModifierPresent)
      return PR_FALSE;
  }

  return PR_TRUE;
}

PRBool 
nsXBLEventHandler::MouseEventMatched(nsIDOMUIEvent* aMouseEvent)
{
  nsAutoString trueString = "true";
  nsAutoString falseString = "false";

  // XXX Check for button and modifier keys.

  return PR_TRUE;
}

NS_IMETHODIMP
nsXBLEventHandler::ExecuteHandler(const nsString& aEventName, nsIDOMEvent* aEvent)
{
  // This is a special-case optimization to make command handling fast.
  // It isn't really a part of XBL, but it helps speed things up.
  nsAutoString command;
  mHandlerElement->GetAttribute(kNameSpaceID_None, kCommandAtom, command);
  if (!command.IsEmpty()) {
    // We are the default action for this command.
    // Stop any other default action from executing.
    aEvent->PreventDefault();

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
  nsAutoString onEvent = "onxbl";
  onEvent += aEventName;
  nsCOMPtr<nsIAtom> onEventAtom = getter_AddRefs(NS_NewAtom(onEvent));

  void* handler = nsnull;
  
  // Compile the event handler.
  nsAutoString handlerText;
  mHandlerElement->GetAttribute(kNameSpaceID_None, kValueAtom, handlerText);
  if (handlerText.IsEmpty())
    return NS_OK; // For whatever reason, they didn't give us anything to do.

  
  // Compile the handler and bind it to the element.
  nsCOMPtr<nsIDocument> boundDocument;
  mBoundElement->GetDocument(*getter_AddRefs(boundDocument));
  nsCOMPtr<nsIScriptGlobalObject> boundGlobal;
  boundDocument->GetScriptGlobalObject(getter_AddRefs(boundGlobal));

  nsCOMPtr<nsIScriptContext> boundContext;
  boundGlobal->GetContext(getter_AddRefs(boundContext));

  nsCOMPtr<nsIScriptObjectOwner> owner = do_QueryInterface(mBoundElement);
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

  nsCOMPtr<nsIDOMXULElement> xulElement = do_QueryInterface(mBoundElement);
  if (xulElement)
    xulElement->GetControllers(getter_AddRefs(controllers));
  else {
    nsCOMPtr<nsIDOMNSHTMLTextAreaElement> htmlTextArea = do_QueryInterface(mBoundElement);
    if (htmlTextArea)
      htmlTextArea->GetControllers(getter_AddRefs(controllers));
    else {
      nsCOMPtr<nsIDOMNSHTMLInputElement> htmlInputElement = do_QueryInterface(mBoundElement);
      if (htmlInputElement)
        htmlInputElement->GetControllers(getter_AddRefs(controllers));
    }
  }

  // Return the first controller.
  // XXX Just ditch this stupid controllers array.  It was a bad idea.
  if (controllers) {
    controllers->GetControllerAt(0, aResult);
  }
  else *aResult = nsnull;

  return NS_OK;
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

PRBool 
nsXBLEventHandler::IsMatchingCharCode(const PRUint32 aChar, const nsString& aKeyName)
{
  char tempChar[2];
  tempChar[0] = aChar;
  tempChar[1] = 0;
  nsAutoString tempChar2 = tempChar;
  
  return tempChar2.EqualsIgnoreCase(aKeyName);
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLEventHandler(nsIContent* aBoundElement, nsIContent* aHandlerElement, 
                      const nsString& aEventName,
                      nsXBLEventHandler** aResult)
{
  *aResult = new nsXBLEventHandler(aBoundElement, aHandlerElement, aEventName);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
