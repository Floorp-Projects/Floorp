/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* local header for xpconnect tests components */

#ifndef xpctest_private_h___
#define xpctest_private_h___

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIAllocator.h"
#include "nsIXPConnect.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nscore.h"
#include <stdio.h>

#include "xpctest.h"

// {ED132C20-EED1-11d2-BAA4-00805F8A5DD7}
#define NS_ECHO_CID \
{ 0xed132c20, 0xeed1, 0x11d2, \
    { 0xba, 0xa4, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }


// {0ECB3420-0D6F-11d3-BAB8-00805F8A5DD7}
#define NS_CHILD_CID \
{ 0xecb3420, 0xd6f, 0x11d3, \
    { 0xba, 0xb8, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }


// {FD774840-237B-11d3-9879-006008962422}
#define NS_NOISY_CID \
{ 0xfd774840, 0x237b, 0x11d3, \
    { 0x98, 0x79, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

// {4DD7EC80-30D9-11d3-9885-006008962422}
#define NS_STRING_TEST_CID \
{ 0x4dd7ec80, 0x30d9, 0x11d3,\
    { 0x98, 0x85, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

// {DC5FDE90-439D-11d3-988C-006008962422}
#define NS_OVERLOADED_CID \
{ 0xdc5fde90, 0x439d, 0x11d3, \
    { 0x98, 0x8c, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }


// 'namespace' class
class xpctest
{
public:
  static const nsID& GetEchoCID() {static nsID cid = NS_ECHO_CID; return cid;}
  static NS_METHOD ConstructEcho(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  static const nsID& GetChildCID() {static nsID cid = NS_CHILD_CID; return cid;}
  static NS_METHOD ConstructChild(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  static const nsID& GetNoisyCID() {static nsID cid = NS_NOISY_CID; return cid;}
  static NS_METHOD ConstructNoisy(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  static const nsID& GetStringTestCID() {static nsID cid = NS_STRING_TEST_CID; return cid;}
  static NS_METHOD ConstructStringTest(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  static const nsID& GetOverloadedCID() {static nsID cid = NS_OVERLOADED_CID; return cid;}
  static NS_METHOD ConstructOverloaded(nsISupports *aOuter, REFNSIID aIID, void **aResult);

private:
    xpctest();  // not implemented
};

#endif /* xpctest_private_h___ */
