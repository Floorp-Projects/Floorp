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

#ifndef nsIDOMHTMLHtmlElement_h__
#define nsIDOMHTMLHtmlElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLHTMLELEMENT_IID \
 { 0xa6cf9086, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLHtmlElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLHTMLELEMENT_IID; return iid; }

  NS_IMETHOD    GetVersion(nsString& aVersion)=0;
  NS_IMETHOD    SetVersion(const nsString& aVersion)=0;
};


#define NS_DECL_IDOMHTMLHTMLELEMENT   \
  NS_IMETHOD    GetVersion(nsString& aVersion);  \
  NS_IMETHOD    SetVersion(const nsString& aVersion);  \



#define NS_FORWARD_IDOMHTMLHTMLELEMENT(_to)  \
  NS_IMETHOD    GetVersion(nsString& aVersion) { return _to GetVersion(aVersion); } \
  NS_IMETHOD    SetVersion(const nsString& aVersion) { return _to SetVersion(aVersion); } \


extern "C" NS_DOM nsresult NS_InitHTMLHtmlElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLHtmlElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLHtmlElement_h__
