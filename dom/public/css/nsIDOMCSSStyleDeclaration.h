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

#ifndef nsIDOMCSSStyleDeclaration_h__
#define nsIDOMCSSStyleDeclaration_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMCSSSTYLEDECLARATION_IID \
 { 0xa6cf90be, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMCSSStyleDeclaration : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCSSSTYLEDECLARATION_IID; return iid; }

  NS_IMETHOD    GetCssText(nsString& aCssText)=0;
  NS_IMETHOD    SetCssText(const nsString& aCssText)=0;

  NS_IMETHOD    GetLength(PRUint32* aLength)=0;

  NS_IMETHOD    GetPropertyValue(const nsString& aPropertyName, nsString& aReturn)=0;

  NS_IMETHOD    GetPropertyPriority(const nsString& aPropertyName, nsString& aReturn)=0;

  NS_IMETHOD    SetProperty(const nsString& aPropertyName, const nsString& aValue, const nsString& aPriority)=0;

  NS_IMETHOD    Item(PRUint32 aIndex, nsString& aReturn)=0;
};


#define NS_DECL_IDOMCSSSTYLEDECLARATION   \
  NS_IMETHOD    GetCssText(nsString& aCssText);  \
  NS_IMETHOD    SetCssText(const nsString& aCssText);  \
  NS_IMETHOD    GetLength(PRUint32* aLength);  \
  NS_IMETHOD    GetPropertyValue(const nsString& aPropertyName, nsString& aReturn);  \
  NS_IMETHOD    GetPropertyPriority(const nsString& aPropertyName, nsString& aReturn);  \
  NS_IMETHOD    SetProperty(const nsString& aPropertyName, const nsString& aValue, const nsString& aPriority);  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsString& aReturn);  \



#define NS_FORWARD_IDOMCSSSTYLEDECLARATION(_to)  \
  NS_IMETHOD    GetCssText(nsString& aCssText) { return _to GetCssText(aCssText); } \
  NS_IMETHOD    SetCssText(const nsString& aCssText) { return _to SetCssText(aCssText); } \
  NS_IMETHOD    GetLength(PRUint32* aLength) { return _to GetLength(aLength); } \
  NS_IMETHOD    GetPropertyValue(const nsString& aPropertyName, nsString& aReturn) { return _to GetPropertyValue(aPropertyName, aReturn); }  \
  NS_IMETHOD    GetPropertyPriority(const nsString& aPropertyName, nsString& aReturn) { return _to GetPropertyPriority(aPropertyName, aReturn); }  \
  NS_IMETHOD    SetProperty(const nsString& aPropertyName, const nsString& aValue, const nsString& aPriority) { return _to SetProperty(aPropertyName, aValue, aPriority); }  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsString& aReturn) { return _to Item(aIndex, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitCSSStyleDeclarationClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCSSStyleDeclaration(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCSSStyleDeclaration_h__
