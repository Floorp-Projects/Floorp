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

#ifndef nsIDOMNSDocument_h__
#define nsIDOMNSDocument_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMElement;
class nsIDOMStyleSheetCollection;

#define NS_IDOMNSDOCUMENT_IID \
 { 0xa6cf90cd, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} } 

class nsIDOMNSDocument : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNSDOCUMENT_IID; return iid; }

  NS_IMETHOD    GetStyleSheets(nsIDOMStyleSheetCollection** aStyleSheets)=0;

  NS_IMETHOD    CreateElementWithNameSpace(const nsString& aTagName, const nsString& aNameSpace, nsIDOMElement** aReturn)=0;
};


#define NS_DECL_IDOMNSDOCUMENT   \
  NS_IMETHOD    GetStyleSheets(nsIDOMStyleSheetCollection** aStyleSheets);  \
  NS_IMETHOD    CreateElementWithNameSpace(const nsString& aTagName, const nsString& aNameSpace, nsIDOMElement** aReturn);  \



#define NS_FORWARD_IDOMNSDOCUMENT(_to)  \
  NS_IMETHOD    GetStyleSheets(nsIDOMStyleSheetCollection** aStyleSheets) { return _to GetStyleSheets(aStyleSheets); } \
  NS_IMETHOD    CreateElementWithNameSpace(const nsString& aTagName, const nsString& aNameSpace, nsIDOMElement** aReturn) { return _to CreateElementWithNameSpace(aTagName, aNameSpace, aReturn); }  \


#endif // nsIDOMNSDocument_h__
