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

#ifndef nsXBLMutationHandler_h__
#define nsXBLMutationHandler_h__

#include "nsIDOMMutationListener.h"
#include "nsXBLEventHandler.h"

class nsIXBLBinding;
class nsIDOMEvent;
class nsIContent;
class nsIDOMUIEvent;
class nsIAtom;
class nsIController;
class nsIXBLPrototypeHandler;

class nsXBLMutationHandler : public nsIDOMMutationListener, 
                             public nsXBLEventHandler
{
public:
  nsXBLMutationHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler);
  virtual ~nsXBLMutationHandler();
  
  // nsIDOMetc.
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };
  
  NS_IMETHOD SubtreeModified(nsIDOMEvent* aEvent);
  NS_IMETHOD AttrModified(nsIDOMEvent* aEvent);
  NS_IMETHOD CharacterDataModified(nsIDOMEvent* aEvent);
  NS_IMETHOD NodeInserted(nsIDOMEvent* aEvent);
  NS_IMETHOD NodeRemoved(nsIDOMEvent* aEvent);
  NS_IMETHOD NodeInsertedIntoDocument(nsIDOMEvent* aEvent);
  NS_IMETHOD NodeRemovedFromDocument(nsIDOMEvent* aEvent);
   
  NS_DECL_ISUPPORTS_INHERITED

protected:
  static PRUint32 gRefCnt;
  static nsIAtom* kSubtreeModifiedAtom;
  static nsIAtom* kAttrModifiedAtom;
  static nsIAtom* kCharacterDataModifiedAtom;
  static nsIAtom* kNodeInsertedAtom;
  static nsIAtom* kNodeRemovedAtom;
  static nsIAtom* kNodeInsertedIntoDocumentAtom;
  static nsIAtom* kNodeRemovedFromDocumentAtom;

protected:
  // Members
};

extern nsresult
NS_NewXBLMutationHandler(nsIDOMEventReceiver* aEventReceiver, nsIXBLPrototypeHandler* aHandlerElement, 
                     nsXBLMutationHandler** aResult);


#endif
