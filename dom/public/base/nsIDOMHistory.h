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

#ifndef nsIDOMHistory_h__
#define nsIDOMHistory_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMHISTORY_IID \
 { 0x896d1d20, 0xb4c4, 0x11d2, \
  { 0xbd, 0x93, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4 } } 

class nsIDOMHistory : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHISTORY_IID; return iid; }

  NS_IMETHOD    GetLength(PRInt32* aLength)=0;

  NS_IMETHOD    GetCurrent(nsString& aCurrent)=0;

  NS_IMETHOD    GetPrevious(nsString& aPrevious)=0;

  NS_IMETHOD    GetNext(nsString& aNext)=0;

  NS_IMETHOD    Back()=0;

  NS_IMETHOD    Forward()=0;

  NS_IMETHOD    Go(PRInt32 aIndex)=0;
};


#define NS_DECL_IDOMHISTORY   \
  NS_IMETHOD    GetLength(PRInt32* aLength);  \
  NS_IMETHOD    GetCurrent(nsString& aCurrent);  \
  NS_IMETHOD    GetPrevious(nsString& aPrevious);  \
  NS_IMETHOD    GetNext(nsString& aNext);  \
  NS_IMETHOD    Back();  \
  NS_IMETHOD    Forward();  \
  NS_IMETHOD    Go(PRInt32 aIndex);  \



#define NS_FORWARD_IDOMHISTORY(_to)  \
  NS_IMETHOD    GetLength(PRInt32* aLength) { return _to GetLength(aLength); } \
  NS_IMETHOD    GetCurrent(nsString& aCurrent) { return _to GetCurrent(aCurrent); } \
  NS_IMETHOD    GetPrevious(nsString& aPrevious) { return _to GetPrevious(aPrevious); } \
  NS_IMETHOD    GetNext(nsString& aNext) { return _to GetNext(aNext); } \
  NS_IMETHOD    Back() { return _to Back(); }  \
  NS_IMETHOD    Forward() { return _to Forward(); }  \
  NS_IMETHOD    Go(PRInt32 aIndex) { return _to Go(aIndex); }  \


extern "C" NS_DOM nsresult NS_InitHistoryClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHistory(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHistory_h__
