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

#ifndef nsDOMCID_h__
#define nsDOMCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

#define NS_DOM_SCRIPT_OBJECT_FACTORY_CID            \
 { /* 9eb760f0-4380-11d2-b328-00805f8a3859 */       \
  0x9eb760f0, 0x4380, 0x11d2,                       \
 {0xb3, 0x28, 0x00, 0x80, 0x5f, 0x8a, 0x38, 0x59} }

#define NS_DOM_NATIVE_OBJECT_REGISTRY_CID            \
 { /* 651074a0-4cd4-11d2-b328-00805f8a3859 */        \
  0x651074a0, 0x4cd4, 0x11d2,                        \
 {0xb3, 0x28, 0x00, 0x80, 0x5f, 0x8a, 0x38, 0x59 } }

#define NS_SCRIPT_NAMESET_REGISTRY_CID               \
 { /* 45f27d10-987b-11d2-bd40-00105aa45e89 */        \
  0x45f27d10, 0x987b, 0x11d2,                        \
 {0xbd, 0x40, 0x00, 0x10, 0x5a, 0xa4, 0x5e, 0x89} }

//The dom cannot provide the crypto or pkcs11 classes that
//were used in older days, so if someone wants to provide
//the service they must implement an object and give it 
//this class ID
#define NS_CRYPTO_CONTRACTID "@mozilla.org/security/crypto;1"
#define NS_PKCS11_CONTRACTID "@mozilla.org/security/pkcs11;1"

#endif /* nsDOMCID_h__ */
