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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

#ifndef nsIEventListenerManager_h__
#define nsIEventListenerManager_h__

#include "nsEvent.h"
#include "nsISupports.h"
#include "nsVoidArray.h"

class nsIPresContext;
class nsIDOMEventListener;
class nsIScriptContext;
class nsIDOMEventTarget;

/*
 * Event listener manager interface.
 */
#define NS_IEVENTLISTENERMANAGER_IID \
{ /* cd91bcf0-ded9-11d1-bd85-00805f8ae3f4 */ \
0xcd91bcf0, 0xded9, 0x11d1, \
{0xbd, 0x85, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIEventListenerManager : public nsISupports {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IEVENTLISTENERMANAGER_IID)

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
                                    const nsAReadableString& type,
                                    PRInt32 flags) = 0;

  /**
  * Removes events listeners of all types.
  * @param an event listener
  */
  NS_IMETHOD RemoveEventListenerByType(nsIDOMEventListener *aListener,
                                       const nsAReadableString& type,
                                       PRInt32 flags) = 0;

  /**
  * Creates a script event listener for the given script object with
  * name aName and function body aFunc.
  * @param an event listener
  */
  NS_IMETHOD AddScriptEventListener(nsIScriptContext*aContext,
                                    nsISupports *aObject,
                                    nsIAtom *aName,
                                    const nsAReadableString& aFunc,
                                    PRBool aDeferCompilation) = 0;


  NS_IMETHOD RemoveScriptEventListener(nsIAtom *aName) = 0;

  /**
  * Registers an event listener that already exists on the given
  * script object with the event listener manager.
  * @param an event listener
  */
  NS_IMETHOD RegisterScriptEventListener(nsIScriptContext *aContext,
                                         nsISupports *aObject,
                                         nsIAtom* aName) = 0;

  /**
  * Compiles any event listeners that already exists on the given
  * script object for a given event type.
  * @param an event listener */
  NS_IMETHOD CompileScriptEventListener(nsIScriptContext *aContext,
                                        nsISupports *aObject,
                                        nsIAtom* aName,
                                        PRBool *aDidCompile) = 0;

  /**
  * Causes a check for event listeners and processing by them if they exist.
  * Event flags live in nsGUIEvent.h
  * @param an event listener
  */
  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext,
                         nsEvent* aEvent,
                         nsIDOMEvent** aDOMEvent,
                         nsIDOMEventTarget* aCurrentTarget,
                         PRUint32 aFlags,
                         nsEventStatus* aEventStatus) = 0;

  /**
  * Creates a DOM event that can subsequently be passed into HandleEvent.
  * (used rarely in the situation where methods on the event need to be
  * invoked prior to the processing of the event).
  */
  NS_IMETHOD CreateEvent(nsIPresContext* aPresContext,
                         nsEvent* aEvent,
                         const nsAReadableString& aEventType,
                         nsIDOMEvent** aDOMEvent) = 0;

  /**
  * Changes script listener of specified event types from bubbling
  * listeners to capturing listeners.
  * @param event types */
  NS_IMETHOD CaptureEvent(PRInt32 aEventTypes) = 0;

  /**
  * Changes script listener of specified event types from capturing
  * listeners to bubbling listeners.
  * @param event types */
  NS_IMETHOD ReleaseEvent(PRInt32 aEventTypes) = 0;

  /**
  * Removes all event listeners registered by this instance of the listener
  * manager.
  */
  NS_IMETHOD RemoveAllListeners(PRBool aScriptOnly) = 0;

  /**
  * Removes all event listeners registered by this instance of the listener
  * manager.
  */
  NS_IMETHOD SetListenerTarget(nsISupports* aTarget) = 0;

  /**
  * Allows us to quickly determine if we have mutation listeners registered.
  */
  NS_IMETHOD HasMutationListeners(PRBool* aListener) = 0;
};

extern nsresult
NS_NewEventListenerManager(nsIEventListenerManager** aInstancePtrResult);

#endif // nsIEventListenerManager_h__
