/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   David W. Hyatt <hyatt@netscape.com> (Original Author)
 */

#ifndef nsWindowRoot_h__
#define nsWindowRoot_h__

class nsIDOMWindow;
class nsIDOMEventListener;
class nsIEventListenerManager;
class nsIDOMEvent;

#include "nsGUIEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOM3EventTarget.h"
#include "nsIChromeEventHandler.h"
#include "nsIEventListenerManager.h"
#include "nsPIWindowRoot.h"
#include "nsIFocusController.h"
#include "nsIDOMEventTarget.h"

class nsWindowRoot : public nsIDOMEventReceiver, public nsIDOM3EventTarget, public nsIChromeEventHandler, public nsPIWindowRoot
{
public:
  nsWindowRoot(nsIDOMWindow* aWindow);
  virtual ~nsWindowRoot();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTTARGET
  NS_DECL_NSIDOM3EVENTTARGET

  NS_IMETHOD HandleChromeEvent(nsIPresContext* aPresContext,
                               nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                               PRUint32 aFlags, nsEventStatus* aEventStatus);

  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);
  NS_IMETHOD GetSystemEventGroup(nsIDOMEventGroup** aGroup);

  // nsPIWindowRoot
  NS_IMETHOD GetFocusController(nsIFocusController** aResult);

protected:
  // Members
  nsIDOMWindow* mWindow; // [Weak]. The window will hold on to us and let go when it dies.
  nsCOMPtr<nsIEventListenerManager> mListenerManager; // [Strong]. We own the manager, which owns event listeners attached
                                                      // to us.
  nsCOMPtr<nsIFocusController> mFocusController; // The focus controller for the root.
};

extern nsresult
NS_NewWindowRoot(nsIDOMWindow* aWindow,
                 nsIChromeEventHandler** aResult);

#endif
