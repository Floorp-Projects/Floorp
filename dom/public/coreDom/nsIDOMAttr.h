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

#ifndef nsIDOMAttr_h__
#define nsIDOMAttr_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMNode.h"

class nsIDOMElement;

#define NS_IDOMATTR_IID \
 { 0xa6cf9070, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMAttr : public nsIDOMNode {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMATTR_IID; return iid; }

  NS_IMETHOD    GetName(nsAWritableString& aName)=0;

  NS_IMETHOD    GetSpecified(PRBool* aSpecified)=0;

  NS_IMETHOD    GetValue(nsAWritableString& aValue)=0;
  NS_IMETHOD    SetValue(const nsAReadableString& aValue)=0;

  NS_IMETHOD    GetOwnerElement(nsIDOMElement** aOwnerElement)=0;
};


#define NS_DECL_IDOMATTR   \
  NS_IMETHOD    GetName(nsAWritableString& aName);  \
  NS_IMETHOD    GetSpecified(PRBool* aSpecified);  \
  NS_IMETHOD    GetValue(nsAWritableString& aValue);  \
  NS_IMETHOD    SetValue(const nsAReadableString& aValue);  \
  NS_IMETHOD    GetOwnerElement(nsIDOMElement** aOwnerElement);  \



#define NS_FORWARD_IDOMATTR(_to)  \
  NS_IMETHOD    GetName(nsAWritableString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    GetSpecified(PRBool* aSpecified) { return _to GetSpecified(aSpecified); } \
  NS_IMETHOD    GetValue(nsAWritableString& aValue) { return _to GetValue(aValue); } \
  NS_IMETHOD    SetValue(const nsAReadableString& aValue) { return _to SetValue(aValue); } \
  NS_IMETHOD    GetOwnerElement(nsIDOMElement** aOwnerElement) { return _to GetOwnerElement(aOwnerElement); } \


extern "C" NS_DOM nsresult NS_InitAttrClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptAttr(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMAttr_h__
