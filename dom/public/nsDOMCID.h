/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

//The dom cannot provide the crypto or pkcs11 classes that
//were used in older days, so if someone wants to provide
//the service they must implement an object and give it 
//this class ID
#define NS_CRYPTO_CONTRACTID "@mozilla.org/security/crypto;1"
#define NS_PKCS11_CONTRACTID "@mozilla.org/security/pkcs11;1"

#define NS_XPATH_EVALUATOR_CONTRACTID "@mozilla.org/dom/xpath-evaluator;1"

#endif /* nsDOMCID_h__ */
