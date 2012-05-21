/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class used to manage the wrapped native objects within a JS scope. */

#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "jsproxy.h"

#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;

/***************************************************************************/

#ifdef XPC_TRACK_SCOPE_STATS
static int DEBUG_TotalScopeCount;
static int DEBUG_TotalLiveScopeCount;
static int DEBUG_TotalMaxScopeCount;
static int DEBUG_TotalScopeTraversalCount;
static bool    DEBUG_DumpedStats;
#endif

#ifdef DEBUG
static void DEBUG_TrackNewScope(XPCWrappedNativeScope* scope)
{
#ifdef XPC_TRACK_SCOPE_STATS
    DEBUG_TotalScopeCount++;
    DEBUG_TotalLiveScopeCount++;
    if (DEBUG_TotalMaxScopeCount < DEBUG_TotalLiveScopeCount)
        DEBUG_TotalMaxScopeCount = DEBUG_TotalLiveScopeCount;
#endif
}

static void DEBUG_TrackDeleteScope(XPCWrappedNativeScope* scope)
{
#ifdef XPC_TRACK_SCOPE_STATS
    DEBUG_TotalLiveScopeCount--;
#endif
}

static void DEBUG_TrackScopeTraversal()
{
#ifdef XPC_TRACK_SCOPE_STATS
    DEBUG_TotalScopeTraversalCount++;
#endif
}

static void DEBUG_TrackScopeShutdown()
{
#ifdef XPC_TRACK_SCOPE_STATS
    if (!DEBUG_DumpedStats) {
        DEBUG_DumpedStats = true;
        printf("%d XPCWrappedNativeScope(s) were constructed.\n",
               DEBUG_TotalScopeCount);

        printf("%d XPCWrappedNativeScopes(s) max alive at one time.\n",
               DEBUG_TotalMaxScopeCount);

        printf("%d XPCWrappedNativeScope(s) alive now.\n" ,
               DEBUG_TotalLiveScopeCount);

        printf("%d traversals of Scope list.\n",
               DEBUG_TotalScopeTraversalCount);
    }
#endif
}
#else
#define DEBUG_TrackNewScope(scope) ((void)0)
#define DEBUG_TrackDeleteScope(scope) ((void)0)
#define DEBUG_TrackScopeTraversal() ((void)0)
#define DEBUG_TrackScopeShutdown() ((void)0)
#endif

/***************************************************************************/

XPCWrappedNativeScope* XPCWrappedNativeScope::gScopes = nsnull;
XPCWrappedNativeScope* XPCWrappedNativeScope::gDyingScopes = nsnull;

// static
XPCWrappedNativeScope*
XPCWrappedNativeScope::GetNewOrUsed(XPCCallContext& ccx, JSObject* aGlobal, nsISupports* aNative)
{

    XPCWrappedNativeScope* scope = FindInJSObjectScope(ccx, aGlobal, true);
    if (!scope)
        scope = new XPCWrappedNativeScope(ccx, aGlobal, aNative);
    else {
        // We need to call SetGlobal in order to refresh our cached
        // mPrototypeJSObject and to clear mPrototypeNoHelper (so we get a new
        // new one if requested in the new scope) in the case where the global
        // object is being reused (JS_ClearScope has been called).  NOTE: We are
        // only called by nsXPConnect::InitClasses.
        scope->SetGlobal(ccx, aGlobal, aNative);
    }
    if (js::GetObjectClass(aGlobal)->flags & JSCLASS_XPCONNECT_GLOBAL)
        JS_SetReservedSlot(aGlobal,
                           JSCLASS_GLOBAL_SLOT_COUNT,
                           PRIVATE_TO_JSVAL(scope));
    return scope;
}

XPCWrappedNativeScope::XPCWrappedNativeScope(XPCCallContext& ccx,
                                             JSObject* aGlobal,
                                             nsISupports* aNative)
    :   mRuntime(ccx.GetRuntime()),
        mWrappedNativeMap(Native2WrappedNativeMap::newMap(XPC_NATIVE_MAP_SIZE)),
        mWrappedNativeProtoMap(ClassInfo2WrappedNativeProtoMap::newMap(XPC_NATIVE_PROTO_MAP_SIZE)),
        mMainThreadWrappedNativeProtoMap(ClassInfo2WrappedNativeProtoMap::newMap(XPC_NATIVE_PROTO_MAP_SIZE)),
        mComponents(nsnull),
        mNext(nsnull),
        mGlobalJSObject(nsnull),
        mPrototypeJSObject(nsnull),
        mPrototypeNoHelper(nsnull),
        mScriptObjectPrincipal(nsnull),
        mNewDOMBindingsEnabled(ccx.GetRuntime()->NewDOMBindingsEnabled()),
        mExperimentalBindingsEnabled(ccx.GetRuntime()->ExperimentalBindingsEnabled())
{
    // add ourselves to the scopes list
    {   // scoped lock
        XPCAutoLock lock(mRuntime->GetMapLock());

#ifdef DEBUG
        for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext)
            MOZ_ASSERT(aGlobal != cur->GetGlobalJSObjectPreserveColor(), "dup object");
#endif

        mNext = gScopes;
        gScopes = this;

        // Grab the XPCContext associated with our context.
        mContext = XPCContext::GetXPCContext(ccx.GetJSContext());
        mContext->AddScope(this);
    }

    if (aGlobal)
        SetGlobal(ccx, aGlobal, aNative);

    DEBUG_TrackNewScope(this);
    MOZ_COUNT_CTOR(XPCWrappedNativeScope);
}

// static
JSBool
XPCWrappedNativeScope::IsDyingScope(XPCWrappedNativeScope *scope)
{
    for (XPCWrappedNativeScope *cur = gDyingScopes; cur; cur = cur->mNext) {
        if (scope == cur)
            return true;
    }
    return false;
}

void
XPCWrappedNativeScope::SetComponents(nsXPCComponents* aComponents)
{
    mComponents = aComponents;
}

nsXPCComponents*
XPCWrappedNativeScope::GetComponents()
{
    return mComponents;
}

// Dummy JS class to let wrappers w/o an xpc prototype share
// scopes. By doing this we avoid allocating a new scope for every
// wrapper on creation of the wrapper, and most wrappers won't need
// their own scope at all for the lifetime of the wrapper.
// WRAPPER_SLOTS is key here (even though there's never anything
// in the private data slot in these prototypes), as the number of
// reserved slots in this class needs to match that of the wrappers
// for the JS engine to share scopes.

js::Class XPC_WN_NoHelper_Proto_JSClass = {
    "XPC_WN_NoHelper_Proto_JSClass",// name;
    WRAPPER_SLOTS,                  // flags;

    /* Mandatory non-null function pointer members. */
    JS_PropertyStub,                // addProperty;
    JS_PropertyStub,                // delProperty;
    JS_PropertyStub,                // getProperty;
    JS_StrictPropertyStub,          // setProperty;
    JS_EnumerateStub,               // enumerate;
    JS_ResolveStub,                 // resolve;
    JS_ConvertStub,                 // convert;
    nsnull,                         // finalize;

    /* Optionally non-null members start here. */
    nsnull,                         // checkAccess;
    nsnull,                         // call;
    nsnull,                         // construct;
    nsnull,                         // hasInstance;
    nsnull,                         // trace;

    JS_NULL_CLASS_EXT,
    XPC_WN_NoCall_ObjectOps
};


void
XPCWrappedNativeScope::SetGlobal(XPCCallContext& ccx, JSObject* aGlobal,
                                 nsISupports* aNative)
{
    // We allow for calling this more than once. This feature is used by
    // nsXPConnect::InitClassesWithNewWrappedGlobal.

    mGlobalJSObject = aGlobal;
    mScriptObjectPrincipal = nsnull;

    // Try to find the native global object. If we didn't receive it explicitly,
    // we might be able to find it in the private slot.
    nsISupports *native;
    if (aNative) {
        native = aNative;
    } else {
        const JSClass *jsClass = js::GetObjectJSClass(aGlobal);
        nsISupports *priv;
        if (!(~jsClass->flags & (JSCLASS_HAS_PRIVATE |
                                 JSCLASS_PRIVATE_IS_NSISUPPORTS))) {
            // Our global has an nsISupports native pointer.  Let's
            // see whether it's what we want.
            priv = static_cast<nsISupports*>(xpc_GetJSPrivate(aGlobal));
        } else if ((jsClass->flags & JSCLASS_IS_DOMJSCLASS) &&
                   dom::DOMJSClass::FromJSClass(jsClass)->mDOMObjectIsISupports) {
            priv = dom::UnwrapDOMObject<nsISupports>(aGlobal, jsClass);
        } else {
            priv = nsnull;
        }
        nsCOMPtr<nsIXPConnectWrappedNative> wn = do_QueryInterface(priv);
        if (wn)
            native = static_cast<XPCWrappedNative*>(wn.get())->GetIdentityObject();
        else
            native = priv;
    }

    // Now init our script object principal, if the new global has one.
    nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(native);
    mScriptObjectPrincipal = sop;

    // Lookup 'globalObject.Object.prototype' for our wrapper's proto
    JSObject *objectPrototype =
        JS_GetObjectPrototype(ccx.GetJSContext(), aGlobal);
    if (objectPrototype)
        mPrototypeJSObject = objectPrototype;
    else
        NS_ERROR("Can't get globalObject.Object.prototype");

    // Clear the no helper wrapper prototype object so that a new one
    // gets created if needed.
    mPrototypeNoHelper = nsnull;
}

XPCWrappedNativeScope::~XPCWrappedNativeScope()
{
    MOZ_COUNT_DTOR(XPCWrappedNativeScope);
    DEBUG_TrackDeleteScope(this);

    // We can do additional cleanup assertions here...

    if (mWrappedNativeMap) {
        NS_ASSERTION(0 == mWrappedNativeMap->Count(), "scope has non-empty map");
        delete mWrappedNativeMap;
    }

    if (mWrappedNativeProtoMap) {
        NS_ASSERTION(0 == mWrappedNativeProtoMap->Count(), "scope has non-empty map");
        delete mWrappedNativeProtoMap;
    }

    if (mMainThreadWrappedNativeProtoMap) {
        NS_ASSERTION(0 == mMainThreadWrappedNativeProtoMap->Count(), "scope has non-empty map");
        delete mMainThreadWrappedNativeProtoMap;
    }

    if (mContext)
        mContext->RemoveScope(this);

    // This should not be necessary, since the Components object should die
    // with the scope but just in case.
    if (mComponents)
        mComponents->mScope = nsnull;

    // XXX we should assert that we are dead or that xpconnect has shutdown
    // XXX might not want to do this at xpconnect shutdown time???
    mComponents = nsnull;

    JSRuntime *rt = mRuntime->GetJSRuntime();
    mGlobalJSObject.finalize(rt);
    mPrototypeJSObject.finalize(rt);
}

JSObject *
XPCWrappedNativeScope::GetPrototypeNoHelper(XPCCallContext& ccx)
{
    // We could create this prototype in SetGlobal(), but all scopes
    // don't need one, so we save ourselves a bit of space if we
    // create these when they're needed.
    if (!mPrototypeNoHelper) {
        mPrototypeNoHelper =
            xpc_NewSystemInheritingJSObject(ccx,
                                            js::Jsvalify(&XPC_WN_NoHelper_Proto_JSClass),
                                            mPrototypeJSObject,
                                            false, mGlobalJSObject);

        NS_ASSERTION(mPrototypeNoHelper,
                     "Failed to create prototype for wrappers w/o a helper");
    } else {
        xpc_UnmarkGrayObject(mPrototypeNoHelper);
    }

    return mPrototypeNoHelper;
}

static JSDHashOperator
WrappedNativeJSGCThingTracer(JSDHashTable *table, JSDHashEntryHdr *hdr,
                             uint32_t number, void *arg)
{
    XPCWrappedNative* wrapper = ((Native2WrappedNativeMap::Entry*)hdr)->value;
    if (wrapper->HasExternalReference() && !wrapper->IsWrapperExpired()) {
        JSTracer* trc = (JSTracer *)arg;
        JS_CALL_OBJECT_TRACER(trc, wrapper->GetFlatJSObjectPreserveColor(),
                              "XPCWrappedNative::mFlatJSObject");
    }

    return JS_DHASH_NEXT;
}

// static
void
XPCWrappedNativeScope::TraceJS(JSTracer* trc, XPCJSRuntime* rt)
{
    // FIXME The lock may not be necessary during tracing as that serializes
    // access to JS runtime. See bug 380139.
    XPCAutoLock lock(rt->GetMapLock());

    // Do JS_CallTracer for all wrapped natives with external references.
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        cur->mWrappedNativeMap->Enumerate(WrappedNativeJSGCThingTracer, trc);
    }
}

static JSDHashOperator
WrappedNativeSuspecter(JSDHashTable *table, JSDHashEntryHdr *hdr,
                       uint32_t number, void *arg)
{
    XPCWrappedNative* wrapper = ((Native2WrappedNativeMap::Entry*)hdr)->value;

    if (wrapper->HasExternalReference()) {
        nsCycleCollectionTraversalCallback *cb =
            static_cast<nsCycleCollectionTraversalCallback *>(arg);
        XPCJSRuntime::SuspectWrappedNative(wrapper, *cb);
    }

    return JS_DHASH_NEXT;
}

// static
void
XPCWrappedNativeScope::SuspectAllWrappers(XPCJSRuntime* rt,
                                          nsCycleCollectionTraversalCallback& cb)
{
    XPCAutoLock lock(rt->GetMapLock());

    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        cur->mWrappedNativeMap->Enumerate(WrappedNativeSuspecter, &cb);
    }
}

// static
void
XPCWrappedNativeScope::StartFinalizationPhaseOfGC(JSFreeOp *fop, XPCJSRuntime* rt)
{
    // FIXME The lock may not be necessary since we are inside JSGC_MARK_END
    // callback and GX serializes access to JS runtime. See bug 380139.
    XPCAutoLock lock(rt->GetMapLock());

    // We are in JSGC_MARK_END and JSGC_FINALIZE_END must always follow it
    // calling FinishedFinalizationPhaseOfGC and clearing gDyingScopes in
    // KillDyingScopes.
    NS_ASSERTION(gDyingScopes == nsnull,
                 "JSGC_MARK_END without JSGC_FINALIZE_END");

    XPCWrappedNativeScope* prev = nsnull;
    XPCWrappedNativeScope* cur = gScopes;

    while (cur) {
        XPCWrappedNativeScope* next = cur->mNext;

        if (cur->mGlobalJSObject &&
            JS_IsAboutToBeFinalized(cur->mGlobalJSObject)) {
            cur->mGlobalJSObject.finalize(fop->runtime());
            cur->mScriptObjectPrincipal = nsnull;
            if (cur->GetCachedDOMPrototypes().IsInitialized())
                 cur->GetCachedDOMPrototypes().Clear();
            // Move this scope from the live list to the dying list.
            if (prev)
                prev->mNext = next;
            else
                gScopes = next;
            cur->mNext = gDyingScopes;
            gDyingScopes = cur;
            cur = nsnull;
        } else {
            if (cur->mPrototypeJSObject &&
                JS_IsAboutToBeFinalized(cur->mPrototypeJSObject)) {
                cur->mPrototypeJSObject.finalize(fop->runtime());
            }
            if (cur->mPrototypeNoHelper &&
                JS_IsAboutToBeFinalized(cur->mPrototypeNoHelper)) {
                cur->mPrototypeNoHelper = nsnull;
            }
        }
        if (cur)
            prev = cur;
        cur = next;
    }
}

// static
void
XPCWrappedNativeScope::FinishedFinalizationPhaseOfGC()
{
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();

    // FIXME The lock may not be necessary since we are inside
    // JSGC_FINALIZE_END callback and at this point GC still serializes access
    // to JS runtime. See bug 380139.
    XPCAutoLock lock(rt->GetMapLock());
    KillDyingScopes();
}

static JSDHashOperator
WrappedNativeMarker(JSDHashTable *table, JSDHashEntryHdr *hdr,
                    uint32_t number_t, void *arg)
{
    ((Native2WrappedNativeMap::Entry*)hdr)->value->Mark();
    return JS_DHASH_NEXT;
}

// We need to explicitly mark all the protos too because some protos may be
// alive in the hashtable but not currently in use by any wrapper
static JSDHashOperator
WrappedNativeProtoMarker(JSDHashTable *table, JSDHashEntryHdr *hdr,
                         uint32_t number, void *arg)
{
    ((ClassInfo2WrappedNativeProtoMap::Entry*)hdr)->value->Mark();
    return JS_DHASH_NEXT;
}

// static
void
XPCWrappedNativeScope::MarkAllWrappedNativesAndProtos()
{
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        cur->mWrappedNativeMap->Enumerate(WrappedNativeMarker, nsnull);
        cur->mWrappedNativeProtoMap->Enumerate(WrappedNativeProtoMarker, nsnull);
        cur->mMainThreadWrappedNativeProtoMap->Enumerate(WrappedNativeProtoMarker, nsnull);
    }

    DEBUG_TrackScopeTraversal();
}

#ifdef DEBUG
static JSDHashOperator
ASSERT_WrappedNativeSetNotMarked(JSDHashTable *table, JSDHashEntryHdr *hdr,
                                 uint32_t number, void *arg)
{
    ((Native2WrappedNativeMap::Entry*)hdr)->value->ASSERT_SetsNotMarked();
    return JS_DHASH_NEXT;
}

static JSDHashOperator
ASSERT_WrappedNativeProtoSetNotMarked(JSDHashTable *table, JSDHashEntryHdr *hdr,
                                      uint32_t number, void *arg)
{
    ((ClassInfo2WrappedNativeProtoMap::Entry*)hdr)->value->ASSERT_SetNotMarked();
    return JS_DHASH_NEXT;
}

// static
void
XPCWrappedNativeScope::ASSERT_NoInterfaceSetsAreMarked()
{
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        cur->mWrappedNativeMap->Enumerate(ASSERT_WrappedNativeSetNotMarked, nsnull);
        cur->mWrappedNativeProtoMap->Enumerate(ASSERT_WrappedNativeProtoSetNotMarked, nsnull);
        cur->mMainThreadWrappedNativeProtoMap->Enumerate(ASSERT_WrappedNativeProtoSetNotMarked, nsnull);
    }
}
#endif

static JSDHashOperator
WrappedNativeTearoffSweeper(JSDHashTable *table, JSDHashEntryHdr *hdr,
                            uint32_t number, void *arg)
{
    ((Native2WrappedNativeMap::Entry*)hdr)->value->SweepTearOffs();
    return JS_DHASH_NEXT;
}

// static
void
XPCWrappedNativeScope::SweepAllWrappedNativeTearOffs()
{
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext)
        cur->mWrappedNativeMap->Enumerate(WrappedNativeTearoffSweeper, nsnull);

    DEBUG_TrackScopeTraversal();
}

// static
void
XPCWrappedNativeScope::KillDyingScopes()
{
    // always called inside the lock!
    XPCWrappedNativeScope* cur = gDyingScopes;
    while (cur) {
        XPCWrappedNativeScope* next = cur->mNext;
        delete cur;
        cur = next;
    }
    gDyingScopes = nsnull;
}

struct ShutdownData
{
    ShutdownData()
        : wrapperCount(0),
          protoCount(0) {}
    int wrapperCount;
    int protoCount;
};

static JSDHashOperator
WrappedNativeShutdownEnumerator(JSDHashTable *table, JSDHashEntryHdr *hdr,
                                uint32_t number, void *arg)
{
    ShutdownData* data = (ShutdownData*) arg;
    XPCWrappedNative* wrapper = ((Native2WrappedNativeMap::Entry*)hdr)->value;

    if (wrapper->IsValid()) {
        wrapper->SystemIsBeingShutDown();
        data->wrapperCount++;
    }
    return JS_DHASH_REMOVE;
}

static JSDHashOperator
WrappedNativeProtoShutdownEnumerator(JSDHashTable *table, JSDHashEntryHdr *hdr,
                                     uint32_t number, void *arg)
{
    ShutdownData* data = (ShutdownData*) arg;
    ((ClassInfo2WrappedNativeProtoMap::Entry*)hdr)->value->
        SystemIsBeingShutDown();
    data->protoCount++;
    return JS_DHASH_REMOVE;
}

//static
void
XPCWrappedNativeScope::SystemIsBeingShutDown()
{
    DEBUG_TrackScopeTraversal();
    DEBUG_TrackScopeShutdown();

    int liveScopeCount = 0;

    ShutdownData data;

    XPCWrappedNativeScope* cur;

    // First move all the scopes to the dying list.

    cur = gScopes;
    while (cur) {
        XPCWrappedNativeScope* next = cur->mNext;
        cur->mNext = gDyingScopes;
        gDyingScopes = cur;
        cur = next;
        liveScopeCount++;
    }
    gScopes = nsnull;

    // Walk the unified dying list and call shutdown on all wrappers and protos

    for (cur = gDyingScopes; cur; cur = cur->mNext) {
        // Give the Components object a chance to try to clean up.
        if (cur->mComponents)
            cur->mComponents->SystemIsBeingShutDown();

        // Walk the protos first. Wrapper shutdown can leave dangling
        // proto pointers in the proto map.
        cur->mWrappedNativeProtoMap->
                Enumerate(WrappedNativeProtoShutdownEnumerator,  &data);
        cur->mMainThreadWrappedNativeProtoMap->
                Enumerate(WrappedNativeProtoShutdownEnumerator,  &data);
        cur->mWrappedNativeMap->
                Enumerate(WrappedNativeShutdownEnumerator,  &data);
    }

    // Now it is safe to kill all the scopes.
    KillDyingScopes();

#ifdef XPC_DUMP_AT_SHUTDOWN
    if (data.wrapperCount)
        printf("deleting nsXPConnect  with %d live XPCWrappedNatives\n",
               data.wrapperCount);
    if (data.protoCount)
        printf("deleting nsXPConnect  with %d live XPCWrappedNativeProtos\n",
               data.protoCount);
    if (liveScopeCount)
        printf("deleting nsXPConnect  with %d live XPCWrappedNativeScopes\n",
               liveScopeCount);
#endif
}


/***************************************************************************/

static
XPCWrappedNativeScope*
GetScopeOfObject(JSObject* obj)
{
    nsISupports* supports;
    js::Class* clazz = js::GetObjectClass(obj);
    JSBool isWrapper = IS_WRAPPER_CLASS(clazz);

    if (isWrapper && IS_SLIM_WRAPPER_OBJECT(obj))
        return GetSlimWrapperProto(obj)->GetScope();

    if (!isWrapper || !(supports = (nsISupports*) xpc_GetJSPrivate(obj)))
        return nsnull;

#ifdef DEBUG
    {
        nsCOMPtr<nsIXPConnectWrappedNative> iface = do_QueryInterface(supports);

        NS_ASSERTION(iface, "Uh, how'd this happen?");
    }
#endif

    // obj is one of our nsXPConnectWrappedNative objects.
    return ((XPCWrappedNative*)supports)->GetScope();
}


#ifdef DEBUG
void DEBUG_CheckForComponentsInScope(JSContext* cx, JSObject* obj,
                                     JSObject* startingObj,
                                     JSBool OKIfNotInitialized,
                                     XPCJSRuntime* runtime)
{
    if (OKIfNotInitialized)
        return;

    if (!(JS_GetOptions(cx) & JSOPTION_PRIVATE_IS_NSISUPPORTS))
        return;

    const char* name = runtime->GetStringName(XPCJSRuntime::IDX_COMPONENTS);
    jsval prop;
    if (JS_LookupProperty(cx, obj, name, &prop) && !JSVAL_IS_PRIMITIVE(prop))
        return;

    // This is pretty much always bad. It usually means that native code is
    // making a callback to an interface implemented in JavaScript, but the
    // document where the JS object was created has already been cleared and the
    // global properties of that document's window are *gone*. Generally this
    // indicates a problem that should be addressed in the design and use of the
    // callback code.
    NS_ERROR("XPConnect is being called on a scope without a 'Components' property!  (stack and details follow)");
    printf("The current JS stack is:\n");
    xpc_DumpJSStack(cx, true, true, true);

    printf("And the object whose scope lacks a 'Components' property is:\n");
    js_DumpObject(startingObj);

    JSObject *p = startingObj;
    while (js::IsWrapper(p)) {
        p = js::GetProxyPrivate(p).toObjectOrNull();
        if (!p)
            break;
        printf("which is a wrapper for:\n");
        js_DumpObject(p);
    }
}
#else
#define DEBUG_CheckForComponentsInScope(ccx, obj, startingObj, OKIfNotInitialized, runtime) \
    ((void)0)
#endif

// static
XPCWrappedNativeScope*
XPCWrappedNativeScope::FindInJSObjectScope(JSContext* cx, JSObject* obj,
                                           JSBool OKIfNotInitialized,
                                           XPCJSRuntime* runtime)
{
    XPCWrappedNativeScope* scope;

    if (!obj)
        return nsnull;

    // If this object is itself a wrapped native then we can get the
    // scope directly.

    scope = GetScopeOfObject(obj);
    if (scope)
        return scope;

    // Else we'll have to look up the parent chain to get the scope

    JSAutoEnterCompartment ac;
    ac.enterAndIgnoreErrors(cx, obj);

#ifdef DEBUG
    JSObject *startingObj = obj;
#endif

    obj = JS_GetGlobalForObject(cx, obj);

    if (js::GetObjectClass(obj)->flags & JSCLASS_XPCONNECT_GLOBAL) {
        scope = XPCWrappedNativeScope::GetNativeScope(obj);
        if (scope)
            return scope;
    }

    if (!runtime) {
        runtime = nsXPConnect::GetRuntimeInstance();
        NS_ASSERTION(runtime, "This should never be null!");
    }

    // XXX We are assuming that the scope count is low enough that traversing
    // the linked list is more reasonable then doing a hashtable lookup.
    XPCWrappedNativeScope* found = nsnull;
    {   // scoped lock
        XPCAutoLock lock(runtime->GetMapLock());

        DEBUG_TrackScopeTraversal();

        for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
            if (obj == cur->GetGlobalJSObjectPreserveColor()) {
                found = cur;
                break;
            }
        }
    }

    if (found) {
        // This cannot be called within the map lock!
        DEBUG_CheckForComponentsInScope(cx, obj, startingObj,
                                        OKIfNotInitialized, runtime);
        return found;
    }

    // Failure to find the scope is only OK if the caller told us it might fail.
    // This flag would only be set in the call from
    // XPCWrappedNativeScope::GetNewOrUsed
    NS_ASSERTION(OKIfNotInitialized, "No scope has this global object!");
    return nsnull;
}

/***************************************************************************/

static JSDHashOperator
WNProtoSecPolicyClearer(JSDHashTable *table, JSDHashEntryHdr *hdr,
                        uint32_t number, void *arg)
{
    XPCWrappedNativeProto* proto =
        ((ClassInfo2WrappedNativeProtoMap::Entry*)hdr)->value;
    *(proto->GetSecurityInfoAddr()) = nsnull;
    return JS_DHASH_NEXT;
}

// static
nsresult
XPCWrappedNativeScope::ClearAllWrappedNativeSecurityPolicies(XPCCallContext& ccx)
{
    // Hold the lock throughout.
    XPCAutoLock lock(ccx.GetRuntime()->GetMapLock());

    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        cur->mWrappedNativeProtoMap->Enumerate(WNProtoSecPolicyClearer, nsnull);
        cur->mMainThreadWrappedNativeProtoMap->Enumerate(WNProtoSecPolicyClearer, nsnull);
    }

    DEBUG_TrackScopeTraversal();

    return NS_OK;
}

static JSDHashOperator
WNProtoRemover(JSDHashTable *table, JSDHashEntryHdr *hdr,
               uint32_t number, void *arg)
{
    XPCWrappedNativeProtoMap* detachedMap = (XPCWrappedNativeProtoMap*)arg;

    XPCWrappedNativeProto* proto = (XPCWrappedNativeProto*)
        ((ClassInfo2WrappedNativeProtoMap::Entry*)hdr)->value;

    detachedMap->Add(proto);

    return JS_DHASH_REMOVE;
}

void
XPCWrappedNativeScope::RemoveWrappedNativeProtos()
{
    XPCAutoLock al(mRuntime->GetMapLock());

    mWrappedNativeProtoMap->Enumerate(WNProtoRemover,
                                      GetRuntime()->GetDetachedWrappedNativeProtoMap());
    mMainThreadWrappedNativeProtoMap->Enumerate(WNProtoRemover,
                                                GetRuntime()->GetDetachedWrappedNativeProtoMap());
}

static PLDHashOperator
TraceDOMPrototype(const char* aKey, JSObject* aData, void* aClosure)
{
    JSTracer *trc = static_cast<JSTracer*>(aClosure);
    JS_CALL_OBJECT_TRACER(trc, aData, "DOM prototype");
    return PL_DHASH_NEXT;
}

void
XPCWrappedNativeScope::TraceDOMPrototypes(JSTracer *trc)
{
    if (mCachedDOMPrototypes.IsInitialized()) {
        mCachedDOMPrototypes.EnumerateRead(TraceDOMPrototype, trc);
    }
}

/***************************************************************************/

// static
void
XPCWrappedNativeScope::DebugDumpAllScopes(PRInt16 depth)
{
#ifdef DEBUG
    depth-- ;

    // get scope count.
    int count = 0;
    XPCWrappedNativeScope* cur;
    for (cur = gScopes; cur; cur = cur->mNext)
        count++ ;

    XPC_LOG_ALWAYS(("chain of %d XPCWrappedNativeScope(s)", count));
    XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("gDyingScopes @ %x", gDyingScopes));
        if (depth)
            for (cur = gScopes; cur; cur = cur->mNext)
                cur->DebugDump(depth);
    XPC_LOG_OUTDENT();
#endif
}

#ifdef DEBUG
static JSDHashOperator
WrappedNativeMapDumpEnumerator(JSDHashTable *table, JSDHashEntryHdr *hdr,
                               uint32_t number, void *arg)
{
    ((Native2WrappedNativeMap::Entry*)hdr)->value->DebugDump(*(PRInt16*)arg);
    return JS_DHASH_NEXT;
}
static JSDHashOperator
WrappedNativeProtoMapDumpEnumerator(JSDHashTable *table, JSDHashEntryHdr *hdr,
                                    uint32_t number, void *arg)
{
    ((ClassInfo2WrappedNativeProtoMap::Entry*)hdr)->value->DebugDump(*(PRInt16*)arg);
    return JS_DHASH_NEXT;
}
#endif

void
XPCWrappedNativeScope::DebugDump(PRInt16 depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("XPCWrappedNativeScope @ %x", this));
    XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("mRuntime @ %x", mRuntime));
        XPC_LOG_ALWAYS(("mNext @ %x", mNext));
        XPC_LOG_ALWAYS(("mComponents @ %x", mComponents.get()));
        XPC_LOG_ALWAYS(("mGlobalJSObject @ %x", mGlobalJSObject.get()));
        XPC_LOG_ALWAYS(("mPrototypeJSObject @ %x", mPrototypeJSObject.get()));
        XPC_LOG_ALWAYS(("mPrototypeNoHelper @ %x", mPrototypeNoHelper));

        XPC_LOG_ALWAYS(("mWrappedNativeMap @ %x with %d wrappers(s)",         \
                        mWrappedNativeMap,                                    \
                        mWrappedNativeMap ? mWrappedNativeMap->Count() : 0));
        // iterate contexts...
        if (depth && mWrappedNativeMap && mWrappedNativeMap->Count()) {
            XPC_LOG_INDENT();
            mWrappedNativeMap->Enumerate(WrappedNativeMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }

        XPC_LOG_ALWAYS(("mWrappedNativeProtoMap @ %x with %d protos(s)",      \
                        mWrappedNativeProtoMap,                               \
                        mWrappedNativeProtoMap ? mWrappedNativeProtoMap->Count() : 0));
        // iterate contexts...
        if (depth && mWrappedNativeProtoMap && mWrappedNativeProtoMap->Count()) {
            XPC_LOG_INDENT();
            mWrappedNativeProtoMap->Enumerate(WrappedNativeProtoMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }

        XPC_LOG_ALWAYS(("mMainThreadWrappedNativeProtoMap @ %x with %d protos(s)", \
                        mMainThreadWrappedNativeProtoMap,                     \
                        mMainThreadWrappedNativeProtoMap ? mMainThreadWrappedNativeProtoMap->Count() : 0));
        // iterate contexts...
        if (depth && mMainThreadWrappedNativeProtoMap && mMainThreadWrappedNativeProtoMap->Count()) {
            XPC_LOG_INDENT();
            mMainThreadWrappedNativeProtoMap->Enumerate(WrappedNativeProtoMapDumpEnumerator, &depth);
            XPC_LOG_OUTDENT();
        }
    XPC_LOG_OUTDENT();
#endif
}

size_t
XPCWrappedNativeScope::SizeOfAllScopesIncludingThis(nsMallocSizeOfFun mallocSizeOf)
{
    XPCJSRuntime *rt = nsXPConnect::GetRuntimeInstance();
    XPCAutoLock lock(rt->GetMapLock());

    size_t n = 0;
    for (XPCWrappedNativeScope *cur = gScopes; cur; cur = cur->mNext) {
        n += cur->SizeOfIncludingThis(mallocSizeOf);
    }
    return n;
}

size_t
XPCWrappedNativeScope::SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf)
{
    size_t n = 0;
    n += mallocSizeOf(this);
    n += mWrappedNativeMap->SizeOfIncludingThis(mallocSizeOf);
    n += mWrappedNativeProtoMap->SizeOfIncludingThis(mallocSizeOf);
    n += mMainThreadWrappedNativeProtoMap->SizeOfIncludingThis(mallocSizeOf);

    // There are other XPCWrappedNativeScope members that could be measured;
    // the above ones have been seen by DMD to be worth measuring.  More stuff
    // may be added later.

    return n;
}
