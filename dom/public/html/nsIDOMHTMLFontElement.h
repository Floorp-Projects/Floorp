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

  NS_IMETHOD    GetColor(nsString& aColor)=0;
  NS_IMETHOD    SetColor(const nsString& aColor)=0;

  NS_IMETHOD    GetFace(nsString& aFace)=0;
  NS_IMETHOD    SetFace(const nsString& aFace)=0;

  NS_IMETHOD    GetSize(nsString& aSize)=0;
  NS_IMETHOD    SetSize(const nsString& aSize)=0;
};


#define NS_DECL_IDOMHTMLFONTELEMENT   \
  NS_IMETHOD    GetColor(nsString& aColor);  \
  NS_IMETHOD    SetColor(const nsString& aColor);  \
  NS_IMETHOD    GetFace(nsString& aFace);  \
  NS_IMETHOD    SetFace(const nsString& aFace);  \
  NS_IMETHOD    GetSize(nsString& aSize);  \
  NS_IMETHOD    SetSize(const nsString& aSize);  \



#define NS_FORWARD_IDOMHTMLFONTELEMENT(_to)  \
  NS_IMETHOD    GetColor(nsString& aColor) { return _to GetColor(aColor); } \
  NS_IMETHOD    SetColor(const nsString& aColor) { return _to SetColor(aColor); } \
  NS_IMETHOD    GetFace(nsString& aFace) { return _to GetFace(aFace); } \
  NS_IMETHOD    SetFace(const nsString& aFace) { return _to SetFace(aFace); } \
  NS_IMETHOD    GetSize(nsString& aSize) { return _to GetSize(aSize); } \
  NS_IMETHOD    SetSize(const nsString& aSize) { return _to SetSize(aSize); } \


extern "C" NS_DOM nsresult NS_InitHTMLFontElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLFontElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLFontElement_h__
