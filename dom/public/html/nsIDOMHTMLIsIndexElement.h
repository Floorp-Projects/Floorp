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

#ifndef nsIDOMHTMLIsIndexElement_h__
#define nsIDOMHTMLIsIndexElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLFormElement;

#define NS_IDOMHTMLISINDEXELEMENT_IID \
 { 0xa6cf908c, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLIsIndexElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLISINDEXELEMENT_IID; return iid; }

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;

  NS_IMETHOD    GetPrompt(nsString& aPrompt)=0;
  NS_IMETHOD    SetPrompt(const nsString& aPrompt)=0;
};


#define NS_DECL_IDOMHTMLISINDEXELEMENT   \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm);  \
  NS_IMETHOD    GetPrompt(nsString& aPrompt);  \
  NS_IMETHOD    SetPrompt(const nsString& aPrompt);  \



#define NS_FORWARD_IDOMHTMLISINDEXELEMENT(_to)  \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm) { return _to GetForm(aForm); } \
  NS_IMETHOD    GetPrompt(nsString& aPrompt) { return _to GetPrompt(aPrompt); } \
  NS_IMETHOD    SetPrompt(const nsString& aPrompt) { return _to SetPrompt(aPrompt); } \


extern "C" NS_DOM nsresult NS_InitHTMLIsIndexElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLIsIndexElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLIsIndexElement_h__
