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

#ifndef nsIDOMHTMLInsElement_h__
#define nsIDOMHTMLInsElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLINSELEMENT_IID \
{ 0x6f76530d,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLInsElement : public nsIDOMHTMLElement {
public:

  NS_IMETHOD    GetCite(nsString& aCite)=0;
  NS_IMETHOD    SetCite(const nsString& aCite)=0;

  NS_IMETHOD    GetDateTime(nsString& aDateTime)=0;
  NS_IMETHOD    SetDateTime(const nsString& aDateTime)=0;
};


#define NS_DECL_IDOMHTMLINSELEMENT   \
  NS_IMETHOD    GetCite(nsString& aCite);  \
  NS_IMETHOD    SetCite(const nsString& aCite);  \
  NS_IMETHOD    GetDateTime(nsString& aDateTime);  \
  NS_IMETHOD    SetDateTime(const nsString& aDateTime);  \



#define NS_FORWARD_IDOMHTMLINSELEMENT(_to)  \
  NS_IMETHOD    GetCite(nsString& aCite) { return _to##GetCite(aCite); } \
  NS_IMETHOD    SetCite(const nsString& aCite) { return _to##SetCite(aCite); } \
  NS_IMETHOD    GetDateTime(nsString& aDateTime) { return _to##GetDateTime(aDateTime); } \
  NS_IMETHOD    SetDateTime(const nsString& aDateTime) { return _to##SetDateTime(aDateTime); } \


extern nsresult NS_InitHTMLInsElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLInsElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLInsElement_h__
