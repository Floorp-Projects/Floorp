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
class nsIDOMEvent;
 
/*
 * Event listener manager
 */

class nsEventListenerManager : public nsIEventListenerManager {

public:
  nsEventListenerManager();
  virtual ~nsEventListenerManager();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  /**
  * Retrieves events listeners of all types. 
  * @param
  */

  virtual nsresult GetEventListeners(nsVoidArray *aListeners, const nsIID& aIID);

  /**
  * Sets events listeners of all types. 
  * @param an event listener
  */

  virtual nsresult AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID);

  virtual nsresult RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID);

  virtual nsresult CaptureEvent(nsIDOMEventListener *aListener);
  virtual nsresult ReleaseEvent(nsIDOMEventListener *aListener);

  virtual nsresult HandleEvent(nsIPresContext& aPresContext, nsGUIEvent* aEvent, nsIDOMEvent* aDOMEvent, nsEventStatus& aEventStatus);

protected:

  PRUint32 mRefCnt : 31;

  virtual nsresult nsEventListenerManager::TranslateGUI2DOM(nsGUIEvent*& aGUIEvent, 
                                                  nsIPresContext& aPresContext, 
                                                  nsIDOMEvent** aDOMEvent);

  nsVoidArray* mEventListeners;
  nsVoidArray* mMouseListeners;
  nsVoidArray* mMouseMotionListeners;
  nsVoidArray* mKeyListeners;
  nsVoidArray* mLoadListeners;
  nsVoidArray* mFocusListeners;
  nsVoidArray* mDragListeners;

};

#endif // nsEventListenerManager_h__
