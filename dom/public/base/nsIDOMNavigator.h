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

#ifndef nsIDOMNavigator_h__
#define nsIDOMNavigator_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "jsapi.h"

class nsIDOMPluginArray;
class nsIDOMMimeTypeArray;

#define NS_IDOMNAVIGATOR_IID \
 { 0xa6cf906e, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMNavigator : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNAVIGATOR_IID; return iid; }

  NS_IMETHOD    GetAppCodeName(nsString& aAppCodeName)=0;

  NS_IMETHOD    GetAppName(nsString& aAppName)=0;

  NS_IMETHOD    GetAppVersion(nsString& aAppVersion)=0;

  NS_IMETHOD    GetLanguage(nsString& aLanguage)=0;

  NS_IMETHOD    GetMimeTypes(nsIDOMMimeTypeArray** aMimeTypes)=0;

  NS_IMETHOD    GetPlatform(nsString& aPlatform)=0;

  NS_IMETHOD    GetOscpu(nsString& aOscpu)=0;

  NS_IMETHOD    GetVendor(nsString& aVendor)=0;

  NS_IMETHOD    GetVendorSub(nsString& aVendorSub)=0;

  NS_IMETHOD    GetProduct(nsString& aProduct)=0;

  NS_IMETHOD    GetProductSub(nsString& aProductSub)=0;

  NS_IMETHOD    GetPlugins(nsIDOMPluginArray** aPlugins)=0;

  NS_IMETHOD    GetSecurityPolicy(nsString& aSecurityPolicy)=0;

  NS_IMETHOD    GetUserAgent(nsString& aUserAgent)=0;

  NS_IMETHOD    GetCookieEnabled(PRBool* aCookieEnabled)=0;

  NS_IMETHOD    JavaEnabled(PRBool* aReturn)=0;

  NS_IMETHOD    TaintEnabled(PRBool* aReturn)=0;

  NS_IMETHOD    Preference(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn)=0;
};


#define NS_DECL_IDOMNAVIGATOR   \
  NS_IMETHOD    GetAppCodeName(nsString& aAppCodeName);  \
  NS_IMETHOD    GetAppName(nsString& aAppName);  \
  NS_IMETHOD    GetAppVersion(nsString& aAppVersion);  \
  NS_IMETHOD    GetLanguage(nsString& aLanguage);  \
  NS_IMETHOD    GetMimeTypes(nsIDOMMimeTypeArray** aMimeTypes);  \
  NS_IMETHOD    GetPlatform(nsString& aPlatform);  \
  NS_IMETHOD    GetOscpu(nsString& aOscpu);  \
  NS_IMETHOD    GetVendor(nsString& aVendor);  \
  NS_IMETHOD    GetVendorSub(nsString& aVendorSub);  \
  NS_IMETHOD    GetProduct(nsString& aProduct);  \
  NS_IMETHOD    GetProductSub(nsString& aProductSub);  \
  NS_IMETHOD    GetPlugins(nsIDOMPluginArray** aPlugins);  \
  NS_IMETHOD    GetSecurityPolicy(nsString& aSecurityPolicy);  \
  NS_IMETHOD    GetUserAgent(nsString& aUserAgent);  \
  NS_IMETHOD    GetCookieEnabled(PRBool* aCookieEnabled);  \
  NS_IMETHOD    JavaEnabled(PRBool* aReturn);  \
  NS_IMETHOD    TaintEnabled(PRBool* aReturn);  \
  NS_IMETHOD    Preference(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn);  \



#define NS_FORWARD_IDOMNAVIGATOR(_to)  \
  NS_IMETHOD    GetAppCodeName(nsString& aAppCodeName) { return _to GetAppCodeName(aAppCodeName); } \
  NS_IMETHOD    GetAppName(nsString& aAppName) { return _to GetAppName(aAppName); } \
  NS_IMETHOD    GetAppVersion(nsString& aAppVersion) { return _to GetAppVersion(aAppVersion); } \
  NS_IMETHOD    GetLanguage(nsString& aLanguage) { return _to GetLanguage(aLanguage); } \
  NS_IMETHOD    GetMimeTypes(nsIDOMMimeTypeArray** aMimeTypes) { return _to GetMimeTypes(aMimeTypes); } \
  NS_IMETHOD    GetPlatform(nsString& aPlatform) { return _to GetPlatform(aPlatform); } \
  NS_IMETHOD    GetOscpu(nsString& aOscpu) { return _to GetOscpu(aOscpu); } \
  NS_IMETHOD    GetVendor(nsString& aVendor) { return _to GetVendor(aVendor); } \
  NS_IMETHOD    GetVendorSub(nsString& aVendorSub) { return _to GetVendorSub(aVendorSub); } \
  NS_IMETHOD    GetProduct(nsString& aProduct) { return _to GetProduct(aProduct); } \
  NS_IMETHOD    GetProductSub(nsString& aProductSub) { return _to GetProductSub(aProductSub); } \
  NS_IMETHOD    GetPlugins(nsIDOMPluginArray** aPlugins) { return _to GetPlugins(aPlugins); } \
  NS_IMETHOD    GetSecurityPolicy(nsString& aSecurityPolicy) { return _to GetSecurityPolicy(aSecurityPolicy); } \
  NS_IMETHOD    GetUserAgent(nsString& aUserAgent) { return _to GetUserAgent(aUserAgent); } \
  NS_IMETHOD    GetCookieEnabled(PRBool* aCookieEnabled) { return _to GetCookieEnabled(aCookieEnabled); } \
  NS_IMETHOD    JavaEnabled(PRBool* aReturn) { return _to JavaEnabled(aReturn); }  \
  NS_IMETHOD    TaintEnabled(PRBool* aReturn) { return _to TaintEnabled(aReturn); }  \
  NS_IMETHOD    Preference(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn) { return _to Preference(cx, argv, argc, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitNavigatorClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptNavigator(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMNavigator_h__
