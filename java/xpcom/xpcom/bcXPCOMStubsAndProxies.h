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

#ifndef __bcXPCOMStubsAndProxies_h
#define __bcXPCOMStubsAndProxies_h
#include "nsISupports.h"
#include "bcIORB.h"
#include "bcIStub.h"


/* 843ff582-1dd2-11b2-84b5-b43ba3ad3ef4 */

#define BC_XPCOMSTUBSANDPROXIES_IID \
  {0x843ff582, 0x1dd2, 0x11b2, \
  {0x84, 0xb5,0xb4, 0x3b, 0xa3, 0xad, 0x3e, 0xf4}}

#define BC_XPCOMSTUBSANDPROXIES_PROGID "component://netscape/blackwood/blackconnect/xpcom-stubs-and-proxies"

#define BC_XPCOMSTUBSANDPROXIES_CID \
   {0x7de11df0, 0x1dd2, 0x11b2, \
   {0xb1, 0xe1, 0xd9, 0xd5, 0xc6, 0xdd, 0x06, 0x8b }}

class nsSupportsHashtable;

class bcXPCOMStubsAndProxies : public nsISupports {
    NS_DECL_ISUPPORTS
    NS_DEFINE_STATIC_IID_ACCESSOR(BC_XPCOMSTUBSANDPROXIES_IID)  
    NS_IMETHOD GetStub(nsISupports *obj, bcIStub **stub);
    NS_IMETHOD GetOID(nsISupports *obj, bcIORB *orb, bcOID *oid);
    NS_IMETHOD GetProxy(bcOID oid, const nsIID &iid, bcIORB *orb, nsISupports **proxy);
    bcXPCOMStubsAndProxies();
    virtual ~bcXPCOMStubsAndProxies();
private:
    nsSupportsHashtable * oid2objectMap;
};
#endif /*  __bcXPCOMStubsAndProxies_h */








