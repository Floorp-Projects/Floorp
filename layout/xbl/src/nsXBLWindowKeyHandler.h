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

#ifndef nsXBLWindowKeyHandler_h__
#define nsXBLWindowKeyHandler_h__

#include "nsIDOMKeyListener.h"
#include "nsIDOMElement.h"

class nsIAtom;
class nsIDOMEventReceiver;
struct nsXBLSpecialDocInfo;

class nsXBLWindowKeyHandler : public nsIDOMKeyListener
{
public:
  nsXBLWindowKeyHandler(nsIDOMElement* aElement, nsIDOMEventReceiver* aReceiver);
  virtual ~nsXBLWindowKeyHandler();
  
  // nsIDOMetc.
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };
  
  virtual nsresult KeyUp(nsIDOMEvent* aKeyEvent);
  virtual nsresult KeyDown(nsIDOMEvent* aKeyEvent);
  virtual nsresult KeyPress(nsIDOMEvent* aKeyEvent);
   
  NS_DECL_ISUPPORTS

protected:
  NS_IMETHOD WalkHandlers(nsIDOMEvent* aKeyEvent, nsIAtom* aEventType);
  NS_IMETHOD WalkHandlersInternal(nsIDOMKeyEvent* aKeyEvent, nsIAtom* aEventType, 
                                  nsIXBLPrototypeHandler* aHandler);

  NS_IMETHOD EnsureHandlers();

  PRBool IsEditor();

  static PRUint32 gRefCnt;
  static nsIAtom* kKeyUpAtom;
  static nsIAtom* kKeyDownAtom;
  static nsIAtom* kKeyPressAtom;

protected:
  // Members
  nsIDOMElement* mElement;
  nsCOMPtr<nsIXBLPrototypeHandler> mHandler;
  nsCOMPtr<nsIXBLPrototypeHandler> mPlatformHandler;

  nsXBLSpecialDocInfo* mXBLSpecialDocInfo;

  nsIDOMEventReceiver* mReceiver;
};

extern nsresult
NS_NewXBLWindowKeyHandler(nsIDOMElement* aElement,
                          nsIDOMEventReceiver* aReceiver,
                          nsXBLWindowKeyHandler** aResult);

#endif
