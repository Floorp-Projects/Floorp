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

#ifndef nsIDOMHTMLTheadElement_h__
#define nsIDOMHTMLTheadElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLTHEADELEMENT_IID \
{ 0x6f76532a,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLTheadElement : public nsIDOMHTMLElement {
public:

  NS_IMETHOD    GetAlign(nsString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsString& aAlign)=0;

  NS_IMETHOD    GetCh(nsString& aCh)=0;
  NS_IMETHOD    SetCh(const nsString& aCh)=0;

  NS_IMETHOD    GetChOff(nsString& aChOff)=0;
  NS_IMETHOD    SetChOff(const nsString& aChOff)=0;

  NS_IMETHOD    GetVAlign(nsString& aVAlign)=0;
  NS_IMETHOD    SetVAlign(const nsString& aVAlign)=0;
};


#define NS_DECL_IDOMHTMLTHEADELEMENT   \
  NS_IMETHOD    GetAlign(nsString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsString& aAlign);  \
  NS_IMETHOD    GetCh(nsString& aCh);  \
  NS_IMETHOD    SetCh(const nsString& aCh);  \
  NS_IMETHOD    GetChOff(nsString& aChOff);  \
  NS_IMETHOD    SetChOff(const nsString& aChOff);  \
  NS_IMETHOD    GetVAlign(nsString& aVAlign);  \
  NS_IMETHOD    SetVAlign(const nsString& aVAlign);  \



#define NS_FORWARD_IDOMHTMLTHEADELEMENT(_to)  \
  NS_IMETHOD    GetAlign(nsString& aAlign) { return _to##GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsString& aAlign) { return _to##SetAlign(aAlign); } \
  NS_IMETHOD    GetCh(nsString& aCh) { return _to##GetCh(aCh); } \
  NS_IMETHOD    SetCh(const nsString& aCh) { return _to##SetCh(aCh); } \
  NS_IMETHOD    GetChOff(nsString& aChOff) { return _to##GetChOff(aChOff); } \
  NS_IMETHOD    SetChOff(const nsString& aChOff) { return _to##SetChOff(aChOff); } \
  NS_IMETHOD    GetVAlign(nsString& aVAlign) { return _to##GetVAlign(aVAlign); } \
  NS_IMETHOD    SetVAlign(const nsString& aVAlign) { return _to##SetVAlign(aVAlign); } \


extern nsresult NS_InitHTMLTheadElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTheadElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTheadElement_h__
