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

#ifndef nsIDOMNSDocument_h__
#define nsIDOMNSDocument_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "jsapi.h"

class nsIDOMElement;
class nsIDOMPluginArray;
class nsIDOMRange;

#define NS_IDOMNSDOCUMENT_IID \
 { 0xa6cf90cd, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} } 

class nsIDOMNSDocument : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNSDOCUMENT_IID; return iid; }

  NS_IMETHOD    GetCharacterSet(nsAWritableString& aCharacterSet)=0;

  NS_IMETHOD    GetPlugins(nsIDOMPluginArray** aPlugins)=0;

  NS_IMETHOD    GetLocation(jsval* aLocation)=0;
  NS_IMETHOD    SetLocation(jsval aLocation)=0;

  NS_IMETHOD    CreateElementWithNameSpace(const nsAReadableString& aTagName, const nsAReadableString& aNameSpace, nsIDOMElement** aReturn)=0;

  NS_IMETHOD    CreateRange(nsIDOMRange** aReturn)=0;

  NS_IMETHOD    Load(const nsAReadableString& aUrl)=0;
};


#define NS_DECL_IDOMNSDOCUMENT   \
  NS_IMETHOD    GetCharacterSet(nsAWritableString& aCharacterSet);  \
  NS_IMETHOD    GetPlugins(nsIDOMPluginArray** aPlugins);  \
  NS_IMETHOD    GetLocation(jsval* aLocation);  \
  NS_IMETHOD    SetLocation(jsval aLocation);  \
  NS_IMETHOD    CreateElementWithNameSpace(const nsAReadableString& aTagName, const nsAReadableString& aNameSpace, nsIDOMElement** aReturn);  \
  NS_IMETHOD    CreateRange(nsIDOMRange** aReturn);  \
  NS_IMETHOD    Load(const nsAReadableString& aUrl);  \



#define NS_FORWARD_IDOMNSDOCUMENT(_to)  \
  NS_IMETHOD    GetCharacterSet(nsAWritableString& aCharacterSet) { return _to GetCharacterSet(aCharacterSet); } \
  NS_IMETHOD    GetPlugins(nsIDOMPluginArray** aPlugins) { return _to GetPlugins(aPlugins); } \
  NS_IMETHOD    GetLocation(jsval* aLocation) { return _to GetLocation(aLocation); } \
  NS_IMETHOD    SetLocation(jsval aLocation) { return _to SetLocation(aLocation); } \
  NS_IMETHOD    CreateElementWithNameSpace(const nsAReadableString& aTagName, const nsAReadableString& aNameSpace, nsIDOMElement** aReturn) { return _to CreateElementWithNameSpace(aTagName, aNameSpace, aReturn); }  \
  NS_IMETHOD    CreateRange(nsIDOMRange** aReturn) { return _to CreateRange(aReturn); }  \
  NS_IMETHOD    Load(const nsAReadableString& aUrl) { return _to Load(aUrl); }  \


#endif // nsIDOMNSDocument_h__
