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

#ifndef nsIDOMHTMLSelectElement_h__
#define nsIDOMHTMLSelectElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLElement;
class nsIDOMHTMLFormElement;
class nsIDOMHTMLCollection;

#define NS_IDOMHTMLSELECTELEMENT_IID \
{ 0x6f76531b,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLSelectElement : public nsIDOMHTMLElement {
public:

  NS_IMETHOD    GetType(nsString& aType)=0;
  NS_IMETHOD    SetType(const nsString& aType)=0;

  NS_IMETHOD    GetSelectedIndex(PRInt32* aSelectedIndex)=0;
  NS_IMETHOD    SetSelectedIndex(PRInt32 aSelectedIndex)=0;

  NS_IMETHOD    GetValue(nsString& aValue)=0;
  NS_IMETHOD    SetValue(const nsString& aValue)=0;

  NS_IMETHOD    GetLength(PRInt32* aLength)=0;
  NS_IMETHOD    SetLength(PRInt32 aLength)=0;

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;
  NS_IMETHOD    SetForm(nsIDOMHTMLFormElement* aForm)=0;

  NS_IMETHOD    GetOptions(nsIDOMHTMLCollection** aOptions)=0;
  NS_IMETHOD    SetOptions(nsIDOMHTMLCollection* aOptions)=0;

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetMultiple(PRBool* aMultiple)=0;
  NS_IMETHOD    SetMultiple(PRBool aMultiple)=0;

  NS_IMETHOD    GetName(nsString& aName)=0;
  NS_IMETHOD    SetName(const nsString& aName)=0;

  NS_IMETHOD    GetSize(PRInt32* aSize)=0;
  NS_IMETHOD    SetSize(PRInt32 aSize)=0;

  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex)=0;
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex)=0;

  NS_IMETHOD    Add(nsIDOMHTMLElement* aElement, nsIDOMHTMLElement* aBefore)=0;

  NS_IMETHOD    Remove(PRInt32 aIndex)=0;

  NS_IMETHOD    Blur()=0;

  NS_IMETHOD    Focus()=0;
};


#define NS_DECL_IDOMHTMLSELECTELEMENT   \
  NS_IMETHOD    GetType(nsString& aType);  \
  NS_IMETHOD    SetType(const nsString& aType);  \
  NS_IMETHOD    GetSelectedIndex(PRInt32* aSelectedIndex);  \
  NS_IMETHOD    SetSelectedIndex(PRInt32 aSelectedIndex);  \
  NS_IMETHOD    GetValue(nsString& aValue);  \
  NS_IMETHOD    SetValue(const nsString& aValue);  \
  NS_IMETHOD    GetLength(PRInt32* aLength);  \
  NS_IMETHOD    SetLength(PRInt32 aLength);  \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm);  \
  NS_IMETHOD    SetForm(nsIDOMHTMLFormElement* aForm);  \
  NS_IMETHOD    GetOptions(nsIDOMHTMLCollection** aOptions);  \
  NS_IMETHOD    SetOptions(nsIDOMHTMLCollection* aOptions);  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetMultiple(PRBool* aMultiple);  \
  NS_IMETHOD    SetMultiple(PRBool aMultiple);  \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    SetName(const nsString& aName);  \
  NS_IMETHOD    GetSize(PRInt32* aSize);  \
  NS_IMETHOD    SetSize(PRInt32 aSize);  \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex);  \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex);  \
  NS_IMETHOD    Add(nsIDOMHTMLElement* aElement, nsIDOMHTMLElement* aBefore);  \
  NS_IMETHOD    Remove(PRInt32 aIndex);  \
  NS_IMETHOD    Blur();  \
  NS_IMETHOD    Focus();  \



#define NS_FORWARD_IDOMHTMLSELECTELEMENT(superClass)  \
  NS_IMETHOD    GetType(nsString& aType) { return superClass::GetType(aType); } \
  NS_IMETHOD    SetType(const nsString& aType) { return superClass::SetType(aType); } \
  NS_IMETHOD    GetSelectedIndex(PRInt32* aSelectedIndex) { return superClass::GetSelectedIndex(aSelectedIndex); } \
  NS_IMETHOD    SetSelectedIndex(PRInt32 aSelectedIndex) { return superClass::SetSelectedIndex(aSelectedIndex); } \
  NS_IMETHOD    GetValue(nsString& aValue) { return superClass::GetValue(aValue); } \
  NS_IMETHOD    SetValue(const nsString& aValue) { return superClass::SetValue(aValue); } \
  NS_IMETHOD    GetLength(PRInt32* aLength) { return superClass::GetLength(aLength); } \
  NS_IMETHOD    SetLength(PRInt32 aLength) { return superClass::SetLength(aLength); } \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm) { return superClass::GetForm(aForm); } \
  NS_IMETHOD    SetForm(nsIDOMHTMLFormElement* aForm) { return superClass::SetForm(aForm); } \
  NS_IMETHOD    GetOptions(nsIDOMHTMLCollection** aOptions) { return superClass::GetOptions(aOptions); } \
  NS_IMETHOD    SetOptions(nsIDOMHTMLCollection* aOptions) { return superClass::SetOptions(aOptions); } \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return superClass::GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return superClass::SetDisabled(aDisabled); } \
  NS_IMETHOD    GetMultiple(PRBool* aMultiple) { return superClass::GetMultiple(aMultiple); } \
  NS_IMETHOD    SetMultiple(PRBool aMultiple) { return superClass::SetMultiple(aMultiple); } \
  NS_IMETHOD    GetName(nsString& aName) { return superClass::GetName(aName); } \
  NS_IMETHOD    SetName(const nsString& aName) { return superClass::SetName(aName); } \
  NS_IMETHOD    GetSize(PRInt32* aSize) { return superClass::GetSize(aSize); } \
  NS_IMETHOD    SetSize(PRInt32 aSize) { return superClass::SetSize(aSize); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return superClass::GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return superClass::SetTabIndex(aTabIndex); } \
  NS_IMETHOD    Add(nsIDOMHTMLElement* aElement, nsIDOMHTMLElement* aBefore) { return superClass::Add(aElement, aBefore); }  \
  NS_IMETHOD    Remove(PRInt32 aIndex) { return superClass::Remove(aIndex); }  \
  NS_IMETHOD    Blur() { return superClass::Blur(); }  \
  NS_IMETHOD    Focus() { return superClass::Focus(); }  \


extern nsresult NS_InitHTMLSelectElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLSelectElement(nsIScriptContext *aContext, nsIDOMHTMLSelectElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLSelectElement_h__
