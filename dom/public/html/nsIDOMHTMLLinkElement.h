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

#ifndef nsIDOMHTMLLinkElement_h__
#define nsIDOMHTMLLinkElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLLinkElement;

#define NS_IDOMHTMLLINKELEMENT_IID \
{ 0x6f76530d,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLLinkElement : public nsIDOMHTMLElement {
public:

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetCharset(nsString& aCharset)=0;
  NS_IMETHOD    SetCharset(const nsString& aCharset)=0;

  NS_IMETHOD    GetHref(nsString& aHref)=0;
  NS_IMETHOD    SetHref(const nsString& aHref)=0;

  NS_IMETHOD    GetHreflang(nsString& aHreflang)=0;
  NS_IMETHOD    SetHreflang(const nsString& aHreflang)=0;

  NS_IMETHOD    GetMedia(nsString& aMedia)=0;
  NS_IMETHOD    SetMedia(const nsString& aMedia)=0;

  NS_IMETHOD    GetRel(nsString& aRel)=0;
  NS_IMETHOD    SetRel(const nsString& aRel)=0;

  NS_IMETHOD    GetRev(nsString& aRev)=0;
  NS_IMETHOD    SetRev(const nsString& aRev)=0;

  NS_IMETHOD    GetTarget(nsString& aTarget)=0;
  NS_IMETHOD    SetTarget(const nsString& aTarget)=0;

  NS_IMETHOD    GetType(nsString& aType)=0;
  NS_IMETHOD    SetType(const nsString& aType)=0;
};

extern nsresult NS_InitHTMLLinkElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLLinkElement(nsIScriptContext *aContext, nsIDOMHTMLLinkElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLLinkElement_h__
