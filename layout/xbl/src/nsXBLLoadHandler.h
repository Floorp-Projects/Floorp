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

#ifndef nsXBLLoadHandler_h__
#define nsXBLLoadHandler_h__

#include "nsIDOMLoadListener.h"
#include "nsXBLEventHandler.h"

class nsIXBLBinding;
class nsIDOMEvent;
class nsIContent;
class nsIDOMUIEvent;
class nsIAtom;
class nsIController;
class nsIXBLPrototypeHandler;

class nsXBLLoadHandler : public nsIDOMLoadListener, 
                         public nsXBLEventHandler
{
public:
  nsXBLLoadHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler);
  virtual ~nsXBLLoadHandler();
  
  // nsIDOMetc.
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };
  
  virtual nsresult Load(nsIDOMEvent* aEvent);
  virtual nsresult Unload(nsIDOMEvent* aEvent);
  virtual nsresult Abort(nsIDOMEvent* aEvent);
  virtual nsresult Error(nsIDOMEvent* aEvent);
   
  NS_DECL_ISUPPORTS_INHERITED

protected:
  static PRUint32 gRefCnt;
  static nsIAtom* kLoadAtom;
  static nsIAtom* kUnloadAtom;
  static nsIAtom* kAbortAtom;
  static nsIAtom* kErrorAtom;
  
protected:
  // Members
};

extern nsresult
NS_NewXBLLoadHandler(nsIDOMEventReceiver* aEventReceiver, nsIXBLPrototypeHandler* aHandlerElement, 
                     nsXBLLoadHandler** aResult);


#endif
