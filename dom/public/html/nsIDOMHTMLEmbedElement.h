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

#ifndef nsIDOMHTMLEmbedElement_h__
#define nsIDOMHTMLEmbedElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLEMBEDELEMENT_IID \
 { 0x123f90ab, 0x15b3, 0x11d2, \
  { 0x45, 0x6e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLEmbedElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLEMBEDELEMENT_IID; return iid; }

  NS_IMETHOD    GetAlign(nsString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsString& aAlign)=0;

  NS_IMETHOD    GetHeight(nsString& aHeight)=0;
  NS_IMETHOD    SetHeight(const nsString& aHeight)=0;

  NS_IMETHOD    GetName(nsString& aName)=0;
  NS_IMETHOD    SetName(const nsString& aName)=0;

  NS_IMETHOD    GetSrc(nsString& aSrc)=0;
  NS_IMETHOD    SetSrc(const nsString& aSrc)=0;

  NS_IMETHOD    GetType(nsString& aType)=0;
  NS_IMETHOD    SetType(const nsString& aType)=0;

  NS_IMETHOD    GetWidth(nsString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsString& aWidth)=0;
};


#define NS_DECL_IDOMHTMLEMBEDELEMENT   \
  NS_IMETHOD    GetAlign(nsString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsString& aAlign);  \
  NS_IMETHOD    GetHeight(nsString& aHeight);  \
  NS_IMETHOD    SetHeight(const nsString& aHeight);  \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    SetName(const nsString& aName);  \
  NS_IMETHOD    GetSrc(nsString& aSrc);  \
  NS_IMETHOD    SetSrc(const nsString& aSrc);  \
  NS_IMETHOD    GetType(nsString& aType);  \
  NS_IMETHOD    SetType(const nsString& aType);  \
  NS_IMETHOD    GetWidth(nsString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsString& aWidth);  \



#define NS_FORWARD_IDOMHTMLEMBEDELEMENT(_to)  \
  NS_IMETHOD    GetAlign(nsString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetHeight(nsString& aHeight) { return _to GetHeight(aHeight); } \
  NS_IMETHOD    SetHeight(const nsString& aHeight) { return _to SetHeight(aHeight); } \
  NS_IMETHOD    GetName(nsString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetSrc(nsString& aSrc) { return _to GetSrc(aSrc); } \
  NS_IMETHOD    SetSrc(const nsString& aSrc) { return _to SetSrc(aSrc); } \
  NS_IMETHOD    GetType(nsString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    SetType(const nsString& aType) { return _to SetType(aType); } \
  NS_IMETHOD    GetWidth(nsString& aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsString& aWidth) { return _to SetWidth(aWidth); } \


extern "C" NS_DOM nsresult NS_InitHTMLEmbedElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLEmbedElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLEmbedElement_h__
