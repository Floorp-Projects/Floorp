/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Kushnirskiy <idk@eng.sun.com>
 */

#ifndef __bcIXPCOMStubsAndProxies_h__
#define __bcIXPCOMStubsAndProxies_h__
#include "nsISupports.h"
#include "bcDefs.h"

/* 843ff582-1dd2-11b2-84b5-b43ba3ad3ef4 */
#define BC_XPCOMSTUBSANDPROXIES_IID \
  {0x843ff582, 0x1dd2, 0x11b2, \
  {0x84, 0xb5,0xb4, 0x3b, 0xa3, 0xad, 0x3e, 0xf4}}

class bcIStub;
class bcIORB;

class bcIXPCOMStubsAndProxies : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(BC_XPCOMSTUBSANDPROXIES_IID)  
    NS_IMETHOD GetStub(nsISupports *obj, bcIStub **stub) = 0;
    NS_IMETHOD GetOID(nsISupports *obj, bcIORB *orb, bcOID *oid) = 0;
    NS_IMETHOD GetProxy(bcOID oid, const nsIID &iid, bcIORB *orb, nsISupports **proxy) = 0;
};

#endif
