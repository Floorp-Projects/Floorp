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

#ifndef nsIDOMHTMLAreaElement_h__
#define nsIDOMHTMLAreaElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLAREAELEMENT_IID \
{ 0x6f7652f0,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMHTMLAreaElement : public nsIDOMHTMLElement {
public:

  NS_IMETHOD    GetAccessKey(nsString& aAccessKey)=0;
  NS_IMETHOD    SetAccessKey(const nsString& aAccessKey)=0;

  NS_IMETHOD    GetAlt(nsString& aAlt)=0;
  NS_IMETHOD    SetAlt(const nsString& aAlt)=0;

  NS_IMETHOD    GetCoords(nsString& aCoords)=0;
  NS_IMETHOD    SetCoords(const nsString& aCoords)=0;

  NS_IMETHOD    GetHref(nsString& aHref)=0;
  NS_IMETHOD    SetHref(const nsString& aHref)=0;

  NS_IMETHOD    GetNoHref(PRBool* aNoHref)=0;
  NS_IMETHOD    SetNoHref(PRBool aNoHref)=0;

  NS_IMETHOD    GetShape(nsString& aShape)=0;
  NS_IMETHOD    SetShape(const nsString& aShape)=0;

  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex)=0;
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex)=0;

  NS_IMETHOD    GetTarget(nsString& aTarget)=0;
  NS_IMETHOD    SetTarget(const nsString& aTarget)=0;
};


#define NS_DECL_IDOMHTMLAREAELEMENT   \
  NS_IMETHOD    GetAccessKey(nsString& aAccessKey);  \
  NS_IMETHOD    SetAccessKey(const nsString& aAccessKey);  \
  NS_IMETHOD    GetAlt(nsString& aAlt);  \
  NS_IMETHOD    SetAlt(const nsString& aAlt);  \
  NS_IMETHOD    GetCoords(nsString& aCoords);  \
  NS_IMETHOD    SetCoords(const nsString& aCoords);  \
  NS_IMETHOD    GetHref(nsString& aHref);  \
  NS_IMETHOD    SetHref(const nsString& aHref);  \
  NS_IMETHOD    GetNoHref(PRBool* aNoHref);  \
  NS_IMETHOD    SetNoHref(PRBool aNoHref);  \
  NS_IMETHOD    GetShape(nsString& aShape);  \
  NS_IMETHOD    SetShape(const nsString& aShape);  \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex);  \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex);  \
  NS_IMETHOD    GetTarget(nsString& aTarget);  \
  NS_IMETHOD    SetTarget(const nsString& aTarget);  \



#define NS_FORWARD_IDOMHTMLAREAELEMENT(superClass)  \
  NS_IMETHOD    GetAccessKey(nsString& aAccessKey) { return superClass::GetAccessKey(aAccessKey); } \
  NS_IMETHOD    SetAccessKey(const nsString& aAccessKey) { return superClass::SetAccessKey(aAccessKey); } \
  NS_IMETHOD    GetAlt(nsString& aAlt) { return superClass::GetAlt(aAlt); } \
  NS_IMETHOD    SetAlt(const nsString& aAlt) { return superClass::SetAlt(aAlt); } \
  NS_IMETHOD    GetCoords(nsString& aCoords) { return superClass::GetCoords(aCoords); } \
  NS_IMETHOD    SetCoords(const nsString& aCoords) { return superClass::SetCoords(aCoords); } \
  NS_IMETHOD    GetHref(nsString& aHref) { return superClass::GetHref(aHref); } \
  NS_IMETHOD    SetHref(const nsString& aHref) { return superClass::SetHref(aHref); } \
  NS_IMETHOD    GetNoHref(PRBool* aNoHref) { return superClass::GetNoHref(aNoHref); } \
  NS_IMETHOD    SetNoHref(PRBool aNoHref) { return superClass::SetNoHref(aNoHref); } \
  NS_IMETHOD    GetShape(nsString& aShape) { return superClass::GetShape(aShape); } \
  NS_IMETHOD    SetShape(const nsString& aShape) { return superClass::SetShape(aShape); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return superClass::GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return superClass::SetTabIndex(aTabIndex); } \
  NS_IMETHOD    GetTarget(nsString& aTarget) { return superClass::GetTarget(aTarget); } \
  NS_IMETHOD    SetTarget(const nsString& aTarget) { return superClass::SetTarget(aTarget); } \


extern nsresult NS_InitHTMLAreaElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLAreaElement(nsIScriptContext *aContext, nsIDOMHTMLAreaElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLAreaElement_h__
