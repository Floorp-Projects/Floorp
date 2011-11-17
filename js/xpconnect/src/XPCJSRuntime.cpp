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
 *   Nicholas Nethercote <nnethercote@mozilla.com>
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

#include "mozilla/Util.h"

#include "xpcprivate.h"
#include "xpcpublic.h"
#include "WrapperFactory.h"
#include "dom_quickstubs.h"

#include "jsgcchunk.h"
#include "jsscope.h"
#include "nsIMemoryReporter.h"
#include "nsPrintfCString.h"
#include "mozilla/FunctionTimer.h"
#include "prsystem.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

using namespace mozilla;
using namespace mozilla::xpconnect::memory;

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
    "__exposedProps__",     // IDX_EXPOSEDPROPS
    "__scriptOnly__",       // IDX_SCRIPTONLY
    "baseURIObject",        // IDX_BASEURIOBJECT
    "nodePrincipal",        // IDX_NODEPRINCIPAL
    "documentURIObject"     // IDX_DOCUMENTURIOBJECT
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
    while (wrapper) {
        if (wrapper->IsSubjectToFinalization()) {
            js::AutoSwitchCompartment sc(data->cx,
                                         wrapper->GetJSObjectPreserveColor());
            if (JS_IsAboutToBeFinalized(data->cx,
                                        wrapper->GetJSObjectPreserveColor()))
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
    if (iface->IsMarked()) {
        iface->Unmark();
        return JS_DHASH_NEXT;
    }

#ifdef XPC_REPORT_NATIVE_INTERFACE_AND_SET_FLUSHING
    fputs("- Destroying XPCNativeInterface for ", stdout);
    JS_PutString(JSVAL_TO_STRING(iface->GetName()), stdout);
    putc('\n', stdout);
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
    if (set->IsMarked())
        return JS_DHASH_NEXT;
    return JS_DHASH_REMOVE;
}

static JSDHashOperator
NativeSetSweeper(JSDHashTable *table, JSDHashEntryHdr *hdr,
                 uint32 number, void *arg)
{
    XPCNativeSet* set = ((NativeSetMap::Entry*)hdr)->key_value;
    if (set->IsMarked()) {
        set->Unmark();
        return JS_DHASH_NEXT;
    }

#ifdef XPC_REPORT_NATIVE_INTERFACE_AND_SET_FLUSHING
    printf("- Destroying XPCNativeSet for:\n");
    PRUint16 count = set->GetInterfaceCount();
    for (PRUint16 k = 0; k < count; k++) {
        XPCNativeInterface* iface = set->GetInterfaceAt(k);
        fputs("    ", stdout);
        JS_PutString(JSVAL_TO_STRING(iface->GetName()), stdout);
        putc('\n', stdout);
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
    if (shared->IsMarked()) {
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
    if (self) {
        if (operation == JSCONTEXT_NEW) {
            if (!self->OnJSContextNew(cx))
                return JS_FALSE;
        } else if (operation == JSCONTEXT_DESTROY) {
            delete XPCContext::GetXPCContext(cx);
        }
    }
    return JS_TRUE;
}

xpc::CompartmentPrivate::~CompartmentPrivate()
{
    MOZ_COUNT_DTOR(xpc::CompartmentPrivate);
}

static JSBool
CompartmentCallback(JSContext *cx, JSCompartment *compartment, uintN op)
{
    JS_ASSERT(op == JSCOMPARTMENT_DESTROY);

    XPCJSRuntime* self = nsXPConnect::GetRuntimeInstance();
    if (!self)
        return JS_TRUE;

    nsAutoPtr<xpc::CompartmentPrivate> priv(static_cast<xpc::CompartmentPrivate*>(JS_SetCompartmentPrivate(cx, compartment, nsnull)));
    if (!priv)
        return JS_TRUE;

    if (xpc::PtrAndPrincipalHashKey *key = priv->key) {
        XPCCompartmentMap &map = self->GetCompartmentMap();
#ifdef DEBUG
        {
            JSCompartment *current = NULL;  // init to shut GCC up
            NS_ASSERTION(map.Get(key, &current), "no compartment?");
            NS_ASSERTION(current == compartment, "compartment mismatch");
        }
#endif
        map.Remove(key);
    } else {
        nsISupports *ptr = priv->ptr;
        XPCMTCompartmentMap &map = self->GetMTCompartmentMap();
#ifdef DEBUG
        {
            JSCompartment *current;
            NS_ASSERTION(map.Get(ptr, &current), "no compartment?");
            NS_ASSERTION(current == compartment, "compartment mismatch");
        }
#endif
        map.Remove(ptr);
    }

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
    if (!mJSHolders.ops)
        return NS_ERROR_OUT_OF_MEMORY;

    ObjectHolder *entry =
        reinterpret_cast<ObjectHolder*>(JS_DHashTableOperate(&mJSHolders,
                                                             aHolder,
                                                             JS_DHASH_ADD));
    if (!entry)
        return NS_ERROR_OUT_OF_MEMORY;

    entry->holder = aHolder;
    entry->tracer = aTracer;

    return NS_OK;
}

nsresult
XPCJSRuntime::RemoveJSHolder(void* aHolder)
{
    if (!mJSHolders.ops)
        return NS_ERROR_OUT_OF_MEMORY;

    JS_DHashTableOperate(&mJSHolders, aHolder, JS_DHASH_REMOVE);

    return NS_OK;
}

// static
void XPCJSRuntime::TraceBlackJS(JSTracer* trc, void* data)
{
    XPCJSRuntime* self = (XPCJSRuntime*)data;

    // Skip this part if XPConnect is shutting down. We get into
    // bad locking problems with the thread iteration otherwise.
    if (!self->GetXPConnect()->IsShuttingDown()) {
        Mutex* threadLock = XPCPerThreadData::GetLock();
        if (threadLock)
        { // scoped lock
            MutexAutoLock lock(*threadLock);

            XPCPerThreadData* iterp = nsnull;
            XPCPerThreadData* thread;

            while (nsnull != (thread =
                              XPCPerThreadData::IterateThreads(&iterp))) {
                // Trace those AutoMarkingPtr lists!
                thread->TraceJS(trc);
            }
        }
    }

    {
        XPCAutoLock lock(self->mMapLock);

        // XPCJSObjectHolders don't participate in cycle collection, so always
        // trace them here.
        XPCRootSetElem *e;
        for (e = self->mObjectHolderRoots; e; e = e->GetNextRoot())
            static_cast<XPCJSObjectHolder*>(e)->TraceJS(trc);
    }
}

// static
void XPCJSRuntime::TraceGrayJS(JSTracer* trc, void* data)
{
    XPCJSRuntime* self = (XPCJSRuntime*)data;

    // Mark these roots as gray so the CC can walk them later.
    self->TraceXPConnectRoots(trc);
}

static void
TraceJSObject(PRUint32 aLangID, void *aScriptThing, const char *name,
              void *aClosure)
{
    if (aLangID == nsIProgrammingLanguage::JAVASCRIPT) {
        JS_CALL_TRACER(static_cast<JSTracer*>(aClosure), aScriptThing,
                       js_GetGCThingTraceKind(aScriptThing), name);
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

static PLDHashOperator
TraceExpandos(XPCWrappedNative *wn, JSObject *&expando, void *aClosure)
{
    if (wn->IsWrapperExpired())
        return PL_DHASH_REMOVE;
    JS_CALL_OBJECT_TRACER(static_cast<JSTracer *>(aClosure), expando, "expando object");
    return PL_DHASH_NEXT;
}

static PLDHashOperator
TraceDOMExpandos(nsPtrHashKey<JSObject> *expando, void *aClosure)
{
    JS_CALL_OBJECT_TRACER(static_cast<JSTracer *>(aClosure), expando->GetKey(),
                          "DOM expando object");
    return PL_DHASH_NEXT;
}

static PLDHashOperator
TraceCompartment(xpc::PtrAndPrincipalHashKey *aKey, JSCompartment *compartment, void *aClosure)
{
    xpc::CompartmentPrivate *priv = (xpc::CompartmentPrivate *)
        JS_GetCompartmentPrivate(static_cast<JSTracer *>(aClosure)->context, compartment);
    if (priv->expandoMap)
        priv->expandoMap->Enumerate(TraceExpandos, aClosure);
    if (priv->domExpandoMap)
        priv->domExpandoMap->EnumerateEntries(TraceDOMExpandos, aClosure);
    return PL_DHASH_NEXT;
}

void XPCJSRuntime::TraceXPConnectRoots(JSTracer *trc)
{
    JSContext *iter = nsnull, *acx;
    while ((acx = JS_ContextIterator(GetJSRuntime(), &iter))) {
        JS_ASSERT(acx->hasRunOption(JSOPTION_UNROOTED_GLOBAL));
        if (acx->globalObject)
            JS_CALL_OBJECT_TRACER(trc, acx->globalObject, "XPC global object");
    }

    XPCAutoLock lock(mMapLock);

    XPCWrappedNativeScope::TraceJS(trc, this);

    for (XPCRootSetElem *e = mVariantRoots; e ; e = e->GetNextRoot())
        static_cast<XPCTraceableVariant*>(e)->TraceJS(trc);

    for (XPCRootSetElem *e = mWrappedJSRoots; e ; e = e->GetNextRoot())
        static_cast<nsXPCWrappedJS*>(e)->TraceJS(trc);

    if (mJSHolders.ops)
        JS_DHashTableEnumerate(&mJSHolders, TraceJSHolder, trc);

    // Trace compartments.
    GetCompartmentMap().EnumerateRead(TraceCompartment, trc);
}

struct Closure
{
    JSContext *cx;
    bool cycleCollectionEnabled;
    nsCycleCollectionTraversalCallback *cb;
};

static void
CheckParticipatesInCycleCollection(PRUint32 aLangID, void *aThing,
                                   const char *name, void *aClosure)
{
    Closure *closure = static_cast<Closure*>(aClosure);

    closure->cycleCollectionEnabled =
        aLangID == nsIProgrammingLanguage::JAVASCRIPT &&
        AddToCCKind(js_GetGCThingTraceKind(aThing)) &&
        xpc::ParticipatesInCycleCollection(closure->cx,
                                           static_cast<js::gc::Cell*>(aThing));
}

static JSDHashOperator
NoteJSHolder(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 number,
             void *arg)
{
    ObjectHolder* entry = reinterpret_cast<ObjectHolder*>(hdr);
    Closure *closure = static_cast<Closure*>(arg);

    entry->tracer->Trace(entry->holder, CheckParticipatesInCycleCollection,
                         closure);
    if (!closure->cycleCollectionEnabled)
        return JS_DHASH_NEXT;

    closure->cb->NoteRoot(nsIProgrammingLanguage::CPLUSPLUS, entry->holder,
                          entry->tracer);

    return JS_DHASH_NEXT;
}

// static
void
XPCJSRuntime::SuspectWrappedNative(JSContext *cx, XPCWrappedNative *wrapper,
                                   nsCycleCollectionTraversalCallback &cb)
{
    if (!wrapper->IsValid() || wrapper->IsWrapperExpired())
        return;

    NS_ASSERTION(NS_IsMainThread() || NS_IsCycleCollectorThread(),
                 "Suspecting wrapped natives from non-CC thread");

    // Only suspect wrappedJSObjects that are in a compartment that
    // participates in cycle collection.
    JSObject* obj = wrapper->GetFlatJSObjectPreserveColor();
    if (!xpc::ParticipatesInCycleCollection(cx, obj))
        return;

    // Only record objects that might be part of a cycle as roots, unless
    // the callback wants all traces (a debug feature).
    if (xpc_IsGrayGCThing(obj) || cb.WantAllTraces())
        cb.NoteRoot(nsIProgrammingLanguage::JAVASCRIPT, obj,
                    nsXPConnect::GetXPConnect());
}

static PLDHashOperator
SuspectExpandos(XPCWrappedNative *wrapper, JSObject *expando, void *arg)
{
    Closure* closure = static_cast<Closure*>(arg);
    XPCJSRuntime::SuspectWrappedNative(closure->cx, wrapper, *closure->cb);

    return PL_DHASH_NEXT;
}

static PLDHashOperator
SuspectDOMExpandos(nsPtrHashKey<JSObject> *expando, void *arg)
{
    Closure *closure = static_cast<Closure*>(arg);
    closure->cb->NoteXPCOMRoot(static_cast<nsISupports*>(expando->GetKey()->getPrivate()));
    return PL_DHASH_NEXT;
}

static PLDHashOperator
SuspectCompartment(xpc::PtrAndPrincipalHashKey *key, JSCompartment *compartment, void *arg)
{
    Closure* closure = static_cast<Closure*>(arg);
    xpc::CompartmentPrivate *priv = (xpc::CompartmentPrivate *)
        JS_GetCompartmentPrivate(closure->cx, compartment);
    if (priv->expandoMap)
        priv->expandoMap->EnumerateRead(SuspectExpandos, arg);
    if (priv->domExpandoMap)
        priv->domExpandoMap->EnumerateEntries(SuspectDOMExpandos, arg);
    return PL_DHASH_NEXT;
}

void
XPCJSRuntime::AddXPConnectRoots(JSContext* cx,
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
    while ((acx = JS_ContextIterator(GetJSRuntime(), &iter))) {
        // Only skip JSContexts with outstanding requests if the
        // callback does not want all traces (a debug feature).
        // Otherwise, we do want to know about all JSContexts to get
        // better graphs and explanations.
        if (!cb.WantAllTraces() && nsXPConnect::GetXPConnect()->GetOutstandingRequests(acx))
            continue;
        cb.NoteRoot(nsIProgrammingLanguage::CPLUSPLUS, acx,
                    nsXPConnect::JSContextParticipant());
    }

    XPCAutoLock lock(mMapLock);

    XPCWrappedNativeScope::SuspectAllWrappers(this, cx, cb);

    for (XPCRootSetElem *e = mVariantRoots; e ; e = e->GetNextRoot())
        cb.NoteXPCOMRoot(static_cast<XPCTraceableVariant*>(e));

    for (XPCRootSetElem *e = mWrappedJSRoots; e ; e = e->GetNextRoot()) {
        nsXPCWrappedJS *wrappedJS = static_cast<nsXPCWrappedJS*>(e);
        JSObject *obj = wrappedJS->GetJSObjectPreserveColor();

        // Only suspect wrappedJSObjects that are in a compartment that
        // participates in cycle collection.
        if (!xpc::ParticipatesInCycleCollection(cx, obj))
            continue;

        cb.NoteXPCOMRoot(static_cast<nsIXPConnectWrappedJS *>(wrappedJS));
    }

    Closure closure = { cx, true, &cb };
    if (mJSHolders.ops) {
        JS_DHashTableEnumerate(&mJSHolders, NoteJSHolder, &closure);
    }

    // Suspect wrapped natives with expando objects.
    GetCompartmentMap().EnumerateRead(SuspectCompartment, &closure);
}

template<class T> static void
DoDeferredRelease(nsTArray<T> &array)
{
    while (1) {
        PRUint32 count = array.Length();
        if (!count) {
            array.Compact();
            break;
        }
        T wrapper = array[count-1];
        array.RemoveElementAt(count-1);
        NS_RELEASE(wrapper);
    }
}

static JSDHashOperator
SweepWaiverWrappers(JSDHashTable *table, JSDHashEntryHdr *hdr,
                    uint32 number, void *arg)
{
    JSContext *cx = (JSContext *)arg;
    JSObject *key = ((JSObject2JSObjectMap::Entry *)hdr)->key;
    JSObject *value = ((JSObject2JSObjectMap::Entry *)hdr)->value;

    if (JS_IsAboutToBeFinalized(cx, key) || JS_IsAboutToBeFinalized(cx, value))
        return JS_DHASH_REMOVE;
    return JS_DHASH_NEXT;
}

static PLDHashOperator
SweepExpandos(XPCWrappedNative *wn, JSObject *&expando, void *arg)
{
    JSContext *cx = (JSContext *)arg;
    return JS_IsAboutToBeFinalized(cx, wn->GetFlatJSObjectPreserveColor())
           ? PL_DHASH_REMOVE
           : PL_DHASH_NEXT;
}

static PLDHashOperator
SweepCompartment(nsCStringHashKey& aKey, JSCompartment *compartment, void *aClosure)
{
    xpc::CompartmentPrivate *priv = (xpc::CompartmentPrivate *)
        JS_GetCompartmentPrivate((JSContext *)aClosure, compartment);
    if (priv->waiverWrapperMap)
        priv->waiverWrapperMap->Enumerate(SweepWaiverWrappers, (JSContext *)aClosure);
    if (priv->expandoMap)
        priv->expandoMap->Enumerate(SweepExpandos, (JSContext *)aClosure);
    return PL_DHASH_NEXT;
}

// static
JSBool XPCJSRuntime::GCCallback(JSContext *cx, JSGCStatus status)
{
    XPCJSRuntime* self = nsXPConnect::GetRuntimeInstance();
    if (!self)
        return JS_TRUE;

    switch (status) {
        case JSGC_BEGIN:
        {
            if (!NS_IsMainThread()) {
                return JS_FALSE;
            }

            // We seem to sometime lose the unrooted global flag. Restore it
            // here. FIXME: bug 584495.
            JSContext *iter = nsnull, *acx;

            while ((acx = JS_ContextIterator(cx->runtime, &iter))) {
                if (!acx->hasRunOption(JSOPTION_UNROOTED_GLOBAL))
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

            // Find dying scopes.
            XPCWrappedNativeScope::FinishedMarkPhaseOfGC(cx, self);

            // Sweep compartments.
            self->GetCompartmentMap().EnumerateRead((XPCCompartmentMap::EnumReadFunction)
                                                    SweepCompartment, cx);

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
            if (!self->GetXPConnect()->IsShuttingDown()) {
                Mutex* threadLock = XPCPerThreadData::GetLock();
                if (threadLock)
                { // scoped lock
                    MutexAutoLock lock(*threadLock);

                    XPCPerThreadData* iterp = nsnull;
                    XPCPerThreadData* thread;

                    while (nsnull != (thread =
                                      XPCPerThreadData::IterateThreads(&iterp))) {
                        // Mark those AutoMarkingPtr lists!
                        thread->MarkAutoRootsAfterJSFinalize();

                        XPCCallContext* ccxp = thread->GetCallContext();
                        while (ccxp) {
                            // Deal with the strictness of callcontext that
                            // complains if you ask for a set when
                            // it is in a state where the set could not
                            // possibly be valid.
                            if (ccxp->CanGetSet()) {
                                XPCNativeSet* set = ccxp->GetSet();
                                if (set)
                                    set->Mark();
                            }
                            if (ccxp->CanGetInterface()) {
                                XPCNativeInterface* iface = ccxp->GetInterface();
                                if (iface)
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
            if (!self->GetXPConnect()->IsShuttingDown()) {
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
            if (!self->GetXPConnect()->IsShuttingDown()) {
                Mutex* threadLock = XPCPerThreadData::GetLock();
                if (threadLock) {
                    // Do the marking...

                    { // scoped lock
                        MutexAutoLock lock(*threadLock);

                        XPCPerThreadData* iterp = nsnull;
                        XPCPerThreadData* thread;

                        while (nsnull != (thread =
                                          XPCPerThreadData::IterateThreads(&iterp))) {
                            XPCCallContext* ccxp = thread->GetCallContext();
                            while (ccxp) {
                                // Deal with the strictness of callcontext that
                                // complains if you ask for a tearoff when
                                // it is in a state where the tearoff could not
                                // possibly be valid.
                                if (ccxp->CanGetTearOff()) {
                                    XPCWrappedNativeTearOff* to =
                                        ccxp->GetTearOff();
                                    if (to)
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

            // Do any deferred releases of native objects.
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
    while (self->mWatchdogThread) {
        // Sleep only 1 second if recently (or currently) active; otherwise, hibernate
        if (self->mLastActiveTime == -1 || PR_Now() - self->mLastActiveTime <= PRTime(2*PR_USEC_PER_SEC))
            sleepInterval = PR_TicksPerSecond();
        else {
            sleepInterval = PR_INTERVAL_NO_TIMEOUT;
            self->mWatchdogHibernating = true;
        }
#ifdef DEBUG
        PRStatus status =
#endif
            PR_WaitCondVar(self->mWatchdogWakeup, sleepInterval);
        JS_ASSERT(status == PR_SUCCESS);
        JSContext* cx = nsnull;
        while ((cx = js_NextActiveContext(self->mJSRuntime, cx))) {
            js::TriggerOperationCallback(cx);
        }
    }

    /* Wake up the main thread waiting for the watchdog to terminate. */
    PR_NotifyCondVar(self->mWatchdogWakeup);
}

//static
void
XPCJSRuntime::ActivityCallback(void *arg, JSBool active)
{
    XPCJSRuntime* self = static_cast<XPCJSRuntime*>(arg);
    if (active) {
        self->mLastActiveTime = -1;
        if (self->mWatchdogHibernating) {
            self->mWatchdogHibernating = false;
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

    if (mDetachedWrappedNativeProtoMap)
        mDetachedWrappedNativeProtoMap->
            Enumerate(DetachedWrappedNativeProtoShutdownMarker, cx);
}

JSContext *
XPCJSRuntime::GetJSCycleCollectionContext()
{
    if (!mJSCycleCollectionContext) {
        mJSCycleCollectionContext = JS_NewContext(mJSRuntime, 0);
        if (!mJSCycleCollectionContext)
            return nsnull;
        JS_ClearContextThread(mJSCycleCollectionContext);
    }
    return mJSCycleCollectionContext;
}

XPCJSRuntime::~XPCJSRuntime()
{
    if (mWatchdogWakeup) {
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

    if (mJSCycleCollectionContext) {
        JS_SetContextThread(mJSCycleCollectionContext);
        JS_DestroyContextNoGC(mJSCycleCollectionContext);
    }

#ifdef XPC_DUMP_AT_SHUTDOWN
    {
    // count the total JSContexts in use
    JSContext* iter = nsnull;
    int count = 0;
    while (JS_ContextIterator(mJSRuntime, &iter))
        count ++;
    if (count)
        printf("deleting XPCJSRuntime with %d live JSContexts\n", count);
    }
#endif

    // clean up and destroy maps...
    if (mWrappedJSMap) {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mWrappedJSMap->Count();
        if (count)
            printf("deleting XPCJSRuntime with %d live wrapped JSObject\n", (int)count);
#endif
        mWrappedJSMap->Enumerate(WrappedJSShutdownMarker, mJSRuntime);
        delete mWrappedJSMap;
    }

    if (mWrappedJSClassMap) {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mWrappedJSClassMap->Count();
        if (count)
            printf("deleting XPCJSRuntime with %d live nsXPCWrappedJSClass\n", (int)count);
#endif
        delete mWrappedJSClassMap;
    }

    if (mIID2NativeInterfaceMap) {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mIID2NativeInterfaceMap->Count();
        if (count)
            printf("deleting XPCJSRuntime with %d live XPCNativeInterfaces\n", (int)count);
#endif
        delete mIID2NativeInterfaceMap;
    }

    if (mClassInfo2NativeSetMap) {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mClassInfo2NativeSetMap->Count();
        if (count)
            printf("deleting XPCJSRuntime with %d live XPCNativeSets\n", (int)count);
#endif
        delete mClassInfo2NativeSetMap;
    }

    if (mNativeSetMap) {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mNativeSetMap->Count();
        if (count)
            printf("deleting XPCJSRuntime with %d live XPCNativeSets\n", (int)count);
#endif
        delete mNativeSetMap;
    }

    if (mMapLock)
        XPCAutoLock::DestroyLock(mMapLock);

    if (mThisTranslatorMap) {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mThisTranslatorMap->Count();
        if (count)
            printf("deleting XPCJSRuntime with %d live ThisTranslator\n", (int)count);
#endif
        delete mThisTranslatorMap;
    }

#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
    if (DEBUG_WrappedNativeHashtable) {
        int LiveWrapperCount = 0;
        JS_DHashTableEnumerate(DEBUG_WrappedNativeHashtable,
                               DEBUG_WrapperChecker, &LiveWrapperCount);
        if (LiveWrapperCount)
            printf("deleting XPCJSRuntime with %d live XPCWrappedNative (found in wrapper check)\n", (int)LiveWrapperCount);
        JS_DHashTableDestroy(DEBUG_WrappedNativeHashtable);
    }
#endif

    if (mNativeScriptableSharedMap) {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mNativeScriptableSharedMap->Count();
        if (count)
            printf("deleting XPCJSRuntime with %d live XPCNativeScriptableShared\n", (int)count);
#endif
        delete mNativeScriptableSharedMap;
    }

    if (mDyingWrappedNativeProtoMap) {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mDyingWrappedNativeProtoMap->Count();
        if (count)
            printf("deleting XPCJSRuntime with %d live but dying XPCWrappedNativeProto\n", (int)count);
#endif
        delete mDyingWrappedNativeProtoMap;
    }

    if (mDetachedWrappedNativeProtoMap) {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mDetachedWrappedNativeProtoMap->Count();
        if (count)
            printf("deleting XPCJSRuntime with %d live detached XPCWrappedNativeProto\n", (int)count);
#endif
        delete mDetachedWrappedNativeProtoMap;
    }

    if (mExplicitNativeWrapperMap) {
#ifdef XPC_DUMP_AT_SHUTDOWN
        uint32 count = mExplicitNativeWrapperMap->Count();
        if (count)
            printf("deleting XPCJSRuntime with %d live explicit XPCNativeWrapper\n", (int)count);
#endif
        delete mExplicitNativeWrapperMap;
    }

    // unwire the readable/JSString sharing magic
    XPCStringConvert::ShutdownDOMStringFinalizer();

    XPCConvert::RemoveXPCOMUCStringFinalizer();

    if (mJSHolders.ops) {
        JS_DHashTableFinish(&mJSHolders);
        mJSHolders.ops = nsnull;
    }

    if (mJSRuntime) {
        JS_DestroyRuntime(mJSRuntime);
        JS_ShutDown();
#ifdef DEBUG_shaver_off
        fprintf(stderr, "nJRSI: destroyed runtime %p\n", (void *)mJSRuntime);
#endif
    }

    XPCPerThreadData::ShutDown();
}

namespace {

#ifdef JS_TRACER

PRInt64
GetCompartmentTjitCodeSize(JSCompartment *c)
{
    if (c->hasTraceMonitor()) {
        size_t total, frag_size, free_size;
        c->traceMonitor()->getCodeAllocStats(total, frag_size, free_size);
        return total;
    }
    return 0;
}

PRInt64
GetCompartmentTjitDataAllocatorsMainSize(JSCompartment *c)
{
    return c->hasTraceMonitor()
         ? c->traceMonitor()->getVMAllocatorsMainSize(moz_malloc_usable_size)
         : 0;
}

PRInt64
GetCompartmentTjitDataAllocatorsReserveSize(JSCompartment *c)
{
    return c->hasTraceMonitor()
         ? c->traceMonitor()->getVMAllocatorsReserveSize(moz_malloc_usable_size)
         : 0;
}

PRInt64
GetCompartmentTjitDataTraceMonitorSize(JSCompartment *c)
{
    return c->hasTraceMonitor()
         ? c->traceMonitor()->getTraceMonitorSize(moz_malloc_usable_size)
         : 0;
}

#endif  // JS_TRACER

void
CompartmentCallback(JSContext *cx, void *vdata, JSCompartment *compartment)
{
    // Append a new CompartmentStats to the vector.
    IterateData *data = static_cast<IterateData *>(vdata);
    CompartmentStats compartmentStats(cx, compartment);
    CompartmentStats *curr =
        data->compartmentStatsVector.AppendElement(compartmentStats);
    data->currCompartmentStats = curr;

    // Get the compartment-level numbers.
#ifdef JS_METHODJIT
    size_t method, regexp, unused;
    compartment->getMjitCodeStats(method, regexp, unused);
    curr->mjitCodeMethod = method;
    curr->mjitCodeRegexp = regexp;
    curr->mjitCodeUnused = unused;
#endif
#ifdef JS_TRACER
    curr->tjitCode = GetCompartmentTjitCodeSize(compartment);
    curr->tjitDataAllocatorsMain = GetCompartmentTjitDataAllocatorsMainSize(compartment);
    curr->tjitDataAllocatorsReserve = GetCompartmentTjitDataAllocatorsReserveSize(compartment);
    curr->tjitDataNonAllocators = GetCompartmentTjitDataTraceMonitorSize(compartment);
#endif
    JS_GetTypeInferenceMemoryStats(cx, compartment, &curr->typeInferenceMemory,
                                   moz_malloc_usable_size);
}

void
ChunkCallback(JSContext *cx, void *vdata, js::gc::Chunk *chunk)
{
    IterateData *data = static_cast<IterateData *>(vdata);
    for (uint32 i = 0; i < js::gc::ArenasPerChunk; i++)
        if (chunk->decommittedArenas.get(i))
            data->gcHeapChunkDirtyDecommitted += js::gc::ArenaSize;
}

void
ArenaCallback(JSContext *cx, void *vdata, js::gc::Arena *arena,
              JSGCTraceKind traceKind, size_t thingSize)
{
    IterateData *data = static_cast<IterateData *>(vdata);

    data->currCompartmentStats->gcHeapArenaHeaders +=
        sizeof(js::gc::ArenaHeader);
    size_t allocationSpace = arena->thingsSpan(thingSize);
    data->currCompartmentStats->gcHeapArenaPadding +=
        js::gc::ArenaSize - allocationSpace - sizeof(js::gc::ArenaHeader);
    // We don't call the callback on unused things.  So we compute the
    // unused space like this:  arenaUnused = maxArenaUnused - arenaUsed.
    // We do this by setting arenaUnused to maxArenaUnused here, and then
    // subtracting thingSize for every used cell, in CellCallback().
    data->currCompartmentStats->gcHeapArenaUnused += allocationSpace;
}

void
CellCallback(JSContext *cx, void *vdata, void *thing, JSGCTraceKind traceKind,
             size_t thingSize)
{
    IterateData *data = static_cast<IterateData *>(vdata);
    CompartmentStats *curr = data->currCompartmentStats;
    switch (traceKind) {
        case JSTRACE_OBJECT:
        {
            JSObject *obj = static_cast<JSObject *>(thing);
            if (obj->isFunction()) {
                curr->gcHeapObjectsFunction += thingSize;
            } else {
                curr->gcHeapObjectsNonFunction += thingSize;
            }
            curr->objectSlots += obj->sizeOfSlotsArray(moz_malloc_usable_size);
            break;
        }
        case JSTRACE_STRING:
        {
            JSString *str = static_cast<JSString *>(thing);
            curr->gcHeapStrings += thingSize;
            curr->stringChars += str->charsHeapSize(moz_malloc_usable_size);
            break;
        }
        case JSTRACE_SHAPE:
        {
            js::Shape *shape = static_cast<js::Shape *>(thing);
            if (shape->inDictionary()) {
                curr->gcHeapShapesDict += thingSize;
                curr->shapesExtraDictTables += shape->sizeOfPropertyTable(moz_malloc_usable_size);
            } else {
                curr->gcHeapShapesTree += thingSize;
                curr->shapesExtraTreeTables += shape->sizeOfPropertyTable(moz_malloc_usable_size);
                curr->shapesExtraTreeShapeKids += shape->sizeOfKids(moz_malloc_usable_size);
            }
            break;
        }
        case JSTRACE_SCRIPT:
        {
            JSScript *script = static_cast<JSScript *>(thing);
            curr->gcHeapScripts += thingSize;
            curr->scriptData += script->dataSize(moz_malloc_usable_size);
#ifdef JS_METHODJIT
            curr->mjitData += script->jitDataSize(moz_malloc_usable_size);
#endif
            break;
        }
        case JSTRACE_TYPE_OBJECT:
        {
            js::types::TypeObject *obj = static_cast<js::types::TypeObject *>(thing);
            curr->gcHeapTypeObjects += thingSize;
            JS_GetTypeInferenceObjectStats(obj, &curr->typeInferenceMemory, moz_malloc_usable_size);
            break;
        }
        case JSTRACE_XML:
        {
            curr->gcHeapXML += thingSize;
            break;
        }
    }
    // Yes, this is a subtraction:  see ArenaCallback() for details.
    curr->gcHeapArenaUnused -= thingSize;
}

template <int N>
inline void
ReportMemory(const nsACString &path, PRInt32 kind, PRInt32 units,
             PRInt64 amount, const char (&desc)[N],
             nsIMemoryMultiReporterCallback *callback, nsISupports *closure)
{
    callback->Callback(NS_LITERAL_CSTRING(""), path, kind, units, amount,
                       NS_LITERAL_CSTRING(desc), closure);
}

template <int N>
inline void
ReportMemoryBytes(const nsACString &path, PRInt32 kind, PRInt64 amount,
                  const char (&desc)[N],
                  nsIMemoryMultiReporterCallback *callback,
                  nsISupports *closure)
{
    ReportMemory(path, kind, nsIMemoryReporter::UNITS_BYTES, amount, desc,
                 callback, closure);
}

template <int N>
inline void
ReportMemoryBytes0(const nsCString &path, PRInt32 kind, PRInt64 amount,
                   const char (&desc)[N],
                   nsIMemoryMultiReporterCallback *callback,
                   nsISupports *closure)
{
    if (amount)
        ReportMemoryBytes(path, kind, amount, desc, callback, closure);
}

template <int N>
inline void
ReportMemoryPercentage(const nsACString &path, PRInt32 kind, PRInt64 amount,
                       const char (&desc)[N],
                       nsIMemoryMultiReporterCallback *callback,
                       nsISupports *closure)
{
    ReportMemory(path, kind, nsIMemoryReporter::UNITS_PERCENTAGE, amount, desc,
                 callback, closure);
}

template <int N>
inline const nsCString
MakeMemoryReporterPath(const nsACString &pathPrefix,
                       const nsACString &compartmentName,
                       const char (&reporterName)[N])
{
  return pathPrefix + NS_LITERAL_CSTRING("compartment(") + compartmentName +
         NS_LITERAL_CSTRING(")/") + nsDependentCString(reporterName);
}

} // anonymous namespace

#define JS_GC_HEAP_KIND  nsIMemoryReporter::KIND_NONHEAP

// We have per-compartment GC heap totals, so we can't put the total GC heap
// size in the explicit allocations tree.  But it's a useful figure, so put it
// in the "others" list.

static PRInt64
GetGCChunkTotalBytes()
{
    JSRuntime *rt = nsXPConnect::GetRuntimeInstance()->GetJSRuntime();
    return PRInt64(JS_GetGCParameter(rt, JSGC_TOTAL_CHUNKS)) * js::gc::ChunkSize;
}

NS_MEMORY_REPORTER_IMPLEMENT(XPConnectJSGCHeap,
                             "js-gc-heap",
                             KIND_OTHER,
                             nsIMemoryReporter::UNITS_BYTES,
                             GetGCChunkTotalBytes,
                             "Memory used by the garbage-collected JavaScript heap.")

static PRInt64
GetJSSystemCompartmentCount()
{
    JSRuntime *rt = nsXPConnect::GetRuntimeInstance()->GetJSRuntime();
    size_t n = 0;
    for (size_t i = 0; i < rt->compartments.length(); i++) {
        if (rt->compartments[i]->isSystemCompartment) {
            n++;
        }
    }
    return n;
}

static PRInt64
GetJSUserCompartmentCount()
{
    JSRuntime *rt = nsXPConnect::GetRuntimeInstance()->GetJSRuntime();
    size_t n = 0;
    for (size_t i = 0; i < rt->compartments.length(); i++) {
        if (!rt->compartments[i]->isSystemCompartment) {
            n++;
        }
    }
    return n;
}

// Nb: js-system-compartment-count + js-user-compartment-count could be
// different to the number of compartments reported by
// XPConnectJSCompartmentsMultiReporter if a garbage collection occurred
// between them being consulted.  We could move these reporters into
// XPConnectJSCompartmentCount to avoid that problem, but then we couldn't
// easily report them via telemetry, so we live with the small risk of
// inconsistencies.
NS_MEMORY_REPORTER_IMPLEMENT(XPConnectJSSystemCompartmentCount,
                             "js-compartments-system",
                             KIND_OTHER,
                             nsIMemoryReporter::UNITS_COUNT,
                             GetJSSystemCompartmentCount,
                             "The number of JavaScript compartments for system code.  The sum of this "
                             "and 'js-compartments-user' might not match the number of "
                             "compartments listed under 'js' if a garbage collection occurs at an "
                             "inopportune time, but such cases should be rare.")

NS_MEMORY_REPORTER_IMPLEMENT(XPConnectJSUserCompartmentCount,
                             "js-compartments-user",
                             KIND_OTHER,
                             nsIMemoryReporter::UNITS_COUNT,
                             GetJSUserCompartmentCount,
                             "The number of JavaScript compartments for user code.  The sum of this "
                             "and 'js-compartments-system' might not match the number of "
                             "compartments listed under 'js' if a garbage collection occurs at an "
                             "inopportune time, but such cases should be rare.")

namespace mozilla {
namespace xpconnect {
namespace memory {

CompartmentStats::CompartmentStats(JSContext *cx, JSCompartment *c)
{
    memset(this, 0, sizeof(*this));

    if (c == cx->runtime->atomsCompartment) {
        name.AssignLiteral("atoms");
    } else if (c->principals) {
        if (c->principals->codebase) {
            name.Assign(c->principals->codebase);

            // If it's the system compartment, append the address.
            // This means that multiple system compartments (and there
            // can be many) can be distinguished.
            if (c->isSystemCompartment) {
                if (c->data &&
                    !((xpc::CompartmentPrivate*)c->data)->location.IsEmpty()) {
                    name.AppendLiteral(", ");
                    name.Append(((xpc::CompartmentPrivate*)c->data)->location);
                }

                // ample; 64-bit address max is 18 chars
                static const int maxLength = 31;
                nsPrintfCString address(maxLength, ", 0x%llx", PRUint64(c));
                name.Append(address);
            }

            // A hack: replace forward slashes with '\\' so they aren't
            // treated as path separators.  Users of the reporters
            // (such as about:memory) have to undo this change.
            name.ReplaceChar('/', '\\');
        } else {
            name.AssignLiteral("null-codebase");
        }
    } else {
        name.AssignLiteral("null-principal");
    }
}

JSBool
CollectCompartmentStatsForRuntime(JSRuntime *rt, IterateData *data)
{
    JSContext *cx = JS_NewContext(rt, 0);
    if (!cx) {
        NS_ERROR("couldn't create context for memory tracing");
        return false;
    }

    {
        JSAutoRequest ar(cx);

        data->compartmentStatsVector.SetCapacity(rt->compartments.length());

        data->gcHeapChunkCleanDecommitted =
            rt->gcChunkPool.countDecommittedArenas(rt) *
            js::gc::ArenaSize;
        data->gcHeapChunkCleanUnused =
            PRInt64(JS_GetGCParameter(rt, JSGC_UNUSED_CHUNKS)) *
            js::gc::ChunkSize -
            data->gcHeapChunkCleanDecommitted;
        data->gcHeapChunkTotal =
            PRInt64(JS_GetGCParameter(rt, JSGC_TOTAL_CHUNKS)) *
            js::gc::ChunkSize;

        js::IterateCompartmentsArenasCells(cx, data, CompartmentCallback,
                                           ArenaCallback, CellCallback);
        js::IterateChunks(cx, data, ChunkCallback);

        for (js::ThreadDataIter i(rt); !i.empty(); i.popFront())
            data->stackSize += i.threadData()->stackSpace.committedSize();

        size_t usable = moz_malloc_usable_size(rt);
        data->runtimeObjectSize = usable ? usable : sizeof(JSRuntime);

        // Nb: |countMe| is false because atomState.atoms is within JSRuntime,
        // and so counted when JSRuntime is counted.
        data->atomsTableSize =
            rt->atomState.atoms.sizeOf(moz_malloc_usable_size, /* countMe */false);
    }

    JS_DestroyContextNoGC(cx);

    // This is initialized to all bytes stored in used chunks, and then we
    // subtract used space from it each time around the loop.
    data->gcHeapChunkDirtyUnused = data->gcHeapChunkTotal -
                                   data->gcHeapChunkCleanUnused -
                                   data->gcHeapChunkCleanDecommitted -
                                   data->gcHeapChunkDirtyDecommitted;

    for (PRUint32 index = 0;
         index < data->compartmentStatsVector.Length();
         index++) {
        CompartmentStats &stats = data->compartmentStatsVector[index];

        PRInt64 used = stats.gcHeapArenaHeaders +
                       stats.gcHeapArenaPadding +
                       stats.gcHeapArenaUnused +
                       stats.gcHeapObjectsNonFunction +
                       stats.gcHeapObjectsFunction +
                       stats.gcHeapStrings +
                       stats.gcHeapShapesTree +
                       stats.gcHeapShapesDict +
                       stats.gcHeapScripts +
                       stats.gcHeapTypeObjects +
                       stats.gcHeapXML;

        data->gcHeapChunkDirtyUnused -= used;
        data->gcHeapArenaUnused += stats.gcHeapArenaUnused;
        data->totalObjects += stats.gcHeapObjectsNonFunction + 
                              stats.gcHeapObjectsFunction +
                              stats.objectSlots;
        data->totalShapes  += stats.gcHeapShapesTree + 
                              stats.gcHeapShapesDict +
                              stats.shapesExtraTreeTables +
                              stats.shapesExtraDictTables +
                              stats.typeInferenceMemory.emptyShapes;
        data->totalScripts += stats.gcHeapScripts + 
                              stats.scriptData;
        data->totalStrings += stats.gcHeapStrings + 
                              stats.stringChars;
#ifdef JS_METHODJIT
        data->totalMjit    += stats.mjitCodeMethod + 
                              stats.mjitCodeRegexp +
                              stats.mjitCodeUnused +
                              stats.mjitData;
#endif
        data->totalTypeInference += stats.gcHeapTypeObjects +
                                    stats.typeInferenceMemory.objects +
                                    stats.typeInferenceMemory.scripts + 
                                    stats.typeInferenceMemory.tables;
        data->totalAnalysisTemp  += stats.typeInferenceMemory.temporary;
    }

    size_t numDirtyChunks = (data->gcHeapChunkTotal -
                             data->gcHeapChunkCleanUnused) /
                            js::gc::ChunkSize;
    PRInt64 perChunkAdmin =
        sizeof(js::gc::Chunk) - (sizeof(js::gc::Arena) * js::gc::ArenasPerChunk);
    data->gcHeapChunkAdmin = numDirtyChunks * perChunkAdmin;
    data->gcHeapChunkDirtyUnused -= data->gcHeapChunkAdmin;

    // Why 10000x?  100x because it's a percentage, and another 100x
    // because nsIMemoryReporter requires that for percentage amounts so
    // they can be fractional.
    data->gcHeapUnusedPercentage = (data->gcHeapChunkCleanUnused +
                                    data->gcHeapChunkDirtyUnused +
                                    data->gcHeapChunkCleanDecommitted +
                                    data->gcHeapChunkDirtyDecommitted +
                                    data->gcHeapArenaUnused) * 10000 /
                                   data->gcHeapChunkTotal;

    return true;
}

#define SLOP_BYTES_STRING \
    " The measurement includes slop bytes caused by the heap allocator rounding up request sizes."

static void
ReportCompartmentStats(const CompartmentStats &stats,
                       const nsACString &pathPrefix,
                       nsIMemoryMultiReporterCallback *callback,
                       nsISupports *closure)
{
    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "gc-heap/arena/headers"),
                       JS_GC_HEAP_KIND, stats.gcHeapArenaHeaders,
                       "Memory on the compartment's garbage-collected JavaScript heap, within "
                       "arenas, that is used to hold internal book-keeping information.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "gc-heap/arena/padding"),
                       JS_GC_HEAP_KIND, stats.gcHeapArenaPadding,
                       "Memory on the compartment's garbage-collected JavaScript heap, within "
                       "arenas, that is unused and present only so that other data is aligned. "
                       "This constitutes internal fragmentation.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "gc-heap/arena/unused"),
                       JS_GC_HEAP_KIND, stats.gcHeapArenaUnused,
                       "Memory on the compartment's garbage-collected JavaScript heap, within "
                       "arenas, that could be holding useful data but currently isn't.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "gc-heap/objects/non-function"),
                       JS_GC_HEAP_KIND, stats.gcHeapObjectsNonFunction,
                       "Memory on the compartment's garbage-collected JavaScript heap that holds "
                       "non-function objects.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "gc-heap/objects/function"),
                       JS_GC_HEAP_KIND, stats.gcHeapObjectsFunction,
                       "Memory on the compartment's garbage-collected JavaScript heap that holds "
                       "function objects.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "gc-heap/strings"),
                       JS_GC_HEAP_KIND, stats.gcHeapStrings,
                       "Memory on the compartment's garbage-collected JavaScript heap that holds "
                       "string headers.  String headers contain various pieces of information "
                       "about a string, but do not contain (except in the case of very short "
                       "strings) the string characters;  characters in longer strings are counted "
                       "under 'gc-heap/string-chars' instead.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "gc-heap/scripts"),
                       JS_GC_HEAP_KIND, stats.gcHeapScripts,
                       "Memory on the compartment's garbage-collected JavaScript heap that holds "
                       "JSScript instances. A JSScript is created for each user-defined function "
                       "in a script. One is also created for the top-level code in a script.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "gc-heap/shapes/tree"),
                       JS_GC_HEAP_KIND, stats.gcHeapShapesTree,
                       "Memory on the compartment's garbage-collected JavaScript heap that holds "
                       "shapes that are in a property tree.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "gc-heap/shapes/dict"),
                       JS_GC_HEAP_KIND, stats.gcHeapShapesDict,
                       "Memory on the compartment's garbage-collected JavaScript heap that holds "
                       "shapes that are in dictionary mode.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "gc-heap/type-objects"),
                       JS_GC_HEAP_KIND, stats.gcHeapTypeObjects,
                       "Memory on the compartment's garbage-collected JavaScript heap that holds "
                       "type inference information.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "gc-heap/xml"),
                       JS_GC_HEAP_KIND, stats.gcHeapXML,
                       "Memory on the compartment's garbage-collected JavaScript heap that holds "
                       "E4X XML objects.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "object-slots"),
                       nsIMemoryReporter::KIND_HEAP, stats.objectSlots,
                       "Memory allocated for the compartment's non-fixed object slot arrays, "
                       "which are used to represent object properties.  Some objects also "
                       "contain a fixed number of slots which are stored on the compartment's "
                       "JavaScript heap; those slots are not counted here, but in "
                       "'gc-heap/objects' instead." SLOP_BYTES_STRING,
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "string-chars"),
                       nsIMemoryReporter::KIND_HEAP, stats.stringChars,
                       "Memory allocated to hold the compartment's string characters.  Sometimes "
                       "more memory is allocated than necessary, to simplify string "
                       "concatenation.  Each string also includes a header which is stored on the "
                       "compartment's JavaScript heap;  that header is not counted here, but in "
                       "'gc-heap/strings' instead.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "shapes-extra/tree-tables"),
                       nsIMemoryReporter::KIND_HEAP, stats.shapesExtraTreeTables,
                       "Memory allocated for the compartment's property tables that belong to "
                       "shapes that are in a property tree." SLOP_BYTES_STRING,
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "shapes-extra/dict-tables"),
                       nsIMemoryReporter::KIND_HEAP, stats.shapesExtraDictTables,
                       "Memory allocated for the compartment's property tables that belong to "
                       "shapes that are in dictionary mode." SLOP_BYTES_STRING,
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "shapes-extra/tree-shape-kids"),
                       nsIMemoryReporter::KIND_HEAP, stats.shapesExtraTreeShapeKids,
                       "Memory allocated for the compartment's kid hashes that belong to shapes "
                       "that are in a property tree.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "shapes-extra/empty-shape-arrays"),
                       nsIMemoryReporter::KIND_HEAP,
                       stats.typeInferenceMemory.emptyShapes,
                       "Memory used for arrays attached to prototype JS objects managing shape "
                       "information.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "script-data"),
                       nsIMemoryReporter::KIND_HEAP, stats.scriptData,
                       "Memory allocated for JSScript bytecode and various variable-length "
                       "tables." SLOP_BYTES_STRING,
                       callback, closure);

#ifdef JS_METHODJIT
    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "mjit-code/method"),
                       nsIMemoryReporter::KIND_NONHEAP, stats.mjitCodeMethod,
                       "Memory used by the method JIT to hold the compartment's generated code.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "mjit-code/regexp"),
                       nsIMemoryReporter::KIND_NONHEAP, stats.mjitCodeRegexp,
                       "Memory used by the regexp JIT to hold the compartment's generated code.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "mjit-code/unused"),
                       nsIMemoryReporter::KIND_NONHEAP, stats.mjitCodeUnused,
                       "Memory allocated by the method and/or regexp JIT to hold the "
                       "compartment's code, but which is currently unused.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "mjit-data"),
                       nsIMemoryReporter::KIND_HEAP, stats.mjitData,
                       "Memory used by the method JIT for the compartment's compilation data: "
                       "JITScripts, native maps, and inline cache structs." SLOP_BYTES_STRING,
                       callback, closure);
#endif
#ifdef JS_TRACER
    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "tjit-code"),
                       nsIMemoryReporter::KIND_NONHEAP, stats.tjitCode,
                       "Memory used by the trace JIT to hold the compartment's generated code.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "tjit-data/allocators-main"),
                       nsIMemoryReporter::KIND_HEAP,
                       stats.tjitDataAllocatorsMain,
                       "Memory used by the trace JIT to store the compartment's trace-related "
                       "data.  This data is allocated via the compartment's VMAllocators.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "tjit-data/allocators-reserve"),
                       nsIMemoryReporter::KIND_HEAP,
                       stats.tjitDataAllocatorsReserve,
                       "Memory used by the trace JIT and held in reserve for the compartment's "
                       "VMAllocators in case of OOM.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "tjit-data/trace-monitor"),
                       nsIMemoryReporter::KIND_HEAP,
                       stats.tjitDataNonAllocators,
                       "Memory used by the trace JIT that is stored in the TraceMonitor.  This "
                       "includes the TraceMonitor object itself, plus its TraceNativeStorage, "
                       "RecordAttemptMap, and LoopProfileMap.",
                       callback, closure);
#endif

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "type-inference/script-main"),
                       nsIMemoryReporter::KIND_HEAP,
                       stats.typeInferenceMemory.scripts,
                       "Memory used during type inference to store type sets of variables "
                       "and dynamically observed types.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "type-inference/object-main"),
                       JS_GC_HEAP_KIND,
                       stats.typeInferenceMemory.objects,
                       "Memory used during type inference to store types and possible "
                       "property types of JS objects.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "type-inference/tables"),
                       nsIMemoryReporter::KIND_HEAP,
                       stats.typeInferenceMemory.tables,
                       "Memory used during type inference for compartment-wide tables.",
                       callback, closure);

    ReportMemoryBytes0(MakeMemoryReporterPath(pathPrefix, stats.name,
                                              "analysis-temporary"),
                       nsIMemoryReporter::KIND_HEAP,
                       stats.typeInferenceMemory.temporary,
                       "Memory used during type inference and compilation to hold transient "
                       "analysis information.  Cleared on GC.",
                       callback, closure);
}

void
ReportJSRuntimeStats(const IterateData &data, const nsACString &pathPrefix,
                     nsIMemoryMultiReporterCallback *callback,
                     nsISupports *closure)
{
    for (PRUint32 index = 0;
         index < data.compartmentStatsVector.Length();
         index++) {
        ReportCompartmentStats(data.compartmentStatsVector[index], pathPrefix,
                               callback, closure);
    }

    ReportMemoryBytes(pathPrefix + NS_LITERAL_CSTRING("runtime/runtime-object"),
                      nsIMemoryReporter::KIND_NONHEAP, data.runtimeObjectSize,
                      "Memory used by the JSRuntime object." SLOP_BYTES_STRING,
                      callback, closure);

    ReportMemoryBytes(pathPrefix + NS_LITERAL_CSTRING("runtime/atoms-table"),
                      nsIMemoryReporter::KIND_NONHEAP, data.atomsTableSize,
                      "Memory used by the atoms table.",
                      callback, closure);

    ReportMemoryBytes(pathPrefix + NS_LITERAL_CSTRING("stack"),
                      nsIMemoryReporter::KIND_NONHEAP, data.stackSize,
                      "Memory used for the JavaScript stack.  This is the committed portion "
                      "of the stack; any uncommitted portion is not measured because it "
                      "hardly costs anything.",
                      callback, closure);

    ReportMemoryBytes(pathPrefix +
                      NS_LITERAL_CSTRING("gc-heap-chunk-dirty-unused"),
                      JS_GC_HEAP_KIND, data.gcHeapChunkDirtyUnused,
                      "Memory on the garbage-collected JavaScript heap, within chunks with at "
                      "least one allocated GC thing, that could be holding useful data but "
                      "currently isn't.  Memory here is mutually exclusive with memory reported"
                      "under gc-heap-decommitted.",
                      callback, closure);

    ReportMemoryBytes(pathPrefix +
                      NS_LITERAL_CSTRING("gc-heap-chunk-clean-unused"),
                      JS_GC_HEAP_KIND, data.gcHeapChunkCleanUnused,
                      "Memory on the garbage-collected JavaScript heap taken by completely empty "
                      "chunks, that soon will be released unless claimed for new allocations.  "
                      "Memory here is mutually exclusive with memory reported under "
                      "gc-heap-decommitted.",
                      callback, closure);

    ReportMemoryBytes(pathPrefix +
                      NS_LITERAL_CSTRING("gc-heap-decommitted"),
                      JS_GC_HEAP_KIND,
                      data.gcHeapChunkCleanDecommitted + data.gcHeapChunkDirtyDecommitted,
                      "Memory in the address space of the garbage-collected JavaScript heap that "
                      "is currently returned to the OS.",
                      callback, closure);

    ReportMemoryBytes(pathPrefix +
                      NS_LITERAL_CSTRING("gc-heap-chunk-admin"),
                      JS_GC_HEAP_KIND, data.gcHeapChunkAdmin,
                      "Memory on the garbage-collected JavaScript heap, within chunks, that is "
                      "used to hold internal book-keeping information.",
                      callback, closure);

}

} // namespace memory
} // namespace xpconnect
} // namespace mozilla

class XPConnectJSCompartmentsMultiReporter : public nsIMemoryMultiReporter
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD CollectReports(nsIMemoryMultiReporterCallback *callback,
                              nsISupports *closure)
    {
        JSRuntime *rt = nsXPConnect::GetRuntimeInstance()->GetJSRuntime();

        // In the first step we get all the stats and stash them in a local
        // data structure.  In the second step we pass all the stashed stats to
        // the callback.  Separating these steps is important because the
        // callback may be a JS function, and executing JS while getting these
        // stats seems like a bad idea.
        IterateData data;
        if (!CollectCompartmentStatsForRuntime(rt, &data))
            return NS_ERROR_FAILURE;

        NS_NAMED_LITERAL_CSTRING(pathPrefix, "explicit/js/");

        // This is the second step (see above).
        ReportJSRuntimeStats(data, pathPrefix, callback, closure);

        ReportMemoryBytes(NS_LITERAL_CSTRING("js-gc-heap-chunk-dirty-unused"),
                          nsIMemoryReporter::KIND_OTHER,
                          data.gcHeapChunkDirtyUnused,
                          "The same as 'explicit/js/gc-heap-chunk-dirty-unused'.  Shown here for "
                          "easy comparison with other 'js-gc' reporters.",
                          callback, closure);

        ReportMemoryBytes(NS_LITERAL_CSTRING("js-gc-heap-chunk-clean-unused"),
                          nsIMemoryReporter::KIND_OTHER,
                          data.gcHeapChunkCleanUnused,
                          "The same as 'explicit/js/gc-heap-chunk-clean-unused'.  Shown here for "
                          "easy comparison with other 'js-gc' reporters.",
                          callback, closure);

        ReportMemoryBytes(NS_LITERAL_CSTRING("js-gc-heap-decommitted"),
                          nsIMemoryReporter::KIND_OTHER,
                          data.gcHeapChunkCleanDecommitted + data.gcHeapChunkDirtyDecommitted,
                          "The same as 'explicit/js/gc-heap-decommitted'.  Shown here for "
                          "easy comparison with other 'js-gc' reporters.",
                          callback, closure);

        ReportMemoryBytes(NS_LITERAL_CSTRING("js-gc-heap-arena-unused"),
                          nsIMemoryReporter::KIND_OTHER,
                          data.gcHeapArenaUnused,
                          "Memory on the garbage-collected JavaScript heap, within arenas, that "
                          "could be holding useful data but currently isn't.  This is the sum of "
                          "all compartments' 'gc-heap/arena-unused' numbers.",
                          callback, closure);

        ReportMemoryPercentage(NS_LITERAL_CSTRING("js-gc-heap-unused-fraction"),
                               nsIMemoryReporter::KIND_OTHER,
                               data.gcHeapUnusedPercentage,
                               "Fraction of the garbage-collected JavaScript heap that is unused. "
                               "Computed as ('js-gc-heap-chunk-clean-unused' + "
                               "'js-gc-heap-chunk-dirty-unused' + 'js-gc-heap-decommitted' + "
                               "'js-gc-heap-arena-unused') / 'js-gc-heap'.",
                               callback, closure);

        ReportMemoryBytes(NS_LITERAL_CSTRING("js-total-objects"),
                          nsIMemoryReporter::KIND_OTHER, data.totalObjects,
                          "Memory used for all object-related data.  This is the sum of all "
                          "compartments' 'gc-heap/objects-non-function', "
                          "'gc-heap/objects-function' and 'object-slots' numbers.",
                          callback, closure);

        ReportMemoryBytes(NS_LITERAL_CSTRING("js-total-shapes"),
                          nsIMemoryReporter::KIND_OTHER, data.totalShapes,
                          "Memory used for all shape-related data.  This is the sum of all "
                          "compartments' 'gc-heap/shapes/tree', 'gc-heap/shapes/dict', "
                          "'shapes-extra/tree-tables', 'shapes-extra/dict-tables', "
                          "'shapes-extra/tree-shape-kids' and 'shapes-extra/empty-shape-arrays'.",
                          callback, closure);

        ReportMemoryBytes(NS_LITERAL_CSTRING("js-total-scripts"),
                          nsIMemoryReporter::KIND_OTHER, data.totalScripts,
                          "Memory used for all script-related data.  This is the sum of all "
                          "compartments' 'gc-heap/scripts' and 'script-data' numbers.",
                          callback, closure);

        ReportMemoryBytes(NS_LITERAL_CSTRING("js-total-strings"),
                          nsIMemoryReporter::KIND_OTHER, data.totalStrings,
                          "Memory used for all string-related data.  This is the sum of all "
                          "compartments' 'gc-heap/strings' and 'string-chars' numbers.",
                          callback, closure);
#ifdef JS_METHODJIT
        ReportMemoryBytes(NS_LITERAL_CSTRING("js-total-mjit"),
                          nsIMemoryReporter::KIND_OTHER, data.totalMjit,
                          "Memory used by the method JIT.  This is the sum of all compartments' "
                          "'mjit-code-method', 'mjit-code-regexp', 'mjit-code-unused' and '"
                          "'mjit-data' numbers.",
                          callback, closure);
#endif
        ReportMemoryBytes(NS_LITERAL_CSTRING("js-total-type-inference"),
                          nsIMemoryReporter::KIND_OTHER, data.totalTypeInference,
                          "Non-transient memory used by type inference.  This is the sum of all "
                          "compartments' 'gc-heap/type-objects', 'type-inference/script-main', "
                          "'type-inference/object-main' and 'type-inference/tables' numbers.",
                          callback, closure);

        ReportMemoryBytes(NS_LITERAL_CSTRING("js-total-analysis-temporary"),
                          nsIMemoryReporter::KIND_OTHER, data.totalAnalysisTemp,
                          "Transient memory used during type inference and compilation. "
                          "This is the sum of all compartments' 'analysis-temporary' numbers.",
                          callback, closure);

        return NS_OK;
    }
};

NS_IMPL_THREADSAFE_ISUPPORTS1(XPConnectJSCompartmentsMultiReporter
                              , nsIMemoryMultiReporter
                              )

#ifdef MOZ_CRASHREPORTER
static JSBool
DiagnosticMemoryCallback(void *ptr, size_t size)
{
    return CrashReporter::RegisterAppMemory(ptr, size) == NS_OK;
}
#endif

static void
AccumulateTelemetryCallback(int id, JSUint32 sample)
{
    switch (id) {
      case JS_TELEMETRY_GC_REASON:
        Telemetry::Accumulate(Telemetry::GC_REASON, sample);
        break;
      case JS_TELEMETRY_GC_IS_COMPARTMENTAL:
        Telemetry::Accumulate(Telemetry::GC_IS_COMPARTMENTAL, sample);
        break;
      case JS_TELEMETRY_GC_IS_SHAPE_REGEN:
        Telemetry::Accumulate(Telemetry::GC_IS_SHAPE_REGEN, sample);
        break;
      case JS_TELEMETRY_GC_MS:
        Telemetry::Accumulate(Telemetry::GC_MS, sample);
        break;
      case JS_TELEMETRY_GC_MARK_MS:
        Telemetry::Accumulate(Telemetry::GC_MARK_MS, sample);
        break;
      case JS_TELEMETRY_GC_SWEEP_MS:
        Telemetry::Accumulate(Telemetry::GC_SWEEP_MS, sample);
        break;
    }
}

bool XPCJSRuntime::gNewDOMBindingsEnabled;

XPCJSRuntime::XPCJSRuntime(nsXPConnect* aXPConnect)
 : mXPConnect(aXPConnect),
   mJSRuntime(nsnull),
   mJSCycleCollectionContext(nsnull),
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
   mWatchdogHibernating(false),
   mLastActiveTime(-1)
{
#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
    DEBUG_WrappedNativeHashtable =
        JS_NewDHashTable(JS_DHashGetStubOps(), nsnull,
                         sizeof(JSDHashEntryStub), 128);
#endif
    NS_TIME_FUNCTION;

    DOM_InitInterfaces();
    Preferences::AddBoolVarCache(&gNewDOMBindingsEnabled, "dom.new_bindings",
                                 JS_FALSE);


    // these jsids filled in later when we have a JSContext to work with.
    mStrIDs[0] = JSID_VOID;

    mJSRuntime = JS_NewRuntime(32L * 1024L * 1024L); // pref ?
    if (!mJSRuntime)
        NS_RUNTIMEABORT("JS_NewRuntime failed.");

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
        JS_SetExtraGCRootsTracer(mJSRuntime, TraceBlackJS, this);
        JS_SetGrayGCRootsTracer(mJSRuntime, TraceGrayJS, this);
        JS_SetWrapObjectCallbacks(mJSRuntime,
                                  xpc::WrapperFactory::Rewrap,
                                  xpc::WrapperFactory::PrepareForWrapping);
#ifdef MOZ_CRASHREPORTER
        JS_EnumerateDiagnosticMemoryRegions(DiagnosticMemoryCallback);
#endif
        JS_SetAccumulateTelemetryCallback(mJSRuntime, AccumulateTelemetryCallback);
        mWatchdogWakeup = JS_NEW_CONDVAR(mJSRuntime->gcLock);
        if (!mWatchdogWakeup)
            NS_RUNTIMEABORT("JS_NEW_CONDVAR failed.");

        mJSRuntime->setActivityCallback(ActivityCallback, this);

        NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(XPConnectJSGCHeap));
        NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(XPConnectJSSystemCompartmentCount));
        NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(XPConnectJSUserCompartmentCount));
        NS_RegisterMemoryMultiReporter(new XPConnectJSCompartmentsMultiReporter);
    }

    if (!JS_DHashTableInit(&mJSHolders, JS_DHashGetStubOps(), nsnull,
                           sizeof(ObjectHolder), 512))
        mJSHolders.ops = nsnull;

    mCompartmentMap.Init();
    mMTCompartmentMap.Init();

    // Install a JavaScript 'debugger' keyword handler in debug builds only
#ifdef DEBUG
    if (mJSRuntime && !JS_GetGlobalDebugHooks(mJSRuntime)->debuggerHandler)
        xpc_InstallJSDebuggerKeywordHandler(mJSRuntime);
#endif

    if (mWatchdogWakeup) {
        AutoLockJSGC lock(mJSRuntime);

        mWatchdogThread = PR_CreateThread(PR_USER_THREAD, WatchdogMain, this,
                                          PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
                                          PR_UNJOINABLE_THREAD, 0);
        if (!mWatchdogThread)
            NS_RUNTIMEABORT("PR_CreateThread failed!");
    }
}

// static
XPCJSRuntime*
XPCJSRuntime::newXPCJSRuntime(nsXPConnect* aXPConnect)
{
    NS_PRECONDITION(aXPConnect,"bad param");

    XPCJSRuntime* self = new XPCJSRuntime(aXPConnect);

    if (self                                  &&
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
        self->mWatchdogThread) {
        return self;
    }

    NS_RUNTIMEABORT("new XPCJSRuntime failed to initialize.");

    delete self;
    return nsnull;
}

JSBool
XPCJSRuntime::OnJSContextNew(JSContext *cx)
{
    NS_TIME_FUNCTION;

    // if it is our first context then we need to generate our string ids
    JSBool ok = JS_TRUE;
    if (JSID_IS_VOID(mStrIDs[0])) {
        JS_SetGCParameterForThread(cx, JSGC_MAX_CODE_CACHE_BYTES, 16 * 1024 * 1024);
        {
            // Scope the JSAutoRequest so it goes out of scope before calling
            // mozilla::dom::binding::DefineStaticJSVals.
            JSAutoRequest ar(cx);
            for (uintN i = 0; i < IDX_TOTAL_COUNT; i++) {
                JSString* str = JS_InternString(cx, mStrings[i]);
                if (!str || !JS_ValueToId(cx, STRING_TO_JSVAL(str), &mStrIDs[i])) {
                    mStrIDs[0] = JSID_VOID;
                    ok = JS_FALSE;
                    break;
                }
                mStrJSVals[i] = STRING_TO_JSVAL(str);
            }
        }

        ok = mozilla::dom::binding::DefineStaticJSVals(cx);
    }
    if (!ok)
        return JS_FALSE;

    XPCPerThreadData* tls = XPCPerThreadData::GetData(cx);
    if (!tls)
        return JS_FALSE;

    XPCContext* xpc = new XPCContext(this, cx);
    if (!xpc)
        return JS_FALSE;

    JS_SetNativeStackQuota(cx, 128 * sizeof(size_t) * 1024);

    // we want to mark the global object ourselves since we use a different color
    JS_ToggleOptions(cx, JSOPTION_UNROOTED_GLOBAL);

    return JS_TRUE;
}

JSBool
XPCJSRuntime::DeferredRelease(nsISupports* obj)
{
    NS_ASSERTION(obj, "bad param");

    if (mNativesToReleaseArray.IsEmpty()) {
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
        while (JS_ContextIterator(mJSRuntime, &iter))
            ++cxCount;
        XPC_LOG_ALWAYS(("%d JS context(s)", cxCount));

        iter = nsnull;
        while (JS_ContextIterator(mJSRuntime, &iter)) {
            XPCContext *xpc = XPCContext::GetXPCContext(iter);
            XPC_LOG_INDENT();
            xpc->DebugDump(depth);
            XPC_LOG_OUTDENT();
        }

        XPC_LOG_ALWAYS(("mWrappedJSClassMap @ %x with %d wrapperclasses(s)",  \
                        mWrappedJSClassMap, mWrappedJSClassMap ?              \
                        mWrappedJSClassMap->Count() : 0));
        // iterate wrappersclasses...
        if (depth && mWrappedJSClassMap && mWrappedJSClassMap->Count()) {
            XPC_LOG_INDENT();
            mWrappedJSClassMap->Enumerate(WrappedJSClassMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }
        XPC_LOG_ALWAYS(("mWrappedJSMap @ %x with %d wrappers(s)",             \
                        mWrappedJSMap, mWrappedJSMap ?                        \
                        mWrappedJSMap->Count() : 0));
        // iterate wrappers...
        if (depth && mWrappedJSMap && mWrappedJSMap->Count()) {
            XPC_LOG_INDENT();
            mWrappedJSMap->Enumerate(WrappedJSMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }

        XPC_LOG_ALWAYS(("mIID2NativeInterfaceMap @ %x with %d interface(s)",  \
                        mIID2NativeInterfaceMap, mIID2NativeInterfaceMap ?    \
                        mIID2NativeInterfaceMap->Count() : 0));

        XPC_LOG_ALWAYS(("mClassInfo2NativeSetMap @ %x with %d sets(s)",       \
                        mClassInfo2NativeSetMap, mClassInfo2NativeSetMap ?    \
                        mClassInfo2NativeSetMap->Count() : 0));

        XPC_LOG_ALWAYS(("mThisTranslatorMap @ %x with %d translator(s)",      \
                        mThisTranslatorMap, mThisTranslatorMap ?              \
                        mThisTranslatorMap->Count() : 0));

        XPC_LOG_ALWAYS(("mNativeSetMap @ %x with %d sets(s)",                 \
                        mNativeSetMap, mNativeSetMap ?                        \
                        mNativeSetMap->Count() : 0));

        // iterate sets...
        if (depth && mNativeSetMap && mNativeSetMap->Count()) {
            XPC_LOG_INDENT();
            mNativeSetMap->Enumerate(NativeSetDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }

        XPC_LOG_OUTDENT();
#endif
}

/***************************************************************************/

void
XPCRootSetElem::AddToRootSet(XPCLock *lock, XPCRootSetElem **listHead)
{
    NS_ASSERTION(!mSelfp, "Must be not linked");

    XPCAutoLock autoLock(lock);

    mSelfp = listHead;
    mNext = *listHead;
    if (mNext) {
        NS_ASSERTION(mNext->mSelfp == listHead, "Must be list start");
        mNext->mSelfp = &mNext;
    }
    *listHead = this;
}

void
XPCRootSetElem::RemoveFromRootSet(XPCLock *lock)
{
    NS_ASSERTION(mSelfp, "Must be linked");

    XPCAutoLock autoLock(lock);

    NS_ASSERTION(*mSelfp == this, "Link invariant");
    *mSelfp = mNext;
    if (mNext)
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
    bool found = extraGCCallbacks.RemoveElement(cb);
    if (!found) {
        NS_ERROR("Removing a callback which was never added.");
    }
}
