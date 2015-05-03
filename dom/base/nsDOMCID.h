/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMCID_h__
#define nsDOMCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

#define NS_DOM_SCRIPT_OBJECT_FACTORY_CID            \
 { /* 9eb760f0-4380-11d2-b328-00805f8a3859 */       \
  0x9eb760f0, 0x4380, 0x11d2,                       \
 {0xb3, 0x28, 0x00, 0x80, 0x5f, 0x8a, 0x38, 0x59} }

#define NS_SCRIPT_NAMESET_REGISTRY_CID               \
 { /* 45f27d10-987b-11d2-bd40-00105aa45e89 */        \
  0x45f27d10, 0x987b, 0x11d2,                        \
 {0xbd, 0x40, 0x00, 0x10, 0x5a, 0xa4, 0x5e, 0x89} }

#define SERVICEWORKERPERIODICUPDATER_CONTRACTID \
  "@mozilla.org/service-worker-periodic-updater;1"

//The dom cannot provide the crypto or pkcs11 classes that
//were used in older days, so if someone wants to provide
//the service they must implement an object and give it 
//this class ID
#define NS_CRYPTO_CONTRACTID "@mozilla.org/security/crypto;1"
#define NS_PKCS11_CONTRACTID "@mozilla.org/security/pkcs11;1"

#define NS_XPATH_EVALUATOR_CONTRACTID "@mozilla.org/dom/xpath-evaluator;1"

#endif /* nsDOMCID_h__ */
