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
 { 0xa6cf90b0, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLAreaElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLAREAELEMENT_IID; return iid; }

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



#define NS_FORWARD_IDOMHTMLAREAELEMENT(_to)  \
  NS_IMETHOD    GetAccessKey(nsString& aAccessKey) { return _to GetAccessKey(aAccessKey); } \
  NS_IMETHOD    SetAccessKey(const nsString& aAccessKey) { return _to SetAccessKey(aAccessKey); } \
  NS_IMETHOD    GetAlt(nsString& aAlt) { return _to GetAlt(aAlt); } \
  NS_IMETHOD    SetAlt(const nsString& aAlt) { return _to SetAlt(aAlt); } \
  NS_IMETHOD    GetCoords(nsString& aCoords) { return _to GetCoords(aCoords); } \
  NS_IMETHOD    SetCoords(const nsString& aCoords) { return _to SetCoords(aCoords); } \
  NS_IMETHOD    GetHref(nsString& aHref) { return _to GetHref(aHref); } \
  NS_IMETHOD    SetHref(const nsString& aHref) { return _to SetHref(aHref); } \
  NS_IMETHOD    GetNoHref(PRBool* aNoHref) { return _to GetNoHref(aNoHref); } \
  NS_IMETHOD    SetNoHref(PRBool aNoHref) { return _to SetNoHref(aNoHref); } \
  NS_IMETHOD    GetShape(nsString& aShape) { return _to GetShape(aShape); } \
  NS_IMETHOD    SetShape(const nsString& aShape) { return _to SetShape(aShape); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return _to GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return _to SetTabIndex(aTabIndex); } \
  NS_IMETHOD    GetTarget(nsString& aTarget) { return _to GetTarget(aTarget); } \
  NS_IMETHOD    SetTarget(const nsString& aTarget) { return _to SetTarget(aTarget); } \


extern "C" NS_DOM nsresult NS_InitHTMLAreaElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLAreaElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLAreaElement_h__
