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

#ifndef nsIDOMHTMLPreElement_h__
#define nsIDOMHTMLPreElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLPREELEMENT_IID \
 { 0xa6cf90a4, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLPreElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLPREELEMENT_IID; return iid; }

  NS_IMETHOD    GetWidth(PRInt32* aWidth)=0;
  NS_IMETHOD    SetWidth(PRInt32 aWidth)=0;
};


#define NS_DECL_IDOMHTMLPREELEMENT   \
  NS_IMETHOD    GetWidth(PRInt32* aWidth);  \
  NS_IMETHOD    SetWidth(PRInt32 aWidth);  \



#define NS_FORWARD_IDOMHTMLPREELEMENT(_to)  \
  NS_IMETHOD    GetWidth(PRInt32* aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(PRInt32 aWidth) { return _to SetWidth(aWidth); } \


extern "C" NS_DOM nsresult NS_InitHTMLPreElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLPreElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLPreElement_h__
