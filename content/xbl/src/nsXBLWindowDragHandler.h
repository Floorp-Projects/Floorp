/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  - Mike Pinkerton (pinkerton@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsXBLWindowDragHandler_h__
#define nsXBLWindowDragHandler_h__

#include "nsIDOMDragListener.h"
#include "nsIDOMElement.h"
#include "nsXBLWindowHandler.h"

class nsIAtom;
class nsIDOMEventReceiver;
class nsXBLSpecialDocInfo;

class nsXBLWindowDragHandler : public nsIDOMDragListener, public nsXBLWindowHandler
{
public:
  nsXBLWindowDragHandler(nsIDOMEventReceiver* aReceiver);
  virtual ~nsXBLWindowDragHandler();
  
  // nsIDOMetc.
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };
  NS_IMETHOD DragGesture(nsIDOMEvent* aMouseEvent) ;
  NS_IMETHOD DragOver(nsIDOMEvent* aMouseEvent) ;
  NS_IMETHOD DragEnter(nsIDOMEvent* aMouseEvent) ;
  NS_IMETHOD DragExit(nsIDOMEvent* aMouseEvent) ;
  NS_IMETHOD DragDrop(nsIDOMEvent* aMouseEvent) ;
   
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
