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

// 'namespace' class
class xpctest
{
public:
  static const nsID& GetEchoCID() {static nsID cid = NS_ECHO_CID; return cid;}
  static NS_METHOD ConstructEcho(nsISupports *aOuter, REFNSIID aIID, void **aResult);
private:
    xpctest();  // not implemented
};

#endif /* xpctest_private_h___ */
