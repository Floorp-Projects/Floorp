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

#ifndef nsIDOMHTMLFontElement_h__
#define nsIDOMHTMLFontElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLFONTELEMENT_IID \
 { 0xa6cf90a7, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLFontElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLFONTELEMENT_IID; return iid; }

  NS_IMETHOD    GetColor(nsAWritableString& aColor)=0;
  NS_IMETHOD    SetColor(const nsAReadableString& aColor)=0;

  NS_IMETHOD    GetFace(nsAWritableString& aFace)=0;
  NS_IMETHOD    SetFace(const nsAReadableString& aFace)=0;

  NS_IMETHOD    GetSize(nsAWritableString& aSize)=0;
  NS_IMETHOD    SetSize(const nsAReadableString& aSize)=0;
};


#define NS_DECL_IDOMHTMLFONTELEMENT   \
  NS_IMETHOD    GetColor(nsAWritableString& aColor);  \
  NS_IMETHOD    SetColor(const nsAReadableString& aColor);  \
  NS_IMETHOD    GetFace(nsAWritableString& aFace);  \
  NS_IMETHOD    SetFace(const nsAReadableString& aFace);  \
  NS_IMETHOD    GetSize(nsAWritableString& aSize);  \
  NS_IMETHOD    SetSize(const nsAReadableString& aSize);  \



#define NS_FORWARD_IDOMHTMLFONTELEMENT(_to)  \
  NS_IMETHOD    GetColor(nsAWritableString& aColor) { return _to GetColor(aColor); } \
  NS_IMETHOD    SetColor(const nsAReadableString& aColor) { return _to SetColor(aColor); } \
  NS_IMETHOD    GetFace(nsAWritableString& aFace) { return _to GetFace(aFace); } \
  NS_IMETHOD    SetFace(const nsAReadableString& aFace) { return _to SetFace(aFace); } \
  NS_IMETHOD    GetSize(nsAWritableString& aSize) { return _to GetSize(aSize); } \
  NS_IMETHOD    SetSize(const nsAReadableString& aSize) { return _to SetSize(aSize); } \


extern "C" NS_DOM nsresult NS_InitHTMLFontElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLFontElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLFontElement_h__
