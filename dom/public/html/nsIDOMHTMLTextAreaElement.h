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

#ifndef nsIDOMHTMLTextAreaElement_h__
#define nsIDOMHTMLTextAreaElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLFormElement;

#define NS_IDOMHTMLTEXTAREAELEMENT_IID \
 { 0xa6cf9094, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLTextAreaElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLTEXTAREAELEMENT_IID; return iid; }

  NS_IMETHOD    GetDefaultValue(nsAWritableString& aDefaultValue)=0;
  NS_IMETHOD    SetDefaultValue(const nsAReadableString& aDefaultValue)=0;

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;

  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey)=0;
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey)=0;

  NS_IMETHOD    GetCols(PRInt32* aCols)=0;
  NS_IMETHOD    SetCols(PRInt32 aCols)=0;

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetName(nsAWritableString& aName)=0;
  NS_IMETHOD    SetName(const nsAReadableString& aName)=0;

  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly)=0;
  NS_IMETHOD    SetReadOnly(PRBool aReadOnly)=0;

  NS_IMETHOD    GetRows(PRInt32* aRows)=0;
  NS_IMETHOD    SetRows(PRInt32 aRows)=0;

  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex)=0;
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex)=0;

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;

  NS_IMETHOD    GetValue(nsAWritableString& aValue)=0;
  NS_IMETHOD    SetValue(const nsAReadableString& aValue)=0;

  NS_IMETHOD    Blur()=0;

  NS_IMETHOD    Focus()=0;

  NS_IMETHOD    Select()=0;
};


#define NS_DECL_IDOMHTMLTEXTAREAELEMENT   \
  NS_IMETHOD    GetDefaultValue(nsAWritableString& aDefaultValue);  \
  NS_IMETHOD    SetDefaultValue(const nsAReadableString& aDefaultValue);  \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm);  \
  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey);  \
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey);  \
  NS_IMETHOD    GetCols(PRInt32* aCols);  \
  NS_IMETHOD    SetCols(PRInt32 aCols);  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetName(nsAWritableString& aName);  \
  NS_IMETHOD    SetName(const nsAReadableString& aName);  \
  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly);  \
  NS_IMETHOD    SetReadOnly(PRBool aReadOnly);  \
  NS_IMETHOD    GetRows(PRInt32* aRows);  \
  NS_IMETHOD    SetRows(PRInt32 aRows);  \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex);  \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex);  \
  NS_IMETHOD    GetType(nsAWritableString& aType);  \
  NS_IMETHOD    GetValue(nsAWritableString& aValue);  \
  NS_IMETHOD    SetValue(const nsAReadableString& aValue);  \
  NS_IMETHOD    Blur();  \
  NS_IMETHOD    Focus();  \
  NS_IMETHOD    Select();  \



#define NS_FORWARD_IDOMHTMLTEXTAREAELEMENT(_to)  \
  NS_IMETHOD    GetDefaultValue(nsAWritableString& aDefaultValue) { return _to GetDefaultValue(aDefaultValue); } \
  NS_IMETHOD    SetDefaultValue(const nsAReadableString& aDefaultValue) { return _to SetDefaultValue(aDefaultValue); } \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm) { return _to GetForm(aForm); } \
  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey) { return _to GetAccessKey(aAccessKey); } \
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey) { return _to SetAccessKey(aAccessKey); } \
  NS_IMETHOD    GetCols(PRInt32* aCols) { return _to GetCols(aCols); } \
  NS_IMETHOD    SetCols(PRInt32 aCols) { return _to SetCols(aCols); } \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return _to GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return _to SetDisabled(aDisabled); } \
  NS_IMETHOD    GetName(nsAWritableString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsAReadableString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetReadOnly(PRBool* aReadOnly) { return _to GetReadOnly(aReadOnly); } \
  NS_IMETHOD    SetReadOnly(PRBool aReadOnly) { return _to SetReadOnly(aReadOnly); } \
  NS_IMETHOD    GetRows(PRInt32* aRows) { return _to GetRows(aRows); } \
  NS_IMETHOD    SetRows(PRInt32 aRows) { return _to SetRows(aRows); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return _to GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return _to SetTabIndex(aTabIndex); } \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    GetValue(nsAWritableString& aValue) { return _to GetValue(aValue); } \
  NS_IMETHOD    SetValue(const nsAReadableString& aValue) { return _to SetValue(aValue); } \
  NS_IMETHOD    Blur() { return _to Blur(); }  \
  NS_IMETHOD    Focus() { return _to Focus(); }  \
  NS_IMETHOD    Select() { return _to Select(); }  \


extern "C" NS_DOM nsresult NS_InitHTMLTextAreaElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTextAreaElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTextAreaElement_h__
