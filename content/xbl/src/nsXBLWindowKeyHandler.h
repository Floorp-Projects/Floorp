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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
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

#ifndef nsXBLWindowKeyHandler_h__
#define nsXBLWindowKeyHandler_h__

#include "nsIDOMKeyListener.h"
#include "nsIDOMElement.h"
#include "nsXBLWindowHandler.h"

class nsIAtom;
class nsIDOMEventReceiver;

class nsXBLWindowKeyHandler : public nsIDOMKeyListener, public nsXBLWindowHandler
{
public:
  nsXBLWindowKeyHandler(nsIDOMElement* aElement, nsIDOMEventReceiver* aReceiver);
  virtual ~nsXBLWindowKeyHandler();
  
  // nsIDOMetc.
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);
   
  NS_DECL_ISUPPORTS

protected:

  NS_IMETHOD WalkHandlers(nsIDOMEvent* aKeyEvent, nsIAtom* aEventType);

    // lazily load the handlers. Overridden to handle being attached
    // to a particular element rather than the document
  virtual nsresult EnsureHandlers();
  
    // check if the given handler cares about the given key event
  PRBool EventMatched ( nsIXBLPrototypeHandler* inHandler, nsIAtom* inEventType,
                          nsIDOMEvent* inEvent ) ;

    // We need our own refcount (even though our base class has one) because
    // we have our own statics that need to be initialized and the creation of
    // other subclasses would cause us to miss things if we shared the counter.  
  static PRUint32 gRefCnt;
  static nsIAtom* kKeyUpAtom;
  static nsIAtom* kKeyDownAtom;
  static nsIAtom* kKeyPressAtom;

};

extern nsresult
NS_NewXBLWindowKeyHandler(nsIDOMElement* aElement,
                          nsIDOMEventReceiver* aReceiver,
                          nsXBLWindowKeyHandler** aResult);

#endif
