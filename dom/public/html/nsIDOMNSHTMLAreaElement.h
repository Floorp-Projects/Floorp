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

#ifndef nsIDOMNSHTMLAreaElement_h__
#define nsIDOMNSHTMLAreaElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMNSHTMLAREAELEMENT_IID \
 { 0xa6cf911b, 0x15b3, 0x11d2,  \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMNSHTMLAreaElement : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNSHTMLAREAELEMENT_IID; return iid; }

  NS_IMETHOD    GetProtocol(nsString& aProtocol)=0;

  NS_IMETHOD    GetHost(nsString& aHost)=0;

  NS_IMETHOD    GetHostname(nsString& aHostname)=0;

  NS_IMETHOD    GetPathname(nsString& aPathname)=0;

  NS_IMETHOD    GetSearch(nsString& aSearch)=0;

  NS_IMETHOD    GetPort(nsString& aPort)=0;

  NS_IMETHOD    GetHash(nsString& aHash)=0;
};


#define NS_DECL_IDOMNSHTMLAREAELEMENT   \
  NS_IMETHOD    GetProtocol(nsString& aProtocol);  \
  NS_IMETHOD    GetHost(nsString& aHost);  \
  NS_IMETHOD    GetHostname(nsString& aHostname);  \
  NS_IMETHOD    GetPathname(nsString& aPathname);  \
  NS_IMETHOD    GetSearch(nsString& aSearch);  \
  NS_IMETHOD    GetPort(nsString& aPort);  \
  NS_IMETHOD    GetHash(nsString& aHash);  \



#define NS_FORWARD_IDOMNSHTMLAREAELEMENT(_to)  \
  NS_IMETHOD    GetProtocol(nsString& aProtocol) { return _to GetProtocol(aProtocol); } \
  NS_IMETHOD    GetHost(nsString& aHost) { return _to GetHost(aHost); } \
  NS_IMETHOD    GetHostname(nsString& aHostname) { return _to GetHostname(aHostname); } \
  NS_IMETHOD    GetPathname(nsString& aPathname) { return _to GetPathname(aPathname); } \
  NS_IMETHOD    GetSearch(nsString& aSearch) { return _to GetSearch(aSearch); } \
  NS_IMETHOD    GetPort(nsString& aPort) { return _to GetPort(aPort); } \
  NS_IMETHOD    GetHash(nsString& aHash) { return _to GetHash(aHash); } \


#endif // nsIDOMNSHTMLAreaElement_h__
