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
{ 0x6f7652f7,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLBRElement : public nsIDOMHTMLElement {
public:

  NS_IMETHOD    GetClear(nsString& aClear)=0;
  NS_IMETHOD    SetClear(const nsString& aClear)=0;
};


#define NS_DECL_IDOMHTMLBRELEMENT   \
  NS_IMETHOD    GetClear(nsString& aClear);  \
  NS_IMETHOD    SetClear(const nsString& aClear);  \



#define NS_FORWARD_IDOMHTMLBRELEMENT(_to)  \
  NS_IMETHOD    GetClear(nsString& aClear) { return _to##GetClear(aClear); } \
  NS_IMETHOD    SetClear(const nsString& aClear) { return _to##SetClear(aClear); } \


extern nsresult NS_InitHTMLBRElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLBRElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLBRElement_h__
