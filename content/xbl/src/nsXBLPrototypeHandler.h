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

#ifndef nsXBLPrototypeHandler_h__
#define nsXBLPrototypeHandler_h__

#include "nsIXBLPrototypeHandler.h"
#include "nsIAtom.h"

class nsIXBLBinding;
class nsIDOMEvent;
class nsIContent;
class nsIDOMUIEvent;
class nsIDOMKeyEvent;
class nsIDOMMouseEvent;
class nsString;

class nsXBLPrototypeHandler : public nsIXBLPrototypeHandler
{
public:
  nsXBLPrototypeHandler(nsIContent* aHandlerElement);
  virtual ~nsXBLPrototypeHandler();
  
  NS_DECL_ISUPPORTS

  NS_IMETHOD MouseEventMatched(nsIAtom* aEventType, nsIDOMMouseEvent* aEvent, PRBool* aResult);
  NS_IMETHOD KeyEventMatched(nsIAtom* aEventType, nsIDOMKeyEvent* aEvent, PRBool* aResult);

  NS_IMETHOD GetHandlerElement(nsIContent** aResult);

  NS_IMETHOD GetNextHandler(nsIXBLPrototypeHandler** aResult);
  NS_IMETHOD SetNextHandler(nsIXBLPrototypeHandler* aHandler);

  NS_IMETHOD ExecuteHandler(nsIDOMEventReceiver* aReceiver, nsIDOMEvent* aEvent);

  NS_IMETHOD GetEventName(nsIAtom** aResult);

  NS_IMETHOD BindingAttached(nsIDOMEventReceiver* aReceiver);
  NS_IMETHOD BindingDetached(nsIDOMEventReceiver* aReceiver);
  
public:
  static nsresult GetTextData(nsIContent *aParent, nsString& aResult);

  static PRUint32 gRefCnt;
  
  static nsIAtom* kBindingAttachedAtom;
  static nsIAtom* kBindingDetachedAtom;
  static nsIAtom* kKeyAtom;
  static nsIAtom* kKeyCodeAtom;
  static nsIAtom* kCharCodeAtom;
  static nsIAtom* kActionAtom;
  static nsIAtom* kCommandAtom;
  static nsIAtom* kOnCommandAtom;
  static nsIAtom* kFocusCommandAtom;
  static nsIAtom* kClickCountAtom;
  static nsIAtom* kButtonAtom;
  static nsIAtom* kModifiersAtom;
  static nsIAtom* kTypeAtom;

protected:
  NS_IMETHOD GetController(nsIDOMEventReceiver* aReceiver, nsIController** aResult);
  
  inline PRInt32 GetMatchingKeyCode(const nsString& aKeyName);
  void ConstructMask();
  PRBool ModifiersMatchMask(nsIDOMUIEvent* aEvent);

  inline PRBool KeyEventMatched(nsIDOMKeyEvent* aKeyEvent);
  inline PRBool MouseEventMatched(nsIDOMMouseEvent* aMouseEvent);

  PRInt32 KeyToMask(PRInt32 key);
  
  static PRInt32 kAccelKey;
  static PRInt32 kMenuAccessKey;
  static void InitAccessKeys();

  static const PRInt32 cShift;
  static const PRInt32 cAlt;
  static const PRInt32 cControl;
  static const PRInt32 cMeta;

protected:
  nsIContent* mHandlerElement; // This ref is weak.

  PRInt32 mKeyMask;          // Which modifier keys this event handler expects to have down
                             // in order to be matched.

  PRInt32 mDetail;           // For key events, contains a charcode or keycode. For
                             // mouse events, stores the button info.
  
  PRInt32 mDetail2;          // Miscellaneous extra information.  For key events,
                             // stores whether or not we're a key code or char code.
                             // For mouse events, stores the clickCount.

  nsCOMPtr<nsIXBLPrototypeHandler> mNextHandler; // Prototype handlers are chained. We own the next handler in the chain.
  nsCOMPtr<nsIAtom> mEventName; // The type of the event, e.g., "keypress"
};

extern nsresult
NS_NewXBLPrototypeHandler(nsIContent* aHandlerElement, nsIXBLPrototypeHandler** aResult);

#endif
