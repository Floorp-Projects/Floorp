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

#ifndef nsIDOMNSHTMLButtonElement_h__
#define nsIDOMNSHTMLButtonElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMNSHTMLBUTTONELEMENT_IID \
 { 0x6fd344d0, 0x7e5f, 0x11d2, \
  { 0xbd, 0x91, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4 } } 

class NS_NO_VTABLE nsIDOMNSHTMLButtonElement : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMNSHTMLBUTTONELEMENT_IID)

  NS_IMETHOD    Blur()=0;

  NS_IMETHOD    Focus()=0;
};


#define NS_DECL_IDOMNSHTMLBUTTONELEMENT   \
  NS_IMETHOD    Blur();  \
  NS_IMETHOD    Focus();  \



#define NS_FORWARD_IDOMNSHTMLBUTTONELEMENT(_to)  \
  NS_IMETHOD    Blur() { return _to Blur(); }  \
  NS_IMETHOD    Focus() { return _to Focus(); }  \


#endif // nsIDOMNSHTMLButtonElement_h__
