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
 *  - David W. Hyatt (hyatt@netscape.com)
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

#ifndef nsXBLWindowHandler_h__
#define nsXBLWindowHandler_h__

#include "prtypes.h"
#include "nsError.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIXBLPrototypeHandler.h"


class nsIDOMEventReceiver;
class nsIDOMElement;
class nsIDOMEvent;
class nsIAtom;
class nsIXBLDocumentInfo;
class nsXBLSpecialDocInfo;


//
// class nsXBLWindowHandler
//
// A virtual base class that all window-level event handlers inherit
// from. Provides the code to parse and walk the XBL bindings.
//
class nsXBLWindowHandler 
{
public:

  nsXBLWindowHandler(nsIDOMElement* aElement, nsIDOMEventReceiver* aReceiver);
  virtual ~nsXBLWindowHandler();

protected:

    // are we working with editor or browser?
  PRBool IsEditor() ;

    // lazily load the handlers
  virtual nsresult EnsureHandlers();
  
    // walk the handlers, looking for one to handle the event
  virtual nsresult WalkHandlersInternal(nsIDOMEvent* aKeyEvent, nsIAtom* aEventType, 
                                          nsIXBLPrototypeHandler* aHandler);

    // create the event handler list from the given document/URI
  void GetHandlers(nsIXBLDocumentInfo* aInfo,
                   const nsAReadableCString& aRef,
                   nsIXBLPrototypeHandler** aResult) ;

    // does the handler care about the particular event?
  virtual PRBool EventMatched ( nsIXBLPrototypeHandler* inHandler, nsIAtom* inEventType,
                                 nsIDOMEvent* inEvent ) = 0;                                 
                    
  nsIDOMElement* mElement;            // weak ref
  nsIDOMEventReceiver* mReceiver;     // weak ref

  nsCOMPtr<nsIXBLPrototypeHandler> mHandler;            // XP bindings
  nsCOMPtr<nsIXBLPrototypeHandler> mPlatformHandler;    // platform-specific bindings
  nsCOMPtr<nsIXBLPrototypeHandler> mUserHandler;        // user-specific bindings

  static nsXBLSpecialDocInfo* sXBLSpecialDocInfo;       // holds document info about bindings
    
private:

  static PRUint32 sRefCnt;

}; // class nsXBLWindowHandler


#endif
