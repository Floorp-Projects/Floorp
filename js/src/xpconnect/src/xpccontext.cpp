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
       xpcc->mConstuctorStrID           &&
       xpcc->mToStringStrID)
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
    JS_ValueToId(aJSContext, 
                 STRING_TO_JSVAL(JS_InternString(aJSContext, "constructor")), 
                 &mConstuctorStrID);
    JS_ValueToId(aJSContext, 
                 STRING_TO_JSVAL(JS_InternString(aJSContext, "toString")), 
                 &mToStringStrID);
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

JSBool
XPCContext::Init(JSObject* aGlobalObj /*= NULL*/)
{
    if(aGlobalObj)
        mGlobalObj = aGlobalObj;
    return /* xpc_InitIDClass(this)            && */
           nsXPCWrappedJSClass::InitForContext(this) &&
           nsXPCWrappedNativeClass::InitForContext(this);
}

#ifdef DEBUG
JS_STATIC_DLL_CALLBACK(intN)
WrappedNativeClassMapDumpEnumerator(JSHashEntry *he, intN i, void *arg)
{
    ((nsXPCWrappedNativeClass*)he->value)->DebugDump(*(int*)arg);
    return HT_ENUMERATE_NEXT;
}
JS_STATIC_DLL_CALLBACK(intN)
WrappedJSClassMapDumpEnumerator(JSHashEntry *he, intN i, void *arg)
{
    ((nsXPCWrappedJSClass*)he->value)->DebugDump(*(int*)arg);
    return HT_ENUMERATE_NEXT;
}
JS_STATIC_DLL_CALLBACK(intN)
WrappedNativeMapDumpEnumerator(JSHashEntry *he, intN i, void *arg)
{
    ((nsXPCWrappedNative*)he->value)->DebugDump(*(int*)arg);
    return HT_ENUMERATE_NEXT;
}
JS_STATIC_DLL_CALLBACK(intN)
WrappedJSMapDumpEnumerator(JSHashEntry *he, intN i, void *arg)
{
    ((nsXPCWrappedJS*)he->value)->DebugDump(*(int*)arg);
    return HT_ENUMERATE_NEXT;
}
#endif


void
XPCContext::DebugDump(int depth)
{
#ifdef DEBUG
    depth--;
    XPC_LOG_ALWAYS(("XPCContext @ %x", this));
        XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("mJSContext @ %x", mJSContext));
        XPC_LOG_ALWAYS(("mGlobalObj @ %x", mGlobalObj));
        XPC_LOG_ALWAYS(("mWrappedNativeClassMap @ %x with %d classes", \
            mWrappedNativeClassMap, \
            mWrappedNativeClassMap ? mWrappedNativeClassMap->Count() : 0));
        XPC_LOG_ALWAYS(("mWrappedJSClassMap @ %x with %d classes", \
            mWrappedJSClassMap, \
            mWrappedJSClassMap ? mWrappedJSClassMap->Count() : 0));
        XPC_LOG_ALWAYS(("mWrappedNativeMap @ %x with %d wrappers", \
            mWrappedNativeMap, \
            mWrappedNativeMap ? mWrappedNativeMap->Count() : 0));
        XPC_LOG_ALWAYS(("mWrappedJSMap @ %x with %d wrappers", \
            mWrappedJSMap, \
            mWrappedJSMap ? mWrappedJSMap->Count() : 0));

        if(depth && mWrappedNativeClassMap && mWrappedNativeClassMap->Count())
        {
            XPC_LOG_ALWAYS(("The %d WrappedNativeClasses...",\
                            mWrappedNativeClassMap->Count()));
            XPC_LOG_INDENT();
            mWrappedNativeClassMap->Enumerate(WrappedNativeClassMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }

        if(depth && mWrappedJSClassMap && mWrappedJSClassMap->Count())
        {
            XPC_LOG_ALWAYS(("The %d WrappedJSClasses...",\
                            mWrappedJSClassMap->Count()));
            XPC_LOG_INDENT();
            mWrappedJSClassMap->Enumerate(WrappedJSClassMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }

        if(depth && mWrappedNativeMap && mWrappedNativeMap->Count())
        {
            XPC_LOG_ALWAYS(("The %d WrappedNatives...",\
                            mWrappedNativeMap->Count()));
            XPC_LOG_INDENT();
            mWrappedNativeMap->Enumerate(WrappedNativeMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }

        if(depth && mWrappedJSMap && mWrappedJSMap->Count())
        {
            XPC_LOG_ALWAYS(("The %d WrappedJSs...",\
                            mWrappedJSMap->Count()));
            XPC_LOG_INDENT();
            mWrappedJSMap->Enumerate(WrappedJSMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }

        XPC_LOG_OUTDENT();
#endif
}
