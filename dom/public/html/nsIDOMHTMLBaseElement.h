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

#ifndef nsIDOMHTMLBaseElement_h__
#define nsIDOMHTMLBaseElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLBASEELEMENT_IID \
 { 0xa6cf908b, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLBaseElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLBASEELEMENT_IID; return iid; }

  NS_IMETHOD    GetHref(nsString& aHref)=0;
  NS_IMETHOD    SetHref(const nsString& aHref)=0;

  NS_IMETHOD    GetTarget(nsString& aTarget)=0;
  NS_IMETHOD    SetTarget(const nsString& aTarget)=0;
};


#define NS_DECL_IDOMHTMLBASEELEMENT   \
  NS_IMETHOD    GetHref(nsString& aHref);  \
  NS_IMETHOD    SetHref(const nsString& aHref);  \
  NS_IMETHOD    GetTarget(nsString& aTarget);  \
  NS_IMETHOD    SetTarget(const nsString& aTarget);  \



#define NS_FORWARD_IDOMHTMLBASEELEMENT(_to)  \
  NS_IMETHOD    GetHref(nsString& aHref) { return _to GetHref(aHref); } \
  NS_IMETHOD    SetHref(const nsString& aHref) { return _to SetHref(aHref); } \
  NS_IMETHOD    GetTarget(nsString& aTarget) { return _to GetTarget(aTarget); } \
  NS_IMETHOD    SetTarget(const nsString& aTarget) { return _to SetTarget(aTarget); } \


extern "C" NS_DOM nsresult NS_InitHTMLBaseElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLBaseElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLBaseElement_h__
