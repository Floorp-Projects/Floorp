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


#define NS_IDOMHTMLLINKELEMENT_IID \
 { 0xa6cf9088, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLLinkElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLLINKELEMENT_IID; return iid; }

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


#define NS_DECL_IDOMHTMLLINKELEMENT   \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetCharset(nsString& aCharset);  \
  NS_IMETHOD    SetCharset(const nsString& aCharset);  \
  NS_IMETHOD    GetHref(nsString& aHref);  \
  NS_IMETHOD    SetHref(const nsString& aHref);  \
  NS_IMETHOD    GetHreflang(nsString& aHreflang);  \
  NS_IMETHOD    SetHreflang(const nsString& aHreflang);  \
  NS_IMETHOD    GetMedia(nsString& aMedia);  \
  NS_IMETHOD    SetMedia(const nsString& aMedia);  \
  NS_IMETHOD    GetRel(nsString& aRel);  \
  NS_IMETHOD    SetRel(const nsString& aRel);  \
  NS_IMETHOD    GetRev(nsString& aRev);  \
  NS_IMETHOD    SetRev(const nsString& aRev);  \
  NS_IMETHOD    GetTarget(nsString& aTarget);  \
  NS_IMETHOD    SetTarget(const nsString& aTarget);  \
  NS_IMETHOD    GetType(nsString& aType);  \
  NS_IMETHOD    SetType(const nsString& aType);  \



#define NS_FORWARD_IDOMHTMLLINKELEMENT(_to)  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return _to GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return _to SetDisabled(aDisabled); } \
  NS_IMETHOD    GetCharset(nsString& aCharset) { return _to GetCharset(aCharset); } \
  NS_IMETHOD    SetCharset(const nsString& aCharset) { return _to SetCharset(aCharset); } \
  NS_IMETHOD    GetHref(nsString& aHref) { return _to GetHref(aHref); } \
  NS_IMETHOD    SetHref(const nsString& aHref) { return _to SetHref(aHref); } \
  NS_IMETHOD    GetHreflang(nsString& aHreflang) { return _to GetHreflang(aHreflang); } \
  NS_IMETHOD    SetHreflang(const nsString& aHreflang) { return _to SetHreflang(aHreflang); } \
  NS_IMETHOD    GetMedia(nsString& aMedia) { return _to GetMedia(aMedia); } \
  NS_IMETHOD    SetMedia(const nsString& aMedia) { return _to SetMedia(aMedia); } \
  NS_IMETHOD    GetRel(nsString& aRel) { return _to GetRel(aRel); } \
  NS_IMETHOD    SetRel(const nsString& aRel) { return _to SetRel(aRel); } \
  NS_IMETHOD    GetRev(nsString& aRev) { return _to GetRev(aRev); } \
  NS_IMETHOD    SetRev(const nsString& aRev) { return _to SetRev(aRev); } \
  NS_IMETHOD    GetTarget(nsString& aTarget) { return _to GetTarget(aTarget); } \
  NS_IMETHOD    SetTarget(const nsString& aTarget) { return _to SetTarget(aTarget); } \
  NS_IMETHOD    GetType(nsString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    SetType(const nsString& aType) { return _to SetType(aType); } \


extern "C" NS_DOM nsresult NS_InitHTMLLinkElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLLinkElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLLinkElement_h__
