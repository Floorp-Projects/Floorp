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

#ifndef nsIDOMLinkStyle_h__
#define nsIDOMLinkStyle_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMStyleSheet;

#define NS_IDOMLINKSTYLE_IID \
 { 0x24d89a65, 0xf598, 0x481e, \
  { 0xa2, 0x97, 0x23, 0xcc, 0x02, 0x59, 0x9b, 0xbd } } 

class NS_NO_VTABLE nsIDOMLinkStyle : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMLINKSTYLE_IID; return iid; }

  NS_IMETHOD    GetSheet(nsIDOMStyleSheet** aSheet)=0;
};


#define NS_DECL_IDOMLINKSTYLE   \
  NS_IMETHOD    GetSheet(nsIDOMStyleSheet** aSheet);  \



#define NS_FORWARD_IDOMLINKSTYLE(_to)  \
  NS_IMETHOD    GetSheet(nsIDOMStyleSheet** aSheet) { return _to GetSheet(aSheet); } \


#endif // nsIDOMLinkStyle_h__
