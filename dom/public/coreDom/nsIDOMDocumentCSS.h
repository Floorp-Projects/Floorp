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

#ifndef nsIDOMDocumentCSS_h__
#define nsIDOMDocumentCSS_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMDocumentStyle.h"

class nsIDOMElement;
class nsIDOMCSSStyleDeclaration;

#define NS_IDOMDOCUMENTCSS_IID \
 { 0x39f76c23, 0x45b2, 0x428a, \
  { 0x92, 0x40, 0xa9, 0x81, 0xe5, 0xab, 0xf1, 0x48 } } 

class nsIDOMDocumentCSS : public nsIDOMDocumentStyle {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMDOCUMENTCSS_IID; return iid; }

  NS_IMETHOD    GetOverrideStyle(nsIDOMElement* aElt, const nsAReadableString& aPseudoElt, nsIDOMCSSStyleDeclaration** aReturn)=0;
};


#define NS_DECL_IDOMDOCUMENTCSS   \
  NS_IMETHOD    GetOverrideStyle(nsIDOMElement* aElt, const nsAReadableString& aPseudoElt, nsIDOMCSSStyleDeclaration** aReturn);  \



#define NS_FORWARD_IDOMDOCUMENTCSS(_to)  \
  NS_IMETHOD    GetOverrideStyle(nsIDOMElement* aElt, const nsAReadableString& aPseudoElt, nsIDOMCSSStyleDeclaration** aReturn) { return _to GetOverrideStyle(aElt, aPseudoElt, aReturn); }  \


#endif // nsIDOMDocumentCSS_h__
