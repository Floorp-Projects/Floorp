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

#ifndef nsIDOMCSSImportRule_h__
#define nsIDOMCSSImportRule_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMCSSRule.h"

class nsIDOMCSSStyleSheet;

#define NS_IDOMCSSIMPORTRULE_IID \
 { 0xa6cf90cf, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} } 

class nsIDOMCSSImportRule : public nsIDOMCSSRule {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCSSIMPORTRULE_IID; return iid; }

  NS_IMETHOD    GetHref(nsString& aHref)=0;
  NS_IMETHOD    SetHref(const nsString& aHref)=0;

  NS_IMETHOD    GetMedia(nsString& aMedia)=0;
  NS_IMETHOD    SetMedia(const nsString& aMedia)=0;

  NS_IMETHOD    GetStyleSheet(nsIDOMCSSStyleSheet** aStyleSheet)=0;
};


#define NS_DECL_IDOMCSSIMPORTRULE   \
  NS_IMETHOD    GetHref(nsString& aHref);  \
  NS_IMETHOD    SetHref(const nsString& aHref);  \
  NS_IMETHOD    GetMedia(nsString& aMedia);  \
  NS_IMETHOD    SetMedia(const nsString& aMedia);  \
  NS_IMETHOD    GetStyleSheet(nsIDOMCSSStyleSheet** aStyleSheet);  \



#define NS_FORWARD_IDOMCSSIMPORTRULE(_to)  \
  NS_IMETHOD    GetHref(nsString& aHref) { return _to GetHref(aHref); } \
  NS_IMETHOD    SetHref(const nsString& aHref) { return _to SetHref(aHref); } \
  NS_IMETHOD    GetMedia(nsString& aMedia) { return _to GetMedia(aMedia); } \
  NS_IMETHOD    SetMedia(const nsString& aMedia) { return _to SetMedia(aMedia); } \
  NS_IMETHOD    GetStyleSheet(nsIDOMCSSStyleSheet** aStyleSheet) { return _to GetStyleSheet(aStyleSheet); } \


extern "C" NS_DOM nsresult NS_InitCSSImportRuleClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCSSImportRule(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCSSImportRule_h__
