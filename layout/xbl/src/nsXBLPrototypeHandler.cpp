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

PRInt32 nsXBLPrototypeHandler::kMenuAccessKey = -1;
PRInt32 nsXBLPrototypeHandler::kAccelKey = -1;

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
    InitAccessKeys();
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
nsXBLPrototypeHandler::InitAccessKeys()
{
  if (kAccelKey >= 0 && kMenuAccessKey >= 0)
    return;

  // Compiled-in defaults, in case we can't get the pref --
  // mac doesn't have menu shortcuts, other platforms use alt.
#ifdef XP_MAC
  kMenuAccessKey = 0;
  kAccelKey = nsIDOMKeyEvent::DOM_VK_META;
#else
  kMenuAccessKey = nsIDOMKeyEvent::DOM_VK_ALT;
  kAccelKey = nsIDOMKeyEvent::DOM_VK_CONTROL;
#endif

  // Get the menu access key value from prefs, overriding the default:
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefs)
  {
    prefs->GetIntPref("ui.key.menuAccessKey", &kMenuAccessKey);
    prefs->GetIntPref("ui.key.accelKey", &kAccelKey);
  }
}


NS_IMETHODIMP
nsXBLPrototypeHandler::KeyEventMatched(nsIDOMKeyEvent* aKeyEvent, PRBool* aResult)
{
  *aResult = PR_TRUE;

  if (!mHandlerElement) {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  if (mDetail == 0 && mDetail2 == 0 && mKeyMask == 0)
    return NS_OK; // No filters set up. It's generic.

  // Get the keycode and charcode of the key event.
  PRUint32 keyCode, charCode;
  aKeyEvent->GetKeyCode(&keyCode);
  aKeyEvent->GetCharCode(&charCode);

  PRBool keyMatched = (mDetail == (mDetail2 ? charCode : keyCode));

  if (!keyMatched) {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  // Now check modifier keys
  PRBool result = ModifiersMatchMask(aKeyEvent);
  *aResult = result;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeHandler::MouseEventMatched(nsIDOMMouseEvent* aMouseEvent, PRBool* aResult)
{
  *aResult = PR_TRUE;

  if (!mHandlerElement) {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  if (mDetail == 0 && mDetail2 == 0 && mKeyMask == 0)
    return NS_OK; // No filters set up. It's generic.

  unsigned short button;
  aMouseEvent->GetButton(&button);
  if (mDetail != 0 && (button != mDetail)) {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  PRInt32 clickcount;
  aMouseEvent->GetDetail(&clickcount);
  if (mDetail2 != 0 && (clickcount != mDetail2)) {
    *aResult = PR_FALSE;
    return NS_OK;
  }
  
  PRBool result = ModifiersMatchMask(aMouseEvent);
  *aResult = result;
  return NS_OK;
}

PRUint32 nsXBLPrototypeHandler::GetMatchingKeyCode(const nsString& aKeyName)
{
  nsCAutoString keyName; keyName.AssignWithConversion(aKeyName);

  // XXX: be sure to check this periodically for new symbol additions!
  if (keyName.EqualsIgnoreCase("VK_CANCEL"))
    return nsIDOMKeyEvent::DOM_VK_CANCEL;
  
  if(keyName.EqualsIgnoreCase("VK_BACK"))
    return nsIDOMKeyEvent::DOM_VK_BACK_SPACE;

  if(keyName.EqualsIgnoreCase("VK_TAB"))
    return nsIDOMKeyEvent::DOM_VK_TAB;
  
  if(keyName.EqualsIgnoreCase("VK_CLEAR"))
    return nsIDOMKeyEvent::DOM_VK_CLEAR;

  if(keyName.EqualsIgnoreCase("VK_RETURN"))
    return nsIDOMKeyEvent::DOM_VK_RETURN;

  if(keyName.EqualsIgnoreCase("VK_ENTER"))
    return nsIDOMKeyEvent::DOM_VK_ENTER;

  if(keyName.EqualsIgnoreCase("VK_SHIFT"))
    return nsIDOMKeyEvent::DOM_VK_SHIFT;

  if(keyName.EqualsIgnoreCase("VK_CONTROL"))
    return nsIDOMKeyEvent::DOM_VK_CONTROL;

  if(keyName.EqualsIgnoreCase("VK_ALT"))
    return nsIDOMKeyEvent::DOM_VK_ALT;

  if(keyName.EqualsIgnoreCase("VK_PAUSE"))
    return nsIDOMKeyEvent::DOM_VK_PAUSE;

  if(keyName.EqualsIgnoreCase("VK_CAPS_LOCK"))
    return nsIDOMKeyEvent::DOM_VK_CAPS_LOCK;

  if(keyName.EqualsIgnoreCase("VK_ESCAPE"))
    return nsIDOMKeyEvent::DOM_VK_ESCAPE;

   
  if(keyName.EqualsIgnoreCase("VK_SPACE"))
    return nsIDOMKeyEvent::DOM_VK_SPACE;

  if(keyName.EqualsIgnoreCase("VK_PAGE_UP"))
    return nsIDOMKeyEvent::DOM_VK_PAGE_UP;

  if(keyName.EqualsIgnoreCase("VK_PAGE_DOWN"))
    return nsIDOMKeyEvent::DOM_VK_PAGE_DOWN;

  if(keyName.EqualsIgnoreCase("VK_END"))
    return nsIDOMKeyEvent::DOM_VK_END;

  if(keyName.EqualsIgnoreCase("VK_HOME"))
    return nsIDOMKeyEvent::DOM_VK_HOME;

  if(keyName.EqualsIgnoreCase("VK_LEFT"))
    return nsIDOMKeyEvent::DOM_VK_LEFT;

  if(keyName.EqualsIgnoreCase("VK_UP"))
    return nsIDOMKeyEvent::DOM_VK_UP;

  if(keyName.EqualsIgnoreCase("VK_RIGHT"))
    return nsIDOMKeyEvent::DOM_VK_RIGHT;

  if(keyName.EqualsIgnoreCase("VK_DOWN"))
    return nsIDOMKeyEvent::DOM_VK_DOWN;

  if(keyName.EqualsIgnoreCase("VK_PRINTSCREEN"))
    return nsIDOMKeyEvent::DOM_VK_PRINTSCREEN;

  if(keyName.EqualsIgnoreCase("VK_INSERT"))
    return nsIDOMKeyEvent::DOM_VK_INSERT;

  if(keyName.EqualsIgnoreCase("VK_DELETE"))
    return nsIDOMKeyEvent::DOM_VK_DELETE;

  if(keyName.EqualsIgnoreCase("VK_0"))
    return nsIDOMKeyEvent::DOM_VK_0;

  if(keyName.EqualsIgnoreCase("VK_1"))
    return nsIDOMKeyEvent::DOM_VK_1;

  if(keyName.EqualsIgnoreCase("VK_2"))
    return nsIDOMKeyEvent::DOM_VK_2;

  if(keyName.EqualsIgnoreCase("VK_3"))
    return nsIDOMKeyEvent::DOM_VK_3;

  if(keyName.EqualsIgnoreCase("VK_4"))
    return nsIDOMKeyEvent::DOM_VK_4;

  if(keyName.EqualsIgnoreCase("VK_5"))
    return nsIDOMKeyEvent::DOM_VK_5;

  if(keyName.EqualsIgnoreCase("VK_6"))
    return nsIDOMKeyEvent::DOM_VK_6;

  if(keyName.EqualsIgnoreCase("VK_7"))
    return nsIDOMKeyEvent::DOM_VK_7;

  if(keyName.EqualsIgnoreCase("VK_8"))
    return nsIDOMKeyEvent::DOM_VK_8;

  if(keyName.EqualsIgnoreCase("VK_9"))
    return nsIDOMKeyEvent::DOM_VK_9;

  if(keyName.EqualsIgnoreCase("VK_SEMICOLON"))
    return nsIDOMKeyEvent::DOM_VK_SEMICOLON;

  if(keyName.EqualsIgnoreCase("VK_EQUALS"))
    return nsIDOMKeyEvent::DOM_VK_EQUALS;
  if(keyName.EqualsIgnoreCase("VK_A"))
    return nsIDOMKeyEvent::DOM_VK_A;
  if(keyName.EqualsIgnoreCase("VK_B"))
    return nsIDOMKeyEvent::DOM_VK_B;
  if(keyName.EqualsIgnoreCase("VK_C"))
    return nsIDOMKeyEvent::DOM_VK_C;
  if(keyName.EqualsIgnoreCase("VK_D"))
    return nsIDOMKeyEvent::DOM_VK_D;
  if(keyName.EqualsIgnoreCase("VK_E"))
    return nsIDOMKeyEvent::DOM_VK_E;
  if(keyName.EqualsIgnoreCase("VK_F"))
    return nsIDOMKeyEvent::DOM_VK_F;
  if(keyName.EqualsIgnoreCase("VK_G"))
    return nsIDOMKeyEvent::DOM_VK_G;
  if(keyName.EqualsIgnoreCase("VK_H"))
    return nsIDOMKeyEvent::DOM_VK_H;
  if(keyName.EqualsIgnoreCase("VK_I"))
    return nsIDOMKeyEvent::DOM_VK_I;
  if(keyName.EqualsIgnoreCase("VK_J"))
    return nsIDOMKeyEvent::DOM_VK_J;
  if(keyName.EqualsIgnoreCase("VK_K"))
    return nsIDOMKeyEvent::DOM_VK_K;
  if(keyName.EqualsIgnoreCase("VK_L"))
    return nsIDOMKeyEvent::DOM_VK_L;
  if(keyName.EqualsIgnoreCase("VK_M"))
    return nsIDOMKeyEvent::DOM_VK_M;
  if(keyName.EqualsIgnoreCase("VK_N"))
    return nsIDOMKeyEvent::DOM_VK_N;
  if(keyName.EqualsIgnoreCase("VK_O"))
    return nsIDOMKeyEvent::DOM_VK_O;
  if(keyName.EqualsIgnoreCase("VK_P"))
    return nsIDOMKeyEvent::DOM_VK_P;
  if(keyName.EqualsIgnoreCase("VK_Q"))
    return nsIDOMKeyEvent::DOM_VK_Q;
  if(keyName.EqualsIgnoreCase("VK_R"))
    return nsIDOMKeyEvent::DOM_VK_R;
  if(keyName.EqualsIgnoreCase("VK_S"))
    return nsIDOMKeyEvent::DOM_VK_S;
  if(keyName.EqualsIgnoreCase("VK_T"))
    return nsIDOMKeyEvent::DOM_VK_T;
  if(keyName.EqualsIgnoreCase("VK_U"))
    return nsIDOMKeyEvent::DOM_VK_U;
  if(keyName.EqualsIgnoreCase("VK_V"))
    return nsIDOMKeyEvent::DOM_VK_V;
  if(keyName.EqualsIgnoreCase("VK_W"))
    return nsIDOMKeyEvent::DOM_VK_W;
  if(keyName.EqualsIgnoreCase("VK_X"))
    return nsIDOMKeyEvent::DOM_VK_X;
  if(keyName.EqualsIgnoreCase("VK_Y"))
    return nsIDOMKeyEvent::DOM_VK_Y;
  if(keyName.EqualsIgnoreCase("VK_Z"))
    return nsIDOMKeyEvent::DOM_VK_Z;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD0"))
    return nsIDOMKeyEvent::DOM_VK_NUMPAD0;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD1"))
    return nsIDOMKeyEvent::DOM_VK_NUMPAD1;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD2"))
    return nsIDOMKeyEvent::DOM_VK_NUMPAD2;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD3"))
    return nsIDOMKeyEvent::DOM_VK_NUMPAD3;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD4"))
    return nsIDOMKeyEvent::DOM_VK_NUMPAD4;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD5"))
    return nsIDOMKeyEvent::DOM_VK_NUMPAD5;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD6"))
    return nsIDOMKeyEvent::DOM_VK_NUMPAD6;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD7"))
    return nsIDOMKeyEvent::DOM_VK_NUMPAD7;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD8"))
    return nsIDOMKeyEvent::DOM_VK_NUMPAD8;
  if(keyName.EqualsIgnoreCase("VK_NUMPAD9"))
    return nsIDOMKeyEvent::DOM_VK_NUMPAD9;
  if(keyName.EqualsIgnoreCase("VK_MULTIPLY"))
    return nsIDOMKeyEvent::DOM_VK_MULTIPLY;
  if(keyName.EqualsIgnoreCase("VK_ADD"))
    return nsIDOMKeyEvent::DOM_VK_ADD;
  if(keyName.EqualsIgnoreCase("VK_SEPARATOR"))
    return nsIDOMKeyEvent::DOM_VK_SEPARATOR;
  if(keyName.EqualsIgnoreCase("VK_SUBTRACT"))
    return nsIDOMKeyEvent::DOM_VK_SUBTRACT;
  if(keyName.EqualsIgnoreCase("VK_DECIMAL"))
    return nsIDOMKeyEvent::DOM_VK_DECIMAL;
  if(keyName.EqualsIgnoreCase("VK_DIVIDE"))
    return nsIDOMKeyEvent::DOM_VK_DIVIDE;
  if(keyName.EqualsIgnoreCase("VK_F1"))
    return nsIDOMKeyEvent::DOM_VK_F1;
  if(keyName.EqualsIgnoreCase("VK_F2"))
    return nsIDOMKeyEvent::DOM_VK_F2;
  if(keyName.EqualsIgnoreCase("VK_F3"))
    return nsIDOMKeyEvent::DOM_VK_F3;
  if(keyName.EqualsIgnoreCase("VK_F4"))
    return nsIDOMKeyEvent::DOM_VK_F4;
  if(keyName.EqualsIgnoreCase("VK_F5"))
    return nsIDOMKeyEvent::DOM_VK_F5;
  if(keyName.EqualsIgnoreCase("VK_F6"))
    return nsIDOMKeyEvent::DOM_VK_F6;
  if(keyName.EqualsIgnoreCase("VK_F7"))
    return nsIDOMKeyEvent::DOM_VK_F7;
  if(keyName.EqualsIgnoreCase("VK_F8"))
    return nsIDOMKeyEvent::DOM_VK_F8;
  if(keyName.EqualsIgnoreCase("VK_F9"))
    return nsIDOMKeyEvent::DOM_VK_F9;
  if(keyName.EqualsIgnoreCase("VK_F10"))
    return nsIDOMKeyEvent::DOM_VK_F10;
  if(keyName.EqualsIgnoreCase("VK_F11"))
    return nsIDOMKeyEvent::DOM_VK_F11;
  if(keyName.EqualsIgnoreCase("VK_F12"))
    return nsIDOMKeyEvent::DOM_VK_F12;
  if(keyName.EqualsIgnoreCase("VK_F13"))
    return nsIDOMKeyEvent::DOM_VK_F13;
  if(keyName.EqualsIgnoreCase("VK_F14"))
    return nsIDOMKeyEvent::DOM_VK_F14;
  if(keyName.EqualsIgnoreCase("VK_F15"))
    return nsIDOMKeyEvent::DOM_VK_F15;
  if(keyName.EqualsIgnoreCase("VK_F16"))
    return nsIDOMKeyEvent::DOM_VK_F16;
  if(keyName.EqualsIgnoreCase("VK_F17"))
    return nsIDOMKeyEvent::DOM_VK_F17;
  if(keyName.EqualsIgnoreCase("VK_F18"))
    return nsIDOMKeyEvent::DOM_VK_F18;
  if(keyName.EqualsIgnoreCase("VK_F19"))
    return nsIDOMKeyEvent::DOM_VK_F19;
  if(keyName.EqualsIgnoreCase("VK_F20"))
    return nsIDOMKeyEvent::DOM_VK_F20;
  if(keyName.EqualsIgnoreCase("VK_F21"))
    return nsIDOMKeyEvent::DOM_VK_F21;
  if(keyName.EqualsIgnoreCase("VK_F22"))
    return nsIDOMKeyEvent::DOM_VK_F22;
  if(keyName.EqualsIgnoreCase("VK_F23"))
    return nsIDOMKeyEvent::DOM_VK_F23;
  if(keyName.EqualsIgnoreCase("VK_F24"))
    return nsIDOMKeyEvent::DOM_VK_F24;
  if(keyName.EqualsIgnoreCase("VK_NUM_LOCK"))
    return nsIDOMKeyEvent::DOM_VK_NUM_LOCK;
  if(keyName.EqualsIgnoreCase("VK_SCROLL_LOCK"))
    return nsIDOMKeyEvent::DOM_VK_SCROLL_LOCK;
  if(keyName.EqualsIgnoreCase("VK_COMMA"))
    return nsIDOMKeyEvent::DOM_VK_COMMA;
  if(keyName.EqualsIgnoreCase("VK_PERIOD"))
    return nsIDOMKeyEvent::DOM_VK_PERIOD;
  if(keyName.EqualsIgnoreCase("VK_SLASH"))
    return nsIDOMKeyEvent::DOM_VK_SLASH;
  if(keyName.EqualsIgnoreCase("VK_BACK_QUOTE"))
    return nsIDOMKeyEvent::DOM_VK_BACK_QUOTE;
  if(keyName.EqualsIgnoreCase("VK_OPEN_BRACKET"))
    return nsIDOMKeyEvent::DOM_VK_OPEN_BRACKET;
  if(keyName.EqualsIgnoreCase("VK_BACK_SLASH"))
    return nsIDOMKeyEvent::DOM_VK_BACK_SLASH;
  if(keyName.EqualsIgnoreCase("VK_CLOSE_BRACKET"))
    return nsIDOMKeyEvent::DOM_VK_CLOSE_BRACKET;
  if(keyName.EqualsIgnoreCase("VK_QUOTE"))
    return nsIDOMKeyEvent::DOM_VK_QUOTE;

  return 0;
}

PRInt32 nsXBLPrototypeHandler::KeyToMask(PRInt32 key)
{
  switch (key)
  {
    case nsIDOMKeyEvent::DOM_VK_META:
      return cMeta;
      break;

    case nsIDOMKeyEvent::DOM_VK_ALT:
      return cAlt;
      break;

    case nsIDOMKeyEvent::DOM_VK_CONTROL:
    default:
      return cControl;
  }
  return cControl;  // for warning avoidance
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
    else if (PL_strcmp(token, "accel") == 0)
      mKeyMask |= KeyToMask(kAccelKey);
    else if (PL_strcmp(token, "access") == 0)
      mKeyMask |= KeyToMask(kMenuAccessKey);
    
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
