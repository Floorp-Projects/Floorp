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

  NS_IMETHOD    GetAccessKey(nsString& aAccessKey)=0;
  NS_IMETHOD    SetAccessKey(const nsString& aAccessKey)=0;

  NS_IMETHOD    GetCharset(nsString& aCharset)=0;
  NS_IMETHOD    SetCharset(const nsString& aCharset)=0;

  NS_IMETHOD    GetCoords(nsString& aCoords)=0;
  NS_IMETHOD    SetCoords(const nsString& aCoords)=0;

  NS_IMETHOD    GetHref(nsString& aHref)=0;
  NS_IMETHOD    SetHref(const nsString& aHref)=0;

  NS_IMETHOD    GetHreflang(nsString& aHreflang)=0;
  NS_IMETHOD    SetHreflang(const nsString& aHreflang)=0;

  NS_IMETHOD    GetName(nsString& aName)=0;
  NS_IMETHOD    SetName(const nsString& aName)=0;

  NS_IMETHOD    GetRel(nsString& aRel)=0;
  NS_IMETHOD    SetRel(const nsString& aRel)=0;

  NS_IMETHOD    GetRev(nsString& aRev)=0;
  NS_IMETHOD    SetRev(const nsString& aRev)=0;

  NS_IMETHOD    GetShape(nsString& aShape)=0;
  NS_IMETHOD    SetShape(const nsString& aShape)=0;

  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex)=0;
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex)=0;

  NS_IMETHOD    GetTarget(nsString& aTarget)=0;
  NS_IMETHOD    SetTarget(const nsString& aTarget)=0;

  NS_IMETHOD    GetType(nsString& aType)=0;
  NS_IMETHOD    SetType(const nsString& aType)=0;

  NS_IMETHOD    Blur()=0;

  NS_IMETHOD    Focus()=0;
};


#define NS_DECL_IDOMHTMLANCHORELEMENT   \
  NS_IMETHOD    GetAccessKey(nsString& aAccessKey);  \
  NS_IMETHOD    SetAccessKey(const nsString& aAccessKey);  \
  NS_IMETHOD    GetCharset(nsString& aCharset);  \
  NS_IMETHOD    SetCharset(const nsString& aCharset);  \
  NS_IMETHOD    GetCoords(nsString& aCoords);  \
  NS_IMETHOD    SetCoords(const nsString& aCoords);  \
  NS_IMETHOD    GetHref(nsString& aHref);  \
  NS_IMETHOD    SetHref(const nsString& aHref);  \
  NS_IMETHOD    GetHreflang(nsString& aHreflang);  \
  NS_IMETHOD    SetHreflang(const nsString& aHreflang);  \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    SetName(const nsString& aName);  \
  NS_IMETHOD    GetRel(nsString& aRel);  \
  NS_IMETHOD    SetRel(const nsString& aRel);  \
  NS_IMETHOD    GetRev(nsString& aRev);  \
  NS_IMETHOD    SetRev(const nsString& aRev);  \
  NS_IMETHOD    GetShape(nsString& aShape);  \
  NS_IMETHOD    SetShape(const nsString& aShape);  \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex);  \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex);  \
  NS_IMETHOD    GetTarget(nsString& aTarget);  \
  NS_IMETHOD    SetTarget(const nsString& aTarget);  \
  NS_IMETHOD    GetType(nsString& aType);  \
  NS_IMETHOD    SetType(const nsString& aType);  \
  NS_IMETHOD    Blur();  \
  NS_IMETHOD    Focus();  \



#define NS_FORWARD_IDOMHTMLANCHORELEMENT(_to)  \
  NS_IMETHOD    GetAccessKey(nsString& aAccessKey) { return _to GetAccessKey(aAccessKey); } \
  NS_IMETHOD    SetAccessKey(const nsString& aAccessKey) { return _to SetAccessKey(aAccessKey); } \
  NS_IMETHOD    GetCharset(nsString& aCharset) { return _to GetCharset(aCharset); } \
  NS_IMETHOD    SetCharset(const nsString& aCharset) { return _to SetCharset(aCharset); } \
  NS_IMETHOD    GetCoords(nsString& aCoords) { return _to GetCoords(aCoords); } \
  NS_IMETHOD    SetCoords(const nsString& aCoords) { return _to SetCoords(aCoords); } \
  NS_IMETHOD    GetHref(nsString& aHref) { return _to GetHref(aHref); } \
  NS_IMETHOD    SetHref(const nsString& aHref) { return _to SetHref(aHref); } \
  NS_IMETHOD    GetHreflang(nsString& aHreflang) { return _to GetHreflang(aHreflang); } \
  NS_IMETHOD    SetHreflang(const nsString& aHreflang) { return _to SetHreflang(aHreflang); } \
  NS_IMETHOD    GetName(nsString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetRel(nsString& aRel) { return _to GetRel(aRel); } \
  NS_IMETHOD    SetRel(const nsString& aRel) { return _to SetRel(aRel); } \
  NS_IMETHOD    GetRev(nsString& aRev) { return _to GetRev(aRev); } \
  NS_IMETHOD    SetRev(const nsString& aRev) { return _to SetRev(aRev); } \
  NS_IMETHOD    GetShape(nsString& aShape) { return _to GetShape(aShape); } \
  NS_IMETHOD    SetShape(const nsString& aShape) { return _to SetShape(aShape); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return _to GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return _to SetTabIndex(aTabIndex); } \
  NS_IMETHOD    GetTarget(nsString& aTarget) { return _to GetTarget(aTarget); } \
  NS_IMETHOD    SetTarget(const nsString& aTarget) { return _to SetTarget(aTarget); } \
  NS_IMETHOD    GetType(nsString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    SetType(const nsString& aType) { return _to SetType(aType); } \
  NS_IMETHOD    Blur() { return _to Blur(); }  \
  NS_IMETHOD    Focus() { return _to Focus(); }  \


extern "C" NS_DOM nsresult NS_InitHTMLAnchorElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLAnchorElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLAnchorElement_h__
