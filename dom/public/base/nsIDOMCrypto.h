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

#ifndef nsIDOMCrypto_h__
#define nsIDOMCrypto_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "jsapi.h"

class nsIDOMCRMFObject;

#define NS_IDOMCRYPTO_IID \
 {0xf45efbe0, 0x1d52, 0x11d4, \
         {0x8a, 0x7c, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3}} 

class nsIDOMCrypto : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCRYPTO_IID; return iid; }

  NS_IMETHOD    GetVersion(nsAWritableString& aVersion)=0;

  NS_IMETHOD    GenerateCRMFRequest(JSContext* cx, jsval* argv, PRUint32 argc, nsIDOMCRMFObject** aReturn)=0;

  NS_IMETHOD    ImportUserCertificates(const nsAReadableString& aNickname, const nsAReadableString& aCmmfResponse, PRBool aDoForcedBackup, nsAWritableString& aReturn)=0;

  NS_IMETHOD    PopChallengeResponse(const nsAReadableString& aChallenge, nsAWritableString& aReturn)=0;

  NS_IMETHOD    Random(PRInt32 aNumBytes, nsAWritableString& aReturn)=0;

  NS_IMETHOD    SignText(JSContext* cx, jsval* argv, PRUint32 argc, nsAWritableString& aReturn)=0;

  NS_IMETHOD    Alert(const nsAReadableString& aMessage)=0;

  NS_IMETHOD    Logout()=0;

  NS_IMETHOD    DisableRightClick()=0;
};


#define NS_DECL_IDOMCRYPTO   \
  NS_IMETHOD    GetVersion(nsAWritableString& aVersion);  \
  NS_IMETHOD    GenerateCRMFRequest(JSContext* cx, jsval* argv, PRUint32 argc, nsIDOMCRMFObject** aReturn);  \
  NS_IMETHOD    ImportUserCertificates(const nsAReadableString& aNickname, const nsAReadableString& aCmmfResponse, PRBool aDoForcedBackup, nsAWritableString& aReturn);  \
  NS_IMETHOD    PopChallengeResponse(const nsAReadableString& aChallenge, nsAWritableString& aReturn);  \
  NS_IMETHOD    Random(PRInt32 aNumBytes, nsAWritableString& aReturn);  \
  NS_IMETHOD    SignText(JSContext* cx, jsval* argv, PRUint32 argc, nsAWritableString& aReturn);  \
  NS_IMETHOD    Alert(const nsAReadableString& aMessage);  \
  NS_IMETHOD    Logout();  \
  NS_IMETHOD    DisableRightClick();  \



#define NS_FORWARD_IDOMCRYPTO(_to)  \
  NS_IMETHOD    GetVersion(nsAWritableString& aVersion) { return _to GetVersion(aVersion); } \
  NS_IMETHOD    GenerateCRMFRequest(JSContext* cx, jsval* argv, PRUint32 argc, nsIDOMCRMFObject** aReturn) { return _to GenerateCRMFRequest(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    ImportUserCertificates(const nsAReadableString& aNickname, const nsAReadableString& aCmmfResponse, PRBool aDoForcedBackup, nsAWritableString& aReturn) { return _to ImportUserCertificates(aNickname, aCmmfResponse, aDoForcedBackup, aReturn); }  \
  NS_IMETHOD    PopChallengeResponse(const nsAReadableString& aChallenge, nsAWritableString& aReturn) { return _to PopChallengeResponse(aChallenge, aReturn); }  \
  NS_IMETHOD    Random(PRInt32 aNumBytes, nsAWritableString& aReturn) { return _to Random(aNumBytes, aReturn); }  \
  NS_IMETHOD    SignText(JSContext* cx, jsval* argv, PRUint32 argc, nsAWritableString& aReturn) { return _to SignText(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    Alert(const nsAReadableString& aMessage) { return _to Alert(aMessage); }  \
  NS_IMETHOD    Logout() { return _to Logout(); }  \
  NS_IMETHOD    DisableRightClick() { return _to DisableRightClick(); }  \


extern "C" NS_DOM nsresult NS_InitCryptoClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCrypto(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCrypto_h__
