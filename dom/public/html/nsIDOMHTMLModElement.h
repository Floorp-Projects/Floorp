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

#ifndef nsIDOMHTMLModElement_h__
#define nsIDOMHTMLModElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLMODELEMENT_IID \
 { 0xa6cf90a9, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLModElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLMODELEMENT_IID; return iid; }

  NS_IMETHOD    GetCite(nsString& aCite)=0;
  NS_IMETHOD    SetCite(const nsString& aCite)=0;

  NS_IMETHOD    GetDateTime(nsString& aDateTime)=0;
  NS_IMETHOD    SetDateTime(const nsString& aDateTime)=0;
};


#define NS_DECL_IDOMHTMLMODELEMENT   \
  NS_IMETHOD    GetCite(nsString& aCite);  \
  NS_IMETHOD    SetCite(const nsString& aCite);  \
  NS_IMETHOD    GetDateTime(nsString& aDateTime);  \
  NS_IMETHOD    SetDateTime(const nsString& aDateTime);  \



#define NS_FORWARD_IDOMHTMLMODELEMENT(_to)  \
  NS_IMETHOD    GetCite(nsString& aCite) { return _to GetCite(aCite); } \
  NS_IMETHOD    SetCite(const nsString& aCite) { return _to SetCite(aCite); } \
  NS_IMETHOD    GetDateTime(nsString& aDateTime) { return _to GetDateTime(aDateTime); } \
  NS_IMETHOD    SetDateTime(const nsString& aDateTime) { return _to SetDateTime(aDateTime); } \


extern "C" NS_DOM nsresult NS_InitHTMLModElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLModElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLModElement_h__
