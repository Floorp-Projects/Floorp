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

#ifndef nsIDOMNSHTMLTextAreaElement_h__
#define nsIDOMNSHTMLTextAreaElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIControllers;

#define NS_IDOMNSHTMLTEXTAREAELEMENT_IID \
 { 0xca066b44, 0x9ddf, 0x11d3, \
  { 0xbc, 0xcc, 0x00, 0x60, 0xb0, 0xfc, 0x76, 0xbd } } 

class NS_NO_VTABLE nsIDOMNSHTMLTextAreaElement : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNSHTMLTEXTAREAELEMENT_IID; return iid; }

  NS_IMETHOD    GetControllers(nsIControllers** aControllers)=0;
};


#define NS_DECL_IDOMNSHTMLTEXTAREAELEMENT   \
  NS_IMETHOD    GetControllers(nsIControllers** aControllers);  \



#define NS_FORWARD_IDOMNSHTMLTEXTAREAELEMENT(_to)  \
  NS_IMETHOD    GetControllers(nsIControllers** aControllers) { return _to GetControllers(aControllers); } \


#endif // nsIDOMNSHTMLTextAreaElement_h__
