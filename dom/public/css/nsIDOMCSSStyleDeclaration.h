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

#ifndef nsIDOMCSSStyleDeclaration_h__
#define nsIDOMCSSStyleDeclaration_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMCSSRule;
class nsIDOMCSSValue;

#define NS_IDOMCSSSTYLEDECLARATION_IID \
 { 0xa6cf90be, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMCSSStyleDeclaration : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCSSSTYLEDECLARATION_IID; return iid; }

  NS_IMETHOD    GetCssText(nsAWritableString& aCssText)=0;
  NS_IMETHOD    SetCssText(const nsAReadableString& aCssText)=0;

  NS_IMETHOD    GetLength(PRUint32* aLength)=0;

  NS_IMETHOD    GetParentRule(nsIDOMCSSRule** aParentRule)=0;

  NS_IMETHOD    GetPropertyValue(const nsAReadableString& aPropertyName, nsAWritableString& aReturn)=0;

  NS_IMETHOD    GetPropertyCSSValue(const nsAReadableString& aPropertyName, nsIDOMCSSValue** aReturn)=0;

  NS_IMETHOD    RemoveProperty(const nsAReadableString& aPropertyName, nsAWritableString& aReturn)=0;

  NS_IMETHOD    GetPropertyPriority(const nsAReadableString& aPropertyName, nsAWritableString& aReturn)=0;

  NS_IMETHOD    SetProperty(const nsAReadableString& aPropertyName, const nsAReadableString& aValue, const nsAReadableString& aPriority)=0;

  NS_IMETHOD    Item(PRUint32 aIndex, nsAWritableString& aReturn)=0;
};


#define NS_DECL_IDOMCSSSTYLEDECLARATION   \
  NS_IMETHOD    GetCssText(nsAWritableString& aCssText);  \
  NS_IMETHOD    SetCssText(const nsAReadableString& aCssText);  \
  NS_IMETHOD    GetLength(PRUint32* aLength);  \
  NS_IMETHOD    GetParentRule(nsIDOMCSSRule** aParentRule);  \
  NS_IMETHOD    GetPropertyValue(const nsAReadableString& aPropertyName, nsAWritableString& aReturn);  \
  NS_IMETHOD    GetPropertyCSSValue(const nsAReadableString& aPropertyName, nsIDOMCSSValue** aReturn);  \
  NS_IMETHOD    RemoveProperty(const nsAReadableString& aPropertyName, nsAWritableString& aReturn);  \
  NS_IMETHOD    GetPropertyPriority(const nsAReadableString& aPropertyName, nsAWritableString& aReturn);  \
  NS_IMETHOD    SetProperty(const nsAReadableString& aPropertyName, const nsAReadableString& aValue, const nsAReadableString& aPriority);  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsAWritableString& aReturn);  \



#define NS_FORWARD_IDOMCSSSTYLEDECLARATION(_to)  \
  NS_IMETHOD    GetCssText(nsAWritableString& aCssText) { return _to GetCssText(aCssText); } \
  NS_IMETHOD    SetCssText(const nsAReadableString& aCssText) { return _to SetCssText(aCssText); } \
  NS_IMETHOD    GetLength(PRUint32* aLength) { return _to GetLength(aLength); } \
  NS_IMETHOD    GetParentRule(nsIDOMCSSRule** aParentRule) { return _to GetParentRule(aParentRule); } \
  NS_IMETHOD    GetPropertyValue(const nsAReadableString& aPropertyName, nsAWritableString& aReturn) { return _to GetPropertyValue(aPropertyName, aReturn); }  \
  NS_IMETHOD    GetPropertyCSSValue(const nsAReadableString& aPropertyName, nsIDOMCSSValue** aReturn) { return _to GetPropertyCSSValue(aPropertyName, aReturn); }  \
  NS_IMETHOD    RemoveProperty(const nsAReadableString& aPropertyName, nsAWritableString& aReturn) { return _to RemoveProperty(aPropertyName, aReturn); }  \
  NS_IMETHOD    GetPropertyPriority(const nsAReadableString& aPropertyName, nsAWritableString& aReturn) { return _to GetPropertyPriority(aPropertyName, aReturn); }  \
  NS_IMETHOD    SetProperty(const nsAReadableString& aPropertyName, const nsAReadableString& aValue, const nsAReadableString& aPriority) { return _to SetProperty(aPropertyName, aValue, aPriority); }  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsAWritableString& aReturn) { return _to Item(aIndex, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitCSSStyleDeclarationClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCSSStyleDeclaration(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCSSStyleDeclaration_h__
