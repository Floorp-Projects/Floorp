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

#ifndef nsIDOMHTMLSelectElement_h__
#define nsIDOMHTMLSelectElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMNSHTMLOptionCollection;
class nsIDOMHTMLElement;
class nsIDOMHTMLFormElement;

#define NS_IDOMHTMLSELECTELEMENT_IID \
 { 0xa6cf9090, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLSelectElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLSELECTELEMENT_IID; return iid; }

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;

  NS_IMETHOD    GetSelectedIndex(PRInt32* aSelectedIndex)=0;
  NS_IMETHOD    SetSelectedIndex(PRInt32 aSelectedIndex)=0;

  NS_IMETHOD    GetValue(nsAWritableString& aValue)=0;
  NS_IMETHOD    SetValue(const nsAReadableString& aValue)=0;

  NS_IMETHOD    GetLength(PRUint32* aLength)=0;
  NS_IMETHOD    SetLength(PRUint32 aLength)=0;

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;

  NS_IMETHOD    GetOptions(nsIDOMNSHTMLOptionCollection** aOptions)=0;

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetMultiple(PRBool* aMultiple)=0;
  NS_IMETHOD    SetMultiple(PRBool aMultiple)=0;

  NS_IMETHOD    GetName(nsAWritableString& aName)=0;
  NS_IMETHOD    SetName(const nsAReadableString& aName)=0;

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
  NS_IMETHOD    GetType(nsAWritableString& aType);  \
  NS_IMETHOD    GetSelectedIndex(PRInt32* aSelectedIndex);  \
  NS_IMETHOD    SetSelectedIndex(PRInt32 aSelectedIndex);  \
  NS_IMETHOD    GetValue(nsAWritableString& aValue);  \
  NS_IMETHOD    SetValue(const nsAReadableString& aValue);  \
  NS_IMETHOD    GetLength(PRUint32* aLength);  \
  NS_IMETHOD    SetLength(PRUint32 aLength);  \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm);  \
  NS_IMETHOD    GetOptions(nsIDOMNSHTMLOptionCollection** aOptions);  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetMultiple(PRBool* aMultiple);  \
  NS_IMETHOD    SetMultiple(PRBool aMultiple);  \
  NS_IMETHOD    GetName(nsAWritableString& aName);  \
  NS_IMETHOD    SetName(const nsAReadableString& aName);  \
  NS_IMETHOD    GetSize(PRInt32* aSize);  \
  NS_IMETHOD    SetSize(PRInt32 aSize);  \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex);  \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex);  \
  NS_IMETHOD    Add(nsIDOMHTMLElement* aElement, nsIDOMHTMLElement* aBefore);  \
  NS_IMETHOD    Remove(PRInt32 aIndex);  \
  NS_IMETHOD    Blur();  \
  NS_IMETHOD    Focus();  \



#define NS_FORWARD_IDOMHTMLSELECTELEMENT(_to)  \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    GetSelectedIndex(PRInt32* aSelectedIndex) { return _to GetSelectedIndex(aSelectedIndex); } \
  NS_IMETHOD    SetSelectedIndex(PRInt32 aSelectedIndex) { return _to SetSelectedIndex(aSelectedIndex); } \
  NS_IMETHOD    GetValue(nsAWritableString& aValue) { return _to GetValue(aValue); } \
  NS_IMETHOD    SetValue(const nsAReadableString& aValue) { return _to SetValue(aValue); } \
  NS_IMETHOD    GetLength(PRUint32* aLength) { return _to GetLength(aLength); } \
  NS_IMETHOD    SetLength(PRUint32 aLength) { return _to SetLength(aLength); } \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm) { return _to GetForm(aForm); } \
  NS_IMETHOD    GetOptions(nsIDOMNSHTMLOptionCollection** aOptions) { return _to GetOptions(aOptions); } \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return _to GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return _to SetDisabled(aDisabled); } \
  NS_IMETHOD    GetMultiple(PRBool* aMultiple) { return _to GetMultiple(aMultiple); } \
  NS_IMETHOD    SetMultiple(PRBool aMultiple) { return _to SetMultiple(aMultiple); } \
  NS_IMETHOD    GetName(nsAWritableString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsAReadableString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetSize(PRInt32* aSize) { return _to GetSize(aSize); } \
  NS_IMETHOD    SetSize(PRInt32 aSize) { return _to SetSize(aSize); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return _to GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return _to SetTabIndex(aTabIndex); } \
  NS_IMETHOD    Add(nsIDOMHTMLElement* aElement, nsIDOMHTMLElement* aBefore) { return _to Add(aElement, aBefore); }  \
  NS_IMETHOD    Remove(PRInt32 aIndex) { return _to Remove(aIndex); }  \
  NS_IMETHOD    Blur() { return _to Blur(); }  \
  NS_IMETHOD    Focus() { return _to Focus(); }  \


extern "C" NS_DOM nsresult NS_InitHTMLSelectElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLSelectElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLSelectElement_h__
