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

#include "nsGUIEvent.h"
#include "nsISupports.h"
#include "nsVoidArray.h"

class nsIPresContext;
class nsIDOMEventListener;
class nsIScriptObjectOwner;
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
  static const nsIID& GetIID() { static nsIID iid = NS_IEVENTLISTENERMANAGER_IID; return iid; }

 /**
  * Retrieves events listeners of all types.
  * @param
  */
  virtual nsresult GetEventListeners(nsVoidArray **aListeners, const nsIID& aIID) = 0;

  /**
  * Sets events listeners of all types.
  * @param an event listener
  */
  virtual nsresult AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID, PRInt32 flags) = 0;

  /**
  * Removes events listeners of all types.
  * @param an event listener
  */
  virtual nsresult RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID, PRInt32 flags) = 0;

  /**
  * Sets events listeners of all types.
  * @param an event listener
  */
  virtual nsresult AddEventListenerByType(nsIDOMEventListener *aListener, const nsAReadableString& type, PRInt32 flags) = 0;

  /**
  * Removes events listeners of all types.
  * @param an event listener
  */
  virtual nsresult RemoveEventListenerByType(nsIDOMEventListener *aListener, const nsAReadableString& type, PRInt32 flags) = 0;

  /**
  * Creates a script event listener for the given script object with name mName and function
  * body mFunc.
  * @param an event listener
  */
  virtual nsresult AddScriptEventListener(nsIScriptContext*aContext,
                                          nsIScriptObjectOwner *aScriptObjectOwner,
                                          nsIAtom *aName,
                                          const nsAReadableString& aFunc,
                                          REFNSIID aIID,
                                          PRBool aDeferCompilation) = 0;

  /**
  * Registers an event listener that already exists on the given script object with the event
  * listener manager.
  * @param an event listener
  */
  virtual nsresult RegisterScriptEventListener(nsIScriptContext *aContext,
                                               nsIScriptObjectOwner *aScriptObjectOwner,
                                               nsIAtom* aName,
                                               REFNSIID aIID) = 0;

  /**
  * Causes a check for event listeners and processing by them if they exist.
  * Event flags live in nsGUIEvent.h
  * @param an event listener
  */
  virtual nsresult HandleEvent(nsIPresContext* aPresContext,
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
  virtual nsresult CreateEvent(nsIPresContext* aPresContext,
                               nsEvent* aEvent,
                               const nsAReadableString& aEventType,
                               nsIDOMEvent** aDOMEvent) = 0;

  /**
  * Changes script listener of specified event types from bubbling listeners to capturing listeners.
  * @param event types
  */
  virtual nsresult CaptureEvent(PRInt32 aEventTypes) = 0;

  /**
  * Changes script listener of specified event types from capturing listeners to bubbling listeners.
  * @param event types
  */
  virtual nsresult ReleaseEvent(PRInt32 aEventTypes) = 0;

  /**
  * Removes all event listeners registered by this instance of the listener
  * manager.
  */
  virtual nsresult RemoveAllListeners(PRBool aScriptOnly) = 0;

  /**
  * Removes all event listeners registered by this instance of the listener
  * manager.
  */
  virtual nsresult SetListenerTarget(nsISupports* aTarget) = 0;
};

extern NS_HTML nsresult NS_NewEventListenerManager(nsIEventListenerManager** aInstancePtrResult);

#endif // nsIEventListenerManager_h__
