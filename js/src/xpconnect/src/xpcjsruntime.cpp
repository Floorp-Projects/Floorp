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
 *   John Bandhauer <jband@netscape.com> (original author)
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
    "toSource",             // IDX_TO_SOURCE
    "lastResult",           // IDX_LAST_RESULT
    "returnCode",           // IDX_RETURN_CODE
    "value",                // IDX_VALUE
    "QueryInterface",       // IDX_QUERY_INTERFACE
    "Components",           // IDX_COMPONENTS
    "wrappedJSObject",      // IDX_WRAPPED_JSOBJECT
    "Object",               // IDX_OBJECT
    "prototype",            // IDX_PROTOTYPE
    "createInstance"        // IDX_CREATE_INSTANCE
};

/***************************************************************************/

// GCCallback calls are chained
static JSGCCallback gOldJSGCCallback;

// data holder class for the enumerator callback below
struct JSDyingJSObjectData
{
    JSContext* cx;
    nsVoidArray* array;
};

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
WrappedJSDyingJSObjectFinder(JSDHashTable *table, JSDHashEntryHdr *hdr,
                uint32 number, void *arg)
{
    JSDyingJSObjectData* data = (JSDyingJSObjectData*) arg;
    nsXPCWrappedJS* wrapper = ((JSObject2WrappedJSMap::Entry*)hdr)->value;
    NS_ASSERTION(wrapper, "found a null JS wrapper!");

    // walk the wrapper chain and find any whose JSObject is to be finalized
    while(wrapper)
    {
        if(wrapper->IsSubjectToFinalization())
        {
            if(JS_IsAboutToBeFinalized(data->cx, wrapper->GetJSObject()))
                data->array->AppendElement(wrapper);
        }
        wrapper = wrapper->GetNextWrapper();
    }
    return JS_DHASH_NEXT;
}

struct CX_AND_XPCRT_Data
{
    JSContext* cx;
    XPCJSRuntime* rt;
};

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
NativeInterfaceGC(JSDHashTable *table, JSDHashEntryHdr *hdr,
                  uint32 number, void *arg)
{
    CX_AND_XPCRT_Data* data = (CX_AND_XPCRT_Data*) arg;
    ((IID2NativeInterfaceMap::Entry*)hdr)->value->
            DealWithDyingGCThings(data->cx, data->rt);
    return JS_DHASH_NEXT;
}

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
NativeInterfaceSweeper(JSDHashTable *table, JSDHashEntryHdr *hdr,
                       uint32 number, void *arg)
{
    CX_AND_XPCRT_Data* data = (CX_AND_XPCRT_Data*) arg;
    XPCNativeInterface* iface = ((IID2NativeInterfaceMap::Entry*)hdr)->value;
    if(iface->IsMarked())
    {
        iface->Unmark();
        return JS_DHASH_NEXT;
    }

#ifdef XPC_REPORT_NATIVE_INTERFACE_AND_SET_FLUSHING
    printf("- Destroying XPCNativeInterface for %s\n",
            JS_GetStringBytes(JSVAL_TO_STRING(iface->GetName())));
#endif

    XPCNativeInterface::DestroyInstance(data->cx, data->rt, iface);
    return JS_DHASH_REMOVE;
}

// *Some* NativeSets are referenced from mClassInfo2NativeSetMap.
// *All* NativeSets are referenced from mNativeSetMap.
// So, in mClassInfo2NativeSetMap we just clear references to the unmarked.
// In mNativeSetMap we clear the references to the unmarked *and* delete them.

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
NativeUnMarkedSetRemover(JSDHashTable *table, JSDHashEntryHdr *hdr,
                         uint32 number, void *arg)
{
    XPCNativeSet* set = ((ClassInfo2NativeSetMap::Entry*)hdr)->value;
    if(set->IsMarked())
        return JS_DHASH_NEXT;
    return JS_DHASH_REMOVE;
}

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
NativeSetSweeper(JSDHashTable *table, JSDHashEntryHdr *hdr,
                 uint32 number, void *arg)
{
    XPCNativeSet* set = ((NativeSetMap::Entry*)hdr)->key_value;
    if(set->IsMarked())
    {
        set->Unmark();
        return JS_DHASH_NEXT;
    }

#ifdef XPC_REPORT_NATIVE_INTERFACE_AND_SET_FLUSHING
    printf("- Destroying XPCNativeSet for:\n");
    PRUint16 count = set->GetInterfaceCount();
    for(PRUint16 k = 0; k < count; k++)
    {
        XPCNativeInterface* iface = set->GetInterfaceAt(k);
        printf("    %s\n",JS_GetStringBytes(JSVAL_TO_STRING(iface->GetName())));
    }
#endif

    XPCNativeSet::DestroyInstance(set);
    return JS_DHASH_REMOVE;
}

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
JSClassSweeper(JSDHashTable *table, JSDHashEntryHdr *hdr,
               uint32 number, void *arg)
{
    XPCNativeScriptableShared* shared =
        ((XPCNativeScriptableSharedMap::Entry*) hdr)->key;
    if(shared->IsMarked())
    {
#ifdef off_XPC_REPORT_JSCLASS_FLUSHING
        printf("+ Marked XPCNativeScriptableShared for: %s @ %x\n",
               shared->GetJSClass()->name,
               shared->GetJSClass());
#endif
        shared->Unmark();
        return JS_DHASH_NEXT;
    }

#ifdef XPC_REPORT_JSCLASS_FLUSHING
    printf("- Destroying XPCNativeScriptableShared for: %s @ %x\n",
           shared->GetJSClass()->name,
           shared->GetJSClass());
#endif

    delete shared;
    return JS_DHASH_REMOVE;
}

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
DyingProtoKiller(JSDHashTable *table, JSDHashEntryHdr *hdr,
                 uint32 number, void *arg)
{
    XPCWrappedNativeProto* proto =
        (XPCWrappedNativeProto*)((JSDHashEntryStub*)hdr)->key;
    delete proto;
    return JS_DHASH_REMOVE;
}

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
DetachedWrappedNativeProtoMarker(JSDHashTable *table, JSDHashEntryHdr *hdr,
                                 uint32 number, void *arg)
{
    XPCWrappedNativeProto* proto = 
        (XPCWrappedNativeProto*)((JSDHashEntryStub*)hdr)->key;

    proto->Mark();
    return JS_DHASH_NEXT;
}

// static
JSBool XPCJSRuntime::GCCallback(JSContext *cx, JSGCStatus status)
{
    nsVoidArray* dyingWrappedJSArray;

    XPCJSRuntime* self = nsXPConnect::GetRuntime();
    if(self)
    {
        switch(status)
        {
            case JSGC_BEGIN:
            {
                if(self->GetMainThreadOnlyGC() &&
                   PR_GetCurrentThread() != nsXPConnect::GetMainThread())
                {
                    return JS_FALSE;
                }
                break;
            }
            case JSGC_MARK_END:
            {
                NS_ASSERTION(!self->mDoingFinalization, "bad state");

                // Skip this part if XPConnect is shutting down. We get into
                // bad locking problems with the thread iteration otherwise.
                if(!self->GetXPConnect()->IsShuttingDown())
                {
                    PRLock* threadLock = XPCPerThreadData::GetLock();
                    if(threadLock)
                    { // scoped lock
                        nsAutoLock lock(threadLock);

                        XPCPerThreadData* iterp = nsnull;
                        XPCPerThreadData* thread;

                        while(nsnull != (thread =
                                     XPCPerThreadData::IterateThreads(&iterp)))
                        {
                            // Mark those AutoMarkingPtr lists!
                            thread->MarkAutoRootsBeforeJSFinalize(cx);
                        }
                    }
                }

                dyingWrappedJSArray = &self->mWrappedJSToReleaseArray;
                {
                    XPCLock* lock = self->GetMainThreadOnlyGC() ?
                                    nsnull : self->GetMapLock();

                    XPCAutoLock al(lock); // lock the wrapper map if necessary
                    JSDyingJSObjectData data = {cx, dyingWrappedJSArray};

                    // Add any wrappers whose JSObjects are to be finalized to
                    // this array. Note that this is a nsVoidArray because
                    // we do not want to be changing the refcount of these wrappers.
                    // We add them to the array now and Release the array members
                    // later to avoid the posibility of doing any JS GCThing
                    // allocations during the gc cycle.
                    self->mWrappedJSMap->
                        Enumerate(WrappedJSDyingJSObjectFinder, &data);
                }

                // Do cleanup in NativeInterfaces. This part just finds 
                // member cloned function objects that are about to be 
                // collected. It does not deal with collection of interfaces or
                // sets at this point.
                CX_AND_XPCRT_Data data = {cx, self};

                self->mIID2NativeInterfaceMap->
                    Enumerate(NativeInterfaceGC, &data);

                // Find dying scopes...
                XPCWrappedNativeScope::FinishedMarkPhaseOfGC(cx, self);

                self->mDoingFinalization = JS_TRUE;
                break;
            }
            case JSGC_FINALIZE_END:
            {
                NS_ASSERTION(self->mDoingFinalization, "bad state");
                self->mDoingFinalization = JS_FALSE;

#ifdef XPC_REPORT_NATIVE_INTERFACE_AND_SET_FLUSHING
                printf("--------------------------------------------------------------\n");
                int setsBefore = (int) self->mNativeSetMap->Count();
                int ifacesBefore = (int) self->mIID2NativeInterfaceMap->Count();
#endif

                // We use this occasion to mark and sweep NativeInterfaces,
                // NativeSets, and the WrappedNativeJSClasses...

                // Do the marking...
                XPCWrappedNativeScope::MarkAllWrappedNativesAndProtos();

                self->mDetachedWrappedNativeProtoMap->
                    Enumerate(DetachedWrappedNativeProtoMarker, nsnull);

                // Mark the sets used in the call contexts. There is a small
                // chance that a wrapper's set will change *while* a call is
                // happening which uses that wrapper's old interfface set. So,
                // we need to do this marking to avoid collecting those sets
                // that might no longer be otherwise reachable from the wrappers
                // or the wrapperprotos.

                // Skip this part if XPConnect is shutting down. We get into
                // bad locking problems with the thread iteration otherwise.
                if(!self->GetXPConnect()->IsShuttingDown())
                {
                    PRLock* threadLock = XPCPerThreadData::GetLock();
                    if(threadLock)
                    { // scoped lock
                        nsAutoLock lock(threadLock);

                        XPCPerThreadData* iterp = nsnull;
                        XPCPerThreadData* thread;

                        while(nsnull != (thread =
                                     XPCPerThreadData::IterateThreads(&iterp)))
                        {
                            // Mark those AutoMarkingPtr lists!
                            thread->MarkAutoRootsAfterJSFinalize();

                            XPCCallContext* ccxp = thread->GetCallContext();
                            while(ccxp)
                            {
                                // Deal with the strictness of callcontext that
                                // complains if you ask for a set when
                                // it is in a state where the set could not
                                // possibly be valid.
                                if(ccxp->CanGetSet())
                                {
                                    XPCNativeSet* set = ccxp->GetSet();
                                    if(set)
                                        set->Mark();
                                }
                                if(ccxp->CanGetInterface())
                                {
                                    XPCNativeInterface* iface = ccxp->GetInterface();
                                    if(iface)
                                        iface->Mark();
                                }
                                ccxp = ccxp->GetPrevCallContext();
                            }
                        }
                    }
                }

                // Do the sweeping...

                // We don't want to sweep the JSClasses at shutdown time.
                // At this point there may be JSObjects using them that have
                // been removed from the other maps.
                if(!self->GetXPConnect()->IsShuttingDown())
                {
                    self->mNativeScriptableSharedMap->
                        Enumerate(JSClassSweeper, nsnull);
                }

                self->mClassInfo2NativeSetMap->
                    Enumerate(NativeUnMarkedSetRemover, nsnull);

                self->mNativeSetMap->
                    Enumerate(NativeSetSweeper, nsnull);

                CX_AND_XPCRT_Data data = {cx, self};

                self->mIID2NativeInterfaceMap->
                    Enumerate(NativeInterfaceSweeper, &data);

#ifdef DEBUG
                XPCWrappedNativeScope::ASSERT_NoInterfaceSetsAreMarked();
#endif

#ifdef XPC_REPORT_NATIVE_INTERFACE_AND_SET_FLUSHING
                int setsAfter = (int) self->mNativeSetMap->Count();
                int ifacesAfter = (int) self->mIID2NativeInterfaceMap->Count();

                printf("\n");
                printf("XPCNativeSets:        before: %d  collected: %d  remaining: %d\n",
                       setsBefore, setsBefore - setsAfter, setsAfter);
                printf("XPCNativeInterfaces:  before: %d  collected: %d  remaining: %d\n",
                       ifacesBefore, ifacesBefore - ifacesAfter, ifacesAfter);
                printf("--------------------------------------------------------------\n");
#endif

                // Sweep scopes needing cleanup
                XPCWrappedNativeScope::FinishedFinalizationPhaseOfGC(cx);

                // Now we are going to recycle any unused WrappedNativeTearoffs.
                // We do this by iterating all the live callcontexts (on all
                // threads!) and marking the tearoffs in use. And then we
                // iterate over all the WrappedNative wrappers and sweep their
                // tearoffs.
                //
                // This allows us to perhaps minimize the growth of the
                // tearoffs. And also makes us not hold references to interfaces
                // on our wrapped natives that we are not actually using.
                //
                // XXX We may decide to not do this on *every* gc cycle.

                // Skip this part if XPConnect is shutting down. We get into
                // bad locking problems with the thread iteration otherwise.
                if(!self->GetXPConnect()->IsShuttingDown())
                {
                    PRLock* threadLock = XPCPerThreadData::GetLock();
                    if(threadLock)
                    {
                        // Do the marking...
                        
                        { // scoped lock
                            nsAutoLock lock(threadLock);

                            XPCPerThreadData* iterp = nsnull;
                            XPCPerThreadData* thread;

                            while(nsnull != (thread =
                                     XPCPerThreadData::IterateThreads(&iterp)))
                            {
                                XPCCallContext* ccxp = thread->GetCallContext();
                                while(ccxp)
                                {
                                    // Deal with the strictness of callcontext that
                                    // complains if you ask for a tearoff when
                                    // it is in a state where the tearoff could not
                                    // possibly be valid.
                                    if(ccxp->CanGetTearOff())
                                    {
                                        XPCWrappedNativeTearOff* to = 
                                            ccxp->GetTearOff();
                                        if(to)
                                            to->Mark();
                                    }
                                    ccxp = ccxp->GetPrevCallContext();
                                }
                            }
                        }
    
                        // Do the sweeping...
                        XPCWrappedNativeScope::SweepAllWrappedNativeTearOffs();
                    }
                }

                // Now we need to kill the 'Dying' XPCWrappedNativeProtos.
                // We transfered these native objects to this table when their
                // JSObject's were finalized. We did not destroy them immediately
                // at that point because the ordering of JS finalization is not
                // deterministic and we did not yet know if any wrappers that
                // might still be referencing the protos where still yet to be
                // finalized and destroyed. We *do* know that the protos'
                // JSObjects would not have been finalized if there were any
                // wrappers that referenced the proto but where not themselves
                // slated for finalization in this gc cycle. So... at this point
                // we know that any and all wrappers that might have been
                // referencing the protos in the dying list are themselves dead.
                // So, we can safely delete all the protos in the list.

                self->mDyingWrappedNativeProtoMap->
                    Enumerate(DyingProtoKiller, nsnull);

                break;
            }
            case JSGC_END:
            {
                // NOTE that this event happens outside of the gc lock in
                // the js engine. So this could be sumultaneous with the
                // events above.

                // Release all the members whose JSObjects are now known
                // to be dead.

                dyingWrappedJSArray = &self->mWrappedJSToReleaseArray;
                XPCLock* lock = self->GetMainThreadOnlyGC() ?
                                nsnull : self->GetMapLock();
                while(1)
                {
                    nsXPCWrappedJS* wrapper;
                    {
                        XPCAutoLock al(lock); // lock if necessary
                        PRInt32 count = dyingWrappedJSArray->Count();
                        if(!count)
                        {
                            dyingWrappedJSArray->Compact();
                            break;
                        }
                        wrapper = NS_REINTERPRET_CAST(nsXPCWrappedJS*,
                                    dyingWrappedJSArray->ElementAt(count-1));
                        dyingWrappedJSArray->RemoveElementAt(count-1);
                    }
                    NS_RELEASE(wrapper);
                }

                // Do any deferred released of native objects.
                if(self->GetDeferReleases())
                {
                    nsVoidArray* array = &self->mNativesToReleaseArray;
#ifdef XPC_TRACK_DEFERRED_RELEASES
                    printf("XPC - Begin deferred Release of %d nsISupports pointers\n",
                           array->Count());
#endif
                    while(1)
                    {
                        nsISupports* obj;
                        {
                            XPCAutoLock al(lock); // lock if necessary
                            PRInt32 count = array->Count();
                            if(!count)
                            {
                                array->Compact();
                                break;
                            }
                            obj = NS_REINTERPRET_CAST(nsISupports*,
                                    array->ElementAt(count-1));
                            array->RemoveElementAt(count-1);
                        }
                        NS_RELEASE(obj);
                    }
#ifdef XPC_TRACK_DEFERRED_RELEASES
                    printf("XPC - End deferred Releases\n");
#endif
                }
                break;
            }
            default:
                break;
        }
    }

    // always chain to old GCCallback if non-null.
    return gOldJSGCCallback ? gOldJSGCCallback(cx, status) : JS_TRUE;
}

/***************************************************************************/

#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
JS_STATIC_DLL_CALLBACK(JSDHashOperator)
DEBUG_WrapperChecker(JSDHashTable *table, JSDHashEntryHdr *hdr,
                     uint32 number, void *arg)
{
    XPCWrappedNative* wrapper = (XPCWrappedNative*)((JSDHashEntryStub*)hdr)->key;
    NS_ASSERTION(!wrapper->IsValid(), "found a 'valid' wrapper!");
    ++ *((int*)arg);
    return JS_DHASH_NEXT;
}
#endif

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
WrappedJSShutdownMarker(JSDHashTable *table, JSDHashEntryHdr *hdr,
                        uint32 number, void *arg)
{
    JSRuntime* rt = (JSRuntime*) arg;
    nsXPCWrappedJS* wrapper = ((JSObject2WrappedJSMap::Entry*)hdr)->value;
    NS_ASSERTION(wrapper, "found a null JS wrapper!");
    NS_ASSERTION(wrapper->IsValid(), "found an invalid JS wrapper!");
    wrapper->SystemIsBeingShutDown(rt);
    return JS_DHASH_NEXT;
}

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
DetachedWrappedNativeProtoShutdownMarker(JSDHashTable *table, JSDHashEntryHdr *hdr,
                                         uint32 number, void *arg)
{
    XPCWrappedNativeProto* proto = 
        (XPCWrappedNativeProto*)((JSDHashEntryStub*)hdr)->key;

    proto->SystemIsBeingShutDown(*((XPCCallContext*)arg));
    return JS_DHASH_NEXT;
}

void XPCJSRuntime::SystemIsBeingShutDown(XPCCallContext* ccx)
{
    if(mDetachedWrappedNativeProtoMap)
        mDetachedWrappedNativeProtoMap->
            Enumerate(DetachedWrappedNativeProtoShutdownMarker, ccx);
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

    if(mIID2NativeInterfaceMap)
    {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mIID2NativeInterfaceMap->Count();
        if(count)
            printf("deleting XPCJSRuntime with %d live XPCNativeInterfaces\n", (int)count);
#endif
        delete mIID2NativeInterfaceMap;
    }

    if(mClassInfo2NativeSetMap)
    {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mClassInfo2NativeSetMap->Count();
        if(count)
            printf("deleting XPCJSRuntime with %d live XPCNativeSets\n", (int)count);
#endif
        delete mClassInfo2NativeSetMap;
    }

    if(mNativeSetMap)
    {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mNativeSetMap->Count();
        if(count)
            printf("deleting XPCJSRuntime with %d live XPCNativeSets\n", (int)count);
#endif
        delete mNativeSetMap;
    }

    if(mMapLock)
        XPCAutoLock::DestroyLock(mMapLock);
    NS_IF_RELEASE(mJSRuntimeService);

    if(mThisTranslatorMap)
    {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mThisTranslatorMap->Count();
        if(count)
            printf("deleting XPCJSRuntime with %d live ThisTranslator\n", (int)count);
#endif
        delete mThisTranslatorMap;
    }

#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
    if(DEBUG_WrappedNativeHashtable)
    {
        int LiveWrapperCount = 0;
        JS_DHashTableEnumerate(DEBUG_WrappedNativeHashtable,
                               DEBUG_WrapperChecker, &LiveWrapperCount);
        if(LiveWrapperCount)
            printf("deleting XPCJSRuntime with %d live XPCWrappedNative (found in wrapper check)\n", (int)LiveWrapperCount);
        JS_DHashTableDestroy(DEBUG_WrappedNativeHashtable);
    }
#endif

    if(mNativeScriptableSharedMap)
    {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mNativeScriptableSharedMap->Count();
        if(count)
            printf("deleting XPCJSRuntime with %d live XPCNativeScriptableShared\n", (int)count);
#endif
        delete mNativeScriptableSharedMap;
    }

    if(mDyingWrappedNativeProtoMap)
    {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mDyingWrappedNativeProtoMap->Count();
        if(count)
            printf("deleting XPCJSRuntime with %d live but dying XPCWrappedNativeProto\n", (int)count);
#endif
        delete mDyingWrappedNativeProtoMap;
    }

    if(mDetachedWrappedNativeProtoMap)
    {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mDetachedWrappedNativeProtoMap->Count();
        if(count)
            printf("deleting XPCJSRuntime with %d live detached XPCWrappedNativeProto\n", (int)count);
#endif
        delete mDetachedWrappedNativeProtoMap;
    }

    // unwire the readable/JSString sharing magic
    XPCStringConvert::ShutdownDOMStringFinalizer();
}

XPCJSRuntime::XPCJSRuntime(nsXPConnect* aXPConnect,
                           nsIJSRuntimeService* aJSRuntimeService)
 : mXPConnect(aXPConnect),
   mJSRuntime(nsnull),
   mJSRuntimeService(aJSRuntimeService),
   mContextMap(JSContext2XPCContextMap::newMap(XPC_CONTEXT_MAP_SIZE)),
   mWrappedJSMap(JSObject2WrappedJSMap::newMap(XPC_JS_MAP_SIZE)),
   mWrappedJSClassMap(IID2WrappedJSClassMap::newMap(XPC_JS_CLASS_MAP_SIZE)),
   mIID2NativeInterfaceMap(IID2NativeInterfaceMap::newMap(XPC_NATIVE_INTERFACE_MAP_SIZE)),
   mClassInfo2NativeSetMap(ClassInfo2NativeSetMap::newMap(XPC_NATIVE_SET_MAP_SIZE)),
   mNativeSetMap(NativeSetMap::newMap(XPC_NATIVE_SET_MAP_SIZE)),
   mThisTranslatorMap(IID2ThisTranslatorMap::newMap(XPC_THIS_TRANSLATOR_MAP_SIZE)),
   mNativeScriptableSharedMap(XPCNativeScriptableSharedMap::newMap(XPC_NATIVE_JSCLASS_MAP_SIZE)),
   mDyingWrappedNativeProtoMap(XPCWrappedNativeProtoMap::newMap(XPC_DYING_NATIVE_PROTO_MAP_SIZE)),
   mDetachedWrappedNativeProtoMap(XPCWrappedNativeProtoMap::newMap(XPC_DETACHED_NATIVE_PROTO_MAP_SIZE)),
   mMapLock(XPCAutoLock::NewLock("XPCJSRuntime::mMapLock")),
   mWrappedJSToReleaseArray(),
   mNativesToReleaseArray(),
   mMainThreadOnlyGC(JS_FALSE),
   mDeferReleases(JS_FALSE),
   mDoingFinalization(JS_FALSE)
{
#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
    DEBUG_WrappedNativeHashtable =
        JS_NewDHashTable(JS_DHashGetStubOps(), nsnull,
                         sizeof(JSDHashEntryStub), 128);
#endif

    // these jsids filled in later when we have a JSContext to work with.
    mStrIDs[0] = 0;

    if(mJSRuntimeService)
    {
        NS_ADDREF(mJSRuntimeService);
        mJSRuntimeService->GetRuntime(&mJSRuntime);
    }

    NS_ASSERTION(!gOldJSGCCallback, "XPCJSRuntime created more than once");
    if(mJSRuntime)
        gOldJSGCCallback = JS_SetGCCallbackRT(mJSRuntime, GCCallback);

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

    if(self                                  &&
       self->GetJSRuntime()                  &&
       self->GetContextMap()                 &&
       self->GetWrappedJSMap()               &&
       self->GetWrappedJSClassMap()          &&
       self->GetIID2NativeInterfaceMap()     &&
       self->GetClassInfo2NativeSetMap()     &&
       self->GetNativeSetMap()               &&
       self->GetThisTranslatorMap()          &&
       self->GetNativeScriptableSharedMap()  &&
       self->GetDyingWrappedNativeProtoMap() &&
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

    { // scoped lock
        XPCAutoLock lock(GetMapLock());
        xpcc = mContextMap->Find(cx);
    }

    // else resync with the JSRuntime's JSContext list and see if it is found
    if(!xpcc)
        xpcc = SyncXPCContextList(cx);
    return xpcc;
}


JS_STATIC_DLL_CALLBACK(JSDHashOperator)
SweepContextsCB(JSDHashTable *table, JSDHashEntryHdr *hdr,
                   uint32 number, void *arg)
{
    XPCContext* xpcc = ((JSContext2XPCContextMap::Entry*)hdr)->value;
    if(xpcc->IsMarked())
    {
        xpcc->Unmark();
        return JS_DHASH_NEXT;
    }

    // this XPCContext represents a dead JSContext - delete it
    delete xpcc;
    return JS_DHASH_REMOVE;
}

XPCContext*
XPCJSRuntime::SyncXPCContextList(JSContext* cx /* = nsnull */)
{
    // hold the map lock through this whole thing
    XPCAutoLock lock(GetMapLock());

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
        if(xpcc)
        {
            xpcc->Mark();
        }

        // if it is our first context then we need to generate our string ids
        if(!mStrIDs[0])
            GenerateStringIDs(cur);

        if(cx && cx == cur)
            found = xpcc;
    }
    // get rid of any XPCContexts that represent dead JSContexts
    mContextMap->Enumerate(SweepContextsCB, 0);

    XPCPerThreadData* tls = XPCPerThreadData::GetData();
    if(tls)
    {
        if(found)
            tls->SetRecentContext(cx, found);
        else
            tls->ClearRecentContext();
    }

    return found;
}


JS_STATIC_DLL_CALLBACK(JSDHashOperator)
PurgeContextsCB(JSDHashTable *table, JSDHashEntryHdr *hdr,
                uint32 number, void *arg)
{
    delete ((JSContext2XPCContextMap::Entry*)hdr)->value;
    return JS_DHASH_REMOVE;
}

void
XPCJSRuntime::PurgeXPCContextList()
{
    // hold the map lock through this whole thing
    XPCAutoLock lock(GetMapLock());

    // get rid of all XPCContexts
    mContextMap->Enumerate(PurgeContextsCB, nsnull);
}

JSBool
XPCJSRuntime::GenerateStringIDs(JSContext* cx)
{
    NS_PRECONDITION(!mStrIDs[0],"string ids generated twice!");
    for(uintN i = 0; i < IDX_TOTAL_COUNT; i++)
    {
        JSString* str = JS_InternString(cx, mStrings[i]);
        if(!str || !JS_ValueToId(cx, STRING_TO_JSVAL(str), &mStrIDs[i]))
        {
            mStrIDs[0] = 0;
            return JS_FALSE;
        }

        mStrJSVals[i] = STRING_TO_JSVAL(str);
    }
    return JS_TRUE;
}

JSBool
XPCJSRuntime::DeferredRelease(nsISupports* obj)
{
    NS_ASSERTION(obj, "bad param");
    NS_ASSERTION(GetDeferReleases(), "bad call");

    XPCLock* lock = GetMainThreadOnlyGC() ? nsnull : GetMapLock();
    {
        XPCAutoLock al(lock); // lock if necessary
        if(!mNativesToReleaseArray.Count())
        {
            // This array sometimes has 1000's
            // of entries, and usually has 50-200 entries. Avoid lots
            // of incremental grows.  We compact it down when we're done.
            mNativesToReleaseArray.SizeTo(256);
        }
        return mNativesToReleaseArray.AppendElement(obj);
    }        
}

/***************************************************************************/

#ifdef DEBUG
JS_STATIC_DLL_CALLBACK(JSDHashOperator)
ContextMapDumpEnumerator(JSDHashTable *table, JSDHashEntryHdr *hdr,
                         uint32 number, void *arg)
{
    ((JSContext2XPCContextMap::Entry*)hdr)->value->DebugDump(*(PRInt16*)arg);
    return JS_DHASH_NEXT;
}
JS_STATIC_DLL_CALLBACK(JSDHashOperator)
WrappedJSClassMapDumpEnumerator(JSDHashTable *table, JSDHashEntryHdr *hdr,
                                uint32 number, void *arg)
{
    ((IID2WrappedJSClassMap::Entry*)hdr)->value->DebugDump(*(PRInt16*)arg);
    return JS_DHASH_NEXT;
}
JS_STATIC_DLL_CALLBACK(JSDHashOperator)
WrappedJSMapDumpEnumerator(JSDHashTable *table, JSDHashEntryHdr *hdr,
                           uint32 number, void *arg)
{
    ((JSObject2WrappedJSMap::Entry*)hdr)->value->DebugDump(*(PRInt16*)arg);
    return JS_DHASH_NEXT;
}
JS_STATIC_DLL_CALLBACK(JSDHashOperator)
NativeSetDumpEnumerator(JSDHashTable *table, JSDHashEntryHdr *hdr,
                        uint32 number, void *arg)
{
    ((NativeSetMap::Entry*)hdr)->key_value->DebugDump(*(PRInt16*)arg);
    return JS_DHASH_NEXT;
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
        XPC_LOG_ALWAYS(("mJSRuntimeService @ %x", mJSRuntimeService));

        XPC_LOG_ALWAYS(("mWrappedJSToReleaseArray @ %x with %d wrappers(s)", \
                         &mWrappedJSToReleaseArray,
                         mWrappedJSToReleaseArray.Count()));

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

        XPC_LOG_ALWAYS(("mIID2NativeInterfaceMap @ %x with %d interface(s)", \
                         mIID2NativeInterfaceMap, mIID2NativeInterfaceMap ? \
                                    mIID2NativeInterfaceMap->Count() : 0));

        XPC_LOG_ALWAYS(("mClassInfo2NativeSetMap @ %x with %d sets(s)", \
                         mClassInfo2NativeSetMap, mClassInfo2NativeSetMap ? \
                                    mClassInfo2NativeSetMap->Count() : 0));

        XPC_LOG_ALWAYS(("mThisTranslatorMap @ %x with %d translator(s)", \
                         mThisTranslatorMap, mThisTranslatorMap ? \
                                    mThisTranslatorMap->Count() : 0));

        XPC_LOG_ALWAYS(("mNativeSetMap @ %x with %d sets(s)", \
                         mNativeSetMap, mNativeSetMap ? \
                                    mNativeSetMap->Count() : 0));

        // iterate sets...
        if(depth && mNativeSetMap && mNativeSetMap->Count())
        {
            XPC_LOG_INDENT();
            mNativeSetMap->Enumerate(NativeSetDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }

        XPC_LOG_OUTDENT();
#endif
}

