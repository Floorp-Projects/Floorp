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

#ifndef nsIDOMCSSValue_h__
#define nsIDOMCSSValue_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMCSSVALUE_IID \
 { 0x009f7ea5, 0x9e80, 0x41be, \
  { 0xb0, 0x08, 0xdb, 0x62, 0xf1, 0x08, 0x23, 0xf2 } } 

class nsIDOMCSSValue : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCSSVALUE_IID; return iid; }
  enum {
    CSS_INHERIT = 0,
    CSS_PRIMITIVE_VALUE = 1,
    CSS_VALUE_LIST = 2,
    CSS_CUSTOM = 3
  };

  NS_IMETHOD    GetCssText(nsAWritableString& aCssText)=0;
  NS_IMETHOD    SetCssText(const nsAReadableString& aCssText)=0;

  NS_IMETHOD    GetCssValueType(PRUint16* aCssValueType)=0;
};


#define NS_DECL_IDOMCSSVALUE   \
  NS_IMETHOD    GetCssText(nsAWritableString& aCssText);  \
  NS_IMETHOD    SetCssText(const nsAReadableString& aCssText);  \
  NS_IMETHOD    GetCssValueType(PRUint16* aCssValueType);  \



#define NS_FORWARD_IDOMCSSVALUE(_to)  \
  NS_IMETHOD    GetCssText(nsAWritableString& aCssText) { return _to GetCssText(aCssText); } \
  NS_IMETHOD    SetCssText(const nsAReadableString& aCssText) { return _to SetCssText(aCssText); } \
  NS_IMETHOD    GetCssValueType(PRUint16* aCssValueType) { return _to GetCssValueType(aCssValueType); } \


extern "C" NS_DOM nsresult NS_InitCSSValueClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCSSValue(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCSSValue_h__
