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

#ifndef nsIDOMHTMLAnchorElement_h__
#define nsIDOMHTMLAnchorElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLANCHORELEMENT_IID \
 { 0xa6cf90aa, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLAnchorElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLANCHORELEMENT_IID; return iid; }

  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey)=0;
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey)=0;

  NS_IMETHOD    GetCharset(nsAWritableString& aCharset)=0;
  NS_IMETHOD    SetCharset(const nsAReadableString& aCharset)=0;

  NS_IMETHOD    GetCoords(nsAWritableString& aCoords)=0;
  NS_IMETHOD    SetCoords(const nsAReadableString& aCoords)=0;

  NS_IMETHOD    GetHref(nsAWritableString& aHref)=0;
  NS_IMETHOD    SetHref(const nsAReadableString& aHref)=0;

  NS_IMETHOD    GetHreflang(nsAWritableString& aHreflang)=0;
  NS_IMETHOD    SetHreflang(const nsAReadableString& aHreflang)=0;

  NS_IMETHOD    GetName(nsAWritableString& aName)=0;
  NS_IMETHOD    SetName(const nsAReadableString& aName)=0;

  NS_IMETHOD    GetRel(nsAWritableString& aRel)=0;
  NS_IMETHOD    SetRel(const nsAReadableString& aRel)=0;

  NS_IMETHOD    GetRev(nsAWritableString& aRev)=0;
  NS_IMETHOD    SetRev(const nsAReadableString& aRev)=0;

  NS_IMETHOD    GetShape(nsAWritableString& aShape)=0;
  NS_IMETHOD    SetShape(const nsAReadableString& aShape)=0;

  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex)=0;
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex)=0;

  NS_IMETHOD    GetTarget(nsAWritableString& aTarget)=0;
  NS_IMETHOD    SetTarget(const nsAReadableString& aTarget)=0;

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;
  NS_IMETHOD    SetType(const nsAReadableString& aType)=0;

  NS_IMETHOD    Blur()=0;

  NS_IMETHOD    Focus()=0;
};


#define NS_DECL_IDOMHTMLANCHORELEMENT   \
  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey);  \
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey);  \
  NS_IMETHOD    GetCharset(nsAWritableString& aCharset);  \
  NS_IMETHOD    SetCharset(const nsAReadableString& aCharset);  \
  NS_IMETHOD    GetCoords(nsAWritableString& aCoords);  \
  NS_IMETHOD    SetCoords(const nsAReadableString& aCoords);  \
  NS_IMETHOD    GetHref(nsAWritableString& aHref);  \
  NS_IMETHOD    SetHref(const nsAReadableString& aHref);  \
  NS_IMETHOD    GetHreflang(nsAWritableString& aHreflang);  \
  NS_IMETHOD    SetHreflang(const nsAReadableString& aHreflang);  \
  NS_IMETHOD    GetName(nsAWritableString& aName);  \
  NS_IMETHOD    SetName(const nsAReadableString& aName);  \
  NS_IMETHOD    GetRel(nsAWritableString& aRel);  \
  NS_IMETHOD    SetRel(const nsAReadableString& aRel);  \
  NS_IMETHOD    GetRev(nsAWritableString& aRev);  \
  NS_IMETHOD    SetRev(const nsAReadableString& aRev);  \
  NS_IMETHOD    GetShape(nsAWritableString& aShape);  \
  NS_IMETHOD    SetShape(const nsAReadableString& aShape);  \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex);  \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex);  \
  NS_IMETHOD    GetTarget(nsAWritableString& aTarget);  \
  NS_IMETHOD    SetTarget(const nsAReadableString& aTarget);  \
  NS_IMETHOD    GetType(nsAWritableString& aType);  \
  NS_IMETHOD    SetType(const nsAReadableString& aType);  \
  NS_IMETHOD    Blur();  \
  NS_IMETHOD    Focus();  \



#define NS_FORWARD_IDOMHTMLANCHORELEMENT(_to)  \
  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey) { return _to GetAccessKey(aAccessKey); } \
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey) { return _to SetAccessKey(aAccessKey); } \
  NS_IMETHOD    GetCharset(nsAWritableString& aCharset) { return _to GetCharset(aCharset); } \
  NS_IMETHOD    SetCharset(const nsAReadableString& aCharset) { return _to SetCharset(aCharset); } \
  NS_IMETHOD    GetCoords(nsAWritableString& aCoords) { return _to GetCoords(aCoords); } \
  NS_IMETHOD    SetCoords(const nsAReadableString& aCoords) { return _to SetCoords(aCoords); } \
  NS_IMETHOD    GetHref(nsAWritableString& aHref) { return _to GetHref(aHref); } \
  NS_IMETHOD    SetHref(const nsAReadableString& aHref) { return _to SetHref(aHref); } \
  NS_IMETHOD    GetHreflang(nsAWritableString& aHreflang) { return _to GetHreflang(aHreflang); } \
  NS_IMETHOD    SetHreflang(const nsAReadableString& aHreflang) { return _to SetHreflang(aHreflang); } \
  NS_IMETHOD    GetName(nsAWritableString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsAReadableString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetRel(nsAWritableString& aRel) { return _to GetRel(aRel); } \
  NS_IMETHOD    SetRel(const nsAReadableString& aRel) { return _to SetRel(aRel); } \
  NS_IMETHOD    GetRev(nsAWritableString& aRev) { return _to GetRev(aRev); } \
  NS_IMETHOD    SetRev(const nsAReadableString& aRev) { return _to SetRev(aRev); } \
  NS_IMETHOD    GetShape(nsAWritableString& aShape) { return _to GetShape(aShape); } \
  NS_IMETHOD    SetShape(const nsAReadableString& aShape) { return _to SetShape(aShape); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return _to GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return _to SetTabIndex(aTabIndex); } \
  NS_IMETHOD    GetTarget(nsAWritableString& aTarget) { return _to GetTarget(aTarget); } \
  NS_IMETHOD    SetTarget(const nsAReadableString& aTarget) { return _to SetTarget(aTarget); } \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    SetType(const nsAReadableString& aType) { return _to SetType(aType); } \
  NS_IMETHOD    Blur() { return _to Blur(); }  \
  NS_IMETHOD    Focus() { return _to Focus(); }  \


extern "C" NS_DOM nsresult NS_InitHTMLAnchorElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLAnchorElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLAnchorElement_h__
