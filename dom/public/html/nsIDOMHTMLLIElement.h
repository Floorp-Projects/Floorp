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

#ifndef nsIDOMHTMLLIElement_h__
#define nsIDOMHTMLLIElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLLIELEMENT_IID \
 { 0xa6cf909e, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLLIElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLLIELEMENT_IID; return iid; }

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;
  NS_IMETHOD    SetType(const nsAReadableString& aType)=0;

  NS_IMETHOD    GetValue(PRInt32* aValue)=0;
  NS_IMETHOD    SetValue(PRInt32 aValue)=0;
};


#define NS_DECL_IDOMHTMLLIELEMENT   \
  NS_IMETHOD    GetType(nsAWritableString& aType);  \
  NS_IMETHOD    SetType(const nsAReadableString& aType);  \
  NS_IMETHOD    GetValue(PRInt32* aValue);  \
  NS_IMETHOD    SetValue(PRInt32 aValue);  \



#define NS_FORWARD_IDOMHTMLLIELEMENT(_to)  \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    SetType(const nsAReadableString& aType) { return _to SetType(aType); } \
  NS_IMETHOD    GetValue(PRInt32* aValue) { return _to GetValue(aValue); } \
  NS_IMETHOD    SetValue(PRInt32 aValue) { return _to SetValue(aValue); } \


extern "C" NS_DOM nsresult NS_InitHTMLLIElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLLIElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLLIElement_h__
