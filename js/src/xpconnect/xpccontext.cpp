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

/* Per JSContext object. */

#include "xpcprivate.h"

// static
XPCContext*
XPCContext::newXPCContext(JSContext* aJSContext,
                        JSObject* aGlobalObj,
                        int WrappedJSMapSize,
                        int WrappedNativeMapSize,
                        int WrappedJSClassMapSize,
                        int WrappedNativeClassMapSize)
{
    XPCContext* xpcc;

    NS_PRECONDITION(aJSContext,"bad param");
    NS_PRECONDITION(aGlobalObj,"bad param");
    NS_PRECONDITION(WrappedJSMapSize,"bad param");
    NS_PRECONDITION(WrappedNativeMapSize,"bad param");
    NS_PRECONDITION(WrappedJSClassMapSize,"bad param");
    NS_PRECONDITION(WrappedNativeClassMapSize,"bad param");

    xpcc = new XPCContext(aJSContext,
                        aGlobalObj,
                        WrappedJSMapSize,
                        WrappedNativeMapSize,
                        WrappedJSClassMapSize,
                        WrappedNativeClassMapSize);

    if(xpcc                             &&
       xpcc->GetXPConnect()             &&
       xpcc->GetWrappedJSMap()          &&
       xpcc->GetWrappedNativeMap()      &&
       xpcc->GetWrappedJSClassMap()     &&
       xpcc->GetWrappedNativeClassMap() &&
       xpc_InitIDClass(xpcc)            &&
       nsXPCWrappedJSClass::InitForContext(xpcc) &&
       nsXPCWrappedNativeClass::InitForContext(xpcc))
    {
        return xpcc;
    }
    delete xpcc;
    return NULL;
}

XPCContext::XPCContext(JSContext* aJSContext,
                     JSObject* aGlobalObj,
                     int WrappedJSMapSize,
                     int WrappedNativeMapSize,
                     int WrappedJSClassMapSize,
                     int WrappedNativeClassMapSize)
{
    mXPConnect = nsXPConnect::GetXPConnect();
    mJSContext = aJSContext;
    mGlobalObj = aGlobalObj;
    mWrappedJSMap = JSObject2WrappedJSMap::newMap(WrappedJSMapSize);
    mWrappedNativeMap = Native2WrappedNativeMap::newMap(WrappedNativeMapSize);
    mWrappedJSClassMap = IID2WrappedJSClassMap::newMap(WrappedJSClassMapSize);
    mWrappedNativeClassMap = IID2WrappedNativeClassMap::newMap(WrappedNativeClassMapSize);
}

XPCContext::~XPCContext()
{
    if(mXPConnect)
        NS_RELEASE(mXPConnect);
    if(mWrappedJSMap)
        delete mWrappedJSMap;
    if(mWrappedNativeMap)
        delete mWrappedNativeMap;
    if(mWrappedJSClassMap)
        delete mWrappedJSClassMap;
    if(mWrappedNativeClassMap)
        delete mWrappedNativeClassMap;
}
