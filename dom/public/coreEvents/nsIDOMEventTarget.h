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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMEventTarget_h__
#define nsIDOMEventTarget_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMEventListener;

#define NS_IDOMEVENTTARGET_IID \
 { 0x1c773b30, 0xd1cf, 0x11d2, \
  { 0xbd, 0x95, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4 } } 

class nsIDOMEventTarget : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMEVENTTARGET_IID; return iid; }

  NS_IMETHOD    AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, PRBool aPostProcess, PRBool aUseCapture)=0;

  NS_IMETHOD    RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, PRBool aPostProcess, PRBool aUseCapture)=0;
};


#define NS_DECL_IDOMEVENTTARGET   \
  NS_IMETHOD    AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, PRBool aPostProcess, PRBool aUseCapture);  \
  NS_IMETHOD    RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, PRBool aPostProcess, PRBool aUseCapture);  \



#define NS_FORWARD_IDOMEVENTTARGET(_to)  \
  NS_IMETHOD    AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, PRBool aPostProcess, PRBool aUseCapture) { return _to AddEventListener(aType, aListener, aPostProcess, aUseCapture); }  \
  NS_IMETHOD    RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, PRBool aPostProcess, PRBool aUseCapture) { return _to RemoveEventListener(aType, aListener, aPostProcess, aUseCapture); }  \


#endif // nsIDOMEventTarget_h__
