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

#ifndef nsIDOMHTMLHeadingElement_h__
#define nsIDOMHTMLHeadingElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLHEADINGELEMENT_IID \
 { 0xa6cf90a2, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLHeadingElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLHEADINGELEMENT_IID; return iid; }

  NS_IMETHOD    GetAlign(nsAWritableString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign)=0;
};


#define NS_DECL_IDOMHTMLHEADINGELEMENT   \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign);  \



#define NS_FORWARD_IDOMHTMLHEADINGELEMENT(_to)  \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign) { return _to SetAlign(aAlign); } \


extern "C" NS_DOM nsresult NS_InitHTMLHeadingElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLHeadingElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLHeadingElement_h__
