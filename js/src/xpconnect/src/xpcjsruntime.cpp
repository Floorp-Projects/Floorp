/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Per JSRuntime object */

#include "xpcprivate.h"

/***************************************************************************/

const char* XPCJSRuntime::mStrings[] = {
    "constructor",          // IDX_CONSTRUCTOR
    "toString",             // IDX_TO_STRING
    "lastResult",           // IDX_LAST_RESULT
    "returnCode",           // IDX_RETURN_CODE
    "value",                // IDX_VALUE
    "QueryInterface",       // IDX_QUERY_INTERFACE
    "Components",           // IDX_COMPONENTS
    "wrappedJSObject"       // IDX_WRAPPED_JSOBJECT
};

/***************************************************************************/

#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
JS_STATIC_DLL_CALLBACK(JSHashNumber)
hash_root(const void *key)
{
    return ((JSHashNumber) key) >> 2; /* help lame MSVC1.5 on Win16 */
}

JS_STATIC_DLL_CALLBACK(intN)
DEBUG_WrapperChecker(JSHashEntry *he, intN i, void *arg)
{
    NS_ASSERTION(!((nsXPCWrappedNative*)he->value)->IsValid(), "found a 'valid' wrapper!");
    ++ *((int*)arg);
    return HT_ENUMERATE_NEXT;
}
#endif

JS_STATIC_DLL_CALLBACK(intN)
WrappedJSShutdownMarker(JSHashEntry *he, intN i, void *arg)
{
    JSRuntime* rt = (JSRuntime*) arg;
    nsXPCWrappedJS* wrapper = (nsXPCWrappedJS*)he->value;
    NS_ASSERTION(wrapper, "found a null JS wrapper!");
    NS_ASSERTION(wrapper->IsValid(), "found an invalid JS wrapper!");
    wrapper->SystemIsBeingShutDown(rt);
    return HT_ENUMERATE_NEXT;
}

XPCJSRuntime::~XPCJSRuntime()
{
#ifdef XPC_DUMP_AT_SHUTDOWN
    {
    // count the total JSContexts in use
    JSContext* iter = nsnull;
    int count = 0;
    while(JS_ContextIterator(mJSRuntime, &iter))
        count ++;
    if(count)
        printf("deleting XPCJSRuntime with %d live JSContexts\n", count);        
    }
#endif
    
    // clean up and destroy maps...

    if(mContextMap)
    {
        PurgeXPCContextList();
        delete mContextMap;
    }

    if(mWrappedJSMap)
    {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mWrappedJSMap->Count();
        if(count)
            printf("deleting XPCJSRuntime with %d live wrapped JSObject\n", (int)count);        
#endif
        mWrappedJSMap->Enumerate(WrappedJSShutdownMarker, mJSRuntime); 
        delete mWrappedJSMap;
    }

    if(mWrappedJSClassMap)
    {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mWrappedJSClassMap->Count();
        if(count)
            printf("deleting XPCJSRuntime with %d live nsXPCWrappedJSClass\n", (int)count);        
#endif
        delete mWrappedJSClassMap;
    }

    if(mWrappedNativeClassMap)
    {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mWrappedNativeClassMap->Count();
        if(count)
            printf("deleting XPCJSRuntime with %d live nsXPCWrappedNativeClass\n", (int)count);        
#endif
        delete mWrappedNativeClassMap;
    }

    if(mMapLock)
        PR_DestroyLock(mMapLock);
    NS_IF_RELEASE(mJSRuntimeService);

#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
    int LiveWrapperCount = 0;
    JS_HashTableEnumerateEntries(DEBUG_WrappedNativeHashtable, 
                                 DEBUG_WrapperChecker, &LiveWrapperCount);
    if(LiveWrapperCount)
        printf("deleting XPCJSRuntime with %d live nsXPCWrappedNative (found in wrapper check)\n", (int)LiveWrapperCount);        
    JS_HashTableDestroy(DEBUG_WrappedNativeHashtable);
#endif
}

XPCJSRuntime::XPCJSRuntime(nsXPConnect* aXPConnect,
                           nsIJSRuntimeService* aJSRuntimeService)
 : mXPConnect(aXPConnect),
   mJSRuntime(nsnull),
   mJSRuntimeService(aJSRuntimeService),
   mContextMap(JSContext2XPCContextMap::newMap(XPC_CONTEXT_MAP_SIZE)),
   mWrappedJSMap(JSObject2WrappedJSMap::newMap(XPC_JS_MAP_SIZE)),
   mWrappedJSClassMap(IID2WrappedJSClassMap::newMap(XPC_JS_CLASS_MAP_SIZE)),
   mWrappedNativeClassMap(IID2WrappedNativeClassMap::newMap(XPC_NATIVE_CLASS_MAP_SIZE)),
   mMapLock(PR_NewLock())
{

#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
   DEBUG_WrappedNativeHashtable = JS_NewHashTable(128, hash_root,
                                                  JS_CompareValues, 
                                                  JS_CompareValues,
                                                  nsnull, nsnull);
#endif

    // these jsids filled in later when we have a JSContext to work with.
    mStrIDs[0] = 0;

    if(mJSRuntimeService)
    {
        NS_ADDREF(mJSRuntimeService);
        mJSRuntimeService->GetRuntime(&mJSRuntime);
    }

    // Install a JavaScript 'debugger' keyword handler in debug builds only
#ifdef DEBUG
    if(mJSRuntime)
        xpc_InstallJSDebuggerKeywordHandler(mJSRuntime);
#endif
} 

// static
XPCJSRuntime*
XPCJSRuntime::newXPCJSRuntime(nsXPConnect* aXPConnect,
                              nsIJSRuntimeService* aJSRuntimeService)
{
    NS_PRECONDITION(aXPConnect,"bad param");
    NS_PRECONDITION(aJSRuntimeService,"bad param");

    XPCJSRuntime* self;

    self = new XPCJSRuntime(aXPConnect, 
                            aJSRuntimeService);

    if(self                             &&
       self->GetJSRuntime()             &&
       self->GetContextMap()            &&
       self->GetWrappedJSMap()          &&
       self->GetWrappedJSClassMap()     &&
       self->GetWrappedNativeClassMap() &&
       self->GetMapLock())
    {
        return self;
    }
    delete self;
    return nsnull;
}

XPCContext*
XPCJSRuntime::GetXPCContext(JSContext* cx)
{
    XPCContext* xpcc;

    // find it in the map.

    PR_Lock(mMapLock);
    xpcc = mContextMap->Find(cx);
    PR_Unlock(mMapLock);

    // else resync with the JSRuntime's JSContext list and see if it is found
    if(!xpcc)
        xpcc = SyncXPCContextList(cx);
    return xpcc;
}


JS_STATIC_DLL_CALLBACK(intN)
KillDeadContextsCB(JSHashEntry *he, intN i, void *arg)
{
    JSRuntime* rt = (JSRuntime*) arg;
    XPCContext* xpcc = (XPCContext*) he->value;
    JSContext* cx = xpcc->GetJSContext();
    JSContext* iter = nsnull;
    JSContext* cur;

    // if this XPCContext represents a live JSContext then fine.
    while(nsnull != (cur = JS_ContextIterator(rt, &iter)))
    {
        if(cur == cx)
            return HT_ENUMERATE_NEXT;
    }

    // this XPCContext represents a dead JSContext - delete it
    delete xpcc;
    return HT_ENUMERATE_REMOVE;
}

XPCContext*
XPCJSRuntime::SyncXPCContextList(JSContext* cx /* = nsnull */)
{
    // hold the map lock through this whole thing
    nsAutoLock lock(mMapLock);

    // get rid of any XPCContexts that represent dead JSContexts
    mContextMap->Enumerate(KillDeadContextsCB, mJSRuntime);
    
    XPCContext* found = nsnull;

    // add XPCContexts that represent any JSContexts we have not seen before
    JSContext *cur, *iter = nsnull;
    while(nsnull != (cur = JS_ContextIterator(mJSRuntime, &iter)))
    {
        XPCContext* xpcc = mContextMap->Find(cur);
        if(!xpcc)
        {
            xpcc = XPCContext::newXPCContext(this, cur);
            if(xpcc)
                mContextMap->Add(xpcc);
        }

        // if it is our first context then we need to generate our string ids
        if(!mStrIDs[0])
            GenerateStringIDs(cur);

        if(cx && cx == cur)
            found = xpcc;
    }
    return found;
}

JS_STATIC_DLL_CALLBACK(intN)
PurgeContextsCB(JSHashEntry *he, intN i, void *arg)
{
    delete (XPCContext*) he->value;
    return HT_ENUMERATE_REMOVE;
}

void 
XPCJSRuntime::PurgeXPCContextList()
{
    // hold the map lock through this whole thing
    nsAutoLock lock(mMapLock);

    // get rid of all XPCContexts
    mContextMap->Enumerate(PurgeContextsCB, nsnull);
}

JSBool 
XPCJSRuntime::GenerateStringIDs(JSContext* cx)
{
    NS_PRECONDITION(!mStrIDs[0],"string ids generated twice!");
    for(uintN i = 0; i < IDX_TOTAL_COUNT; i++)
    {
        JS_ValueToId(cx,
                     STRING_TO_JSVAL(JS_InternString(cx, mStrings[i])),
                     &mStrIDs[i]);
        if(!mStrIDs[i])
        {
            mStrIDs[0] = 0;
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

#ifdef DEBUG
JS_STATIC_DLL_CALLBACK(intN)
ContextMapDumpEnumerator(JSHashEntry *he, intN i, void *arg)
{
    ((XPCContext*)he->value)->DebugDump(*(PRInt16*)arg);
    return HT_ENUMERATE_NEXT;
}
JS_STATIC_DLL_CALLBACK(intN)
WrappedJSClassMapDumpEnumerator(JSHashEntry *he, intN i, void *arg)
{
    ((nsXPCWrappedJSClass*)he->value)->DebugDump(*(PRInt16*)arg);
    return HT_ENUMERATE_NEXT;
}
JS_STATIC_DLL_CALLBACK(intN)
WrappedJSMapDumpEnumerator(JSHashEntry *he, intN i, void *arg)
{
    ((nsXPCWrappedJS*)he->value)->DebugDump(*(PRInt16*)arg);
    return HT_ENUMERATE_NEXT;
}
JS_STATIC_DLL_CALLBACK(intN)
WrappedNativeClassMapDumpEnumerator(JSHashEntry *he, intN i, void *arg)
{
    ((nsXPCWrappedNativeClass*)he->value)->DebugDump(*(PRInt16*)arg);
    return HT_ENUMERATE_NEXT;
}
#endif

void
XPCJSRuntime::DebugDump(PRInt16 depth)
{
#ifdef DEBUG
    depth--;
    XPC_LOG_ALWAYS(("XPCJSRuntime @ %x", this));
        XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("mXPConnect @ %x", mXPConnect));
        XPC_LOG_ALWAYS(("mJSRuntime @ %x", mJSRuntime));
        XPC_LOG_ALWAYS(("mMapLock @ %x", mMapLock));

        XPC_LOG_ALWAYS(("mContextMap @ %x with %d context(s)", \
                         mContextMap, mContextMap ? mContextMap->Count() : 0));
        // iterate contexts...
        if(depth && mContextMap && mContextMap->Count())
        {
            XPC_LOG_INDENT();
            mContextMap->Enumerate(ContextMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }

        XPC_LOG_ALWAYS(("mWrappedJSClassMap @ %x with %d wrapperclasses(s)", \
                         mWrappedJSClassMap, mWrappedJSClassMap ? \
                                            mWrappedJSClassMap->Count() : 0));
        // iterate wrappersclasses...
        if(depth && mWrappedJSClassMap && mWrappedJSClassMap->Count())
        {
            XPC_LOG_INDENT();
            mWrappedJSClassMap->Enumerate(WrappedJSClassMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }
        XPC_LOG_ALWAYS(("mWrappedJSMap @ %x with %d wrappers(s)", \
                         mWrappedJSMap, mWrappedJSMap ? \
                                            mWrappedJSMap->Count() : 0));
        // iterate wrappers...
        if(depth && mWrappedJSMap && mWrappedJSMap->Count())
        {
            XPC_LOG_INDENT();
            mWrappedJSMap->Enumerate(WrappedJSMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }

        XPC_LOG_ALWAYS(("mWrappedNativeClassMap @ %x with %d wrapperclasses(s)", \
                         mWrappedNativeClassMap, mWrappedNativeClassMap ? \
                                            mWrappedNativeClassMap->Count() : 0));
        // iterate wrappersclasses...
        if(depth && mWrappedNativeClassMap && mWrappedNativeClassMap->Count())
        {
            XPC_LOG_INDENT();
            mWrappedNativeClassMap->Enumerate(WrappedNativeClassMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }
        XPC_LOG_OUTDENT();
#endif
}

