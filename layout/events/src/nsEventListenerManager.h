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

#ifndef nsEventListenerManager_h__
#define nsEventListenerManager_h__

#include "nsIEventListenerManager.h"
#include "jsapi.h"

class nsIDOMEvent;
 
/*
 * Event listener manager
 */

class nsEventListenerManager : public nsIEventListenerManager {

public:
  nsEventListenerManager();
  virtual ~nsEventListenerManager();

  NS_DECL_ISUPPORTS

  nsVoidArray** GetListenersByIID(const nsIID& aIID);
  
  void ReleaseListeners(nsVoidArray* aListeners);

  /**
  * Retrieves events listeners of all types. 
  * @param
  */

  virtual nsresult GetEventListeners(nsVoidArray **aListeners, const nsIID& aIID);

  /**
  * Sets events listeners of all types. 
  * @param an event listener
  */

  virtual nsresult AddEventListener(nsIDOMEventListener *aListener, REFNSIID aIID);
  virtual nsresult AddScriptEventListener(nsIScriptContext* aContext, 
                                          nsIScriptObjectOwner *aScriptObjectOwner, 
                                          nsIAtom *aName, 
                                          const nsString& aFunc, 
                                          const nsIID& aIID);
  virtual nsresult RegisterScriptEventListener(nsIScriptContext *aContext, 
                                               nsIScriptObjectOwner *aScriptObjectOwner, 
                                               const nsIID& aIID);

  virtual nsresult RemoveEventListener(nsIDOMEventListener *aListener, REFNSIID aIID);

  virtual nsresult CaptureEvent(nsIDOMEventListener *aListener);
  virtual nsresult ReleaseEvent(nsIDOMEventListener *aListener);

  virtual nsresult HandleEvent(nsIPresContext& aPresContext, 
                               nsEvent* aEvent, 
                               nsIDOMEvent** aDOMEvent, 
                               nsEventStatus& aEventStatus);

  virtual nsresult CreateEvent(nsIPresContext& aPresContext, 
                               nsEvent* aEvent, 
                               nsIDOMEvent** aDOMEvent);

protected:
  nsresult SetJSEventListener(nsIScriptContext *aContext, JSObject *aObject, REFNSIID aIID);

  nsVoidArray* mEventListeners;
  nsVoidArray* mMouseListeners;
  nsVoidArray* mMouseMotionListeners;
  nsVoidArray* mKeyListeners;
  nsVoidArray* mLoadListeners;
  nsVoidArray* mFocusListeners;
  nsVoidArray* mFormListeners;
  nsVoidArray* mDragListeners;
  nsVoidArray* mPaintListeners;
  nsVoidArray* mTextListeners;

};

#endif // nsEventListenerManager_h__
