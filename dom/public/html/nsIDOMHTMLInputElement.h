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

#ifndef nsIDOMHTMLInputElement_h__
#define nsIDOMHTMLInputElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLFormElement;

#define NS_IDOMHTMLINPUTELEMENT_IID \
{ 0x6f765307,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLInputElement : public nsIDOMHTMLElement {
public:

  NS_IMETHOD    GetDefaultValue(nsString& aDefaultValue)=0;
  NS_IMETHOD    SetDefaultValue(const nsString& aDefaultValue)=0;

  NS_IMETHOD    GetDefaultChecked(PRBool* aDefaultChecked)=0;
  NS_IMETHOD    SetDefaultChecked(PRBool aDefaultChecked)=0;

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;

  NS_IMETHOD    GetAccept(nsString& aAccept)=0;
  NS_IMETHOD    SetAccept(const nsString& aAccept)=0;

  NS_IMETHOD    GetAccessKey(nsString& aAccessKey)=0;
  NS_IMETHOD    SetAccessKey(const nsString& aAccessKey)=0;

  NS_IMETHOD    GetAlign(nsString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsString& aAlign)=0;

  NS_IMETHOD    GetAlt(nsString& aAlt)=0;
  NS_IMETHOD    SetAlt(const nsString& aAlt)=0;

  NS_IMETHOD    GetChecked(PRBool* aChecked)=0;
  NS_IMETHOD    SetChecked(PRBool aChecked)=0;

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetMaxLength(PRInt32* aMaxLength)=0;
  NS_IMETHOD    SetMaxLength(PRInt32 aMaxLength)=0;

  NS_IMETHOD    GetName(nsString& aName)=0;
  NS_IMETHOD    SetName(const nsString& aName)=0;

  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly)=0;
  NS_IMETHOD    SetReadOnly(PRBool aReadOnly)=0;

  NS_IMETHOD    GetSize(nsString& aSize)=0;
  NS_IMETHOD    SetSize(const nsString& aSize)=0;

  NS_IMETHOD    GetSrc(nsString& aSrc)=0;
  NS_IMETHOD    SetSrc(const nsString& aSrc)=0;

  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex)=0;
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex)=0;

  NS_IMETHOD    GetType(nsString& aType)=0;

  NS_IMETHOD    GetUseMap(nsString& aUseMap)=0;
  NS_IMETHOD    SetUseMap(const nsString& aUseMap)=0;

  NS_IMETHOD    GetValue(nsString& aValue)=0;
  NS_IMETHOD    SetValue(const nsString& aValue)=0;

  NS_IMETHOD    Blur()=0;

  NS_IMETHOD    Focus()=0;

  NS_IMETHOD    Select()=0;

  NS_IMETHOD    Click()=0;
};


#define NS_DECL_IDOMHTMLINPUTELEMENT   \
  NS_IMETHOD    GetDefaultValue(nsString& aDefaultValue);  \
  NS_IMETHOD    SetDefaultValue(const nsString& aDefaultValue);  \
  NS_IMETHOD    GetDefaultChecked(PRBool* aDefaultChecked);  \
  NS_IMETHOD    SetDefaultChecked(PRBool aDefaultChecked);  \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm);  \
  NS_IMETHOD    GetAccept(nsString& aAccept);  \
  NS_IMETHOD    SetAccept(const nsString& aAccept);  \
  NS_IMETHOD    GetAccessKey(nsString& aAccessKey);  \
  NS_IMETHOD    SetAccessKey(const nsString& aAccessKey);  \
  NS_IMETHOD    GetAlign(nsString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsString& aAlign);  \
  NS_IMETHOD    GetAlt(nsString& aAlt);  \
  NS_IMETHOD    SetAlt(const nsString& aAlt);  \
  NS_IMETHOD    GetChecked(PRBool* aChecked);  \
  NS_IMETHOD    SetChecked(PRBool aChecked);  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetMaxLength(PRInt32* aMaxLength);  \
  NS_IMETHOD    SetMaxLength(PRInt32 aMaxLength);  \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    SetName(const nsString& aName);  \
  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly);  \
  NS_IMETHOD    SetReadOnly(PRBool aReadOnly);  \
  NS_IMETHOD    GetSize(nsString& aSize);  \
  NS_IMETHOD    SetSize(const nsString& aSize);  \
  NS_IMETHOD    GetSrc(nsString& aSrc);  \
  NS_IMETHOD    SetSrc(const nsString& aSrc);  \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex);  \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex);  \
  NS_IMETHOD    GetType(nsString& aType);  \
  NS_IMETHOD    GetUseMap(nsString& aUseMap);  \
  NS_IMETHOD    SetUseMap(const nsString& aUseMap);  \
  NS_IMETHOD    GetValue(nsString& aValue);  \
  NS_IMETHOD    SetValue(const nsString& aValue);  \
  NS_IMETHOD    Blur();  \
  NS_IMETHOD    Focus();  \
  NS_IMETHOD    Select();  \
  NS_IMETHOD    Click();  \



#define NS_FORWARD_IDOMHTMLINPUTELEMENT(superClass)  \
  NS_IMETHOD    GetDefaultValue(nsString& aDefaultValue) { return superClass::GetDefaultValue(aDefaultValue); } \
  NS_IMETHOD    SetDefaultValue(const nsString& aDefaultValue) { return superClass::SetDefaultValue(aDefaultValue); } \
  NS_IMETHOD    GetDefaultChecked(PRBool* aDefaultChecked) { return superClass::GetDefaultChecked(aDefaultChecked); } \
  NS_IMETHOD    SetDefaultChecked(PRBool aDefaultChecked) { return superClass::SetDefaultChecked(aDefaultChecked); } \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm) { return superClass::GetForm(aForm); } \
  NS_IMETHOD    GetAccept(nsString& aAccept) { return superClass::GetAccept(aAccept); } \
  NS_IMETHOD    SetAccept(const nsString& aAccept) { return superClass::SetAccept(aAccept); } \
  NS_IMETHOD    GetAccessKey(nsString& aAccessKey) { return superClass::GetAccessKey(aAccessKey); } \
  NS_IMETHOD    SetAccessKey(const nsString& aAccessKey) { return superClass::SetAccessKey(aAccessKey); } \
  NS_IMETHOD    GetAlign(nsString& aAlign) { return superClass::GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsString& aAlign) { return superClass::SetAlign(aAlign); } \
  NS_IMETHOD    GetAlt(nsString& aAlt) { return superClass::GetAlt(aAlt); } \
  NS_IMETHOD    SetAlt(const nsString& aAlt) { return superClass::SetAlt(aAlt); } \
  NS_IMETHOD    GetChecked(PRBool* aChecked) { return superClass::GetChecked(aChecked); } \
  NS_IMETHOD    SetChecked(PRBool aChecked) { return superClass::SetChecked(aChecked); } \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return superClass::GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return superClass::SetDisabled(aDisabled); } \
  NS_IMETHOD    GetMaxLength(PRInt32* aMaxLength) { return superClass::GetMaxLength(aMaxLength); } \
  NS_IMETHOD    SetMaxLength(PRInt32 aMaxLength) { return superClass::SetMaxLength(aMaxLength); } \
  NS_IMETHOD    GetName(nsString& aName) { return superClass::GetName(aName); } \
  NS_IMETHOD    SetName(const nsString& aName) { return superClass::SetName(aName); } \
  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly) { return superClass::GetReadOnly(aReadOnly); } \
  NS_IMETHOD    SetReadOnly(PRBool aReadOnly) { return superClass::SetReadOnly(aReadOnly); } \
  NS_IMETHOD    GetSize(nsString& aSize) { return superClass::GetSize(aSize); } \
  NS_IMETHOD    SetSize(const nsString& aSize) { return superClass::SetSize(aSize); } \
  NS_IMETHOD    GetSrc(nsString& aSrc) { return superClass::GetSrc(aSrc); } \
  NS_IMETHOD    SetSrc(const nsString& aSrc) { return superClass::SetSrc(aSrc); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return superClass::GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return superClass::SetTabIndex(aTabIndex); } \
  NS_IMETHOD    GetType(nsString& aType) { return superClass::GetType(aType); } \
  NS_IMETHOD    GetUseMap(nsString& aUseMap) { return superClass::GetUseMap(aUseMap); } \
  NS_IMETHOD    SetUseMap(const nsString& aUseMap) { return superClass::SetUseMap(aUseMap); } \
  NS_IMETHOD    GetValue(nsString& aValue) { return superClass::GetValue(aValue); } \
  NS_IMETHOD    SetValue(const nsString& aValue) { return superClass::SetValue(aValue); } \
  NS_IMETHOD    Blur() { return superClass::Blur(); }  \
  NS_IMETHOD    Focus() { return superClass::Focus(); }  \
  NS_IMETHOD    Select() { return superClass::Select(); }  \
  NS_IMETHOD    Click() { return superClass::Click(); }  \


extern nsresult NS_InitHTMLInputElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLInputElement(nsIScriptContext *aContext, nsIDOMHTMLInputElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLInputElement_h__
