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

#ifndef nsIDOMCSSStyleRule_h__
#define nsIDOMCSSStyleRule_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMCSSRule.h"

class nsIDOMCSSStyleDeclaration;

#define NS_IDOMCSSSTYLERULE_IID \
 { 0xa6cf90bf, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMCSSStyleRule : public nsIDOMCSSRule {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCSSSTYLERULE_IID; return iid; }

  NS_IMETHOD    GetSelectorText(nsString& aSelectorText)=0;
  NS_IMETHOD    SetSelectorText(const nsString& aSelectorText)=0;

  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle)=0;
  NS_IMETHOD    SetStyle(nsIDOMCSSStyleDeclaration* aStyle)=0;
};


#define NS_DECL_IDOMCSSSTYLERULE   \
  NS_IMETHOD    GetSelectorText(nsString& aSelectorText);  \
  NS_IMETHOD    SetSelectorText(const nsString& aSelectorText);  \
  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle);  \
  NS_IMETHOD    SetStyle(nsIDOMCSSStyleDeclaration* aStyle);  \



#define NS_FORWARD_IDOMCSSSTYLERULE(_to)  \
  NS_IMETHOD    GetSelectorText(nsString& aSelectorText) { return _to GetSelectorText(aSelectorText); } \
  NS_IMETHOD    SetSelectorText(const nsString& aSelectorText) { return _to SetSelectorText(aSelectorText); } \
  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle) { return _to GetStyle(aStyle); } \
  NS_IMETHOD    SetStyle(nsIDOMCSSStyleDeclaration* aStyle) { return _to SetStyle(aStyle); } \


extern "C" NS_DOM nsresult NS_InitCSSStyleRuleClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCSSStyleRule(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCSSStyleRule_h__
