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

#ifndef nsIDOMHTMLMetaElement_h__
#define nsIDOMHTMLMetaElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLMETAELEMENT_IID \
 { 0xa6cf908a, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLMetaElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLMETAELEMENT_IID; return iid; }

  NS_IMETHOD    GetContent(nsString& aContent)=0;
  NS_IMETHOD    SetContent(const nsString& aContent)=0;

  NS_IMETHOD    GetHttpEquiv(nsString& aHttpEquiv)=0;
  NS_IMETHOD    SetHttpEquiv(const nsString& aHttpEquiv)=0;

  NS_IMETHOD    GetName(nsString& aName)=0;
  NS_IMETHOD    SetName(const nsString& aName)=0;

  NS_IMETHOD    GetScheme(nsString& aScheme)=0;
  NS_IMETHOD    SetScheme(const nsString& aScheme)=0;
};


#define NS_DECL_IDOMHTMLMETAELEMENT   \
  NS_IMETHOD    GetContent(nsString& aContent);  \
  NS_IMETHOD    SetContent(const nsString& aContent);  \
  NS_IMETHOD    GetHttpEquiv(nsString& aHttpEquiv);  \
  NS_IMETHOD    SetHttpEquiv(const nsString& aHttpEquiv);  \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    SetName(const nsString& aName);  \
  NS_IMETHOD    GetScheme(nsString& aScheme);  \
  NS_IMETHOD    SetScheme(const nsString& aScheme);  \



#define NS_FORWARD_IDOMHTMLMETAELEMENT(_to)  \
  NS_IMETHOD    GetContent(nsString& aContent) { return _to GetContent(aContent); } \
  NS_IMETHOD    SetContent(const nsString& aContent) { return _to SetContent(aContent); } \
  NS_IMETHOD    GetHttpEquiv(nsString& aHttpEquiv) { return _to GetHttpEquiv(aHttpEquiv); } \
  NS_IMETHOD    SetHttpEquiv(const nsString& aHttpEquiv) { return _to SetHttpEquiv(aHttpEquiv); } \
  NS_IMETHOD    GetName(nsString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetScheme(nsString& aScheme) { return _to GetScheme(aScheme); } \
  NS_IMETHOD    SetScheme(const nsString& aScheme) { return _to SetScheme(aScheme); } \


extern "C" NS_DOM nsresult NS_InitHTMLMetaElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLMetaElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLMetaElement_h__
