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

  NS_IMETHOD    GetCharset(nsAWritableString& aCharset)=0;
  NS_IMETHOD    SetCharset(const nsAReadableString& aCharset)=0;

  NS_IMETHOD    GetHref(nsAWritableString& aHref)=0;
  NS_IMETHOD    SetHref(const nsAReadableString& aHref)=0;

  NS_IMETHOD    GetHreflang(nsAWritableString& aHreflang)=0;
  NS_IMETHOD    SetHreflang(const nsAReadableString& aHreflang)=0;

  NS_IMETHOD    GetMedia(nsAWritableString& aMedia)=0;
  NS_IMETHOD    SetMedia(const nsAReadableString& aMedia)=0;

  NS_IMETHOD    GetRel(nsAWritableString& aRel)=0;
  NS_IMETHOD    SetRel(const nsAReadableString& aRel)=0;

  NS_IMETHOD    GetRev(nsAWritableString& aRev)=0;
  NS_IMETHOD    SetRev(const nsAReadableString& aRev)=0;

  NS_IMETHOD    GetTarget(nsAWritableString& aTarget)=0;
  NS_IMETHOD    SetTarget(const nsAReadableString& aTarget)=0;

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;
  NS_IMETHOD    SetType(const nsAReadableString& aType)=0;
};


#define NS_DECL_IDOMHTMLLINKELEMENT   \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetCharset(nsAWritableString& aCharset);  \
  NS_IMETHOD    SetCharset(const nsAReadableString& aCharset);  \
  NS_IMETHOD    GetHref(nsAWritableString& aHref);  \
  NS_IMETHOD    SetHref(const nsAReadableString& aHref);  \
  NS_IMETHOD    GetHreflang(nsAWritableString& aHreflang);  \
  NS_IMETHOD    SetHreflang(const nsAReadableString& aHreflang);  \
  NS_IMETHOD    GetMedia(nsAWritableString& aMedia);  \
  NS_IMETHOD    SetMedia(const nsAReadableString& aMedia);  \
  NS_IMETHOD    GetRel(nsAWritableString& aRel);  \
  NS_IMETHOD    SetRel(const nsAReadableString& aRel);  \
  NS_IMETHOD    GetRev(nsAWritableString& aRev);  \
  NS_IMETHOD    SetRev(const nsAReadableString& aRev);  \
  NS_IMETHOD    GetTarget(nsAWritableString& aTarget);  \
  NS_IMETHOD    SetTarget(const nsAReadableString& aTarget);  \
  NS_IMETHOD    GetType(nsAWritableString& aType);  \
  NS_IMETHOD    SetType(const nsAReadableString& aType);  \



#define NS_FORWARD_IDOMHTMLLINKELEMENT(_to)  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return _to GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return _to SetDisabled(aDisabled); } \
  NS_IMETHOD    GetCharset(nsAWritableString& aCharset) { return _to GetCharset(aCharset); } \
  NS_IMETHOD    SetCharset(const nsAReadableString& aCharset) { return _to SetCharset(aCharset); } \
  NS_IMETHOD    GetHref(nsAWritableString& aHref) { return _to GetHref(aHref); } \
  NS_IMETHOD    SetHref(const nsAReadableString& aHref) { return _to SetHref(aHref); } \
  NS_IMETHOD    GetHreflang(nsAWritableString& aHreflang) { return _to GetHreflang(aHreflang); } \
  NS_IMETHOD    SetHreflang(const nsAReadableString& aHreflang) { return _to SetHreflang(aHreflang); } \
  NS_IMETHOD    GetMedia(nsAWritableString& aMedia) { return _to GetMedia(aMedia); } \
  NS_IMETHOD    SetMedia(const nsAReadableString& aMedia) { return _to SetMedia(aMedia); } \
  NS_IMETHOD    GetRel(nsAWritableString& aRel) { return _to GetRel(aRel); } \
  NS_IMETHOD    SetRel(const nsAReadableString& aRel) { return _to SetRel(aRel); } \
  NS_IMETHOD    GetRev(nsAWritableString& aRev) { return _to GetRev(aRev); } \
  NS_IMETHOD    SetRev(const nsAReadableString& aRev) { return _to SetRev(aRev); } \
  NS_IMETHOD    GetTarget(nsAWritableString& aTarget) { return _to GetTarget(aTarget); } \
  NS_IMETHOD    SetTarget(const nsAReadableString& aTarget) { return _to SetTarget(aTarget); } \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    SetType(const nsAReadableString& aType) { return _to SetType(aType); } \


extern "C" NS_DOM nsresult NS_InitHTMLLinkElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLLinkElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLLinkElement_h__
