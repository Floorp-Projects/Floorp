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

#ifndef nsIDOMHTMLScriptElement_h__
#define nsIDOMHTMLScriptElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLSCRIPTELEMENT_IID \
 { 0xa6cf90b1, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLScriptElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLSCRIPTELEMENT_IID; return iid; }

  NS_IMETHOD    GetText(nsAWritableString& aText)=0;
  NS_IMETHOD    SetText(const nsAReadableString& aText)=0;

  NS_IMETHOD    GetHtmlFor(nsAWritableString& aHtmlFor)=0;
  NS_IMETHOD    SetHtmlFor(const nsAReadableString& aHtmlFor)=0;

  NS_IMETHOD    GetEvent(nsAWritableString& aEvent)=0;
  NS_IMETHOD    SetEvent(const nsAReadableString& aEvent)=0;

  NS_IMETHOD    GetCharset(nsAWritableString& aCharset)=0;
  NS_IMETHOD    SetCharset(const nsAReadableString& aCharset)=0;

  NS_IMETHOD    GetDefer(PRBool* aDefer)=0;
  NS_IMETHOD    SetDefer(PRBool aDefer)=0;

  NS_IMETHOD    GetSrc(nsAWritableString& aSrc)=0;
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc)=0;

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;
  NS_IMETHOD    SetType(const nsAReadableString& aType)=0;
};


#define NS_DECL_IDOMHTMLSCRIPTELEMENT   \
  NS_IMETHOD    GetText(nsAWritableString& aText);  \
  NS_IMETHOD    SetText(const nsAReadableString& aText);  \
  NS_IMETHOD    GetHtmlFor(nsAWritableString& aHtmlFor);  \
  NS_IMETHOD    SetHtmlFor(const nsAReadableString& aHtmlFor);  \
  NS_IMETHOD    GetEvent(nsAWritableString& aEvent);  \
  NS_IMETHOD    SetEvent(const nsAReadableString& aEvent);  \
  NS_IMETHOD    GetCharset(nsAWritableString& aCharset);  \
  NS_IMETHOD    SetCharset(const nsAReadableString& aCharset);  \
  NS_IMETHOD    GetDefer(PRBool* aDefer);  \
  NS_IMETHOD    SetDefer(PRBool aDefer);  \
  NS_IMETHOD    GetSrc(nsAWritableString& aSrc);  \
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc);  \
  NS_IMETHOD    GetType(nsAWritableString& aType);  \
  NS_IMETHOD    SetType(const nsAReadableString& aType);  \



#define NS_FORWARD_IDOMHTMLSCRIPTELEMENT(_to)  \
  NS_IMETHOD    GetText(nsAWritableString& aText) { return _to GetText(aText); } \
  NS_IMETHOD    SetText(const nsAReadableString& aText) { return _to SetText(aText); } \
  NS_IMETHOD    GetHtmlFor(nsAWritableString& aHtmlFor) { return _to GetHtmlFor(aHtmlFor); } \
  NS_IMETHOD    SetHtmlFor(const nsAReadableString& aHtmlFor) { return _to SetHtmlFor(aHtmlFor); } \
  NS_IMETHOD    GetEvent(nsAWritableString& aEvent) { return _to GetEvent(aEvent); } \
  NS_IMETHOD    SetEvent(const nsAReadableString& aEvent) { return _to SetEvent(aEvent); } \
  NS_IMETHOD    GetCharset(nsAWritableString& aCharset) { return _to GetCharset(aCharset); } \
  NS_IMETHOD    SetCharset(const nsAReadableString& aCharset) { return _to SetCharset(aCharset); } \
  NS_IMETHOD    GetDefer(PRBool* aDefer) { return _to GetDefer(aDefer); } \
  NS_IMETHOD    SetDefer(PRBool aDefer) { return _to SetDefer(aDefer); } \
  NS_IMETHOD    GetSrc(nsAWritableString& aSrc) { return _to GetSrc(aSrc); } \
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc) { return _to SetSrc(aSrc); } \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    SetType(const nsAReadableString& aType) { return _to SetType(aType); } \


extern "C" NS_DOM nsresult NS_InitHTMLScriptElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLScriptElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLScriptElement_h__
