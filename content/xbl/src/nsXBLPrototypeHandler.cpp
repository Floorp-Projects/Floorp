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
#include "nsXBLPrototypeHandler.h"
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
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

PRUint32 nsXBLPrototypeHandler::gRefCnt = 0;
nsIAtom* nsXBLPrototypeHandler::kKeyCodeAtom = nsnull;
nsIAtom* nsXBLPrototypeHandler::kCharCodeAtom = nsnull;
nsIAtom* nsXBLPrototypeHandler::kKeyAtom = nsnull;
nsIAtom* nsXBLPrototypeHandler::kActionAtom = nsnull;
nsIAtom* nsXBLPrototypeHandler::kCommandAtom = nsnull;
nsIAtom* nsXBLPrototypeHandler::kClickCountAtom = nsnull;
nsIAtom* nsXBLPrototypeHandler::kButtonAtom = nsnull;
nsIAtom* nsXBLPrototypeHandler::kModifiersAtom = nsnull;

PRInt32 nsXBLPrototypeHandler::kAccessKey = -1;

const PRInt32 nsXBLPrototypeHandler::cShift = (1<<1);
const PRInt32 nsXBLPrototypeHandler::cAlt = (1<<2);
const PRInt32 nsXBLPrototypeHandler::cControl = (1<<3);
const PRInt32 nsXBLPrototypeHandler::cMeta = (1<<4);

nsXBLPrototypeHandler::nsXBLPrototypeHandler(nsIContent* aHandlerElement)
{
  NS_INIT_REFCNT();
  mHandlerElement = aHandlerElement;
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

    // Get the primary accelerator key.
    InitAccessKey();
  }

  // Make sure our mask is initialized.
  ConstructMask();
}

nsXBLPrototypeHandler::~nsXBLPrototypeHandler()
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
  }
}

NS_IMPL_ISUPPORTS1(nsXBLPrototypeHandler, nsIXBLPrototypeHandler)

NS_IMETHODIMP
nsXBLPrototypeHandler::EventMatched(nsIDOMEvent* aEvent, PRBool* aResult)
{
  nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aEvent));
  if (mouse)
    *aResult = MouseEventMatched(mouse);
  else {
    nsCOMPtr<nsIDOMKeyEvent> key(do_QueryInterface(aEvent));
    if (key)
      *aResult = KeyEventMatched(key);
    else *aResult = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeHandler::GetHandlerElement(nsIContent** aResult)
{
  *aResult = mHandlerElement;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeHandler::GetNextHandler(nsIXBLPrototypeHandler** aResult)
{
  *aResult = mNextHandler;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}
  
NS_IMETHODIMP
nsXBLPrototypeHandler::SetNextHandler(nsIXBLPrototypeHandler* aHandler)
{
  mNextHandler = aHandler;
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Get the menu access key from prefs.
// XXX Eventually pick up using CSS3 key-equivalent property or somesuch
void
nsXBLPrototypeHandler::InitAccessKey()
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
nsXBLPrototypeHandler::KeyEventMatched(nsIDOMKeyEvent* aKeyEvent)
{
  if (!mHandlerElement)
    return PR_FALSE;

  if (mDetail == 0 && mDetail2 == 0 && mKeyMask == 0)
    return PR_TRUE; // No filters set up. It's generic.

  // Get the keycode and charcode of the key event.
  PRUint32 keyCode, charCode;
  aKeyEvent->GetKeyCode(&keyCode);
  aKeyEvent->GetCharCode(&charCode);

  PRBool keyMatched = (mDetail == (mDetail2 ? charCode : keyCode));

  if (!keyMatched)
    return PR_FALSE;

  // Now check modifier keys
  return ModifiersMatchMask(aKeyEvent);
}

PRBool 
nsXBLPrototypeHandler::MouseEventMatched(nsIDOMMouseEvent* aMouseEvent)
{
  if (!mHandlerElement)
    return PR_FALSE;

  if (mDetail == 0 && mDetail2 == 0 && mKeyMask == 0)
    return PR_TRUE; // No filters set up. It's generic.

  unsigned short button;
  aMouseEvent->GetButton(&button);
  if (mDetail != 0 && (button != mDetail))
    return PR_FALSE;

  PRInt32 clickcount;
  aMouseEvent->GetDetail(&clickcount);
  if (mDetail2 != 0 && (clickcount != mDetail2))
    return PR_FALSE;
  
  return ModifiersMatchMask(aMouseEvent);
}

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

PRUint32 nsXBLPrototypeHandler::GetMatchingKeyCode(const nsString& aKeyName)
{
  PRBool ret = PR_FALSE;

  nsCAutoString keyName; keyName.AssignWithConversion(aKeyName);

  if (keyName.EqualsIgnoreCase("VK_CANCEL"))
    return VK_CANCEL;
  
  if(keyName.EqualsIgnoreCase("VK_BACK"))
    return VK_BACK;

  if(keyName.EqualsIgnoreCase("VK_TAB"))
    return VK_TAB;
  
  if(keyName.EqualsIgnoreCase("VK_CLEAR"))
    return VK_CLEAR;

  if(keyName.EqualsIgnoreCase("VK_RETURN"))
    return VK_RETURN;

  if(keyName.EqualsIgnoreCase("VK_ENTER"))
    return VK_ENTER;

  if(keyName.EqualsIgnoreCase("VK_SHIFT"))
    return VK_SHIFT;

  if(keyName.EqualsIgnoreCase("VK_CONTROL"))
    return VK_CONTROL;

  if(keyName.EqualsIgnoreCase("VK_ALT"))
    return VK_ALT;

  if(keyName.EqualsIgnoreCase("VK_PAUSE"))
    return VK_PAUSE;

  if(keyName.EqualsIgnoreCase("VK_CAPS_LOCK"))
    return VK_CAPS_LOCK;

  if(keyName.EqualsIgnoreCase("VK_ESCAPE"))
    return VK_ESCAPE;

   
  if(keyName.EqualsIgnoreCase("VK_SPACE"))
    return VK_SPACE;

  if(keyName.EqualsIgnoreCase("VK_PAGE_UP"))
    return VK_PAGE_UP;

  if(keyName.EqualsIgnoreCase("VK_PAGE_DOWN"))
    return VK_PAGE_DOWN;

  if(keyName.EqualsIgnoreCase("VK_END"))
    return VK_END;

  if(keyName.EqualsIgnoreCase("VK_HOME"))
    return VK_HOME;

  if(keyName.EqualsIgnoreCase("VK_LEFT"))
    return VK_LEFT;

  if(keyName.EqualsIgnoreCase("VK_UP"))
    return VK_UP;

  if(keyName.EqualsIgnoreCase("VK_RIGHT"))
    return VK_RIGHT;

  if(keyName.EqualsIgnoreCase("VK_DOWN"))
    return VK_DOWN;

  if(keyName.EqualsIgnoreCase("VK_PRINTSCREEN"))
    return VK_PRINTSCREEN;

  if(keyName.EqualsIgnoreCase("VK_INSERT"))
    return VK_INSERT;

  if(keyName.EqualsIgnoreCase("VK_DELETE"))
    return VK_DELETE;

  if(keyName.EqualsIgnoreCase("VK_0"))
    return VK_0;

  if(keyName.EqualsIgnoreCase("VK_1"))
    return VK_1;

  if(keyName.EqualsIgnoreCase("VK_2"))
    return VK_2;

  if(keyName.EqualsIgnoreCase("VK_3"))
    return VK_3;

  if(keyName.EqualsIgnoreCase("VK_4"))
    return VK_4;

  if(keyName.EqualsIgnoreCase("VK_5"))
    return VK_5;

  if(keyName.EqualsIgnoreCase("VK_6"))
    return VK_6;

  if(keyName.EqualsIgnoreCase("VK_7"))
    return VK_7;

  if(keyName.EqualsIgnoreCase("VK_8"))
    return VK_8;

  if(keyName.EqualsIgnoreCase("VK_9"))
    return VK_9;

  if(keyName.EqualsIgnoreCase("VK_SEMICOLON"))
    return VK_SEMICOLON;

  if(keyName.EqualsIgnoreCase("VK_EQUALS"))
    return VK_EQUALS;
  if(keyName.EqualsIgnoreCase("VK_A"))
    return VK_A;
  if(keyName.EqualsIgnoreCase("VK_B"))
    return VK_B;
  if(keyName.EqualsIgnoreCase("VK_C"))
    return VK_C;
  if(keyName.EqualsIgnoreCase("VK_D"))
    return VK_D;
  if(keyName.EqualsIgnoreCase("VK_E"))
    return VK_E;
  if(keyName.EqualsIgnoreCase("VK_F"))
    return VK_F;
  if(keyName.EqualsIgnoreCase("VK_G"))
    return VK_G;
  if(keyName.EqualsIgnoreCase("VK_H"))
    return VK_H;
  if(keyName.EqualsIgnoreCase("VK_I"))
    return VK_I;
  if(keyName.EqualsIgnoreCase("VK_J"))
    return VK_J;
  if(keyName.EqualsIgnoreCase("VK_K"))
    return VK_K;
  if(keyName.EqualsIgnoreCase("VK_L"))
    return VK_L;
  if(keyName.EqualsIgnoreCase("VK_M"))
    return VK_M;
  if(keyName.EqualsIgnoreCase("VK_N"))
    return VK_N;
  if(keyName.EqualsIgnoreCase("VK_O"))
    return VK_O;
  if(keyName.EqualsIgnoreCase("VK_P"))
    return VK_P;
  if(keyName.EqualsIgnoreCase("VK_Q"))
    return VK_Q;
  if(keyName.EqualsIgnoreCase("VK_R"))
    return VK_R;
  if(keyName.EqualsIgnoreCase("VK_S"))
    return VK_S;
  if(keyName.EqualsIgnoreCase("VK_T"))
    return VK_T;
  if(keyName.EqualsIgnoreCase("VK_U"))
    return VK_U;
  if(keyName.EqualsIgnoreCase("VK_V"))
    return VK_V;
  if(keyName.EqualsIgnoreCase("VK_W"))
    return VK_W;
  if(keyName.EqualsIgnoreCase("VK_X"))
    return VK_X;
  if(keyName.EqualsIgnoreCase("VK_Y"))
    return VK_Y;
  if(keyName.EqualsIgnoreCase("VK_Z"))
    return VK_Z;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD0"))
    return VK_NUMPAD0;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD1"))
    return VK_NUMPAD1;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD2"))
    return VK_NUMPAD2;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD3"))
    return VK_NUMPAD3;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD4"))
    return VK_NUMPAD4;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD5"))
    return VK_NUMPAD5;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD6"))
    return VK_NUMPAD6;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD7"))
    return VK_NUMPAD7;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD8"))
    return VK_NUMPAD8;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD9"))
    return VK_NUMPAD9;
  if(keyName.EqualsIgnoreCase("VK_MULTIPLY"))
    return VK_MULTIPLY;
  if(keyName.EqualsIgnoreCase("VK_ADD"))
    return VK_ADD;
  if(keyName.EqualsIgnoreCase("VK_SEPARATOR"))
    return VK_SEPARATOR;
  if(keyName.EqualsIgnoreCase("VK_SUBTRACT"))
    return VK_SUBTRACT;
  if(keyName.EqualsIgnoreCase("VK_DECIMAL"))
    return VK_DECIMAL;
  if(keyName.EqualsIgnoreCase("VK_DIVIDE"))
    return VK_DIVIDE;
  if(keyName.EqualsIgnoreCase("VK_F1"))
    return VK_F1;
  if(keyName.EqualsIgnoreCase("VK_F2"))
    return VK_F2;
  if(keyName.EqualsIgnoreCase("VK_F3"))
    return VK_F3;
  if(keyName.EqualsIgnoreCase("VK_F4"))
    return VK_F4;
  if(keyName.EqualsIgnoreCase("VK_F5"))
    return VK_F5;
  if(keyName.EqualsIgnoreCase("VK_F6"))
    return VK_F6;
  if(keyName.EqualsIgnoreCase("VK_F7"))
    return VK_F7;
  if(keyName.EqualsIgnoreCase("VK_F8"))
    return VK_F8;
  if(keyName.EqualsIgnoreCase("VK_F9"))
    return VK_F9;
  if(keyName.EqualsIgnoreCase("VK_F10"))
    return VK_F10;
  if(keyName.EqualsIgnoreCase("VK_F11"))
    return VK_F11;
  if(keyName.EqualsIgnoreCase("VK_F12"))
    return VK_F12;
  if(keyName.EqualsIgnoreCase("VK_F13"))
    return VK_F13;
  if(keyName.EqualsIgnoreCase("VK_F14"))
    return VK_F14;
  if(keyName.EqualsIgnoreCase("VK_F15"))
    return VK_F15;
  if(keyName.EqualsIgnoreCase("VK_F16"))
    return VK_F16;
  if(keyName.EqualsIgnoreCase("VK_F17"))
    return VK_F17;
  if(keyName.EqualsIgnoreCase("VK_F18"))
    return VK_F18;
  if(keyName.EqualsIgnoreCase("VK_F19"))
    return VK_F19;
  if(keyName.EqualsIgnoreCase("VK_F20"))
    return VK_F20;
  if(keyName.EqualsIgnoreCase("VK_F21"))
    return VK_F21;
  if(keyName.EqualsIgnoreCase("VK_F22"))
    return VK_F22;
  if(keyName.EqualsIgnoreCase("VK_F23"))
    return VK_F23;
  if(keyName.EqualsIgnoreCase("VK_F24"))
    return VK_F24;
  if(keyName.EqualsIgnoreCase("VK_NUM_LOCK"))
    return VK_NUM_LOCK;
  if(keyName.EqualsIgnoreCase("VK_SCROLL_LOCK"))
    return VK_SCROLL_LOCK;
  if(keyName.EqualsIgnoreCase("VK_COMMA"))
    return VK_COMMA;
  if(keyName.EqualsIgnoreCase("VK_PERIOD"))
    return VK_PERIOD;
  if(keyName.EqualsIgnoreCase("VK_SLASH"))
    return VK_SLASH;
  if(keyName.EqualsIgnoreCase("VK_BACK_QUOTE"))
    return VK_BACK_QUOTE;
  if(keyName.EqualsIgnoreCase("VK_OPEN_BRACKET"))
    return VK_OPEN_BRACKET;
  if(keyName.EqualsIgnoreCase("VK_BACK_SLASH"))
    return VK_BACK_SLASH;
  if(keyName.EqualsIgnoreCase("VK_CLOSE_BRACKET"))
    return VK_CLOSE_BRACKET;
  if(keyName.EqualsIgnoreCase("VK_QUOTE"))
    return VK_QUOTE;


  return 0;
}

void
nsXBLPrototypeHandler::ConstructMask()
{
  mDetail = 0;
  mDetail2 = 0;
  mKeyMask = 0;

  nsAutoString key;
  mHandlerElement->GetAttribute(kNameSpaceID_None, kKeyAtom, key);
  if (key.IsEmpty()) 
    mHandlerElement->GetAttribute(kNameSpaceID_None, kCharCodeAtom, key);
  
  if (!key.IsEmpty()) {
    // We have a charcode.
    mDetail2 = 1;
    mDetail = key[0];
  }
  else {
    mHandlerElement->GetAttribute(kNameSpaceID_None, kKeyCodeAtom, key);
    if (!key.IsEmpty())
      mDetail = GetMatchingKeyCode(key);
  }

  nsAutoString buttonStr, clickCountStr;
  mHandlerElement->GetAttribute(kNameSpaceID_None, kClickCountAtom, clickCountStr);
  mHandlerElement->GetAttribute(kNameSpaceID_None, kButtonAtom, buttonStr);

  if (!buttonStr.IsEmpty()) {
    PRInt32 error;
    mDetail = buttonStr.ToInteger(&error);
  }

  if (!clickCountStr.IsEmpty()) {
    PRInt32 error;
    mDetail2 = clickCountStr.ToInteger(&error);
  }

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
nsXBLPrototypeHandler::ModifiersMatchMask(nsIDOMUIEvent* aEvent)
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
NS_NewXBLPrototypeHandler(nsIContent* aHandlerElement, nsIXBLPrototypeHandler** aResult)
{
  *aResult = new nsXBLPrototypeHandler(aHandlerElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
