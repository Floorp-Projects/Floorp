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
 *  - David W. Hyatt (hyatt@netscape.com)
 *  - Mike Pinkerton (pinkerton@netscape.com)
 */

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
struct nsXBLSpecialDocInfo;


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
  void GetHandlers(nsIXBLDocumentInfo* aInfo, const nsAReadableCString& aDocURI, 
                    const nsAReadableCString& aRef, nsIXBLPrototypeHandler** aResult) ;

    // does the handler care about the particular event?
  virtual PRBool EventMatched ( nsIXBLPrototypeHandler* inHandler, nsIAtom* inEventType,
                                 nsIDOMEvent* inEvent ) = 0;                                 
                    
  nsIDOMElement* mElement;            // weak ref
  nsIDOMEventReceiver* mReceiver;     // weak ref

  nsCOMPtr<nsIXBLPrototypeHandler> mHandler;            // XP bindings
  nsCOMPtr<nsIXBLPrototypeHandler> mPlatformHandler;    // platform-specific bindings

  static nsXBLSpecialDocInfo* sXBLSpecialDocInfo;       // holds document info about bindings
    
private:

  static PRUint32 sRefCnt;

}; // class nsXBLWindowHandler


#endif
