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

#ifndef nsIDOMCSSStyleSheet_h__
#define nsIDOMCSSStyleSheet_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMStyleSheet.h"

class nsIDOMCSSRule;
class nsIDOMCSSRuleList;

#define NS_IDOMCSSSTYLESHEET_IID \
 { 0xa6cf90c2, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMCSSStyleSheet : public nsIDOMStyleSheet {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCSSSTYLESHEET_IID; return iid; }

  NS_IMETHOD    GetOwnerRule(nsIDOMCSSRule** aOwnerRule)=0;

  NS_IMETHOD    GetCssRules(nsIDOMCSSRuleList** aCssRules)=0;

  NS_IMETHOD    InsertRule(const nsAReadableString& aRule, PRUint32 aIndex, PRUint32* aReturn)=0;

  NS_IMETHOD    DeleteRule(PRUint32 aIndex)=0;
};


#define NS_DECL_IDOMCSSSTYLESHEET   \
  NS_IMETHOD    GetOwnerRule(nsIDOMCSSRule** aOwnerRule);  \
  NS_IMETHOD    GetCssRules(nsIDOMCSSRuleList** aCssRules);  \
  NS_IMETHOD    InsertRule(const nsAReadableString& aRule, PRUint32 aIndex, PRUint32* aReturn);  \
  NS_IMETHOD    DeleteRule(PRUint32 aIndex);  \



#define NS_FORWARD_IDOMCSSSTYLESHEET(_to)  \
  NS_IMETHOD    GetOwnerRule(nsIDOMCSSRule** aOwnerRule) { return _to GetOwnerRule(aOwnerRule); } \
  NS_IMETHOD    GetCssRules(nsIDOMCSSRuleList** aCssRules) { return _to GetCssRules(aCssRules); } \
  NS_IMETHOD    InsertRule(const nsAReadableString& aRule, PRUint32 aIndex, PRUint32* aReturn) { return _to InsertRule(aRule, aIndex, aReturn); }  \
  NS_IMETHOD    DeleteRule(PRUint32 aIndex) { return _to DeleteRule(aIndex); }  \


extern "C" NS_DOM nsresult NS_InitCSSStyleSheetClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCSSStyleSheet(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCSSStyleSheet_h__
