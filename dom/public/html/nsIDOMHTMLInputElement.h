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

class nsIDOMHTMLInputElement;
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
  NS_IMETHOD    SetForm(nsIDOMHTMLFormElement* aForm)=0;

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
  NS_IMETHOD    SetType(const nsString& aType)=0;

  NS_IMETHOD    GetUseMap(nsString& aUseMap)=0;
  NS_IMETHOD    SetUseMap(const nsString& aUseMap)=0;

  NS_IMETHOD    GetValue(nsString& aValue)=0;
  NS_IMETHOD    SetValue(const nsString& aValue)=0;

  NS_IMETHOD    Blur()=0;

  NS_IMETHOD    Focus()=0;

  NS_IMETHOD    Select()=0;

  NS_IMETHOD    Click()=0;
};

extern nsresult NS_InitHTMLInputElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLInputElement(nsIScriptContext *aContext, nsIDOMHTMLInputElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLInputElement_h__
