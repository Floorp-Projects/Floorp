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

  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey)=0;
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey)=0;

  NS_IMETHOD    GetAlt(nsAWritableString& aAlt)=0;
  NS_IMETHOD    SetAlt(const nsAReadableString& aAlt)=0;

  NS_IMETHOD    GetCoords(nsAWritableString& aCoords)=0;
  NS_IMETHOD    SetCoords(const nsAReadableString& aCoords)=0;

  NS_IMETHOD    GetHref(nsAWritableString& aHref)=0;
  NS_IMETHOD    SetHref(const nsAReadableString& aHref)=0;

  NS_IMETHOD    GetNoHref(PRBool* aNoHref)=0;
  NS_IMETHOD    SetNoHref(PRBool aNoHref)=0;

  NS_IMETHOD    GetShape(nsAWritableString& aShape)=0;
  NS_IMETHOD    SetShape(const nsAReadableString& aShape)=0;

  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex)=0;
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex)=0;

  NS_IMETHOD    GetTarget(nsAWritableString& aTarget)=0;
  NS_IMETHOD    SetTarget(const nsAReadableString& aTarget)=0;
};


#define NS_DECL_IDOMHTMLAREAELEMENT   \
  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey);  \
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey);  \
  NS_IMETHOD    GetAlt(nsAWritableString& aAlt);  \
  NS_IMETHOD    SetAlt(const nsAReadableString& aAlt);  \
  NS_IMETHOD    GetCoords(nsAWritableString& aCoords);  \
  NS_IMETHOD    SetCoords(const nsAReadableString& aCoords);  \
  NS_IMETHOD    GetHref(nsAWritableString& aHref);  \
  NS_IMETHOD    SetHref(const nsAReadableString& aHref);  \
  NS_IMETHOD    GetNoHref(PRBool* aNoHref);  \
  NS_IMETHOD    SetNoHref(PRBool aNoHref);  \
  NS_IMETHOD    GetShape(nsAWritableString& aShape);  \
  NS_IMETHOD    SetShape(const nsAReadableString& aShape);  \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex);  \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex);  \
  NS_IMETHOD    GetTarget(nsAWritableString& aTarget);  \
  NS_IMETHOD    SetTarget(const nsAReadableString& aTarget);  \



#define NS_FORWARD_IDOMHTMLAREAELEMENT(_to)  \
  NS_IMETHOD    GetAccessKey(nsAWritableString& aAccessKey) { return _to GetAccessKey(aAccessKey); } \
  NS_IMETHOD    SetAccessKey(const nsAReadableString& aAccessKey) { return _to SetAccessKey(aAccessKey); } \
  NS_IMETHOD    GetAlt(nsAWritableString& aAlt) { return _to GetAlt(aAlt); } \
  NS_IMETHOD    SetAlt(const nsAReadableString& aAlt) { return _to SetAlt(aAlt); } \
  NS_IMETHOD    GetCoords(nsAWritableString& aCoords) { return _to GetCoords(aCoords); } \
  NS_IMETHOD    SetCoords(const nsAReadableString& aCoords) { return _to SetCoords(aCoords); } \
  NS_IMETHOD    GetHref(nsAWritableString& aHref) { return _to GetHref(aHref); } \
  NS_IMETHOD    SetHref(const nsAReadableString& aHref) { return _to SetHref(aHref); } \
  NS_IMETHOD    GetNoHref(PRBool* aNoHref) { return _to GetNoHref(aNoHref); } \
  NS_IMETHOD    SetNoHref(PRBool aNoHref) { return _to SetNoHref(aNoHref); } \
  NS_IMETHOD    GetShape(nsAWritableString& aShape) { return _to GetShape(aShape); } \
  NS_IMETHOD    SetShape(const nsAReadableString& aShape) { return _to SetShape(aShape); } \
  NS_IMETHOD    GetTabIndex(PRInt32* aTabIndex) { return _to GetTabIndex(aTabIndex); } \
  NS_IMETHOD    SetTabIndex(PRInt32 aTabIndex) { return _to SetTabIndex(aTabIndex); } \
  NS_IMETHOD    GetTarget(nsAWritableString& aTarget) { return _to GetTarget(aTarget); } \
  NS_IMETHOD    SetTarget(const nsAReadableString& aTarget) { return _to SetTarget(aTarget); } \


extern "C" NS_DOM nsresult NS_InitHTMLAreaElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLAreaElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLAreaElement_h__
