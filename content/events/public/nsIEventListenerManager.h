/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsIEventListenerManager_h__
#define nsIEventListenerManager_h__

#include "nsEvent.h"
#include "nsISupports.h"

class nsPresContext;
class nsIDOMEventListener;
class nsIScriptContext;
class nsIDOMEventTarget;
class nsIDOMEventGroup;
class nsIAtom;

/*
 * Event listener manager interface.
 */
#define NS_IEVENTLISTENERMANAGER_IID \
{ 0x06177dc7, 0x4ad9, 0x493c, \
  { 0x9c, 0x04, 0x28, 0xf0, 0xf7, 0x39, 0xa0, 0xfe } }


class nsIEventListenerManager : public nsISupports {

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IEVENTLISTENERMANAGER_IID)

  /**
  * Sets events listeners of all types.
  * @param an event listener
  */
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,
                                   const nsIID& aIID, PRInt32 flags) = 0;

  /**
  * Removes events listeners of all types.
  * @param an event listener
  */
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                      const nsIID& aIID, PRInt32 flags) = 0;

  /**
  * Sets events listeners of all types.
  * @param an event listener
  */
  NS_IMETHOD AddEventListenerByType(nsIDOMEventListener *aListener,
                                    const nsAString& type,
                                    PRInt32 flags,
                                    nsIDOMEventGroup* aEvtGrp) = 0;

  /**
  * Removes events listeners of all types.
  * @param an event listener
  */
  NS_IMETHOD RemoveEventListenerByType(nsIDOMEventListener *aListener,
                                       const nsAString& type,
                                       PRInt32 flags,
                                       nsIDOMEventGroup* aEvtGrp) = 0;

  /**
  * Creates a script event listener for the given script object with
  * name aName and function body aFunc.
  * @param an event listener
  */
  NS_IMETHOD AddScriptEventListener(nsISupports *aObject,
                                    nsIAtom *aName,
                                    const nsAString& aFunc,
                                    PRUint32 aLanguage,
                                    PRBool aDeferCompilation,
                                    PRBool aPermitUntrustedEvents) = 0;


  NS_IMETHOD RemoveScriptEventListener(nsIAtom *aName) = 0;

  /**
  * Registers an event listener that already exists on the given
  * script object with the event listener manager. If the event
  * listener is registerd from chrome code, the event listener will
  * only ever receive trusted events.
  * @param the name of an event listener
  */
  NS_IMETHOD RegisterScriptEventListener(nsIScriptContext *aContext,
                                         void *aScopeObject,
                                         nsISupports *aObject,
                                         nsIAtom* aName) = 0;

  /**
  * Compiles any event listeners that already exists on the given
  * script object for a given event type.
  * @param an event listener */
  NS_IMETHOD CompileScriptEventListener(nsIScriptContext *aContext,
                                        void *aScopeObject,
                                        nsISupports *aObject,
                                        nsIAtom* aName,
                                        PRBool *aDidCompile) = 0;

  /**
  * Causes a check for event listeners and processing by them if they exist.
  * Event flags live in nsGUIEvent.h
  * @param an event listener
  */
  NS_IMETHOD HandleEvent(nsPresContext* aPresContext,
                         nsEvent* aEvent,
                         nsIDOMEvent** aDOMEvent,
                         nsISupports* aCurrentTarget,
                         PRUint32 aFlags,
                         nsEventStatus* aEventStatus) = 0;

  /**
  * Tells the event listener manager that its target (which owns it) is
  * no longer using it (and could go away).
  *
  * It also clears the weak pointer set by the call to
  * |SetListenerTarget|.
  */
  NS_IMETHOD Disconnect() = 0;

  /**
  * Tells the event listener manager what its target is.  This must be
  * followed by a call to |Disconnect| before the target is destroyed.
  */
  NS_IMETHOD SetListenerTarget(nsISupports* aTarget) = 0;

  /**
  * Allows us to quickly determine if we have mutation listeners registered.
  */
  NS_IMETHOD HasMutationListeners(PRBool* aListener) = 0;

  /**
  * Gets the EventGroup registered for use by system event listeners.
  * This is a special EventGroup which is used in the secondary DOM Event
  * Loop pass for evaluation of system event listeners.
  */
  NS_IMETHOD GetSystemEventGroupLM(nsIDOMEventGroup** aGroup) = 0;

  /**
   * Allows us to quickly determine whether we have unload or beforeunload
   * listeners registered.
   */
  virtual PRBool HasUnloadListeners() = 0;

  /**
   * Returns the mutation bits depending on which mutation listeners are
   * registered to this listener manager.
   * @note If a listener is an nsIDOMMutationListener, all possible mutation
   *       event bits are returned.
   */
  virtual PRUint32 MutationListenerBits() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIEventListenerManager,
                              NS_IEVENTLISTENERMANAGER_IID)

nsresult
NS_NewEventListenerManager(nsIEventListenerManager** aInstancePtrResult);

#endif // nsIEventListenerManager_h__
