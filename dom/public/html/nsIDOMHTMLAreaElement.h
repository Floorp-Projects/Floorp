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

class nsIDOMHTMLAreaElement;

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

extern nsresult NS_InitHTMLAreaElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLAreaElement(nsIScriptContext *aContext, nsIDOMHTMLAreaElement *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLAreaElement_h__
