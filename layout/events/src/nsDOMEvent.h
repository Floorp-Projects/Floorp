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
#include "nsIPrivateDOMEvent.h"

#include "nsIPresContext.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
class nsIContent;

class nsDOMEvent : public nsIDOMEvent, public nsIDOMNSEvent, public nsIPrivateDOMEvent {

#define DOM_EVENT_INIT      0x0001
#define DOM_EVENT_BUBBLE    0x0002
#define DOM_EVENT_CAPTURE   0x0004

public:
  enum nsDOMEvents {
    eDOMEvents_mousedown=0, eDOMEvents_mouseup=1, eDOMEvents_click=2, eDOMEvents_dblclick=3, eDOMEvents_mouseover=4, eDOMEvents_mouseout=5,
    eDOMEvents_mousemove=6, eDOMEvents_keydown=7, eDOMEvents_keyup=8, eDOMEvents_keypress=9, eDOMEvents_focus=10, eDOMEvents_blur=11,
    eDOMEvents_load=12, eDOMEvents_unload=13, eDOMEvents_abort=14, eDOMEvents_error=15, eDOMEvents_submit, eDOMEvents_reset
  };

  nsDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent);
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

  // nsIDOMNSEvent interface
  NS_IMETHOD    GetLayerX(PRInt32* aLayerX);
  NS_IMETHOD    SetLayerX(PRInt32 aLayerX);

  NS_IMETHOD    GetLayerY(PRInt32* aLayerY);
  NS_IMETHOD    SetLayerY(PRInt32 aLayerY);

  // nsIPrivateDOMEvent interface
  NS_IMETHOD    DuplicatePrivateData();

protected:

  PRUint32 mRefCnt : 31;
  nsEvent *mEvent;
  nsIPresContext *mPresContext;

  const char* GetEventName(PRUint32 aEventType);

};

#endif // nsDOMEvent_h__
