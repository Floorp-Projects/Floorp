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

#ifndef nsIDOMHTMLTableColElement_h__
#define nsIDOMHTMLTableColElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLTABLECOLELEMENT_IID \
 { 0xa6cf90b4, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLTableColElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLTABLECOLELEMENT_IID; return iid; }

  NS_IMETHOD    GetAlign(nsAWritableString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign)=0;

  NS_IMETHOD    GetCh(nsAWritableString& aCh)=0;
  NS_IMETHOD    SetCh(const nsAReadableString& aCh)=0;

  NS_IMETHOD    GetChOff(nsAWritableString& aChOff)=0;
  NS_IMETHOD    SetChOff(const nsAReadableString& aChOff)=0;

  NS_IMETHOD    GetSpan(PRInt32* aSpan)=0;
  NS_IMETHOD    SetSpan(PRInt32 aSpan)=0;

  NS_IMETHOD    GetVAlign(nsAWritableString& aVAlign)=0;
  NS_IMETHOD    SetVAlign(const nsAReadableString& aVAlign)=0;

  NS_IMETHOD    GetWidth(nsAWritableString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth)=0;
};


#define NS_DECL_IDOMHTMLTABLECOLELEMENT   \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign);  \
  NS_IMETHOD    GetCh(nsAWritableString& aCh);  \
  NS_IMETHOD    SetCh(const nsAReadableString& aCh);  \
  NS_IMETHOD    GetChOff(nsAWritableString& aChOff);  \
  NS_IMETHOD    SetChOff(const nsAReadableString& aChOff);  \
  NS_IMETHOD    GetSpan(PRInt32* aSpan);  \
  NS_IMETHOD    SetSpan(PRInt32 aSpan);  \
  NS_IMETHOD    GetVAlign(nsAWritableString& aVAlign);  \
  NS_IMETHOD    SetVAlign(const nsAReadableString& aVAlign);  \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth);  \



#define NS_FORWARD_IDOMHTMLTABLECOLELEMENT(_to)  \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetCh(nsAWritableString& aCh) { return _to GetCh(aCh); } \
  NS_IMETHOD    SetCh(const nsAReadableString& aCh) { return _to SetCh(aCh); } \
  NS_IMETHOD    GetChOff(nsAWritableString& aChOff) { return _to GetChOff(aChOff); } \
  NS_IMETHOD    SetChOff(const nsAReadableString& aChOff) { return _to SetChOff(aChOff); } \
  NS_IMETHOD    GetSpan(PRInt32* aSpan) { return _to GetSpan(aSpan); } \
  NS_IMETHOD    SetSpan(PRInt32 aSpan) { return _to SetSpan(aSpan); } \
  NS_IMETHOD    GetVAlign(nsAWritableString& aVAlign) { return _to GetVAlign(aVAlign); } \
  NS_IMETHOD    SetVAlign(const nsAReadableString& aVAlign) { return _to SetVAlign(aVAlign); } \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth) { return _to SetWidth(aWidth); } \


extern "C" NS_DOM nsresult NS_InitHTMLTableColElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTableColElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTableColElement_h__
