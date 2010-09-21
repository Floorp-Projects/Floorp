/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Per JSRuntime object */

#include "xpcprivate.h"
#include "dom_quickstubs.h"

#include "jsgcchunk.h"
#include "nsIMemoryReporter.h"
#include "mozilla/FunctionTimer.h"

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
    "Function",             // IDX_FUNCTION
    "prototype",            // IDX_PROTOTYPE
    "createInstance",       // IDX_CREATE_INSTANCE
    "item",                 // IDX_ITEM
    "__proto__",            // IDX_PROTO
    "__iterator__",         // IDX_ITERATOR
    "__exposedProps__"      // IDX_EXPOSEDPROPS
};

/***************************************************************************/

// data holder class for the enumerator callback below
struct JSDyingJSObjectData
{
    JSContext* cx;
    nsTArray<nsXPCWrappedJS*>* array;
};

static JSDHashOperator
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
            JSAutoEnterCompartment ac(data->cx, wrapper->GetJSObject());
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

static JSDHashOperator
NativeInterfaceSweeper(JSDHashTable *table, JSDHashEntryHdr *hdr,
                       uint32 number, void *arg)
{
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

    XPCNativeInterface::DestroyInstance(iface);
    return JS_DHASH_REMOVE;
}

// *Some* NativeSets are referenced from mClassInfo2NativeSetMap.
// *All* NativeSets are referenced from mNativeSetMap.
// So, in mClassInfo2NativeSetMap we just clear references to the unmarked.
// In mNativeSetMap we clear the references to the unmarked *and* delete them.

static JSDHashOperator
NativeUnMarkedSetRemover(JSDHashTable *table, JSDHashEntryHdr *hdr,
                         uint32 number, void *arg)
{
    XPCNativeSet* set = ((ClassInfo2NativeSetMap::Entry*)hdr)->value;
    if(set->IsMarked())
        return JS_DHASH_NEXT;
    return JS_DHASH_REMOVE;
}

static JSDHashOperator
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

static JSDHashOperator
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

static JSDHashOperator
DyingProtoKiller(JSDHashTable *table, JSDHashEntryHdr *hdr,
                 uint32 number, void *arg)
{
    XPCWrappedNativeProto* proto =
        (XPCWrappedNativeProto*)((JSDHashEntryStub*)hdr)->key;
    delete proto;
    return JS_DHASH_REMOVE;
}

static JSDHashOperator
DetachedWrappedNativeProtoMarker(JSDHashTable *table, JSDHashEntryHdr *hdr,
                                 uint32 number, void *arg)
{
    XPCWrappedNativeProto* proto = 
        (XPCWrappedNativeProto*)((JSDHashEntryStub*)hdr)->key;

    proto->Mark();
    return JS_DHASH_NEXT;
}

// GCCallback calls are chained
static JSBool
ContextCallback(JSContext *cx, uintN operation)
{
    XPCJSRuntime* self = nsXPConnect::GetRuntimeInstance();
    if(self)
    {
        if(operation == JSCONTEXT_NEW)
        {
            if(!self->OnJSContextNew(cx))
                return JS_FALSE;
        }
        else if(operation == JSCONTEXT_DESTROY)
        {
            delete XPCContext::GetXPCContext(cx);
        }
    }
    return JS_TRUE;
}

static JSBool
CompartmentCallback(JSContext *cx, JSCompartment *compartment, uintN op)
{
    if(op == JSCOMPARTMENT_NEW)
        return JS_TRUE;

    XPCJSRuntime* self = nsXPConnect::GetRuntimeInstance();
    if(!self)
        return JS_TRUE;

    XPCCompartmentMap& map = self->GetCompartmentMap();
    nsAdoptingCString origin;
    origin.Adopt(static_cast<char *>(JS_SetCompartmentPrivate(cx, compartment, nsnull)));

#ifdef DEBUG
    {
        JSCompartment *current;
        NS_ASSERTION(map.Get(origin, &current), "no compartment?");
        NS_ASSERTION(current == compartment, "compartment mismatch");
    }
#endif

    map.Remove(origin);
    return JS_TRUE;
}

struct ObjectHolder : public JSDHashEntryHdr
{
    void *holder;
    nsScriptObjectTracer* tracer;
};

nsresult
XPCJSRuntime::AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer)
{
    if(!mJSHolders.ops)
        return NS_ERROR_OUT_OF_MEMORY;

    ObjectHolder *entry =
        reinterpret_cast<ObjectHolder*>(JS_DHashTableOperate(&mJSHolders,
                                                             aHolder,
                                                             JS_DHASH_ADD));
    if(!entry)
        return NS_ERROR_OUT_OF_MEMORY;

    entry->holder = aHolder;
    entry->tracer = aTracer;

    return NS_OK;
}

nsresult
XPCJSRuntime::RemoveJSHolder(void* aHolder)
{
    if(!mJSHolders.ops)
        return NS_ERROR_OUT_OF_MEMORY;

    JS_DHashTableOperate(&mJSHolders, aHolder, JS_DHASH_REMOVE);

    return NS_OK;
}

// static
void XPCJSRuntime::TraceJS(JSTracer* trc, void* data)
{
    XPCJSRuntime* self = (XPCJSRuntime*)data;

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
                // Trace those AutoMarkingPtr lists!
                thread->TraceJS(trc);
            }
        }
    }

    // XPCJSObjectHolders don't participate in cycle collection, so always trace
    // them here.
    for(XPCRootSetElem *e = self->mObjectHolderRoots; e ; e = e->GetNextRoot())
        static_cast<XPCJSObjectHolder*>(e)->TraceJS(trc);

    // Mark these roots as gray so the CC can walk them later.
    js::GCMarker *gcmarker = NULL;
    if (IS_GC_MARKING_TRACER(trc)) {
        gcmarker = static_cast<js::GCMarker *>(trc);
        JS_ASSERT(gcmarker->getMarkColor() == XPC_GC_COLOR_BLACK);
        gcmarker->setMarkColor(XPC_GC_COLOR_GRAY);
    }
    self->TraceXPConnectRoots(trc);
    if (gcmarker)
        gcmarker->setMarkColor(XPC_GC_COLOR_BLACK);
}

static void
TraceJSObject(PRUint32 aLangID, void *aScriptThing, void *aClosure)
{
    if(aLangID == nsIProgrammingLanguage::JAVASCRIPT)
    {
        JS_CALL_TRACER(static_cast<JSTracer*>(aClosure), aScriptThing,
                       js_GetGCThingTraceKind(aScriptThing),
                       "JSObjectHolder");
    }
}

static JSDHashOperator
TraceJSHolder(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 number,
              void *arg)
{
    ObjectHolder* entry = reinterpret_cast<ObjectHolder*>(hdr);

    entry->tracer->Trace(entry->holder, TraceJSObject, arg);

    return JS_DHASH_NEXT;
}

struct ClearedGlobalObject : public JSDHashEntryHdr
{
    JSContext* mContext;
    JSObject* mGlobalObject;
};

void XPCJSRuntime::TraceXPConnectRoots(JSTracer *trc)
{
    JSContext *iter = nsnull, *acx;
    while ((acx = JS_ContextIterator(GetJSRuntime(), &iter))) {
        JS_ASSERT(JS_HAS_OPTION(acx, JSOPTION_UNROOTED_GLOBAL));
        if (acx->globalObject)
            JS_CALL_OBJECT_TRACER(trc, acx->globalObject, "global object");
    }

    XPCWrappedNativeScope::TraceJS(trc, this);

    for(XPCRootSetElem *e = mVariantRoots; e ; e = e->GetNextRoot())
        static_cast<XPCTraceableVariant*>(e)->TraceJS(trc);

    for(XPCRootSetElem *e = mWrappedJSRoots; e ; e = e->GetNextRoot())
        static_cast<nsXPCWrappedJS*>(e)->TraceJS(trc);

    if(mJSHolders.ops)
        JS_DHashTableEnumerate(&mJSHolders, TraceJSHolder, trc);
}

static JSDHashOperator
NoteJSHolder(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 number,
             void *arg)
{
    ObjectHolder* entry = reinterpret_cast<ObjectHolder*>(hdr);

    nsCycleCollectionTraversalCallback* cb =
        static_cast<nsCycleCollectionTraversalCallback*>(arg);
    cb->NoteRoot(nsIProgrammingLanguage::CPLUSPLUS, entry->holder,
                 entry->tracer);

    return JS_DHASH_NEXT;
}


void XPCJSRuntime::AddXPConnectRoots(JSContext* cx,
                                     nsCycleCollectionTraversalCallback &cb)
{
    // For all JS objects that are held by native objects but aren't held
    // through rooting or locking, we need to add all the native objects that
    // hold them so that the JS objects are colored correctly in the cycle
    // collector. This includes JSContexts that don't have outstanding requests,
    // because their global object wasn't marked by the JS GC. All other JS
    // roots were marked by the JS GC and will be colored correctly in the cycle
    // collector.

    JSContext *iter = nsnull, *acx;
    while((acx = JS_ContextIterator(GetJSRuntime(), &iter)))
    {
        // Only skip JSContexts with outstanding requests if the
        // callback does not want all traces (a debug feature).
        // Otherwise, we do want to know about all JSContexts to get
        // better graphs and explanations.
        if(!cb.WantAllTraces() && nsXPConnect::GetXPConnect()->GetOutstandingRequests(acx))
            continue;
        cb.NoteRoot(nsIProgrammingLanguage::CPLUSPLUS, acx,
                    nsXPConnect::JSContextParticipant());
    }

    XPCWrappedNativeScope::SuspectAllWrappers(this, cx, cb);

    for(XPCRootSetElem *e = mVariantRoots; e ; e = e->GetNextRoot())
        cb.NoteXPCOMRoot(static_cast<XPCTraceableVariant*>(e));

    for(XPCRootSetElem *e = mWrappedJSRoots; e ; e = e->GetNextRoot())
    {
        nsIXPConnectWrappedJS *wrappedJS = static_cast<nsXPCWrappedJS*>(e);
        cb.NoteXPCOMRoot(wrappedJS);
    }

    if(mJSHolders.ops)
        JS_DHashTableEnumerate(&mJSHolders, NoteJSHolder, &cb);
}

void
XPCJSRuntime::ClearWeakRoots()
{
    JSContext *iter = nsnull, *acx;

    while((acx = JS_ContextIterator(GetJSRuntime(), &iter)))
    {
        if(XPCPerThreadData::IsMainThread(acx) &&
           !nsXPConnect::GetXPConnect()->GetOutstandingRequests(acx))
        {
            JS_ClearNewbornRoots(acx);
        }
    }
}

template<class T> static void
DoDeferredRelease(nsTArray<T> &array)
{
    while(1)
    {
        PRUint32 count = array.Length();
        if(!count)
        {
            array.Compact();
            break;
        }
        T wrapper = array[count-1];
        array.RemoveElementAt(count-1);
        NS_RELEASE(wrapper);
    }
}

// static
JSBool XPCJSRuntime::GCCallback(JSContext *cx, JSGCStatus status)
{
    XPCJSRuntime* self = nsXPConnect::GetRuntimeInstance();
    if(self)
    {
        switch(status)
        {
            case JSGC_BEGIN:
            {
                if(!NS_IsMainThread())
                {
                    return JS_FALSE;
                }

                // We seem to sometime lose the unrooted global flag. Restore it
                // here. FIXME: bug 584495.
                JSContext *iter = nsnull, *acx;

                while((acx = JS_ContextIterator(cx->runtime, &iter))) {
                    if (!JS_HAS_OPTION(acx, JSOPTION_UNROOTED_GLOBAL))
                        JS_ToggleOptions(acx, JSOPTION_UNROOTED_GLOBAL);
                }
                break;
            }
            case JSGC_MARK_END:
            {
                NS_ASSERTION(!self->mDoingFinalization, "bad state");
    
                // mThreadRunningGC indicates that GC is running
                { // scoped lock
                    XPCAutoLock lock(self->GetMapLock());
                    NS_ASSERTION(!self->mThreadRunningGC, "bad state");
                    self->mThreadRunningGC = PR_GetCurrentThread();
                }

                nsTArray<nsXPCWrappedJS*>* dyingWrappedJSArray =
                    &self->mWrappedJSToReleaseArray;

                {
                    JSDyingJSObjectData data = {cx, dyingWrappedJSArray};

                    // Add any wrappers whose JSObjects are to be finalized to
                    // this array. Note that we do not want to be changing the
                    // refcount of these wrappers.
                    // We add them to the array now and Release the array members
                    // later to avoid the posibility of doing any JS GCThing
                    // allocations during the gc cycle.
                    self->mWrappedJSMap->
                        Enumerate(WrappedJSDyingJSObjectFinder, &data);
                }

                // Find dying scopes...
                XPCWrappedNativeScope::FinishedMarkPhaseOfGC(cx, self);

                self->mDoingFinalization = JS_TRUE;
                break;
            }
            case JSGC_FINALIZE_END:
            {
                NS_ASSERTION(self->mDoingFinalization, "bad state");
                self->mDoingFinalization = JS_FALSE;

                // Release all the members whose JSObjects are now known
                // to be dead.
                DoDeferredRelease(self->mWrappedJSToReleaseArray);

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

                DOM_MarkInterfaces();

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

                self->mIID2NativeInterfaceMap->
                    Enumerate(NativeInterfaceSweeper, nsnull);

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


                // mThreadRunningGC indicates that GC is running.
                // Clear it and notify waiters.
                { // scoped lock
                    XPCAutoLock lock(self->GetMapLock());
                    NS_ASSERTION(self->mThreadRunningGC == PR_GetCurrentThread(), "bad state");
                    self->mThreadRunningGC = nsnull;
                    xpc_NotifyAll(self->GetMapLock());
                }

                break;
            }
            case JSGC_END:
            {
                // NOTE that this event happens outside of the gc lock in
                // the js engine. So this could be simultaneous with the
                // events above.

                // Do any deferred released of native objects.
#ifdef XPC_TRACK_DEFERRED_RELEASES
                printf("XPC - Begin deferred Release of %d nsISupports pointers\n",
                       self->mNativesToReleaseArray.Length());
#endif
                DoDeferredRelease(self->mNativesToReleaseArray);
#ifdef XPC_TRACK_DEFERRED_RELEASES
                printf("XPC - End deferred Releases\n");
#endif
                break;
            }
            default:
                break;
        }
    }

    nsTArray<JSGCCallback> callbacks(self->extraGCCallbacks);
    for (PRUint32 i = 0; i < callbacks.Length(); ++i) {
        if (!callbacks[i](cx, status))
            return JS_FALSE;
    }

    return JS_TRUE;
}

// Auto JS GC lock helper.
class AutoLockJSGC
{
public:
    AutoLockJSGC(JSRuntime* rt) : mJSRuntime(rt) { JS_LOCK_GC(mJSRuntime); }
    ~AutoLockJSGC() { JS_UNLOCK_GC(mJSRuntime); }
private:
    JSRuntime* mJSRuntime;

    // Disable copy or assignment semantics.
    AutoLockJSGC(const AutoLockJSGC&);
    void operator=(const AutoLockJSGC&);
};

//static
void
XPCJSRuntime::WatchdogMain(void *arg)
{
    XPCJSRuntime* self = static_cast<XPCJSRuntime*>(arg);

    // Lock lasts until we return
    AutoLockJSGC lock(self->mJSRuntime);

    PRIntervalTime sleepInterval;
    while (self->mWatchdogThread)
    {
        // Sleep only 1 second if recently (or currently) active; otherwise, hibernate
        if (self->mLastActiveTime == -1 || PR_Now() - self->mLastActiveTime <= 2*PR_USEC_PER_SEC)
            sleepInterval = PR_TicksPerSecond();
        else
        {
            sleepInterval = PR_INTERVAL_NO_TIMEOUT;
            self->mWatchdogHibernating = PR_TRUE;
        }
#ifdef DEBUG
        PRStatus status =
#endif
            PR_WaitCondVar(self->mWatchdogWakeup, sleepInterval);
        JS_ASSERT(status == PR_SUCCESS);
        JSContext* cx = nsnull;
        while((cx = js_NextActiveContext(self->mJSRuntime, cx)))
        {
            js::TriggerOperationCallback(cx);
        }
    }

    /* Wake up the main thread waiting for the watchdog to terminate. */
    PR_NotifyCondVar(self->mWatchdogWakeup);
}

//static
void
XPCJSRuntime::ActivityCallback(void *arg, PRBool active)
{
    XPCJSRuntime* self = static_cast<XPCJSRuntime*>(arg);
    if (active) {
        self->mLastActiveTime = -1;
        if (self->mWatchdogHibernating)
        {
            self->mWatchdogHibernating = PR_FALSE;
            PR_NotifyCondVar(self->mWatchdogWakeup);
        }
    } else {
        self->mLastActiveTime = PR_Now();
    }
}


/***************************************************************************/

#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
static JSDHashOperator
DEBUG_WrapperChecker(JSDHashTable *table, JSDHashEntryHdr *hdr,
                     uint32 number, void *arg)
{
    XPCWrappedNative* wrapper = (XPCWrappedNative*)((JSDHashEntryStub*)hdr)->key;
    NS_ASSERTION(!wrapper->IsValid(), "found a 'valid' wrapper!");
    ++ *((int*)arg);
    return JS_DHASH_NEXT;
}
#endif

static JSDHashOperator
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

static JSDHashOperator
DetachedWrappedNativeProtoShutdownMarker(JSDHashTable *table, JSDHashEntryHdr *hdr,
                                         uint32 number, void *arg)
{
    XPCWrappedNativeProto* proto = 
        (XPCWrappedNativeProto*)((JSDHashEntryStub*)hdr)->key;

    proto->SystemIsBeingShutDown((JSContext*)arg);
    return JS_DHASH_NEXT;
}

void XPCJSRuntime::SystemIsBeingShutDown(JSContext* cx)
{
    DOM_ClearInterfaces();

    if(mDetachedWrappedNativeProtoMap)
        mDetachedWrappedNativeProtoMap->
            Enumerate(DetachedWrappedNativeProtoShutdownMarker, cx);
}

XPCJSRuntime::~XPCJSRuntime()
{
    if (mWatchdogWakeup)
    {
        // If the watchdog thread is running, tell it to terminate waking it
        // up if necessary and wait until it signals that it finished. As we
        // must release the lock before calling PR_DestroyCondVar, we use an
        // extra block here.
        {
            AutoLockJSGC lock(mJSRuntime);
            if (mWatchdogThread) {
                mWatchdogThread = nsnull;
                PR_NotifyCondVar(mWatchdogWakeup);
                PR_WaitCondVar(mWatchdogWakeup, PR_INTERVAL_NO_TIMEOUT);
            }
        }
        PR_DestroyCondVar(mWatchdogWakeup);
        mWatchdogWakeup = nsnull;
    }

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

    if(mExplicitNativeWrapperMap)
    {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mExplicitNativeWrapperMap->Count();
        if(count)
            printf("deleting XPCJSRuntime with %d live explicit XPCNativeWrapper\n", (int)count);
#endif
        delete mExplicitNativeWrapperMap;
    }

    // unwire the readable/JSString sharing magic
    XPCStringConvert::ShutdownDOMStringFinalizer();

    XPCConvert::RemoveXPCOMUCStringFinalizer();

    if(mJSHolders.ops)
    {
        JS_DHashTableFinish(&mJSHolders);
        mJSHolders.ops = nsnull;
    }

    if(mJSRuntime)
    {
        JS_DestroyRuntime(mJSRuntime);
        JS_ShutDown();
#ifdef DEBUG_shaver_off
        fprintf(stderr, "nJRSI: destroyed runtime %p\n", (void *)mJSRuntime);
#endif
    }

    XPCPerThreadData::ShutDown();
}

class XPConnectGCChunkAllocator
    : public js::GCChunkAllocator
{
public:
    XPConnectGCChunkAllocator() {}

    static PRInt64 GetGCChunkBytesInUse(void *data) {
        XPConnectGCChunkAllocator *allocator =
            static_cast<XPConnectGCChunkAllocator*>(data);
        return allocator->mNumGCChunksInUse * js::GC_CHUNK_SIZE;
    }
private:
    virtual void *doAlloc() {
        void *chunk = 0;
#ifdef MOZ_MEMORY
        posix_memalign(&chunk, js::GC_CHUNK_SIZE, js::GC_CHUNK_SIZE);
#else
        chunk = js::AllocGCChunk();
#endif
        if (chunk)
            mNumGCChunksInUse++;
        return chunk;
    }

    virtual void doFree(void *chunk) {
        mNumGCChunksInUse--;
#ifdef MOZ_MEMORY
        free(chunk);
#else
        js::FreeGCChunk(chunk);
#endif
    }

protected:
    PRUint32 mNumGCChunksInUse;
};

static XPConnectGCChunkAllocator gXPCJSChunkAllocator;

NS_MEMORY_REPORTER_IMPLEMENT(XPConnectJSRuntimeGCChunks,
                             "xpconnect/js/gcchunks",
                             "Memory in use by main JS Runtime GC chunks",
                             XPConnectGCChunkAllocator::GetGCChunkBytesInUse,
                             &gXPCJSChunkAllocator)

XPCJSRuntime::XPCJSRuntime(nsXPConnect* aXPConnect)
 : mXPConnect(aXPConnect),
   mJSRuntime(nsnull),
   mWrappedJSMap(JSObject2WrappedJSMap::newMap(XPC_JS_MAP_SIZE)),
   mWrappedJSClassMap(IID2WrappedJSClassMap::newMap(XPC_JS_CLASS_MAP_SIZE)),
   mIID2NativeInterfaceMap(IID2NativeInterfaceMap::newMap(XPC_NATIVE_INTERFACE_MAP_SIZE)),
   mClassInfo2NativeSetMap(ClassInfo2NativeSetMap::newMap(XPC_NATIVE_SET_MAP_SIZE)),
   mNativeSetMap(NativeSetMap::newMap(XPC_NATIVE_SET_MAP_SIZE)),
   mThisTranslatorMap(IID2ThisTranslatorMap::newMap(XPC_THIS_TRANSLATOR_MAP_SIZE)),
   mNativeScriptableSharedMap(XPCNativeScriptableSharedMap::newMap(XPC_NATIVE_JSCLASS_MAP_SIZE)),
   mDyingWrappedNativeProtoMap(XPCWrappedNativeProtoMap::newMap(XPC_DYING_NATIVE_PROTO_MAP_SIZE)),
   mDetachedWrappedNativeProtoMap(XPCWrappedNativeProtoMap::newMap(XPC_DETACHED_NATIVE_PROTO_MAP_SIZE)),
   mExplicitNativeWrapperMap(XPCNativeWrapperMap::newMap(XPC_NATIVE_WRAPPER_MAP_SIZE)),
   mMapLock(XPCAutoLock::NewLock("XPCJSRuntime::mMapLock")),
   mThreadRunningGC(nsnull),
   mWrappedJSToReleaseArray(),
   mNativesToReleaseArray(),
   mDoingFinalization(JS_FALSE),
   mVariantRoots(nsnull),
   mWrappedJSRoots(nsnull),
   mObjectHolderRoots(nsnull),
   mWatchdogWakeup(nsnull),
   mWatchdogThread(nsnull),
   mWatchdogHibernating(PR_FALSE),
   mLastActiveTime(-1)
{
#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
    DEBUG_WrappedNativeHashtable =
        JS_NewDHashTable(JS_DHashGetStubOps(), nsnull,
                         sizeof(JSDHashEntryStub), 128);
#endif
    NS_TIME_FUNCTION;

    DOM_InitInterfaces();

    // these jsids filled in later when we have a JSContext to work with.
    mStrIDs[0] = JSID_VOID;

    mJSRuntime = JS_NewRuntime(32L * 1024L * 1024L); // pref ?
    if(mJSRuntime)
    {
        // Unconstrain the runtime's threshold on nominal heap size, to avoid
        // triggering GC too often if operating continuously near an arbitrary
        // finite threshold (0xffffffff is infinity for uint32 parameters).
        // This leaves the maximum-JS_malloc-bytes threshold still in effect
        // to cause period, and we hope hygienic, last-ditch GCs from within
        // the GC's allocator.
        JS_SetGCParameter(mJSRuntime, JSGC_MAX_BYTES, 0xffffffff);
        JS_SetContextCallback(mJSRuntime, ContextCallback);
        JS_SetCompartmentCallback(mJSRuntime, CompartmentCallback);
        JS_SetGCCallbackRT(mJSRuntime, GCCallback);
        JS_SetExtraGCRoots(mJSRuntime, TraceJS, this);
        mWatchdogWakeup = JS_NEW_CONDVAR(mJSRuntime->gcLock);

        mJSRuntime->setActivityCallback(ActivityCallback, this);

        mJSRuntime->setCustomGCChunkAllocator(&gXPCJSChunkAllocator);

        NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(XPConnectJSRuntimeGCChunks));
    }

    if(!JS_DHashTableInit(&mJSHolders, JS_DHashGetStubOps(), nsnull,
                          sizeof(ObjectHolder), 512))
        mJSHolders.ops = nsnull;

    mCompartmentMap.Init();

    // Install a JavaScript 'debugger' keyword handler in debug builds only
#ifdef DEBUG
    if(mJSRuntime && !JS_GetGlobalDebugHooks(mJSRuntime)->debuggerHandler)
        xpc_InstallJSDebuggerKeywordHandler(mJSRuntime);
#endif

    if (mWatchdogWakeup) {
        AutoLockJSGC lock(mJSRuntime);

        mWatchdogThread = PR_CreateThread(PR_USER_THREAD, WatchdogMain, this,
                                          PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
                                          PR_UNJOINABLE_THREAD, 0);
    }
}

// static
XPCJSRuntime*
XPCJSRuntime::newXPCJSRuntime(nsXPConnect* aXPConnect)
{
    NS_PRECONDITION(aXPConnect,"bad param");

    XPCJSRuntime* self = new XPCJSRuntime(aXPConnect);

    if(self                                  &&
       self->GetJSRuntime()                  &&
       self->GetWrappedJSMap()               &&
       self->GetWrappedJSClassMap()          &&
       self->GetIID2NativeInterfaceMap()     &&
       self->GetClassInfo2NativeSetMap()     &&
       self->GetNativeSetMap()               &&
       self->GetThisTranslatorMap()          &&
       self->GetNativeScriptableSharedMap()  &&
       self->GetDyingWrappedNativeProtoMap() &&
       self->GetExplicitNativeWrapperMap()   &&
       self->GetMapLock()                    &&
       self->mWatchdogThread)
    {
        return self;
    }
    delete self;
    return nsnull;
}

JSBool
XPCJSRuntime::OnJSContextNew(JSContext *cx)
{
    NS_TIME_FUNCTION;

    // if it is our first context then we need to generate our string ids
    JSBool ok = JS_TRUE;
    if(JSID_IS_VOID(mStrIDs[0]))
    {
        JS_SetGCParameterForThread(cx, JSGC_MAX_CODE_CACHE_BYTES, 16 * 1024 * 1024);
        JSAutoRequest ar(cx);
        for(uintN i = 0; i < IDX_TOTAL_COUNT; i++)
        {
            JSString* str = JS_InternString(cx, mStrings[i]);
            if(!str || !JS_ValueToId(cx, STRING_TO_JSVAL(str), &mStrIDs[i]))
            {
                mStrIDs[0] = JSID_VOID;
                ok = JS_FALSE;
                break;
            }
            mStrJSVals[i] = STRING_TO_JSVAL(str);
        }
    }
    if (!ok)
        return JS_FALSE;

    XPCPerThreadData* tls = XPCPerThreadData::GetData(cx);
    if(!tls)
        return JS_FALSE;

    XPCContext* xpc = new XPCContext(this, cx);
    if (!xpc)
        return JS_FALSE;

    JS_SetNativeStackQuota(cx, 128 * sizeof(size_t) * 1024);
    JS_SetScriptStackQuota(cx, 25 * sizeof(size_t) * 1024 * 1024);

    // we want to mark the global object ourselves since we use a different color
    JS_ToggleOptions(cx, JSOPTION_UNROOTED_GLOBAL);

    return JS_TRUE;
}

JSBool
XPCJSRuntime::DeferredRelease(nsISupports* obj)
{
    NS_ASSERTION(obj, "bad param");

    if(mNativesToReleaseArray.IsEmpty())
    {
        // This array sometimes has 1000's
        // of entries, and usually has 50-200 entries. Avoid lots
        // of incremental grows.  We compact it down when we're done.
        mNativesToReleaseArray.SetCapacity(256);
    }
    return mNativesToReleaseArray.AppendElement(obj) != nsnull;
}

/***************************************************************************/

#ifdef DEBUG
static JSDHashOperator
WrappedJSClassMapDumpEnumerator(JSDHashTable *table, JSDHashEntryHdr *hdr,
                                uint32 number, void *arg)
{
    ((IID2WrappedJSClassMap::Entry*)hdr)->value->DebugDump(*(PRInt16*)arg);
    return JS_DHASH_NEXT;
}
static JSDHashOperator
WrappedJSMapDumpEnumerator(JSDHashTable *table, JSDHashEntryHdr *hdr,
                           uint32 number, void *arg)
{
    ((JSObject2WrappedJSMap::Entry*)hdr)->value->DebugDump(*(PRInt16*)arg);
    return JS_DHASH_NEXT;
}
static JSDHashOperator
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

        XPC_LOG_ALWAYS(("mWrappedJSToReleaseArray @ %x with %d wrappers(s)", \
                         &mWrappedJSToReleaseArray,
                         mWrappedJSToReleaseArray.Length()));

        int cxCount = 0;
        JSContext* iter = nsnull;
        while(JS_ContextIterator(mJSRuntime, &iter))
            ++cxCount;
        XPC_LOG_ALWAYS(("%d JS context(s)", cxCount));

        iter = nsnull;
        while(JS_ContextIterator(mJSRuntime, &iter))
        {
            XPCContext *xpc = XPCContext::GetXPCContext(iter);
            XPC_LOG_INDENT();
            xpc->DebugDump(depth);
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

/***************************************************************************/

void
XPCRootSetElem::AddToRootSet(JSRuntime* rt, XPCRootSetElem** listHead)
{
    NS_ASSERTION(!mSelfp, "Must be not linked");

    AutoLockJSGC lock(rt);
    mSelfp = listHead;
    mNext = *listHead;
    if(mNext)
    {
        NS_ASSERTION(mNext->mSelfp == listHead, "Must be list start");
        mNext->mSelfp = &mNext;
    }
    *listHead = this;
}

void
XPCRootSetElem::RemoveFromRootSet(JSRuntime* rt)
{
    NS_ASSERTION(mSelfp, "Must be linked");

    AutoLockJSGC lock(rt);
    NS_ASSERTION(*mSelfp == this, "Link invariant");
    *mSelfp = mNext;
    if(mNext)
        mNext->mSelfp = mSelfp;
#ifdef DEBUG
    mSelfp = nsnull;
    mNext = nsnull;
#endif
}

void
XPCJSRuntime::AddGCCallback(JSGCCallback cb)
{
    NS_ASSERTION(cb, "null callback");
    extraGCCallbacks.AppendElement(cb);
}

void
XPCJSRuntime::RemoveGCCallback(JSGCCallback cb)
{
    NS_ASSERTION(cb, "null callback");
    PRBool found = extraGCCallbacks.RemoveElement(cb);
    if (!found) {
        NS_ERROR("Removing a callback which was never added.");
    }
}
