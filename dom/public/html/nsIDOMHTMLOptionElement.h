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

#ifndef nsIDOMHTMLOptionElement_h__
#define nsIDOMHTMLOptionElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLFormElement;

#define NS_IDOMHTMLOPTIONELEMENT_IID \
 { 0xa6cf9092, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLOptionElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLOPTIONELEMENT_IID; return iid; }

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;

  NS_IMETHOD    GetDefaultSelected(PRBool* aDefaultSelected)=0;
  NS_IMETHOD    SetDefaultSelected(PRBool aDefaultSelected)=0;

  NS_IMETHOD    GetText(nsString& aText)=0;

  NS_IMETHOD    GetIndex(PRInt32* aIndex)=0;
  NS_IMETHOD    SetIndex(PRInt32 aIndex)=0;

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetLabel(nsString& aLabel)=0;
  NS_IMETHOD    SetLabel(const nsString& aLabel)=0;

  NS_IMETHOD    GetSelected(PRBool* aSelected)=0;

  NS_IMETHOD    GetValue(nsString& aValue)=0;
  NS_IMETHOD    SetValue(const nsString& aValue)=0;
};


#define NS_DECL_IDOMHTMLOPTIONELEMENT   \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm);  \
  NS_IMETHOD    GetDefaultSelected(PRBool* aDefaultSelected);  \
  NS_IMETHOD    SetDefaultSelected(PRBool aDefaultSelected);  \
  NS_IMETHOD    GetText(nsString& aText);  \
  NS_IMETHOD    GetIndex(PRInt32* aIndex);  \
  NS_IMETHOD    SetIndex(PRInt32 aIndex);  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetLabel(nsString& aLabel);  \
  NS_IMETHOD    SetLabel(const nsString& aLabel);  \
  NS_IMETHOD    GetSelected(PRBool* aSelected);  \
  NS_IMETHOD    GetValue(nsString& aValue);  \
  NS_IMETHOD    SetValue(const nsString& aValue);  \



#define NS_FORWARD_IDOMHTMLOPTIONELEMENT(_to)  \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm) { return _to GetForm(aForm); } \
  NS_IMETHOD    GetDefaultSelected(PRBool* aDefaultSelected) { return _to GetDefaultSelected(aDefaultSelected); } \
  NS_IMETHOD    SetDefaultSelected(PRBool aDefaultSelected) { return _to SetDefaultSelected(aDefaultSelected); } \
  NS_IMETHOD    GetText(nsString& aText) { return _to GetText(aText); } \
  NS_IMETHOD    GetIndex(PRInt32* aIndex) { return _to GetIndex(aIndex); } \
  NS_IMETHOD    SetIndex(PRInt32 aIndex) { return _to SetIndex(aIndex); } \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return _to GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return _to SetDisabled(aDisabled); } \
  NS_IMETHOD    GetLabel(nsString& aLabel) { return _to GetLabel(aLabel); } \
  NS_IMETHOD    SetLabel(const nsString& aLabel) { return _to SetLabel(aLabel); } \
  NS_IMETHOD    GetSelected(PRBool* aSelected) { return _to GetSelected(aSelected); } \
  NS_IMETHOD    GetValue(nsString& aValue) { return _to GetValue(aValue); } \
  NS_IMETHOD    SetValue(const nsString& aValue) { return _to SetValue(aValue); } \


extern "C" NS_DOM nsresult NS_InitHTMLOptionElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLOptionElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLOptionElement_h__
