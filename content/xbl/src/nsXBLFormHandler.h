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

#ifndef nsXBLFormHandler_h__
#define nsXBLFormHandler_h__

#include "nsIDOMFormListener.h"
#include "nsXBLEventHandler.h"

class nsIXBLBinding;
class nsIDOMEvent;
class nsIContent;
class nsIDOMUIEvent;
class nsIAtom;
class nsIController;
class nsIXBLPrototypeHandler;

class nsXBLFormHandler : public nsIDOMFormListener, 
                         public nsXBLEventHandler
{
public:
  nsXBLFormHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler, nsIAtom* aEventName);
  virtual ~nsXBLFormHandler();
  
  // nsIDOMetc.
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };
  
  virtual nsresult Submit(nsIDOMEvent* aEvent);
  virtual nsresult Reset(nsIDOMEvent* aEvent);
  virtual nsresult Change(nsIDOMEvent* aEvent);
  virtual nsresult Select(nsIDOMEvent* aEvent);
  virtual nsresult Input(nsIDOMEvent* aEvent);
   
  NS_DECL_ISUPPORTS_INHERITED

protected:
  static PRUint32 gRefCnt;
  static nsIAtom* kSubmitAtom;
  static nsIAtom* kResetAtom;
  static nsIAtom* kChangeAtom;
  static nsIAtom* kSelectAtom;
  static nsIAtom* kInputAtom;

protected:
  // Members
};

extern nsresult
NS_NewXBLFormHandler(nsIDOMEventReceiver* aEventReceiver, nsIXBLPrototypeHandler* aHandlerElement, 
                      nsIAtom* aEventName,
                      nsXBLFormHandler** aResult);


#endif
