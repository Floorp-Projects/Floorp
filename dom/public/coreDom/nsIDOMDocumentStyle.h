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

#ifndef nsIDOMDocumentStyle_h__
#define nsIDOMDocumentStyle_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMStyleSheetList;

#define NS_IDOMDOCUMENTSTYLE_IID \
 { 0x3d9f4973, 0xdd2e, 0x48f5, \
  { 0xb5, 0xf7, 0x26, 0x34, 0xe0, 0x9e, 0xad, 0xd9 } } 

class NS_NO_VTABLE nsIDOMDocumentStyle : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMDOCUMENTSTYLE_IID; return iid; }

  NS_IMETHOD    GetStyleSheets(nsIDOMStyleSheetList** aStyleSheets)=0;
};


#define NS_DECL_IDOMDOCUMENTSTYLE   \
  NS_IMETHOD    GetStyleSheets(nsIDOMStyleSheetList** aStyleSheets);  \



#define NS_FORWARD_IDOMDOCUMENTSTYLE(_to)  \
  NS_IMETHOD    GetStyleSheets(nsIDOMStyleSheetList** aStyleSheets) { return _to GetStyleSheets(aStyleSheets); } \


#endif // nsIDOMDocumentStyle_h__
