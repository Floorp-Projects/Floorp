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

#ifndef nsSOAPPropertyBag_h__
#define nsSOAPPropertyBag_h__

#include "nsString.h"
#include "nsIPropertyBag.h"
#include "nsISOAPPropertyBagMutator.h"
#include "nsCOMPtr.h"
#include "nsIXPCScriptable.h"
#include "nsHashtable.h"
#include "nsSupportsArray.h"


class nsSOAPPropertyBag;
class nsSOAPPropertyBagMutator:public nsISOAPPropertyBagMutator {
public:
  nsSOAPPropertyBagMutator();
  virtual ~nsSOAPPropertyBagMutator();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISOAPPROPERTYBAGMUTATOR

protected:
  nsCOMPtr<nsIPropertyBag> mBag;
  nsSOAPPropertyBag* mSOAPBag;
};

#define NS_SOAPPROPERTY_CID                        \
 { /* 147d8bfe-1dd2-11b2-b72e-bba9d7a0dab7 */      \
 0x147d8bfe, 0x1dd2, 0x11b2,                       \
 {0xb7, 0x2e, 0xbb, 0xa9, 0xd7, 0xa0, 0xda, 0xb7} }
#define NS_SOAPPROPERTY_CONTRACTID "@mozilla.org/xmlextras/soap/property;1"
#define NS_SOAPPROPERTYBAG_CID                        \
 { /* 205621ac-1dd2-11b2-8c86-ede3fe564ef1 */      \
 0x205621ac, 0x1dd2, 0x11b2,                       \
 {0x8c, 0x86, 0xed, 0xe3, 0xfe, 0x56, 0x4e, 0xf1} }
#define NS_SOAPPROPERTYBAG_CONTRACTID "@mozilla.org/xmlextras/soap/property/bag;1"
#define NS_SOAPPROPERTYBAGENUMERATOR_CID                        \
 { /* 34e1f484-1dd2-11b2-a8c0-f20a9f3a0c55 */      \
 0x34e1f484, 0x1dd2, 0x11b2,                       \
 {0xa8, 0xc0, 0xf2, 0x0a, 0x9f, 0x3a, 0x0c, 0x55} }
#define NS_SOAPPROPERTYBAGENUMERATOR_CONTRACTID "@mozilla.org/xmlextras/soap/property/bag/enumerator;1"
#endif
