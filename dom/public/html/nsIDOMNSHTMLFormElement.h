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

#ifndef nsIDOMNSHTMLFormElement_h__
#define nsIDOMNSHTMLFormElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMElement;

#define NS_IDOMNSHTMLFORMELEMENT_IID \
{ 0x6f76532f,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMNSHTMLFormElement : public nsISupports {
public:

  NS_IMETHOD    GetEncoding(nsString& aEncoding)=0;

  NS_IMETHOD    GetLength(PRUint32* aLength)=0;

  NS_IMETHOD    NamedItem(const nsString& aName, nsIDOMElement** aReturn)=0;
};


#define NS_DECL_IDOMNSHTMLFORMELEMENT   \
  NS_IMETHOD    GetEncoding(nsString& aEncoding);  \
  NS_IMETHOD    GetLength(PRUint32* aLength);  \
  NS_IMETHOD    NamedItem(const nsString& aName, nsIDOMElement** aReturn);  \



#define NS_FORWARD_IDOMNSHTMLFORMELEMENT(_to)  \
  NS_IMETHOD    GetEncoding(nsString& aEncoding) { return _to##GetEncoding(aEncoding); } \
  NS_IMETHOD    GetLength(PRUint32* aLength) { return _to##GetLength(aLength); } \
  NS_IMETHOD    NamedItem(const nsString& aName, nsIDOMElement** aReturn) { return _to##NamedItem(aName, aReturn); }  \


#endif // nsIDOMNSHTMLFormElement_h__
