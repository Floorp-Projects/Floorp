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

#ifndef nsIDOMHTMLFormElement_h__
#define nsIDOMHTMLFormElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLCollection;

#define NS_IDOMHTMLFORMELEMENT_IID \
{ 0x6f765301,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLFormElement : public nsIDOMHTMLElement {
public:

  NS_IMETHOD    GetElements(nsIDOMHTMLCollection** aElements)=0;

  NS_IMETHOD    GetName(nsString& aName)=0;

  NS_IMETHOD    GetAcceptCharset(nsString& aAcceptCharset)=0;
  NS_IMETHOD    SetAcceptCharset(const nsString& aAcceptCharset)=0;

  NS_IMETHOD    GetAction(nsString& aAction)=0;
  NS_IMETHOD    SetAction(const nsString& aAction)=0;

  NS_IMETHOD    GetEnctype(nsString& aEnctype)=0;
  NS_IMETHOD    SetEnctype(const nsString& aEnctype)=0;

  NS_IMETHOD    GetMethod(nsString& aMethod)=0;
  NS_IMETHOD    SetMethod(const nsString& aMethod)=0;

  NS_IMETHOD    GetTarget(nsString& aTarget)=0;
  NS_IMETHOD    SetTarget(const nsString& aTarget)=0;

  NS_IMETHOD    Reset()=0;

  NS_IMETHOD    Submit()=0;
};


#define NS_DECL_IDOMHTMLFORMELEMENT   \
  NS_IMETHOD    GetElements(nsIDOMHTMLCollection** aElements);  \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    GetAcceptCharset(nsString& aAcceptCharset);  \
  NS_IMETHOD    SetAcceptCharset(const nsString& aAcceptCharset);  \
  NS_IMETHOD    GetAction(nsString& aAction);  \
  NS_IMETHOD    SetAction(const nsString& aAction);  \
  NS_IMETHOD    GetEnctype(nsString& aEnctype);  \
  NS_IMETHOD    SetEnctype(const nsString& aEnctype);  \
  NS_IMETHOD    GetMethod(nsString& aMethod);  \
  NS_IMETHOD    SetMethod(const nsString& aMethod);  \
  NS_IMETHOD    GetTarget(nsString& aTarget);  \
  NS_IMETHOD    SetTarget(const nsString& aTarget);  \
  NS_IMETHOD    Reset();  \
  NS_IMETHOD    Submit();  \



#define NS_FORWARD_IDOMHTMLFORMELEMENT(_to)  \
  NS_IMETHOD    GetElements(nsIDOMHTMLCollection** aElements) { return _to##GetElements(aElements); } \
  NS_IMETHOD    GetName(nsString& aName) { return _to##GetName(aName); } \
  NS_IMETHOD    GetAcceptCharset(nsString& aAcceptCharset) { return _to##GetAcceptCharset(aAcceptCharset); } \
  NS_IMETHOD    SetAcceptCharset(const nsString& aAcceptCharset) { return _to##SetAcceptCharset(aAcceptCharset); } \
  NS_IMETHOD    GetAction(nsString& aAction) { return _to##GetAction(aAction); } \
  NS_IMETHOD    SetAction(const nsString& aAction) { return _to##SetAction(aAction); } \
  NS_IMETHOD    GetEnctype(nsString& aEnctype) { return _to##GetEnctype(aEnctype); } \
  NS_IMETHOD    SetEnctype(const nsString& aEnctype) { return _to##SetEnctype(aEnctype); } \
  NS_IMETHOD    GetMethod(nsString& aMethod) { return _to##GetMethod(aMethod); } \
  NS_IMETHOD    SetMethod(const nsString& aMethod) { return _to##SetMethod(aMethod); } \
  NS_IMETHOD    GetTarget(nsString& aTarget) { return _to##GetTarget(aTarget); } \
  NS_IMETHOD    SetTarget(const nsString& aTarget) { return _to##SetTarget(aTarget); } \
  NS_IMETHOD    Reset() { return _to##Reset(); }  \
  NS_IMETHOD    Submit() { return _to##Submit(); }  \


extern nsresult NS_InitHTMLFormElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLFormElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLFormElement_h__
