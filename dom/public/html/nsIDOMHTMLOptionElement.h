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
class nsIDOMHTMLOptionElement;

#define NS_IDOMHTMLOPTIONELEMENT_IID \
{ 0x6f765315,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLOptionElement : public nsIDOMHTMLElement {
public:

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;
  NS_IMETHOD    SetForm(nsIDOMHTMLFormElement* aForm)=0;

  NS_IMETHOD    GetDefaultSelected(PRBool* aDefaultSelected)=0;
  NS_IMETHOD    SetDefaultSelected(PRBool aDefaultSelected)=0;

  NS_IMETHOD    GetText(nsString& aText)=0;
  NS_IMETHOD    SetText(const nsString& aText)=0;

  NS_IMETHOD    GetIndex(PRInt32* aIndex)=0;
  NS_IMETHOD    SetIndex(PRInt32 aIndex)=0;

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetLabel(nsString& aLabel)=0;
  NS_IMETHOD    SetLabel(const nsString& aLabel)=0;

  NS_IMETHOD    GetSelected(PRBool* aSelected)=0;
  NS_IMETHOD    SetSelected(PRBool aSelected)=0;

  NS_IMETHOD    GetValue(nsString& aValue)=0;
  NS_IMETHOD    SetValue(const nsString& aValue)=0;
};

extern nsresult NS_InitHTMLOptionElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLOptionElement(nsIScriptContext *aContext, nsIDOMHTMLOptionElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLOptionElement_h__
