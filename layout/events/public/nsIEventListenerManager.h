/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIEventListenerManager_h__
#define nsIEventListenerManager_h__

#include "nsGUIEvent.h"
#include "nsISupports.h"
#include "nsVoidArray.h"

class nsIPresContext;
class nsIDOMEventListener;
class nsIScriptObjectOwner;

/*
 * Event listener manager interface.
 */
#define NS_IEVENTLISTENERMANAGER_IID \
{ /* cd91bcf0-ded9-11d1-bd85-00805f8ae3f4 */ \
0xcd91bcf0, 0xded9, 0x11d1, \
{0xbd, 0x85, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIEventListenerManager : public nsISupports {

public:

 /**
  * Retrieves events listeners of all types. 
  * @param
  */

  virtual nsresult GetEventListeners(nsVoidArray **aListeners, const nsIID& aIID) = 0;

  /**
  * Sets events listeners of all types. 
  * @param an event listener
  */

  virtual nsresult AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID) = 0;
  
  /**
  * Creates a script event listener for the given script object with name mName and function
  * content mFunc.
  * @param an event listener
  */

  virtual nsresult AddScriptEventListener(nsIScriptContext*aContext, nsIScriptObjectOwner *aScriptObjectOwner, 
                                          nsIAtom *aName, const nsString& aFunc, REFNSIID aIID) = 0; 
  /**
  * Registers an event listners which already exists on the given script object with the event
  * listener manager.
  * @param an event listener
  */

  virtual nsresult RegisterScriptEventListener(nsIScriptContext *aContext, nsIScriptObjectOwner *aScriptObjectOwner, 
                                               REFNSIID aIID) = 0;

  /**
  * Removes events listeners of all types. 
  * @param an event listener
  */

  virtual nsresult RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID) = 0;

  /**
  * Causes a check for event listeners and processing by them if they exist.
  * @param an event listener
  */

  virtual nsresult HandleEvent(nsIPresContext& aPresContext,
                               nsEvent* aEvent,
                               nsIDOMEvent** aDOMEvent,
                               nsEventStatus& aEventStatus) = 0;

  /**
  * Creates a DOM event that can subsequently be passed into HandleEvent.
  * (used rarely in the situation where methods on the event need to be
  * invoked prior to the processing of the event).
  */
  virtual nsresult CreateEvent(nsIPresContext& aPresContext,
                               nsEvent* aEvent,
                               nsIDOMEvent** aDOMEvent) = 0;

  /**
  * Captures all events designated for descendant objects at the current level.
  * @param an event listener
  */

  virtual nsresult CaptureEvent(nsIDOMEventListener *aListener) = 0;

  /**
  * Releases all events designated for descendant objects at the current level.
  * @param an event listener
  */

  virtual nsresult ReleaseEvent(nsIDOMEventListener *aListener) = 0;

};

extern NS_HTML nsresult NS_NewEventListenerManager(nsIEventListenerManager** aInstancePtrResult); 

#endif // nsIEventListenerManager_h__
