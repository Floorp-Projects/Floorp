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
#ifndef _nsCrypto_h_
#define _nsCrypto_h_
#include "nsIDOMCRMFObject.h"
#include "nsIDOMCrypto.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMPkcs11.h"

#define NS_CRYPTO_CLASSNAME "Crypto JavaScript Class"
#define NS_CRYPTO_CID \
  {0x929d9320, 0x251e, 0x11d4, { 0x8a, 0x7c, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3} }

#define NS_PKCS11_CLASSNAME "Pkcs11 JavaScript Class"
#define NS_PKCS11_CID \
  {0x74b7a390, 0x3b41, 0x11d4, { 0x8a, 0x80, 0x00, 0x60, 0x08, 0xc8, 0x44, 0xc3} }

class nsIPSMComponent;
class nsIDOMScriptObjectFactory;


class nsCRMFObject : public nsIDOMCRMFObject,
                     public nsIScriptObjectOwner {
public:
  nsCRMFObject();
  virtual ~nsCRMFObject();
  NS_DECL_IDOMCRMFOBJECT
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  nsresult init();

  nsresult SetCRMFRequest(char *inRequest);
private:

  nsString mBase64Request;
  void *mScriptObject;
};


class nsCrypto: public nsIDOMCrypto,
                public nsIScriptObjectOwner {
public:
  nsCrypto();
  virtual ~nsCrypto();
  nsresult init();
  
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  NS_DECL_ISUPPORTS
  NS_DECL_IDOMCRYPTO

  static nsresult GetScriptObjectFactory(nsIDOMScriptObjectFactory **aResult);
  static nsIDOMScriptObjectFactory *gScriptObjectFactory;
  static nsIPrincipal* GetScriptPrincipal(JSContext *cx);
  static const char *kPSMComponentContractID;

 private:

  nsIPSMComponent *mPSM;
  nsString         mVersionString;
  PRBool           mVersionStringSet;
  void            *mScriptObject;
};

class nsPkcs11 : public nsIDOMPkcs11,
                 public nsIScriptObjectOwner {
public:
  nsPkcs11();
  virtual ~nsPkcs11();

  nsresult init();
  NS_DECL_ISUPPORTS
  NS_DECL_IDOMPKCS11
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

private:
  nsIPSMComponent *mPSM;
  void            *mScriptObject;
};

nsresult
getPSMComponent(nsIPSMComponent ** retPSM);

#endif //_nsCrypto_h_




