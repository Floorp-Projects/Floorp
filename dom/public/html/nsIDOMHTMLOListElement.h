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

#ifndef nsIDOMHTMLOListElement_h__
#define nsIDOMHTMLOListElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMHTMLElement.h"


#define NS_IDOMHTMLOLISTELEMENT_IID \
 { 0xa6cf909a, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMHTMLOListElement : public nsIDOMHTMLElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMHTMLOLISTELEMENT_IID; return iid; }

  NS_IMETHOD    GetCompact(PRBool* aCompact)=0;
  NS_IMETHOD    SetCompact(PRBool aCompact)=0;

  NS_IMETHOD    GetStart(PRInt32* aStart)=0;
  NS_IMETHOD    SetStart(PRInt32 aStart)=0;

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;
  NS_IMETHOD    SetType(const nsAReadableString& aType)=0;
};


#define NS_DECL_IDOMHTMLOLISTELEMENT   \
  NS_IMETHOD    GetCompact(PRBool* aCompact);  \
  NS_IMETHOD    SetCompact(PRBool aCompact);  \
  NS_IMETHOD    GetStart(PRInt32* aStart);  \
  NS_IMETHOD    SetStart(PRInt32 aStart);  \
  NS_IMETHOD    GetType(nsAWritableString& aType);  \
  NS_IMETHOD    SetType(const nsAReadableString& aType);  \



#define NS_FORWARD_IDOMHTMLOLISTELEMENT(_to)  \
  NS_IMETHOD    GetCompact(PRBool* aCompact) { return _to GetCompact(aCompact); } \
  NS_IMETHOD    SetCompact(PRBool aCompact) { return _to SetCompact(aCompact); } \
  NS_IMETHOD    GetStart(PRInt32* aStart) { return _to GetStart(aStart); } \
  NS_IMETHOD    SetStart(PRInt32 aStart) { return _to SetStart(aStart); } \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    SetType(const nsAReadableString& aType) { return _to SetType(aType); } \


extern "C" NS_DOM nsresult NS_InitHTMLOListElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptHTMLOListElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMHTMLOListElement_h__
