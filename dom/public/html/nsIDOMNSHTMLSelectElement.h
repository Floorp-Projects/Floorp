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

#ifndef nsIDOMNSHTMLSelectElement_h__
#define nsIDOMNSHTMLSelectElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMElement;

#define NS_IDOMNSHTMLSELECTELEMENT_IID \
 { 0xa6cf9105, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMNSHTMLSelectElement : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNSHTMLSELECTELEMENT_IID; return iid; }

  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMElement** aReturn)=0;
};


#define NS_DECL_IDOMNSHTMLSELECTELEMENT   \
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMElement** aReturn);  \



#define NS_FORWARD_IDOMNSHTMLSELECTELEMENT(_to)  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMElement** aReturn) { return _to Item(aIndex, aReturn); }  \


#endif // nsIDOMNSHTMLSelectElement_h__
