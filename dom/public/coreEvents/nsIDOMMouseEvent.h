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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMMouseEvent_h__
#define nsIDOMMouseEvent_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMUIEvent.h"

class nsIDOMEventTarget;

#define NS_IDOMMOUSEEVENT_IID \
 { 0xff751edc, 0x8b02, 0xaae7, \
  { 0x00, 0x10, 0x83, 0x01, 0x83, 0x8a, 0x31, 0x23 } } 

class nsIDOMMouseEvent : public nsIDOMUIEvent {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMMOUSEEVENT_IID; return iid; }

  NS_IMETHOD    GetScreenX(PRInt32* aScreenX)=0;

  NS_IMETHOD    GetScreenY(PRInt32* aScreenY)=0;

  NS_IMETHOD    GetClientX(PRInt32* aClientX)=0;

  NS_IMETHOD    GetClientY(PRInt32* aClientY)=0;

  NS_IMETHOD    GetAltKey(PRBool* aAltKey)=0;

  NS_IMETHOD    GetCtrlKey(PRBool* aCtrlKey)=0;

  NS_IMETHOD    GetShiftKey(PRBool* aShiftKey)=0;

  NS_IMETHOD    GetMetaKey(PRBool* aMetaKey)=0;

  NS_IMETHOD    GetButton(PRUint16* aButton)=0;

  NS_IMETHOD    GetRelatedTarget(nsIDOMEventTarget** aRelatedTarget)=0;

  NS_IMETHOD    InitMouseEvent(const nsAReadableString& aTypeArg, PRBool aCtrlKeyArg, PRBool aAltKeyArg, PRBool aShiftKeyArg, PRBool aMetaKeyArg, PRInt32 aScreenXArg, PRInt32 aScreenYArg, PRInt32 aClientXArg, PRInt32 aClientYArg, PRUint16 aButtonArg, PRUint16 aDetailArg)=0;
};


#define NS_DECL_IDOMMOUSEEVENT   \
  NS_IMETHOD    GetScreenX(PRInt32* aScreenX);  \
  NS_IMETHOD    GetScreenY(PRInt32* aScreenY);  \
  NS_IMETHOD    GetClientX(PRInt32* aClientX);  \
  NS_IMETHOD    GetClientY(PRInt32* aClientY);  \
  NS_IMETHOD    GetAltKey(PRBool* aAltKey);  \
  NS_IMETHOD    GetCtrlKey(PRBool* aCtrlKey);  \
  NS_IMETHOD    GetShiftKey(PRBool* aShiftKey);  \
  NS_IMETHOD    GetMetaKey(PRBool* aMetaKey);  \
  NS_IMETHOD    GetButton(PRUint16* aButton);  \
  NS_IMETHOD    GetRelatedTarget(nsIDOMEventTarget** aRelatedTarget);  \
  NS_IMETHOD    InitMouseEvent(const nsAReadableString& aTypeArg, PRBool aCtrlKeyArg, PRBool aAltKeyArg, PRBool aShiftKeyArg, PRBool aMetaKeyArg, PRInt32 aScreenXArg, PRInt32 aScreenYArg, PRInt32 aClientXArg, PRInt32 aClientYArg, PRUint16 aButtonArg, PRUint16 aDetailArg);  \



#define NS_FORWARD_IDOMMOUSEEVENT(_to)  \
  NS_IMETHOD    GetScreenX(PRInt32* aScreenX) { return _to GetScreenX(aScreenX); } \
  NS_IMETHOD    GetScreenY(PRInt32* aScreenY) { return _to GetScreenY(aScreenY); } \
  NS_IMETHOD    GetClientX(PRInt32* aClientX) { return _to GetClientX(aClientX); } \
  NS_IMETHOD    GetClientY(PRInt32* aClientY) { return _to GetClientY(aClientY); } \
  NS_IMETHOD    GetAltKey(PRBool* aAltKey) { return _to GetAltKey(aAltKey); } \
  NS_IMETHOD    GetCtrlKey(PRBool* aCtrlKey) { return _to GetCtrlKey(aCtrlKey); } \
  NS_IMETHOD    GetShiftKey(PRBool* aShiftKey) { return _to GetShiftKey(aShiftKey); } \
  NS_IMETHOD    GetMetaKey(PRBool* aMetaKey) { return _to GetMetaKey(aMetaKey); } \
  NS_IMETHOD    GetButton(PRUint16* aButton) { return _to GetButton(aButton); } \
  NS_IMETHOD    GetRelatedTarget(nsIDOMEventTarget** aRelatedTarget) { return _to GetRelatedTarget(aRelatedTarget); } \
  NS_IMETHOD    InitMouseEvent(const nsAReadableString& aTypeArg, PRBool aCtrlKeyArg, PRBool aAltKeyArg, PRBool aShiftKeyArg, PRBool aMetaKeyArg, PRInt32 aScreenXArg, PRInt32 aScreenYArg, PRInt32 aClientXArg, PRInt32 aClientYArg, PRUint16 aButtonArg, PRUint16 aDetailArg) { return _to InitMouseEvent(aTypeArg, aCtrlKeyArg, aAltKeyArg, aShiftKeyArg, aMetaKeyArg, aScreenXArg, aScreenYArg, aClientXArg, aClientYArg, aButtonArg, aDetailArg); }  \


#endif // nsIDOMMouseEvent_h__
