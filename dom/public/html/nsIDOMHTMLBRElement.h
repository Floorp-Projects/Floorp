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

#ifndef nsIDOMHTMLBRElement_h__
#define nsIDOMHTMLBRElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLBRELEMENT_IID \
 { 0xa6cf90a5, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLBRElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLBRELEMENT_IID; return iid; }

  NS_IMETHOD    GetClear(nsString& aClear)=0;
  NS_IMETHOD    SetClear(const nsString& aClear)=0;
};


#define NS_DECL_IDOMHTMLBRELEMENT   \
  NS_IMETHOD    GetClear(nsString& aClear);  \
  NS_IMETHOD    SetClear(const nsString& aClear);  \



#define NS_FORWARD_IDOMHTMLBRELEMENT(_to)  \
  NS_IMETHOD    GetClear(nsString& aClear) { return _to GetClear(aClear); } \
  NS_IMETHOD    SetClear(const nsString& aClear) { return _to SetClear(aClear); } \


extern "C" NS_DOM nsresult NS_InitHTMLBRElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLBRElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLBRElement_h__
