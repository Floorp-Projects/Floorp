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

#ifndef nsDOMEvent_h__
#define nsDOMEvent_h__

#include "nsIDOMEvent.h"
#include "nsISupports.h"
#include "nsIPresContext.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
class nsIContent;

class nsDOMEvent : public nsIDOMEvent, public nsIDOMNSEvent {

public:

  enum nsEventStatus {  
    nsEventStatus_eIgnore, // The event is ignored, do default processing           
    nsEventStatus_eConsumeNoDefault, // The event is consumed, don't do default processing
    nsEventStatus_eConsumeDoDefault // The event is consumed, but do default processing
  };

  nsDOMEvent(nsIPresContext* aPresContext);
  virtual ~nsDOMEvent();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  // nsIDOMEventInterface
  NS_IMETHOD    GetType(nsString& aType);
  NS_IMETHOD    SetType(const nsString& aType);

  NS_IMETHOD    GetTarget(nsIDOMNode** aTarget);
  NS_IMETHOD    SetTarget(nsIDOMNode* aTarget);

  NS_IMETHOD    GetScreenX(PRInt32* aScreenX);
  NS_IMETHOD    SetScreenX(PRInt32 aScreenX);

  NS_IMETHOD    GetScreenY(PRInt32* aScreenY);
  NS_IMETHOD    SetScreenY(PRInt32 aScreenY);

  NS_IMETHOD    GetClientX(PRInt32* aClientX);
  NS_IMETHOD    SetClientX(PRInt32 aClientX);

  NS_IMETHOD    GetClientY(PRInt32* aClientY);
  NS_IMETHOD    SetClientY(PRInt32 aClientY);

  NS_IMETHOD    GetAltKey(PRBool* aAltKey);
  NS_IMETHOD    SetAltKey(PRBool aAltKey);

  NS_IMETHOD    GetCtrlKey(PRBool* aCtrlKey);
  NS_IMETHOD    SetCtrlKey(PRBool aCtrlKey);

  NS_IMETHOD    GetShiftKey(PRBool* aShiftKey);
  NS_IMETHOD    SetShiftKey(PRBool aShiftKey);

  NS_IMETHOD    GetMetaKey(PRBool* aMetaKey);
  NS_IMETHOD    SetMetaKey(PRBool aMetaKey);

  NS_IMETHOD    GetCharCode(PRUint32* aCharCode);
  NS_IMETHOD    SetCharCode(PRUint32 aCharCode);

  NS_IMETHOD    GetKeyCode(PRUint32* aKeyCode);
  NS_IMETHOD    SetKeyCode(PRUint32 aKeyCode);

  NS_IMETHOD    GetButton(PRUint32* aButton);
  NS_IMETHOD    SetButton(PRUint32 aButton);

  // nsINSEventInterface
  NS_IMETHOD    GetLayerX(PRInt32* aLayerX);
  NS_IMETHOD    SetLayerX(PRInt32 aLayerX);

  NS_IMETHOD    GetLayerY(PRInt32* aLayerY);
  NS_IMETHOD    SetLayerY(PRInt32 aLayerY);

  // Local functions
  NS_IMETHOD SetGUIEvent(nsGUIEvent *aEvent);
  NS_IMETHOD SetEventTarget(nsISupports *aTarget);

protected:

  PRUint32 mRefCnt : 31;
  nsGUIEvent *kEvent;
  nsISupports *kTarget;
  nsIPresContext *kPresContext;

  const char* GetEventName(PRUint32 aEventType);

};
#endif // nsDOMEvent_h__
