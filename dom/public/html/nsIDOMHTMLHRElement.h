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

#ifndef nsIDOMHTMLHRElement_h__
#define nsIDOMHTMLHRElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLHRELEMENT_IID \
 { 0xa6cf90a8, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLHRElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLHRELEMENT_IID; return iid; }

  NS_IMETHOD    GetAlign(nsAWritableString& aAlign)=0;
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign)=0;

  NS_IMETHOD    GetNoShade(PRBool* aNoShade)=0;
  NS_IMETHOD    SetNoShade(PRBool aNoShade)=0;

  NS_IMETHOD    GetSize(nsAWritableString& aSize)=0;
  NS_IMETHOD    SetSize(const nsAReadableString& aSize)=0;

  NS_IMETHOD    GetWidth(nsAWritableString& aWidth)=0;
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth)=0;
};


#define NS_DECL_IDOMHTMLHRELEMENT   \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign);  \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign);  \
  NS_IMETHOD    GetNoShade(PRBool* aNoShade);  \
  NS_IMETHOD    SetNoShade(PRBool aNoShade);  \
  NS_IMETHOD    GetSize(nsAWritableString& aSize);  \
  NS_IMETHOD    SetSize(const nsAReadableString& aSize);  \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth);  \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth);  \



#define NS_FORWARD_IDOMHTMLHRELEMENT(_to)  \
  NS_IMETHOD    GetAlign(nsAWritableString& aAlign) { return _to GetAlign(aAlign); } \
  NS_IMETHOD    SetAlign(const nsAReadableString& aAlign) { return _to SetAlign(aAlign); } \
  NS_IMETHOD    GetNoShade(PRBool* aNoShade) { return _to GetNoShade(aNoShade); } \
  NS_IMETHOD    SetNoShade(PRBool aNoShade) { return _to SetNoShade(aNoShade); } \
  NS_IMETHOD    GetSize(nsAWritableString& aSize) { return _to GetSize(aSize); } \
  NS_IMETHOD    SetSize(const nsAReadableString& aSize) { return _to SetSize(aSize); } \
  NS_IMETHOD    GetWidth(nsAWritableString& aWidth) { return _to GetWidth(aWidth); } \
  NS_IMETHOD    SetWidth(const nsAReadableString& aWidth) { return _to SetWidth(aWidth); } \


extern "C" NS_DOM nsresult NS_InitHTMLHRElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLHRElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLHRElement_h__
