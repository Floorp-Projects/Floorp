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

class nsIXBLBinding;
class nsIDOMEvent;
class nsIContent;
class nsIDOMUIEvent;
class nsIDOMKeyEvent;
class nsIDOMMouseEvent;
class nsIAtom;
class nsString;

class nsXBLPrototypeHandler : public nsIXBLPrototypeHandler
{
public:
  nsXBLPrototypeHandler(nsIContent* aHandlerElement);
  virtual ~nsXBLPrototypeHandler();
  
  NS_DECL_ISUPPORTS

  NS_IMETHOD EventMatched(nsIDOMEvent* aEvent, PRBool* aResult);
  NS_IMETHOD GetHandlerElement(nsIContent** aResult);

  NS_IMETHOD GetNextHandler(nsIXBLPrototypeHandler** aResult);
  NS_IMETHOD SetNextHandler(nsIXBLPrototypeHandler* aHandler);

protected:
  inline PRUint32 GetMatchingKeyCode(const nsString& aKeyName);
  void ConstructMask();
  PRBool ModifiersMatchMask(nsIDOMUIEvent* aEvent);

  inline PRBool KeyEventMatched(nsIDOMKeyEvent* aKeyEvent);
  inline PRBool MouseEventMatched(nsIDOMMouseEvent* aMouseEvent);
  
  static PRUint32 gRefCnt;
  static nsIAtom* kKeyAtom;
  static nsIAtom* kKeyCodeAtom;
  static nsIAtom* kCharCodeAtom;
  static nsIAtom* kActionAtom;
  static nsIAtom* kCommandAtom;
  static nsIAtom* kClickCountAtom;
  static nsIAtom* kButtonAtom;
  static nsIAtom* kModifiersAtom;

  static PRInt32 kAccessKey;
  static void InitAccessKey();

  static const PRInt32 cShift;
  static const PRInt32 cAlt;
  static const PRInt32 cControl;
  static const PRInt32 cMeta;

protected:
  nsIContent* mHandlerElement; // This ref is weak.

  PRInt32 mKeyMask;          // Which modifier keys this event handler expects to have down
                             // in order to be matched.

  PRUint32 mDetail;          // For key events, contains a charcode or keycode. For
                             // mouse events, stores the button info.
  
  PRInt32 mDetail2;          // Miscellaneous extra information.  For key events,
                             // stores whether or not we're a key code or char code.
                             // For mouse events, stores the clickCount.

  nsCOMPtr<nsIXBLPrototypeHandler> mNextHandler; // Prototype handlers are chained. We own the next handler in the chain.
};

extern nsresult
NS_NewXBLPrototypeHandler(nsIContent* aHandlerElement, nsIXBLPrototypeHandler** aResult);

#endif
