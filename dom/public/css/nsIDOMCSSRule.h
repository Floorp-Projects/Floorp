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

#ifndef nsIDOMCSSRule_h__
#define nsIDOMCSSRule_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMCSSRule;
class nsIDOMCSSStyleSheet;

#define NS_IDOMCSSRULE_IID \
 { 0xa6cf90c1, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} } 

class nsIDOMCSSRule : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCSSRULE_IID; return iid; }
  enum {
    UNKNOWN_RULE = 0,
    STYLE_RULE = 1,
    CHARSET_RULE = 2,
    IMPORT_RULE = 3,
    MEDIA_RULE = 4,
    FONT_FACE_RULE = 5,
    PAGE_RULE = 6
  };

  NS_IMETHOD    GetType(PRUint16* aType)=0;

  NS_IMETHOD    GetCssText(nsAWritableString& aCssText)=0;
  NS_IMETHOD    SetCssText(const nsAReadableString& aCssText)=0;

  NS_IMETHOD    GetParentStyleSheet(nsIDOMCSSStyleSheet** aParentStyleSheet)=0;

  NS_IMETHOD    GetParentRule(nsIDOMCSSRule** aParentRule)=0;
};


#define NS_DECL_IDOMCSSRULE   \
  NS_IMETHOD    GetType(PRUint16* aType);  \
  NS_IMETHOD    GetCssText(nsAWritableString& aCssText);  \
  NS_IMETHOD    SetCssText(const nsAReadableString& aCssText);  \
  NS_IMETHOD    GetParentStyleSheet(nsIDOMCSSStyleSheet** aParentStyleSheet);  \
  NS_IMETHOD    GetParentRule(nsIDOMCSSRule** aParentRule);  \



#define NS_FORWARD_IDOMCSSRULE(_to)  \
  NS_IMETHOD    GetType(PRUint16* aType) { return _to GetType(aType); } \
  NS_IMETHOD    GetCssText(nsAWritableString& aCssText) { return _to GetCssText(aCssText); } \
  NS_IMETHOD    SetCssText(const nsAReadableString& aCssText) { return _to SetCssText(aCssText); } \
  NS_IMETHOD    GetParentStyleSheet(nsIDOMCSSStyleSheet** aParentStyleSheet) { return _to GetParentStyleSheet(aParentStyleSheet); } \
  NS_IMETHOD    GetParentRule(nsIDOMCSSRule** aParentRule) { return _to GetParentRule(aParentRule); } \


extern "C" NS_DOM nsresult NS_InitCSSRuleClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCSSRule(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCSSRule_h__
