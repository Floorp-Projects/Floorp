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
#include "nsINSEvent.h"
#include "nsISupports.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
class nsIContent;

class nsDOMEvent : public nsIDOMEvent, public nsINSEvent {

public:

  enum nsEventStatus {  
    nsEventStatus_eIgnore, // The event is ignored, do default processing           
    nsEventStatus_eConsumeNoDefault, // The event is consumed, don't do default processing
    nsEventStatus_eConsumeDoDefault // The event is consumed, but do default processing
  };

  nsDOMEvent();
  virtual ~nsDOMEvent();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  // nsIDOMEventInterface
  NS_IMETHOD GetType(nsString& aType);

  NS_IMETHOD GetTarget(nsIDOMNode** aTarget);

  NS_IMETHOD GetScreenX(PRInt32& aX);
  NS_IMETHOD GetScreenY(PRInt32& aY);

  NS_IMETHOD GetClientX(PRInt32& aX);
  NS_IMETHOD GetClientY(PRInt32& aY);

  NS_IMETHOD GetAltKey(PRBool& aIsDown);
  NS_IMETHOD GetCtrlKey(PRBool& aIsDown);
  NS_IMETHOD GetShiftKey(PRBool& aIsDown);
  NS_IMETHOD GetMetaKey(PRBool& aIsDown);

  NS_IMETHOD GetCharCode(PRUint32& aCharCode);
  NS_IMETHOD GetKeyCode(PRUint32& aKeyCode);
  NS_IMETHOD GetButton(PRUint32& aButton);

  // nsINSEventInterface
  NS_IMETHOD GetLayerX(PRInt32& aX);
  NS_IMETHOD GetLayerY(PRInt32& aY);

  NS_IMETHOD SetGUIEvent(nsGUIEvent *aEvent);
  NS_IMETHOD SetEventTarget(nsISupports *aTarget);

protected:

  PRUint32 mRefCnt : 31;
  nsGUIEvent *kEvent;
  nsISupports *kTarget;

};
#endif // nsDOMEvent_h__