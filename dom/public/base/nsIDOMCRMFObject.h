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

#ifndef nsIDOMCRMFObject_h__
#define nsIDOMCRMFObject_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMCRMFOBJECT_IID \
 {0x16da46c0, 0x208d, 0x11d4, \
       {0x8a, 0x7c, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3}} 

class nsIDOMCRMFObject : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCRMFOBJECT_IID; return iid; }

  NS_IMETHOD    GetRequest(nsAWritableString& aRequest)=0;
};


#define NS_DECL_IDOMCRMFOBJECT   \
  NS_IMETHOD    GetRequest(nsAWritableString& aRequest);  \



#define NS_FORWARD_IDOMCRMFOBJECT(_to)  \
  NS_IMETHOD    GetRequest(nsAWritableString& aRequest) { return _to GetRequest(aRequest); } \


extern "C" NS_DOM nsresult NS_InitCRMFObjectClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCRMFObject(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCRMFObject_h__
