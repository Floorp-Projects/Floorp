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

#ifndef nsIDOMNSHTMLSelectElement_h__
#define nsIDOMNSHTMLSelectElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMNode;

#define NS_IDOMNSHTMLSELECTELEMENT_IID \
 { 0xa6cf9105, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMNSHTMLSelectElement : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNSHTMLSELECTELEMENT_IID; return iid; }

  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn)=0;

  NS_IMETHOD    NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn)=0;
};


#define NS_DECL_IDOMNSHTMLSELECTELEMENT   \
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);  \
  NS_IMETHOD    NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn);  \



#define NS_FORWARD_IDOMNSHTMLSELECTELEMENT(_to)  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn) { return _to Item(aIndex, aReturn); }  \
  NS_IMETHOD    NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn) { return _to NamedItem(aName, aReturn); }  \


#endif // nsIDOMNSHTMLSelectElement_h__
