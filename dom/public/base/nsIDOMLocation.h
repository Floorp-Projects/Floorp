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

#ifndef nsIDOMLocation_h__
#define nsIDOMLocation_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMLOCATION_IID \
 { 0xa6cf906d, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMLocation : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMLOCATION_IID; return iid; }

  NS_IMETHOD    GetHash(nsAWritableString& aHash)=0;
  NS_IMETHOD    SetHash(const nsAReadableString& aHash)=0;

  NS_IMETHOD    GetHost(nsAWritableString& aHost)=0;
  NS_IMETHOD    SetHost(const nsAReadableString& aHost)=0;

  NS_IMETHOD    GetHostname(nsAWritableString& aHostname)=0;
  NS_IMETHOD    SetHostname(const nsAReadableString& aHostname)=0;

  NS_IMETHOD    GetHref(nsAWritableString& aHref)=0;
  NS_IMETHOD    SetHref(const nsAReadableString& aHref)=0;

  NS_IMETHOD    GetPathname(nsAWritableString& aPathname)=0;
  NS_IMETHOD    SetPathname(const nsAReadableString& aPathname)=0;

  NS_IMETHOD    GetPort(nsAWritableString& aPort)=0;
  NS_IMETHOD    SetPort(const nsAReadableString& aPort)=0;

  NS_IMETHOD    GetProtocol(nsAWritableString& aProtocol)=0;
  NS_IMETHOD    SetProtocol(const nsAReadableString& aProtocol)=0;

  NS_IMETHOD    GetSearch(nsAWritableString& aSearch)=0;
  NS_IMETHOD    SetSearch(const nsAReadableString& aSearch)=0;

  NS_IMETHOD    Reload(PRBool aForceget)=0;

  NS_IMETHOD    Replace(const nsAReadableString& aUrl)=0;

  NS_IMETHOD    ToString(nsAWritableString& aReturn)=0;
};


#define NS_DECL_IDOMLOCATION   \
  NS_IMETHOD    GetHash(nsAWritableString& aHash);  \
  NS_IMETHOD    SetHash(const nsAReadableString& aHash);  \
  NS_IMETHOD    GetHost(nsAWritableString& aHost);  \
  NS_IMETHOD    SetHost(const nsAReadableString& aHost);  \
  NS_IMETHOD    GetHostname(nsAWritableString& aHostname);  \
  NS_IMETHOD    SetHostname(const nsAReadableString& aHostname);  \
  NS_IMETHOD    GetHref(nsAWritableString& aHref);  \
  NS_IMETHOD    SetHref(const nsAReadableString& aHref);  \
  NS_IMETHOD    GetPathname(nsAWritableString& aPathname);  \
  NS_IMETHOD    SetPathname(const nsAReadableString& aPathname);  \
  NS_IMETHOD    GetPort(nsAWritableString& aPort);  \
  NS_IMETHOD    SetPort(const nsAReadableString& aPort);  \
  NS_IMETHOD    GetProtocol(nsAWritableString& aProtocol);  \
  NS_IMETHOD    SetProtocol(const nsAReadableString& aProtocol);  \
  NS_IMETHOD    GetSearch(nsAWritableString& aSearch);  \
  NS_IMETHOD    SetSearch(const nsAReadableString& aSearch);  \
  NS_IMETHOD    Reload(PRBool aForceget);  \
  NS_IMETHOD    Replace(const nsAReadableString& aUrl);  \
  NS_IMETHOD    ToString(nsAWritableString& aReturn);  \



#define NS_FORWARD_IDOMLOCATION(_to)  \
  NS_IMETHOD    GetHash(nsAWritableString& aHash) { return _to GetHash(aHash); } \
  NS_IMETHOD    SetHash(const nsAReadableString& aHash) { return _to SetHash(aHash); } \
  NS_IMETHOD    GetHost(nsAWritableString& aHost) { return _to GetHost(aHost); } \
  NS_IMETHOD    SetHost(const nsAReadableString& aHost) { return _to SetHost(aHost); } \
  NS_IMETHOD    GetHostname(nsAWritableString& aHostname) { return _to GetHostname(aHostname); } \
  NS_IMETHOD    SetHostname(const nsAReadableString& aHostname) { return _to SetHostname(aHostname); } \
  NS_IMETHOD    GetHref(nsAWritableString& aHref) { return _to GetHref(aHref); } \
  NS_IMETHOD    SetHref(const nsAReadableString& aHref) { return _to SetHref(aHref); } \
  NS_IMETHOD    GetPathname(nsAWritableString& aPathname) { return _to GetPathname(aPathname); } \
  NS_IMETHOD    SetPathname(const nsAReadableString& aPathname) { return _to SetPathname(aPathname); } \
  NS_IMETHOD    GetPort(nsAWritableString& aPort) { return _to GetPort(aPort); } \
  NS_IMETHOD    SetPort(const nsAReadableString& aPort) { return _to SetPort(aPort); } \
  NS_IMETHOD    GetProtocol(nsAWritableString& aProtocol) { return _to GetProtocol(aProtocol); } \
  NS_IMETHOD    SetProtocol(const nsAReadableString& aProtocol) { return _to SetProtocol(aProtocol); } \
  NS_IMETHOD    GetSearch(nsAWritableString& aSearch) { return _to GetSearch(aSearch); } \
  NS_IMETHOD    SetSearch(const nsAReadableString& aSearch) { return _to SetSearch(aSearch); } \
  NS_IMETHOD    Reload(PRBool aForceget) { return _to Reload(aForceget); }  \
  NS_IMETHOD    Replace(const nsAReadableString& aUrl) { return _to Replace(aUrl); }  \
  NS_IMETHOD    ToString(nsAWritableString& aReturn) { return _to ToString(aReturn); }  \


extern "C" NS_DOM nsresult NS_InitLocationClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptLocation(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMLocation_h__
