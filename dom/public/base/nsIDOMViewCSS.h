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

#ifndef nsIDOMViewCSS_h__
#define nsIDOMViewCSS_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMAbstractView.h"

class nsIDOMElement;
class nsIDOMCSSStyleDeclaration;

#define NS_IDOMVIEWCSS_IID \
 { 0x0b9341f3, 0x95d4, 0x4fa4, \
  { 0xad, 0xcd, 0xe1, 0x19, 0xe0, 0xdb, 0x28, 0x89 } } 

class nsIDOMViewCSS : public nsIDOMAbstractView {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMVIEWCSS_IID; return iid; }

  NS_IMETHOD    GetComputedStyle(nsIDOMElement* aElt, const nsAReadableString& aPseudoElt, nsIDOMCSSStyleDeclaration** aReturn)=0;
};


#define NS_DECL_IDOMVIEWCSS   \
  NS_IMETHOD    GetComputedStyle(nsIDOMElement* aElt, const nsAReadableString& aPseudoElt, nsIDOMCSSStyleDeclaration** aReturn);  \



#define NS_FORWARD_IDOMVIEWCSS(_to)  \
  NS_IMETHOD    GetComputedStyle(nsIDOMElement* aElt, const nsAReadableString& aPseudoElt, nsIDOMCSSStyleDeclaration** aReturn) { return _to GetComputedStyle(aElt, aPseudoElt, aReturn); }  \


#endif // nsIDOMViewCSS_h__
