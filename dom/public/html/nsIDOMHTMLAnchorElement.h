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
{ 0x6f7652ee,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLAnchorElement : public nsIDOMHTMLElement {
public:

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



#define NS_FORWARD_IDOMHTMLANCHORELEMENT(superClass)  \
  NS_IMETHOD    GetAccessKey(nsString& aAccessKey) { return superClass::GetAccessKey(aAccessKey); } \
  NS_IMETHOD    SetAccessKey(const nsString& aAccessKey) { return superClass::SetAccessKey(aAccessKey); } \
  NS_IMETHOD    GetCharset(nsString& aCharset) { return superClass::GetCharset(aCharset); } \
  NS_IMETHOD    SetCharset(const nsString& aCharset) { return superClass::SetCharset(aCharset); } \
  NS_IMETHOD    GetCoords(nsString& aCoords) { return superClass::GetCoords(aCoords); } \
  NS_IMETHOD    SetCoords(const nsString& aCoords) { return superClass::SetCoords(aCoords); } \
  NS_IMETHOD    GetHref(nsString& aHref) { return superClass::GetHref(aHref); } \
  NS_IMETHOD    SetHref(const nsString& aHref) { return superClass::SetHref(aHref); } \
  NS_IMETHOD    GetHreflang(nsString& aHreflang) { return superClass::GetHreflang(aHreflang); } \
  NS_IMETHOD    SetHreflang(const nsString& aHreflang) { return superClass::SetHreflang(aHreflang); } \
  NS_IMETHOD    GetName(nsString& aName) { return superClass::GetName(aName); } \
  NS_IMETHOD    SetName(const nsString& aName) { return superClass::SetName(aName); } \
  NS_IMETHOD    GetRel(nsString& aRel) { return superClass::GetRel(aRel); } \
  NS_IMETHOD    SetRel(const nsString& aRel) { return superClass::SetRel(aRel); } \
  NS_IMETHOD    GetRev(nsString& aRev) { return superClass::GetRev(aRev); } \
  NS_IMETHOD    SetRev(const nsString& aRev) { return superClass::SetRev(aRev); } \
  NS_IMETHOD    GetShape(nsString& aShape) { return superClass::GetShape(aShape); } \
  NS_IMETHOD    SetShape(const nsString& aShape) { return superClass::SetShape(aShape); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return superClass::GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return superClass::SetTabIndex(aTabIndex); } \
  NS_IMETHOD    GetTarget(nsString& aTarget) { return superClass::GetTarget(aTarget); } \
  NS_IMETHOD    SetTarget(const nsString& aTarget) { return superClass::SetTarget(aTarget); } \
  NS_IMETHOD    GetType(nsString& aType) { return superClass::GetType(aType); } \
  NS_IMETHOD    SetType(const nsString& aType) { return superClass::SetType(aType); } \
  NS_IMETHOD    Blur() { return superClass::Blur(); }  \
  NS_IMETHOD    Focus() { return superClass::Focus(); }  \


extern nsresult NS_InitHTMLAnchorElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLAnchorElement(nsIScriptContext *aContext, nsIDOMHTMLAnchorElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLAnchorElement_h__
