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

#ifndef nsIDOMHistory_h__
#define nsIDOMHistory_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "jsapi.h"


#define NS_IDOMHISTORY_IID \
 { 0x896d1d20, 0xb4c4, 0x11d2, \
  { 0xbd, 0x93, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4 } } 

class nsIDOMHistory : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHISTORY_IID; return iid; }

  NS_IMETHOD    GetLength(PRInt32* aLength)=0;

  NS_IMETHOD    GetCurrent(nsAWritableString& aCurrent)=0;

  NS_IMETHOD    GetPrevious(nsAWritableString& aPrevious)=0;

  NS_IMETHOD    GetNext(nsAWritableString& aNext)=0;

  NS_IMETHOD    Back()=0;

  NS_IMETHOD    Forward()=0;

  NS_IMETHOD    Go(JSContext* cx, jsval* argv, PRUint32 argc)=0;

  NS_IMETHOD    Item(PRUint32 aIndex, nsAWritableString& aReturn)=0;
};


#define NS_DECL_IDOMHISTORY   \
  NS_IMETHOD    GetLength(PRInt32* aLength);  \
  NS_IMETHOD    GetCurrent(nsAWritableString& aCurrent);  \
  NS_IMETHOD    GetPrevious(nsAWritableString& aPrevious);  \
  NS_IMETHOD    GetNext(nsAWritableString& aNext);  \
  NS_IMETHOD    Back();  \
  NS_IMETHOD    Forward();  \
  NS_IMETHOD    Go(JSContext* cx, jsval* argv, PRUint32 argc);  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsAWritableString& aReturn);  \



#define NS_FORWARD_IDOMHISTORY(_to)  \
  NS_IMETHOD    GetLength(PRInt32* aLength) { return _to GetLength(aLength); } \
  NS_IMETHOD    GetCurrent(nsAWritableString& aCurrent) { return _to GetCurrent(aCurrent); } \
  NS_IMETHOD    GetPrevious(nsAWritableString& aPrevious) { return _to GetPrevious(aPrevious); } \
  NS_IMETHOD    GetNext(nsAWritableString& aNext) { return _to GetNext(aNext); } \
  NS_IMETHOD    Back() { return _to Back(); }  \
  NS_IMETHOD    Forward() { return _to Forward(); }  \
  NS_IMETHOD    Go(JSContext* cx, jsval* argv, PRUint32 argc) { return _to Go(cx, argv, argc); }  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsAWritableString& aReturn) { return _to Item(aIndex, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitHistoryClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHistory(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHistory_h__
