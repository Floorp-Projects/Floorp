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

#ifndef nsIDOMCSSStyleSheet_h__
#define nsIDOMCSSStyleSheet_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMStyleSheet.h"

class nsIDOMHTMLElement;
class nsIDOMStyleSheetCollection;
class nsIDOMCSSStyleRuleCollection;
class nsIDOMCSSStyleSheet;

#define NS_IDOMCSSSTYLESHEET_IID \
 { 0xa6cf90c2, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMCSSStyleSheet : public nsIDOMStyleSheet {
public:

  NS_IMETHOD    GetOwningElement(nsIDOMHTMLElement** aOwningElement)=0;

  NS_IMETHOD    GetParentStyleSheet(nsIDOMCSSStyleSheet** aParentStyleSheet)=0;

  NS_IMETHOD    GetHref(nsString& aHref)=0;

  NS_IMETHOD    GetTitle(nsString& aTitle)=0;

  NS_IMETHOD    GetImports(nsIDOMStyleSheetCollection** aImports)=0;

  NS_IMETHOD    GetRules(nsIDOMCSSStyleRuleCollection** aRules)=0;

  NS_IMETHOD    AddRule(const nsString& aSelector, const nsString& aDeclaration, PRUint32 aIndex, PRUint32* aReturn)=0;

  NS_IMETHOD    AddImport(const nsString& aUrl, PRUint32 aIndex, PRUint32* aReturn)=0;

  NS_IMETHOD    RemoveRule(PRUint32 aIndex)=0;

  NS_IMETHOD    RemoveImport(PRUint32 aIndex)=0;
};


#define NS_DECL_IDOMCSSSTYLESHEET   \
  NS_IMETHOD    GetOwningElement(nsIDOMHTMLElement** aOwningElement);  \
  NS_IMETHOD    GetParentStyleSheet(nsIDOMCSSStyleSheet** aParentStyleSheet);  \
  NS_IMETHOD    GetHref(nsString& aHref);  \
  NS_IMETHOD    GetTitle(nsString& aTitle);  \
  NS_IMETHOD    GetImports(nsIDOMStyleSheetCollection** aImports);  \
  NS_IMETHOD    GetRules(nsIDOMCSSStyleRuleCollection** aRules);  \
  NS_IMETHOD    AddRule(const nsString& aSelector, const nsString& aDeclaration, PRUint32 aIndex, PRUint32* aReturn);  \
  NS_IMETHOD    AddImport(const nsString& aUrl, PRUint32 aIndex, PRUint32* aReturn);  \
  NS_IMETHOD    RemoveRule(PRUint32 aIndex);  \
  NS_IMETHOD    RemoveImport(PRUint32 aIndex);  \



#define NS_FORWARD_IDOMCSSSTYLESHEET(_to)  \
  NS_IMETHOD    GetOwningElement(nsIDOMHTMLElement** aOwningElement) { return _to##GetOwningElement(aOwningElement); } \
  NS_IMETHOD    GetParentStyleSheet(nsIDOMCSSStyleSheet** aParentStyleSheet) { return _to##GetParentStyleSheet(aParentStyleSheet); } \
  NS_IMETHOD    GetHref(nsString& aHref) { return _to##GetHref(aHref); } \
  NS_IMETHOD    GetTitle(nsString& aTitle) { return _to##GetTitle(aTitle); } \
  NS_IMETHOD    GetImports(nsIDOMStyleSheetCollection** aImports) { return _to##GetImports(aImports); } \
  NS_IMETHOD    GetRules(nsIDOMCSSStyleRuleCollection** aRules) { return _to##GetRules(aRules); } \
  NS_IMETHOD    AddRule(const nsString& aSelector, const nsString& aDeclaration, PRUint32 aIndex, PRUint32* aReturn) { return _to##AddRule(aSelector, aDeclaration, aIndex, aReturn); }  \
  NS_IMETHOD    AddImport(const nsString& aUrl, PRUint32 aIndex, PRUint32* aReturn) { return _to##AddImport(aUrl, aIndex, aReturn); }  \
  NS_IMETHOD    RemoveRule(PRUint32 aIndex) { return _to##RemoveRule(aIndex); }  \
  NS_IMETHOD    RemoveImport(PRUint32 aIndex) { return _to##RemoveImport(aIndex); }  \


extern nsresult NS_InitCSSStyleSheetClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCSSStyleSheet(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCSSStyleSheet_h__
