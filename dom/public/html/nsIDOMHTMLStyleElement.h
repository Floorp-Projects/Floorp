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

#ifndef nsIDOMHTMLStyleElement_h__
#define nsIDOMHTMLStyleElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLSTYLEELEMENT_IID \
 { 0xa6cf908d, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLStyleElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLSTYLEELEMENT_IID; return iid; }

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetMedia(nsAWritableString& aMedia)=0;
  NS_IMETHOD    SetMedia(const nsAReadableString& aMedia)=0;

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;
  NS_IMETHOD    SetType(const nsAReadableString& aType)=0;
};


#define NS_DECL_IDOMHTMLSTYLEELEMENT   \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetMedia(nsAWritableString& aMedia);  \
  NS_IMETHOD    SetMedia(const nsAReadableString& aMedia);  \
  NS_IMETHOD    GetType(nsAWritableString& aType);  \
  NS_IMETHOD    SetType(const nsAReadableString& aType);  \



#define NS_FORWARD_IDOMHTMLSTYLEELEMENT(_to)  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return _to GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return _to SetDisabled(aDisabled); } \
  NS_IMETHOD    GetMedia(nsAWritableString& aMedia) { return _to GetMedia(aMedia); } \
  NS_IMETHOD    SetMedia(const nsAReadableString& aMedia) { return _to SetMedia(aMedia); } \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    SetType(const nsAReadableString& aType) { return _to SetType(aType); } \


extern "C" NS_DOM nsresult NS_InitHTMLStyleElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLStyleElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLStyleElement_h__
