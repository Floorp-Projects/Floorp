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
 * Contributor(s): 
 *  - Mike Pinkerton (pinkerton@netscape.com)
 */

#ifndef nsXBLWindowDragHandler_h__
#define nsXBLWindowDragHandler_h__

#include "nsIDOMDragListener.h"
#include "nsIDOMElement.h"
#include "nsXBLWindowHandler.h"

class nsIAtom;
class nsIDOMEventReceiver;
struct nsXBLSpecialDocInfo;

class nsXBLWindowDragHandler : public nsIDOMDragListener, public nsXBLWindowHandler
{
public:
  nsXBLWindowDragHandler(nsIDOMEventReceiver* aReceiver);
  virtual ~nsXBLWindowDragHandler();
  
  // nsIDOMetc.
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };
  virtual nsresult DragGesture(nsIDOMEvent* aMouseEvent) ;
  virtual nsresult DragOver(nsIDOMEvent* aMouseEvent) ;
  virtual nsresult DragEnter(nsIDOMEvent* aMouseEvent) ;
  virtual nsresult DragExit(nsIDOMEvent* aMouseEvent) ;
  virtual nsresult DragDrop(nsIDOMEvent* aMouseEvent) ;
   
  NS_DECL_ISUPPORTS

protected:
  NS_IMETHOD WalkHandlers(nsIDOMEvent* aKeyEvent, nsIAtom* aEventType);

    // check if the given handler cares about the given key event
  PRBool EventMatched ( nsIXBLPrototypeHandler* inHandler, nsIAtom* inEventType,
                          nsIDOMEvent* inEvent ) ;
  
    // We need our own refcount (even though our base class has one) because
    // we have our own statics that need to be initialized and the creation of
    // other subclasses would cause us to miss things if we shared the counter.  
  static PRUint32 gRefCnt;
  static nsIAtom* kDragGestureAtom;
  static nsIAtom* kDragOverAtom;
  static nsIAtom* kDragEnterAtom;
  static nsIAtom* kDragExitAtom;
  static nsIAtom* kDragDropAtom;

};

extern nsresult
NS_NewXBLWindowDragHandler(nsIDOMEventReceiver* aReceiver,
                            nsXBLWindowDragHandler** aResult);

#endif
