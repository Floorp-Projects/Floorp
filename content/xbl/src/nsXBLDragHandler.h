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

#ifndef nsXBLDragHandler_h__
#define nsXBLDragHandler_h__

#include "nsIDOMDragListener.h"
#include "nsXBLEventHandler.h"

class nsIXBLBinding;
class nsIDOMEvent;
class nsIContent;
class nsIDOMUIEvent;
class nsIAtom;
class nsIController;
class nsIXBLPrototypeHandler;

class nsXBLDragHandler : public nsIDOMDragListener, 
                         public nsXBLEventHandler
{
public:
  nsXBLDragHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler);
  virtual ~nsXBLDragHandler();
  
  // nsIDOMetc.
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };
  
  virtual nsresult DragEnter(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragOver(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragExit(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragDrop(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragGesture(nsIDOMEvent* aMouseEvent);
   
  NS_DECL_ISUPPORTS_INHERITED

protected:
  static PRUint32 gRefCnt;
  static nsIAtom* kDragEnterAtom;
  static nsIAtom* kDragOverAtom;
  static nsIAtom* kDragExitAtom;
  static nsIAtom* kDragDropAtom;
  static nsIAtom* kDragGestureAtom;

protected:
  // Members
};

extern nsresult
NS_NewXBLDragHandler(nsIDOMEventReceiver* aEventReceiver, nsIXBLPrototypeHandler* aHandlerElement, 
                     nsXBLDragHandler** aResult);


#endif
