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

#ifndef nsIDOMHTMLTitleElement_h__
#define nsIDOMHTMLTitleElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLTITLEELEMENT_IID \
 { 0xa6cf9089, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLTitleElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLTITLEELEMENT_IID; return iid; }

  NS_IMETHOD    GetText(nsAWritableString& aText)=0;
  NS_IMETHOD    SetText(const nsAReadableString& aText)=0;
};


#define NS_DECL_IDOMHTMLTITLEELEMENT   \
  NS_IMETHOD    GetText(nsAWritableString& aText);  \
  NS_IMETHOD    SetText(const nsAReadableString& aText);  \



#define NS_FORWARD_IDOMHTMLTITLEELEMENT(_to)  \
  NS_IMETHOD    GetText(nsAWritableString& aText) { return _to GetText(aText); } \
  NS_IMETHOD    SetText(const nsAReadableString& aText) { return _to SetText(aText); } \


extern "C" NS_DOM nsresult NS_InitHTMLTitleElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLTitleElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLTitleElement_h__
