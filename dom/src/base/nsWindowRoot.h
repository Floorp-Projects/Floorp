/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David W. Hyatt <hyatt@netscape.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsWindowRoot_h__
#define nsWindowRoot_h__

class nsIDOMWindow;
class nsIDOMEventListener;
class nsIEventListenerManager;
class nsIDOMEvent;
class nsEventChainPreVisitor;
class nsEventChainPostVisitor;

#include "nsIDOMEventReceiver.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOMNSEventTarget.h"
#include "nsIEventListenerManager.h"
#include "nsPIWindowRoot.h"
#include "nsIFocusController.h"
#include "nsIDOMEventTarget.h"
#include "nsCycleCollectionParticipant.h"

class nsWindowRoot : public nsIDOMEventReceiver,
                     public nsIDOM3EventTarget,
                     public nsIDOMNSEventTarget,
                     public nsPIWindowRoot
{
public:
  nsWindowRoot(nsIDOMWindow* aWindow);
  virtual ~nsWindowRoot();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMEVENTTARGET
  NS_DECL_NSIDOM3EVENTTARGET
  NS_DECL_NSIDOMNSEVENTTARGET

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual nsresult DispatchDOMEvent(nsEvent* aEvent,
                                    nsIDOMEvent* aDOMEvent,
                                    nsPresContext* aPresContext,
                                    nsEventStatus* aEventStatus);

  // nsIDOMEventReceiver
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID);
  NS_IMETHOD GetListenerManager(PRBool aCreateIfNotFound,
                                nsIEventListenerManager** aResult);
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);
  NS_IMETHOD GetSystemEventGroup(nsIDOMEventGroup** aGroup);

  // nsPIWindowRoot
  NS_IMETHOD GetFocusController(nsIFocusController** aResult);

  NS_DECL_CYCLE_COLLECTION_CLASS(nsWindowRoot)

protected:
  // Members
  nsIDOMWindow* mWindow; // [Weak]. The window will hold on to us and let go when it dies.
  nsCOMPtr<nsIEventListenerManager> mListenerManager; // [Strong]. We own the manager, which owns event listeners attached
                                                      // to us.
  nsCOMPtr<nsIFocusController> mFocusController; // The focus controller for the root.
};

extern nsresult
NS_NewWindowRoot(nsIDOMWindow* aWindow,
                 nsPIDOMEventTarget** aResult);

#endif
