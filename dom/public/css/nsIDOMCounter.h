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

#ifndef nsIDOMCounter_h__
#define nsIDOMCounter_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMCOUNTER_IID \
 { 0x31adb439, 0x0055, 0x402d, \
  { 0x9b, 0x1d, 0xd5, 0xca, 0x94, 0xf3, 0xf5, 0x5b } } 

class nsIDOMCounter : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCOUNTER_IID; return iid; }

  NS_IMETHOD    GetIdentifier(nsAWritableString& aIdentifier)=0;

  NS_IMETHOD    GetListStyle(nsAWritableString& aListStyle)=0;

  NS_IMETHOD    GetSeparator(nsAWritableString& aSeparator)=0;
};


#define NS_DECL_IDOMCOUNTER   \
  NS_IMETHOD    GetIdentifier(nsAWritableString& aIdentifier);  \
  NS_IMETHOD    GetListStyle(nsAWritableString& aListStyle);  \
  NS_IMETHOD    GetSeparator(nsAWritableString& aSeparator);  \



#define NS_FORWARD_IDOMCOUNTER(_to)  \
  NS_IMETHOD    GetIdentifier(nsAWritableString& aIdentifier) { return _to GetIdentifier(aIdentifier); } \
  NS_IMETHOD    GetListStyle(nsAWritableString& aListStyle) { return _to GetListStyle(aListStyle); } \
  NS_IMETHOD    GetSeparator(nsAWritableString& aSeparator) { return _to GetSeparator(aSeparator); } \


extern "C" NS_DOM nsresult NS_InitCounterClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCounter(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCounter_h__
