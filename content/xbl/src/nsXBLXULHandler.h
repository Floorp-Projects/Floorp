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

#ifndef nsXBLXULHandler_h__
#define nsXBLXULHandler_h__

#include "nsIDOMMenuListener.h"
#include "nsXBLEventHandler.h"

class nsIXBLBinding;
class nsIDOMEvent;
class nsIContent;
class nsIDOMUIEvent;
class nsIAtom;
class nsIController;
class nsIXBLPrototypeHandler;

class nsXBLXULHandler : public nsIDOMMenuListener, 
                        public nsXBLEventHandler
{
public:
  nsXBLXULHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler);
  virtual ~nsXBLXULHandler();
  
  // nsIDOMetc.
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };
  
  NS_IMETHOD Create(nsIDOMEvent* aEvent);
  NS_IMETHOD Close(nsIDOMEvent* aEvent);
  NS_IMETHOD Destroy(nsIDOMEvent* aEvent);
  NS_IMETHOD Action(nsIDOMEvent* aEvent);
  NS_IMETHOD Broadcast(nsIDOMEvent* aEvent);
  NS_IMETHOD CommandUpdate(nsIDOMEvent* aEvent);
   
  NS_DECL_ISUPPORTS_INHERITED

protected:
  static PRUint32 gRefCnt;
  static nsIAtom* kCreateAtom;
  static nsIAtom* kCloseAtom;
  static nsIAtom* kDestroyAtom;
  static nsIAtom* kCommandUpdateAtom;
  static nsIAtom* kBroadcastAtom;

protected:
  // Members
};

extern nsresult
NS_NewXBLXULHandler(nsIDOMEventReceiver* aEventReceiver, nsIXBLPrototypeHandler* aHandlerElement, 
                    nsXBLXULHandler** aResult);


#endif
