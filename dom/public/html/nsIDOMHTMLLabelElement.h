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

#ifndef nsIDOMHTMLLabelElement_h__
#define nsIDOMHTMLLabelElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLFormElement;

#define NS_IDOMHTMLLABELELEMENT_IID \
 { 0xa6cf9096, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLLabelElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLLABELELEMENT_IID; return iid; }

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;

  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey)=0;
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey)=0;

  NS_IMETHOD    GetHtmlFor(nsAWritableString& aHtmlFor)=0;
  NS_IMETHOD    SetHtmlFor(const nsAReadableString& aHtmlFor)=0;
};


#define NS_DECL_IDOMHTMLLABELELEMENT   \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm);  \
  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey);  \
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey);  \
  NS_IMETHOD    GetHtmlFor(nsAWritableString& aHtmlFor);  \
  NS_IMETHOD    SetHtmlFor(const nsAReadableString& aHtmlFor);  \



#define NS_FORWARD_IDOMHTMLLABELELEMENT(_to)  \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm) { return _to GetForm(aForm); } \
  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey) { return _to GetAccessKey(aAccessKey); } \
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey) { return _to SetAccessKey(aAccessKey); } \
  NS_IMETHOD    GetHtmlFor(nsAWritableString& aHtmlFor) { return _to GetHtmlFor(aHtmlFor); } \
  NS_IMETHOD    SetHtmlFor(const nsAReadableString& aHtmlFor) { return _to SetHtmlFor(aHtmlFor); } \


extern "C" NS_DOM nsresult NS_InitHTMLLabelElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLLabelElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLLabelElement_h__
