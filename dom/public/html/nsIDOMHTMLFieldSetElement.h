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

#ifndef nsIDOMHTMLFieldSetElement_h__
#define nsIDOMHTMLFieldSetElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"

class nsIDOMHTMLFormElement;

#define NS_IDOMHTMLFIELDSETELEMENT_IID \
 { 0xa6cf9097, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLFieldSetElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLFIELDSETELEMENT_IID; return iid; }

  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm)=0;
};


#define NS_DECL_IDOMHTMLFIELDSETELEMENT   \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm);  \



#define NS_FORWARD_IDOMHTMLFIELDSETELEMENT(_to)  \
  NS_IMETHOD    GetForm(nsIDOMHTMLFormElement** aForm) { return _to GetForm(aForm); } \


extern "C" NS_DOM nsresult NS_InitHTMLFieldSetElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLFieldSetElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLFieldSetElement_h__
