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

#ifndef nsIDOMAttribute_h__
#define nsIDOMAttribute_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMNode.h"


#define NS_IDOMATTRIBUTE_IID \
{ 0x6f7652e0,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMAttribute : public nsIDOMNode {
public:

  NS_IMETHOD    GetName(nsString& aName)=0;

  NS_IMETHOD    GetSpecified(PRBool* aSpecified)=0;
  NS_IMETHOD    SetSpecified(PRBool aSpecified)=0;

  NS_IMETHOD    GetValue(nsString& aValue)=0;
};


#define NS_DECL_IDOMATTRIBUTE   \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    GetSpecified(PRBool* aSpecified);  \
  NS_IMETHOD    SetSpecified(PRBool aSpecified);  \
  NS_IMETHOD    GetValue(nsString& aValue);  \



#define NS_FORWARD_IDOMATTRIBUTE(_to)  \
  NS_IMETHOD    GetName(nsString& aName) { return _to##GetName(aName); } \
  NS_IMETHOD    GetSpecified(PRBool* aSpecified) { return _to##GetSpecified(aSpecified); } \
  NS_IMETHOD    SetSpecified(PRBool aSpecified) { return _to##SetSpecified(aSpecified); } \
  NS_IMETHOD    GetValue(nsString& aValue) { return _to##GetValue(aValue); } \


extern nsresult NS_InitAttributeClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptAttribute(nsIScriptContext *aContext, nsIDOMAttribute *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMAttribute_h__
