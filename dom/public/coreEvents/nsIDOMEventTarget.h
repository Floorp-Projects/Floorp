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

#ifndef nsIDOMEventTarget_h__
#define nsIDOMEventTarget_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMEventListener;
class nsIDOMEvent;

#define NS_IDOMEVENTTARGET_IID \
 { 0x1c773b30, 0xd1cf, 0x11d2, \
  { 0xbd, 0x95, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4 } } 

class nsIDOMEventTarget : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMEVENTTARGET_IID; return iid; }

  NS_IMETHOD    AddEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, PRBool aUseCapture)=0;

  NS_IMETHOD    RemoveEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, PRBool aUseCapture)=0;

  NS_IMETHOD    DispatchEvent(nsIDOMEvent* aEvt)=0;
};


#define NS_DECL_IDOMEVENTTARGET   \
  NS_IMETHOD    AddEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, PRBool aUseCapture);  \
  NS_IMETHOD    RemoveEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, PRBool aUseCapture);  \
  NS_IMETHOD    DispatchEvent(nsIDOMEvent* aEvt);  \



#define NS_FORWARD_IDOMEVENTTARGET(_to)  \
  NS_IMETHOD    AddEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, PRBool aUseCapture) { return _to AddEventListener(aType, aListener, aUseCapture); }  \
  NS_IMETHOD    RemoveEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, PRBool aUseCapture) { return _to RemoveEventListener(aType, aListener, aUseCapture); }  \
  NS_IMETHOD    DispatchEvent(nsIDOMEvent* aEvt) { return _to DispatchEvent(aEvt); }  \


#endif // nsIDOMEventTarget_h__
