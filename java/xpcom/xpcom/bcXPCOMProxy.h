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

#ifndef __bcXPCOMProxy_h
#define __bcXPCOMProxy_h

#include "xptcall.h"
#include "nsISupports.h"
#include "nsIInterfaceInfo.h"

#include "bcDefs.h"
#include "bcIORB.h"

// {a67aeec0-1dd1-11b2-b4a5-cb39ec760f0d}
#define BC_XPCOMPROXY_IID \
{ 0xa67aeec0, 0x1dd1, 0x11b2, { 0xb4, 0xa5, 0xcb, 0x39, 0xec, 0x76, 0x0f, 0x0d } }

class bcXPCOMProxy :  public nsXPTCStubBase {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(BC_XPCOMPROXY_IID)
    NS_DECL_ISUPPORTS
    bcXPCOMProxy(bcOID oid, const nsIID &iid, bcIORB *orb);
    virtual ~bcXPCOMProxy();
    NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info);
    // call this method and return result
    NS_IMETHOD CallMethod(PRUint16 methodIndex,
                          const nsXPTMethodInfo* info,
                          nsXPTCMiniVariant* params);
private:
    bcOID oid;
    nsID iid;
    bcIORB *orb;
    nsIInterfaceInfo * interfaceInfo;
};
#endif

