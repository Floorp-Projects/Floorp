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

#ifndef nsIDOMHTMLInputElement_h__
#define nsIDOMHTMLInputElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLFormElement;

#define NS_IDOMHTMLINPUTELEMENT_IID \
 { 0xa6cf9093, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLInputElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLINPUTELEMENT_IID; return iid; }

  NS_IMETHOD    GetDefaultValue(nsAWritableString& aDefaultValue)=0;
  NS_IMETHOD    SetDefaultValue(const nsAReadableString& aDefaultValue)=0;

  NS_IMETHOD    GetDefaultChecked(PRBool* aDefaultChecked)=0;
  NS_IMETHOD    SetDefaultChecked(PRBool aDefaultChecked)=0;

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;

  NS_IMETHOD    GetAccept(nsAWritableString& aAccept)=0;
  NS_IMETHOD    SetAccept(const nsAReadableString& aAccept)=0;

  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey)=0;
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey)=0;

  NS_IMETHOD    GetAlign(nsAWritableString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign)=0;

  NS_IMETHOD    GetAlt(nsAWritableString& aAlt)=0;
  NS_IMETHOD    SetAlt(const nsAReadableString& aAlt)=0;

  NS_IMETHOD    GetChecked(PRBool* aChecked)=0;
  NS_IMETHOD    SetChecked(PRBool aChecked)=0;

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetMaxLength(PRInt32* aMaxLength)=0;
  NS_IMETHOD    SetMaxLength(PRInt32 aMaxLength)=0;

  NS_IMETHOD    GetName(nsAWritableString& aName)=0;
  NS_IMETHOD    SetName(const nsAReadableString& aName)=0;

  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly)=0;
  NS_IMETHOD    SetReadOnly(PRBool aReadOnly)=0;

  NS_IMETHOD    GetSize(nsAWritableString& aSize)=0;
  NS_IMETHOD    SetSize(const nsAReadableString& aSize)=0;

  NS_IMETHOD    GetSrc(nsAWritableString& aSrc)=0;
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc)=0;

  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex)=0;
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex)=0;

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;
  NS_IMETHOD    SetType(const nsAReadableString& aType)=0;

  NS_IMETHOD    GetUseMap(nsAWritableString& aUseMap)=0;
  NS_IMETHOD    SetUseMap(const nsAReadableString& aUseMap)=0;

  NS_IMETHOD    GetValue(nsAWritableString& aValue)=0;
  NS_IMETHOD    SetValue(const nsAReadableString& aValue)=0;

  NS_IMETHOD    Blur()=0;

  NS_IMETHOD    Focus()=0;

  NS_IMETHOD    Select()=0;

  NS_IMETHOD    Click()=0;
};


#define NS_DECL_IDOMHTMLINPUTELEMENT   \
  NS_IMETHOD    GetDefaultValue(nsAWritableString& aDefaultValue);  \
  NS_IMETHOD    SetDefaultValue(const nsAReadableString& aDefaultValue);  \
  NS_IMETHOD    GetDefaultChecked(PRBool* aDefaultChecked);  \
  NS_IMETHOD    SetDefaultChecked(PRBool aDefaultChecked);  \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm);  \
  NS_IMETHOD    GetAccept(nsAWritableString& aAccept);  \
  NS_IMETHOD    SetAccept(const nsAReadableString& aAccept);  \
  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey);  \
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey);  \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign);  \
  NS_IMETHOD    GetAlt(nsAWritableString& aAlt);  \
  NS_IMETHOD    SetAlt(const nsAReadableString& aAlt);  \
  NS_IMETHOD    GetChecked(PRBool* aChecked);  \
  NS_IMETHOD    SetChecked(PRBool aChecked);  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetMaxLength(PRInt32* aMaxLength);  \
  NS_IMETHOD    SetMaxLength(PRInt32 aMaxLength);  \
  NS_IMETHOD    GetName(nsAWritableString& aName);  \
  NS_IMETHOD    SetName(const nsAReadableString& aName);  \
  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly);  \
  NS_IMETHOD    SetReadOnly(PRBool aReadOnly);  \
  NS_IMETHOD    GetSize(nsAWritableString& aSize);  \
  NS_IMETHOD    SetSize(const nsAReadableString& aSize);  \
  NS_IMETHOD    GetSrc(nsAWritableString& aSrc);  \
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc);  \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex);  \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex);  \
  NS_IMETHOD    GetType(nsAWritableString& aType);  \
  NS_IMETHOD    SetType(const nsAReadableString& aType);  \
  NS_IMETHOD    GetUseMap(nsAWritableString& aUseMap);  \
  NS_IMETHOD    SetUseMap(const nsAReadableString& aUseMap);  \
  NS_IMETHOD    GetValue(nsAWritableString& aValue);  \
  NS_IMETHOD    SetValue(const nsAReadableString& aValue);  \
  NS_IMETHOD    Blur();  \
  NS_IMETHOD    Focus();  \
  NS_IMETHOD    Select();  \
  NS_IMETHOD    Click();  \



#define NS_FORWARD_IDOMHTMLINPUTELEMENT(_to)  \
  NS_IMETHOD    GetDefaultValue(nsAWritableString& aDefaultValue) { return _to GetDefaultValue(aDefaultValue); } \
  NS_IMETHOD    SetDefaultValue(const nsAReadableString& aDefaultValue) { return _to SetDefaultValue(aDefaultValue); } \
  NS_IMETHOD    GetDefaultChecked(PRBool* aDefaultChecked) { return _to GetDefaultChecked(aDefaultChecked); } \
  NS_IMETHOD    SetDefaultChecked(PRBool aDefaultChecked) { return _to SetDefaultChecked(aDefaultChecked); } \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm) { return _to GetForm(aForm); } \
  NS_IMETHOD    GetAccept(nsAWritableString& aAccept) { return _to GetAccept(aAccept); } \
  NS_IMETHOD    SetAccept(const nsAReadableString& aAccept) { return _to SetAccept(aAccept); } \
  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey) { return _to GetAccessKey(aAccessKey); } \
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey) { return _to SetAccessKey(aAccessKey); } \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetAlt(nsAWritableString& aAlt) { return _to GetAlt(aAlt); } \
  NS_IMETHOD    SetAlt(const nsAReadableString& aAlt) { return _to SetAlt(aAlt); } \
  NS_IMETHOD    GetChecked(PRBool* aChecked) { return _to GetChecked(aChecked); } \
  NS_IMETHOD    SetChecked(PRBool aChecked) { return _to SetChecked(aChecked); } \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return _to GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return _to SetDisabled(aDisabled); } \
  NS_IMETHOD    GetMaxLength(PRInt32* aMaxLength) { return _to GetMaxLength(aMaxLength); } \
  NS_IMETHOD    SetMaxLength(PRInt32 aMaxLength) { return _to SetMaxLength(aMaxLength); } \
  NS_IMETHOD    GetName(nsAWritableString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsAReadableString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly) { return _to GetReadOnly(aReadOnly); } \
  NS_IMETHOD    SetReadOnly(PRBool aReadOnly) { return _to SetReadOnly(aReadOnly); } \
  NS_IMETHOD    GetSize(nsAWritableString& aSize) { return _to GetSize(aSize); } \
  NS_IMETHOD    SetSize(const nsAReadableString& aSize) { return _to SetSize(aSize); } \
  NS_IMETHOD    GetSrc(nsAWritableString& aSrc) { return _to GetSrc(aSrc); } \
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc) { return _to SetSrc(aSrc); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return _to GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return _to SetTabIndex(aTabIndex); } \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    SetType(const nsAReadableString& aType) { return _to SetType(aType); } \
  NS_IMETHOD    GetUseMap(nsAWritableString& aUseMap) { return _to GetUseMap(aUseMap); } \
  NS_IMETHOD    SetUseMap(const nsAReadableString& aUseMap) { return _to SetUseMap(aUseMap); } \
  NS_IMETHOD    GetValue(nsAWritableString& aValue) { return _to GetValue(aValue); } \
  NS_IMETHOD    SetValue(const nsAReadableString& aValue) { return _to SetValue(aValue); } \
  NS_IMETHOD    Blur() { return _to Blur(); }  \
  NS_IMETHOD    Focus() { return _to Focus(); }  \
  NS_IMETHOD    Select() { return _to Select(); }  \
  NS_IMETHOD    Click() { return _to Click(); }  \


extern "C" NS_DOM nsresult NS_InitHTMLInputElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLInputElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLInputElement_h__
