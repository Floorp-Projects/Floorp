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

#ifndef nsIDOMLocation_h__
#define nsIDOMLocation_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "jsapi.h"


#define NS_IDOMLOCATION_IID \
 { 0xa6cf906d, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMLocation : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMLOCATION_IID; return iid; }

  NS_IMETHOD    GetHash(nsString& aHash)=0;
  NS_IMETHOD    SetHash(const nsString& aHash)=0;

  NS_IMETHOD    GetHost(nsString& aHost)=0;
  NS_IMETHOD    SetHost(const nsString& aHost)=0;

  NS_IMETHOD    GetHostname(nsString& aHostname)=0;
  NS_IMETHOD    SetHostname(const nsString& aHostname)=0;

  NS_IMETHOD    GetHref(nsString& aHref)=0;
  NS_IMETHOD    SetHref(const nsString& aHref)=0;

  NS_IMETHOD    GetPathname(nsString& aPathname)=0;
  NS_IMETHOD    SetPathname(const nsString& aPathname)=0;

  NS_IMETHOD    GetPort(nsString& aPort)=0;
  NS_IMETHOD    SetPort(const nsString& aPort)=0;

  NS_IMETHOD    GetProtocol(nsString& aProtocol)=0;
  NS_IMETHOD    SetProtocol(const nsString& aProtocol)=0;

  NS_IMETHOD    GetSearch(nsString& aSearch)=0;
  NS_IMETHOD    SetSearch(const nsString& aSearch)=0;

  NS_IMETHOD    Reload(JSContext *cx, jsval *argv, PRUint32 argc)=0;

  NS_IMETHOD    Replace(const nsString& aUrl)=0;

  NS_IMETHOD    ToString(nsString& aReturn)=0;
};


#define NS_DECL_IDOMLOCATION   \
  NS_IMETHOD    GetHash(nsString& aHash);  \
  NS_IMETHOD    SetHash(const nsString& aHash);  \
  NS_IMETHOD    GetHost(nsString& aHost);  \
  NS_IMETHOD    SetHost(const nsString& aHost);  \
  NS_IMETHOD    GetHostname(nsString& aHostname);  \
  NS_IMETHOD    SetHostname(const nsString& aHostname);  \
  NS_IMETHOD    GetHref(nsString& aHref);  \
  NS_IMETHOD    SetHref(const nsString& aHref);  \
  NS_IMETHOD    GetPathname(nsString& aPathname);  \
  NS_IMETHOD    SetPathname(const nsString& aPathname);  \
  NS_IMETHOD    GetPort(nsString& aPort);  \
  NS_IMETHOD    SetPort(const nsString& aPort);  \
  NS_IMETHOD    GetProtocol(nsString& aProtocol);  \
  NS_IMETHOD    SetProtocol(const nsString& aProtocol);  \
  NS_IMETHOD    GetSearch(nsString& aSearch);  \
  NS_IMETHOD    SetSearch(const nsString& aSearch);  \
  NS_IMETHOD    Reload(JSContext *cx, jsval *argv, PRUint32 argc);  \
  NS_IMETHOD    Replace(const nsString& aUrl);  \
  NS_IMETHOD    ToString(nsString& aReturn);  \



#define NS_FORWARD_IDOMLOCATION(_to)  \
  NS_IMETHOD    GetHash(nsString& aHash) { return _to GetHash(aHash); } \
  NS_IMETHOD    SetHash(const nsString& aHash) { return _to SetHash(aHash); } \
  NS_IMETHOD    GetHost(nsString& aHost) { return _to GetHost(aHost); } \
  NS_IMETHOD    SetHost(const nsString& aHost) { return _to SetHost(aHost); } \
  NS_IMETHOD    GetHostname(nsString& aHostname) { return _to GetHostname(aHostname); } \
  NS_IMETHOD    SetHostname(const nsString& aHostname) { return _to SetHostname(aHostname); } \
  NS_IMETHOD    GetHref(nsString& aHref) { return _to GetHref(aHref); } \
  NS_IMETHOD    SetHref(const nsString& aHref) { return _to SetHref(aHref); } \
  NS_IMETHOD    GetPathname(nsString& aPathname) { return _to GetPathname(aPathname); } \
  NS_IMETHOD    SetPathname(const nsString& aPathname) { return _to SetPathname(aPathname); } \
  NS_IMETHOD    GetPort(nsString& aPort) { return _to GetPort(aPort); } \
  NS_IMETHOD    SetPort(const nsString& aPort) { return _to SetPort(aPort); } \
  NS_IMETHOD    GetProtocol(nsString& aProtocol) { return _to GetProtocol(aProtocol); } \
  NS_IMETHOD    SetProtocol(const nsString& aProtocol) { return _to SetProtocol(aProtocol); } \
  NS_IMETHOD    GetSearch(nsString& aSearch) { return _to GetSearch(aSearch); } \
  NS_IMETHOD    SetSearch(const nsString& aSearch) { return _to SetSearch(aSearch); } \
  NS_IMETHOD    Reload(JSContext *cx, jsval *argv, PRUint32 argc) { return _to Reload(cx, argv, argc); }  \
  NS_IMETHOD    Replace(const nsString& aUrl) { return _to Replace(aUrl); }  \
  NS_IMETHOD    ToString(nsString& aReturn) { return _to ToString(aReturn); }  \


extern "C" NS_DOM nsresult NS_InitLocationClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptLocation(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMLocation_h__
